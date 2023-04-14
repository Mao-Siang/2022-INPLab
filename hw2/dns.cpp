/*
 * Lab problem set for INP course
 * by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 * License: GPLv2
 */
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <regex>

using std::string;
using std::vector;

#define MAXLINE 1024
#define err_quit(m) \
    do {            \
        perror(m);  \
        exit(-1);   \
    } while (0)

struct header{
    uint16_t ID;
    uint16_t flag;
    uint16_t qdcount; // number of question
    uint16_t ancount; // number of resource record
    uint16_t nscount; // number of authority
    uint16_t arcount; // number of additional

    header(): ID(0), flag(0), qdcount(0), ancount(0), nscount(0), arcount(0) {}
};

struct question{
    vector<uint8_t> name;
    uint16_t qtype;
    uint16_t qclass;

    question(): qtype(0), qclass(0), name(0){}
};

struct resource_record{
    vector<uint8_t> name;
    uint16_t type;
    uint16_t cls = 1; // internet
    uint32_t ttl;
    uint16_t rdlength;
    vector<uint8_t> rdata;

    resource_record(): type(0), cls(0), rdlength(0), ttl(0), rdata(0), name(0){};
};

struct dns_msg{
    header hdr;
    question q;
    resource_record* rrs;

    dns_msg(): rrs(0){}
};

std::vector<string> split(std::string s, char c){
    std::vector<string> result;
    size_t pos = s.find(c);
    while (pos != string::npos){
        result.push_back(s.substr(0, pos));
        s = s.substr(pos + 1);
        pos = s.find(c);
    }
    result.push_back(s);
    return result;
}

header read_header(uint8_t* buffer){
    header hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(&hdr.ID, buffer, 2);
    hdr.ID = ntohs(hdr.ID);
    memcpy(&hdr.flag, buffer + 2, 2);
    hdr.flag = ntohs(hdr.flag);
    memcpy(&hdr.qdcount, buffer + 4, 2);
    hdr.qdcount = ntohs(hdr.qdcount);
    memcpy(&hdr.ancount, buffer + 6, 2);
    hdr.ancount = ntohs(hdr.ancount);

    memcpy(&hdr.nscount, buffer + 8, 2);
    hdr.nscount = ntohs(hdr.nscount);
    memcpy(&hdr.arcount, buffer + 10, 2);
    hdr.arcount = ntohs(hdr.arcount);

    return hdr;
};

question read_question(uint8_t* buffer, size_t& size, std::vector<string>& qlabels){
    question q;
    memset(&q, 0, sizeof(q));
    int offset = 0;

    while (true){
        q.name.push_back(buffer[offset]);
        int index_size = buffer[offset++];
        if (index_size == 0) break;

        string label = "";
        for (int i = 0;i < index_size; i++){
            q.name.push_back(buffer[offset]);
            label += buffer[offset++];
        }
        qlabels.push_back(label);
    }

    memcpy(&q.qtype, buffer + offset, 2);
    q.qtype = ntohs(q.qtype);
    memcpy(&q.qclass, buffer + offset + 2, 2);
    q.qclass = ntohs(q.qclass);

    size = offset + 4;
    return q;
}

vector<uint8_t> send_reply(header hdr, question q, vector<resource_record>& answers){
    vector<uint8_t> reply(12);
    // header
    memcpy(reply.data(), &hdr, sizeof(hdr));
    // question
    for (uint8_t u : q.name){
        reply.push_back(u);
    }
    reply.push_back((uint8_t)(q.qtype & 0x00FF));
    reply.push_back((uint8_t)((q.qtype & 0xFF00) >> 8));
    reply.push_back((uint8_t)(q.qclass & 0x00FF));
    reply.push_back((uint8_t)((q.qclass & 0xFF00) >> 8));

    // answer
    for (auto rr : answers){
        for (auto n : rr.name) reply.push_back(n);

        reply.push_back((uint8_t)(rr.type & 0x00FF));
        reply.push_back((uint8_t)((rr.type & 0xFF00) >> 8));

        reply.push_back((uint8_t)(rr.cls & 0x00FF));
        reply.push_back((uint8_t)((rr.cls & 0xFF00) >> 8));
        uint32_t tmp_ttl = ntohl(rr.ttl);
        uint8_t arr[4];
        for (int i = 3; i >= 0;i--){
            arr[i] = tmp_ttl % 256;
            tmp_ttl /= 256;
        }
        for (int i = 0;i < 4;i++) reply.push_back(arr[i]);
        reply.push_back((uint8_t)(rr.rdlength & 0x00FF));
        reply.push_back((uint8_t)((rr.rdlength & 0xFF00) >> 8));
        for (auto n : rr.rdata) reply.push_back(n);
    }
    return reply;
}

