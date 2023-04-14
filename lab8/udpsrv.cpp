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
#include <vector>

#define MAXLINE 2048
#define MSGLINE 100

#define err_quit(m) \
    do {            \
        perror(m);  \
        exit(-1);   \
    } while (0)

struct file {
    file() : isend(false), seq(0), file_no(0) {}
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
    int success =
        sendto(fd, ack, ack_size, 0, (struct sockaddr*)&csin, csinlen);
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
    sendto(fd, ack, ack_size, 0, (struct sockaddr*)&csin, csinlen);
    f.fs.close();
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
    if (bind(s, (struct sockaddr*)&sin, sizeof(sin)) < 0) err_quit("bind");

    std::string save_path = argv[1];
    int n_files = atoi(argv[2]);
    // n_files = 100;
    int end_files = 0;

    struct sockaddr_in csin;
    socklen_t csinlen = sizeof(csin);
    int rlen;

    std::vector<file> file_arr(n_files);
    for (int i = 0; i < n_files; i++) file_arr[i].init(i, save_path);

    timeval ts{1, 0};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &ts, sizeof(ts));
    // int optval = 2621440;
    // socklen_t optlen = sizeof(optval);
    // setsockopt(s, SOL_SOCKET, SO_RCVBUF, &optval, optlen);

    while (end_files < n_files) {
        // std::cout << "end_files: " << end_files << std::endl;
        char buf[MAXLINE] = {};
        int ret =
            recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&csin, &csinlen);
        // std::cout << "ret: " << ret << std::endl;
        if (ret < 0) {
            if (errno == EAGAIN) continue;
            perror("recvfrom");
            break;
        }

        std::string packet(buf);
        std::stringstream ss(packet);

        // puts(buf);

        int length;
        std::string type, check_data;
        ss >> type >> length;
        ss.get();
        getline(ss, check_data, '\0');
        ss.clear();

        // puts("before");
        // std::cout << check_data.size() << " " << length << std::endl;
        if (check_data.size() != length) continue;
        // puts("after");

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
            // std::cout << "*server body: " << body.size() << std::endl;
            recv_data(s, f, seq, body, csin);
        } else if (type == "END") {
            recv_end(s, f, csin);
            if (f.isend) continue;
            f.isend = true;
            end_files++;
            std::cout << "end_files: " << end_files << std::endl;
        } else
            continue;
    }

    timeval tv{1, 0};
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
        } else if (type == "END") {
            recv_end(s, f, csin);
            if (f.isend) continue;
            f.isend = true;
            end_files++;
        } else
            continue;
    }

    puts("* Server stops");

    close(s);
    return 0;
}