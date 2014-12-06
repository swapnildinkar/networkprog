#include "arp.h"
#include "utils.h"
#include <stdlib.h>
#include <assert.h>


/* Construct ARP frame */
arp_frame_t *
construct_arp (int hard_type, int prot_type, int op, char *src_mac,
                    char *src_ip, char *dest_mac, char *dest_ip) {
    
    assert (src_mac);
    assert (src_ip);
    assert (dest_mac);
    assert (dest_ip);

    arp_frame_t *arp_frame  = (arp_frame_t *)calloc(1, sizeof(arp_frame_t));

    arp_frame->hard_type    = htons (hard_type);
    arp_frame->prot_type    = htons (ARP_HDR_PROTO);
    arp_frame->hard_size    =  (HW_ADDR_LEN);
    arp_frame->prot_size    =  (sizeof (prot_type));
    arp_frame->op           = htons (op);

    DEBUG (printf ("op: %d\n", op));
    memcpy (arp_frame->src_mac, src_mac, HW_ADDR_LEN);
    strncpy (arp_frame->src_ip, src_ip, IP_LEN);
    memcpy (arp_frame->dest_mac, dest_mac, HW_ADDR_LEN);
    strncpy (arp_frame->dest_ip, dest_ip, IP_LEN);

    return arp_frame;
}



void print_cache () {
        
        c_entry_t *entry = cache_table_head;

        printf("-----------------------------------------------"
                    "--------------------\n");
        printf("| %15s | %18s | %5s | %8s |\n", "IP ADDR", "HW ADDR", "ifno", "conn fd");
        printf("-----------------------------------------------"
                    "--------------------\n");
        
        for (; entry != NULL; entry = entry->next) {

            printf("| %s  |", entry->ip_addr);
            print_mac(entry->mac_addr);
            printf("| %5d | %8d |\n", entry->if_no, entry->sockfd);
            printf("-----------------------------------------------"
                    "--------------------\n");
        }

    return;
}







/* send raw ethernet frame */
int 
send_raw_frame (int sockfd, char *src_macaddr, 
        char *dest_macaddr, int int_index, arp_frame_t *arpframe) {

    /*target address*/
    struct sockaddr_ll socket_address;

    /*buffer for ethernet frame*/
    void* buffer = (void*)malloc(ETH_FRAME_LEN);

    /*pointer to ethenet header*/
    unsigned char* etherhead = buffer;

    /*userdata in ethernet frame*/
    unsigned char* data = buffer + 14;

    /*another pointer to ethernet header*/
    struct ethhdr *eh = (struct ethhdr *)etherhead;

    int i, j, send_result = 0;

    /*our MAC address*/
    char *src_mac  = convert_to_mac(src_macaddr);

    /* dest Mac address */
    char *dest_mac = convert_to_mac(dest_macaddr);

    /*prepare sockaddr_ll*/

    /*RAW communication*/
    socket_address.sll_family   = PF_PACKET;   
    socket_address.sll_protocol = htons(ETH_P_IP); 

    /*index of the network device */
    socket_address.sll_ifindex  = int_index;

    /*ARP hardware identifier is ethernet*/
    socket_address.sll_hatype   = ARPHRD_ETHER;

    /*target is another host*/
    socket_address.sll_pkttype  = PACKET_OTHERHOST;

    /*address length*/
    socket_address.sll_halen    = ETH_ALEN;     

    /*MAC - begin*/
    for(i = 0; i < HW_ADDR_LEN; ++i) {
        socket_address.sll_addr[i]  = dest_mac[i];     
    }
    socket_address.sll_addr[6]  = 0x00;/*not used*/
    socket_address.sll_addr[7]  = 0x00;/*not used*/


    /* construct the ethernet frame header
     * -------------------------------
     * |Dest Mac | Src Mac | type id |
     * -------------------------------
     */
    memcpy((void *)buffer, (void *)dest_mac, ETH_ALEN);
    memcpy((void *)(buffer+ETH_ALEN), (void *)src_mac, ETH_ALEN);
    eh->h_proto = htons(ARP_HDR_PROTO);

    /* copy the arp frame data into ethernet frame */
    memcpy((void *)data, (void *)arpframe, sizeof(arp_frame_t));

    char src_vmname[INET_ADDRSTRLEN], *src_ip;
    if ((src_ip = get_self_ip()) == NULL) {
        return -1;
    }

    /* Get name of vm we recvd packet from */
    strcpy(src_vmname, get_name_ip(src_ip));
    assert(src_vmname);

    printf("ARP on node %s sending ETH_FRAME. SRC MAC_ADDR :", src_vmname);
    print_mac (src_macaddr);
    printf("DEST MAC_ADDR :");
    print_mac (dest_macaddr);
    printf("\n");

    /* send the packet on wire */
    if ((send_result = sendto(sockfd, buffer, ETH_FRAME_LEN, 0, 
                    (struct sockaddr*)&socket_address, sizeof(socket_address))) < 0) {

        perror("\nError in Sending frame\n");     
    }

    return send_result;
}

