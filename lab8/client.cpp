/*
 * Lab problem set for INP course
 * by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 * License: GPLv2
 */
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define err_quit(m) \
    do {            \
        perror(m);  \
        exit(-1);   \
    } while (0)

#define MAXLINE 1000
using std::string;

bool is_number(string s) {
    for (auto c : s) {
        if (!isdigit(c)) return false;
    }
    return true;
}

void sig_child(int s) {
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
    }
    exit(0);
}

static struct sockaddr_in sin;
static socklen_t slen = sizeof(sin);
static pid_t child_pid = -1;

int main(int argc, char* argv[]) {
    if (argc < 4) {
        return -fprintf(stderr,
                        "usage: %s ... <path-to-read-files> "
                        "<total-number-of-files> <port> <server-ip-address>\n",
                        argv[0]);
    }

    // signal(SIGCHLD, sig_child);

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(strtol(argv[argc - 2], NULL, 0));
    if (inet_pton(AF_INET, argv[argc - 1], &sin.sin_addr) <= 0) {
        return -fprintf(stderr, "** cannot convert IPv4 address for %s\n",
                        argv[argc - 1]);
    }

    int s = -1;
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) err_quit("socket");

    timeval tv = {0, 5000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    std::vector<std::vector<string>> data(1000);
    char buf[MAXLINE];
    string sbuf = "";

    // read all data
    int data_size = atoi(argv[argc - 3]);
    // data_size = 100;

    for (int i = 0; i < data_size; i++) {
        string fname = std::to_string(i);
        while (fname.size() < 6) fname = "0" + fname;

        string path = argv[argc - 4] + ("/" + fname);
        int fd = open(path.c_str(), O_RDONLY);

        int size = 0, n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            sbuf = buf;
            // std::cout << buf;
            data[i].push_back(sbuf);
            size += n;
            memset(buf, 0, sizeof(buf));
        }
        // std::cout << "client: " << i << " " << size << std::endl;
        // return 0;

        close(fd);
    }

    std::vector<std::vector<bool>> acked(data_size,
                                         std::vector<bool>(35, false));
    std::vector<int> cur_seq(data_size, 0);
    std::vector<bool> ended(data_size, false);
    std::vector<bool> ack_ended(data_size, false);
    bool all_end_acked = false;
    string hdr, pkt = "";

    int end_files = 0;
    while (!all_end_acked) {
        // send all data
        if (end_files > 500) {
            timeval tv = {0, 2500};
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        }
        for (int i = 0; i < data_size; i++) {
            if (ended[i]) continue;
            string fname = std::to_string(i);
            while (fname.size() < 6) fname = "0" + fname;

            pkt = std::to_string(i) + " " + std::to_string(cur_seq[i]) + " " +
                  data[i][cur_seq[i]];
            // std::cout << cur_seq[i] << std::endl;
            pkt = "DATA " + std::to_string(pkt.size()) + " " + pkt;

            int n = sendto(s, pkt.data(), pkt.size(), 0, (const sockaddr*)&sin,
                           sizeof(sin));
        }

        // Recieve ACK
        int recv = 0;

        while (1) {
            memset(buf, 0, sizeof(buf));
            if ((recv = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&sin,
                                 &slen)) > 0) {
                sbuf = buf;
                // puts(buf);
                std::stringstream ss(sbuf);
                string type = "";
                int len, fnum, seq_num = 0;
                ss >> type >> len;
                ss.get();
                std::string content;
                getline(ss, content, '\0');
                if (len != content.size()) continue;

                ss.clear();
                ss.str(content);
                ss >> fnum >> seq_num;

                if (type == "ACK") {
                    if (cur_seq[fnum] != seq_num) continue;
                    acked[fnum][cur_seq[fnum]] = true;
                    cur_seq[fnum] += 1;
                } else if (type == "ENDACK") {
                    if (ack_ended[fnum]) continue;
                    ack_ended[fnum] = true;
                    end_files++;
                    // printf("ack ended: %d\n", fnum);
                }

                continue;
            }
            break;
        }

        // check if all package are acked
        for (int i = 0; i < data_size; i++) {
            // std::cout << cur_seq[0] << " " << data[0].size() << " " <<
            // ended[0]  << " " << ack_ended[0] << std::endl;
            if (cur_seq[i] < data[i].size()) continue;
            ended[i] = true;
        }

        // Send END

        for (int i = 0; i < data_size; i++) {
            if (ended[i] && !ack_ended[i]) {
                string endmsg = std::to_string(i);
                string out_msg =
                    "END " + std::to_string(endmsg.size()) + " " + endmsg;
                sendto(s, out_msg.data(), out_msg.size(), 0,
                       (const sockaddr*)&sin, slen);
            }
        }

        // check if all ENDACK are recieved
        all_end_acked = (end_files >= data_size);
    }

    close(s);
    return 0;
}
