#include <pcap.h>
#include <pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "Sniffer.h"
#include "MatchReport.h"
#include "../Common/PacketBuffer.h"

#define MAX_PACKET_SIZE 65535
#define MAX_REPORTED_RULES 65535
#define MAX_REPORTS 65535
#define STR_ANY "any"
#define STR_FILTER "ip"
#define MAGIC_NUM 0xDEE4
#define BUFFER_TIMEOUT 10 // seconds
#define BUFFER_CLEANNING_INTERVAL 3 // seconds
#define IP_TOS_HAS_MATCHES_MASK 0xC0
#define IP_TOS_UNSET_MATCHES_MASK 0x3F

#define REPORT_PACKET_OFFSET_MAGIC_NUM 0
#define REPORT_PACKET_OFFSET_NUM_REPORTS 2
#define REPORT_PACKET_OFFSET_SEQNUM 4
#define REPORT_PACKET_OFFSET_FLOW_OFF 8
#define REPORT_PACKET_OFFSET_REPORTS_START 12
#define REPORT_PACKET_REPORT_SIZE 4
#define REPORT_PACKET_OFFSET_START_IDX 2

#define GET_MBPS(bytes, usecs) \
	((bytes) * 8.0 * 1000000) / ((usecs) * 1024 * 1024)

typedef struct {
	int counter;
	int linkHdrLen;
	int last;
	pcap_t *pcap_in;
	pcap_t *pcap_out;
	struct timeval start, end, first_packet, last_packet;
	int started; // Used to determine if a packet is the first one we see
	long bytes;
	MatchReport reports[MAX_REPORTED_RULES];
	int num_reports;
	PacketBuffer dataPacketQueue;
	PacketBuffer matchPacketQueue;
	pthread_t bufferWorker;
	int terminated;
} ProcessorData;

static ProcessorData *_global_processor;

ProcessorData *init_processor(pcap_t *pcap_in, pcap_t *pcap_out, int linkHdrLen, int last) {
	ProcessorData *processor;

	processor = (ProcessorData*)malloc(sizeof(ProcessorData));
	if (!processor) {
		fprintf(stderr, "FATAL: Out of memory\n");
		exit(1);
	}

	processor->counter = 0;
	processor->pcap_in = pcap_in;
	processor->pcap_out = pcap_out;
	processor->linkHdrLen = linkHdrLen;
	processor->last = last;
	processor->bytes = 0;
	memset(processor->reports, 0, sizeof(MatchReport) * MAX_REPORTED_RULES);
	processor->num_reports = 0;
	processor->terminated = 0;
	processor->started = 0;

	packet_buffer_init(&(processor->dataPacketQueue));
	packet_buffer_init(&(processor->matchPacketQueue));

	return processor;
}

void destroy_processor(ProcessorData *processor) {
	packet_buffer_destroy(&(processor->dataPacketQueue), 1);
	packet_buffer_destroy(&(processor->matchPacketQueue), 1);
	free(processor);
}

static inline InPacket *buffer_packet(Packet *packet, const struct pcap_pkthdr *pkthdr, const unsigned char *pktptr, unsigned int seqnum_key) {
	InPacket *res;

	res = (InPacket*)malloc(sizeof(InPacket));
	if (!res) {
		fprintf(stderr, "FATAL: Out of memory\n");
		exit(1);
	}
	res->pkthdr = *pkthdr;
	res->pktdata = (unsigned char*)malloc(sizeof(unsigned char) * pkthdr->len);
	if (!(res->pktdata)) {
		fprintf(stderr, "FATAL: Out of memory\n");
		exit(1);
	}
	memcpy(res->pktdata, pktptr, sizeof(unsigned char) * pkthdr->len);
	res->seqnum = seqnum_key;
	res->timestamp = time(0);
	res->packet = *packet;
	return res;
}

static inline void free_buffered_packet(InPacket *pkt) {
	free(pkt->pktdata);
	free(pkt);
}

