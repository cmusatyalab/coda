#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vice/timecalls.h,v 4.1 1997/01/08 21:52:01 rvb Exp $";
#endif /*_BLURB_*/



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

#define START_NSC_TIMING(id)	unsigned long a/**/id, b/**/id;			\
                        float timediff/**/id;					\
                        struct timeval after/**/id, before/**/id;		\
                        { 		                                        \
			   if (clockFD > 0)                                     \
			       ioctl(clockFD, NSC_GET_COUNTER, &b/**/id);	\
			   else							\
			       gettimeofday(&before/**/id, 0);			\
			}

#define END_NSC_TIMING(id) {							\
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

