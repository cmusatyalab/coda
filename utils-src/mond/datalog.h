#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1995 Carnegie Mellon University
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

static char *rcsid = "$Header$";
#endif /*_BLURB_*/



#ifndef _DATALOG_H_
#define _DATALOG_H_

extern long ScanPastMagicNumber(long *);

extern int ReadSessionRecord(VmonVenusId*, VmonSessionId*, VolumeId*, UserId*, VmonAVSG*,
			     RPC2_Unsigned*, RPC2_Unsigned*, RPC2_Unsigned*,
			     VmonSessionEventArray*, SessionStatistics*, CacheStatistics*);
extern int ReadCommRecord(VmonVenusId *, RPC2_Unsigned *, RPC2_Integer *, RPC2_Unsigned *,
			  VmonCommEventType *);

extern int ReadClientCall(VmonVenusId *, long *, unsigned long *, 
			  CallCountEntry **);


extern int ReadClientMCall(VmonVenusId *, long *, 
                          unsigned long *, MultiCallEntry **);

extern int ReadClientRVM(VmonVenusId *, long *, RvmStatistics *);

extern int ReadVCB(VmonVenusId *, long *, long *, VolumeId *, VCBStatistics *);

extern int ReadAdviceCall(VmonVenusId *, long *, UserId *, AdviceStatistics *, 
			  unsigned long *, AdviceCalls **, 
		   	  unsigned long *, AdviceResults **);

extern int ReadMiniCacheCall(VmonVenusId*,
			     long*,
			     unsigned long*,
			     VmonMiniCacheStat**,
			     unsigned long*,
			     VmonMiniCacheStat**);

extern int ReadOverflow(VmonVenusId *, RPC2_Unsigned *, RPC2_Unsigned *, RPC2_Unsigned *,
		 RPC2_Unsigned *, RPC2_Unsigned *, RPC2_Unsigned *);
extern int ReadSrvCall(SmonViceId *, unsigned long *, unsigned long *,	CallCountEntry **,
		unsigned long *, CallCountEntry **, unsigned long *,
		CallCountEntry **, unsigned long *, CallCountEntry **, unsigned long *,
		MultiCallEntry **, SmonStatistics *);
extern int ReadResEvent(SmonViceId *, unsigned long *, unsigned long *,
		 long *, long *, long *, unsigned long *, ResOpEntry **);

extern int ReadRvmResEvent(SmonViceId*, unsigned long*, unsigned long*,
			   FileResStats*, DirResStats*, long*, HistoElem**,
			   long*, HistoElem**,
			   ResConflictStats*, long*, HistoElem**, long*,
			   HistoElem**, ResLogStats*, long*, HistoElem**,
			   long*, HistoElem**);

extern int ReadSrvOverflow(SmonViceId *, unsigned long *, unsigned long *,
		    unsigned long *,long *);

extern int ReadIotInfoCall(VmonVenusId *, IOT_INFO *, RPC2_Integer *, RPC2_String *);

extern int ReadIotStatsCall(VmonVenusId *, RPC2_Integer *, IOT_STAT *);

extern int ReadSubtreeCall(VmonVenusId *, RPC2_Integer *, LocalSubtreeStats *);

extern int ReadRepairCall(VmonVenusId *, RPC2_Integer *, RepairSessionStats *);

extern int ReadRwsStatsCall(VmonVenusId *, RPC2_Integer *, ReadWriteSharingStats *);

extern void RemoveCountArray(unsigned long, CallCountEntry*);
extern void RemoveMultiArray(unsigned long, MultiCallEntry*);

#endif _DATALOG_H_
