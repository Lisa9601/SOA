/* ---------------------------------------------------------------------------------------------------------------------
 DEMO

 A simple demo of the usage of the system calls defined in /lib/service.c
------------------- -------------------------------------------------------------------------------------------------- */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>

// System calls numbers
#define TAG_GET 134
#define TAG_SEND 174
#define TAG_RECEIVE 182
#define TAG_CTL 183


void show_help();
void print_error(char *string);


int main(void){

    char *command, *choice1, *choice2, *choice3, *buffer;
    char *s1, *s2, *s3;
    int p1, p2, p3, ret;
    int uid;

    uid = (int)getuid();

    system("clear");	// Cleans the terminal

    printf("\n ***** Welcome to SOA demo *****\n\n");

    show_help();

    while(1){

        printf("[\033[94mSOA demo\033[0m]~$ ");

        // Get command from user
        ret = scanf("%m[^\n]s", &command);
        getchar();

        // Checking scanf for errors
        if(ret < 0){
            print_error("Error reading from stdin");
            continue;
        }
        else if(ret == 0){
            continue;
        }

        choice1 = strtok(command, "'");
        choice2 = strtok(NULL, "'");

        choice3 = strtok(choice1, " ");

        if(strcmp(choice3, "get") == 0){

            s1 = strtok(NULL, " ");

            if(s1 == NULL) {
                print_error("Wrong number of parameters");
                continue;
            }

            p1 = atoi(s1);

            if(p1 == 0){
                // Private tag service
                ret = syscall(TAG_GET, IPC_PRIVATE, 1, uid);
            }
            else{
                ret = syscall(TAG_GET, p1, 1, uid);
            }

            if(ret < 0){
                print_error("Error");
            }
            else{
                printf("tag descriptor : %d\n",ret);
            }
        }
        else if(strcmp(choice3, "open") == 0){

            s1 = strtok(NULL, " ");

            if(s1 == NULL) {
                print_error("Wrong number of parameters");
                continue;
            }

            p1 = atoi(s1);

            ret = syscall(TAG_GET, p1, 2, uid);

            if(ret < 0){
                print_error("Error");
            }
            else{
                printf("tag descriptor : %d\n",ret);
            }
        }
        else if(strcmp(choice3, "send") == 0){

            s1 = strtok(NULL, " ");
            s2 = strtok(NULL, " ");

            if(s1 == NULL || s2 == NULL) {
                print_error("Wrong number of parameters");
                continue;
            }

            p1 = atoi(s1);
            p2 = atoi(s2);

            ret = syscall(TAG_SEND, p1, p2, choice2, strlen(choice2));

            if(ret != 0){
                print_error("Error");
            }

        }
        else if(strcmp(choice3, "recv") == 0){

            s1 = strtok(NULL, " ");
            s2 = strtok(NULL, " ");
            s3 = strtok(NULL, " ");

            if(s1 == NULL || s2 == NULL || s3 == NULL) {
                print_error("Wrong number of parameters");
                continue;
            }

            p1 = atoi(s1);
            p2 = atoi(s2);
            p3 = atoi(s3);

            buffer = (char *)malloc(p3*sizeof(char));
            memset(buffer, 0 , p3*sizeof(char)); // Empty buffer

            // Checking if buffer was correctly allocated
            if (buffer == NULL){
                print_error("Buffer allocation error");
                continue;
            }

            ret = syscall(TAG_RECEIVE, p1, p2, buffer, p3);

            if(ret != 0){
                print_error("Error");
            }
            else{
                printf("Buffer received : %s\n", buffer);
            }

            free(buffer);

        }
        else if(strcmp(choice3, "awake") == 0){

            s1 = strtok(NULL, " ");

            if(s1 == NULL) {
                print_error("Wrong number of parameters");
                continue;
            }

            p1 = atoi(s1);

            ret = syscall(TAG_CTL, p1, 3);

            if(ret != 0){
                print_error("Error");
            }

        }
        else if(strcmp(choice3, "del") == 0){

            s1 = strtok(NULL, " ");

            if(s1 == NULL) {
                print_error("Wrong number of parameters");
                continue;
            }

            p1 = atoi(s1);

            ret = syscall(TAG_CTL, p1, 4);

            if(ret != 0){
                print_error("Error");
            }

        }
        else if(strcmp(choice3, "help") == 0){

            show_help();

        }
        else if(strcmp(choice3, "quit") == 0){

            break;

        }

    }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

/* Shows the list of available commands */
void show_help(){

    printf("\n ----------------------------------------------------------------------\n");
    printf("|                                                                      |\n");
    printf("| get key                          - create new tag (key = 0 private)  |\n");
    printf("| open key                         - open tag                          |\n");
    printf("| send tag level 'message'         - send message to tag               |\n");
    printf("| recv tag level size              - receive message from tag          |\n");
    printf("| awake tag                        - awake all threads from tag        |\n");
    printf("| del tag                          - remove tag                        |\n");
    printf("| help                             - show this manual                  |\n");
    printf("| quit                             - quit                              |\n");
    printf("|                                                                      |\n");
    printf(" ----------------------------------------------------------------------\n\n");

}

/* Prints an error in red
 *
 * string = message to show before the error
 *
*/
void print_error(char *string){

    printf("\033[1;31m\n"); // set red

    perror(string);

    printf("\033[0m\n"); // remove red
}