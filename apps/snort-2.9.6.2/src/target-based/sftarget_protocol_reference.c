/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
** Copyright (C) 2006-2013 Sourcefire, Inc.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
 * Author: Steven Sturges
 * sftarget_protocol_reference.c
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef TARGET_BASED

#include "sftarget_protocol_reference.h"
#include "sfutil/sfghash.h"

#include "log.h"
#include "util.h"
#include "snort_debug.h"

#include "stream_api.h"
#include "spp_frag3.h"
#include "sftarget_reader.h"
#include "sftarget_hostentry.h"

int16_t protocolReferenceTCP;
int16_t protocolReferenceUDP;
int16_t protocolReferenceICMP;

static SFGHASH *proto_reference_table = NULL;
static int16_t protocol_number = 1;

static char *standard_protocols[] =
{
    /* Transport Protocols */
    "ip",
    "tcp",
    "udp",
    "icmp",
    /* Application Protocols */
    "http",
    "ftp",
    "telnet",
    "smtp",
    "ssh",
    "dcerpc",
    "netbios-dgm",
    "netbios-ns",
    "netbios-ssn",
    "nntp",
    "dns",
    "isakmp",
    "finger",
    "imap",
    "oracle",
    "pop2",
    "pop3",
    "snmp",
    "tftp",
    "x11",
    "ftp-data",
    NULL
};

/* XXX XXX Probably need to do this during swap time since the
 * proto_reference_table is accessed during runtime */
int16_t AddProtocolReference(const char *protocol)
{
    SFTargetProtocolReference *reference;

    if (!protocol)
        return SFTARGET_UNKNOWN_PROTOCOL;

    if (!proto_reference_table)
    {
        InitializeProtocolReferenceTable();
    }

    reference = sfghash_find(proto_reference_table, (void *)protocol);
    if (reference)
    {
        DEBUG_WRAP(
            DebugMessage(DEBUG_ATTRIBUTE,
                "Protocol Reference for %s exists as %d\n",
                protocol, reference->ordinal););
        return reference->ordinal;
    }

    reference = SnortAlloc(sizeof(SFTargetProtocolReference));
    reference->ordinal = protocol_number++;
    if (protocol_number > MAX_PROTOCOL_ORDINAL)
    {
        /* XXX: If we see this warning message, should
        * increase MAX_PROTOCOL_ORDINAL definition.  The ordinal is
        * stored as a signed 16bit int, so it can be increased upto
        * 32k without requiring a change in space.  It is currently
        * defined as 8192.
        */
        LogMessage("WARNING: protocol_number wrapped.   This may result"
                   "in odd behavior and potential false positives.\n");

        /* 1 is the first protocol id we use. */
        /* 0 is not used */
        /* -1 means unknwon */
        protocol_number = 1;
    }
    SnortStrncpy(reference->name, protocol, STD_BUF);

    sfghash_add(proto_reference_table, reference->name, reference);

    DEBUG_WRAP(
            DebugMessage(DEBUG_ATTRIBUTE,
                "Added Protocol Reference for %s as %d\n",
            protocol, reference->ordinal););

    return reference->ordinal;
}

int16_t FindProtocolReference(const char *protocol)
{
    SFTargetProtocolReference *reference;

    if (!protocol)
        return SFTARGET_UNKNOWN_PROTOCOL;

    if (!proto_reference_table)
    {
        InitializeProtocolReferenceTable();
    }

    reference = sfghash_find(proto_reference_table, (void *)protocol);

    if (reference)
        return reference->ordinal;

    return SFTARGET_UNKNOWN_PROTOCOL;
}

void InitializeProtocolReferenceTable(void)
{
    char **protocol;

    /* If already initialized, we're done */
    if (proto_reference_table)
        return;

    proto_reference_table = sfghash_new(65, 0, 1, free);

    if (!proto_reference_table)
    {
        FatalError("Failed to Initialize Target-Based Protocol Reference Table\n");
    }

    /* Initialize the standard protocols from the list above */
    for (protocol = standard_protocols; *protocol; protocol++)
    {
        AddProtocolReference(*protocol);
    }
    protocolReferenceTCP = FindProtocolReference("tcp");
    protocolReferenceUDP = FindProtocolReference("udp");
    protocolReferenceICMP = FindProtocolReference("icmp");
}

void FreeProtoocolReferenceTable(void)
{
    sfghash_delete(proto_reference_table);
    proto_reference_table = NULL;
}

int16_t GetProtocolReference(Packet *p)
{
    int16_t protocol = 0;
    int16_t ipprotocol = 0;

    if (!p)
        return protocol;

    if (p->application_protocol_ordinal != 0)
        return p->application_protocol_ordinal;

    do /* Simple do loop to break out of quickly, not really a loop */
    {
        HostAttributeEntry *host_entry;
        if (p->ssnptr && stream_api)
        {
            /* Use session information via Stream API */
            protocol = stream_api->get_application_protocol_id(p->ssnptr);
            if (protocol != 0)
            {
                break;
            }
        }

        if (p->fragtracker)
        {
            protocol = fragGetApplicationProtocolId(p);
            /* Use information from frag tracker */
            if (protocol != 0)
            {
                break;
            }
        }

        switch (GET_IPH_PROTO(p))
        {
        case IPPROTO_TCP:
            ipprotocol = protocolReferenceTCP;
            break;
        case IPPROTO_UDP:
            ipprotocol = protocolReferenceUDP;
            break;
        case IPPROTO_ICMP:
            ipprotocol = protocolReferenceICMP;
            break;
        }

        /* Lookup the destination host to find the protocol for the
         * destination port
         */
        host_entry = SFAT_LookupHostEntryByDst(p);
        if (host_entry)
        {
            protocol = getApplicationProtocolId(host_entry,
                            ipprotocol,
                            p->dp,
                            SFAT_SERVICE);
        }

        if (protocol != 0)
        {

            break;
        }

        /* If not found, do same for src host/src port. */
        host_entry = SFAT_LookupHostEntryBySrc(p);
        if (host_entry)
        {
            protocol = getApplicationProtocolId(host_entry,
                            ipprotocol,
                            p->sp,
                            SFAT_SERVICE);
        }
        if (protocol != 0)
        {
            break;
        }

    } while (0); /* Simple do loop to break out of quickly, not really a loop */

    /* Store it to alleviate future lookups */
    p->application_protocol_ordinal = protocol;

    return protocol;
}

#endif /* TARGET_BASED */
