/* $Id$ */
/****************************************************************************
 *
 * Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 * Copyright (C) 2012-2013 Sourcefire, Inc.
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
 *****************************************************************************/

/**************************************************************************
 *
 * stream5_ha.c
 *
 * Authors: Michael Altizer <maltizer@sourcefire.com>, Russ Combs <rcombs@sourcefire.com>
 *
 * Description:
 *
 * Stream5 high availability support.
 *
 **************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "mstring.h"
#include "packet_time.h"
#include "parser.h"
#include "sfcontrol_funcs.h"
#ifdef SIDE_CHANNEL
#include "sidechannel.h"
#endif
#include "stream5_ha.h"
#include "util.h"

/*
 * Stream5 HA messages will have the following format:
 *
 * <message>  ::= <header> <has-rec> <psd-rec>
 * <header>   ::= <event> version <message length> <key>
 * <event>    ::= HA_EVENT_UPDATE | HA_EVENT_DELETE
 * <key>      ::= <ipv4-key> | <ipv6-key>
 * <ipv4-key> ::= HA_TYPE_KEY sizeof(ipv4-key) ipv4-key
 * <ipv6-key> ::= HA_TYPE_KEY sizeof(ipv6-key) ipv6-key
 * <has-rec>  ::= HA_TYPE_HAS sizeof(has-rec) has-rec | (null)
 * <psd-rec>  ::= HA_TYPE_PSD sizeof(psd-rec) psd-preprocid psd-subcode psd-rec <psd-rec> | (null)
 */

typedef struct _StreamHAFuncsNode
{
    uint16_t id;
    uint16_t mask;
    uint8_t preproc_id;
    uint8_t subcode;
    uint8_t size;
    StreamHAProducerFunc produce;
    StreamHAConsumerFunc consume;
    uint32_t produced;
    uint32_t consumed;
} StreamHAFuncsNode;

typedef enum
{
    HA_TYPE_KEY,    // Lightweight Session Key
    HA_TYPE_HAS,    // Lightweight Session Data
    HA_TYPE_PSD,    // Preprocessor-specific Data
    HA_TYPE_MAX
} HA_Type;

typedef struct _MsgHeader
{
    uint8_t event;
    uint8_t version;
    uint16_t total_length;
    uint8_t key_type;
    uint8_t key_size;
} MsgHeader;

typedef struct _RecordHeader
{
    uint8_t type;
    uint8_t length;
} RecordHeader;

typedef struct _PreprocDataHeader
{
    uint8_t preproc_id;
    uint8_t subcode;
} PreprocDataHeader;

/* Something more will probably be added to this structure in the future... */
#define HA_SESSION_FLAG_LOW     0x01     // client address / port is low in key
#define HA_SESSION_FLAG_IP6     0x02     // key addresses are ip6
typedef struct
{
    Stream5HAState ha_state;
    uint8_t flags;
} Stream5HASession;

typedef struct
{
    uint32_t update_messages_received;
    uint32_t update_messages_received_no_session;
    uint32_t delete_messages_received;
    uint32_t update_messages_sent_immediately;
    uint32_t update_messages_sent_normally;
    uint32_t delete_messages_sent;
    uint32_t delete_messages_not_sent;
} Stream5HAStats;

typedef struct _HADebugSessionConstraints
{
    sfip_t sip;
    sfip_t dip;
    uint16_t sport;
    uint16_t dport;
    uint8_t protocol;
} HADebugSessionConstraints;

#define MAX_STREAM_HA_FUNCS 8  // depends on sizeof(Stream5LWSession.ha_pending_mask)
#define HA_MESSAGE_VERSION  0x81
static StreamHAFuncsNode *stream_ha_funcs[MAX_STREAM_HA_FUNCS];
static int n_stream_ha_funcs = 0;
static int runtime_output_fd = -1;
static uint8_t file_io_buffer[UINT16_MAX];
static Stream5HAStats s5ha_stats;

/* Runtime debugging stuff. */
#define HA_DEBUG_SESSION_ID_SIZE    (39+1+5+5+39+1+5+1+3+1) /* "<IPv6 address>:<port> <-> <IPv6 address>:<port> <ipproto>\0" */
static HADebugSessionConstraints s5_ha_debug_info;
static volatile int s5_ha_debug_flag = 0;
static char s5_ha_debug_session[HA_DEBUG_SESSION_ID_SIZE];

#define IP6_SESSION_KEY_SIZE sizeof(SessionKey)
#define IP4_SESSION_KEY_SIZE (IP6_SESSION_KEY_SIZE - 24)

#ifdef PERF_PROFILING
PreprocStats s5HAPerfStats;
PreprocStats s5HAConsumePerfStats;
PreprocStats s5HAProducePerfStats;
#endif

//--------------------------------------------------------------------
//  Runtime debugging support.
//--------------------------------------------------------------------
static inline bool Stream5HADebugCheck(const SessionKey *key, volatile int debug_flag,
                                        HADebugSessionConstraints *info, char *debug_session, size_t debug_session_len)
{
#ifndef REG_TEST
    if (debug_flag)
    {
        if ((!info->protocol || info->protocol == key->protocol) &&
                (((!info->sport || info->sport == key->port_l) &&
                  (!sfip_is_set(&info->sip) || memcmp(&info->sip.ip, key->ip_l, sizeof(info->sip.ip)) == 0) &&
                  (!info->dport || info->dport == key->port_h) &&
                  (!sfip_is_set(&info->dip) || memcmp(&info->dip.ip, key->ip_h, sizeof(info->dip.ip)) == 0)) ||
                 ((!info->sport || info->sport == key->port_h) &&
                  (!sfip_is_set(&info->sip) || memcmp(&info->sip.ip, key->ip_h, sizeof(info->sip.ip)) == 0) &&
                  (!info->dport || info->dport == key->port_l) &&
                  (!sfip_is_set(&info->dip) || memcmp(&info->dip.ip, key->ip_l, sizeof(info->dip.ip)) == 0))))
        {
#endif
            int af;
            char lipstr[INET6_ADDRSTRLEN];
            char hipstr[INET6_ADDRSTRLEN];

            if (!key->ip_l[1] && !key->ip_l[2] && !key->ip_l[3] && !key->ip_h[1] && !key->ip_h[2] && !key->ip_h[3])
                af = AF_INET;
            else
                af = AF_INET6;

            lipstr[0] = '\0';
            sfip_raw_ntop(af, key->ip_l, lipstr, sizeof(lipstr));
            hipstr[0] = '\0';
            sfip_raw_ntop(af, key->ip_h, hipstr, sizeof(hipstr));
            snprintf(debug_session, debug_session_len, "%s:%hu <-> %s:%hu %hhu",
                    lipstr, key->port_l, hipstr, key->port_h, key->protocol);
            return true;
#ifndef REG_TEST
        }
    }

    return false;
#endif
}

