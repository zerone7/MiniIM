#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "log.h"

static inline void get_sock_info(int sockfd, uint32_t *ip, uint16_t *port)
{
	struct sockaddr_in addr;
	socklen_t len;

	len = sizeof(struct sockaddr_in);
	getsockname(sockfd, (struct sockaddr *)&addr, &len);
	*ip= ntohl(addr.sin_addr.s_addr);
	*port = ntohs(addr.sin_port);
}

static inline int connect_to_server(const char *ip, uint16_t port)
{
	struct sockaddr_in addr;
	int fd;

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		log_err("create socket error\n");
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
		log_err("connect to server %s error\n", ip);
		return -1;
	}
	return fd;
}

/* set socket to non blocking */
static inline int set_nonblocking(int socket_fd)
{
	int flags;

	if ((flags = fcntl(socket_fd, F_GETFL, 0)) < 0) {
		log_err("can not get socket lock\n");
		return -1;
	}

	flags |= O_NONBLOCK;
	if (fcntl(socket_fd, F_SETFL, flags) < 0) {
		log_err("can not set socket lock\n");
		return -1;
	}

	return 0;
}

static inline int add_to_epoll(int epoll_fd, int socket_fd)
{
	struct epoll_event event;
	if (set_nonblocking(socket_fd) < 0) {
		log_warning("set infd to nonblockint mode error\n");
		close(socket_fd);
		return -1;
	}

	event.data.fd = socket_fd;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) < 0) {
		log_warning("add fd to monitor error\n");
		close(socket_fd);
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
