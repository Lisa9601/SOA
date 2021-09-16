/* ---------------------------------------------------------------------------------------------------------------------
 TEST TAG SEND
---------------------------------------------------------------------------------------------------------------------- */

#include "./test.h"
#include "../config.h"

#define RECVS 5
#define MESSAGE "Sender message"

int main(void){
    int i, num, desc, uid, threads;
    pthread_t tids[RECVS];
    struct info_t *info[RECVS];
    char *message;

    uid = (int)getuid();

    // Create tag service
    if((desc = syscall(TAG_GET, 0, CREATE, uid)) < 0){
        perror("Tag service creation failed");
        return -1;
    }

    // Spawn receivers
    for(i=0; i<RECVS; i++){
        info[i] = (struct info_t *)malloc(sizeof(struct info_t));
        info[i]->tag = desc;
        info[i]->lv = 1;
        info[i]->message = NULL;
        info[i]->ret = -1;
    }

    threads = 0;

    for (i=0; i<RECVS; i++){
        if(pthread_create(&tids[i], NULL, receiver, (void *)info[i]) == 0) threads++;
    }

    // Create new message
    message = (char *)malloc(sizeof(char)*BUFF_SIZE);

// Tag sender test -----------------------------------------------------------------------------------------------------

    printf("\nTesting sending and empty message                       ...");

    sleep(RECVS/2);

    snprintf(message, sizeof(char)*BUFF_SIZE, "%s", "\0");

    syscall(TAG_SEND, desc, 1, message, strlen(message));

    num = 0;

    for(i=0; i<RECVS; i++){
        pthread_join(tids[i], NULL);
        if(info[i]->ret >= 0) num++;
    }

    printf("\t%d/%d tags successfully received the message\n", num, threads);

    // Reset info
    for(i=0; i<RECVS; i++){
        free(info[i]->message);
        info[i]->message = NULL;
    }

    threads = 0;

    for (i=0; i<RECVS; i++){
        if(pthread_create(&tids[i], NULL, receiver, (void *)info[i]) == 0) threads++;
    }

    printf("\nTesting sending message                                 ...");

    sleep(RECVS/2);

    snprintf(message, sizeof(char)*BUFF_SIZE, "%s\n", MESSAGE);

    syscall(TAG_SEND, desc, 1, message, BUFF_SIZE);

    num = 0;

    for(i=0; i<RECVS; i++){
        pthread_join(tids[i], NULL);
        if(info[i]->ret >= 0 && strcmp(info[i]->message, message) == 0) num++;
    }

    printf("\t%d/%d tags successfully received the message\n", num, threads);

    // Remove message
    free(message);

    // Remove tag
    syscall(TAG_CTL, 0, REMOVE);

    // Reclaim space
    for(i=0; i<RECVS; i++){
        free(info[i]->message);
        free(info[i]);
    }
}