/*
 * StateMachineDumper.c
 *
 *  Created on: Jan 18, 2011
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <stdio.h>
#include "../Common/StateTable.h"
#include "../Common/PatternTable.h"
#include "../Common/HashMap/HashMap.h"
#include "StateGenerator.h"
#include "StateMachineDumper.h"

void dumpState(State *state, FILE *file, int isSimple) {
	int size, len;

	if (isSimple) {
		size = (state[4] << 8) | (state[5]);
	} else {
		size = getStateSizeInBytes(state);
	}
	if (size <= 0 || size > 1000) {
		fprintf(stderr, "State size is illegal: %d\n", size);
		exit(1);
	}

	len = fwrite(state, 1, size, file);
	if (len != size) {
		fprintf(stderr, "Error dumping state to file\n");
		exit(1);
	}
}

void dumpStates(StateMachine *machine, FILE *file) {
	StateTable *table;
	State **states;
	int i;

	table = machine->states;
	states = table->table;
	for (i = 0; i < table->size; i++) {
		dumpState(states[i], file, machine->isSimple);
	}
}

void readStates(StateMachine *machine, uchar *buff, int count) {
	StateTable *states;
	State *current;
	int i;

	states = machine->states;
	i = 0;
	current = (State*)buff;
	while (i < count) {
		if (current == NULL) {
			fprintf(stderr, "Error reading states from file\n");
			exit(1);
		}

		//id = getStateID(current);
		setState(states, i, current);
/*
		if (i != id) {
			printf("here");
		}
*/
		current = getNextStatePointer(current);
		i++;
	}
	states->nextID = count;
}

void readStateTable(StateMachine *machine, const char *path) {
	char newpath[256];
	uchar size[2];
	uchar *buff;
	int pathlen, len, tableSize, buffSize;
	FILE *file;

	pathlen = strlen(path);
	strcpy(newpath, path);
	strcpy(&(newpath[pathlen]), ".states");

	file = fopen(newpath, "rb");
	if (!file) {
		fprintf(stderr, "Error opening states dump file for reading\n");
		exit(1);
	}

	fseek(file, 0L, SEEK_END);
	buffSize = ftell(file) - 2;
	fseek(file, 0L, SEEK_SET);

	len = fread(size, 1, 2, file);
	if (len != 2) {
		fprintf(stderr, "Error reading state table size from file\n");
		exit(1);
	}
	tableSize = ((size[0]) << 8) | size[1];

	buff = (uchar*)malloc(sizeof(uchar) * buffSize);
	if (buff == NULL) {
		fprintf(stderr, "Error allocating memory for states buffer\n");
		exit(1);
	}
	len = fread(buff, sizeof(char), buffSize, file);
	if (len != buffSize) {
		fprintf(stderr, "Error reading states data from file\n");
		exit(1);
	}

	machine->states = createStateTable(tableSize);
	readStates(machine, buff, tableSize);

	fclose(file);
}

void dumpStateTable(StateMachine *machine, const char *path) {
	char newpath[256];
	uchar size[2];
	int pathlen, len;
	FILE *file;

	pathlen = strlen(path);
	strcpy(newpath, path);
	strcpy(&(newpath[pathlen]), ".states");

	file = fopen(newpath, "wb");
	if (!file) {
		fprintf(stderr, "Error opening states dump file for writing\n");
		exit(1);
	}

	// Write size (2 bytes)
	size[0] = (uchar)((machine->states->size) >> 8);
	size[1] = (uchar)(machine->states->size);

	len = fwrite(size, 1, 2, file);
	if (len != 2) {
		fprintf(stderr, "Error dumping state table size to file\n");
		exit(1);
	}

	// Write states
	dumpStates(machine, file);

	fclose(file);
}

void readFirstLevelLookupTable(StateMachine *machine, FILE *file) {
	int len;
	STATE_PTR_TYPE *table;

	table = machine->firstLevelLookupTable;
	len = fread(table, sizeof(STATE_PTR_TYPE), 256, file);
	if (len != 256) {
		fprintf(stderr, "Error reading first level lookup table from file\n");
		exit(1);
	}
}

void dumpFirstLevelLookupTable(StateMachine *machine, FILE *file) {
	int len;
	len = fwrite(machine->firstLevelLookupTable, sizeof(STATE_PTR_TYPE), 256, file);
	if (len != 256) {
		fprintf(stderr, "Error dumping first level lookup table to file\n");
		exit(1);
	}
}

