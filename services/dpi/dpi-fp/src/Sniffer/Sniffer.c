#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#ifdef __linux__
#include <sched.h>
#endif
#include <pthread.h>
#include "Sniffer.h"
#include "../StateMachine/TableStateMachine.h"
#include "../StateMachine/TableStateMachineGenerator.h"
#include "../Common/Types.h"
#include "../Common/PacketBuffer.h"

#define MAX_PACKET_SIZE 65535
#define MAX_REPORTED_RULES 1024
#define STR_ANY "any"
#define STR_FILTER "ip"
#define MAGIC_NUM 0xDEE4
#define MAX_THREADS 8
#define MAX_REPORTS_PER_PACKET 350

#define USAGE "Usage: %s (in=<iface>|infile=<file>) (out=<iface>|outfile=<file>) rules=<file> [max=<#>] [workers=<#>] [noreport] [batch]\n\tin=<iface>\tSet input capture interface\n\tout=<iface>\tSet output interface\n\tinfile=<file>\tSet input pcap file (cannot use with 'in')\n\toutfile=<file>\tSet output pcap file (cannot use with 'out', not implemented yet)\n\trules=<file>\tSet rules file\n\tmax=<#>\t\tMaximal number of rules to use from file\n\tworkers=<#>\tSet number of workers (default: 1)\n\tnoreport\tDo not send report packets. Handle report internally.\n\tbatch\t\tReport results in batch mode\n\nThis tool may require root privileges.\n"

#define GET_MBPS(bytes, usecs) \
	((bytes) * 8.0 * 1000000) / ((usecs) * 1024 * 1024)

static struct timespec _100_nanos = {0, 100};

struct st_processor_data;

void *worker_start(void *);

typedef struct {
	int id;
	struct st_processor_data *processor;
	PacketBuffer *queue;
#ifdef __linux__
	cpu_set_t cpuset;
	pthread_attr_t attr;
#endif
} WorkerData;

typedef struct st_processor_data {
	int counter;
	TableStateMachine *machine;
	int linkHdrLen;
	pcap_t *pcap_in;
	pcap_t *pcap_out;
	struct timeval start, end;
	struct timeval first_packet[MAX_THREADS], last_packet[MAX_THREADS];
	int started[MAX_THREADS];
	long bytes[MAX_THREADS];
	// For Standalone middlebox mode that does not report its matches
	int no_report;
	long total_reports[MAX_THREADS];
	int terminated;
	pthread_t workers[MAX_THREADS];
	PacketBuffer queues[MAX_THREADS];
	WorkerData workerData[MAX_THREADS];
	int num_workers;
	int next_queue;
	int batch_mode;
} ProcessorData;

typedef struct {
	unsigned short magicnum;
	unsigned short numReports;
	unsigned int seqNum;
	unsigned int flowOffset;
} ResultsPacketHeader;

typedef struct {
	unsigned short rid;
	short idx;
} ResultPacketReport;

static ProcessorData *_global_processor;

