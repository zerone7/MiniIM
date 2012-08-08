/*************************************************************
 * main.c
 *      服务器端程序入口，负责创建各个模块子进程
 *
 *************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "modules.h"

#define CHILDNUM        4
#define PROCESS_MASK    0x1

int main()
{
    int i, alive = 0;
    pid_t pid[CHILDNUM]; //子进程进程号
    pid_t waitpid;

    void (*fucs[CHILDNUM])() = {user, friend, message, status}; //子进程入口函数

    for(i = 0; i < CHILDNUM; i++)
        pid[i] = -1;

    for(i = 0; i < CHILDNUM; i++)
    {
        if(PROCESS_MASK & 1<<i)
        {
            /* 依次创建各个模块进程 */
            if((pid[i] = fork()) == 0) //子进程
            {
                fucs[i]();
                printf("process %d exit\n", i);
                return 0;
            }
            else if(pid[i] < 0) //错误处理
            {
                printf("Error creating process %d\n", i);
                return -1;
            }

            alive++;
        }
    }

    /* 等待子进程全部退出 */
    while(alive)
    {
        waitpid = wait(NULL);
        printf("====> child process %d exit !\n", waitpid);
        alive--;
    }
    printf("Main process exit\n", waitpid);

    return 0;
}
