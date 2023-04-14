#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <iostream>
using namespace std;

int main(int argc, char* argv[]){
    cout << argc << " " << argv[0] << endl;

    string s = "21341";
    char buf[64] = { "12345\0" };
    s = buf;
    cout << s.size() << endl;

    return 0;
}