#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "protocol.h"
#include "list_packet.h"
#include "packet_dump.h"
#include "network.h"
#include "client_user.h"

/* 1st:
 * last packet is incomplete, but we have got the length field,
 * just read the left data, and copy them to the last packet
 * in receive queue */
static int last_packet_incomplete(struct packet_reader *reader,
		const char *buf, int count)
{
	if (!reader->lp) {
		log_warning("incomplete packet missing\n");
		reader->expect_bytes = 0;
		return -1;
	}

	struct list_packet *packet = reader->lp;
	int packet_length = get_length(packet);
	if (packet_length < PACKET_HEADER_LEN) {
		log_warning("packet length field %d is too small\n",
				packet_length);
		reader->expect_bytes = 0;
		return -1;
	} else if (packet_length > MAX_PACKET_LEN) {
		log_warning("packet length field %d is too big\n",
				packet_length);
		reader->expect_bytes = 0;
		return -1;
	}

	int have_read = packet_length - reader->expect_bytes;
	if (have_read < 0) {
		log_warning("imcomplete packet length wrong\n");
		reader->expect_bytes = 0;
		return -1;
	}

	int read_bytes = (count < reader->expect_bytes) ?
		count : reader->expect_bytes;
	memcpy(&packet->packet + have_read, buf, read_bytes);
	if (count < reader->expect_bytes) {
		reader->expect_bytes -= count;
	} else {
		list_add_tail(&(reader->lp->list), &reader->recv_packet_list);
		reader->expect_bytes = 0;
		reader->lp = NULL;
		log_debug("recv packet len %hu, cmd 0x%04hx, uin %u from server\n",
				get_length(packet), get_command(packet),
				get_uin(packet));
		dump_packet(packet, RECV_PACKET, "server");
	}

	return read_bytes;
}

/* 2nd:
 * last packet is incomplete, we have only got _ONE_ byte, which
 * means the length field is incomplete too, we should get the
 * length first, and malloc a packet, copy 2 bytes length field
 * to it, then we go to 1st */
static int last_packet_incomplete_1byte(struct packet_reader *reader,
		const char *buf, int count)
{
	int read_bytes = 1;
	memcpy(reader->length + 1, buf, read_bytes);

	int packet_length = get_field_uint16_t(reader->length, 0);
	if (packet_length < PACKET_HEADER_LEN) {
		log_warning("packet length field %d is too small\n",
				packet_length);
		reader->length_incomplete = false;
		return -1;
	} else if (packet_length > MAX_PACKET_LEN) {
		log_warning("packet length field %d is too big\n",
				packet_length);
		reader->length_incomplete = false;
		return -1;
	}

	struct list_packet *packet = malloc(LIST_PACKET_SIZE);
	INIT_LIST_HEAD(&packet->list);
	memcpy(&packet->packet, reader->length, 2);
	reader->length_incomplete = false;
	reader->expect_bytes = packet_length - 2;
	reader->lp = packet;
	return read_bytes;
}

/* 3rd:
 * last packet complete, it's a new packet now! but if count
 * is _ONE_ byte, then we go to 2nd , if count less than
 * packet_length, we go to 1st */
static int last_packet_complete(struct packet_reader *reader,
		const char *buf, int count)
{
	int read_bytes;
	if (count < 2) {
		read_bytes = count;
		reader->length_incomplete = true;
		memcpy(reader->length, buf, read_bytes);
	} else {
		int packet_length = get_field_uint16_t(buf, 0);
		if (packet_length < PACKET_HEADER_LEN) {
			log_warning("packet length field %d is too small\n",
					packet_length);
			return -1;
		} else if (packet_length > MAX_PACKET_LEN) {
			log_warning("packet length field %d is too big\n",
					packet_length);
			return -1;
		}

		struct list_packet *packet = malloc(LIST_PACKET_SIZE);
		INIT_LIST_HEAD(&packet->list);
		read_bytes = (count < packet_length) ? count : packet_length;
		memcpy(&packet->packet, buf, read_bytes);
		if (count < packet_length) {
			reader->expect_bytes = packet_length - read_bytes;
			reader->lp = packet;
		} else {
			list_add_tail(&packet->list, &reader->recv_packet_list);
			log_debug("recv packet len %hu, cmd 0x%04hx, uin %u from server\n",
					get_length(packet),
					get_command(packet),
					get_uin(packet));
			dump_packet(packet, RECV_PACKET, "server");
		}
	}

	return read_bytes;
}

