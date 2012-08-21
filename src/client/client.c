#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <termios.h>
#include <sys/epoll.h>
#include <errno.h>
#include "protocol.h"
#include "log.h"
#include "network.h"
#include "list_packet.h"
#include "uthash.h"
#include "client_user.h"

#define MAX_PASS_LEN		20
#define CONN_SERVER_IP		"127.0.0.1"
#define CONN_SERVER_PORT	27182

FILE *log_fp = NULL;
char* getpass(const char *prompt);

int main(int argc, char *argv[])
{
	LOG_INIT("log_client");

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
	setup_epoll(&user);

	log_notice("send login packet to server\n");
	cmd_login(&user, uin, pass);
	epoll_loop(&user);

	LOG_DESTROY();
}

int setup_epoll(struct client_user *user)
{
	assert(user);
	struct epoll_event event;

	if ((user->epoll = epoll_create1(0)) < 0) {
		log_err("create epoll monitor fd failed\n");
		return -1;
	}
	log_notice("create epoll %d\n", user->epoll);

	set_nonblocking(user->socket);
	event.data.fd = user->socket;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(user->epoll, EPOLL_CTL_ADD, user->socket, &event) < 0) {
		log_err("can not add sfd to monitored fd set\n");
		return -1;
	}
	log_notice("add socket %d to epoll %d, mode %s\n",
			user->socket, user->epoll, "EPOLLIN | EPOLLET");

	event.data.fd = fileno(stdin);
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(user->epoll, EPOLL_CTL_ADD, fileno(stdin), &event) < 0) {
		log_err("can not add sfd to monitored fd set\n");
		return -1;
	}

	return 0;
}

static void get_nick(struct contact *contact_table, uint32_t uin, char *str)
{
	struct contact *c;
	HASH_FIND_INT(contact_table, &uin, c);
	if (c) {
		strcpy(str, c->nick);
	} else {
		strcpy(str, "stranger");
	}
}

static void srv_set_nick_ok(struct client_user *user, struct list_packet *lp)
{
	log_info("recv set nick ok packet from server\n");
	printf("change nick name success\n");
}

static void srv_add_contact_wait(struct client_user *user, struct list_packet *lp)
{
	log_info("recv add contact wait packet from server\n");
	printf("wait for his/her permission\n");
}

/* get contact list, send contact_info_multi packet */
static void srv_contact_list(struct client_user *user, struct list_packet *lp)
{
	log_info("recv contact list packet from server\n");
	static const uint16_t max_per_request = 100;
	user->contact_count = get_field_htons(&lp->packet, PARAMETERS_OFFSET);
	if (user->contact_count == 0) {
		cmd_offline_msg(user);
		return;
	}

	int i, n = user->contact_count / max_per_request;
	char *start = ((char *)&lp->packet) + PACKET_HEADER_LEN + 2;
	for (i = 0; i < n; i++) {
		cmd_contact_info_multi(user, max_per_request,
				(uint32_t *)(start + i * max_per_request));
	}

	uint16_t mod = user->contact_count % max_per_request;
	if (mod) {
		cmd_contact_info_multi(user, mod,
				(uint32_t *)(start + n * max_per_request));
	}
}

static int contact_compare(struct contact *lhs, struct contact *rhs)
{
	return lhs->is_online - rhs->is_online;
}

static void print_contact_list(struct contact *contacts)
{
	HASH_SORT(contacts, contact_compare);

	struct contact *current, *tmp;
	HASH_ITER(hh, contacts, current, tmp) {
		char buf[16];
		if (current->is_online) {
			strcpy(buf, "online");
		} else {
			strcpy(buf, "offline");
		}
		printf("%d\t%s\t%s\n", current->uin, current->nick, buf);
	}
}

