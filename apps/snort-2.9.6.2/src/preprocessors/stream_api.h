/* $Id$ */

/*
 ** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 * ** Copyright (C) 2005-2013 Sourcefire, Inc.
 * ** AUTHOR: Steven Sturges
 * **
 * ** This program is free software; you can redistribute it and/or modify
 * ** it under the terms of the GNU General Public License Version 2 as
 * ** published by the Free Software Foundation.  You may not use, modify or
 * ** distribute this program under any other version of the GNU General
 * ** Public License.
 * **
 * ** This program is distributed in the hope that it will be useful,
 * ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 * ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * ** GNU General Public License for more details.
 * **
 * ** You should have received a copy of the GNU General Public License
 * ** along with this program; if not, write to the Free Software
 * ** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * */

/* stream_api.h
 *
 * Purpose: Definition of the StreamAPI.  To be used as a common interface
 *          for TCP (and later UDP & ICMP) Stream access for other
 *          preprocessors and detection plugins.
 *
 * Arguments:
 *
 * Effect:
 *
 * Comments:
 *
 * Any comments?
 *
 */

#ifndef STREAM_API_H_
#define STREAM_API_H_

#include <sys/types.h>

#include "ipv6_port.h"
#include "preprocids.h" /* IDs are used when setting preproc specific data */
#include "bitop.h"
#include "decode.h"
#include "sfPolicy.h"

#define EXPECT_FLAG_ALWAYS 0x01

#define SSN_MISSING_NONE   0x00
#define SSN_MISSING_BEFORE 0x01

#define SSN_DIR_NONE 0x0
#define SSN_DIR_FROM_CLIENT 0x1
#define SSN_DIR_FROM_SENDER 0x1
#define SSN_DIR_FROM_SERVER 0x2
#define SSN_DIR_FROM_RESPONDER 0x2
#define SSN_DIR_BOTH 0x03

#define SSNFLAG_SEEN_CLIENT         0x00000001
#define SSNFLAG_SEEN_SENDER         0x00000001
#define SSNFLAG_SEEN_SERVER         0x00000002
#define SSNFLAG_SEEN_RESPONDER      0x00000002
#define SSNFLAG_ESTABLISHED         0x00000004
#define SSNFLAG_NMAP                0x00000008
#define SSNFLAG_ECN_CLIENT_QUERY    0x00000010
#define SSNFLAG_ECN_SERVER_REPLY    0x00000020
#define SSNFLAG_HTTP_1_1            0x00000040 /* has stream seen HTTP 1.1? */
#define SSNFLAG_SEEN_PMATCH         0x00000080 /* seen pattern match? */
#define SSNFLAG_MIDSTREAM           0x00000100 /* picked up midstream */
#define SSNFLAG_CLIENT_FIN          0x00000200 /* server sent fin */
#define SSNFLAG_SERVER_FIN          0x00000400 /* client sent fin */
#define SSNFLAG_CLIENT_PKT          0x00000800 /* packet is from the client */
#define SSNFLAG_SERVER_PKT          0x00001000 /* packet is from the server */
#define SSNFLAG_COUNTED_INITIALIZE  0x00002000
#define SSNFLAG_COUNTED_ESTABLISH   0x00004000
#define SSNFLAG_COUNTED_CLOSING     0x00008000
#define SSNFLAG_TIMEDOUT            0x00010000
#define SSNFLAG_PRUNED              0x00020000
#define SSNFLAG_RESET               0x00040000
#define SSNFLAG_DROP_CLIENT         0x00080000
#define SSNFLAG_DROP_SERVER         0x00100000
#define SSNFLAG_LOGGED_QUEUE_FULL   0x00200000
#define SSNFLAG_STREAM_ORDER_BAD    0x00400000
#define SSNFLAG_FORCE_BLOCK         0x00800000
#define SSNFLAG_CLIENT_SWAP         0x01000000
#define SSNFLAG_CLIENT_SWAPPED      0x02000000
#define SSNFLAG_ALL                 0xFFFFFFFF /* all that and a bag of chips */
#define SSNFLAG_NONE                0x00000000 /* nothing, an MT bag of chips */

