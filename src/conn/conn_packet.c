#include "list.h"
#include "conn_packet.h"
#include "conn_server.h"
#include "conn_log.h"
#include "conn_network.h"
#include "conn_connection.h"

/* this function ONLY removes the packets, does not remove the timer */
void conn_destroy(struct conn_server *server, struct connection *conn)
{
	/* remove recv packets */
	struct list_head *head = &conn->recv_packet_list;
	struct list_packet *p;
	while (!list_empty(head)) {
		p = list_first_entry(head, struct list_packet, list);
		list_del(&p->list);
		allocator_free(&server->packet_allocator, p);
	}

	/* remove send packets */
	head = &conn->send_packet_list;
	while (!list_empty(head)) {
		p = list_first_entry(head, struct list_packet, list);
		list_del(&p->list);
		allocator_free(&server->packet_allocator, p);
	}
}

/* user logout, client timeout or something error, we need to
 * close the connection and release resource */
void close_connection(struct conn_server *server, struct connection *conn)
{
	/* remove from fd-conn hash map */
	close(conn->sfd);

	struct fd_entry *fd_entry;
	HASH_FIND_INT(server->fd_conn_map, &conn->sfd, fd_entry);
	if (fd_entry) {
		HASH_DEL(server->fd_conn_map, fd_entry);
		free(fd_entry);
	}

	/* remove from uin-conn hash map */
	struct uin_entry *uin_entry;
	HASH_FIND_INT(server->uin_conn_map, &conn->uin, uin_entry);
	if (uin_entry) {
		HASH_DEL(server->uin_conn_map, uin_entry);
		free(uin_entry);
	}

	/* remove from timer */
	timer_del(conn);
	conn_destroy(server, conn);

	/* free the conn struct */
	allocator_free(&server->conn_allocator, conn);
}

/* send offline message to status server */
void send_offline_to_status(struct conn_server *server, uint32_t uin)
{
	struct list_packet *lp = allocator_malloc(&server->packet_allocator);
	packet_init(lp);
	set_length(lp, 24);
	set_command(lp, CMD_STATUS_CHANGE);
	set_uin(lp, uin);
	set_field_uint32_t(get_parameters(lp), 0, uin);
	set_field_uint16_t(get_parameters(lp), 10,
			STATUS_CHANGE_OFFLINE);
	list_add_tail(&lp->list, &(server->status_conn.send_packet_list));
	wait_for_write(server->efd, server->status_conn.sfd);
}

/* send conn's ip and port which used to connect to user server
 * to mesage server */
void send_conn_info_to_message(struct conn_server *server)
{
	struct list_packet *lp = allocator_malloc(&server->packet_allocator);
	packet_init(lp);
	set_length(lp, 18);
	set_command(lp, CMD_CONN_INFO);
	set_field_uint32_t(get_parameters(lp), 0, server->conn_user_ip);
	set_field_uint16_t(get_parameters(lp), 4, server->conn_user_port);
	list_add_tail(&lp->list, &(server->message_conn.send_packet_list));
	wait_for_write(server->efd, server->message_conn.sfd);
}

/* client packet handler */
void cmd_packet_handler(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	uint16_t command = get_command(packet);
	/* check login status */
	if (conn->type != LOGIN_OK_CONNECTION &&
			command != CMD_LOGIN) {
		/* the client not login, ignore this packet */
		allocator_free(&server->packet_allocator, packet);
		return;
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

void cmd_keep_alive(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	/* add packet to keep alive list, wait for the timer to deal with it */
	add_keep_alive_packet(&server->keep_alive_list, packet);
}

void cmd_login(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	conn->type = LOGIN_UNCOMPLETE_CONNECTION;
	conn->uin = get_uin(packet);
	/* NOTE: this is used for test */
	struct uin_entry *uin_entry = malloc(sizeof(struct uin_entry));
	uin_entry->uin = conn->uin;
	uin_entry->conn = conn;
	HASH_ADD_INT(server->uin_conn_map, uin, uin_entry);

	cmd_user(server, conn, packet);
}

void cmd_logout(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	allocator_free(&server->packet_allocator, packet);

	/* send status change command to status server */
	send_offline_to_status(server, get_uin(packet));
	close_connection(server, conn);
}

/* we need to forward this packet to user server, so put it onto
 * user_conn's send queue */
void cmd_user(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	list_add_tail(&packet->list, &(server->user_conn.send_packet_list));
	wait_for_write(server->efd, server->user_conn.sfd);
}

/* we need to forward this packet to contact server, so put it onto
 * contact_conn's send queue */
void cmd_contact(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	list_add_tail(&packet->list, &(server->contact_conn.send_packet_list));
	wait_for_write(server->efd, server->contact_conn.sfd);
}

/* we need to forward this packet to message server, so put it onto
 * message_conn's send queue */
void cmd_message(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	list_add_tail(&packet->list, &(server->message_conn.send_packet_list));
	wait_for_write(server->efd, server->message_conn.sfd);
}

/* backend server packet handler */
void srv_packet_handler(struct conn_server *server, struct list_packet *packet)
{
	uint16_t command = get_command(packet);
	switch (command) {
	case SRV_ERROR:
		srv_error(server, packet);
		break;
	case SRV_LOGIN_OK:
		srv_login_ok(server, packet);
		break;
	case SRV_SET_NICK_OK:
	case SRV_ADD_CONTACT_WAIT:
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

void srv_error(struct conn_server *server, struct list_packet *packet)
{
	uint32_t uin = get_uin(packet);
	struct connection *conn = get_conn_by_uin(server, uin);
	if (!conn) {
		allocator_free(&server->packet_allocator, packet);
		return;
	}

	conn->type = LOGIN_ERROR_CONNECTION;
	list_add_tail(&packet->list, &conn->send_packet_list);
	wait_for_write(server->efd, conn->sfd);
}

/* client login successful, we mark the conn as login_ok,
 * and send the packet to client */
void srv_login_ok(struct conn_server *server, struct list_packet *packet)
{
	uint32_t uin = get_uin(packet);
	struct connection *conn = get_conn_by_uin(server, uin);
	if (!conn) {
		allocator_free(&server->packet_allocator, packet);
		return;
	}

	conn->type = LOGIN_OK_CONNECTION;
	list_add_tail(&packet->list, &conn->send_packet_list);
	wait_for_write(server->efd, conn->sfd);
}

/* other kind of packet, check clienti's login type,
 * if login ok, send the packet to client */
void srv_other_packet(struct conn_server *server, struct list_packet *packet)
{
	uint32_t uin = get_uin(packet);
	struct connection *conn = get_conn_by_uin(server, uin);
	/* need to check login status */
	if (!conn || conn->type != LOGIN_OK_CONNECTION) {
		allocator_free(&server->packet_allocator, packet);
		return;
	}

	list_add_tail(&packet->list, &conn->send_packet_list);
	wait_for_write(server->efd, conn->sfd);
}
