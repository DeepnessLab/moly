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

// @file    active.h
// @author  Russ Combs <rcombs@sourcefire.com>

#ifndef __ACTIVE_H__
#define __ACTIVE_H__

#include "decode.h"
#include "snort.h"

#ifdef ACTIVE_RESPONSE
#include "encode.h"

int Active_Init(SnortConfig*);
int Active_Term(void);

typedef void (*Active_ResponseFunc)(Packet*, void* data);

int Active_QueueReject(void);
int Active_QueueResponse(Active_ResponseFunc, void*);
int Active_ResetQueue(void);

// this must be called on the wire packet and not a
// reassembled packet so that encoding is correct.
int Active_SendResponses(Packet*);
uint64_t Active_GetInjects(void);

// NULL flags implies ENC_FLAG_FWD
void Active_KillSession(Packet*, EncodeFlags*);

void Active_SendReset(Packet*, EncodeFlags);
void Active_SendUnreach(Packet*, EncodeType);
void Active_SendData(Packet*, EncodeFlags, const uint8_t* buf, uint32_t len);
void Active_InjectData(Packet*, EncodeFlags, const uint8_t* buf, uint32_t len);

int Active_IsRSTCandidate(const Packet*);
int Active_IsUNRCandidate(const Packet*);

int Active_IsEnabled(void);
void Active_SetEnabled(int on_off);

#endif // ACTIVE_RESPONSE

typedef enum {
    ACTIVE_ALLOW,       // don't drop
    ACTIVE_CANT_DROP,   // can't drop
    ACTIVE_WOULD_DROP,  // would drop
    ACTIVE_DROP,        // should drop
    ACTIVE_FORCE_DROP   // must drop
} tActiveDrop;

extern tActiveDrop active_drop_pkt;
extern int active_drop_ssn;
#ifdef ACTIVE_RESPONSE
extern int active_have_rsp;
#endif
extern int active_tunnel_bypass;
extern int active_suspend;

static inline void Active_Reset (void)
{
    active_drop_pkt = ACTIVE_ALLOW;
    active_drop_ssn = 0;
#ifdef ACTIVE_RESPONSE
    active_have_rsp = 0;
#endif
    active_tunnel_bypass = 0;
}

static inline void Active_Suspend (void)
{
    active_suspend = 1;
}

static inline void Active_Resume (void)
{
    active_suspend = 0;
}

static inline bool Active_Suspended (void)
{
    return ( active_suspend != 0 );
}

static tActiveDrop Active_GetDisposition (void)
{
    return active_drop_pkt;
}

static inline void Active_CantDrop(void)
{
#if 0
    // not yet supported
    if ( active_drop_pkt < ACTIVE_CANT_DROP )
        active_drop_pkt = ACTIVE_CANT_DROP;
#else
    if ( active_drop_pkt < ACTIVE_WOULD_DROP )
        active_drop_pkt = ACTIVE_WOULD_DROP;
#endif
}

static inline void Active_ForceDropPacket (void)
{
    if ( Active_Suspended() )
        Active_CantDrop();

    else
        active_drop_pkt = ACTIVE_FORCE_DROP;
}

static inline void Active_DropPacket (const Packet* p)
{
    if ( Active_Suspended() )
    {
        Active_CantDrop();
    }
    else if ( active_drop_pkt != ACTIVE_FORCE_DROP )
    {
        if ( ScInlineMode() )
        {
            if ( DAQ_GetInterfaceMode(p->pkth) == DAQ_MODE_INLINE )
                active_drop_pkt = ACTIVE_DROP;
            else
                active_drop_pkt = ACTIVE_WOULD_DROP;
        }
        else if (ScInlineTestMode())
        {
            active_drop_pkt = ACTIVE_WOULD_DROP;
        }
    }
}

static inline void Active_DropSession (const Packet* p)
{
    if ( Active_Suspended() )
    {
        Active_CantDrop();
        return;
    }
    active_drop_ssn = 1;
    Active_DropPacket(p);
}

static inline void Active_ForceDropSession (void)
{
    if ( Active_Suspended() )
    {
        Active_CantDrop();
        return;
    }
    active_drop_ssn = 1;
    Active_ForceDropPacket();
}

static inline int Active_PacketWouldBeDropped (void)
{
    return (active_drop_pkt == ACTIVE_WOULD_DROP );
}

static inline int Active_PacketForceDropped (void)
{
    return (active_drop_pkt == ACTIVE_FORCE_DROP );
}

static inline int Active_PacketWasDropped (void)
{
    return ( active_drop_pkt >= ACTIVE_DROP );
}

static inline int Active_SessionWasDropped (void)
{
    return ( active_drop_ssn != 0 );
}

#ifdef ACTIVE_RESPONSE
static inline int Active_ResponseQueued (void)
{
    return ( active_have_rsp != 0 );
}
#endif

static inline void Active_SetTunnelBypass (void)
{
    active_tunnel_bypass++;
}

static inline void Active_ClearTunnelBypass (void)
{
    active_tunnel_bypass--;
}

static inline int Active_GetTunnelBypass (void)
{
    return ( active_tunnel_bypass > 0 );
}

// drops current session with active response invoked
// for rules with action = drop | sdrop | reject
int Active_DropAction(Packet*);

// drops current session w/o active response invoked
// for rules with custom response = resp3 | react
int Active_IgnoreSession(Packet*);

// force drops the current session w/o active response invoked
// ignores policy/inline test mode and treat drop as alert
int Active_ForceDropAction(Packet *p);

// force drops the current session with active response invoked
// ignores policy/inline test mode and treat drop as alert
int Active_ForceDropResetAction(Packet *p);

const char* Active_GetDispositionString();

#endif // __ACTIVE_H__

