/* cygwinextra.h -- definitions needed for cygwin (currently 1.1) 
 *		    These are defined for most UNIXes but not cygwin
 */

/* Should be in sys/time.h. */
#define timercmp(tm1, tm2, op) ( \
   ((tm1)->tv_sec == (tm2)->tv_sec \
        ? (int) (tm1)->tv_usec - (int) (tm2)->tv_usec \
        : (int) (tm1)->tv_sec - (int) (tm2)->tv_sec) op 0)

#define timerclear(tm) ((tm)->tv_sec = 0, (tm)->tv_usec = 0)
#define timerisset(tm) ((tm)->tv_sec || (tm)->tv_usec)

/* the pure Cygnus version of gettimeofday returns 1 on success */
/* #define gettimeofday(a,b) (gettimeofday(a,b) == -1 ? -1 : 0) */

/* Should be in stdlib.h */
int mkstemp(char *);

  
