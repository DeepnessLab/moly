/*
 * StateMachineGenerator.c
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <stdio.h>
#include "../Common/Flags.h"
#include "../AhoCorasick/ACBuilder.h"
#include "../AhoCorasick/NodeQueue.h"
#include "../Common/StateTable.h"
#include "../Common/FastLookup/FastLookup.h"
#include "StateGenerator.h"
#include "StateMachineGenerator.h"

#include "../States/SimpleLinearEncodedState.h"

#define MAX_PATH_LENGTH 512
#define MAX_FAIL_PATH_LENGTH 128

typedef struct {
	char c;
	int match;
	char *messages[MAX_FAIL_PATH_LENGTH];
	int messagesLen[MAX_FAIL_PATH_LENGTH];
	int numMessages;
	STATE_PTR_TYPE state;
	short ptrType;
} NextStatePtr;

void collectCompressionStats(Node *node, int *pathCompressed, int *leafCompressed) {
	Pair *pair;

	if (node->numGotos == 0 && node->id != -1 && node->id != -2) {
		fprintf(stderr, "Bad compression state\n");
		exit(1);
	}
	if (node->numGotos != 0 && node->id == -1) {
		fprintf(stderr, "Bad compression state\n");
		exit(1);
	}
	if (node->numGotos != 0 && node->numGotos != 1 && node->id == -2) {
		fprintf(stderr, "Bad compression state\n");
		exit(1);
	}

	if (node->id == -1)
		(*leafCompressed)++;
	else if (node->id == -2)
		(*pathCompressed)++;

	hashmap_iterator_reset(node->gotos);
	while ((pair = hashmap_iterator_next(node->gotos)) != NULL) {
		collectCompressionStats(pair->ptr, pathCompressed, leafCompressed);
	}
}

void printCompressionStats(ACTree *tree) {
	int pathComp, leafComp;
	pathComp = leafComp = 0;

	collectCompressionStats(tree->root, &pathComp, &leafComp);

	printf("Total nodes compressed with path compression: %d (%f%%)\n", pathComp, (pathComp * 100.0) / tree->size);
	printf("Total nodes compressed with leaf compression: %d (%f%%)\n", leafComp, (leafComp * 100.0) / tree->size);
}

// Generates empty compressed states and assigns them an ID
void generateStates(StateMachine *machine, ACTree *tree, int maxGotosLE, int maxGotosBM, int verbose) {
	Node *node, *next, *fail;
	Pair *pair;
	State *state;
	int id;
	int stateType, numPtrsToL12;
	int pathIdx;
	char chars[MAX_PATH_LENGTH]; // <- for debug purposes only
	NodeQueue queue;
	int counters[4];

	memset(counters, 0, 4 * sizeof(int));

	nodequeue_init(&queue);

	nodequeue_enqueue(&queue, tree->root);

	// Generate new states (no leaves are generated!)
	// States are created with no data!
	while (!nodequeue_isempty(&queue)) {
		node = nodequeue_dequeue(&queue);

		// Create a state
		if (node->numGotos > 1) {
			if (node->numGotos <= maxGotosLE)
				stateType = STATE_TYPE_LINEAR_ENCODED;
			else if (node->numGotos <= maxGotosBM)
				stateType = STATE_TYPE_BITMAP_ENCODED;
			else
				stateType = STATE_TYPE_LOOKUP_TABLE;

			numPtrsToL12 = 0; // goto ptrs
			hashmap_iterator_reset(node->gotos);
			while ((pair = hashmap_iterator_next(node->gotos)) != NULL) {
				if (pair->ptr->depth == 1 || pair->ptr->depth == 2) {
					if (pair->ptr->numGotos == 0) {
						// This node will not be here soon
						fail = pair->ptr->failure;
						while (fail != NULL && fail->id != 0 && fail->numGotos == 0) {
							fail = fail->failure;
						}
						if (fail != NULL && (fail->depth == 1 || fail->depth == 2)) {
							numPtrsToL12++;
						}
					} else {
						numPtrsToL12++;
					}

					if (node == tree->root) {
						pair->ptr->isFirstLevelNode = 1;
						pair->ptr->c1 = pair->c;
					} else if (node->isFirstLevelNode) {
						pair->ptr->isSecondLevelNode = 1;
						pair->ptr->c1 = node->c1;
						pair->ptr->c2 = pair->c;
					} else {
						pair->ptr->c1 = pair->c;
					}
				}
			}

			state = generateEmptyBranchState(stateType, machine->states->nextID, node->numGotos, numPtrsToL12);
			counters[stateType]++;
#ifdef PRINT_STATE_GENERATION
			printf("Generated empty branch state of type %X. ID: %d, # gotos: %d, #ptrs to L1/2: %d. State address: %p. Node address: %p ", stateType, machine->states->nextID, node->numGotos, numPtrsToL12, state, node);
			printf("Gotos: ");
			hashmap_print(node->gotos, &printPair);
			printf("\n");
#endif

			// Add child nodes to queue
			hashmap_iterator_reset(node->gotos);
			while ((pair = hashmap_iterator_next(node->gotos)) != NULL) {
				nodequeue_enqueue(&queue, pair->ptr);
				pair->ptr->id = -1;
			}

			// Add state to state table
			id = addState(machine->states, state);

			// Set new id on node
			node->id = id;
			if (id >= 100000) {
				fprintf(stderr, "Suspicious node ID: %d\n", id);
			}
		} else if (node->numGotos == 1) {
			pathIdx = 0;
			next = node;
			numPtrsToL12 = 0; // fail ptrs
			while (1) {
				hashmap_iterator_reset(next->gotos);
				pair = hashmap_iterator_next(next->gotos);
				if (pair) {
					if (next->failure->depth == 0 || next->failure->depth == 1 || next->failure->depth == 2) {
						//numPtrsToL12++;
						/*
						if (!(next->failure->isFirstLevelNode) && !(next->failure->isSecondLevelNode)) {
							fprintf(stderr, "Unmarked first/second level state (id: %d)\n", next->failure->id);
							//exit(1);
						}
						*/
						if (next->failure->numGotos == 0) {
							// This node will not be here soon
							fail = next->failure->failure;
							while (fail != NULL && fail->id != 0 && fail->numGotos == 0) {
								fail = fail->failure;
							}
							if ((fail == NULL) || (fail != NULL && (fail->depth == 0 || fail->depth == 1 || fail->depth == 2))) {
								numPtrsToL12++;
							}
						} else {
							numPtrsToL12++;
						}
					}
					pathIdx++;
					next = pair->ptr;
					chars[pathIdx - 1] = pair->c; // <- for debug purposes only
					next->id = -2; // <- -2 means that this node is compressed with path compression
				} else {
					next = NULL;
				}

				if (next == NULL || next->hasFailInto || next->numGotos > 1) {
					// End of path
					chars[pathIdx] = '\0'; // <- for debug purposes only
					state = generateEmptyPathCompressedState(machine->states->nextID, pathIdx, numPtrsToL12);
					counters[STATE_TYPE_PATH_COMPRESSED]++;
#ifdef PRINT_STATE_GENERATION
					printf("Generated empty path compressed state. ID: %d, length: %d, #ptrs to L1/2: %d. State address: %p. Node address: %p", machine->states->nextID, pathIdx, numPtrsToL12, state, node);
					printf(" chars: %s\n", chars);
#endif
					if (next != NULL) {
						if (node->isFirstLevelNode && pathIdx == 1) {
							next->isSecondLevelNode = 1;
							next->c1 = node->c1;
							next->c2 = chars[0];
						}
						nodequeue_enqueue(&queue, next);
						next->id = -1;
					}
					break;
				}
			}

			// Add state to state table
			id = addState(machine->states, state);

			// Set new id on node
			node->id = id;
			if (id >= 100000) {
				fprintf(stderr, "Suspicious node ID: %d\n", id);
			}
		}
	}

	if (verbose) {
		printf("Total states created:\n");
		for (pathIdx = 0; pathIdx < 4; pathIdx++) {
			printf("Type %X: %d\n", pathIdx, counters[pathIdx]);
		}
		printf("Total states in AC: %d\n", tree->size);

		printCompressionStats(tree);
	}

	nodequeue_destroy_elements(&queue, 1);
}

