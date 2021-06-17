#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "my_signal.h"
#include "logUtil.h"

int usage()
{
    char msg[] = "Usage: server unix_domain_path";
    fprintf(stderr, "%s\n", msg);

    return 0;
}

void sig_chld(int signo)
{
    pid_t pid;
    int stat;
    while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        ;
    }
    return;
}

int child_proc(int connfd)
{
    char buf[64*1024];
    memset(buf, 'X', sizeof(buf));
    unsigned long total_bytes = 0;
    for ( ; ; ) {
        int n = write(connfd, buf, sizeof(buf));
        if (n < 0) {
            if ((errno == ECONNRESET) || (errno == EPIPE)) {
                fprintfwt(stderr, "server: connection reset by client. wrote %ld bytes\n", total_bytes);
                break;
            }
            else {
                err(1, "write");
            }
        }
        if (n == 0) { /* EOF */
            fprintfwt(stderr, "EOF. wrote %ld bytes\n", total_bytes);
            close(connfd);
        }
        total_bytes += n;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        usage();
        exit(1);
    }

    char *unix_domain_path = argv[1];
    int listenfd = socket(AF_LOCAL, SOCK_STREAM, 0);

    my_signal(SIGCHLD, sig_chld);
    my_signal(SIGPIPE, SIG_IGN);

    unlink(unix_domain_path);
    struct sockaddr_un cliaddr, servaddr;
    memset(&cliaddr,  0, sizeof(cliaddr));
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, unix_domain_path);

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        err(1, "bind");
    }
    if (listen(listenfd, 10) < 0) {
        err(1, "listen");
    }

    for ( ; ; ) {
        socklen_t clilen = sizeof(cliaddr);
        int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
        if (connfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                err(1, "accept");
            }
        }
        
        pid_t pid = fork();
        if (pid < 0) {
            err(1, "fork");
        }
        if (pid == 0) { /* child */
            if (close(listenfd) < 0) {
                err(1, "close listenfd");
            }
            child_proc(connfd /*, bufsize, sleep_usec, rate */);
            exit(0);
        }
        else {
            close(connfd);
        }
    }
    return 0;
}
