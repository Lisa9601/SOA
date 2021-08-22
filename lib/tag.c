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
#include <linux/cred.h>
#include <linux/string.h>
#include "../include/tag.h"
#include "../include/level.h"
#include "../config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lisa Trombetti <lisa.trombetti96@gmail.com>");
MODULE_DESCRIPTION("TAG SERVICE");

#define MODNAME "TAG SERVICE"

// Command numbers
#define CREATE 1
#define OPEN 2
#define AWAKE_ALL 3
#define REMOVE 4

// TAGS ----------------------------------------------------------------------------------------------------------------

struct tag_t {

    int key;                // Key
    int private;            // If service it's private this value is set to 1
    int busy;
    uid_t perm;             // User id for permission check

    struct list_head *lv_head; // List of levels
    spinlock_t lv_lock;        // Level list write lock

};

static struct tag_t* tags[MAX_TAGS] = {NULL};   // List of tags
static int start = 0;                           // Where to start searching for an available slot
static spinlock_t tag_lock;                     // Tag list lock


/* Search tag by key
 *
 * key = key to be searched
 * uid = user id for permission check
 *
 */
int search_tag(int key, uid_t uid) {
    int i;

    spin_lock(&tag_lock);
    for(i=0; i<MAX_TAGS; i++) {
        // Key found
        if(tags[i] != NULL && tags[i]->key == key){

            // Checks if service it's private
            if(tags[i]->private == 1){
                spin_unlock(&tag_lock);
                return -2;
            }

            // Check user permission
            if(tags[i]->perm != -1 && tags[i]->perm != uid){
                spin_unlock(&tag_lock);
                return -3;
            }

            spin_unlock(&tag_lock);
            return i;
        }
    }

    spin_unlock(&tag_lock);
    return -1;
}

/* Insert a new tag
 *
 * key = tag's key
 * private = whether the service should be private or not (private=1, not_private=0)
 * uid = user id for permission check
 *
 */
int insert_tag(int key, int private, uid_t uid){
    int i, desc;
    struct tag_t *new;
    struct list_head *lv_head;

    desc = -1;

    spin_lock(&tag_lock);
    for(i=0; i<MAX_TAGS; i++) {

        // Check if key already exists
        if (tags[i] != NULL && tags[i]->key == key) {
            spin_unlock(&tag_lock);
            return -2;
        }

        // Free slot found
        if (desc == -1 && tags[i] == NULL) desc = i;

    }

    if(desc != -1){
        // Initializing list of levels
        lv_head = (struct list_head *)kmalloc(sizeof(struct list_head), GFP_ATOMIC);

        // Check if level list was correctly allocated
        if(lv_head == NULL){
            spin_unlock(&tag_lock);
            return -3;
        }

        INIT_LIST_HEAD(lv_head);

        new = (struct tag_t *)kmalloc(sizeof(struct tag_t), GFP_ATOMIC);

        // Check if new tag was correctly allocated
        if(new == NULL){
            spin_unlock(&tag_lock);
            kfree(lv_head); // Release level list
            return -3;
        }

        new->key = key;
        new->private = private;
        new->busy = 0;
        new->perm = uid;
        new->lv_head = lv_head;

        tags[desc] = new;  // Add new tag
        start = desc+1; // Start searching from the following element
    }

    spin_unlock(&tag_lock);
    return desc;
}

/* Checks if tag service it's active and checks user permission
 *
 * tag = tag service to check
 * uid = user id for permission checking
 *
 */
int check_tag(struct tag_t* tag, uid_t uid){

    // Check if tag service it's active
    if(tag == NULL) return -1;

    // Check permission
    if(tag->perm != -1 && tag->perm != uid) return -2;

    // Check if it's busy
    if(tag->busy == 1) return -3;

    return 0;
}

/* Deletes a tag
 *
 * desc = descriptor of the tag service to be removed
 * uid = user id for permission checking
 *
 */
