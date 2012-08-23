#ifndef _CONN_TIMER_H_
#define _CONN_TIMER_H_

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "list.h"
#include "conn_define.h"
#include "conn_connection.h"

/* timer structure used for client timeout */
struct conn_timer {
	struct list_head *timer_slots;
	uint8_t current;
	uint8_t max_slots;
};

/* timer tick, simply increase current, call this once each second */
static inline void timer_tick(struct conn_timer *timer)
{
	assert(timer);
	timer->current = (timer->current + 1) % timer->max_slots;
}

/* get current list head, they are timeouted */
static inline struct list_head* get_timeout_list(struct conn_timer *timer)
{
	assert(timer && timer->current < timer->max_slots);
	return &timer->timer_slots[timer->current];
}

/* add a connection to timer */
static inline void timer_add(struct conn_timer *timer, struct connection *conn)
{
	assert(timer && conn);
	int timeout_seconds = timer->current + timer->max_slots - 1;
	int insert_index = timeout_seconds % timer->max_slots;
	conn->timer_slot = (uint8_t)insert_index;
	list_add_tail(&conn->timer_list, &timer->timer_slots[insert_index]);
}

/* delete a connection from timer */
static inline void timer_del(struct connection *conn)
{
	assert(conn);
	list_del(&conn->timer_list);
}

/* move a connection from one slot to another slot */
static inline void timer_mark_alive(struct conn_timer *timer, struct connection *conn)
{
	assert(timer && conn);
	timer_del(conn);
	timer_add(timer, conn);
}

void timer_init(struct conn_timer *timer);
struct conn_server;
void timer_expire_time(struct conn_server *server);

#endif