void setRealPointers(int currentState, char c, int isFailure, Node *nextNode, NextStatePtr *next) {
	next->match = 0;
	next->c = c;
	next->numMessages = 0;
	if (nextNode->id < 0) {
		while (nextNode->id < 0) {
			if (nextNode->match) {
				if (isFailure && nextNode->id < 0) {
					fprintf(stderr, "Error: found a fail pointer that points to an accepting state\n");
					exit(1);
				}
				next->match = 1;
				next->messages[next->numMessages] = nextNode->message;
				next->messagesLen[next->numMessages++] = nextNode->messageLength;
			}
			nextNode = nextNode->failure;
		}
	}
	next->state = nextNode->id;
	next->ptrType = (nextNode->isFirstLevelNode ? PTR_TYPE_LEVEL1 : (nextNode->isSecondLevelNode ? PTR_TYPE_LEVEL2 : PTR_TYPE_REGULAR));
	if (nextNode->match) {
		if (isFailure && nextNode->id < 0) {
			fprintf(stderr, "Error: found a fail pointer that points to an accepting state\n");
			exit(1);
		}
		next->match = 1;
		next->messages[next->numMessages] = nextNode->message;
		next->messagesLen[next->numMessages++] = nextNode->messageLength;
	}
}

// Places data in states and eventually connects them to each other through the FastLookup machanism
void setStatesData(StateMachine *machine, ACTree *tree, PatternTable *patternTable, int verbose) {
	Node *node, *next;
	Pair *pair;
	StateTable *table;
	State *state;
	int i, j, k, ignoreNext;
	char chars[MAX_PATH_LENGTH];
	NextStatePtr nexts[MAX_PATH_LENGTH];
	STATE_PTR_TYPE ptrs[MAX_PATH_LENGTH];
	int matches[MAX_PATH_LENGTH];
	short ptrType[MAX_PATH_LENGTH];
	NextStatePtr nextPtr;
	char lastChar;
	int pathIdx;
	NodeQueue queue;
	int step;

	nodequeue_init(&queue);

	table = machine->states;

	nodequeue_enqueue(&queue, tree->root);

	step = 0;

	while (!nodequeue_isempty(&queue)) {
		step++;

		node = nodequeue_dequeue(&queue);

		// Fetch state
		state = getState(table, node->id);
		if (state == NULL) {
			fprintf(stderr, "Cannot find a state for node %d\n", node->id);
			exit(1);
		}

		if (node->isFirstLevelNode) {
			machine->firstLevelLookupTable[(int)((unsigned char)(node->c1))] = (unsigned short)(node->id);
		} else if (node->isSecondLevelNode) {
			STATE_PTR_TYPE *val = (STATE_PTR_TYPE*)malloc(sizeof(STATE_PTR_TYPE));
			*val = node->id;
			hashmap_put(machine->secondLevelLookupHash, GET_SECOND_LEVEL_HASH_KEY(((unsigned char)(node->c1)), ((unsigned char)(node->c2))), val);
		}

		// Set data
		if (node->numGotos > 1) {
			i = 0;
			j = 0; // counts number of patterns in this state's row in pattern table
			hashmap_iterator_reset(node->gotos);
			while ((pair = hashmap_iterator_next(node->gotos)) != NULL) {
				chars[i] = pair->c;
				setRealPointers(node->id, pair->c, 0, pair->ptr, &(nexts[i]));
				ptrs[i] = nexts[i].state;
				ptrType[i] = nexts[i].ptrType;
				matches[i] = nexts[i].match;
				if (matches[i]) {
					j++;
				}
				i++;
			}

			patterntable_add_state(patternTable, node->id, j);

			j = 0; // counts index of pattern in state's row in pattern table
			for (i = 0; i < node->numGotos; i++) {
				for (k = 0; k < nexts[i].numMessages; k++) {
					// ugly workaround - maybe incorrect
					if (nexts[i].messages[k] == NULL)
						continue;

					patterntable_add_pattern(patternTable, node->id, j, nexts[i].messages[k], nexts[i].messagesLen[k]);
				}
				if (nexts[i].numMessages > 0) {
					j++;
				}
			}
			setRealPointers(node->id, 0, 1, node->failure, &nextPtr);
			setDataBranchState(machine, state, (uchar*)chars, ptrs, ptrType, matches, nextPtr.state); //node->failure->id);

			// Add child nodes to queue
			if (node != NULL) {
				hashmap_iterator_reset(node->gotos);
				while ((pair = hashmap_iterator_next(node->gotos)) != NULL) {
					if (pair->ptr->id < 0) {
						continue;
					}
					nodequeue_enqueue(&queue, pair->ptr);
				}
			}
		} else if (node->numGotos == 1) {
			pathIdx = 0;
			next = node;
			ignoreNext = 0;
			//Node *nextFail = node;
			lastChar = node->isSecondLevelNode ? node->c2 : node->c1;
			j = 0; // counts number of patterns in this state's row in pattern table
			while (1) {
				hashmap_iterator_reset(next->gotos);
				pair = hashmap_iterator_next(next->gotos);

				if (pair) {
					setRealPointers(next->id, lastChar, 1, next->failure, &(nexts[pathIdx]));

					chars[pathIdx] = pair->c;
					lastChar = pair->c;
					ptrs[pathIdx] = nexts[pathIdx].state; //pair->ptr->failure->id;
					ptrType[pathIdx] = nexts[pathIdx].ptrType;
					/*
					matches[pathIdx] = pair->ptr->match || nexts[pathIdx].match;
					if (matches[pathIdx]) {
						if (pair->ptr->match) {
							nexts[pathIdx].messages[nexts[pathIdx].numMessages++] = pair->ptr->message;
						}
						j++;
					}
					*/
					matches[pathIdx] = next->match || nexts[pathIdx].match;
					if (matches[pathIdx]) {
						if (next->match) {
							nexts[pathIdx].messages[nexts[pathIdx].numMessages] = next->message;
							nexts[pathIdx].messagesLen[nexts[pathIdx].numMessages++] = next->messageLength;
						}
						j++;
					}

					next = pair->ptr;
					//nextFail = pair->ptr;
					pathIdx++;
					matches[pathIdx] = 0;
					nexts[pathIdx].numMessages = 0;
				} else {
					if (next->match) {
						matches[pathIdx] = 1;
						nexts[pathIdx].messages[nexts[pathIdx].numMessages] = next->message;
						nexts[pathIdx].messagesLen[nexts[pathIdx].numMessages++] = next->messageLength;
						j++;
					}
					next = next->failure;
					ignoreNext = 1;
				}

				if (/*next == NULL || */next->hasFailInto || next->numGotos > 1) {
					// End of path
					setRealPointers(node->id, 0, 0, next, &nextPtr);
					if (nextPtr.match) {
						/*
						// Next pointer causes a match
						if (!matches[pathIdx - 1]) {
							// It is not already a match
							matches[pathIdx - 1] = 1;
							j++; // another pattern entry in table
						}
						for (i = 0; i < nextPtr.numMessages; i++) {
							nexts[pathIdx - 1].messages[nexts[pathIdx - 1].numMessages++] = nextPtr.messages[i];
						}
						*/
						// Next pointer causes a match
						if (!matches[pathIdx]) {
							// It is not already a match
							matches[pathIdx] = 1;
							j++; // another pattern entry in table
						}
						for (i = 0; i < nextPtr.numMessages; i++) {
							nexts[pathIdx].messages[nexts[pathIdx].numMessages] = nextPtr.messages[i];
							nexts[pathIdx].messagesLen[nexts[pathIdx].numMessages++] = nextPtr.messagesLen[i];
						}
					}

					patterntable_add_state(patternTable, node->id, j);

					j = 0; // counts index of pattern in state's row in pattern table
					for (i = 1; i <= pathIdx; i++) {
						for (k = 0; k < nexts[i].numMessages; k++) {
							/*
							// ugly workaround - maybe incorrect
							if (nexts[i].messages[k] == NULL)
								continue;
							 */
							patterntable_add_pattern(patternTable, node->id, j, nexts[i].messages[k], nexts[i].messagesLen[k]);
						}
						if (nexts[i].numMessages > 0) {
							j++;
						}
					}

					setDataPathCompressedState(machine, state, (uchar*)chars, ptrs, ptrType, &(matches[1]), nextPtr.state);
					break;
				}
			}

			if (!ignoreNext && next != node && next != NULL) {
				nodequeue_enqueue(&queue, next);
			}
		}
	}
	nodequeue_destroy_elements(&queue, 1);
}

