#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define DEVICE_NAME "char_dev"
#define DEVICE_FILE_NAME "char_dev"
#define MAJOR_NUM 235
#define BUF_LEN 80

/* Ioctl command definitions */
#define IOCTL_SET_MSG _IOW(MAJOR_NUM, 0, char *)
#define IOCTL_GET_MSG _IOR(MAJOR_NUM, 1, char *)
#define IOCTL_GET_NTH_BYTE _IO(MAJOR_NUM, 2)

static char message[BUF_LEN + 1];
static struct class *cls;
static atomic_t already_open = ATOMIC_INIT(0);

/* This function is called whenever a process attempts to open the device file */
static int device_open(struct inode *inode, struct file *file)
{
    pr_info("device_open(%p)\n", file);

    /* Increment the module's reference count */
    try_module_get(THIS_MODULE);

    return 0;
}

/* This function is called whenever a process closes the device file */
static int device_release(struct inode *inode, struct file *file)
{
    pr_info("device_release(%p,%p)\n", inode, file);

    /* Decrement the module's reference count */
    module_put(THIS_MODULE);

    return 0;
}

/* This function is called whenever a process tries to read from our device file */
static ssize_t device_read(struct file *file, char __user *buffer,
                           size_t length, loff_t *offset)
{
    int bytes_read = 0;
    int max_bytes = BUF_LEN - *offset;

    /* Ensure we don't read past the end of the message */
    if (*offset >= BUF_LEN || message[*offset] == '\0') {
        return 0; /* End of file */
    }

    /* Copy data from kernel space to user space */
    while (length && bytes_read < max_bytes && message[*offset + bytes_read] != '\0') {
        if (put_user(message[*offset + bytes_read], buffer + bytes_read)) {
            return -EFAULT;
        }
        length--;
        bytes_read++;
    }

    pr_info("Sent %d characters to the user\n", bytes_read);

    /* Update the offset */
    *offset += bytes_read;

    /* Return the number of bytes successfully read */
    return bytes_read;
}

/* This function is called whenever a process tries to write to our device file */
static ssize_t device_write(struct file *file, const char __user *buffer,
                            size_t length, loff_t *offset)
{
    int i;

    pr_info("device_write(%p,%p,%ld)\n", file, buffer, length);

    /* Copy data from user space to kernel space */
    for (i = 0; i < length && i < BUF_LEN; i++) {
        if (get_user(message[i], buffer + i)) {
            return -EFAULT;
        }
    }

    message[i] = '\0'; /* Null-terminate the message */
    pr_info("Received %d characters from the user\n", i);

    return i; /* Return the number of bytes successfully written */
}

/* This function is called whenever a process tries to do an ioctl on our device file */
static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
    int i;
    char *temp;
    char ch;

    /* Don't allow concurrent access */
    if (!atomic_cmpxchg(&already_open, 0, 1)) {
        pr_info("Device already open, cannot perform ioctl\n");
        return -EBUSY;
    }

    switch (ioctl_num) {
    case IOCTL_SET_MSG:
        /* Receive a pointer to a message (in user space) and set that to be the device's message */
        temp = (char *)ioctl_param;
        i = 0;

        /* Copy data from user space to kernel space */
        do {
            if (get_user(ch, temp + i)) {
                atomic_set(&already_open, 0);
                return -EFAULT;
            }
            message[i] = ch;
            i++;
        } while (ch && i < BUF_LEN);

        message[i - 1] = '\0'; /* Null-terminate the message */
        pr_info("Received message via ioctl: %s\n", message);
        break;

    case IOCTL_GET_MSG:
        /* Give the current message to the calling process */
        temp = (char *)ioctl_param;
        i = 0;

        /* Copy data from kernel space to user space */
        do {
            ch = message[i];
            if (put_user(ch, temp + i)) {
                atomic_set(&already_open, 0);
                return -EFAULT;
            }
            i++;
        } while (ch && i < BUF_LEN);

        if (put_user('\0', temp + i)) {
            atomic_set(&already_open, 0);
            return -EFAULT;
        }
        break;

    case IOCTL_GET_NTH_BYTE:
        /* Return the nth byte of the message */
        return message[ioctl_param];

    default:
        return -ENOTTY;
    }

    atomic_set(&already_open, 0);
    return 0;
}

/* This structure will hold the functions to be called when a process does
 * something to the device we created.
 */
static struct file_operations fops = {
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
    .unlocked_ioctl = device_ioctl,
};

/* Initialize the module - Register the character device */
static int __init chardev2_init(void)
{
    /* Register the character device */
    int ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);

    /* Negative values signify an error */
    if (ret_val < 0) {
        pr_alert("%s failed with %d\n",
                 "Sorry, registering the character device ", ret_val);
        return ret_val;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_FILE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_FILE_NAME);
#endif
    device_create(cls, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_FILE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_FILE_NAME);

    return 0;
}

/* Cleanup - unregister the appropriate file from /proc */
static void __exit chardev2_exit(void)
{
    device_destroy(cls, MKDEV(MAJOR_NUM, 0));
    class_destroy(cls);

    /* Unregister the device */
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

module_init(chardev2_init);
module_exit(chardev2_exit);

MODULE_LICENSE("GPL");

