#include "fishnode.h"
#include "fish.h"
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

// #define DEBUG
#define L2_IMPL

static int noprompt = 0;

void sigint_handler(int sig)
{
   if (SIGINT == sig)
       fish_main_exit();
}

static int print_route(void *callback_data __attribute__((unused)),
        fnaddr_t dest, int prefix_len __attribute((unused)),
        fnaddr_t net_hop __attribute((unused)),
        int metric __attribute((unused)),
        void *entry_data __attribute__((unused))) {
   printf("%s\n", fn_ntoa(dest));
   return 0;
}

static void keyboard_callback(char *line)
{
   if (0 == strcasecmp("show neighbors", line))
      fish_print_neighbor_table();
   else if (0 == strcasecmp("show arp", line))
      fish_print_arp_table();
   else if (0 == strcasecmp("show route", line)) {
      fish_print_forwarding_table();
      fish_fwd.iterate_entries(&print_route, NULL, FISH_FWD_TYPE_BROADCAST);
   }
   else if (0 == strcasecmp("show dv", line))
      fish_print_dv_state();
   else if (0 == strcasecmp("quit", line) || 0 == strcasecmp("exit", line))
      fish_main_exit();
   else if (0 == strcasecmp("show topo", line))
      fish_print_lsa_topo();
   else if (0 == strcasecmp("help", line) || 0 == strcasecmp("?", line)) {
      printf("Available commands are:\n"
             "    exit                         Quit the fishnode\n"
             "    help                         Display this message\n"
             "    quit                         Quit the fishnode\n"
             "    show arp                     Display the ARP table\n"
             "    show dv                      Display the dv routing state\n"
             "    show neighbors               Display the neighbor table\n"
             "    show route                   Display the forwarding table\n"
             "    show topo                    Display the link-state routing\n"
             "                                 algorithm's view of the network\n"
             "                                 topology\n"
             "    ?                            Display this message\n"
            );
   }
   else if (line[0] != 0)
      printf("Type 'help' or '?' for a list of available commands.  "
             "Unknown command: %s\n", line);

   if (!noprompt)
      printf("> ");

   fflush(stdout);
}

// Prototypes for program 2.  Taken directly from fish.h header file
#ifdef L2_IMPL

void parse_l2_header(void *l2frame, l2_header_t *header) {
    memcpy(header, l2frame, sizeof(l2_header_t));

    header->checksum = ntohs(header->checksum);
    header->length = ntohs(header->length);
    
    return;
}

void parse_l3_arp_header(void* l3frame, l3_arp_frame_t* header) {
    memcpy(header, l3frame, sizeof(l3_arp_frame_t));

    header->type = ntohl(header->type);
    
    return;
}

void send_arp_response(l2_header_t* l2_header, l3_arp_frame_t* req_arp_header) {
    #define RSP_L2_LENGTH (sizeof(l2_header_t) + sizeof(l3_arp_frame_t))

    // construct l3 arp response header
    l3_arp_frame_t rsp_arp_header;
    rsp_arp_header.type = htonl(L3_ARP_RSP);
    rsp_arp_header.queried_l3_addr = req_arp_header->queried_l3_addr;
    rsp_arp_header.l2_for_query_l3 = fish_getl2address();

    // construct l2 rsp header
    l2_header_t rsp_l2_header;
    rsp_l2_header.dst_l2_addr = l2_header->src_l2_addr;
    rsp_l2_header.src_l2_addr = fish_getl2address();
    rsp_l2_header.checksum = 0; 
    rsp_l2_header.length = htons(RSP_L2_LENGTH);
    rsp_l2_header.protocol = L2_PROTO_ARP;
    
    // construct l2 frame
    uint8_t l2_frame[RSP_L2_LENGTH];
    memcpy(l2_frame, &rsp_l2_header, sizeof(l2_header_t));
    memcpy(l2_frame + sizeof(l2_header_t), &rsp_arp_header, sizeof(l3_arp_frame_t));
    
    uint16_t checksum = in_cksum(l2_frame, RSP_L2_LENGTH);
    l2_header_t* p_l2_frame = (l2_header_t*)l2_frame;
    p_l2_frame->checksum = checksum;

    if (fish_l1_send((void*)l2_frame) != 0) {
        // printf("[SEND_ARP_RESP] Failed to send ARP Response\n");
        return;
    }   

    return;
}

