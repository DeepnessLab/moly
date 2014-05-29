/*
 * BitArray.h
 *
 *  Created on: Jan 11, 2011
 *      Author: yotamhc
 */

#ifndef BITARRAY_H_
#define BITARRAY_H_

#include "../Types.h"

#define   MASK_BITS_0	0x001
#define  MASK_BITS_01	0x003
#define MASK_BITS_012	0x007
#define MASK_BITS_123	0x00E
#define MASK_BITS_234	0x01C
#define MASK_BITS_345	0x038
#define MASK_BITS_456	0x070
#define MASK_BITS_567	0x0E0
#define MASK_BITS_67	0x0C0
#define MASK_BITS_7		0x080

#define GET_MATCH_BIT(value) ((value) & 0x01)
#define GET_PTR_TYPE(value) (((value) >> 1) & 0x03)
/*
#define GET_3BITS_ELEMENT(arr, i) \
		(((((3 * (i)) % 8) < 6) ? ((arr)[(3 * (i)) / 8] & MASK_BITS[(3 * (i)) % 8]) : 	\
				(((3 * (i)) % 8) == 6) ?											\
						(((arr)[(3 * (i)) / 8 + 1] & MASK_BITS_0) << 8) |							\
						 ((arr)[(3 * (i)) / 8] & MASK_BITS_67) :								\
							 (((3 * (i)) % 8) == 7) ?								\
										(((arr)[(3 * (i)) / 8 + 1] & MASK_BITS_01) << 8) |		\
										 ((arr)[(3 * (i)) / 8] & MASK_BITS_7) : 0) >> ((3 * (i)) % 8))
*/

inline uchar GET_3BITS_ELEMENT(uchar *arr, int i);

#ifdef COUNT_CALLS
int getCounter_BitArray();
#endif

inline void SET_3BITS_ELEMENT(uchar *arr, int i, uchar value);

#define GET_1BIT_ELEMENT(arr, i) (((arr)[(i) / 8] >> ((i) % 8)) & 0x01)

#define SET_1BIT_ELEMENT(arr, i, value) \
	(arr)[(i) / 8] = ((((arr)[(i) / 8]) & 0xFF) | (((value) & 0x01) << ((i) % 8)))

#define GET_2BITS_ELEMENT(arr, i) (((arr)[((i) / 4)] >> (2 * ((i) % 4))) & 0x03)

#define SET_2BITS_ELEMENT(arr, i, value) \
	(arr)[(i) / 4] = ((((arr)[(i) / 4]) & 0xFF) | (((value) & 0x03) << (2 * ((i) % 4))))

#endif //BITARRAY_H_