ProcessorData *init_processor(TableStateMachine *machine, pcap_t *pcap_in, pcap_t *pcap_out, int linkHdrLen, int num_workers, int no_report, int batch) {
	int i;
	ProcessorData *processor;

	processor = (ProcessorData*)malloc(sizeof(ProcessorData));

	processor->counter = 0;
	processor->machine = machine;
	processor->pcap_in = pcap_in;
	processor->pcap_out = pcap_out;
	processor->linkHdrLen = linkHdrLen;
	processor->no_report = no_report;
	processor->terminated = 0;
	processor->next_queue = 0;
	processor->batch_mode = batch;

	processor->num_workers = num_workers;
	for (i = 0; i < num_workers; i++) {
		packet_buffer_init(&(processor->queues[i]));
		processor->started[i] = 0;
		processor->bytes[i] = 0;
		processor->total_reports[i] = 0;
		processor->workerData[i].id = i;
		processor->workerData[i].processor = processor;
		processor->workerData[i].queue = &(processor->queues[i]);
#ifdef __linux__
		CPU_ZERO(&(processor->workerData[i].cpuset));
		CPU_SET(i, &(processor->workerData[i].cpuset));
		pthread_attr_init(&(processor->workerData[i].attr));
		pthread_attr_setaffinity_np(&(processor->workerData[i].attr), sizeof(cpu_set_t), &(processor->workerData[i].cpuset));
		pthread_attr_setscope(&(processor->workerData[i].attr), PTHREAD_SCOPE_SYSTEM);
		pthread_create(&(processor->workers[i]), &(processor->workerData[i].attr), worker_start, &(processor->workerData[i]));
#else
		pthread_create(&(processor->workers[i]), NULL, worker_start, &(processor->workerData[i]));
#endif
	}

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

static inline int find_results(ProcessorData *processor, MatchReport *reports, int num_reports, ResultPacketReport *rules) {
	int i,j, r, num_rules;
	MatchRule *state_rules;

	r = 0;
	for (i = 0; i < num_reports; i++) {
		state_rules = processor->machine->matchRules[reports[i].state];
		num_rules = processor->machine->numRules[reports[i].state];
		for (j = 0; j < num_rules; j++) {
			rules[r].rid = state_rules[j].rid;
			rules[r].idx = reports[i].position - state_rules[j].len;
			r++;
		}
	}
	return r;
}

static inline int build_result_packet(ProcessorData *processor, const struct pcap_pkthdr *pkthdr, const unsigned char *packetptr,
		Packet *in_packet, MatchReport *reports, int num_reports, unsigned char *result) {
	int hdrs_len, data_len;
	ResultPacketReport rules[MAX_REPORTED_RULES];
	int r;
	ResultsPacketHeader *reshdr;

    struct ip *iphdr = (struct ip*)result;
    struct udphdr *udphdr;

	if (num_reports == 0) {
		return 0;
	}

	// Find results
	r = find_results(processor, reports, num_reports, rules);

	// TODO: Break into several packets
	if (r > MAX_REPORTS_PER_PACKET) {
		r = MAX_REPORTS_PER_PACKET;
	}

	// Compute data length
	data_len = 12 + (r * 4); // 12 = result packet header (e.g. magic num).

	// Copy L2 headers
	hdrs_len = pkthdr->len - in_packet->ip_len;
	memcpy(result, packetptr, hdrs_len);

	// Build IP header
	iphdr = (struct ip*)&(result[hdrs_len]);
	iphdr->ip_v = 4;
	iphdr->ip_hl = 5;
	iphdr->ip_tos = in_packet->ip_tos;
	iphdr->ip_len = htons(28 + data_len);
	iphdr->ip_id = 0;
	iphdr->ip_off = 0;
	iphdr->ip_ttl = 64;
	iphdr->ip_p = IPPROTO_UDP;
	iphdr->ip_sum = 0;
	iphdr->ip_src.s_addr = in_packet->ip_src;
	iphdr->ip_dst.s_addr = in_packet->ip_dst;

	// Build UDP header
	udphdr = (struct udphdr*)&(result[hdrs_len + 20]);
#ifdef __APPLE__
	udphdr->uh_dport = in_packet->transport.tp_dst;
	udphdr->uh_sport = in_packet->transport.tp_src;
	udphdr->uh_sum = 0;
	udphdr->uh_ulen = 8 + data_len;
#elif __linux__
	udphdr->dest = in_packet->transport.tp_dst;
	udphdr->source = in_packet->transport.tp_src;
	udphdr->check = 0;
	udphdr->len = htons(8 + data_len);
#endif
	// Build results header
	reshdr = (ResultsPacketHeader*)&(result[hdrs_len + 28]);
	reshdr->magicnum = htons(MAGIC_NUM);
	reshdr->numReports = htons(r);
	reshdr->flowOffset = 0;
	reshdr->seqNum = htonl(in_packet->seqnum);

	// Write reports to packet
	memcpy(&(result[hdrs_len + 40]), rules, sizeof(ResultPacketReport) * r);

	return hdrs_len + 40 + (r * sizeof(ResultPacketReport));
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

static inline int count_results_for_noreport_mode(ProcessorData *processor, MatchReport *reports, int num_reports) {
	int r;
	ResultPacketReport rules[MAX_REPORTED_RULES];

	r = find_results(processor, reports, num_reports, rules);
	return r;
}

void *worker_start(void *param) {
	WorkerData *workerData;
	ProcessorData *processor;
	PacketBuffer *queue;
	InPacket *pkt;
	int current;
	int res, r;
	Packet *packet;
	unsigned char *ptr;
	MatchReport reports[MAX_REPORTS];
	unsigned char data[MAX_PACKET_SIZE];
	int size, id;

	workerData = (WorkerData*)param;
	processor = workerData->processor;
	queue = workerData->queue;
	id = workerData->id;
	current = 0;

	pkt = NULL;

	while ((pkt = packet_buffer_dequeue(queue)) || !processor->terminated) {
		if (!pkt) {
			nanosleep(&_100_nanos, NULL);
			continue;
		}

		if (!processor->started[id]) {
			processor->started[id] = 1;
			gettimeofday(&(processor->first_packet[id]), NULL);
		}

		packet = &(pkt->packet);

		// Scan payload
		// TODO: Per-flow scan (remember current state for each flow)
		MATCH_TABLE_MACHINE(processor->machine, current, packet->payload, packet->payload_len, reports, res);

		processor->bytes[id] += packet->payload_len;

		if (processor->no_report) {
			// Count reports
			r = count_results_for_noreport_mode(processor, reports, res);
			processor->total_reports[id] += r;
			// Forward packet
			pcap_sendpacket(processor->pcap_out, pkt->pktdata, pkt->pkthdr.len);
		} else {
			// Send original packet
			if (!res) {
				// No matches - send as is
				pcap_sendpacket(processor->pcap_out, pkt->pktdata, pkt->pkthdr.len);
			} else {
				// Matches exist - change ECN to 11b and send
				ptr = (unsigned char *)(pkt->pktdata);
				ptr[processor->linkHdrLen + 1] = ptr[processor->linkHdrLen + 1] | 0xC0;
				pcap_sendpacket(processor->pcap_out, ptr, pkt->pkthdr.len);

				// Build results packet
				size = build_result_packet(processor, &(pkt->pkthdr), pkt->pktdata, packet, reports, res, data);
#ifdef VERBOSE
				printf("Matches: %d, Input packet length: %u, Result packet length: %d, seqnum/checksum: %u\n", res, pkt->pkthdr.len, size, packet->seqnum);
#endif
				// Send results packet
				if (size) {
					pcap_sendpacket(processor->pcap_out, data, size);
				}
			}
		}

		free_buffered_packet(pkt);
		gettimeofday(&(processor->last_packet[id]), NULL);
	}

	return NULL;
}

void process_packet(unsigned char *arg, const struct pcap_pkthdr *pkthdr, const unsigned char *packetptr) {
	ProcessorData *processor;
	Packet packet;
	InPacket *bpkt;

	processor = (ProcessorData*)arg;

	parse_packet(processor, packetptr, &packet);

	bpkt = buffer_packet(&packet, pkthdr, packetptr, 0);
	packet_buffer_enqueue(&(processor->queues[processor->next_queue]), bpkt);

	processor->next_queue = (processor->next_queue + 1) % (processor->num_workers);
}

void stop(int res) {
	// Finish
	long usecs_packets[MAX_THREADS];
	long total_bytes, thread_bytes[MAX_THREADS];
	int i;
	double throughput[MAX_THREADS];
	double total_throughput;
	long total_reports;

	_global_processor->terminated = 1;

	for (i = 0; i < _global_processor->num_workers; i++) {
		pthread_join(_global_processor->workers[i], NULL);
	}

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

	total_bytes = 0;
	total_throughput = 0;
	total_reports = 0;
	for (i = 0; i < _global_processor->num_workers; i++) {
		if (_global_processor->started[i]) {
			total_bytes += _global_processor->bytes[i];
			thread_bytes[i] = _global_processor->bytes[i];
			usecs_packets[i] = (_global_processor->last_packet[i].tv_sec * 1000000 + _global_processor->last_packet[i].tv_usec) - (_global_processor->first_packet[i].tv_sec * 1000000 + _global_processor->first_packet[i].tv_usec);
			throughput[i] = GET_MBPS(thread_bytes[i], usecs_packets[i]);
			total_throughput += throughput[i];
			total_reports += _global_processor[i].total_reports[i];
		} else {
			usecs_packets[i] = 0;
			thread_bytes[i] = 0;
			throughput[i] = 0;
		}
	}

	if (!_global_processor->batch_mode) {
		if (!(_global_processor->no_report)) {
			printf("+--------------------------- Timing Results --------------------------+\n");
			printf("| Thrd. | Total Time (usec) | Total Bytes (bytes) | Throughput (Mbps) |\n");
			printf("+-------+-------------------+---------------------+-------------------+\n");
			for (i = 0; i < _global_processor->num_workers; i++) {
				printf("| %5d | %17ld | %19ld | %17.3f |\n", i, usecs_packets[i], thread_bytes[i], throughput[i]);
			}
			printf("+-------+-------------------+---------------------+-------------------+\n");
			printf("| Total |         -         | %19ld | %17.3f |\n", total_bytes, total_throughput);
			printf("+-------+-------------------+---------------------+-------------------+\n");
		} else {
			printf("+----------------------------------- Timing Results ------------------------------------+\n");
			printf("| Thrd. | Total Time (usec) | Total Bytes (bytes) | Throughput (Mbps) |     Reports     |\n");
			printf("+-------+-------------------+---------------------+-------------------+-----------------+\n");
			for (i = 0; i < _global_processor->num_workers; i++) {
				printf("| %5d | %17ld | %19ld | %17.3f | %15ld |\n", i, usecs_packets[i], thread_bytes[i], throughput[i], _global_processor->total_reports[i]);
			}
			printf("+-------+-------------------+---------------------+-------------------+-----------------+\n");
			printf("| Total |         -         | %19ld | %17.3f | %15ld |\n", total_bytes, total_throughput, total_reports);
			printf("+-------+-------------------+---------------------+-------------------+-----------------+\n");
		}
	} else {
		printf("Batch Mode Results Report\n");
		printf("=========================\n");
		printf("(use with grep)\n\n");
		printf("   \tTID\tpat#\tusecs\tTotalBytes\tThpt(Mbps)\t%s\n", (_global_processor->no_report) ? "Reports" : "");

		for (i = 0; i < _global_processor->num_workers; i++) {
			if (!(_global_processor->no_report)) {
				printf("RES\tT%d\t%d\t%ld\t%ld\t%f\n", i, _global_processor->machine->total_rules, usecs_packets[i], thread_bytes[i], throughput[i]);
			} else {
				printf("RES\tT%d\t%d\t%ld\t%ld\t%f\t%ld\n", i, _global_processor->machine->total_rules, usecs_packets[i], thread_bytes[i], throughput[i], _global_processor->total_reports[i]);
			}
		}
		if (!(_global_processor->no_report)) {
			printf("RES\tTOT\t%d\t------\t%ld\t%f\n", _global_processor->machine->total_rules, total_bytes, total_throughput);
		} else {
			printf("RES\tTOT\t%d\t------\t%ld\t%f\t%ld\n", _global_processor->machine->total_rules, total_bytes, total_throughput, total_reports);
		}
	}

	if (_global_processor->pcap_in) {
		pcap_close(_global_processor->pcap_in);
	}
	if (_global_processor->pcap_out) {
		pcap_close(_global_processor->pcap_out);
	}

	destroy_processor(_global_processor);

	exit(0);
}


void sniff(char *in_if, char *out_if, char *in_file, char *out_file, TableStateMachine *machine, int num_workers, int no_report, int batch) {
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

	if (in_if || out_if) {
		// Find available interfaces
		if (pcap_findalldevs(&devices, errbuf)) {
			fprintf(stderr, "[Sniffer] ERROR: Cannot find network interface (pcap_findalldevs error: %s)\n", errbuf);
			exit(1);
		}

		// Find requested interface
		next_device = devices;
		while (next_device) {
			printf("[Sniffer] Found network interface: %s\n", next_device->name);
			if (in_if && (strcmp(in_if, next_device->name) == 0)) {
				device_in = in_if;
			}
			if (out_if && (strcmp(out_if, next_device->name) == 0)) {
				device_out = out_if;
			}
			next_device = next_device->next;
		}
		pcap_freealldevs(devices);
		if (in_if && (strcmp(in_if, STR_ANY) == 0)) {
			device_in = STR_ANY;
		}

		if (in_if && !device_in) {
			fprintf(stderr, "[Sniffer] ERROR: Cannot find input network interface\n");
			exit(1);
		}
		if (out_if && !device_out) {
			fprintf(stderr, "[Sniffer] ERROR: Cannot find output network interface\n");
			exit(1);
		}
	}

	if (in_if) {
		printf("[Sniffer] Sniffer is capturing from device: %s\n", device_in);
	} else {
		printf("[Sniffer] Sniffer is reading packets from file: %s\n", in_file);
	}
	if (out_if) {
		printf("[Sniffer] Packets are sent on device: %s\n", device_out);
	} else {
		printf("[Sniffer] Packets written to file: %s\n", out_file);
	}

	if (in_if) {
		hpcap[0] = pcap_create(device_in, errbuf);
	} else {
		hpcap[0] = pcap_open_offline(in_file, errbuf);
	}
	if (out_if) {
		hpcap[1] = pcap_create(device_out, errbuf);
	} else {
		//hpcap[1] = pcap_dump_fopen(out_file, errbuf);
	}

	for (i = 0; i < 2; i++) {
		mode = (i == 0) ? "input" : "output";
		// Check pcap handle
		if (!hpcap[i]) {
			fprintf(stderr, "[Sniffer] ERROR: Cannot create %s pcap handle (pcap_create/pcap_open_offline error: %s)\n", mode ,errbuf);
			exit(1);
		}

		if (in_if && i == 0) {
			// Set promiscuous mode
			pcap_set_promisc(hpcap[0], 1);
		}

		// Activate PCAP
		if ((in_if && i == 0) || (out_if && i == 1)) {
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
		}

		if (i == 0) {
			if (in_if) {
				// Set capture direction (ingress only)
				res = pcap_setdirection(hpcap[0], PCAP_D_IN);
				if (res) {
					fprintf(stderr, "[Sniffer] ERROR: Cannot set capture direction (pcap_setdirection error: %s, return value: %d)\n", pcap_geterr(hpcap[0]), res);
					exit(1);
				}
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
	processor = init_processor(machine, hpcap[0], hpcap[1], linkHdrLen, num_workers, no_report, batch);
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
	char *in_file = NULL;
	char *out_file = NULL;
	TableStateMachine *machine;
	char *patterns = NULL;
	int i;
	char *param, *arg;
	int auto_mode, no_report, batch, max_rules;
	int num_workers;


	// ************* BEGIN DEBUG
	//struct pcap_pkthdr pkthdr;
	//char pkt[2048];
	//ProcessorData *processor;
	// ************* END


	auto_mode = 0;
	no_report = 0;
	num_workers = 1;
	batch = 0;
	max_rules = 0;

	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			param = strsep(&argv[i], "=");
			arg = argv[i];
			if (strcmp(param, "in") == 0) {
				in_if = arg;
			} else if (strcmp(param, "out") == 0) {
				out_if = arg;
			} else if (strcmp(param, "infile") == 0) {
				in_file = arg;
			} else if (strcmp(param, "outfile") == 0) {
				out_file = arg;
			} else if (strcmp(param, "rules") == 0) {
				patterns = arg;
			} else if (strcmp(param, "max") == 0) {
				max_rules = atoi(arg);
			} else if (strcmp(param, "workers") == 0) {
				num_workers = atoi(arg);
			} else if (strcmp(param, "noreport") == 0) {
				no_report = 1;
			} else if (strcmp(param, "batch") == 0) {
				batch = 1;
			} else if (strcmp(param, "auto") == 0) {
				auto_mode = 1;
				break;
			}
		}
	}

	if (auto_mode == 0 && ((in_if == NULL && in_file == NULL) || (out_if == NULL && out_file == NULL) || patterns == NULL || max_rules < 0 || num_workers < 1)) {
		// Show usage
		fprintf(stderr, USAGE, argv[0]);
		exit(1);
	} else if (auto_mode == 1) {
		// Set defaults
		in_if = "eth0:1";
		out_if = "eth0:2";
		patterns = "../../SnortPatternsFull2.json";
		max_rules = 0;
		num_workers = 1;
	}

	machine = generateTableStateMachine(patterns, max_rules, 0);

	// ************* BEGIN DEBUG
	//void process_packet(unsigned char *arg, const struct pcap_pkthdr *pkthdr, const unsigned char *packetptr)
	//processor = init_processor(machine, NULL, NULL, 0);
	//strcpy(pkt,"008g");
	//process_packet((unsigned char *)processor, &pkthdr, (unsigned char*)pkt);
	// ************* END


	sniff(in_if, out_if, in_file, out_file, machine, num_workers, no_report, batch);

	return 0;
}
