/*
 * Types.h
 *
 *  Created on: Jun 11, 2014
 *      Author: yotamhc
 */

#ifndef TYPES_H_
#define TYPES_H_

typedef struct {
	// IPv4
	in_addr_t ip_src;
	in_addr_t ip_dst;
	unsigned short ip_id;
	unsigned char ip_tos;
	unsigned char ip_ttl;
	unsigned char ip_proto;
	unsigned short ip_len;

	union {
		struct _icmp {
			// ICMP
			unsigned short icmp_type;
			unsigned short icmp_code;
		} icmp;
		struct _transport {
			// Transport
			unsigned short tp_src;
			unsigned short tp_dst;
		} transport;
	};
	unsigned int seqnum;

	// Data
	unsigned int payload_len;
	unsigned char *payload;
} Packet;

#endif /* TYPES_H_ */
