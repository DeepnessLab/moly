/****************************************************************************
 *
 * Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 * Copyright (C) 2003-2013 Sourcefire, Inc.
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

/**
**  @file       preproc_setup.c
**
**  @author     Daniel Roelker <droelker@sourcefire.com>
**
**  @brief      This file initializes HttpInspect as a Snort
**              preprocessor.
**
**  This file registers the HttpInspect initialization function,
**  adds the HttpInspect function into the preprocessor list, reads
**  the user configuration in the snort.conf file, and prints out
**  the configuration that is read.
**
**  In general, this file is a wrapper to HttpInspect functionality,
**  by interfacing with the Snort preprocessor functions.  The rest
**  of HttpInspect should be separate from the preprocessor hooks.
**
**  NOTES
**
**  - 2.10.03:  Initial Development.  DJR
*/

#include <assert.h>
#include <string.h>
#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "decode.h"
#include "plugbase.h"
#include "snort_debug.h"
#include "util.h"
#include "parser.h"

#include "hi_ui_config.h"
#include "hi_ui_server_lookup.h"
#include "hi_client.h"
#include "hi_norm.h"
#include "snort_httpinspect.h"
#include "hi_util_kmap.h"
#include "hi_util_xmalloc.h"
#include "hi_cmd_lookup.h"
#include "hi_paf.h"

#include "snort.h"
#include "profiler.h"
#include "mstring.h"
#include "sp_preprocopt.h"
#include "detection_util.h"

#ifdef TARGET_BASED
#include "stream_api.h"
#include "sftarget_protocol_reference.h"
#endif
#include "snort_stream5_session.h"
#include "sfPolicy.h"
#include "mempool.h"
#include "file_api.h"
#include "sf_email_attach_decode.h"
/*
**  Defines for preprocessor initialization
*/
/**
**  snort.conf preprocessor keyword
*/
#define GLOBAL_KEYWORD   "http_inspect"
#define SERVER_KEYWORD   "http_inspect_server"

const char *PROTOCOL_NAME = "HTTP";

/**
**  The length of the error string buffer.
*/
#define ERRSTRLEN 1000

/*
**  External Global Variables
**  Variables that we need from Snort to log errors correctly and such.
*/
extern char *file_name;
extern int file_line;

/*
**  Global Variables
**  This is the only way to work with Snort preprocessors because
**  the user configuration must be kept between the Init function
**  the actual preprocessor.  There is no interaction between the
**  two except through global variable usage.
*/
tSfPolicyUserContextId hi_config = NULL;

#ifdef TARGET_BASED
/* Store the protocol id received from the stream reassembler */
int16_t hi_app_protocol_id;
#endif

#ifdef PERF_PROFILING
PreprocStats hiPerfStats;
PreprocStats hiDetectPerfStats;
int hiDetectCalled = 0;
#endif

static tSfPolicyId httpCurrentPolicy = 0;

#ifdef ZLIB
MemPool *hi_gzip_mempool = NULL;
uint8_t decompression_buffer[65535];
#endif

uint8_t dechunk_buffer[65535];

MemPool *http_mempool = NULL;
MemPool *mime_decode_mempool = NULL;
MemPool *mime_log_mempool = NULL;
int hex_lookup[256];
int valid_lookup[256];
/*
** Prototypes
*/
static void HttpInspectDropStats(int);
static void HttpInspect(Packet *, void *);
static void HttpInspectCleanExit(int, void *);
static void HttpInspectReset(int, void *);
static void HttpInspectResetStats(int, void *);
static void HttpInspectInit(struct _SnortConfig *, char *);
static void addServerConfPortsToStream5(struct _SnortConfig *sc, void *);
static void HttpInspectFreeConfigs(tSfPolicyUserContextId);
static void HttpInspectFreeConfig(HTTPINSPECT_GLOBAL_CONF *);
static int HttpInspectCheckConfig(struct _SnortConfig *);
static void HttpInspectAddPortsOfInterest(struct _SnortConfig *, HTTPINSPECT_GLOBAL_CONF *, tSfPolicyId);
static int HttpEncodeInit(struct _SnortConfig *, char *, char *, void **);
static int HttpEncodeEval(void *, const uint8_t **, void *);
static void HttpEncodeCleanup(void *);
static void HttpInspectRegisterRuleOptions(struct _SnortConfig *);
static void HttpInspectRegisterXtraDataFuncs(HTTPINSPECT_GLOBAL_CONF *);
static inline void InitLookupTables(void);
#ifdef TARGET_BASED
static void HttpInspectAddServicesOfInterest(struct _SnortConfig *, tSfPolicyId);
#endif

#ifdef SNORT_RELOAD
static void HttpInspectReloadGlobal(struct _SnortConfig *, char *, void **);
static void HttpInspectReload(struct _SnortConfig *, char *, void **);
static int HttpInspectReloadVerify(struct _SnortConfig *, void *);
static void * HttpInspectReloadSwap(struct _SnortConfig *, void *);
static void HttpInspectReloadSwapFree(void *);
#endif


/*
**  NAME
**    HttpInspect::
*/
/**
**  This function wraps the functionality in the generic HttpInspect
**  processing.  We get a Packet structure and pass this into the
**  HttpInspect module where the first stage in HttpInspect is the
**  Session Inspection stage where most of the other Snortisms are
**  taken care of.  After that, the modules should be fairly generic,
**  and that's what we're trying to do here.
**
**  @param p a Packet structure that contains Snort info about the
**  packet.
**
**  @return void
*/
static void HttpInspect(Packet *p, void *context)
{
    tSfPolicyId policy_id = getRuntimePolicy();
    HTTPINSPECT_GLOBAL_CONF *pPolicyConfig = NULL ;
    PROFILE_VARS;
    sfPolicyUserPolicySet (hi_config, policy_id);
    pPolicyConfig = (HTTPINSPECT_GLOBAL_CONF *)sfPolicyUserDataGetCurrent(hi_config);

    if ( pPolicyConfig == NULL)
        return;

    // preconditions - what we registered for
    assert(IsTCP(p) && p->dsize && p->data);

    PREPROC_PROFILE_START(hiPerfStats);

    /*
    **  Pass in the configuration and the packet.
    */
    SnortHttpInspect(pPolicyConfig, p);

    ClearHttpBuffers();

    /* XXX:
     * NOTE: this includes the HTTPInspect directly
     * calling the detection engine -
     * to get the true HTTPInspect only stats, have another
     * var inside SnortHttpInspect that tracks the time
     * spent in Detect().
     * Subtract the ticks from this if iCallDetect == 0
     */
    PREPROC_PROFILE_END(hiPerfStats);
#ifdef PERF_PROFILING
    if (hiDetectCalled)
    {
        hiPerfStats.ticks -= hiDetectPerfStats.ticks;
        /* And Reset ticks to 0 */
        hiDetectPerfStats.ticks = 0;
        hiDetectCalled = 0;
    }
#endif

    return;
}