/* get contact info */
static void srv_contact_info_multi(struct client_user *user, struct list_packet *lp)
{
	log_info("recv contact info multi packet from server\n");
	uint16_t count = get_field_htons(&lp->packet, PARAMETERS_OFFSET);
	int i;
	char *p = ((char *)&lp->packet) + PARAMETERS_OFFSET + 2;
	for (i = 0; i < count; i++) {
		uint32_t uin = get_field_htonl(p, 0);
		struct contact *c;
		HASH_FIND_INT(user->contact_table, &uin, c);
		if (!c) {
			c = malloc(sizeof(struct contact));
			c->uin = uin;
			HASH_ADD_INT(user->contact_table, uin, c);
		}
		c->is_online = get_field_htons(p, 4);
		int length = get_field_htons(p, 6);
		memcpy(&c->nick, p + 8, length);
		c->nick[length - 1] = '\0';
		p += length + 8;
	}

	if (HASH_COUNT(user->contact_table) == user->contact_count) {
		/* print the contact list */
		print_contact_list(user->contact_table);
		cmd_offline_msg(user);
	}
}

/* read message from buffer */
static void __srv_message(struct client_user *user, char *p)
{
	struct offline_msg_table *msg_table;
	uint32_t uin = get_field_htonl(p, 0);
	HASH_FIND_INT(user->offline_msg_table, &uin, msg_table);
	if (!msg_table) {
		msg_table = malloc(sizeof(struct offline_msg_table));
		INIT_LIST_HEAD(&msg_table->head);
		msg_table->uin = uin;
		HASH_ADD_INT(user->offline_msg_table, uin, msg_table);
	}

	struct offline_msg *msg = malloc(sizeof(struct offline_msg));
	msg->uin = uin;
	msg->type = get_field_htons(p, 8);
	msg->message = NULL;
	int length = get_field_htons(p, 10);
	if (length > 0) {
		msg->message = malloc(length);
		memcpy(msg->message, p + 12, length);
		msg->message[length - 1] = '\0';
	}
	list_add_tail(&msg->list, &msg_table->head);
}

/* add message to offline message hash table */
static void srv_offline_msg(struct client_user *user, struct list_packet *lp)
{
	log_info("recv offline message packet from server\n");
	int count = get_field_htons(&lp->packet, PARAMETERS_OFFSET);
	int i;
	char *p = ((char *)&lp->packet) + PARAMETERS_OFFSET + 2;
	for (i = 0; i < count; i++) {
		int length = get_field_htons(p, 10);
		__srv_message(user, p);
		p += length + 12;
	}
}

/* add message to offline message hash table */
static void srv_offline_msg_done(struct client_user *user, struct list_packet *lp)
{
	log_info("recv offline message done packet from server\n");
	srv_offline_msg(user, lp);
}

/* add message to offline message hash table, because we are chatting with
 * other people or in auth mode */
static void srv_message(struct client_user *user, struct list_packet *lp)
{
	log_info("recv message packet from server\n");
	char *p = ((char *)&lp->packet) + PARAMETERS_OFFSET;
	__srv_message(user, p);
}

/* process packets in login mode, only wait for SRV_LOGIN_OK or SRV_ERROR,
 * ignore other packets */
static void login_process_packets(struct client_user *user)
{
	struct list_head *head = get_recv_list(user);
	/* process all packet on list */
	while (!list_empty(head)) {
		struct list_packet *lp =
			list_first_entry(head, struct list_packet, list);
		list_del(&lp->list);
		uint16_t command = get_command_host(lp);
		free(lp);
		switch (command) {
			case SRV_LOGIN_OK:
				printf("login success\n");
				user->mode = COMMAND_MODE;
				cmd_contact_list(user);
				break;
			case SRV_ERROR:
				printf("login failed\n");
				exit(0);
				break;
			default:
				break;
		}
	}
}

