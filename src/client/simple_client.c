#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include "simple_client.h"
#define PORT1   49872
#define PORT2   49893

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
        else if(!strcmp(module, "message\n"))
            message_test();
        else if(!strcmp(module, "friend\n"))
            friend_test();
        else if(!strcmp(module, "exit\n"))
            break;
        else
            printf("Unknown Module\n");

        printf("Connect to: ");
    }

    return 0;
}

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
        case CONN:
            addr.sin_addr.s_addr = inet_addr(CONN_IP);
            addr.sin_port = htons(CONN_PORT);
            break;
        default:
            return -1;
    }

    if((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }
/*    if(module == USER)
    {
        struct sockaddr_in uaddr;
        memset(&uaddr, 0, sizeof(addr));
        uaddr.sin_family = AF_INET;
        uaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        uaddr.sin_port = htons(PORT2);

        if (bind(fd, (struct sockaddr *)&uaddr, sizeof(uaddr)) < 0) {
            perror("bind");
            return -1;
        }
    }
*/
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
    char cmd[100];
    char *pcmd;
    char nick[] = "justanick";
    struct packet *loginpack, *nickpack, *recvpack, *addpack, *packet;

    loginpack = (struct packet *)malloc(BUFSIZE);
    nickpack = (struct packet *)malloc(BUFSIZE);
    recvpack = (struct packet *)malloc(BUFSIZE);
    addpack = (struct packet *)malloc(BUFSIZE);
    packet = (struct packet *)malloc(BUFSIZE);

    sprintf(PARAM_PASSWD(loginpack), "kyleluo");
    *PARAM_PASSLEN(loginpack) = 7;
    loginpack->len = HEADER_LEN + 7 + 2;
    loginpack->ver = 1;
    loginpack->cmd = 0x0101;
    loginpack->uin = 10010;

    printf("len %d, passwd: %s\n", loginpack->len, PARAM_PASSWD(loginpack));

    client_sockfd = connect_to(USER);
    //client_sockfd = connect_to(CONN);
    if(client_sockfd < 0)
    {
        printf("connect to USER error\n");
        exit(-1);
    }
	printf("connected to server USER\n");

    while(fgets(cmd, 20, stdin))
    {
        pcmd = strtok(cmd, " ");
        if(!strcmp(pcmd, "login\n")) // correclt loging packet
        {
            loginpack->uin = 17784;
		    send(client_sockfd, loginpack, loginpack->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, nick %s.\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin,PARAM_NICK(recvpack));
        }
        else if(!strcmp(pcmd, "xlogin\n")) // faulse login packet
        {
            loginpack->uin = 11086;
		    send(client_sockfd, loginpack, loginpack->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, params %04x.\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin,*(int *)recvpack->params);
        }
        else if(!strcmp(pcmd, "add\n"))
        {
            addpack->len = HEADER_LEN;
            addpack->ver = 1;
            addpack->cmd = CMD_FRIEND_ADD;
            addpack->uin = 11111;
            send(client_sockfd, addpack, addpack->len, 0);
        }
        else if(!strcmp(cmd, "nick"))
        {
            pcmd = strtok(NULL, " ");
            strcpy(PARAM_NICK(nickpack), pcmd);
            *PARAM_NICKLEN(nickpack) = strlen(pcmd)+1;
            nickpack->ver = 1;
            nickpack->cmd = 0x0201;
            nickpack->uin = 11111;
            nickpack->len = HEADER_LEN + 3 + strlen(pcmd);

            send(client_sockfd, nickpack, nickpack->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0); 
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, nick %s, nicklen %d.\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin, PARAM_NICK(recvpack), *PARAM_NICKLEN(recvpack));
        }
        else if(!strcmp(cmd, "regist"))
        {
            packet->len = HEADER_LEN;
            packet->ver = 1;
            packet->cmd = CMD_REGISTER;
            pcmd = strtok(NULL, " ");
            strcpy(packet->params+2, pcmd);
            len = strlen(pcmd)+1;
            *(uint16_t *)(packet->params) = len;
            packet->len += 2+len;
            pcmd = strtok(NULL, " ");
            *(uint16_t *)(packet->params+2+len) = strlen(pcmd);
            packet->len += 2+strlen(pcmd);
            strcpy(packet->params+4+len, pcmd);
            send(client_sockfd, packet, packet->len, 0);
            printf("register, nick '%s' len %d, passwd '%s' len %d\n", packet->params + 2, \
                    *(uint16_t *)packet->params, packet->params+4+len, \
                    *(uint16_t *)(packet->params + 2 + len));

            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d.\n", recvpack->len, recvpack->cmd, \
                    recvpack->uin);
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
	int len, n, i;
    uint32_t uin, *pip, *uins;
    uint16_t *pstat, stat;
    char cmd[20];
    char *pcmd;
    struct packet *gstatpack, *sstatpack, *recvpack;
    struct status_info *multi_stat;
    struct sockaddr_in addr;
    socklen_t   addrlen;

    gstatpack = (struct packet *)malloc(BUFSIZE);
    sstatpack = (struct packet *)malloc(BUFSIZE);
    recvpack = (struct packet *)malloc(BUFSIZE);

    client_sockfd = connect_to(STATUS);
    //client_sockfd = connect_to(CONN);
    if(client_sockfd < 0)
    {
        printf("connect to USER error\n");
        exit(-1);
    }
    getsockname(client_sockfd, (struct sockaddr *)&addr, &addrlen); 
	printf("connected to server STATUS, my ip %d\n", (int)addr.sin_addr.s_addr);

    while(fgets(cmd, 20, stdin))
    {
        pcmd = strtok(cmd, " ");
        if(!strcmp(pcmd, "gstat")) // correclt loging packet
        {
            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            gstatpack->uin = uin;
            gstatpack->len = HEADER_LEN + 4;
            gstatpack->ver = 1;
            gstatpack->cmd = CMD_GET_STATUS;
            *(uint32_t *)gstatpack->params = uin;
		    send(client_sockfd, gstatpack, gstatpack->len, 0);

            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            pip = PARAM_IP(recvpack);
            pstat = PARAM_TYPE(recvpack);
            printf("Packet receive %d bytes -------------->\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, stat %d, ip %d.\n", \
                    recvpack->len, recvpack->cmd, recvpack->uin, *pstat, *pip);
        }
        else if(!strcmp(pcmd, "mstat\n"))
        {
            gstatpack->uin = (uint32_t)10000;
            gstatpack->ver = 1;
            gstatpack->cmd = CMD_MULTI_STATUS;
            n = 10;
            *(uint16_t *)gstatpack->params = (uint16_t)n;
            printf("%d uins to send\n", *(uint16_t *)gstatpack->params);
            uins = (uint32_t *)(gstatpack->params + 2);

            for(i = 0; i < n; i++)
            {
                uins[i] = 10000+i;
            //    printf("user: %d, ", uins[i]);
            }
            gstatpack->len = HEADER_LEN + 2 + 4*n;
		    send(client_sockfd, gstatpack, gstatpack->len, 0);

            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            printf("Packet receive %d bytes -------------->\n", n);
            printf("Packet: len %d, cmd %04x, uin %d.\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin);
            n = *(uint16_t *)recvpack->params; 
            multi_stat = (struct status_info *)(recvpack->params + 2);
            for(i = 0; i < n; i++)
                printf("user: %d, status: %d\n", multi_stat[i].uin, multi_stat[i].stat);
        }
        else if(!strcmp(pcmd, "sstat"))
        {
            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            sstatpack->uin = uin;
            sstatpack->ver = 1;
            sstatpack->cmd = CMD_STATUS_CHANGE;
            sstatpack->len = HEADER_LEN + 10;

            *PARAM_UIN(sstatpack) = uin;

            /* 设置用户连接的接口服务器IP */
            pip = PARAM_IP(sstatpack);
            *pip = (uint32_t)addr.sin_addr.s_addr;

            /* 设置要改变的状态 1:上线;0:下线 */
            pcmd = strtok(NULL, " ");
            stat = atoi(pcmd);
            pstat = PARAM_TYPE(sstatpack);
            *pstat = stat ? 1 : 0;
            send(client_sockfd, sstatpack, sstatpack->len, 0);

            n = recv(client_sockfd, recvpack, BUFSIZE, 0); 
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin);
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


void message_test()
{
	int client_sockfd;
	int len, n;
    uint32_t uin, *pip;
    uint16_t *pstat, stat;
    char cmd[20];
    char chat[100];
    char *pcmd;
    struct packet *offlinepack, *chatpack, *recvpack, *packet;
    struct sockaddr_in addr;
    socklen_t   addrlen;

    offlinepack = (struct packet *)malloc(BUFSIZE);
    chatpack = (struct packet *)malloc(BUFSIZE);
    packet = (struct packet *)malloc(BUFSIZE);
    recvpack = (struct packet *)malloc(BUFSIZE);

    sprintf(chat,"%s", "This is a chat message example, hehe :)");

    client_sockfd = connect_to(MESSAGE);
    //client_sockfd = connect_to(CONN);
    if(client_sockfd < 0)
    {
        printf("connect to MESSAGE error\n");
        exit(-1);
    }

    getsockname(client_sockfd, (struct sockaddr *)&addr, &addrlen); 
	printf("connected to server MESSAGE, my ip %d\n", (int)addr.sin_addr.s_addr);

    packet->len = 18;
    packet->ver = 1;
    packet->cmd = CMD_CONN_INFO;
    packet->uin = 10086;
    *(uint32_t *)packet->params = ntohl(inet_addr("127.0.0.1"));
    *(uint16_t *)(packet->params + 4) = PORT2;
    write(client_sockfd, packet, packet->len);

    while(fgets(cmd, 20, stdin))
    {
        pcmd = strtok(cmd, " ");
        if(!strcmp(pcmd, "offline"))
        {
            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            offlinepack->uin = uin;
            offlinepack->len = HEADER_LEN;
            offlinepack->ver = 1;
            offlinepack->cmd = CMD_OFFLINE_MSG;
            
		    send(client_sockfd, offlinepack, offlinepack->len, 0);

            get_offline_msgs(client_sockfd, recvpack);
        }
        else if(!strcmp(pcmd, "login\n"))
        {
            sprintf(PARAM_PASSWD(packet), "10010");
            *PARAM_PASSLEN(packet) = 6;
            packet->len = HEADER_LEN + sizeof("10010") + 2;
            packet->ver = 1;
            packet->cmd = 0x0101;
            packet->uin = 10010;
		    send(client_sockfd, packet, packet->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, nick %s.\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin,PARAM_NICK(recvpack));

            n = recv(client_sockfd, recvpack, BUFSIZE, 0); 
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin);
            printf("Message: from %d, type %d, len %d, msg '%s'\n", *PARAM_UIN(recvpack), \
                    *PARAM_TYPE(recvpack), *(uint16_t *)(recvpack->params+10), (char *)recvpack->params+12);
        }
        else if(!strcmp(pcmd, "chat"))
        {
            /*pcmd = strtok(NULL, " "); // From uin
            uin = atoi(pcmd);
            chatpack->uin = uin;
            chatpack->ver = 1;
            chatpack->cmd = CMD_MESSAGE;
            chatpack->len = HEADER_LEN + 11 + strlen(chat);

            pcmd = strtok(NULL, " "); //to uin
            uin = atoi(pcmd);
            *PARAM_TO_UIN(chatpack) = uin;

            *PARAM_LEN(chatpack) = strlen(chat) + 1;

            strcpy((char *)(chatpack->params+10), chat);
            send(client_sockfd, chatpack, chatpack->len, 0);
            printf("Chat_msg: from %d, to %d, len %d\n", chatpack->uin, *PARAM_TO_UIN(chatpack),\
                    *PARAM_LENGTH(chatpack));
            printf("Message: %s\n", chatpack->params+10);
*/
           n = recv(client_sockfd, recvpack, BUFSIZE, 0); 
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin);
            printf("Message: from %d, type %d, len %d, msg '%s'\n", *PARAM_UIN(recvpack), \
                    *PARAM_TYPE(recvpack), *(uint16_t *)(recvpack->params+10), (char *)recvpack->params+12);
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

void get_offline_msgs(int client_sockfd, struct packet *recvpack)
{
    int done, n, msgnum, i;
    uint32_t *puin;
    uint16_t *ptype, *plen;
    char *msg, *pcontent;
    
    done = 0;
    while(!done)
    {
        n = recv(client_sockfd, recvpack, BUFSIZE, 0);
        msgnum = *(uint16_t *)recvpack->params;
        msg = recvpack->params + 2;
        printf("Packet receive %d bytes ------------->\n", n);
        printf("Packet: len %d, cmd %04x, uin %d, containing %d msgs.\n", recvpack->len, \
                recvpack->cmd, recvpack->uin, msgnum);

        for(i = 0; i < msgnum; i++)
        {
            puin = (uint32_t *)(msg + MSG_FROM_UIN_OFFSET);
            ptype = (uint16_t *)(msg + MSG_TYPE_OFFSET);
            plen = (uint16_t *)(msg + MSG_LENGTH_OFFSET);
            pcontent = msg + MSG_CONTENT_OFFSET;

            printf("Message %d: from %d, to %d, type %d, len %d\n", i+1, *puin, recvpack->uin,\
                    *ptype, *plen);
            printf("\t%s\n", pcontent);

            msg += 13 + *plen;
        }

        if(recvpack->cmd == SRV_OFFLINE_MSG_DONE)
            done = 1;
    }
}

void friend_test()
{
	int client_sockfd;
	int len, n;
    uint32_t uin, *pip, *puin;
    uint16_t *pstat, stat, *pcount;
    char cmd[20];
    char *pcmd;
    struct packet *listpack, *infopack, *recvpack, *packet;
    struct sockaddr_in addr;
    socklen_t   addrlen;

    listpack = (struct packet *)malloc(BUFSIZE);
    infopack = (struct packet *)malloc(BUFSIZE);
    recvpack = (struct packet *)malloc(BUFSIZE);
    packet = (struct packet *)malloc(BUFSIZE);

    client_sockfd = connect_to(FRIEND);
    //client_sockfd = connect_to(CONN);
    if(client_sockfd < 0)
    {
        printf("connect to FRIEND error\n");
        exit(-1);
    }

    getsockname(client_sockfd, (struct sockaddr *)&addr, &addrlen); 
	printf("connected to server FRIEND, my ip %d\n", (int)addr.sin_addr.s_addr);

    while(fgets(cmd, 20, stdin))
    {
        pcmd = strtok(cmd, " ");
        if(!strcmp(pcmd, "list"))
        {
            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            listpack->uin = uin;
            listpack->len = HEADER_LEN;
            listpack->ver = 1;
            listpack->cmd = CMD_CONTACT_LIST;
		    send(client_sockfd, listpack, listpack->len, 0);

            print_friend_list(client_sockfd, recvpack);
        }
        else if(!strcmp(pcmd, "login\n"))
        {
            sprintf(PARAM_PASSWD(packet), "10086");
            *PARAM_PASSLEN(packet) = 6;
            packet->len = HEADER_LEN + sizeof("10086") + 2;
            packet->ver = 1;
            packet->cmd = 0x0101;
            packet->uin = 10086;
		    send(client_sockfd, packet, packet->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, nick %s.\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin,PARAM_NICK(recvpack));
        }
        else if(!strcmp(pcmd, "info\n"))
        {
            infopack->uin = 10086;
            infopack->len = HEADER_LEN + 2 + 5*4;
            infopack->ver = 1;
            infopack->cmd = CMD_CONTACT_INFO_MULTI;
            pcount = (uint16_t *)infopack->params;
            *pcount = 5;
            puin = (uint32_t *)(infopack->params + 2);
            puin[0] = 10086;
            puin[1] = 10010;
            puin[2] = 10000;
            puin[3] = 519;
            puin[4] = 11111;
            send(client_sockfd, infopack, infopack->len, 0);

            print_friend_info(client_sockfd, recvpack);
        }
        else if(!strcmp(pcmd, "add"))
        {
            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            packet->uin = 10086;   
            packet->len = HEADER_LEN + 4;
            packet->ver = 1;
            packet->cmd = CMD_ADD_CONTACT;
            *(uint32_t *)packet->params = uin;
            send(client_sockfd, packet, packet->len, 0);
            
            n = recv(client_sockfd, recvpack, BUFSIZE, 0); 
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d\n", recvpack->len, \
                    recvpack->cmd, recvpack->uin);
            printf("Friend: uin %d, nicklen %d, nick %s\n", *(uint32_t *)recvpack->params, \
                    *(uint16_t *)(recvpack->params+4), (char *)recvpack->params+6);

        }
        else if(!strcmp(pcmd, "reply"))
        {
            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            packet->len = HEADER_LEN + 6;
            packet->ver = 1;
            packet->cmd = CMD_ADD_CONTACT_REPLY;
            packet->uin = uin;

            pcmd = strtok(NULL, " ");
            uin = atoi(pcmd);
            *(uint32_t *)packet->params = uin;
            *(uint16_t *)(packet->params + 4) = 1;
            printf("Reply: from %d, to %d, reply_type %d\n", packet->uin, uin, 1); 
            send(client_sockfd, packet, packet->len, 0);
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
            
void print_friend_list(int client_sockfd, struct packet *recvpack)
{
    int n, count, i;
    uint32_t *friend_list;

    n = recv(client_sockfd, recvpack, BUFSIZE, 0);
    count = *(uint16_t *)recvpack->params;
    printf("There are %d friends, they are=>\n", count);

    friend_list = (uint32_t *)(recvpack->params + 2);
    for(i = 0; i < count; i++)
        printf("%d, ", friend_list[i]);
    printf("\n");
}

void print_friend_info(int client_sockfd, struct packet *recvpack)
{
    int n, count, i;
    struct contact_info *friend;
    char *start;

    n = recv(client_sockfd, recvpack, BUFSIZE, 0);
    count = *(uint16_t *)recvpack->params;
    printf("There are %d friends.\n", count);

    start = recvpack->params + 2;
    for(i = 0; i < count; i++)
    {
        friend = (struct contact_info *)start;
        printf("user(%d): stat %d, len %d, nick '%s'\n", friend->uin, friend->stat, friend->len, friend->nick);
        start += 8 + friend->len;
    }
}
