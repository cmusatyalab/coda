#include <sys/time.h>
#include <time.h>
#include <unistd.h>

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    int res;
    
    res = _gettimeofday(tv, tz);
    time(&tv->tv_sec);

    return res;
}