static void print_offline_msg_list(struct client_user *user)
{
	if (!HASH_COUNT(user->offline_msg_table)) {
		return;
	}

	printf("you have %d messages\n", HASH_COUNT(user->offline_msg_table));
	printf("num\tuin(nick)\tcount\n");
	struct offline_msg_table *current, *tmp;
	int i = 0;
	HASH_ITER(hh, user->offline_msg_table, current, tmp) {
		i++;
		struct list_head *head = &current->head, *cur;
		int j = 0;
		list_for_each(cur, head) {
			j++;
		}

		char nick[MAX_NICK_LENGTH + 1];
		get_nick(user->contact_table, current->uin, nick);
		printf("%d\t%u(%s)\t%d\n", i, current->uin, nick, j);
	}
	printf("input number to read the message\n");
}

static void print_offline_msg(struct client_user *user, int i)
{
	assert(i <= HASH_COUNT(user->offline_msg_table));

	struct offline_msg_table *msg_table, *tmp;
	int j = 0;
	HASH_ITER(hh, user->offline_msg_table, msg_table, tmp) {
		j++;
		if (j == i) {
			break;
		}
	}
	assert(msg_table);

	struct offline_msg *msg = list_first_entry(&msg_table->head,
			struct offline_msg, list);
	list_del(&msg->list);
	switch (msg->type) {
	case MSG_TYPE_REQUEST:
		printf("%u want to add you as a friend, ", msg->uin);
		printf("input 'yes' to accept or 'no' to refuse\n");
		user->mode = AUTH_MODE;
		user->chat_uin = msg->uin;
		break;
	case MSG_TYPE_ACCEPT:
		printf("%u accept your friend request\n", msg->uin);
		break;
	case MSG_TYPE_REFUSE:
		printf("%u refuse your friend request\n", msg->uin);
		break;
	case MSG_TYPE_CHAT:
		printf("%u said:\n", msg->uin);
		printf("%s\n", msg->message);
		user->chat_uin = msg->uin;
		user->mode = CHAT_MODE;
		break;
	default:
		log_warning("unknown message type\n");
		break;
	}

	if (msg->message) {
		free(msg->message);
	}
	free(msg);

	if (list_empty(&msg_table->head)) {
		HASH_DEL(user->offline_msg_table, msg_table);
		free(msg_table);
	}
}

/* process packets in command mode */
static void command_process_packets(struct client_user *user)
{
	struct list_head *head = get_recv_list(user);
	list_splice_init(&user->pending_list, head);
	/* process all packet on list */
	while (!list_empty(head)) {
		struct list_packet *lp =
			list_first_entry(head, struct list_packet, list);
		list_del(&lp->list);
		uint16_t command = get_command_host(lp);
		switch (command) {
		case SRV_SET_NICK_OK:
			srv_set_nick_ok(user, lp);
			free(lp);
			break;
		case SRV_ADD_CONTACT_WAIT:
			srv_add_contact_wait(user, lp);
			free(lp);
			break;
		case SRV_CONTACT_LIST:
			srv_contact_list(user, lp);
			free(lp);
			break;
		case SRV_CONTACT_INFO_MULTI:
			srv_contact_info_multi(user, lp);
			free(lp);
			break;
		case SRV_OFFLINE_MSG:
			srv_offline_msg(user, lp);
			break;
		case SRV_OFFLINE_MSG_DONE:
			srv_offline_msg_done(user, lp);
			break;
		case SRV_MESSAGE:
			srv_message(user, lp);
			break;
		default:
			break;
		}
	}
	print_offline_msg_list(user);
}

/* print message in chat mode */
static void print_chat_message(struct client_user *user, struct list_packet *lp)
{
	struct packet *p = &lp->packet;
	uint32_t to_uin = get_field_htonl(p, PARAMETERS_OFFSET);
	char nick[MAX_NICK_LENGTH + 1];
	get_nick(user->contact_table, to_uin, nick);
	int length = get_field_htons(p, PARAMETERS_OFFSET + 10);
	char *msg = (char *)p + PARAMETERS_OFFSET + 12;
	msg[length - 1] = '\0';
	/* TODO: need to print nick and time */
	printf("\n%u(%s) said: \n", to_uin, nick);
	printf("%s\n", msg);
}

