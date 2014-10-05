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
**  @file       hi_server.h
**  
**  @author     Daniel Roelker <droelker@sourcefire.com>
**  
**  @brief      Header file for HttpInspect Server Module
**  
**  This file defines the server structure and functions to access server
**  inspection.
**  
**  NOTE:
**      - Initial development.  DJR
*/
#ifndef __HI_SERVER_H__
#define __HI_SERVER_H__

#include "hi_include.h"
#include "hi_util.h"
#include "snort_httpinspect.h"
#include "hi_client.h"

typedef struct s_HI_SERVER_RESP
{
    const u_char *status_code;
    const u_char *status_msg;
    const u_char *header_raw;
    const u_char *header_norm;
    COOKIE_PTR cookie;
    const u_char *cookie_norm;
    const u_char *body;

    u_int body_size;
    u_int status_code_size;
    u_int status_msg_size;
    u_int header_raw_size;
    u_int header_norm_size;
    u_int cookie_norm_size;

    uint16_t header_encode_type;
    uint16_t cookie_encode_type;

} HI_SERVER_RESP;


typedef struct s_HI_SERVER
{
    HI_SERVER_RESP response;
    HI_SERVER_EVENTS event_list;
} HI_SERVER;

int hi_server_inspection(void *, Packet *, HttpSessionData *);

#endif