static void HttpInspectDropStats(int exiting)
{
    if(!hi_stats.total)
        return;

    LogMessage("HTTP Inspect - encodings (Note: stream-reassembled "
               "packets included):\n");

#ifdef WIN32
    LogMessage("    POST methods:                         %-10I64u\n", hi_stats.post);
    LogMessage("    GET methods:                          %-10I64u\n", hi_stats.get);
    LogMessage("    HTTP Request Headers extracted:       %-10I64u\n", hi_stats.req_headers);
#ifdef DEBUG
    if (hi_stats.req_headers == 0)
    LogMessage("    Avg Request Header length:            %-10s\n", "n/a");
    else
    LogMessage("    Avg Request Header length:            %-10.2f\n", (double)hi_stats.req_header_len / (double)hi_stats.req_headers);
#endif
    LogMessage("    HTTP Request cookies extracted:       %-10I64u\n", hi_stats.req_cookies);
#ifdef DEBUG
    if (hi_stats.req_cookies == 0)
    LogMessage("    Avg Request Cookie length:            %-10s\n", "n/a");
    else
    LogMessage("    Avg Request Cookie length:            %-10.2f\n", (double)hi_stats.req_cookie_len / (double)hi_stats.req_cookies);
#endif
    LogMessage("    Post parameters extracted:            %-10I64u\n", hi_stats.post_params);
    LogMessage("    HTTP Response Headers extracted:      %-10I64u\n", hi_stats.resp_headers);
#ifdef DEBUG
    if (hi_stats.resp_headers == 0)
    LogMessage("    Avg Response Header length:           %-10s\n", "n/a");
    else
    LogMessage("    Avg Response Header length:           %-10.2f\n", (double)hi_stats.resp_header_len / (double)hi_stats.resp_headers);
#endif
    LogMessage("    HTTP Response cookies extracted:      %-10I64u\n", hi_stats.resp_cookies);
#ifdef DEBUG
    if (hi_stats.resp_cookies == 0)
    LogMessage("    Avg Response Cookie length:           %-10s\n", "n/a");
    else
    LogMessage("    Avg Response Cookie length:           %-10.2f\n", (double)hi_stats.resp_cookie_len / (double)hi_stats.resp_cookies);
#endif
    LogMessage("    Unicode:                              %-10I64u\n", hi_stats.unicode);
    LogMessage("    Double unicode:                       %-10I64u\n", hi_stats.double_unicode);
    LogMessage("    Non-ASCII representable:              %-10I64u\n", hi_stats.non_ascii);
    LogMessage("    Directory traversals:                 %-10I64u\n", hi_stats.dir_trav);
    LogMessage("    Extra slashes (\"//\"):                 %-10I64u\n", hi_stats.slashes);
    LogMessage("    Self-referencing paths (\"./\"):        %-10I64u\n", hi_stats.self_ref);
#ifdef ZLIB
    LogMessage("    HTTP Response Gzip packets extracted: %-10I64u\n", hi_stats.gzip_pkts);
    if (hi_stats.gzip_pkts == 0)
    {
    LogMessage("    Gzip Compressed Data Processed:       %-10s\n", "n/a");
    LogMessage("    Gzip Decompressed Data Processed:     %-10s\n", "n/a");
    }
    else
    {
    LogMessage("    Gzip Compressed Data Processed:       %-10.2f\n", (double)hi_stats.compr_bytes_read);
    LogMessage("    Gzip Decompressed Data Processed:     %-10.2f\n", (double)hi_stats.decompr_bytes_read);
    }
#endif
    LogMessage("    Total packets processed:              %-10I64u\n", hi_stats.total);
#else
    LogMessage("    POST methods:                         "FMTu64("-10")"\n", hi_stats.post);
    LogMessage("    GET methods:                          "FMTu64("-10")"\n", hi_stats.get);
    LogMessage("    HTTP Request Headers extracted:       "FMTu64("-10")"\n", hi_stats.req_headers);
#ifdef DEBUG
    if (hi_stats.req_headers == 0)
    LogMessage("    Avg Request Header length:            %-10s\n", "n/a");
    else
    LogMessage("    Avg Request Header length:            %-10.2f\n", (double)hi_stats.req_header_len / (double)hi_stats.req_headers);
#endif
    LogMessage("    HTTP Request Cookies extracted:       "FMTu64("-10")"\n", hi_stats.req_cookies);
#ifdef DEBUG
    if (hi_stats.req_cookies == 0)
    LogMessage("    Avg Request Cookie length:            %-10s\n", "n/a");
    else
    LogMessage("    Avg Request Cookie length:            %-10.2f\n", (double)hi_stats.req_cookie_len / (double)hi_stats.req_cookies);
#endif
    LogMessage("    Post parameters extracted:            "FMTu64("-10")"\n", hi_stats.post_params);
    LogMessage("    HTTP response Headers extracted:      "FMTu64("-10")"\n", hi_stats.resp_headers);
#ifdef DEBUG
    if (hi_stats.resp_headers == 0)
    LogMessage("    HTTP response Avg Header length:      %-10s\n", "n/a");
    else
    LogMessage("    Avg Response Header length:           %-10.2f\n", (double)hi_stats.resp_header_len / (double)hi_stats.resp_headers);
#endif
    LogMessage("    HTTP Response Cookies extracted:      "FMTu64("-10")"\n", hi_stats.resp_cookies);
#ifdef DEBUG
    if (hi_stats.resp_cookies == 0)
    LogMessage("    Avg Response Cookie length:           %-10s\n", "n/a");
    else
    LogMessage("    Avg Response Cookie length:           %-10.2f\n", (double)hi_stats.resp_cookie_len / (double)hi_stats.resp_cookies);
#endif
    LogMessage("    Unicode:                              "FMTu64("-10")"\n", hi_stats.unicode);
    LogMessage("    Double unicode:                       "FMTu64("-10")"\n", hi_stats.double_unicode);
    LogMessage("    Non-ASCII representable:              "FMTu64("-10")"\n", hi_stats.non_ascii);
    LogMessage("    Directory traversals:                 "FMTu64("-10")"\n", hi_stats.dir_trav);
    LogMessage("    Extra slashes (\"//\"):                 "FMTu64("-10")"\n", hi_stats.slashes);
    LogMessage("    Self-referencing paths (\"./\"):        "FMTu64("-10")"\n", hi_stats.self_ref);
#ifdef ZLIB
    LogMessage("    HTTP Response Gzip packets extracted: "FMTu64("-10")"\n", hi_stats.gzip_pkts);
    if (hi_stats.gzip_pkts == 0)
    {
    LogMessage("    Gzip Compressed Data Processed:       %-10s\n", "n/a");
    LogMessage("    Gzip Decompressed Data Processed:     %-10s\n", "n/a");
    }
    else
    {
    LogMessage("    Gzip Compressed Data Processed:       %-10.2f\n", (double)hi_stats.compr_bytes_read);
    LogMessage("    Gzip Decompressed Data Processed:     %-10.2f\n", (double)hi_stats.decompr_bytes_read);
    }
#endif
    LogMessage("    Total packets processed:              "FMTu64("-10")"\n", hi_stats.total);
#endif
}

static void HttpInspectCleanExit(int signal, void *data)
{
    hi_paf_term();

    HI_SearchFree();

    HttpInspectFreeConfigs(hi_config);

#ifdef ZLIB
    if (mempool_destroy(hi_gzip_mempool) == 0)
    {
        free(hi_gzip_mempool);
        hi_gzip_mempool = NULL;
    }
#endif

    if (mempool_destroy(http_mempool) == 0)
    {
        free(http_mempool);
        http_mempool = NULL;
    }
    if (mempool_destroy(mime_decode_mempool) == 0)
    {
        free(mime_decode_mempool);
        mime_decode_mempool = NULL;
    }
    if (mempool_destroy(mime_log_mempool) == 0)
    {
        free(mime_log_mempool);
        mime_log_mempool = NULL;
    }

}

