/*
 * ACTypes.h
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#ifndef ACTYPES_H_
#define ACTYPES_H_

#include "../Common/HashMap/HashMap.h"

struct st_node;

typedef struct {
	char c;
	struct st_node *ptr;
} Pair;

typedef struct st_node {
	int id;
	int numGotos;
	int match;
	char *message;
	int messageLength;
	HashMap *gotos;
	struct st_node *failure;
	int hasFailInto; // TRUE if some other node fails into this node
	int depth;
	char c1, c2;
	int isFirstLevelNode;
	int isSecondLevelNode;
	int marked;
} Node;

typedef struct {
	Node *root;
	int size;
} ACTree;

#endif /* ACTYPES_H_ */
