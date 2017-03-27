/*
 * TableStateMachine.h
 *
 *  Created on: Jan 23, 2011
 *      Author: yotamhc
 */

#ifndef TABLESTATEMACHINE_H_
#define TABLESTATEMACHINE_H_
#include "../Common/Types.h"
#include "../Common/BitArray/BitArray.h"
#include "../Common/MatchRule.h"
#include "../Sniffer/ContentMatchReport.h"

#define MAX_REPORTS 1024

typedef struct {
	STATE_PTR_TYPE_WIDE *table;
	unsigned char *matches;
	//char **patterns;
	MatchRule **matchRules; // A pointer to an array of per-state array of match rules
	int *numRules; // An array of per state number of rules
	unsigned int numStates;
	int total_rules;
#ifdef DEPTHMAP
	int *depthMap;
#endif
} TableStateMachine;

TableStateMachine *createTableStateMachine(unsigned int numStates, int totalRules);
void destroyTableStateMachine(TableStateMachine *machine);

void setGoto(TableStateMachine *machine, STATE_PTR_TYPE_WIDE currentState, char c, STATE_PTR_TYPE_WIDE nextState);
void setMatch(TableStateMachine *machine, STATE_PTR_TYPE_WIDE state, MatchRule *rules, int numRules);

int matchTableMachine(TableStateMachine *tableMachine, char *input, int length, int verbose);

#define GET_TABLE_IDX(state, c) \
	(((state) * 256) + (unsigned char)(c))

#define GET_NEXT_STATE(table, state, c) \
	((table)[GET_TABLE_IDX(state, c)])

// Params:
//   TableStateMachine *machine, STATE_PTR_TYPE_WIDE current, char *input, int length, MatchReport *reports, int res
#define MATCH_TABLE_MACHINE(machine, current, input, length, reports, res) \
{ 																				\
	STATE_PTR_TYPE_WIDE next;													\
	STATE_PTR_TYPE_WIDE *table;													\
	unsigned char *matches;														\
	int idx;																	\
																				\
	res = 0;																	\
	table = (machine)->table;													\
	matches = (machine)->matches;												\
	idx = 0;																	\
																				\
	while (idx < (length)) {													\
		next = GET_NEXT_STATE(table, (current), input[idx]);					\
		if (GET_1BIT_ELEMENT(matches, next)) {									\
			/* It's a match! */													\
			(reports)[res].position = idx;										\
			(reports)[res++].state = next;										\
			if (res == MAX_REPORTS)												\
				break;															\
		}																		\
		(current) = next;														\
		idx++;																	\
	}																			\
}

#endif /* TABLESTATEMACHINE_H_ */