/* generate packet when we read data from fd */
int read_packet(struct packet_reader *reader,
		const char *buf, int count)
{
	int read_bytes;
	while (count > 0) {
		/* last packet read incomplete, but we konw the packet length */
		if (reader->expect_bytes > 0) {
			read_bytes = last_packet_incomplete(reader, buf, count);
		} else if (reader->length_incomplete) {
			/* last packet read incomplete,
			 * we don't know the packet length either */
			read_bytes = last_packet_incomplete_1byte(reader, buf, count);
		} else {
			read_bytes = last_packet_complete(reader, buf, count);
		}

		if (read_bytes < 0) {
			return -1;
		}
		buf += read_bytes;
		count -= read_bytes;
	}

	/* we have read overflow */
	return (count < 0) ? -1 : 0;
}

void client_user_init(struct client_user *user)
{
	packet_reader_init(&user->reader);
	INIT_LIST_HEAD(&user->wait_list);
	INIT_LIST_HEAD(&user->msg_list);
	user->contact_map = NULL;
	user->msg_map = NULL;
	user->mode = LOGIN_MODE;
}

void client_user_destroy(struct client_user *user)
{
	cmd_logout(user);
	close(user->socket);
	packet_reader_destory(&user->reader);

	/* free all waiting packets */
	struct list_packet *current, *tmp;
	list_for_each_entry_safe(current, tmp, get_wait_list(user), list) {
		list_del(&current->list);
		free(current);
	}

	/* free all msg packets */
	list_for_each_entry_safe(current, tmp, get_msg_list(user), list) {
		list_del(&current->list);
		free(current);
	}

	/* free all messages */
	struct message_map *msg_map, *msg_map_tmp;
	HASH_ITER(hh, user->msg_map, msg_map, msg_map_tmp) {
		struct message *msg, *tmp_msg;
		list_for_each_entry_safe(msg, tmp_msg, &msg_map->head, list) {
			list_del(&msg->list);
			message_destory(msg);
		}

		HASH_DEL(user->msg_map, msg_map);
		free(msg_map);
	}

	/* free all contacts */
	struct contact *contact, *contact_tmp;
	HASH_ITER(hh, user->contact_map, contact, contact_tmp) {
		HASH_DEL(user->contact_map, contact);
		free(contact);
	}
}

/* malloc a list_packet struct, init header field */
static struct list_packet* create_packet(uint32_t uin,
		uint16_t length, uint16_t command)
{
	int lp_length = sizeof(struct list_packet) +
		length - PACKET_HEADER_LEN;
	struct list_packet *lp = malloc(lp_length);
	memset(lp, 0, lp_length);
	INIT_LIST_HEAD(&lp->list);
	set_length(lp, length);
	set_version(lp, PROTOCOL_VERSION);
	set_command(lp, command);
	set_uin(lp, uin);
	return lp;
}

/* send packet to server */
static inline int send_packet(struct client_user *user, struct list_packet *lp)
{
	int length = get_length(lp);
	if (length != send(user->socket, &lp->packet, length, 0)) {
		log_err("send packet cmd 0x%04hx failed\n", get_command(lp));
		return -1;
	}

	log_debug("send packet len %hu, cmd 0x%04hx, uin %u,  to server\n",
			get_length(lp), get_command(lp), get_uin(lp));
	dump_packet(lp, SEND_PACKET, "server");

	free(lp);
	return 0;
}

/* send keep alive packet to server */
int cmd_keep_alive(struct client_user *user)
{
	uint16_t length = PACKET_HEADER_LEN;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_KEEP_ALIVE);
	return send_packet(user, lp);
}

