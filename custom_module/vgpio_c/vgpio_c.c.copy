#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabian T Garber");
MODULE_DESCRIPTION("Virtual GPIO Driver with Blocking Read and Debug Mode");
MODULE_VERSION("0.7");

#define DEVICE_NAME "vgpio_c"
#define GPIO_MAGIC 'g'
#define NUM_GPIO_PINS 8  // Number of virtual GPIOs

// Debug flag (default: 0)
static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable debug output (default: 0)");

struct gpio_data {
    int pin;
    int value;
};

#define GPIO_SET_VALUE _IOW(GPIO_MAGIC, 1, struct gpio_data)
#define GPIO_GET_VALUE _IOR(GPIO_MAGIC, 2, struct gpio_data)

static struct class *vgpio_class = NULL;
static struct device *vgpio_device = NULL;
static dev_t dev;
static struct cdev cdev;
static spinlock_t gpio_lock;
static wait_queue_head_t gpio_wait_queue;
static bool gpio_values[NUM_GPIO_PINS] = {0};
static bool gpio_changed = false;

// Device Open
static int device_open(struct inode *inode, struct file *file)
{
    dev_info(vgpio_device, "Virtual GPIO device opened\n");
    return 0;
}

// Device Close
static int device_release(struct inode *inode, struct file *file)
{
    dev_info(vgpio_device, "Virtual GPIO device closed\n");
    return 0;
}

// Device IOCTL
static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct gpio_data data;

    switch (cmd) {
        case GPIO_SET_VALUE:
            if (copy_from_user(&data, (struct gpio_data __user *)arg, sizeof(data)))
                return -EFAULT;
            if (data.pin < 0 || data.pin >= NUM_GPIO_PINS)
                return -EINVAL;

            spin_lock(&gpio_lock);
            gpio_values[data.pin] = (bool)data.value;
            gpio_changed = true;
            spin_unlock(&gpio_lock);
            wake_up_interruptible(&gpio_wait_queue);

            if (debug) // Only print if debug mode is enabled
                dev_info(vgpio_device, "GPIO[%d] set to %d\n", data.pin, data.value);
            break;

        case GPIO_GET_VALUE:
            if (copy_from_user(&data, (struct gpio_data __user *)arg, sizeof(data)))
                return -EFAULT;
            if (data.pin < 0 || data.pin >= NUM_GPIO_PINS)
                return -EINVAL;

            spin_lock(&gpio_lock);
            data.value = gpio_values[data.pin];
            spin_unlock(&gpio_lock);

            if (copy_to_user((struct gpio_data __user *)arg, &data, sizeof(data)))
                return -EFAULT;
            break;

        default:
            return -EINVAL;
    }
    return 0;
}

// Blocking Read for Waiting User-space
static ssize_t device_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    if (wait_event_interruptible(gpio_wait_queue, gpio_changed))
        return -ERESTARTSYS; // If interrupted

    gpio_changed = false;
    char data = '1';
    if (copy_to_user(buf, &data, 1))
        return -EFAULT;

    return 1;
}

// File Operations
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .unlocked_ioctl = device_ioctl,
    .read = device_read,  // Read blocks until a virtual event
};

// Module Init
static int __init virtual_gpio_init(void)
{
    int ret;

    spin_lock_init(&gpio_lock);
    init_waitqueue_head(&gpio_wait_queue);

    dev = MKDEV(0, 0);
    ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        dev_alert(vgpio_device, "Failed to allocate char device\n");
        return ret;
    }

    cdev_init(&cdev, &fops);
    cdev.owner = THIS_MODULE;
    ret = cdev_add(&cdev, dev, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev, 1);
        return ret;
    }

    vgpio_class = class_create(DEVICE_NAME);
    if (IS_ERR(vgpio_class)) {
        cdev_del(&cdev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(vgpio_class);
    }

    vgpio_device = device_create(vgpio_class, NULL, dev, NULL, DEVICE_NAME);
    if (IS_ERR(vgpio_device)) {
        class_destroy(vgpio_class);
        cdev_del(&cdev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(vgpio_device);
    }

    dev_info(vgpio_device, "Virtual GPIO driver loaded\n");
    return 0;
}

// Module Exit
static void __exit virtual_gpio_exit(void)
{
    device_destroy(vgpio_class, dev);
    class_destroy(vgpio_class);
    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);
    dev_info(vgpio_device, "Virtual GPIO driver unloaded\n");
}

module_init(virtual_gpio_init);
module_exit(virtual_gpio_exit);
