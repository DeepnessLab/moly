/*
** Copyright (C) 2014-2015 Cisco and/or its affiliates. All rights reserved.
** Copyright (C) 2005-2013 Sourcefire, Inc.
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


#ifndef _APPID_H_
#define _APPID_H_

#include <stdint.h>
#include <attribute.h>

#include <netinet/in.h>
#include "profiler.h"
#include "commonAppMatcher.h"
#include "client_app_api.h"
#include "service_api.h"
#include "flow.h"
#include "common_util.h"
#include "sip_common.h"
#include "appInfoTable.h"
#include "thirdparty_appid_utils.h"

#define PP_APP_ID   1

#define MIN_SFTP_PACKET_COUNT   30
#define MAX_SFTP_PACKET_COUNT   55

/*#define  RNA_FULL_CLEANUP   1 */

extern uint8_t  appIdPriorityArray[SF_APPID_MAX+1];

struct AppIdData * getAppIdData(void* lwssn);

void fwAppIdInit(void);
void fwAppIdFini(tAppIdConfig *pConfig);
void fwAppIdSearch(SFSnortPacket *p);
void httpHeaderCallback (SFSnortPacket *p, HttpParsedHeaders *const headers);
void SipSessionSnortCallback (void *ssnptr, ServiceEventType eventType, void *eventData);

void readRnaAppMappingTable(const char *path, tAppIdConfig *pConfig);
tAppId appGetAppFromServiceId(uint32_t serviceId, tAppIdConfig *pConfig);
tAppId appGetAppFromClientId(uint32_t clientId, tAppIdConfig *pConfig);
tAppId appGetAppFromPayloadId(uint32_t payloadId, tAppIdConfig *pConfig);
void appSharedDataDelete(tAppIdData * sharedData);
void AppIdAddUser(tAppIdData *flowp, const char *username, tAppId appId, int success);
void AppIdAddDnsQueryInfo(tAppIdData *flow,
                          uint16_t id,
                          const uint8_t *host, uint8_t host_len, uint16_t host_offset,
                          uint16_t record_type);
void AppIdAddDnsResponseInfo(tAppIdData *flow,
                             uint16_t id,
                             const uint8_t *host, uint8_t host_len, uint16_t host_offset,
                             uint8_t response_type, uint32_t ttl);
void AppIdResetDnsInfo(tAppIdData *flow);

void AppIdAddPayload(tAppIdData *flow, tAppId payload_id);
tAppIdData* appSharedDataAlloc(uint8_t proto, sfaddr_t *ip);
tAppId getOpenAppId(void *ssnptr);

void appSetServiceValidator(RNAServiceValidationFCN fcn, tAppId appId, unsigned extractsInfo, tAppIdConfig *pConfig);
void appSetLuaServiceValidator(RNAServiceValidationFCN fcn, tAppId appId, unsigned extractsInfo, struct _Detector *dat);
void appSetClientValidator(RNAClientAppFCN fcn, tAppId appId, unsigned extractsInfo, tAppIdConfig *pConfig);
void appSetLuaClientValidator(RNAClientAppFCN fcn, tAppId appId, unsigned extractsInfo, struct _Detector *data);
int sslAppGroupIdLookup(void *ssnptr, const char * serverName, const char * commonName, tAppId *serviceAppId, tAppId *clientAppId, tAppId *payloadAppId);
tAppId getAppId(void *ssnptr);

#ifdef FW_TRACKER_DEBUG
void logAppIdInfo(SFSnortPacket *p, char *message, tAppId id);
#endif
int AppIdDebug(uint16_t type, const uint8_t *data, uint32_t length, void **new_context,
               char* statusBuf, int statusBuf_len);

extern char app_id_debug_session[FW_DEBUG_SESSION_ID_SIZE];
extern bool app_id_debug_session_flag;

#ifdef PERF_PROFILING
extern PreprocStats httpPerfStats;
extern PreprocStats clientMatchPerfStats;
extern PreprocStats serviceMatchPerfStats;
extern PreprocStats luaDetectorsPerfStats;
extern PreprocStats luaCiscoPerfStats;
extern PreprocStats luaCustomPerfStats;
extern PreprocStats tpPerfStats;
extern PreprocStats tpLibPerfStats;
#endif

