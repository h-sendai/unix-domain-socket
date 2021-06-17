#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int usage()
{
    char msg[] = "Usage: server unix_domain_path";
    fprintf(stderr, "%s\n", msg);

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

    socklen_t clilen = sizeof(cliaddr);
    int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
    if (connfd < 0) {
        err(1, "accept");
    }

    char buf[64*1024];
    memset(buf, 'X', sizeof(buf));
    unsigned long total_bytes = 0;
    for ( ; ; ) {
        int n = write(connfd, buf, sizeof(buf));
        if (n < 0) {
            fprintf(stderr, "wrote %ld bytes\n", total_bytes);
            err(1, "read");
        }
        if (n == 0) { /* EOF */
            fprintf(stderr, "wrote %ld bytes\n", total_bytes);
            close(connfd);
        }
        total_bytes += n;
    }

    return 0;
}
