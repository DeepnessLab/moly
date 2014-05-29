/*
 * ACBuilder.h
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#ifndef ACBUILDER_H_
#define ACBUILDER_H_

#include "ACTypes.h"
#include "../Common/Flags.h"

void acBuildTree(ACTree *tree, const char *path, int avoidFailToLeaves, int mixIDs);
void acDestroyTreeNodes(ACTree *tree);
Node *acGetNextNode(Node *node, char c);
void acPrintTree(ACTree *tree);
void acReorderStates(ACTree *tree, int *old2new);
#ifdef FIND_MC2_BAD_PATH
int acFindBadMC2Path(ACTree *tree, int *path_states, char *path_chars, int max_size);
int acCountUncommonStates(int *path_states, int size);
#endif

#endif /* ACBUILDER_H_ */