void send_l2_frame(fn_l2addr_t dst_l2_addr, uint8_t* l2_frame) {
    // add l2 addr
    l2_header_t* l2_header = (l2_header_t*)l2_frame;
    l2_header->dst_l2_addr = dst_l2_addr;

    // calc checksum
    uint16_t checksum = in_cksum(l2_frame, ntohs(l2_header->length));
    l2_header->checksum = checksum;

    // l1 send
    if (fish_l1_send((void*)l2_frame) != 0) {
        // printf("[L2 SEND] Failed to send ARP Response\n");
        return;
    }   

    // free frame
    free((void*)l2_frame);

    // return
    return;
}

void my_resolve_arp_cb(fn_l2addr_t l2_addr, void* l2_frame) {
    // check validity of l2 addr
    if (!FNL2_VALID(l2_addr)) {
        free(l2_frame);
        return;
    }

    // send frame
    send_l2_frame(l2_addr, l2_frame);

    return;
}

int my_fish_l2_send(void *l3frame, fnaddr_t next_hop, int len, uint8_t l2_proto) {
    // construct l2 frame without dst l2 address since dont know yet
    // make l2 header
    l2_header_t l2_header;
    l2_header.dst_l2_addr = (fn_l2addr_t){0};
    l2_header.src_l2_addr = fish_getl2address();
    l2_header.checksum = 0;
    l2_header.length = htons(sizeof(l2_header_t) + len);
    l2_header.protocol = l2_proto;

    // wrap l3 frame in l2 frame
    uint8_t* l2_frame = (uint8_t*)malloc(sizeof(l2_header_t) + len);
    if (!l2_frame) {
        printf("[L2_SEND] Failed to allocate memory for L2 frame\n");
        return 1;
    }

    memcpy(l2_frame, &l2_header, sizeof(l2_header_t));
    memcpy(l2_frame + sizeof(l2_header_t), l3frame, len);

    if (next_hop == ALL_NEIGHBORS) {
        send_l2_frame(ALL_L2_NEIGHBORS, l2_frame);
    } else {
        fish_arp.resolve_fnaddr(next_hop, my_resolve_arp_cb, (void*)l2_frame);
        return 1;
    }

    return 0;
}

int my_fishnode_l2_receive(void *l2frame) {
    // recieve l2 frame
    l2_header_t header;
    parse_l2_header(l2frame, &header);

    // if size doesnt add up, drop frame
    if (header.length > MTU || header.length < sizeof(l2_header_t)) {
        // printf("[L2_RECEIVE] invalid length\n");
        return 1;
    }

    // if invalid checksum, drop frame
    if (in_cksum(l2frame, header.length) != 0) {
        // printf("[L2_RECEIVE] invalid checksum\n");
        return 1;
    }

    // if not l2 destination, drop frame
    bool valid_l2 = FNL2_VALID(header.dst_l2_addr);
    bool broadcast_addr = FNL2_EQ(header.dst_l2_addr, ALL_L2_NEIGHBORS); 
    fn_l2addr_t my_l2_addr = fish_getl2address();
    bool my_unicast_addr = FNL2_EQ(header.dst_l2_addr, my_l2_addr);
    if (!valid_l2 || !(broadcast_addr || my_unicast_addr)) {
        // if (!valid_l2) printf("[L2_RECEIVE] Not valid\n");
        // if (!broadcast_addr) printf("[L2_RECEIVE] Not broadcast\n");
        // if (!my_unicast_addr) printf("[L2_RECEIVE] Not meant for me\n");
        // printf("[L2_RECEIVE] Not valid address or not broadcast or not meant for me\n");
        return 1;
    }

    // remove l2 header
    void *l3frame = (void *)((uint8_t *)(l2frame) + sizeof(l2_header_t));
    int len = header.length - sizeof(l2_header_t);
    
    // pass l3 frame to l3 receive function
    switch (header.protocol) {
        case L2_PROTO_L3: 
            fish_l3.fish_l3_receive(l3frame, len, header.protocol);
            break;
        case L2_PROTO_ARP:
            fish_arp.arp_received(l2frame);
            break;
        default:
            // printf("[L2_RECEIVE] Unknown protocol\n");
            return 1;    
    }

    return 0;
}