typedef enum {
    STREAM_FLPOLICY_NONE,
    STREAM_FLPOLICY_FOOTPRINT,       /* size-based footprint flush */
    STREAM_FLPOLICY_LOGICAL,         /* queued bytes-based flush */
    STREAM_FLPOLICY_RESPONSE,        /* flush when we see response */
    STREAM_FLPOLICY_SLIDING_WINDOW,  /* flush on sliding window */
#if 0
    STREAM_FLPOLICY_CONSUMED,        /* purge consumed bytes */
#endif
    STREAM_FLPOLICY_IGNORE,          /* ignore this traffic */
    STREAM_FLPOLICY_PROTOCOL,        /* protocol aware flushing (PAF) */
#ifdef NORMALIZER
    STREAM_FLPOLICY_FOOTPRINT_IPS,   /* protocol agnostic ips */
    STREAM_FLPOLICY_PROTOCOL_IPS,    /* protocol aware ips */
#endif
    STREAM_FLPOLICY_MAX
} FlushPolicy;

#define STREAM_FLPOLICY_SET_ABSOLUTE    0x01
#define STREAM_FLPOLICY_SET_APPEND      0x02

#define UNKNOWN_PORT 0

#define STREAM_API_VERSION5 5

typedef struct _StreamSessionKey
{
/* XXX If this data structure changes size, HashKeyCmp must be updated! */
    uint32_t   ip_l[4]; /* Low IP */
    uint32_t   ip_h[4]; /* High IP */
    uint16_t   port_l; /* Low Port - 0 if ICMP */
    uint16_t   port_h; /* High Port - 0 if ICMP */
    uint16_t   vlan_tag;
    uint8_t    protocol;
    char       pad;
    uint32_t   mplsLabel; /* MPLS label */
    uint16_t   addressSpaceId;
    uint16_t   addressSpaceIdPad1;
/* XXX If this data structure changes size, HashKeyCmp must be updated! */
} StreamSessionKey;

typedef void (*LogExtraData)(void *ssnptr, void *config, LogFunction *funcs, uint32_t max_count, uint32_t xtradata_mask, uint32_t id, uint32_t sec);
typedef void (*StreamAppDataFree)(void *);
typedef int (*PacketIterator)
    (
     DAQ_PktHdr_t *,
     uint8_t *,  /* pkt pointer */
     void *      /* user-defined data pointer */
    );

typedef int (*StreamSegmentIterator)
    (
     DAQ_PktHdr_t *,
     uint8_t *,  /* pkt pointer */
     uint8_t *,  /* payload pointer */
     uint32_t,   /* sequence number */
     void *      /* user-defined data pointer */
    );

typedef struct _StreamFlowData
{
    BITOP boFlowbits;
    unsigned char flowb[1];
} StreamFlowData;

// for protocol aware flushing (PAF):
typedef enum {
    PAF_ABORT,   // non-paf operation
    PAF_START,   // internal use only
    PAF_SEARCH,  // searching for next flush point
    PAF_FLUSH,   // flush at given offset
    PAF_SKIP     // skip ahead to given offset
} PAF_Status;

typedef PAF_Status (*PAF_Callback)(  // return your scan state
    void* session,         // session pointer
    void** user,           // arbitrary user data hook
    const uint8_t* data,   // in order segment data as it arrives
    uint32_t len,          // length of data
    uint32_t flags,        // packet flags indicating direction of data
    uint32_t* fp           // flush point (offset) relative to data
);

typedef struct _StreamSessionLimits
{
    uint32_t tcp_session_limit;
    uint32_t udp_session_limit;
    uint32_t icmp_session_limit;
    uint32_t ip_session_limit;
} StreamSessionLimits;

typedef enum {
    SE_REXMIT,
    SE_EOF,
    SE_MAX
} Stream_Event;

typedef void (*Stream_Callback)(Packet *);

#ifdef ENABLE_HA
typedef uint32_t (*StreamHAProducerFunc)(void *ssnptr, uint8_t *buf);
typedef int (*StreamHAConsumerFunc)(void *ssnptr, const uint8_t *data, uint8_t length);
#endif

