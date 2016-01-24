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
**  @file       snort_httpinspect.c
**
**  @author     Daniel Roelker <droelker@sourcefire.com>
**
**  @brief      This file wraps the HttpInspect functionality for Snort
**              and starts the HttpInspect flow.
**
**
**  The file takes a Packet structure from the Snort IDS to start the
**  HttpInspect flow.  This also uses the Stream Interface Module which
**  is also Snort-centric.  Mainly, just a wrapper to HttpInspect
**  functionality, but a key part to starting the basic flow.
**
**  The main bulk of this file is taken up with user configuration and
**  parsing.  The reason this is so large is because HttpInspect takes
**  very detailed configuration parameters for each specified server.
**  Hopefully every web server that is out there can be emulated
**  with these configuration options.
**
**  The main functions of note are:
**    - HttpInspectSnortConf::this is the configuration portion
**    - SnortHttpInspect::this is the actual inspection flow
**    - LogEvents:this is where we log the HttpInspect events
**
**  NOTES:
**
**  - 2.11.03:  Initial Development.  DJR
**  - 2.4.05:   Added tab_uri_delimiter config option.  AJM.
*/
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "snort.h"
#include "detect.h"
#include "decode.h"
#include "log.h"
#include "event.h"
#include "generators.h"
#include "snort_debug.h"
#include "plugbase.h"
#include "util.h"
#include "event_queue.h"
#include "stream_api.h"
#include "sfsnprintfappend.h"

#include "hi_return_codes.h"
#include "hi_ui_config.h"
#include "hi_ui_iis_unicode_map.h"
#include "hi_si.h"
#include "hi_mi.h"
#include "hi_norm.h"
#include "hi_client.h"
#include "snort_httpinspect.h"
#include "detection_util.h"
#include "profiler.h"
#include "hi_cmd_lookup.h"
#include "Unified2_common.h"
#include "mempool.h"
#include "file_api.h"
#include "sf_email_attach_decode.h"
#ifdef PERF_PROFILING
extern PreprocStats hiDetectPerfStats;
extern int hiDetectCalled;
#endif

extern char *snort_conf_dir;

#ifdef ZLIB
extern MemPool *hi_gzip_mempool;
#endif


/* Stats tracking for HTTP Inspect */
HIStats hi_stats;

DataBuffer HttpDecodeBuf;

const HiSearchToken hi_patterns[] =
{
    {"<SCRIPT",         7,  HI_JAVASCRIPT},
    {NULL,              0, 0}
};

const HiSearchToken html_patterns[] =
{
    {"JAVASCRIPT",      10, HTML_JS},
    {"ECMASCRIPT",      10, HTML_EMA},
    {"VBSCRIPT",         8, HTML_VB},
    {NULL,               0, 0}
};

void *hi_javascript_search_mpse = NULL;
void *hi_htmltype_search_mpse = NULL;
HISearch hi_js_search[HI_LAST];
HISearch hi_html_search[HTML_LAST];
HISearch *hi_current_search = NULL;
HISearchInfo hi_search_info;


#define MAX_FILENAME    1000

/*
**  GLOBAL subkeywords.
*/
/**
**  Takes an integer arugment
*/
#define MAX_PIPELINE  "max_pipeline"
/**
**  Specifies whether to alert on anomalous
**  HTTP servers or not.
*/
#define ANOMALOUS_SERVERS "detect_anomalous_servers"
/**
**  Alert on general proxy use
*/
#define PROXY_ALERT "proxy_alert"
/**
**  Takes an inspection type argument
**  stateful or stateless
*/
#define INSPECT_TYPE  "inspection_type"
#define DEFAULT       "default"

/*
**  GLOBAL subkeyword values
*/
#define INSPECT_TYPE_STATELESS "stateless"
#define INSPECT_TYPE_STATEFUL  "stateful"

/*
**  SERVER subkeywords.
*/
#define PORTS             "ports"
#define FLOW_DEPTH        "flow_depth"
#define SERVER_FLOW_DEPTH "server_flow_depth"
#define CLIENT_FLOW_DEPTH "client_flow_depth"
#define POST_DEPTH        "post_depth"
#define IIS_UNICODE_MAP   "iis_unicode_map"
#define CHUNK_LENGTH      "chunk_length"
#define SMALL_CHUNK_LENGTH  "small_chunk_length"
#define MAX_HDR_LENGTH    "max_header_length"
#define PIPELINE          "no_pipeline_req"
#define ASCII             "ascii"
#define DOUBLE_DECODE     "double_decode"
#define U_ENCODE          "u_encode"
#define BARE_BYTE         "bare_byte"
/* Base 36 is deprecated and essentially a noop
 * Leave this here so as to print out a warning when the option is used */
#define BASE36            "base36"
#define UTF_8             "utf_8"
#define IIS_UNICODE       "iis_unicode"
#define NON_RFC_CHAR      "non_rfc_char"
#define MULTI_SLASH       "multi_slash"
#define IIS_BACKSLASH     "iis_backslash"
#define DIRECTORY         "directory"
#define APACHE_WS         "apache_whitespace"
#define IIS_DELIMITER     "iis_delimiter"
#define PROFILE           "profile"
#define NON_STRICT        "non_strict"
#define ALLOW_PROXY       "allow_proxy_use"
#define OVERSIZE_DIR      "oversize_dir_length"
#define INSPECT_URI_ONLY  "inspect_uri_only"
#define GLOBAL_ALERT      "no_alerts"
#define WEBROOT           "webroot"
#define TAB_URI_DELIMITER "tab_uri_delimiter"
#define WHITESPACE        "whitespace_chars"
#define NORMALIZE_HEADERS "normalize_headers"
#define NORMALIZE_COOKIES "normalize_cookies"
#define NORMALIZE_UTF     "normalize_utf"
#define NORMALIZE_JS      "normalize_javascript"
#define MAX_JS_WS         "max_javascript_whitespaces"
#define MAX_HEADERS       "max_headers"
#define INSPECT_COOKIES   "enable_cookie"
#define EXTRACT_GZIP      "inspect_gzip"
#define UNLIMIT_DECOMPRESS "unlimited_decompress"
#define INSPECT_RESPONSE  "extended_response_inspection"
#define COMPRESS_DEPTH    "compress_depth"
#define DECOMPRESS_DEPTH  "decompress_depth"
#define MAX_GZIP_MEM      "max_gzip_mem"
#define EXTENDED_ASCII    "extended_ascii_uri"
#define OPT_DISABLED      "disabled"
#define ENABLE_XFF        "enable_xff"
#define XFF_HEADERS_TOK   "xff_headers"
#define HTTP_METHODS      "http_methods"
#define LOG_URI           "log_uri"
#define LOG_HOSTNAME      "log_hostname"
#define HTTP_MEMCAP       "memcap"
#define MAX_SPACES    "max_spaces"

#define MAX_CLIENT_DEPTH 1460
#define MAX_SERVER_DEPTH 65535

/*
**  Alert subkeywords
*/
#define BOOL_YES     "yes"
#define BOOL_NO      "no"

/*
**  PROFILE subkeywords
*/
#define APACHE        "apache"
#define IIS           "iis"
#define IIS4_0        "iis4_0"
#define IIS5_0        "iis5_0" /* 5.0 only. For 5.1 and beyond, use IIS */
#define ALL           "all"

/*
**  IP Address list delimiters
*/
#define START_IPADDR_LIST "{"
#define END_IPADDR_LIST   "}"

/*
**  Port list delimiters
*/
#define START_PORT_LIST "{"
#define END_PORT_LIST   "}"

/*
**  XFF Header list delimiters, states, etc.
*/
#define START_XFF_HEADER_LIST  "{"
#define END_XFF_HEADER_LIST    "}"
#define START_XFF_HEADER_ENTRY "["
#define END_XFF_HEADER_ENTRY   "]"

#define XFF_MIN_PREC        (1)
#define XFF_MAX_PREC        (255)

#define XFF_STATE_START     (1)
#define XFF_STATE_OPEN      (2)
#define XFF_STATE_NAME      (3)
#define XFF_STATE_PREC      (4)
#define XFF_STATE_CLOSE     (5)
#define XFF_STATE_END       (6)

/*
**  Keyword for the default server configuration
*/
#define SERVER_DEFAULT "default"

typedef enum {
    CONFIG_MAX_SPACES = 0,
    CONFIG_MAX_JS_WS 
} SpaceType;


/*
**  NAME
**    ProcessGlobalAlert::
*/
/**
**  Process the global alert keyword.
**
**  There is no arguments to this keyword, because you can only turn
**  all the alerts off.  As of now, we aren't going to support turning
**  all the alerts on.
**
**  @param GlobalConf  pointer to the global configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the lenght of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
/*
static int ProcessGlobalAlert(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                              char *ErrorString, int ErrStrLen)
{
    GlobalConf->no_alerts = 1;

    return 0;
}
*/

/*
**  NAME
**    ProcessMaxPipeline::
*/
/**
**  Process the max pipeline configuration.
**
**  This sets the maximum number of pipeline requests that we
**  will buffer while waiting for responses, before inspection.
**  There is a maximum limit on this, but we can track a user
**  defined amount.
**
**  @param GlobalConf  pointer to the global configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the lenght of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessMaxPipeline(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                              char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    char *pcEnd = NULL;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to token '%s'.", MAX_PIPELINE);

        return -1;
    }

    GlobalConf->max_pipeline_requests = strtol(pcToken, &pcEnd, 10);

    /*
    **  Let's check to see if the entire string was valid.
    **  If there is an address here, then there was an
    **  invalid character in the string.
    */
    if(*pcEnd)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to token '%s'.  Must be a positive "
                      "number between 0 and %d.", MAX_PIPELINE,
                      HI_UI_CONFIG_MAX_PIPE);

        return -1;
    }

    if(GlobalConf->max_pipeline_requests < 0 ||
       GlobalConf->max_pipeline_requests > HI_UI_CONFIG_MAX_PIPE)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to token '%s'.  Must be a positive "
                      "number between 0 and %d.", MAX_PIPELINE, HI_UI_CONFIG_MAX_PIPE);

        return -1;
    }

    return 0;
}

/*
**  NAME
**    ProcessInspectType::
*/
/**
**  Process the type of inspection.
**
**  This sets the type of inspection for HttpInspect to do.
**
**  @param GlobalConf  pointer to the global configuration
**  @param ErrorString error string buffer
**
**  @param ErrStrLen   the lenght of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessInspectType(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                              char *ErrorString, int ErrStrLen)
{
    char *pcToken;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to token '%s'.", INSPECT_TYPE);

        return -1;
    }

    if(!strcmp(INSPECT_TYPE_STATEFUL, pcToken))
    {
        GlobalConf->inspection_type = HI_UI_CONFIG_STATEFUL;

        /*
        **  We don't support this option yet, so we'll give an error and
        **  bail.
        */
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Stateful HttpInspect processing is not yet available.  "
                      "Please use stateless processing for now.");

        return -1;
    }
    else if(!strcmp(INSPECT_TYPE_STATELESS, pcToken))
    {
        GlobalConf->inspection_type = HI_UI_CONFIG_STATELESS;
    }
    else
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to token '%s'.  Must be either '%s' or '%s'.",
                      INSPECT_TYPE, INSPECT_TYPE_STATEFUL, INSPECT_TYPE_STATELESS);

        return -1;
    }

    return 0;
}

static int ProcessIISUnicodeMap(int **iis_unicode_map,
                                char **iis_unicode_map_filename,
                                int *iis_unicode_map_codepage,
                                char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int  iRet;
    char filename[MAX_FILENAME];
    char *pcEnd;
    int  iCodeMap;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to token '%s'.", IIS_UNICODE_MAP);

        return -1;
    }

    /*
    **  If an absolute path is specified, then use that.
    */
#ifndef WIN32
    if(pcToken[0] == '/')
    {
        iRet = SnortSnprintf(filename, sizeof(filename), "%s", pcToken);
    }
    else
    {
        /*
        **  Set up the file name directory
        */
        if (snort_conf_dir[strlen(snort_conf_dir) - 1] == '/')
        {
            iRet = SnortSnprintf(filename, sizeof(filename),
                                 "%s%s", snort_conf_dir, pcToken);
        }
        else
        {
            iRet = SnortSnprintf(filename, sizeof(filename),
                                 "%s/%s", snort_conf_dir, pcToken);
        }
    }
#else
    if(strlen(pcToken)>3 && pcToken[1]==':' && pcToken[2]=='\\')
    {
        iRet = SnortSnprintf(filename, sizeof(filename), "%s", pcToken);
    }
    else
    {
        /*
        **  Set up the file name directory
        */
        if (snort_conf_dir[strlen(snort_conf_dir) - 1] == '\\' ||
            snort_conf_dir[strlen(snort_conf_dir) - 1] == '/' )
        {
            iRet = SnortSnprintf(filename, sizeof(filename),
                                 "%s%s", snort_conf_dir, pcToken);
        }
        else
        {
            iRet = SnortSnprintf(filename, sizeof(filename),
                                 "%s\\%s", snort_conf_dir, pcToken);
        }
    }
#endif

    if(iRet != SNORT_SNPRINTF_SUCCESS)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                 "Filename too long for token '%s'.", IIS_UNICODE_MAP);

        return -1;
    }

    /*
    **  Set the filename
    */
    *iis_unicode_map_filename = strdup(filename);
    if(*iis_unicode_map_filename == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Could not strdup() '%s' filename.",
                      IIS_UNICODE_MAP);

        return -1;
    }

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No codemap to select from IIS Unicode Map file.");

        return -1;
    }

    /*
    **  Grab the unicode codemap to use
    */
    iCodeMap = strtol(pcToken, &pcEnd, 10);
    if(*pcEnd || iCodeMap < 0)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid IIS codemap argument.");

        return -1;
    }

    /*
    **  Set the codepage
    */
    *iis_unicode_map_codepage = iCodeMap;

    /*
    **  Assume that the pcToken we now have is the filename of the map
    **  table.
    */
    iRet = hi_ui_parse_iis_unicode_map(iis_unicode_map, filename, iCodeMap);
    if (iRet)
    {
        if(iRet == HI_INVALID_FILE)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Unable to open the IIS Unicode Map file '%s'.",
                          filename);
        }
        else if(iRet == HI_FATAL_ERR)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Did not find specified IIS Unicode codemap in "
                          "the specified IIS Unicode Map file.");
        }
        else
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "There was an error while parsing the IIS Unicode Map file.");
        }

        return -1;
    }

    return 0;
}

static int ProcessOversizeDir(HTTPINSPECT_CONF *ServerConf,
                              char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    char *pcEnd;
    int  iDirLen;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to token '%s'.", OVERSIZE_DIR);

        return -1;
    }

    /*
    **  Grab the oversize directory length
    */
    iDirLen = strtol(pcToken, &pcEnd, 10);
    if(*pcEnd || iDirLen < 0)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to token '%s'.", OVERSIZE_DIR);

        return -1;
    }

    ServerConf->long_dir = iDirLen;

    return 0;
}