void my_arp_received(void *l2frame) {
    l2_header_t l2_header;
    parse_l2_header(l2frame, &l2_header);
    
    // parse ARP header
    void *l3_frame = (void *)((uint8_t *)(l2frame) + sizeof(l2_header_t));
    l3_arp_frame_t arp_header;
    parse_l3_arp_header(l3_frame, &arp_header);

    switch (arp_header.type) {
        case L3_ARP_REQ:
            bool im_queried_l3 = arp_header.queried_l3_addr == fish_getaddress();
            if (!im_queried_l3) {
                // printf("[ARP_RECV] Not queried L3 addr\n");
                return;
            }

            send_arp_response(&l2_header, &arp_header);
            
            break;
        case L3_ARP_RSP:
            fish_arp.add_arp_entry(arp_header.l2_for_query_l3, arp_header.queried_l3_addr, 180); // TODO: add timeout constant

            break;
        default:
            printf("[ARP_RECV] Invalid ARP Type\n");
    }

    return;
}

void my_send_arp_request(fnaddr_t l3addr) {
    // create l3 arp req frame
    l3_arp_frame_t req_arp_header;
    req_arp_header.type = htonl(L3_ARP_REQ);
    req_arp_header.queried_l3_addr = l3addr;
    req_arp_header.l2_for_query_l3 = fish_getl2address();

    // call l2 send
    fish_l2.fish_l2_send(&req_arp_header, ALL_NEIGHBORS, sizeof(l3_arp_frame_t), L2_PROTO_ARP);

    return;
}

// void my_add_arp_entry(fn_l2addr_t l2addr, fnaddr_t addr, int timeout)
// {
// }

// void my_resolve_fnaddr(fnaddr_t addr, arp_resolution_cb cb, void *param)
// {
// }

#endif
/* 

   #ifdef L3_IMPL
   int my_fishnode_l3_receive(void *l3frame, int len)
   {
      return 0;
   }

   int my_fish_l3_send(void *l4frame, int len, fnaddr_t dst_addr,
                  uint8_t proto, uint8_t ttl)
   {
      return 0;
   }

   int my_fish_l3_forward(void *l3frame, int len)
   {
      return 0;
   }

   // Callback to broadcast DV advertisement
   void my_timed_event(void*)
   {
   }

   // Full functionality
   void* my_add_fwtable_entry(fnaddr_t dst, int prefix_length, fnaddr_t next_hop,
                     int metric, char type, void *user_data)
   {
      return NULL;
   }

   void* my_remove_fwtable_entry(void *route_key)
   {
      return NULL;
   }

   int my_update_fwtable_metric(void *route_key, int new_metric)
   {
      return 0;
   }

   fnaddr_t my_longest_prefix_match(fnaddr_t addr)
   {
      return 0;
   }
   #endif 
*/

