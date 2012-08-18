/************************************************************
 * protocol.h
 *      网络通信协议相关的数据定义
 *
 ***********************************************************/
#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>

/*
 * 客户端发送给服务器的请求/命令
 */
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
#define CMD_MSG_FRIEND          0x1203
#define REP_STATUS_CHANGED      0x2101
#define REP_STATUS              0x2201
#define REP_MULTI_STATUS        0x2202
#define CMD_FRIEND_ADD          0x1301

/*
 * Message类型
 */
#define MSG_TYPE_REQUEST        1
#define MSG_TYPE_ACCEPT         2
#define MSG_TYPE_REFUSE         3
#define MSG_TYPE_CHAT           4

#define MAX_PACKET_LEN          5018  
#define PACKET_HEADER_LEN       12

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
#endif