/* process packets in chat mode, only wait for SRV_MESSAGE packets */
static void chat_process_packets(struct client_user *user)
{
	struct list_head *head = get_recv_list(user);
	/* process all packet on list */
	while (!list_empty(head)) {
		struct list_packet *lp =
			list_first_entry(head, struct list_packet, list);
		list_del(&lp->list);
		uint16_t command = get_command_host(lp);
		uint32_t to_uin = get_field_htonl(&lp->packet, PARAMETERS_OFFSET);
		if (command == SRV_MESSAGE && to_uin == user->chat_uin) {
			/* print message to stdout */
			log_info("recv message packet from server\n");
			print_chat_message(user, lp);
			free(lp);
		} else {
			/* add packet to pending list */
			list_add_tail(&lp->list, &user->pending_list);
		}
	}
}

/* process packets in auth mode, pending all packets */
static void auth_process_packets(struct client_user *user)
{
	struct list_head *head = get_recv_list(user);
	/* process all packet on list */
	while (!list_empty(head)) {
		struct list_packet *lp =
			list_first_entry(head, struct list_packet, list);
		list_del(&lp->list);
		/* add packet to pending list */
		list_add_tail(&lp->list, &user->pending_list);
	}
}

static void process_packet(struct client_user *user)
{
	switch (user->mode) {
	case LOGIN_MODE:
		login_process_packets(user);
		break;
	case COMMAND_MODE:
		command_process_packets(user);
		break;
	case CHAT_MODE:
		chat_process_packets(user);
		break;
	case AUTH_MODE:
		auth_process_packets(user);
		break;
	}
}

static void packet_read_handler(struct client_user *user)
{
	bool err_handle = false;
	ssize_t count;
	char buf[8192];

	/* use fucntion fill_packet to generate packet */
	while (1) {
		memset(buf, 0, sizeof(buf));
		if ((count = read(user->socket, buf, sizeof(buf))) < 0) {
			if (errno != EAGAIN) {
				log_warning("read data error\n");
				err_handle = true;
			}
			break;
		} else if (count == 0) {
			/* End of file, The remote has closed the connection */
			err_handle = true;
			break;
		} else {
			log_info("read %d bytes data from %d\n", count, user->socket);
			if (read_packet(&user->reader, buf, count) < 0) {
				log_warning("read packet error, close the conncetion\n");
				err_handle = true;
				break;
			}
		}
	}

	if (err_handle) {
		log_warning("read packet error\n");
	} else {
		struct list_head new_head;
		struct list_head *head = &(user->reader.recv_packet_list);
		struct list_head *last;
		if (user->reader.expect_bytes > 0) {
			last = head->prev->prev;
		} else {
			last = head->prev;
		}
		list_cut_position(&new_head, head, last);
		list_splice_tail(&new_head, get_recv_list(user));

		process_packet(user);
	}
}

static void login_input(struct client_user *user, char *buf, int count)
{
	printf("waiting for login\n");
}

static void command_input(struct client_user *user, char *buf, int count)
{
	char *p;
	if (!strncmp(buf, "add", 3)) {
		/* add contact */
		p = buf + 3;
		uint32_t uin;
		sscanf(p, "%u", &uin);
		cmd_add_contact(user, uin);
	} else if (!strncmp(buf, "msg", 3)) {
		/* get offline msg */
		cmd_offline_msg(user);
	} else if (!strncmp(buf, "nick", 4)) {
		/* set nick name */
		p = buf + 5;
		p[strlen(p) - 1] = '\0';
		cmd_set_nick(user, p);
	} else if (!strncmp(buf, "list", 4)) {
		/* list contact */
		cmd_contact_list(user);
	} else if (!strncmp(buf, "chat", 4)) {
		/* enter chat mode */
		p = buf + 5;
		sscanf(p, "%u", &user->chat_uin);
		printf("%u\n", user->chat_uin);
		user->mode = CHAT_MODE;
		printf("enter chat mode\n");
	} else {
		int i;
		sscanf(buf, "%d", &i);
		if (i > HASH_COUNT(user->offline_msg_table)) {
			printf("unknown command\n");
		} else {
			print_offline_msg(user, i);
		}
	}
}

