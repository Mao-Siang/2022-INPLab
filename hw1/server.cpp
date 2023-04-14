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
#include <sstream>
#include <map>
#include <algorithm>
const int MAXLINE = 1024;
using std::string;
using std::vector;

void sig_sigpipe(int s) {}
struct usr{
    std::string user, host, srv, real, chnl, ip;
    usr(){
        user = "";
        host = "";
        srv = "";
        real = "";
        chnl = "";
        ip = "";
    }
};

struct ch{
    std::string topic;
    vector<int> memb;

    ch(){
        topic = "";
        memb.clear();
    }
};

int main(int argc, char* argv[])
{
    signal(SIGPIPE, sig_sigpipe);

    int i, maxi, maxfd, listenfd, connfd, sockfd;
    int nready, client[FD_SETSIZE];
    ssize_t n;
    fd_set rset, allset;
    char buf[MAXLINE];
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    vector<usr> user(FD_SETSIZE, usr());
    vector<string> nickname(FD_SETSIZE, "");
    std::map<string, ch> channel;
    string msg = "";
    string prefix = ":mircd ";

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

    int online = 0;
    for (;;) {

        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &rset)){
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (sockaddr*)&cliaddr, &clilen);
            online++;
            for (i = 0;i < FD_SETSIZE;i++){
                if (client[i] < 0) {
                    client[i] = connfd;
                    char ipaddr[64];
                    inet_ntop(AF_INET, (sockaddr*)&cliaddr.sin_addr, ipaddr, clilen);
                    user[i].ip = (ipaddr);
                    break;
                }
            }

            FD_SET(connfd, &allset);


            if (i == FD_SETSIZE) exit(0);

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
                    online--;
                }
                else{
                    string sbuf = buf;

                    printf("---------------------------\n");
                    printf("%s\n", sbuf.c_str());
                    // printf("cmd len: %ld \n", cmd.size());
                    printf("---------------------------\n");
                    int pos = sbuf.find(" ");

                    while (sbuf.back() == '\n' || sbuf.back() == '\r')
                        sbuf.pop_back();

                    string cmd = ((pos == -1) ? sbuf : (sbuf.substr(0, pos)));
                    // if (pos == -1) {
                    //     cmd.pop_back();
                    // }


                    if (cmd == "NICK"){

                        if (pos == -1) {
                            // no nickname given
                            msg = prefix + "431 " + nickname[sockfd] + " :No nickname given\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                        else
                        {
                            string nick = sbuf.substr(pos + 1);
                            printf("%s", nick.c_str());
                            if (std::find(nickname.begin(), nickname.end(), nick) != nickname.end()){
                                // nick collision
                                msg = prefix + "436 " + nick + " :Nickname collision KILL\n";
                                send(sockfd, msg.c_str(), msg.length(), 0);
                            }
                            else{
                                // change nickname
                                nickname[sockfd] = nick;
                                // motd
                                if (user[i].user != ""){
                                    msg = prefix + "001 " + nickname[sockfd] + " :Welcome to the minimized IRC daemon!\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "251 " + nickname[sockfd] + " :There are " + std::to_string(online) + " users and 0 invisible on 1 servers\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "375 " + nickname[sockfd] + " :- mircd Message of the day -\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-               @                    _ \n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-   ____  ___   _   _ _   ____.     | |\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-  /  _ `'_  \\ | | | '_/ /  __|  ___| |\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-  | | | | | | | | | |   | |    /  _  |\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-  | | | | | | | | | |   | |__  | |_| |\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-  |_| |_| |_| |_| |_|   \\____| \\___,_|\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "376 " + nickname[sockfd] + " :End of /MOTD command\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                }
                            }
                        }
                    }
                    else if (cmd == "USER"){
                        if (pos == -1){
                            msg = prefix + "461 " + nickname[sockfd] + " " + cmd + " :Not enough parameters\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                        else{
                            std::stringstream ss(sbuf.substr(pos + 1));
                            vector<string> param;
                            string s;
                            while (ss >> s){
                                param.push_back(s);
                            }
                            printf("%ld\n", param.size());
                            if (param.size() < 4){
                                msg = prefix + "461 " + nickname[sockfd] + " " + cmd + " :Not enough parameters\n";
                                send(sockfd, msg.c_str(), msg.length(), 0);
                            }
                            else{
                                user[i].user = param[0];
                                user[i].host = param[1];
                                user[i].srv = param[2];
                                user[i].real = param[3].substr(1);

                                // motd
                                if (nickname[sockfd] != ""){
                                    msg = prefix + "001 " + nickname[sockfd] + " :Welcome to the minimized IRC daemon!\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "251 " + nickname[sockfd] + " :There are " + std::to_string(online) + " users and 0 invisible on 1 servers\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "375 " + nickname[sockfd] + " :- mircd Message of the day -\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-               @                    _ \n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-   ____  ___   _   _ _   ____.     | |\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-  /  _ `'_  \\ | | | '_/ /  __|  ___| |\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-  | | | | | | | | | |   | |    /  _  |\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-  | | | | | | | | | |   | |__  | |_| |\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "372 " + nickname[sockfd] + " :-  |_| |_| |_| |_| |_|   \\____| \\___,_|\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                    msg = prefix + "376 " + nickname[sockfd] + " :End of /MOTD command\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                }
                            }
                        }
                    }
                    else if (cmd == "PING"){
                        if (pos == -1){
                            msg = prefix + "409 " + nickname[sockfd] + " :No origin specified\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                        else{
                            msg = "PONG " + sbuf.substr(pos + 1) + "\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                    }
                    else if (cmd == "LIST"){
                        if (pos == -1){
                            msg = prefix + "321 " + nickname[sockfd] + " Channel :Users  Name\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                            for (const auto p : channel){
                                msg = prefix + "322 " + nickname[sockfd] + " " + p.first + " :" + p.second.topic + '\n';
                                printf("msg: %s\n", msg.c_str());
                                send(sockfd, msg.c_str(), msg.length(), 0);
                            }
                            msg = prefix + "323 " + nickname[sockfd] + " :End of LIST\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                        else{
                            string ch_name = sbuf.substr(pos + 1);
                            ch_name.pop_back();
                            msg = prefix + "321 " + nickname[sockfd] + " Channel :Users  Name\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                            msg = prefix + "322 " + nickname[sockfd] + " " + ch_name + " :" + channel[ch_name].topic + '\n';
                            send(sockfd, msg.c_str(), msg.length(), 0);
                            msg = prefix + "323 " + nickname[sockfd] + " :End of LIST\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                    }
                    else if (cmd == "JOIN"){
                        if (pos == -1) {
                            msg = prefix + "461 " + nickname[sockfd] + " " + cmd + " :Not enough parameters\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                        else{
                            string ch_name = sbuf.substr(pos + 1);

                            printf("chnl: %s\n", ch_name.c_str());
                            user[i].chnl = ch_name;
                            printf("user chnl: %s \n", user[i].chnl.c_str());
                            channel[ch_name].memb.push_back(sockfd);

                            // JOIN message
                            msg = ":" + nickname[sockfd] + " JOIN " + ch_name + '\n';
                            send(sockfd, msg.c_str(), msg.length(), 0);

                            // Is there any topic
                            if (channel[ch_name].topic == ""){
                                msg = prefix + "331 " + nickname[sockfd] + " " + ch_name + " :No topic is set\n";
                                send(sockfd, msg.c_str(), msg.length(), 0);
                            }
                            else{
                                msg = prefix + "332 " + nickname[sockfd] + " " + ch_name + " :" + channel[ch_name].topic + '\n';
                                send(sockfd, msg.c_str(), msg.length(), 0);
                            }

                            // Member of the channel
                            for (int fd : channel[ch_name].memb){
                                msg = prefix + "353 " + nickname[sockfd] + " " + ch_name + " :" + nickname[fd] + '\n';
                                send(sockfd, msg.c_str(), msg.length(), 0);
                            }
                            // End of Name List
                            msg = prefix + "366 " + nickname[sockfd] + " " + ch_name + " :End of Names List\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);

                        }
                    }
                    else if (cmd == "TOPIC"){

                        if (pos == -1){
                            msg = prefix + "461 " + nickname[sockfd] + " " + cmd + " :Not enough parameters\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                        else{
                            std::stringstream ss(sbuf.substr(pos + 1));
                            vector<string> param;
                            string s;
                            while (ss >> s){
                                param.push_back(s);
                            }

                            if (param.size() == 1){
                                // Show topic
                                // if (channel[param[0]].topic == ""){
                                //     // No topic
                                //     msg = prefix + "331 " + nickname[sockfd] + " " + param[0] + " :No topic is set\n";
                                //     send(sockfd, msg.c_str(), msg.length(), 0);
                                // }
                                if (user[i].chnl != param[0]){
                                    // not on that channel
                                    msg = prefix + "442 " + nickname[sockfd] + " " + param[0] + " :You're not on that channel\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                }
                                else{
                                    // send topic
                                    msg = prefix + "332 " + nickname[sockfd] + " " + param[0] + " " + channel[param[0]].topic + '\n';
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                }


                            }
                            else{
                                // Change Topic
                                printf("%s, %s\n", user[i].chnl.c_str(), param[0].c_str());
                                if (user[i].chnl == param[0]){
                                    string topic = "";
                                    for (int j = 1; j < param.size();j++){
                                        topic += param[j] + " ";
                                    }
                                    topic.pop_back();
                                    channel[param[0]].topic = topic.substr(1);
                                }
                                else{
                                    msg = prefix + "442 " + nickname[sockfd] + " " + param[0] + " :You're not on that channel\n";
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                }
                            }
                        }
                    }
                    else if (cmd == "NAMES"){
                        string ch_name = "";
                        if (pos == -1){
                            for (const auto p : channel){
                                for (int fd : p.second.memb){
                                    msg = prefix + "353 " + nickname[sockfd] + " " + p.first + " :" + nickname[fd] + '\n';
                                    send(sockfd, msg.c_str(), msg.length(), 0);
                                }
                            }
                        }
                        else{
                            ch_name = sbuf.substr(pos + 1);
                            for (int fd : channel[ch_name].memb){
                                msg = prefix + "353 " + nickname[sockfd] + " " + ch_name + " :" + nickname[fd] + '\n';
                                send(sockfd, msg.c_str(), msg.length(), 0);
                            }
                        }
                        // End of Name List
                        msg = prefix + "366 " + nickname[sockfd] + " " + user[i].chnl + " :End of Names List\n";
                        send(sockfd, msg.c_str(), msg.length(), 0);
                    }
                    else if (cmd == "PART"){
                        if (pos == -1){
                            // need more parameters
                            msg = prefix + "461 " + nickname[sockfd] + " " + cmd + " :Not enough parameters\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                        else{
                            std::stringstream ss(sbuf.substr(pos + 1));
                            vector<string> param;
                            string s;
                            while (ss >> s){
                                param.push_back(s);
                            }
                            string ch_name = param[0];
                            // ch_name.pop_back();
                            if (channel.find(ch_name) == channel.end()){
                                // no such channel
                                msg = prefix + "403 " + nickname[sockfd] + " " + ch_name + " :No such channel\n";
                                send(sockfd, msg.c_str(), msg.length(), 0);
                            }
                            else if (user[i].chnl != ch_name){
                                // not on channel
                                msg = prefix + "442 " + nickname[sockfd] + " " + ch_name + " :You're not on that channel\n";
                                send(sockfd, msg.c_str(), msg.length(), 0);
                            }
                            else{
                                // leave channel
                                printf("leave chnl\n");

                                for (int fd : channel[user[i].chnl].memb){
                                    msg = ":" + nickname[sockfd] + " PART :" + user[i].chnl + '\n';
                                    send(fd, msg.c_str(), msg.length(), 0);
                                }

                                user[i].chnl = "";
                                channel[ch_name].memb.erase(std::find(channel[ch_name].memb.begin(), channel[ch_name].memb.end(), sockfd));
                            }
                        }
                    }
                    else if (cmd == "USERS"){
                        msg = prefix + "392 " + nickname[sockfd] + " :UserID   Terminal  Host\n";
                        send(sockfd, msg.c_str(), msg.length(), 0);
                        for (int j = 0;j <= maxi; j++){
                            if (client[j] == -1) continue;
                            msg = prefix + "393 " + nickname[sockfd] + " :" + nickname[client[j]] + " - " + user[j].ip + '\n';
                            // format problem
                            char str[100];
                            printf("%ld\n", msg.size());
                            snprintf(str, 100, "%s393 %s :%-8s %-9s %-8s\n", prefix.c_str(), nickname[sockfd].c_str(), nickname[client[j]].c_str(), "-", user[j].ip.c_str());
                            msg = str;
                            send(sockfd, msg.c_str(), msg.size(), 0);
                        }
                        msg = prefix + "394 " + nickname[sockfd] + " :End of users\n";
                        send(sockfd, msg.c_str(), msg.length(), 0);
                    }
                    else if (cmd == "PRIVMSG"){
                        std::stringstream ss(sbuf.substr(pos + 1));
                        vector<string> param;
                        string s;
                        while (ss >> s){
                            param.push_back(s);
                        }
                        printf("param[0]: %s\n", param[0].c_str());

                        if (pos == -1){
                            // no recipent
                            msg = prefix + "411 " + nickname[sockfd] + " " + cmd + " :No recipient given (" + cmd + ")\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                        else if (channel.find(param[0]) == channel.end()){
                            // no such nick/channel
                            msg = prefix + "401 " + nickname[sockfd] + " " + param[0] + " :No such nick/channel\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);
                        }
                        else if (param.size() < 2){
                            // no text to send
                            msg = prefix + "412 " + nickname[sockfd] + " :No text to send\n";
                            send(sockfd, msg.c_str(), msg.length(), 0);

                        }
                        else{
                            for (int fd : channel[param[0]].memb){
                                if (fd == sockfd) continue;
                                string sent = "";
                                for (int j = 1; j < param.size();j++){
                                    sent += param[j] + " ";
                                }
                                sent.pop_back();
                                msg = ":" + nickname[sockfd] + " PRIVMSG " + user[i].chnl + " " + sent + '\n';
                                send(fd, msg.c_str(), msg.length(), 0);
                            }
                        }


                    }
                    else if (cmd == "QUIT"){
                        close(sockfd);
                        client[i] = -1;
                        FD_CLR(sockfd, &allset);

                        if (user[i].chnl != ""){
                            channel[user[i].chnl].memb.erase(std::find(channel[user[i].chnl].memb.begin(), channel[user[i].chnl].memb.end(), sockfd));
                        }
                        user[i] = usr();
                        nickname[sockfd] = "";
                        online--;
                    }
                    else{
                        msg = prefix + "421 " + nickname[sockfd] + " " + cmd + " :Unknown command\n";
                        send(sockfd, msg.c_str(), msg.length(), 0);
                    }
                }

                if (--nready <= 0) break;
            }
        }
    }

    return 0;
}