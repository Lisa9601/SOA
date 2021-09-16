int insert_level(struct list_head *lv_head, spinlock_t lock, int num);
int search_level(struct list_head *lv_head, int num);
int wait_for_message(struct list_head *lv_head, int num, char *buffer, size_t size);
int wakeup_all(struct list_head *lv_head, spinlock_t lock);
int wakeup_level(struct list_head *lv_head, spinlock_t lock, int num, char * message);
int cleanup_levels(struct list_head *lv_head, spinlock_t lock);
int force_cleanup(struct list_head *lv_head, spinlock_t lock);
