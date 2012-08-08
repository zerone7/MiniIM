#ifndef _CONN_PACKET_H_
#define _CONN_PACKET_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "conn_list.h"

#define LIST_PACKET_SIZE	(sizeof(struct list_head) + MAX_PACKET_LEN)

/* packet list */
struct list_packet {
	struct list_head list;
	struct packet packet;
};

/* init the packet */
static inline void packet_init(struct list_packet *packet)
{
	assert(packet);
	memset(packet, 0, LIST_PACKET_SIZE);
	INIT_LIST_HEAD(&packet->list);
}

/* convert header from network byte order to host byte order */
static inline void convert_header(struct list_packet *packet)
{
	assert(packet);
	struct packet *p = &packet->packet;
	p->len = ntohs(p->len);
	p->ver = ntohs(p->ver);
	p->cmd = ntohs(p->cmd);
	p->uin = ntohl(p->uin);
}

/* get length of the packet */
static inline uint16_t get_length(struct list_packet *packet)
{
	assert(packet);
	return packet->packet.len;
}

/* get version of the protocol */
static inline uint16_t get_version(struct list_packet *packet)
{
	assert(packet);
	return packet->packet.ver;
}

/* get command of the packet */
static inline uint16_t get_command(struct list_packet *packet)
{
	assert(packet);
	return packet->packet.cmd;
}

/* get uin of the packet */
static inline uint32_t get_uin(struct list_packet *packet)
{
	assert(packet);
	return packet->packet.uin;
}

/* get parameters of the command */
static inline uint8_t* get_parameters(struct list_packet *packet)
{
	assert(packet);
	return packet->packet.params;
}

struct conn_server;

/* client packet handler */
int cmd_packet_handler(struct conn_server *server, struct list_packet *packet);
int cmd_keep_alive(struct conn_server *server, struct list_packet *packet);
int cmd_logout(struct conn_server *server, struct list_packet *packet);
int cmd_user(struct conn_server *server, struct list_packet *packet);
int cmd_contact(struct conn_server *server, struct list_packet *packet);
int cmd_message(struct conn_server *server, struct list_packet *packet);

/* backend server packet handler */
int srv_packet_handler(struct conn_server *server, struct list_packet *packet);
int srv_error(struct conn_server *server, struct list_packet *packet);
int srv_login_ok(struct conn_server *server, struct list_packet *packet);
int srv_other_packet(struct conn_server *server, struct list_packet *packet);

#endif
