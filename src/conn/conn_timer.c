#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "conn_timer.h"
#include "conn_log.h"

struct conn_timer *timer;

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
	timer_tick(timer);
	/* TODO: need to be implemented */
	log_debug("timer tick every second\n");
}