/* send login packet to server */
int cmd_login(struct client_user *user,
		uint32_t uin, const char *password)
{
	uint16_t pass_len = strlen(password);
	assert(pass_len <= MAX_PASSWORD_LENGTH);

	uint16_t length = PACKET_HEADER_LEN + 2 + pass_len;
	struct list_packet *lp = create_packet(uin, length, CMD_LOGIN);
	cl_set_password(lp, password, pass_len);

	user->uin = uin;
	user->mode = LOGIN_MODE;

	return send_packet(user, lp);
}

/* send logout packet to server */
int cmd_logout(struct client_user *user)
{
	uint16_t length = PACKET_HEADER_LEN;
	struct list_packet *lp = create_packet(user->uin, length, CMD_LOGOUT);

	if (length != send(user->socket, &lp->packet, length, 0)) {
		log_err("send logout packet failed\n");
		return -1;
	}

	return send_packet(user, lp);
}

/* send set nick packet to server */
int cmd_set_nick(struct client_user *user, const char *nick)
{
	uint16_t nick_len = strlen(nick);
	assert(nick_len <= MAX_NICK_LENGTH);

	uint16_t length = PACKET_HEADER_LEN + 2 + nick_len + 1;
	struct list_packet *lp = create_packet(user->uin, length, CMD_SET_NICK);
	csn_set_nick(lp, nick, nick_len);

	return send_packet(user, lp);
}

/* send add contact packet to server */
int cmd_add_contact(struct client_user *user, uint32_t to_uin)
{
	uint16_t length = PACKET_HEADER_LEN + 4;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_ADD_CONTACT);
	cac_set_uin(lp, to_uin);

	return send_packet(user, lp);
}

/* send add contact reply to server */
int cmd_add_contact_reply(struct client_user *user,
		uint32_t to_uin, uint16_t reply_type)
{
	uint16_t length = PACKET_HEADER_LEN + 4 + 2;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_ADD_CONTACT_REPLY);
	cacr_set_to_uin(lp, to_uin);
	cacr_set_reply_type(lp, reply_type);

	return send_packet(user, lp);
}

/* send contact list packet to server */
int cmd_contact_list(struct client_user *user)
{
	uint16_t length = PACKET_HEADER_LEN;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_CONTACT_LIST);

	return send_packet(user, lp);
}

/* send contact info multi packet to server */
int cmd_contact_info_multi(struct client_user *user,
		uint16_t count, uint32_t *uins)
{
	assert(count <= 100);
	uint16_t length = PACKET_HEADER_LEN + 2 + count * 4;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_CONTACT_INFO_MULTI);
	ccim_set_uins(lp, uins, count);

	return send_packet(user, lp);
}

/* send message packet to server */
int cmd_message(struct client_user *user,
		uint32_t to_uin, const char *message)
{
	uint16_t msg_len = strlen(message);
	assert(msg_len <= MAX_MESSAGE_LENGTH);

	uint16_t length = PACKET_HEADER_LEN + 10 + msg_len + 1;
	struct list_packet *lp = create_packet(user->uin, length, CMD_MESSAGE);
	cm_set_to_uin(lp, to_uin);
	cm_set_message(lp, message, msg_len);

	return send_packet(user, lp);
}

/* send offline msg packet to server */
int cmd_offline_msg(struct client_user *user)
{
	uint16_t length = PACKET_HEADER_LEN;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_OFFLINE_MSG);
	return send_packet(user, lp);
}

static uint16_t get_wait_cmd(uint16_t send_cmd)
{
	switch (send_cmd) {
	case CMD_LOGIN:
		return SRV_LOGIN_OK;
	case CMD_SET_NICK:
		return SRV_SET_NICK_OK;
	case CMD_ADD_CONTACT:
		return SRV_ADD_CONTACT_WAIT;
	case CMD_CONTACT_LIST:
		return SRV_CONTACT_LIST;
	case CMD_CONTACT_INFO_MULTI:
		return SRV_CONTACT_INFO_MULTI;
	case CMD_OFFLINE_MSG:
		return SRV_OFFLINE_MSG_DONE;
	default:
		return SRV_ERROR;
	}
}

/* keep alive function called by ui */
int keep_alive(struct client_user *user)
{
	return cmd_keep_alive(user);
}

