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

void putStates(TableStateMachine *machine, ACTree *tree, int verbose) {
	Node *node, *fail;
	NodeQueue queue;
	Pair *pair;
	STATE_PTR_TYPE_WIDE row[256];
	STATE_PTR_TYPE_WIDE value;
	int hasValue[256];
	int i, done;

	nodequeue_init(&queue);
	nodequeue_enqueue(&queue, tree->root);

	while (!nodequeue_isempty(&queue)) {
		node = nodequeue_dequeue(&queue);
		node->marked = 1;

#ifdef DEPTHMAP
		machine->depthMap[node->id] = node->depth;
#endif

		memset(row, 0, sizeof(STATE_PTR_TYPE_WIDE) * 256);
		memset(hasValue, 0, sizeof(int) * 256);

		if (node->match) {
			setMatch(machine, node->id, node->rules, node->numRules);
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
}

TableStateMachine *generateTableStateMachine(const char *path, int verbose) {
	ACTree tree;
	TableStateMachine *machine;

	acBuildTree(&tree, path);

	machine = createTableStateMachine(tree.size);

	// Put states data
	putStates(machine, &tree, verbose);

	// Destroy AC tree
	acDestroyTreeNodes(&tree);

	return machine;
}
