/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _TIMECALLS_H_
#define _TIMECALLS_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
    
#include <stdio.h>
#include <sys/time.h>
#include <sys/file.h>
#include "coda_assert.h"
#include <mach.h>

#include <sys/ioctl.h>
#ifdef __cplusplus
}
#endif __cplusplus

extern int clockFD;
extern struct hgram Create_Total_hg, Remove_Total_hg, Link_Total_hg;
extern struct hgram Rename_Total_hg, MakeDir_Total_hg,
	      RemoveDir_Total_hg, SymLink_Total_hg, SpoolVMLogRecord_hg,
    	PutObjects_Transaction_hg, PutObjects_TransactionEnd_hg, PutObjects_Inodes_hg, PutObjects_RVM_hg;

#ifdef  __STDC__
#define NSC_SHOW_COUNTER_INFO   _IO('c', 0)
#define NSC_GET_COUNTER         _IOR('c', 1, long)
#else   __STDC__
#define NSC_SHOW_COUNTER_INFO   _IO(c, 0)
#define NSC_GET_COUNTER         _IOR(c, 1, long)
#endif  __STDC__

#ifdef _TIMECALLS_
#define START_NSC_TIMING(id) START_TIMING(id) \
			unsigned long a/**/id, b/**/id;			\
                        float timediff/**/id;					\
                        struct timeval after/**/id, before/**/id;		\
                        { 		                                        \
			   if (clockFD > 0)                                     \
			       ioctl(clockFD, NSC_GET_COUNTER, &b/**/id);	\
			   else							\
			       gettimeofday(&before/**/id, 0);			\
			}

#define END_NSC_TIMING(id) { END_TIMING(id) \
			    if (clockFD > 0) {					\
			        ioctl(clockFD, NSC_GET_COUNTER, &a/**/id);	\
				if (a/**/id > b/**/id)				\
				    timediff/**/id = (a/**/id - b/**/id) / 25;	\
				else {						\
				    float tmp1 = (b/**/id - a/**/id) / 25;	\
				    timediff/**/id = (171798691.84 - tmp1);	\
				}						\
			    } else {						\
			        gettimeofday(&after/**/id, 0);			\
			        timediff/**/id = (after/**/id.tv_sec - before/**/id.tv_sec) * 1000000 + after/**/id.tv_usec - before/**/id.tv_usec; \
			    }							\
				UpdateHisto(&id/**/_hg, (double)timediff/**/id);\
			  }
#else /* !_TIMECALLS_ */
#define START_NSC_TIMING(id) START_TIMING(id)
#define END_NSC_TIMING(id) END_TIMING(id)
#endif

#endif /* _TIMECALLS_H_ */

