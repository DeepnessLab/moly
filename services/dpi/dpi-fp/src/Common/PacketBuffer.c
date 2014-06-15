/*
 * PacketBuffer.c
 *
 *  Created on: Jun 8, 2014
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "PacketBuffer.h"

#ifndef USE_MUTEX
static struct timespec _100_nanos = {0, 100};
#endif

static inline void lock(PacketBuffer *q) {
#ifdef USE_MUTEX
	pthread_mutex_lock(&(q->mutex));
#else
	while (__sync_lock_test_and_set(&(q->lock), 1)) {
		nanosleep(&_100_nanos, NULL);
	}
#endif
}

static inline void unlock(PacketBuffer *q) {
#ifdef USE_MUTEX
	pthread_mutex_unlock(&(q->mutex));
#else
	__sync_lock_release(&(q->lock));
#endif
}

void packet_buffer_init(PacketBuffer *q) {
	q->size = 0;
	q->head = q->tail = NULL;
#ifdef USE_MUTEX
	pthread_mutex_init(&(q->mutex), NULL);
#else
	q->lock = 0;
#endif
}

void packet_buffer_destroy(PacketBuffer *q, int destroyItems) {
	PacketBufferItem *item, *next;
	item = q->head;
	while (item) {
		if (destroyItems) {
			free(item->packet);
		}
		next = item->next;
		free(item);
		item = next;
	}
#ifdef USE_MUTEX
	pthread_mutex_destroy(&(q->mutex));
#endif
	q->head = q->tail = NULL;
	q->size = 0;
}

void packet_buffer_enqueue(PacketBuffer *q, InPacket *packet) {
	PacketBufferItem *i;

	lock(q);

	i = (PacketBufferItem*)malloc(sizeof(PacketBufferItem));
	if (!i) {
		fprintf(stderr, "FATAL: Out of memory\n");
		exit(1);
	}
	i->packet = packet;
	i->next = NULL;
	i->prev = q->tail;
	if (q->tail)
		q->tail->next = i;
	q->tail = i;
	if (!q->head)
		q->head = i;
	q->size++;

	unlock(q);
}

InPacket *packet_buffer_dequeue(PacketBuffer *q) {
	void *res;
	PacketBufferItem *i;

	lock(q);

	if (q->size == 0) {
		unlock(q);
		return NULL;
	}

	i = q->head;
	res = i->packet;
	q->head = i->next;
	if (q->head)
		q->head->prev = NULL;
	if (q->size == 1)
		q->tail = NULL;
	q->size--;
	free(i);

	unlock(q);

	return res;
}

InPacket *packet_buffer_peek(PacketBuffer *q) {
	InPacket *res;

	lock(q);
	if (q->size == 0) {
		unlock(q);
		return NULL;
	}

	res = q->head->packet;
	unlock(q);
	return res;
}

InPacket *packet_buffer_pop(PacketBuffer *q, unsigned int src_ip, unsigned int dst_ip, unsigned short src_port, unsigned short dst_port, unsigned int seqnum) {
	PacketBufferItem *i, *temp;
	InPacket *res;

	lock(q);

	if (q->size == 0) {
		unlock(q);
		return NULL;
	}

	i = q->head;
	while (i) {
		if (i->packet->seqnum == seqnum &&
				i->packet->packet.ip_src == src_ip &&
				i->packet->packet.ip_dst == dst_ip &&
				i->packet->packet.transport.tp_src == src_port &&
				i->packet->packet.transport.tp_dst == dst_port) {
			res = i->packet;

			// Remove element
			temp = i->prev;
			if (temp)
				temp->next = i->next;
			if (i->next)
				i->next->prev = temp;
			if (i == q->head)
				q->head = i->next;
			if (i == q->tail)
				q->tail = temp;
			q->size--;
			free(i);

			unlock(q);
			return res;
		}
		i = i->next;
	}

	unlock(q);
	return NULL;
}

#define TEST_PACKET_COUNT 1024

#define ASSERT(cond, msg) \
	if (!(cond)) { fprintf(stderr, "ASSERTION FAILED: %s\n", (msg)); exit(1); }

int _main() {
	// Small test program
	InPacket packets[TEST_PACKET_COUNT];
	int i;
	PacketBuffer buff;
	InPacket *pkt;

	for (i = 0; i < TEST_PACKET_COUNT; i++) {
		packets[i].packet.ip_src = i;
		packets[i].packet.ip_dst = i+1;
		packets[i].packet.ip_id = 0;
		packets[i].packet.ip_len = 0;
		packets[i].packet.ip_proto = 6;
		packets[i].packet.ip_tos = 0;
		packets[i].packet.ip_ttl = 64;
		packets[i].packet.seqnum = i;
		packets[i].packet.payload_len = 5;
		packets[i].packet.payload = (unsigned char *)"hello";
		packets[i].packet.transport.tp_src = 80;
		packets[i].packet.transport.tp_dst = 81;
		packets[i].pktdata = (unsigned char*)"hello";
		packets[i].pkthdr.caplen = 0;
		packets[i].pkthdr.len = 5;
		packets[i].pkthdr.ts.tv_sec = 0;
		packets[i].pkthdr.ts.tv_usec = 0;
	}

	packet_buffer_init(&buff);

	for (i = 0; i < TEST_PACKET_COUNT; i++) {
		packet_buffer_enqueue(&buff, &(packets[i]));
		ASSERT(buff.size == (i+1), "Unexpected size of queue after enqueue");
	}

	for (i = 0; i < TEST_PACKET_COUNT; i++) {
		pkt = packet_buffer_dequeue(&buff);
		ASSERT(pkt == &(packets[i]), "dequeued packet data is different");
		ASSERT(buff.size == (TEST_PACKET_COUNT - i - 1), "Unexpected size of queue after dequeue");
	}

	ASSERT(buff.size == 0, "Queue should be empty by now");
	ASSERT(buff.head == NULL, "Head is not NULL but queue is empty");
	ASSERT(buff.tail == NULL, "Tail is not NULL but queue is empty");

	for (i = 0; i < TEST_PACKET_COUNT; i++) {
		packet_buffer_enqueue(&buff, &(packets[i]));
		ASSERT(buff.size == (i+1), "Unexpected size of queue after enqueue");
	}

	for (i = 0; i < TEST_PACKET_COUNT; i += 7) {
		pkt = packet_buffer_pop(&buff, packets[i].packet.ip_src, packets[i].packet.ip_dst, packets[i].packet.transport.tp_src, packets[i].packet.transport.tp_dst, packets[i].seqnum);
		ASSERT(pkt == &(packets[i]), "poped packet data is different")
	}

	packet_buffer_destroy(&buff, 0);

	printf("TEST SUCCEEDED\n");

	return 0;
}