static void HttpInspectReset(int signal, void *data)
{
    return;
}

static void HttpInspectResetStats(int signal, void *data)
{
    memset(&hi_stats, 0, sizeof(hi_stats));
}

#ifdef ZLIB
static void SetMaxGzipSession(HTTPINSPECT_GLOBAL_CONF *pPolicyConfig)
{
    pPolicyConfig->max_gzip_sessions =
        pPolicyConfig->max_gzip_mem / sizeof(DECOMPRESS_STATE);
}

static void CheckGzipConfig(HTTPINSPECT_GLOBAL_CONF *pPolicyConfig,
        tSfPolicyUserContextId context)
{
    HTTPINSPECT_GLOBAL_CONF *defaultConfig =
        (HTTPINSPECT_GLOBAL_CONF *)sfPolicyUserDataGetDefault(context);

    if (pPolicyConfig == defaultConfig)
    {
        if (!pPolicyConfig->max_gzip_mem)
            pPolicyConfig->max_gzip_mem = DEFAULT_MAX_GZIP_MEM;

        if (!pPolicyConfig->compr_depth)
            pPolicyConfig->compr_depth = DEFAULT_COMP_DEPTH;

        if (!pPolicyConfig->decompr_depth)
            pPolicyConfig->decompr_depth = DEFAULT_DECOMP_DEPTH;

        SetMaxGzipSession(pPolicyConfig);
    }
    else if (defaultConfig == NULL)
    {
        if (pPolicyConfig->max_gzip_mem)
        {
            FatalError("http_inspect: max_gzip_mem must be "
                    "configured in the default policy.\n");
        }

        if (pPolicyConfig->compr_depth)
        {
            FatalError("http_inspect: compress_depth must be "
                    "configured in the default policy.\n");
        }

        if (pPolicyConfig->decompr_depth)
        {
            FatalError("http_inspect: decompress_depth must be "
                    "configured in the default policy.\n");
        }
    }
    else
    {
        pPolicyConfig->max_gzip_mem = defaultConfig->max_gzip_mem;
        pPolicyConfig->compr_depth = defaultConfig->compr_depth;
        pPolicyConfig->decompr_depth = defaultConfig->decompr_depth;
        pPolicyConfig->max_gzip_sessions = defaultConfig->max_gzip_sessions;
    }
}
#endif


static void CheckMemcap(HTTPINSPECT_GLOBAL_CONF *pPolicyConfig,
        tSfPolicyUserContextId context)
{
    HTTPINSPECT_GLOBAL_CONF *defaultConfig =
        (HTTPINSPECT_GLOBAL_CONF *)sfPolicyUserDataGetDefault(context);

    if (pPolicyConfig == defaultConfig)
    {
        if (!pPolicyConfig->memcap)
            pPolicyConfig->memcap = DEFAULT_HTTP_MEMCAP;

    }
    else if (defaultConfig == NULL)
    {
        if (pPolicyConfig->memcap)
        {
            FatalError("http_inspect: memcap must be "
                    "configured in the default policy.\n");
        }

    }
    else
    {
        pPolicyConfig->memcap = defaultConfig->memcap;
    }
}

/*
 **  NAME
 **    HttpInspectInit::
*/
/**
**  This function initializes HttpInspect with a user configuration.
**
**  The function is called when HttpInspect is configured in
**  snort.conf.  It gets passed a string of arguments, which gets
**  parsed into configuration constructs that HttpInspect understands.
**
**  This function gets called for every HttpInspect configure line.  We
**  use this characteristic to split up the configuration, so each line
**  is a configuration construct.  We need to keep track of what part
**  of the configuration has been configured, so we don't configure one
**  part, then configure it again.
**
**  Any upfront memory is allocated here (if necessary).
**
**  @param args a string to the preprocessor arguments.
**
**  @return void
*/
static void HttpInspectInit(struct _SnortConfig *sc, char *args)
{
    char ErrorString[ERRSTRLEN];
    int  iErrStrLen = ERRSTRLEN;
    int  iRet;
    char *pcToken;
    HTTPINSPECT_GLOBAL_CONF *pPolicyConfig = NULL;
    tSfPolicyId policy_id = getParserPolicy(sc);

    ErrorString[0] = '\0';

    if ((args == NULL) || (strlen(args) == 0))
        ParseError("No arguments to HttpInspect configuration.");

    /* Find out what is getting configured */
    pcToken = strtok(args, CONF_SEPARATORS);
    if (pcToken == NULL)
    {
        FatalError("%s(%d)strtok returned NULL when it should not.",
                   __FILE__, __LINE__);
    }

    if (hi_config == NULL)
    {
        hi_config = sfPolicyConfigCreate();
        memset(&hi_stats, 0, sizeof(HIStats));

        /*
         **  Remember to add any cleanup functions into the appropriate
         **  lists.
         */
        AddFuncToPreprocCleanExitList(HttpInspectCleanExit, NULL, PRIORITY_APPLICATION, PP_HTTPINSPECT);
        AddFuncToPreprocResetList(HttpInspectReset, NULL, PRIORITY_APPLICATION, PP_HTTPINSPECT);
        AddFuncToPreprocResetStatsList(HttpInspectResetStats, NULL, PRIORITY_APPLICATION, PP_HTTPINSPECT);
        AddFuncToConfigCheckList(sc, HttpInspectCheckConfig);

        RegisterPreprocStats("http_inspect", HttpInspectDropStats);

#ifdef PERF_PROFILING
        RegisterPreprocessorProfile("httpinspect", &hiPerfStats, 0, &totalPerfStats);
#endif

#ifdef TARGET_BASED
        /* Find and cache protocol ID for packet comparison */
        hi_app_protocol_id = AddProtocolReference("http");
#endif
        hi_paf_init(0);  // FIXTHIS is cap needed?
        HI_SearchInit();
    }

    /*
    **  Global Configuration Processing
    **  We only process the global configuration once, but always check for
    **  user mistakes, like configuring more than once.  That's why we
    **  still check for the global token even if it's been checked.
    **  Force the first configuration to be the global one.
    */
    sfPolicyUserPolicySet (hi_config, policy_id);
    pPolicyConfig = (HTTPINSPECT_GLOBAL_CONF *)sfPolicyUserDataGetCurrent(hi_config);
    if (pPolicyConfig == NULL)
    {
        if (strcasecmp(pcToken, GLOBAL) != 0)
        {
            ParseError("Must configure the http inspect global "
                       "configuration first.");
        }

        HttpInspectRegisterRuleOptions(sc);

        pPolicyConfig = (HTTPINSPECT_GLOBAL_CONF *)SnortAlloc(sizeof(HTTPINSPECT_GLOBAL_CONF));
        if (!pPolicyConfig)
        {
             ParseError("HTTP INSPECT preprocessor: memory allocate failed.\n");
        }

        sfPolicyUserDataSetCurrent(hi_config, pPolicyConfig);

        iRet = HttpInspectInitializeGlobalConfig(pPolicyConfig,
                                                 ErrorString, iErrStrLen);
        if (iRet == 0)
        {
            iRet = ProcessGlobalConf(pPolicyConfig,
                                     ErrorString, iErrStrLen);

            if (iRet == 0)
            {
#ifdef ZLIB
                CheckGzipConfig(pPolicyConfig, hi_config);
#endif
                CheckMemcap(pPolicyConfig, hi_config);
                PrintGlobalConf(pPolicyConfig);

                /* Add HttpInspect into the preprocessor list */
                if ( pPolicyConfig->disabled )
                    return;
                AddFuncToPreprocList(sc, HttpInspect, PRIORITY_APPLICATION, PP_HTTPINSPECT, PROTO_BIT__TCP);
            }
        }
    }
    else
    {
        if (strcasecmp(pcToken, SERVER) != 0)
        {
            if (strcasecmp(pcToken, GLOBAL) != 0)
                ParseError("Must configure the http inspect global configuration first.");
            else
                ParseError("Invalid http inspect token: %s.", pcToken);
        }

        iRet = ProcessUniqueServerConf(pPolicyConfig,
                                       ErrorString, iErrStrLen);
    }



    if (iRet)
    {
        if(iRet > 0)
        {
            /*
            **  Non-fatal Error
            */
            if(*ErrorString)
            {
                ErrorMessage("%s(%d) => %s\n",
                        file_name, file_line, ErrorString);
            }
        }
        else
        {
            /*
            **  Fatal Error, log error and exit.
            */
            if(*ErrorString)
            {
                FatalError("%s(%d) => %s\n",
                        file_name, file_line, ErrorString);
            }
            else
            {
                /*
                **  Check if ErrorString is undefined.
                */
                if(iRet == -2)
                {
                    FatalError("%s(%d) => ErrorString is undefined.\n",
                            file_name, file_line);
                }
                else
                {
                    FatalError("%s(%d) => Undefined Error.\n",
                            file_name, file_line);
                }
            }
        }
    }

}

