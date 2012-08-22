#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include "protocol.h"
#include "log.h"
#include "network.h"
#include "list_packet.h"
#include "packet_dump.h"
#include "uthash.h"
#include "client_user.h"

#define MAX_PASS_LEN		20
#define CONN_SERVER_IP		"10.18.124.77"
#define CONN_SERVER_PORT	34567

FILE *log_fp = NULL;
FILE *dump_fp = NULL;

static void get_nick(struct contact *contact_map, uint32_t uin, char *str)
{
	struct contact *c;
	HASH_FIND_INT(contact_map, &uin, c);
	if (c) {
		strcpy(str, c->nick);
	} else {
		sprintf(str, "%u", uin);
	}
}

static int ui_add_contact(struct client_user *user, char *buf, int count)
{
	char *p = buf + 3;
	uint32_t uin;
	sscanf(p, "%u", &uin);
	if (add_contact(user, uin) < 0) {
		printf("can not add user as contact\n");
	} else {
		printf("the request has been sent\n");
	}

	return 0;
}

static int ui_set_nick(struct client_user *user, char *buf, int count)
{
	char *p = buf + 5;
	p[strlen(p) - 1] = '\0';
	char new_nick[MAX_NICK_LENGTH + 1];
	if (set_nick(user, p, new_nick) < 0) {
		printf("can not set nick name to %s\n", p);
	} else {
		printf("set nick name ok, your new nick name is %s\n", new_nick);
	}

	return 0;
}

static int contact_compare(struct contact *lhs, struct contact *rhs)
{
	return rhs->is_online - lhs->is_online;
}

static void ui_print_contacts(struct contact *contact_map)
{
	if (!HASH_COUNT(contact_map)) {
		return;
	}

	printf("status\tuin\tnick name\n");
	HASH_SORT(contact_map, contact_compare);

	struct contact *current, *tmp;
	HASH_ITER(hh, contact_map, current, tmp) {
		char buf[16];
		if (current->is_online) {
			strcpy(buf, "  +");
		} else {
			strcpy(buf, "  -");
		}
		printf("%s\t%u\t%s\n", buf, current->uin, current->nick);
	}
}

static int ui_show_contacts(struct client_user *user)
{
	if (get_contacts(user) < 0) {
		printf("get contact list error\n");
		return -1;
	}

	ui_print_contacts(user->contact_map);
	return 0;
}

static int ui_chat(struct client_user *user, char *buf, int count)
{
	char *p = buf + 5;
	sscanf(p, "%u", &user->chat_uin);
	printf("%u\n", user->chat_uin);
	user->mode = CHAT_MODE;
	printf("enter chat mode, type message to send to your friend, use \\q to quit\n");
	return 0;
}

static inline void ui_print_request(struct message *msg)
{
	printf("%u want to add you as a friend, ", msg->uin);
	printf("input 'yes' to accept or 'no' to refuse\n");
}

static inline void ui_print_accept(struct message *msg)
{
	printf("%u accept your friend request\n", msg->uin);
}

static inline void ui_print_refuse(struct message *msg)
{
	printf("%u refuse your friend request\n", msg->uin);
}

static inline void ui_print_chat(struct client_user *user, struct message *msg)
{
	char nick[MAX_NICK_LENGTH + 1];
	time_t rawtime;
	struct tm *timeinfo;
	char buffer[16];

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	memset(buffer, 0, sizeof(buffer));
	strftime(buffer, sizeof(buffer), "%X", timeinfo);
	get_nick(user->contact_map, msg->uin, nick);

	printf("%s %s: \n", nick, buffer);
	printf("%s\n", msg->msg);
}

/* show stored message, could be online msg or offline msg */
static void ui_print_message(struct client_user *user, int i)
{
	assert(i <= HASH_COUNT(user->msg_map));

	struct message_map *msg_map, *tmp;
	int j = 0;
	HASH_ITER(hh, user->msg_map, msg_map, tmp) {
		j++;
		if (j == i) {
			break;
		}
	}
	assert(msg_map);

	struct message *msg = list_first_entry(&msg_map->head,
			struct message, list);
	list_del(&msg->list);
	switch (msg->type) {
	case MSG_TYPE_REQUEST:
		user->mode = AUTH_MODE;
		user->chat_uin = msg->uin;
		ui_print_request(msg);
		break;
	case MSG_TYPE_ACCEPT:
		ui_print_accept(msg);
		break;
	case MSG_TYPE_REFUSE:
		ui_print_refuse(msg);
		break;
	case MSG_TYPE_CHAT:
		user->chat_uin = msg->uin;
		user->mode = CHAT_MODE;
		ui_print_chat(user, msg);
		break;
	default:
		log_warning("unknown message type\n");
		break;
	}

	message_destory(msg);
	if (list_empty(&msg_map->head)) {
		HASH_DEL(user->msg_map, msg_map);
		free(msg_map);
	}
}

