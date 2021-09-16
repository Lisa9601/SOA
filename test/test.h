#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>

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

#define BUFF_SIZE 1024


struct info_t{

    int tag;        // tag service descriptor
    int lv;         // level number

    char *message;  // message to be sent or received
    int size;       // message size
    int ret;        // return value

};


/*
 * Simple receiver thread
 *
 * arg = thread's arguments, must be a struct info_t
 *
 */
void *receiver(void *arg){
    char *buffer;
    struct info_t *i = (struct info_t *)arg;

    buffer = (char *)malloc(sizeof(char)*BUFF_SIZE);

    // Check if buffer was allocated
    if(buffer == NULL){
        i->ret = -1;
        perror("Buffer allocation error");
        pthread_exit(NULL);
    }

    i->ret = syscall(TAG_RECEIVE, i->tag, i->lv, buffer, BUFF_SIZE);
    i->message = buffer;

    pthread_exit(NULL);
}
