#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "protocol.h"
#include "list_packet.h"
#include "network.h"
#include "client_user.h"

/* 1st:
 * last packet is incomplete, but we have got the length field,
 * just read the left data, and copy them to the last packet
 * in receive queue */
static int last_packet_incomplete(struct packet_reader *reader,
		const char *buf, int count)
{
	if (list_empty(&reader->recv_packet_list)) {
		log_warning("incomplete packet missing\n");
		reader->expect_bytes = 0;
		return -1;
	}

	struct list_head *last = reader->recv_packet_list.prev;
	struct list_packet *packet =
		list_entry(last, struct list_packet, list);
	int packet_length = get_length_host(packet);
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
		reader->expect_bytes = 0;
		log_debug("recv packet len %hu, cmd %#hx, uin %u from server\n",
				get_length_host(packet),
				get_command_host(packet),
				get_uin_host(packet));
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

	/* TODO: need to change to network byte order */
	int packet_length = (*((uint16_t *)reader->length));
	//int packet_length = ntohs(*((uint16_t *)conn->length));
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
	list_add_tail(&packet->list, &reader->recv_packet_list);
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
		/* TODO: need to change to network byte order */
		int packet_length = (*((uint16_t *)buf));
		//int packet_length = ntohs(*((uint16_t *)buf));
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
		} else {
			log_debug("recv packet len %hu, cmd %#hx, uin %u from server\n",
					get_length_host(packet),
					get_command_host(packet),
					get_uin_host(packet));
		}

		list_add_tail(&packet->list, &reader->recv_packet_list);
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
	INIT_LIST_HEAD(&user->recv_packet_list);
	INIT_LIST_HEAD(&user->send_packet_list);
	INIT_LIST_HEAD(&user->other_msg_list);
	user->packet = malloc(MAX_PACKET_LEN);
	memset(user->packet, 0, MAX_PACKET_LEN);
	user->type = LOGIN_INCOMPLETE;
}

static struct list_packet* create_packet(uint32_t uin,
		uint16_t length, uint16_t command)
{
	int lp_length = sizeof(struct list_head) + length;
	struct list_packet *lp = malloc(lp_length);
	struct packet *packet = &lp->packet;
	memset(lp, 0, lp_length);
	set_field_htons(packet, LENGTH_OFFSET, length);
	set_field_htons(packet, VERSION_OFFSET, PROTOCOL_VERSION);
	set_field_htons(packet, COMMAND_OFFSET, command);
	set_field_htonl(packet, UIN_OFFSET, uin);
	return lp;
}

void cmd_keep_alive(struct client_user *user)
{
	uint16_t length = PACKET_HEADER_LEN;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_KEEP_ALIVE);
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}

void cmd_login(struct client_user *user,
		uint32_t uin, const char *password)
{
	uint16_t pass_len = strlen(password);
	assert(pass_len <= MAX_PASSWORD_LENGTH);

	uint16_t length = PACKET_HEADER_LEN + 2 + pass_len;
	struct list_packet *lp = create_packet(uin, length, CMD_LOGIN);
	struct packet *packet = &lp->packet;

	set_field_htons(packet, PARAMETERS_OFFSET, pass_len);
	set_field(packet, PARAMETERS_OFFSET + 2, pass_len, password);

	user->uin = uin;
	user->type = LOGIN_INCOMPLETE;
	user->mode = LOGIN_MODE;
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}

void cmd_logout(struct client_user *user)
{
	uint16_t length = PACKET_HEADER_LEN;
	struct list_packet *lp = create_packet(user->uin, length, CMD_LOGOUT);

	user->type = LOGIN_INCOMPLETE;
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}

void cmd_set_nick(struct client_user *user, const char *nick)
{
	uint16_t nick_len = strlen(nick);
	assert(nick_len++ <= MAX_NICK_LENGTH);

	uint16_t length = PACKET_HEADER_LEN + 2 + nick_len;
	struct list_packet *lp = create_packet(user->uin, length, CMD_SET_NICK);
	struct packet *packet = &lp->packet;

	set_field_htons(packet, PARAMETERS_OFFSET, nick_len);
	set_field(packet, PARAMETERS_OFFSET + 2, nick_len, nick);
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}

void cmd_add_contact(struct client_user *user, uint32_t to_uin)
{
	uint16_t length = PACKET_HEADER_LEN + 4;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_ADD_CONTACT);
	struct packet *packet = &lp->packet;

	set_field_htonl(packet, PARAMETERS_OFFSET, to_uin);
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}

void cmd_add_contact_reply(struct client_user *user,
		uint32_t to_uin, uint16_t reply_type)
{
	uint16_t length = PACKET_HEADER_LEN + 4 + 2;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_ADD_CONTACT_REPLY);
	struct packet *packet = &lp->packet;

	set_field_htonl(packet, PARAMETERS_OFFSET, to_uin);
	set_field_htons(packet, PARAMETERS_OFFSET + 4, reply_type);
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}

void cmd_contact_list(struct client_user *user)
{
	uint16_t length = PACKET_HEADER_LEN;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_CONTACT_LIST);
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}

void cmd_contact_info_multi(struct client_user *user,
		uint16_t count, uint32_t *uins)
{
	assert(count <= 100);
	uint16_t length = PACKET_HEADER_LEN + 2 + count * 4;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_CONTACT_INFO_MULTI);
	struct packet *packet = &lp->packet;

	set_field_htons(packet, PARAMETERS_OFFSET, count);
	set_field(packet, PARAMETERS_OFFSET + 2, count * 4, uins);
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}

void cmd_message(struct client_user *user,
		uint32_t to_uin, const char *message)
{
	uint16_t msg_len = strlen(message);
	assert(msg_len++ <= MAX_MESSAGE_LENGTH);

	uint16_t length = PACKET_HEADER_LEN + 2 + msg_len;
	struct list_packet *lp = create_packet(user->uin, length, CMD_MESSAGE);
	struct packet *packet = &lp->packet;

	set_field_htons(packet, PARAMETERS_OFFSET, msg_len);
	set_field(packet, PARAMETERS_OFFSET + 2, msg_len, message);
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}

void cmd_offline_msg(struct client_user *user)
{
	uint16_t length = PACKET_HEADER_LEN;
	struct list_packet *lp =
		create_packet(user->uin, length, CMD_OFFLINE_MSG);
	list_add_tail(&lp->list, get_send_list(user));
	wait_for_write(user->epoll, user->socket);
}
