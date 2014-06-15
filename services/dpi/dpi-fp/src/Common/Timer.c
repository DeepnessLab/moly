/*
 * Timer.c
 *
 *  Created on: Dec 15, 2011
 *      Author: yotam
 */
#include <stdlib.h>
#include <sys/time.h>
#include "Timer.h"

void startTiming(Timer *timer) {
	timer->micros = 0;
	gettimeofday(&(timer->start), NULL);
}

void endTiming(Timer *timer) {
	gettimeofday(&(timer->finish), NULL);
	timer->micros += (timer->finish.tv_sec * 1000000 + timer->finish.tv_usec) - (timer->start.tv_sec * 1000000 + timer->start.tv_usec);
}