/* Update the routing table entry */
int
update_c_entry (arp_frame_t *recv_buf, c_entry_t *c_entry, 
        int intf_n) {

    char src_vmname[INET_ADDRSTRLEN];

    if (!recv_buf || !c_entry)
        return -1;

    strcpy(c_entry->ip_addr, recv_buf->src_ip);
    strcpy(c_entry->mac_addr, recv_buf->src_mac);

    c_entry->if_no         = intf_n;
    //c_entry->sll_hatype    = recv_buf->hop_count + 1;
    //c_entry->sockfd        = recv_buf->broadcast_id;

    strcpy(src_vmname, get_name_ip(c_entry->ip_addr));
    
    printf("Updating entry in routing table for ip %s\n", src_vmname);
    return 1;
}

/* insert entry in cache based on values from proc */
int
insert_in_cache (char *ip_addr, char *HWaddr, int if_index, 
                                unsigned short sll_hatype, int connfd) {
    assert(ip_addr);
    assert(HWaddr);

    /* create new entry */
    c_entry_t *c_entry = (c_entry_t *)calloc(1, sizeof(c_entry_t));

    strcpy(c_entry->ip_addr, ip_addr);
    memcpy(c_entry->mac_addr, HWaddr, HW_ADDR_LEN);
    c_entry->if_no      = if_index;
    c_entry->sll_hatype = sll_hatype;
    c_entry->sockfd     = connfd;
    
    if (!cache_table_head) {
        cache_table_head = c_entry;
        return 1;
    }

    c_entry->next     = cache_table_head;
    cache_table_head  = c_entry;
    
    return 1;
}

int 
update_cache_entry (c_entry_t *entry, char *src_ip, char *srcmac, 
                            int sll_ifindex, int sll_hatype, int connfd) {
    assert(src_ip);
    
    if (entry == NULL)
        return -1;

    memcpy(entry->mac_addr, srcmac, HW_ADDR_LEN);
    entry->if_no = sll_ifindex;
    entry->sll_hatype = sll_hatype;
    entry->sockfd = connfd;

    return 1;
}




/* get the entry based on ip */
c_entry_t *
find_c_entry (char *ipaddr) {

    assert(ipaddr);
    c_entry_t *node = cache_table_head;

    for(; node != NULL; node = node->next) {

        if (strcmp(node->ip_addr, ipaddr) == 0)
            return node;
    }
    
    return NULL;
}








/* file the cache table entry */
int
insert_c_entry (arp_frame_t *recvd_frame, c_entry_t **c_entry, int intf_n) {
    
    assert(recvd_frame);

    /* create new entry */
    *c_entry = (c_entry_t *)calloc(1, sizeof(c_entry_t));

    /* insert all the fields in the cache table. */
    if (update_c_entry (recvd_frame, *c_entry, intf_n) < 0)
        return -1;

    DEBUG(printf("\nInserting in cache table for dest ip %s\n", (*c_entry)->ip_addr));

    /* if head is null */
    if (!cache_table_head ) {
        cache_table_head = *c_entry;
        return 1;
    }

    /* insert it as top of cache table */
    (*c_entry)->next = cache_table_head;
    cache_table_head = *c_entry;

    return 1;
}

