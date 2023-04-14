#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>

static struct timeval _t0;
static unsigned long long bytesent = 0;

double tv2s(struct timeval* ptv) {
    return 1.0 * (ptv->tv_sec) + 0.000001 * (ptv->tv_usec);
}

void handler(int s) {
    struct timeval _t1;
    double t0, t1;
    gettimeofday(&_t1, NULL);
    t0 = tv2s(&_t0);
    t1 = tv2s(&_t1);
    fprintf(stderr, "\n%lu.%06ld %llu bytes sent in %.6fs (%.6f Mbps; %.6f MBps)\n",
        _t1.tv_sec, _t1.tv_usec, bytesent, t1 - t0, 8.0 * (bytesent / 1000000.0) / (t1 - t0), (bytesent / 1000000.0) / (t1 - t0));
    exit(0);
}

int main(int argc, char* argv[]) {

    signal(SIGINT, handler);
    signal(SIGTERM, handler);

    int s = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr); // 140.113.213.213
    sin.sin_port = htons(10003);

    int c = connect(s, (const sockaddr*)&sin, sizeof(sin));

    printf("connect: %d", c);

    gettimeofday(&_t0, NULL);

    double speed = std::stod(argv[1]);
    int idx = (speed == 1.0) ? 0 : (speed == 1.5) ? 1 : (speed == 2) ? 2 : 3;
    int num = (int)speed;
    std::vector<int> hdr_len = { 100000 / 8, 100000 / 8, 175000 / 8, 250000 / 8 };
    std::vector<int64_t> buf((125000 * speed - num * 125), 1);//0- hdr_len[idx]
    while (1) {
        timespec t = { 1,0 };
        send(s, buf.data(), buf.size() * 8, 0);
        bytesent = bytesent + buf.size() * 8;
        nanosleep(&t, NULL);
    }

    // std::vector<int64_t> test(125, 0);
    // while (1){
    //     send(s, test.data(), 8 * test.size(), 0);
    //     timespec t = { 1,0 };
    //     nanosleep(&t, NULL);
    // }


    return 0;
}