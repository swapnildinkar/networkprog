#include	"unp.h"
#include	<time.h>
#include    <pthread.h>

void * time_srv(void * confd){
    int n;
    char				buff[MAXLINE];
    fd_set              rset;
    time_t				ticks;
    int connfd = *(int *)confd;
    struct timeval timeout;
    int maxfd = connfd+1;
    FD_ZERO(&rset);
    printf("--------------------------------------------------\n");
    printf("Server Started.\n");
    printf("--------------------------------------------------\n");
    for ( ; ; ) {
        FD_SET(connfd, &rset);
        if(select(maxfd, &rset, NULL, NULL, &timeout) > 0){
            Close(connfd);
            break;
        }
        else {
            ticks = time(NULL);
            snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
            Write(connfd, buff, strlen(buff));
        }
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
    }
    
    printf("Time server thread closed.\n");
    return(NULL);
}

void * echo_srv(void * confd){
    int n;
    char				buff[MAXLINE];
    fd_set              rset;
    int connfd = *(int *)confd;
    int maxfd = connfd+1;
    
    
    while(1){
        FD_SET(connfd, &rset);
        if((n = Readline(connfd, buff, sizeof(buff))) > 0){
            Writen(connfd, buff, n);
        }
        else if(n==0){
            Close(connfd);
            break;
        }
    }
    printf("Echo server thread closed.\n");
    return(NULL);
}

int init_ports(int portno){
    int					listenfd, connfd, maxfd;
    struct sockaddr_in	servaddr;
    
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(portno);
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
        err_sys("setsockopt(SO_REUSEADDR) failed");
    Bind(listenfd, (SA *) &servaddr, sizeof(servaddr));
    Listen(listenfd, LISTENQ);
    return listenfd;
}

int
main(int argc, char **argv)
{
    int					timelistenfd, echolistenfd, connfd;
    fd_set              rset;
    timelistenfd = init_ports(1300);
    echolistenfd = init_ports(1301);
    int maxfd = max(timelistenfd, echolistenfd) + 1;
    for(;;){
        
        FD_SET( timelistenfd, &rset);
        FD_SET( echolistenfd, &rset);
        select(maxfd, &rset, NULL, NULL, NULL);
        if(FD_ISSET(timelistenfd, &rset)){
            printf("Time server thread created.\n");
            pthread_t      tid;  // thread ID
            pthread_attr_t attr; // thread attribute
            
            // set thread detachstate attribute to DETACHED
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            
            connfd = Accept(timelistenfd, (SA *) NULL, NULL);
            // create the thread 
            pthread_create(&tid, &attr, &time_srv, &connfd);
        }
        else{
            printf("Echo server thread created.\n");
            pthread_t      tid;  // thread ID
            pthread_attr_t attr; // thread attribute
            
            // set thread detachstate attribute to DETACHED
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            
            connfd = Accept(echolistenfd, (SA *) NULL, NULL);
            // create the thread
            pthread_create(&tid, &attr, &echo_srv, &connfd);
        }
    }
}