/* Check if a cache entry exists */
int
get_c_entry (char *dest_ip, c_entry_t **c_entry) {
    assert (dest_ip);

    /* if cache table is empty */
    if (!cache_table_head) 
        return 0;

    c_entry_t *curr = cache_table_head;
    int i = 0;

    /* iterate over all entries in routing table */
    for (; curr != NULL; curr = curr->next) {

        /* if entry with given destination exists */
        if (strcmp (curr->ip_addr, dest_ip) == 0) {
            *c_entry = curr;
            return 1;
        }
    }
    return 1;
}

/* convert the uint32 attribs from network to host order */
int 
convert_net_host_order(arp_frame_t* arp_frame) {
    assert(arp_frame); 

    arp_frame->hard_type    = ntohs (arp_frame->hard_type);
    arp_frame->prot_type    = ntohs (arp_frame->prot_type);
    arp_frame->op           = ntohs (arp_frame->op);
    
    return 0;
}

/* broadcast the rreq packet received from this proc*/
int 
send_arp_req_broadcast (int sockfd, char *dest_ip, char *src_ip) {
    assert(dest_ip);
    assert(src_ip);

    struct hwa_info *curr = Get_hw_struct_head(); 
    char if_name[MAXLINE];
    arp_frame_t *arpframe;

    char src_vmname[INET_ADDRSTRLEN], dst_vmname[INET_ADDRSTRLEN];

    strcpy(src_vmname, get_name_ip(src_ip));
    strcpy(dst_vmname, get_name_ip(dest_ip));

    int type = __ARPREQ, hard_type = 0, prot_type = 0;

    /* dest mac with all ones */
    unsigned char dest_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    /* loop through all interfaces */
    for (; curr != NULL; curr = curr->hwa_next) {

        memset(if_name, 0, MAXLINE); 

        /* if there are aliases in if name with colons, split them */
        sscanf(curr->if_name, "%[^:]", if_name); 

        /*send packet on eth0 */
        if (strcmp(if_name, "eth0") == 0) {

            printf ("ARP REQ sent. Source node %s, Destination Node %s\n", 
                        src_vmname, dst_vmname);

            /* construct the odr frame */
            arpframe = construct_arp (hard_type, prot_type, __ARPREQ, curr->if_haddr, 
                    src_ip, dest_mac, dest_ip);

            if (arpframe == NULL) {
                fprintf(stderr, "\nError creating odr frame");
                return -1;
            }

            if(send_raw_frame (sockfd, curr->if_haddr, 
                        dest_mac, curr->if_index, arpframe) < 0) {
                return -1;
            }

            break;
        }
    }
    return 0;
}

/* Send an arp response */
int
send_arp_response (int pf_packet, char *dest_ip, char *dest_hw_addr, int sll_ifindex) {
    assert(dest_ip);
    assert(dest_hw_addr);

    arp_frame_t *arpframe;

    arpframe = construct_arp (0, 0, __ARPREP, get_hwaddr_eth0(), 
            get_self_ip(), dest_hw_addr, dest_ip);

    if (arpframe == NULL) {
        fprintf(stderr, "\nError creating odr frame");
        return -1;
    }

    printf("\nSending ARP REPLY to ip %s\n", dest_ip);

    if(send_raw_frame (pf_packet, get_hwaddr_eth0(), 
                dest_hw_addr, sll_ifindex, arpframe) < 0) {
        return -1;
    }
}


/* Handle ARP Requests received from peer message. */
int 
handle_proc_msg (int proc_sockfd, int arp_sockfd, char *buff, int connfd) {
    
    assert(buff);
    
    char *src_ip = get_self_ip ();
   // struct in_addr destip = *(struct in_addr *)buff;
    char dstip[IP_LEN], sll_ifindex[16], sll_hatype[MAXLINE], sll_halen[MAXLINE];
    
    sscanf(buff, "%[^','],%[^','],%[^','],%s",                                                                                                                                             
                                dstip, sll_ifindex, sll_hatype, sll_halen); 

    DEBUG(printf("\nReceived : %s\n", dstip));
    
    /* send the arp request out on all interfaces */
    if (send_arp_req_broadcast (arp_sockfd, dstip, src_ip) < 0) {
        printf ("Error sending ARP REQ\n");
        return -1;
    }
    
    if (insert_in_cache (dstip, "0.0.0.0", atoi(sll_ifindex), (unsigned short)atoi(sll_hatype), connfd)  < 0) {
        fprintf(stderr, "Error in inserting entry in cache"); 
        return -1;
    }
    
    print_cache();
    return 1;
}

