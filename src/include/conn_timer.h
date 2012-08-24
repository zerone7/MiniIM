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
	uint8_t prev;
	uint8_t current;
	uint8_t max_slots;
};

/* timer tick, simply increase current, call this once each second */
static inline void timer_tick(struct conn_timer *timer, uint8_t interval)
{
	assert(timer && interval < timer->max_slots);
	timer->prev = timer->current;
	timer->current = (timer->current + interval) % timer->max_slots;
}

/* get current list head, they are timeouted */
static inline struct list_head* get_timeout_list(struct conn_timer *timer)
{
	assert(timer && timer->current < timer->max_slots);

	int i, start = timer->prev + 1;
	int end = (timer->prev < timer->current) ?
		timer->current : (timer->current + timer->max_slots);
	struct list_head *ret_head =
		&timer->timer_slots[start++ % timer->max_slots];
	for (i = start; i <= end; i++) {
		list_splice_tail_init(&timer->timer_slots[i % timer->max_slots],
				ret_head);
	}

	return ret_head;
}

/* add a connection to timer */
static inline void timer_add(struct conn_timer *timer, struct connection *conn)
{
	assert(timer && conn);
	int insert_index = (!timer->current) ? (timer->max_slots - 1) :
		(timer->current - 1);
	/* just insert on current list, because current is not going to
	 * timeout in the next second, current + 1 is going to */
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