static int read_socket(struct client_user *user, char *buf, int count, int timeout)
{
	fd_set rset;
	FD_ZERO(&rset);
	int maxfdp1 = user->socket + 1;
	FD_SET(user->socket, &rset);
	timeout = (timeout < 0) ? 0 : timeout;
	struct timeval time = {timeout, 0};

	select(maxfdp1, &rset, NULL, NULL, &time);

	if (FD_ISSET(user->socket, &rset)) {
		return recv(user->socket, buf, count, 0);
	}

	return 0;
}

/* wait for reply of specific send_cmd */
static int wait_for_reply(struct client_user *user, uint16_t send_cmd)
{
	char buf[8192];
	memset(buf, 0, sizeof(buf));
	uint16_t wait_cmd = get_wait_cmd(send_cmd);
	uint16_t wait_cmd_2 = (wait_cmd == SRV_OFFLINE_MSG_DONE) ?
		SRV_OFFLINE_MSG : wait_cmd;

	int count = read_socket(user, buf, sizeof(buf), 30);
	read_packet(&user->reader, buf, count);

	struct list_head *head = get_recv_list(user);
	struct list_packet *current, *tmp;
	list_for_each_entry_safe(current, tmp, head, list) {
		uint16_t command = get_command(current);
		uint16_t client_cmd = se_get_client_cmd(current);
		list_del(&current->list);

		if (command == wait_cmd) {
			list_add_tail(&current->list, &user->wait_list);
		} else if (command == SRV_ERROR && client_cmd == send_cmd) {
			list_add_tail(&current->list, &user->wait_list);
		} else if (wait_cmd != wait_cmd_2 && command == wait_cmd_2) {
			list_add_tail(&current->list, &user->wait_list);
		} else if (command == SRV_MESSAGE) {
			list_add_tail(&current->list, &user->msg_list);
		} else {
			log_warning("receive unexpected pakcet, command %hu\n",
					command);
			free(current);
		}
	}

	return list_empty(get_wait_list(user)) ? -1 : 0;
}

/* login function called by ui */
int login(struct client_user *user, uint32_t uin,
		const char *password, char *nick)
{
	if (cmd_login(user, uin, password) < 0) {
		log_err("can not send login packet to server\n");
		return -1;
	}

	uint16_t send_cmd = CMD_LOGIN;
	if (wait_for_reply(user, send_cmd) < 0) {
		log_err("can not get reply from server\n");
		return -1;
	}

	int ret;
	struct list_packet *lp = list_first_entry(get_wait_list(user),
			struct list_packet, list);
	list_del(&lp->list);
	if (get_command(lp) == get_wait_cmd(send_cmd)) {
		/* copy nick name to nick str */
		int length = slo_get_nick_length(lp);
		memcpy(nick, slo_get_nick(lp), length);
		nick[length - 1] = '\0';
		ret = 0;
	} else {
		log_warning("receive SRV_ERROR packet from server\n");
		ret = -1;
	}

	free(lp);
	return ret;
}

/* logout function called by ui */
int logout(struct client_user *user)
{
	return cmd_logout(user);
}

/* set nick function called by ui */
int set_nick(struct client_user *user, const char *nick,
		char *new_nick)
{
	if (cmd_set_nick(user, nick) < 0) {
		log_err("can not send set nick packet to server\n");
		return -1;
	}

	uint16_t send_cmd = CMD_SET_NICK;
	if (wait_for_reply(user, send_cmd) < 0) {
		log_err("can not get reply from server\n");
		return -1;
	}

	int ret;
	struct list_packet *lp = list_first_entry(get_wait_list(user),
			struct list_packet, list);
	list_del(&lp->list);
	if (get_command(lp) == get_wait_cmd(send_cmd)) {
		int nick_len = ssno_get_nick_length(lp);
		memcpy(new_nick, ssno_get_nick(lp), nick_len);
		new_nick[nick_len - 1] = '\0';
		ret = 0;
	} else {
		log_warning("receive SRV_ERROR packet from server\n");
		ret = -1;
	}

	free(lp);
	return ret;
}

