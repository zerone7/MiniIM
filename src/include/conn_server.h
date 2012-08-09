#ifndef _CONN_SERVER_H_
#define _CONN_SERVER_H_

#include <stdint.h>
#include <sys/epoll.h>
#include "conn_list.h"
#include "conn_hash.h"
#include "conn_timer.h"
#include "conn_allocator.h"

struct conn_server {
	struct conn_timer timer;	/* timer struct */
	struct list_head keep_alive_list;	/* keep alive packet queue */
	struct allocator packet_allocator;
	struct allocator conn_allocator;
	struct connection user_conn;
	struct connection contact_conn;
	struct connection status_conn;
	struct connection message_conn;
	struct epoll_event *events;
	hash_set_t uin_conn_map;
	hash_set_t fd_conn_map;
	uint32_t max_events;	/* max events we can monitor */
	int sfd;		/* listen socket fd */
	int efd;		/* epoll monitor fd */
	int user_fd;		/* socket connect to user server */
	int contact_fd;		/* socket connect to contact server */
	int status_fd;		/* socket connect to status server */
	int message_fd;		/* socket connect to message server */
	uint16_t port;		/* listen port */
};

int conn_server_init(struct conn_server *server);

#endif
