/* ---------------------------------------------------------------------------------------------------------------------
RCU LEVEL LIST

 This module implements an rcu list in which the elements are levels ( see /include/types.h for struct level_t).
--------------------------------------------------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/sched.h>
#include "../include/level.h"
#include "../include/struct.h"
#include "../config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lisa Trombetti <lisa.trombetti96@gmail.com>");
MODULE_DESCRIPTION("RCU LEVEL LIST");

#define MODNAME "RCU LV LIST"


/* Insert a new level
 *
 * lv_head = head of the list in which the new level will be added
 * lock = list write lock
 * num = level number
 *
 */
int insert_level(struct list_head *lv_head, spinlock_t lock, int num){
    struct level_t *new;

    // Allocate new level struct
    new = (struct level_t *)kmalloc(sizeof(struct level_t), GFP_ATOMIC);
    if(new == NULL) {
        printk(KERN_ERR "%s: Unable to allocate new level\n", MODNAME);
        return -ENOMEM;
    }

    new->num = num;
    new->message = NULL;
    new->threads = 0;

    // Initialize wait queue
    new->wq = (wait_queue_head_t *) kmalloc(sizeof(wait_queue_head_t), GFP_ATOMIC);
    if(new->wq == NULL) {
        printk(KERN_ERR "%s: Unable to allocate new wait queue\n", MODNAME);
        kfree(new);
        return -ENOMEM;
    }
    init_waitqueue_head(new->wq);

    spin_lock(&lock);
    list_add_tail_rcu(&(new->list), lv_head); // Add at the tail of the list
    spin_unlock(&lock);

    return 0;
}

/* Searches a level by number
 *
 * lv_head = head of the list where the level should be searched
 * num = level number
 *
 */
int search_level(struct list_head *lv_head, int num){
    struct level_t *p;

    rcu_read_lock();

    list_for_each_entry(p, lv_head, list){
        // Level found
        if(p->num == num){
            rcu_read_unlock();
            return 0;
        }
    }

    rcu_read_unlock();
    return -1;
}

/* Replaces level with an empty one
 *
 * level = level to modify
 * lock = level list write lock
 *
 */
int replace_level(struct level_t *level, spinlock_t lock){
    struct level_t *new;

    // Allocate new level
    new = (struct level_t *)kmalloc(sizeof(struct level_t), GFP_ATOMIC);
    if(new == NULL){
        printk(KERN_ERR "%s: Unable to allocate new level for replacement\n", MODNAME);
        return -ENOMEM;
    }

    new->num = level->num;
    new->message = NULL;
    new->threads = 0;

    // Initialize wait queue
    new->wq = (wait_queue_head_t *) kmalloc(sizeof(wait_queue_head_t), GFP_ATOMIC);
    if(new->wq == NULL) {
        printk(KERN_ERR "%s: Unable to allocate new wait queue for replacement\n", MODNAME);
        kfree(new);
        return -ENOMEM;
    }
    init_waitqueue_head(new->wq);

    spin_lock(&lock);
    list_replace_rcu(&level->list, &new->list); // Replace with new level
    spin_unlock(&lock);

    return 0;
}

/* Wait for a message from the specified level to be delivered
 *
 * lv_head = head of the list where to search the level
 * num = level number
 * buffer = where to copy the message
 * size = buffer's size
 *
 */
int wait_for_message(struct list_head *lv_head, int num, char *buffer, size_t size){
    struct level_t *p;
    int ret;

    rcu_read_lock();
    list_for_each_entry_rcu(p, lv_head, list) {

        // Level found
        if(p->num == num){
            __sync_fetch_and_add(&p->threads,1); // Signal that a new thread is waiting

            ret = wait_event_interruptible(*p->wq, p->message != NULL); // Wait for message

            if(ret == 0){
                // Copy message
                strncpy((char*)buffer, (char*)p->message, min(size, strlen(p->message)));

                __sync_fetch_and_add(&p->threads,-1); // Signal that the message was read

                rcu_read_unlock();
                return 0;
            }

            __sync_fetch_and_add(&p->threads,-1); // Signal that the thread is waiting no more

            rcu_read_unlock();
            printk(KERN_ERR "%s: Process %d woken up by signal\n", MODNAME, current->pid);
            return ret;
        }
    }

    rcu_read_unlock();
    printk(KERN_ERR "%s: Unable to wait for message, level %d doesn't exist\n", MODNAME, num);
    return -1;
}

