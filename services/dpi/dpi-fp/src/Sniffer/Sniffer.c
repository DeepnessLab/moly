#include <pcap.h>
#include <pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
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
#include "Sniffer.h"
#include "../StateMachine/TableStateMachine.h"
#include "../StateMachine/TableStateMachineGenerator.h"

#define MAX_PACKET_SIZE 65535
#define MAX_REPORTED_RULES 65535
#define MAX_REPORTS 65535
#define STR_ANY "any"
#define STR_FILTER "ip"
#define MAGIC_NUM 0xDEE4

typedef struct {
	int counter;
	TableStateMachine *machine;
	int linkHdrLen;
	pcap_t *pcap_in;
	pcap_t *pcap_out;
	struct timeval start, end;
	long bytes;
} ProcessorData;

typedef struct {
	// IPv4
	in_addr_t ip_src;
	in_addr_t ip_dst;
	unsigned short ip_id;
	unsigned char ip_tos;
	unsigned char ip_ttl;
	unsigned char ip_proto;

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

	// Data
	unsigned int payload_len;
	unsigned char *payload;
} Packet;

static ProcessorData *_global_processor;

ProcessorData *init_processor(TableStateMachine *machine, pcap_t *pcap_in, pcap_t *pcap_out, int linkHdrLen) {
	ProcessorData *processor;

	processor = (ProcessorData*)malloc(sizeof(ProcessorData));

	processor->counter = 0;
	processor->machine = machine;
	processor->pcap_in = pcap_in;
	processor->pcap_out = pcap_out;
	processor->linkHdrLen = linkHdrLen;
	processor->bytes = 0;

	return processor;
}

