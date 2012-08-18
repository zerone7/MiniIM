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

static void srv_error(struct client_user *user, struct list_packet *lp)
{
	printf("server error\n");
	free(lp);
}

static void srv_login_ok(struct client_user *user, struct list_packet *lp)
{
	printf("login ok\n");
	free(lp);
}

static void srv_set_nick_ok(struct client_user *user, struct list_packet *lp)
{
	printf("change nick name success\n");
	free(lp);
}

static void srv_add_contact_wait(struct client_user *user, struct list_packet *lp)
{
	printf("wait for his/her permission\n");
	free(lp);
}

static void process_packet(struct client_user *user)
{
	struct list_head *head = get_recv_list(user);
	/* process all packet on list */
	while (!list_empty(head)) {
		struct list_packet *lp =
			list_first_entry(head, struct list_packet, list);
		list_del(&lp->list);
		uint16_t command = get_command_host(lp);
		switch (command) {
		case SRV_ERROR:
			srv_error(user, lp);
			break;
		case SRV_LOGIN_OK:
			break;
		case SRV_SET_NICK_OK:
			break;
		case SRV_ADD_CONTACT_WAIT:
			break;
		case SRV_CONTACT_LIST:
			break;
		case SRV_CONTACT_INFO_MULTI:
			break;
		case SRV_OFFLINE_MSG:
			break;
		case SRV_OFFLINE_MSG_DONE:
			break;
		default:
			break;
		}
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
		process_packet(user);
	}
}

static void login_input(struct client_user *user, const char *buf, int count)
{
	printf("waiting for login in\n");
	/* TODO: used for test */
	user->mode = COMMAND_MODE;
}

static void command_input(struct client_user *user, const char *buf, int count)
{
	const char *p;
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
		cmd_set_nick(user, p);
	} else if (!strncmp(buf, "list", 4)) {
		/* list contact */
		cmd_contact_list(user);
	} else if (!strncmp(buf, "chat", 4)) {
		/* enter chat mode */
		p = buf + 5;
		sscanf(p, "%u", &user->chat_uin);
		user->mode = CHAT_MODE;
		printf("enter chat mode\n");
	} else {
		printf("unknown command\n");
	}
}

static void chat_input(struct client_user *user, const char *buf, int count)
{
	if (!strncmp(buf, "\\q", 2)) {
		/* quit chat mode */
		user->mode = COMMAND_MODE;
		printf("quit chat mode\n");
	} else {
		/* send chat message */
		cmd_message(user, user->chat_uin, buf);
	}
}

static void auth_input(struct client_user *user, const char *buf, int count)
{
	if (!strncmp(buf, "yes", 3)) {
		/* agree */
		uint16_t reply_type = 0x0001;
		cmd_add_contact_reply(user, user->chat_uin, reply_type);
	} else if (!strncmp(buf, "no", 2)) {
		/* reject */
		uint16_t reply_type = 0x0002;
		cmd_add_contact_reply(user, user->chat_uin, reply_type);
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
