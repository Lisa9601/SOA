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

static int desc_list[MAX_TAGS] = {0};   // List of descriptors
static int start = 0;                   // Where to start searching for an available descriptor
static spinlock_t desc_lock;            // Descriptor list lock

// LEVELS --------------------------------------------------------------------------------------------------------------

struct level_t {
    struct list_head list;

    int num;                        // Level number
    pid_t pids[MAX_THREADS];        // Pointer to array of pids
    int threads;                    // Current number of threads waiting
};

/* Insert a new level
 *
 * lv_head = head of the list in which the new level will be inserted
 * num = level number
 * pid = process id to add to the array of waiting processes
 *
 */
int insert_level(struct list_head *lv_head, spinlock_t lock, int num, pid_t pid){
    struct level_t *new;

    new = (struct level_t *)kmalloc(sizeof(struct level_t), GFP_ATOMIC);
    new->num = num;
    memset(new->pids, 0, sizeof(new->pids));
    new->pids[0] = pid; // Add pid to list
    new->threads = 1;

    spin_lock(&lock);
    list_add_tail_rcu(&(new->list), lv_head); // Add at the tail of the list
    spin_unlock(&lock);

    return 0;
}

/* Update a level
 *
 * lv_head = head of the list where the level should be searched
 * lock = list write lock
 * num = level number
 * pid = process id to add to the array of waiting processes
 *
 */
int update_level(struct list_head *lv_head, spinlock_t lock, int num, pid_t pid) {
    int i;
    struct level_t *p;
    struct level_t *new;

    rcu_read_lock();
    list_for_each_entry_rcu(p, lv_head, list) {

        // Level found
        if(p->num == num){
            new = (struct level_t *)kmalloc(sizeof(struct level_t), GFP_ATOMIC);
            new->num = num;
            new->threads = p->threads + 1;
            memset(new->pids, 0, sizeof(new->pids));

            // Copy array
            for(i=0; i< p->threads; i++){
                if(p->pids[i] == 0) break;
                new->pids[i] = p->pids[i];
            }
            new->pids[p->threads] = pid; // Add new pid

            spin_lock(&lock);
            list_replace_rcu(&p->list, &new->list); // Replace with new level
            spin_unlock(&lock);

            rcu_read_unlock();
            synchronize_rcu();
            kfree(p); // Reclaim space

            return 0;
        }
    }

    rcu_read_unlock();
    return -1;
}

/* Delete a level
 *
 * lv_head = head of the list where the level should be searched
 * lock = list write lock
 * num = level number
 *
 */
int delete_level(struct list_head *lv_head, spinlock_t lock, int num){
    struct level_t *p;

    rcu_read_lock();
    list_for_each_entry(p, lv_head, list){

        // Level found
        if(p->num == num){
            spin_lock(&lock);
            list_del_rcu(&p->list); // Remove element
            spin_unlock(&lock);

            rcu_read_unlock();
            synchronize_rcu();
            kfree(p); // Reclaim space
            return 0;
        }
    }

    rcu_read_unlock();
    return -1;
}

/* Wakes up all threads waiting for the message from that level
 *
 * lv_head = head of the list where to search the level
 * lock = list write lock
 * num = level number
 *
 */
int wakeup_threads(struct list_head *lv_head, spinlock_t lock, int num){
    int i;
    struct level_t *p;
    struct level_t *new;

    rcu_read_lock();

    if(num == -1 ){
        list_for_each_entry(p, lv_head, list){
            // Wake up waiting threads
            for(i=0; i<p->threads; i++){

                // SIGH

            }

            // Replace with new level with zero threads waiting
            new = (struct level_t *)kmalloc(sizeof(struct level_t), GFP_ATOMIC);
            new->num = num;
            new->threads = 0;
            memset(new->pids, 0, sizeof(new->pids));

            spin_lock(&lock);
            list_replace_rcu(&p->list, &new->list); // Replace with new level
            spin_unlock(&lock);

            synchronize_rcu();
            kfree(p); // Reclaim space
        }

        rcu_read_unlock();
        return 0;

    }
    list_for_each_entry(p, lv_head, list){

        // Level found
        if(p->num == num){
            // Wake up waiting threads
            for(i=0; i<p->threads; i++){

                printk("%s: Woken up process %d\n", MODNAME, p->pids[i]);

            }

            // Replace with new level with zero threads waiting
            new = (struct level_t *)kmalloc(sizeof(struct level_t), GFP_ATOMIC);
            new->num = num;
            new->threads = 0;
            memset(new->pids, 0, sizeof(new->pids));

            spin_lock(&lock);
            list_replace_rcu(&p->list, &new->list); // Replace with new level
            spin_unlock(&lock);

            rcu_read_unlock();
            synchronize_rcu();
            kfree(p); // Reclaim space

            return 0;
        }
    }

    rcu_read_unlock();
    return -1;
}

