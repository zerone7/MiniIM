#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "conn_timer.h"
#include "conn_log.h"
#include "conn_packet.h"
#include "conn_server.h"

/* initialize the timer struct */
void timer_init(struct conn_timer *timer)
{
	int i;
	struct itimerval val;
	struct sigaction action;

	/* initialize list head */
	assert(timer);
	timer->current = 0;
	timer->max_slots = CLIENT_TIMEOUT + 8;
	timer->delay = timer->max_slots - CLIENT_TIMEOUT - 1;
	timer->timer_slots = malloc(sizeof(struct list_head) * timer->max_slots);
	assert(timer->timer_slots);
	for (i = 0; i < timer->max_slots; i++) {
		INIT_LIST_HEAD(&timer->timer_slots[i]);
	}

	/* register signal hanbler function */
	action.sa_handler = every_second_func;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	sigaction(SIGALRM, &action, NULL);

	/* initialize timer */
	val.it_value.tv_sec = 1;
	val.it_value.tv_usec = 0;
	val.it_interval = val.it_value;
	setitimer(ITIMER_REAL, &val, NULL);
}

/* this function will be called every second */
void every_second_func(int signo)
{
	log_debug("timer tick, current index %hu\n", srv->timer.current);
	timer_tick(&srv->timer);

	/* move alive connection */
	struct list_head *alive_list = &srv->keep_alive_list;
	struct list_packet *packet;
	struct connection *conn;
	uint32_t uin;
	while (!list_empty(alive_list)) {
		packet = list_first_entry(alive_list, struct list_packet, list);
		list_del(&packet->list);
		uin = get_uin_host(packet);
		allocator_free(&srv->packet_allocator, packet);

		conn = get_conn_by_uin(srv, uin);
		if (conn) {
			timer_move(&srv->timer, conn);
			log_debug("client %u is alive\n", conn->uin);
		}
	}

	/* remove dead connection */
	struct list_head *timeout_list = get_timeout_list(&srv->timer);
	while (!list_empty(timeout_list)) {
		conn = list_first_entry(timeout_list,
				struct connection, timer_list);
		send_offline_to_status(srv, conn->uin);
		log_info("client %u is dead\n", conn->uin);
		close_connection(srv, conn);
	}
}
