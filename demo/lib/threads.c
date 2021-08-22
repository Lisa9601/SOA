#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/syscall.h>
#include "./include/threads.h"


typedef struct{

    int num;        // Thread's number
    int key;        // Tag service key
    int perm;       // Permission

} info;

void *sender(void *arg){
    int tag, num, key, perm;

    info *i = (info *)arg;
    num = i->num;
    key = i->key;
    perm = i->perm;

    printf("SENDER %d - key %d : Starting ...\n", num, key);

    tag = syscall(TAG_GET, key, CREATE, perm);

    if (tag == -1) perror("Cannot create service!");

    printf("SENDER %d - key %d : Shutting down ...\n", num, key);

    pthread_exit(NULL);
}


void *receiver(void *arg){
    int tag, num, key, perm;

    info *i = (info *)arg;
    num = i->num;
    key = i->key;
    perm = i->perm;

    printf("RECEIVER %d - key %d : Starting ...\n", num, key);

    tag = syscall(TAG_GET, key, OPEN, perm);

    if (tag == -1) perror("Cannot create service!");

    printf("RECEIVER %d - key %d : Shutting down ...\n", num, key);

    pthread_exit(NULL);
}