/*
 * DpiSrv.c
 *
 *  Created on: Oct 5, 2014
 *      Author: yotamhc
 */

#include <stdlib.h>

#define MAX_REPORTS 10
#define IP_OPT_TYPE_EXP 94 // See RFC 3692

typedef struct {
	unsigned short rid;
	unsigned char is_range : 1;
	unsigned short position : 15;
} MatchReport;

typedef struct {
	unsigned short rid;
	unsigned char is_range : 1;
	unsigned short position : 15;
	unsigned short length;
} MatchReportRange;

int mpseSearchDpiSrv( Packet *p, ACSM_STRUCT2 * acsm, unsigned char *Tx, int n,
        int (*Match)(void * id, void *tree, int index, void *data, void *neg_list),
        void *data, int* current_state ) {

	MatchReport *report;
	MatchReportRange *rangeReport;
	Options *opt;
	int num_reports;
	int i, j, count;
	unsigned int pos;
	ACSM_PATTERN2 **MatchList = acsm->acsmMatchList;
	ACSM_PATTERN2 *mlist;

	// Verify option is of the correct type
	opt = &(p->ip_options[0]);
	if (opt->code != IP_OPT_TYPE_EXP)
		return 0;

	num_reports = (opt->len - 1) / sizeof(MatchReport);
	report = (MatchReport*)(opt->data + 1);
	count = 0;

	for (i = 0; i < num_reports; i++) {
		if (report->is_range) {
			rangeReport = (MatchReportRange)report;
			mlist = MatchList[rangeReport->rid];
			for (j = 0; j < rangeReport->length; j++) {
				pos = rangeReport->position + j;
				count++;
				if (Match (mlist->udata, mlist->rule_option_tree, pos, data, mlist->neg_list) > 0) {
					return count;
				}
			}
			report = (MatchReport*)(rangeReport + 1);
		} else {
			mlist = MatchList[report->rid];
			count++;
			if (Match (mlist->udata, mlist->rule_option_tree, report->position, data, mlist->neg_list) > 0) {
				return count;
			}
			report++;
		}
	}
	return count;
/*
	// Code from acsmx2:
	ACSM_PATTERN2 *mlist;
	unsigned char *Tend;
	unsigned char *T;
	int index;
	int sindex;
	int nfound = 0;
	acstate_t state;
	ACSM_PATTERN2 **MatchList = acsm->acsmMatchList;

	T = Tx;
	Tend = Tx + n;

	if (current_state == NULL)
		return 0;

	state = *current_state;

	switch (acsm->sizeofstate)
	{
		case 1:
			{
				uint8_t *ps;
				uint8_t **NextState = (uint8_t **)acsm->acsmNextState;
				for( ; T < Tend; T++ ) \
				{ \
					ps = NextState[ state ]; \
					sindex = xlatcase[T[0]]; \
					if (ps[1]) \
					{ \
						mlist = MatchList[state]; \
						if (mlist) \
						{ \
							index = T - mlist->n - Tx; \
							nfound++; \
							if (Match (mlist->udata, mlist->rule_option_tree, index, data, mlist->neg_list) > 0) \
							{ \
								*current_state = state; \
								return nfound; \
							} \
						} \
					} \
					state = ps[2u + sindex]; \
				};
			}
			break;
		case 2:
			{
				uint16_t *ps;
				uint16_t **NextState = (uint16_t **)acsm->acsmNextState;
				for( ; T < Tend; T++ ) \
				{ \
					ps = NextState[ state ]; \
					sindex = xlatcase[T[0]]; \
					if (ps[1]) \
					{ \
						mlist = MatchList[state]; \
						if (mlist) \
						{ \
							index = T - mlist->n - Tx; \
							nfound++; \
							if (Match (mlist->udata, mlist->rule_option_tree, index, data, mlist->neg_list) > 0) \
							{ \
								*current_state = state; \
								return nfound; \
							} \
						} \
					} \
					state = ps[2u + sindex]; \
				};
			}
			break;
		default:
			{
				acstate_t *ps;
				acstate_t **NextState = acsm->acsmNextState;
				for( ; T < Tend; T++ ) \
				{ \
					ps = NextState[ state ]; \
					sindex = xlatcase[T[0]]; \
					if (ps[1]) \
					{ \
						mlist = MatchList[state]; \
						if (mlist) \
						{ \
							index = T - mlist->n - Tx; \
							nfound++; \
							if (Match (mlist->udata, mlist->rule_option_tree, index, data, mlist->neg_list) > 0) \
							{ \
								*current_state = state; \
								return nfound; \
							} \
						} \
					} \
					state = ps[2u + sindex]; \
				};
			}
			break;
	}
*/
	/* Check the last state for a pattern match */
/*
	mlist = MatchList[state];
	if (mlist)
	{
		index = T - mlist->n - Tx;
		nfound++;
		if (Match(mlist->udata, mlist->rule_option_tree, index, data, mlist->neg_list) > 0)
		{
			*current_state = state;
			return nfound;
		}
	}

	*current_state = state;
	return nfound;
*/
}

