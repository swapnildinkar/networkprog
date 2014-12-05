#ifndef __UTILS_H
#define __UTILS_H

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <ctype.h>
#include <unp.h>
#include <netinet/in.h>
#include <arpa/inet.h>        
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if_packet.h>
#include <errno.h>      /* error numbers */
#include <sys/ioctl.h>          /* ioctls */

#define __UNIX_PROC_PATH "procpathfile"
#define IP_LEN      50
#define HW_ADDR_LEN 6
#define IF_NAME     16  /* same as IFNAMSIZ    in <net/if.h> */
#define IF_HADDR     6  /* same as IFHWADDRLEN in <net/if.h> */
#define IP_ALIAS     1  /* hwa_addr is an alias */

#define __DEBUG 0

#ifdef __DEBUG
#  define DEBUG(x) x
#else
#  define DEBUG(x) 
#endif

/************* store info about hw address, int index, ip **************/
struct hwa_info {
    char    if_name[IF_NAME];       /* interface name, null terminated */
    char    if_haddr[IF_HADDR];     /* hardware address */
    int     if_index;               /* interface index */
    short   ip_alias;               /* 1 if hwa_addr is an alias IP address */
    struct  sockaddr  *ip_addr;     /* IP address */
    struct  hwa_info  *hwa_next;    /* next of these structures */
};

struct hwaddr {
    int             sll_ifindex;    /* Interface number */
    unsigned short  sll_hatype;     /* Hardware type */
    unsigned char   sll_halen;      /* Length of address */
    unsigned char   sll_addr[8];    /* Physical layer address */
};

/* head of struct hwa_info */
struct hwa_info *hwa_struct_head;

/* self canonical ip */
extern char * self_ip_addr;

void print_mac(char *);
char * convert_to_mac (char *);
char * get_self_ip ();
char *get_name_ip (char *);
char *get_hwaddr_eth0();
struct hwa_info * Get_hw_struct_head();
int areq (struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr);

#endif /* __UTILS_H */