/* Handle messages received over ethernet. */
int 
handle_ethernet_msg (int arp_sockfd, int proc_sockfd, struct sockaddr_ll *arp_addr,
                        void *recv_buff, char *src_mac) {
    
    struct sockaddr_un arp_proc_addr;
    char str_seq[MAXLINE], temp[MAXLINE];
    char *self_eth_hwaddr, *srcmac_rcvd;
    void *data = recv_buff + 14;
    c_entry_t *entry;

    arp_frame_t *rcvd_frame = (arp_frame_t *) data;
    int n;
    socklen_t socklen = sizeof (arp_proc_addr);
    
    convert_net_host_order (rcvd_frame);
    
    DEBUG (printf ("\nReceived frame src_mac: "));
    print_mac (rcvd_frame->src_mac);
    DEBUG (printf ("\nReceived frame dest_mac: "));
    print_mac (rcvd_frame->dest_mac);

    DEBUG (printf ("\nReceived frame src_ip: %s\n", rcvd_frame->src_ip));
    
    switch (rcvd_frame->op) {
        case __ARPREQ: {

            printf ("ARP Request received.\n");

            /* if dest ip and my self ip matches, this is for me */
            if (strcmp(get_self_ip(), rcvd_frame->dest_ip) == 0) {
                printf ("This request is for me \n"); 
                
                /* update entry in cache for this source */
                if (insert_in_cache (rcvd_frame->src_ip, rcvd_frame->src_mac, 
                     arp_addr->sll_ifindex, arp_addr->sll_hatype, -1)  < 0) {
             
                    fprintf(stderr, "Error in inserting entry in cache"); 
                    return -1;
                }

                /* send an ARP Response */
                send_arp_response(arp_sockfd, rcvd_frame->src_ip, 
                                        rcvd_frame->src_mac, arp_addr->sll_ifindex); 
                
            }
            break;
        }
        case __ARPREP: {
            printf ("ARP Response received.\n");
            
            srcmac_rcvd = convert_to_mac(rcvd_frame->src_mac);

            /* find entry in cache based on recvd ip */
            entry  = find_c_entry(rcvd_frame->src_ip);
            
            printf("\n connfd: %d\n", entry->sockfd);
            if (write(entry->sockfd, srcmac_rcvd, HW_ADDR_LEN) < 0) {
                perror("Error on write");
            }
            
            close (entry->sockfd);
            
            /* update an entry */
            update_cache_entry(entry, rcvd_frame->src_ip, srcmac_rcvd, 
                                arp_addr->sll_ifindex, arp_addr->sll_hatype, -1);
            break;
        }
        default: {
            printf ("Wrong op received: %d\n", rcvd_frame->op);
        }
    }
    
    //memset (&arp_proc_addr, 0, sizeof(struct sockaddr_un));
    //
    ///* ODR process to send message to */
    //arp_proc_addr.sun_family = AF_LOCAL;
    //strcpy(arp_proc_addr.sun_path, __UNIX_PROC_PATH);

    ///* construct the char sequence to write on UNIX Domain socket */
    //sprintf(str_seq, "%s\n", src_mac);
    //
    //Connect (proc_connfd, (struct sockaddr *) &arp_proc_addr, socklen);
    //printf ("Before sendto.\n");
    //
    //Write (proc_connfd, str_seq, strlen(str_seq));
    
    print_cache();
    return 1;
}