static void chat_input(struct client_user *user, char *buf, int count)
{
	if (!strncmp(buf, "\\q", 2)) {
		/* quit chat mode */
		user->mode = COMMAND_MODE;
		printf("quit chat mode\n");
		command_process_packets(user);
	} else {
		/* send chat message */
		int length = strlen(buf);
		buf[strlen(buf) - 1] = '\0';
		cmd_message(user, user->chat_uin, buf);
	}
}

static void auth_input(struct client_user *user, char *buf, int count)
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

		cmd_add_contact_reply(user, user->chat_uin, reply_type);

		user->mode = COMMAND_MODE;
		command_process_packets(user);
	} else {
		printf("you should input yes or no\n");
	}
}

static void input_handler(struct client_user *user)
{
	char buf[8192];
	int count;

	memset(buf, 0, sizeof(buf));
	if ((count = read(fileno(stdin), buf, sizeof(buf))) < 0) {
		log_warning("read from stdin error\n");
		return;
	}

	switch (user->mode) {
	case LOGIN_MODE:
		login_input(user, buf, count);
		break;
	case COMMAND_MODE:
		command_input(user, buf, count);
		break;
	case CHAT_MODE:
		chat_input(user, buf, count);
		break;
	case AUTH_MODE:
		auth_input(user, buf, count);
		break;
	default:
		printf("bad mode\n");
		break;
	}
}

static void packet_write_handler(struct client_user *user)
{
	bool err_handle = false;
	int count;

	/* remove all packet from connection */
	struct list_head *head = &user->send_packet_list;
	struct list_packet *packet = NULL;
	while (!list_empty(head)) {
		packet = list_first_entry(head, struct list_packet, list);
		list_del(&packet->list);

		uint16_t length = get_length_host(packet);
		if ((count = write(user->socket, &packet->packet, length)) < 0) {
			if (errno != EAGAIN) {
				log_warning("write data error\n");
				err_handle = true;
			}
			break;
		} else if (count == 0) {
			/* End of file, The remote has closed the connection */
			err_handle = true;
			break;
		} else {
			if (count != length) {
				log_warning("can not write a whole packet\n");
			}
			log_debug("send packet len %hu, cmd %#hx, uin %u to server\n",
					get_length_host(packet),
					get_command_host(packet),
					get_uin_host(packet));
			free(packet);
		}
	}

	if (err_handle) {
		log_warning("write packet error\n");
	} else {
		wait_for_read(user->epoll, user->socket);
	}
}

int epoll_loop(struct client_user *user)
{
	int max_events = 128;
	struct epoll_event *events =
		calloc(max_events, sizeof(struct epoll_event));
	int epoll = user->epoll;
	const int epoll_timeout = 1000;	/* wait 1 second here */

	/* the event loop */
	while (1) {
		int i, n;
		n = epoll_wait(epoll, events, max_events, epoll_timeout);
		for (i = 0; i < n; i++) {
			if ((events[i].events & EPOLLERR) ||
					(events[i].events & EPOLLHUP)) {
				/* an error has occured on this fd */
				log_warning("epoll error\n");
				continue;
			} else if (events[i].events & EPOLLIN) {
				/* we have data on the fd waiting to be read */
				if (user->socket == events[i].data.fd) {
					/* data incoming from socket */
					packet_read_handler(user);
				} else {
					/* data incoming from stdin */
					input_handler(user);
				}
			} else if (events[i].events & EPOLLOUT) {
				/* write buffer is available */
				packet_write_handler(user);
			} else {
				log_warning("other event\n");
			}
		}
		/* TODO: need to enable timer */
		//timer_expire_time(server);
	}

	free(events);
	close(user->socket);
	return 0;
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
