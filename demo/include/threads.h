// System call numbers
#define TAG_GET 134
#define TAG_SEND 174
#define TAG_RECEIVE 182
#define TAG_CTL 183

// Command numbers
#define CREATE 1
#define OPEN 2
#define AWAKE_ALL 3
#define REMOVE 4

void *sender(void *arg);
void *receiver(void *arg);
