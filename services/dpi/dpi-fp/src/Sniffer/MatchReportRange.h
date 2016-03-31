#ifndef SNIFFER_MATCHREPORTRANGE_H_
#define SNIFFER_MATCHREPORTRANGE_H_

typedef struct {
	uint16_t rid;
	uint8_t  is_range;
	uint16_t position : 15;
	uint16_t length;
} MatchReportRange;

#endif /* SNIFFER_MATCHREPORTRANGE_H_ */
