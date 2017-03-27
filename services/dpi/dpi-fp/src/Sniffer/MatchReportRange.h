#ifndef SNIFFER_MATCHREPORTRANGE_H_
#define SNIFFER_MATCHREPORTRANGE_H_

#include "RuleId.h"

typedef struct {
	rule_id_t rid;
	uint8_t  is_range;
	uint16_t position : 15;
	uint16_t length;
} MatchReportRange;

#endif /* SNIFFER_MATCHREPORTRANGE_H_ */
