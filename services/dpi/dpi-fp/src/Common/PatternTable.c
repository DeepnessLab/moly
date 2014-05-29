/*
 * PatternTable.c
 *
 *  Created on: Jan 14, 2011
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <stdio.h>
#include "PatternTable.h"

#define MAX_PATTERN_LENGTH 1024

PatternTable *patterntable_create(int size) {
	PatternTable *table = (PatternTable*)malloc(sizeof(PatternTable));
	char ***pats = (char***)malloc(sizeof(char**) * size);
	table->patterns = pats;
	table->size = size;
	return table;
}

void patterntable_destroy(PatternTable *table) {
	char ***patterns;
	int i, j;

	patterns = table->patterns;

	for (i = 0; i < table->size; i++) {
		if (patterns[i] == NULL)
			continue;
		j = 0;
		while (patterns[i][j] != NULL) {
			free(patterns[i][j]);
			j++;
		}
	}
	free(patterns);
	free(table);
}

void patterntable_add_state(PatternTable *table, STATE_PTR_TYPE_WIDE state, int numPatterns) {
	char **statePatterns;
	int i;

	if (numPatterns == 0)
		return;

	statePatterns = (char**)malloc(sizeof(char*) * (numPatterns + 1));
	for (i = 0; i < numPatterns + 1; i++) {
		statePatterns[i] = NULL;
	}
	table->patterns[state] = statePatterns;
}

#define TO_HEX(val) \
	(((val) >= 10) ? ('A' + ((val) - 10)) : (char)('0' + (val)))

char *createPattern(char *pattern, int len) {
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

void patterntable_add_pattern(PatternTable *table, STATE_PTR_TYPE_WIDE sourceState, int idx, char *pattern, int len) {
	char *cpy;

	if (table->patterns[sourceState][idx] != NULL) {
		patterntable_update_pattern(table, sourceState, idx, pattern, len);
		return;
	}

	cpy = createPattern(pattern, len);

	/*
	char *cpy = (char*)malloc(sizeof(char) * (len + 1));
	memcpy(cpy, pattern, len);
	cpy[len] = '\0';
	 */
	table->patterns[sourceState][idx] = cpy;
}

void patterntable_update_pattern(PatternTable *table, STATE_PTR_TYPE_WIDE sourceState, int idx, char *more, int len) {
	int oldlen, newlen, freeOld;
	char *old, *next, *new;

	new = createPattern(more, len);
	len = strlen(new);

	freeOld = 1;

	old = table->patterns[sourceState][idx];
	if (old == NULL) {
		old = "\0";
		freeOld = 0;
	}
	oldlen = strlen(old);
/*
	if (strcmp(old, more) == 0) {
		printf("Strings are equal: %s ;; %s\n", old, more);
	}
*/
	newlen = oldlen + len + 1;

	next = (char*)malloc(sizeof(char) * (newlen + 1));

	strcpy(next, old);
	next[oldlen] = ';';
	strcpy(&(next[oldlen + 1]), new);
	/*
	memcpy(&(next[oldlen + 1]), more, len);
	next[newlen] = '\0';
	 */
	table->patterns[sourceState][idx] = next;

	if (freeOld) {
		free(old);
	}
}
