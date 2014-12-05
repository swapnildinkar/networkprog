#include	"unp.h"

int pfd;

void push_to_parent(char* msg){
    write(pfd, msg, strlen(msg));
    exit(EXIT_SUCCESS);
}

void sig_child(int signo){
    push_to_parent("Exit");
}

int main(int argc, char **argv)
{
    
    printf("--------------------------------------------------\n");
    printf("Connected to Time Server.\n");
    printf("--------------------------------------------------\n");
    int					sockfd, n;
    char				recvline[MAXLINE + 1];
    struct sockaddr_in	servaddr;
    
    if (argc != 3)
        push_to_parent("usage: a.out <IPaddress> <FD>");
    pfd = atoi(argv[2]);
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        push_to_parent("Socket error.");
    
    signal(SIGINT, sig_child);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(1300);	/* daytime server */
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
        push_to_parent("inet_pton error");
    
    if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0){
        push_to_parent("connect error");
    }
    
    while ( (n = read(sockfd, recvline, MAXLINE)) > 0) {
        recvline[n] = 0;	/* null terminate */
        if (fputs(recvline, stdout) == EOF)
            push_to_parent("fputs error");
    }
    if (n < 0)
        push_to_parent("read error");
    push_to_parent("Server terminated prematurely.\n");
}
