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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/utils-src/mond/report.h,v 3.3 1995/10/09 19:27:03 satya Exp $";
#endif /*_BLURB_*/



#ifndef _REPORT_H_
#define _REPORT_H_

/* I'm not sure why these are necessary all of a sudden, but... */
typedef class callCountArray;
typedef class multiCallArray;
typedef class Histogram;

extern long ReportSession(VmonVenusId*, VmonSessionId, VolumeId, UserId, 
			  VmonAVSG*, RPC2_Unsigned, RPC2_Unsigned, RPC2_Unsigned, 
			  VmonSessionEventArray*, SessionStatistics*, CacheStatistics*);

extern long ReportCommEvent(VmonVenusId *, RPC2_Unsigned, long, 
			    RPC2_Unsigned, VmonCommEventType);

extern long ReportClntCall(VmonVenusId*, long, class callCountArray*);

extern long ReportClntMCall(VmonVenusId*, long, class multiCallArray*);

extern long ReportClntRVM(VmonVenusId*, long, RvmStatistics*);

extern long ReportVCB(VmonVenusId*, long, long, VolumeId, VCBStatistics*);

extern long ReportAdviceCall(VmonVenusId*, long, UserId, AdviceStatistics*,
			     unsigned long, AdviceCalls*, unsigned long, AdviceResults*);

extern long ReportMiniCacheCall(VmonVenusId*, long, unsigned long, VmonMiniCacheStat*,
				unsigned long, VmonMiniCacheStat*);

extern long ReportOverflow(VmonVenusId *, RPC2_Unsigned, RPC2_Unsigned, 
			   RPC2_Integer, RPC2_Unsigned, RPC2_Unsigned, 
			   RPC2_Integer);

extern long ReportSrvrCall(SmonViceId*,unsigned long, class callCountArray*, 
			   class callCountArray*, class callCountArray*, 
			   class callCountArray*, class multiCallArray*, 
			   SmonStatistics*);

extern long ReportResEvent(SmonViceId*,RPC2_Unsigned, VolumeId, RPC2_Integer, 
			   RPC2_Integer, RPC2_Integer, RPC2_Integer, ResOpEntry[]);

extern long ReportRvmResEvent(SmonViceId, unsigned long, unsigned long, FileResStats,
			      DirResStats, class Histogram*, class Histogram*, 
			      ResConflictStats, class Histogram*, class Histogram*, 
			      ResLogStats, class Histogram*, class Histogram*);

extern long ReportSrvOverflow(SmonViceId *, unsigned long, unsigned long, 
			      unsigned long, long);

extern long ReportIotInfoCall(VmonVenusId *, IOT_INFO *, RPC2_Integer, RPC2_String);

extern long ReportIotStatsCall(VmonVenusId *, RPC2_Integer, IOT_STAT *);

extern long ReportSubtreeCall(VmonVenusId *, RPC2_Integer, LocalSubtreeStats *);

extern long ReportRepairCall(VmonVenusId *, RPC2_Integer, RepairSessionStats *);

extern long ReportRwsStatsCall(VmonVenusId *, RPC2_Integer, ReadWriteSharingStats *);

#endif _REPORT_H_
