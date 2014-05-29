/*
 * ACTypes.h
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#ifndef ACTYPES_H_
#define ACTYPES_H_

#include "../Common/HashMap/HashMap.h"
#include "../Common/MatchRule.h"

#define MAX_RULES_PER_STATE 8

struct st_node;

typedef struct {
	char c;
	struct st_node *ptr;
} Pair;

typedef struct st_node {
	int id;
	int numGotos;
	int match;
	MatchRule rules[MAX_RULES_PER_STATE];
	int numRules;
	HashMap *gotos;
	struct st_node *failure;
	int hasFailInto; // TRUE if some other node fails into this node
	int depth;
	char c1, c2;
	int isFirstLevelNode;
	int isSecondLevelNode;
	int marked;
} Node;

#ifdef PCRE
typedef struct {
	int state;
	char *pcre;
} StateRegexPair;
#endif

typedef struct {
	Node *root;
	int size;
#ifdef PCRE
	HashMap *state_pcre_map;
#endif
} ACTree;

#endif /* ACTYPES_H_ */
