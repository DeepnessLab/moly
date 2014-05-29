/*
 * StateMachine.c
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <math.h>
#include "../Common/BitArray/BitArray.h"

#include "../Common/Flags.h"
#include "../States/LinearEncodedState.h"
#include "../States/BitmapEncodedState.h"
#include "../States/LookupTableState.h"
#include "../States/PathCompressedState.h"
#include "../States/SimpleLinearEncodedState.h"
#include "../Common/StateTable.h"
#include "../Common/FastLookup/FastLookup.h"
#include "StateMachine.h"

#define GET_STATE_TYPE(state) ((*((uchar*)state)) & 0x3)

inline int getNextState(StateMachine *machine, State *current, char *str, int length, int *idx, NextStateResult *result, int verbose) {
	StateHeader *header = (StateHeader*)current;

	switch (header->type) {
	case STATE_TYPE_LINEAR_ENCODED:
		return getNextState_LE(current, str, length, idx, result, machine, machine->patternTable, verbose);
	case STATE_TYPE_BITMAP_ENCODED:
		return getNextState_BM(current, str, length, idx, result, machine, machine->patternTable, verbose);
	case STATE_TYPE_LOOKUP_TABLE:
		return getNextState_LT(current, str, length, idx, result, machine, machine->patternTable, verbose);
	case STATE_TYPE_PATH_COMPRESSED:
		return getNextState_PC(current, str, length, idx, result, machine, machine->patternTable, verbose);
	}
	return FALSE;
}

#define GET_NEXT_STATE(machine, current, str, length, idx, result) 												\
	((GET_STATE_TYPE(current) == STATE_TYPE_LINEAR_ENCODED) ?													\
		getNextState_LE(current, str, length, idx, result, machine, machine->patternTable, verbose) :					\
		((GET_STATE_TYPE(current) == STATE_TYPE_BITMAP_ENCODED) ?												\
			getNextState_BM(current, str, length, idx, result, machine, machine->patternTable, verbose) :				\
			((GET_STATE_TYPE(current) == STATE_TYPE_LOOKUP_TABLE) ?											\
				getNextState_LT(current, str, length, idx, result, machine, machine->patternTable, verbose) :			\
				((GET_STATE_TYPE(current) == STATE_TYPE_PATH_COMPRESSED) ?										\
					getNextState_PC(current, str, length, idx, result, machine, machine->patternTable, verbose) : FALSE	\
					))))

int getStateID(State *state) {
	StateHeader *header = (StateHeader*)state;

	switch (header->type) {
	case STATE_TYPE_LINEAR_ENCODED:
		return getStateID_LE(state);
	case STATE_TYPE_BITMAP_ENCODED:
		return getStateID_BM(state);
	case STATE_TYPE_LOOKUP_TABLE:
		return getStateID_LT(state);
	case STATE_TYPE_PATH_COMPRESSED:
		return getStateID_PC(state);
	}
	return FALSE;
}

int matchRecursive(StateMachine *machine, char *input, int length, int *idx, State *s, int verbose) {
	State *nextState;
	int res;
	NextStateResult next;

	if (*idx >= length)
		return 0;

	res = 0;

	//getNextState(machine, s, input, length, idx, &next);
	//GET_NEXT_STATE(machine, s, input, length, idx, &next);
	switch (GET_STATE_TYPE(s)) {
	case STATE_TYPE_LINEAR_ENCODED:
		getNextState_LE(s, input, length, idx, &next, machine, machine->patternTable, verbose);
		break;
	case STATE_TYPE_BITMAP_ENCODED:
		getNextState_BM(s, input, length, idx, &next, machine, machine->patternTable, verbose);
		break;
	case STATE_TYPE_LOOKUP_TABLE:
		getNextState_LT(s, input, length, idx, &next, machine, machine->patternTable, verbose);
		break;
	case STATE_TYPE_PATH_COMPRESSED:
		getNextState_PC(s, input, length, idx, &next, machine, machine->patternTable, verbose);
		break;
	}

	nextState = next.next;

	if ((s == nextState) && (s == (machine->states->table[0])) && (next.fail)) {
		(*idx)++;
	}

	if (next.match) {
#ifdef PRINT_MATCHES
		if (verbose) {
			printf("%s\n", next.pattern);
		}
#endif
		res = 1;
	}

#ifdef TRACE_STATE_MACHINE
	printf("Current state: %d, next char: %c, %s, next state: %d\n", getStateID(s), input[*idx], (next.match ? "matched" : "not matched"), getStateID(nextState));
#endif

	return matchRecursive(machine, input, length, idx, nextState, verbose) || res;
}

#ifdef COUNT_CALLS
static int counter = 0;
static int lt_cnt = 0;
#endif

#ifdef X_RATIO
	static long int count_root = 0;
	static long int count_match = 0;
	static long int count_states = 0;
	static long int count_fail_noroot = 0;
	static long int count_fail = 0;
	static long int count_fail_up = 0;
	static long int count_forward = 0;

	void printXRatio() {
		/*
		printf("States visited: %ld\n", count_states);
		printf("Root visits: %ld (%2.2f%% of states visited)\n", count_root, ((double)count_root / count_states) * 100);
		printf("Matches: %ld (%2.2f%% of states visited)\n", count_match, ((double)count_match / count_states) * 100);
		printf("Forwards taken: %ld (%2.2f%% of states visited)\n", count_forward, ((double)count_forward / count_states) * 100);
		printf("Failures taken: %ld (%2.2f%% of states visited)\n", count_fail, ((double)count_fail / count_states) * 100);
		printf("Failures not to root: %ld (%2.2f%% of states visited)\n", count_fail_noroot, ((double)count_fail_noroot / count_states) * 100);
		printf("Failures to a different state: %ld (%2.2f%% of states visited)\n", count_fail_up, ((double)count_fail_up / count_states) * 100);
		printf("X-Ratio: %f\n", ((double)count_match) / count_root);
		printf("Y-Ratio: %f\n", 1 - ((double)count_root) / count_states);
		printf("Z-Ratio: %f\n", ((double)count_match) / (count_root + count_fail_noroot));
		printf("a-Ratio: %f\n", ((double)count_match) / count_fail);
		printf("b-Ratio: %f\n", ((double)count_fail_up) / count_forward);
		*/
		printf("%ld\n", count_states);
		printf("%ld\n", count_root);
		printf("%ld\n", count_match);
		printf("%ld\n", count_forward);
		printf("%ld\n", count_fail);
		printf("%ld\n", count_fail_noroot);
		printf("%ld\n", count_fail_up);
		printf("%f\n", ((double)count_match) / count_root);
		printf("%f\n", 1 - ((double)count_root) / count_states);
		printf("%f\n", ((double)count_match) / (count_root + count_fail_noroot));
		printf("%f\n", ((double)count_match) / count_fail);
		printf("%f\n", ((double)count_fail_up) / count_forward);
	}
