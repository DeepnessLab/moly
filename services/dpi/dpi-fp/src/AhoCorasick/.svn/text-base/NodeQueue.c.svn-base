/*
 * NodeQueue.c
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#include <stdlib.h>
#include "NodeQueue.h"

void nodequeue_init(NodeQueue *queue) {
	queue->first = NULL;
	queue->last = NULL;
}

void nodequeue_destroy_elements(NodeQueue *queue, int destroyData) {
	QueueElement *element;
	while(queue->first) {
		element = queue->first;
		queue->first = element->next;
		if (destroyData) {
			free(element->node);
		}
		free(element);
	}
}

void nodequeue_enqueue(NodeQueue *queue, Node *node) {
	QueueElement *elem = (QueueElement*)malloc(sizeof(QueueElement));
	elem->node = node;
	elem->next = NULL;

	if (queue->last) {
		queue->last->next = elem;
		queue->last = elem;
	} else {
		queue->first = queue->last = elem;
	}
}

Node *nodequeue_head(NodeQueue *queue) {
	QueueElement *elem;
	Node *node;

	if (queue->first) {
		elem = queue->first;
		node = elem->node;
		return node;
	} else {
		return NULL;
	}
}

Node *nodequeue_dequeue(NodeQueue *queue) {
	QueueElement *elem;
	Node *node;

	if (queue->first) {
		elem = queue->first;
		queue->first = elem->next;
		if (queue->first == NULL) {
			queue->last = NULL;
		}
		node = elem->node;
		free(elem);
		return node;
	} else {
		return NULL;
	}
}

int nodequeue_isempty(NodeQueue *queue) {
	return (queue->first == NULL);
}