void readSecondLevelLookupHash(StateMachine *machine, FILE *file) {
	HashMap *map;
	int size, len, i;
	uchar buff[4];
	int key;
	STATE_PTR_TYPE *value;

	len = fread(buff, 1, 2, file);
	if (len != 2) {
		fprintf(stderr, "Error reading second level lookup hash size from file\n");
		exit(1);
	}

	size = (int)(((buff[0] & 0xFF) << 8) | (buff[1] & 0xFF));

	map = hashmap_create();
	machine->secondLevelLookupHash = map;

	for (i = 0; i < size; i++) {
		len = fread(buff, 1, 4, file);
		if (len != 4) {
			fprintf(stderr, "Error reading second level lookup hash entry from file\n");
			exit(1);
		}
		key = (int)(((buff[0] &0xFF) << 8) | (buff[1] & 0xFF));
		value = (STATE_PTR_TYPE*)malloc(sizeof(STATE_PTR_TYPE));
		*value = (STATE_PTR_TYPE)(((buff[2] &0xFF) << 8) | (buff[3] & 0xFF));

		hashmap_put(map, key, value);
	}
}

void dumpSecondLevelLookupHash(StateMachine *machine, FILE *file) {
	HashMap *map;
	int size, len;
	uchar buff[4];
	HashObject *entry;
	STATE_PTR_TYPE *value;

	map = machine->secondLevelLookupHash;
	size = hashmap_size(map);

	// write size
	buff[0] = (uchar)(size >> 8);
	buff[1] = (uchar)size;

	len = fwrite(buff, 1, 2, file);
	if (len != 2) {
		fprintf(stderr, "Error dumping second level lookup hash size to file\n");
		exit(1);
	}

	hashmap_iterator_reset(map);
	while ((entry = hashmap_iterator_next_entry(map)) != NULL) {
		value = entry->data;
		buff[0] = (uchar)((entry->key) >> 8);
		buff[1] = (uchar)(entry->key);
		buff[2] = (uchar)((*value) >> 8);
		buff[3] = (uchar)(*value);
		len = fwrite(buff, 1, 4, file);
		if (len != 4) {
			fprintf(stderr, "Error dumping second level lookup hash entry to file\n");
			exit(1);
		}
	}
}

void readFastLookup(StateMachine *machine, const char *path) {
	char newpath[256];
	int pathlen;
	FILE *file;

	pathlen = strlen(path);
	strcpy(newpath, path);
	strcpy(&(newpath[pathlen]), ".lookup");

	file = fopen(newpath, "rb");
	if (!file) {
		fprintf(stderr, "Error opening lookup dump file for reading\n");
		exit(1);
	}

	// read first level table
	readFirstLevelLookupTable(machine, file);

	// read second level hash
	readSecondLevelLookupHash(machine, file);

	fclose(file);
}

void dumpFastLookup(StateMachine *machine, const char *path) {
	char newpath[256];
	int pathlen;
	FILE *file;

	pathlen = strlen(path);
	strcpy(newpath, path);
	strcpy(&(newpath[pathlen]), ".lookup");

	file = fopen(newpath, "wb");
	if (!file) {
		fprintf(stderr, "Error opening lookup dump file for writing\n");
		exit(1);
	}

	// dump first level table
	dumpFirstLevelLookupTable(machine, file);

	// dump second level hash
	dumpSecondLevelLookupHash(machine, file);

	fclose(file);
}

