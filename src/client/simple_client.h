#include <stdint.h>

#define CMD_KEEP_ALIVE          0x000A
#define CMD_LOGIN               0x0101
#define CMD_LOGOUT              0x0102
#define CMD_SET_NICK            0x0201
#define CMD_ADD_CONTACT         0x0301
#define CMD_ADD_CONTACT_REPLY   0x0302
#define CMD_CONTACT_LIST        0x0401
#define CMD_CONTACT_INFO_MULTI  0x0402
#define CMD_MESSAGE             0x0501
#define CMD_OFFLINE_MSG         0x0601

/*
 * 服务器发送给客户端的请求/命令
 */
#define	SRV_ERROR               0x000E
#define SRV_LOGIN_OK            0x0101
#define SRV_SET_NICK_OK         0x0201
#define SRV_ADD_CONTACT_WAIT    0x0301
#define SRV_ADD_CONTACT_AUTH    0x0302
#define SRV_ADD_CONTACT_REPLY   0x0303
#define SRV_CONTACT_LIST        0x0401
#define SRV_CONTACT_INFO_MULTI  0x0402
#define SRV_MESSAGE             0x0501
#define SRV_OFFLINE_MSG         0x0601
#define SRV_OFFLINE_MSG_DONE    0x0602

/*
 * 模块间交互的请求/命令
 */
#define CMD_STATUS_CHANGE       0x1101
#define CMD_GET_STATUS          0x1201
#define CMD_MULTI_STATUS        0x1202
#define REP_STATUS_CHANGED      0x2101
#define REP_STATUS              0x2201

#define USER_IP         "127.0.0.1"  
#define USER_PORT       11001  
#define FRIEND_IP       "127.0.0.1"
#define FRIEND_PORT     11002
#define MESSAGE_IP      "127.0.0.1"
#define MESSAGE_PORT    11003
#define STATUS_IP       "127.0.0.1"
#define STATUS_PORT     11004

#define USER            1
#define FRIEND          2
#define MESSAGE         3
#define STATUS          4

#define HEADER_LEN      12
#define BUFSIZE     100

#define PARAM_IP(x)     (uint32_t *)x->params
#define PARAM_TYPE(x)   (uint16_t *)(x->params + 4)

#define PARAM_PASSLEN(x)    (uint16_t *)x->params 
#define PARAM_PASSWD(x)     (char *)(x->params + 2) 
#define PARAM_NICKLEN(x)    (uint16_t *)x->params
#define PARAM_NICK(x)       (char *)(x->params + 2)

/* 数据包结构 */
struct packet
{
	uint16_t    len;	// 数据包长度
	uint16_t    ver;	// 协议版本
	uint16_t    cmd;	// 请求/命令编号
	uint16_t    pad;	// Pad字节
	uint32_t    uin;	// 用户帐号
	char        params[0];	// 参数
}__attribute__((packed));


int connect_to(int module);
void user_test();
void friend_test();
void message_test();
void status_test();