vector<uint8_t> uint32to8(uint32_t num){
    vector<uint8_t> arr;
    for (int i = 3; i >= 0;i--){
        arr.push_back(num % 256);
        num /= 256;
    }
    return arr;
}

int main(int argc, char* argv[]) {
    int sockfd;
    struct sockaddr_in sin;
    static socklen_t slen = sizeof(sin);

    if (argc < 2) {
        return -fprintf(stderr, "usage: %s ... <port-number> <path/to/the/config/file>\n", argv[0]);
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(strtol(argv[1], NULL, 0));
    printf("server: %ld\n", strtol(argv[1], NULL, 0));

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) err_quit("socket");

    if (bind(sockfd, (struct sockaddr*)&sin, sizeof(sin)) < 0) err_quit("bind");


    struct sockaddr_in csin;
    socklen_t csinlen = sizeof(csin);

    string config_path = argv[2];

    std::fstream fs(config_path);
    string forward_ip;
    std::getline(fs, forward_ip);

    printf("forward: %s\n", forward_ip.data());

    vector<string> my_domain;
    std::map<string, string> domain_data;
    std::map<int, string> idx2type = { {1, "A"},{2, "NS"},{5, "CNAME"},{6, "SOA"},{15, "MX"},{16, "TXT"},{28, "AAAA"} };

    string buf = "";
    while (std::getline(fs, buf)){
        int pos = buf.find(',');
        domain_data[buf.substr(0, pos)] = buf.substr(pos + 1);
        my_domain.push_back(buf.substr(0, pos));
    }

    fs.close();

    ssize_t recvlen;
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    while ((recvlen = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&csin, &csinlen)) > 0){
        size_t size = 0;
        header hdr = read_header(buffer);
        std::vector<string> qlabels = {};
        question q = read_question(buffer + 12, size, qlabels);

        vector<resource_record> answers;
        vector<resource_record> ns;
        vector<resource_record> soa;

        string domain = "";
        for (auto s : qlabels){
            domain += s + ".";
        }
        std::cout << domain << '\n';

        // nip.io service
        bool nip = false;
        string reg = "^([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})\\.([0-9a-zA-Z]{1,61}\\.)*"; // {domain}$
        for (auto d : my_domain){
            std::regex regex(reg + d + "$");
            if (regex_match(domain, regex)){
                dns_msg reply;
                // header part
                reply.hdr.ID = htons(hdr.ID);
                reply.hdr.flag = htons(0x8180);
                reply.hdr.qdcount = htons(0x0001);

                // question part
                reply.q.qtype = htons(q.qtype);
                reply.q.qclass = htons(q.qclass);

                reply.q.name = q.name;

                // resource record
                resource_record rr;
                rr.cls = htons(0x0001); // internet
                rr.type = htons(0x0001); // A
                rr.rdlength = htons(0x0004); // 32 bits

                string addr = "";
                int dot = 0;
                for (char c : domain){
                    if (c == '.') dot++;
                    if (dot == 4) break;
                    addr.push_back(c);
                }

                uint8_t* address = new uint8_t(4);
                if (inet_pton(AF_INET, addr.c_str(), address) <= 0){
                    return -fprintf(stderr, "** cannot convert IPv4 address for %s\n",
                        argv[3]);
                }
                for (int i = 0;i < 4;i++){
                    rr.rdata.push_back(address[i]);
                }

                rr.ttl = htonl(1); // not implemented
                rr.name = q.name;

                answers.push_back(rr);
                reply.hdr.ancount = htons(1);


                vector<uint8_t> rply;
                rply = send_reply(reply.hdr, reply.q, answers);
                int send = sendto(sockfd, rply.data(), rply.size(), 0, (struct sockaddr*)&csin, sizeof(csin));
                printf("nip send: %d\n", send);

                nip = true;
            }
            if (nip) break;
        }

        if (nip) continue;

        int pos = domain.find('.');
        string filename, sub_domain;
        if (domain_data.find(domain) != domain_data.end()){
            filename = domain;
            sub_domain = "@";
        }
        else{
            filename = domain.substr(pos + 1);
            sub_domain = domain.substr(0, pos);
        }
        std::cout << "filename: " << filename << " " << sub_domain << "\n";
        if (domain_data.find(filename) != domain_data.end()){
            // in local domain
            fs.open(domain_data[filename]);
            string type = idx2type[q.qtype];
            std::cout << "type: " << type << '\n';

            dns_msg reply;
            // header part
            reply.hdr.ID = htons(hdr.ID);
            reply.hdr.flag = htons(0x8180);
            reply.hdr.qdcount = htons(0x0001);

            // question part
            reply.q.qtype = htons(q.qtype);
            reply.q.qclass = htons(q.qclass);

            reply.q.name = q.name;

            // get the first line: root domain
            getline(fs, buf);
            std::cout << "buf: " << buf << '\n';

            // read the zone file
            std::vector<string> records;
            while (getline(fs, buf)){
                records.push_back(buf);
            }

            for (auto record : records){
                std::vector<string> split_buf = split(record, ',');
                resource_record rr;
                if (split_buf[3] == "NS"){
                    rr.cls = htons(0x0001);
                    rr.type = htons(0x0002);
                    string s = "";
                    for (char c : split_buf[4]){
                        if (c == '.'){
                            rr.rdata.push_back((uint8_t)s.length());
                            for (char i : s) {
                                rr.rdata.push_back(int(i));
                            }
                            s = "";
                        }
                        else{
                            s += c;
                        }
                    }
                    rr.rdata.push_back(0);
                    // for (int i : rr.rdata){
                    //     std::cout << i << " ";
                    // }
                    // std::cout << "rdata size: " << rr.rdata.size() << '\n';
                    rr.rdlength = htons(rr.rdata.size());
                    rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10));
                    rr.name = q.name;
                    ns.push_back(rr);
                }
                else if (split_buf[3] == "SOA"){
                    rr.cls = htons(0x0001);
                    rr.type = htons(0x0006);
                    rr.name = q.name;
                    rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10));
                    vector<string> v = split(split_buf[4], ' ');
                    string s = "";
                    for (char c : v[0]){
                        if (c == '.'){
                            rr.rdata.push_back((uint8_t)s.length());
                            for (char i : s) {
                                rr.rdata.push_back(int(i));
                            }
                            s = "";
                        }
                        else{
                            s += c;
                        }
                    }
                    rr.rdata.push_back(0);

                    for (char c : v[1]){
                        if (c == '.'){
                            rr.rdata.push_back((uint8_t)s.length());
                            for (char i : s) {
                                rr.rdata.push_back(int(i));
                            }
                            s = "";
                        }
                        else{
                            s += c;
                        }
                    }
                    rr.rdata.push_back(0);

                    uint32_t serial = strtoul(v[2].c_str(), NULL, 10);
                    uint32_t refresh = strtoul(v[3].c_str(), NULL, 10);
                    uint32_t retry = strtoul(v[4].c_str(), NULL, 10);
                    uint32_t expire = strtoul(v[5].c_str(), NULL, 10);
                    uint32_t minimum = strtoul(v[6].c_str(), NULL, 10);
                    serial = htonl(serial);
                    refresh = htonl(refresh);
                    retry = htonl(retry);
                    expire = htonl(expire);
                    minimum = htonl(minimum);

                    for (uint8_t num : uint32to8(serial)) rr.rdata.push_back(num);
                    for (uint8_t num : uint32to8(refresh)) rr.rdata.push_back(num);
                    for (uint8_t num : uint32to8(retry)) rr.rdata.push_back(num);
                    for (uint8_t num : uint32to8(expire)) rr.rdata.push_back(num);
                    for (uint8_t num : uint32to8(minimum)) rr.rdata.push_back(num);
                    rr.rdlength = htons(rr.rdata.size());

                    soa.push_back(rr);
                }
            }

            vector<string> ans_domain;
            int arcount = 0;
            switch (q.qtype)
            {
            case 1:
                // A type

                // answer section 
                for (auto record : records){
                    std::vector<string> split_buf = split(record, ',');
                    // std::cout << split_buf[0] << " " << split_buf[3] << '\n';
                    if (split_buf[0] == sub_domain && split_buf[3] == type){

                        resource_record rr;
                        rr.cls = htons(0x0001); // internet
                        rr.type = htons(0x0001); // A
                        rr.rdlength = htons(0x0004); // 32 bits
                        uint8_t* address = new uint8_t(4);
                        if (inet_pton(AF_INET, split_buf[4].data(), address) <= 0){
                            return -fprintf(stderr, "** cannot convert IPv4 address for %s\n",
                                argv[3]);
                        }
                        for (int i = 0;i < 4;i++){
                            rr.rdata.push_back(address[i]);
                        }

                        rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                        rr.name = q.name;

                        answers.push_back(rr);
                    }

                }
                reply.hdr.ancount = htons(answers.size());

                // authority section
                if (answers.size() != 0){
                    // NS
                    for (auto r : ns){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(ns.size());
                }
                else{
                    // SOA
                    for (auto r : soa){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(soa.size());
                }

                break;
            case 2:
                // NS type
                ans_domain.clear();
                for (auto record : records){
                    std::vector<string> split_buf = split(record, ',');
                    // std::cout << split_buf[0] << " " << split_buf[3] << '\n';
                    if (split_buf[0] == sub_domain && split_buf[3] == type){

                        resource_record rr;
                        rr.cls = htons(0x0001); // internet
                        rr.type = htons(0x0002); // NS
                        int pos = split_buf[4].find('.');
                        ans_domain.push_back(split_buf[4].substr(0, pos));

                        string s = "";
                        for (char c : split_buf[4]){
                            if (c == '.'){
                                rr.rdata.push_back((uint8_t)s.length());
                                for (char i : s) {
                                    rr.rdata.push_back(int(i));
                                }
                                s = "";
                            }
                            else{
                                s += c;
                            }
                        }
                        rr.rdata.push_back(0);

                        rr.rdlength = htons(rr.rdata.size());
                        rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                        rr.name = q.name;

                        answers.push_back(rr);
                    }

                }

                reply.hdr.ancount = htons(answers.size());

                // authority section
                if (answers.size() == 0){
                    // SOA
                    for (auto r : soa){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(soa.size());
                }
                // additional section
                arcount = 0;
                for (auto d : ans_domain){
                    for (auto record : records){
                        std::vector<string> split_buf = split(record, ',');
                        if (split_buf[0] == d && split_buf[3] == "A"){
                            resource_record rr;
                            rr.cls = htons(0x0001); // internet
                            rr.type = htons(0x0001); // A
                            rr.rdlength = htons(0x0004); // 32 bits
                            uint8_t* address = new uint8_t(4);
                            if (inet_pton(AF_INET, split_buf[4].data(), address) <= 0){
                                return -fprintf(stderr, "** cannot convert IPv4 address for %s\n",
                                    argv[3]);
                            }
                            for (int i = 0;i < 4;i++){
                                rr.rdata.push_back(address[i]);
                            }

                            rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                            rr.name.clear();
                            rr.name.push_back(d.length());
                            for (char c : d){
                                rr.name.push_back((int)c);
                            }
                            for (uint8_t u : q.name){
                                rr.name.push_back(u);
                            }
                            // rr.name = q.name;

                            answers.push_back(rr);
                            arcount++;
                        }
                        else if (split_buf[0] == d && split_buf[3] == "AAAA"){

                            resource_record rr;
                            rr.cls = htons(0x0001); // internet
                            rr.type = htons(0x001c); // AAAA

                            uint8_t* address = new uint8_t(16);
                            if (inet_pton(AF_INET6, split_buf[4].data(), address) <= 0){
                                return -fprintf(stderr, "** cannot convert IPv6 address for %s\n",
                                    argv[3]);
                            }
                            for (int i = 0;i < 16;i++){
                                rr.rdata.push_back(address[i]);
                            }

                            rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                            rr.name.clear();
                            rr.name.push_back(d.length());
                            for (char c : d){
                                rr.name.push_back((int)c);
                            }
                            for (uint8_t u : q.name){
                                rr.name.push_back(u);
                            }
                            rr.rdlength = htons(16);
                            answers.push_back(rr);
                            arcount++;
                        }
                    }
                }
                reply.hdr.arcount = htons(arcount);


                break;

            case 5:
                // CNAME
                for (auto record : records){
                    std::vector<string> split_buf = split(record, ',');
                    // std::cout << split_buf[0] << " " << split_buf[3] << '\n';
                    if (split_buf[0] == sub_domain && split_buf[3] == type){

                        resource_record rr;
                        rr.cls = htons(0x0001); // internet
                        rr.type = htons(0x0005); // CNAME

                        string s = "";
                        for (char c : split_buf[4]){
                            if (c == '.'){
                                rr.rdata.push_back((uint8_t)s.length());
                                for (char i : s) {
                                    rr.rdata.push_back(int(i));
                                }
                                s = "";
                            }
                            else{
                                s += c;
                            }
                        }
                        rr.rdata.push_back(0);

                        rr.rdlength = htons(rr.rdata.size());
                        rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                        rr.name = q.name;

                        answers.push_back(rr);
                    }

                }

                reply.hdr.ancount = htons(answers.size());
                // authority section
                if (answers.size() != 0){
                    // NS
                    for (auto r : ns){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(ns.size());
                }
                else{
                    // SOA
                    for (auto r : soa){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(soa.size());
                }
                // no additional section
                break;

            case 6:
                // SOA
                for (auto record : records){
                    std::vector<string> split_buf = split(record, ',');
                    // std::cout << split_buf[0] << " " << split_buf[3] << '\n';
                    if (split_buf[0] == sub_domain && split_buf[3] == type){
                        resource_record rr;
                        rr.cls = htons(0x0001);
                        rr.type = htons(0x0006);
                        rr.name = q.name;
                        rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10));
                        vector<string> v = split(split_buf[4], ' ');
                        string s = "";
                        for (char c : v[0]){
                            if (c == '.'){
                                rr.rdata.push_back((uint8_t)s.length());
                                for (char i : s) {
                                    rr.rdata.push_back(int(i));
                                }
                                s = "";
                            }
                            else{
                                s += c;
                            }
                        }
                        rr.rdata.push_back(0);

                        for (char c : v[1]){
                            if (c == '.'){
                                rr.rdata.push_back((uint8_t)s.length());
                                for (char i : s) {
                                    rr.rdata.push_back(int(i));
                                }
                                s = "";
                            }
                            else{
                                s += c;
                            }
                        }
                        rr.rdata.push_back(0);

                        uint32_t serial = strtoul(v[2].c_str(), NULL, 10);
                        uint32_t refresh = strtoul(v[3].c_str(), NULL, 10);
                        uint32_t retry = strtoul(v[4].c_str(), NULL, 10);
                        uint32_t expire = strtoul(v[5].c_str(), NULL, 10);
                        uint32_t minimum = strtoul(v[6].c_str(), NULL, 10);
                        serial = htonl(serial);
                        refresh = htonl(refresh);
                        retry = htonl(retry);
                        expire = htonl(expire);
                        minimum = htonl(minimum);

                        for (uint8_t num : uint32to8(serial)) rr.rdata.push_back(num);
                        for (uint8_t num : uint32to8(refresh)) rr.rdata.push_back(num);
                        for (uint8_t num : uint32to8(retry)) rr.rdata.push_back(num);
                        for (uint8_t num : uint32to8(expire)) rr.rdata.push_back(num);
                        for (uint8_t num : uint32to8(minimum)) rr.rdata.push_back(num);
                        rr.rdlength = htons(rr.rdata.size());


                        answers.push_back(rr);
                    }

                }

                reply.hdr.ancount = htons(answers.size());
                // authority section
                if (answers.size() != 0){
                    // NS
                    for (auto r : ns){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(ns.size());
                }
                else{
                    // SOA
                    for (auto r : soa){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(soa.size());
                }
                // no additional section
                break;

            case 15:
                // MX
                ans_domain.clear();
                for (auto record : records){
                    std::vector<string> split_buf = split(record, ',');
                    // std::cout << split_buf[0] << " " << split_buf[3] << '\n';
                    if (split_buf[0] == sub_domain && split_buf[3] == type){

                        resource_record rr;
                        rr.cls = htons(0x0001); // internet
                        rr.type = htons(0x000f); // MX

                        vector<string> v = split(split_buf[4], ' ');
                        uint32_t preference = strtoul(v[0].c_str(), NULL, 10);
                        rr.rdata.push_back(preference / 256);
                        rr.rdata.push_back(preference % 256);

                        int pos = v[1].find('.');
                        ans_domain.push_back(v[1].substr(0, pos));

                        string s = "";
                        for (char c : v[1]){
                            if (c == '.'){
                                rr.rdata.push_back((uint8_t)s.length());
                                for (char i : s) {
                                    rr.rdata.push_back(int(i));
                                }
                                s = "";
                            }
                            else{
                                s += c;
                            }
                        }
                        rr.rdata.push_back(0);

                        rr.rdlength = htons(rr.rdata.size());
                        rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                        rr.name = q.name;

                        answers.push_back(rr);
                    }

                }

                reply.hdr.ancount = htons(answers.size());
                // authority section
                if (answers.size() != 0){
                    // NS
                    for (auto r : ns){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(ns.size());
                }
                else{
                    // SOA
                    for (auto r : soa){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(soa.size());
                }
                // additional section
                arcount = 0;
                for (auto d : ans_domain){
                    for (auto record : records){
                        std::vector<string> split_buf = split(record, ',');
                        if (split_buf[0] == d && split_buf[3] == "A"){
                            resource_record rr;
                            rr.cls = htons(0x0001); // internet
                            rr.type = htons(0x0001); // A
                            rr.rdlength = htons(0x0004); // 32 bits
                            uint8_t* address = new uint8_t(4);
                            if (inet_pton(AF_INET, split_buf[4].data(), address) <= 0){
                                return -fprintf(stderr, "** cannot convert IPv4 address for %s\n",
                                    argv[3]);
                            }
                            for (int i = 0;i < 4;i++){
                                rr.rdata.push_back(address[i]);
                            }

                            rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                            rr.name.clear();
                            rr.name.push_back(d.length());
                            for (char c : d){
                                rr.name.push_back((int)c);
                            }
                            for (uint8_t u : q.name){
                                rr.name.push_back(u);
                            }
                            // rr.name = q.name;

                            answers.push_back(rr);
                            arcount++;
                        }
                        else if (split_buf[0] == d && split_buf[3] == "AAAA"){
                            resource_record rr;
                            rr.cls = htons(0x0001); // internet
                            rr.type = htons(0x001c); // AAAA

                            uint8_t* address = new uint8_t(16);
                            if (inet_pton(AF_INET6, split_buf[4].data(), address) <= 0){
                                return -fprintf(stderr, "** cannot convert IPv6 address for %s\n",
                                    argv[3]);
                            }
                            for (int i = 0;i < 16;i++){
                                rr.rdata.push_back(address[i]);
                            }

                            rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                            rr.name.clear();
                            rr.name.push_back(d.length());
                            for (char c : d){
                                rr.name.push_back((int)c);
                            }
                            for (uint8_t u : q.name){
                                rr.name.push_back(u);
                            }
                            rr.rdlength = htons(16);
                            answers.push_back(rr);
                            arcount++;
                        }
                    }
                }
                reply.hdr.arcount = htons(arcount);
                break;

            case 16:
                // TXT
                for (auto record : records){
                    std::vector<string> split_buf = split(record, ',');
                    // std::cout << split_buf[0] << " " << split_buf[3] << '\n';
                    if (split_buf[0] == sub_domain && split_buf[3] == type){

                        resource_record rr;
                        rr.cls = htons(0x0001); // internet
                        rr.type = htons(0x0010); // TXT

                        string s = "";
                        rr.rdata.push_back((uint8_t)split_buf[4].length());
                        for (char c : split_buf[4]){
                            rr.rdata.push_back((uint8_t)c);
                        }
                        // rr.rdata.push_back(0);

                        rr.rdlength = htons(rr.rdata.size());
                        rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                        rr.name = q.name;

                        answers.push_back(rr);
                    }

                }
                reply.hdr.ancount = htons(answers.size());
                // authority section
                if (answers.size() != 0){
                    // NS
                    for (auto r : ns){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(ns.size());
                }
                else{
                    // SOA
                    for (auto r : soa){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(soa.size());
                }

                // no additional section
                break;

            case 28:
                // AAAA
                for (auto record : records){
                    std::vector<string> split_buf = split(record, ',');
                    // std::cout << split_buf[0] << " " << split_buf[3] << '\n';
                    if (split_buf[0] == sub_domain && split_buf[3] == type){

                        resource_record rr;
                        rr.cls = htons(0x0001); // internet
                        rr.type = htons(0x001c); // AAAA

                        uint8_t* address = new uint8_t(16);
                        if (inet_pton(AF_INET6, split_buf[4].data(), address) <= 0){
                            return -fprintf(stderr, "** cannot convert IPv6 address for %s\n",
                                argv[3]);
                        }
                        for (int i = 0;i < 16;i++){
                            rr.rdata.push_back(address[i]);
                        }

                        rr.ttl = htonl(strtoul(split_buf[1].c_str(), NULL, 10)); // not implemented
                        rr.name = q.name;
                        rr.rdlength = htons(16);
                        answers.push_back(rr);
                    }

                }
                reply.hdr.ancount = htons(answers.size());
                // authority section
                if (answers.size() != 0){
                    // NS
                    for (auto r : ns){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(ns.size());
                }
                else{
                    // SOA
                    for (auto r : soa){
                        answers.push_back(r);
                    }
                    reply.hdr.nscount = htons(soa.size());
                }

                // no additional section

                break;
            default:
                break;
            }

            vector<uint8_t> rply;
            rply = send_reply(reply.hdr, reply.q, answers);
            int send = sendto(sockfd, rply.data(), rply.size(), 0, (struct sockaddr*)&csin, sizeof(csin));
            printf("send: %d\n", send);
            fs.close();
        }
        else{
            // not in local
            struct sockaddr_in fsin;
            fsin.sin_port = htons(53);
            fsin.sin_family = AF_INET;
            socklen_t fsinlen = sizeof(fsin);
            inet_pton(AF_INET, forward_ip.c_str(), &fsin.sin_addr);

            int sb1 = sendto(sockfd, buffer, recvlen, 0, (struct sockaddr*)&fsin, sizeof(fsin));
            std::cout << "send1: " << sb1 << '\n';
            recvlen = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&fsin, &fsinlen);
            int sb2 = sendto(sockfd, buffer, recvlen, 0, (struct sockaddr*)&csin, sizeof(csin));

            printf("send1: %d, send2: %d, recv: %ld\n", sb1, sb2, recvlen);
        }
    }

    return 0;
}