/* add contact function called by ui */
int add_contact(struct client_user *user, uint32_t to_uin)
{
	if (cmd_add_contact(user, to_uin) < 0) {
		log_err("can not send add contact packet to server\n");
		return -1;
	}

	uint16_t send_cmd = CMD_ADD_CONTACT;
	if (wait_for_reply(user, send_cmd) < 0) {
		log_err("can not get reply from server\n");
		return -1;
	}

	int ret;
	struct list_packet *lp = list_first_entry(get_wait_list(user),
			struct list_packet, list);
	list_del(&lp->list);
	if (get_command(lp) == get_wait_cmd(send_cmd)) {
		ret = 0;
	} else {
		log_warning("receive SRV_ERROR packet from server\n");
		ret = -1;
	}

	free(lp);
	return ret;
}

/* add contact reply called by ui */
int add_contact_reply(struct client_user *user,
		uint32_t to_uin, uint16_t reply_type)
{
	return cmd_add_contact_reply(user, to_uin, reply_type);
}

/* contact list function called by ui */
int contact_list(struct client_user *user, uint32_t **uins)
{
	if (cmd_contact_list(user) < 0) {
		log_err("can not send contact list packet to server\n");
		return -1;
	}

	uint16_t send_cmd = CMD_CONTACT_LIST;
	if (wait_for_reply(user, send_cmd) < 0) {
		log_err("can not get reply from server\n");
		return -1;
	}

	int ret;
	struct list_packet *lp = list_first_entry(get_wait_list(user),
			struct list_packet, list);
	list_del(&lp->list);
	if (get_command(lp) == get_wait_cmd(send_cmd)) {
		user->contact_count = scl_get_contact_count(lp);
		*uins = malloc(sizeof(uint32_t) * user->contact_count);
		memcpy(*uins, scl_get_contact_uins(lp),
				sizeof(uint32_t) * user->contact_count);
		ret = 0;
	} else {
		log_warning("receive SRV_ERROR packet from server\n");
		ret = -1;
	}

	free(lp);
	return ret;
}

/* read multi contact info from packet */
static void read_contact_info_multi(struct client_user *user,
		struct list_packet *lp)
{
	uint16_t count = scim_get_contact_count(lp);
	char *ptr = scim_get_first_ptr(lp);
	int i;
	for (i = 0; i < count; i++, ptr = scim_get_next_ptr(ptr)) {
		uint32_t uin = scim_get_uin(ptr);
		struct contact *c;
		HASH_FIND_INT(user->contact_map, &uin, c);
		if (c) {
			HASH_DEL(user->contact_map, c);
		} else {
			c = malloc(sizeof(struct contact));
		}
		c->uin = uin;
		c->is_online = scim_get_status(ptr);
		int nick_len = scim_get_nick_length(ptr);
		memcpy(&c->nick, scim_get_nick(ptr), nick_len);
		c->nick[nick_len - 1] = '\0';
		HASH_ADD_INT(user->contact_map, uin, c);
	}
}

/* get multi contact info function called by ui */
int contact_info_multi(struct client_user *user,
		uint16_t count, uint32_t *uins)
{
	if (cmd_contact_info_multi(user, count, uins) < 0) {
		log_err("can not send contact info multi packet to server\n");
		return -1;
	}

	uint16_t send_cmd = CMD_CONTACT_INFO_MULTI;
	if (wait_for_reply(user, send_cmd) < 0) {
		log_err("can not get reply from server\n");
		return -1;
	}

	int ret;
	struct list_packet *lp = list_first_entry(get_wait_list(user),
			struct list_packet, list);
	list_del(&lp->list);
	if (get_command(lp) == get_wait_cmd(send_cmd)) {
		read_contact_info_multi(user, lp);
		ret = 0;
	} else {
		log_warning("receive SRV_ERROR packet from server\n");
		ret = -1;
	}

	free(lp);
	return ret;
}