int main(int argc, char **argv)
{
    struct sigaction sa;
   int arg_offset = 1;

   /* Verify and parse the command line parameters */
    if (argc != 2 && argc != 3 && argc != 4)
    {
        printf("Usage: %s [-noprompt] <fishhead address> [<fn address>]\n", argv[0]);
        return 1;
    }

   if (0 == strcasecmp(argv[arg_offset], "-noprompt")) {
      noprompt = 1;
      arg_offset++;
   }

   /* Install the signal handler */
    sa.sa_handler = sigint_handler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (-1 == sigaction(SIGINT, &sa, NULL))
    {
        perror("Couldn't set signal handler for SIGINT");
        return 2;
    }

   /* Set up debugging output */
#ifdef DEBUG
    // fish_setdebuglevel(FISH_DEBUG_INTERNAL);
    fish_setdebuglevel(FISH_DEBUG_ALL);
#else
    fish_setdebuglevel(FISH_DEBUG_NONE);
#endif
    fish_setdebugfile(stdout);

   /* Join the fishnet */
    if (argc-arg_offset == 1)
        fish_joinnetwork(argv[arg_offset]);
    else
        fish_joinnetwork_addr(argv[arg_offset], fn_aton(argv[arg_offset+1]));

   /* Install the command line parsing callback */
   fish_keybhook(keyboard_callback);
   if (!noprompt)
      printf("> ");
   fflush(stdout);

#ifdef L2_IMPL
   // Examples of overriding function pointers for program 2 base functionality
   fish_l2.fishnode_l2_receive = &my_fishnode_l2_receive;
//    fish_l2.fish_l2_send = &my_fish_l2_send;
//    fish_arp.arp_received = &my_arp_received;
//    fish_arp.send_arp_request = &my_send_arp_request;
   // Full functionality functions
   // fish_arp.add_arp_entry = &my_add_arp_entry;
   // fish_arp.resolve_fnaddr = &my_resolve_fnaddr;
#endif

#ifdef L3_IMPL
   fish_l3.fishnode_l3_receive = &my_fishnode_l3_receive;
   fish_l3.fish_l3_send = &my_fish_l3_send;
   fish_l3.fish_l3_forward = &my_fish_l3_forward;
   // Set up a callback to broadcast DV advertisement
   fish_scheduleevent(0, &my_timed_event, NULL);
   // Full functionality
   fish_fwd.add_fwtable_entry = &my_add_fwtable_entry;
   fish_fwd.remove_fwtable_entry = &my_remove_fwtable_entry;
   fish_fwd.update_fwtable_metric = &my_update_fwtable_metric;
   fish_fwd.longest_prefix_match = &my_longest_prefix_match;
#endif

#if 1
   /* Enable the built-in neighbor protocol implementation.  This will discover
    * one-hop routes in your fishnet.  The link-state routing protocol requires
    * the neighbor protocol to be working, whereas it is redundant with DV.
    * Running them both doesn't break the fishnode, but will cause extra routing
    * overhead */
   fish_enable_neighbor_builtin( 0
         | NEIGHBOR_USE_LIBFISH_NEIGHBOR_DOWN
      );
#endif

   /* Enable the link-state routing protocol.  This requires the neighbor
    * protocol to be enabled. */
   fish_enable_lsarouting_builtin(0);

#if 1
   /* Full-featured DV routing.  I suggest NOT using this until you have some
    * reasonable expectation that your code works.  This generates a lot of
    * routing traffic in fishnet */

   fish_enable_dvrouting_builtin( 0
         | DVROUTING_WITHDRAW_ROUTES
         | DVROUTING_TRIGGERED_UPDATES
         | RVROUTING_USE_LIBFISH_NEIGHBOR_DOWN
         | DVROUTING_SPLIT_HOR_POISON_REV
         | DVROUTING_KEEP_ROUTE_HISTORY
    );
#endif

   /* Execute the libfish event loop */
    fish_main();

   /* Clean up and exit */
   if (!noprompt)
      printf("\n");
   fish_keybhook(NULL);

    printf("Fishnode exiting cleanly.\n");

   fishnet_cleanup();

   // Cleanup your data structures here

    return 0;
}
