/*
 * BitArray.c
 *
 *  Created on: Jan 11, 2011
 *      Author: yotamhc
 */

#include "BitArray.h"

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

static const uchar MASK_BITS[] = {
		MASK_BITS_012,
		MASK_BITS_123,
		MASK_BITS_234,
		MASK_BITS_345,
		MASK_BITS_456,
		MASK_BITS_567
};

static const unsigned char POW2[] = { 0, 2, 4, 8, 16, 32, 64, 128 };

#ifdef COUNT_CALLS
static int counter = 0;
#endif

inline uchar GET_3BITS_ELEMENT(uchar *arr, int i) {
	int bit = (3 * i) % 8;
	int byte = (3 * (i)) / 8;

#ifdef COUNT_CALLS
	counter++;
#endif

	return (((arr[byte] >> bit) % (POW2[min(8 - bit, 3)]))) |
			((bit > 5) ? (((arr[byte + 1]) % (POW2[bit - 5])) << (max(8 - bit, 0))) : 0);
}

#ifdef COUNT_CALLS
int getCounter_BitArray() {
	return counter;
}
#endif

inline void SET_3BITS_ELEMENT(uchar *arr, int i, uchar value) {
	int bit = (3 * (i)) % 8;
	int byte = (3 * (i)) / 8;

	if (bit < 6) {
		arr[byte] = arr[byte] | (((uchar)(value << bit)) & MASK_BITS[bit]);
	} else if (bit == 6) {
		arr[byte] = arr[byte] | (((uchar)(value << 6)) & MASK_BITS_67);
		arr[byte + 1] = arr[byte + 1] | (((uchar)((value << 6) >> 8)) & MASK_BITS_0);
	} else if (bit == 7) {
		arr[byte] = arr[byte] | (((uchar)(value << 7)) & MASK_BITS_7);
		arr[byte + 1] = arr[byte + 1] | (((uchar)((value << 7) >> 8)) & MASK_BITS_01);
	}
}
