#include "utils.h"
#include <assert.h>

/* globals */
char *self_ip_addr;

/* API to find out MAC address of destination IP. */
int 
areq (struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr) {
    
    printf("\nDebug: came in areq\n");
    assert(IPaddr);
    assert(HWaddr);
    
    int sockfd = 0, socklen, length, nbytes, i;
    char dest_ip[IP_LEN];
    struct sockaddr_un arp_proc_addr;
    fd_set set, currset; 
    
    struct sockaddr_in *inaddr = (struct sockaddr_in *)IPaddr;
    
    char str_seq[MAXLINE], buff[MAXLINE]; 
    socklen = sizeof (struct sockaddr_un);
    
    /* create new UNIX Domain socket */
    if((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "\nerror creating unix domain socket\n");
        return -1;
    }

    /* ODR process to send message to */
    memset (&arp_proc_addr, 0, sizeof(struct sockaddr_un));
    arp_proc_addr.sun_family = AF_LOCAL;
    strcpy(arp_proc_addr.sun_path, __UNIX_PROC_PATH);
    
    /* connect on socket */
    if (connect(sockfd, (struct sockaddr *)&arp_proc_addr, sizeof(arp_proc_addr)) < 0) {
        fprintf(stderr, "\nerror connecting unix domain socket\n");
        return -1;
    }
    
    printf("\n Dest ip sent %s\n", inet_ntoa(inaddr->sin_addr));

    if ((length = write(sockfd, &inaddr->sin_addr, sizeof(int))) < 0) {
        fprintf(stderr, "\nerror writing on unix domain socket\n");
        return -1;
    }

    /* Get MAC address returned by ARP, and give it back to client. */
   
    FD_ZERO (&set);
    FD_SET (sockfd, &set);
    
    if (select (sockfd + 1, &set, NULL, NULL, NULL) < 0) {
        if (errno == EINTR) {
        
        }
        perror ("Select error.");
    }
 
    /* Received msg from ARP. */
    if (FD_ISSET (sockfd, &set)) {
        
        memset(buff, 0, MAXLINE); 
        memset(&arp_proc_addr, 0, sizeof(arp_proc_addr));
 
        DEBUG(printf ("\n====================================PROC_MESSAGE_RECEIVED====================================\n"));
        /* block on recvfrom. collect info in 
         * proc_addr and data in buff */
        if ((nbytes = read(sockfd, buff, MAXLINE)) <= 0) { 
            perror("Error in recvfrom");
            return 0;
        }
        buff[nbytes] = '\0';

        printf("\nlength of recvd buff %d\n", nbytes); 
        print_mac(buff);    
        for (i = 0; i < nbytes; ++i) {
           HWaddr->sll_addr[i] = buff[i]; 
        }
    }

    return 1;
}

/***********************************************************************
 * function get_hw_addrs() has been copied from Implementation given 
 * in a directory on minix
 ***********************************************************************/

struct hwa_info *
get_hw_addrs()
{
    struct hwa_info *hwa, *hwahead, **hwapnext;
    int     sockfd, len, lastlen, alias, nInterfaces, i;
    char        *ptr, *buf, lastname[IF_NAME], *cptr;
    struct ifconf   ifc;
    struct ifreq    *ifr, *item, ifrcopy;
    struct sockaddr *sinptr;

    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

    lastlen = 0;
    len = 100 * sizeof(struct ifreq);   /* initial buffer size guess */
    for ( ; ; ) {
        buf = (char*) Malloc(len);
        ifc.ifc_len = len;
        ifc.ifc_buf = buf;
        if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
            if (errno != EINVAL || lastlen != 0)
                err_sys("ioctl error");
        } else {
            if (ifc.ifc_len == lastlen)
                break;      /* success, len has not changed */
            lastlen = ifc.ifc_len;
        }
        len += 10 * sizeof(struct ifreq);   /* increment */
        free(buf);
    }

    hwahead = NULL;
    hwapnext = &hwahead;
    lastname[0] = 0;

    ifr = ifc.ifc_req;
    nInterfaces = ifc.ifc_len / sizeof(struct ifreq);
    for(i = 0; i < nInterfaces; i++)  {
        item = &ifr[i];
        alias = 0; 
        hwa = (struct hwa_info *) Calloc(1, sizeof(struct hwa_info));
        memcpy(hwa->if_name, item->ifr_name, IF_NAME);      /* interface name */
        hwa->if_name[IF_NAME-1] = '\0';
        /* start to check if alias address */
        if ( (cptr = (char *) strchr(item->ifr_name, ':')) != NULL)
            *cptr = 0;      /* replace colon will null */
        if (strncmp(lastname, item->ifr_name, IF_NAME) == 0) {
            alias = IP_ALIAS;
        }
        memcpy(lastname, item->ifr_name, IF_NAME);
        ifrcopy = *item;
        *hwapnext = hwa;        /* prev points to this new one */
        hwapnext = &hwa->hwa_next;  /* pointer to next one goes here */

        hwa->ip_alias = alias;      /* alias IP address flag: 0 if no; 1 if yes */
        sinptr = &item->ifr_addr;
        hwa->ip_addr = (struct sockaddr *) Calloc(1, sizeof(struct sockaddr));
        memcpy(hwa->ip_addr, sinptr, sizeof(struct sockaddr));  /* IP address */
        if (ioctl(sockfd, SIOCGIFHWADDR, &ifrcopy) < 0)
            perror("SIOCGIFHWADDR");  /* get hw address */
        memcpy(hwa->if_haddr, ifrcopy.ifr_hwaddr.sa_data, IF_HADDR);
        if (ioctl(sockfd, SIOCGIFINDEX, &ifrcopy) < 0)
            perror("SIOCGIFINDEX");   /* get interface index */
        memcpy(&hwa->if_index, &ifrcopy.ifr_ifindex, sizeof(int));
    }
    free(buf);
    return(hwahead);    /* pointer to first structure in linked list */
}