static int ui_show_message(struct client_user *user, char *buf, int count)
{
	int i;
	sscanf(buf, "%d", &i);
	if (i > HASH_COUNT(user->msg_map)) {
		printf("unknown command\n");
	} else {
		ui_print_message(user, i);
	}

	return 0;
}

static int command_input(struct client_user *user, char *buf, int count)
{
	if (!strncmp(buf, "add", 3)) {
		/* add contact */
		return ui_add_contact(user, buf, count);
	} else if (!strncmp(buf, "nick", 4)) {
		/* set nick name */
		return ui_set_nick(user, buf, count);
	} else if (!strncmp(buf, "list", 4)) {
		/* list contact */
		return ui_show_contacts(user);
	} else if (!strncmp(buf, "chat", 4)) {
		/* enter chat mode */
		return ui_chat(user, buf, count);
	} else {
		return ui_show_message(user, buf, count);
	}
}

static int ui_show_usage_command(struct client_user *user)
{
	if (HASH_COUNT(user->msg_map)) {
		return 0;
	}

	printf("please enter the following command: \n");
	printf("  list  \t-list you friends\n");
	printf("  nick  \t-change your nick name\n");
	printf("  chat [uin]\t-chat to your friend\n");
	printf("  add [uin]\t-add friend\n");
	return 0;
}

static int ui_quit_chat_mode(struct client_user *user)
{
	printf("quit chat mode\n");
	ui_show_usage_command(user);
	return 0;
}

static int ui_send_message(struct client_user *user, uint32_t to_uin, char *msg)
{
	if (send_message(user, to_uin, msg) < 0) {
		printf("can not send message to user %u\n", to_uin);
	}

	return 0;
}

static int chat_input(struct client_user *user, char *buf, int count)
{
	if (!strncmp(buf, "\\q", 2)) {
		/* quit chat mode */
		user->mode = COMMAND_MODE;
		ui_quit_chat_mode(user);
	} else {
		/* send chat message */
		buf[strlen(buf) - 1] = '\0';
		ui_send_message(user, user->chat_uin, buf);
	}

	return 0;
}

static int ui_add_contact_reply(struct client_user *user,
		uint32_t to_uin, uint16_t reply_type)
{
	int ret = add_contact_reply(user, to_uin, reply_type);
	if (ret < 0) {
		printf("can not send reply to server\n");
	} else {
		printf("the reply has beed sent\n");
	}
	return 0;
}

static int auth_input(struct client_user *user, char *buf, int count)
{
	int input_yes = !strncmp(buf, "yes", 3);
	int input_no = !strncmp(buf, "no", 2);
	if (input_yes || input_no) {
		uint16_t reply_type;
		if (input_yes) {
			/* agree */
			reply_type = 0x0001;
		} else {
			/* reject */
			reply_type = 0x0002;
		}

		user->mode = COMMAND_MODE;
		ui_add_contact_reply(user, user->chat_uin, reply_type);
		ui_show_usage_command(user);
	} else {
		printf("you should input yes or no\n");
	}

	return 0;
}

static int input_handler(struct client_user *user, char *buf, int count)
{
	switch (user->mode) {
	case COMMAND_MODE:
		return command_input(user, buf, count);
	case CHAT_MODE:
		return chat_input(user, buf, count);
	case AUTH_MODE:
		return auth_input(user, buf, count);
	default:
		return -1;
	}
}

static int ui_show_chat_message(struct client_user *user)
{
	if (user->mode != CHAT_MODE) {
		return 0;
	}

	read_online_message(user);
	struct message_map *msg_map = get_message_by_uin(user, user->chat_uin);
	if (!msg_map) {
		return 0;
	}

	struct message *current, *tmp;
	list_for_each_entry_safe(current, tmp, &msg_map->head, list) {
		ui_print_chat(user, current);
		list_del(&current->list);
		message_destory(current);
	}

	if (list_empty(&msg_map->head)) {
		HASH_DEL(user->msg_map, msg_map);
		free(msg_map);
		msg_map = NULL;
	}
	return 0;
}

static int ui_show_msg_list(struct client_user *user)
{
	if (!HASH_COUNT(user->msg_map)) {
		return 0;
	}

	printf("you have %d messages\n", HASH_COUNT(user->msg_map));
	printf("num\tuin(nick)\tcount\n");
	struct message_map *current, *tmp;
	int i = 0;
	HASH_ITER(hh, user->msg_map, current, tmp) {
		i++;
		struct list_head *head = &current->head, *cur;
		int j = 0;
		list_for_each(cur, head) {
			j++;
		}

		char nick[MAX_NICK_LENGTH + 1];
		get_nick(user->contact_map, current->uin, nick);
		printf("%d\t%u(%s)\t%d\n", i, current->uin, nick, j);
	}
	printf("input number to read the message\n");
	return 0;
}

