/*
 * Types.h
 *
 *  Created on: Jan 11, 2011
 *      Author: yotamhc
 */

#ifndef TYPES_H_
#define TYPES_H_
#include "Flags.h"

#define TRUE 1
#define FALSE 0

#define STATE_TYPE_LINEAR_ENCODED 	0x00
#define STATE_TYPE_BITMAP_ENCODED 	0x01
#define STATE_TYPE_LOOKUP_TABLE 	0x02
#define STATE_TYPE_PATH_COMPRESSED 	0x03

#define PTR_TYPE_REGULAR 0x0
#define PTR_TYPE_LEVEL0 0x1
#define PTR_TYPE_LEVEL1 0x2
#define PTR_TYPE_LEVEL2 0x3

#define MAX_NODE_DEGREE 64
#define MAX_STATE_ID 65536

#define PTR_SIZE 2

typedef unsigned char uchar;
typedef unsigned short STATE_PTR_TYPE;
typedef unsigned int STATE_PTR_TYPE_WIDE;
typedef unsigned char StateType;
typedef uchar State;

typedef struct {
	State *next;
	unsigned int match : 1;
	unsigned int fail : 7;
	char *pattern;
} NextStateResult;

typedef struct {
	unsigned int type : 2;
	unsigned int size : 6;
} StateHeader;

typedef struct {
	State **table;
	int size;
	int nextID;
} StateTable;

#define GET_HEADER_AS_BYTE0(header) (((header).type & 0x03) | (((header).size & 0x03F) << 2))

#endif /* TYPES_H_ */
