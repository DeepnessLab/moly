/*
 * smtp_util.c
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
 * Copyright (C) 2014-2015 Cisco and/or its affiliates. All rights reserved.
 * Copyright (C) 2005-2013 Sourcefire, Inc.
 *
 * Author: Andy  Mullican
 *
 * Description:
 *
 * This file contains SMTP helper functions.
 *
 * Entry point functions:
 *
 *    safe_strchr()
 *    safe_strstr()
 *    copy_to_space()
 *    safe_sscanf()
 *
 *
 */

#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sf_types.h"
#include "snort_debug.h"
#include "snort_bounds.h"

#include "snort_smtp.h"
#include "smtp_util.h"
#include "sf_dynamic_preprocessor.h"
#include "sf_snort_packet.h"

extern SMTP *smtp_ssn;
extern char smtp_normalizing;

void SMTP_GetEOL(const uint8_t *ptr, const uint8_t *end,
                 const uint8_t **eol, const uint8_t **eolm)
{
    const uint8_t *tmp_eol;
    const uint8_t *tmp_eolm;

    /* XXX maybe should fatal error here since none of these
     * pointers should be NULL */
    if (ptr == NULL || end == NULL || eol == NULL || eolm == NULL)
        return;

    tmp_eol = (uint8_t *)memchr(ptr, '\n', end - ptr);
    if (tmp_eol == NULL)
    {
        tmp_eol = end;
        tmp_eolm = end;
    }
    else
    {
        /* end of line marker (eolm) should point to marker and
         * end of line (eol) should point to end of marker */
        if ((tmp_eol > ptr) && (*(tmp_eol - 1) == '\r'))
        {
            tmp_eolm = tmp_eol - 1;
        }
        else
        {
            tmp_eolm = tmp_eol;
        }

        /* move past newline */
        tmp_eol++;
    }

    *eol = tmp_eol;
    *eolm = tmp_eolm;
}

int SMTP_CopyToAltBuffer(SFSnortPacket *p, const uint8_t *start, int length)
{
    uint8_t *alt_buf;
    int alt_size;
    uint16_t *alt_len;
    int ret;

    /* if we make a call to this it means we want to use the alt buffer
     * regardless of whether we copy any data into it or not - barring a failure */
    smtp_normalizing = 1;

    /* if start and end the same, nothing to copy */
    if (length == 0)
        return 0;

    alt_buf = _dpd.altBuffer->data;
    alt_size = sizeof(_dpd.altBuffer->data);
    alt_len = &_dpd.altBuffer->len;

    ret = SafeMemcpy(alt_buf + *alt_len, start, length, alt_buf, alt_buf + alt_size);

    if (ret != SAFEMEM_SUCCESS)
    {
        _dpd.DetectFlag_Disable(SF_FLAG_ALT_DECODE);
        smtp_normalizing = 0;
        return -1;
    }
    *alt_len += length;

    _dpd.SetAltDecode(*alt_len);

    return 0;
}
/* Accumulate EOL seperated headers, one or more at a time */
int SMTP_CopyEmailHdrs(const uint8_t *start, int length, MAIL_LogState *log_state)
{
    int log_avail = 0;
    uint8_t *log_buf;
    uint32_t *hdrs_logged;
    int ret = 0;

    if ((log_state == NULL) || (length <= 0))
        return -1;


    log_avail = (log_state->log_depth - log_state->hdrs_logged);
    hdrs_logged = &(log_state->hdrs_logged);
    log_buf = (uint8_t *)log_state->emailHdrs;

    if(log_avail <= 0)
    {
        return -1;
    }

    if(length > log_avail )
    {
        length = log_avail;
    }

    /* appended by the EOL \r\n */

    ret = SafeMemcpy(log_buf + *hdrs_logged, start, length, log_buf, log_buf+(log_state->log_depth));

    if (ret != SAFEMEM_SUCCESS)
    {
        return -1;
    }

    *hdrs_logged += length;

    return 0;
}

