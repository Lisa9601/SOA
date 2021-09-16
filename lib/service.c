/* ---------------------------------------------------------------------------------------------------------------------
 SERVICE

 This module implements a Linux kernel subsystem that allows exchanging messages across threads. Detailed info on what
 this module does can be found in the README.md file.
--------------------------------------------------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/uaccess.h>
#include "../include/service.h"
#include "../include/tag.h"
#include "../config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lisa Trombetti <lisa.trombetti96@gmail.com>");
MODULE_DESCRIPTION("SERVICE");

#define MODNAME "SERVICE"

// Command numbers
#define CREATE 1
#define OPEN 2
#define AWAKE_ALL 3
#define REMOVE 4


// INIT AND CLEANUP ----------------------------------------------------------------------------------------------------

void cleanup_service(void){
    printk("%s: Shutting down service\n", MODNAME);
    cleanup_tags();
}

// ---------------------------------------------------------------------------------------------------------------------


int tag_get(int key, int command, int permission){
    int desc, private;

    private = 1; // By default service it's private

    printk(KERN_DEBUG "%s: tag_get called with params %d - %d - %d\n", MODNAME, key, command, permission);

    if(command == CREATE){
        // Valid key values can only be integer numbers >= 0
        if(key < 0){
            printk(KERN_ERR "%s: Invalid key value %d, must be an integer >= 0\n", MODNAME, key);
            return -EINVAL;
        }

        if(key != IPC_PRIVATE) private = 0; // Service won't be private

        // Try to insert new tag
        desc = insert_tag(key, private, (uid_t)permission);

        desc < 0 ? printk("%s: Unable to create new tag service with key %d\n", MODNAME, key) : printk("%s: New tag service %d created\n", MODNAME, desc);

        return desc;
    }
    else if (command == OPEN){

        // Try to open tag
        desc = open_tag(key, (uid_t)permission);

        desc < 0 ? printk("%s: Unable to open tag service with key %d\n", MODNAME, key) : printk("%s: Tag service %d opened by process %d\n", MODNAME, desc, current->pid);

        return desc;
    }

    printk(KERN_ERR "%s: Wrong command %d, must be either %d (create) or %d (open)\n", MODNAME, command, CREATE, OPEN);
    return -EINVAL;
}


int tag_send(int tag, int level, char *buffer, size_t size){
    int ret;
    char *copied;
    uid_t perm;

    perm = current_uid().val;

    // Check message's size
    if(size > MAX_SIZE){
        printk(KERN_ERR "%s: Maximum size of %d exceeded by message\n", MODNAME, MAX_SIZE);
        return -EINVAL;
    }

    if(size == 0){
        // Empty buffer it's allowed buf size can't be zero
        size = 1;
    }

    copied = (char *)kmalloc(size*sizeof(char), GFP_KERNEL);

    // Copy message to be sent
    ret = copy_from_user((char*)copied,(char*)buffer,size);
    if(ret < 0){
        printk(KERN_ERR "%s: Error copying message from user space\n",MODNAME);
        kfree(copied);
        return -1;
    }

    printk(KERN_DEBUG "%s: tag_send called with params %d - %d - %s - %zu\n", MODNAME, tag, level, copied, size);

    // Send message
    if(wakeup_tag_level(tag, level, perm, copied) < 0){
        printk("%s: Unable to send message to tag service %d level %d\n", MODNAME, tag, level);
        kfree(copied);
        return -1;
    }

    printk("%s: Message successfully sent to tag service %d level %d\n", MODNAME, tag, level);
    kfree(copied);
    return 0;
}


int tag_receive(int tag, int level, char *buffer, size_t size){
    int ret;
    char *message;
    uid_t perm;

    perm = current_uid().val;

    printk(KERN_DEBUG "%s: tag_receive called with params %d - %d - %zu\n", MODNAME, tag, level, size);

    // Create new memory location to store the message
    message = (char *)kmalloc(size*sizeof(char), GFP_KERNEL);

    // Wait for message
    if(wait_tag_message(tag, level, perm, message, size) < 0) {
        printk("%s: Unable to receive new message from tag service %d level %d\n", MODNAME, tag, level);
        kfree(message);
        return -1;
    }

    // Copy to user space
    ret = copy_to_user((char*)buffer, (char*)message, min(size, strlen(message)));
    if(ret < 0){
        printk(KERN_ERR "%s: Error copying message to user space\n",MODNAME);
        kfree(message);
        return -1;
    }

    printk("%s: New message successfully sent to process %d", MODNAME, current->pid);
    kfree(message);
    return 0;
}


int tag_ctl(int tag, int command){
    uid_t perm;

    perm = current_uid().val;

    printk(KERN_DEBUG "%s: tag_ctl called with params %d - %d\n", MODNAME, tag, command);

    if(command == AWAKE_ALL){
        // Awake all sleeping threads
        if(wakeup_tag_level(tag, -1, perm, NULL) < 0){
            printk("%s: Unable to awake all threads for tag service %d\n", MODNAME, tag);
            return -1;
        }

        printk("%s: All threads awakened for tag service %d\n", MODNAME, tag);
        return 0;
    }
    else if(command == REMOVE){
        // Delete tag
        if(delete_tag(tag, perm) < 0){
            printk("%s: Unable to remove tag service %d\n", MODNAME, tag);
            return -1;
        }

        printk("%s: Tag service %d successfully removed\n", MODNAME, tag);
        return 0;
    }

    printk(KERN_ERR "%s: Wrong command %d, must be either %d (awake all) or %d (remove)\n", MODNAME, command, AWAKE_ALL, REMOVE);
    return -EINVAL;
}