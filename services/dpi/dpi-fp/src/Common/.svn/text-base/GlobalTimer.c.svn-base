/*
 * GlobalTimer.c
 *
 *  Created on: Dec 22, 2011
 *      Author: yotam
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "GlobalTimer.h"

void *global_timer_thread_start(void *param) {
	GlobalTimer *gtimer;
	double *rate;
	//long int *time;
	unsigned int bytes;
	int i;
	struct timespec t;

	t.tv_sec = 0;
	t.tv_nsec = GLOBAL_TIMER_SAMPLE_INTERVAL;

	gtimer = (GlobalTimer*)param;

	while (!(gtimer->stopped)) {
		//sleep(GLOBAL_TIMER_SAMPLE_INTERVAL);
		nanosleep(&t, NULL);
		//sleep(1);

		//while (!__sync_lock_test_and_set(&(gtimer->report_lock), 1)) { /* busy wait */ }

		endTiming(&(gtimer->t));
/*
		time = (long int*)malloc(sizeof(long int));
		*time = gtimer->t.micros;
		list_enqueue(&(gtimer->times), (void*)time);
*/
		for (i = 0; i < gtimer->num_scanners; i++) {
			bytes = __sync_lock_test_and_set(&(gtimer->bytes_processed[i]), 0);
			rate = (double*)malloc(sizeof(double));
			*rate = GET_TRANSFER_RATE(bytes, &(gtimer->t));

			list_enqueue(&(gtimer->lists[i]), (void*)rate);
		}
		gtimer->intervals++;

		//gtimer->report_lock = 0;

		// Restart timing
		startTiming(&(gtimer->t));
	}

	pthread_exit(NULL);
}

void global_timer_init(GlobalTimer *gtimer, int num_scanners) {
	int i;

	gtimer->num_scanners = num_scanners;
	gtimer->intervals = 0;
	memset(gtimer->bytes_processed, 0, sizeof(unsigned int) * MAX_NUM_OF_SCANNERS);

	gtimer->lists = (LinkedList*)malloc(sizeof(LinkedList) * num_scanners);

	for (i = 0; i < num_scanners; i++) {
		list_init(&(gtimer->lists[i]));
	}
	//list_init(&(gtimer->times));

	gtimer->evt_id = 0;
	memset(gtimer->events, 0, sizeof(GlobalTimerEvent*) * MAX_NUM_OF_EVENTS);

	gtimer->report_lock = 0;
}

void global_timer_start(GlobalTimer *gtimer) {
	gtimer->stopped = 0;
	startTiming(&(gtimer->t));
	pthread_create(&(gtimer->thread), NULL, global_timer_thread_start, (void*)gtimer);
}

void global_timer_set(GlobalTimer *gtimer, int scanner_id, unsigned int bytes_processed) {
#ifdef __GNUC__
	//while (!__sync_lock_test_and_set(&(gtimer->report_lock), 1)) { /* busy wait */ }
	__sync_fetch_and_add(&(gtimer->bytes_processed[scanner_id]), bytes_processed);
	//gtimer->report_lock = 0;
#else
	gtimer->bytes_processed[scanner_id] += bytes_processed;
#endif
}

void global_timer_end(GlobalTimer *gtimer) {
	//endTiming(&(gtimer->t));
	gtimer->stopped = 1;
}

void global_timer_join(GlobalTimer *gtimer) {
	pthread_join(gtimer->thread, NULL);
}

unsigned int global_timer_get_bytes_processed(GlobalTimer *gtimer, int scanner_id) {
	return gtimer->bytes_processed[scanner_id];
}

void global_timer_get_results(GlobalTimer *gtimer, GlobalTimerResult *result) {
	int thread, interval, status;
	double *value;
	//long int *time;

	result->times = (long int*)malloc(sizeof(long int) * gtimer->intervals);
	result->results = (double*)malloc(sizeof(double) * gtimer->num_scanners * gtimer->intervals);
	result->intervals = gtimer->intervals;
	result->num_scanners = gtimer->num_scanners;
/*
	for (interval = 0; interval < gtimer->intervals; interval++) {
		time = list_dequeue(&(gtimer->times), &status);

		if (status != LIST_STATUS_DEQUEUE_SUCCESS) {
			fprintf(stderr, "ERROR: Failed to dequeue time value! (GlobalTimer)\n");
			exit(1);
		}
		result->times[interval] = *time;
	}
*/
	for (thread = 0; thread < gtimer->num_scanners; thread++) {
		for (interval = 0; interval < gtimer->intervals; interval++) {
			value = list_dequeue(&(gtimer->lists[thread]), &status);

			if (status != LIST_STATUS_DEQUEUE_SUCCESS) {
				fprintf(stderr, "ERROR: Failed to dequeue! (GlobalTimer)\n");
				exit(1);
			}

			result->results[gtimer->intervals * thread + interval] = *value;
			//free(value);
		}
	}
}

void global_timer_destroy(GlobalTimer *gtimer) {
	int i;

	for (i = 0; i < gtimer->num_scanners; i++) {
		list_destroy(&(gtimer->lists[i]), 1);
	}
	for (i = 0; i < MAX_NUM_OF_EVENTS; i++) {
		if (gtimer->events[i]) {
			free(gtimer->events[i]);
		}
	}

	free(gtimer->lists);
}

void global_timer_destroy_result(GlobalTimerResult *result) {
	free(result->times);
	free(result->results);
}

void global_timer_set_event(GlobalTimer* gtimer, char *text, char *source) {
	GlobalTimerEvent *evt;
	int id;

	id = __sync_fetch_and_add(&(gtimer->evt_id), 1);

	evt = (GlobalTimerEvent*)malloc(sizeof(GlobalTimerEvent));
	evt->text = text;
	evt->source = source;
	evt->interval = gtimer->intervals;

	gtimer->events[id] = evt;
}

int global_timer_get_events(GlobalTimer *gtimer, GlobalTimerEvent ***events) {
	int i;
	for (i = 0; i < MAX_NUM_OF_EVENTS && gtimer->events[i]; i++)
		;
	*events = (gtimer->events);
	return i;
}
