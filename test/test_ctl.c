/* ---------------------------------------------------------------------------------------------------------------------
 TEST TAG CTL
---------------------------------------------------------------------------------------------------------------------- */

#include "./test.h"
#include "../config.h"

#define RECVS 5

int main(void){
    int i, num, created, desc, uid, threads;
    pthread_t tids[RECVS];
    struct info_t *info[RECVS];

    uid = (int)getuid();

    created = 0;

    // Creating tags
    for(i=0; i<MAX_TAGS; i++){
        if(syscall(TAG_GET, i, CREATE, uid) >= 0){
            created ++;
        }
    }


// Tag deletion test ---------------------------------------------------------------------------------------------------

    printf("\nTesting tag deletion ...                                   ");

    num = 0;

    for(i=0; i<MAX_TAGS; i++){
        if(syscall(TAG_CTL, i, REMOVE) >= 0){
            num ++;
        }
    }

    printf("\t%d/%d tags removed\n", num, created);

    // Create new tag
    desc = syscall(TAG_GET, 0, CREATE, uid);

    for(i=0; i<RECVS; i++){
        info[i] = (struct info_t *)malloc(sizeof(struct info_t));
        info[i]->tag = desc;
        info[i]->lv = 1;
        info[i]->message = NULL;
        info[i]->ret = -1;
    }

    threads = 0;

    // Create receivers
    for (i=0; i<RECVS; i++){
        if(pthread_create(&tids[i], NULL, receiver, (void *)info[i]) == 0) threads++;
    }

    printf("\nTesting tag deletion with waiting threads...               ");

    sleep(RECVS/2);

    syscall(TAG_CTL, desc, REMOVE) < 0 ? printf("\t0/1 tags removed\n") : printf("\t1/1 tags removed\n");

// Tag awakening test --------------------------------------------------------------------------------------------------

    printf("\nTesting awakening tag...                                   ");

    syscall(TAG_CTL, desc, AWAKE_ALL);

    num = 0;

    for(i=0; i<RECVS; i++){
        pthread_join(tids[i], NULL);
        if(info[i]->ret >= 0) num++;
    }

    printf("\t%d/%d tags awakened\n", num, threads);

    // Remove tag
    syscall(TAG_CTL, 0, REMOVE);

    // Reclaim space
    for(i=0; i<RECVS; i++){
        free(info[i]->message);
        free(info[i]);
    }
}