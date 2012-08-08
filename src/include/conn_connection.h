#ifndef _CONN_CONNECTION_H_
#define _CONN_CONNECTION_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "conn_list.h"

#define NOT_LOGIN_CONNECTION		1
#define LOGIN_OK_CONNECTION		2
#define LOGIN_UNCOMPLETE_CONNECTION	3
#define LOGIN_ERROR_CONNECTION		4

/* connection structure for each connection */
struct connection {
	struct list_head timer_list;
	struct list_head recv_packet_list;
	struct list_head send_packet_list;
	uint32_t uin;
	int sfd;
	uint16_t expect_bytes;
	uint8_t length[2];
	uint8_t type;
	bool length_incomplete;
};

/* uin to *conn hash map entry */
struct uin_entry {
	uint32_t uin;
	struct connection *conn;
};

/* socket fd to *conn hash map entry */
struct fd_entry {
	int fd;
	struct connection *conn;
};

static inline void conn_init(struct connection *conn)
{
	assert(conn);
	memset(conn, 0, sizeof(struct connection));
	INIT_LIST_HEAD(&conn->timer_list);
	INIT_LIST_HEAD(&conn->recv_packet_list);
	INIT_LIST_HEAD(&conn->send_packet_list);
	conn->type = NOT_LOGIN_CONNECTION;
}

#endif
