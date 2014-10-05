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

#ifndef _PREPROC_IDS_H
#define _PREPROC_IDS_H

/*
**  Preprocessor Communication Defines
**  ----------------------------------
**  These defines allow preprocessors to be turned
**  on and off for each packet.  Preprocessors can be
**  turned off and on before preprocessing occurs and
**  during preprocessing.
**
**  Currently, the order in which the preprocessors are
**  placed in the snort.conf determine the order of
**  evaluation.  So if one module wants to turn off
**  another module, it must come first in the order.
*/

// currently 32 bits (preprocessors)
// are available.

#define PP_BO                      0
//#define PP_DCERPC                  1
#define PP_DNS                     2
#define PP_FRAG3                   3
#define PP_FTPTELNET               4
#define PP_HTTPINSPECT             5
#define PP_PERFMONITOR             6
#define PP_RPCDECODE               7
#define PP_SHARED_RULES            8
#define PP_SFPORTSCAN              9
#define PP_SMTP                   10
#define PP_SSH                    11
#define PP_SSL                    12
#define PP_STREAM5                13
#define PP_TELNET                 14
#define PP_ARPSPOOF               15
#define PP_DCE2                   16
#define PP_SDF                    17
#define PP_NORMALIZE              18
#define PP_ISAKMP                 19  // used externally
#define PP_SKYPE                  20  // used externally
#define PP_SIP                    21
#define PP_POP                    22
#define PP_IMAP                   23
#define PP_APPLICATION_IDENTIFICATION 24  // used externally
#define PP_RULE_ENGINE            25  // used externally
#define PP_REPUTATION             26
#define PP_GTP                    27
#define PP_MODBUS                 28
#define PP_DNP3                   29
#define PP_FILE                   30
#define PP_FILE_INSPECT           31

#define PP_ALL_ON         0xFFFFFFFF
#define PP_ALL_OFF        0x00000000

#define PRIORITY_FIRST           0x0
#define PRIORITY_NORMALIZE       0x4
#define PRIORITY_NETWORK        0x10
#define PRIORITY_TRANSPORT     0x100
#define PRIORITY_TUNNEL        0x105
#define PRIORITY_SCANNER       0x110
#define PRIORITY_SESSION       0x1FF
#define PRIORITY_APPLICATION   0x200
#define PRIORITY_LAST         0xffff

#endif /* _PREPROC_IDS_H */