#endif

int matchIterative(StateMachine *machine, char *input, int length, int *idx, State *s, int verbose, MachineStats *stats, int drop) {
	State *nextState;
	int res;
	NextStateResult next;
	State **stateTable;
	STATE_PTR_TYPE nxtid;
	int lastRootVisit;
	int rootVisits;

#ifdef TRACE_STATE_MACHINE
	char lastC;
#endif

#ifdef COUNT_CALLS
	counter++;
#endif

	res = 0;
	lastRootVisit = 0;
	rootVisits = 0;
	stateTable = machine->states->table;

	while (*idx < length) {
#ifdef TRACE_STATE_MACHINE
		lastC = input[*idx];
#endif
		next.fail = FALSE;
		//getNextState(machine, s, input, length, idx, &next);
		//GET_NEXT_STATE(machine, s, input, length, idx, &next);

		if (s == stateTable[0]) {
			lastRootVisit = 0;
			rootVisits++;
			if (drop && rootVisits > drop) {
				// Drop
				return -1;
			}
			nxtid = GET_FIRST_LEVEL_STATE(machine, input[*idx]);
			if (nxtid == LOOKUP_VALUE_NOT_FOUND) {
				next.fail = TRUE;
				nextState = s;
			} else {
				nextState = GET_STATE_POINTER_FROM_ID(machine, nxtid);
				if (nextState == s)
					next.fail = TRUE;
				else
					(*idx)++;
			}
			// THIS PREVENTS ONE CHAR MATCHES!
		} else {
			lastRootVisit++;
			/*
			if (drop && lastRootVisit > drop) {
				// Drop packet
				return -1;
			}
			*/
			switch (GET_STATE_TYPE(s)) {
			case STATE_TYPE_LOOKUP_TABLE:
				//getNextState_LT(s, input, length, idx, &next, machine, machine->patternTable, verbose);
				GET_NEXT_STATE_LT(s, input, length, idx, &next, machine, machine->patternTable, verbose)
				break;
			case STATE_TYPE_PATH_COMPRESSED:
				//getNextState_PC(s, input, length, idx, &next, machine, machine->patternTable, verbose);
				GET_NEXT_STATE_PC(s, input, length, idx, &next, machine, machine->patternTable, verbose)
				break;
			case STATE_TYPE_LINEAR_ENCODED:
				getNextState_LE(s, input, length, idx, &next, machine, machine->patternTable, verbose);
				break;
			case STATE_TYPE_BITMAP_ENCODED:
				getNextState_BM(s, input, length, idx, &next, machine, machine->patternTable, verbose);
				break;
			}

			nextState = next.next;
		}
#ifdef COUNT_FAIL_PERCENT
		if (next.fail && (s != nextState || s != stateTable[0])) {
			stats->totalFailures += 1;
		} else {
			stats->totalGotos += 1;
		}
#endif

		if ((s == nextState) && (s == (stateTable[0])) && (next.fail)) {
			(*idx)++;
		}

#ifdef X_RATIO
		count_states++;
		if (nextState == stateTable[0]) {
			count_root++;
		} else if (next.fail) {
			count_fail_noroot++;
		}
		if (next.fail && s != nextState) {
			count_fail_up++;
		}

		if (next.fail) {
			count_fail++;
		} else {
			count_forward++;
		}
#endif

		if (next.match) {
#ifdef PRINT_MATCHES
			if (verbose) {
				printf("%s\n", next.pattern);
			}
#endif
#ifdef X_RATIO
			count_match++;
#endif
			res = 1;
		}

#ifdef TRACE_STATE_MACHINE
		printf("Current state: %d, last char: %c, next char: %c, %s, next state: %d, failure: %s\n", getStateID(s), lastC, input[*idx], (next.match ? "matched" : "not matched"), getStateID(nextState), (next.fail ? "yes" : "no"));
#endif

		s = nextState;
	}

	return res;
}