static void Stream5HADebugParse(const char *desc, const uint8_t *data, uint32_t length,
        volatile int *debug_flag, HADebugSessionConstraints *info)
{
    *debug_flag = 0;
    memset(info, 0, sizeof(*info));
    do
    {
        if (length >= sizeof(info->protocol))
        {
            info->protocol = *(uint8_t *)data;
            length -= sizeof(info->protocol);
            data += sizeof(info->protocol);
        }
        else
            break;

        if (length >= sizeof(info->sip.ip))
        {
            if (memcmp(data + 4, info->sip.ip8 + 4, 12) == 0)
            {
                if (memcmp(data, info->sip.ip8, 4) != 0)
                    sfip_set_raw(&info->sip, (void *) data, AF_INET);
            }
            else
                sfip_set_raw(&info->sip, (void *) data, AF_INET6);
            length -= sizeof(info->sip.ip);
            data += sizeof(info->sip.ip);
        }
        else
            break;

        if (length >= sizeof(info->sport))
        {
            info->sport = *(uint16_t *)data;
            length -= sizeof(info->sport);
            data += sizeof(info->sport);
        }
        else
            break;

        if (length >= sizeof(info->dip.ip))
        {
            if (memcmp(data + 4, info->dip.ip8 + 4, 12) == 0)
            {
                if (memcmp(data, info->dip.ip8, 4) != 0)
                    sfip_set_raw(&info->dip, (void *) data, AF_INET);
            }
            else
                sfip_set_raw(&info->dip, (void *) data, AF_INET6);
            length -= sizeof(info->dip.ip);
            data += sizeof(info->dip.ip);
        }
        else
            break;

        if (length >= sizeof(info->dport))
        {
            info->dport = *(uint16_t *)data;
            length -= sizeof(info->dport);
            data += sizeof(info->dport);
        }
        else
            break;
    } while (0);

    if (info->protocol || sfip_is_set(&info->sip) || info->sport || sfip_is_set(&info->dip) || info->dport)
    {
        char sipstr[INET6_ADDRSTRLEN];
        char dipstr[INET6_ADDRSTRLEN];

        sipstr[0] = '\0';
        if (sfip_is_set(&info->sip))
            sfip_ntop(&info->sip, sipstr, sizeof(sipstr));
        else
            snprintf(sipstr, sizeof(sipstr), "any");

        dipstr[0] = '\0';
        if (sfip_is_set(&info->dip))
            sfip_ntop(&info->dip, dipstr, sizeof(dipstr));
        else
            snprintf(dipstr, sizeof(dipstr), "any");

        LogMessage("Debugging %s with %s-%hu and %s-%hu %hhu\n", desc,
                    sipstr, info->sport, dipstr, info->dport, info->protocol);
        *debug_flag = 1;
    }
    else
        LogMessage("Debugging %s disabled\n", desc);
}

static int Stream5DebugHA(uint16_t type, const uint8_t *data, uint32_t length, void **new_context, char *statusBuf, int statusBuf_len)
{
    Stream5HADebugParse("S5HA", data, length, &s5_ha_debug_flag, &s5_ha_debug_info);

    return 0;
}

//--------------------------------------------------------------------
// Protocol-specific HA API
// could use an array here (and an enum instead of IPPROTO_*)
//--------------------------------------------------------------------

static const HA_Api *s_tcp = NULL;
static const HA_Api *s_udp = NULL;
static const HA_Api *s_ip = NULL;

int ha_set_api(unsigned proto, const HA_Api *api)
{
    switch (proto)
    {
        case IPPROTO_TCP:
            s_tcp = api;
            break;
        case IPPROTO_UDP:
            s_udp = api;
            break;
        case IPPROTO_IP:
            s_ip = api;
            break;
        default:
            return -1;
    }
    return 0;
}

static inline const HA_Api *ha_get_api(unsigned proto)
{
    switch (proto)
    {
        case IPPROTO_TCP:
            return s_tcp;
        case IPPROTO_UDP:
            return s_udp;
        case IPPROTO_ICMP:
        case IPPROTO_IP:
        default:
            return s_ip;
    }
    return NULL;
}

int RegisterStreamHAFuncs(uint32_t preproc_id, uint8_t subcode, uint8_t size,
                            StreamHAProducerFunc produce, StreamHAConsumerFunc consume)
{
    StreamHAFuncsNode *node;
    int i, idx;

    if (produce == NULL || consume == NULL)
    {
        FatalError("One must be both a producer and a consumer to participate in Stream5 HA!\n");
    }

    if (preproc_id > UINT8_MAX)
    {
        FatalError("Preprocessor ID must be between 0 and %d to participate in Stream5 HA!\n", UINT8_MAX);
    }

    idx = n_stream_ha_funcs;
    for (i = 0; i < n_stream_ha_funcs; i++)
    {
        node = stream_ha_funcs[i];
        if (node)
        {
            if (preproc_id == node->preproc_id && subcode == node->subcode)
            {
                FatalError("Duplicate Stream5 HA registration attempt for preprocessor %hu with subcode %hu\n",
                           node->preproc_id, node->subcode);
            }
        }
        else if (idx == n_stream_ha_funcs)
            idx = i;
    }

    if (idx == MAX_STREAM_HA_FUNCS)
    {
        FatalError("Attempted to register more than %d Stream5 HA types!\n", MAX_STREAM_HA_FUNCS);
    }

    if (idx == n_stream_ha_funcs)
        n_stream_ha_funcs++;

    node = (StreamHAFuncsNode *) SnortAlloc(sizeof(StreamHAFuncsNode));
    node->id = idx;
    node->mask = (1 << idx);
    node->preproc_id = (uint8_t) preproc_id;
    node->subcode = subcode;
    node->size = size;
    node->produce = produce;
    node->consume = consume;

    stream_ha_funcs[idx] = node;

    LogMessage("Stream5HA: Registered node %hu for preprocessor ID %hhu with subcode %hhu (size %hhu)\n",
                node->id, node->preproc_id, node->subcode, node->size);

    return idx;
}

void UnregisterStreamHAFuncs(uint32_t preproc_id, uint8_t subcode)
{
    StreamHAFuncsNode *node;
    int i;

    for (i = 0; i < n_stream_ha_funcs; i++)
    {
        node = stream_ha_funcs[i];
        if (node && preproc_id == node->preproc_id && subcode == node->subcode)
        {
            stream_ha_funcs[i] = NULL;
            free(node);
            break;
        }
    }

    if ((i + 1) == n_stream_ha_funcs)
        n_stream_ha_funcs--;
}

void Stream5SetHAPendingBit(void *ssnptr, int bit)
{
    Stream5LWSession *lwssn = (Stream5LWSession*) ssnptr;

    if (!lwssn)
        return;

    if (bit >= n_stream_ha_funcs || !stream_ha_funcs[bit])
    {
        FatalError("Attempted to set illegal HA pending bit %d!\n", bit);
    }

    lwssn->ha_pending_mask |= (1 << bit);
}

