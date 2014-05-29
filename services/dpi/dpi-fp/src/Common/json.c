/*
 * json.c
 *
 *  Created on: May 26, 2014
 *      Author: yotamhc
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "json.h"

#define MAX_JSON_OBJECT_LEN 2048
#define MAX_FIELDS_IN_OBJECT 20
#define MAX_FIELD_NAME_LENGTH 128
#define MAX_FIELD_VALUE_LENGTH 1024

int parse_single_object(char *data, int len, void *result, json_parser parser, void **nextresult) {
	int i, j, level, start, end, inquote, pair_start, pair, eqsign, spsign;
	char quote_type;
	char ***pairs; //[MAX_FIELDS_IN_OBJECT][2][MAX_JSON_OBJECT_LEN];

	pairs = (char***)malloc(sizeof(char**) * MAX_FIELDS_IN_OBJECT);
	for (i = 0; i < MAX_FIELDS_IN_OBJECT; i++) {
		pairs[i] = (char**)malloc(sizeof(char*) * 2);
	}

	pair_start = -1;
	start = end = -1;
	inquote = 0;
	level = 0;
	pair = 0;

	for (i = 0; i < len; i++) {
		if (!inquote && data[i] == '{') {
			level++;
			if (level == 1)
				start = i;
			continue;
		} else if (!inquote && data[i] == '}') {
			level--;
			if (level == 0) {
				end = i;
			} else if (level < 0) {
				fprintf(stderr, "[JSON] ERROR: Invalid JSON data.\n");
				return -2;
			}
		} else if (!inquote && (data[i] == '"' || data[i] == '\'')) {
			inquote = 1;
			quote_type = data[i];
			continue;
		} else if (inquote && data[i] == quote_type) {
			inquote = 0;
			continue;
		} else if (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r') {
			continue;
		}
		if (inquote) {
			continue;
		} else if (pair_start == -1) {
			pair_start = i;
			continue;
		} else if (data[i] == ',' || end != -1) {
			if (pair == MAX_FIELDS_IN_OBJECT) {
				fprintf(stderr, "[JSON] ERROR: Too many fields in a single object. Recompile JSON with higher value for MAX_FIELDS_IN_OBJECT (current: %d).\n", MAX_FIELDS_IN_OBJECT);
				return -3;
			}
			eqsign = -1;
			spsign = -1;
			for (j = pair_start; j < i; j++) {
				if (data[j] == ':') {
					eqsign = j;
					break;
				} else if (data[j] == ' ' || data[j] == '\t' || data[j] == '\n' || data[j] == '\r') {
					spsign = j;
				} else if (data[j] == '"' || data[j] == '\'') {
					break;
				}
			}
			if (eqsign == -1) {
				fprintf(stderr, "[JSON] ERROR: Invalid JSON data.\n");
				return -2;
			}
			if (spsign == -1)
				spsign = eqsign;

			pairs[pair][0] = (char*)malloc(sizeof(char) * (j - pair_start + 1));
			strncpy(pairs[pair][0], &(data[pair_start]), j - pair_start);
			pair_start = -1;
			for (j = eqsign+1; j < i; j++) {
				if (pair_start == -1 && data[j] != ' ' && data[j] != '\t' && data[j] != '\n' && data[j] != '\r') {
					pair_start = j;
					break;
				}
			}
			if (pair_start == -1) {
				fprintf(stderr, "[JSON] ERROR: Invalid JSON data.\n");
				return -2;
			}
			for (j = i-1; j > pair_start; j--) {
				if (data[j] != ' ' && data[j] != '\t' && data[j] != '\n' && data[j] != '\r') {
					break;
				}
			}
			pairs[pair][1] = (char*)malloc(sizeof(char) * (j - pair_start + 2));
			strncpy(pairs[pair][1], &(data[pair_start]), j - pair_start + 1);
			pair_start = -1;
			pair++;
			if (end != -1) {
				break;
			}
			continue;
		}
	}
	if (inquote || (start == 0 && end <= 0)) {
		fprintf(stderr, "[JSON] ERROR: Invalid JSON data.\n");
		return -2;
	} else if (start < 0 && end < 0) {
		// No more data
		return 0;
	}

	(*nextresult) = parser(pairs, pair, result);
	return end + 1;
}

int parse_json_file(const char *path, json_parser parser, void *results) {
	FILE *f;
	char data[MAX_JSON_OBJECT_LEN];
	void *res_ptr;
	int len, last;
	int next, count;

	f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "[JSON] ERROR: Cannot open JSON file: %s\n", path);
		return -1;
	}

	res_ptr = results;
	last = 0;
	next = 0;
	count = 0;
	while (1) {
		len = fread(&(data[last]), 1, MAX_JSON_OBJECT_LEN - last, f) + last;
		if (len <= 0)
			break;

		next = parse_single_object(data, len, res_ptr, parser, &res_ptr);
		if (next < 0) {
			return next;
		}
		if (next == 0) {
			break;
		}
		count++;

		memcpy(data, &(data[next]), len - next);

		last = len - next;
	}

	fclose(f);
	return count;
}
