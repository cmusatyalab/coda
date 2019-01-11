/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/* timing.c 
 * class for recording timevalues stamped with an id
 * 	post processing prints out the delta between the 
 *	different times 
 * This package does not do any locking - external 
 *	synchronization is required for correct functionality
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "coda_string.h"
#include <sys/time.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "coda_assert.h"

#ifdef __cplusplus
}
#endif

#include "timing.h"

/* c = a - b where a, b, c are timevals */
void tvaminustvb(struct timeval *a, struct timeval *b, struct timeval *c)
{
    int carryover = 0;

    if (a->tv_usec < b->tv_usec) {
        carryover  = 1;
        c->tv_usec = a->tv_usec + 1000000 - b->tv_usec;
    } else
        c->tv_usec = a->tv_usec - b->tv_usec;
    if (carryover)
        c->tv_sec = a->tv_sec - b->tv_sec - 1;
    else
        c->tv_sec = a->tv_sec - b->tv_sec;
}

timing_path::timing_path(int n)
{
    nentries   = 0;
    maxentries = n;
    arr        = (tpe *)malloc(sizeof(tpe) * n);
}

timing_path::~timing_path()
{
    free(arr);

    nentries = 0;
}
void timing_path::grow_storage()
{
    CODA_ASSERT(nentries == maxentries);
    tpe *tmparr = 0;
    if (maxentries) {
        tmparr = (tpe *)malloc(sizeof(tpe) * 2 * maxentries);
        memcpy(tmparr, arr, sizeof(tpe) * maxentries);
        maxentries += maxentries;
    } else {
        tmparr = (tpe *)malloc(sizeof(tpe) * TIMEGROWSIZE);
        maxentries += TIMEGROWSIZE;
    }
    CODA_ASSERT(tmparr);
    free(arr);
    arr = tmparr;
}

void timing_path::insert(int id)
{
    if (nentries >= maxentries)
        grow_storage();
    arr[nentries].id = id;

#ifdef _NSC_TIMING_
    extern clockFD;
#define NSC_GET_COUNTER _IOR('c', 1, long)
    if (clockFD > 0) {
        arr[nentries].tv.tv_sec = 0;
        ioctl(clockFD, NSC_GET_COUNTER, &arr[nentries].tv.tv_usec);
    }
#else /* _NSC_TIMING_ */
    gettimeofday(&arr[nentries].tv, NULL);
#endif
    nentries++;
}

void timing_path::postprocess()
{
    postprocess(stdout);
}
void timing_path::postprocess(FILE *fp)
{
    fflush(fp);
    postprocess(fileno(fp));
}
void timing_path::postprocess(int fd)
{
    char buf[256];
    if (!nentries) {
        sprintf(buf, "PostProcess: No entries\n");
        write(fd, buf, strlen(buf));
    } else {
        timeval difft;
        sprintf(buf, "There are %d entries\n", nentries);
        write(fd, buf, strlen(buf));
        sprintf(buf, "Entry[0] is id: %d time (%lu.%lu)\n", arr[0].id,
                arr[0].tv.tv_sec, arr[0].tv.tv_usec);
        write(fd, buf, strlen(buf));
#ifdef _NSC_TIMING_
        for (int i = 1; i < nentries; i++) {
            difft.tv_sec = 0;
            if (arr[i].tv.tv_usec > arr[i - 1].tv.tv_usec)
                difft.tv_usec =
                    (arr[i].tv.tv_usec - arr[i - 1].tv.tv_usec) / 25;
            else {
                difft.tv_usec =
                    (arr[i - 1].tv.tv_usec - arr[i].tv.tv_usec) / 25;
                difft.tv_usec = 171798692 - difft.tv_usec;
            }
            sprintf(buf,
                    "Entry[%d] id: %d time (%u.%u), delta (%u secs %u usecs)\n",
                    i, arr[i].id, arr[i].tv.tv_sec, arr[i].tv.tv_usec,
                    difft.tv_sec, difft.tv_usec);
            write(fd, buf, strlen(buf));
        }
        if (nentries > 1) {
            difft.tv_sec = 0;
            if (arr[nentries - 1].tv.tv_usec > arr[0].tv.tv_usec)
                difft.tv_usec =
                    (arr[nentries - 1].tv.tv_usec - arr[0].tv.tv_usec) / 25;
            else {
                difft.tv_usec =
                    (arr[0].tv.tv_usec - arr[nentries - 1].tv.tv_usec) / 25;
                difft.tv_usec = 171798692 - difft.tv_usec;
            }
            sprintf(
                buf,
                "Final delta between entry id %d and %d is %u secs %u usecs\n",
                arr[nentries - 1].id, arr[0].id, difft.tv_sec, difft.tv_usec);
            write(fd, buf, strlen(buf));
        }
#else /* _NSC_TIMING_ */
        for (int i = 1; i < nentries; i++) {
            tvaminustvb(&arr[i].tv, &arr[i - 1].tv, &difft);
            sprintf(
                buf,
                "Entry[%d] id: %d time (%lu.%lu), delta (%lu secs %lu usecs)\n",
                i, arr[i].id, arr[i].tv.tv_sec, arr[i].tv.tv_usec, difft.tv_sec,
                difft.tv_usec);
            write(fd, buf, strlen(buf));
        }
        if (nentries > 1) {
            tvaminustvb(&arr[nentries - 1].tv, &arr[0].tv, &difft);
            sprintf(
                buf,
                "Final delta between entry id %d and %d is %lu secs %lu usecs\n",
                arr[nentries - 1].id, arr[0].id, difft.tv_sec, difft.tv_usec);
            write(fd, buf, strlen(buf));
        }
#endif /* _NSC_TIMING_ */
    }
}