static void Stream5ParseHAArgs(SnortConfig *sc, Stream5HAConfig *config, char *args)
{
    char **toks;
    int num_toks;
    int i;
    char **stoks = NULL;
    int s_toks;
    char *endPtr = NULL;
    unsigned long int value;

    if (config == NULL)
        return;

    if ((args == NULL) || (strlen(args) == 0))
        return;

    toks = mSplit(args, ",", 0, &num_toks, 0);

    for (i = 0; i < num_toks; i++)
    {
        stoks = mSplit(toks[i], " ", 2, &s_toks, 0);

        if (s_toks == 0)
        {
            FatalError("%s(%d) => Missing parameter in Stream5 HA config.\n",
                    file_name, file_line);
        }

        if (!strcmp(stoks[0], "min_session_lifetime"))
        {
            if (stoks[1])
                value = strtoul(stoks[1], &endPtr, 10);
            else
                value = 0;

            if (!stoks[1] || (endPtr == &stoks[1][0]))
            {
                FatalError("%s(%d) => Invalid '%s' in config file. Requires integer parameter.\n",
                           file_name, file_line, stoks[0]);
            }

            if (value > UINT16_MAX)
            {
                FatalError("%s(%d) => '%s %lu' invalid: value must be between 0 and %d milliseconds.\n",
                           file_name, file_line, stoks[0], value, UINT16_MAX);
            }

            config->min_session_lifetime.tv_sec = 0;
            while (value >= 1000)
            {
                config->min_session_lifetime.tv_sec++;
                value -= 1000;
            }
            config->min_session_lifetime.tv_usec = value * 1000;
        }
        else if (!strcmp(stoks[0], "min_sync_interval"))
        {
            if (stoks[1])
                value = strtoul(stoks[1], &endPtr, 10);
            else
                value = 0;

            if (!stoks[1] || (endPtr == &stoks[1][0]))
            {
                FatalError("%s(%d) => Invalid '%s' in config file. Requires integer parameter.\n",
                           file_name, file_line, stoks[0]);
            }

            if (value > UINT16_MAX)
            {
                FatalError("%s(%d) => '%s %lu' invalid: value must be between 0 and %d milliseconds.\n",
                           file_name, file_line, stoks[0], value, UINT16_MAX);
            }

            config->min_sync_interval.tv_sec = 0;
            while (value >= 1000)
            {
                config->min_sync_interval.tv_sec++;
                value -= 1000;
            }
            config->min_sync_interval.tv_usec = value * 1000;
        }
        else if (!strcmp(stoks[0], "startup_input_file"))
        {
            if (!stoks[1])
            {
                FatalError("%s(%d) => '%s' missing an argument\n", file_name, file_line, stoks[0]);
            }
            if (config->startup_input_file)
            {
                FatalError("%s(%d) => '%s' specified multiple times\n", file_name, file_line, stoks[0]);
            }
            config->startup_input_file = SnortStrdup(stoks[1]);
        }
        else if (!strcmp(stoks[0], "runtime_output_file"))
        {
            if (!stoks[1])
            {
                FatalError("%s(%d) => '%s' missing an argument\n", file_name, file_line, stoks[0]);
            }
            if (config->runtime_output_file)
            {
                FatalError("%s(%d) => '%s' specified multiple times\n", file_name, file_line, stoks[0]);
            }
            config->runtime_output_file = SnortStrdup(stoks[1]);
        }
        else if (!strcmp(stoks[0], "shutdown_output_file"))
        {
            if (!stoks[1])
            {
                FatalError("%s(%d) => '%s' missing an argument\n", file_name, file_line, stoks[0]);
            }
            if (config->shutdown_output_file)
            {
                FatalError("%s(%d) => '%s' specified multiple times\n", file_name, file_line, stoks[0]);
            }
            config->shutdown_output_file = SnortStrdup(stoks[1]);
        }
        else if (!strcmp(stoks[0], "use_side_channel"))
        {
#ifdef SIDE_CHANNEL
            if (!sc->side_channel_config.enabled)
            {
                FatalError("%s(%d) => '%s' cannot be specified without enabling the Snort side channel.\n",
                            file_name, file_line, stoks[0]);
            }
            config->use_side_channel = 1;
#else
            FatalError("%s(%d) => Snort has been compiled without Side Channel support.\n", file_name, file_line);
#endif
        }
        else
        {
            FatalError("%s(%d) => Invalid Stream5 HA config option '%s'\n",
                    file_name, file_line, stoks[0]);
        }

        mSplitFree(&stoks, s_toks);
    }

    mSplitFree(&toks, num_toks);
#ifdef REG_TEST
    if(sc->ha_out)
    {
        if(config->runtime_output_file)
            free(config->runtime_output_file);
        config->runtime_output_file = SnortStrdup(sc->ha_out);
    }
    if(sc->ha_in)
    {
        if(config->startup_input_file)
            free(config->startup_input_file);
        config->startup_input_file = SnortStrdup(sc->ha_in);
    }
#endif

}

static void Stream5PrintHAConfig(Stream5HAConfig *config)
{
    if (config == NULL)
        return;

    LogMessage("Stream5 HA config:\n");
    LogMessage("    Minimum Session Lifetime: %lu milliseconds\n",
                config->min_session_lifetime.tv_sec * 1000 + config->min_session_lifetime.tv_usec / 1000);
    LogMessage("    Minimum Sync Interval: %lu milliseconds\n",
                config->min_sync_interval.tv_sec * 1000 + config->min_sync_interval.tv_usec / 1000);
    if (config->startup_input_file)
        LogMessage("    Startup Input File:    %s\n", config->startup_input_file);
    if (config->runtime_output_file)
        LogMessage("    Runtime Output File:   %s\n", config->runtime_output_file);
    if (config->shutdown_output_file)
        LogMessage("    Shutdown Output File:  %s\n", config->shutdown_output_file);
#ifdef REG_TEST
    LogMessage("    Stream5 LWS HA Data Size: %zu\n", sizeof(Stream5HASession));
#endif
}

void Stream5PrintHAStats(void)
{
    StreamHAFuncsNode *node;
    int i;

    LogMessage("  High Availability\n");
    LogMessage("          Updates Received: %u\n", s5ha_stats.update_messages_received);
    LogMessage("Updates Received (No Session): %u\n", s5ha_stats.update_messages_received_no_session);
    LogMessage("        Deletions Received: %u\n", s5ha_stats.delete_messages_received);
    LogMessage("  Updates Sent Immediately: %u\n", s5ha_stats.update_messages_sent_immediately);
    LogMessage("     Updates Sent Normally: %u\n", s5ha_stats.update_messages_sent_normally);
    LogMessage("            Deletions Sent: %u\n", s5ha_stats.delete_messages_sent);
    LogMessage("        Deletions Not Sent: %u\n", s5ha_stats.delete_messages_not_sent);
    for (i = 0; i < n_stream_ha_funcs; i++)
    {
        node = stream_ha_funcs[i];
        if (!node)
            continue;
        LogMessage("        Node %hhu/%hhu: %u produced, %u consumed\n",
                    node->preproc_id, node->subcode, node->produced, node->consumed);
    }
}

void Stream5ResetHAStats(void)
{
    memset(&s5ha_stats, 0, sizeof(s5ha_stats));
}

