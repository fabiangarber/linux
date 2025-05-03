#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/printk.h>

#include "vgpio_c.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabian T Garber");
MODULE_DESCRIPTION("Virtual GPIO Driver with Blocking Read and Debug Mode");
MODULE_VERSION("0.7");

#define NUM_GPIO_PINS 8  // Number of virtual GPIOs

#define GPIO_SET_VALUE _IOW(MAJOR_NUM, 0, struct gpio_data)
#define GPIO_GET_VALUE _IOR(MAJOR_NUM, 1, struct gpio_data)

// Debug flag (default: 0)
static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable debug output (default: 0)");

static struct class *vgpio_class = NULL;
//static struct device *vgpio_device = NULL;
static spinlock_t gpio_lock;
static wait_queue_head_t gpio_wait_queue;
static bool gpio_values[NUM_GPIO_PINS] = {0};
static bool gpio_changed = false;

// Device Open
static int device_open(struct inode *inode, struct file *file)
{
    pr_info("Virtual GPIO device opened\n");
    try_module_get(THIS_MODULE);
    return 0;
}

// Device Close
static int device_release(struct inode *inode, struct file *file)
{
    pr_info("Virtual GPIO device closed\n");
    module_put(THIS_MODULE);
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

            //spin_lock(&gpio_lock);
            gpio_values[data.pin] = (bool)data.value;
            gpio_changed = true;
            //spin_unlock(&gpio_lock);
            wake_up_interruptible(&gpio_wait_queue);

            if (debug) // Only print if debug mode is enabled
                pr_info("GPIO[%d] set to %d\n", data.pin, data.value);
            break;

        case GPIO_GET_VALUE:
            if (copy_from_user(&data, (struct gpio_data __user *)arg, sizeof(data)))
                return -EFAULT;
            if (data.pin < 0 || data.pin >= NUM_GPIO_PINS)
                return -EINVAL;

            //spin_lock(&gpio_lock);
            data.value = gpio_values[data.pin];
            //spin_unlock(&gpio_lock);

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
    int ret = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);

    spin_lock_init(&gpio_lock);
    init_waitqueue_head(&gpio_wait_queue);

    /* Register the character device */

    /* Negative values signify an error */
    if (ret < 0) {
        pr_alert("%s failed with %d\n",
                 "Sorry, registering the character device ", ret);
        return ret;
    }

    vgpio_class = class_create(DEVICE_NAME);

    device_create(vgpio_class, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_NAME);

    pr_info("Virtual GPIO driver loaded\n");
    return 0;
}

// Module Exit
static void __exit virtual_gpio_exit(void)
{
    device_destroy(vgpio_class, MKDEV(MAJOR_NUM, 0));
    class_destroy(vgpio_class);
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    pr_info("Virtual GPIO driver unloaded\n");
}

module_init(virtual_gpio_init);
module_exit(virtual_gpio_exit);
