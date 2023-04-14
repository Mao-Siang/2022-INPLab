#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string>
#include <cstring>
#include <vector>
#include <sys/select.h>
#include <ctime>
#include <sys/signal.h>
#include <queue>

#include "./name.cpp"
const int MAXLINE = 1024;
using pii = std::pair<int, int>;
std::vector<int> offline;

struct data{
    std::string name;
    std::string ip;

    data(){
        name = "";
        ip = "";
    }
};

std::string get_time(){
    time_t timer = time(0);
    tm* ltm = localtime(&timer);

    std::string cur_time = "";
    cur_time = std::to_string(1900 + ltm->tm_year) + "-" + std::to_string(1 + ltm->tm_mon) + "-" + std::to_string(ltm->tm_mday) + " "
        + std::to_string(ltm->tm_hour) + ":" + std::to_string(ltm->tm_min) + ":" + std::to_string(ltm->tm_sec);

    return cur_time + " ";
}

void Send(int __fd, const void* __buf, size_t __nbyte, int i){
    int n;
    if ((n = send(__fd, __buf, __nbyte, MSG_NOSIGNAL)) < 0){
        offline.push_back(i);
    }
    return;
}

void send_msg_to_all(int* client, std::string msg, int sender){

    for (int i = 0;i < FD_SETSIZE;i++){
        if (i == sender || client[i] == -1) continue;
        Send(client[i], msg.c_str(), msg.length(), i);
    }
}



int main(int argc, char* argv[])
{
    signal(SIGPIPE, SIG_IGN);

    int i, maxi, maxfd, listenfd, connfd, sockfd;
    int nready, client[FD_SETSIZE];
    std::vector<data> user(FD_SETSIZE, data());
    ssize_t n;
    fd_set rset, allset;
    char buf[MAXLINE];
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    std::string welcome_msg = "*** Welcome to the simple CHAT server\n";
    std::string online_msg1 = "*** Total ";
    std::string online_msg2 = " users online now. Your name is ";
    std::string msg = "";
    int online = 0;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(std::stoi(argv[1]));

    bind(listenfd, (const sockaddr*)&servaddr, sizeof(servaddr));

    listen(listenfd, 1024);

    maxfd = listenfd;
    maxi = -1;

    for (i = 0;i < FD_SETSIZE;i++) client[i] = -1;
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);


    for (;;) {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &rset)){
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (sockaddr*)&cliaddr, &clilen);
            online++;
            srand(time(NULL));
            std::string name = animals[rand() % 889];

            for (i = 0;i < FD_SETSIZE;i++){
                if (client[i] < 0) {
                    client[i] = connfd;
                    user[i].name = name;
                    char ip[32];
                    inet_ntop(AF_INET, (sockaddr*)&cliaddr.sin_addr, ip, 32);
                    user[i].ip = std::string(ip) + ":" + std::to_string(cliaddr.sin_port);
                    break;
                }

            }

            FD_SET(connfd, &allset);

            msg = get_time() + welcome_msg;
            Send(connfd, msg.c_str(), msg.length(), i);
            msg = get_time() + online_msg1 + std::to_string(online) + online_msg2 + "<" + name + ">\n";
            Send(connfd, msg.c_str(), msg.length(), i);
            msg = get_time() + "<" + name + ">" + " has just landed on the server.\n";
            send_msg_to_all(client, msg, i);
            printf("* client connected from %s \n", user[i].ip.c_str());

            // if (i == FD_SETSIZE) exit(0);

            if (connfd > maxfd) maxfd = connfd;
            if (i > maxi) maxi = i;
            if (--nready <= 0) continue;

        }

        for (i = 0;i <= maxi;i++){
            if ((sockfd = client[i]) < 0) continue;

            if (FD_ISSET(sockfd, &rset)){
                // printf("FD_ISSET\n");
                memset(buf, 0, sizeof(buf));
                if ((n = read(sockfd, buf, MAXLINE)) == 0){
                    // printf("someone disconnect\n");
                    msg = get_time() + "User <" + user[i].name + "> has left the server\n";
                    close(sockfd);
                    FD_CLR(i, &allset);
                    client[i] = -1;
                    send_msg_to_all(client, msg, i);
                    online--;
                    printf("* client %s disconnected\n", user[i].ip.c_str());

                }
                else{
                    std::string sbuf = buf;
                    if (sbuf[0] == '/'){
                        if (sbuf.substr(0, 5) == "/name") {
                            user[i].name = sbuf.substr(6, sbuf.size() - 7);
                            msg = get_time() + "*** Nickname changed to <" + sbuf.substr(6, sbuf.size() - 7) + ">\n";
                            Send(sockfd, msg.c_str(), msg.length(), i);
                            msg = "*** User <" + user[i].name + "> renamed to <" + sbuf.substr(6, sbuf.size() - 7) + ">\n";
                            send_msg_to_all(client, msg, i);
                        }
                        else if (sbuf.substr(0, 4) == "/who"){
                            Send(sockfd, "--------------------------------------------------\n", 52, i);
                            for (int j = 0;j <= maxi;j++){
                                if (client[j] < 0) continue;
                                msg = ((client[j] == sockfd) ? "* " : "  ") + user[j].name;
                                for (int k = 0;k < 15 - user[j].name.length();k++) msg += " ";
                                msg += (user[j].ip + '\n');
                                Send(sockfd, msg.c_str(), msg.length(), i);
                            }
                            Send(sockfd, "--------------------------------------------------\n", 52, i);
                        }
                        else{
                            int pos = sbuf.find(" ");
                            msg = get_time() + "*** Unknown or incomplete command " + sbuf.substr(0, pos + 1) + '\n';
                            Send(sockfd, msg.c_str(), msg.length(), i);
                        }
                    }
                    else{
                        msg = get_time() + "<" + user[i].name + "> " + buf;
                        send_msg_to_all(client, msg, i);
                    }


                }
                // printf("nready: %d\n", nready);
                if (--nready <= 0) break;
            }
        }
    }

    return 0;
}