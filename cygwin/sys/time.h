
#include "i586-pc-cygwin32/include/sys/time.h"
#include <time.h>

#define timercmp(tm1, tm2, op) ( \
   ((tm1)->tv_sec == (tm2)->tv_sec \
        ? (int) (tm1)->tv_usec - (int) (tm2)->tv_usec \
        : (int) (tm1)->tv_sec - (int) (tm2)->tv_sec) op 0)

#define timerclear(tm) ((tm)->tv_sec = 0, (tm)->tv_usec = 0)
#define timerisset(tm) ((tm)->tv_sec || (tm)->tv_usec)

#define setitimer(a,b,c) (printf("setitimer() not supported in CYGWIN32\n"), exit(1))

#ifndef __CYGWIN32__
#if 0
#endif

#define gettimeofday(a,b) (gettimeofday(a,b) == 1 ? 0 : -1)
#endif
