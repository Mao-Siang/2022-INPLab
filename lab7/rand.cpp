#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string>

char* gen_secret() {
    static int len;
    static char buf[64];

    printf("** Thank you for your message, let me generate a secret for you.\n");
    len = snprintf(buf, 10, "%x", rand());
    len += snprintf(buf + len, 10, "%x", rand());
    len += snprintf(buf + len, 10, "%x", rand());
    len += snprintf(buf + len, 10, "%x", rand());
    buf[len] = '\0';
    return buf;
}
int main(){
    int seed = 0;

    scanf("%d", &seed);

    srand(seed);
    puts(gen_secret());
    rand();
    puts(gen_secret());
    return 0;
}