/*
 * TableStateMachineGenerator.c
 *
 *  Created on: Jan 25, 2011
 *      Author: yotamhc
 */
#include <stdlib.h>
#include <stdio.h>
#include "../AhoCorasick/ACTypes.h"
#include "../AhoCorasick/ACBuilder.h"
#include "../AhoCorasick/NodeQueue.h"
#include "../Common/HashMap/HashMap.h"
#include "TableStateMachineGenerator.h"

#ifdef COUNT_BY_DEPTH
#define DEPTHMAP
#endif
#ifdef PRINT_STATE_VISIT_HIST
#define DEPTHMAP
#endif

void putStates(TableStateMachine *machine, ACTree *tree, int verbose) {
	Node *node, *fail;
	NodeQueue queue;
	Pair *pair;
	STATE_PTR_TYPE_WIDE row[256];
	STATE_PTR_TYPE_WIDE value;
	int hasValue[256];
	int i, done;
#ifdef AC_PRINT_DEPTH_HIST
	long int depth[200];

	memset(depth, 0, sizeof(long int) * 200);
#endif

	nodequeue_init(&queue);
	nodequeue_enqueue(&queue, tree->root);

	while (!nodequeue_isempty(&queue)) {
		node = nodequeue_dequeue(&queue);
		node->marked = 1;

#ifdef DEPTHMAP
		machine->depthMap[node->id] = node->depth;
#endif
#ifdef AC_PRINT_DEPTH_HIST
		depth[node->depth]++;
#endif

		memset(row, 0, sizeof(STATE_PTR_TYPE_WIDE) * 256);
		memset(hasValue, 0, sizeof(int) * 256);

		if (node->match) {
			setMatch(machine, node->id, node->message, node->messageLength);
		}

		if (node->numGotos > 0) {
			hashmap_iterator_reset(node->gotos);
			while ((pair = hashmap_iterator_next(node->gotos)) != NULL) {
				row[(int)((unsigned char)(pair->c))] = pair->ptr->id;
				hasValue[(int)((unsigned char)(pair->c))] = 1;

				if (!(pair->ptr->marked)) {
					nodequeue_enqueue(&queue, pair->ptr);
				} else {
					printf("Found marked node!\n");
				}
			}
		}

		done = 0;
		fail = node->failure;
		do {
			if (fail == NULL)
				fail = tree->root;

			if (fail->id == 0)
				done = 1;

			if (fail->numGotos > 0) {
				hashmap_iterator_reset(fail->gotos);
				while ((pair = hashmap_iterator_next(fail->gotos)) != NULL) {
					if (!hasValue[(int)((unsigned char)(pair->c))]) {
						row[(int)((unsigned char)(pair->c))] = pair->ptr->id;
						hasValue[(int)((unsigned char)(pair->c))] = 1;
					}
				}
			}
			fail = fail->failure;
		} while (!done);

		for (i = 0; i < 256; i++) {
			value = (hasValue[i] ? row[i] : 0);
			setGoto(machine, node->id, (char)i, value);
		}
	}
	nodequeue_destroy_elements(&queue, 1);

#ifdef AC_PRINT_DEPTH_HIST
	for (i = 0; i < 200; i++) {
		printf("%d\t%ld\n", i, depth[i]);
	}
#endif
}

#ifdef HEAVY_PACKET_RECOGNITION

