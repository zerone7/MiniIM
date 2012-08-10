#ifndef _CONN_DEFINE_H_
#define _CONN_DEFINE_H_

#include <stddef.h>

#define CONN_SERVER_PORT	27182
#define MAX_EPOLL_EVENTS	10240
#define CLIENT_TIMEOUT		120

#define container_of(ptr, type, member) ({	\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type, member) ); })

#endif