static int ProcessHttpMemcap(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                char *ErrorString, int ErrStrLen)
{
    char *pcToken, *pcEnd;
    int memcap;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                    "No argument to '%s' token.", HTTP_MEMCAP);
        return -1;
    }

    memcap = SnortStrtolRange(pcToken, &pcEnd, 10, 0 , INT_MAX);
    if(*pcEnd)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                    "Invalid argument to '%s'.", HTTP_MEMCAP);

        return -1;
    }

    if(memcap < MIN_HTTP_MEMCAP || memcap > MAX_HTTP_MEMCAP)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid argument to '%s'.  Must be between %d and "
                "%d.", HTTP_MEMCAP, MIN_HTTP_MEMCAP, MAX_HTTP_MEMCAP);

        return -1;
    }

    GlobalConf->memcap = memcap;

    return 0;

}


#ifdef ZLIB
static int ProcessMaxGzipMem(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
        char *ErrorString, int ErrStrLen)
{
    char *pcToken, *pcEnd;
    int max_gzip_mem;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "No argument to '%s' token.", MAX_GZIP_MEM);
        return -1;
    }

    max_gzip_mem = SnortStrtolRange(pcToken, &pcEnd, 10, 0, INT_MAX);
    if ((pcEnd == pcToken) || *pcEnd || (errno == ERANGE))
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.", MAX_GZIP_MEM);
        return -1;
    }

    if(max_gzip_mem < GZIP_MEM_MIN)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.", MAX_GZIP_MEM);
        return -1;
    }
    GlobalConf->max_gzip_mem = max_gzip_mem;

    return 0;

}

static int ProcessCompressDepth(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                            char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int  compress_depth;
    char *pcEnd;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "No argument to '%s' token.", COMPRESS_DEPTH);

        return -1;
    }

    compress_depth = SnortStrtol(pcToken, &pcEnd, 10);
    if(*pcEnd)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid argument to '%s'.", COMPRESS_DEPTH);

        return -1;
    }

    if(compress_depth <= 0 || compress_depth > MAX_GZIP_DEPTH)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid argument to '%s'.  Must be between 1 and "
                "%d.", COMPRESS_DEPTH, MAX_GZIP_DEPTH);

        return -1;
    }

    GlobalConf->compr_depth = compress_depth;

    return 0;
}

static int ProcessDecompressDepth(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                            char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int  decompress_depth;
    char *pcEnd;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "No argument to '%s' token.", DECOMPRESS_DEPTH);

        return -1;
    }

    decompress_depth = SnortStrtol(pcToken, &pcEnd, 10);
    if(*pcEnd)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid argument to '%s'.", DECOMPRESS_DEPTH);

        return -1;
    }

    if(decompress_depth <= 0 || decompress_depth > MAX_GZIP_DEPTH)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid argument to '%s'.  Must be between 1 and "
                "%d.", DECOMPRESS_DEPTH, MAX_GZIP_DEPTH);

        return -1;
    }

    GlobalConf->decompr_depth = decompress_depth;

    return 0;
}
#endif

/*
**  NAME
**      ProcessGlobalConf::
*/
/**
**  This is where we process the global configuration for HttpInspect.
**
**  We set the values of the global configuraiton here.  Any errors that
**  are encountered are specified in the error string and the type of
**  error is returned through the return code, i.e. fatal, non-fatal.
**
**  The configuration options that are dealt with here are:
**      - global_alert
**          This tells us whether to do any internal alerts or not, on
**          a global scale.
**      - max_pipeline
**          Tells HttpInspect how many pipeline requests to buffer looking
**          for a response before inspection.
**      - inspection_type
**          What type of inspection for HttpInspect to do, stateless or
**          stateful.
**
**  @param GlobalConf  pointer to the global configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the lenght of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
int ProcessGlobalConf(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                      char *ErrorString, int ErrStrLen)
{
    int  iRet;
    char *pcToken;
    int  iTokens = 0;

    while ((pcToken = strtok(NULL, CONF_SEPARATORS)) != NULL)
    {
        /*
        **  Show that we at least got one token
        */
        iTokens = 1;

        /*
        **  Search for configuration keywords
        */
        if(!strcmp(MAX_PIPELINE, pcToken))
        {
            iRet = ProcessMaxPipeline(GlobalConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(INSPECT_TYPE, pcToken))
        {
            iRet = ProcessInspectType(GlobalConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(IIS_UNICODE_MAP, pcToken))
        {
            iRet = ProcessIISUnicodeMap(&GlobalConf->iis_unicode_map, &GlobalConf->iis_unicode_map_filename,
                                        &GlobalConf->iis_unicode_codepage, ErrorString,ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(ANOMALOUS_SERVERS, pcToken))
        {
            /*
            **  This is easy to configure since we just look for the token
            **  and turn on the option.
            */
            GlobalConf->anomalous_servers = 1;
        }
        else if(!strcmp(PROXY_ALERT, pcToken))
        {
            GlobalConf->proxy_alert = 1;
        }
#ifdef ZLIB
        else if (!strcmp(MAX_GZIP_MEM, pcToken))
        {
            iRet = ProcessMaxGzipMem(GlobalConf, ErrorString, ErrStrLen);
            if(iRet)
                return iRet;
        }
        else if (!strcmp(COMPRESS_DEPTH, pcToken))
        {
            iRet = ProcessCompressDepth(GlobalConf, ErrorString, ErrStrLen);
            if (iRet)
                return iRet;
        }
        else if (!strcmp(DECOMPRESS_DEPTH, pcToken))
        {
            iRet = ProcessDecompressDepth(GlobalConf, ErrorString, ErrStrLen);
            if(iRet)
                return iRet;
        }
#endif
        else if (!strcmp(OPT_DISABLED, pcToken))
        {
            GlobalConf->disabled = 1;
            return 0;
        }
        else if (!strcmp(HTTP_MEMCAP, pcToken))
        {
            iRet = ProcessHttpMemcap(GlobalConf, ErrorString, ErrStrLen);
            if(iRet)
                return iRet;
        }
        else if (!file_api->parse_mime_decode_args(&(GlobalConf->decode_conf),pcToken, "HTTP"))
        {
            continue;
        }
        else
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Invalid keyword '%s' for '%s' configuration.",
                          pcToken, GLOBAL);

            return -1;
        }
    }

    /*
    **  If there are not any tokens to the configuration, then
    **  we let the user know and log the error.  return non-fatal
    **  error.
    */
    if(!iTokens)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No tokens to '%s' configuration.", GLOBAL);

        return -1;
    }

    /*
    **  Let's check to make sure that we get a default IIS Unicode Codemap
    */
    if(!GlobalConf->iis_unicode_map)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Global configuration must contain an IIS Unicode Map "
                      "configuration.  Use token '%s'.", IIS_UNICODE_MAP);

        return -1;
    }

    return 0;
}


/*
**  NAME
**    ProcessProfile::
*/
/** Returns error messages for failed hi_ui_config_set_profile calls.
 **
 ** Called exclusively by ProcessProfile.
 */
static inline int _ProcessProfileErr(int iRet, char* ErrorString,
                                     int ErrStrLen, char *token)
{
    if(iRet == HI_MEM_ALLOC_FAIL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Memory allocation failed while setting the '%s' "
                      "profile.", token);
        return -1;
    }
    else
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Undefined error code for set_profile_%s.", token);
        return -1;
    }
}

/*
**  NAME
**    ProcessProfile::
*/
/**
**  Process the PROFILE configuration.
**
**  This function verifies that the argument to the profile configuration
**  is valid.  We also check to make sure there is no additional
**  configuration after the PROFILE.  This is no allowed, so we
**  alert on that fact.
**
**  @param ServerConf  pointer to the server configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessProfile(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                          HTTPINSPECT_CONF *ServerConf,
                          char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int  iRet;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to '%s'.", PROFILE);

        return -1;
    }

    /*
    **  Load the specific type of profile
    */
    if(!strcmp(APACHE, pcToken))
    {
        iRet = hi_ui_config_set_profile_apache(ServerConf);
        if (iRet)
        {
            /*  returns -1 */
            return _ProcessProfileErr(iRet, ErrorString, ErrStrLen, pcToken);
        }

        ServerConf->profile = HI_APACHE;
    }
    else if(!strcmp(IIS, pcToken))
    {
        iRet = hi_ui_config_set_profile_iis(ServerConf, GlobalConf->iis_unicode_map);
        if (iRet)
        {
            /* returns -1 */
            return _ProcessProfileErr(iRet, ErrorString, ErrStrLen, pcToken);
        }

        ServerConf->profile = HI_IIS;
    }
    else if(!strcmp(IIS4_0, pcToken) || !strcmp(IIS5_0, pcToken))
    {
        iRet = hi_ui_config_set_profile_iis_4or5(ServerConf, GlobalConf->iis_unicode_map);
        if (iRet)
        {
            /* returns -1 */
            return _ProcessProfileErr(iRet, ErrorString, ErrStrLen, pcToken);
        }

        ServerConf->profile = (pcToken[3]=='4'?HI_IIS4:HI_IIS5);
    }
    else if(!strcmp(ALL, pcToken))
    {
        iRet = hi_ui_config_set_profile_all(ServerConf, GlobalConf->iis_unicode_map);
        if (iRet)
        {
            /* returns -1 */
            return _ProcessProfileErr(iRet, ErrorString, ErrStrLen, pcToken);
        }

        ServerConf->profile = HI_ALL;
    }
    else
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid profile argument '%s'.", pcToken);

        return -1;
    }

    return 0;


}

/*
**  NAME
**    ProcessPorts::
*/
/**
**  Process the port list for the server configuration.
**
**  This configuration is a list of valid ports and is ended by a
**  delimiter.
**
**  @param ServerConf  pointer to the server configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessPorts(HTTPINSPECT_CONF *ServerConf,
                        char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    char *pcEnd;
    int  iPort;
    int  iEndPorts = 0;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(!pcToken)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid port list format.");

        return -1;
    }

    if(strcmp(START_PORT_LIST, pcToken))
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Must start a port list with the '%s' token.",
                      START_PORT_LIST);

        return -1;
    }

    memset(ServerConf->ports, 0, MAXPORTS_STORAGE);

    while ((pcToken = strtok(NULL, CONF_SEPARATORS)) != NULL)
    {
        if(!strcmp(END_PORT_LIST, pcToken))
        {
            iEndPorts = 1;
            break;
        }

        iPort = strtol(pcToken, &pcEnd, 10);

        /*
        **  Validity check for port
        */
        if(*pcEnd)
        {
            SnortSnprintf(ErrorString, ErrStrLen, "Invalid port number.");

            return -1;
        }

        if(iPort < 0 || iPort > MAXPORTS-1)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Invalid port number.  Must be between 0 and 65535.");

            return -1;
        }

        ServerConf->ports[iPort/8] |= (1 << (iPort % 8) );

        if(ServerConf->port_count < MAXPORTS)
            ServerConf->port_count++;
    }

    if(!iEndPorts)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Must end '%s' configuration with '%s'.",
                      PORTS, END_PORT_LIST);

        return -1;
    }

    return 0;
}

/*
**  NAME
**    ProcessFlowDepth::
*/
/**
**  Configure the flow depth for a server.
**
**  Check that the value for flow depth is within bounds
**  and is a valid number.
**
**  @param ServerConf  pointer to the server configuration
**  @param ServerOrClient which flowdepth is being set
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessFlowDepth(HTTPINSPECT_CONF *ServerConf, int ServerOrClient,
                            char *ErrorString, int ErrStrLen, char *pToken, int maxDepth)
{
    char *pcToken;
    int  iFlowDepth;
    char *pcEnd;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to '%s' token.", pToken);

        return -1;
    }

    iFlowDepth = SnortStrtol(pcToken, &pcEnd, 10);
    if(*pcEnd)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.", pToken);

        return -1;
    }

    /* -1 here is okay, which means ignore ALL server side traffic */
    if(iFlowDepth < -1 || iFlowDepth > maxDepth)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.  Must be between -1 and %d.",
                      pToken, maxDepth);

        return -1;
    }

    if (ServerOrClient == HI_SI_CLIENT_MODE)
        ServerConf->client_flow_depth = iFlowDepth;
    else
        ServerConf->server_flow_depth = iFlowDepth;

    return 0;
}

/*
**  NAME
**    ProcessPostDepth::
*/
/**
**  Configure the post depth for client requests
**
**  Checks that the value for flow depth is within bounds
**  and is a valid number.
**
**  @param ServerConf  pointer to the server configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessPostDepth(HTTPINSPECT_CONF *ServerConf,
                            char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int  post_depth;
    char *pcEnd;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "No argument to '%s' token.", POST_DEPTH);

        return -1;
    }

    post_depth = SnortStrtol(pcToken, &pcEnd, 10);
    if(*pcEnd)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid argument to '%s'.", POST_DEPTH);

        return -1;
    }

    /* 0 means 'any depth' */
    if(post_depth < -1 || post_depth > ( IP_MAXPACKET - (IP_HEADER_LEN + TCP_HEADER_LEN) ) )
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid argument to '%s'.  Must be between -1 and "
                "%d.", POST_DEPTH,( IP_MAXPACKET - (IP_HEADER_LEN + TCP_HEADER_LEN) ));

        return -1;
    }

    ServerConf->post_depth = post_depth;

    return 0;
}


