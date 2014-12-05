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

    arp_frame_t *arp_frame     = (arp_frame_t *)calloc(1, sizeof(arp_frame_t));

    arp_frame->hard_type    = htonl (hard_type);
    arp_frame->prot_type    = htonl (prot_type);
    arp_frame->hard_size    = htonl (HW_ADDR_LEN);
    arp_frame->prot_size    = htonl (sizeof (prot_type));
    arp_frame->op           = htonl (op);

    DEBUG (printf ("op: %d\n", op));
    strncpy (arp_frame->src_mac, src_mac, HW_ADDR_LEN);
    strncpy (arp_frame->src_ip, src_ip, IP_LEN);
    strncpy (arp_frame->dest_mac, dest_mac, HW_ADDR_LEN);
    strncpy (arp_frame->dest_ip, dest_ip, IP_LEN);

    return arp_frame;
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

/* file the cache table entry */
int
insert_c_entry (arp_frame_t *recvd_frame, c_entry_t **c_entry,
        int intf_n) {
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
    cache_table_head->prev = (*c_entry);
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

    arp_frame->hard_type    = ntohl (arp_frame->hard_type);
    arp_frame->prot_type    = ntohl (arp_frame->prot_type);
    arp_frame->hard_size    = ntohl (arp_frame->hard_size);
    arp_frame->prot_size    = ntohl (arp_frame->prot_size);
    arp_frame->op           = ntohl (arp_frame->op);
    
    return 0;
}

/* broadcast the rreq packet received from this proc*/
int 
send_arp_req_broadcast (int sockfd, char *dest_ip, char *src_ip) {
    assert(dest_ip);
    assert(src_ip);
    //assert(payload);

    struct hwa_info *curr = Get_hw_struct_head(); 
    char if_name[MAXLINE];
    arp_frame_t *arpframe;

    char src_vmname[INET_ADDRSTRLEN], dst_vmname[INET_ADDRSTRLEN];

    strcpy(src_vmname, get_name_ip(src_ip));
    strcpy(dst_vmname, get_name_ip(dest_ip));
    assert(src_vmname);
    assert(dst_vmname);

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
        }
    }
    return 0;
}

/* Handle ARP Requests received from peer message. */
int 
handle_proc_msg (int proc_sockfd, int arp_sockfd) {
    
    char *dest_ip, *src_ip;

    src_ip = get_self_ip ();
    dest_ip = get_self_ip ();

    if (send_arp_req_broadcast (arp_sockfd, dest_ip, src_ip) < 0) {
        printf ("Error sending ARP REQ\n");
        return -1;
    }
    return 1;
}

/* Handle messages received over ethernet. */
int 
handle_ethernet_msg (int proc_connfd, int arp_sockfd, struct sockaddr_ll *arp_addr,
                        void *recv_buff, char *src_mac) {
    struct sockaddr_un arp_proc_addr;
    char str_seq[MAXLINE], temp[MAXLINE];
    arp_frame_t *rcvd_frame = (arp_frame_t *) recv_buff;
    int n;
    socklen_t socklen = sizeof (arp_proc_addr);
    
    DEBUG (printf ("Received frame hdr protocol ID: %d\n", arp_addr->sll_protocol));
    DEBUG (printf ("Received frame hdr protocol ID: %u\n", arp_addr->sll_pkttype));
    DEBUG (printf ("Received frame pckt protocol ID: %d\n", rcvd_frame->prot_type));
    DEBUG (printf ("Received frame op: %d\n", rcvd_frame->op));
    DEBUG (printf ("Received frame src_mac: %d\n", rcvd_frame->src_mac));
    DEBUG (printf ("Received frame dest_mac: %d\n", rcvd_frame->dest_mac));
    print_mac (rcvd_frame->src_mac);
    print_mac (rcvd_frame->dest_mac);

    DEBUG (printf ("Received frame src_ip: %d\n", rcvd_frame->src_ip));
    
    convert_net_host_order (rcvd_frame);

    DEBUG (printf ("Received frame hdr protocol ID: %d\n", arp_addr->sll_protocol));
    DEBUG (printf ("Received frame hdr protocol ID: %u\n", arp_addr->sll_pkttype));
    DEBUG (printf ("Received frame pckt protocol ID: %d\n", rcvd_frame->prot_type));
    DEBUG (printf ("Received frame op: %d\n", rcvd_frame->op));
  
    switch (rcvd_frame->op) {
        case __ARPREQ: {

            printf ("ARP Request received.\n");
            break;
        }
        case __ARPREP: {
            printf ("ARP Response received.\n");
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
    
    return 1;
}

int main (int argc, const char *argv[]) {

    int proc_sockfd, arp_sockfd, socklen, len, proc_connfd;
    struct sockaddr_un serv_addr, proc_addr, resp_addr;
    struct sockaddr_ll arp_addr;
    fd_set set, currset;
    char* src_mac;
    char buff[MAXLINE];

    /* initializations */
    socklen        = sizeof(struct sockaddr_un);
    void *recv_buf = malloc(ETH_FRAME_LEN); 
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

    /* TESTING */
    handle_proc_msg (proc_sockfd, arp_sockfd);
    DEBUG (printf ("ARP REQ sent!\n"));

    /* Monitor both sockets. */
    while (1) {
        currset = set;
        if (select(max(proc_sockfd, arp_sockfd)+1, 
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
            
            Accept (proc_connfd, (struct sockaddr *) &serv_addr, &socklen);
            
            /* block on recvfrom. collect info in 
             * proc_addr and data in buff */
            if (Read (proc_connfd, buff, sizeof (buff)) < 0) { 
                perror("Error in Read");
                return 0;
            }

            /* populate the send params from char sequence received from process */
            //send_params_t* sparams = get_send_params (buff);
            
            handle_proc_msg (proc_connfd, arp_sockfd);
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
    }

    return 1;
}
