/* ---------------------------------------------------------------------------------------------------------------------
	DEMO

 Detailed info on how to use the system calls and their parameters can be found in the README.md file.
--------------------------------------------------------------------------------------------------------------------- */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// System call numbers
#define TAG_GET 134
#define TAG_SEND 174
#define TAG_RECEIVE 182
#define TAG_CTL 183

// Shows list of commands
void show_help();

// Prints an error in red
void print_error(char *string);


int main(int argc, char** argv){

    char *command;
    char *choice;
    char *buffer;
    int p1, p2, p3, ret;
    char *p4;

    system("clear");	// Cleans the terminal

    printf("\t***** Welcome to SOA demo *****\n\n");

    show_help();

    while(1){

        printf("--> ");

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

        choice = strtok(command, " ");

        if(strcmp(choice, "get") == 0){

            p1 = atoi(strtok(NULL, " "));
            p2 = atoi(strtok(NULL, " "));
            p3 = atoi(strtok(NULL, " "));

            ret = syscall(TAG_GET, p1, p2, p3);

            if(ret != 0){
                print_error("Error");
            }
        }
        else if(strcmp(choice, "send") == 0){

            p1 = atoi(strtok(NULL, " "));
            p2 = atoi(strtok(NULL, " "));
            p4 = strtok(NULL, " ");

            ret = syscall(TAG_SEND, p1, p2, p4, strlen(p4));

            if(ret != 0){
                print_error("Error");
            }

        }
        else if(strcmp(choice, "receive") == 0){

            p1 = atoi(strtok(NULL, " "));
            p2 = atoi(strtok(NULL, " "));
            p3 = atoi(strtok(NULL, " "));

            buffer = (char *)malloc(p3*sizeof(char));

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
        else if(strcmp(choice, "ctl") == 0){

            p1 = atoi(strtok(NULL, " "));
            p2 = atoi(strtok(NULL, " "));

            ret = syscall(TAG_CTL, p1, p2);

            if(ret != 0){
                print_error("Error");
            }

        }
        else if(strcmp(choice, "help") == 0){

            show_help();

        }
        else if(strcmp(choice, "quit") == 0){

            break;

        }

    }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

void show_help(){

    printf("\n --------------------------------------------------------------\n");
    printf("|                                                              |\n");
    printf("| get key command permission            - tag_get              |\n");
    printf("| send tag level buffer                 - tag_send             |\n");
    printf("| receive tag level size                - tag_receive          |\n");
    printf("| ctl tag command                       - tag_ctl              |\n");
    printf("| help                                  - Shows this manual    |\n");
    printf("| quit                                  - Quit                 |\n");
    printf("|                                                              |\n");
    printf(" --------------------------------------------------------------\n\n");

}


void print_error(char *string){

    printf("\033[1;31m\n"); // set red

    perror(string);

    printf("\033[0m\n"); // remove red
}
