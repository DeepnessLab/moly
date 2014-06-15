/*
 * StateTable.h
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#ifndef STATETABLE_H_
#define STATETABLE_H_

#include "Types.h"

StateTable *createStateTable(int size);
int addState(StateTable *table, State *state);
void setState(StateTable *table, int id, State *state);
State *getState(StateTable *table, int id);
int getStateTableSize(StateTable* table);
int getStateTableOccupancy(StateTable* table);
void destroyStateTable(StateTable *table);

#endif /* STATETABLE_H_ */
