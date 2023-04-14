#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string>
#include <cstring>
#include <sys/wait.h>

using sighandler_t = void(*)(int);

void handler(int sig)
{
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0){
        printf("child %d terminated.\n", pid);
    }
    return;
}

int main(int argc, char* argv[])
{
    int					listenfd, connfd;
    pid_t				childpid;
    socklen_t			clilen;
    struct sockaddr_in	cliaddr, servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(std::stoi(argv[1]));

    bind(listenfd, (const sockaddr*)&servaddr, sizeof(servaddr));

    listen(listenfd, 1024);
    signal(SIGCHLD, handler);

    for (; ; ) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (sockaddr*)&cliaddr, &clilen);

        printf("%d\n", cliaddr.sin_port);


        if ((childpid = fork()) == 0) {	/* child process */
            close(listenfd);	/* close listening socket */

            int newfd = dup(1);
            dup2(connfd, 1);

            if (newfd == -1) perror("dup");

            char* envp[] = { NULL };

            std::string ag = argv[2];
            if (ag == "date"){
                char* arg[] = { NULL };
                int e = execve("/usr/bin/date", arg, envp);
                if (e == -1){
                    perror("date Error");
                }
            }
            else if (ag[0] == '/'){

                char* arg[argc - 2];
                for (int i = 0;i < argc - 2;i++) arg[i] = argv[i + 2];
                arg[argc - 3] = NULL;

                int e = execve(ag.c_str(), arg, envp);
                if (e == -1){
                    perror("Error");
                    close(connfd);
                    exit(-1);
                }
            }
            else if (ag == "ls"){
                dup2(connfd, 2);
                char* arg[] = { argv[2], argv[3],argv[4],argv[5],argv[6], NULL };
                int e = execve("/usr/bin/ls", arg, envp);
                if (e == -1) {
                    perror("ls Error");
                    exit(-1);
                }
            }
            else if (ag == "timeout"){
                int cinfd = dup(0);
                dup2(connfd, 0);
                char* arg[] = { argv[2],argv[3],argv[4],argv[5],argv[6], NULL };

                int e = execve("/usr/bin/timeout", arg, envp);
                if (e == -1){
                    perror("timeout Error");
                    exit(-1);
                }
            }

            exit(0);
        }
        close(connfd);			/* parent closes connected socket */
    }
}