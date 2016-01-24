/*
 * DpiSrv.h
 *
 *  Created on: Oct 5, 2014
 *      Author: yotamhc
 */

#ifndef DPISRV_H_
#define DPISRV_H_

#define USE_DPI_SRV

#define IP_TOS_HAS_MATCHES_MASK 0xC0

#include "../decode.h"

int mpseSearchDpiSrv( Packet *p, void *pvoid, const unsigned char * T, int n,
                int ( *action )(void* id, void * tree, int index, void *data, void *neg_list),
                void * data, int* current_state );


#endif /* DPISRV_H_ */
