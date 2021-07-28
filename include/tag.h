asmlinkage int sys_tag_get(int key, int command, int permission);
asmlinkage int sys_tag_send(int tag, int level, char *buffer, size_t size);
asmlinkage int sys_tag_receive(int tag, int level, char *buffer, size_t size);
asmlinkage int sys_tag_ctl(int tag, int command);