void Stream5HAInit(struct _SnortConfig *sc, char *args)
{
    tSfPolicyId policy_id = getParserPolicy(sc);
    Stream5Config *pDefaultPolicyConfig = NULL;
    Stream5Config *pCurrentPolicyConfig = NULL;

    if (s5_config == NULL)
        FatalError("Tried to config stream5 HA policy without global config!\n");

    sfPolicyUserPolicySet(s5_config, policy_id);

    pDefaultPolicyConfig = (Stream5Config *)sfPolicyUserDataGet(s5_config, getDefaultPolicy());
    pCurrentPolicyConfig = (Stream5Config *)sfPolicyUserDataGetCurrent(s5_config);

    if ((policy_id != getDefaultPolicy()) && ((pDefaultPolicyConfig == NULL) || (pDefaultPolicyConfig->ha_config == NULL)))
    {
        ParseError("Stream5 HA: Must configure default policy if other targeted policies are configured.\n");
    }

    if ((pCurrentPolicyConfig == NULL) || (pCurrentPolicyConfig->global_config == NULL))
    {
        FatalError("Tried to config stream5 HA policy without global config!\n");
    }

    if (pCurrentPolicyConfig->ha_config != NULL)
    {
        FatalError("%s(%d) ==> Cannot duplicate Stream5 HA configuration\n", file_name, file_line);
    }

    if (!pCurrentPolicyConfig->global_config->enable_ha)
    {
# ifdef SNORT_RELOAD
        /* Return if we're reloading - the discrepancy will be handled in
         * the reload verify */
        if (GetReloadStreamConfig(sc) != NULL)
# endif
            return;
    }

    pCurrentPolicyConfig->ha_config = (Stream5HAConfig*)SnortAlloc(sizeof(*pCurrentPolicyConfig->ha_config));

    Stream5ParseHAArgs(sc, pCurrentPolicyConfig->ha_config, args);

    if (policy_id != getDefaultPolicy())
    {
        pCurrentPolicyConfig->ha_config->min_session_lifetime = pDefaultPolicyConfig->ha_config->min_session_lifetime;
        pCurrentPolicyConfig->ha_config->min_sync_interval = pDefaultPolicyConfig->ha_config->min_sync_interval;
    }

#ifdef PERF_PROFILING
    RegisterPreprocessorProfile("s5HAProduce", &s5HAProducePerfStats, 2, &s5HAPerfStats);
    RegisterPreprocessorProfile("s5HAConsume", &s5HAConsumePerfStats, 0, &totalPerfStats);
#endif

    ControlSocketRegisterHandler(CS_TYPE_DEBUG_S5_HA, Stream5DebugHA, NULL, NULL);
    LogMessage("Registered Stream5HA Debug control socket message type (0x%x)\n", CS_TYPE_DEBUG_S5_HA);

    Stream5PrintHAConfig(pCurrentPolicyConfig->ha_config);
}

#if defined(SNORT_RELOAD)
void Stream5HAReload(struct _SnortConfig *sc, char *args, void **new_config)
{
    tSfPolicyUserContextId s5_swap_config = (tSfPolicyUserContextId)*new_config;
    tSfPolicyId policy_id = getParserPolicy(sc);
    Stream5Config *config;

    s5_swap_config = (tSfPolicyUserContextId)GetReloadStreamConfig(sc);
    if (s5_swap_config == NULL)
        FatalError("Tried to config stream5 HA without global config!\n");

    config = (Stream5Config *)sfPolicyUserDataGet(s5_swap_config, policy_id);

    if ((config == NULL) || (config->global_config == NULL))
    {
        FatalError("Tried to config stream5 HA without global config!\n");
    }

    if (config->ha_config == NULL)
        config->ha_config = (Stream5HAConfig *)SnortAlloc(sizeof(*config->ha_config));

    Stream5ParseHAArgs(sc, config->ha_config, args);
    Stream5PrintHAConfig(config->ha_config);
}
#endif

int Stream5VerifyHAConfig(struct _SnortConfig *sc, Stream5HAConfig *config, tSfPolicyId policy_id)
{
    if (config == NULL)
        return -1;

    return 0;
}

void Stream5HAConfigFree(Stream5HAConfig *config)
{
    if (config == NULL)
        return;

    if (config->startup_input_file)
        free(config->startup_input_file);

    if (config->runtime_output_file)
        free(config->runtime_output_file);

    if (config->shutdown_output_file)
        free(config->shutdown_output_file);

    free(config);
}

// This MUST have the exact same logic as GetLWSessionKeyFromIpPort()
// TBD any alternative to this approach?
static inline bool IsClientLower(const sfip_t *cltIP, uint16_t cltPort,
                                 const sfip_t *srvIP, uint16_t srvPort, char proto)
{
    if (IS_IP4(cltIP))
    {
        if (cltIP->ip32 < srvIP->ip32)
            return true;

        if (cltIP->ip32 > srvIP->ip32)
            return false;

        switch (proto)
        {
            case IPPROTO_TCP:
            case IPPROTO_UDP:
                if (cltPort < srvPort)
                    return true;
        }
        return false;
    }
    if (sfip_fast_lt6(cltIP, srvIP))
        return true;

    if (sfip_fast_lt6(srvIP, cltIP))
        return false;

    switch (proto)
    {
        case IPPROTO_TCP:
        case IPPROTO_UDP:
            if (cltPort < srvPort)
                return true;
    }
    return false;
}

static Stream5LWSession *DeserializeHASession(const SessionKey *key, const Stream5HASession *has, Stream5LWSession *lwssn)
{
    Stream5LWSession *retSsn;
    int family;

    if (!lwssn)
    {
        const HA_Api *api;

        api = ha_get_api(key->protocol);
        retSsn = api->create_session(key);

        retSsn->ha_flags &= ~HA_FLAG_NEW;

        family = (has->flags & HA_SESSION_FLAG_IP6) ? AF_INET6 : AF_INET;
        if (has->flags & HA_SESSION_FLAG_LOW)
        {
            sfip_set_raw(&retSsn->server_ip, retSsn->key->ip_l, family);
            sfip_set_raw(&retSsn->client_ip, retSsn->key->ip_h, family);
            retSsn->server_port = retSsn->key->port_l;
            retSsn->client_port = retSsn->key->port_h;
        }
        else
        {
            sfip_set_raw(&retSsn->client_ip, retSsn->key->ip_l, family);
            sfip_set_raw(&retSsn->server_ip, retSsn->key->ip_h, family);
            retSsn->client_port = retSsn->key->port_l;
            retSsn->server_port = retSsn->key->port_h;
        }
    }
    else
    {
        retSsn = lwssn;
    }

    retSsn->ha_state = has->ha_state;

    return retSsn;
}

static inline int DeserializePreprocData(uint8_t event, Stream5LWSession *lwssn, uint8_t preproc_id,
                                         uint8_t subcode, const uint8_t *data, uint8_t length)
{
    StreamHAFuncsNode *node;
    int i;

    for (i = 0; i < n_stream_ha_funcs; i++)
    {
        node = stream_ha_funcs[i];
        if (node && preproc_id == node->preproc_id && subcode == node->subcode)
        {
            if (node->size < length)
            {
                ErrorMessage("Stream5 HA preprocessor data record's length exceeds expected size! (%u vs %u)\n",
                        length, node->size);
                return -1;
            }
            node->consumed++;
            return node->consume(lwssn, data, length);
        }
    }

    ErrorMessage("Stream5 HA preprocessor data record received with unrecognized preprocessor ID/subcode! (%hhu:%hhu)\n",
            preproc_id, subcode);
    return -1;
}

