#include "tour.h"
#include <stdio.h>
#include <assert.h>

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
    t_frame->index = 0;
    
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

/* handle the received tour packet */
int
handle_tour (tour_frame_t *t_frame, int rt_sockfd) {

    /* check if this is the final node */
    if (t_frame->index + 1 == t_frame->size) {
        
        printf ("This is the last node of the tour!\n");
        return 1;
    }

    /* if this is an intermediate node */
    
    printf ("This is an intermediate node of the tour!\n");
    
    t_frame->index += 1;
    udp_write (t_frame, sizeof (tour_frame_t), t_frame->payload[t_frame->index], 
                    rt_sockfd);
    
    return 1;
}

/* start the tour */
int 
start_tour (tour_frame_t *t_frame, int rt_sockfd, int argc, char *argv[]) {

    fill_t_frame_payload (t_frame, argc, argv); 
    
    /* start the tour; send packet to the first node in the tour */
    udp_write (t_frame, sizeof (tour_frame_t), t_frame->payload[1], rt_sockfd);
    printf ("packet sent!\n");

    return 1;
}

int main (int argc, char *argv[]) {
    int pf_sockfd, rt_sockfd, len, one = 1;
    socklen_t rtsize = sizeof (struct sockaddr_in);
    const int *val = &one;
    fd_set set, currset;
    tour_frame_t *t_frame = calloc (1, sizeof (tour_frame_t));
    char rt_buf[sizeof (tour_frame_t)]; 
    
    struct sockaddr_in rt_addr;

    /* Get the new PF Packet socket */
    pf_sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
  
    /* The rt socket */
    rt_sockfd = Socket(AF_INET, SOCK_RAW, IPPROTO_AND);
    
    if(setsockopt(rt_sockfd, IPPROTO_IP, IP_HDRINCL, val, sizeof(1)) < 0)
    {
        perror("setsockopt() error");
        exit(-1);
    }
  
    /* start the tour if the user has provided the tour. */
    if (argc > 0) {
        start_tour (t_frame, rt_sockfd, argc, argv);
    }

    FD_ZERO(&set);
    FD_SET(rt_sockfd, &set);
    //FD_SET(arp_sockfd, &set);

    while (1) {
        currset = set;
        if (select(max(rt_sockfd, 1)+1, 
                    &currset, NULL, NULL, NULL) < 0) {
            if(errno == EINTR) {
                continue;
            }
            perror("Select error");
        }
        
        printf ("Packet Received!\n");

        /* receiving from another tour process */
        if (FD_ISSET(rt_sockfd, &currset)) {
            printf ("Packet received on route socket");
        
            if ((len = recvfrom (rt_sockfd, rt_buf, ETH_FRAME_LEN, 0, 
                            (struct sockaddr *) &rt_addr, &rtsize)) < 0) {
                perror("\nError in recvfrom");
                return 0;
            }
            
            t_frame = (tour_frame_t *) rt_buf;
            
            if (ntohs(t_frame->ip_hdr.ip_id) != HDR_ID) {
                continue;
            }
            DEBUG (print_tour (t_frame));
            handle_tour (t_frame, rt_sockfd);
        }

    }
    return 0;
}