extern unsigned dhcp_fp_table_size;
extern unsigned long app_id_raw_packet_count;
extern unsigned long app_id_processed_packet_count;
extern unsigned long app_id_ignored_packet_count;
extern int app_id_debug;
extern unsigned isIPv4HostMonitored(uint32_t ip4, int32_t zone);
extern void checkSandboxDetection(tAppId appId);
static inline void initializePriorityArray()
{
    int i;
    for (i=0; i < SF_APPID_MAX; i++)
        appIdPriorityArray[i] = 2;
}

static inline void setAppPriority (tAppId app_id, uint8_t  bit_val)
{
    if (app_id < SF_APPID_MAX && bit_val <= APPID_MAX_PRIORITY )
    appIdPriorityArray[app_id] = bit_val;
}

static inline int getAppPriority (tAppId app_id)
{
    if (app_id > APP_ID_NONE && app_id < SF_APPID_MAX)
        return  appIdPriorityArray[app_id] ;
    else
        return -1;
}

static inline int ThirdPartyAppIDFoundProto(tAppId proto, tAppId* proto_list)
{
    unsigned int proto_cnt = 0;
    while (proto_list[proto_cnt] != APP_ID_NONE)
        if (proto_list[proto_cnt++] == proto)
            return 1;    // found
    return 0;            // not found
}
static inline int TPIsAppIdDone(void *tpSession)
{
    if (thirdparty_appid_module)
    {
        unsigned state;

        if (tpSession)
            state = thirdparty_appid_module->session_state_get(tpSession);
        else
            state = TP_STATE_INIT;
        return (state  == TP_STATE_CLASSIFIED || state == TP_STATE_TERMINATED || state == TP_STATE_HA);
    }
    return true;
}

static inline int TPIsAppIdAvailable(void * tpSession)
{
    if (thirdparty_appid_module)
    {
        unsigned state;

        if (tpSession)
            state = thirdparty_appid_module->session_state_get(tpSession);
        else
            state = TP_STATE_INIT;
        return (state == TP_STATE_CLASSIFIED || state == TP_STATE_TERMINATED || state == TP_STATE_MONITORING);
    }
    return true;
}

static inline tAppId isAppDetectionDone(tAppIdData *flow)
{
    return getAppIdExtFlag(flow, APPID_SESSION_SERVICE_DETECTED);
}

static inline tAppId pickServiceAppId(tAppIdData *flow)
{
    tAppId rval;

    if (!flow || flow->common.fsf_type.flow_type != APPID_SESSION_TYPE_NORMAL)
        return APP_ID_NONE;

    if (getAppIdExtFlag(flow, APPID_SESSION_SERVICE_DETECTED))
    {
        bool deferred = appInfoEntryFlagGet(flow->serviceAppId, APPINFO_FLAG_DEFER, appIdActiveConfigGet()) || appInfoEntryFlagGet(flow->tpAppId, APPINFO_FLAG_DEFER, appIdActiveConfigGet());

        if (flow->serviceAppId > APP_ID_NONE && !deferred)
            return flow->serviceAppId;
        if (TPIsAppIdAvailable(flow->tpsession))
        {
            if (flow->tpAppId > APP_ID_NONE)
                return flow->tpAppId;
            else if (deferred)
                return flow->serviceAppId;
            else
                rval = APP_ID_UNKNOWN_UI;
        }
        else
            rval = flow->tpAppId;
    }
    else if (flow->tpAppId > APP_ID_NONE)
        return flow->tpAppId;
    else
        rval = APP_ID_NONE;

    if (flow->clientServiceAppId > APP_ID_NONE)
        return flow->clientServiceAppId;

    if (flow->portServiceAppId > APP_ID_NONE)
        return flow->portServiceAppId;

    return rval;
}

