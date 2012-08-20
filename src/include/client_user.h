#ifndef _CLIENT_USER_H_
#define _CLIENT_USER_H_

#include <stdint.h>
#include <stdbool.h>
#include "list.h"
#include "protocol.h"
#include "uthash.h"

#define LOGIN_INCOMPLETE	1
#define LOGIN_OK		2
#define LOGIN_ERROR		3

#define LOGIN_MODE		1
#define COMMAND_MODE		2
#define CHAT_MODE		3
#define AUTH_MODE		4

#define LENGTH_OFFSET		0
#define VERSION_OFFSET		2
#define COMMAND_OFFSET		4
#define PAD_OFFSET		6
#define UIN_OFFSET		8
#define PARAMETERS_OFFSET	12

#define PROTOCOL_VERSION	0x0001
#define MAX_PASSWORD_LENGTH	16
#define MAX_NICK_LENGTH		32
#define MAX_MESSAGE_LENGTH	4096

#define char_ptr(ptr)		((char *)(ptr))

/* TODO: need to change to network byte order */
#define get_field_htons(ptr, offset)	\
	(*((uint16_t *)((char_ptr(ptr) + (offset)))))

/* TODO: need to change to network byte order */
#define get_field_htonl(ptr, offset)	\
	(*((uint32_t *)((char_ptr(ptr) + (offset)))))

/* TODO: need to change to network byte order */
#define set_field_htons(ptr, offset, value)	\
	*((uint16_t *)(char_ptr(ptr) + (offset))) = ((value))

/* TODO: need to change to network byte order */
#define set_field_htonl(ptr, offset, value)	\
	*((uint32_t *)(char_ptr(ptr) + (offset))) = ((value))

#define set_field(ptr, offset, size, val_ptr)	\
	memcpy(char_ptr(ptr) + (offset), (val_ptr), (size));

struct contact {
	uint32_t uin;
	int is_online;
	char nick[MAX_NICK_LENGTH + 1];
	UT_hash_handle hh;
};

struct offline_msg_table {
	uint32_t uin;
	struct list_head head;
	UT_hash_handle hh;
};

struct offline_msg {
	struct list_head list;
	uint32_t uin;
	int type;
	char *message;
};

struct packet_reader {
	struct list_head recv_packet_list;
	int expect_bytes;
	char length[2];
	bool length_incomplete;
};

struct client_user {
	struct packet_reader reader;
	struct list_head recv_packet_list;
	struct list_head send_packet_list;
	struct list_head pending_list;
	struct offline_msg_table *offline_msg_table;
	struct packet *packet;
	uint32_t uin;
	uint32_t chat_uin;
	int contact_count;
	struct contact *contact_table;
	int socket;
	int epoll;
	int mode;
};

int read_packet(struct packet_reader *reader,
		const char *buf, int count);

static inline void packet_reader_init(struct packet_reader *pr)
{
	INIT_LIST_HEAD(&pr->recv_packet_list);
	pr->expect_bytes = 0;
	pr->length_incomplete = false;
}

static inline struct list_head* get_recv_list(struct client_user *user)
{
	return (user) ? &user->recv_packet_list : NULL;
}

static inline struct list_head* get_send_list(struct client_user *user)
{
	return (user) ? &user->send_packet_list : NULL;
}

void client_user_init(struct client_user *user);
void cmd_keep_alive(struct client_user *user);
void cmd_login(struct client_user *user,
		uint32_t uin, const char *password);
void cmd_logout(struct client_user *user);
void cmd_set_nick(struct client_user *user, const char *nick);
void cmd_add_contact(struct client_user *user, uint32_t to_uin);
void cmd_add_contact_reply(struct client_user *user,
		uint32_t to_uin, uint16_t reply_type);
void cmd_contact_list(struct client_user *user);
void cmd_contact_info_multi(struct client_user *user,
		uint16_t count, uint32_t *uins);
void cmd_message(struct client_user *user,
		uint32_t to_uin, const char *message);
void cmd_offline_msg(struct client_user *user);
#if 0
void srv_error(struct client_user *user);
void srv_login_ok(struct client_user *user);
void srv_set_nick_ok(struct client_user *user);
void srv_add_contact_wait(struct client_user *user);
void srv_contact_list(struct client_user *user);
void srv_contact_info_multi(struct client_user *user);
void srv_offline_msg(struct client_user *user);
#endif
#endif
