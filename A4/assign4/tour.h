#include <unp.h>
#include "utils.h"
#include <netinet/ip.h> 

#define ETH_P_IP        0x0800
#define IPPROTO_AND     155
#define HDR_ID          256

typedef struct tour_frame {
    struct ip ip_hdr;
    char payload[25][IP_LEN];
    char multicast_ip[IP_LEN];
    int  multicast_port;
    int index;
    int size;
}tour_frame_t;


