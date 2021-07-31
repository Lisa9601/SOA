/* ---------------------------------------------------------------------------------------------------------------------
	TAG SERVICE

 Detailed info on what this module does can be found in the README.md file.
--------------------------------------------------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include "../include/tag.h"

#define MODNAME "TAG SERVICE"
#define CREATE 1
#define OPEN 2
#define AWAKE_ALL 3
#define REMOVE 4
#define MAX_LV 32   			// Max number of levels
#define MAX_SIZE 4096		    // Max buffer size
#define MAX_TAGS 256			// Max number of tag services


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lisa Trombetti <lisa.trombetti96@gmail.com>");

// ---------------------------------------------------------------------------------------------------------------------

struct tag_t {
    struct list_head list;

    int key;
    int desc;
    int private;
};

static LIST_HEAD(head);
static spinlock_t list_lock;

// Search a tag by key
int search_tag_by_key(int key) {
    struct tag_t *p;

    rcu_read_lock();
    list_for_each_entry_rcu(p, &head, list) {

        // Key found
        if(p->key == key){
            rcu_read_unlock();

            // Checks if service is private
            if(p->private == 1) return -2;

            return p->desc;
        }
    }
    rcu_read_unlock();
    return -1;
}

// Search a tag by descriptor
int search_tag_by_desc(int desc){
    struct tag_t *p;

    rcu_read_lock();
    list_for_each_entry_rcu(p, &head, list) {

        // Descriptor found
        if(p->desc == desc){
            rcu_read_unlock();
            return 1;
        }
    }
    rcu_read_unlock();
    return -1;
}

// Insert a new tag
int insert_tag(int key, int desc, int private){

    struct tag_t *new = (struct tag_t *)kmalloc(sizeof(struct tag_t), GFP_ATOMIC);
    new->key = key;
    new->desc = desc;
    new->private = private;

    spin_lock(&list_lock);
    list_add_tail_rcu(&(new->list), &head); // Add at the tail of the list
    spin_unlock(&list_lock);

    return 0;
}

// Deletes a tag
int delete_tag(int desc){

    struct tag_t *p;

    spin_lock(&list_lock);
    list_for_each_entry(p, &head, list){

        // Descriptor found
        if(p->desc == desc){
            list_del_rcu(&p->list); // Remove element
            spin_unlock(&list_lock);
            synchronize_rcu();
            kfree(p); // Reclaim space
            return 0;
        }
    }
    spin_unlock(&list_lock);
    return -1;
}

// ---------------------------------------------------------------------------------------------------------------------

int tag_get(int key, int command, int permission) {

    int desc = 0;
    int private = 1;
    int ret;

    printk("%s: tag_get called with params %d - %d - %d\n", MODNAME, key, command, permission);

    if(command == CREATE){

        // Valid key values can only be integer numbers >= 1
        if(key < 1){
            printk("%s: Invalid key value %d, must be an integer >= 1\n", MODNAME, key);
            return -EINVAL;
        }

        /*
        if(list->count >= 255){
            printk("%s: maximum number of services reached!\n", MODNAME);
            return -1;
        }
        */

        if(key != IPC_PRIVATE) private=0;

        if(search_tag_by_key(key) != -1){
            // Key already exists
            printk("%s: tag service with key %d already exists\n", MODNAME, key);
            return -1;
        }

        desc = key;
        ret = insert_tag(key, desc, private);

        if (ret != 0){
            // Fai roba
            printk("LOL WHAT ?!");
        }

        printk("%s: new tag service %d created\n", MODNAME, desc);

        return 0;

        /*
        if((desc = tag_list_insert(list, key, private)) == -1) {
            printk("%s: unable to add new tag\n", MODNAME);
        }

        printk("%s: new tag service created\n", MODNAME);


        t_tag *p = list->head;
        while (p!= NULL) {
            printk("%s: %d %d\n", MODNAME, p->key, p->desc);
            p = p->next;
        }

        return desc;

         */
    }
    else if (command == OPEN){

        desc = search_tag_by_key(key);

        if(desc == -1) {
            printk("%s: tag service with key %d not found\n", MODNAME, key);
        }
        else if(desc == -2){
            printk("%s: tag service with key %d is private and cannot be opened\n", MODNAME, key);
            desc = -1;
        }
        else {
            printk("%s: tag service %d with key %d opened\n", MODNAME, desc, key);
        }

        return desc;
    }

    printk("%s: Wrong command %d, must be either %d (create) or %d (open)\n", MODNAME, command, CREATE, OPEN);
    return -EINVAL;
}


int tag_send(int tag, int level, char *buffer, size_t size){

    int ret;
    char *copied = (char *)kmalloc(size*sizeof(char), GFP_KERNEL);

    ret = copy_from_user((char*)copied,(char*)buffer,size);

    printk("%s: copy from user returned %d\n",MODNAME,ret);

    printk("%s: sys_tag_send called with params %d - %d - %s - %zu\n", MODNAME, tag, level, copied, size);

    return 0;
}


int tag_receive(int tag, int level, char *buffer, size_t size){

    printk("%s: sys_tag_receive called with params %d - %d - %zu\n", MODNAME, tag, level, size);

    return 0;
}


int tag_ctl(int tag, int command){

    int ret;

    printk("%s: sys_tag_ctl called with params %d - %d\n", MODNAME, tag, command);

    if(command == AWAKE_ALL){
        printk("%s: AWAKE ALL!\n", MODNAME);

        // DA FARE

        return -1;
    }
    else if(command == REMOVE){

        ret = delete_tag(tag);

        if(ret == -1){
            // There's no service with descriptor tag
            printk("%s: unable to remove tag service with descriptor %d\n", MODNAME, tag);
            return -ENODATA;
        }

        printk("%s: tag service %d removed\n", MODNAME, tag);
        return 0;
    }

    printk("%s: Wrong command %d, must be either %d (awake all) or %d (remove)\n", MODNAME, command, AWAKE_ALL, REMOVE);
    return -EINVAL;
}