#include <iostream>
#include <signal.h>

void sig_term(int s){
    std::cerr << "SIGTERM " << s << std::endl;
    std::exit(0);
}



int main(int argc, char** argv)
{
    signal(SIGTERM, sig_term);
    while (true);
}