/*
**  NAME
**    ProcessChunkLength::
*/
/**
**  Process and verify the chunk length for the server configuration.
**
**  @param ServerConf  pointer to the server configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessChunkLength(HTTPINSPECT_CONF *ServerConf,
                              char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int  iChunkLength;
    char *pcEnd;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to '%s' token.", CHUNK_LENGTH);

        return -1;
    }

    iChunkLength = SnortStrtolRange(pcToken, &pcEnd, 10, 0, INT_MAX);
    if ((pcEnd == pcToken) || *pcEnd || (errno == ERANGE))
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.", CHUNK_LENGTH);

        return -1;
    }

    ServerConf->chunk_length = iChunkLength;

    return 0;
}

/*
**  NAME
**    ProcessSmallChunkLength::
*/
/**
**  Process and verify the small chunk length for the server configuration.
**
**  @param ServerConf  pointer to the server configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessSmallChunkLength(HTTPINSPECT_CONF *ServerConf,
                                   char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    char *pcEnd;
    int num_toks = 0;
    bool got_param_end = 0;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if (!pcToken)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid cmd list format.");

        return -1;
    }

    if (strcmp(START_PORT_LIST, pcToken))
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Must start small chunk length parameters with the '%s' token.",
                START_PORT_LIST);

        return -1;
    }

    while ((pcToken = strtok(NULL, CONF_SEPARATORS)) != NULL)
    {
        if (!strcmp(END_PORT_LIST, pcToken))
        {
            got_param_end = 1;
            break;
        }

        num_toks++;
        if (num_toks > 2)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Too many arguments to '%s'.", SMALL_CHUNK_LENGTH);

            return -1;
        }

        if (num_toks == 1)
        {
            uint32_t chunk_length = (uint32_t)SnortStrtoulRange(pcToken, &pcEnd, 10, 0, UINT8_MAX);
            if ((pcEnd == pcToken) || *pcEnd || (errno == ERANGE))
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                              "Invalid argument to chunk length param of '%s'. "
                              "Must be between 0 and %u.\n",
                              SMALL_CHUNK_LENGTH, UINT8_MAX);

                return -1;
            }

            ServerConf->small_chunk_length.size = (uint8_t)chunk_length;
        }
        else
        {
            uint32_t num_chunks_threshold = (uint32_t)SnortStrtoulRange(pcToken, &pcEnd, 10, 0, UINT8_MAX);
            if ((pcEnd == pcToken) || *pcEnd || (errno == ERANGE))
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                              "Invalid argument to number of consecutive chunks "
                              "threshold of '%s'. Must be between 0 and %u.\n",
                              SMALL_CHUNK_LENGTH, UINT8_MAX);

                return -1;
            }

            ServerConf->small_chunk_length.num = (uint8_t)num_chunks_threshold;
        }
    }

    if (num_toks != 2)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Not enough arguments to '%s'.", SMALL_CHUNK_LENGTH);

        return -1;
    }

    if (!got_param_end)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Must end '%s' configuration with '%s'.", SMALL_CHUNK_LENGTH, END_PORT_LIST);

        return -1;
    }

    return 0;
}

/*
**  NAME
**    ProcessMaxHeaders::
*/
/**
**  Process and verify the maximum allowed number of headers for the
**  server configuration.
**
**  @param ServerConf  pointer to the server configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessMaxHeaders(HTTPINSPECT_CONF *ServerConf,
                              char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int  length;
    char *pcEnd;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to '%s' token.", MAX_HEADERS);

        return -1;
    }

    length = strtol(pcToken, &pcEnd, 10);
    if(*pcEnd || pcEnd == pcToken)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.", MAX_HEADERS);

        return -1;
    }

    if(length < 0)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'. Valid range is 0 to 1024.", MAX_HEADERS);

        return -1;
    }

    if(length > 1024)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.  Valid range is 0 to 1024.", MAX_HEADERS);

        return -1;
    }

    ServerConf->max_headers = length;

    return 0;
}

/*
**  NAME
**    ProcessMaxHdrLen::
*/
/**
**  Process and verify the maximum allowed header length for the
**  server configuration.
**
**  @param ServerConf  pointer to the server configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessMaxHdrLen(HTTPINSPECT_CONF *ServerConf,
                              char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int  length;
    char *pcEnd;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to '%s' token.", MAX_HDR_LENGTH);

        return -1;
    }

    length = strtol(pcToken, &pcEnd, 10);
    if(*pcEnd || pcEnd == pcToken)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.", MAX_HDR_LENGTH);

        return -1;
    }

    if(length < 0)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'. Valid range is 0 to 65535.", MAX_HDR_LENGTH);

        return -1;
    }

    if(length > 65535)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.  Valid range is 0 to 65535.", MAX_HDR_LENGTH);

        return -1;
    }

    ServerConf->max_hdr_len = length;

    return 0;
}

/*
**  NAME
**    ProcessConfOpt::
*/
/**
**  Set the CONF_OPT on and alert fields.
**
**  We check to make sure of valid parameters and then
**  set the appropriate fields.  Not much more to it, than
**  that.
**
**  @param ConfOpt  pointer to the configuration option
**  @param Option   character pointer to the option being configured
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessConfOpt(HTTPINSPECT_CONF_OPT *ConfOpt, char *Option,
                          char *ErrorString, int ErrStrLen)
{
    char *pcToken;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to token '%s'.", Option);

        return -1;
    }

    /*
    **  Check for the alert value
    */
    if(!strcmp(BOOL_YES, pcToken))
    {
        ConfOpt->alert = 1;
    }
    else if(!strcmp(BOOL_NO, pcToken))
    {
        ConfOpt->alert = 0;
    }
    else
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to token '%s'.", Option);

        return -1;
    }

    ConfOpt->on = 1;

    return 0;
}

/*
**  NAME
**    ProcessNonRfcChar::
*/
/***
**  Configure any characters that the user wants alerted on in the
**  URI.
**
**  This function allocates the memory for CONF_OPT per character and
**  configures the alert option.
**
**  @param ConfOpt  pointer to the configuration option
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessNonRfcChar(HTTPINSPECT_CONF *ServerConf,
                             char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    char *pcEnd;
    int  iChar;
    int  iEndChar = 0;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(!pcToken)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid '%s' list format.", NON_RFC_CHAR);

        return -1;
    }

    if(strcmp(START_PORT_LIST, pcToken))
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Must start a '%s' list with the '%s' token.",
                      NON_RFC_CHAR, START_PORT_LIST);

        return -1;
    }

    while ((pcToken = strtok(NULL, CONF_SEPARATORS)) != NULL)
    {
        if(!strcmp(END_PORT_LIST, pcToken))
        {
            iEndChar = 1;
            break;
        }

        iChar = strtol(pcToken, &pcEnd, 16);
        if(*pcEnd)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Invalid argument to '%s'.  Must be a single character.",
                          NON_RFC_CHAR);

            return -1;
        }

        if(iChar < 0 || iChar > 255)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Invalid character value to '%s'.  Must be a single "
                          "character no greater than 255.", NON_RFC_CHAR);

            return -1;
        }

        ServerConf->non_rfc_chars[iChar] = 1;
    }

    if(!iEndChar)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Must end '%s' configuration with '%s'.",
                      NON_RFC_CHAR, END_PORT_LIST);

        return -1;
    }

    return 0;
}

/*
**  NAME
**    ProcessWhitespaceChars::
*/
/***
**  Configure any characters that the user wants to be treated as
**  whitespace characters before and after a URI.
**
**
**  @param ServerConf  pointer to the server configuration structure
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessWhitespaceChars(HTTPINSPECT_CONF *ServerConf,
                             char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    char *pcEnd;
    int  iChar;
    int  iEndChar = 0;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(!pcToken)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid '%s' list format.", WHITESPACE);

        return -1;
    }

    if(strcmp(START_PORT_LIST, pcToken))
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Must start a '%s' list with the '%s' token.",
                      WHITESPACE, START_PORT_LIST);

        return -1;
    }

    while ((pcToken = strtok(NULL, CONF_SEPARATORS)) != NULL)
    {
        if(!strcmp(END_PORT_LIST, pcToken))
        {
            iEndChar = 1;
            break;
        }

        iChar = strtol(pcToken, &pcEnd, 16);
        if(*pcEnd)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Invalid argument to '%s'.  Must be a single character.",
                          WHITESPACE);

            return -1;
        }

        if(iChar < 0 || iChar > 255)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Invalid character value to '%s'.  Must be a single "
                          "character no greater than 255.", WHITESPACE);

            return -1;
        }

        ServerConf->whitespace[iChar] = HI_UI_CONFIG_WS_BEFORE_URI;
    }

    if(!iEndChar)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Must end '%s' configuration with '%s'.",
                       WHITESPACE, END_PORT_LIST);

        return -1;
    }

    return 0;
}

static bool Is_Field_Prec_Unique( uint8_t *Name_Array[], uint8_t Prec_Array[],
                                 uint8_t *Name, uint8_t Precedence,
                                 char *ErrorString, int ErrStrLen )
{
    int i;

    for( i=0; i<HI_UI_CONFIG_MAX_XFF_FIELD_NAMES; i++ )
    {
        /* A NULL entry indicates the end of the list */
        if( Name_Array[i] == NULL )
            break;

        if( strcmp( (char *)Name, (char *)Name_Array[i] ) == 0 )
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Duplicate XFF Field name: %s.", Name);

            return( false );
        }

        if( (Precedence != 0) && (Precedence == Prec_Array[i]) )
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Duplicate XFF Precedence value: %u.", Precedence);

            return( false );
        }
    }
    return( true );
}

static int Find_Open_Field( uint8_t *Name_Array[] )
{
    int i;

    for( i=0; i<HI_UI_CONFIG_MAX_XFF_FIELD_NAMES; i++ )
        if( Name_Array[i] == NULL )
            return( i );
    return( -1 );
}

static void Push_Down_XFF_List( uint8_t *Name_Array[], uint8_t *Length_Array, uint8_t Prec_Array[], int Start )
{
    int i;

    for( i=(HI_UI_CONFIG_MAX_XFF_FIELD_NAMES-1); i>=Start; i-- )
    {
        Name_Array[i] = Name_Array[i-1];
        Length_Array[i] = Length_Array[i-1];
        Prec_Array[i] = Prec_Array[i-1];
    }
}

/* The caller of Add_XFF_Field is responsible for determining that at least one
   additional entry is availble in both the field name array and the precedence
   value array. */
static int Add_XFF_Field( HTTPINSPECT_CONF *ServerConf, uint8_t *Prec_Array, uint8_t *Field_Name,
                          uint8_t Precedence, char *ErrorString, int ErrStrLen )
{
    uint8_t **Fields = ServerConf->xff_headers;
    uint8_t *Lengths = ServerConf->xff_header_lengths;
    uint8_t Length;
    int i;
    char **Builtin_Fields;
    char *cp;
    const char *Special_Chars = { "_-" };

    if( (Length = strlen( (char *)Field_Name )) > UINT8_MAX )
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Field name limited to %u characters", UINT8_MAX);
        return -1;
    }

    cp = Field_Name;
    while( *cp != '\0' )
    {
        *cp = (uint8_t)toupper(*cp);  // Fold to upper case for runtime comparisons
        if( (isalnum( *cp ) == 0) && (strchr( Special_Chars, (int)(*cp) ) == NULL) ) 
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Invalid xff field name: %s ", Field_Name);
            return( -1 );
        }
        cp += 1;
    }

    Builtin_Fields = hi_client_get_field_names();
    for( i=0; Builtin_Fields[i]!=NULL; i++ )
    {
        if( strcasecmp(Builtin_Fields[i], HI_UI_CONFIG_XFF_FIELD_NAME) == 0 ) continue;
        if( strcasecmp(Builtin_Fields[i], HI_UI_CONFIG_TCI_FIELD_NAME) == 0 ) continue;
        if( strcasecmp(Builtin_Fields[i], (char *)Field_Name) == 0 )
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Cannot use: '%s' as an xff header.", Field_Name);
            return -1;
        }
    }

    if( !Is_Field_Prec_Unique( Fields, Prec_Array, Field_Name, Precedence,
                              ErrorString, ErrStrLen ) )
        return( -1 );

    if( Precedence == 0 )
    {
        if( (i = Find_Open_Field( Fields )) >= 0 )
        {
            Fields[i] = Field_Name;
            Lengths[i] = Length;
            Prec_Array[i] = 0;
            return( 0 );
        }
        else
            return( -1 );
    }

    for( i=0; i<HI_UI_CONFIG_MAX_XFF_FIELD_NAMES; i++ )
    {
        /* If the space is open, place the name & prec */
        if( Fields[i] == NULL )
        {
            Fields[i] = Field_Name;
            Lengths[i] = Length;
            Prec_Array[i] = Precedence;
            break;
        }

        /* if the new entry is higher precedence than the current list element,
           push the list down and place the new entry here. */
        if( Precedence < Prec_Array[i] )
        {
            Push_Down_XFF_List( Fields, Lengths, Prec_Array, i+1 );
            Fields[i] = Field_Name;
            Lengths[i] = Length;
            Prec_Array[i] = Precedence;
            break;
        }
    }

    return( 0 );
}

static int ProcessXFF_HeaderList(HTTPINSPECT_CONF *ServerConf,
                      char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int Parse_State;
    bool Keep_Parsing;
    bool Have_XFF;
    bool Have_TCI;
    uint8_t Prec_List[HI_UI_CONFIG_MAX_XFF_FIELD_NAMES];
    unsigned char Count = 0;
    unsigned char Max_XFF = { (HI_UI_CONFIG_MAX_XFF_FIELD_NAMES-XFF_BUILTIN_NAMES) };
    uint8_t *Field_Name;
    uint8_t Precedence;
    int i;


    /* NOTE:  This procedure assumes that the ServerConf->xff_headers array
              contains all NULL's due to the structure allocation process. */

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(!pcToken)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid XFF list format.");

        return -1;
    }

    for( i=0; i<HI_UI_CONFIG_MAX_XFF_FIELD_NAMES; i++ ) Prec_List[i] = 0;
    Parse_State = XFF_STATE_START;
    Keep_Parsing = true;

    do
    {
        switch( Parse_State )
        {
            case( XFF_STATE_START ):
            {
                if( strcmp( START_XFF_HEADER_LIST, pcToken ) != 0 )
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                        "Must start an xff header list with the '%s' token.",
                        START_XFF_HEADER_LIST);
                    return( -1 );
                }
                Parse_State = XFF_STATE_OPEN;
                break;
            }
            case( XFF_STATE_OPEN ):
            {
                if( strcmp( START_XFF_HEADER_ENTRY, pcToken ) == 0 )
                    Parse_State = XFF_STATE_NAME;
                else if( strcmp( END_XFF_HEADER_LIST, pcToken ) == 0 )
                {
                    Parse_State = XFF_STATE_END;
                    Keep_Parsing = false;
                }
                else
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                                 "xff header parsing error");
                    return( -1 );
                }
                break;
            }
            case( XFF_STATE_NAME ):
            {
                Field_Name = (uint8_t *)SnortStrdup( pcToken );
                Parse_State = XFF_STATE_PREC;
                break;
            }
            case( XFF_STATE_PREC ):
            {
                Precedence = xatou( pcToken, "Precedence" );

                if( strcasecmp( (char *)Field_Name, HI_UI_CONFIG_XFF_FIELD_NAME) == 0 )
                {
                    Max_XFF += 1;
                    Have_XFF = true;
                }
                else if( strcasecmp( (char *)Field_Name, HI_UI_CONFIG_TCI_FIELD_NAME) == 0 )
                {
                    Max_XFF += 1;
                    Have_TCI = true;
                }
                else
                    Count += 1;

                if( Count > Max_XFF )
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                                 "too many xff field names");
                    return( -1 );
                }

                if( Add_XFF_Field( ServerConf, Prec_List, Field_Name, Precedence,
                                   ErrorString, ErrStrLen ) != 0 )
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                                 "Error adding xff header: %s", Field_Name);
                    return( -1 );
                }

                Parse_State = XFF_STATE_CLOSE;
                break;
            }
            case( XFF_STATE_CLOSE ):
            {
                if( strcmp( END_XFF_HEADER_ENTRY, pcToken ) != 0 )
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                        "Must end an xff header entry with the '%s' token.",
                        END_XFF_HEADER_ENTRY);
                    return( -1 );
                }
                Parse_State = XFF_STATE_OPEN;
                break;
            }
            default:
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                             "xff header parsing error");
                return( -1 );
            }
        }
    }
    while( Keep_Parsing && ((pcToken = strtok(NULL, CONF_SEPARATORS)) != NULL) );

    /* NOTE:  The number of fields added here MUST be represented in XFF_BUILTIN_NAMES value
              to assure that we reserve room for them on the list. */
    if( !Have_XFF )
    {
        Field_Name = (uint8_t *)SnortStrdup( HI_UI_CONFIG_XFF_FIELD_NAME );
        if( Add_XFF_Field( ServerConf, Prec_List, Field_Name, 0,
                           ErrorString, ErrStrLen ) != 0 )
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                         "problem adding builtin xff field - too many fields?");
            return( -1 );
        }
    }

    if( !Have_TCI )
    {
        Field_Name = (uint8_t *)SnortStrdup( HI_UI_CONFIG_TCI_FIELD_NAME );
        if( Add_XFF_Field( ServerConf, Prec_List, Field_Name, 0,
                           ErrorString, ErrStrLen ) != 0 )
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                         "problem adding builtin xff field - too many fields?");
            return( -1 );
        }
    }


    return 0;
}

