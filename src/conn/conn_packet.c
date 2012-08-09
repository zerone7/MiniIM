#include "conn_packet.h"
#include "conn_list.h"
#include "conn_server.h"
#include "conn_log.h"

/* client packet handler */
int cmd_packet_handler(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	uint16_t command = get_command_host(packet);
	if (conn->type != LOGIN_OK_CONNECTION &&
			command != CMD_LOGIN) {
		close_connection(server, conn);
		allocator_free(&server->packet_allocator, packet);
	}

	switch (command) {
	case CMD_KEEP_ALIVE:
		cmd_keep_alive(server, conn, packet);
		break;
	case CMD_LOGIN:
		cmd_login(server, conn, packet);
		break;
	case CMD_LOGOUT:
		cmd_logout(server, conn, packet);
		break;
	case CMD_SET_NICK:
		cmd_user(server, conn, packet);
		break;
	case CMD_ADD_CONTACT:
	case CMD_ADD_CONTACT_REPLY:
	case CMD_CONTACT_LIST:
	case CMD_CONTACT_INFO_MULTI:
		cmd_contact(server, conn, packet);
		break;
	case CMD_MESSAGE:
	case CMD_OFFLINE_MSG:
		cmd_message(server, conn, packet);
		break;
	default:
		log_err("unkonwn command %#hx\n", command);
		break;
	}
}

/* user logout, client timeout or something error, we need to
 * close the connection and release resource */
void close_connection(struct conn_server *server, struct connection *conn)
{
	/* remove from fd-conn hash map */
	int fd = conn->sfd;
	close(fd);
	hset_erase(&server->fd_conn_map, &fd);

	/* remove from uin-conn hash map */
	uint32_t uin = conn->uin;
	if (uin > 0) {
		hset_erase(&server->uin_conn_map, &uin);
	}

	/* remove from timer */
	timer_del(conn);
	/* free the conn struct */
	allocator_free(&server->conn_allocator, conn);
}

int cmd_keep_alive(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	/* add packet to keep alive list, wait for the timer to deal with it */
	add_keep_alive_packet(&server->keep_alive_list, packet);
}

int cmd_login(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	conn->type = LOGIN_UNCOMPLETE_CONNECTION;
	cmd_user(server, conn, packet);
}

int cmd_logout(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	allocator_free(&server->packet_allocator, packet);
	close_connection(server, conn);

	/* TODO: send status change command to status server */
}

int cmd_user(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	list_add(&packet->list, &(server->user_conn.send_packet_list));
}

/* TODO: need to complete */
int cmd_contact(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
}

/* TODO: need to complete */
int cmd_message(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
}

/* backend server packet handler */
int srv_packet_handler(struct conn_server *server, struct list_packet *packet)
{
	uint16_t command = get_command_host(packet);
	switch (command) {
	case SRV_ERROR:
		srv_error(server, packet);
		break;
	case SRV_LOGIN_OK:
		srv_login_ok(server, packet);
		break;
	case SRV_SET_NICK_OK:
	case SRV_ADD_CONTACT_WAIT:
	case SRV_ADD_CONTACT_AUTH:
	case SRV_ADD_CONTACT_REPLY:
	case SRV_CONTACT_LIST:
	case SRV_CONTACT_INFO_MULTI:
	case SRV_MESSAGE:
	case SRV_OFFLINE_MSG:
	case SRV_OFFLINE_MSG_DONE:
		srv_other_packet(server, packet);
		break;
	default:
		log_err("unkonwn command %#hx\n", command);
		break;
	}
}

/* TODO: need to complete */
int srv_error(struct conn_server *server, struct list_packet *packet)
{
}

/* TODO: need to complete */
int srv_login_ok(struct conn_server *server, struct list_packet *packet)
{
}

/* TODO: need to complete */
int srv_other_packet(struct conn_server *server, struct list_packet *packet)
{
}
