#ifndef _CONN_DEFINE_H_
#define _CONN_DEFINE_H_

#include <stddef.h>

#define USER_SERVER_IP		127.0.0.1
#define CONTACT_SERVER_IP	127.0.0.1
#define STATUS_SERVER_IP	127.0.0.1
#define MESSAGE_SERVER_IP	127.0.0.1
#define USER_SERVER_PORT	31415
#define CONTACT_SERVER_PORT	31416
#define STATUS_SERVER_PORT	31417
#define MESSAGE_SERVER_PORT	31418
#define CONN_SERVER_PORT	27182
#define MAX_EPOLL_EVENTS	10240
#define CLIENT_TIMEOUT		120

#define container_of(ptr, type, member) ({	\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type, member) ); })

#endif
