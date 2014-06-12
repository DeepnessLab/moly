/*
 * Flags.h
 *
 *  Created on: Jan 17, 2011
 *      Author: yotamhc
 */

#ifndef FLAGS_H_
#define FLAGS_H_

//#define TRACE_STATE_MACHINE
//#define PRINT_AHO_CORASICK
//#define PRINT_STATE_GENERATION
//#define PRINT_MATCHES
//#define COUNT_FAIL_PERCENT
//#define MIX_IDS
//#define PRINT_CHAR_COUNT
//#define PRINT_OUR_WC_PATTERNS
//#define PRINT_LENGTH_HIST
//#define SIMPLE_USE_TABLE
//#define FIND_FAIL_HISTOGRAM
//#define COUNT_BY_DEPTH
//#define DEPTHMAP_INFOCOM
//#define DEPTHJUMP_INFOCOM
//#define COUNT_CALLS
//#define X_RATIO // # of matches / # of root visits
//#define AC_PRINT_DEPTH_HIST
//#define PRINT_STATE_VISIT_HIST
//#define HYBRID_SCANNER
//#define HEAVY_PACKET_RECOGNITION
//#define HEAVY_PACKET_RECOGNITION_BYTES_TO_READ_BEFORE_CHECKING 	20
//#define HEAVY_PACKET_RECOGNITION_UNCOMMON_RATE_TO_TAKE_ACTION	0.25
//#define GLOBAL_TIMING
//#define MIXED_TRAFFIC
//#define DEPTHMAP // needed for heavy packet transfer!

////////////////////////////////////////////////////////////////////////////////////////////////
// The following flags should be commented out to use the automatic makefile for remote tests //
//#define PACKETS_TO_STEAL 2
//#define ALERT_ALWAYS // always use alert mode
//#define ALERT_NEVER  // never use alert mode
//#define FORCE_TRACE  // use trace define by FORCED_TRACE
//#define FORCED_TRACE "path"
//#define FORCED_TRACE_REPEAT 2
//#define DEDICATED_USE_COMPRESSED
////////////////////////////////////////////////////////////////////////////////////////////////

//#define EXTRA_EVENTS
//#define PRINT_GLOBAL_TIMER_EVENTS
//#define SCANNER_STATISTICS
//#define DROP_STATISTICS
//#define DROP_ALL_HEAVY
//#define COUNT_DROPPED
//#define LOCK_CYCLIC_QUEUE
//#define USE_MUTEX_LOCKS
//#define FIND_MC2_BAD_PATH
//#define ISOLATION_TEST
//#define PAPI
//#define DONT_TRANSFER_STOLEN

//#define PCRE

#define MIN_PATTERN_LENGTH 16

#endif /* FLAGS_H_ */