static int ConsumeHAMessage(const uint8_t *msg, uint32_t msglen)
{
    const HA_Api *api;
    Stream5HASession *has;
    Stream5LWSession *lwssn;
    SessionKey key;
    MsgHeader *msg_hdr;
    RecordHeader *rec_hdr;
    PreprocDataHeader *psd_hdr;
    uint32_t offset;
    bool debug_flag;
    int rval = 1;
    PROFILE_VARS;

    PREPROC_PROFILE_START(s5HAConsumePerfStats);

    /* Read the message header */
    if (msglen < sizeof(*msg_hdr))
    {
        ErrorMessage("Stream5 HA message length shorter than header length! (%u)\n", msglen);
        goto consume_exit;
    }
    msg_hdr = (MsgHeader *) msg;
    offset = sizeof(*msg_hdr);

    if (msg_hdr->total_length != msglen)
    {
        ErrorMessage("Stream5 HA message header's total length does not match actual length! (%u vs %u)\n",
                msg_hdr->total_length, msglen);
        goto consume_exit;
    }

    if (msg_hdr->event != HA_EVENT_UPDATE && msg_hdr->event != HA_EVENT_DELETE)
    {
        ErrorMessage("Stream5 HA message has unknown event type: %hhu!\n", msg_hdr->event);
        goto consume_exit;
    }

    /* Read the key */
    if (msg_hdr->key_size == IP4_SESSION_KEY_SIZE) /* IPv4, miniature key */
    {
        /* Lower IPv4 address */
        memcpy(&key.ip_l, msg + offset, 4);
        key.ip_l[1] = key.ip_l[2] = key.ip_l[3] = 0;
        offset += 4;
        /* Higher IPv4 address */
        memcpy(&key.ip_h, msg + offset, 4);
        key.ip_h[1] = key.ip_h[2] = key.ip_h[3] = 0;
        offset += 4;
        /* The remainder of the key */
        memcpy(((uint8_t *) &key) + 32, msg + offset, IP4_SESSION_KEY_SIZE - 8);
        offset += IP4_SESSION_KEY_SIZE - 8;
    }
    else if (msg_hdr->key_size == IP6_SESSION_KEY_SIZE) /* IPv6, full-size key */
    {
        memcpy(&key, msg + offset, IP6_SESSION_KEY_SIZE);
        offset += IP6_SESSION_KEY_SIZE;
    }
    else
    {
        ErrorMessage("Stream5 HA message has unrecognized key size: %hhu!\n", msg_hdr->key_size);
        goto consume_exit;
    }

    debug_flag = Stream5HADebugCheck(&key, s5_ha_debug_flag, &s5_ha_debug_info, s5_ha_debug_session, sizeof(s5_ha_debug_session));

    api = ha_get_api(key.protocol);
    if (!api)
    {
        ErrorMessage("Stream5 HA message has unhandled protocol: %u!\n", key.protocol);
        goto consume_exit;
    }

    if (msg_hdr->event == HA_EVENT_DELETE)
    {
        if (debug_flag)
            LogMessage("S5HADbg Consuming deletion message for %s\n", s5_ha_debug_session);
        if (offset != msglen)
        {
            ErrorMessage("Stream5 HA deletion message contains extraneous data! (%u bytes)\n", msglen - offset);
            goto consume_exit;
        }
        s5ha_stats.delete_messages_received++;
        rval = api->delete_session(&key);
        if (debug_flag)
        {
            if (!rval)
                LogMessage("S5HADbg Deleted LWSession for %s\n", s5_ha_debug_session);
            else
                LogMessage("S5HADbg Could not delete LWSession for %s\n", s5_ha_debug_session);
        }
        goto consume_exit;
    }

    if (debug_flag)
        LogMessage("S5HADbg Consuming update message for %s\n", s5_ha_debug_session);

    lwssn = api->get_lws(&key);

    /* Read any/all records. */
    while (offset < msglen)
    {
        if (sizeof(*rec_hdr) > (msglen - offset))
        {
            ErrorMessage("Stream5 HA message contains a truncated record header! (%zu vs %u)\n",
                    sizeof(*rec_hdr), msglen - offset);
            goto consume_exit;
        }
        rec_hdr = (RecordHeader *) (msg + offset);
        offset += sizeof(*rec_hdr);

        switch (rec_hdr->type)
        {
            case HA_TYPE_HAS:
                if (rec_hdr->length != sizeof(*has))
                {
                    ErrorMessage("Stream5 HA message contains incorrectly size HA Session record! (%u vs %zu)\n",
                            rec_hdr->length, sizeof(*has));
                    goto consume_exit;
                }
                if (rec_hdr->length > (msglen - offset))
                {
                    ErrorMessage("Stream5 HA message contains truncated HA Session record data! (%u vs %u)\n",
                            rec_hdr->length, msglen - offset);
                    goto consume_exit;
                }
                has = (Stream5HASession *) (msg + offset);
                offset += rec_hdr->length;
                if (debug_flag)
                {
#ifdef TARGET_BASED
                    LogMessage("S5HADbg %s LWSession for %s - SF=0x%x IPP=0x%hx AP=0x%hx DIR=%hhu IDIR=%hhu\n",
                                (lwssn) ? "Updating" : "Creating", s5_ha_debug_session, has->ha_state.session_flags,
                                has->ha_state.ipprotocol, has->ha_state.application_protocol,
                                has->ha_state.direction, has->ha_state.ignore_direction);
#else
                    LogMessage("S5HADbg %s LWSession for %s - SF=0x%x DIR=%hhu IDIR=%hhu\n",
                                (lwssn) ? "Updating" : "Creating", s5_ha_debug_session, has->ha_state.session_flags,
                                has->ha_state.direction, has->ha_state.ignore_direction);
#endif
                }
                lwssn = DeserializeHASession(&key, has, lwssn);
                break;

            case HA_TYPE_PSD:
                if (!lwssn)
                {
                    //ErrorMessage("Stream5 HA message with preprocessor data record received for non-existent session!\n");
                    s5ha_stats.update_messages_received_no_session++;
                    goto consume_exit;
                }
                if (sizeof(*psd_hdr) > (msglen - offset))
                {
                    ErrorMessage("Stream5 HA message contains a truncated preprocessor data record header! (%zu vs %u)\n",
                            sizeof(*psd_hdr), msglen - offset);
                    goto consume_exit;
                }
                psd_hdr = (PreprocDataHeader *) (msg + offset);
                offset += sizeof(*psd_hdr);
                if (rec_hdr->length > (msglen - offset))
                {
                    ErrorMessage("Stream5 HA message contains truncated preprocessor data record data! (%u vs %u)\n",
                            rec_hdr->length, msglen - offset);
                    goto consume_exit;
                }
                if (debug_flag)
                {
                    LogMessage("SFHADbg Consuming %hhu byte preprocessor data record for %s with PPID=%hhu and SC=%hhu\n",
                                rec_hdr->length, s5_ha_debug_session, psd_hdr->preproc_id, psd_hdr->subcode);
                }
                if (DeserializePreprocData(msg_hdr->event, lwssn, psd_hdr->preproc_id, psd_hdr->subcode,
                                            msg + offset, rec_hdr->length) != 0)
                {
                    ErrorMessage("Stream5 HA message contained invalid preprocessor data record!\n");
                    goto consume_exit;
                }
                offset += rec_hdr->length;
                break;

            default:
                ErrorMessage("Stream5 HA message contains unrecognized record type: %hhu!\n", rec_hdr->type);
                goto consume_exit;
        }
    }
    /* Mark the session as being in standby mode since we just received an update. */
    if (lwssn && !(lwssn->ha_flags & HA_FLAG_STANDBY))
    {
        if (api->deactivate_session)
            api->deactivate_session(lwssn);
        lwssn->ha_flags |= HA_FLAG_STANDBY;
    }

    s5ha_stats.update_messages_received++;
    rval = 0;

consume_exit:
    PREPROC_PROFILE_END(s5HAConsumePerfStats);
    return rval;
}

