#ifndef FISHNODE_H
#define FISHNODE_H

#include <stdint.h>
#include "fish.h"

typedef struct __attribute__((packed)) l2_header_t {
    fn_l2addr_t dst_l2_addr;
    fn_l2addr_t src_l2_addr;
    uint16_t checksum;
    uint16_t length; // including l2 and on
    uint8_t protocol;
} l2_header_t;

typedef enum { 
    L2_PROTO_L3 = 1,
    L2_PROTO_ARP = 2,
    L2_PROTO_NEIGHBOR = 3,
    L2_PROTO_DVR = 4,
} l2_protocol_t;

typedef struct __attribute__((packed)) l3_arp_frame_t {
    uint32_t type;
    fnaddr_t queried_l3_addr;
    fn_l2addr_t l2_for_query_l3;
} l3_arp_frame_t;

typedef enum {
    L3_ARP_REQ = 1,
    L3_ARP_RSP = 2,
} l3_arp_type_t;


#endif /* FISHNODE_H */
