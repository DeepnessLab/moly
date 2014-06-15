/*
 * PacketBuffer.h
 *
 *  Created on: Jun 8, 2014
 *      Author: yotamhc
 */

#ifndef PACKETBUFFER_H_
#define PACKETBUFFER_H_

#include <pthread.h>
#include <pcap.h>
#include "Types.h"

//#define USE_MUTEX

typedef struct {
	struct pcap_pkthdr pkthdr;
	unsigned char *pktdata;
	unsigned long timestamp;
	unsigned int seqnum;
	Packet packet;
} InPacket;

typedef struct sq_item {
	InPacket *packet;
	struct sq_item *next, *prev;
} PacketBufferItem;

typedef struct {
	PacketBufferItem *head, *tail;
	int size;
#ifdef USE_MUTEX
	pthread_mutex_t mutex;
#else
	int lock;
#endif
} PacketBuffer;

void packet_buffer_init(PacketBuffer *q);

void packet_buffer_destroy(PacketBuffer *q, int destroyItems);

void packet_buffer_enqueue(PacketBuffer *q, InPacket *packet);

InPacket *packet_buffer_dequeue(PacketBuffer *q);

InPacket *packet_buffer_peek(PacketBuffer *q);

InPacket *packet_buffer_pop(PacketBuffer *q, unsigned int src_ip, unsigned int dst_ip, unsigned short src_port, unsigned short dst_port, unsigned int seqnum);

#endif /* PACKETBUFFER_H_ */
