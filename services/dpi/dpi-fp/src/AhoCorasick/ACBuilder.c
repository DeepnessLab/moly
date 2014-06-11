/*
 * ACBuilder.c
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "../Common/Flags.h"
#include "ACBuilder.h"
#include "NodeQueue.h"
#include "../Common/json.h"

#define READ_BUFFER_SIZE 1024
#define MAX_STATES 65536

Node *createNewNode(ACTree *tree, Node *parent) {
	Node *node = (Node*)malloc(sizeof(Node));
	node->id = tree->size++;
	node->gotos = hashmap_create();
	if (node->gotos == NULL) {
		fprintf(stderr, "ERROR: Cannot create new hashmap\n");
		exit(1);
	}
	node->failure = NULL;
	node->numGotos = 0;
	node->match = 0;
	node->numRules = 0;
	node->hasFailInto = 0;
	if (parent != NULL) {
		node->depth = parent->depth + 1;
	} else {
		node->depth = 0;
	}
	node->isFirstLevelNode = 0;
	node->isSecondLevelNode = 0;
	node->c1 = 0;
	node->c2 = 0;
	node->marked = 0;
	memset(node->rules, 0, sizeof(MatchRule) * MAX_RULES_PER_STATE);
	return node;
}

Pair *createNewPair(char c, Node *node) {
	Pair *p = (Pair*)malloc(sizeof(Pair));
	p->c = c;
	p->ptr = node;
	return p;
}

Node *acGetNextNode(Node *node, char c) {
	Pair *pair;
	pair = (Pair*)(hashmap_get(node->gotos, (int)c));
	if (pair == NULL)
		return NULL;
	return pair->ptr;
}

void enter(ACTree *tree, MatchRule *rule) {
	Node *state = tree->root;
	int j = 0, p;
	Node *next, *newState;
	char *pattern;
	int len;
	MatchRule *ruleCpy;

	pattern = rule->pattern;
	len = rule->len;

	while ((next = acGetNextNode(state, pattern[j])) != NULL && j < len) {
		state = next;
		j++;
	}

	for (p = j; p < len; p++) {
		newState = createNewNode(tree, state);
		Pair *pair = createNewPair(pattern[p], newState);
		hashmap_put(state->gotos, pattern[p], pair);
		state->numGotos++;
		state = newState;
	}

	// Match
	state->match = 1;

	if (state->numRules >= MAX_RULES_PER_STATE) {
		fprintf(stderr, "[ACBuilder] Too many rules for a single state: %d\n", state->id);
		exit(1);
	}
	ruleCpy = &(state->rules[state->numRules]);
	state->numRules++;
	ruleCpy->pattern = (char*)malloc(sizeof(char) * (len + 1));
	memcpy(ruleCpy->pattern, pattern, sizeof(char) * len);
	ruleCpy->is_regex = 0;
	ruleCpy->len = len;
	ruleCpy->rid = rule->rid;
}

void constructFailures(ACTree *tree) {
	NodeQueue q;
	Node *root, *state, *r, *s;
	Pair *pair;
	HashMap *map;
	char a;
	int toL0, toL1, toL2;

	toL0 = toL1 = toL2 = 0;

	nodequeue_init(&q);

	root = tree->root;
	root->failure = root;

	map = root->gotos;
	hashmap_iterator_reset(map);
	while ((pair = hashmap_iterator_next(map)) != NULL) {
		nodequeue_enqueue(&q, pair->ptr);
		pair->ptr->failure = root;
		toL0++;
	}

	while (!nodequeue_isempty(&q)) {
		r = nodequeue_dequeue(&q);
		map = r->gotos;
		hashmap_iterator_reset(map);
		while ((pair = hashmap_iterator_next(map)) != NULL) {
			a = pair->c;
			s = pair->ptr;
			nodequeue_enqueue(&q, s);
			if (r->failure == NULL)
				r->failure = root;
			state = r->failure;
			while (state->id != 0 && acGetNextNode(state, a) == NULL) {
				state = state->failure;
			}
			state = acGetNextNode(state, a);
			if (state == NULL) {
				state = root;
			}
			state->hasFailInto++;
			s->failure = state;

			if (state->depth == 1)
				toL1++;
			else if (state->depth == 2)
				toL2++;
			else if (state == root)
				toL0++;

		}
	}

	nodequeue_destroy_elements(&q, 1);
}

void findFails(Node *node, int *l0, int *l1, int *l2) {
	Pair *pair;

	if (node->failure->depth == 1)
		(*l1)++;
	else if (node->failure->depth == 2)
		(*l2)++;
	else if (node->failure->depth == 0)
		(*l0)++;

	hashmap_iterator_reset(node->gotos);
	while ((pair = hashmap_iterator_next(node->gotos))) {
		findFails(pair->ptr, l0, l1, l2);
	}
}

void getL2nums(ACTree *tree) {
	Node *root = tree->root;
	Pair *pair;
	int l0, l1, l2;

	l1 = root->numGotos;
	l2 = 0;

	hashmap_iterator_reset(root->gotos);
	while ((pair = hashmap_iterator_next(root->gotos))) {
		l2 += pair->ptr->numGotos;
	}

	l0 = l1 = l2 = 0;

	findFails(root, &l0, &l1, &l2);
}

void printPair(void *data) {
	Pair *pair;
	pair = (Pair*)data;
	if (pair->c >= 32 && pair->c < 127) {
		printf("%c:%d", pair->c, pair->ptr->id);
	} else {
		printf("%0X:%d", (int)((pair->c) & 0x0FF), pair->ptr->id);
	}
}

void printNode(Node *node) {
	int i, j, val;
	printf("Node ID: %d, Depth: %d, Gotos: ", node->id, node->depth);
	hashmap_print(node->gotos, &printPair);
	printf(", Failure: %d, Match: %d", node->failure->id, node->match);
	if (node->match) {
		printf(" (messages: ");//, node->message);
		for (i = 0; i < node->numRules; i++) {
			for (j = 0; j < node->rules[i].len; j++) {
				if (node->rules[i].pattern[j] >= 32 && node->rules[i].pattern[j] < 127) {
					printf("%c", node->rules[i].pattern[j]);
				}else {
					val = (int)((node->rules[i].pattern[j]) & 0x0FF);
					printf("|");
					if (val < 16) {
						printf("0");
					}
					printf("%X|", val);
				}
			}
			if (i < node->numRules - 1) {
				printf(", ");
			}
		}
	}
	if (node->hasFailInto) {
		printf(" *Has %d fail(s) into it*", node->hasFailInto);
	}
	printf("\n");
}

void acPrintTree(ACTree *tree) {
	NodeQueue queue;
	Node *node;
	Pair *pair;
	int i;

	nodequeue_init(&queue);

	printf("Printing Aho-Corasick tree:\n");

	nodequeue_enqueue(&queue, tree->root);

	while (!nodequeue_isempty(&queue)) {
		node = nodequeue_dequeue(&queue);
		for (i = 0; i < node->depth; i++) {
			printf("-");
		}
		printNode(node);

		hashmap_iterator_reset(node->gotos);
		while ((pair = hashmap_iterator_next(node->gotos)) != NULL) {
			nodequeue_enqueue(&queue, pair->ptr);
		}
	}

	nodequeue_destroy_elements(&queue, 1);
}

int pattern_to_bin(char *pattern, int len, char *bin) {
	int i, j;
	char hex[3];
	hex[2] = '\0';
	for (i = 0, j = 0; i < len && pattern[i] != '\0'; i++) {
		if (pattern[i] == '|') {
			hex[0] = pattern[i+1];
			hex[1] = pattern[i+2];
			bin[j] = (char)strtol(hex, NULL, 16);
			i += 3;
			j++;
		} else {
			bin[j] = pattern[i];
			j++;
		}
	}
	return j;
}

void *match_rule_parser(char ***pairs, int numPairs, void *result) {
	int i;
	MatchRule *rule;
	int pat_len;
	char pattern[MAX_PATTERN_LENGTH];

	rule = (MatchRule*)result;
	rule->is_regex = 0;
	rule->len = -1;
	rule->rid = 0;
	rule->pattern = NULL;

	for (i = 0; i < numPairs; i++) {
		if (strcmp(pairs[i][0], "className") == 0) {
			if (strcmp(pairs[i][1], "\"MatchRule\"") != 0 && strcmp(pairs[i][1], "'MatchRule'") != 0) {
				fprintf(stderr, "[ACBuilder] Invalid JSON object class: %s.\n", pairs[i][1]);
				exit(1);
			}
		} else if (strcmp(pairs[i][0], "pattern") == 0) {
			pat_len = strlen(pairs[i][1]);
			rule->len = pattern_to_bin(&(pairs[i][1][1]), pat_len - 2, pattern);
			rule->pattern = (char*)malloc(sizeof(char) * rule->len);
			memcpy(rule->pattern, pattern, sizeof(char) * rule->len);
		} else if (strcmp(pairs[i][0], "is_regex") == 0) {
			if (strcmp(pairs[i][1], "true") == 0) {
				rule->is_regex = 1;
			}
		} else if (strcmp(pairs[i][0], "rid") == 0) {
			rule->rid = (unsigned int)atoi(pairs[i][1]);
		} else {
			// Ignore other fields
		}
	}

	return result + sizeof(MatchRule);
}

#define MAX_RULES 8192

void acBuildTree(ACTree *tree, const char *path) {
	MatchRule rules[MAX_RULES];
	int i, count, numRules;

	json_parser parser = match_rule_parser;

	numRules = parse_json_file(path, parser, rules);
	if (numRules < 0) {
		// Error
		exit(numRules);
	} else if (numRules == 0) {
		// No rules
		return;
	}

	tree->size = 0;
	count = 0;
	i = 0;
	tree->root = createNewNode(tree, NULL);

	while (i < numRules && (MAX_RULES <= 0 || count < MAX_RULES)) {
		if (rules[i].len < MIN_PATTERN_LENGTH) {
			i++;
			continue;
		}
		enter(tree, &(rules[i]));
		i++;
		count++;
	}

	constructFailures(tree);

	getL2nums(tree);

	for (i = 0; i < numRules; i++) {
		if (rules[i].pattern) {
			free(rules[i].pattern);
		}
	}

	printf("+---------- AC DFA Info ----------+\n");
	printf("| Total rules: %20d |\n", numRules);
	printf("| Total states: %19d |\n", tree->size);
	printf("| Total bytes: %20d |\n", tree->size * 4);
	printf("+---------------------------------+\n");
}

void acDestroyNodesRecursive(Node *node) {
	Pair *pair;
	Node *child;
	HashMap *map = node->gotos;
	hashmap_iterator_reset(map);
	int i;

	while ((pair = (Pair*)hashmap_iterator_next(map)) != NULL) {
		child = pair->ptr;
		acDestroyNodesRecursive(child);
		free(pair);
	}
	hashmap_destroy(map);
	for (i = 0; i < MAX_RULES_PER_STATE; i++) {
		if (node->rules[i].pattern) {
			free(node->rules[i].pattern);
		}
	}
	free(node);
}

void acDestroyTreeNodes(ACTree *tree) {
	acDestroyNodesRecursive(tree->root);
}
