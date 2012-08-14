#include <stdlib.h>
#include "conn_timer.h"
#include "conn_log.h"
#include "conn_packet.h"
#include "conn_server.h"

/* initialize the timer struct */
void timer_init(struct conn_timer *timer)
{
	int i;

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
}

/* mark the connection as 'unsafe', so we can't find it before delete it */
void timer_mark_dead(struct conn_server *server, struct connection *conn)
{
	struct conn_timer *timer = &server->timer;
	if (!is_alive_conn(timer, conn)) {
		return;
	}

	uint8_t current = timer->current;
	/* delay 4 seconds to delete it */
	uint8_t new_index = (timer->current + 4) % timer->max_slots;
	conn->timer_slot = new_index;
	timer_del(conn);
	list_add_tail(&conn->timer_list, &timer->timer_slots[new_index]);
}

/* this function will be called every second */
void timer_expire_time(struct conn_server *server)
{
	timer_tick(&server->timer);

	/* move alive connection */
	struct list_head *alive_list = &server->keep_alive_list;
	struct list_packet *packet;
	struct connection *conn;
	uint32_t uin;
	while (!list_empty(alive_list)) {
		packet = list_first_entry(alive_list, struct list_packet, list);
		list_del(&packet->list);
		uin = get_uin_host(packet);
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