#ifdef COUNT_CALLS
void printCounters() {
	//printCounter_LT();
	//printf("getNextState_LT count: %d\n", lt_cnt);
	printCounter_PC();
	printf("GET_3BITS_ELEMENT count: %d\n", getCounter_BitArray());
	printf("matchIterative count: %d\n", counter);
}
#endif

int matchIterativeSimple(StateMachine *machine, char *input, int length, int *idx, State *s, int verbose, MachineStats *stats) {
	State *nextState;
	int res, id;
	NextStateResult next;
	State **stateTable;
#ifdef TRACE_STATE_MACHINE
	char lastC;
	int id2;
#endif

	res = 0;
	stateTable = machine->states->table;

	while (*idx < length) {
#ifdef TRACE_STATE_MACHINE
		lastC = input[*idx];
#endif
		//getNextState(machine, s, input, length, idx, &next);
		//GET_NEXT_STATE(machine, s, input, length, idx, &next);

		getNextState_SLE(s, input, length, idx, &next, machine, machine->patternTable, verbose);

		nextState = next.next;

		if (verbose && ((((StateHeader*)nextState)->size) & 0x20)) {
			// Count pattern index
			id = (nextState[2] << 8) | nextState[3];
			if ((((StateHeader*)nextState)->size) & 0x10) {
				id += 0x10000;
			}
			next.pattern = machine->patternTable->patterns[id][0];
			next.match = TRUE;
		}


		if ((s == nextState) && (s == (stateTable[0])) && (next.fail)) {
			(*idx)++;
		}

		if (next.match) {
#ifdef PRINT_MATCHES
			if (verbose) {
				printf("%s\n", next.pattern);
			}
#endif
			res = 1;
		}
#ifdef TRACE_STATE_MACHINE
		id = (nextState[2] << 8) | nextState[3];
		id2 = (s[2] << 8) | s[3];
		printf("Current state: %d (%p), last char: %c (0x%X), next char: %c (0x%X), %s, next state: %d (%p)\n", id2, s, lastC, (unsigned int)lastC, input[*idx], (unsigned int)(input[*idx]), (next.match ? "matched" : "not matched"), id, nextState);
#endif

		s = nextState;
	}

	return res;
}

int match(StateMachine *machine, char *input, int length, int verbose, MachineStats *stats, int init_idx, int drop) {
	int idx = 0;

	if (init_idx > 0) {
		idx = init_idx;
	}

	//return matchRecursive(machine, input, length, &idx, machine->states->table[0], verbose);
	if (machine->isSimple) {
		return matchIterativeSimple(machine, input, length, &idx, machine->states->table[0], verbose, stats);
	} else {
		return matchIterative(machine, input, length, &idx, machine->states->table[0], verbose, stats, drop);
	}
}

void compressStateTable(StateMachine *machine) {
	StateTable *oldTable, *newTable;
	int size, i;
	oldTable = machine->states;
	size = oldTable->nextID;
	newTable = createStateTable(size);
	for (i = 0; i < size; i++) {
		newTable->table[i] = oldTable->table[i];
	}
	newTable->nextID = i;
	machine->states = newTable;
	destroyStateTable(oldTable);
}
