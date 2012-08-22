#include <assert.h>
#include <string.h>
#include "modules.h"
#include "conn_log.h"
#include "conn_server.h"
#include "conn_packet.h"
#include "conn_timer.h"
#include "conn_network.h"

FILE *log_fp = NULL;

int conn_server_init(struct conn_server *server)
{
	assert(server);

	memset(server, 0, sizeof(struct conn_server));
	timer_init(&server->timer);
	allocator_init(&server->packet_allocator, LIST_PACKET_SIZE);
	allocator_init(&server->conn_allocator, sizeof(struct connection));
	conn_init(&server->user_conn);
	conn_init(&server->contact_conn);
	conn_init(&server->status_conn);
	conn_init(&server->message_conn);

	INIT_LIST_HEAD(&server->keep_alive_list);

	/* initialize uin to connection hash map, set uin to be key */
	HSET_INIT(&server->uin_conn_map, sizeof(struct uin_entry));
	__set_key_size(&server->uin_conn_map, sizeof(uint32_t));

	/* initialize socket fd to connection hash map, set socket fd to be key */
	HSET_INIT(&server->fd_conn_map, sizeof(struct fd_entry));
	__set_key_size(&server->fd_conn_map, sizeof(int));

	inet_pton(AF_INET, CONN_USER_IP, &server->conn_user_ip);
	server->conn_user_ip = ntohl(server->conn_user_ip);
	server->conn_user_port = CONN_USER_PORT;

	return 0;
}

int main(int argc, char *argv[])
{
	struct conn_server server;

	LOG_INIT("stdout");
	conn_server_init(&server);

	/* connect to user server */
	int fd;
	if ((fd = connect_bind_to_server(USER_IP, USER_PORT,
					CONN_USER_IP, CONN_USER_PORT)) < 0) {
		log_err("can not connect to user server %s(%hu)\n",
				USER_IP, USER_PORT);
		return 0;
	}
	server.user_conn.sfd = fd;
	log_notice("connect to user server %s(%hu)\n",
			USER_IP, USER_PORT);

	/* connect to contact server */
	if ((fd = connect_to_server(FRIEND_IP, FRIEND_PORT)) < 0) {
		log_err("can not connect to contact server %s(%hu)\n",
				FRIEND_IP, FRIEND_PORT);
		return 0;
	}
	server.contact_conn.sfd = fd;
	log_notice("connect to contact server %s(%hu)\n",
			FRIEND_IP, FRIEND_PORT);

	/* connect to status server */
	if ((fd = connect_to_server(STATUS_IP, STATUS_PORT)) < 0) {
		log_err("can not connect to status server %s(%hu)\n",
				STATUS_IP, STATUS_PORT);
		return 0;
	}
	server.status_conn.sfd = fd;
	log_notice("connect to status server %s(%hu)\n",
			STATUS_IP, STATUS_PORT);

	/* connect to message server */
	if ((fd = connect_to_server(MESSAGE_IP, MESSAGE_PORT)) < 0) {
		log_err("can not connect to message server %s(%hu)\n",
				MESSAGE_IP, MESSAGE_PORT);
		return 0;
	}
	server.message_conn.sfd = fd;
	log_notice("connect to message server %s(%hu)\n",
			MESSAGE_IP, MESSAGE_PORT);

	if (setup_socket(&server, CONN_SERVER_PORT) < 0) {
		log_err("setup listen socket error\n");
		return 0;
	}
	log_notice("setup listen socket %d success\n", server.sfd);

	if (setup_epoll(&server, MAX_EPOLL_EVENTS) < 0) {
		log_err("setup epoll error\n");
		return 0;
	}
	log_notice("setup epoll on socket %d success\n", server.sfd);
	add_to_epoll(server.efd, server.user_conn.sfd);
	add_to_epoll(server.efd, server.contact_conn.sfd);
	add_to_epoll(server.efd, server.status_conn.sfd);
	add_to_epoll(server.efd, server.message_conn.sfd);
	send_conn_info_to_message(&server);

	log_notice("starting epoll loop\n");
	epoll_loop(&server);

	LOG_DESTROY();
	return 0;
}
