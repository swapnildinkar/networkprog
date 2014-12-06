#include "unp.h"
#include "utils.h"
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if_packet.h>

#define ARP_HDR_PROTO 50005
#define ARP_FRAME_PROTO 10002

/* Type of frame */
#define __ARPREQ       1
#define __ARPREP       2

/* Cache entry */
typedef struct c_entry {
    char    ip_addr[IP_LEN];
    char    mac_addr[HW_ADDR_LEN];
    int     if_no;
    unsigned short sll_hatype;
    int     sockfd;
    struct c_entry *next;
} c_entry_t;

/* ARP Frame */
typedef struct arp_frame {
    uint16_t   hard_type;
    uint16_t   prot_type;
    uint8_t    hard_size;
    uint8_t    prot_size;
    uint16_t   op;
    char  src_mac[HW_ADDR_LEN];
    char  src_ip[IP_LEN];
    char  dest_mac[HW_ADDR_LEN];
    char  dest_ip[IP_LEN];
} arp_frame_t;

/* head of odr routing table */
c_entry_t *cache_table_head;