/* Wakes up all threads waiting in the list
 *
 * lv_head = head of the list where to search the level
 * lock = list write lock
 *
 */
int wakeup_all(struct list_head *lv_head, spinlock_t lock){
    struct level_t *p;
    char *message;

    // Allocate new empty message
    message = (char *)kmalloc(sizeof(char), GFP_ATOMIC);
    if(message == NULL){
        printk(KERN_ERR "%s: Unable to allocate new message to wake up waiting threads\n", MODNAME);
        return -ENOMEM;
    }
    snprintf(message, sizeof(char), "%s", "\n");

    rcu_read_lock();

    list_for_each_entry(p, lv_head, list){

        if(!__sync_bool_compare_and_swap(&p->message, NULL, message)){
            printk(KERN_ERR "%s: Unable to send message to wake up level %d\n", MODNAME, p->num);
            rcu_read_unlock();
            kfree(message);
            return -1;
        }

        wake_up_interruptible(p->wq); // Wake up waiting threads

        // Replace level with an empty one
        if(replace_level(p, lock) < 0){
            rcu_read_unlock();
            kfree(message);
            return -1;
        }

        rcu_read_unlock();
        synchronize_rcu();
        kfree(p->wq); // Reclaim space
        kfree(p);
        rcu_read_lock();
    }

    rcu_read_unlock();
    kfree(message);
    return 0;
}

/* Sends message to all waiting threads from level waking them up
 *
 * lv_head = head of the list where to search the level
 * lock = list write lock
 * num = level number
 * message = message to be sent
 *
 */
int wakeup_level(struct list_head *lv_head, spinlock_t lock, int num, char * message){
    struct level_t *p;

    rcu_read_lock();

    list_for_each_entry(p, lv_head, list){

        // Level found
        if(p->num == num){

            if(!__sync_bool_compare_and_swap(&p->message, NULL, message)){
                printk(KERN_WARNING "%s: Unable to send message to level %d\n", MODNAME, p->num);
                rcu_read_unlock();
                return -1;
            }

            wake_up_interruptible(p->wq); // Wake up waiting threads

            if(replace_level(p, lock) < 0){
                rcu_read_unlock();
                return -1;
            }

            rcu_read_unlock();
            synchronize_rcu();
            kfree(p->wq); // Reclaim space
            kfree(p);

            return 0;
        }
    }

    rcu_read_unlock();
    printk("%s: Level %d doesn't exist, message will be discarded\n", MODNAME, num);
    return 0;
}

/* Removes all levels in the list if no thread is currently waiting
 *
 * lv_head = head of the list
 * lock = list write lock
 *
 */
int cleanup_levels(struct list_head *lv_head, spinlock_t lock){
    struct level_t *p;

    rcu_read_lock();

    list_for_each_entry(p, lv_head, list){

        if(p->threads > 0){
            rcu_read_unlock();
            printk(KERN_ERR "%s: Unable to remove level %d, threads are still waiting for message\n", MODNAME, p->num);
            return -1;
        }

        spin_lock(&lock);
        list_del_rcu(&p->list); // Remove element
        spin_unlock(&lock);

        rcu_read_unlock();
        synchronize_rcu();
        kfree(p); // Reclaim space
        rcu_read_lock();
    }

    rcu_read_unlock();
    return 0;
}

/* Forcefully removes all levels in the list
 *
 * lv_head = head of the list
 * lock = list write lock
 *
 */
int force_cleanup(struct list_head *lv_head, spinlock_t lock){
    struct level_t *p;

    rcu_read_lock();

    list_for_each_entry(p, lv_head, list){

        spin_lock(&lock);
        list_del_rcu(&p->list); // Remove element
        spin_unlock(&lock);

        rcu_read_unlock();
        synchronize_rcu();
        kfree(p); // Reclaim space
        rcu_read_lock();
    }

    rcu_read_unlock();
    return 0;
}