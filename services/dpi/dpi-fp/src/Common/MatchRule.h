/*
 * MatchRule.h
 *
 *  Created on: May 22, 2014
 *      Author: yotamhc
 */

#ifndef MATCHRULE_H_
#define MATCHRULE_H_

#define MAX_PATTERN_LENGTH 1024

typedef struct {
	char *pattern;
	int len;
	int is_regex;
	unsigned int rid;
} MatchRule;

#endif /* MATCHRULE_H_ */