static inline tAppId pickOnlyServiceAppId(tAppIdData *flow)
{
    if (!flow || flow->common.fsf_type.flow_type != APPID_SESSION_TYPE_NORMAL)
        return APP_ID_NONE;

    bool deferred = appInfoEntryFlagGet(flow->serviceAppId, APPINFO_FLAG_DEFER, appIdActiveConfigGet()) || appInfoEntryFlagGet(flow->tpAppId, APPINFO_FLAG_DEFER, appIdActiveConfigGet());

    if (flow->serviceAppId > APP_ID_NONE && !deferred)
        return flow->serviceAppId;

    if (TPIsAppIdAvailable(flow->tpsession) && flow->tpAppId > APP_ID_NONE)
        return flow->tpAppId;
    else if (deferred)
        return flow->serviceAppId;

    if (flow->serviceAppId < APP_ID_NONE)
        return APP_ID_UNKNOWN_UI;

    return APP_ID_NONE;
}

static inline tAppId pickMiscAppId(tAppIdData *flow)
{
    if (!flow || flow->common.fsf_type.flow_type != APPID_SESSION_TYPE_NORMAL)
        return APP_ID_NONE;
    if (flow->miscAppId > APP_ID_NONE)
        return flow->miscAppId;
    return APP_ID_NONE;
}

static inline tAppId pickClientAppId(tAppIdData *flow)
{
    if (!flow || flow->common.fsf_type.flow_type != APPID_SESSION_TYPE_NORMAL)
        return APP_ID_NONE;
    if (flow->clientAppId > APP_ID_NONE)
        return flow->clientAppId;
    return APP_ID_NONE;
}

static inline tAppId pickPayloadId(tAppIdData *flow)
{
    if (!flow || flow->common.fsf_type.flow_type != APPID_SESSION_TYPE_NORMAL)
        return APP_ID_NONE;
    if (flow->payloadAppId > APP_ID_NONE)
        return flow->payloadAppId;
    if (flow->tpPayloadAppId > APP_ID_NONE)
        return flow->tpPayloadAppId;
    return APP_ID_NONE;
}

static inline tAppId pickReferredPayloadId(tAppIdData *flow)
{
    if (!flow || flow->common.fsf_type.flow_type != APPID_SESSION_TYPE_NORMAL)
        return APP_ID_NONE;
    if (flow->referredPayloadAppId > APP_ID_NONE)
        return flow->referredPayloadAppId;
    return APP_ID_NONE;
}
static inline tAppId fwPickServiceAppId(tAppIdData *session)
{
    tAppId appId;
    appId = pickServiceAppId(session);
    if (appId == APP_ID_NONE)
        appId = session->encrypted.serviceAppId;
    return appId;
}

static inline tAppId fwPickMiscAppId(tAppIdData *session)
{
    tAppId appId;
    appId = pickMiscAppId(session);
    if (appId == APP_ID_NONE)
        appId = session->encrypted.miscAppId;
    return appId;
}

static inline tAppId fwPickClientAppId(tAppIdData *session)
{
    tAppId appId;
    appId = pickClientAppId(session);
    return appId;
}

static inline tAppId fwPickPayloadAppId(tAppIdData *session)
{
    tAppId appId;
    appId = pickPayloadId(session);
    if (appId == APP_ID_NONE)
        appId = session->encrypted.payloadAppId;
    return appId;
}

static inline tAppId fwPickReferredPayloadAppId(tAppIdData *session)
{
    tAppId appId;
    appId = pickReferredPayloadId(session);
    if (appId == APP_ID_NONE)
        appId = session->encrypted.referredAppId;
    return appId;
}

static inline tAppIdData* appSharedGetData(const SFSnortPacket *p)
{
    return _dpd.sessionAPI->get_application_data(p->stream_session, PP_APP_ID);
}

static inline unsigned int isFwSessionSslDecrypted(tAppIdData *session)
{
    return getAppIdExtFlag(session, APPID_SESSION_DECRYPTED);
}
static inline int testSSLAppIdForReinspect (tAppId app_id)
{
    if (app_id <= SF_APPID_MAX && (app_id == APP_ID_SSL || appInfoEntryFlagGet(app_id, APPINFO_FLAG_SSL_INSPECT, appIdActiveConfigGet())))
        return 1;
    else
        return 0;
}
#endif
