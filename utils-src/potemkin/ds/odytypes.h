/*
** odytypes.h
**
** the every-odyssey-file-should-have-them definitions
**
** Types: bool        boolean variables
**        magic_t     magic number tags for structures
**        CMPFN       comparison function of two anonymous objects
**        TIMEVAL     a time value; currently a struct timeval
**                    might change with better machines.  Full precision.
**
** Macros: CODA_ASSERT     use instead of CODA_ASSERT.h
**         ALLOC      use instead of malloc for objects
**         NALLOC     use instead of malloc for arrays
**         FREE       use instead of free
**         TIME       get current time
**         TIMEDIFF   difference in time, in TIMEVAL units.
*/

#ifndef _ODYTYPES_H_
#define _ODYTYPES_H_

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

/**************************************** prototypes */
#ifndef __P
#ifdef __STDC__
#define __P(args)     args
#else /* __STDC__ */
#define __P(args)     ()
#endif /* __STDC__ */
#endif /* __P */


/**************************************** booleans */
typedef enum {TRUE =1, FALSE=0} bool;

/*************************************** magic number tags */
typedef unsigned long magic_t;

/*************************************** Common function pointer types */

#ifdef __STDC__

typedef long (*COMPFN)(void*,void*);  /* used by data structure routines */

#else /* __STDC__ */

typdef int (*COMPFN)();

#endif /* __STDC__ */

/*************************************** TIMEVAL */

typedef struct timeval TIMEVAL;

/*************************************** assertions */
#ifndef __FILE__
#define __FILE__ "()"
#endif

#ifndef __LINE__
#define __LINE__ 0
#endif

#ifdef __STDC__

#define CODA_ASSERT(cond) 						      \
do {                                                                  \
    if (!(cond)) {                                                    \
	int *j = 0;                                                   \
	fprintf(stderr, "Assert (%s) in %s:%d\n",                     \
		#cond, __FILE__, __LINE__);                           \
	fflush(stderr);                                               \
	*j = 1; /* cause a real SIGTRAP to happen: can be caught */   \
    }                                                                 \
} while (0)

#else /* __STDC__ */

#define CODA_ASSERT(cond) 						      \
do {                                                                  \
    if (!(cond)) {                                                    \
	int *j = 0;                                                   \
	fprintf(stderr, "Assert failed in %s:%d\n",                   \
		__FILE__, __LINE__);                                  \
	fflush(stderr);                                               \
	*j = 1; /* cause a real SIGTRAP to happen: can be caught */   \
    }                                                                 \
} while (0)

#endif /* __STDC__ */

/*************************************** Allocation, deallocation, validity */

#define ALLOC(X,T)                                                \
do {                                                              \
    if (((X) = (T *)malloc(sizeof(T))) == NULL)                   \
	CODA_ASSERT(0);                                                \
} while (0)

#define NALLOC(X,T,S)                                  \
do {                                                   \
    if (((X) = (T *)malloc(sizeof(T)*(S))) == NULL) {  \
	CODA_ASSERT(0);                                     \
    }                                                  \
} while (0)

#define FREE(X)           \
do {                      \
    if ((X) != NULL) {    \
	free((X));        \
	(X) = NULL;       \
    }                     \
} while (0)

/*************************************** time operations */

/* tvp should be of type TIMEVAL */
#define TIME(tv)    (gettimeofday(&(tv),NULL))

/* tvb, tve, tvr should be of type TIMEVAL */
/* tve should be >= tvb. */
#define TIMEDIFF(tvb,tve,tvr)                           \
do {                                                    \
    CODA_ASSERT(((tve).sec > (tvb).sec)                      \
	   || (((tve).sec == (tvb).sec)                 \
	       && ((tve).usec > (tvb).usec)));          \
    if ((tve).usec < (tvb).usec) {                      \
	(tvr).usec = 1000000 + (tve).usec - (tvb).usec; \
	(tvr).sec = (tve).sec - (tvb).sec - 1;          \
    } else {                                            \
	(tvr).usec = (tve).usec - (tvb).usec;           \
	(tvr).sec = (tve).sec - (tvb).sec;              \
    }                                                   \
} while (0)


#endif /* _ODYTYPES_H_ */
