/*
 * GlobalTimer.h
 *
 *  Created on: Dec 22, 2011
 *      Author: yotam
 */

#ifndef GLOBALTIMER_H_
#define GLOBALTIMER_H_

#include <pthread.h>
#include "Timer.h"
#include "../DumpReader/BoundedBuffer/LinkedList.h"

#define MAX_NUM_OF_EVENTS 100
#define MAX_NUM_OF_SCANNERS 128
#define GLOBAL_TIMER_SAMPLE_INTERVAL 100000000 // nanoseconds
#define GLOBAL_TIMER_SCANNER_REPORT_INTERVAL 10 // packets

typedef struct {
	char *text;
	int interval;
	char *source;
} GlobalTimerEvent;

typedef struct {
	Timer t;
	int num_scanners;
	unsigned int bytes_processed[MAX_NUM_OF_SCANNERS];
	pthread_t thread;
	int stopped;
	LinkedList *lists;
	LinkedList times;
	int intervals;
	GlobalTimerEvent *events[MAX_NUM_OF_EVENTS];
	int evt_id;
	int report_lock;
} GlobalTimer;

typedef struct {
	long int *times;
	double *results; // dim is [intervals * num_scanners], access: [intervals * scanner_id + interval_id]
	int intervals;
	int num_scanners;
} GlobalTimerResult;

#define GET_SCANNER_THROUGHPUT_FOR_INTERVAL(gtimer_result, scanner_id, interval_id) \
	(((gtimer_result)->results)[(gtimer_result)->intervals * scanner_id + interval_id])

void global_timer_init(GlobalTimer *gtimer, int num_scanners);
void global_timer_start(GlobalTimer *gtimer);
void global_timer_set(GlobalTimer *gtimer, int scanner_id, unsigned int bytes_processed);
void global_timer_end(GlobalTimer *gtimer);
void global_timer_join(GlobalTimer *gtimer);
unsigned int global_timer_get_bytes_processed(GlobalTimer *gtimer, int scanner_id);
void global_timer_get_results(GlobalTimer *gtimer, GlobalTimerResult *result);
void global_timer_destroy(GlobalTimer *gtimer);
void global_timer_destroy_result(GlobalTimerResult *result);
void global_timer_set_event(GlobalTimer* gtimer, char *text, char *source);
int global_timer_get_events(GlobalTimer *gtimer, GlobalTimerEvent ***events);

#endif /* GLOBALTIMER_H_ */
