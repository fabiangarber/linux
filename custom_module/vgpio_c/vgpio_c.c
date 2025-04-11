#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h> // Add this for msleep
#include <linux/cdev.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabian T Garber");
MODULE_DESCRIPTION("Simple Virtual GPIO Driver");
MODULE_VERSION("0.1");

#define DEVICE_NAME "virtual_gpio"

// IOCTL commands
#define GPIO_MAGIC 'g'
#define GPIO_SET_VALUE _IOW(GPIO_MAGIC, 1, int)
#define GPIO_GET_VALUE _IOR(GPIO_MAGIC, 2, int)

static int major_number;
static struct class *vgpio_class = NULL;
static struct device *vgpio_device = NULL;
static dev_t dev;
static struct cdev cdev;

// Virtual GPIO pin state
static bool gpio_value = 0;

static int device_open(struct inode *inode, struct file *file) {
  printk(KERN_INFO pr_fmt("Device opened\n"));
  return 0;
}

static int device_release(struct inode *inode, struct file *file) {
  printk(KERN_INFO pr_fmt("Device closed\n"));
  return 0;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  int ret = 0;
  bool value;

  switch (cmd) {
  case GPIO_SET_VALUE:
    ret = get_user(value, (bool __user *)arg);
    if (ret == 0) {
      gpio_value = value;
      printk(KERN_INFO pr_fmt("GPIO value set to %d\n"), gpio_value);
    }
    break;
  case GPIO_GET_VALUE:
    ret = put_user(gpio_value, (bool __user *)arg);
    break;
  default:
    ret = -EINVAL;
  }
  return ret;
}

static struct file_operations fops = {
  .owner = THIS_MODULE,
  .open = device_open,
  .release = device_release,
  .unlocked_ioctl = device_ioctl,
};

static int __init virtual_gpio_init(void) {
  int ret;

  // Allocate major number dynamically
  printk(KERN_INFO pr_fmt("Allocate major number\n"));
  ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
  if (ret < 0) {
    printk(KERN_ERR pr_fmt("Failed to allocate major number\n"));
    return ret;
  }
  major_number = MAJOR(dev);

  // Initialize cdev structure
  printk(KERN_INFO pr_fmt("cdev_init\n"));
  cdev_init(&cdev, &fops);
  printk(KERN_INFO pr_fmt("cdev_owner\n"));
  cdev.owner = THIS_MODULE;

  // Add character device to the system
  ret = cdev_add(&cdev, dev, 1);
  if (ret < 0) {
    printk(KERN_ERR pr_fmt("Failed to add cdev\n"));
    unregister_chrdev_region(dev, 1);
    return ret;
  }

  // Create device class
  vgpio_class = class_create("vgpio");
  if (IS_ERR(vgpio_class)) {
    printk(KERN_ERR pr_fmt("Failed to create class\n"));
    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);
    return PTR_ERR(vgpio_class);
  }

  // Create device
  vgpio_device = device_create(vgpio_class, NULL, dev, NULL, DEVICE_NAME);
  if (IS_ERR(vgpio_device)) {
    printk(KERN_ERR pr_fmt("Failed to create device\n"));
    class_destroy(vgpio_class);
    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);
    return PTR_ERR(vgpio_device);
  }

  printk(KERN_INFO pr_fmt("Driver loaded\n"));
  return 0;
}

static void __exit virtual_gpio_exit(void) {
  printk(KERN_INFO pr_fmt("Start cleanup\n"));
  device_destroy(vgpio_class, dev);
  class_destroy(vgpio_class);
  cdev_del(&cdev);
  unregister_chrdev_region(dev, 1);
  printk(KERN_INFO pr_fmt("Driver unloaded\n"));
}

module_init(virtual_gpio_init);
module_exit(virtual_gpio_exit);