/*
**  NAME
**    SetupHttpInspect::
*/
/**
**  This function initializes HttpInspect as a Snort preprocessor.
**
**  It registers the preprocessor keyword for use in the snort.conf
**  and sets up the initialization module for the preprocessor, in
**  case it is configured.
**
**  This function must be called in InitPreprocessors() in plugbase.c
**  in order to be recognized by Snort.
**
**  @param none
**
**  @return void
*/
void SetupHttpInspect(void)
{
#ifndef SNORT_RELOAD
    RegisterPreprocessor(GLOBAL_KEYWORD, HttpInspectInit);
    RegisterPreprocessor(SERVER_KEYWORD, HttpInspectInit);
#else
    RegisterPreprocessor(GLOBAL_KEYWORD, HttpInspectInit, HttpInspectReloadGlobal,
                         HttpInspectReloadVerify, HttpInspectReloadSwap,
                         HttpInspectReloadSwapFree);
    RegisterPreprocessor(SERVER_KEYWORD, HttpInspectInit,
                         HttpInspectReload, NULL, NULL, NULL);
#endif
    InitLookupTables();
    InitJSNormLookupTable();

    DEBUG_WRAP(DebugMessage(DEBUG_HTTPINSPECT, "Preprocessor: HttpInspect is "
                "setup . . .\n"););
}

static void HttpInspectRegisterRuleOptions(struct _SnortConfig *sc)
{
    RegisterPreprocessorRuleOption(sc, "http_encode", &HttpEncodeInit,
                                    &HttpEncodeEval, &HttpEncodeCleanup , NULL, NULL, NULL, NULL );
}

static void HttpInspectRegisterXtraDataFuncs(HTTPINSPECT_GLOBAL_CONF *pPolicyConfig)
{
    if (!stream_api || !pPolicyConfig)
        return;

    pPolicyConfig->xtra_trueip_id = stream_api->reg_xtra_data_cb(GetHttpTrueIP);
    pPolicyConfig->xtra_uri_id = stream_api->reg_xtra_data_cb(GetHttpUriData);
    pPolicyConfig->xtra_hname_id = stream_api->reg_xtra_data_cb(GetHttpHostnameData);
#ifndef SOURCEFIRE
#ifdef ZLIB
    pPolicyConfig->xtra_gzip_id = stream_api->reg_xtra_data_cb(GetHttpGzipData);
#endif
    pPolicyConfig->xtra_jsnorm_id = stream_api->reg_xtra_data_cb(GetHttpJSNormData);
#endif

}

static void updateConfigFromFileProcessing (HTTPINSPECT_GLOBAL_CONF *pPolicyConfig)
{
    HTTPINSPECT_CONF *ServerConf = pPolicyConfig->global_server;
    /*Either one is unlimited*/
    int64_t fileDepth = file_api->get_max_file_depth();

    /*Config file policy*/
    if (fileDepth > -1)
    {
        ServerConf->inspect_response = 1;
        ServerConf->extract_gzip = 1;
        ServerConf->log_uri = 1;
        ServerConf->unlimited_decompress = 1;
        pPolicyConfig->mime_conf.log_filename = 1;
    }

    if (!fileDepth || (!ServerConf->server_flow_depth))
        ServerConf->server_extract_size = 0;
    else if (ServerConf->server_flow_depth > fileDepth)
        ServerConf->server_extract_size = ServerConf->server_flow_depth;
    else
        ServerConf->server_extract_size = fileDepth;

    if (!fileDepth || (!ServerConf->post_depth))
        ServerConf->post_extract_size = 0;
    else if (ServerConf->post_depth > fileDepth)
        ServerConf->post_extract_size = ServerConf->post_depth;
    else
        ServerConf->post_extract_size = fileDepth;

}

static int HttpInspectVerifyPolicy(struct _SnortConfig *sc, tSfPolicyUserContextId config,
        tSfPolicyId policyId, void* pData)
{
    HTTPINSPECT_GLOBAL_CONF *pPolicyConfig = (HTTPINSPECT_GLOBAL_CONF *)pData;

    HttpInspectRegisterXtraDataFuncs(pPolicyConfig);

    if ( pPolicyConfig->disabled )
        return 0;

    if (!stream_api || (stream_api->version < STREAM_API_VERSION5))
    {
        ErrorMessage("HttpInspectConfigCheck() Streaming & reassembly "
                     "must be enabled\n");
        return -1;
    }


    if (pPolicyConfig->global_server == NULL)
    {
        ErrorMessage("HttpInspectConfigCheck() default server configuration "
                     "not specified\n");
        return -1;
    }

#ifdef TARGET_BASED
    HttpInspectAddServicesOfInterest(sc, policyId);
#endif
    updateConfigFromFileProcessing(pPolicyConfig);
    HttpInspectAddPortsOfInterest(sc, pPolicyConfig, policyId);
    return 0;
}


/** Add ports configured for http preprocessor to stream5 port filtering so that if
 * any_any rules are being ignored them the the packet still reaches http-inspect.
 *
 * For ports in global_server configuration, server_lookup,
 * add the port to stream5 port filter list.
 */
