#ifndef _CONN_PACKET_H_
#define _CONN_PACKET_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "conn_list.h"

#define LIST_PACKET_SIZE	(sizeof(struct list_head) + MAX_PACKET_LEN)

/* packet list */
struct list_packet {
	struct list_head list;
	struct packet packet;
};

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

/* init the packet */
static inline void packet_init(struct list_packet *packet)
{
	assert(packet);
	memset(packet, 0, LIST_PACKET_SIZE);
	INIT_LIST_HEAD(&packet->list);
}

/* get length of the packet */
static inline uint16_t get_length_host(struct list_packet *packet)
{
	assert(packet);
	return ntohs(packet->packet.len);
}

/* get version of the protocol */
static inline uint16_t get_version_host(struct list_packet *packet)
{
	assert(packet);
	return ntohs(packet->packet.ver);
}

/* get command of the packet */
static inline uint16_t get_command_host(struct list_packet *packet)
{
	assert(packet);
	return ntohs(packet->packet.cmd);
}

/* get uin of the packet */
static inline uint32_t get_uin_host(struct list_packet *packet)
{
	assert(packet);
	return ntohl(packet->packet.uin);
}

/* get parameters of the command */
static inline uint8_t* get_parameters_network(struct list_packet *packet)
{
	assert(packet);
	return packet->packet.params;
}

static inline void add_keep_alive_packet(struct list_head *keep_alive_list,
		struct list_packet *packet)
{
	sigset_t mask, oldmask;

	/* block the SIGALRM signal before call list_add */
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);

	list_add(&packet->list, keep_alive_list);

	/* unblock the SIGALRM signal after call list_add */
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

#endif