/*
 * File I/O
 */
static inline ssize_t Read(int fd, void *buf, size_t count)
{
    ssize_t n;
    errno = 0;

    while ((n = read(fd, buf, count)) <= (ssize_t) count)
    {
        if (n == (ssize_t) count)
            return 0;

        if (n > 0)
        {
            buf = (uint8_t *) buf + n;
            count -= n;
        }
        else if (n == 0)
            break;
        else if (errno != EINTR)
        {
            ErrorMessage("Error reading from Stream5 HA message file: %s (%d)\n", strerror(errno), errno);
            break;
        }
    }
    return -1;
}

static int ReadHAMessagesFromFile(const char *filename)
{
    MsgHeader *msg_header;
    uint8_t *msg;
    int rval, fd;

    fd = open(filename, O_RDONLY, 0664);
    if (fd < 0)
    {
        FatalError("Could not open %s for reading HA messages from: %s (%d)\n", filename, strerror(errno), errno);
    }

    LogMessage("Reading Stream5 HA messages from '%s'...\n", filename);
    msg = file_io_buffer;
    while ((rval = Read(fd, msg, sizeof(*msg_header))) == 0)
    {
        msg_header = (MsgHeader *) msg;
        if (msg_header->total_length < sizeof(*msg_header))
        {
            ErrorMessage("Stream5 HA Message total length (%hu) is way too short!\n", msg_header->total_length);
            close(fd);
            return -1;
        }
        else if (msg_header->total_length > (UINT16_MAX - sizeof(*msg_header)))
        {
            ErrorMessage("Stream5 HA Message total length (%hu) is too long!\n", msg_header->total_length);
            close(fd);
            return -1;
        }
        else if (msg_header->total_length > sizeof(*msg_header))
        {
            if ((rval = Read(fd, msg + sizeof(*msg_header), msg_header->total_length - sizeof(*msg_header))) != 0)
            {
                ErrorMessage("Error reading the remaining %zu bytes of an HA message from file: %s (%d)\n",
                        msg_header->total_length - sizeof(*msg_header), strerror(errno), errno);
                close(fd);
                return rval;
            }
        }
        if ((rval = ConsumeHAMessage(msg, msg_header->total_length)) != 0)
        {
            close(fd);
            return rval;
        }
    }
    close(fd);

    return 0;
}

static inline ssize_t Write(int fd, const void *buf, size_t count)
{
    ssize_t n;
    errno = 0;

    while ((n = write(fd, buf, count)) <= (ssize_t) count)
    {
        if (n == (ssize_t) count)
            return 0;

        if (n > 0)
            count -= n;
        else if (errno != EINTR)
        {
            ErrorMessage("Error writing to Stream5 HA message file: %s (%d)\n", strerror(errno), errno);
            break;
        }
    }

    return -1;
}

static uint32_t WriteHAMessageHeader(uint8_t event, uint16_t msglen, const SessionKey *key, uint8_t *msg)
{
    MsgHeader *msg_hdr;
    uint32_t offset;

    msg_hdr = (MsgHeader *) msg;
    offset = sizeof(*msg_hdr);
    msg_hdr->event = event;
    msg_hdr->version = HA_MESSAGE_VERSION;
    msg_hdr->total_length = msglen;
    msg_hdr->key_type = HA_TYPE_KEY;

    if (key->ip_l[1] || key->ip_l[2] || key->ip_l[3] || key->ip_h[1] || key->ip_h[2] || key->ip_h[3])
    {
        msg_hdr->key_size = IP6_SESSION_KEY_SIZE;
        memcpy(msg + offset, key, IP6_SESSION_KEY_SIZE);
        offset += IP6_SESSION_KEY_SIZE;
    }
    else
    {
        msg_hdr->key_size = IP4_SESSION_KEY_SIZE;
        memcpy(msg + offset, &key->ip_l[0], sizeof(key->ip_l[0]));
        offset += sizeof(key->ip_l[0]);
        memcpy(msg + offset, &key->ip_h[0], sizeof(key->ip_h[0]));
        offset += sizeof(key->ip_h[0]);
        memcpy(msg + offset, ((uint8_t *) key) + 32, IP4_SESSION_KEY_SIZE - 8);
        offset += IP4_SESSION_KEY_SIZE - 8;
    }
    return offset;
}

static void UpdateHAMessageHeaderLength(uint8_t *msg, uint16_t msglen)
{
    MsgHeader *msg_hdr;

    msg_hdr = (MsgHeader *) msg;
    msg_hdr->total_length = msglen;
}

static uint32_t WriteHASession(Stream5LWSession *lwssn, uint8_t *msg)
{
    Stream5HASession *has;
    RecordHeader *rec_hdr;
    uint32_t offset;

    rec_hdr = (RecordHeader *) msg;
    offset = sizeof(*rec_hdr);
    rec_hdr->type = HA_TYPE_HAS;
    rec_hdr->length = sizeof(*has);

    has = (Stream5HASession *) (msg + offset);
    offset += sizeof(*has);
    has->ha_state = lwssn->ha_state;

    if (!IsClientLower(&lwssn->client_ip, lwssn->client_port, &lwssn->server_ip, lwssn->server_port, lwssn->key->protocol))
        has->flags |= HA_SESSION_FLAG_LOW;

    if (lwssn->client_ip.family == AF_INET6)
        has->flags |= HA_SESSION_FLAG_IP6;

    return offset;
}

static uint32_t WritePreprocDataRecord(Stream5LWSession *lwssn, StreamHAFuncsNode *node, uint8_t *msg)
{
    RecordHeader *rec_hdr;
    PreprocDataHeader *psd_hdr;
    uint32_t offset;

    rec_hdr = (RecordHeader *) msg;
    offset = sizeof(*rec_hdr);
    rec_hdr->type = HA_TYPE_PSD;

    psd_hdr = (PreprocDataHeader *) (msg + offset);
    offset += sizeof(*psd_hdr);
    psd_hdr->preproc_id = node->preproc_id;
    psd_hdr->subcode = node->subcode;

    rec_hdr->length = node->produce(lwssn, msg + offset);
    offset += rec_hdr->length;
    node->produced++;

    return offset;
}