static void HttpInspectAddPortsOfInterest(struct _SnortConfig *sc, HTTPINSPECT_GLOBAL_CONF *config, tSfPolicyId policy_id)
{
    if (config == NULL)
        return;

    httpCurrentPolicy = policy_id;

    addServerConfPortsToStream5(sc, (void *)config->global_server);
    hi_ui_server_iterate(sc, config->server_lookup, addServerConfPortsToStream5);
}

/**Add server ports from http_inspect preprocessor from snort.comf file to pass through
 * port filtering.
 */
static void addServerConfPortsToStream5(struct _SnortConfig *sc, void *pData)
{
    unsigned int i;

    HTTPINSPECT_CONF *pConf = (HTTPINSPECT_CONF *)pData;
    if (pConf)
    {
        for (i = 0; i < MAXPORTS; i++)
        {
            if (pConf->ports[i/8] & (1 << (i % 8) ))
            {
                bool client = (pConf->client_flow_depth > -1);
                bool server = (pConf->server_extract_size > -1);
                int64_t fileDepth = file_api->get_max_file_depth();

                //Add port the port
                stream_api->set_port_filter_status
                    (sc, IPPROTO_TCP, (uint16_t)i, PORT_MONITOR_SESSION, httpCurrentPolicy, 1);

                // there is a fundamental issue here in that both hi and s5
                // can configure ports per ip independently of each other.
                // as is, we enable paf for all http servers if any server
                // has a flow depth enabled (per direction).  still, if eg
                // all server_flow_depths are -1, we will only enable client.
                if (fileDepth > 0)
                    hi_paf_register_port(sc, (uint16_t)i, client, server, httpCurrentPolicy, true);
                else
                    hi_paf_register_port(sc, (uint16_t)i, client, server, httpCurrentPolicy, false);
            }
        }
    }
}

#ifdef TARGET_BASED
/**
 * @param service ordinal number of service.
 */
static void HttpInspectAddServicesOfInterest(struct _SnortConfig *sc, tSfPolicyId policy_id)
{
    /* Add ordinal number for the service into stream5 */
    if (hi_app_protocol_id != SFTARGET_UNKNOWN_PROTOCOL)
    {
        stream_api->set_service_filter_status(sc, hi_app_protocol_id, PORT_MONITOR_SESSION, policy_id, 1);

        if (file_api->get_max_file_depth() > 0)
            hi_paf_register_service(sc, hi_app_protocol_id, true, true, httpCurrentPolicy, true);
        else
            hi_paf_register_service(sc, hi_app_protocol_id, true, true, httpCurrentPolicy, false);
    }
}
#endif

typedef struct _HttpEncodeData
{
    int http_type;
    int encode_type;
}HttpEncodeData;

static int HttpEncodeInit(struct _SnortConfig *sc, char *name, char *parameters, void **dataPtr)
{
    char **toks, **toks1;
    int num_toks, num_toks1;
    int i;
    char *etype;
    char *btype;
    char *findStr1, *findStr2;
    int negate_flag = 0;
    unsigned pos;
    HttpEncodeData *idx= NULL;

    idx = (HttpEncodeData *) SnortAlloc(sizeof(HttpEncodeData));

    if(idx == NULL)
    {
        FatalError("%s(%d): Failed allocate data for %s option\n",
            file_name, file_line, name);
    }


    toks = mSplit(parameters, ",", 2, &num_toks, 0);

    if(num_toks != 2 )
    {
        FatalError("%s (%d): %s option takes two parameters \n",
            file_name, file_line, name);
    }

    btype = toks[0];
    if(!strcasecmp(btype, "uri"))
    {
        idx->http_type = HTTP_BUFFER_URI;
    }
    else if(!strcasecmp(btype, "header"))
    {
        idx->http_type = HTTP_BUFFER_HEADER;
    }
    /* This keyword will not be used until post normalization is turned on */
    /*else if(!strcasecmp(btype, "post"))
    {
        idx->http_type = HTTP_BUFFER_CLIENT_BODY;
    }*/
    else if(!strcasecmp(btype, "cookie"))
    {
        idx->http_type = HTTP_BUFFER_COOKIE;
    }
    /*check for a negation when OR is present. OR and negation is not supported*/
    findStr1 = strchr(toks[1], '|');
    if( findStr1 )
    {
        findStr2 = strchr(toks[1], '!' );
        if( findStr2 )
        {
            FatalError("%s (%d): \"|\" is not supported in conjunction with \"!\" for %s option \n",
                    file_name, file_line, name);
        }

        pos = findStr1 - toks[1];
        if ( pos == 0 || pos == (strlen(toks[1]) - 1) )
        {
            FatalError("%s (%d): Invalid Parameters for %s option \n",
                    file_name, file_line, name);
        }
    }

     toks1 = mSplit(toks[1], "|", 0, &num_toks1, 0);

     for(i = 0; i < num_toks1; i++)
     {
         etype = toks1[i];

         if( *etype == '!' )
         {
             negate_flag = 1;
             etype++;
             while(isspace((int)*etype)) {etype++;}
         }

         if(!strcasecmp(etype, "utf8"))
         {
             if(negate_flag)
                 idx->encode_type &= ~HTTP_ENCODE_TYPE__UTF8_UNICODE;
             else
                 idx->encode_type |= HTTP_ENCODE_TYPE__UTF8_UNICODE;
         }

         else if(!strcasecmp(etype, "double_encode"))
         {
             if(negate_flag)
                 idx->encode_type &= ~HTTP_ENCODE_TYPE__DOUBLE_ENCODE;
             else idx->encode_type |= HTTP_ENCODE_TYPE__DOUBLE_ENCODE;
         }

         else if(!strcasecmp(etype, "non_ascii"))
         {
             if(negate_flag) idx->encode_type &= ~HTTP_ENCODE_TYPE__NONASCII;
             else
                 idx->encode_type |= HTTP_ENCODE_TYPE__NONASCII;
         }

         /* Base 36 is deprecated and essentially a noop */
         else if(!strcasecmp(etype, "base36"))
         {
             ErrorMessage("WARNING: %s (%d): The \"base36\" argument to the "
                     "\"http_encode\" rule option is deprecated and void "
                     "of functionality.\n", file_name, file_line);

             /* Set encode type so we can check below to see if base36 was the
              * only argument in the encode chain */
             idx->encode_type |= HTTP_ENCODE_TYPE__BASE36;
         }

         else if(!strcasecmp(etype, "uencode"))
         {
             if(negate_flag)
                 idx->encode_type &= ~HTTP_ENCODE_TYPE__UENCODE;
             else
                 idx->encode_type |= HTTP_ENCODE_TYPE__UENCODE;
         }

         else if(!strcasecmp(etype, "bare_byte"))
         {
             if(negate_flag)
                 idx->encode_type &= ~HTTP_ENCODE_TYPE__BARE_BYTE;
             else
                 idx->encode_type |= HTTP_ENCODE_TYPE__BARE_BYTE;
         }
         else if (!strcasecmp(etype, "iis_encode"))
         {
             if(negate_flag)
                 idx->encode_type &= ~HTTP_ENCODE_TYPE__IIS_UNICODE;
             else
                 idx->encode_type |= HTTP_ENCODE_TYPE__IIS_UNICODE;
         }
         else if  (!strcasecmp(etype, "ascii"))
         {
             if(negate_flag)
                 idx->encode_type &= ~HTTP_ENCODE_TYPE__ASCII;
             else
                 idx->encode_type |= HTTP_ENCODE_TYPE__ASCII;
         }

         else
         {
             FatalError("%s(%d): Unknown modifier \"%s\" for option \"%s\"\n",
                     file_name, file_line, toks1[i], name);
         }
         negate_flag = 0;
     }

     /* Only got base36 parameter which is deprecated.  If it's the only
      * parameter in the chain make it so it always matches as if the
      * entire rule option were non-existent. */
     if (idx->encode_type == HTTP_ENCODE_TYPE__BASE36)
     {
         idx->encode_type = 0xffffffff;
     }

     *dataPtr = idx;
     mSplitFree(&toks,num_toks);
     mSplitFree(&toks1,num_toks1);

     return 0;
}


