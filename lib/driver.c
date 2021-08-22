/* ---------------------------------------------------------------------------------------------------------------------
 TAG DRIVER

 Detailed info on what this module does can be found in the README.md file.
--------------------------------------------------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include "../config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lisa Trombetti <lisa.trombetti96@gmail.com>");
MODULE_DESCRIPTION("TAG DRIVER");

#define MODNAME "TAG DRIVER"
#define DEVICE_NAME "tag_dev"
#define BUF_LEN MAX_TAGS*1000


int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);


static int major;
static char *buff;

static struct file_operations fops = {
        .read = device_read,
        .write = device_write,
        .open = device_open,
        .release = device_release
};

/* Initialize device driver */
int init_module(void) {
    major = register_chrdev(0, DEVICE_NAME, &fops);

    // Check if device was registered
    if (major < 0) {
        printk("%s: Unable to register tag_dev device\n", MODNAME);
        return major;
    }

    buff = (char *)kmalloc(sizeof(char)*BUF_LEN, GFP_KERNEL);

    // Check if buffer was correctly allocated
    if(!buff){
        printk("%s: Memory allocation for buffer failed\n", MODNAME);
        return -ENOMEM;
    }

    //printk("'mknod /dev/hello c %d 0'.\n", Major);

    printk("%s: tag_dev successfully registered with major number %d\n", MODNAME, major);
    return 0;
}

/* Remove device driver */
void cleanup_module(void) {

    // Unregister driver
    unregister_chrdev(major, DEVICE_NAME);

    // Free buffer
    if(buff){
        kfree(buff);
    }

    printk("%s: tag_dev unregistered successfully\n", MODNAME);
}

/* Open device file */
static int device_open(struct inode *inode, struct file *file) {
    return 0;
}

/* Close device file */
static int device_release(struct inode *inode, struct file *file) {
    return 0;
}

/* Read dev file*/
static ssize_t device_read(struct file *filp, char *user_buff, size_t size, loff_t *off) {

    ssize_t len = min(sizeof(buff) - *off, size);

    if (len <= 0) return 0;

    // Copy data to user
    if (copy_to_user(user_buff, buff + *off, len)) return -EFAULT;

    *off += len;
    return len;
}


/* Write device file*/
static ssize_t device_write(struct file *filp, const char *user_buff, size_t size, loff_t *off) {
    ssize_t len;

    len = min(sizeof(buff) - *off, size);

    if (len <= 0) return 0;

    // Read data from user
    if (copy_from_user(buff + *off, user_buff, len)) return -EFAULT;

    *off += len;
    return len;
}