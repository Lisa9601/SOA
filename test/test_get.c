/* ---------------------------------------------------------------------------------------------------------------------
 TEST TAG GET
---------------------------------------------------------------------------------------------------------------------- */

#include "./test.h"
#include "../config.h"


int main(void){
    int i, num, num_all, uid;

    uid = (int)getuid();

// Tag creation test ---------------------------------------------------------------------------------------------------

    printf("\nTesting tag creation with slots available...               ");

    num = 0;
    num_all = 0;

    // Creating half with user uid
    for(i=0; i<MAX_TAGS/2; i++){
        if(syscall(TAG_GET, i, CREATE, uid) >= 0){
            num ++;
        }
    }

    // Creating half open to all users
    for(i=MAX_TAGS/2; i<MAX_TAGS; i++){
        if(syscall(TAG_GET, i, CREATE, -1) >= 0){
            num_all ++;
        }
    }

    printf("\t%d/%d created with user %d --- %d/%d created open to all\n", num, MAX_TAGS, uid, num_all, MAX_TAGS);

    printf("\nTesting tag creation with already existing tags...         ");

    num = 0;

    for(i=0; i<MAX_TAGS; i++){
        if(syscall(TAG_GET, i, CREATE, uid) >= 0){
            num ++;
        }
    }

    printf("\t%d/%d tags created\n", num, MAX_TAGS);

    printf("\nTesting tag creation with max number of services reached...");

    num = 0;

    for(i=MAX_TAGS; i<MAX_TAGS*2; i++){
        if(syscall(TAG_GET, i, CREATE, uid) >= 0){
            num ++;
        }
    }

    printf("\t%d/%d tags created\n", num, MAX_TAGS);


// Tag opening test ----------------------------------------------------------------------------------------------------

    printf("\nTesting opening tags with permission...                    ");
    num = 0;

    for(i=0; i<MAX_TAGS; i++){
        if(syscall(TAG_GET, i, OPEN, uid) >= 0){
            num ++;
        }
    }

    printf("\t%d/%d tags opened\n", num, MAX_TAGS);

    printf("\nTesting opening tags without permission...                 ");

    num = 0;

    for(i=0; i<MAX_TAGS; i++){
        if(syscall(TAG_GET, i, OPEN, uid+1) >= 0){
            num ++;
        }
    }

    printf("\t%d/%d tags opened\n", num, MAX_TAGS);

// ---------------------------------------------------------------------------------------------------------------------

    // Removing all tags
    for(i=0; i<MAX_TAGS; i++){
        syscall(TAG_CTL, i, REMOVE);
    }
}