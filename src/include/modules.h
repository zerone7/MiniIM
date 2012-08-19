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
#include <assert.h>

#include "protocol.h"

#define _INET_
//#define _MODULE_

#ifdef  _INET_
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

/* Debug defines */
#define DEBUG_USER
#define DEBUG_FRIEND
#define DEBUG_MESSAGE
#define DEBUG_STATUS

/*
 * error_packet
 * @len: packet length
 * @ver: protocol version
 * @cmd: command
 * @pad: pad bytes
 * @uin: user uin
 * @client_cmd: the command that cause the error
 * @type: error type
 */
struct error_packet
{
	uint16_t    len;
	uint16_t    ver;
	uint16_t    cmd;
	uint16_t    pad;
	uint32_t    uin;
    uint16_t    client_cmd;
    uint16_t    type;
}__attribute__((packed));

/* init packet header */
static inline void fill_packet_header(struct packet *pack,\
        uint16_t len, uint16_t cmd, uint32_t uin)
{
    pack->len = len;
    pack->ver = 1;
    pack->cmd = cmd;
    pack->uin = uin;
}

void user();
void friend();
void message();
void status();
int connect_to(int module);
int service(int module, int con_num);
void set_nonblock(int fd);
int send_error_packet(uint32_t uin, uint16_t cmd, \
        uint16_t error, int sockfd);
int packet_read(int sockfd, char *buff, int len, int epfd);