void readPatterns(StateMachine *machine, FILE *file) {
	char ***patterns;
	int len, slen;
	unsigned int stateID, patternIdx, patCount, size;
	uchar heading[3];
	uchar patLen[2];
	PatternTable *table;
	char *pattern;

	size = (unsigned int)machine->states->size;
	patterns = (char***)malloc(sizeof(char**) * size);
	memset(patterns, 0, sizeof(char**) * size);

	table = (PatternTable*)malloc(sizeof(PatternTable));
	table->patterns = patterns;
	table->size = size;

	machine->patternTable = table;

	while (!feof(file)) {
		len = fread(heading, 1, 3, file);
		if (len == 0) {
			break;
		}
		if (len != 3) {
			fprintf(stderr, "Error reading pattern table (state header) from file\n");
			exit(1);
		}
		stateID = ((heading[0] & 0xFF) << 8) | (heading[1] & 0xFF);
		if (stateID >= size) {
			fprintf(stderr, "Error reading pattern table (invalid state ID) from file\n");
			exit(1);
		}

		patCount = heading[2];
		if (patCount >= 1000000) { // to avoid overflows
			fprintf(stderr, "Error reading pattern table (invalid pattern count) from file\n");
			exit(1);
		}

		patterns[stateID] = (char**)malloc(sizeof(char*) * (patCount + 1));
		for (patternIdx = 0; patternIdx <= patCount; patternIdx++) {
			patterns[stateID][patternIdx] = NULL;
		}

		for (patternIdx = 0; patternIdx < patCount; patternIdx++) {
			len = fread(patLen, 1, 2, file);
			if (len != 2) {
				fprintf(stderr, "Error reading pattern table (pattern length) from file\n");
				exit(1);
			}

			slen = ((patLen[0] & 0xFF) << 8) | (patLen[1] & 0xFF);

			pattern = (char*)malloc(sizeof(char) * slen);

			len = fread(pattern, sizeof(char), slen, file);
			if (len != slen) {
				fprintf(stderr, "Error reading pattern table (pattern data) from file\n");
				exit(1);
			}

			patterns[stateID][patternIdx] = pattern;
		}
	}
}

void readPatternsTable(StateMachine *machine, const char *path) {
	char newpath[256];
	int pathlen;
	FILE *file;

	pathlen = strlen(path);
	strcpy(newpath, path);
	strcpy(&(newpath[pathlen]), ".patterns");

	file = fopen(newpath, "rb");
	if (!file) {
		fprintf(stderr, "Error opening patterns dump file for reading\n");
		exit(1);
	}

	// read the patterns
	readPatterns(machine, file);

	fclose(file);

}

void dumpPatterns(StateMachine *machine, FILE *file) {
	char ***patterns;
	int len, slen;
	unsigned int stateID, patternIdx, patCount, size;
	uchar heading[3];
	uchar patLen[2];

	patterns = machine->patternTable->patterns;
	size = (unsigned int)machine->patternTable->size;

	for (stateID = 0; stateID < size; stateID++) {
		if (patterns[stateID] == NULL)
			continue;
		patternIdx = 0;
		patCount = 0;
		while (patterns[stateID][patCount] != NULL) {
			patCount++;
		}

		heading[0] = (uchar)(stateID >> 8);
		heading[1] = (uchar)(stateID);
		heading[2] = (uchar)(patCount);

		len = fwrite(heading, 1, 3, file);
		if (len != 3) {
			fprintf(stderr, "Error dumping pattern table (state header) to file\n");
			exit(1);
		}

		while (patterns[stateID][patternIdx] != NULL) {
			slen = strlen(patterns[stateID][patternIdx]) + 1;
			patLen[0] = (uchar)(slen >> 8);
			patLen[1] = (uchar)(slen);

			len = fwrite(patLen, 1, 2, file);
			if (len != 2) {
				fprintf(stderr, "Error dumping pattern table (pattern length) to file\n");
				exit(1);
			}

			len = fwrite(patterns[stateID][patternIdx], sizeof(char), slen, file);
			if (len != slen) {
				fprintf(stderr, "Error dumping pattern table (pattern data) to file\n");
				exit(1);
			}

			patternIdx++;
		}
	}
}

void dumpPatternsTable(StateMachine *machine, const char *path) {
	char newpath[256];
	int pathlen;
	FILE *file;

	pathlen = strlen(path);
	strcpy(newpath, path);
	strcpy(&(newpath[pathlen]), ".patterns");

	file = fopen(newpath, "wb");
	if (!file) {
		fprintf(stderr, "Error opening patterns dump file for writing\n");
		exit(1);
	}

	// do the dump
	dumpPatterns(machine, file);

	fclose(file);

}

StateMachine *createStateMachineFromDump(const char *path) {
	StateMachine *machine;

	machine = (StateMachine*)malloc(sizeof(StateMachine));
	machine->isLoadedFromDump = 1;
	machine->isSimple = 0;

	readStateMachine(path, machine);

	return machine;
}

void dumpStateMachine(StateMachine *machine, const char *path) {
	dumpStateTable(machine, path);
	if (!(machine->isSimple)) {
		dumpFastLookup(machine, path);
	}
	dumpPatternsTable(machine, path);
}

void readStateMachine(const char *path, StateMachine *machine) {
	readStateTable(machine, path);
	readFastLookup(machine, path);
	readPatternsTable(machine, path);
}


