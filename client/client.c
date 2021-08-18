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
#include "set_cpu.h"

#define DEFAULT_UNIX_DOMAIN_PATH "/tmp/unix"
volatile sig_atomic_t has_alrm = 0;
volatile sig_atomic_t has_int  = 0;

int usage()
{
    char msg[] = "Usage: client [-b bufsize] [-c cpu_num] [unix_domain_path]\n"
                 "If unix_domain_path is not specified, default unix domain path is /tmp/unix\n"
                 "Options\n"
                 "-b bufsize: bufsize.  suffix k for kilo, m for mega.  Default 32kB\n"
                 "-c cpu_num: set cpu number running on the client program.\n";

    fprintf(stderr, "%s", msg);

    return 0;
}

void sig_alrm(int signo)
{
    has_alrm = 1;
    return;
}

void sig_int(int signo)
{
    has_int = 1;
    return;
}

int main(int argc, char *argv[])
{
    int c;
    int bufsize = 32*1024; /* default bufsize 32kB */
    int cpu_num = -1;
    while ( (c = getopt(argc, argv, "b:c:h")) != -1) {
        switch (c) {
            case 'b':
                bufsize = get_num(optarg);
                break;
            case 'c':
                cpu_num = strtol(optarg, NULL, 0);
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

    if (cpu_num != -1) {
        if (set_cpu(cpu_num) < 0) {
            exit(1);
        }
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

    my_signal(SIGINT,  sig_int);
    my_signal(SIGTERM, sig_int);
    my_signal(SIGALRM, sig_alrm);

    set_timer(1, 0, 1, 0);

    for ( ; ; ) {
        if (has_alrm) {
            gettimeofday(&now, NULL);
            timersub(&now, &start, &elapsed);
            timersub(&now, &prev,  &interval);
            unsigned long interval_bytes = total_bytes - prev_total_bytes;
            double interval_sec = interval.tv_sec + 0.000001*interval.tv_usec;
            double transfer_rate      = (double) interval_bytes / (double) interval_sec / 1024.0 / 1024.0;
            double transfer_rate_gbps = (double) interval_bytes * 8 / (double) interval_sec / 1000000000.0;
            printf("%ld.%06ld %.3f MB/s %.3f Gbps\n", elapsed.tv_sec, elapsed.tv_usec, transfer_rate, transfer_rate_gbps);
            prev_total_bytes = total_bytes;
            prev = now;
            has_alrm = 0;
        }
        if (has_int) {
            gettimeofday(&now, NULL);
            timersub(&now, &start, &elapsed);
            double running_sec = elapsed.tv_sec + 0.000001*elapsed.tv_usec;
            double transfer_rate      = (double) total_bytes / running_sec / 1024.0 / 1024.0;
            double transfer_rate_gbps = (double) total_bytes * 8 / running_sec / 1000000000.0;
            printf("transfer_rate: %.3f MB/s %.3f Gbps runnging %.3f seconds\n", transfer_rate, transfer_rate_gbps, running_sec);
            exit(0);
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
