#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include "simple_client.h"

int connect_to(int module)
{
    int fd;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    switch(module)
    {
        case USER:
            addr.sin_addr.s_addr = inet_addr(USER_IP);
            addr.sin_port = htons(USER_PORT);
            break;
        case FRIEND:
            addr.sin_addr.s_addr = inet_addr(FRIEND_IP);
            addr.sin_port = htons(FRIEND_PORT);
            break;
        case MESSAGE:
            addr.sin_addr.s_addr = inet_addr(MESSAGE_IP);
            addr.sin_port = htons(MESSAGE_PORT);
            break;
        case STATUS:
            addr.sin_addr.s_addr = inet_addr(STATUS_IP);
            addr.sin_port = htons(STATUS_PORT);
            break;
        default:
            return -1;
    }

    if((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }

    if(connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0)
    {
        perror("connect");
        return -1;
    }
    
    return fd;
}

void user_test()
{
	int client_sockfd;
	int len, n;
    char cmd[20];
    char nick[] = "justanick";
    struct packet *loginpack, *nickpack, *recvpack;

    loginpack = (struct packet *)malloc(BUFSIZE);
    nickpack = (struct packet *)malloc(BUFSIZE);
    recvpack = (struct packet *)malloc(BUFSIZE);

    sprintf(PARAM_PASSWD(loginpack), "10086");
    *PARAM_PASSLEN(loginpack) = 6;
    loginpack->len = HEADER_LEN + sizeof("10086") + 2;
    loginpack->ver = 1;
    loginpack->cmd = 0x0101;
    loginpack->uin = 10086;

    printf("len %d, passwd: %s\n", loginpack->len, PARAM_PASSWD(loginpack));

    sprintf(PARAM_NICK(nickpack), "%s", nick);
    *PARAM_NICKLEN(nickpack) = (uint16_t)sizeof(nick);
    nickpack->ver = 1;
    nickpack->cmd = 0x0201;
    nickpack->uin = 11111;
    nickpack->len = HEADER_LEN + 2 + sizeof(nick);

    client_sockfd = connect_to(USER);
    if(client_sockfd < 0)
    {
        printf("connect to USER error\n");
        exit(-1);
    }
	printf("connected to server USER\n");

    while(fgets(cmd, 20, stdin))
    {
        if(!strcmp(cmd, "login\n")) // correclt loging packet
        {
            loginpack->uin = 10086;
		    send(client_sockfd, loginpack, loginpack->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, nick %s.\n", recvpack->len, recvpack->cmd, recvpack->uin,PARAM_NICK(recvpack));
        }
        else if(!strcmp(cmd, "xlogin\n")) // faulse login packet
        {
            loginpack->uin = 11086;
		    send(client_sockfd, loginpack, loginpack->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, params %04x.\n", recvpack->len, recvpack->cmd, recvpack->uin,*(int *)recvpack->params);
        }
        else if(!strcmp(cmd, "nick\n"))
        {
            send(client_sockfd, nickpack, nickpack->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0); 
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, nick %s, nicklen %d.\n", recvpack->len, recvpack->cmd, recvpack->uin, PARAM_NICK(recvpack), *PARAM_NICKLEN(recvpack));
        }
        else if(!strcmp(cmd, "srv\n"))
        {
            send(client_sockfd, "close", 6, 0);
            break;
        }
        else if(!strcmp(cmd, "exit\n"))
            break;
        else
            printf("Unknown command!\n");

    }

	close(client_sockfd);//关闭套接字
}


void status_test()
{
	int client_sockfd;
	int len, n;
    uint32_t uin, *pip;
    uint16_t *pstat;
    char cmd[20];
    char *pcmd;
    struct packet *gstatpack, *sstatpack, *recvpack;

    gstatpack = (struct packet *)malloc(BUFSIZE);
    sstatpack = (struct packet *)malloc(BUFSIZE);
    recvpack = (struct packet *)malloc(BUFSIZE);

    client_sockfd = connect_to(STATUS);
    if(client_sockfd < 0)
    {
        printf("connect to USER error\n");
        exit(-1);
    }
	printf("connected to server STATUS\n");

    while(fgets(cmd, 20, stdin))
    {
        pcmd = strtok(cmd, " ");
        if(!strcmp(pcmd, "gstat")) // correclt loging packet
        {
            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            gstatpack->uin = uin;
            gstatpack->len = HEADER_LEN;
            gstatpack->ver = 1;
            gstatpack->cmd = CMD_GET_STATUS;
            
		    send(client_sockfd, gstatpack, gstatpack->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            pip = PARAM_IP(recvpack);
            pstat = PARAM_TYPE(recvpack);
            printf("Packet receive %d bytes -------------->\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, stat %d, ip %d.\n", recvpack->len, recvpack->cmd, recvpack->uin, *pstat, *pip);
        }
        else if(!strcmp(pcmd, "sstat"))
        {
            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            sstatpack->uin = uin;
            sstatpack->ver = 1;
            sstatpack->cmd = CMD_STATUS_CHANGE;
            sstatpack->len = HEADER_LEN + 6;

            /* 设置用户连接的接口服务器IP */
            pip = PARAM_IP(sstatpack);
            *pip = 12345;

            /* 设置要改变的状态 1:上线;0:下线 */
            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            pstat = PARAM_TYPE(sstatpack);
            *pstat = uin ? 1 : 0;

            send(client_sockfd, sstatpack, sstatpack->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0); 
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d\n", recvpack->len, recvpack->cmd, recvpack->uin);
        }
        else if(!strcmp(pcmd, "srv\n"))
        {
            send(client_sockfd, "close", 6, 0);
            break;
        }
        else if(!strcmp(pcmd, "exit\n"))
            break;
        else
            printf("Unknown command!\n");

    }

	close(client_sockfd);//关闭套接字
}

int main(int argc, char *argv[])
{
    char module[10];

    printf("Connect to: ");
    while(fgets(module, 10, stdin))
    {
        if(!strcmp(module, "user\n"))
            user_test();
        else if(!strcmp(module, "status\n"))
            status_test();
        else if(!strcmp(module, "exit\n"))
            break;
        else
            printf("Unknown Module\n");

        printf("Connect to: ");
    }

    return 0;
}
