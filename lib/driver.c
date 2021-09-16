/* ---------------------------------------------------------------------------------------------------------------------
 TAG DRIVER

 This module implements a simple device driver which keeps information about the tag services currently active
--------------------------------------------------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "../include/tag.h"
#include "../include/driver.h"
#include "../config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lisa Trombetti <lisa.trombetti96@gmail.com>");
MODULE_DESCRIPTION("TAG DRIVER");

#define MODNAME "TAG DRIVER"
#define DEVICE_NAME "tag_dev"
#define BUFF_LEN (MAX_TAGS*MAX_LV + 1)*100 // Buffer size

static char *buff; // Buffer used to keep tag services info

// Device
static dev_t dev = 0;
static struct class *dev_class;
static struct cdev c_dev;
static spinlock_t dev_lock;


// Device file operations
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
        .owner = THIS_MODULE,
        .read = device_read,
        .write = device_write,
        .open = device_open,
        .release = device_release
};


/* Initialize device driver */
int init_device(void) {

    // Allocate Major number
    if((alloc_chrdev_region(&dev, 0, 1, "ext_dev")) < 0){
        printk(KERN_ERR "%s: Cannot allocate Major number for device\n", MODNAME);
        return -1;
    }

    printk("%s: Major = %d Minor = %d \n", MODNAME, MAJOR(dev), MINOR(dev));

    // Create class
    if((dev_class = class_create(THIS_MODULE, "ext_class")) == NULL){
        printk(KERN_ERR "%s: Cannot create the struct class for device\n", MODNAME);
        unregister_chrdev_region(dev,1);
        return -1;
    }

    // Create device
    if((device_create(dev_class, NULL, dev, NULL, DEVICE_NAME)) == NULL){
        printk(KERN_ERR "%s: Cannot create the device\n", MODNAME);
        class_destroy(dev_class);
        unregister_chrdev_region(dev,1);
        return -1;
    }

    cdev_init(&c_dev, &fops);

    // Add device
    if(cdev_add(&c_dev, dev, 1) == -1){
        printk( KERN_ERR "%s: Device addition failed\n", MODNAME);
        device_destroy(dev_class,dev);
        class_destroy(dev_class);
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    // Allocate new buffer
    buff = (char *)kmalloc(sizeof(char)*BUFF_LEN, GFP_ATOMIC);
    if(buff == NULL){
        printk(KERN_ERR "%s: Unable to allocate new buffer\n", MODNAME);
        cdev_del(&c_dev);
        device_destroy(dev_class,dev);
        class_destroy(dev_class);
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    printk("%s: %s successfully registered\n", MODNAME, DEVICE_NAME);
    return 0;
}


/* Remove device driver */
void cleanup_device(void) {

    cdev_del(&c_dev);
    device_destroy(dev_class,dev);
    class_destroy(dev_class);
    unregister_chrdev_region(dev, 1);

    kfree(buff);

    printk("%s: %s unregistered successfully\n", MODNAME, DEVICE_NAME);
}


/* Open device file */
static int device_open(struct inode *inode, struct file *file) {
    // Do nothing
    return 0;
}


/* Close device file */
static int device_release(struct inode *inode, struct file *file) {
    // Do nothing
    return 0;
}


/* Read device file */
static ssize_t device_read(struct file *filp, char __user *user_buff, size_t size, loff_t *off) {
    ssize_t len;

    // Update buffer with new tag info
    spin_lock(&dev_lock);
    if(tag_info(buff) < 0) printk("%s: Unable to update buffer with new info\n", MODNAME);
    spin_unlock(&dev_lock);

    len = min((size_t)BUFF_LEN - (size_t)*off, size);

    if (len <= 0) {
        return 0;
    }

    // Copy data to user
    if (copy_to_user(user_buff, buff + *off, len)) {
        return -EFAULT;
    }

    *off += len;

    return len;
}

/* Write device file */
static ssize_t device_write(struct file *filp, const char *user_buff, size_t size, loff_t *off) {
    printk("%s: Write not implemented\n", MODNAME);
    return -1;
}