#ifndef _CONN_SERVER_H_
#define _CONN_SERVER_H_

#include <stdint.h>
#include <sys/epoll.h>
#include "list.h"
#include "uthash.h"
#include "conn_timer.h"
#include "conn_allocator.h"
#include "conn_connection.h"

struct conn_server {
	struct conn_timer timer;	/* timer struct */
	struct list_head keep_alive_list;	/* keep alive packet queue */
	struct allocator packet_allocator;
	struct allocator conn_allocator;
	struct connection user_conn;
	struct connection contact_conn;
	struct connection status_conn;
	struct connection message_conn;
	struct uin_entry *uin_conn_map;
	struct fd_entry *fd_conn_map;
	struct epoll_event *events;
	uint32_t max_events;	/* max events we can monitor */
	int sfd;		/* listen socket fd */
	int efd;		/* epoll monitor fd */
	int user_fd;		/* socket connect to user server */
	int contact_fd;		/* socket connect to contact server */
	int status_fd;		/* socket connect to status server */
	int message_fd;		/* socket connect to message server */
	uint32_t conn_user_ip;
	uint16_t conn_user_port;
	uint16_t port;		/* listen port */
};

int conn_server_init(struct conn_server *server);

/* use fd to lookup conn, only return the _SAFE_ conn */
static inline struct connection* get_conn_by_fd(struct conn_server *server,
		int fd)
{
	if (fd == server->user_conn.sfd) {
		return &server->user_conn;
	} else if (fd == server->contact_conn.sfd) {
		return &server->contact_conn;
	} else if (fd == server->status_conn.sfd) {
		return &server->status_conn;
	} else if (fd == server->message_conn.sfd) {
		return &server->message_conn;
	}

	struct fd_entry *fd_entry;
	HASH_FIND_INT(server->fd_conn_map, &fd, fd_entry);
	return fd_entry ? fd_entry->conn : NULL;
}

/* use uin to lookup conn, only return the _SAFE_ conn */
static inline struct connection* get_conn_by_uin(struct conn_server *server,
		uint32_t uin)
{
	struct uin_entry *uin_entry;
	HASH_FIND_INT(server->uin_conn_map, &uin, uin_entry);
	return uin_entry ? uin_entry->conn : NULL;
}

#endif
