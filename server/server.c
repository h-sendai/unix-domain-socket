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
#include "get_num.h"

int usage()
{
    char msg[] = "Usage: server [-b bufsize] unix_domain_path\n"
                 "Options\n"
                 "-b bufsize: bufsize.  suffix k for kilo, m for mega.  Default 32kB.\n";
    fprintf(stderr, "%s", msg);

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

int child_proc(int connfd, int bufsize)
{
    char *buf = malloc(bufsize);
    memset(buf, 'X', bufsize);
    unsigned long total_bytes = 0;
    struct timeval start;
    gettimeofday(&start, NULL);
    for ( ; ; ) {
        int n = write(connfd, buf, bufsize);
        if (n < 0) {
            if ((errno == ECONNRESET) || (errno == EPIPE)) {
                struct timeval stop, elapsed;
                gettimeofday(&stop, NULL);
                timersub(&stop, &start, &elapsed);
                double elapsed_sec = elapsed.tv_sec + 0.000001*elapsed.tv_usec;
                double transfer_rate_mb_s = total_bytes / elapsed_sec / 1024.0 / 1024.0;
                fprintfwt(stderr, "server: connection reset by client. wrote %ld bytes. %.3f MB/s\n", total_bytes, transfer_rate_mb_s);
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
    int c;
    int bufsize = 32*1024;
    while ( (c = getopt(argc, argv, "b:h")) != -1) {
        switch (c) {
            case 'b':
                bufsize = get_num(optarg);
                break;
            case 'h':
                usage();
                exit(0);
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 1) {
        usage();
        exit(1);
    }

    char *unix_domain_path = argv[0];
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
            child_proc(connfd, bufsize /*, sleep_usec, rate */);
            exit(0);
        }
        else {
            close(connfd);
        }
    }
    return 0;
}