static int HttpEncodeEval(void *p, const uint8_t **cursor, void *dataPtr)
{
    Packet* pkt = p;
    HttpEncodeData* idx = (HttpEncodeData *)dataPtr;
    const HttpBuffer* hb;

    if ( !pkt || !idx )
        return DETECTION_OPTION_NO_MATCH;

    hb = GetHttpBuffer(idx->http_type);

    if ( hb && (hb->encode_type & idx->encode_type) )
        return DETECTION_OPTION_MATCH;

    return DETECTION_OPTION_NO_MATCH;
}

static void HttpEncodeCleanup(void *dataPtr)
{
    HttpEncodeData *idx = dataPtr;
    if (idx)
    {
        free(idx);
    }
}


#ifdef ZLIB
static int HttpInspectExtractGzipIterate(void *data)
{
    HTTPINSPECT_CONF *server = (HTTPINSPECT_CONF *)data;

    if (server == NULL)
        return 0;

    if (server->extract_gzip)
        return 1;

    return 0;
}

static int HttpInspectExtractGzip(struct _SnortConfig *sc,
        tSfPolicyUserContextId config,
        tSfPolicyId policyId, void *pData)
{
    HTTPINSPECT_GLOBAL_CONF *context = (HTTPINSPECT_GLOBAL_CONF *)pData;

    if (pData == NULL)
        return 0;

    if(context->disabled)
        return 0;

    if ((context->global_server != NULL) && context->global_server->extract_gzip)
        return 1;

    if (context->server_lookup != NULL)
    {
        if (sfrt_iterate2(context->server_lookup, HttpInspectExtractGzipIterate) != 0)
            return 1;
    }

    return 0;
}
#endif

static int HttpInspectExtractUriHostIterate(void *data)
{
    HTTPINSPECT_CONF *server = (HTTPINSPECT_CONF *)data;

    if (server == NULL)
        return 0;

    if (server->log_uri || server->log_hostname)
        return 1;

    return 0;
}

static int HttpInspectExtractUriHost(struct _SnortConfig *sc,
                tSfPolicyUserContextId config,
                tSfPolicyId policyId, void *pData)
{
    HTTPINSPECT_GLOBAL_CONF *context = (HTTPINSPECT_GLOBAL_CONF *)pData;

    if (pData == NULL)
        return 0;

    if(context->disabled)
        return 0;

    if ((context->global_server != NULL) && (context->global_server->log_uri || context->global_server->log_hostname))
        return 1;

    if (context->server_lookup != NULL)
    {
        if (sfrt_iterate2(context->server_lookup, HttpInspectExtractUriHostIterate) != 0)
            return 1;
    }

    return 0;
}

static int HttpEnableDecoding(struct _SnortConfig *sc,
            tSfPolicyUserContextId config,
            tSfPolicyId policyId, void *pData)
{
    HTTPINSPECT_GLOBAL_CONF *context = (HTTPINSPECT_GLOBAL_CONF *)pData;

    if (pData == NULL)
        return 0;

    if(context->disabled)
        return 0;

    if((context->global_server != NULL) && (context->global_server->post_extract_size > -1)
            && (file_api->is_decoding_enabled(&(context->decode_conf))))
        return 1;

    return 0;
}

static int HttpEnableMimeLog(struct _SnortConfig *sc,
            tSfPolicyUserContextId config,
            tSfPolicyId policyId, void *pData)
{
    HTTPINSPECT_GLOBAL_CONF *context = (HTTPINSPECT_GLOBAL_CONF *)pData;

    if (pData == NULL)
        return 0;

    if(context->disabled)
        return 0;

    if((context->global_server != NULL) && (context->global_server->post_extract_size > -1)
            && (file_api->is_mime_log_enabled(&(context->mime_conf))))
        return 1;

    return 0;
}

/*
**  NAME
**    HttpInspectCheckConfig::
*/
/**
**  This function verifies the HttpInspect configuration is complete
**
**  @return none
*/
static int HttpInspectCheckConfig(struct _SnortConfig *sc)
{
    HTTPINSPECT_GLOBAL_CONF *defaultConfig;

    if (hi_config == NULL)
        return 0;

    if (sfPolicyUserDataIterate (sc, hi_config, HttpInspectVerifyPolicy))
        return -1;

    defaultConfig = (HTTPINSPECT_GLOBAL_CONF *)sfPolicyUserDataGetDefault(hi_config);


#ifdef ZLIB
    {
        if (sfPolicyUserDataIterate(sc, hi_config, HttpInspectExtractGzip) != 0)
        {
            if (defaultConfig == NULL)
            {
                WarningMessage("http_inspect:  Must configure a default global "
                        "configuration if you want to enable gzip in any "
                        "server configuration.\n");
                return -1;
            }

            hi_gzip_mempool = (MemPool *)SnortAlloc(sizeof(MemPool));

            if (mempool_init(hi_gzip_mempool, defaultConfig->max_gzip_sessions,
                        sizeof(DECOMPRESS_STATE)) != 0)
            {
                if(defaultConfig->max_gzip_sessions)
                    FatalError("http_inspect: Error setting the \"max_gzip_mem\" \n");
                else
                    FatalError("http_inspect:  Could not allocate gzip mempool.\n");
            }
        }
    }
#endif
    if (sfPolicyUserDataIterate(sc, hi_config, HttpInspectExtractUriHost) != 0)
    {
        uint32_t max_sessions_logged;
        if (defaultConfig == NULL)
        {
            WarningMessage("http_inspect:  Must configure a default global "
                        "configuration if you want to enable logging of uri or hostname in any "
                        "server configuration.\n");
            return -1;
        }

        max_sessions_logged = defaultConfig->memcap / (MAX_URI_EXTRACTED + MAX_HOSTNAME);

        http_mempool = (MemPool *)SnortAlloc(sizeof(MemPool));
        if (mempool_init(http_mempool, max_sessions_logged, (MAX_URI_EXTRACTED + MAX_HOSTNAME)) != 0)
        {
            FatalError("http_inspect:  Could not allocate HTTP mempool.\n");
        }
    }

    if (defaultConfig)
    {
        defaultConfig->decode_conf.file_depth = file_api->get_max_file_depth();
        if (defaultConfig->decode_conf.file_depth > -1)
        {
            defaultConfig->mime_conf.log_filename = 1;
        }
    }

    if (sfPolicyUserDataIterate(sc, hi_config, HttpEnableDecoding) != 0)
    {
        if (defaultConfig == NULL)
        {
            WarningMessage("http_inspect:  Must configure a default global "
                    "configuration if you want to enable decoding in any "
                    "server configuration.\n");
            return -1;
        }
        updateMaxDepth(defaultConfig->decode_conf.file_depth, &defaultConfig->decode_conf.max_depth);
        mime_decode_mempool = (MemPool *)file_api->init_mime_mempool(defaultConfig->decode_conf.max_mime_mem,
                defaultConfig->decode_conf.max_depth, mime_decode_mempool, PROTOCOL_NAME);
    }

    if (sfPolicyUserDataIterate(sc, hi_config, HttpEnableMimeLog) != 0)
    {
        if (defaultConfig == NULL)
        {
            ErrorMessage("http_inspect:  Must configure a default global "
                    "configuration if you want to enable mime log in any "
                    "server configuration.\n");
            return -1;
        }
        mime_log_mempool = (MemPool *)file_api->init_log_mempool(0,
                defaultConfig->mime_conf.memcap, mime_log_mempool, "HTTP");
    }
    return 0;
}

