#include "conn_packet.h"
#include "conn_list.h"
#include "conn_server.h"
#include "conn_log.h"
#include "conn_network.h"

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
	int fd = conn->sfd;
	close(fd);
	hset_erase(&server->fd_conn_map, &fd);

	/* remove from uin-conn hash map */
	hset_erase(&server->uin_conn_map, &conn->uin);

	/* remove from timer */
	timer_del(conn);
	conn_destroy(server, conn);

	/* free the conn struct */
	allocator_free(&server->conn_allocator, conn);
}

/* send offline message to status server */
void send_offline_to_status(struct conn_server *server, uint32_t uin)
{
	struct list_packet *packet = allocator_malloc(&server->packet_allocator);
	packet_init(packet);
	struct packet *p = &packet->packet;
	/* TODO: need to change to network byte order */
	p->len = (22);
	p->ver = (0x01);
	p->cmd = (CMD_STATUS_CHANGE);
	*((uint32_t *)((char *)p + 12)) = uin;
	*((uint16_t *)((char *)p + 20)) = (STATUS_CHANGE_OFFLINE);
	/*p->len = htons(18);
	p->ver = htons(0x01);
	p->cmd = htons(CMD_STATUS_CHANGE);
	p->uin = htonl(uin);
	*((uint16_t *)(p + 16)) = htons(STATUS_CHANGE_OFFLINE);*/
	list_add_tail(&packet->list, &(server->status_conn.send_packet_list));
	wait_for_write(server->efd, server->status_conn.sfd);
}

/* send conn's ip and port which used to connect to user server
 * to mesage server */
void send_conn_info_to_message(struct conn_server *server)
{
	struct list_packet *lp = allocator_malloc(&server->packet_allocator);
	packet_init(lp);
	struct packet *p = &lp->packet;
	p->len = 18;
	p->ver = 0x01;
	p->cmd = CMD_CONN_INFO;
	*((uint32_t *)((char *)p + 12)) = server->conn_user_ip;
	*((uint16_t *)((char *)p + 16)) = server->conn_user_port;
	list_add_tail(&lp->list, &(server->message_conn.send_packet_list));
}

/* client packet handler */
void cmd_packet_handler(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	uint16_t command = get_command_host(packet);
	/* TODO: need to enable login type check */
	/*if (conn->type != LOGIN_OK_CONNECTION &&
			command != CMD_LOGIN) {*/
		/* the client not login, ignore this packet */
		/*allocator_free(&server->packet_allocator, packet);
		return;
	}*/

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
	conn->uin = get_uin_host(packet);
	/* NOTE: this is used for test */
	struct uin_entry entry = {conn->uin, conn};
	hset_insert(&server->uin_conn_map, &entry);
	cmd_user(server, conn, packet);
}

void cmd_logout(struct conn_server *server, struct connection *conn,
		struct list_packet *packet)
{
	allocator_free(&server->packet_allocator, packet);

	/* send status change command to status server */
	send_offline_to_status(server, get_uin_host(packet));
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
	uint32_t uin = get_uin_host(packet);
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
	uint32_t uin = get_uin_host(packet);
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
	uint32_t uin = get_uin_host(packet);
	struct connection *conn = get_conn_by_uin(server, uin);
	/* TODO: need to check login status */
	if (!conn /*|| conn->type != LOGIN_OK_CONNECTION*/) {
		allocator_free(&server->packet_allocator, packet);
		return;
	}

	list_add_tail(&packet->list, &conn->send_packet_list);
	wait_for_write(server->efd, conn->sfd);
}
