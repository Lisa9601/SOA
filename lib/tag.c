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
int insert_tag(int key, int desc){

    struct tag_t *new = kmalloc(sizeof(tag_t), GFP_ATOMIC);

    new->key = key;
    new->desc = key; // =(((((

    spin_lock(&list_lock);
    list_add_tail_rcu(&(new->list), &head);
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
            list_del_rcu(&p->list);
            spin_unlock(&list_lock);
            synchronize_rcu();
            kfree(p);
            return 0;
        }
    }
    synchronize_rcu(&list_lock);
    return -1;
}

// ---------------------------------------------------------------------------------------------------------------------

int tag_get(int key, int command, int permission) {

    int desc = 0;
    int private = 1;

    printk("%s: sys_tag_get called with params %d - %d - %d\n", MODNAME, key, command, permission);

    if(command == CREATE){

        // Valid key values can only be integer numbers >= 1
        if(key < 1){
            printk("%s: invalid key value %d, must be an integer >= 1\n", MODNAME, key);
            return -EINVAL;
        }

        struct tag_t *new = kmalloc(sizeof(tag_t), GFP_ATOMIC);
        new->key = key;
        new->desc = key; // =(((((
        spin_lock(&list_lock);
        list_add_tail_rcu(&(new->list), &head);
        spin_unlock(&list_lock);

        return 0;

        /*
        if(list->count >= 255){
            printk("%s: maximum number of services reached!\n", MODNAME);
            return -1;
        }


        if(key != IPC_PRIVATE){

            private = 0;

            if(tag_list_search_by_key(list, key) != -1){
                // Key already exists
                printk("%s: tag service with key %d already exists\n", MODNAME, key);
                return -1;
            }
        }

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

        //desc = tag_list_search_by_key(list, key);

        if(desc == -1) {
            printk("%s: cannot open tag service with key %d\n", MODNAME, key);
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

    printk("%s: sys_tag_ctl called with params %d - %d\n", MODNAME, tag, command);

    if(command == AWAKE_ALL){
        printk("%s: AWAKE ALL!\n", MODNAME);

        return -1;
    }
    else if(command == REMOVE){
        struct tag_t  *e;

        spin_lock(&list_lock);
        list_for_each_entry(e, &head, list) {
            if (e->key == tag) {
                list_del_rcu(&e->list);
                spin_unlock(&list_lock);
                synchronize_rcu();
                kfree(e);

                printk("%s: tag service %d removed\n", MODNAME, tag);

                return 0;
            }
        }
        spin_unlock(&list_lock);

        // If there's no service with descriptor tag an error is returned
        printk("%s: unable to remove tag service with descriptor %d\n", MODNAME, tag);
        return -ENODATA;
    }

    printk("%s: Wrong command %d, must be either %d (awake all) or %d (remove)\n", MODNAME, command, AWAKE_ALL, REMOVE);
    return -EINVAL;
}