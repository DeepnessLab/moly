/*
 * StateGenerator.h
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#ifndef STATEGENERATOR_H_
#define STATEGENERATOR_H_

#include "../Common/Types.h"
#include "StateMachine.h"

//State *generateBranchState(StateType type, int size, char *chars, STATE_PTR_TYPE *gotos, int *matches, STATE_PTR_TYPE failure);
//State *generatePathCompressedState(int size, char *chars, STATE_PTR_TYPE *failures, int *matches, STATE_PTR_TYPE next);

State *generateEmptyBranchState(StateType type, int id, int size, int numPtrsToFirstOrSecondLevel);
State *generateEmptyPathCompressedState(int id, int size, int numFailPtrsToFirstOrSecondLevel);

void setDataBranchState(StateMachine *machine, State *state, uchar *chars, STATE_PTR_TYPE *gotos, short *ptrTypes, int *matches, STATE_PTR_TYPE failure);
void setDataPathCompressedState(StateMachine *machine, State *pathCompressedState, uchar *chars, STATE_PTR_TYPE *failures, short *ptrTypes, int *matches, STATE_PTR_TYPE next);

int getStateSizeInBytes(State *state);
State *getNextStatePointer(State *state);

#endif /* STATEGENERATOR_H_ */