void *process_buffer_timeout(void *param) {
	InPacket *pkt;
	ProcessorData *processor;

	processor = (ProcessorData*)param;

	// TODO: Also clean matchPacketQueue

	while (!processor->terminated) {
		// Wait
		sleep(BUFFER_CLEANNING_INTERVAL);

		if (processor->terminated)
			break;

		// Clean buffer
		pkt = packet_buffer_peek(&(processor->dataPacketQueue));

		while (pkt && pkt->timestamp + BUFFER_TIMEOUT < time(0)) {
			pkt = packet_buffer_dequeue(&(processor->dataPacketQueue));
			// Drop packet!
			free_buffered_packet(pkt);

			pkt = packet_buffer_peek(&(processor->dataPacketQueue));
		}
	}

	// Clear buffer
	pkt = packet_buffer_dequeue(&(processor->dataPacketQueue));

	while (pkt) {
		free_buffered_packet(pkt);
		pkt = packet_buffer_dequeue(&(processor->dataPacketQueue));
	}
	return NULL;
}

static inline void parse_packet(ProcessorData *processor, const unsigned char *packetptr, Packet *packet) {
    struct ip* iphdr;
    struct icmp* icmphdr;
    struct tcphdr* tcphdr;
    struct udphdr* udphdr;
    //char iphdrInfo[256], srcip[256], dstip[256];
    //unsigned short id, seq;
    unsigned int transport_len;

    // Skip the datalink layer header and get the IP header fields.
    packetptr += processor->linkHdrLen;
    iphdr = (struct ip*)packetptr;
    packet->ip_src = iphdr->ip_src.s_addr;
    packet->ip_dst = iphdr->ip_dst.s_addr;
    packet->ip_id = ntohs(iphdr->ip_id);
    packet->ip_tos = iphdr->ip_tos;
    packet->ip_ttl = iphdr->ip_ttl;
    packet->ip_proto = iphdr->ip_p;
    packet->ip_len = ntohs(iphdr->ip_len);

    packet->payload = NULL;
    packet->payload_len = 0;

    /*
    strcpy(srcip, inet_ntoa(iphdr->ip_src));
    strcpy(dstip, inet_ntoa(iphdr->ip_dst));
    sprintf(iphdrInfo, "ID:%d TOS:0x%x, TTL:%d IpLen:%d DgLen:%d",
            ntohs(iphdr->ip_id), iphdr->ip_tos, iphdr->ip_ttl,
            4*iphdr->ip_hl, ntohs(iphdr->ip_len));
    */

    // Advance to the transport layer header then parse and display
    // the fields based on the type of header: tcp, udp or icmp.
    packet->seqnum = 0;
    packet->transport.tp_src = 0;
    packet->transport.tp_dst = 0;
    packetptr += 4*iphdr->ip_hl;
    switch (iphdr->ip_p)
    {
    case IPPROTO_TCP:
        tcphdr = (struct tcphdr*)packetptr;
#ifdef __APPLE__
        transport_len = 4 * tcphdr->th_off;
        packet->transport.tp_src = tcphdr->th_sport;
        packet->transport.tp_dst = tcphdr->th_dport;
        packet->seqnum = tcphdr->th_seq;
#elif __linux__
        transport_len = 4 * tcphdr->doff;
        packet->transport.tp_src = tcphdr->source;
        packet->transport.tp_dst = tcphdr->dest;
        packet->seqnum = tcphdr->seq;
#endif
        packet->payload_len = ntohs(iphdr->ip_len) - (4*iphdr->ip_hl) - transport_len;
        packetptr += transport_len;
        packet->payload = (unsigned char *)packetptr;
        break;

    case IPPROTO_UDP:
        udphdr = (struct udphdr*)packetptr;
        transport_len = 8;
#ifdef __APPLE__
        packet->transport.tp_src = udphdr->uh_sport;
        packet->transport.tp_dst = udphdr->uh_dport;
        packet->seqnum = udphdr->uh_sum;
#elif __linux__
        packet->transport.tp_src = udphdr->source;
        packet->transport.tp_dst = udphdr->dest;
        packet->seqnum = udphdr->check;
#endif
        /*
        printf("UDP  %s:%d -> %s:%d\n", srcip, ntohs(udphdr->uh_sport),
               dstip, ntohs(udphdr->uh_dport));
        printf("%s\n", iphdrInfo);
        */
        packet->payload_len = ntohs(iphdr->ip_len) - (4*iphdr->ip_hl) - transport_len;
        packetptr += transport_len;
        packet->payload = (unsigned char *)packetptr;
        break;

    case IPPROTO_ICMP:
        icmphdr = (struct icmp*)packetptr;
        packet->icmp.icmp_type = icmphdr->icmp_type;
        packet->icmp.icmp_code = icmphdr->icmp_code;
        packet->seqnum = 0;
        /*
        printf("ICMP %s -> %s\n", srcip, dstip);
        printf("%s\n", iphdrInfo);
        memcpy(&id, (u_char*)icmphdr+4, 2);
        memcpy(&seq, (u_char*)icmphdr+6, 2);
        printf("Type:%d Code:%d ID:%d Seq:%d\n", icmphdr->type, icmphdr->code,
               ntohs(id), ntohs(seq));
        */
        packet->payload_len = 0;
        packet->payload = NULL;
        break;
    }
}

