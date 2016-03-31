#ifndef SNIFFER_MATCHREPORT_H_
#define SNIFFER_MATCHREPORT_H_

typedef struct {
	uint16_t rid;
	uint8_t is_range;
	uint16_t position : 15;
} MatchReport;

#endif /* SNIFFER_MATCHREPORT_H_ */
