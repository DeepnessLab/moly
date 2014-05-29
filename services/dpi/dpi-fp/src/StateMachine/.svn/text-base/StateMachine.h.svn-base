/*
 * StateMachine.h
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#ifndef STATEMACHINE_H_
#define STATEMACHINE_H_

#include "../Common/Types.h"
#include "../Common/PatternTable.h"
#include "../Common/HashMap/HashMap.h"

typedef struct {
	int totalFailures;
	int totalGotos;
} MachineStats;


typedef struct {
	StateTable *states;
	PatternTable *patternTable;
	STATE_PTR_TYPE firstLevelLookupTable[256];
	HashMap *secondLevelLookupHash;
	int isLoadedFromDump;
	int isSimple;
} StateMachine;

//int getNextState(StateMachine *machine, State *current, char *str, int length, int *idx, NextStateResult *result);
int match(StateMachine *machine, char *input, int length, int verbose, MachineStats *stats, int init_idx, int drop);
void compressStateTable(StateMachine *machine);
int getStateID(State *state);
#ifdef COUNT_CALLS
void printCounters();
#endif
#ifdef X_RATIO
void printXRatio();
#endif

#endif /* STATEMACHINE_H_ */