void generateSimpleStates(StateMachine *machine, ACTree *tree, int verbose) {
	Node *node;
	Pair *pair;
	State *state;
	int id;
	NodeQueue queue;

	nodequeue_init(&queue);

	nodequeue_enqueue(&queue, tree->root);

	while (!nodequeue_isempty(&queue)) {
		node = nodequeue_dequeue(&queue);

		state = createEmptyState_SLE(machine->states->nextID, node->numGotos);

		id = addState(machine->states, state);
		node->id = id;

		hashmap_iterator_reset(node->gotos);
		while ((pair = hashmap_iterator_next(node->gotos)) != NULL) {
			nodequeue_enqueue(&queue, pair->ptr);
		}
	}
}

void setSimpleStatesData(StateMachine *machine, ACTree *tree, int verbose) {
	//void setStateData_SLE(State *state, uchar *chars, STATE_PTR_TYPE_WIDE *gotos, STATE_PTR_TYPE_WIDE failure, int match) {
	int i;
	Node *node;
	Pair *pair;
	NodeQueue queue;
	uchar *chars;
	STATE_PTR_TYPE_WIDE *gotos;
	STATE_PTR_TYPE_WIDE failure;
	int match;
	State *state;

	nodequeue_init(&queue);

	nodequeue_enqueue(&queue, tree->root);

	while (!(nodequeue_isempty(&queue))) {
		node = nodequeue_dequeue(&queue);

		chars = (uchar*)malloc(sizeof(uchar) * node->numGotos);
		gotos = (STATE_PTR_TYPE_WIDE*)malloc(sizeof(STATE_PTR_TYPE_WIDE) * node->numGotos);

		i = 0;
		hashmap_iterator_reset(node->gotos);
		while ((pair = hashmap_iterator_next(node->gotos)) != NULL) {
			chars[i] = pair->c;
			gotos[i] = pair->ptr->id;

			nodequeue_enqueue(&queue, pair->ptr);

			i++;
		}

		failure = node->failure->id;

		match = node->match;

		if (match) {
			patterntable_add_state(machine->patternTable, node->id, 1);
			patterntable_add_pattern(machine->patternTable, node->id, 0, node->message, node->messageLength);
		}

		state = getState(machine->states, node->id);
		setStateData_SLE(state, chars, gotos, failure, match);

		free(chars);
		free(gotos);
	}
}

