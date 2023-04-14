#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <cstring>
#include <string>
#include <vector>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/wait.h>

int cmdsockfd, sockfd, sockfd2, sockfd3, sockfd4, sockfd5;
struct sockaddr_in  servaddr, servaddr2, servaddr3, servaddr4, servaddr5, servaddr6;

void sighandler(int s) {
    // send(cmdsockfd, "/report\n", 8, 0);
    exit(0);
}

void sig_term(int s){
    send(cmdsockfd, "/report\n", 8, 0);
    char str[64];
    memset(str, 0, sizeof(str));
    read(cmdsockfd, str, 64);
    std::string buf = str;
    while (buf.back() != '\n') buf.pop_back();
    printf("%s", str);
    exit(0);
}

void sig_child(int s)
{
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0){
    }
    exit(0);
}

int main(int argc, char** argv)
{
    signal(SIGPIPE, sighandler);
    signal(SIGTERM, sig_term);
    signal(SIGCHLD, sig_child);

    pid_t childpid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
    sockfd3 = socket(AF_INET, SOCK_STREAM, 0);
    sockfd4 = socket(AF_INET, SOCK_STREAM, 0);
    sockfd5 = socket(AF_INET, SOCK_STREAM, 0);
    cmdsockfd = socket(AF_INET, SOCK_STREAM, 0);


    // cmd port
    bzero(&servaddr2, sizeof(servaddr2));
    servaddr2.sin_family = AF_INET;
    servaddr2.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &servaddr2.sin_addr);

    // sink port
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]) + 1);
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    bzero(&servaddr3, sizeof(servaddr3));
    servaddr3.sin_family = AF_INET;
    servaddr3.sin_port = htons(atoi(argv[2]) + 1);
    inet_pton(AF_INET, argv[1], &servaddr3.sin_addr);

    bzero(&servaddr4, sizeof(servaddr4));
    servaddr4.sin_family = AF_INET;
    servaddr4.sin_port = htons(atoi(argv[2]) + 1);
    inet_pton(AF_INET, argv[1], &servaddr4.sin_addr);

    bzero(&servaddr5, sizeof(servaddr5));
    servaddr5.sin_family = AF_INET;
    servaddr5.sin_port = htons(atoi(argv[2]) + 1);
    inet_pton(AF_INET, argv[1], &servaddr5.sin_addr);

    bzero(&servaddr6, sizeof(servaddr6));
    servaddr6.sin_family = AF_INET;
    servaddr6.sin_port = htons(atoi(argv[2]) + 1);
    inet_pton(AF_INET, argv[1], &servaddr6.sin_addr);
    // send
    connect(cmdsockfd, (const sockaddr*)&servaddr2, sizeof(servaddr2));
    connect(sockfd, (const sockaddr*)&servaddr, sizeof(servaddr));
    connect(sockfd2, (const sockaddr*)&servaddr3, sizeof(servaddr3));
    connect(sockfd3, (const sockaddr*)&servaddr4, sizeof(servaddr4));
    connect(sockfd4, (const sockaddr*)&servaddr5, sizeof(servaddr5));
    connect(sockfd5, (const sockaddr*)&servaddr6, sizeof(servaddr6));

    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    setsockopt(sockfd2, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    setsockopt(sockfd3, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    setsockopt(sockfd4, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    setsockopt(sockfd5, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    // int bufsize = 1000000;
    // setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(int));

    char buf[65535];
    memset(buf, '1', sizeof(buf));
    int s = send(cmdsockfd, "/reset\n", 7, 0);

    for (int i = 0;i < 5;i++){
        if ((childpid = fork()) == 0){
            for (;;)
            {
                int n = send(sockfd, buf, sizeof(buf), 0);
                send(sockfd2, buf, sizeof(buf), 0);
                send(sockfd3, buf, sizeof(buf), 0);
                send(sockfd4, buf, sizeof(buf), 0);
                send(sockfd5, buf, sizeof(buf), 0);

                if (n == -1) printf("g");
            }
        }
    }



    int n;
    char str[1024];
    for (;;){
        memset(str, 0, sizeof(str));
        if ((n = read(cmdsockfd, &str, 1024)) < 0){
            close(cmdsockfd);
            break;
        }
        else{
            printf("%s", str);
        }
    }

    exit(0);
}