int delete_tag(int desc, uid_t uid){
    int ret;
    struct tag_t *tag;

    spin_lock(&tag_lock);

    // Check tag service
    ret = check_tag(tags[desc], uid);

    if(ret != 0){
        spin_unlock(&tag_lock);
        return ret;
    }

    tags[desc]->busy = 1;
    spin_unlock(&tag_lock);

    // Check if some thread is waiting
    ret = cleanup_levels(tags[desc]->lv_head, tags[desc]->lv_lock, 0);

    spin_lock(&tag_lock);

    if(ret < 0){
        tags[desc]->busy = 0;
        spin_unlock(&tag_lock);
        return -1;
    }

    tag = tags[desc];
    tags[desc] = NULL;
    start = desc; // Start searching from this point next time

    spin_unlock(&tag_lock);

    kfree(tag); // Reclaim space
    return 0;
}

/* Update a level from a specified tag service by adding a new process to the waiting list
 *
 * desc = descriptor of the tag
 * level = level number
 * pid = process id to add to the waiting threads
 * uid = user id for permission checking
 *
 */
int add_pid_to_tag_level(int desc, int level, pid_t pid, uid_t uid){
    int ret;

    // Check level number
    if(level < 0 || level >= MAX_LV){
        return -2;
    }

    spin_lock(&tag_lock);

    // Check tag service
    ret = check_tag(tags[desc], uid);

    if(ret != 0){
        spin_unlock(&tag_lock);
        return ret;
    }

    tags[desc]->busy = 1;
    spin_unlock(&tag_lock);

    ret = update_level(tags[desc]->lv_head, tags[desc]->lv_lock, level, pid);
    if( ret == -1){
        // If level doesn't already exist add new level
        if(insert_level(tags[desc]->lv_head, tags[desc]->lv_lock, level, pid) == -1){
            ret = -1;
        }
        else{
            ret = 0;
        }
    }

    spin_lock(&tag_lock);
    tags[desc]->busy = 0;
    spin_unlock(&tag_lock);

    return ret;
}

/* Wakes up all threads waiting for the message from that level from that tag service
 *
 * desc = descriptor of the tag
 * level = level number
 * uid = user id for permission check
 *
*/
int wakeup_tag_level(int desc, int level, uid_t uid){
    int ret;

    spin_lock(&tag_lock);

    // Check tag service
    ret = check_tag(tags[desc], uid);
    if(ret != 0){
        spin_unlock(&tag_lock);
        return ret;
    }

    tags[desc]->busy = 1;
    spin_unlock(&tag_lock);

    ret = wakeup_threads(tags[desc]->lv_head, tags[desc]->lv_lock, level);

    spin_lock(&tag_lock);
    tags[desc]->busy = 0;
    spin_unlock(&tag_lock);

    return ret;
}

/* Removes all tags in the list */
void cleanup_tags(void){
    int i;

    for(i=0; i<MAX_TAGS; i++){

        if(tags[i] != NULL){

            spin_lock(&tag_lock);
            tags[i]->busy = 1;
            spin_unlock(&tag_lock);

            wakeup_threads(tags[i]->lv_head, tags[i]->lv_lock, -1); // Wakeup all threads
            cleanup_levels(tags[i]->lv_head, tags[i]->lv_lock, 1);  // Cleanup all levels

            spin_lock(&tag_lock);
            kfree(tags[i]); // Reclaim space
            tags[i] = NULL;
            spin_unlock(&tag_lock);

            printk("%s: Tag service %d removed\n", MODNAME, i);
        }

    }

    printk("%s: All tag services have been removed\n", MODNAME);
}


// System calls' body --------------------------------------------------------------------------------------------------

