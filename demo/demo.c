#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Systemcall numbers
#define TAG_GET 134
#define TAG_SEND 174
#define TAG_RECEIVE 182
#define TAG_CTL 183

// Shows list of commands
void show_help();

int main(int argc, char** argv){

    char *command;
    char *choice;
    char *buffer;
    int p1, p2, p3;
    char *p4;

    system("clear");	// Cleans the terminal

    printf("\t***** Welcome to SOA demo *****\n\n");

    show_help();

    while(1){

        printf("--> ");

        scanf("%m[^\n]s", &command);	// Controllo su scanf ?????????????
        getchar();

        choice = strtok(command, " ");

        if(strcmp(choice, "get") == 0){

            p1 = atoi(strtok(NULL, " "));
            p2 = atoi(strtok(NULL, " "));
            p3 = atoi(strtok(NULL, " "));

            printf("tag_get returned %ld\n", syscall(TAG_GET, p1, p2, p3));

        }
        else if(strcmp(choice, "send") == 0){

            p1 = atoi(strtok(NULL, " "));
            p2 = atoi(strtok(NULL, " "));
            p4 = strtok(NULL, " ");
            p3 = atoi(strtok(NULL, " "));

            printf("tag_send returned %ld\n", syscall(TAG_SEND, p1, p2, p4, p3));

        }
        else if(strcmp(choice, "receive") == 0){

            p1 = atoi(strtok(NULL, " "));
            p2 = atoi(strtok(NULL, " "));
            p3 = atoi(strtok(NULL, " "));

            buffer = (char *)malloc(p3*sizeof(char));

            printf("tag_receive returned %ld\n", syscall(TAG_RECEIVE, p1, p2, buffer, p3));
            printf("Buffer : %s\n", buffer);

            free(buffer);

        }
        else if(strcmp(choice, "ctl") == 0){

            p1 = atoi(strtok(NULL, " "));
            p2 = atoi(strtok(NULL, " "));

            printf("tag_ctl returned %ld\n", syscall(TAG_CTL, p1, p2));

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

void show_help(){

    printf("\n --------------------------------------------------------------\n");
    printf("|                                                              |\n");
    printf("| get key command permission            - tag_get              |\n");
    printf("| send tag level buffer size            - tag_send             |\n");
    printf("| receive tag level size                - tag_receive          |\n");
    printf("| ctl tag command                       - tag_ctl              |\n");
    printf("| help                                  - Shows this manual    |\n");
    printf("| quit                                  - Quit                 |\n");
    printf("|                                                              |\n");
    printf(" --------------------------------------------------------------\n\n");

}
