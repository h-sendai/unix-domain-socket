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

#include "my_signal.h"
#include "set_timer.h"

volatile sig_atomic_t has_alrm = 0;

int usage()
{
    char msg[] = "Usage: client unix_domain_path";
    fprintf(stderr, "%s\n", msg);

    return 0;
}

void sig_alrm(int signo)
{
    has_alrm = 1;
    return;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        usage();
        exit(1);
    }

    char *unix_domain_path = argv[1];
    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);

    struct sockaddr_un servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, unix_domain_path);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        err(1, "connect");
    }

    my_signal(SIGALRM, sig_alrm);
    set_timer(1, 0, 1, 0);

    char buf[64*1024];
    struct timeval start, elapsed, now, prev, interval;
    gettimeofday(&start, NULL);
    prev = start;
    unsigned long total_bytes = 0;
    unsigned long prev_total_bytes = 0;
    for ( ; ; ) {
        if (has_alrm) {
            gettimeofday(&now, NULL);
            timersub(&now, &start, &elapsed);
            timersub(&now, &prev,  &interval);
            unsigned long interval_bytes = total_bytes - prev_total_bytes;
            double interval_sec = interval.tv_sec + 0.000001*interval.tv_usec;
            double transfer_rate = (double) interval_bytes / (double) interval_sec / 1024.0 / 1024.0;
            printf("%ld.%06ld %.3f MB/s\n", elapsed.tv_sec, elapsed.tv_usec, transfer_rate);
            prev_total_bytes = total_bytes;
            prev = now;
            has_alrm = 0;
        }
        int n = read(sockfd, buf, sizeof(buf));
        if (n < 0) {
            fprintf(stderr, "read %ld bytes\n", total_bytes);
            err(1, "read");
        }
        if (n == 0) { /* EOF */
            fprintf(stderr, "read %ld bytes\n", total_bytes);
            close(sockfd);
        }
        total_bytes += n;
    }

    return 0;
}
