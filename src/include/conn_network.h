#ifndef _CONN_NETWORK_H_
#define _CONN_NETWORK_H_

#include <stdint.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "conn_log.h"
#include "conn_server.h"

int setup_socket(struct conn_server *server, uint16_t port);
int setup_epoll(struct conn_server *server, uint32_t max_events);
int epoll_loop(struct conn_server *server);

/* set socket to non blocking */
static inline int set_nonblocking(int sfd)
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

static inline int add_to_epoll(int efd, int sfd)
{
	struct epoll_event event;
	if (set_nonblocking(sfd) < 0) {
		log_warning("set infd to nonblockint mode error\n");
		close(sfd);
		return -1;
	}

	event.data.fd = sfd;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event) < 0) {
		log_warning("add fd to monitor error\n");
		close(sfd);
		return -1;
	}

	return 0;
}

/* wait for epoll in event */
static inline int wait_for_read(int epoll_fd, int socket_fd)
{
	struct epoll_event event;

	event.events = EPOLLIN | EPOLLET;
	event.data.fd = socket_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket_fd, &event) < 0) {
		log_err("can not change to wait for epoll in on socket %d\n",
				socket_fd);
		return -1;
	}

	return 0;
}

/* wair for epoll out event */
static inline int wait_for_write(int epoll_fd, int socket_fd)
{
	struct epoll_event event;

	event.events = EPOLLOUT | EPOLLET;
	event.data.fd = socket_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket_fd, &event) < 0) {
		log_err("can not change to wait for epoll out on socket %d\n",
				socket_fd);
		return -1;
	}

	return 0;
}

#endif
