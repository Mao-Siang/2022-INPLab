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
#include <netinet/ip.h>

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
#define PROTOCOL_NUM 161
#define HDR_LEN 28
using std::string;

unsigned short cksum(void* in, int sz) {
    long sum = 0;
    unsigned short* ptr = (unsigned short*)in;
    for (; sz > 1; sz -= 2) sum += *ptr++;
    if (sz > 0) sum += *((unsigned char*)ptr);
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}


bool is_number(string s) {
    for (auto c : s) {
        if (!isdigit(c)) return false;
    }
    return true;
}


static struct sockaddr_in sin;
static socklen_t slen = sizeof(sin);
struct udphdr
{
    u_int16_t source;
    u_int16_t dest;
    u_int16_t len;
    u_int16_t check;
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        return -fprintf(stderr,
            "usage: %s ... <path-to-read-files>  "
            "<total-number-of-files> <broadcast-address>\n",
            argv[0]);
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    int s = -1;
    if ((s = socket(AF_INET, SOCK_RAW, PROTOCOL_NUM)) < 0) err_quit("socket");
    timeval tv = { 0, 2500 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int on = 1;
    setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(10005);
    if (inet_pton(AF_INET, argv[3], &sin.sin_addr) <= 0) {
        return -fprintf(stderr, "** cannot convert IPv4 address for %s\n",
            argv[3]);
    }

    // set ip header
    // struct ip is friendly to non linux platform
    char packet[1300];
    struct ip* iph = (struct ip*)packet;
    // struct udphdr* udph = (struct udphdr*)(packet + sizeof(struct ip));
    char* pkt_ptr = packet + sizeof(struct ip);
    memset(packet, 0, sizeof(packet));

    iph->ip_v = 4;
    iph->ip_hl = 5;
    iph->ip_id = htons(12345);
    iph->ip_tos = 0;
    iph->ip_ttl = 64;
    iph->ip_off = 0;
    iph->ip_src.s_addr = inet_addr(argv[3]);
    iph->ip_dst.s_addr = sin.sin_addr.s_addr;
    iph->ip_len = 0;
    iph->ip_sum = 0;
    iph->ip_p = PROTOCOL_NUM;


    // udph->dest = htons(10005);
    // udph->source = htons(10005);
    // udph->len = 0;
    // udph->check = 0;

    std::vector<std::vector<string>> data(1000);
    char buf[MAXLINE];
    string sbuf = "";

    // read all data
    int data_size = atoi(argv[2]);
    // data_size = 1;
    for (int i = 0; i < data_size; i++) {
        string fname = std::to_string(i);
        while (fname.size() < 6) fname = "0" + fname;

        string path = argv[1] + ("/" + fname);
        int fd = open(path.c_str(), O_RDONLY);

        int size = 0, n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            sbuf = buf;
            if (n == MAXLINE) sbuf = sbuf.substr(0, 1000);
            data[i].push_back(sbuf);
            size += n;
            printf("n: %d\n", n);
            memset(buf, 0, sizeof(buf));
        }
        printf("size %d: %d\n", i + 1, size);
        close(fd);
    }
    // printf("data1: %s\n", data[0][0].data());
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
        // if (end_files > 500) {
        //     timeval tv = { 0, 2500 };
        //     setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        // }
        for (int i = 0; i < data_size; i++) {
            if (ended[i]) continue;
            // if (i == 0) printf("data: %s\n", data[i][cur_seq[i]]);
            string fname = std::to_string(i);
            while (fname.size() < 6) fname = "0" + fname;

            pkt = std::to_string(i) + " " + std::to_string(cur_seq[i]) + " " +
                data[i][cur_seq[i]];

            std::cout << "data size: " << data[i][cur_seq[i]].size() << std::endl;
            std::cout << "pkt size: " << pkt.size() << std::endl;
            pkt = "DATA " + std::to_string(pkt.size()) + " " + pkt;

            strncpy(pkt_ptr, pkt.data(), pkt.size());

            // set checksum
            iph->ip_len = pkt.size() + sizeof(struct ip);
            // udph->len = sizeof(struct ip) + pkt.size();
            // printf("iplen: %d\n", iph->ip_len);
            iph->ip_sum = cksum(packet, iph->ip_len);
            int n = sendto(s, packet, iph->ip_len, 0, (const sockaddr*)&sin,
                sizeof(sin));
            // printf("send: %d\n", n);
        }

        // Recieve ACK
        int recv = 0;

        while (1) {

            memset(buf, 0, sizeof(buf));
            if ((recv = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&sin,
                &slen)) > 0) {
                // printf("recv: %d\n", recv);

                ip* iph = (struct ip*)buf;
                // udphdr* udph = (struct udphdr*)(buf + sizeof(struct ip));
                // printf("buf: %s\n", buf);
                if (iph->ip_v != 4) continue;
                if (iph->ip_id != htons(12345)) continue;
                if (iph->ip_src.s_addr != sin.sin_addr.s_addr) continue;

                // if (udph->dest != htons(10005)) continue;
                // if (udph->source != htons(10005)) continue;

                sbuf = buf + sizeof(struct ip);
                // printf("sbuf: %s\n", sbuf.data());
                std::stringstream ss(sbuf);
                string type = "";
                int len, fnum, seq_num = 0;
                ss >> type >> len;
                ss.get();
                std::string content;
                getline(ss, content, '\0');

                // std::cout << len << " " << content.size() << std::endl;

                if (len != content.size()) continue;
                // puts("after");
                ss.clear();
                ss.str(content);
                ss >> fnum >> seq_num;

                if (type == "ACK") {
                    if (cur_seq[fnum] != seq_num) continue;
                    acked[fnum][cur_seq[fnum]] = true;
                    cur_seq[fnum] += 1;

                }
                else if (type == "ENDACK") {
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
            if (cur_seq[i] < data[i].size()) continue;
            ended[i] = true;
        }

        // Send END
        for (int i = 0; i < data_size; i++) {
            if (ended[i] && !ack_ended[i]) {
                string endmsg = std::to_string(i);
                string out_msg =
                    "END " + std::to_string(endmsg.size()) + " " + endmsg;

                strncpy(pkt_ptr, out_msg.data(), out_msg.size() + 1);
                iph->ip_len = sizeof(struct ip) + out_msg.size();
                iph->ip_sum = cksum(packet, iph->ip_len);
                int n = sendto(s, packet, iph->ip_len, 0,
                    (const sockaddr*)&sin, slen);
                // printf("send end: %d\n", n);
            }
        }

        // check if all ENDACK are recieved
        all_end_acked = (end_files >= data_size);
    }

    close(s);
    return 0;
}