static int ProcessHttpMethodList(HTTPINSPECT_CONF *ServerConf,
                      char *ErrorString, int ErrStrLen)
{
    HTTP_CMD_CONF *HTTPMethods = NULL;
    char *pcToken;
    char *cmd;
    int  iEndCmds = 0;
    int  iRet;


    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(!pcToken)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Invalid cmd list format.");

        return -1;
    }

    if(strcmp(START_PORT_LIST, pcToken))
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                "Must start a http request method list with the '%s' token.",
                START_PORT_LIST);

        return -1;
    }

    while ((pcToken = strtok(NULL, CONF_SEPARATORS)) != NULL)
    {
        if(!strcmp(END_PORT_LIST, pcToken))
        {
            iEndCmds = 1;
            break;
        }

        cmd = pcToken;

        /* Max method length cannot exceed 256, as this is the size of the key array used in KMapAdd function */
        if(strlen(pcToken) > MAX_METHOD_LEN)
        {
            snprintf(ErrorString, ErrStrLen,
                    "Length of the http request method should not exceed the max request method length of '%d'.",
                    MAX_METHOD_LEN);
            return -1;
        }

        HTTPMethods = http_cmd_lookup_find(ServerConf->cmd_lookup, cmd, strlen(cmd), &iRet);

        if (HTTPMethods == NULL)
        {
            /* Add it to the list */
            // note that struct includes 1 byte for null, so just add len
            HTTPMethods = (HTTP_CMD_CONF *)calloc(1, sizeof(HTTP_CMD_CONF)+strlen(cmd));
            if (HTTPMethods == NULL)
            {
                FatalError("%s(%d) Could not allocate memory for HTTP Method configuration.\n",
                                           __FILE__, __LINE__);
            }

            strcpy(HTTPMethods->cmd_name, cmd);

            http_cmd_lookup_add(ServerConf->cmd_lookup, cmd, strlen(cmd), HTTPMethods);
        }
    }


    if(!iEndCmds)
    {
        snprintf(ErrorString, ErrStrLen,
            "Must end '%s' configuration with '%s'.",
                HTTP_METHODS, END_PORT_LIST);

        return -1;
    }

    return 0;
}

/*
**  NAME
**    ProcessMaxSpaces::
*/
/**
**  Process and verify the maximum allowed spaces for the
**  server configuration.
**
**  @param ServerConf  pointer to the server configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessMaxSpaces(HTTPINSPECT_CONF *ServerConf,
                              char *ErrorString, int ErrStrLen, char *configOption, SpaceType type)
{
    char *pcToken;
    int  num_spaces;
    char *pcEnd;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No argument to '%s' token.", configOption);

        return -1;
    }

     num_spaces = SnortStrtolRange(pcToken, &pcEnd, 10, 0 , INT_MAX);
     if(*pcEnd)
     {
         SnortSnprintf(ErrorString, ErrStrLen,
                     "Invalid argument to '%s'.", configOption);

         return -1;
     }

    if(num_spaces < 0)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'. Valid range is 0 to 65535.", configOption);

        return -1;
    }

    if(num_spaces > 65535)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "Invalid argument to '%s'.  Valid range is 0 to 65535.", configOption);

        return -1;
    }

    switch(type)
    {
        case CONFIG_MAX_SPACES:
            ServerConf->max_spaces = num_spaces;
            break;
        case CONFIG_MAX_JS_WS:
            ServerConf->max_js_ws = num_spaces;
            break;
        default:
            break;
    }

    return 0;
}


/*
**  NAME
**    ProcessServerConf::
*/
/**
**  Process the global server configuration.
**
**  Take the configuration and translate into the global server
**  configuration.  We also check for any configuration errors and
**  invalid keywords.
**
**  @param ServerConf  pointer to the server configuration
**  @param ErrorString error string buffer
**  @param ErrStrLen   the length of the error string buffer
**
**  @return an error code integer
**          (0 = success, >0 = non-fatal error, <0 = fatal error)
**
**  @retval  0 successs
**  @retval -1 generic fatal error
**  @retval  1 generic non-fatal error
*/
static int ProcessServerConf(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                             HTTPINSPECT_CONF *ServerConf,
                             char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    int  iRet;
    int  iPorts = 0;
    HTTPINSPECT_CONF_OPT *ConfOpt;

    /*
    **  Check for profile keyword first, it's the only place in the
    **  configuration that is correct.
    */
    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(pcToken == NULL)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "WARNING: No tokens to '%s' configuration.", SERVER);

        return 1;
    }

    if(!strcmp(PROFILE, pcToken))
    {
        iRet = ProcessProfile(GlobalConf, ServerConf, ErrorString, ErrStrLen);
        if (iRet)
        {
            return iRet;
        }

        pcToken = strtok(NULL, CONF_SEPARATORS);
        if(pcToken == NULL)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "No port list to the profile token.");

            return -1;
        }

        do
        {
            if(!strcmp(PORTS, pcToken))
            {
                iRet = ProcessPorts(ServerConf, ErrorString, ErrStrLen);
                if (iRet)
                {
                    return iRet;
                }

                iPorts = 1;
            }
            else if(!strcmp(IIS_UNICODE_MAP, pcToken))
            {
                iRet = ProcessIISUnicodeMap(&ServerConf->iis_unicode_map,
                                            &ServerConf->iis_unicode_map_filename,
                                            &ServerConf->iis_unicode_codepage,
                                            ErrorString,ErrStrLen);
                if (iRet)
                {
                    return -1;
                }
            }
            else if(!strcmp(ALLOW_PROXY, pcToken))
            {
                ServerConf->allow_proxy = 1;
            }
            else if(!strcmp(FLOW_DEPTH, pcToken) || !strcmp(SERVER_FLOW_DEPTH, pcToken))
            {
                iRet = ProcessFlowDepth(ServerConf, HI_SI_SERVER_MODE, ErrorString, ErrStrLen, pcToken, MAX_SERVER_DEPTH);
                if (iRet)
                {
                    return iRet;
                }
            }
            else if(!strcmp(CLIENT_FLOW_DEPTH, pcToken))
            {
                iRet = ProcessFlowDepth(ServerConf, HI_SI_CLIENT_MODE, ErrorString, ErrStrLen, pcToken, MAX_CLIENT_DEPTH);
                if (iRet)
                {
                    return iRet;
                }
            }
            else if(!strcmp(POST_DEPTH, pcToken))
            {
                iRet = ProcessPostDepth(ServerConf, ErrorString, ErrStrLen);
                if (iRet)
                {
                    return iRet;
                }
            }
            else if(!strcmp(GLOBAL_ALERT, pcToken))
            {
                ServerConf->no_alerts = 1;
            }
            else if(!strcmp(OVERSIZE_DIR, pcToken))
            {
                iRet = ProcessOversizeDir(ServerConf, ErrorString, ErrStrLen);
                if (iRet)
                {
                    return iRet;
                }

            }
            else if(!strcmp(INSPECT_URI_ONLY, pcToken))
            {
                ServerConf->uri_only = 1;
            }
            else if (!strcmp(NORMALIZE_HEADERS, pcToken))
            {
                ServerConf->normalize_headers = 1;
            }
            else if (!strcmp(NORMALIZE_COOKIES, pcToken))
            {
                ServerConf->normalize_cookies = 1;
            }
            else if (!strcmp(NORMALIZE_UTF, pcToken))
            {
                ServerConf->normalize_utf = 1;
            }
            else if (!strcmp(NORMALIZE_JS, pcToken))
            {
                if(!ServerConf->inspect_response)
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                                        "Enable '%s' before setting '%s'",INSPECT_RESPONSE, NORMALIZE_JS);
                    return -1;
                }
                ServerConf->normalize_javascript = 1;
            }
            else if(!strcmp(MAX_JS_WS, pcToken))
            {
                if(!ServerConf->normalize_javascript)
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                                "Enable '%s' before setting '%s'", NORMALIZE_JS, MAX_JS_WS);
                    return -1;
                }
                iRet = ProcessMaxSpaces(ServerConf, ErrorString, ErrStrLen, MAX_JS_WS, CONFIG_MAX_JS_WS);
                if (iRet)
                {
                    return iRet;
                }
            }

            else if (!strcmp(INSPECT_COOKIES, pcToken))
            {
                ServerConf->enable_cookie = 1;
            }
            else if (!strcmp(INSPECT_RESPONSE, pcToken))
            {
                ServerConf->inspect_response = 1;
            }
            else if (!strcmp(HTTP_METHODS, pcToken))
            {
                iRet = ProcessHttpMethodList(ServerConf, ErrorString, ErrStrLen);
                if (iRet)
                {
                    return iRet;
                }
            }
#ifdef ZLIB
            else if (!strcmp(EXTRACT_GZIP, pcToken))
            {
                if(!ServerConf->inspect_response)
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                            "Enable '%s' before setting '%s'",INSPECT_RESPONSE, EXTRACT_GZIP);
                    return -1;
                }

                ServerConf->extract_gzip = 1;
            }
            else if (!strcmp(UNLIMIT_DECOMPRESS, pcToken))
            {
                if(!ServerConf->extract_gzip)
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                            "Enable '%s' before setting '%s'",EXTRACT_GZIP, UNLIMIT_DECOMPRESS);
                    return -1;
                }

                if((GlobalConf->compr_depth != MAX_GZIP_DEPTH) && (GlobalConf->decompr_depth != MAX_GZIP_DEPTH))
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                            "'%s' and '%s' should be set to max in the default policy to enable '%s'",
                            COMPRESS_DEPTH, DECOMPRESS_DEPTH, UNLIMIT_DECOMPRESS);
                    return -1;
                }

                GlobalConf->compr_depth = GlobalConf->decompr_depth = MAX_GZIP_DEPTH;
                ServerConf->unlimited_decompress = 1;
            }
#endif
            else if(!strcmp(MAX_HDR_LENGTH, pcToken))
            {
                iRet = ProcessMaxHdrLen(ServerConf, ErrorString, ErrStrLen);
                if (iRet)
                {
                    return iRet;
                }
            }
            else if(!strcmp(MAX_HEADERS, pcToken))
            {
                iRet = ProcessMaxHeaders(ServerConf, ErrorString, ErrStrLen);
                if (iRet)
                {
                    return iRet;
                }
            }
            else if(!strcmp(MAX_SPACES, pcToken))
            {
                iRet = ProcessMaxSpaces(ServerConf, ErrorString, ErrStrLen, MAX_SPACES, CONFIG_MAX_SPACES);
                if (iRet)
                {
                    return iRet;
                }
            }
            else if(!strcmp(ENABLE_XFF, pcToken))
            {
                ServerConf->enable_xff = 1;
            }
            else if(!strcmp(XFF_HEADERS_TOK, pcToken))
            {
                if(!ServerConf->enable_xff)
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                                  "Enable '%s' before setting '%s'",ENABLE_XFF, XFF_HEADERS_TOK);
                    return -1;
                }

                if( ((iRet = ProcessXFF_HeaderList(ServerConf, ErrorString, ErrStrLen)) != 0)  )
                {
                    return iRet;
                }
            }
            else if(!strcmp(LOG_URI, pcToken))
            {
                ServerConf->log_uri = 1;
            }
            else if(!strcmp(LOG_HOSTNAME, pcToken))
            {
                ServerConf->log_hostname = 1;
            }
            else if(!strcmp(SMALL_CHUNK_LENGTH, pcToken))
            {
                iRet = ProcessSmallChunkLength(ServerConf,ErrorString,ErrStrLen);
                if (iRet)
                {
                    return iRet;
                }
            }
            else
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                              "Invalid token while configuring the profile token.  "
                              "The only allowed tokens when configuring profiles "
                              "are: '%s', '%s', '%s', '%s', '%s', '%s', '%s', "
                              "'%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s' "
                              "'%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', and '%s'. ",
                              PORTS,IIS_UNICODE_MAP, ALLOW_PROXY, FLOW_DEPTH,
                              CLIENT_FLOW_DEPTH, GLOBAL_ALERT, OVERSIZE_DIR, MAX_HDR_LENGTH,
                              INSPECT_URI_ONLY, INSPECT_COOKIES, INSPECT_RESPONSE,
                              EXTRACT_GZIP,MAX_HEADERS, NORMALIZE_COOKIES, ENABLE_XFF, XFF_HEADERS_TOK,
                              NORMALIZE_HEADERS, NORMALIZE_UTF, UNLIMIT_DECOMPRESS, HTTP_METHODS, 
                              LOG_URI, LOG_HOSTNAME, MAX_SPACES, NORMALIZE_JS, MAX_JS_WS);

                return -1;
            }

        }  while ((pcToken = strtok(NULL, CONF_SEPARATORS)) != NULL);

        if(!iPorts)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "No port list to the profile token.");

            return -1;
        }

        return 0;
    }

    /*
    **  If there is no profile configuration then we go into the hard-core
    **  configuration.
    */

    hi_ui_config_reset_http_methods(ServerConf);

    do
    {
        if(!strcmp(PORTS, pcToken))
        {
            iRet = ProcessPorts(ServerConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(FLOW_DEPTH, pcToken) || !strcmp(SERVER_FLOW_DEPTH, pcToken))
        {
            iRet = ProcessFlowDepth(ServerConf, HI_SI_SERVER_MODE, ErrorString, ErrStrLen, pcToken, MAX_SERVER_DEPTH);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(CLIENT_FLOW_DEPTH, pcToken))
        {
            iRet = ProcessFlowDepth(ServerConf, HI_SI_CLIENT_MODE, ErrorString, ErrStrLen, pcToken, MAX_CLIENT_DEPTH);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(POST_DEPTH, pcToken))
        {
            iRet = ProcessPostDepth(ServerConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(IIS_UNICODE_MAP, pcToken))
        {
            iRet = ProcessIISUnicodeMap(&ServerConf->iis_unicode_map,
                                        &ServerConf->iis_unicode_map_filename,
                                        &ServerConf->iis_unicode_codepage,
                                        ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(CHUNK_LENGTH, pcToken))
        {
            iRet = ProcessChunkLength(ServerConf,ErrorString,ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(SMALL_CHUNK_LENGTH, pcToken))
        {
            iRet = ProcessSmallChunkLength(ServerConf,ErrorString,ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(PIPELINE, pcToken))
        {
            ServerConf->no_pipeline = 1;
        }
        else if(!strcmp(NON_STRICT, pcToken))
        {
            ServerConf->non_strict = 1;
        }
        else if(!strcmp(ALLOW_PROXY, pcToken))
        {
            ServerConf->allow_proxy = 1;
        }
        else if(!strcmp(GLOBAL_ALERT, pcToken))
        {
            ServerConf->no_alerts = 1;
        }
        else if(!strcmp(TAB_URI_DELIMITER, pcToken))
        {
            ServerConf->tab_uri_delimiter = 1;
        }
        else if(!strcmp(EXTENDED_ASCII, pcToken))
        {
            ServerConf->extended_ascii_uri = 1;
        }
        else if (!strcmp(NORMALIZE_HEADERS, pcToken))
        {
            ServerConf->normalize_headers = 1;
        }
        else if (!strcmp(NORMALIZE_COOKIES, pcToken))
        {
            ServerConf->normalize_cookies = 1;
        }
        else if (!strcmp(NORMALIZE_UTF, pcToken))
        {
            ServerConf->normalize_utf = 1;
        }
        else if (!strcmp(NORMALIZE_JS, pcToken))
        {
            if(!ServerConf->inspect_response)
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                        "Enable '%s' before setting '%s'", INSPECT_RESPONSE, NORMALIZE_JS);
                return -1;
            }
            ServerConf->normalize_javascript = 1;
        }
        else if(!strcmp(MAX_JS_WS, pcToken))
        {
            if(!ServerConf->normalize_javascript)
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                                    "Enable '%s' before setting '%s'", NORMALIZE_JS, MAX_JS_WS);
                return -1;
            }
            iRet = ProcessMaxSpaces(ServerConf, ErrorString, ErrStrLen, MAX_JS_WS, CONFIG_MAX_JS_WS);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(OVERSIZE_DIR, pcToken))
        {
            iRet = ProcessOversizeDir(ServerConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }

        }
        else if(!strcmp(INSPECT_URI_ONLY, pcToken))
        {
            ServerConf->uri_only = 1;
        }

        else if(!strcmp(INSPECT_COOKIES, pcToken))
        {
            ServerConf->enable_cookie = 1;
        }
        else if(!strcmp(INSPECT_RESPONSE, pcToken))
        {
            ServerConf->inspect_response = 1;
        }
        else if (!strcmp(HTTP_METHODS, pcToken))
        {
            iRet = ProcessHttpMethodList(ServerConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
#ifdef ZLIB
        else if(!strcmp(EXTRACT_GZIP, pcToken))
        {
            if(!ServerConf->inspect_response)
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                        "Enable '%s' inspection before setting '%s'",INSPECT_RESPONSE, EXTRACT_GZIP);
                return -1;
            }

            ServerConf->extract_gzip = 1;
        }
        else if(!strcmp(UNLIMIT_DECOMPRESS, pcToken))
        {
            if(!ServerConf->extract_gzip)
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                        "Enable '%s' inspection before setting '%s'",EXTRACT_GZIP, UNLIMIT_DECOMPRESS);
                return -1;
            }
            if((GlobalConf->compr_depth != MAX_GZIP_DEPTH) && (GlobalConf->decompr_depth != MAX_GZIP_DEPTH))
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                    "'%s' and '%s' should be set to max in the default policy to enable '%s'",
                    COMPRESS_DEPTH, DECOMPRESS_DEPTH, UNLIMIT_DECOMPRESS);
                return -1;
            }

            GlobalConf->compr_depth = GlobalConf->decompr_depth = MAX_GZIP_DEPTH;
            ServerConf->unlimited_decompress = 1;
        }
