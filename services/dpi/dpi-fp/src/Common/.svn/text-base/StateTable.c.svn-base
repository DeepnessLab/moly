/*
 * StateTable.c
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <string.h>
#include "StateTable.h"

StateTable *createStateTable(int size) {
	StateTable *table;
	State **data;

	table = (StateTable*)malloc(sizeof(StateTable));
	data = (State**)malloc(size * sizeof(State*));
	memset(data, 0, size * sizeof(State*));
	table->table = data;
	table->size = size;
	table->nextID = 0;

	return table;
}

int addState(StateTable *table, State *state) {
	int id = table->nextID++;
	table->table[id] = state;
	return id;
}

void setState(StateTable *table, int id, State *state) {
	table->table[id] = state;
}

State *getState(StateTable *table, int id) {
	return table->table[id];
}

int getStateTableSize(StateTable* table) {
	return table->size;
}

int getStateTableOccupancy(StateTable* table) {
	return table->nextID;
}

void destroyStateTable(StateTable *table) {
	free(table->table);
	free(table);
}
