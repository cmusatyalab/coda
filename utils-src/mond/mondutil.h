#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/utils-src/mond/mondutil.h,v 3.2 1995/10/09 19:27:00 satya Exp $";
#endif /*_BLURB_*/



#ifndef _MONDUTIL_H_
#define _MONDUTIL_H_

extern void SetDate(void);
extern int DateChanged(void);
extern void InitRPC(int);
extern void InitSignals(void);
extern bbuf *Buff_Init(void);
extern void Log_Init(void);
extern void Log_Done(void);
extern void Data_Init(void);
extern void Data_Done(void);
extern void BrainSurgeon(void);
extern void PrintPinged(RPC2_Handle);
extern int CheckCVResult(RPC2_Handle,int,const char*,
			 const char*);
extern void LogEventArray(VmonSessionEventArray*);

#endif _MONDUTIL_H_