int main (int argc, const char *argv[]) {

    int proc_sockfd, arp_sockfd, socklen, len, connfd = -1, maxfd;
    struct sockaddr_un serv_addr, proc_addr, resp_addr;
    struct sockaddr_ll arp_addr;
    fd_set set, currset;
    char* src_mac;
    char buff[MAXLINE];

    /* initializations */
    socklen           = sizeof(struct sockaddr_un);
    void *recv_buf    = malloc(ETH_FRAME_LEN); 
    socklen_t arpsize = sizeof(struct sockaddr_ll);

    DEBUG (printf ("HW addr for eth0: "));
    src_mac = get_hwaddr_eth0 ();
    print_mac (src_mac);
    printf ("\n");

    /* create new UNIX Domain socket */
    if((proc_sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "\nerror creating unix domain socket\n");
        return 0;
    }

    printf("\n====================== ARP INITIALIZING ============================\n");

    unlink(__UNIX_PROC_PATH);
    memset(&serv_addr, 0, sizeof(serv_addr)); 
    
    serv_addr.sun_family = AF_LOCAL;
    strcpy(serv_addr.sun_path, __UNIX_PROC_PATH);

    /* Bind the UNIX Domain socket */
    Bind(proc_sockfd, (SA *)&serv_addr, SUN_LEN(&serv_addr));
    DEBUG(printf("\nUnix Domain socket %d, sun_path file name %s\n",
                proc_sockfd, __UNIX_PROC_PATH)); 
    
    Listen(proc_sockfd, LISTENQ);

    printf ("ARP Module called.");
    
    /* Get the new PF Packet socket */
    if((arp_sockfd = socket(PF_PACKET, SOCK_RAW, htons(ARP_HDR_PROTO))) < 0) {
        perror("error"); 
        fprintf(stderr, "error creating PF Packet socket\n");
        return 0;
    }

    FD_ZERO(&set);
    FD_SET(proc_sockfd, &set);
    FD_SET(arp_sockfd, &set);
    
    maxfd = max (proc_sockfd, arp_sockfd);
    /* TESTING */
    //handle_proc_msg (proc_sockfd, arp_sockfd);
    //DEBUG (printf ("ARP REQ sent!\n"));

    /* Monitor both sockets. */
    while (1) {
        currset = set;
        if (select(maxfd+1, 
                    &currset, NULL, NULL, NULL) < 0) {
            if(errno == EINTR) {
                continue;
            }
            perror("Select error");
        }

        /* receiving from peer process */
        if (FD_ISSET(proc_sockfd, &currset)) {
            memset(buff, 0, MAXLINE); 
            memset(&proc_addr, 0, sizeof(proc_addr));
            

            DEBUG(printf ("\n====================================PROC_MESSAGE_RECEIVED====================================\n"));
            
            if((connfd = accept(proc_sockfd, (struct sockaddr *) &serv_addr, &socklen)) < 0) {
                if (errno == EINTR)
                    continue;

                perror("error in accept\n");
                return -1;
            }
            
            /* block on recvfrom. collect info in 
             * proc_addr and data in buff */
            if (Read (connfd, buff, sizeof (buff)) < 0) { 
                perror("Error in Read");
                return 0;
            }

            /* populate the send params from char sequence received from process */
            //send_params_t* sparams = get_send_params (buff);
            
            handle_proc_msg (proc_sockfd, arp_sockfd, buff, connfd);
            
            /* we need to listen on connfd */
            FD_SET (connfd, &set);
            maxfd = max(maxfd, connfd);
        }

        /* receiving on ethernet interface */
        if (FD_ISSET(arp_sockfd, &currset)) {
            memset(recv_buf, 0, ETH_FRAME_LEN); 
            memset(&arp_addr, 0, sizeof(arp_addr));

            DEBUG(printf ("\n====================================ETHERNET_MESSAGE_RECEIVED====================================\n"));
            if ((len = recvfrom(arp_sockfd, recv_buf, ETH_FRAME_LEN, 0, 
                            (struct sockaddr *)&arp_addr, &arpsize)) < 0) {
                perror("\nError in recvfrom");
                return 0;
            }

            if (handle_ethernet_msg (arp_sockfd, proc_sockfd, &arp_addr, recv_buf, src_mac) < 0)
                return 0;
        }

        if (FD_ISSET(connfd, &currset)) {


        }
    }

    return 1;
}
