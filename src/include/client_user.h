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

struct message_map {
	uint32_t uin;
	struct list_head head;
	UT_hash_handle hh;
};

struct message {
	struct list_head list;
	uint32_t uin;
	int type;
	char *msg;
};

struct packet_reader {
	struct list_head recv_packet_list;
	struct list_packet *lp;
	int expect_bytes;
	char length[2];
	bool length_incomplete;
};

struct client_user {
	struct packet_reader reader;
	struct list_head wait_list;
	struct list_head msg_list;
	struct message_map *msg_map;
	fd_set rset;
	uint32_t uin;
	uint32_t chat_uin;
	int contact_count;
	struct contact *contact_map;
	int socket;
	int mode;
};

int read_packet(struct packet_reader *reader,
		const char *buf, int count);

static inline void message_destory(struct message *msg)
{
	if (msg->msg) {
		free(msg->msg);
		msg->msg = NULL;
	}

	if (msg->list.next != NULL) {
		list_del(&msg->list);
	}
	free(msg);
}

static inline void packet_reader_init(struct packet_reader *pr)
{
	INIT_LIST_HEAD(&pr->recv_packet_list);
	pr->lp = NULL;
	pr->expect_bytes = 0;
	pr->length_incomplete = false;
}

static inline struct list_head* get_recv_list(struct client_user *user)
{
	return (user) ? &(user->reader.recv_packet_list) : NULL;
}

static inline struct list_head* get_wait_list(struct client_user *user)
{
	return (user) ? &user->wait_list : NULL;
}

static inline struct list_head* get_msg_list(struct client_user *user)
{
	return (user) ? &user->msg_list : NULL;
}

void client_user_init(struct client_user *user);
int cmd_keep_alive(struct client_user *user);
int cmd_login(struct client_user *user,
		uint32_t uin, const char *password);
int cmd_logout(struct client_user *user);
int cmd_set_nick(struct client_user *user, const char *nick);
int cmd_add_contact(struct client_user *user, uint32_t to_uin);
int cmd_add_contact_reply(struct client_user *user,
		uint32_t to_uin, uint16_t reply_type);
int cmd_contact_list(struct client_user *user);
int cmd_contact_info_multi(struct client_user *user,
		uint16_t count, uint32_t *uins);
int cmd_message(struct client_user *user,
		uint32_t to_uin, const char *message);
int cmd_offline_msg(struct client_user *user);

int keep_alive(struct client_user *user);
int login(struct client_user *user, uint32_t uin,
		const char *password, char *nick);
int logout(struct client_user *user);
int set_nick(struct client_user *user, const char *nick, char *new_nick);
int add_contact(struct client_user *user, uint32_t to_uin);
int add_contact_reply(struct client_user *user,
		uint32_t to_uin, uint16_t reply_type);
int get_contacts(struct client_user *user);
int contact_list(struct client_user *user, uint32_t **uins);
int contact_info_multi(struct client_user *user,
		uint16_t count, uint32_t *uins);
int send_message(struct client_user *user,
		uint32_t to_uin, const char *message);
int get_offline_msg(struct client_user *user);
int read_online_message(struct client_user *user);
struct message_map* get_message_by_uin(struct client_user *user, uint32_t uin);

#endif
