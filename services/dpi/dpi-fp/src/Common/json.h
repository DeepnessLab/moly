/*
 * json.h
 *
 *  Created on: May 26, 2014
 *      Author: yotamhc
 */

#ifndef JSON_H_
#define JSON_H_

typedef void *(*json_parser)(char ***pairs, int numPairs, void *result);
typedef void (*test_func)();

int parse_json_file(const char *path, json_parser parser, void *results);

#endif /* JSON_H_ */
