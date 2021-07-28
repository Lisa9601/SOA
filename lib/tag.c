/* -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	TAG SERVICE
   ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include "../include/tag.h"

#define MODNAME "TAG SERVICE"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lisa Trombetti <lisa.trombetti96@gmail.com>");


asmlinkage int sys_tag_get(int key, int command, int permission) {

    printk("%s: sys_tag_get\n", MODNAME);

    return 0;
}


asmlinkage int sys_tag_send(int tag, int level, char *buffer, size_t size){

    printk("%s: sys_tag_send\n", MODNAME);

    return 0;
}


asmlinkage int sys_tag_receive(int tag, int level, char *buffer, size_t size){

    printk("%s: sys_tag_receive\n", MODNAME);

    return 0;
}


asmlinkage int sys_tag_ctl(int tag, int command){

    printk("%s: sys_tag_ctl\n", MODNAME);

    return 0;
}