/* get all contact information */
int get_contacts(struct client_user *user)
{
	uint32_t *uins = NULL;
	if (contact_list(user, &uins) < 0) {
		if (uins) {
			free(uins);
		}

		return -1;
	}

	static const uint16_t max_per_request = 100;
	int i, n = user->contact_count / max_per_request;
	uint32_t *p = uins;
	for (i = 0; i < n; i++) {
		if (contact_info_multi(user, max_per_request,
				p + i * max_per_request) < 0) {
			free(uins);
			return -1;
		}
	}

	uint16_t mod = user->contact_count % max_per_request;
	if (mod) {
		if (contact_info_multi(user, mod,
					p + n * max_per_request) < 0) {
			free(uins);
			return -1;
		}
	}

	/* free the uins array at last */
	free(uins);
	return 0;
}

/* send message function called by ui */
int send_message(struct client_user *user,
		uint32_t to_uin, const char *message)
{
	return cmd_message(user, to_uin, message);
}

/* read online message or offline message from buffer */
static void read_message_from_buffer(struct client_user *user, char *p)
{
	struct message_map *msg_map;
	uint32_t uin = som_get_from_uin(p);
	HASH_FIND_INT(user->msg_map, &uin, msg_map);
	if (!msg_map) {
		msg_map = malloc(sizeof(struct message_map));
		INIT_LIST_HEAD(&msg_map->head);
		msg_map->uin = uin;
		HASH_ADD_INT(user->msg_map, uin, msg_map);
	}

	struct message *msg = malloc(sizeof(struct message));
	msg->uin = uin;
	msg->type = som_get_type(p);
	msg->msg = NULL;
	int msg_len = som_get_msg_length(p);
	if (msg_len > 0) {
		msg->msg = malloc(msg_len);
		memcpy(msg->msg, som_get_message(p), msg_len);
		msg->msg[msg_len - 1] = '\0';
	}

	list_add_tail(&msg->list, &msg_map->head);
}

/* read offline message from packet */
static void read_offline_msg(struct client_user *user, struct list_packet *lp)
{
	int count = som_get_msg_count(lp);
	char *ptr = som_get_first_ptr(lp);
	int i;
	for (i = 0; i < count; i++, ptr = som_get_next_ptr(ptr)) {
		read_message_from_buffer(user, ptr);
	}
}

/* get all offline message */
int get_offline_msg(struct client_user *user)
{
	if (cmd_offline_msg(user) < 0) {
		log_err("can not send offline msg packet to server\n");
		return -1;
	}

	uint16_t send_cmd = CMD_OFFLINE_MSG;
	if (wait_for_reply(user, send_cmd) < 0) {
		log_err("can not get reply from server\n");
		return -1;
	}

	int ret = -1;
	struct list_packet *current = NULL;
	list_for_each_entry(current, get_wait_list(user), list) {
		if (get_command(current) == get_wait_cmd(send_cmd)) {
			ret = 0;
		}
	}

	struct list_packet *tmp = NULL;
	if (ret < 0) {
		/* free all packets */
		list_for_each_entry_safe(current, tmp,
				get_wait_list(user), list) {
			list_del(&current->list);
			free(current);
		}
		return ret;
	}

	list_for_each_entry_safe(current, tmp, get_wait_list(user), list) {
		list_del(&current->list);
		read_offline_msg(user, current);
		free(current);
	}

	return ret;
}

/* get online message from server */
int read_online_message(struct client_user *user)
{
	/* read message from msg list */
	struct list_head *head = get_msg_list(user);
	struct list_packet *current, *tmp;
	list_for_each_entry_safe(current, tmp, head, list) {
		list_del(&current->list);
		read_message_from_buffer(user, get_parameters(current));
	}

	/* read message from socket */
	char buf[8192];
	memset(buf, 0, sizeof(buf));
	int count = read_socket(user, buf, sizeof(buf), 0);
	read_packet(&user->reader, buf, count);

	head = get_recv_list(user);
	list_for_each_entry_safe(current, tmp, head, list) {
		uint16_t command = get_command(current);
		if (command == SRV_MESSAGE) {
			list_del(&current->list);
			read_message_from_buffer(user, get_parameters(current));
		}
	}

	return 0;
}

/* use uin to look up message */
struct message_map* get_message_by_uin(struct client_user *user, uint32_t uin)
{
	struct message_map *map = NULL;
	HASH_FIND_INT(user->msg_map, &uin, map);
	return map;
}
