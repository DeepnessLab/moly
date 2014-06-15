/*
 * MatchReport.h
 *
 *  Created on: Jun 4, 2014
 *      Author: yotamhc
 */

#ifndef MATCHREPORT_H_
#define MATCHREPORT_H_

typedef struct {
	int rid;
	int startIdxInPacket;
	int startIdxInFlow;
} MatchReport;


#endif /* MATCHREPORT_H_ */
