#ifndef SNIFFER_RULEID_H_
#define SNIFFER_RULEID_H_

/* The rule ID size can be of two sizes: uint16, uint32.
 * Out of the box the size is uint16, in order to save space. */
#define RULE_ID_SIZE 16

#if RULE_ID_SIZE == 16
	typedef uint16_t rule_id_t;
#else
	typedef uint32_t rule_id_t;
#endif

#endif /* SNIFFER_RULEID_H_ */
