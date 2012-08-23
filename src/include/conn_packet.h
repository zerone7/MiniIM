#ifndef _CONN_PACKET_H_
#define _CONN_PACKET_H_

#include <assert.h>
#include <stdint.h>
#include "list.h"
#include "list_packet.h"
#include "conn_connection.h"

#define STATUS_CHANGE_OFFLINE	0x00
#define STATUS_CHANGE_ONLINE	0x01

struct conn_server;

void close_connection(struct conn_server *server, struct connection *conn);
void send_offline_to_status(struct conn_server *server, uint32_t uin);
void send_conn_info_to_message(struct conn_server *server);

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