StateMachine *createSimpleStateMachine(const char *path, int maxGotosLE, int maxGotosBM, int verbose) {
	ACTree tree;
	StateTable *table;
	StateMachine *machine;
	PatternTable *patternTable;

	acBuildTree(&tree, path, 0, 0);

	table = createStateTable(tree.size);
	machine = (StateMachine*)malloc(sizeof(StateMachine));

	machine->isLoadedFromDump = 0;
	machine->isSimple = 1;
	machine->states = table;

	// Generate tree states
	generateSimpleStates(machine, &tree, verbose);

	// Add data into states
	patternTable = patterntable_create(table->nextID);
	machine->patternTable = patternTable;
	setSimpleStatesData(machine, &tree, verbose);

	// Destroy AC tree
	acDestroyTreeNodes(&tree);

	return machine;
}

StateMachine *createStateMachine(const char *path, int maxGotosLE, int maxGotosBM, int verbose) {
	ACTree tree;
	StateTable *table;
	StateMachine *machine;
	PatternTable *patternTable;
	int i;

	acBuildTree(&tree, path, 1, 0);

	table = createStateTable(tree.size);
	machine = (StateMachine*)malloc(sizeof(StateMachine));

	machine->isLoadedFromDump = 0;
	machine->isSimple = 0;
	machine->states = table;

	// Generate compressed tree states
	generateStates(machine, &tree, maxGotosLE, maxGotosBM, verbose);

	// Build fast lookup tables
	for (i = 0; i < 256; i++) {
		machine->firstLevelLookupTable[i] = LOOKUP_VALUE_NOT_FOUND;
	}
	machine->secondLevelLookupHash = hashmap_create();

	// Add data into states
	patternTable = patterntable_create(table->nextID);
	machine->patternTable = patternTable;
	setStatesData(machine, &tree, patternTable, verbose);

	// Compress state table
	compressStateTable(machine);

	// Destroy AC tree
	acDestroyTreeNodes(&tree);

	return machine;
}

void destroyStateMachine(StateMachine *machine) {
	State *state;
	StateTable *table;
	STATE_PTR_TYPE *ptr;
	int i;

	patterntable_destroy(machine->patternTable);

	if (!(machine->isSimple)) {
		hashmap_iterator_reset(machine->secondLevelLookupHash);
		while ((ptr = hashmap_iterator_next(machine->secondLevelLookupHash)) != NULL) {
			free(ptr);
		}

		hashmap_destroy(machine->secondLevelLookupHash);
	}

	table = machine->states;

	if (machine->isLoadedFromDump) {
		state = getState(table, 0);
		free(state);
	} else {
		for (i = 0; i < table->size; i++) {
			state = getState(table, i);
			if (state != NULL)
				free(state);
		}
	}

	destroyStateTable(table);
	free(machine);
}