void destroy_processor(ProcessorData *processor) {
	free(processor);
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
    packetptr += 4*iphdr->ip_hl;
    switch (iphdr->ip_p)
    {
    case IPPROTO_TCP:
        tcphdr = (struct tcphdr*)packetptr;
#ifdef __APPLE__
        transport_len = 4 * tcphdr->th_off;
        packet->transport.tp_src = tcphdr->th_sport;
        packet->transport.tp_dst = tcphdr->th_dport;
#elif __linux__
        transport_len = 4 * tcphdr->doff;
        packet->transport.tp_src = tcphdr->source;
        packet->transport.tp_dst = tcphdr->dest;
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
#elif __linux__
        packet->transport.tp_src = udphdr->source;
        packet->transport.tp_dst = udphdr->dest;
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

static inline int build_result_packet(ProcessorData *processor, const struct pcap_pkthdr *pkthdr, const unsigned char *packetptr,
		Packet *in_packet, MatchReport *reports, int num_reports, unsigned char *result) {
	int hdrs_len;
	unsigned char *ptr;
	MatchRule rules[MAX_REPORTED_RULES];
	int start_idx_in_pkt[MAX_REPORTED_RULES];
	MatchRule *state_rules;
	int i, r;


	if (num_reports == 0) {
		memcpy(result, packetptr, pkthdr->len);
		return pkthdr->len;
	}

	// Copy headers
	hdrs_len = pkthdr->len - in_packet->payload_len;
	memcpy(result, packetptr, hdrs_len);

	// Find results
	r = 0;
	for (i = 0; i < num_reports; i++) {
		state_rules = processor->machine->matchRules[reports[i].state];
		if (state_rules) {
			memcpy(&(rules[r]), state_rules, sizeof(MatchRule) * processor->machine->numRules[reports[i].state]);
			start_idx_in_pkt[r] = reports[i].position - state_rules[0].len;
			r++;
		}
	}

	// Put results before L7 payload
	ptr = &(result[hdrs_len]);
	*((unsigned short*)ptr) = htons(MAGIC_NUM);
	ptr += 2;
	*((unsigned short*)ptr) = htons(r);
	ptr += 2;

	for (i = 0; i < r; i++) {
		*((unsigned int*)ptr) = htonl(rules[i].rid);
		ptr += 4;
		*((unsigned int*)ptr) = htonl(start_idx_in_pkt[i]);
		ptr += 4;
		*((unsigned int*)ptr) = 0;
		ptr += 4;
	}

	// Put L7 payload
	memcpy(ptr, in_packet->payload, in_packet->payload_len);

	return (int)(ptr - result);
}

void process_packet(unsigned char *arg, const struct pcap_pkthdr *pkthdr, const unsigned char *packetptr) {
	ProcessorData *processor;
	int current;
	int res;
	Packet packet;
	MatchReport reports[MAX_REPORTS];
	unsigned char data[MAX_PACKET_SIZE];
	int size;

	processor = (ProcessorData*)arg;
	current = 0;

	parse_packet(processor, packetptr, &packet);
	//printf("[Sniffer] Packet src_ip=%d,dst_ip=%d,ip_tos=%d,ip_proto=%d,src_port=%d,dst_port=%d,data=\"%s\"\n",
	//		packet.ip_src, packet.ip_dst, packet.ip_tos, packet.ip_proto, packet.transport.tp_src, packet.transport.tp_dst,
	//		(char*)(packet.payload));

	// Scan payload
	// TODO: Per-flow scan (remember current state for each flow)
	MATCH_TABLE_MACHINE(processor->machine, current, packet.payload, packet.payload_len, reports, res);

	processor->bytes += packet.payload_len;

	// Build outgoing packet
	size = build_result_packet(processor, pkthdr, packetptr, &packet, reports, res, data);

	// TODO: Send outgoing packet
	pcap_sendpacket(processor->pcap_out, data, size);
}

void stop(int res) {
	// Finish
	long usecs;

	gettimeofday(&(_global_processor->end), NULL);

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

	destroyTableStateMachine(_global_processor->machine);

	usecs = (_global_processor->end.tv_sec * 1000000 + _global_processor->end.tv_usec) - (_global_processor->start.tv_sec * 1000000 + _global_processor->start.tv_usec);
	printf("Total bytes: %ld\n", _global_processor->bytes);
	printf("Total time: %ld usec.\n", usecs);
	printf("Total throughput: %4.3f Mbps\n", (_global_processor->bytes * 8.0 * 1000000) / (usecs * 1024 * 1024));

	destroy_processor(_global_processor);

	exit(0);
}

void sniff(char *in_if, char *out_if, TableStateMachine *machine) {
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
	processor = init_processor(machine, hpcap[0], hpcap[1], linkHdrLen);
	_global_processor = processor;

	// Set signal handler
	signal(SIGINT, stop);
	signal(SIGTERM, stop);
	signal(SIGQUIT, stop);

	// Run sniffer
	gettimeofday(&(processor->start), NULL);
	printf("[Sniffer] Sniffer is running (input: %s, outout: %s)...\n", in_if, out_if);
	res = pcap_loop(hpcap[0], -1, process_packet, (unsigned char *)(processor));

	stop(res);
}

int main(int argc, char *argv[]) {
	char *in_if = NULL;
	char *out_if = NULL;
	TableStateMachine *machine;
	char *patterns = NULL;
	int i;
	char *param, *arg;
	int auto_mode;

	auto_mode = 0;

	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			param = strsep(&argv[i], ":");
			arg = argv[i];
			if (strcmp(param, "in") == 0) {
				in_if = arg;
			} else if (strcmp(param, "out") == 0) {
				out_if = arg;
			} else if (strcmp(param, "rules") == 0) {
				patterns = arg;
			} else if (strcmp(param, "auto") == 0) {
				auto_mode = 1;
				break;
			}
		}
	}
	if (auto_mode == 0 && (in_if == NULL || out_if == NULL || patterns == NULL)) {
		// Show usage
		fprintf(stderr, "Usage: %s in:<input-interface> out:<output-interface> rules:<rules file>\nThis tool may require root privileges.\n", argv[0]);
		exit(1);
	} else if (auto_mode == 1) {
		// Set defaults
		in_if = "en0";
		out_if = "en0";
		patterns = "SnortPatternsFull2.json";
	}

	machine = generateTableStateMachine(patterns, 0);

	sniff(in_if, out_if, machine);

	return 0;
}
