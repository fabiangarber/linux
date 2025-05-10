// toggle.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include "ioctl_toggle.h"

static int toggle_value = 0;
static dev_t dev_num;
static struct cdev cdev;
static struct class *toggle_class;

static int toggle_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int toggle_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long toggle_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case IOCTL_TOGGLE_GET:
            if (copy_to_user((int *)arg, &toggle_value, sizeof(toggle_value))) {
                return -EFAULT;
            }
            break;
        case IOCTL_TOGGLE_SET:
            if (copy_from_user(&toggle_value, (int *)arg, sizeof(toggle_value))) {
                return -EFAULT;
            }
            if (toggle_value != 0 && toggle_value != 1) {
                return -EINVAL;
            }
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}

static struct file_operations toggle_fops = {
    .owner = THIS_MODULE,
    .open = toggle_open,
    .release = toggle_release,
    .unlocked_ioctl = toggle_ioctl,
};

static int __init toggle_init(void)
{
    if (alloc_chrdev_region(&dev_num, 0, 1, "toggle") < 0) {
        return -1;
    }

    cdev_init(&cdev, &toggle_fops);
    if (cdev_add(&cdev, dev_num, 1) < 0) {
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    toggle_class = class_create("toggle");
    device_create(toggle_class, NULL, dev_num, NULL, "toggle");

    printk(KERN_INFO "Toggle module loaded\n");
    return 0;
}

static void __exit toggle_exit(void)
{
    device_destroy(toggle_class, dev_num);
    class_destroy(toggle_class);
    cdev_del(&cdev);
    unregister_chrdev_region(dev_num, 1);

    printk(KERN_INFO "Toggle module unloaded\n");
}

module_init(toggle_init);
module_exit(toggle_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux kernel module to toggle a value using ioctl");
MODULE_VERSION("0.1");
