/* $Id$ */
/****************************************************************************
 *
 * Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 * Copyright (C) 2011-2013 Sourcefire, Inc.
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

//--------------------------------------------------------------------
// s5 protocol aware flushing stuff
//
// @file    stream5_paf.h
// @author  Russ Combs <rcombs@sourcefire.com>
//--------------------------------------------------------------------

#ifndef __STREAM5_PAF_H__
#define __STREAM5_PAF_H__

#include "sf_types.h"
#include "sfPolicy.h"
#include "stream_api.h"

void* s5_paf_new(void);     // create new paf config (per policy)
void s5_paf_delete(void*);  // free config

bool s5_paf_register_port(
    struct _SnortConfig *, // snort configuration
    tSfPolicyId,     // applicable policy
    uint16_t port,   // server port
    bool toServer,   // direction of interest relative to server port
    PAF_Callback,    // stateful byte scanning function
    bool autoEnable  // enable PAF reassembly regardless of s5 ports config
);

bool s5_paf_register_service(
    struct _SnortConfig *, // snort configuration
    tSfPolicyId,      // same as above
    uint16_t service, // service ordinal
    bool toServer,
    PAF_Callback,
    bool autoEnable
);

// flush indicates s5 port config
uint8_t s5_paf_port_registration (void* pv, uint16_t port, bool c2s, bool flush);
uint8_t s5_paf_service_registration (void* pv, uint16_t service, bool c2s, bool flush);

typedef struct {
    void* user;      // arbitrary user data

    uint32_t seq;    // stream cursor
    uint32_t pos;    // last flush position

    uint32_t fpt;    // current flush point
    uint32_t tot;    // total bytes flushed

    PAF_Status paf;  // current scan state
    uint8_t cb_mask; // callback mask
} PAF_State;         // per session direction

// called at session start
void s5_paf_setup(PAF_State*, uint8_t registration);
void s5_paf_clear(PAF_State*);  // called at session end

static inline uint32_t s5_paf_position (PAF_State* ps)
{
    return ps->seq;
}

static inline uint32_t s5_paf_initialized (PAF_State* ps)
{
    return ( ps->paf != PAF_START );
}

static inline uint32_t s5_paf_active (PAF_State* ps)
{
    return ( ps->paf != PAF_ABORT );
}

// called on each in order segment
uint32_t s5_paf_check(
    void* paf_config, PAF_State*, void* ssn,
    const uint8_t* data, uint32_t len, uint32_t total,
    uint32_t seq, uint16_t port, uint32_t* flags, uint32_t fuzz);

#endif

