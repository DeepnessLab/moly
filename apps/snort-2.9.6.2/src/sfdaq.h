/* $Id$ */
/****************************************************************************
 *
 * Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 * Copyright (C) 2005-2013 Sourcefire, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.  You may not use, modify or
 * distribute this program under any other version of the GNU General
 * Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 ****************************************************************************/

// @file    sfdaq.h
// @author  Russ Combs <rcombs@sourcefire.com>

#ifndef __DAQ_H__
#define __DAQ_H__

#include <stdio.h>
#include <daq.h>

#define PKT_TIMEOUT  1000  // ms, worst daq resolution is 1 sec

struct _SnortConfig;

void DAQ_Load(const struct _SnortConfig*);
void DAQ_Unload(void);

void DAQ_Init(const struct _SnortConfig*);
void DAQ_Term(void);
void DAQ_Abort(void);

int DAQ_PrintTypes(FILE*);
const char* DAQ_GetType(void);

int DAQ_Unprivileged(void);
int DAQ_UnprivilegedStart(void);
int DAQ_CanReplace(void);
int DAQ_CanInject(void);
int DAQ_CanWhitelist(void);
int DAQ_RawInjection(void);

const char* DAQ_GetInterfaceSpec(void);
uint32_t DAQ_GetSnapLen(void);
int DAQ_GetBaseProtocol(void);
int DAQ_SetFilter(const char*);

// total stats are accumulated when daq is deleted
int DAQ_New(const struct _SnortConfig*, const char* intf);
int DAQ_Delete(void);

int DAQ_Start(void);
int DAQ_WasStarted(void);
int DAQ_Stop(void);

// TBD some stuff may be inlined once encapsulations are straight
// (but only where performance justifies exposing implementation!)
int DAQ_Acquire(int max, DAQ_Analysis_Func_t, uint8_t* user);
int DAQ_Inject(const DAQ_PktHdr_t*, int rev, const uint8_t* buf, uint32_t len);
int DAQ_BreakLoop(int error);
#ifdef HAVE_DAQ_ACQUIRE_WITH_META
void DAQ_Set_MetaCallback(DAQ_Meta_Func_t meta_callback);
#endif
DAQ_Mode DAQ_GetInterfaceMode(const DAQ_PktHdr_t *h);

int DAQ_ModifyFlow(const void* h, uint32_t id);

#ifdef HAVE_DAQ_ADDRESS_SPACE_ID
static inline uint16_t DAQ_GetAddressSpaceID(const DAQ_PktHdr_t *h)
{
    return h->address_space_id;
}
#endif

// returns total stats if no daq else current stats
// returns statically allocated stats - don't free
const DAQ_Stats_t* DAQ_GetStats(void);

#endif // __DAQ_H__

