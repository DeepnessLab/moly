/*
 * Timer.h
 *
 *  Created on: Dec 15, 2011
 *      Author: yotam
 */

#ifndef TIMER_H_
#define TIMER_H_

typedef struct {
	struct timeval start;
	struct timeval finish;
	unsigned long micros;
} Timer;

#define BYTES_TO_MBITS 8.0/(1024 * 1024)
#define GET_TRANSFER_RATE(bytes, timerPtr) (((bytes) * BYTES_TO_MBITS) / (((timerPtr)->micros) / 1000000.0))

void startTiming(Timer *timer);
void endTiming(Timer *timer);

#endif /* TIMER_H_ */
