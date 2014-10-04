#include<stdio.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include	"unp.h"


#define SIZE 1024


int main(int argc, char *argv[])
{
    printf("--------------------------------------------------\n");
    char choice[4];
    int pid, i, n;
    int nread, status;
    int pfd[2];
    struct hostent *he;
    fd_set stdin_fd;
    char buf[SIZE];
    struct in_addr **addr_list;
    if ((he = gethostbyname(argv[1])) == NULL) {  // get the host info
        herror("gethostbyname");
        return 2;
    }
    
    printf("Official name is: %s\n", he->h_name);
    addr_list = (struct in_addr **)he->h_addr_list;
    for(i = 0; addr_list[i] != NULL; i++) {
        printf("%s \n", inet_ntoa(*addr_list[i]));
    }
    
    FD_ZERO(&stdin_fd);
    while(1){
        printf("\n--------------------------------------------------\n");
        printf("What server do you want to connect to?\n");
        scanf("%4s",choice);
        printf("\n--------------------------------------------------\n");
        if(strcmp(choice, "time") == 0)
        {
            if (pipe(pfd) == -1)
            {
                perror("pipe failed");
                return(0);
            }
            
            char temp[5];
            sprintf(temp, "%d", pfd[1]);
//            printf("\n pfd[1]: %d",pfd[1]);
            pid = fork();
            
            if(pid < 0)
            {
                printf("Fork failed.\n");
            }
            else if(pid == 0){
                close(pfd[0]);
                // child process
//                strcpy(buf, "hello...\n");
//                dup2(pfd[1], STDOUT_FILENO);
//                printf("%s",temp);
                /* include null terminator in write */
                if( (execlp("xterm", "xterm", "-e", "./time_cli", inet_ntoa(*addr_list[0]), temp, (char *) 0)) < 0){
                    printf("Error connecting to time server.\n");
                    return(0);
                }
                else{
                }
                
                close(pfd[1]);
                
                printf("\n--------------------------------------------------\n");
                return(0);
            }
            else{
                // parent process
                    
                close(pfd[1]);
                while(1){
                    FD_SET(fileno(stdin), &stdin_fd);
                    FD_SET(pfd[0], &stdin_fd);
                    int maxfd = max(fileno(stdin), pfd[0])+1;
                    
                    select(maxfd, &stdin_fd, NULL,NULL,NULL);
                    if(FD_ISSET(pfd[0], &stdin_fd)){
                        if((n = read(pfd[0], buf, SIZE)) != 0) {
                            buf[n] = '\0';
                            printf("child returned: %s\n", buf);
                        }
                        wait(NULL);
                        break;
                    }
                }
                close(pfd[0]);
                
                printf("\n--------------------------------------------------\n");
            }
        }
        else if(strcmp(choice, "echo") == 0)
        {
            
            if (pipe(pfd) == -1)
            {
                perror("pipe failed");
                return(0);
            }
            
            char temp[5];
            sprintf(temp, "%d", pfd[1]);
            pid = fork();
            
            if(pid < 0)
            {
                printf("Fork failed.\n");
            }
            else if(pid == 0){
                close(pfd[0]);
                if( (execlp("xterm", "xterm", "-e", "./echo_cli", inet_ntoa(*addr_list[0]), temp, (char *) 0)) < 0){
                    printf("Error connecting to time server.\n");
                    return(0);
                }
                else{
                }
                
                close(pfd[1]);
                return(0);
            }
            else{
                // parent process
                
                close(pfd[1]);
                
                while(1){
                    FD_SET(fileno(stdin), &stdin_fd);
                    FD_SET(pfd[0], &stdin_fd);
                    int maxfd = max(fileno(stdin), pfd[0])+1;
                    
                    select(maxfd, &stdin_fd, NULL,NULL,NULL);
                    if(FD_ISSET(pfd[0], &stdin_fd)){
                        if((n = read(pfd[0], buf, SIZE)) != 0) {
                            buf[n] = '\0';
                            printf("Child returned: %s\n", buf);
                        }
                        wait(NULL);
                        break;
                    }
                }
                close(pfd[0]);
                
                printf("\n--------------------------------------------------\n");
            }
        }
        else if(strcmp(choice, "quit") == 0)
        {
            
            printf("\n--------------------------------------------------\n");
            return(1);
        }
        else{
            
            printf("\n--------------------------------------------------\n");
            printf("Incorrect choice.\n");
            printf("\n--------------------------------------------------\n");
        }
    }
    return(0);
}
