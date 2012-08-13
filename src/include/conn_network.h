#ifndef _CONN_SERVER_H_
#define _CONN_SERVER_H_

#include <stdint.h>
#include "conn_server.h"

int setup_socket(struct conn_server *server, uint16_t port);
int setup_epoll(struct conn_server *server, uint32_t max_events);
int epoll_loop(struct conn_server *server);
int add_to_epoll(int efd, int sfd);

#endif
