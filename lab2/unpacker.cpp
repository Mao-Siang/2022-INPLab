#include <bits/stdc++.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
using namespace std;

struct header {
    uint32_t magic;
    int32_t  off_str;
    int32_t  off_dat;
    uint32_t n_files;
}__attribute((packed)) pako_header;


struct FILE_E{
    int32_t off_str;
    uint32_t size;
    int32_t off_con;
    uint64_t checksum;
}__attribute((packed));

int main(int argc, char* argv[]){
    if (argc != 3) {
        cerr << "usage: " << argv[0] << " pak_file directory" << endl;
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    read(fd, &pako_header, sizeof(pako_header));

    uint32_t tot_size = sizeof(FILE_E) * pako_header.n_files;
    vector<FILE_E> files(pako_header.n_files);
    read(fd, files.data(), tot_size);

    //convert endian
    for (int i = 0;i < pako_header.n_files; i++){
        files[i].size = ntohl(files[i].size);
        files[i].checksum = ntohll(files[i].checksum);
    }

    //read file names
    vector<string> file_name;
    for (int i = 0;i < pako_header.n_files;i++){
        lseek(fd, pako_header.off_str + files[i].off_str, SEEK_SET);
        int8_t word;
        string name = "/";
        while (read(fd, &word, 1) == 1 && word != 0){
            cout << word;
            name += word;
        }
        file_name.push_back(name);
        cerr << "size: " << files[i].size << endl;
    }

    //unpack files
    for (int i = 0;i < pako_header.n_files;i++){
        lseek(fd, pako_header.off_dat + files[i].off_con, SEEK_SET);
        // confirm checksum
        uint64_t checksum = 0;
        uint32_t data_size = ((files[i].size % 8u != 0) ? files[i].size + 8u - (files[i].size % 8u) : files[i].size);

        cerr << "data size:" << data_size << endl;

        vector<uint64_t> data(data_size / 8, 0);
        read(fd, data.data(), files[i].size);

        for (auto& num : data) checksum ^= num;

        cerr << "checksum: " << (checksum ^ files[i].checksum) << endl;

        if (checksum != files[i].checksum) continue;

        //unpack data
        vector<uint8_t> content(files[i].size);
        lseek(fd, pako_header.off_dat + files[i].off_con, SEEK_SET);
        read(fd, content.data(), files[i].size);

        // write data
        string dst = argv[2] + file_name[i];
        int fout = creat(dst.c_str(), 0666);
        write(fout, content.data(), files[i].size);

        close(fout);
    }

    close(fd);
    return 0;
}