/* Removes all levels in the list
 *
 * lv_head = head of the list
 * lock = list write lock
 *
 */
int cleanup_levels(struct list_head *lv_head, spinlock_t lock){

    struct level_t *p;

    spin_lock(&lock);
    list_for_each_entry(p, lv_head, list){

        if(p->threads > 0){
            return -1;
        }
        list_del_rcu(&p->list); // Remove element
        synchronize_rcu();
        kfree(p); // Reclaim space

    }
    spin_unlock(&lock);
    return 0;
}

// TAGS ----------------------------------------------------------------------------------------------------------------

struct tag_t {
    struct list_head list;

    int key;                // Key
    int desc;               // Descriptor
    int private;            // If service it's private this value is set to 1
    uid_t perm;             // User id used for permission check

    struct list_head *lv_head; // List of levels
    spinlock_t lv_lock;
};


static LIST_HEAD(head);     // List of tags
static spinlock_t list_lock;


/* Search a tag by key
 *
 * key = key to be searched
 *
 */
int search_tag_by_key(int key) {
    struct tag_t *p;

    rcu_read_lock();
    list_for_each_entry_rcu(p, &head, list) {
        // Key found
        if(p->key == key){
            rcu_read_unlock();

            // Checks if service is private
            if(p->private == 1) return -2;

            if(p->perm != -1 && p->perm != current_uid().val) return -3;

            return p->desc;
        }
    }
    rcu_read_unlock();
    return -1;
}

/* Search a tag by descriptor
 *
 * desc = descriptor to be searched
 *
 */
int search_tag_by_desc(int desc){
    struct tag_t *p;

    rcu_read_lock();
    list_for_each_entry_rcu(p, &head, list) {

        // Descriptor found
        if(p->desc == desc){
            rcu_read_unlock();

            if(p->perm != -1 && p->perm != current_uid().val) return -3;

            return 1;
        }
    }
    rcu_read_unlock();
    return -1;
}

/* Insert a new tag
 *
 * key = tag's key
 * desc = tag's descriptor
 * private = whether the service should be private or not (private=1, not_private=0)
 *
 */

int insert_tag(int key, int desc, int private, uid_t perm){
    struct tag_t *new;

    // Initializing list of levels
    struct list_head *lv_head = (struct list_head *)kmalloc(sizeof(struct list_head), GFP_ATOMIC);
    INIT_LIST_HEAD(lv_head);

    new = (struct tag_t *)kmalloc(sizeof(struct tag_t), GFP_ATOMIC);
    new->key = key;
    new->desc = desc;
    new->private = private;
    new->perm = perm;
    new->lv_head = lv_head;

    spin_lock(&list_lock);
    list_add_tail_rcu(&(new->list), &head); // Add at the tail of the list
    spin_unlock(&list_lock);

    return 0;
}

/* Deletes a tag
 *
 * desc = descriptor of the tag service to be removed
 *
 */
int delete_tag(int desc){
    struct tag_t *p;

    rcu_read_lock();
    list_for_each_entry(p, &head, list){

        // Descriptor found
        if(p->desc == desc){

            if(p->perm != -1 && p->perm != current_uid().val){
                rcu_read_unlock();
                return -3;
            }

            // Check if some thread is waiting
            if(cleanup_levels(p->lv_head, p->lv_lock) == -1){
                rcu_read_unlock();
                return -2;
            }
            spin_lock(&list_lock);
            list_del_rcu(&p->list); // Remove element
            spin_unlock(&list_lock);

            rcu_read_unlock();
            synchronize_rcu();
            kfree(p); // Reclaim space
            return 0;
        }
    }

    rcu_read_unlock();
    return -1;
}

/* Update a level from a specified tag service
 *
 * desc = descriptor of the tag
 * level = level number
 * pid = process id to add to the array of waiting threads
 *
 */
int update_tag_level(int desc, int level, pid_t pid){
    struct tag_t *p;

    rcu_read_lock();
    list_for_each_entry_rcu(p, &head, list) {

        // Descriptor found
        if(p->desc == desc){

            if(p->perm != -1 && p->perm != current_uid().val){
                rcu_read_unlock();
                return -3;
            }

            // Check if level doesn't exist
            if(update_level(p->lv_head, p->lv_lock, level, pid) == -1){
                insert_level(p->lv_head, p->lv_lock, level, pid); // Insert new level
            }

            rcu_read_unlock();
            return 0;
        }
    }

    rcu_read_unlock();
    return -1;
}

/* Wakes up all threads waiting for the message from that level from that tag service
 *
 * desc = descriptor of the tag
 * level = level number
 *
 */