static inline void handle_matches(ProcessorData *processor, Packet *dataPkt, const struct pcap_pkthdr *dataPkthdr, const unsigned char *dataPacketPtr,
		Packet *matchPkt, const struct pcap_pkthdr *matchPkthdr, const unsigned char *matchPacketPtr) {
	int num_reports, i, total_reports;
	//unsigned int flow_offset;
	unsigned char *ptr;

	num_reports = ((0x0FFFF) & (ntohs(*(unsigned short*)(&(matchPkt->payload[REPORT_PACKET_OFFSET_NUM_REPORTS])))));
	total_reports = num_reports + processor->num_reports;
	//flow_offset = ((0x0FFFFFFFF) & (ntohl(*(unsigned int*)(&(packet.payload[REPORT_PACKET_OFFSET_FLOW_OFF])))));
	if (num_reports > 0 && total_reports < MAX_REPORTED_RULES) {
		for (i = processor->num_reports; i < total_reports; i++) {
			processor->reports[i].rid = ntohs(*(unsigned short*)(&(matchPkt->payload[REPORT_PACKET_OFFSET_REPORTS_START + (i * REPORT_PACKET_REPORT_SIZE)])));
			processor->reports[i].startIdxInPacket = (int)ntohs(*(short*)(&(matchPkt->payload[REPORT_PACKET_OFFSET_REPORTS_START + REPORT_PACKET_OFFSET_START_IDX + (i * REPORT_PACKET_REPORT_SIZE)])));
		}
	}
	processor->num_reports = total_reports;
	processor->bytes += dataPkt->payload_len;

	// Forward both packets
	ptr = (unsigned char *)dataPacketPtr;
	if (processor->last) {
		// Set ECN bits to 0
		ptr[processor->linkHdrLen + 1] = dataPacketPtr[processor->linkHdrLen + 1] & IP_TOS_UNSET_MATCHES_MASK;
	}
#ifdef VERBOSE
	printf("Forwarding data packet... (length: %d, content: %s)\n", dataPkthdr->len, dataPacketPtr);
#endif
	pcap_sendpacket(processor->pcap_out, ptr, dataPkthdr->len);
	if (!processor->last) {
#ifdef VERBOSE
		printf("Forwarding match packet..\n");
#endif
		pcap_sendpacket(processor->pcap_out, matchPacketPtr, matchPkthdr->len);
	}
}