static int ui_show_msg_list_notice(struct client_user *user)
{
	static int msg_count = 0;
	if (HASH_COUNT(user->msg_map) != msg_count) {
		ui_show_msg_list(user);
		msg_count = HASH_COUNT(user->msg_map);
	}
	return 0;
}

static int ui_show_offline_msg_list(struct client_user *user)
{
	if (!HASH_COUNT(user->msg_map)) {
		printf("you have zero offline message\n");
		return 0;
	} else {
		return ui_show_msg_list(user);
	}
}

static int ui_login(struct client_user *user,
		uint32_t uin, const char *password)
{
	char nick[MAX_NICK_LENGTH + 1];
	if (login(user, uin, password, nick) < 0) {
		printf("login failed, please check your uin and password\n");
		return -1;
	} else {
		printf("welcome back, %s!\n", nick);
		if (ui_show_contacts(user) < 0) {
			return -1;
		}

		if (get_offline_msg(user) < 0) {
			return -1;
		}

		ui_show_offline_msg_list(user);
		return ui_show_usage_command(user);
	}
}

void select_loop(struct client_user *user)
{
	FD_ZERO(&user->rset);
	while (1) {
		FD_SET(fileno(stdin), &user->rset);
		int maxfdp1 = fileno(stdin) + 1;
		struct timeval timeout = {1, 0};
		select(maxfdp1, &user->rset, NULL, NULL, &timeout);

		if (FD_ISSET(fileno(stdin), &user->rset)) {
			/* TODO: we have data on stdin to read */
			char buf[8192];
			int count;

			memset(buf, 0, sizeof(buf));
			if ((count = read(fileno(stdin), buf, sizeof(buf))) < 0) {
				log_warning("read from stdin error\n");
				break;
			}

			if (user->mode == COMMAND_MODE &&
					!strcmp(buf, "quit")) {
				/* TODO: quit the program */
				break;
			}

			input_handler(user, buf, count);
		}

		/* TODO: read online message */
		read_online_message(user);
		if (user->mode == COMMAND_MODE) {
			ui_show_msg_list_notice(user);
		} else if (user->mode == CHAT_MODE) {
			ui_show_chat_message(user);
		}
	}
}

char* getpass(const char *prompt)
{
	static char buf[MAX_PASS_LEN + 1];
	char *ptr;
	sigset_t sig, osig;
	struct termios ts, ots;
	FILE *fp;
	int c;

	if ((fp = fopen(ctermid(NULL), "r+")) == NULL) {
		return NULL;
	}
	setbuf(fp, NULL);

	/* block SIGINT and SIGTSTP */
	sigemptyset(&sig);
	sigaddset(&sig, SIGINT);
	sigaddset(&sig, SIGTSTP);
	sigprocmask(SIG_BLOCK, &sig, &osig);

	/* save TTY state */
	tcgetattr(fileno(fp), &ts);
	ots = ts;
	ts.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	tcsetattr(fileno(fp), TCSAFLUSH, &ts);
	fputs(prompt, fp);

	ptr = buf;
	while ((c = getc(fp)) != EOF && c != '\n') {
		if (ptr < &buf[MAX_PASS_LEN]) {
			*ptr++ = c;
		}
	}

	*ptr = 0;
	putc('\n', fp);

	/* restore TTY and mask state */
	tcsetattr(fileno(fp), TCSAFLUSH, &ots);
	sigprocmask(SIG_SETMASK, &osig, NULL);
	fclose(fp);
	return buf;
}

int main(int argc, char *argv[])
{
	LOG_INIT("log_client");
	DUMP_INIT("dump_client");

	printf("Please enter you uin: ");
	uint32_t uin;
	scanf("%u", &uin);

	char *pass;
	if ((pass = getpass("Enter password: ")) == NULL) {
		printf("get password error or password is too short\n");
		return 0;
	}

	struct client_user user;
	client_user_init(&user);
	if ((user.socket = connect_to_server(CONN_SERVER_IP,
					CONN_SERVER_PORT)) < 0) {
		log_err("can not connect to conn server %s(%hu)\n",
				CONN_SERVER_IP, CONN_SERVER_PORT);
		return 0;
	}

	if (ui_login(&user, uin, pass) < 0) {
		printf("some error occured\n");
		return 0;
	}

	user.mode = COMMAND_MODE;
	select_loop(&user);

	DUMP_DESTROY();
	LOG_DESTROY();
	return 0;
}
