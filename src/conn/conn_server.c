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

int conn_server_init(struct conn_server *server)
{
	assert(server);

	memset(server, 0, sizeof(struct conn_server));
	timer_init(&server->timer);
	timer = &server->timer;
	INIT_LIST_HEAD(&server->keep_alive_list);
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
			} else {
				/* we have data on the fd waiting to be read */
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

	LOG_INIT("conn_log");
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