static int HttpInspectFreeConfigPolicy(tSfPolicyUserContextId config,tSfPolicyId policyId, void* pData )
{
    HTTPINSPECT_GLOBAL_CONF *pPolicyConfig = (HTTPINSPECT_GLOBAL_CONF *)pData;
    HttpInspectFreeConfig(pPolicyConfig);
    sfPolicyUserDataClear (config, policyId);
    return 0;
}

static void HttpInspectFreeConfigs(tSfPolicyUserContextId config)
{

    if (config == NULL)
        return;
    sfPolicyUserDataFreeIterate (config, HttpInspectFreeConfigPolicy);
    sfPolicyConfigDelete(config);

}

static void HttpInspectFreeConfig(HTTPINSPECT_GLOBAL_CONF *config)
{
    if (config == NULL)
        return;

    hi_ui_server_lookup_destroy(config->server_lookup);

    xfree(config->iis_unicode_map_filename);
    xfree(config->iis_unicode_map);

    if (config->global_server != NULL)
    {
        http_cmd_lookup_cleanup(&(config->global_server->cmd_lookup));
        free(config->global_server);
    }

    free(config);
}

#ifdef SNORT_RELOAD
static void _HttpInspectReload(struct _SnortConfig *sc, tSfPolicyUserContextId hi_swap_config, char *args)
{
    char ErrorString[ERRSTRLEN];
    int  iErrStrLen = ERRSTRLEN;
    int  iRet;
    HTTPINSPECT_GLOBAL_CONF *pPolicyConfig = NULL;
    char *pcToken;
    tSfPolicyId policy_id = getParserPolicy(sc);

    ErrorString[0] = '\0';

    if ((args == NULL) || (strlen(args) == 0))
        ParseError("No arguments to HttpInspect configuration.");

    /* Find out what is getting configured */
    pcToken = strtok(args, CONF_SEPARATORS);
    if (pcToken == NULL)
    {
        FatalError("%s(%d)strtok returned NULL when it should not.",
                   __FILE__, __LINE__);
    }

    /*
    **  Global Configuration Processing
    **  We only process the global configuration once, but always check for
    **  user mistakes, like configuring more than once.  That's why we
    **  still check for the global token even if it's been checked.
    **  Force the first configuration to be the global one.
    */
    sfPolicyUserPolicySet (hi_swap_config, policy_id);
    pPolicyConfig = (HTTPINSPECT_GLOBAL_CONF *)sfPolicyUserDataGetCurrent(hi_swap_config);
    if (pPolicyConfig == NULL)
    {
        if (strcasecmp(pcToken, GLOBAL) != 0)
            ParseError("Must configure the http inspect global configuration first.");

        HttpInspectRegisterRuleOptions(sc);

        pPolicyConfig = (HTTPINSPECT_GLOBAL_CONF *)SnortAlloc(sizeof(HTTPINSPECT_GLOBAL_CONF));
        if (!pPolicyConfig)
        {
             ParseError("HTTP INSPECT preprocessor: memory allocate failed.\n");
        }
        sfPolicyUserDataSetCurrent(hi_swap_config, pPolicyConfig);
        iRet = HttpInspectInitializeGlobalConfig(pPolicyConfig,
                                                 ErrorString, iErrStrLen);
        if (iRet == 0)
        {
            iRet = ProcessGlobalConf(pPolicyConfig,
                                     ErrorString, iErrStrLen);

            if (iRet == 0)
            {
#ifdef ZLIB
                CheckGzipConfig(pPolicyConfig, hi_swap_config);
#endif
                CheckMemcap(pPolicyConfig, hi_swap_config);
                PrintGlobalConf(pPolicyConfig);

                /* Add HttpInspect into the preprocessor list */
                if ( pPolicyConfig->disabled )
                    return;
                AddFuncToPreprocList(sc, HttpInspect, PRIORITY_APPLICATION, PP_HTTPINSPECT, PROTO_BIT__TCP);

            }
        }
    }
    else
    {
        if (strcasecmp(pcToken, SERVER) != 0)
        {
            if (strcasecmp(pcToken, GLOBAL) != 0)
                ParseError("Must configure the http inspect global configuration first.");
            else
                ParseError("Invalid http inspect token: %s.", pcToken);
        }

        iRet = ProcessUniqueServerConf(pPolicyConfig,
                                       ErrorString, iErrStrLen);
    }

    if (iRet)
    {
        if(iRet > 0)
        {
            /*
            **  Non-fatal Error
            */
            if(*ErrorString)
            {
                ErrorMessage("%s(%d) => %s\n",
                        file_name, file_line, ErrorString);
            }
        }
        else
        {
            /*
            **  Fatal Error, log error and exit.
            */
            if(*ErrorString)
            {
                FatalError("%s(%d) => %s\n",
                        file_name, file_line, ErrorString);
            }
            else
            {
                /*
                **  Check if ErrorString is undefined.
                */
                if(iRet == -2)
                {
                    FatalError("%s(%d) => ErrorString is undefined.\n",
                            file_name, file_line);
                }
                else
                {
                    FatalError("%s(%d) => Undefined Error.\n",
                            file_name, file_line);
                }
            }
        }
    }
}

static void HttpInspectReloadGlobal(struct _SnortConfig *sc, char *args, void **new_config)
{
    tSfPolicyUserContextId hi_swap_config = (tSfPolicyUserContextId)*new_config;
    if (!hi_swap_config)
    {
        hi_swap_config = sfPolicyConfigCreate();
        if (!hi_swap_config)
            FatalError("No memory to allocate http inspect swap_configuration.\n");
        *new_config = hi_swap_config;
    }
    _HttpInspectReload(sc, hi_swap_config, args);
}

static void HttpInspectReload(struct _SnortConfig *sc, char *args, void **new_config)
{
    tSfPolicyUserContextId hi_swap_config;
    hi_swap_config = (tSfPolicyUserContextId)GetRelatedReloadData(sc, GLOBAL_KEYWORD);
    _HttpInspectReload(sc, hi_swap_config, args);
}

