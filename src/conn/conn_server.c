#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <netinet/in.h>
#include "conn_define.h"
#include "conn_log.h"
#include "conn_server.h"
#include "conn_packet.h"
#include "conn_connection.h"

int conn_server_init(struct conn_server *server)
{
	assert(server);

	memset(server, 0, sizeof(struct conn_server));
	timer_init(&server->timer);
	allocator_init(&server->packet_allocator, LIST_PACKET_SIZE);
	allocator_init(&server->conn_allocator, sizeof(struct connection));

	/* initialize global viariable timer, which is used by signal handler */
	timer = &server->timer;
	INIT_LIST_HEAD(&server->keep_alive_list);

	/* initialize uin to connection hash map, set uin to be key */
	HSET_INIT(&server->uin_conn_map, sizeof(struct uin_entry));
	__set_key_size(&server->uin_conn_map, sizeof(uint32_t));

	/* initialize socket fd to connection hash map, set socket fd to be key */
	HSET_INIT(&server->fd_conn_map, sizeof(struct fd_entry));
	__set_key_size(&server->fd_conn_map, sizeof(int));
	return 0;
}

int main(int argc, char *argv[])
{
	struct conn_server server;

	LOG_INIT("log_conn");
	conn_server_init(&server);

	/* TODO: connect to user server */

	/* TODO: connect to contact server */

	/* TODO: connect to status server */

	/* TODO: connect to message server */

	if (setup_socket(&server, CONN_SERVER_PORT) < 0) {
		log_err("setup listen socket error\n");
		return 0;
	}

	if (setup_epoll(&server, MAX_EPOLL_EVENTS) < 0) {
		log_err("setup epoll error\n");
		return 0;
	}

	epoll_loop(&server);

	LOG_DESTROY();
	return 0;
}