void reorderStatesForCommonOrdering(ACTree *tree, const char *common_reorder_map_path) {
	FILE *f;
	char buff[80];
	char *retval;
	int value;
	int i;
	int *reorderMap;

	f = fopen(common_reorder_map_path, "r");
	if (!f) {
		fprintf(stderr, "ERROR: Cannot read common reorder map file\n");
		return;
	}

	i = 0;

	reorderMap = (int*)malloc(sizeof(int) * tree->size);

	do {
		retval = fgets(buff, 80, f);
		if (!retval || buff[0] == '&') {
			break;
		}
		if (i >= tree->size) {
			fprintf(stderr, "ERROR: Number of values in common reorder map is bigger than number of states\n");
			free(reorderMap);
			fclose(f);
			return;
		}
		buff[strlen(buff)] = '\0';
		value = atoi(buff);
		if (value >= tree->size) {
			fprintf(stderr, "ERROR: Reorder map points to non-existing state\n");
			free(reorderMap);
			fclose(f);
			return;
		}

		reorderMap[value] = i;

		i++;
	} while (retval);

	fclose(f);

	acReorderStates(tree, reorderMap);

	free(reorderMap);

	if (i > tree->size) {
		fprintf(stderr, "ERROR: Number of values in common marks file is bigger than number of states\n");
		return;
	} else if (i < tree->size) {
		fprintf(stderr, "ERROR: Number of values in common marks file is smaller than number of states\n");
		return;
	}
	return;
}
/*
int markCommonStates(TableStateMachine *machine, const char *common_marks_path) {
	// File should contain a byte for each state in the state machine
	// Each byte should be '1' or '0'. '&' means EOF.

	FILE *f;
	int value;
	int i;

	f = fopen(common_marks_path, "r");
	if (!f) {
		fprintf(stderr, "ERROR: Cannot read common marks file\n");
		return 1;
	}

	i = 0;

	do {
		value = fgetc(f);
		if (value == '&') {
			break;
		}
		if (i >= machine->numStates) {
			fprintf(stderr, "ERROR: Number of values in common marks file is bigger than number of states\n");
			fclose(f);
			return 1;
		}
		if (value == '1') {
			SET_COMMON_STATE(machine, i);
			//((machine)->commons)[(i) / 8] = (((((machine)->commons)[(i) / 8]) & 0xFF) | (((1) & 0x01) << ((i) % 8)))
			//if ((i / 8) >= 9648)
			//	printf("i is too big: %d, (i/8=%d)\n", i, ((i)/8));
		} else if (value != '0') {
			fprintf(stderr, "ERROR: Invalid values in common marks file\n");
			fclose(f);
			return 1;
		}
		i++;
	} while (value != EOF);

	fclose(f);

	if (i > machine->numStates) {
		fprintf(stderr, "ERROR: Number of values in common marks file is bigger than number of states\n");
		return 1;
	} else if (i < machine->numStates) {
		fprintf(stderr, "ERROR: Number of values in common marks file is smaller than number of states\n");
		return 1;
	}
	return 0;
}
*/
#endif

#ifdef FIND_MC2_BAD_PATH
#define FIND_MC2_PATH_LENGTH 500
#define MC2_PATH_FILE "/tmp/mc2_bad_path.bin"
#endif

TableStateMachine *generateTableStateMachine(const char *path, int num_commons, double uncommon_rate_limit, const char *common_marks_path, const char *common_reorder_map_path, int verbose) {
	ACTree tree;
	TableStateMachine *machine;
#ifdef FIND_MC2_BAD_PATH
	int len, sum, written;
	unsigned char size[2];
	int path_states[FIND_MC2_PATH_LENGTH];
	char path_chars[FIND_MC2_PATH_LENGTH];
	FILE *file;
#endif

	acBuildTree(&tree, path, 0, 1);

#ifdef HEAVY_PACKET_RECOGNITION
#ifndef PRINT_STATE_VISIT_HIST
	if (common_reorder_map_path != NULL) {
		reorderStatesForCommonOrdering(&tree, common_reorder_map_path);
	}
#endif

#ifdef FIND_MC2_BAD_PATH
	len = acFindBadMC2Path(&tree, path_states, path_chars, FIND_MC2_PATH_LENGTH);
	sum = acCountUncommonStates(path_states, len);
	printf("Found bad MC2 path of length %d, with %d uncommon states\n", len, sum);
	file = fopen(MC2_PATH_FILE, "w");
	if (!file) {
		fprintf(stderr, "ERROR: Cannot open file %s\n", MC2_PATH_FILE);
		exit(1);
	}
	size[0] = (unsigned char)len;
	size[1] = (unsigned char)(len >> 8);

	written = fwrite(size, sizeof(unsigned char), 2, file);
	if (written != 2) {
		fprintf(stderr, "ERROR: Cannot write size to file %s\n", MC2_PATH_FILE);
		exit(1);
	}

	written = fwrite(path_chars, sizeof(char), len, file);
	if (written != len) {
		fprintf(stderr, "ERROR: Cannot write file %s\n", MC2_PATH_FILE);
		exit(1);
	}

	fclose(file);
#endif

#endif

	machine = createTableStateMachine(tree.size, num_commons, uncommon_rate_limit);

	// Put states data
	putStates(machine, &tree, verbose);

	// Destroy AC tree
	acDestroyTreeNodes(&tree);
/*
#ifdef HEAVY_PACKET_RECOGNITION
	if (!common_reorder_map_path && common_marks_path != NULL) {
		markCommonStates(machine, common_marks_path);
	}
#endif
*/
	return machine;
}
