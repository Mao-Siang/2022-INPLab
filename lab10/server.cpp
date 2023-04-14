/*
 * Lab problem set for INP course
 * by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 * License: GPLv2
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <netinet/ip.h>
#include <vector>

#define MAXLINE 2048
#define MSGLINE 100
#define HDR_LEN 28
#define PROTOCOL_NUM 161

#define err_quit(m) \
    do {            \
        perror(m);  \
        exit(-1);   \
    } while (0)

struct udphdr
{
    u_int16_t source;
    u_int16_t dest;
    u_int16_t len;
    u_int16_t check;
};

struct file {
    file(): isend(false), seq(0), file_no(0) {}
    void init(int ind, std::string& save_path) {
        char buf[100] = {};
        snprintf(buf, 100, "%06d", ind);
        file_no = ind;
        filename = buf;
        path = save_path + "/" + filename;
        fs.open(path);
    }
    void write_file(std::string& content) { fs << content; }

    std::string filename, path;
    std::vector<std::string> content;
    std::ofstream fs;
    bool isend;
    int seq, file_no;
};

char packet[1300];
struct ip* iph = (struct ip*)packet;
// struct udphdr* udph = (struct udphdr*)(packet + sizeof(struct ip));
char* pkt_ptr = packet + sizeof(struct ip);

unsigned short cksum(void* in, int sz) {
    long sum = 0;
    unsigned short* ptr = (unsigned short*)in;
    for (; sz > 1; sz -= 2) sum += *ptr++;
    if (sz > 0) sum += *((unsigned char*)ptr);
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

void recv_data(int fd, file& f, int seq, std::string& body,
    struct sockaddr_in& csin) {
    if (seq > f.seq) {
        std::cout << seq << " " << f.seq << std::endl;
        err_quit("server is behind the client");
    }
    socklen_t csinlen = sizeof(csin);
    char ack[MSGLINE] = {}, content[MSGLINE] = {};
    int content_size = snprintf(content, MSGLINE, "%d %d", f.file_no, seq);
    int ack_size = snprintf(ack, MSGLINE, "ACK %d %s", content_size, content);

    strncpy(pkt_ptr, ack, ack_size + 1);

    iph->ip_len = sizeof(struct ip) + ack_size;
    iph->ip_sum = cksum(packet, iph->ip_len);

    int success =
        sendto(fd, packet, iph->ip_len + ack_size, 0, (struct sockaddr*)&csin, csinlen);

    // printf("success: %d\n", success);
    if (seq != f.seq) return;
    f.write_file(body);
    f.seq++;
}

void recv_end(int fd, file& f, struct sockaddr_in& csin) {
    socklen_t csinlen = sizeof(csin);
    char ack[MSGLINE] = {};
    std::string file_no = std::to_string(f.file_no);
    int ack_size =
        snprintf(ack, MSGLINE, "ENDACK %ld %s", file_no.size(), file_no.data());

    pkt_ptr = packet + sizeof(struct ip);
    strncpy(pkt_ptr, ack, ack_size + 1);

    iph->ip_len = sizeof(struct ip) + ack_size;
    iph->ip_sum = cksum(packet, iph->ip_len);
    sendto(fd, packet, iph->ip_len, 0, (struct sockaddr*)&csin, csinlen);
    f.fs.close();
}

int main(int argc, char* argv[]) {
    int s;
    struct sockaddr_in sin;

    if (argc < 4) {
        return -fprintf(stderr, "usage: %s ... <port>\n", argv[0]);
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(10005);
    if (inet_pton(AF_INET, argv[3], &sin.sin_addr) <= 0) {
        return -fprintf(stderr, "** cannot convert IPv4 address for %s\n",
            argv[3]);
    }
    //ip header
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

    if ((s = socket(AF_INET, SOCK_RAW, PROTOCOL_NUM)) < 0) err_quit("socket");
    if (bind(s, (struct sockaddr*)&sin, sizeof(sin)) < 0) err_quit("bind");

    std::string save_path = argv[1];
    int n_files = atoi(argv[2]);
    // n_files = 1;
    int end_files = 0;

    struct sockaddr_in csin;
    socklen_t csinlen = sizeof(csin);
    int rlen;

    std::vector<file> file_arr(n_files);
    for (int i = 0; i < n_files; i++) file_arr[i].init(i, save_path);

    timeval ts { 1, 0 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &ts, sizeof(ts));

    int on = 1;
    setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

    while (end_files < n_files) {
        // std::cout << "end_files: " << end_files << std::endl;
        char buf[MAXLINE];
        memset(buf, 0, sizeof(buf));
        int ret =
            recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&csin, &csinlen);
        // std::cout << "ret: " << ret << std::endl;
        if (ret < 0) {
            if (errno == EAGAIN) continue;
            perror("recvfrom");
            break;
        }

        struct ip* iph = (struct ip*)buf;

        if (iph->ip_v != 4) continue;
        if (iph->ip_id != htons(12345)) continue;
        // if (iph->ip_len != ret) continue;
        if (iph->ip_src.s_addr != sin.sin_addr.s_addr) continue;

        // if (udph->dest != htons(10005)) continue;
        // if (udph->source != htons(10005)) continue;

        char* pkt_ptr = buf + sizeof(struct ip);
        std::string packet(pkt_ptr);
        std::stringstream ss(packet);

        int length;
        std::string type, check_data;
        ss >> type >> length;
        ss.get();
        getline(ss, check_data, '\0');
        ss.clear();

        // puts("before");
        std::cout << check_data.size() << " " << length << std::endl;
        if (check_data.size() != length) continue;
        // puts("after");
        // puts(type.data());
        ss.str(check_data);
        int file_no;
        ss >> file_no;
        file& f = file_arr[file_no];

        if (type == "DATA") {
            int seq;
            std::string body;
            ss >> seq;
            ss.get();
            getline(ss, body, '\0');
            // if (file_no == 0 && seq == 0)
            //     std::cout << "server body: " << body << std::endl;
            recv_data(s, f, seq, body, csin);
        }
        else if (type == "END") {
            recv_end(s, f, csin);
            if (f.isend) continue;
            f.isend = true;
            end_files++;
            // printf("end_files: %d\n", end_files);
        }
        else
            continue;
    }

    timeval tv { 1, 0 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // handle connection
    puts("* Server is going to stop");
    while (1) {
        char buf[MAXLINE] = {};
        int ret =
            recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&csin, &csinlen);
        if (ret < 0) {
            if (errno == EAGAIN) break;
            perror("recvfrom");
            break;
        }

        // std::cout << "bottom " << buf << std::endl;

        std::string packet(buf);
        std::stringstream ss(packet);

        int length;
        std::string type, check_data;
        ss >> type >> length;
        ss.get();
        getline(ss, check_data, '\0');
        ss.clear();


        if (check_data.size() != length) continue;

        ss.str(check_data);
        int file_no;
        ss >> file_no;
        file& f = file_arr[file_no];

        if (type == "DATA") {
            int seq;
            std::string body;
            ss >> seq;
            ss.get();
            getline(ss, body, '\0');
            // recv_data(s, f, seq, body, csin);
        }
        else if (type == "END") {
            recv_end(s, f, csin);
            if (f.isend) continue;
            f.isend = true;
            end_files++;
        }
        else
            continue;
    }

    puts("* Server stops");

    close(s);
    return 0;
}