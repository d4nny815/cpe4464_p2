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

typedef enum : uint8_t {
    L3 = 1,
    ARP = 2,
    NEIGHBOR = 3,
    DVR = 4,
} l2_protocol_t;

int my_fishnode_l2_receive(void *l2frame);
void my_arp_received(void *l2frame);

typedef struct __attribute__((packed)) l3_arp_header_t {
    uint32_t type;
    fnaddr_t queried_l3_addr;
    fn_l2addr_t l2_for_query_l3;
} l3_arp_header_t;

typedef enum : uint32_t {
    REQ = 1,
    RSP = 2,
} l3_arp_type_t;



#endif /* FISHNODE_H */