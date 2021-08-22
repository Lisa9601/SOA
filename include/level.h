int insert_level(struct list_head *lv_head, spinlock_t lock, int num, pid_t pid);
int update_level(struct list_head *lv_head, spinlock_t lock, int num, pid_t pid);
int wakeup_threads(struct list_head *lv_head, spinlock_t lock, int num);
int cleanup_levels(struct list_head *lv_head, spinlock_t lock, int force);
