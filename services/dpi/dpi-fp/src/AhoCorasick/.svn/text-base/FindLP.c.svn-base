/*
 * FindLP.c
 *
 *  Created on: Jan 20, 2011
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <stdio.h>
#include "ACTypes.h"
#include "ACBuilder.h"

typedef struct {
	int weight;
	char buff[1024];
	int length;
} FindResult;

void findLongestWeightedPathRecursive(Node *node, FindResult *res) {
	Pair **pairs;
	int last, i, maxw, maxid, tmpw;
	FindResult *children;

	if (node->numGotos == 0) {
		res->buff[0] = '\0';
		res->length = 0;
		res->weight = 0;
		return;
	}

	pairs = (Pair**)malloc(sizeof(Pair*) * (node->numGotos));
	children = (FindResult*)malloc(sizeof(FindResult) * (node->numGotos));

	if (pairs == NULL || children == NULL) {
		fprintf(stderr, "Out of heap space\n");
		exit(1);
	}

	last = 0;
	hashmap_iterator_reset(node->gotos);
	while ((pairs[last] = hashmap_iterator_next(node->gotos)) != NULL) {
		if (pairs[last] == NULL) {
			fprintf(stderr, "Pair is null!!!\n");
			exit(1);
		}
		last++;
	}

	maxw = 0;
	maxid = -1;
	for (i = 0; i < last; i++) {
		findLongestWeightedPathRecursive(pairs[i]->ptr, &(children[i]));
		tmpw = children[i].weight + ((node->id == 0) ? ((i - 1) * 10) : 0);
		if (maxw < tmpw || maxid == -1) {
			maxw = tmpw;
			maxid = i;
		}
	}

	if (maxid == -1) {
		res->buff[0] = '\0';
		res->length = 0;
		res->weight = 1;
		free(pairs);
		free(children);
		return;
	}

	if (children[maxid].length >= 1024) {
		res->buff[0] = '\0';
		res->length = 0;
		res->weight = 0;
		free(pairs);
		free(children);
		return;
	}

	res->buff[0] = pairs[maxid]->c;
	strcpy(&(res->buff[1]), children[maxid].buff);
	res->length = children[i].length + 1;
	res->weight = maxw + 1;
	//free(pairs);
	//free(children);
}

void findLongestWeightedPath(ACTree *tree) {
	FindResult result;
	findLongestWeightedPathRecursive(tree->root, &result);
	printf("Result path: %s\n", result.buff);
	printf("Length: %d (%d)\n", result.length, (int)strlen(result.buff));
	printf("Weight: %d\n", result.weight);
}

#define SNORT_PATH "/Users/yotamhc/Documents/Study/CompactDFA/Code-Yaron/SnortPatterns.txt"

int main_findLongestPath() {
	ACTree tree;

	acBuildTree(&tree, SNORT_PATH, 0, 1);

	findLongestWeightedPath(&tree);

	acDestroyTreeNodes(&tree);

	return 0;
}

