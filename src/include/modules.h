/**************************************************************
 * modules.h
 *      服务器端各模块共用的数据结构及方法
 *
 *************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>

#define INET
#define MODULE

#ifdef  INET
/* Internet Socket */
#define USER_IP         "127.0.0.1"  
#define USER_PORT       11001  
#define FRIEND_IP       "127.0.0.1"
#define FRIEND_PORT     11002
#define MESSAGE_IP      "127.0.0.1"
#define MESSAGE_PORT    11003
#define STATUS_IP       "127.0.0.1"
#define STATUS_PORT     11004
#else
/* Unix Socket */
#endif

#define USER            1
#define FRIEND          2
#define MESSAGE         3
#define STATUS          4


void user();
void friend();
void message();
void status();
int connect_to(int module);
int service(int module, int con_num);
void set_nonblock(int fd);