static uint32_t CalculateHAMessageSize(uint8_t event, Stream5LWSession *lwssn)
{
    StreamHAFuncsNode *node;
    SessionKey *key;
    uint32_t msg_size;
    int idx;

    key = lwssn->key;

    /* Header (including the key).  IPv4 keys are miniaturized. */
    msg_size = sizeof(MsgHeader);
    if (key->ip_l[1] || key->ip_l[2] || key->ip_l[3] || key->ip_h[1] || key->ip_h[2] || key->ip_h[3])
        msg_size += IP6_SESSION_KEY_SIZE;
    else
        msg_size += IP4_SESSION_KEY_SIZE;

    if (event == HA_EVENT_UPDATE)
    {
        /* HA Session record */
        //if (lwssn->ha_flags & HA_FLAG_MODIFIED)
            msg_size += sizeof(RecordHeader) + sizeof(Stream5HASession);

        /* Preprocessor data records */
        for (idx = 0; idx < n_stream_ha_funcs; idx++)
        {
            if (lwssn->ha_pending_mask & (1 << idx))
            {
                node = stream_ha_funcs[idx];
                if (!node)
                    continue;
                msg_size += sizeof(RecordHeader) + sizeof(PreprocDataHeader) + node->size;
            }
        }
    }

    return msg_size;
}

static uint32_t GenerateHADeletionMessage(uint8_t *msg, uint32_t msg_size, Stream5LWSession *lwssn)
{
    uint32_t msglen;
    PROFILE_VARS;

    PREPROC_PROFILE_START(s5HAProducePerfStats);

    msglen = WriteHAMessageHeader(HA_EVENT_DELETE, msg_size, lwssn->key, msg);

    PREPROC_PROFILE_END(s5HAProducePerfStats);

    return msglen;
}

#ifdef SIDE_CHANNEL
static void SendSCDeletionMessage(Stream5LWSession *lwssn, uint32_t msg_size)
{
    SCMsgHdr *sc_hdr;
    void *msg_handle;
    uint8_t *msg;
    int rval;

    /* Allocate space for the message. */
    if ((rval = SideChannelPreallocMessageTX(msg_size, &sc_hdr, &msg, &msg_handle)) != 0)
    {
        /* TODO: Error stuff goes here. */
        return;
    }

    /* Generate the message. */
    msg_size = GenerateHADeletionMessage(msg, msg_size, lwssn);

    /* Send the message. */
    sc_hdr->type = SC_MSG_TYPE_FLOW_STATE_TRACKING;
    sc_hdr->timestamp = packet_time();
    SideChannelEnqueueMessageTX(sc_hdr, msg, msg_size, msg_handle, NULL);
}
#endif

void Stream5HANotifyDeletion(Stream5LWSession *lwssn)
{
    Stream5Config *pLWSPolicyConfig;
    uint32_t msg_size;
    bool debug_flag;
    PROFILE_VARS;

    PREPROC_PROFILE_START(s5HAPerfStats);

    if (!lwssn)
    {
        PREPROC_PROFILE_END(s5HAPerfStats);
        return;
    }

    pLWSPolicyConfig = (Stream5Config *) sfPolicyUserDataGet(lwssn->config, lwssn->policy_id);
    if (!pLWSPolicyConfig || !pLWSPolicyConfig->global_config || !pLWSPolicyConfig->global_config->enable_ha)
    {
        PREPROC_PROFILE_END(s5HAPerfStats);
        return;
    }

    /* Don't send a deletion notice if we've never sent an update for the flow, it is in standby, or we've already sent one. */
    if (lwssn->ha_flags & (HA_FLAG_NEW|HA_FLAG_STANDBY|HA_FLAG_DELETED))
    {
        s5ha_stats.delete_messages_not_sent++;
        PREPROC_PROFILE_END(s5HAPerfStats);
        return;
    }

    debug_flag = Stream5HADebugCheck(lwssn->key, s5_ha_debug_flag, &s5_ha_debug_info, s5_ha_debug_session, sizeof(s5_ha_debug_session));
    if (debug_flag)
        LogMessage("S5HADbg Producing deletion message for %s\n", s5_ha_debug_session);


    /* Calculate the size of the deletion message. */
    msg_size = CalculateHAMessageSize(HA_EVENT_DELETE, lwssn);

    if (runtime_output_fd >= 0)
    {
        msg_size = GenerateHADeletionMessage(file_io_buffer, msg_size, lwssn);
        if (Write(runtime_output_fd, file_io_buffer, msg_size) == -1)
        {
            /* TODO: Error stuff here. */
        }
    }

#ifdef SIDE_CHANNEL
    if (pLWSPolicyConfig->ha_config->use_side_channel)
    {
        SendSCDeletionMessage(lwssn, msg_size);
    }
#endif

    lwssn->ha_flags |= HA_FLAG_DELETED;

    s5ha_stats.delete_messages_sent++;

    PREPROC_PROFILE_END(s5HAPerfStats);
}

static uint32_t GenerateHAUpdateMessage(uint8_t *msg, uint32_t msg_size, Stream5LWSession *lwssn)
{
    StreamHAFuncsNode *node;
    uint32_t offset;
    int idx;
    PROFILE_VARS;

    PREPROC_PROFILE_START(s5HAProducePerfStats);

    offset = WriteHAMessageHeader(HA_EVENT_UPDATE, msg_size, lwssn->key, msg);
    offset += WriteHASession(lwssn, msg + offset);
    for (idx = 0; idx < n_stream_ha_funcs; idx++)
    {
        if (lwssn->ha_pending_mask & (1 << idx))
        {
            node = stream_ha_funcs[idx];
            if (!node)
                continue;
            offset += WritePreprocDataRecord(lwssn, node, msg + offset);
        }
    }
    /* Update the message header length since it might be shorter than originally anticipated. */
    UpdateHAMessageHeaderLength(msg, offset);

    PREPROC_PROFILE_END(s5HAProducePerfStats);

    return offset;
}

#ifdef SIDE_CHANNEL
static void SendSCUpdateMessage(Stream5LWSession *lwssn, uint32_t msg_size)
{
    SCMsgHdr *schdr;
    void *msg_handle;
    uint8_t *msg;
    int rval;

    /* Allocate space for the message. */
    if ((rval = SideChannelPreallocMessageTX(msg_size, &schdr, &msg, &msg_handle)) != 0)
    {
        /* TODO: Error stuff goes here. */
        return;
    }

    /* Gnerate the message. */
    msg_size = GenerateHAUpdateMessage(msg, msg_size, lwssn);

    /* Send the message. */
    schdr->type = SC_MSG_TYPE_FLOW_STATE_TRACKING;
    schdr->timestamp = packet_time();
    SideChannelEnqueueMessageTX(schdr, msg, msg_size, msg_handle, NULL);
}
#endif

