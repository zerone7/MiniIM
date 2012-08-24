#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include "conn_timer.h"
#include "conn_log.h"
#include "conn_packet.h"
#include "conn_server.h"

#define TIMER_TICK_INTERVAL	1

static uint32_t global_time_value = 0;

static void change_time_value(int signo)
{
	global_time_value++;
}

/* initialize the timer struct */
void timer_init(struct conn_timer *timer)
{
	int i;

	/* initialize list head */
	assert(timer);
	timer->prev = 0;
	timer->current = 0;
	timer->max_slots = CLIENT_TIMEOUT + 1;
	timer->timer_slots = malloc(sizeof(struct list_head) * timer->max_slots);
	assert(timer->timer_slots);
	for (i = 0; i < timer->max_slots; i++) {
		INIT_LIST_HEAD(&timer->timer_slots[i]);
	}

	/* init real time timer */
	struct itimerval val;
	val.it_value.tv_sec = TIMER_TICK_INTERVAL;
	val.it_value.tv_usec = 0;
	val.it_interval = val.it_value;
	setitimer(ITIMER_REAL, &val, NULL);

	/* init signal handler */
	struct sigaction act;
	act.sa_handler = change_time_value;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGALRM, &act, NULL);
}

/* this function will be called every second */
void timer_expire_time(struct conn_server *server)
{
	static uint32_t local_time_value = 0;
	int interval = global_time_value - local_time_value;

	if (!interval) {
		return;
	}

	assert(interval > 0 && interval < server->timer.max_slots);
	local_time_value += interval;
	timer_tick(&server->timer, (uint8_t)interval);

	/* move alive connection */
	struct list_head *alive_list = &server->keep_alive_list;
	struct list_packet *packet;
	struct connection *conn;
	uint32_t uin;
	while (!list_empty(alive_list)) {
		packet = list_first_entry(alive_list, struct list_packet, list);
		list_del(&packet->list);
		uin = get_uin(packet);
		/* NOTE: use shared data here, not recommendate */
		allocator_free(&server->packet_allocator, packet);

		conn = get_conn_by_uin(server, uin);
		if (conn) {
			timer_mark_alive(&server->timer, conn);
			log_debug("client %u is alive\n", uin);
		} else {
			/* we can't get conn by uin, which means
			 * the conn is already dead */
			log_debug("client %u is already dead\n", uin);
		}
	}

	/* remove dead connection */
	struct list_head *timeout_list = get_timeout_list(&server->timer);
	while (!list_empty(timeout_list)) {
		conn = list_first_entry(timeout_list,
				struct connection, timer_list);
		send_offline_to_status(server, conn->uin);
		log_info("client %u is timeout\n", conn->uin);
		close_connection(server, conn);
	}
}