static int HttpInspectReloadVerify(struct _SnortConfig *sc, void *swap_config)
{
    tSfPolicyUserContextId hi_swap_config = (tSfPolicyUserContextId)swap_config;
    HTTPINSPECT_GLOBAL_CONF *defaultConfig;
    HTTPINSPECT_GLOBAL_CONF *defaultSwapConfig;

    if (hi_swap_config == NULL)
        return 0;

    if (sfPolicyUserDataIterate (sc, hi_swap_config, HttpInspectVerifyPolicy))
        return -1;

    defaultConfig = (HTTPINSPECT_GLOBAL_CONF *)sfPolicyUserDataGetDefault(hi_config);
    defaultSwapConfig = (HTTPINSPECT_GLOBAL_CONF *)sfPolicyUserDataGetDefault(hi_swap_config);

    if (!defaultConfig)
        return 0;

#ifdef ZLIB
    {
        if (hi_gzip_mempool != NULL)
        {
            if (defaultSwapConfig == NULL)
            {
                WarningMessage("http_inspect:  Changing gzip parameters requires "
                        "a restart.\n");
                return -1;
            }

            if (defaultSwapConfig->max_gzip_mem != defaultConfig->max_gzip_mem)
            {
                WarningMessage("http_inspect:  Changing max_gzip_mem requires "
                        "a restart.\n");
                return -1;
            }

            if (defaultSwapConfig->compr_depth != defaultConfig->compr_depth)
            {
                WarningMessage("http_inspect:  Changing compress_depth requires "
                        "a restart.\n");
                return -1;
            }

            if (defaultSwapConfig->decompr_depth != defaultConfig->decompr_depth)
            {
                WarningMessage("http_inspect:  Changing decompress_depth requires "
                        "a restart.\n");
                return -1;
            }

        }
        else if (defaultSwapConfig != NULL)
        {
            if (sfPolicyUserDataIterate(sc, hi_swap_config, HttpInspectExtractGzip) != 0)
            {
                if (defaultSwapConfig == NULL)
                {
                    ErrorMessage("http_inspect:  Must configure a default global "
                            "configuration if you want to enable gzip in any "
                            "server configuration.\n");
                    return -1;
                }

                hi_gzip_mempool = (MemPool *)SnortAlloc(sizeof(MemPool));

                if (mempool_init(hi_gzip_mempool, defaultSwapConfig->max_gzip_sessions,
                            sizeof(DECOMPRESS_STATE)) != 0)
                {
                    if (defaultSwapConfig->max_gzip_sessions)
                        FatalError("http_inspect: Error setting the \"max_gzip_mem\" \n");
                    else
                        FatalError("http_inspect:  Could not allocate gzip mempool.\n");
                }
            }
        }
    }
#endif
    if (http_mempool != NULL)
    {
        if (defaultSwapConfig == NULL)
        {
            WarningMessage("http_inspect:  Changing HTTP memcap requires a restart.\n");
            return -1;
        }

        if (defaultSwapConfig->memcap != defaultConfig->memcap)
        {
            WarningMessage("http_inspect:  Changing memcap requires a restart.\n");
            return -1;
        }
    }
    else if (mime_decode_mempool != NULL)
    {
        if (defaultSwapConfig == NULL)
        {
            WarningMessage("http_inspect:  Changing HTTP decode requires a restart.\n");
            return -1;
        }
        defaultSwapConfig->decode_conf.file_depth = file_api->get_max_file_depth();
        if (defaultSwapConfig->decode_conf.file_depth > -1)
        {
            defaultSwapConfig->mime_conf.log_filename = 1;
        }
        if(file_api->is_decoding_conf_changed(&(defaultSwapConfig->decode_conf),
                &(defaultConfig->decode_conf), "HTTP"))
        {
            return -1;
        }
    }
    else
    {
        if (sfPolicyUserDataIterate(sc, hi_swap_config, HttpInspectExtractUriHost) != 0)
        {
            uint32_t max_sessions_logged;

            if (defaultSwapConfig == NULL)
            {
                ErrorMessage("http_inspect:  Must configure a default global "
                            "configuration if you want to enable logging of uri or hostname in any "
                            "server configuration.\n");
                return -1;
            }

            max_sessions_logged = defaultSwapConfig->memcap / (MAX_URI_EXTRACTED + MAX_HOSTNAME);

            http_mempool = (MemPool *)SnortAlloc(sizeof(MemPool));

            if (mempool_init(http_mempool, max_sessions_logged,(MAX_URI_EXTRACTED + MAX_HOSTNAME)) != 0)
            {
                FatalError("http_inspect:  Could not allocate HTTP mempool.\n");
            }
        }
        if (sfPolicyUserDataIterate(sc, hi_swap_config, HttpEnableDecoding) != 0)
        {
            if (defaultSwapConfig == NULL)
            {
                ErrorMessage("http_inspect:  Must configure a default global "
                        "configuration if you want to enable decoding in any "
                        "server configuration.\n");
                return -1;
            }
            updateMaxDepth(defaultSwapConfig->decode_conf.file_depth, &defaultSwapConfig->decode_conf.max_depth);
            mime_decode_mempool = (MemPool *)file_api->init_mime_mempool(defaultSwapConfig->decode_conf.max_mime_mem,
                    defaultSwapConfig->decode_conf.max_depth, mime_decode_mempool, PROTOCOL_NAME);
        }
        if (sfPolicyUserDataIterate(sc, hi_swap_config, HttpEnableMimeLog) != 0)
        {
            if (defaultSwapConfig == NULL)
            {
                ErrorMessage("http_inspect:  Must configure a default global "
                        "configuration if you want to enable mime log in any "
                        "server configuration.\n");
                return -1;
            }
            mime_log_mempool = (MemPool *)file_api->init_log_mempool(0,
                    defaultSwapConfig->mime_conf.memcap, mime_log_mempool, PROTOCOL_NAME);
        }
    }


    return 0;
}

static void * HttpInspectReloadSwap(struct _SnortConfig *sc, void *swap_config)
{
    tSfPolicyUserContextId hi_swap_config = (tSfPolicyUserContextId)swap_config;
    tSfPolicyUserContextId old_config = hi_config;

    if (hi_swap_config == NULL)
        return NULL;

    hi_config = hi_swap_config;

    return (void *)old_config;
}

static void HttpInspectReloadSwapFree(void *data)
{
    if (data == NULL)
        return;

    HttpInspectFreeConfigs((tSfPolicyUserContextId)data);
}
#endif

static inline void InitLookupTables(void)
{
    int iNum;
    int iCtr;

    memset(hex_lookup, INVALID_HEX_VAL, sizeof(hex_lookup));
    memset(valid_lookup, INVALID_HEX_VAL, sizeof(valid_lookup));

    iNum = 0;
    for(iCtr = 48; iCtr < 58; iCtr++)
    {
        hex_lookup[iCtr] = iNum;
        valid_lookup[iCtr] = HEX_VAL;
        iNum++;
    }

    /*
    * Set the upper case values.
    */
    iNum = 10;
    for(iCtr = 65; iCtr < 71; iCtr++)
    {
        hex_lookup[iCtr] = iNum;
        valid_lookup[iCtr] = HEX_VAL;
        iNum++;
    }

    /*
     *  Set the lower case values.
     */
    iNum = 10;
    for(iCtr = 97; iCtr < 103; iCtr++)
    {
        hex_lookup[iCtr] = iNum;
        valid_lookup[iCtr] = HEX_VAL;
        iNum++;
   }
}

