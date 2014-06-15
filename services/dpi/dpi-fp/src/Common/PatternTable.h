/*
 * PatternTable.h
 *
 *  Created on: Jan 14, 2011
 *      Author: yotamhc
 */

#ifndef PATTERNTABLE_H_
#define PATTERNTABLE_H_

#include "Types.h"
#include "HashMap/HashMap.h"

typedef struct {
	char ***patterns;
	int size;
} PatternTable;

PatternTable *patterntable_create(int size);
void patterntable_destroy(PatternTable *table);
void patterntable_add_state(PatternTable *table, STATE_PTR_TYPE_WIDE state, int numPatterns);
void patterntable_add_pattern(PatternTable *table, STATE_PTR_TYPE_WIDE sourceState, int idx, char *pattern, int len);
void patterntable_update_pattern(PatternTable *table, STATE_PTR_TYPE_WIDE sourceState, int idx, char *more, int len);

#endif /* PATTERNTABLE_H_ */
