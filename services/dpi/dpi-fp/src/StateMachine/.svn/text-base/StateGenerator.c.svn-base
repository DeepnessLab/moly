/*
 * StateGenerator.c
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#include <stdio.h>
#include "StateGenerator.h"
#include "../States/LinearEncodedState.h"
#include "../States/BitmapEncodedState.h"
#include "../States/LookupTableState.h"
#include "../States/PathCompressedState.h"

#define MAX_ARRAY_LENGTH 100

typedef int (*SortPred)(uchar, uchar);

int sortPredAlphabetically(uchar a, uchar b) {
	return (a < b);
}

void sortArrays(SortPred pred, int len, uchar *chars, STATE_PTR_TYPE *ptrs, short *ptrTypes, int *matches, char **patterns) {
	// INEFFICIENT - Bubble sort...
	int i, j;
	uchar tmpC;
	STATE_PTR_TYPE tmpS;
	short tmpT;
	int tmpM;
	short tmpI;
	short indexes[MAX_ARRAY_LENGTH];
	char *tmpPatterns[MAX_ARRAY_LENGTH];
	int numPatterns;

	numPatterns = 0;
	for (j = 0; j < len; j++) {
		if (matches[j]) {
			indexes[j] = numPatterns++;
		} else {
			indexes[j] = -1;
		}
	}

	for (i = 0; i < len; i++) {
		for (j = len - 1; j > i; j--) {
			if (!pred(chars[j - 1], chars[j])) {
				tmpC = chars[j];
				tmpS = ptrs[j];
				tmpT = ptrTypes[j];
				tmpM = matches[j];
				tmpI = indexes[j];
				chars[j] = chars[j - 1];
				ptrs[j] = ptrs[j - 1];
				ptrTypes[j] = ptrTypes[j - 1];
				matches[j] = matches[j - 1];
				indexes[j] = indexes[j - 1];
				chars[j - 1] = tmpC;
				ptrs[j - 1] = tmpS;
				ptrTypes[j - 1] = tmpT;
				matches[j - 1] = tmpM;
				indexes[j - 1] = tmpI;
			}
		}
	}

	if (numPatterns > 1) {
		i = 0;
		for (j = 0; j < len && i < numPatterns; j++) {
			if (indexes[j] >= 0) {
				tmpPatterns[i] = patterns[indexes[j]];
				i++;
			}
		}
		memcpy(patterns, tmpPatterns, sizeof(char*) * numPatterns);
	}
}
/*
State *generateBranchState(StateType type, int id, int size, char *chars, STATE_PTR_TYPE *gotos, int *matches, STATE_PTR_TYPE failure) {
	if (size > MAX_NODE_DEGREE)
		type = STATE_TYPE_LOOKUP_TABLE;

	switch (type) {
	case STATE_TYPE_LINEAR_ENCODED:
		// TODO: Sort chars by frequency
		return createState_LE(id, size, chars, gotos, matches, failure);
	case STATE_TYPE_BITMAP_ENCODED:
		// TODO: Sort chars alphabetically
		sortArrays(&sortPredAlphabetically, size, chars, gotos, matches);
		return createState_BM(id, size, chars, gotos, matches, failure);
	case STATE_TYPE_LOOKUP_TABLE:
		return createState_LT(id, size, chars, gotos, matches, failure);
	}
	return 0x0;
}

State *generatePathCompressedState(int size, char *chars, STATE_PTR_TYPE *failures, int *matches, STATE_PTR_TYPE next) {
	return createState_PC(size, chars, failures, matches, next);
}
*/
State *generateEmptyBranchState(StateType type, int id, int size, int numPtrsToFirstOrSecondLevel) {
	if (type != STATE_TYPE_LOOKUP_TABLE && size > MAX_NODE_DEGREE) {
		type = STATE_TYPE_LOOKUP_TABLE;
		fprintf(stderr, "State is too large for type %X, generating lookup table state instead (size=%d).\n", type, size);
	}

	switch (type) {
	case STATE_TYPE_LINEAR_ENCODED:
		return createEmptyState_LE(id, size, numPtrsToFirstOrSecondLevel);
	case STATE_TYPE_BITMAP_ENCODED:
		return createEmptyState_BM(id, size, numPtrsToFirstOrSecondLevel);
	case STATE_TYPE_LOOKUP_TABLE:
		return createEmptyState_LT(id, size, numPtrsToFirstOrSecondLevel);
	}
	return 0x0;
}

State *generateEmptyPathCompressedState(int id, int size, int numFailPtrsToFirstOrSecondLevel) {
	return createEmptyState_PC(id, size, numFailPtrsToFirstOrSecondLevel);
}

void setDataBranchState(StateMachine *machine, State *state, uchar *chars, STATE_PTR_TYPE *gotos, short *ptrTypes, int *matches, STATE_PTR_TYPE failure) {
	switch (((StateHeader*)state)->type) {
	case STATE_TYPE_LINEAR_ENCODED:
		// TODO: Sort chars by frequency
		return setStateData_LE(state, chars, gotos, ptrTypes, matches, failure);
	case STATE_TYPE_BITMAP_ENCODED:
		sortArrays(&sortPredAlphabetically, ((StateHeader*)state)->size, chars, gotos, ptrTypes, matches, machine->patternTable->patterns[getStateID_BM(state)]);
		return setStateData_BM(state, chars, gotos, ptrTypes, matches, failure);
	case STATE_TYPE_LOOKUP_TABLE:
		return setStateData_LT(state, chars, gotos, ptrTypes, matches, failure);
	}
}

void setDataPathCompressedState(StateMachine *machine, State *pathCompressedState, uchar *chars, STATE_PTR_TYPE *failures, short *ptrTypes, int *matches, STATE_PTR_TYPE next) {
	return setStateData_PC(pathCompressedState, chars, failures, ptrTypes, matches, next);
}

int getStateSizeInBytes(State *state) {
	switch (((StateHeader*)state)->type) {
	case STATE_TYPE_LINEAR_ENCODED:
		return getSizeInBytes_LE(state);
	case STATE_TYPE_BITMAP_ENCODED:
		return getSizeInBytes_BM(state);
	case STATE_TYPE_LOOKUP_TABLE:
		return getSizeInBytes_LT(state);
	case STATE_TYPE_PATH_COMPRESSED:
		return getSizeInBytes_PC(state);
	default:
		return -1;
	}
}

State *getNextStatePointer(State *state) {
	switch (((StateHeader*)state)->type) {
	case STATE_TYPE_LINEAR_ENCODED:
		return getNextStatePointer_LE(state);
	case STATE_TYPE_BITMAP_ENCODED:
		return getNextStatePointer_BM(state);
	case STATE_TYPE_LOOKUP_TABLE:
		return getNextStatePointer_LT(state);
	case STATE_TYPE_PATH_COMPRESSED:
		return getNextStatePointer_PC(state);
	default:
		return NULL;
	}
}