void Stream5ProcessHA(void *ssnptr)
{
    struct timeval pkt_time;
    Stream5LWSession *lwssn = (Stream5LWSession*) ssnptr;
    Stream5Config *pLWSPolicyConfig;
    uint32_t msg_size;
    bool debug_flag;
    PROFILE_VARS;

    PREPROC_PROFILE_START(s5HAPerfStats);

    if (!lwssn || (!lwssn->ha_pending_mask && !(lwssn->ha_flags & HA_FLAG_MODIFIED)))
    {
        PREPROC_PROFILE_END(s5HAPerfStats);
        return;
    }

    pLWSPolicyConfig = (Stream5Config *) sfPolicyUserDataGet(lwssn->config, lwssn->policy_id);

    if (!pLWSPolicyConfig || !pLWSPolicyConfig->global_config || !pLWSPolicyConfig->global_config->enable_ha)
    {
        PREPROC_PROFILE_END(s5HAPerfStats);
        return;
    }

    /*
       For now, we are only generating messages for:
        (a) major and critical changes or
        (b) preprocessor changes on already synchronized sessions.
     */
    if (!(lwssn->ha_flags & (HA_FLAG_MAJOR_CHANGE | HA_FLAG_CRITICAL_CHANGE)) &&
        (!lwssn->ha_pending_mask || (lwssn->ha_flags & HA_FLAG_NEW)))
    {
        PREPROC_PROFILE_END(s5HAPerfStats);
        return;
    }

    /* Ensure that a new flow has lived long enough for anyone to care about it
        and that we're not overrunning the synchronization threshold. */
    packet_gettimeofday(&pkt_time);
    if (pkt_time.tv_sec < lwssn->ha_next_update.tv_sec ||
        (pkt_time.tv_sec == lwssn->ha_next_update.tv_sec && pkt_time.tv_usec < lwssn->ha_next_update.tv_usec))
    {
        /* Critical changes will be allowed to bypass the message timing restrictions. */
        if (!(lwssn->ha_flags & HA_FLAG_CRITICAL_CHANGE))
        {
            PREPROC_PROFILE_END(s5HAPerfStats);
            return;
        }
        s5ha_stats.update_messages_sent_immediately++;
    }
    else
        s5ha_stats.update_messages_sent_normally++;

    debug_flag = Stream5HADebugCheck(lwssn->key, s5_ha_debug_flag, &s5_ha_debug_info, s5_ha_debug_session, sizeof(s5_ha_debug_session));
    if (debug_flag)
#ifdef TARGET_BASED
        LogMessage("S5HADbg Producing update message for %s - SF=0x%x IPP=0x%hx AP=0x%hx DIR=%hhu IDIR=%hhu HPM=0x%hhx HF=0x%hhx\n",
                    s5_ha_debug_session, lwssn->ha_state.session_flags, lwssn->ha_state.ipprotocol,
                    lwssn->ha_state.application_protocol, lwssn->ha_state.direction, lwssn->ha_state.ignore_direction,
                    lwssn->ha_pending_mask, lwssn->ha_flags);
#else
        LogMessage("S5HADbg Producing update message for %s - SF=0x%x DIR=%hhu IDIR=%hhu HPM=0x%hhx HF=0x%hhx\n",
                    s5_ha_debug_session, lwssn->ha_state.session_flags,
                    lwssn->ha_state.direction, lwssn->ha_state.ignore_direction,
                    lwssn->ha_pending_mask, lwssn->ha_flags);
#endif

    /* Calculate the size of the update message. */
    msg_size = CalculateHAMessageSize(HA_EVENT_UPDATE, lwssn);

    if (runtime_output_fd >= 0)
    {
        msg_size = GenerateHAUpdateMessage(file_io_buffer, msg_size, lwssn);
        if (Write(runtime_output_fd, file_io_buffer, msg_size) == -1)
        {
            /* TODO: Error stuff here. */
        }
    }

#ifdef SIDE_CHANNEL
    if (pLWSPolicyConfig->ha_config->use_side_channel)
    {
        SendSCUpdateMessage(lwssn, msg_size);
    }
#endif

    /* Calculate the next update threshold. */
    lwssn->ha_next_update.tv_usec += pLWSPolicyConfig->ha_config->min_session_lifetime.tv_usec;
    if (lwssn->ha_next_update.tv_usec > 1000000)
    {
        lwssn->ha_next_update.tv_usec -= 1000000;
        lwssn->ha_next_update.tv_sec++;
    }
    lwssn->ha_next_update.tv_sec += pLWSPolicyConfig->ha_config->min_session_lifetime.tv_sec;

    /* Clear the modified/new flags and pending preprocessor updates. */
    lwssn->ha_flags &= ~(HA_FLAG_NEW|HA_FLAG_MODIFIED|HA_FLAG_MAJOR_CHANGE|HA_FLAG_CRITICAL_CHANGE);
    lwssn->ha_pending_mask = 0;

    PREPROC_PROFILE_END(s5HAPerfStats);
}

#ifdef SIDE_CHANNEL
static int Stream5HASCMsgHandler(SCMsgHdr *hdr, const uint8_t *msg, uint32_t msglen)
{
    int rval;
    PROFILE_VARS;

    PREPROC_PROFILE_START(s5HAPerfStats);

    rval = ConsumeHAMessage(msg, msglen);

    PREPROC_PROFILE_END(s5HAPerfStats);

    return rval;
}
#endif

void Stream5HAPostConfigInit(struct _SnortConfig *sc, int unused, void *arg)
{
    Stream5Config *pDefaultPolicyConfig;
    int rval;

    pDefaultPolicyConfig = (Stream5Config *)sfPolicyUserDataGet(s5_config, getDefaultPolicy());
    if (!pDefaultPolicyConfig->global_config->enable_ha)
        return;

    if (pDefaultPolicyConfig->ha_config->startup_input_file)
    {
        if ((rval = ReadHAMessagesFromFile(pDefaultPolicyConfig->ha_config->startup_input_file)) != 0)
        {
            ErrorMessage("Errors were encountered while reading HA messages from file!");
        }
    }

    if (pDefaultPolicyConfig->ha_config->runtime_output_file)
    {
        runtime_output_fd = open(pDefaultPolicyConfig->ha_config->runtime_output_file, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (runtime_output_fd < 0)
        {
            FatalError("Could not open %s for writing HA messages to: %s (%d)\n",
                        pDefaultPolicyConfig->ha_config->runtime_output_file, strerror(errno), errno);
        }
    }

#ifdef SIDE_CHANNEL
    if (pDefaultPolicyConfig->ha_config->use_side_channel)
    {
        if ((rval = SideChannelRegisterRXHandler(SC_MSG_TYPE_FLOW_STATE_TRACKING, Stream5HASCMsgHandler, NULL)) != 0)
        {
            /* TODO: Fatal error here or something. */
        }
    }
#endif
}

void Stream5CleanHA(void)
{
    int i;

    for (i = 0; i < n_stream_ha_funcs; i++)
    {
        if (stream_ha_funcs[i])
        {
            free(stream_ha_funcs[i]);
            stream_ha_funcs[i] = NULL;
        }
    }
    if (runtime_output_fd >= 0)
    {
        close(runtime_output_fd);
        runtime_output_fd = -1;
    }
}
