#ifndef COMMON_NSH_TYPES_H_
#define COMMON_NSH_TYPES_H_

/* VXLAN Header:

    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |R|R|R|R|I|P|R|R|   Reserved                    |Next Protocol  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                VXLAN Network Identifier (VNI) |   Reserved    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct _VxLANHdr
{
	uint8_t  flag;
	uint16_t reserved;
	uint8_t np;
    uint32_t vni_reserved2;

} VxLANHdr;

/* NSH base Header

     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |Ver|O|C|R|R|R|R|R|R|   Length  |  MD-type=0x1  | Next Protocol |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          Service Path ID                      | Service Index |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
typedef struct _NSHBaseHdr
{
    uint16_t ver_flag_length;
    uint8_t	mtype;				/* MD-type{1: 'Fixed Length', 2: 'Variable Length'} */
    uint8_t np;					/* Next Protocol {1: 'IPv4', 2: 'IPv6', 3: 'Ethernet'} */
    uint32_t srvpid_srvidx;		/* Service Path ID + Service Index */

} NSHBaseHdr;

/* NSH Context Header
	When the base header specifies MD Type 1, NSH defines four 4-byte
 	mandatory context headers.

    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  Network Platform Context                     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  Network Shared Context                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  Service Platform Context                     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  Service Shared Context                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 */

typedef struct _NSHContextHdr
{
    uint32_t net_platform_ctx;
    uint32_t net_shared_ctx;
    uint32_t srv_platform_ctx;
    uint32_t srv_shared_ctx;;

} NSHContextHdr;

/*
	NSH Variable Length Context Header.
	When the base header specifies MD Type 2, NSH defines variable length
	only context headers.  There may be zero or more of these headers as
	per the length field.

	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |          TLV Class            |      Type     |R|R|R|   Len   |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                      Variable Metadata                        |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct _NSHVarLenMDHdr
{
    uint16_t tlv_class;  /* describes the scope of the "Type" field. */
    uint8_t	type;		/* the specific type of information being carried, within the scope of a given TLV Class */
    uint8_t rrr_len;	/* RRR: reserved bit are present for future use. Len: Length of the variable metadata, in 4-byte words. */

} NSHVarLenMDHdr;
#endif /* COMMON_NSH_TYPES_H_ */
