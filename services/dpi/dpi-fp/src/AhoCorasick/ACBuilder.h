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

int acBuildTree(ACTree *tree, const char *path, int max_rules);
void acDestroyTreeNodes(ACTree *tree);
Node *acGetNextNode(Node *node, char c);
void acPrintTree(ACTree *tree);

#endif /* ACBUILDER_H_ */
