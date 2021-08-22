/* ---------------------------------------------------------------------------------------------------------------------
RCU LEVEL LIST

 Detailed info on what this module does can be found in the README.md file.
--------------------------------------------------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include "../include/level.h"
#include "../config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lisa Trombetti <lisa.trombetti96@gmail.com>");
MODULE_DESCRIPTION("RCU LEVEL LIST");

#define MODNAME "RCU LEVEL LIST"


struct level_t {
    struct list_head list;

    int num;                        // Level number
    pid_t pids[MAX_THREADS];        // Pointer to array of pids
    int threads;                    // Current number of threads waiting
};

/* Insert a new level
 *
 * lv_head = head of the list in which the new level will be added
 * lock = list write lock
 * num = level number
 * pid = process id to add to the array of waiting processes
 *
 */
int insert_level(struct list_head *lv_head, spinlock_t lock, int num, pid_t pid){
    struct level_t *new;

    new = (struct level_t *)kmalloc(sizeof(struct level_t), GFP_ATOMIC);

    // Check if level was allocated
    if(new == NULL) return -1;

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
int update_level(struct list_head *lv_head, spinlock_t lock, int num, pid_t pid){
    int i;
    struct level_t *p, *old, *new;

    rcu_read_lock();
    list_for_each_entry_rcu(p, lv_head, list) {

        // Level found
        if(p->num == num){

            new = (struct level_t *)kmalloc(sizeof(struct level_t), GFP_ATOMIC);

            // Check if level was allocated
            if(new == NULL){
                rcu_read_unlock();
                return -2;
            }

            /*
            new->num = num;
            new->threads = p->threads + 1;
            memset(new->pids, 0, sizeof(new->pids));

            // Copy array
            for(i=0; i< p->threads; i++){
                if(p->pids[i] == 0) break;
                new->pids[i] = p->pids[i];
            }
            */

            *new = *p;
            new->pids[new->threads] = pid; // Add new pid
            new->threads++;

                /*
                old = rcu_dereference_protected(p, lockdep_is_held(&lock));
                *new = *old;
                new->pids[new->threads] = pid; // Add new pid
                new->threads = new->threads + 1; // Update number of waiting threads

                rcu_assign_pointer(p, new); // Replace with new level

                spin_unlock(&lock);
                rcu_read_unlock();

                kfree_rcu(old, rcu); // Reclaim space

                */
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

/* Delete a level after removing it from the list
 *
 * level = level to be removed
 * lock = level list write lock
 *
 */
int delete_level(struct level_t *level, spinlock_t lock){

    // Check if there's no level
    if(!level){
        return -1;
    }

    spin_lock(&lock);
    list_del_rcu(&level->list); // Remove level
    spin_unlock(&lock);

    synchronize_rcu();
    kfree(level); // Reclaim space

    return 0;
}

/* Replace a level with a new empty one
 *
 * level = level to be replaced
 * lock = level list write lock
 *
 */
int replace_level(struct level_t *level, spinlock_t lock){
    struct level_t *new;

    new = (struct level_t *)kmalloc(sizeof(struct level_t), GFP_ATOMIC);

    // Check if level was allocated
    if(new == NULL){
        return -1;
    }

    *new = *level;
    memset(new->pids, 0, sizeof(new->pids));
    new->threads = 0;

    spin_lock(&lock);
    list_replace_rcu(&level->list, &new->list); // Replace with new level
    spin_unlock(&lock);

    return 0;
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
    struct level_t *p, *new;

    rcu_read_lock();

    // Wakeup all levels in the list
    if(num == -1 ){
        list_for_each_entry(p, lv_head, list){
            // Wake up waiting threads
            for(i=0; i<p->threads; i++){

                printk("Level %d - Woken up process %d\n", p->num, p->pids[i]);

            }

            if(replace_level(p, lock) < 0){
                rcu_read_unlock();
                return -2;
            }

            rcu_read_unlock();
            synchronize_rcu();
            kfree(p); // Reclaim space
            rcu_read_lock();
        }

        rcu_read_unlock();
        return 0;

    }

    list_for_each_entry(p, lv_head, list){

        // Level found
        if(p->num == num){
            // Wake up waiting threads
            for(i=0; i<p->threads; i++){

                printk("Woken up process %d\n", p->pids[i]);

            }

            if(replace_level(p, lock) < 0){
                rcu_read_unlock();
                return -2;
            }

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
 * force = it's set to 1 if waiting threads don't matter
 *
 */
int cleanup_levels(struct list_head *lv_head, spinlock_t lock, int force){
    struct level_t *p;

    spin_lock(&lock);

    list_for_each_entry(p, lv_head, list){

        if(force !=1 && p->threads > 0){
            spin_unlock(&lock);
            return -1;
        }

        list_del_rcu(&p->list); // Remove element
        synchronize_rcu();
        kfree(p); // Reclaim space
    }

    spin_unlock(&lock);
    return 0;
}