int tag_get(int key, int command, int permission) {
    int desc, private;
    uid_t perm;

    private = 1;
    perm = current_uid().val;

    printk("%s: tag_get called with params %d - %d - %d\n", MODNAME, key, command, permission);

    if(command == CREATE){

        // Valid key values can only be integer numbers >= 1
        if(key < 1){
            printk("%s: Invalid key value %d, must be an integer >= 1\n", MODNAME, key);
            return -EINVAL;
        }

        if(key != IPC_PRIVATE) private = 0;

        // Try to insert new level
        desc = insert_tag(key, private, perm);

        if(desc < 0){
            if(desc == -1){
                printk("%s: Maximum number of tag services reached\n", MODNAME);
            }
            else if(desc == -2){
                printk("%s: Tag service with key %d already exists\n", MODNAME, key);
            }
            else{
                printk("%s: Unable to create new tag service\n", MODNAME);
            }

            return -1;
        }

        printk("%s: New tag service %d created\n", MODNAME, desc);
        return desc;
    }
    else if (command == OPEN){

        desc = search_tag(key, perm);

        if(desc == -1) {
            printk("%s: Tag service with key %d not found\n", MODNAME, key);
        }
        else if(desc == -2){
            printk("%s: Tag service with key %d it's private and cannot be opened\n", MODNAME, key);
            desc = -1;
        }
        else if(desc == -3){
            printk("%s: Tag service with key %d cannot be opened by user with uid %d\n", MODNAME, key, current_uid().val);
        }
        else {
            printk("%s: Tag service %d with key %d opened by process %d\n", MODNAME, desc, key, current->pid);
        }

        return desc;
    }

    printk("%s: Wrong command %d, must be either %d (create) or %d (open)\n", MODNAME, command, CREATE, OPEN);
    return -EINVAL;
}


int tag_send(int tag, int level, char *buffer, size_t size){
    int ret;
    char *copied;

    // Check message's size
    if(size > MAX_SIZE){
        printk("%s: Maximum size exceeded\n", MODNAME);
        return -1;
    }

    copied = (char *)kmalloc(size*sizeof(char), GFP_KERNEL);

    // Check if copy was successful
    ret = copy_from_user((char*)copied,(char*)buffer,size);
    if( ret < 0){
        printk("%s: Error copying message from user space\n",MODNAME);
        return -1;
    }

    printk("%s: sys_tag_send called with params %d - %d - %s - %zu\n", MODNAME, tag, level, copied, size);

    // MODIFICARE PER INSIERE IL NUOVO MESSAGGIO DA INVIARE


    // Check if tag service or level exists
    if(wakeup_tag_level(tag, level, current_uid().val) == -1){
        printk("%s: Unable to send message, tag service %d or level %d doesn't exist\n", MODNAME, tag, level);
        return -ENODATA;
    }

    return 0;
}


int tag_receive(int tag, int level, char *buffer, size_t size){

    printk("%s: sys_tag_receive called with params %d - %d - %zu\n", MODNAME, tag, level, size);

    // Check message's size
    if(size > MAX_SIZE){
        printk("%s: Maximum size exceeded\n", MODNAME);
        return -1;
    }

    // Check if tag service exists
    if(add_pid_to_tag_level(tag, level, current->pid, current_uid().val) == -1) {
        printk("%s: Tag service with descriptor %d doesn't exist\n", MODNAME, tag);
        return -ENODATA;
    }

    // Block until message is received

    return 0;
}


int tag_ctl(int tag, int command){
    int ret;

    printk("%s: sys_tag_ctl called with params %d - %d\n", MODNAME, tag, command);

    if(command == AWAKE_ALL){
        ret = wakeup_tag_level(tag, -1, current_uid().val);

        // Check if no service has descriptor tag
        if(ret == -1){
            printk("%s: Unable to awake all threads, tag service %d doesn't exist\n", MODNAME, tag);
            return -ENODATA;
        }

        printk("%s: All threads awakened for service %d\n", MODNAME, tag);
        return 0;
    }
    else if(command == REMOVE){

        ret = delete_tag(tag, current_uid().val);

        // Check if no service has descriptor tag
        if(ret == -1){
            printk("%s: Unable to remove tag service with descriptor %d, it doesn't exist\n", MODNAME, tag);
            return -ENODATA;
        }
       // Check if threads are waiting
        else if( ret == -2){
            printk("%s: Unable to remove tag service with descriptor %d, threads are waiting for messages\n", MODNAME, tag);
            return -1;
        }

        printk("%s: Tag service %d removed\n", MODNAME, tag);
        return 0;
    }

    printk("%s: Wrong command %d, must be either %d (awake all) or %d (remove)\n", MODNAME, command, AWAKE_ALL, REMOVE);
    return -EINVAL;
}