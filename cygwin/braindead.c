#include <sys/time,h>
#include <time,h>
#include <unistd,h>

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    gettimeofday(tv, tz);
    time(&tv->tv_sec);
}