void process_packet(unsigned char *arg, const struct pcap_pkthdr *pkthdr, const unsigned char *packetptr) {
	ProcessorData *processor;
	Packet packet;
	unsigned int seqnum_id;
	InPacket *bpkt;

	processor = (ProcessorData*)arg;

	if (!processor->started) {
		processor->started = 1;
		gettimeofday(&(processor->first_packet), NULL);
	}

	parse_packet(processor, packetptr, &packet);

	if (packet.ip_proto == IPPROTO_UDP && ntohs(*(unsigned short*)(packet.payload)) == MAGIC_NUM) {
		// Packet contains matching results
		seqnum_id = ((0x0FFFFFFFF) & (ntohl(*(unsigned int*)(&(packet.payload[REPORT_PACKET_OFFSET_SEQNUM])))));
#ifdef VERBOSE
        printf("Received matching results packet (seqnum=%u)\n", seqnum_id);
#endif
        // Find corresponding data packet
		bpkt = packet_buffer_pop(&(processor->dataPacketQueue), packet.ip_src, packet.ip_dst, packet.transport.tp_src, packet.transport.tp_dst, seqnum_id);
		// Handle matches
		if (bpkt) {
			// Found corresponding data packet
#ifdef VERBOSE
		    printf("Found corresponding data packet\n");
#endif
		    handle_matches(processor, &(bpkt->packet), &(bpkt->pkthdr), bpkt->pktdata, &packet, pkthdr, packetptr);
			free_buffered_packet(bpkt);
		} else {
			// Buffer match packet
#ifdef VERBOSE
            printf("Corresponding packet was not found, buffering match packet\n");
#endif
            bpkt = buffer_packet(&packet, pkthdr, packetptr, seqnum_id);
            packet_buffer_enqueue(&(processor->matchPacketQueue), bpkt);
		}
	} else if ((packet.ip_tos & IP_TOS_HAS_MATCHES_MASK) == IP_TOS_HAS_MATCHES_MASK) {
		// Packet has matches
#ifdef VERBOSE
        printf("Received a data packet that has matches (seqnum=%u)\n", packet.seqnum);
#endif
        // Find corresponding match packet
		bpkt = packet_buffer_pop(&(processor->matchPacketQueue), packet.ip_src, packet.ip_dst, packet.transport.tp_src, packet.transport.tp_dst, packet.seqnum);
		// Handle matches
		if (bpkt) {
#ifdef VERBOSE
		    printf("Found corresponding match packet\n");
#endif
		    handle_matches(processor, &packet, pkthdr, packetptr, &(bpkt->packet), &(bpkt->pkthdr), bpkt->pktdata);
			free_buffered_packet(bpkt);
		} else {
#ifdef VERBOSE
		    printf("No corresponding match packet, buffering data packet\n");
#endif
		    bpkt = buffer_packet(&packet, pkthdr, packetptr, packet.seqnum);
			packet_buffer_enqueue(&(processor->dataPacketQueue), bpkt);
		}
	} else {
		// Regular packet with no matches, forward it
#ifdef VERBOSE
        printf("Received a data packet with no matches, forwarding it\n");
#endif
        pcap_sendpacket(processor->pcap_out, packetptr, pkthdr->len);
	}
	gettimeofday(&(processor->last_packet), NULL);
}

void stop(int res) {
	// Finish
	long usecs_total, usecs_packets;

	gettimeofday(&(_global_processor->end), NULL);

	_global_processor->terminated = 1;

	//pthread_join(_global_processor->bufferWorker, NULL);

	switch (res) {
	case 0:
		printf("[Sniffer] Finished scanning file.\n");
		break;
	case -1:
		fprintf(stderr, "[Sniffer] ERROR: Error while sniffing.\n");
		exit(1);
		break;
	case -2:
		printf("[Sniffer] Stopped.");
		break;
	case SIGINT:
		printf("[Sniffer] Interrupted.\n");
		break;
	case SIGTERM:
		printf("[Sniffer] Terminated.\n");
		break;
	case SIGQUIT:
		printf("[Sniffer] Quitting.\n");
		break;
	}

	usecs_total = (_global_processor->end.tv_sec * 1000000 + _global_processor->end.tv_usec) - (_global_processor->start.tv_sec * 1000000 + _global_processor->start.tv_usec);
	if (_global_processor->started)
		usecs_packets = (_global_processor->last_packet.tv_sec * 1000000 + _global_processor->last_packet.tv_usec) - (_global_processor->first_packet.tv_sec * 1000000 + _global_processor->first_packet.tv_usec);
	else
		usecs_packets = 0;
	printf("Total bytes: %ld\n", _global_processor->bytes);
	printf("+---------------- Timing Results ---------------+\n");
	printf("| Cat.  | Total Time (usec) | Throughput (Mbps) |\n");
	printf("+-------+-------------------+-------------------+\n");
	printf("| Gross | %17ld | %14.3f |\n", usecs_total, GET_MBPS(_global_processor->bytes, usecs_total));
	printf("+-------+-------------------+-------------------+\n");
	printf("| Neto  | %17ld | %14.3f |\n", usecs_packets, GET_MBPS(_global_processor->bytes, usecs_packets));
	printf("+-------+-------------------+-------------------+\n");
	printf("\n");
	printf("Total reported matches: %d\n", _global_processor->num_reports);

	destroy_processor(_global_processor);

	exit(0);
}

