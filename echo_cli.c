#include	"unp.h"

int pfd;

void push_to_parent(char* msg){
    write(pfd, msg, strlen(msg));
    exit(EXIT_SUCCESS);
}

void sig_child(int signo){
    push_to_parent("Exit");
}

void
echo_cli(FILE *fp, int sockfd)
{
    int			maxfdp1, stdineof;
    fd_set		rset;
    char		buf[MAXLINE];
    int		n;
    
    stdineof = 0;
    FD_ZERO(&rset);
    for ( ; ; ) {
        if (stdineof == 0)
            FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);
        maxfdp1 = max(fileno(fp), sockfd) + 1;
        Select(maxfdp1, &rset, NULL, NULL, NULL);
        
        if (FD_ISSET(sockfd, &rset)) {	/* socket is readable */
            if ( (n = Read(sockfd, buf, MAXLINE)) == 0) {
                if (stdineof == 1)
                    return;		/* normal termination */
                else{
                    push_to_parent("Server terminated prematurely");
                }
            }
            
            Write(fileno(stdout), buf, n);
        }
        
        if (FD_ISSET(fileno(fp), &rset)) {
            if ( (n = Read(fileno(fp), buf, MAXLINE)) == 0) {
                stdineof = 1;
                Shutdown(sockfd, SHUT_WR);
                FD_CLR(fileno(fp), &rset);
                continue;
            }
            
            Writen(sockfd, buf, n);
        }
    }
}

int
main(int argc, char **argv)
{
    int					sockfd;
    struct sockaddr_in	servaddr;
    
    printf("--------------------------------------------------\n");
    printf("Connected to Echo Client.\n");
    printf("--------------------------------------------------\n");
    pfd = atoi(argv[2]);
    signal(SIGINT, sig_child);
    if (argc != 3)
        push_to_parent("usage: tcpcli <IPaddress>");
    
    sockfd = Socket(AF_INET, SOCK_STREAM, 0);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(1301);
    Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    
    Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));
    
    echo_cli(stdin, sockfd);		/* do it all */
    
}
