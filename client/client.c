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
#include "get_num.h"

#define DEFAULT_UNIX_DOMAIN_PATH "/tmp/unix"
volatile sig_atomic_t has_alrm = 0;

int usage()
{
    char msg[] = "Usage: client [-b bufsize] [unix_domain_path]\n"
                 "If unix_domain_path is not specified, default unix domain path is /tmp/unix\n"
                 "Options\n"
                 "-b bufsize: bufsize.  suffix k for kilo, m for mega.  Default 32kB\n";
    fprintf(stderr, "%s", msg);

    return 0;
}

void sig_alrm(int signo)
{
    has_alrm = 1;
    return;
}

int main(int argc, char *argv[])
{
    int c;
    int bufsize = 32*1024; /* default bufsize 32kB */
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

    char *unix_domain_path;
    if (argc == 0) {
        unix_domain_path = DEFAULT_UNIX_DOMAIN_PATH;
    }
    else if (argc == 1) {
        unix_domain_path = argv[0];
    }
    else {
        usage();
        exit(1);
    }

    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);

    struct sockaddr_un servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, unix_domain_path);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        err(1, "connect");
    }

    char *buf = malloc(bufsize);
    if (buf == NULL) {
        err(1, "malloc for buf");
    }
    struct timeval start, elapsed, now, prev, interval;
    gettimeofday(&start, NULL);
    prev = start;
    unsigned long total_bytes = 0;
    unsigned long prev_total_bytes = 0;

    my_signal(SIGALRM, sig_alrm);
    set_timer(1, 0, 1, 0);

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
        int n = read(sockfd, buf, bufsize);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                fprintf(stderr, "read %ld bytes\n", total_bytes);
                err(1, "read");
            }
        }
        if (n == 0) { /* EOF */
            fprintf(stderr, "read %ld bytes\n", total_bytes);
            close(sockfd);
        }
        total_bytes += n;
    }

    return 0;
}
