#include "tour.h"
#include <stdio.h>
#include <assert.h>

/* global variables */
int already_visited = 0;
int mc_first_msg_rcvd = 0;
/* function returns ip */
int 
get_ip (char *serv_vm, char *canon_ip) {

    assert(serv_vm);
    assert(canon_ip);

    struct hostent *he;
    struct inaddr **addr_list;

    if ((he = gethostbyname(serv_vm)) == NULL) {
        return -1;
    }

    addr_list = (struct in_addr **)he->h_addr_list;
    assert(addr_list);
    assert(addr_list[0]);

    strcpy(canon_ip, inet_ntoa(*(struct in_addr *)addr_list[0]));
    return 0;
}

/* calculate checksum of payload. */
unsigned short 
csum (unsigned short *buf, int nwords)
{      
    unsigned long sum;
    for(sum=0; nwords>0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

/* join the multicast group */
int
join_multicast_group (char *mc_ip_addr, int mc_port, int mc_sockfd) {
    
    assert (mc_ip_addr);
    
    struct sockaddr_in  mc;
    bzero (&mc, sizeof (mc));
    mc.sin_family = AF_INET;
    inet_pton (AF_INET, mc_ip_addr, &(mc.sin_addr));
    mc.sin_port = htons(mc_port);
    
    Bind(mc_sockfd, (SA *) &mc, sizeof(mc));
    Mcast_join (mc_sockfd, (const SA *) &mc, sizeof(mc), NULL, 0);

    return 1;
}

/* include udp_write */
void
udp_write (tour_frame_t *buf, int userlen, char *dest_ip, int rt_sockfd)
{
    struct ip           *ip;
    struct sockaddr_in  dest;
    char *self_ip = get_self_ip ();
    bzero (&dest, sizeof (struct sockaddr_in));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, dest_ip, &(dest.sin_addr));
    
    /* 4fill in and checksum UDP header */
    ip = (struct ip *) buf;

    /* 4fill in rest of IP header; */
    /* 4ip_output() calcuates & stores IP header checksum */
    ip->ip_v = IPVERSION;
    ip->ip_hl = sizeof(struct ip) >> 2;
    ip->ip_tos = 0;
#if defined(linux) || defined(__OpenBSD__)
    ip->ip_len = htons(userlen);    /* network byte order */
#else
    ip->ip_len = userlen;           /* host byte order */
#endif
    ip->ip_id = htons (HDR_ID);          /* let IP set this */
    ip->ip_off = 0;         /* frag offset, MF and DF flags */
    ip->ip_ttl = 1;
    ip->ip_p   = IPPROTO_AND;
    ip->ip_sum = csum ((unsigned short *) buf, sizeof (tour_frame_t));
    inet_pton (AF_INET, dest_ip, &ip->ip_dst);
    inet_pton (AF_INET, self_ip, &ip->ip_src);

    Sendto(rt_sockfd, buf, userlen, 0,(struct sockaddr *) &dest, sizeof (struct sockaddr_in));
}

/* fill the our frame with the tour IP Addrs. */
int
fill_t_frame_payload (tour_frame_t *t_frame, int argc, char *argv[]) {
    int i = 0;
    char ip_addr[MAXLINE];
    
    strcpy (t_frame->payload[0], get_self_ip ());
    
    /* Take input from command line. */
    while (argc > 1) {
        i++;
        get_ip (argv[i], ip_addr);
        
        strcpy (t_frame->payload[i], ip_addr);
        argc--;
    }
    
    t_frame->size = i+1;
    t_frame->index = 1;
    t_frame->mc_port = MC_PORT;
    strcpy (t_frame->mc_ip, MC_IP_ADDR);
    return 1;
}

/* print all the nodes in the tour. */
int
print_tour (tour_frame_t *t_frame) {
    int i = 0;
    for (i = 0;i < t_frame->size; i++) {
        printf ("%s\n", t_frame->payload[i]);
    }
    return 1;
}

/* broadcast on multicast group from final node */
int
send_to_multicast_group (int mc_sockfd, char *mc_ip, int mc_port, char *msg) {
    struct sockaddr_in mc;
    char mc_msg[MAXLINE];
    char *vm_name = get_name_ip (get_self_ip ());
    bzero (&mc, sizeof (struct sockaddr_in));
    mc.sin_family = AF_INET;
    inet_pton (AF_INET, mc_ip, &(mc.sin_addr));
    mc.sin_port = htons (mc_port);
    
    sprintf (mc_msg, "%s%s%s%s", "Node ", vm_name, ". Sending: ", msg);

    printf ("%s\n", mc_msg);
    Sendto(mc_sockfd, msg, strlen (msg), 0,(struct sockaddr *) &mc, 
                sizeof (struct sockaddr_in));

    return 1;
}

/* handle the received tour packet */
int
handle_tour (tour_frame_t *t_frame, int rt_sockfd, int mc_sockfd) {
    
    assert (t_frame);
    
    char mc_msg[MAXLINE];
    char *vm_name = get_name_ip (get_self_ip());
    assert (vm_name);
    t_frame->index += 1;
    
    /* check if this is the final node */
    if (t_frame->index == t_frame->size) {
    
        if (!already_visited) {
            join_multicast_group (t_frame->mc_ip, t_frame->mc_port, mc_sockfd);
        }
       
        sprintf (mc_msg, "%s%s%s", "<<<<< This is node ", vm_name, 
                            ".  Tour has ended.  Group members please identify yourselves. >>>>>");
        
        /* send message to the multicast group. */
        send_to_multicast_group (mc_sockfd, t_frame->mc_ip, t_frame->mc_port, mc_msg);
        already_visited = 0;
        return 1;
    }

    /* if this is an intermediate node */
    
    /* join the multicast group */
 
    if (!already_visited) {
        already_visited = 1;

        join_multicast_group (t_frame->mc_ip, t_frame->mc_port, mc_sockfd);
        DEBUG (printf ("Joined multicast group on ip addr: %s and port :%d\n",
                    t_frame->mc_ip, t_frame->mc_port));

        /* send the packet to the next node in the tour. */
        
        udp_write (t_frame, sizeof (tour_frame_t), t_frame->payload[t_frame->index], 
                rt_sockfd);
        
        return 1;
    }

    /* if the node is intermediate node, and has already been visited. */
    
    /* send the packet to the next node in the tour. */
    udp_write (t_frame, sizeof (tour_frame_t), t_frame->payload[t_frame->index], 
            rt_sockfd);
    
    return 1;
}

/* start the tour */
int 
start_tour (tour_frame_t *t_frame, int rt_sockfd, int argc, char *argv[], int mc_sockfd) {

    fill_t_frame_payload (t_frame, argc, argv); 
    
    join_multicast_group (t_frame->mc_ip, t_frame->mc_port, mc_sockfd);
    already_visited = 1; 

    /* start the tour; send packet to the first node in the tour */
    udp_write (t_frame, sizeof (tour_frame_t), t_frame->payload[1], rt_sockfd);
    printf ("packet sent!\n");

    return 1;
}

int
handle_mc_msg (int mc_sockfd, tour_frame_t *t_frame, char *mc_buf) {
  
    assert (t_frame);
    assert (mc_buf);

    char mc_msg[MAXLINE];

    printf ("Node %s. Received: %s\n", get_name_ip (get_self_ip ()), (char *) mc_buf);
     
    if (!mc_first_msg_rcvd) {
        sprintf (mc_msg, "%s%s%s", "<<<<< Node ", get_name_ip (get_self_ip()),
                ".  I am a member of the group. >>>>>");
        send_to_multicast_group (mc_sockfd, t_frame->mc_ip, t_frame->mc_port, 
                mc_msg);
        mc_first_msg_rcvd = 1;
    }
     
    return 1;
}

int main (int argc, char *argv[]) {
    int pf_sockfd, rt_sockfd, mc_sockfd, len, one = 1;
    socklen_t rtsize = sizeof (struct sockaddr_in);
    const int *val = &one;
    fd_set set, currset;
    struct timeval currtime;
    char curr_time[64], rt_buf[sizeof (tour_frame_t)], mc_buf[MAXLINE]; 
    char* vm_name;
    struct sockaddr_in rt_addr, mc_addr;
    struct tm *nowtm;
    time_t nowtime;
    
    tour_frame_t *t_frame = calloc (1, sizeof (tour_frame_t));
    gettimeofday(&currtime, NULL);
    
    nowtime = currtime.tv_sec;
    nowtm = localtime(&nowtime);
    strftime(curr_time, sizeof (curr_time), "%Y-%m-%d %H:%M:%S", nowtm);
    
    /* Get the new PF Packet socket */
    pf_sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
  
    /* The rt socket */
    rt_sockfd = Socket(AF_INET, SOCK_RAW, IPPROTO_AND);
    
    if(setsockopt(rt_sockfd, IPPROTO_IP, IP_HDRINCL, val, sizeof(1)) < 0)
    {
        perror("setsockopt() error");
        exit(-1);
    }
    
    /* get the multicast socket. */
    mc_sockfd = Socket(AF_INET, SOCK_DGRAM, 0); 

    /* start the tour if the user has provided the tour. */
    if (argc > 1) {
        start_tour (t_frame, rt_sockfd, argc, argv, mc_sockfd);
    }

    FD_ZERO(&set);
    FD_SET(rt_sockfd, &set);
    FD_SET(mc_sockfd, &set);
    
    struct sockaddr_in  dest;
    bzero (&dest, sizeof (struct sockaddr_in));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, "130.245.156.21", &(dest.sin_addr));

    struct hwaddr hw;
    bzero(&hw, sizeof(struct hwaddr));
    hw.sll_ifindex = 2;
    hw.sll_hatype = ARPHRD_ETHER;
    hw.sll_halen = ETH_ALEN; 

    areq((struct sockaddr *)&dest, sizeof(dest), &hw); 
    
    
#if 0
    while (1) {
        currset = set;
        if (select(max(rt_sockfd, mc_sockfd)+1, 
                    &currset, NULL, NULL, NULL) < 0) {
            if(errno == EINTR) {
                continue;
            }
            perror("Select error");
        }
        
        /* receiving from another tour process */
        if (FD_ISSET(rt_sockfd, &currset)) {

            //printf ("======================= RCVD ROUTE MSG ========================\n");
            if ((len = recvfrom (rt_sockfd, rt_buf, ETH_FRAME_LEN, 0, 
                            (struct sockaddr *) &rt_addr, &rtsize)) < 0) {
                perror("\nError in recvfrom");
                return 0;
            }
                    
            t_frame = (tour_frame_t *) rt_buf;
            
            if (ntohs(t_frame->ip_hdr.ip_id) != HDR_ID) {
                continue;
            }
            
            //DEBUG (print_tour (t_frame));
            vm_name = get_name_ip (t_frame->payload[(t_frame->index - 1)]);
            printf ("%s received source routing packet from %s\n", curr_time, vm_name);

            handle_tour (t_frame, rt_sockfd, mc_sockfd);
            //printf ("====================== HNDLD ROUTE MSG ========================\n");
        }

        /* receiving from the multicast group */
        if (FD_ISSET(mc_sockfd, &currset)) {
           
            memset (mc_buf, 0, MAXLINE);
            //printf ("======================= RCVD MCAST MSG ========================\n");
            if ((len = recvfrom (mc_sockfd, mc_buf, ETH_FRAME_LEN, 0, 
                            (struct sockaddr *) &mc_addr, &rtsize)) < 0) {
                perror("\nError in recvfrom");
                return 0;
            }

            handle_mc_msg (mc_sockfd, t_frame, mc_buf);
            
            //printf ("====================== HNDLD MCAST MSG ========================\n");
        }
    }
#endif
    return 0;

}
