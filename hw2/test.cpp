#include <iostream>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

struct data{
    char* c;
    string s;
};

int main(){

    int fd = open("a.txt", O_RDONLY);
    struct data d;
    read(fd, &d, sizeof(d));

    cout << d.c << " " << d.s << endl;

    return 0;
}