typedef struct _stream_api
{
    int version;

    /*
     * Drop on Inline Alerts for Midstream pickups
     *
     * Parameters
     *,
     * Returns
     *     0 if not alerting
     *     !0 if alerting
     */
    int (*alert_inline_midstream_drops)(void);

    /* Set direction of session
     *
     * Parameters:
     *     Session Ptr
     *     New Direction
     *     IP
     *     Port
     */
    void (*update_direction)(void *, char, snort_ip_p, uint16_t );

    /* Get direction of packet
     *
     * Parameters:
     *     Packet
     */
    uint32_t (*get_packet_direction)(Packet *);

    /* Stop inspection for session, up to count bytes (-1 to ignore
     * for life or until resume).
     *
     * If response flag is set, automatically resume inspection up to
     * count bytes when a data packet in the other direction is seen.
     *
     * Also marks the packet to be ignored
     *
     * Parameters
     *     Session Ptr
     *     Packet
     *     Direction
     *     Bytes
     *     Response Flag
     */
    void (*stop_inspection)(void *, Packet *, char, int32_t, int);

    /* Turn off inspection for potential session.
     * Adds session identifiers to a hash table.
     * TCP only.
     *
     * Parameters
     *     IP addr #1
     *     Port #1
     *     IP addr #2
     *     Port #2
     *     Protocol
     *     Current time (from packet)
     *     Preprocessor ID
     *     Direction
     *     Flags (permanent)
     *
     * Returns
     *     0 on success
     *     -1 on failure
     */
    int (*ignore_session)(snort_ip_p, uint16_t, snort_ip_p, uint16_t,
                           uint8_t, time_t, uint32_t, char, char);

    /* Get direction that data is being ignored.
     *
     * Parameters
     *     Session Ptr
     */
    int (*get_ignore_direction)(void *);

    /* Resume inspection for session.
     *
     * Parameters
     *     Session Ptr
     *     Direction
     */
    void (*resume_inspection)(void *, char);

    /* Drop traffic arriving on session.
     *
     * Parameters
     *     Session Ptr
     *     Direction
     */
    void (*drop_traffic)(Packet *, void *, char);

    /* Drop retransmitted packet arriving on session.
     *
     * Parameters
     *     Packet
     */
    void (*drop_packet)(Packet *);

    /* Set a reference to application data for a session
     *
     * Parameters
     *     Session Ptr
     *     Application Protocol
     *     Application Data reference (pointer)
     *     Application Data free function
     *
     * Returns
     *     0 on success
     *     -1 on failure
     */
    int (*set_application_data)(void *, uint32_t, void *, StreamAppDataFree);

    /* Set a reference to application data for a session
     *
     * Parameters
     *     Session Ptr
     *     Application Protocol
     *
     * Returns
     *     Application Data reference (pointer)
     */
    void *(*get_application_data)(void *, uint32_t);

    /* Sets the flags for a session
     * This ORs the supplied flags with the previous values
     *
     * Parameters
     *     Session Ptr
     *     Flags
     *
     * Returns
     *     New Flags
     */
    uint32_t (*set_session_flags)(void *, uint32_t);

    /* Gets the flags for a session
     *
     * Parameters
     *     Session Ptr
     */
    uint32_t (*get_session_flags)(void *);

    /* Flushes the stream on an alert
     * Side that is flushed is the same as the packet.
     *
     * Parameters
     *     Packet
     */
    int (*alert_flush_stream)(Packet *);

    /* Flushes the stream on arrival of another packet
     * Side that is flushed is the opposite of the packet.
     *
     * Parameters
     *     Packet
     */
    int (*response_flush_stream)(Packet *);

    /* Calls user-provided callback function for each packet of
     * a reassembled stream.  If the callback function returns non-zero,
     * iteration ends.
     *
     * Parameters
     *     Packet
     *     Packet Iterator Function (called for each packet in the stream)
     *     user data (may be NULL)
     *
     * Returns
     *     number of packets
     */
    int (*traverse_reassembled)(Packet *, PacketIterator, void *userdata);

    /* Calls user-provided callback function for each segment of
     * a reassembled stream.  If the callback function returns non-zero,
     * iteration ends.
     *
     * Parameters
     *     Packet
     *     StreamSegmentIterator Function (called for each packet in the stream)
     *     user data (may be NULL)
     *
     * Returns
     *     number of packets
     */
    int (*traverse_stream_segments)(Packet *, StreamSegmentIterator, void *userdata);

    /* Add session alert
     *
     * Parameters
     *     Session Ptr
     *     gen ID
     *     sig ID
     *
     * Returns
     *     0 success
     *     -1 failure (max alerts reached)
     *
     */
    int (*add_session_alert)(void *, Packet *p, uint32_t, uint32_t);

    /* Check session alert
     *
     * Parameters
     *     Session Ptr
     *     Packet
     *     gen ID
     *     sig ID
     *
     * Returns
     *     0 if not previously alerted
     *     !0 if previously alerted
     */
    int (*check_session_alerted)(void *, Packet *p, uint32_t, uint32_t);

    /* Set Extra Data Logging
     *
     * Parameters
     *      Session Ptr
     *      Packet
     *      gen ID
     *      sig ID
     * Returns
     *      0 success
     *      -1 failure ( no alerts )
     *
     */
    int (*update_session_alert)(void *, Packet *p, uint32_t, uint32_t, uint32_t, uint32_t);

    /* Get Flowbits data
     *
     * Parameters
     *     Packet
     *
     * Returns
     *     Ptr to Flowbits Data
     */

    StreamFlowData *(*get_flow_data)(Packet *p);

    /* Set reassembly flush policy/direction for given session
     *
     * Parameters
     *     Session Ptr
     *     Flush Policy
     *     Direction(s)
     *     Flags
     *
     * Returns
     *     direction(s) of reassembly for session
     */
    /* XXX Do not attempt to set flush policy to PROTOCOL or PROTOCOL_IPS. */
    char (*set_reassembly)(void *, uint8_t, char, char);

    /* Get reassembly direction for given session
     *
     * Parameters
     *     Session Ptr
     *
     * Returns
     *     direction(s) of reassembly for session
     */
    char (*get_reassembly_direction)(void *);

    /* Get reassembly flush_policy for given session
     *
     * Parameters
     *     Session Ptr
     *     Direction
     *
     * Returns
     *     flush policy for specified direction
     */
    char (*get_reassembly_flush_policy)(void *, char);

    /* Get true/false as to whether stream data is in
     * sequence or packets are missing
     *
     * Parameters
     *     Session Ptr
     *     Direction
     *
     * Returns
     *     true/false
     */
    char (*is_stream_sequenced)(void *, char);

    /* Get whether there are missing packets before, after or
     * before and after reassembled buffer
     *
     * Parameters
     *      Session Ptr
     *      Direction
     *
     * Returns
     *      SSN_MISSING_BEFORE if missing before
     *      SSN_MISSING_NONE if none missing
     */
    int (*missing_in_reassembled)(void *, char);

    /* Get true/false as to whether packets were missed on
     * the stream
     *
     * Parameters
     *     Session Ptr
     *     Direction
     *
     * Returns
     *     true/false
     */
    char (*missed_packets)(void *, char);

#ifdef TARGET_BASED
    /* Get the protocol identifier from a stream
     *
     * Parameters
     *     Session Ptr
     *
     * Returns
     *     integer protocol identifier
     */
    int16_t (*get_application_protocol_id)(void *);

    /* Set the protocol identifier for a stream
     *
     * Parameters
     *     Session Ptr
     *     ID
     *
     * Returns
     *     integer protocol identifier
     */
    int16_t (*set_application_protocol_id)(void *, int16_t);

    /** Set service to either ignore, inspect or maintain session state.
     *  If this is called during parsing a preprocessor configuration, make
     *  sure to set the parsing argument to 1.
     */
    void (*set_service_filter_status)(struct _SnortConfig *sc,
                                      int service, int status, tSfPolicyId policyId, int parsing);
#endif

    /** Get an independent bit to allow an entity to enable and
     *  disable port session tracking and syn session creation
     *  without affecting the status of set by other entities.
     *  Returns a bitmask (with the bit range 3-15) or 0, if no bits
     *  are available.
     */
    uint16_t (*get_preprocessor_status_bit)(void);

    /** Set port to either ignore, inspect or maintain session state.
     *  If this is called during parsing a preprocessor configuration, make
     *  sure to set the parsing argument to 1.
     */
    void (*set_port_filter_status)(struct _SnortConfig *sc,
                                   int protocol, uint16_t port, uint16_t status, tSfPolicyId policyId, int parsing);

    /** Unset port to maintain session state. This function can only
     *  be used with independent bits acquired from
     *  get_preprocessor_status_bit. If this is called during
     *  parsing a preprocessor configuration, make sure to set the
     *  parsing argument to 1.
     */
    void (*unset_port_filter_status)(struct _SnortConfig *sc,
                                     int protocol, uint16_t port, uint16_t status, tSfPolicyId policyId, int parsing);

#ifdef ACTIVE_RESPONSE
    // initialize response count and expiration time
    void (*init_active_response)(Packet *, void *);
#endif

    // Get the TTL value used at session setup
    // outer=0 to get inner ip ttl for ip in ip; else outer=1
    uint8_t (*get_session_ttl)(void *ssnptr, char direction, int outer);

    /* Get the current flush point
     *
     * Arguments
     *  void * - session pointer
     *  char - direction
     *
     * Returns
     *  Current flush point for session
     */
    uint32_t (*get_flush_point)(void *, char);

    /* Set the next flush point
     *
     * Arguments
     *  void * - session pointer
     *  char - direction
     *  uint32_t - flush point size
     */
    void (*set_flush_point)(void *, char, uint32_t);

    /* Turn off inspection for potential session.
     * Adds session identifiers to a hash table.
     * TCP only.
     *
     * Parameters
     *     IP addr #1
     *     Port #1
     *     IP addr #2
     *     Port #2
     *     Protocol
     *     Current time (from packet)
     *     ID,
     *     Preprocessor ID calling this function,
     *     Preprocessor specific data,
     *     Preprocessor data free function. If NULL, then static buffer is assumed.
     *
     * Returns
     *     0 on success
     *     -1 on failure
     */
    int (*set_application_protocol_id_expected)(snort_ip_p, uint16_t, snort_ip_p, uint16_t,
                uint8_t, time_t, int16_t, uint32_t, void*, void (*)(void*));

#ifdef TARGET_BASED
    /* Get server IP address. This could be used either during packet processing or when
     * a session is being closed. Caller should make a deep copy if return value is needed
     * for later use.
     *
     * Arguments
     *  void * - session pointer
     *  uint32_t - direction. Valid values are SSN_DIR_FROM_SERVER or SSN_DIR_FROM_CLIENT
     *
     * Returns
     *  IP address. Contents at the buffer should not be changed. The
     */
     snort_ip_p  (*get_session_ip_address)(void *, uint32_t);
#endif

    // register for stateful scanning of in-order payload to determine flush points
    // autoEnable allows PAF regardless of s5 ports config
    bool (*register_paf_port)(
        struct _SnortConfig *, tSfPolicyId,
        uint16_t server_port, bool toServer,
        PAF_Callback, bool autoEnable);

    // get any paf user data stored for this session
    void** (*get_paf_user_data)(void* ssnptr, bool toServer);

    bool (*is_paf_active)(void* ssn, bool toServer);
    bool (*activate_paf)(void* ssn, bool toServer);

#ifdef ENABLE_HA
    /* Register a high availability producer and consumer function pair for a
     * particular preprocessor ID and subcode combination.
     *
     * Parameters
     *      Processor ID
     *      Subcode
     *      Maximum Message Size
     *      Message Producer Function
     *      Message Consumer Function
     *
     *  Returns
     *      >= 0 on success
     *          The returned value is the bit number in the HA pending bitmask and
     *          should be stored for future calls to set_ha_pending_bit().
     *      < 0 on failure
     */
    int (*register_ha_funcs)(uint32_t preproc_id, uint8_t subcode, uint8_t size,
                             StreamHAProducerFunc produce, StreamHAConsumerFunc consume);

    /* Indicate a pending high availability update for a given session.
     *
     * Parameters
     *      Session Ptr
     *      HA Pending Update Bit
     */
    void (*set_ha_pending_bit)(void *, int bit);

    /* Attempt to process any pending HA events for the given session
     *
     * Parameters
     *      Session Ptr
     */
    void (*process_ha)(void *);
#endif

    /** Set flag to force sessions to be created on SYN packets.
     *  This function can only be used with independent bits
     *  acquired from get_preprocessor_status_bit. If this is called
     *  during parsing a preprocessor configuration, make sure to
     *  set the parsing argument to 1.
     */
    void (*set_tcp_syn_session_status)(struct _SnortConfig *sc, uint16_t status, tSfPolicyId policyId, int parsing);

    /** Unset flag that forces sessions to be created on SYN
     *  packets. This function can only be used with independent
     *  bits acquired from get_preprocessor_status_bit. If this is
     *  called during parsing a preprocessor configuration, make
     *  sure to set the parsing argument to 1.
     */
    void (*unset_tcp_syn_session_status)(struct _SnortConfig *sc, uint16_t status, tSfPolicyId policyId, int parsing);

    /** Retrieve application session data based on the lookup tuples for
     *  cases where Snort does not have an active packet that is
     *  relevant.
     *
     * Parameters
     *     IP addr #1
     *     Port #1 (0 for non TCP/UDP)
     *     IP addr #2
     *     Port #2 (0 for non TCP/UDP)
     *     Protocol
     *     VLAN ID
     *     MPLS ID
     *     Address Space ID
     *     Preprocessor ID
     *
     * Returns
     *     Application Data reference (pointer)
     */
    void *(*get_application_data_from_ip_port)(snort_ip_p, uint16_t, snort_ip_p, uint16_t, char, uint16_t, uint32_t, uint16_t, uint32_t);

    //Register callbacks for extra data logging
    uint32_t (*reg_xtra_data_cb)(LogFunction );

    //Register Extra Data Log Function
    void (*reg_xtra_data_log)(LogExtraData, void *);

    //Get the Extra data map
    uint32_t (*get_xtra_data_map)(LogFunction **);

    //Retrieve the maximum session limits for the given policy
    void (*get_max_session_limits)(struct _SnortConfig *sc, tSfPolicyId, StreamSessionLimits*);

    /* Set direction that data is being ignored.
       *
       * Parameters
       *     Session Ptr
       */
    int (*set_ignore_direction)(void *, int);

    /** Retrieve stream session pointer based on the lookup tuples for
     *  cases where Snort does not have an active packet that is
     *  relevant.
     *
     * Parameters
     *     IP addr #1
     *     Port #1 (0 for non TCP/UDP)
     *     IP addr #2
     *     Port #2 (0 for non TCP/UDP)
     *     Protocol
     *     VLAN ID
     *     MPLS ID
     *     Address Space ID
     *
     * Returns
     *     Stream session pointer
     */
    void *(*get_session_ptr_from_ip_port)(snort_ip_p, uint16_t, snort_ip_p, uint16_t, char, uint16_t, uint32_t, uint16_t);

    /** Retrieve the session key given a stream session pointer.
     *
     * Parameters
     *     Session Ptr
     *
     * Returns
     *     Stream session key
     */
    const StreamSessionKey *(*get_key_from_session_ptr)(const void *);

    /* Delete the session if it is in the closed session state.
     *
     * Parameters
     *     Packet
     */
    void (*check_session_closed)(Packet *);

    /*  Create a session key from the Packet
     *
     *  Parameters
     *      Packet
     */
    StreamSessionKey *(*get_session_key)(Packet *);

    /*  Populate a session key from the Packet
     *
     *  Parameters
     *      Packet
     *      Stream session key pointer
     */
    void (*populate_session_key)(Packet *, StreamSessionKey *);

    /*  Get the application data from the session key
     *
     *  Parameters
     *      SessionKey *
     *      Application Protocol
     */
    void *(*get_application_data_from_key)(const StreamSessionKey *, uint32_t);

    // register for stateful scanning of in-order payload to determine flush points
    // autoEnable allows PAF regardless of s5 ports config
    bool (*register_paf_service)(
        struct _SnortConfig *, tSfPolicyId,
        uint16_t service, bool toServer,
        PAF_Callback, bool autoEnable);

    void (*set_extra_data)(void*, Packet *, uint32_t);
    void (*clear_extra_data)(void*, Packet *, uint32_t);

    /* Time out the specified session.
     *
     * Parameters
     *     Session Ptr
     */
    void (*expire_session)(void *);

    // register returns a non-zero id for use with set; zero is error
    unsigned (*register_event_handler)(Stream_Callback);
    bool (*set_event_handler)(void* ssnptr, unsigned id, Stream_Event);

} StreamAPI;

/* To be set by Stream5 */
extern StreamAPI *stream_api;

/**Port Inspection States. Port can be either ignored,
 * or inspected or session tracked. The values are bitmasks.
 */
typedef enum {
    /**Dont monitor the port. */
    PORT_MONITOR_NONE = 0x00,

    /**Inspect the port. */
    PORT_MONITOR_INSPECT = 0x01,

    /**perform session tracking on the port. */
    PORT_MONITOR_SESSION = 0x02

} PortMonitorStates;

#define PORT_MONITOR_SESSION_BITS   0xFFFE

#endif /* STREAM_API_H_ */

