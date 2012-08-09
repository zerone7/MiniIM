#ifndef _CONN_TIMER_H_
#define _CONN_TIMER_H_

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include "conn_define.h"
#include "conn_list.h"
#include "conn_connection.h"

/* timer structure used for client timeout */
struct conn_timer {
	struct list_head *timer_slots;
	uint8_t current;
	uint8_t max_slots;
	uint8_t delay;
};

/* timer tick, simply increase current, call this once each second */
static inline void timer_tick(struct conn_timer *timer)
{
	assert(timer);
	timer->current = (timer->current + 1) % timer->max_slots;
}

/* block singal alarm */
static inline void block_sigalarm()
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigprocmask(SIG_BLOCK, &mask, NULL);
}

/* unblock signal alarm */
static inline void unblock_sigalarm()
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

/* get current list head, they are timeouted */
static inline struct list_head* get_timeout_list(struct conn_timer *timer)
{
	assert(timer && timer->current < timer->max_slots);
	return &timer->timer_slots[timer->current];
}

/* add a connection to timer, this function
 * should not be called in process context */
static inline void __timer_add(struct conn_timer *timer, struct connection *conn)
{
	assert(timer && conn);
	int timeout_seconds = timer->current + timer->max_slots - 1;
	int insert_index = timeout_seconds % timer->max_slots;
	conn->timer_slot = (uint8_t)insert_index;
	list_add(&conn->timer_list, &timer->timer_slots[insert_index]);
}

/* add a connection to timer, called in process context */
static inline void timer_add(struct conn_timer *timer,
		struct connection *conn)
{

	/* block the SIGALRM signal before call __timer_add */
	block_sigalarm();

	__timer_add(timer, conn);

	/* unblock the SIGALRM signal after call __timer_add */
	unblock_sigalarm();
}

/* delete a connection from timer */
static inline void __timer_del(struct connection *conn)
{
	assert(conn);
	list_del(&conn->timer_list);
}

/* delete a connection from timer, called in process context */
static inline void timer_del(struct connection *conn)
{
	/* block the SIGALRM signal before call __timer_del */
	block_sigalarm();

	__timer_del(conn);

	/* unblock the SIGALRM signal after call __timer_del */
	unblock_sigalarm();
}

/* move a connection from one slot to another slot */
static inline void timer_move(struct conn_timer *timer, struct connection *conn)
{
	assert(timer && conn);
	__timer_del(conn);
	__timer_add(timer, conn);
}

static inline bool is_safe_conn(struct conn_timer *timer, struct connection *conn)
{
	if (conn->timer_slot > timer->current) {
		return (conn->timer_slot - timer->current) > timer->delay;
	} else {
		return (conn->timer_slot + timer->max_slots -
				timer->current) > timer->delay;
	}
}

void timer_init(struct conn_timer *timer);
void every_second_func(int signo);
extern struct conn_timer *timer;

#endif
