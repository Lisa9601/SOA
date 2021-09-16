int search_tag(int key);
int open_tag(int key, uid_t perm);
int insert_tag(int key, int private, uid_t uid);
int delete_tag(int desc, uid_t uid);
int wait_tag_message(int desc, int level, uid_t uid, char *message, size_t size);
int wakeup_tag_level(int desc, int level, uid_t uid, char *message);
void cleanup_tags(void);
int tag_info(char *buffer);