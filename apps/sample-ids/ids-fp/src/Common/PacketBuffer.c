/*
 * PacketBuffer.c
 *
 *  Created on: Jun 8, 2014
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <time.h>
#include "PacketBuffer.h"

static struct timespec _100_nanos = {0, 100};

static inline void lock(PacketBuffer *q) {
	while (__sync_lock_test_and_set(&(q->lock), 1)) {
		nanosleep(&_100_nanos, NULL);
	}
}

static inline void unlock(PacketBuffer *q) {
	__sync_lock_release(&(q->lock));
}

void packet_buffer_init(PacketBuffer *q) {
	q->size = 0;
	q->head = q->tail = NULL;
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
	q->head = q->tail = NULL;
	q->size = 0;
}

void packet_buffer_enqueue(PacketBuffer *q, InPacket *packet) {
	PacketBufferItem *i;

	lock(q);

	i = (PacketBufferItem*)malloc(sizeof(PacketBufferItem));
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

	if (q->size == 0)
		return NULL;

	lock(q);

	i = q->head;
	res = i->packet;
	q->head = i->next;
	q->head->prev = NULL;
	if (q->size == 1)
		q->tail = NULL;
	q->size--;
	free(i);

	unlock(q);

	return res;
}

InPacket *packet_buffer_peek(PacketBuffer *q) {
	if (q->size == 0)
		return NULL;

	return q->head->packet;
}

InPacket *packet_buffer_pop(PacketBuffer *q, unsigned int src_ip, unsigned int dst_ip, unsigned short src_port, unsigned short dst_port, unsigned int seqnum) {
	PacketBufferItem *i, *temp;
	InPacket *res;

	if (q->size == 0)
		return NULL;

	lock(q);

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

			unlock(q);
			return res;
		}
		i = i->next;
	}

	unlock(q);
	return NULL;
}
