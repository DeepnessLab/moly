/*
 * NodeQueue.h
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#ifndef NODEQUEUE_H_
#define NODEQUEUE_H_

#include "ACTypes.h"

typedef struct st_queueelement {
	Node *node;
	struct st_queueelement *next;
} QueueElement;

typedef struct {
	QueueElement *first;
	QueueElement *last;
} NodeQueue;

void nodequeue_init(NodeQueue *queue);
void nodequeue_destroy_elements(NodeQueue *queue, int destroyData);
void nodequeue_enqueue(NodeQueue *queue, Node *node);
Node *nodequeue_head(NodeQueue *queue);
Node *nodequeue_dequeue(NodeQueue *queue);
int nodequeue_isempty(NodeQueue *queue);

#endif /* NODEQUEUE_H_ */
