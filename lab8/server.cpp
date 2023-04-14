/*
 * Lab problem set for INP course
 * by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 * License: GPLv2
 */
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

#define MAXLINE 1024
#define err_quit(m) \
    do {            \
        perror(m);  \
        exit(-1);   \
    } while (0)

struct file_content {
    file_content(): max_dg(0), written(false) {}
    std::string content[64];
    std::string filename;
    int max_dg;
    bool written;
} file_arr[1000];

std::string path_to_save;
void write_file(int file_ind) {
    file_content& file = file_arr[file_ind];
    file.written = true;
    std::string full_path = path_to_save + "/" + file.filename;
    // std::cout << full_path << std::endl;
    std::ofstream fs(full_path);

    for (int i = 0; i <= file.max_dg; i++) fs << file.content[i];
    // std::cout << file.content[0] << std::endl;
}

int main(int argc, char* argv[]) {
    int s;
    struct sockaddr_in sin;

    if (argc < 2) {
        return -fprintf(stderr, "usage: %s ... <port>\n", argv[0]);
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(strtol(argv[argc - 1], NULL, 0));
    printf("server: %ld\n", strtol(argv[argc - 1], NULL, 0));

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) err_quit("socket");
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (bind(s, (struct sockaddr*)&sin, sizeof(sin)) < 0) err_quit("bind");

    path_to_save = argv[1];
    int n_files = atoi(argv[2]), n_ends = 0;
    while (1) {
        struct sockaddr_in csin;
        socklen_t csinlen = sizeof(csin);
        int rlen;

        char buf[MAXLINE] = {};
        int ret;
        if (ret = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&csin,
            &csinlen) < 0) {
            if (errno != EAGAIN) {
                perror("recvfrom");
                break;
            }
            if (n_ends < n_files) continue;
            break;
        }
        std::string str(buf), content;
        std::stringstream ss(str);
        int data_len;

        // puts(buf);
        ss >> data_len;
        ss.get();
        std::getline(ss, content, '\0');

        if (data_len != content.size()) continue;
        ss.clear();
        ss.str(content);

        std::string filename, seq, data;
        ss >> filename >> seq;

        if (seq == "END") {
            int ind = std::stoi(filename);
            char endack[100] = {};
            int ack_size = snprintf(endack, 100, "ENDACK %d", ind);
            sendto(s, endack, ack_size, 0, (struct sockaddr*)&csin, csinlen);
            if (file_arr[ind].written) continue;
            n_ends++;
            std::cout << "n_ends: " << n_ends << std::endl;
            write_file(ind);
            continue;
        }

        ss.get();
        std::getline(ss, data, '\0');

        int ind = std::stoi(filename);
        int seq_i = std::stoi(seq);
        file_content& fc = file_arr[ind];
        fc.max_dg = std::max(seq_i, fc.max_dg);

        char ack[100] = {};
        int ack_size = snprintf(ack, 100, "ACK %d %s", ind, seq.data());
        sendto(s, ack, ack_size, 0, (struct sockaddr*)&csin, csinlen);

        if (fc.filename.empty()) fc.filename = filename;
        // std::cout << ind << " filename: " << fc.filename << std::endl;
        fc.content[seq_i] = data;
    }

    close(s);
    return 0;
}