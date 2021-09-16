/* ---------------------------------------------------------------------------------------------------------------------
 TAG SERVICE

 This module implements a tag service providing functions to create, open, read, write and delete it.
--------------------------------------------------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/cred.h>
#include <linux/string.h>
#include "../include/tag.h"
#include "../include/level.h"
#include "../include/struct.h"
#include "../config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lisa Trombetti <lisa.trombetti96@gmail.com>");
MODULE_DESCRIPTION("TAG SERVICE");

#define MODNAME "TAG SERVICE"


static struct tag_t* tags[MAX_TAGS];   // List of tags
static int start;                      // Where to start searching for an available slot
static spinlock_t tag_lock;            // Tag list write lock


/* Search tag by key
 *
 * key = key to be searched
 *
 */
int search_tag(int key) {
    int i;

    for(i=0; i<MAX_TAGS; i++) {
        // Key found
        if(tags[i] != NULL && tags[i]->key == key) return i;
    }

    return -1;
}


/* Open tag service
 *
 * key = key to be searched
 * perm = user id for permission checking
 *
 */
int open_tag(int key, uid_t perm) {
    int i;

    spin_lock(&tag_lock);

    for(i=0; i<MAX_TAGS; i++) {
        // Key found
        if(tags[i] != NULL && tags[i]->key == key){

            // Check if service it's private
            if(tags[i]->private == 1){
                printk("%s: Tag service with key %d it's private and cannot be opened\n", MODNAME, key);
                spin_unlock(&tag_lock);
                return -1;
            }

            // Check user permission
            if(tags[i]->perm != -1 && tags[i]->perm != perm){
                printk(KERN_ERR "%s: Tag service with key %d can't be opened by user %du\n", MODNAME, key, perm);
                spin_unlock(&tag_lock);
                return -1;
            }

            spin_unlock(&tag_lock);
            return i;
        }
    }

    printk(KERN_ERR "%s: Tag service with key %d doesn't exist\n", MODNAME, key);
    spin_unlock(&tag_lock);
    return -1;
}


/* Insert a new tag
 *
 * key = tag's key
 * private = whether the service should be private or not
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
            printk(KERN_ERR "%s: Tag service with key %d already exists\n", MODNAME, key);
            spin_unlock(&tag_lock);
            return -1;
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
            printk(KERN_WARNING "%s: Unable to allocate new level list for tag service\n", MODNAME);
            return -ENOMEM;
        }

        INIT_LIST_HEAD(lv_head);

        new = (struct tag_t *)kmalloc(sizeof(struct tag_t), GFP_ATOMIC);

        // Check if new tag was correctly allocated
        if(new == NULL){
            spin_unlock(&tag_lock);
            kfree(lv_head); // Release level list
            printk(KERN_WARNING "%s: Unable to allocate new tag\n", MODNAME);
            return -ENOMEM;
        }

        new->key = key;
        new->private = private;
        new->perm = uid;
        new->used = 0;
        new->removing = 0;
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
 * desc = tag descriptor
 * uid = user id for permission checking
 *
 */
int check_tag(struct tag_t* tag, int desc, uid_t uid){

    if(tag == NULL){
        // Check if tag service it's active
        printk(KERN_ERR "%s: Tag service %d to check doesn't exist\n", MODNAME, desc);
        return -1;
    }
    else if(tag->perm != -1 && tag->perm != uid){
        // Check permission
        printk(KERN_ERR "%s: User %du doesn't have required permissions for tag service %d\n", MODNAME, uid, desc);
        return -1;
    }
    else if(tag->removing == 1){
        // Check if service it's being removed
        printk("%s: Tag service %d to check it's being removed\n", MODNAME, uid);
        return -1;
    }

    tag->used++; // Signal that the service it's being used
    return 0;
}

/* Signals that the tag service isn't being used anymore
 *
 * tag = tag service to check
 * desc = tag descriptor
 *
 */
