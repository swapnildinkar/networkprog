#include <unp.h>
#include "utils.h"
#include <netinet/ip.h> 
#include <sys/time.h>
#include <netinet/ip_icmp.h>

#define ETH_P_IP        0x0800
#define IPPROTO_AND     155
#define ICMP_AND        3113
#define HDR_ID          256
#define MC_IP_ADDR      "225.225.225.225"
#define MC_PORT         5678 
#define ETH_HDRLEN      14

typedef struct tour_frame {
    struct ip ip_hdr;
    char payload[25][IP_LEN];
    char mc_ip[IP_LEN];
    int  mc_port;
    int index;
    int size;
}tour_frame_t;