int wakeup_tag_level(int desc, int level){
    struct tag_t *p;

    rcu_read_lock();
    list_for_each_entry_rcu(p, &head, list) {

        // Descriptor found
        if(p->desc == desc){

            rcu_read_unlock();

            if(p->perm != -1 && p->perm != current_uid().val) return -3;

            return wakeup_threads(p->lv_head, p->lv_lock, level);
        }
    }

    rcu_read_unlock();
    return -1;
}

/* Removes all tags in the list */
void cleanup_tags(void){
    struct tag_t *p;

    spin_lock(&list_lock);
    list_for_each_entry(p, &head, list){

        cleanup_levels(p->lv_head, p->lv_lock); // Cleanup all levels
        list_del_rcu(&p->list); // Remove element
        synchronize_rcu();
        kfree(p); // Reclaim space

    }
    spin_unlock(&list_lock);

    printk("%s: All tag services removed\n", MODNAME);
}

// System calls' body --------------------------------------------------------------------------------------------------

int tag_get(int key, int command, int permission) {
    int desc, ret, i;
    int private = 1;
    uid_t perm = -1;

    printk("%s: tag_get called with params %d - %d - %d\n", MODNAME, key, command, permission);

    if(command == CREATE){

        // Valid key values can only be integer numbers >= 1
        if(key < 1){
            printk("%s: Invalid key value %d, must be an integer >= 1\n", MODNAME, key);
            return -EINVAL;
        }

        // Check if service with specified key already exists
        if(search_tag_by_key(key) != -1){
            printk("%s: Tag service with key %d already exists\n", MODNAME, key);
            return -1;
        }

        // Search for a valid descriptor
        spin_lock(&desc_lock);
        for(i=0; i<MAX_TAGS; i++) {
            desc = (start + i)%MAX_TAGS;

            // Empty slot found!
            if(desc_list[desc] == 0){
                desc_list[desc] = 1;
                start = (desc + 1)%MAX_TAGS;
                break;
            }

        }
        spin_unlock(&desc_lock);

        // Check if maximum number of services has been reached
        if(i >= MAX_TAGS){
            printk("%s: Unable to create new service, maximum number reached %d\n", MODNAME, MAX_TAGS);
            return -1;
        }

        if(key != IPC_PRIVATE) private = 0;
        if(permission == 1) perm = current_uid().val;

        ret = insert_tag(key, desc, private, perm);

        if (ret != 0){
            // Fai roba
            printk("LOL WHAT ?!");
        }

        printk("%s: New tag service %d created\n", MODNAME, desc);

        return desc;
    }
    else if (command == OPEN){

        desc = search_tag_by_key(key);

        if(desc == -1) {
            printk("%s: Tag service with key %d not found\n", MODNAME, key);
        }
        else if(desc == -2){
            printk("%s: Tag service with key %d it's private and cannot be opened\n", MODNAME, key);
            desc = -1;
        }
        else if(desc == -3){
            printk("%s: Tag service with key %d cannot be used by user with uid %d\n", MODNAME, key, current_uid().val);
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
    ret = copy_from_user((char*)copied,(char*)buffer,size);     // Copy from user space

    // Check if copy was successful
    if(ret != 0){
        printk("%s: Error copying message from user space\n",MODNAME);
        return -1;
    }

    printk("%s: sys_tag_send called with params %d - %d - %s - %zu\n", MODNAME, tag, level, copied, size);

    // MODIFICARE PER INSIERE IL NUOVO MESSAGGIO DA INVIARE

    // Check if tag service or level exists
    if(wakeup_tag_level(tag, level) == -1){
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
    if(update_tag_level(tag, level, current->pid) == -1) {
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
        // Awake all threads

        ret = wakeup_tag_level(tag, -1);

        // Check if no service has descriptor tag
        if(ret == -1){
            printk("%s: Unable to awake all threads, tag service %d doesn't exist\n", MODNAME, tag);
            return -ENODATA;
        }

        printk("%s: All threads awakened for service %d\n", MODNAME, tag);
        return 0;
    }
    else if(command == REMOVE){

        ret = delete_tag(tag);

        // Check if no service has descriptor tag
        if(ret == -1){
            printk("%s: Unable to remove tag service with descriptor %d, it doesn't exist\n", MODNAME, tag);
            return -ENODATA;
        }
        else if( ret == -2){
            printk("%s: Unable to remove tag service with descriptor %d, threads are waiting for messages\n", MODNAME, tag);
            return -1;
        }

        spin_lock(&desc_lock);
        desc_list[tag] = 0;     // Free descriptor
        start = tag;            // Start searching from this descriptor
        spin_unlock(&desc_lock);

        printk("%s: Tag service %d removed\n", MODNAME, tag);
        return 0;
    }

    printk("%s: Wrong command %d, must be either %d (awake all) or %d (remove)\n", MODNAME, command, AWAKE_ALL, REMOVE);
    return -EINVAL;
}