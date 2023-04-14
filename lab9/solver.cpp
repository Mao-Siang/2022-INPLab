#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <sys/un.h>
#include <string>
#include <vector>

#define UNIXSTR_PATH "/sudoku.sock"

std::vector<std::vector<int>> board(9, std::vector<int>(9, 0));
std::vector<std::pair<int, int>> pos;

std::vector<int> find_valid(int x, int y){
    int num[10] = { 0 };
    // same row and col
    for (int i = 0;i < 9;i++){
        num[board[x][i]] = 1;
        num[board[i][y]] = 1;
    }

    // same block
    int row_start = 3 * (x / 3), col_start = 3 * (y / 3);
    for (int i = 0;i < 3;i++){
        for (int j = 0;j < 3;j++){
            num[board[row_start + i][col_start + j]] = 1;
        }
    }
    std::vector<int> ans;
    for (int i = 1;i < 10;i++){
        if (num[i] == 0) ans.push_back(i);
    }
    return ans;
}

bool solve(int cur){
    if (cur == pos.size()) return true;
    // printf("cur: %d\n", cur);
    int x = pos[cur].first, y = pos[cur].second;
    std::vector<int> valid = find_valid(x, y);
    if (valid.empty()) return false;

    for (int num : valid){
        board[x][y] = num;
        if (!solve(cur + 1)){
            board[x][y] = 0;
        }
        else return true;
    }

    return false;
}

int main(int argc, char** argv)
{

    int sockfd;
    struct sockaddr_un servaddr;

    sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, UNIXSTR_PATH);

    connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    write(sockfd, "S", 1);
    char buf[100];
    memset(buf, 0, sizeof(buf));

    read(sockfd, buf, sizeof(buf));
    std::string serial = buf;
    serial = serial.substr(serial.find(" ") + 1);
    // printf("my: %s\n", serial.c_str());

    // map to a 2d vector

    for (int i = 0;i < 9;i++){
        for (int j = 0;j < 9;j++){
            if (serial[i * 9 + j] == '.'){
                pos.emplace_back(i, j);
            }
            else board[i][j] = (serial[i * 9 + j] - '0');
        }
    }

    bool solved = solve(0);
    if (solved) printf("solved!\n");

    // for (int i = 0;i < 9;i++){
    //     for (int j = 0;j < 9;j++){
    //         printf("%d ", board[i][j]);
    //     }
    //     puts("\n");
    // }

    for (auto p : pos){
        std::string msg = "V " + std::to_string(p.first) + " " + std::to_string(p.second) + " " + std::to_string(board[p.first][p.second]);
        write(sockfd, msg.c_str(), msg.size());
        // memset(buf, 0, sizeof(buf));
        read(sockfd, buf, sizeof(buf));
        // puts(buf);
    }

    write(sockfd, "C", 1);
    sleep(5);
    write(sockfd, "Q", 1);

    close(sockfd);

    return 0;
}