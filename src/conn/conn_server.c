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

/* set socket to non blocking */
static int set_nonblocking(int sfd)
{
	int flags, ret;

	if ((flags = fcntl(sfd, F_GETFL, 0)) < 0) {
		log_err("can not get socket lock\n");
		return -1;
	}

	flags |= O_NONBLOCK;
	if (fcntl(sfd, F_SETFL, flags) < 0) {
		log_err("can not set socket lock\n");
		return -1;
	}

	return 0;
}

/* create listen socket, and bind it to the port */
static int create_and_bind(uint16_t port)
{
	struct sockaddr_in serv_addr;
	int sfd;

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		log_err("create socket failed\n");
		return -1;
	}

	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	if (bind(sfd, (struct sockaddr *)&serv_addr,
				sizeof(struct sockaddr_in)) < 0) {
		log_err("bind socket error\n");
		return -1;
	}

	return sfd;
}

/* accept the connection */
static int accept_handler(struct conn_server *server)
{
	struct epoll_event event;
	while (1) {
		struct sockaddr in_addr;
		int infd;

		if ((infd = accept(server->sfd, NULL, NULL)) < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				/* we have processed all incoming connections */
				return 0;
			} else {
				log_err("accept connection error\n");
				return -1;
			}
		}

		if (set_nonblocking(infd) < 0) {
			log_err("set infd to nonblockint mode error\n");
			return -1;
		}

		event.data.fd = infd;
		event.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(server->efd, EPOLL_CTL_ADD, infd, &event) < 0) {
			log_err("add fd to monitor error\n");
			return -1;
		}

		struct connection *conn = allocator_malloc(&server->conn_allocator);
		conn_init(conn);
		conn->sfd = infd;
		struct fd_entry fd_conn = {infd, conn};
		hset_insert(&server->fd_conn_map, &fd_conn);
		timer_add(&server->timer, conn);
	}
}

/* read data from the socket */
static int read_handler(struct conn_server *server, int infd)
{
	int done = 0;
	ssize_t count;
	char buf[8192];

	while (1) {
		memset(buf, 0, sizeof(buf));
		if ((count = read(infd, buf, sizeof(buf))) < 0) {
			if (errno != EAGAIN) {
				log_err("read data error\n");
				goto read_fail;
			}
			goto read_success;
		} else if (count == 0) {
			/* End of file, The remote has closed the connection */
			goto read_fail;
		}

		/* read data success */
		if (count < 2) {
			log_alert("we only read 1 byte here, need to hanble this situation\n");
			goto read_fail;
		}

		uint16_t length = ntohs(*(uint16_t *)buf);
		if (length > MAX_PACKET_LEN) {
			log_err("length field is too big\n");
			goto read_fail;
		}

		if (length != count) {
			log_alert("read packet uncomplete\n");
			goto read_fail;
		}

		/* use fd to find connection */
		iterator_t it;
		hset_find(&server->fd_conn_map, &infd, &it);
		if (!it.data) {
			log_err("can not find connection\n");
			goto read_fail;
		}

		/* add packet to conn's receive packet list */
		struct connection *conn = ((struct fd_entry *)it.data)->conn;
		struct list_packet *packet =
			allocator_malloc(&server->packet_allocator);
		packet_init(packet);
		memcpy(&packet->packet, buf, length);
		list_add_tail(&packet->list, &conn->recv_packet_list);
		continue;

read_fail:
		done = 1;
read_success:
		break;
	}

	if (done) {
		/* closed connection */
		printf("Closed connection on descriptor %d\n", infd);
	}
}

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
}

/* prepare the socket */
int setup_socket(struct conn_server *server, uint16_t port)
{
	assert(server);

	server->port = port;
	if ((server->sfd = create_and_bind(port)) < 0) {
		log_err("create socket error\n");
		return -1;
	}

	if (set_nonblocking(server->sfd) < 0) {
		log_err("can not set socket to nonblocking mode\n");
		close(server->sfd);
		return -1;
	}

	if (listen(server->sfd, SOMAXCONN) < 0) {
		log_err("can not listen on socket\n");
		close(server->sfd);
		return -1;
	}

	return 0;
}

/* prepare the epoll monitor fd */
int setup_epoll(struct conn_server *server, uint32_t max_events)
{
	assert(server && server->sfd != -1 && max_events > 0);

	struct epoll_event event;

	if ((server->efd = epoll_create1(0)) < 0) {
		log_err("create epoll monitor fd failed\n");
		return -1;
	}

	event.data.fd = server->sfd;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(server->efd, EPOLL_CTL_ADD, server->sfd, &event) < 0) {
		log_err("can not add sfd to monitored fd set\n");
		return -1;
	}

	/* events buffer */
	server->max_events = max_events;
	server->events = calloc(max_events, sizeof(struct epoll_event));
	return 0;
}

/* the main event loop */
int epoll_loop(struct conn_server *server)
{
	struct epoll_event *events = server->events;
	int efd = server->efd;
	int max_events = server->max_events;

	/* the event loop */
	while (1) {
		int i, n;
		n = epoll_wait(efd, events, max_events, -1);
		for (i = 0; i < n; i++) {
			if ((events[i].events & EPOLLERR) ||
					(events[i].events & EPOLLHUP) ||
					(!(events[i].events & EPOLLIN))) {
				/* an error has occured on this fd */
				log_err("epoll error\n");
				close(events[i].data.fd);
				continue;
			} else if (server->sfd == events[i].data.fd) {
				/* one or more incoming connections */
				accept_handler(server);
			} else {
				/* we have data on the fd waiting to be read */
				read_handler(server, events[i].data.fd);
			}
		}
	}

	free(events);
	close(server->sfd);
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
