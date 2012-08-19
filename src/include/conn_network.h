#ifndef _CONN_NETWORK_H_
#define _CONN_NETWORK_H_

#include <stdint.h>
#include "network.h"
#include "conn_log.h"
#include "conn_server.h"

int setup_socket(struct conn_server *server, uint16_t port);
int setup_epoll(struct conn_server *server, uint32_t max_events);
int epoll_loop(struct conn_server *server);

#endif