void
free_hwa_info(struct hwa_info *hwahead)
{
    struct hwa_info *hwa, *hwanext;

    for (hwa = hwahead; hwa != NULL; hwa = hwanext) {
        free(hwa->ip_addr);
        hwanext = hwa->hwa_next;    /* can't fetch hwa_next after free() */
        free(hwa);          /* the hwa_info{} itself */
    }
}
/* end free_hwa_info */

/* get hwa struct head*/
struct hwa_info *
Get_hw_struct_head()
{
    if (hwa_struct_head == NULL) {
        if ((hwa_struct_head = get_hw_addrs()) == NULL)
            err_quit("get_hw_addrs error");
    }

    return hwa_struct_head;
}

/* get hw address for eth0 interface */
char *
get_hwaddr_eth0 () {
    
    char if_name[MAXLINE];
    struct hwa_info *curr = Get_hw_struct_head(); 
    for (; curr != NULL; curr = curr->hwa_next) {
        memset(if_name, 0, MAXLINE);

        /* if there are aliases in if name with colons, split them */
        sscanf(curr->if_name, "%[^:]", if_name);

        if (strcmp(if_name, "eth0") == 0) {
            
            return curr->if_haddr;
        }
    }

    return NULL;
}

/* convert char sequence into mac addr */
void 
print_mac (char *src_macaddr) {
    assert(src_macaddr);
    int i = 0; 
    for(i = 0; i < HW_ADDR_LEN; ++i) {
        printf("%.2x%s",*src_macaddr++ & 0xff, (i == HW_ADDR_LEN-1)?" ":":");
    }
}

char *
convert_to_mac (char *src_macaddr) {
    assert(src_macaddr);
    int i = 0; 
    char *mac = (char *) malloc(HW_ADDR_LEN + 1);

    for(i = 0; i < HW_ADDR_LEN; ++i) {
        mac[i] = *src_macaddr++ & 0xff;    
    }
    return mac;
}

/* Get the canonical IP Address of the current node. */
char *
get_self_ip () {

    if (self_ip_addr != NULL) 
        return self_ip_addr;

    self_ip_addr = (char *)malloc(INET_ADDRSTRLEN);

    struct hwa_info *curr = Get_hw_struct_head();
    char if_name[MAXLINE];

    /* loop through all interfaces */
    for (; curr != NULL; curr = curr->hwa_next) {
        memset(if_name, 0, MAXLINE);

        /* if there are aliases in if name with colons, split them */
        sscanf(curr->if_name, "%[^:]", if_name);

        if (strcmp(if_name, "eth0") == 0) {
            inet_ntop(AF_INET, &( ((struct sockaddr_in *)curr->ip_addr)->sin_addr), 
                    self_ip_addr, INET_ADDRSTRLEN);
            assert(self_ip_addr); 
            return self_ip_addr;
        }
    }

    /* if no canonical ip found */
    DEBUG(printf("\nNo self canonical ip found"));
    self_ip_addr = NULL;
    return NULL;
}

/* get name from ip */
char *get_name_ip (char *ip) {
    
    assert(ip);

    struct hostent *he;
    struct in_addr ipv4addr;

    inet_pton(AF_INET, ip, &ipv4addr);
    he = gethostbyaddr(&ipv4addr, sizeof(ipv4addr), AF_INET);

    assert(he); 
    return he->h_name;
}
