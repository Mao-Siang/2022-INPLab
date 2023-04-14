#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <vector>
#include <sys/select.h>
#include <ctime>
#include <sys/signal.h>


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
    time_t timer = time(NULL);
    tm* ltm = localtime(&timer);

    std::string cur_time = "";
    cur_time = std::to_string(1900 + ltm->tm_year) + "-" + std::to_string(1 + ltm->tm_mon) + "-" + std::to_string(ltm->tm_mday) + " "
        + std::to_string(ltm->tm_hour) + ":" + std::to_string(ltm->tm_min) + ":" + std::to_string(ltm->tm_sec);
    return cur_time + " ";
}

void Send(int __fd, const void* __buf, size_t __nbyte, int i, int* client){
    if (client[i] < 0) return;
    int n;

    if ((n = send(__fd, __buf, __nbyte, 0)) <= 0){
        printf("send error:%s\n", strerror(errno));
        client[i] = -1;
        close(__fd);
        offline.emplace_back(i);
    }
    return;
}

void send_msg_to_all(int* client, std::string msg, int sender){
    errno = 0;
    for (int i = 0;i < FD_SETSIZE;i++){
        if (i == sender || client[i] == -1) continue;
        Send(client[i], msg.c_str(), msg.length(), i, client);
    }
}

void sig_sigpipe(int s) {}



int main(int argc, char* argv[])
{
    signal(SIGPIPE, sig_sigpipe);
    srand(clock());

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
        errno = 0;
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &rset)){
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (sockaddr*)&cliaddr, &clilen);
            online++;

            std::string name = animals[rand() % 889];

            for (i = 0;i < FD_SETSIZE;i++){
                if (client[i] < 0) {
                    client[i] = connfd;
                    user[i].name = name;
                    char ip[64];
                    inet_ntop(AF_INET, (sockaddr*)&cliaddr.sin_addr, ip, 64);
                    user[i].ip = std::string(ip) + ":" + std::to_string(cliaddr.sin_port);
                    break;
                }
            }

            FD_SET(connfd, &allset);

            msg = get_time() + welcome_msg;

            Send(connfd, msg.c_str(), msg.length(), i, client);
            msg = get_time() + online_msg1 + std::to_string(online) + online_msg2 + "<" + name + ">\n";
            Send(connfd, msg.c_str(), msg.length(), i, client);
            msg = get_time() + "<" + name + ">" + " has just landed on the server.\n";
            send_msg_to_all(client, msg, i);
            printf("* client connected from %s \n", user[i].ip.c_str());

            if (i == FD_SETSIZE) exit(0);

            if (connfd > maxfd) maxfd = connfd;
            if (i > maxi) maxi = i;
            if (--nready <= 0) continue;

        }



        for (i = 0;i <= maxi;i++){
            if ((sockfd = client[i]) < 0) continue;

            if (FD_ISSET(sockfd, &rset)){
                memset(buf, 0, sizeof(buf));
                if ((n = read(sockfd, buf, MAXLINE)) <= 0){
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                    offline.emplace_back(i);
                }
                else{
                    std::string sbuf = buf;
                    if (sbuf[0] == '/'){
                        if (sbuf.substr(0, 5) == "/name") {
                            msg = "*** User <" + user[i].name + "> renamed to <" + sbuf.substr(6, sbuf.size() - 7) + ">\n";
                            send_msg_to_all(client, msg, i);
                            user[i].name = sbuf.substr(6, sbuf.size() - 7);
                            msg = get_time() + "*** Nickname changed to <" + sbuf.substr(6, sbuf.size() - 7) + ">\n";
                            Send(sockfd, msg.c_str(), msg.length(), i, client);
                        }
                        else if (sbuf.substr(0, 4) == "/who"){
                            Send(sockfd, "--------------------------------------------------\n", 52, i, client);
                            for (int j = 0;j <= maxi;j++){
                                if (client[j] < 0) continue;
                                msg = ((client[j] == sockfd) ? "* " : "  ") + user[j].name;
                                for (int k = 0;k < 15 - user[j].name.length();k++) msg += " ";
                                msg += (user[j].ip + '\n');
                                Send(sockfd, msg.c_str(), msg.length(), i, client);
                            }
                            Send(sockfd, "--------------------------------------------------\n", 52, i, client);
                        }
                        else{
                            int pos = sbuf.find(" ");
                            if (pos == -1) msg = get_time() + "*** Unknown or incomplete command " + sbuf + '\n';
                            else {
                                msg = get_time() + "*** Unknown or incomplete command " + sbuf.substr(0, pos + 1) + '\n';
                            }
                            Send(sockfd, msg.c_str(), msg.length(), i, client);
                        }
                    }
                    else{
                        msg = get_time() + "<" + user[i].name + "> " + buf;
                        send_msg_to_all(client, msg, i);
                    }
                }

                if (--nready <= 0) break;
            }
        }

        online -= offline.size();
        while (!offline.empty()){
            int j = offline.back();
            offline.pop_back();
            FD_CLR(client[j], &allset);
            printf("* client %s disconnected\n", user[j].ip.c_str());
            msg = get_time() + "User <" + user[j].name + "> has left the server\n";
            send_msg_to_all(client, msg, j);
        }
        // offline.clear();

        sleep(4); // Major Problem
    }

    return 0;
}