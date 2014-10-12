/*
 * TableStateMachine.c
 *
 *  Created on: Jan 23, 2011
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../Common/Flags.h"
#include "../Common/BitArray/BitArray.h"
#include "TableStateMachine.h"

#define MAX_PATTERN_LENGTH 1024


TableStateMachine *createTableStateMachine(unsigned int numStates, int totalRules) {
	TableStateMachine *machine;
	STATE_PTR_TYPE_WIDE *table;
	unsigned char *matches;
	//char **patterns;
	MatchRule **rules;
	int *numRules;
#ifdef DEPTHMAP
	int *depthMap;
#endif

	machine = (TableStateMachine*)malloc(sizeof(TableStateMachine));
	table = (STATE_PTR_TYPE_WIDE*)malloc(sizeof(STATE_PTR_TYPE_WIDE) * numStates * 256);
	matches = (unsigned char*)malloc(sizeof(unsigned char) * (int)(ceil(numStates / 8.0)));
	//patterns = (char**)malloc(sizeof(char*) * numStates);
	rules = (MatchRule**)malloc(sizeof(MatchRule*) * numStates);
	numRules = (int*)malloc(sizeof(int) * numStates);
#ifdef DEPTHMAP
	depthMap = (int*)malloc(sizeof(int) * numStates);
#endif

	memset(table, 0, sizeof(STATE_PTR_TYPE_WIDE) * numStates * 256);
	memset(matches, 0, sizeof(unsigned char) * (int)(ceil(numStates / 8.0)));
	//memset(patterns, 0, sizeof(char*) * numStates);
	memset(rules, 0, sizeof(MatchRule*) * numStates);
	memset(numRules, 0, sizeof(int) * numStates);

	machine->table = table;
	machine->numStates = numStates;
	machine->matches = matches;
	//machine->patterns = patterns;
	machine->matchRules = rules;
	machine->numRules = numRules;
	machine->total_rules = totalRules;
#ifdef DEPTHMAP
	machine->depthMap = depthMap;
#endif


	return machine;
}

void destroyTableStateMachine(TableStateMachine *machine) {
	unsigned int i, j;
	for (i = 0; i < machine->numStates; i++) {
		if (machine->matchRules[i]) {
			for (j = 0; j < machine->numRules[i]; j++) {
				if (machine->matchRules[i][j].pattern) {
					free(machine->matchRules[i][j].pattern);
				}
			}
			free(machine->matchRules[i]);
		}
	}
	free(machine->matchRules);
	free(machine->numRules);
	free(machine->matches);
	free(machine->table);
#ifdef DEPTHMAP
	free(machine->depthMap);
#endif
	free(machine);
}

void setGoto(TableStateMachine *machine, STATE_PTR_TYPE_WIDE currentState, char c, STATE_PTR_TYPE_WIDE nextState) {
	machine->table[GET_TABLE_IDX(currentState, c)] = nextState;
}

#define TO_HEX(val) \
	(((val) >= 10) ? ('A' + ((val) - 10)) : (char)('0' + (val)))

char *createPattern_TM(char *pattern, int len) {
	char buff[MAX_PATTERN_LENGTH];
	char *res;
	int i, j;

	for (i = 0, j = 0; i < len; i++) {
		if (pattern[i] >= 32 && pattern[i] < 127) {
			buff[j++] = pattern[i];
		} else {
			buff[j++] = '|';
			buff[j++] = TO_HEX((pattern[i] & 0x0F0) >> 4);
			buff[j++] = TO_HEX(pattern[i] & 0x00F);
			buff[j++] = '|';
		}
	}
	buff[j++] = '\0';
	res = (char*)malloc(sizeof(char) * j);
	strcpy(res, buff);
	return res;
}

void setMatch(TableStateMachine *machine, STATE_PTR_TYPE_WIDE state, MatchRule *rules, int numRules) {
	MatchRule *rulesCpy;
	int i;

	SET_1BIT_ELEMENT(machine->matches, state, 1);

	rulesCpy = (MatchRule*)malloc(sizeof(MatchRule) * numRules);
	for (i = 0; i < numRules; i++) {
		rulesCpy[i].pattern = (char*)malloc(sizeof(char) * rules[i].len);
		memcpy(rulesCpy[i].pattern, rules[i].pattern, sizeof(char) * rules[i].len);
		rulesCpy[i].len = rules[i].len;
		rulesCpy[i].is_regex = rules[i].is_regex;
		rulesCpy[i].rid = rules[i].rid;
	}
	machine->matchRules[state] = rulesCpy;
	machine->numRules[state] = numRules;
}

STATE_PTR_TYPE_WIDE getNextStateFromTable(TableStateMachine *machine, STATE_PTR_TYPE_WIDE currentState, char c) {
	return GET_NEXT_STATE(machine->table, currentState, c);
}

int matchTableMachine(TableStateMachine *machine, char *input, int length, int verbose) {
	STATE_PTR_TYPE_WIDE current, next;
	STATE_PTR_TYPE_WIDE *table;
	unsigned char *matches;
	int idx;
	int res;

	res = 0;
	table = machine->table;
	matches = machine->matches;
	idx = 0;
	current = 0;

	while (idx < length) {
		next = GET_NEXT_STATE(table, current, input[idx]);
		if (GET_1BIT_ELEMENT(matches, next)) {
			// It's a match!
			res = 1;

		}
		current = next;
		idx++;
	}
	return res;
}

