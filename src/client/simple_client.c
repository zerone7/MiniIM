#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>

#define BUFSIZE     100
#define USER_PORT       11001  
#define FRIEND_PORT     11002
#define MESSAGE_PORT    11003
#define STATUS_PORT     11004

#define HEADER_LEN      12

/* 数据包结构 */
struct packet
{
	uint16_t     len;	// 数据包长度
	uint16_t     ver;	// 协议版本
	uint16_t     cmd;	// 请求/命令编号
	uint16_t     pad;	// Pad字节
	uint32_t     uin;	// 用户帐号
	char        params[0];	// 参数
}__attribute__((packed));

int main(int argc, char *argv[])
{

	int client_sockfd;
	int len, n;
	pid_t pid;
    struct sockaddr_in remote_addr;
    char cmd[20];
    char nick[] = "chnick";
    struct packet *loginpack, *nickpack, *recvpack;

    loginpack = (struct packet *)malloc(BUFSIZE);
    nickpack = (struct packet *)malloc(BUFSIZE);
    recvpack = (struct packet *)malloc(BUFSIZE);

    sprintf(loginpack->params, "10086");
    loginpack->len = HEADER_LEN + sizeof("10086");
    loginpack->ver = 1;
    loginpack->cmd = 0x0101;
    loginpack->uin = 10086;

    printf("len %d, params: %s\n", loginpack->len, loginpack->params);

    sprintf(nickpack->params + 2, "%s", nick);
    *((uint16_t *)nickpack->params) = (uint16_t)sizeof(nick);
    nickpack->ver = 1;
    nickpack->cmd = 0x0201;
    nickpack->uin = 11111;
    nickpack->len = HEADER_LEN + 2 + sizeof(nick);

	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family=AF_INET; 
	remote_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
	remote_addr.sin_port=htons(USER_PORT);
	
	if((client_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)
	{
		perror("socket");
		return 1;
	}
	
	if(connect(client_sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)
	{
		perror("connect");
		return 1;
	}
	printf("connected to server\n");

    while(fgets(cmd, 20, stdin))
    {
        if(!strcmp(cmd, "login\n")) // correclt loging packet
        {
            loginpack->uin = 10086;
		    send(client_sockfd, loginpack, loginpack->len, 0);
            n = recv(client_sockfd, recvpack, BUFSIZE, 0);
            printf("receive %d bytes\n", n);
            printf("Packet: len %d, cmd %04x, uin %d, nick %s.\n", recvpack->len, recvpack->cmd, recvpack->uin,recvpack->params+2);
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
            printf("Packet: len %d, cmd %04x, uin %d, nick %s, nicklen %d.\n", recvpack->len, recvpack->cmd, recvpack->uin,(char *)recvpack->params+2, *(uint16_t *)recvpack->params);
        }
        else
            printf("Unkown command!\n");

    }

	close(client_sockfd);//关闭套接字
    return 0;
}
