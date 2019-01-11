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
#endif /*_BLURB_*/

#ifdef __cplusplus

extern int ReportSession(VmonVenusId *, VmonSessionId, VolumeId, UserId,
                         VmonAVSG *, RPC2_Unsigned, RPC2_Unsigned,
                         RPC2_Unsigned, VmonSessionEventArray *,
                         SessionStatistics *, CacheStatistics *);
extern int ReportCommEvent(VmonVenusId *, RPC2_Unsigned, RPC2_Integer,
                           RPC2_Unsigned, VmonCommEventType);

extern int ReportClientCall(VmonVenusId *, long, unsigned long,
                            CallCountEntry *);

extern int ReportClientMCall(VmonVenusId *, long, unsigned long,
                             MultiCallEntry *);

extern int ReportClientRVM(VmonVenusId *, long, RvmStatistics *);

extern int ReportVCB(VmonVenusId *, long, long, VolumeId, VCBStatistics *);

extern int ReportAdviceCall(VmonVenusId *, long, UserId, AdviceStatistics *,
                            unsigned long, AdviceCalls *, unsigned long,
                            AdviceResults *);

extern int ReportMiniCache(VmonVenusId *, RPC2_Integer, RPC2_Unsigned,
                           VmonMiniCacheStat *, RPC2_Unsigned,
                           VmonMiniCacheStat *);
extern int ReportOverflow(VmonVenusId *, RPC2_Unsigned, RPC2_Unsigned,
                          RPC2_Integer, RPC2_Unsigned, RPC2_Unsigned,
                          RPC2_Integer);
extern int ReportSrvCall(SmonViceId *, unsigned long, unsigned long,
                         CallCountEntry *, unsigned long, CallCountEntry *,
                         unsigned long, CallCountEntry *, unsigned long,
                         CallCountEntry *, unsigned long, MultiCallEntry *,
                         SmonStatistics *);
extern int ReportResEvent(SmonViceId *, unsigned long, VolumeId, long, long,
                          long, long, ResOpEntry *);
extern int ReportRvmResEvent(SmonViceId *, unsigned long, unsigned long,
                             FileResStats *, DirResStats *, long, HistoElem *,
                             long, HistoElem *, ResConflictStats *, long,
                             HistoElem *, long, HistoElem *, ResLogStats *,
                             long, HistoElem *, long, HistoElem *);
extern int ReportSrvOvrflw(SmonViceId *, unsigned long, unsigned long,
                           unsigned long, long);

extern int ReportIotInfoCall(VmonVenusId *, IOT_INFO *, RPC2_Integer,
                             RPC2_String);

extern int ReportIotStatsCall(VmonVenusId *, RPC2_Integer, IOT_STAT *);

extern int ReportSubtreeCall(VmonVenusId *, RPC2_Integer, LocalSubtreeStats *);

extern int ReportRepairCall(VmonVenusId *, RPC2_Integer, RepairSessionStats *);

extern int ReportRwsStatsCall(VmonVenusId *, RPC2_Integer,
                              ReadWriteSharingStats *);

extern int ReportDiscoQ(DiscoMissQ *);
extern int ReportReconnQ(ReconnQ *);

extern int InitDB(char *);
extern int CloseDB();
extern void UpdateDB();
extern void UnlockTables();

#else /*__cplusplus */

extern int ReportSession();
extern int ReportCommEvent();
extern int ReportClientCall();
extern int ReportMiniCache();
extern int ReportOverflow();
extern int ReportSrvCall();
extern int ReportResEvent();
extern int ReportRvmResEvent();
extern int InitDB();
extern int CloseDB();
extern void UpdateDB();
extern void UnlockTables();
extern int ReportIotInfoCall();
extern int ReportIotStatsCall();
extern int ReportSubtreeCall();
extern int ReportRepairCall();
extern int ReportRwsStatsCall();

#endif /* __cplusplus */
