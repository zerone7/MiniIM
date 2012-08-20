/*************************************************************
 * main.c
 *      server main fuction, creata child process for each
 * module
 *
 *************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "modules.h"

#define CHILDNUM        4

#define USER_START      0x1
#define FRIEND_START    0x2
#define MESSAGE_START   0x4
#define STATUS_START    0x8
#define PROCESS_MASK    MESSAGE_START | FRIEND_START | STATUS_START | USER_START

int main()
{
    int i, alive = 0, start;
    pid_t pid[CHILDNUM]; //child process num
    pid_t waitpid;

    /* child process entrance */
    void (*fucs[CHILDNUM])() = {user, friend, message, status};

    for (i = 0; i < CHILDNUM; i++)
        pid[i] = -1;

    start = PROCESS_MASK;
    for (i = 0; i < CHILDNUM; i++) {
        /* create child process according to PROCESS_MASK */
        if (start & 1<<i) {
            if ((pid[i] = fork()) == 0) {
                fucs[i]();
                printf("process %d exit\n", i);
                return 0;
            } else if (pid[i] < 0) {
                printf("Error creating process %d\n", i);
                return -1;
            }
            alive++;
        }
    }

    /* Wait for all child process exit */
    while (alive) {
        waitpid = wait(NULL);
        printf("====> child process %d exit !\n", waitpid);
        alive--;
    }
    printf("Main process exit\n", waitpid);

    return 0;
}
