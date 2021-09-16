struct tag_t{

    int key;                    // Key
    int private;                // If service it's private this value is set to 1
    uid_t perm;                 // User id for permission check

    int used;                   // If service it's being used this value is > 0
    int removing;               // If service it's being removed this value is set to 1

    struct list_head *lv_head;  // List of levels
    spinlock_t lv_lock;         // Level list write lock

};

struct level_t {

    struct list_head list;

    int num;                    // Level number
    char *message;              // Message to send
    int threads;                // Number of processes currently waiting for the message
    wait_queue_head_t *wq;      // Head of wait queue

};

