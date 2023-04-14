#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <fcntl.h>
using namespace std;

int main(){
    // socket
    int s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(10002);
    inet_pton(AF_INET, "140.113.213.213", &sin.sin_addr);
    // connect
    int c = connect(s, (const sockaddr*)&sin, sizeof(sin));

    string start = "GO\n";
    write(s, "GO\n", 3);


    vector<string>content;
    string buf = "";
    char w;
    while (read(s, &w, 1)){
        if (w != '\n')
        {
            buf += w;
        }
        else {
            content.push_back(buf);
            buf = "";
        }

        if (content.size() == 6) break;
    }

    int size = (content[3].size());
    string ans = "\n";
    while (size != 0){
        ans = (char)('0' + (size % 10)) + ans;
        size /= 10;
    }

    cout << ans;
    // cout << "send:" << write(s, ans.c_str(), ans.length()) << endl;

    char result[4096] = {};
    read(s, result, 4096);
    // while (read(s, &w, 1)){
    //     result += w;
    // }
    cout << "result: " << result << endl;


    close(s);
    return 0;
}