int uncheck_tag(struct tag_t* tag, int desc){

    if(tag == NULL){
        // Check if tag service it's active
        printk(KERN_ERR "%s: Tag service %d to uncheck doesn't exist\n", MODNAME, desc);
        return -1;
    }

    tag->used--; // Signal that the task on the service has been completed
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
    if(check_tag(tags[desc], desc, uid) < 0){
        spin_unlock(&tag_lock);
        return -1;
    }
    else if(tags[desc]->used > 1){
        printk("%s: Tag service %d it's being used so it can't be removed\n", MODNAME, desc);
        uncheck_tag(tags[desc], desc);
        spin_unlock(&tag_lock);
        return -1;
    }

    tags[desc]->removing = 1; // Signal that tag service will be removed
    spin_unlock(&tag_lock);

    ret = cleanup_levels(tags[desc]->lv_head, tags[desc]->lv_lock);

    spin_lock(&tag_lock);
    uncheck_tag(tags[desc], desc);

    // Check if levels where removed
    if(ret < 0) {
        tags[desc]->removing = 0;
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

/* Add new process to the waiting list for a message from a specific level
 *
 * desc = descriptor of the tag
 * level = level number
 * uid = user id for permission checking
 * message = where to store the message when sent
 * size = message's size
 *
 */
int wait_tag_message(int desc, int level, uid_t uid, char* message, size_t size){
    int ret;

    // Check level number
    if(level < 0 || level >= MAX_LV){
        printk(KERN_ERR "%s: Level number %d it's out of range [0,%d]\n", MODNAME, level, MAX_LV);
        return -EINVAL;
    }

    spin_lock(&tag_lock);

    ret = check_tag(tags[desc], desc, uid);
    if(ret < 0) {
        spin_unlock(&tag_lock);
        return -1;
    }

    spin_unlock(&tag_lock);

    ret = search_level(tags[desc]->lv_head, level);
    if(ret == -1){
        // If level doesn't already exist add new level
        ret = insert_level(tags[desc]->lv_head, tags[desc]->lv_lock, level);
    }

    if(ret == 0){
        printk(KERN_DEBUG "%s: Process %d waiting for message...\n", MODNAME, current->pid);
        ret = wait_for_message(tags[desc]->lv_head, level, message, size); // Wait for message
    }

    spin_lock(&tag_lock);
    uncheck_tag(tags[desc], desc);
    spin_unlock(&tag_lock);

    return ret;
}

/* Wakes up all threads waiting for the message from that level from that tag service
 *
 * desc = descriptor of the tag
 * level = level number, if -1 all levels will be awakened
 * uid = user id for permission check
 * message = message to be sent
 *
*/
int wakeup_tag_level(int desc, int level, uid_t uid, char *message){
    int ret;

    spin_lock(&tag_lock);

    // Check tag service
    ret = check_tag(tags[desc], desc, uid);
    if(ret < 0) {
        spin_unlock(&tag_lock);
        return -1;
    }

    spin_unlock(&tag_lock);

    if(level < 0){
        //Wake up all levels
        ret = wakeup_all(tags[desc]->lv_head, tags[desc]->lv_lock);
    }
    else{
        //Send message to level
        ret = wakeup_level(tags[desc]->lv_head, tags[desc]->lv_lock, level, message);
    }

    spin_lock(&tag_lock);
    uncheck_tag(tags[desc], desc);
    spin_unlock(&tag_lock);

    return ret;
}

/* Removes all tags currently active */
void cleanup_tags(void){
    int i;

    spin_lock(&tag_lock);

    for(i=0; i<MAX_TAGS; i++){

        if(tags[i] != NULL){

            tags[i]->removing = 1;
            spin_unlock(&tag_lock);

            force_cleanup(tags[i]->lv_head, tags[i]->lv_lock);  // Cleanup all levels

            spin_lock(&tag_lock);
            kfree(tags[i]); // Reclaim space
            tags[i] = NULL;

            printk("%s: Tag service %d removed\n", MODNAME, i);
        }

    }

    spin_unlock(&tag_lock);
    printk("%s: All tag services have been removed\n", MODNAME);
}

/* Writes info about the tag services currently active in a buffer
 *
 * buffer = where to write the info
 *
*/
int tag_info(char* buffer){
    struct level_t *p;
    int i, off;

    snprintf(buffer, sizeof(char)*100, "%s\n", " TAG-key   TAG-creator   TAG-level   Waiting-threads "); // Add header

    off = 100;

    for(i=0; i<MAX_TAGS; i++) {
        if(tags[i] != NULL){
            // Active tag service found

            rcu_read_lock();
            list_for_each_entry(p, tags[i]->lv_head, list){
                // Add level info
                snprintf(buffer + off, sizeof(char)*100, " %7d   %11d   %9d   %15d \n", tags[i]->key, tags[i]->perm, p->num, p->threads);
                off += 100;
            }
            rcu_read_unlock();

        }
    }

    return off;
}