void sniff(char *in_if, char *out_if, int last) {
	pcap_t *hpcap[2];
	char errbuf[PCAP_ERRBUF_SIZE];
	char *device_in = NULL, *device_out = NULL;
	pcap_if_t *devices, *next_device;
	int res;
	struct bpf_program bpf;
	ProcessorData *processor;
    int linktype[2], linkHdrLen, i;
    char *mode;

	memset(errbuf, 0, PCAP_ERRBUF_SIZE);

	// Find available interfaces
	if (pcap_findalldevs(&devices, errbuf)) {
		fprintf(stderr, "[Sniffer] ERROR: Cannot find network interface (pcap_findalldevs error: %s)\n", errbuf);
		exit(1);
	}

	// Find requested interface
	next_device = devices;
	while (next_device) {
		printf("[Sniffer] Found network interface: %s\n", next_device->name);
		if (strcmp(in_if, next_device->name) == 0) {
			device_in = in_if;
		}
		if (strcmp(out_if, next_device->name) == 0) {
			device_out = out_if;
		}
		next_device = next_device->next;
	}
	pcap_freealldevs(devices);
	if (strcmp(in_if, STR_ANY) == 0) {
		device_in = STR_ANY;
	}

	if (!device_in) {
		fprintf(stderr, "[Sniffer] ERROR: Cannot find input network interface\n");
		exit(1);
	}
	if (!device_out) {
		fprintf(stderr, "[Sniffer] ERROR: Cannot find output network interface\n");
		exit(1);
	}

	printf("[Sniffer] Sniffer is capturing from device: %s\n", device_in);
	printf("[Sniffer] Packets are sent on device: %s\n", device_out);

	for (i = 0; i < 2; i++) {
		mode = (i == 0) ? "input" : "output";
		// Create input PCAP handle
		hpcap[i] = pcap_create(device_in, errbuf);
		if (!hpcap[i]) {
			fprintf(stderr, "[Sniffer] ERROR: Cannot create %s pcap handle (pcap_create error: %s)\n", mode ,errbuf);
			exit(1);
		}

		if (i == 0) {
			// Set promiscuous mode
			pcap_set_promisc(hpcap[0], 1);
		}

		// Activate PCAP
		res = pcap_activate(hpcap[i]);
		switch (res) {
		case 0:
			// Success
			break;
		case PCAP_WARNING_PROMISC_NOTSUP:
			fprintf(stderr, "[Sniffer] WARNING: Promiscuous mode is not supported\n");
			exit(1);
			break;
		case PCAP_WARNING:
			fprintf(stderr, "[Sniffer] WARNING: Unknown (%s)\n", pcap_geterr(hpcap[i]));
			exit(1);
			break;
		case PCAP_ERROR_NO_SUCH_DEVICE:
			fprintf(stderr, "[Sniffer] ERROR: Device not found\n");
			exit(1);
			break;
		case PCAP_ERROR_PERM_DENIED:
			fprintf(stderr, "[Sniffer] ERROR: Permission denied\n");
			exit(1);
			break;
		case PCAP_ERROR_PROMISC_PERM_DENIED:
			fprintf(stderr, "[Sniffer] ERROR: Permission denied for promiscuous mode\n");
			exit(1);
			break;
		case PCAP_ERROR_RFMON_NOTSUP:
			fprintf(stderr, "[Sniffer] ERROR: Monitor mode is not supported\n");
			exit(1);
			break;
		case PCAP_ERROR_IFACE_NOT_UP:
			fprintf(stderr, "[Sniffer] ERROR: Interface %s is not available\n", (i == 0) ? device_in : device_out);
			exit(1);
			break;
		default:
			fprintf(stderr, "[Sniffer] ERROR: Unknown (%s)\n", pcap_geterr(hpcap[i]));
			exit(1);
			break;
		}

		if (i == 0) {
			// Set capture direction (ingress only)
			res = pcap_setdirection(hpcap[0], PCAP_D_IN);
			if (res) {
				fprintf(stderr, "[Sniffer] ERROR: Cannot set capture direction (pcap_setdirection error: %s, return value: %d)\n", pcap_geterr(hpcap[0]), res);
				exit(1);
			}
			// Compile PCAP filter (IP packets)
			res = pcap_compile(hpcap[0], &bpf, STR_FILTER, 0, PCAP_NETMASK_UNKNOWN);
			if (res) {
				fprintf(stderr, "[Sniffer] ERROR: Cannot compile packet filter (pcap_compile error: %s)\n", pcap_geterr(hpcap[0]));
				exit(1);
			}

			res = pcap_setfilter(hpcap[0], &bpf);
			if (res) {
				fprintf(stderr, "[Sniffer] ERROR: Cannot set packet filter (pcap_setfilter error: %s, return value: %d)\n", pcap_geterr(hpcap[0]), res);
				exit(1);
			}
			pcap_freecode(&bpf);
		}

		// Find data link type
		if ((linktype[i] = pcap_datalink(hpcap[i])) < 0)
		{
			fprintf(stderr, "[Sniffer] Cannot determine data link type (pcap_datalink error: %s)\n", pcap_geterr(hpcap[i]));
			exit(1);
		}
		if (i == 1 && linktype[0] != linktype[1]) {
			fprintf(stderr, "[Sniffer] Incompatible link types (input=%d, output=%d)\n", linktype[0], linktype[1]);
			exit(1);
		}
	}

	linkHdrLen = 0;
	switch (linktype[0])
	{
	case DLT_NULL:
		linkHdrLen = 4;
		break;

	case DLT_EN10MB:
		linkHdrLen = 14;
		break;

	case DLT_SLIP:
	case DLT_PPP:
		linkHdrLen = 24;
		break;

	default:
		fprintf(stderr, "[Sniffer] Unsupported data link type: %d\n", linktype[0]);
		exit(1);
	}

	// Prepare processor
	processor = init_processor(hpcap[0], hpcap[1], linkHdrLen, last);
	_global_processor = processor;

	// Set signal handler
	signal(SIGINT, stop);
	signal(SIGTERM, stop);
	signal(SIGQUIT, stop);

	// Run buffer cleaning worker
//	pthread_create(&(processor->bufferWorker), NULL, process_buffer_timeout, (void*)processor);

	// Run sniffer
	gettimeofday(&(processor->start), NULL);
	printf("[Sniffer] Sniffer is running (input: %s, outout: %s)...\n", in_if, out_if);
	res = pcap_loop(hpcap[0], -1, process_packet, (unsigned char *)(processor));

	stop(res);
}

int main(int argc, char *argv[]) {
	char *in_if = NULL;
	char *out_if = NULL;
	int i;
	char *param, *arg;
	int auto_mode, last;

	auto_mode = 0;

	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			param = strsep(&argv[i], ":");
			arg = argv[i];
			if (strcmp(param, "in") == 0) {
				in_if = arg;
			} else if (strcmp(param, "out") == 0) {
				out_if = arg;
			} else if (strcmp(param, "last") == 0) {
				last = 1;
			} else if (strcmp(param, "auto") == 0) {
				auto_mode = 1;
				break;
			}
		}
	}
	if (auto_mode == 0 && (in_if == NULL || out_if == NULL)) {
		// Show usage
		fprintf(stderr, "Usage: %s in:<input-interface> out:<output-interface> [last]\nThis tool may require root privileges.\n", argv[0]);
		exit(1);
	} else if (auto_mode == 1) {
		// Set defaults
		in_if = "mbox1-eth0";
		out_if = "mbox1-eth0";
		last = 1;
	}


	sniff(in_if, out_if, last);

	return 0;
}
