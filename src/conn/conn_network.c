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
				log_warning("accept connection error\n");
				return -1;
			}
		}

		if (set_nonblocking(infd) < 0) {
			log_warning("set infd to nonblockint mode error\n");
			close(infd);
			return -1;
		}

		event.data.fd = infd;
		event.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(server->efd, EPOLL_CTL_ADD, infd, &event) < 0) {
			log_warning("add fd to monitor error\n");
			close(infd);
			return -1;
		}

		struct connection *conn = allocator_malloc(&server->conn_allocator);
		conn_init(conn);
		conn->sfd = infd;
		struct fd_entry fd_conn = {infd, conn};
		/* insert conn to fd_conn map */
		hset_insert(&server->fd_conn_map, &fd_conn);
		/* insert conn to timer */
		timer_add(&server->timer, conn);
	}
}

/* 1st:
 * last packet is incomplete, but we have got the length field,
 * just read the left data, and copy them to the last packet
 * in receive queue */
static int last_packet_incomplete(struct conn_server *server,
		struct connection *conn, const char *buf, int count)
{
	if (list_empty(&conn->recv_packet_list)) {
		log_err("incomplete packet missing\n");
		return -1;
	}

	struct list_head *last = conn->recv_packet_list.prev;
	struct list_packet *packet =
		list_entry(last, struct list_packet, list);
	int have_read = get_length_host(packet) - conn->expect_bytes;
	if (have_read < 0) {
		log_err("imcomplete packet length wrong\n");
		return -1;
	}

	int read_bytes = (count < conn->expect_bytes) ?
		count : conn->expect_bytes;
	memcpy(&packet->packet + have_read, buf, read_bytes);
	if (count < conn->expect_bytes) {
		conn->expect_bytes -= count;
	} else {
		conn->expect_bytes = 0;
	}

	return read_bytes;
}

/* 2nd:
 * last packet is incomplete, we have only got _ONE_ byte, which
 * means the length field is incomplete too, we should get the
 * length first, and malloc a packet, copy 2 bytes length field
 * to it, then we go to 1st */
static int last_packet_incomplete_1byte(struct conn_server *server,
		struct connection *conn, const char *buf, int count)
{
	int read_bytes = 1;
	memcpy(conn->length + 1, buf, read_bytes);

	int packet_length = ntohs(*((uint16_t *)conn->length));
	if (packet_length > MAX_PACKET_LEN) {
		log_err("packet length field is too git\n");
		return -1;
	}

	struct list_packet *packet =
		allocator_malloc(&server->packet_allocator);
	packet_init(packet);
	memcpy(&packet->packet, conn->length, 2);
	conn->length_incomplete = false;
	conn->expect_bytes = packet_length - 2;
	list_add_tail(&packet->list, &conn->recv_packet_list);
	return read_bytes;
}

/* 3rd:
 * last packet complete, it's a new packet now! but if count
 * is _ONE_ byte, then we go to 2nd , if count less than
 * packet_length, we go to 1st */
static int last_packet_complete(struct conn_server *server,
		struct connection *conn, const char *buf, int count)
{
	int read_bytes;
	if (count < 2) {
		read_bytes = count;
		conn->length_incomplete = true;
		memcpy(conn->length, buf, read_bytes);
	} else {
		int packet_length = ntohs(*((uint16_t *)buf));
		if (packet_length > MAX_PACKET_LEN) {
			log_err("packet length field is too git\n");
			return -1;
		}

		struct list_packet *packet =
			allocator_malloc(&server->packet_allocator);
		packet_init(packet);
		read_bytes = (count < packet_length) ? count : packet_length;
		memcpy(&packet->packet, buf, read_bytes);
		if (count < packet_length) {
			conn->expect_bytes = packet_length - read_bytes;
		}
		list_add_tail(&packet->list, &conn->recv_packet_list);
	}

	return read_bytes;
}

/* generate packet when we read data from fd */
static int fill_packet(struct conn_server *server, struct connection *conn,
		const char *buf, int count)
{
	int read_bytes;
	while (count > 0) {
		/* last packet read incomplete, but we konw the packet length */
		if (conn->expect_bytes > 0) {
			read_bytes = last_packet_incomplete(server,
					conn, buf, count);
		} else if (conn->length_incomplete) {
			/* last packet read incomplete,
			 * we don't know the packet length either */
			read_bytes = last_packet_incomplete_1byte(server,
					conn, buf, count);
		} else {
			read_bytes = last_packet_complete(server,
					conn, buf, count);
		}

		if (read_bytes < 0) {
			log_err("read packet error\n");
			return -1;
		}
		buf += read_bytes;
		count -= read_bytes;
	}

	/* we have read overflow */
	return (count < 0) ? -1 : 0;
}

/* read data from the socket */
static int read_handler(struct conn_server *server, int infd)
{
	bool err = false;
	ssize_t count;
	char buf[8192];
	struct connection *conn = NULL;

	/* use fd to find connection */
	iterator_t it;
	hset_find(&server->fd_conn_map, &infd, &it);
	if (!it.data) {
		log_err("can not find connection\n");
		err = true;
		/* can not find conn in hash map, need to close the connection */
	} else {
		conn = ((struct fd_entry *)it.data)->conn;
	}

	/* use fucntion fill_packet to generate packet */
	while (!err) {
		memset(buf, 0, sizeof(buf));
		if ((count = read(infd, buf, sizeof(buf))) < 0) {
			if (errno != EAGAIN) {
				log_err("read data error\n");
				err = true;
			}
			break;
		} else if (count == 0) {
			/* End of file, The remote has closed the connection */
			err = true;
		} else {
			if (fill_packet(server, conn, buf, count) < 0) {
				log_err("read packet error\n");
				err = true;
			}
		}
	}

	if (err) {
		/* closed connection */
		printf("Closed connection on descriptor %d\n", infd);
		close(infd);
	}
	return 0;
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
				if (accept_handler(server) < 0) {
					log_warning("accept connection error\n");
				}
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