/* Accumulate email addresses from RCPT TO and/or MAIL FROM commands. Email addresses are separated by comma */
int SMTP_CopyEmailID(const uint8_t *start, int length, int command_type, MAIL_LogState *log_state)
{
    uint8_t *alt_buf;
    int alt_size;
    uint16_t *alt_len;
    int ret;
    int log_avail=0;
    const uint8_t *tmp_eol;

    if ((log_state == NULL) || (length <= 0))
        return -1;

    tmp_eol = (uint8_t *)memchr(start, ':', length);
    if(tmp_eol == NULL)
        return -1;

    if((tmp_eol+1) < (start+length))
    {
        length = length - ( (tmp_eol+1) - start );
        start = tmp_eol+1;
    }
    else
        return -1;



    switch (command_type)
    {
        case CMD_MAIL:
            alt_buf = log_state->senders;
            alt_size = MAX_EMAIL;
            alt_len = &(log_state->snds_logged);
            break;

        case CMD_RCPT:
            alt_buf = log_state->recipients;
            alt_size = MAX_EMAIL;
            alt_len = &(log_state->rcpts_logged);
            break;

        default:
            return -1;
    }

    log_avail = alt_size - *alt_len;

    if(log_avail <= 0 || !alt_buf)
        return -1;
    else if(log_avail < length)
        length = log_avail;

    if ( *alt_len > 0 && ((*alt_len + 1) < alt_size))
    {
        alt_buf[*alt_len] = ',';
        *alt_len = *alt_len + 1;
        if(log_avail == length)
            length--;
    }

    ret = SafeMemcpy(alt_buf + *alt_len, start, length, alt_buf, alt_buf + alt_size);

    if (ret != SAFEMEM_SUCCESS)
    {
        if(*alt_len != 0)
            *alt_len = *alt_len - 1;
        return -1;
    }

    *alt_len += length;

    return 0;
}


void SMTP_LogFuncs(SMTPConfig *config, SFSnortPacket *p, MimeState *mime_ssn)
{
    if((mime_ssn->log_flags == 0) || !config)
        return;

    if(mime_ssn->log_flags & FLAG_FILENAME_PRESENT)
    {
        _dpd.streamAPI->set_extra_data(p->stream_session, p, config->xtra_filename_id);
    }

    if(mime_ssn->log_flags & FLAG_MAIL_FROM_PRESENT)
    {
        _dpd.streamAPI->set_extra_data(p->stream_session, p, config->xtra_mfrom_id);
    }

    if(mime_ssn->log_flags & FLAG_RCPT_TO_PRESENT)
    {
        _dpd.streamAPI->set_extra_data(p->stream_session, p, config->xtra_rcptto_id);
    }

    if(mime_ssn->log_flags & FLAG_EMAIL_HDRS_PRESENT)
    {
        _dpd.streamAPI->set_extra_data(p->stream_session, p, config->xtra_ehdrs_id);
    }

}

#ifdef DEBUG_MSGS
char smtp_print_buffer[65537];

const char * SMTP_PrintBuffer(SFSnortPacket *p)
{
    const uint8_t *ptr = NULL;
    int len = 0;
    int iorig, inew;

    if (smtp_normalizing)
    {
        ptr = _dpd.altBuffer->data;
        len = _dpd.altBuffer->len;
    }
    else
    {
        ptr = p->payload;
        len = p->payload_size;
    }

    for (iorig = 0, inew = 0; iorig < len; iorig++, inew++)
    {
        if ((isascii((int)ptr[iorig]) && isprint((int)ptr[iorig])) || (ptr[iorig] == '\n'))
        {
            smtp_print_buffer[inew] = ptr[iorig];
        }
        else if (ptr[iorig] == '\r' &&
                 ((iorig + 1) < len) && (ptr[iorig + 1] == '\n'))
        {
            iorig++;
            smtp_print_buffer[inew] = '\n';
        }
        else if (isspace((int)ptr[iorig]))
        {
            smtp_print_buffer[inew] = ' ';
        }
        else
        {
            smtp_print_buffer[inew] = '.';
        }
    }

    smtp_print_buffer[inew] = '\0';

    return &smtp_print_buffer[0];
}
#endif

