#ifndef _CONN_PACKET_H_
#define _CONN_PACKET_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "conn_list.h"
#include "conn_timer.h"
#include "conn_connection.h"

#define LIST_PACKET_SIZE	(sizeof(struct list_head) + MAX_PACKET_LEN)
#define STATUS_CHANGE_ONLINE	0x01
#define STATUS_CHANGE_OFFLINE	0x02

/* packet list */
struct list_packet {
	struct list_head list;
	struct packet packet;
};

struct conn_server;

void close_connection(struct conn_server *server, struct connection *conn);
void send_offline_to_status(struct conn_server *server, uint32_t uin);

/* client packet handler */
void cmd_packet_handler(struct conn_server *server, struct connection *conn,
		struct list_packet *packet);
void cmd_keep_alive(struct conn_server *server, struct connection *conn,
		struct list_packet *packet);
void cmd_login(struct conn_server *server, struct connection *conn,
		struct list_packet *packet);
void cmd_logout(struct conn_server *server, struct connection *conn,
		struct list_packet *packet);
void cmd_user(struct conn_server *server, struct connection *conn,
		struct list_packet *packet);
void cmd_contact(struct conn_server *server, struct connection *conn,
		struct list_packet *packet);
void cmd_message(struct conn_server *server, struct connection *conn,
		struct list_packet *packet);

/* backend server packet handler */
void srv_packet_handler(struct conn_server *server, struct list_packet *packet);
void srv_error(struct conn_server *server, struct list_packet *packet);
void srv_login_ok(struct conn_server *server, struct list_packet *packet);
void srv_other_packet(struct conn_server *server, struct list_packet *packet);

/* init the packet */
static inline void packet_init(struct list_packet *packet)
{
	assert(packet);
	memset(packet, 0, LIST_PACKET_SIZE);
	INIT_LIST_HEAD(&packet->list);
}

/* get length of the packet */
static inline uint16_t get_length_host(const struct list_packet *packet)
{
	assert(packet);
	return packet->packet.len;
	/* TODO: need to change to network byte order */
	//return ntohs(packet->packet.len);
}

/* get version of the protocol */
static inline uint16_t get_version_host(const struct list_packet *packet)
{
	assert(packet);
	return packet->packet.ver;
	/* TODO: need to change to network byte order */
	//return ntohs(packet->packet.ver);
}

/* get command of the packet */
static inline uint16_t get_command_host(const struct list_packet *packet)
{
	assert(packet);
	return packet->packet.cmd;
	/* TODO: need to change to network byte order */
	//return ntohs(packet->packet.cmd);
}

/* get uin of the packet */
static inline uint32_t get_uin_host(const struct list_packet *packet)
{
	assert(packet);
	return packet->packet.uin;
	/* TODO: need to change to network byte order */
	//return ntohl(packet->packet.uin);
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
	list_add_tail(&packet->list, keep_alive_list);
}

static inline void add_offline_packet(struct list_head *offline_list,
		struct list_packet *packet)
{
	list_add_tail(&packet->list, offline_list);
}

#endif