#endif
        /*
        **  Start the CONF_OPT configurations.
        */
        else if(!strcmp(ASCII, pcToken))
        {
            ConfOpt = &ServerConf->ascii;
            iRet = ProcessConfOpt(ConfOpt, ASCII, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(UTF_8, pcToken))
        {
            /*
            **  In order for this to work we also need to set ASCII
            */
            ServerConf->ascii.on    = 1;

            ConfOpt = &ServerConf->utf_8;
            iRet = ProcessConfOpt(ConfOpt, UTF_8, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(IIS_UNICODE, pcToken))
        {
            if(ServerConf->iis_unicode_map == NULL)
            {
                ServerConf->iis_unicode_map = GlobalConf->iis_unicode_map;
            }

            /*
            **  We need to set up:
            **    - ASCII
            **    - DOUBLE_DECODE
            **    - U_ENCODE
            **    - BARE_BYTE
            **    - IIS_UNICODE
            **
            **     Base 36 is deprecated and essentially a noop
            **    - BASE36
            */
            ServerConf->ascii.on           = 1;

            ConfOpt = &ServerConf->iis_unicode;
            iRet = ProcessConfOpt(ConfOpt, IIS_UNICODE, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(DOUBLE_DECODE, pcToken))
        {
            ServerConf->ascii.on             = 1;

            ConfOpt = &ServerConf->double_decoding;
            iRet = ProcessConfOpt(ConfOpt, DOUBLE_DECODE, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(U_ENCODE, pcToken))
        {
            /*
            **  We set the unicode map to default if it's not already
            **  set.
            */
            if(ServerConf->iis_unicode_map == NULL)
            {
                ServerConf->iis_unicode_map = GlobalConf->iis_unicode_map;
            }

            ConfOpt = &ServerConf->u_encoding;
            iRet = ProcessConfOpt(ConfOpt, U_ENCODE, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(BARE_BYTE, pcToken))
        {
            ConfOpt = &ServerConf->bare_byte;
            iRet = ProcessConfOpt(ConfOpt, BARE_BYTE, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(BASE36, pcToken))
        {
            /* Base 36 is deprecated and essentially a noop */
            ErrorMessage("WARNING: %s (%d): The \"base36\" option to the "
                    "\"http_inspect\" preprocessor configuration is "
                    "deprecated and void of functionality.\n",
                    file_name, file_line);

            /* Need to get and chuck yes/no argument to option since
             * we're not doing anything with this anymore. */
            pcToken = strtok(NULL, CONF_SEPARATORS);
        }
        else if(!strcmp(NON_RFC_CHAR, pcToken))
        {
            iRet = ProcessNonRfcChar(ServerConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(MULTI_SLASH, pcToken))
        {
            ConfOpt = &ServerConf->multiple_slash;
            iRet = ProcessConfOpt(ConfOpt, MULTI_SLASH, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(IIS_BACKSLASH, pcToken))
        {
            ConfOpt = &ServerConf->iis_backslash;
            iRet = ProcessConfOpt(ConfOpt, IIS_BACKSLASH, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(DIRECTORY, pcToken))
        {
            ConfOpt = &ServerConf->directory;
            iRet = ProcessConfOpt(ConfOpt, DIRECTORY, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(APACHE_WS, pcToken))
        {
            ConfOpt = &ServerConf->apache_whitespace;
            iRet = ProcessConfOpt(ConfOpt, APACHE_WS, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(WHITESPACE, pcToken))
        {
            iRet = ProcessWhitespaceChars(ServerConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
         else if(!strcmp(IIS_DELIMITER, pcToken))
        {
            ConfOpt = &ServerConf->iis_delimiter;
            iRet = ProcessConfOpt(ConfOpt, IIS_DELIMITER, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(WEBROOT, pcToken))
        {
            ConfOpt = &ServerConf->webroot;
            iRet = ProcessConfOpt(ConfOpt, WEBROOT, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(MAX_HDR_LENGTH, pcToken))
        {
            iRet = ProcessMaxHdrLen(ServerConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(MAX_HEADERS, pcToken))
        {
            iRet = ProcessMaxHeaders(ServerConf, ErrorString, ErrStrLen);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(MAX_SPACES, pcToken))
        {
            iRet = ProcessMaxSpaces(ServerConf, ErrorString, ErrStrLen, MAX_SPACES, CONFIG_MAX_SPACES);
            if (iRet)
            {
                return iRet;
            }
        }
        else if(!strcmp(ENABLE_XFF, pcToken))
        {
            ServerConf->enable_xff = 1;
        }
        else if(!strcmp(XFF_HEADERS_TOK, pcToken))
        {
            if(!ServerConf->enable_xff)
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                        "Enable '%s' before setting '%s'",ENABLE_XFF, XFF_HEADERS_TOK);
                return -1;
            }

            if( ((iRet = ProcessXFF_HeaderList(ServerConf, ErrorString, ErrStrLen)) != 0)  )
            {
                return iRet;
            }
        }
        else if(!strcmp(LOG_URI, pcToken))
        {
            ServerConf->log_uri = 1;
        }   
        else if(!strcmp(LOG_HOSTNAME, pcToken))
        {
            ServerConf->log_hostname = 1;
        }
        else
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Invalid keyword '%s' for server configuration.",
                          pcToken);

            return -1;
        }

    } while ((pcToken = strtok(NULL, CONF_SEPARATORS)) != NULL);

    return 0;
}

static int PrintConfOpt(HTTPINSPECT_CONF_OPT *ConfOpt, char *Option)
{
    if(!ConfOpt || !Option)
    {
        return HI_INVALID_ARG;
    }

    if(ConfOpt->on)
    {
        LogMessage("      %s: YES alert: %s\n", Option,
               ConfOpt->alert ? "YES" : "NO");
    }
    else
    {
        LogMessage("      %s: OFF\n", Option);
    }

    return 0;
}

static int PrintServerConf(HTTPINSPECT_CONF *ServerConf)
{
    char buf[STD_BUF+1];
    int iCtr;
    int iChar = 0;
    char* paf = "";
    PROFILES prof;

    if(!ServerConf)
    {
        return HI_INVALID_ARG;
    }

    prof = ServerConf->profile;
    LogMessage("      Server profile: %s\n",
        prof==HI_ALL?"All":
        prof==HI_APACHE?"Apache":
        prof==HI_IIS?"IIS":
        prof==HI_IIS4?"IIS4":"IIS5");


    memset(buf, 0, STD_BUF+1);

    if ( ScPafEnabled() && stream_api )
        paf = " (PAF)";

    SnortSnprintf(buf, STD_BUF + 1, "      Ports%s: ", paf);

    /*
    **  Print out all the applicable ports.
    */
    for(iCtr = 0; iCtr < MAXPORTS; iCtr++)
    {
        if(ServerConf->ports[iCtr/8] & (1 << (iCtr % 8) ))
        {
            sfsnprintfappend(buf, STD_BUF, "%d ", iCtr);
        }
    }

    LogMessage("%s\n", buf);

    LogMessage("      Server Flow Depth: %d\n", ServerConf->server_flow_depth);
    LogMessage("      Client Flow Depth: %d\n", ServerConf->client_flow_depth);
    LogMessage("      Max Chunk Length: %d\n", ServerConf->chunk_length);
    if (ServerConf->small_chunk_length.size > 0)
        LogMessage("      Small Chunk Length Evasion: chunk size <= %u, threshold >= %u times\n",
                   ServerConf->small_chunk_length.size, ServerConf->small_chunk_length.num);
    LogMessage("      Max Header Field Length: %d\n", ServerConf->max_hdr_len);
    LogMessage("      Max Number Header Fields: %d\n", ServerConf->max_headers);
    LogMessage("      Max Number of WhiteSpaces allowed with header folding: %d\n", ServerConf->max_spaces);
    LogMessage("      Inspect Pipeline Requests: %s\n",
               ServerConf->no_pipeline ? "NO" : "YES");
    LogMessage("      URI Discovery Strict Mode: %s\n",
               ServerConf->non_strict ? "NO" : "YES");
    LogMessage("      Allow Proxy Usage: %s\n",
               ServerConf->allow_proxy ? "YES" : "NO");
    LogMessage("      Disable Alerting: %s\n",
               ServerConf->no_alerts ? "YES":"NO");
    LogMessage("      Oversize Dir Length: %d\n",
               ServerConf->long_dir);
    LogMessage("      Only inspect URI: %s\n",
               ServerConf->uri_only ? "YES" : "NO");
    LogMessage("      Normalize HTTP Headers: %s\n",
               ServerConf->normalize_headers ? "YES" : "NO");
    LogMessage("      Inspect HTTP Cookies: %s\n",
               ServerConf->enable_cookie ? "YES" : "NO");
    LogMessage("      Inspect HTTP Responses: %s\n",
               ServerConf->inspect_response ? "YES" : "NO");
#ifdef ZLIB
    LogMessage("      Extract Gzip from responses: %s\n",
               ServerConf->extract_gzip ? "YES" : "NO");
    LogMessage("      Unlimited decompression of gzip data from responses: %s\n",
                ServerConf->unlimited_decompress ? "YES" : "NO");
#endif
    LogMessage("      Normalize Javascripts in HTTP Responses: %s\n",
                       ServerConf->normalize_javascript ? "YES" : "NO");
    if(ServerConf->normalize_javascript)
    {
        if(ServerConf->max_js_ws)
            LogMessage("      Max Number of WhiteSpaces allowed with Javascript Obfuscation in HTTP responses: %d\n", ServerConf->max_js_ws);
    }
    LogMessage("      Normalize HTTP Cookies: %s\n",
               ServerConf->normalize_cookies ? "YES" : "NO");
    LogMessage("      Enable XFF and True Client IP: %s\n",
               ServerConf->enable_xff ? "YES"  :  "NO");
    LogMessage("      Log HTTP URI data: %s\n",
               ServerConf->log_uri ? "YES"  :  "NO");
    LogMessage("      Log HTTP Hostname data: %s\n",
               ServerConf->log_hostname ? "YES"  :  "NO");
    LogMessage("      Extended ASCII code support in URI: %s\n",
               ServerConf->extended_ascii_uri ? "YES" : "NO");


    PrintConfOpt(&ServerConf->ascii, "Ascii");
    PrintConfOpt(&ServerConf->double_decoding, "Double Decoding");
    PrintConfOpt(&ServerConf->u_encoding, "%U Encoding");
    PrintConfOpt(&ServerConf->bare_byte, "Bare Byte");
    PrintConfOpt(&ServerConf->utf_8, "UTF 8");
    PrintConfOpt(&ServerConf->iis_unicode, "IIS Unicode");
    PrintConfOpt(&ServerConf->multiple_slash, "Multiple Slash");
    PrintConfOpt(&ServerConf->iis_backslash, "IIS Backslash");
    PrintConfOpt(&ServerConf->directory, "Directory Traversal");
    PrintConfOpt(&ServerConf->webroot, "Web Root Traversal");
    PrintConfOpt(&ServerConf->apache_whitespace, "Apache WhiteSpace");
    PrintConfOpt(&ServerConf->iis_delimiter, "IIS Delimiter");

    if(ServerConf->iis_unicode_map_filename)
    {
        LogMessage("      IIS Unicode Map Filename: %s\n",
                   ServerConf->iis_unicode_map_filename);
        LogMessage("      IIS Unicode Map Codepage: %d\n",
                   ServerConf->iis_unicode_codepage);
    }
    else if(ServerConf->iis_unicode_map)
    {
        LogMessage("      IIS Unicode Map: "
                   "GLOBAL IIS UNICODE MAP CONFIG\n");
    }
    else
    {
        LogMessage("      IIS Unicode Map:  NOT CONFIGURED\n");
    }

    /*
    **  Print out the non-rfc chars
    */
    memset(buf, 0, STD_BUF+1);
    SnortSnprintf(buf, STD_BUF + 1, "      Non-RFC Compliant Characters: ");
    for(iCtr = 0; iCtr < 256; iCtr++)
    {
        if(ServerConf->non_rfc_chars[iCtr])
        {
            sfsnprintfappend(buf, STD_BUF, "0x%.2x ", (u_char)iCtr);
            iChar = 1;
        }
    }

    if(!iChar)
    {
        sfsnprintfappend(buf, STD_BUF, "NONE");
    }

    LogMessage("%s\n", buf);

    /*
    **  Print out the whitespace chars
    */
    iChar = 0;
    memset(buf, 0, STD_BUF+1);
    SnortSnprintf(buf, STD_BUF + 1, "      Whitespace Characters: ");
    for(iCtr = 0; iCtr < 256; iCtr++)
    {
        if(ServerConf->whitespace[iCtr])
        {
            sfsnprintfappend(buf, STD_BUF, "0x%.2x ", (u_char)iCtr);
            iChar = 1;
        }
    }

    if(!iChar)
    {
        sfsnprintfappend(buf, STD_BUF, "NONE");
    }

    LogMessage("%s\n", buf);

    return 0;
}

int ProcessUniqueServerConf(HTTPINSPECT_GLOBAL_CONF *GlobalConf,
                            char *ErrorString, int ErrStrLen)
{
    char *pcToken;
    char *pIpAddressList = NULL;
    char *pIpAddressList2 = NULL;
    char *brkt = NULL;
    char firstIpAddress = 1;
    sfip_t Ip;
    HTTPINSPECT_CONF *ServerConf = NULL;
    int iRet;
    int retVal = -1;

    pcToken = strtok(NULL, CONF_SEPARATORS);
    if(!pcToken)
    {
        SnortSnprintf(ErrorString, ErrStrLen,
                      "No arguments to '%s' token.", SERVER);

        retVal = -1;
        goto _return;
    }

    /*
    **  Check for the default configuration first
    */
    if (strcasecmp(SERVER_DEFAULT, pcToken) == 0)
    {
        if (GlobalConf->global_server != NULL)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                          "Cannot configure '%s' settings more than once.",
                          GLOBAL_SERVER);

            goto _return;
        }

        GlobalConf->global_server =
            (HTTPINSPECT_CONF *)SnortAlloc(sizeof(HTTPINSPECT_CONF));

        ServerConf = GlobalConf->global_server;

        iRet = hi_ui_config_default(ServerConf);
        if (iRet)
        {
            snprintf(ErrorString, ErrStrLen,
                     "Error configuring default global configuration.");
            return -1;
        }

        iRet = ProcessServerConf(GlobalConf, ServerConf, ErrorString, ErrStrLen);
        if (iRet)
        {
            retVal =  iRet;
            goto _return;
        }

        /*
        **  Start writing out the Default Server Config
        */
        LogMessage("    DEFAULT SERVER CONFIG:\n");
    }
    else
    {
        /*
        **  Convert string to IP address
        */
        /* get the first delimiter*/
        if(strcmp(START_IPADDR_LIST, pcToken) == 0)
        {
            /*list begin token matched*/
            if ((pIpAddressList = strtok(NULL, END_IPADDR_LIST)) == NULL)
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                        "Invalid IP Address list in '%s' token.", SERVER);

                goto _return;
            }
        }
        else
        {
            /*list begin didn't match so this must be an IP address*/
            pIpAddressList = pcToken;
        }


        pIpAddressList2 = strdup(pIpAddressList);
        if (!pIpAddressList2)
        {
            SnortSnprintf(ErrorString, ErrStrLen,
                    "Could not allocate memory for server configuration.");

            goto _return;
        }



        for (pcToken = strtok_r(pIpAddressList, CONF_SEPARATORS, &brkt);
             pcToken;
             pcToken = strtok_r(NULL, CONF_SEPARATORS, &brkt))
        {

            if (sfip_pton(pcToken, &Ip) != SFIP_SUCCESS)
            {
                SnortSnprintf(ErrorString, ErrStrLen,
                        "Invalid IP to '%s' token.", SERVER);

                goto _return;
            }

            if (Ip.family == AF_INET)
            {
                Ip.ip.u6_addr32[0] = ntohl(Ip.ip.u6_addr32[0]);
            }

            /*
             **  allocate the memory for the server configuration
             */
            if (firstIpAddress)
            {
                ServerConf = (HTTPINSPECT_CONF *)calloc(1, sizeof(HTTPINSPECT_CONF));
                if(!ServerConf)
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                            "Could not allocate memory for server configuration.");

                    goto _return;
                }

                iRet = ProcessServerConf(GlobalConf, ServerConf, ErrorString, ErrStrLen);
                if (iRet)
                {
                    retVal = iRet;
                    goto _return;
                }
            }

            iRet = hi_ui_config_add_server(GlobalConf, &Ip, ServerConf);
            if (iRet)
            {
                /*
                 **  Check for already added servers
                 */
                if(iRet == HI_NONFATAL_ERR)
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                            "Duplicate server configuration.");

                    goto _return;
                }
                else
                {
                    SnortSnprintf(ErrorString, ErrStrLen,
                            "Error when adding server configuration.");

                    goto _return;
                }
            }

            if (firstIpAddress)
            {
                //process the first IP address as usual
                firstIpAddress = 0;
            }

            //create a reference
            ServerConf->referenceCount++;

        }

        if (firstIpAddress)
        {
            //no IP address was found
            SnortSnprintf(ErrorString, ErrStrLen,
                    "Invalid IP Address list in '%s' token.", SERVER);

            goto _return;
        }

        /*
        **  Print out the configuration header
        */
        LogMessage("    SERVER: %s\n", pIpAddressList2);
    }

    /*
    **  Finish printing out the server configuration
    */
    PrintServerConf(ServerConf);

    retVal = 0;

_return:
    if (pIpAddressList2)
    {
        free(pIpAddressList2);
    }
    return retVal;
}

int PrintGlobalConf(HTTPINSPECT_GLOBAL_CONF *GlobalConf)
{
    LogMessage("HttpInspect Config:\n");

    LogMessage("    GLOBAL CONFIG\n");
    if(GlobalConf->disabled)
    {
        LogMessage("      Http Inspect: INACTIVE\n");
#ifdef ZLIB
        LogMessage("      Max Gzip Memory: %d\n",
                                GlobalConf->max_gzip_mem);
#endif
        LogMessage("      Memcap used for logging URI and Hostname: %u\n",
                                GlobalConf->memcap);
        return 0;
    }
    LogMessage("      Max Pipeline Requests:    %d\n",
               GlobalConf->max_pipeline_requests);
    LogMessage("      Inspection Type:          %s\n",
               GlobalConf->inspection_type ? "STATEFUL" : "STATELESS");
    LogMessage("      Detect Proxy Usage:       %s\n",
               GlobalConf->proxy_alert ? "YES" : "NO");
    LogMessage("      IIS Unicode Map Filename: %s\n",
               GlobalConf->iis_unicode_map_filename);
    LogMessage("      IIS Unicode Map Codepage: %d\n",
               GlobalConf->iis_unicode_codepage);
    LogMessage("      Memcap used for logging URI and Hostname: %u\n",
               GlobalConf->memcap);
#ifdef ZLIB
    LogMessage("      Max Gzip Memory: %d\n",
                GlobalConf->max_gzip_mem);
    LogMessage("      Max Gzip Sessions: %d\n",
               GlobalConf->max_gzip_sessions);
    LogMessage("      Gzip Compress Depth: %d\n",
               GlobalConf->compr_depth);
    LogMessage("      Gzip Decompress Depth: %d\n",
               GlobalConf->decompr_depth);
#endif

    return 0;
}

/*
**  NAME
**    LogEvents::
*/
/**
**  This is the routine that logs HttpInspect alerts through Snort.
**
**  Every Session gets looked at for any logged events, and if there are
**  events to be logged then we select the one with the highest priority.
**
**  We use a generic event structure that we set for each different event
**  structure.  This way we can use the same code for event logging regardless
**  of what type of event strucure we are dealing with.
**
**  The important things to know about this function is how to work with
**  the event queue.  The number of unique events is contained in the
**  stack_count variable.  So we loop through all the unique events and
**  find which one has the highest priority.  During this loop, we also
**  re-initialize the individual event counts for the next iteration, saving
**  us time in a separate initialization phase.
**
**  After we've iterated through all the events and found the one with the
**  highest priority, we then log that event through snort.
**
**  We've mapped the HttpInspect and the Snort alert IDs together, so we
**  can access them directly instead of having a more complex mapping
**  function.  It's the only good way to do this.
**
**  @param Session          pointer to Session construct
**  @param p                pointer to the Snort packet construct
**  @param iInspectMode     inspection mode to take event queue from
**
**  @return integer
**
**  @retval 0 this function only return success
*/
static inline int LogEvents(HI_SESSION *hi_ssn, Packet *p,
        int iInspectMode, HttpSessionData *hsd)
{
    HI_GEN_EVENTS GenEvents;
    HI_EVENT      *OrigEvent;
    HI_EVENT      *HiEvent = NULL;
    uint32_t     uiMask = 0;
    int           iGenerator;
    int           iStackCnt;
    int           iEvent;
    int           iCtr;

    /*
    **  Set the session ptr, if applicable
    */
    if(iInspectMode == HI_SI_CLIENT_MODE)
    {
        GenEvents.stack =       hi_ssn->client.event_list.stack;
        GenEvents.stack_count = &(hi_ssn->client.event_list.stack_count);
        GenEvents.events =      hi_ssn->client.event_list.events;

        iGenerator = GENERATOR_SPP_HTTP_INSPECT_CLIENT;
    }
    else if(iInspectMode == HI_SI_SERVER_MODE)
    {
        GenEvents.stack =       hi_ssn->server.event_list.stack;
        GenEvents.stack_count = &(hi_ssn->server.event_list.stack_count);
        GenEvents.events =      hi_ssn->server.event_list.events;

        iGenerator = GENERATOR_SPP_HTTP_INSPECT;
    }
    else
    {
        GenEvents.stack =       hi_ssn->anom_server.event_list.stack;
        GenEvents.stack_count = &(hi_ssn->anom_server.event_list.stack_count);
        GenEvents.events =      hi_ssn->anom_server.event_list.events;

        iGenerator = GENERATOR_SPP_HTTP_INSPECT;
    }

    /*
    **  Now starts the generic event processing
    */
    iStackCnt = *(GenEvents.stack_count);

    /*
    **  IMPORTANT::
    **  We have to check the stack count of the event queue before we process
    **  an log.
    */
    if(iStackCnt == 0)
    {
        return 0;
    }

    /*
    **  Cycle through the events and select the event with the highest
    **  priority.
    */
    for(iCtr = 0; iCtr < iStackCnt; iCtr++)
    {
        iEvent = GenEvents.stack[iCtr];
        OrigEvent = &(GenEvents.events[iEvent]);

        /*
        **  Set the event to start off the comparison
        */
        if(!HiEvent)
        {
            HiEvent = OrigEvent;
        }

        /*
        **  This is our "comparison function".  Log the event with the highest
        **  priority.
        */
        if(OrigEvent->event_info->priority < HiEvent->event_info->priority)
        {
            HiEvent = OrigEvent;
        }

        /*
        **  IMPORTANT:
        **    This is how we reset the events in the event queue.
        **    If you miss this step, you can be really screwed.
        */
        OrigEvent->count = 0;
    }

    /*
    **  We use the iEvent+1 because the event IDs between snort and
    **  HttpInspect are mapped off-by-one.  Don't ask why, drink Bud
    **  Dry . . . They're mapped off-by one because in the internal
    **  HttpInspect queue, events are mapped starting at 0.  For some
    **  reason, it appears that the first event can't be zero, so we
    **  use the internal value and add one for snort.
    */
    iEvent = HiEvent->event_info->alert_id + 1;

    uiMask = (uint32_t)(1 << (iEvent & 31));

    if (hsd != NULL)
    {
        /* We've already logged this event for this session,
         * don't log it again */
        if (hsd->event_flags & uiMask)
            return 0;
        hsd->event_flags |= uiMask;
    }

    SnortEventqAdd(iGenerator, iEvent, 1, 0, 3, HiEvent->event_info->alert_str,0);

    /*
    **  Reset the event queue stack counter, in the case of pipelined
    **  requests.
    */
    *(GenEvents.stack_count) = 0;

    return 0;
}

static inline int SetSiInput(HI_SI_INPUT *SiInput, Packet *p)
{
    IP_COPY_VALUE(SiInput->sip, GET_SRC_IP(p));
    IP_COPY_VALUE(SiInput->dip, GET_DST_IP(p));
    SiInput->sport = p->sp;
    SiInput->dport = p->dp;

    /*
    **  We now set the packet direction
    */
    if(p->ssnptr &&
        stream_api->get_session_flags(p->ssnptr) & SSNFLAG_MIDSTREAM)
    {
        SiInput->pdir = HI_SI_NO_MODE;
    }
    else if(p->packet_flags & PKT_FROM_SERVER)
    {
        SiInput->pdir = HI_SI_SERVER_MODE;
    }
    else if(p->packet_flags & PKT_FROM_CLIENT)
    {
        SiInput->pdir = HI_SI_CLIENT_MODE;
    }
    else
    {
        SiInput->pdir = HI_SI_NO_MODE;
    }

    return HI_SUCCESS;

}

static inline void ApplyClientFlowDepth (Packet* p, int flow_depth)
{
    switch (flow_depth)
    {
    case -1:
        // Inspect none of the client if there is normalized/extracted
        // URI/Method/Header/Body data */
        SetDetectLimit(p, 0);
        break;

    case 0:
        // Inspect all of the client, even if there is normalized/extracted
        // URI/Method/Header/Body data */
        /* XXX: HUGE performance hit here */
        SetDetectLimit(p, p->dsize);
        break;

    default:
        // Limit inspection of the client, even if there is normalized/extracted
        // URI/Method/Header/Body data */
        /* XXX: Potential performance hit here */
        if (flow_depth < p->dsize)
        {
            SetDetectLimit(p, flow_depth);
        }
        else
        {
            SetDetectLimit(p, p->dsize);
        }
        break;
    }
}

static inline FilePosition getFilePoistion(Packet *p)
{
    FilePosition position = SNORT_FILE_POSITION_UNKNOWN;

    if(ScPafEnabled())
    {
        if (PacketHasFullPDU(p))
            position = SNORT_FILE_FULL;
        else if (PacketHasStartOfPDU(p))
            position = SNORT_FILE_START;
        else if (p->packet_flags & PKT_PDU_TAIL)
            position = SNORT_FILE_END;
        else if (file_api->get_file_processed_size(p->ssnptr))
            position = SNORT_FILE_MIDDLE;
    }

    return position;
}

// FIXTHIS extra data masks should only be updated as extra data changes state
// eg just once when captured; this function is called on every packet and 
// repeatedly sets the flags on session
static inline void HttpLogFuncs(HTTPINSPECT_GLOBAL_CONF *GlobalConf, HttpSessionData *hsd, Packet *p, int iCallDetect )
{
    if(!hsd)
        return;

    /* for pipelined HTTP requests */
    if ( !iCallDetect )
        stream_api->clear_extra_data(p->ssnptr, p, 0);

    if(hsd->true_ip)
    {
        if(!(p->packet_flags & PKT_STREAM_INSERT) && !(p->packet_flags & PKT_REBUILT_STREAM))
            SetExtraData(p, GlobalConf->xtra_trueip_id);
        else
            stream_api->set_extra_data(p->ssnptr, p, GlobalConf->xtra_trueip_id);
    }

    if(hsd->log_flags & HTTP_LOG_URI)
    {
        stream_api->set_extra_data(p->ssnptr, p, GlobalConf->xtra_uri_id);
    }

    if(hsd->log_flags & HTTP_LOG_HOSTNAME)
    {
        stream_api->set_extra_data(p->ssnptr, p, GlobalConf->xtra_hname_id);
    }

#ifndef SOURCEFIRE
    if(hsd->log_flags & HTTP_LOG_JSNORM_DATA)
    {
        SetExtraData(p, GlobalConf->xtra_jsnorm_id);
    }
#ifdef ZLIB
    if(hsd->log_flags & HTTP_LOG_GZIP_DATA)
    {
        SetExtraData(p, GlobalConf->xtra_gzip_id);
    }
#endif
#endif
}

static inline void setFileName(Packet *p)
{
    uint8_t *buf = NULL;
    uint32_t len = 0;
    uint32_t type = 0;
    GetHttpUriData(p->ssnptr, &buf, &len, &type);
    file_api->set_file_name (p->ssnptr, buf, len);
}

static inline void processFileData(Packet *p, HttpSessionData *hsd, bool *fileProcessed)
{
    if (*fileProcessed)
        return;

    if (hsd->mime_ssn)
    {
        uint8_t *end = ( uint8_t *)(p->data) + p->dsize;
        file_api->process_mime_data(p, p->data, end, end, end, hsd->mime_ssn, 1);
        *fileProcessed = true;
    }
    else if (file_api->get_file_processed_size(p->ssnptr) >0)
    {
        file_api->file_process(p, (uint8_t *)p->data, p->dsize, getFilePoistion(p), true, false);
        *fileProcessed = true;
    }
}

/*
**  NAME
**    SnortHttpInspect::
*/
/**
**  This function calls the HttpInspect function that processes an HTTP
**  session.
**
**  We need to instantiate a pointer for the HI_SESSION that HttpInspect
**  fills in.  Right now stateless processing fills in this session, which
**  we then normalize, and eventually detect.  We'll have to handle
**  separately the normalization events, etc.
**
**  This function is where we can see from the highest level what the
**  HttpInspect flow looks like.
**
**  @param GlobalConf pointer to the global configuration
**  @param p          pointer to the Packet structure
**
**  @return integer
**
**  @retval  0 function successful
**  @retval <0 fatal error
**  @retval >0 non-fatal error
*/
#define HTTP_BUF_URI_FLAG           0x01
#define HTTP_BUF_HEADER_FLAG        0x02
#define HTTP_BUF_CLIENT_BODY_FLAG   0x04
#define HTTP_BUF_METHOD_FLAG        0x08
#define HTTP_BUF_COOKIE_FLAG        0x10
#define HTTP_BUF_STAT_CODE          0x20
#define HTTP_BUF_STAT_MSG           0x40
int SnortHttpInspect(HTTPINSPECT_GLOBAL_CONF *GlobalConf, Packet *p)
{
    HI_SESSION  *Session;
    HI_SI_INPUT SiInput;
    int iInspectMode = 0;
    int iRet;
    int iCallDetect = 1;
    HttpSessionData *hsd = NULL;
    bool fileProcessed = false;

    PROFILE_VARS;

    hi_stats.total++;

    /*
    **  Set up the HI_SI_INPUT pointer.  This is what the session_inspection()
    **  routines use to determine client and server traffic.  Plus, this makes
    **  the HttpInspect library very independent from snort.
    */
    SetSiInput(&SiInput, p);

    /*
    **  HTTPINSPECT PACKET FLOW::
    **
    **  Session Inspection Module::
    **    The Session Inspection Module retrieves the appropriate server
    **    configuration for sessions, and takes care of the stateless
    **    vs. stateful processing in order to do this.  Once this module
    **    does it's magic, we're ready for the primetime.
    **
    **  HTTP Inspection Module::
    **    This isn't really a module in HttpInspect, but more of a helper
    **    function that sends the data to the appropriate inspection
    **    routine (client, server, anomalous server detection).
    **
    **  HTTP Normalization Module::
    **    This is where we normalize the data from the HTTP Inspection
    **    Module.  The Normalization module handles what type of normalization
    **    to do (client, server).
    **
    **  HTTP Detection Module::
    **    This isn't being used in the first iteration of HttpInspect, but
    **    all the HTTP detection components of signatures will be.
    **
    **  HTTP Event Output Module::
    **    The Event Ouput Module handles any events that have been logged
    **    in the inspection, normalization, or detection phases.
    */

    /*
    **  Session Inspection Module::
    */
    iRet = hi_si_session_inspection(GlobalConf, &Session, &SiInput, &iInspectMode, p);
    if (iRet)
        return iRet;

    /* If no mode then we just look for anomalous servers if configured
     * to do so and get out of here */
    if (iInspectMode == HI_SI_NO_MODE)
    {
        /* Let's look for rogue HTTP servers and stuff */
        if (GlobalConf->anomalous_servers && (p->dsize > 5))
        {
            iRet = hi_server_anomaly_detection(Session, p->data, p->dsize);
            if (iRet)
                return iRet;

            /*
             **  We log events before doing detection because every non-HTTP
             **  packet is possible an anomalous server.  So we still want to
             **  go through the regular detection engine, and just log any
             **  alerts here before returning.
             **
             **  Return normally if this isn't either HTTP client or server
             **  traffic.
             */
            if (Session->anom_server.event_list.stack_count)
                LogEvents(Session, p, iInspectMode, NULL);
        }

        return 0;
    }

    hsd = GetHttpSessionData(p);

    if ( ScPafEnabled() &&
        (p->packet_flags & PKT_STREAM_INSERT) &&
        !PacketHasFullPDU(p) )
    {
        int flow_depth;

        if ( iInspectMode == HI_SI_CLIENT_MODE )
        {
            flow_depth = Session->server_conf->client_flow_depth;
            ApplyClientFlowDepth(p, flow_depth);
        }
        else
        {
            ApplyFlowDepth(Session->server_conf, p, hsd, 0, 0, GET_PKT_SEQ(p));
        }

        p->packet_flags |= PKT_HTTP_DECODE;

        if ( p->alt_dsize == 0 )
        {
            DisableDetect(p);
            SetAllPreprocBits(p);
            return 0;
        }
        // see comments on call to Detect() below
        PREPROC_PROFILE_START(hiDetectPerfStats);
        Detect(p);
#ifdef PERF_PROFILING
        hiDetectCalled = 1;
#endif
        PREPROC_PROFILE_END(hiDetectPerfStats);
        return 0;
    }

    if (hsd == NULL)
        hsd = SetNewHttpSessionData(p, (void *)Session);
    else
    {
        /* Gzip data should not be logged with all the packets of the session.*/
        hsd->log_flags &= ~HTTP_LOG_GZIP_DATA;
        hsd->log_flags &= ~HTTP_LOG_JSNORM_DATA;
    }

    /*
    **  HTTP Inspection Module::
    **
    **  This is where we do the client/server inspection and find the
    **  various HTTP protocol fields.  We then normalize these fields and
    **  call the detection engine.
    **
    **  The reason for the loop is for pipelined requests.  Doing pipelined
    **  requests in this way doesn't require any memory or tracking overhead.
    **  Instead, we just process each request linearly.
    */
    do
    {
        /*
        **  INIT:
        **  We set this equal to zero (again) because of the pipelining
        **  requests.  We don't want to bail before we get to setting the
        **  URI, so we make sure here that this can't happen.
        */
        SetHttpDecode(0);
        ClearHttpBuffers();

        iRet = hi_mi_mode_inspection(Session, iInspectMode, p, hsd);
        if (iRet)
        {
            if (hsd)
            {
                processFileData(p, hsd, &fileProcessed);
            }
            LogEvents(Session, p, iInspectMode, hsd);
            return iRet;
        }

        iRet = hi_normalization(Session, iInspectMode, hsd);
        if (iRet)
        {
            LogEvents(Session, p, iInspectMode, hsd);
            return iRet;
        }

        HttpLogFuncs(GlobalConf, hsd, p, iCallDetect);

        /*
        **  Let's setup the pointers for the detection engine, and
        **  then go for it.
        */
        if ( iInspectMode == HI_SI_CLIENT_MODE )
        {
            const HttpBuffer* hb;
            ClearHttpBuffers();  // FIXTHIS needed here and right above??

            if ( Session->client.request.uri_norm )
            {
                SetHttpBufferEncoding(
                    HTTP_BUFFER_URI, 
                    Session->client.request.uri_norm,
                    Session->client.request.uri_norm_size,
                    Session->client.request.uri_encode_type);

                SetHttpBuffer(
                    HTTP_BUFFER_RAW_URI,
                    Session->client.request.uri,
                    Session->client.request.uri_size);

                p->packet_flags |= PKT_HTTP_DECODE;
            }
            else if ( Session->client.request.uri )
            {
                SetHttpBufferEncoding(
                    HTTP_BUFFER_URI,
                    Session->client.request.uri,
                    Session->client.request.uri_size,
                    Session->client.request.uri_encode_type);

                SetHttpBuffer(
                    HTTP_BUFFER_RAW_URI,
                    Session->client.request.uri,
                    Session->client.request.uri_size);

                p->packet_flags |= PKT_HTTP_DECODE;
            }

            if ( Session->client.request.header_norm || 
                 Session->client.request.header_raw )
            {
                if ( Session->client.request.header_norm )
                {
                    SetHttpBufferEncoding(
                        HTTP_BUFFER_HEADER,
                        Session->client.request.header_norm,
                        Session->client.request.header_norm_size,
                        Session->client.request.header_encode_type);

                    SetHttpBuffer(
                        HTTP_BUFFER_RAW_HEADER,
                        Session->client.request.header_raw,
                        Session->client.request.header_raw_size);

                    p->packet_flags |= PKT_HTTP_DECODE;
#ifdef DEBUG
                    hi_stats.req_header_len += Session->client.request.header_norm_size;
#endif
                }
                else
                {
                    SetHttpBufferEncoding(
                        HTTP_BUFFER_HEADER,
                        Session->client.request.header_raw,
                        Session->client.request.header_raw_size,
                        Session->client.request.header_encode_type);

                    SetHttpBuffer(
                        HTTP_BUFFER_RAW_HEADER,
                        Session->client.request.header_raw,
                        Session->client.request.header_raw_size);

                    p->packet_flags |= PKT_HTTP_DECODE;
                }
            }

            if(Session->client.request.method & (HI_POST_METHOD | HI_GET_METHOD))
            {
                if(Session->client.request.post_raw)
                {
                    uint8_t *start = (uint8_t *)(Session->client.request.content_type);

                    if ( hsd && start )
                    {
                        /* mime parsing
                         * mime boundary should be processed before this
                         */
                        uint8_t *end;

                        if (!hsd->mime_ssn)
                        {
                            hsd->mime_ssn = (MimeState *)SnortAlloc(sizeof(MimeState));
                            if (!hsd->mime_ssn)
                                return 0;
                            hsd->mime_ssn->log_config = &(GlobalConf->mime_conf);
                            hsd->mime_ssn->decode_conf = &(GlobalConf->decode_conf);
                            hsd->mime_ssn->mime_mempool = mime_decode_mempool;
                            hsd->mime_ssn->log_mempool = mime_log_mempool;
                            /*Set log buffers per session*/
                            if (file_api->set_log_buffers(&(hsd->mime_ssn->log_state),
                                    hsd->mime_ssn->log_config, hsd->mime_ssn->log_mempool) < 0)
                            {
                                return 0;
                            }
                        }

                        end = (uint8_t *)(Session->client.request.post_raw + Session->client.request.post_raw_size);
                        file_api->process_mime_data(p, start, end, end, end, hsd->mime_ssn, 1);
                    }
                    else
                    {
                        if (file_api->file_process(p,(uint8_t *)Session->client.request.post_raw,
                                (uint16_t)Session->client.request.post_raw_size,
                                getFilePoistion(p), true, false))
                        {
                            setFileName(p);
                        }
                    }

                    if(Session->server_conf->post_depth > -1)
                    {
                        if(Session->server_conf->post_depth &&
                                ((int)Session->client.request.post_raw_size > Session->server_conf->post_depth))
                        {
                            Session->client.request.post_raw_size = Session->server_conf->post_depth;
                        }
                        SetHttpBufferEncoding(
                            HTTP_BUFFER_CLIENT_BODY,
                            Session->client.request.post_raw,
                            Session->client.request.post_raw_size,
                            Session->client.request.post_encode_type);

                        p->packet_flags |= PKT_HTTP_DECODE;
                    }

                }
            }
            else if (hsd)
            {
                processFileData(p, hsd, &fileProcessed);
            }

            if ( Session->client.request.method_raw )
            {
                SetHttpBuffer(
                    HTTP_BUFFER_METHOD,
                    Session->client.request.method_raw,
                    Session->client.request.method_size);

                p->packet_flags |= PKT_HTTP_DECODE;
            }

            if ( Session->client.request.cookie_norm || 
                 Session->client.request.cookie.cookie )
            {
                if ( Session->client.request.cookie_norm )
                {
                    SetHttpBufferEncoding(
                        HTTP_BUFFER_COOKIE,
                        Session->client.request.cookie_norm,
                        Session->client.request.cookie_norm_size,
                        Session->client.request.cookie_encode_type);

                    SetHttpBuffer(
                        HTTP_BUFFER_RAW_COOKIE,
                        Session->client.request.cookie.cookie,
                        Session->client.request.cookie.cookie_end - 
                            Session->client.request.cookie.cookie);

                    p->packet_flags |= PKT_HTTP_DECODE;
                }
                else
                {
                    SetHttpBufferEncoding(
                        HTTP_BUFFER_COOKIE,
                        Session->client.request.cookie.cookie,
                        Session->client.request.cookie.cookie_end - 
                            Session->client.request.cookie.cookie,
                        Session->client.request.cookie_encode_type);

                    SetHttpBuffer(
                        HTTP_BUFFER_RAW_COOKIE,
                        Session->client.request.cookie.cookie,
                        Session->client.request.cookie.cookie_end - 
                            Session->client.request.cookie.cookie);

                    p->packet_flags |= PKT_HTTP_DECODE;
                }
            }
            else if ( !Session->server_conf->enable_cookie && 
                (hb = GetHttpBuffer(HTTP_BUFFER_HEADER)) )
            {
                SetHttpBufferEncoding(
                    HTTP_BUFFER_COOKIE, hb->buf, hb->length, hb->encode_type);

                hb = GetHttpBuffer(HTTP_BUFFER_RAW_HEADER);
                assert(hb);

                SetHttpBuffer(HTTP_BUFFER_RAW_COOKIE, hb->buf, hb->length);

                p->packet_flags |= PKT_HTTP_DECODE;
            }

            if ( IsLimitedDetect(p) )
            {
                ApplyClientFlowDepth(p, Session->server_conf->client_flow_depth);

                if( !GetHttpBufferMask() && (p->alt_dsize == 0)  )
                {
                    DisableDetect(p);
                    SetAllPreprocBits(p);
                    return 0;
                }
            }

        }
        else   /* Server mode */
        {
            const HttpBuffer* hb;

            /*
            **  We check here to see whether this was a server response
            **  header or not.  If the header size is 0 then, we know that this
            **  is not the header and don't do any detection.
            */
            if( !(Session->server_conf->inspect_response) &&
                IsLimitedDetect(p) && !p->alt_dsize )
            {
                DisableDetect(p);

                SetAllPreprocBits(p);
                if(Session->server_conf->server_flow_depth == -1)
                {
                    SetPreprocBit(p, PP_DCE2);
                }
                return 0;
            }
            ClearHttpBuffers();

             if ( Session->server.response.header_norm || 
                  Session->server.response.header_raw )
             {
                 if ( Session->server.response.header_norm )
                 {
                     SetHttpBufferEncoding(
                         HTTP_BUFFER_HEADER,
                         Session->server.response.header_norm,
                         Session->server.response.header_norm_size,
                         Session->server.response.header_encode_type);

                     SetHttpBuffer(
                         HTTP_BUFFER_RAW_HEADER,
                         Session->server.response.header_raw,
                         Session->server.response.header_raw_size);
                 }
                 else
                 {
                     SetHttpBuffer(
                         HTTP_BUFFER_HEADER,
                         Session->server.response.header_raw,
                         Session->server.response.header_raw_size);

                     SetHttpBuffer(
                         HTTP_BUFFER_RAW_HEADER,
                         Session->server.response.header_raw,
                         Session->server.response.header_raw_size);
                 }
             }

             if ( Session->server.response.cookie_norm || 
                  Session->server.response.cookie.cookie )
             {
                 if(Session->server.response.cookie_norm )
                 {
                     SetHttpBufferEncoding(
                         HTTP_BUFFER_COOKIE,
                         Session->server.response.cookie_norm,
                         Session->server.response.cookie_norm_size,
                         Session->server.response.cookie_encode_type);

                     SetHttpBuffer(
                         HTTP_BUFFER_RAW_COOKIE,
                         Session->server.response.cookie.cookie,
                         Session->server.response.cookie.cookie_end -
                             Session->server.response.cookie.cookie);
                 }
                 else
                 {
                     SetHttpBuffer(
                         HTTP_BUFFER_COOKIE,
                         Session->server.response.cookie.cookie,
                         Session->server.response.cookie.cookie_end - 
                             Session->server.response.cookie.cookie);

                     SetHttpBuffer(
                         HTTP_BUFFER_RAW_COOKIE,
                         Session->server.response.cookie.cookie,
                         Session->server.response.cookie.cookie_end -
                             Session->server.response.cookie.cookie);
                 }
             }
             else if ( !Session->server_conf->enable_cookie && 
                 (hb = GetHttpBuffer(HTTP_BUFFER_HEADER)) )
             {
                 SetHttpBufferEncoding(
                     HTTP_BUFFER_COOKIE, hb->buf, hb->length, hb->encode_type);

                 hb = GetHttpBuffer(HTTP_BUFFER_RAW_HEADER);
                 assert(hb);

                 SetHttpBuffer(HTTP_BUFFER_RAW_COOKIE, hb->buf, hb->length);
             }

             if(Session->server.response.status_code)
             {
                 SetHttpBuffer(
                     HTTP_BUFFER_STAT_CODE,
                     Session->server.response.status_code,
                     Session->server.response.status_code_size);
             }

             if(Session->server.response.status_msg)
             {
                 SetHttpBuffer(
                     HTTP_BUFFER_STAT_MSG,
                     Session->server.response.status_msg,
                     Session->server.response.status_msg_size);
             }

             if(Session->server.response.body_size > 0)
             {
                 int detect_data_size = (int)Session->server.response.body_size;

                 /*body_size is included in the data_extracted*/
                 if((Session->server_conf->server_flow_depth > 0) &&
                         (hsd->resp_state.data_extracted  < (Session->server_conf->server_flow_depth + (int)Session->server.response.body_size)))
                 {
                     /*flow_depth is smaller than data_extracted, need to subtract*/
                     if(Session->server_conf->server_flow_depth < hsd->resp_state.data_extracted)
                         detect_data_size -= hsd->resp_state.data_extracted - Session->server_conf->server_flow_depth;
                 }
                 else if (Session->server_conf->server_flow_depth)
                 {
                     detect_data_size = 0;
                 }

                 setFileDataPtr((uint8_t *)Session->server.response.body, (uint16_t)detect_data_size);

                 if (ScPafEnabled() && PacketHasPAFPayload(p)
                         && file_api->file_process(p,(uint8_t *)Session->server.response.body, (uint16_t)Session->server.response.body_size,
                         getFilePoistion(p), false, false))
                 {
                     setFileName(p);
                 }
             }

             if( IsLimitedDetect(p) &&
                 !GetHttpBufferMask() && (p->alt_dsize == 0)  )
             {
                 DisableDetect(p);

                 SetAllPreprocBits(p);
                 if(Session->server_conf->server_flow_depth == -1)
                 {
                     SetPreprocBit(p, PP_DCE2);
                 }
                 return 0;

             }
        }

        /*
        **  If we get here we either had a client or server request/response.
        **  We do the detection here, because we're starting a new paradigm
        **  about protocol decoders.
        **
        **  Protocol decoders are now their own detection engine, since we are
        **  going to be moving protocol field detection from the generic
        **  detection engine into the protocol module.  This idea scales much
        **  better than having all these Packet struct field checks in the
        **  main detection engine for each protocol field.
        */
        PREPROC_PROFILE_START(hiDetectPerfStats);
        Detect(p);
#ifdef PERF_PROFILING
        hiDetectCalled = 1;
#endif
        PREPROC_PROFILE_END(hiDetectPerfStats);

        /*
        **  Handle event stuff after we do detection.
        **
        **  Here's the reason why:
        **    - since snort can only handle one logged event per packet,
        **      we only log HttpInspect events if there wasn't one in the
        **      detection engine.  I say that events generated in the
        **      "advanced generic content matching" engine is more
        **      important than generic events that I can log here.
        */
        LogEvents(Session, p, iInspectMode, hsd);

        /*
        **  We set the global detection flag here so that if request pipelines
        **  fail, we don't do any detection.
        */
        iCallDetect = 0;

    } while(Session->client.request.pipeline_req);

    if ( iCallDetect == 0 )
    {
        /* Detect called at least once from above pkt processing loop. */
        DisableAllDetect(p);

        /* dcerpc2 preprocessor may need to look at this for
         * RPC over HTTP setup */
        SetPreprocBit(p, PP_DCE2);

        /* sensitive_data preprocessor may look for PII over HTTP */
        SetPreprocBit(p, PP_SDF);
    }

    return 0;
}

int HttpInspectInitializeGlobalConfig(HTTPINSPECT_GLOBAL_CONF *config,
                                      char *ErrorString, int iErrStrLen)
{
    int iRet;

    if (config == NULL)
    {
        snprintf(ErrorString, iErrStrLen, "Global configuration is NULL.");
        return -1;
    }

    iRet = hi_ui_config_init_global_conf(config);
    if (iRet)
    {
        snprintf(ErrorString, iErrStrLen,
                 "Error initializing Global Configuration.");
        return -1;
    }

    iRet = hi_client_init(config);
    if (iRet)
    {
        snprintf(ErrorString, iErrStrLen,
                 "Error initializing client module.");
        return -1;
    }

    file_api->set_mime_decode_config_defauts(&(config->decode_conf));
    file_api->set_mime_log_config_defauts(&(config->mime_conf));

    return 0;
}

HttpSessionData * SetNewHttpSessionData(Packet *p, void *data)
{
    HttpSessionData *hsd;

    if (p->ssnptr == NULL)
        return NULL;


    hsd = (HttpSessionData *)SnortAlloc(sizeof(HttpSessionData));
    init_decode_utf_state(&hsd->utf_state);

    stream_api->set_application_data(p->ssnptr, PP_HTTPINSPECT, hsd, FreeHttpSessionData);

    return hsd;
}

void FreeHttpSessionData(void *data)
{
    HttpSessionData *hsd = (HttpSessionData *)data;

    if (hsd == NULL)
        return;

#ifdef ZLIB
    if (hsd->decomp_state != NULL)
    {
        inflateEnd(&(hsd->decomp_state->d_stream));
        mempool_free(hi_gzip_mempool, hsd->decomp_state->bkt);
    }
#endif

    if (hsd->log_state != NULL)
    {
        mempool_free(http_mempool, hsd->log_state->log_bucket);
        free(hsd->log_state);
    }

    if(hsd->true_ip)
        sfip_free(hsd->true_ip);

    file_api->free_mime_session(hsd->mime_ssn);

    free(hsd);
}

int GetHttpTrueIP(void *data, uint8_t **buf, uint32_t *len, uint32_t *type)
{
    sfip_t *true_ip = NULL;

    true_ip = GetTrueIPForSession(data);
    if(!true_ip)
        return 0;

    if(true_ip->family == AF_INET6)
    {
        *type = EVENT_INFO_XFF_IPV6;
        *len = sizeof(struct in6_addr); /*ipv6 address size in bytes*/

    }
    else
    {
        *type = EVENT_INFO_XFF_IPV4;
        *len = sizeof(struct in_addr); /*ipv4 address size in bytes*/
    }

    *buf = true_ip->ip8;
    return 1;
}

int IsGzipData(void *data)
{
    HttpSessionData *hsd = NULL;

    if (data == NULL)
        return -1;
    hsd = (HttpSessionData *)stream_api->get_application_data(data, PP_HTTPINSPECT);

    if(hsd == NULL)
        return -1;

    if((hsd->log_flags & HTTP_LOG_GZIP_DATA) && (file_data_ptr.len > 0 ))
        return 0;
    else
        return -1;
}


int GetHttpGzipData(void *data, uint8_t **buf, uint32_t *len, uint32_t *type)
{
    if(!IsGzipData(data))
    {
        *buf = file_data_ptr.data;
        *len = file_data_ptr.len;
        *type = EVENT_INFO_GZIP_DATA;
        return 1;
    }

    return 0;

}

int IsJSNormData(void *data)
{
    HttpSessionData *hsd = NULL;

    if (data == NULL)
        return -1;
    hsd = (HttpSessionData *)stream_api->get_application_data(data, PP_HTTPINSPECT);

    if(hsd == NULL)
        return -1;

    if((hsd->log_flags & HTTP_LOG_JSNORM_DATA) && (file_data_ptr.len > 0 ))
        return 0;
    else
        return -1;

}

int GetHttpJSNormData(void *data, uint8_t **buf, uint32_t *len, uint32_t *type)
{
    if(!IsJSNormData(data))
    {
        *buf = file_data_ptr.data;
        *len = file_data_ptr.len;
        *type = EVENT_INFO_JSNORM_DATA;
        return 1;
    }

    return 0;
}

int GetHttpUriData(void *data, uint8_t **buf, uint32_t *len, uint32_t *type)
{
    HttpSessionData *hsd = NULL;
        
    if (data == NULL)
        return 0;
    hsd = (HttpSessionData *)stream_api->get_application_data(data, PP_HTTPINSPECT);
            
    if(hsd == NULL)
        return 0;

    if(hsd->log_state && hsd->log_state->uri_bytes > 0)
    {
        *buf = hsd->log_state->uri_extracted;
        *len = hsd->log_state->uri_bytes;
        *type = EVENT_INFO_HTTP_URI;
        return 1;
    }

    return 0;
}


int GetHttpHostnameData(void *data, uint8_t **buf, uint32_t *len, uint32_t *type)
{
    HttpSessionData *hsd = NULL;
        
    if (data == NULL)
        return 0;
    hsd = (HttpSessionData *)stream_api->get_application_data(data, PP_HTTPINSPECT);
            
    if(hsd == NULL)
        return 0;

    if(hsd->log_state && hsd->log_state->hostname_bytes > 0)
    {
        *buf = hsd->log_state->hostname_extracted;
        *len = hsd->log_state->hostname_bytes;
        *type = EVENT_INFO_HTTP_HOSTNAME;
        return 1;
    }
        
    return 0;
}

void HI_SearchInit(void)
{
    const HiSearchToken *tmp;
    hi_javascript_search_mpse = search_api->search_instance_new();
    if (hi_javascript_search_mpse == NULL)
    {
        FatalError("%s(%d) Could not allocate memory for HTTP <script> tag search.\n",
                               __FILE__, __LINE__);
    } 
    for (tmp = &hi_patterns[0]; tmp->name != NULL; tmp++)
    {
        hi_js_search[tmp->search_id].name = tmp->name;
        hi_js_search[tmp->search_id].name_len = tmp->name_len;
        search_api->search_instance_add(hi_javascript_search_mpse, tmp->name, tmp->name_len, tmp->search_id);
    }
    search_api->search_instance_prep(hi_javascript_search_mpse);

    hi_htmltype_search_mpse = search_api->search_instance_new();
    if (hi_htmltype_search_mpse == NULL)
    {
        FatalError("%s(%d) Could not allocate memory for HTTP <script> type search.\n",
                                   __FILE__, __LINE__);
    } 
    for (tmp = &html_patterns[0]; tmp->name != NULL; tmp++)
    {
        hi_html_search[tmp->search_id].name = tmp->name;
        hi_html_search[tmp->search_id].name_len = tmp->name_len;
        search_api->search_instance_add(hi_htmltype_search_mpse, tmp->name, tmp->name_len, tmp->search_id);
    }
    search_api->search_instance_prep(hi_htmltype_search_mpse);
}

void HI_SearchFree(void)
{
    if (hi_javascript_search_mpse != NULL)
        search_api->search_instance_free(hi_javascript_search_mpse);

    if (hi_htmltype_search_mpse != NULL)
        search_api->search_instance_free(hi_htmltype_search_mpse);
}

int HI_SearchStrFound(void *id, void *unused, int index, void *data, void *unused2)
{
    int search_id = (int)(uintptr_t)id;

    hi_search_info.id = search_id;
    hi_search_info.index = index;
    hi_search_info.length = hi_current_search[search_id].name_len;

    /* Returning non-zero stops search, which is okay since we only look for one at a time */
    return 1;
}


