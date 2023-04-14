#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/signal.h>

const int MAXLINE = 1000000;
using pii = std::pair<int, int>;


void sig_sigpipe(int s) {}

std::string getcurtime(timeval* tv){
    gettimeofday(tv, NULL);
    return std::to_string(tv->tv_sec) + "." + std::to_string(tv->tv_usec);
}

int main(int argc, char* argv[])
{

    int i, maxi, maxfd, listenfd, listenfd2, cmdconnfd, connfd, sockfd;
    int nready, client[FD_SETSIZE];
    ssize_t n;
    fd_set rset, allset;
    char buf[MAXLINE];
    socklen_t clilen, clilen2;
    struct sockaddr_in cliaddr, cliaddr2, cliaddr3, servaddr, servaddr2;
    std::string msg = "";
    long long int lasttime = 0;
    long long int counter = 30;
    int online = 0;

    timeval tv;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    listenfd2 = socket(AF_INET, SOCK_STREAM, 0);

    // listen cmd
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(std::stoi(argv[1]));
    // listen sink
    bzero(&servaddr2, sizeof(servaddr2));
    servaddr2.sin_family = AF_INET;
    servaddr2.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr2.sin_port = htons(std::stoi(argv[1]) + 1);

    bind(listenfd, (const sockaddr*)&servaddr, sizeof(servaddr));
    bind(listenfd2, (const sockaddr*)&servaddr2, sizeof(servaddr2));

    int reuse = 1;
    setsockopt(listenfd2, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    // int bufsize = 1000000;
    // setsockopt(listenfd2, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(int));

    listen(listenfd, 1024);
    listen(listenfd2, 1024);

    maxfd = std::max(listenfd, listenfd2);
    maxi = -1;

    for (i = 0;i < FD_SETSIZE;i++){
        client[i] = -1;
    }
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    FD_SET(listenfd2, &allset);

    for (;;) {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &rset)){
            clilen = sizeof(cliaddr);
            cmdconnfd = accept(listenfd, (sockaddr*)&cliaddr, &clilen);

            for (i = 0;i < FD_SETSIZE;i++){
                if (client[i] < 0) {
                    client[i] = cmdconnfd;
                    break;
                }
            }

            FD_SET(cmdconnfd, &allset);

            // if (i == FD_SETSIZE) exit(0);

            if (cmdconnfd > maxfd) maxfd = cmdconnfd;
            if (i > maxi) maxi = i;
            if (--nready <= 0) continue;

        }

        if (FD_ISSET(listenfd2, &rset)){
            clilen2 = sizeof(cliaddr2);
            connfd = accept(listenfd2, (sockaddr*)&cliaddr2, &clilen2);

            for (i = 0;i < FD_SETSIZE;i++){
                if (client[i] < 0) {
                    client[i] = connfd;
                    online++;
                    break;
                }
            }

            FD_SET(connfd, &allset);

            if (connfd > maxfd) maxfd = connfd;
            if (i > maxi) maxi = i;
            if (--nready <= 0) continue;
        }

        for (i = 0;i <= maxi;i++){
            if ((sockfd = client[i]) < 0) continue;

            if (FD_ISSET(sockfd, &rset)){
                memset(buf, 0, sizeof(buf));
                if ((n = read(sockfd, buf, MAXLINE)) == 0){
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                    if (sockfd != cmdconnfd) online--;
                }
                else{
                    std::string sbuf = buf;

                    if (sockfd == cmdconnfd){
                        sbuf.pop_back();
                        if (sbuf == "/reset"){
                            msg = getcurtime(&tv) + " RESET " + std::to_string(counter) + '\n';
                            counter = 0;
                            lasttime = tv.tv_sec * 1000000 + tv.tv_usec;
                            send(sockfd, msg.c_str(), msg.size(), 0);
                        }
                        else if (sbuf == "/ping"){
                            msg = getcurtime(&tv) + " PONG\n";
                            send(sockfd, msg.c_str(), msg.size(), 0);
                        }
                        else if (sbuf == "/report"){
                            // printf("report entered\n");
                            gettimeofday(&tv, NULL);
                            long long int curtime = tv.tv_sec * 1000000 + tv.tv_usec;
                            double speed = 8.0 * counter / (curtime - lasttime);
                            double time = (curtime - lasttime) / 1000000;
                            msg = getcurtime(&tv) + " REPORT " + std::to_string(counter) + " " + std::to_string(time) + "s " + std::to_string(speed) + "Mbps\n";
                            int s = send(sockfd, msg.c_str(), msg.size(), 0);
                            // printf("%d\n", s);
                        }
                        else if (sbuf == "/clients"){
                            msg = getcurtime(&tv) + " CLIENTS " + std::to_string(online) + '\n';
                            send(sockfd, msg.c_str(), msg.size(), 0);
                        }
                    }
                    else{
                        counter += sbuf.size();
                        // printf("%ld\n", sbuf.size());
                    }

                }

                if (--nready <= 0) break;
            }


        }

    }

    return 0;
}