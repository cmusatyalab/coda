/*
** Note: this file must be compiled by a C compiler.  C++
** is unhappy with code produced by esql.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include "coda_string.h"
#include "rpc2.h"
#include "lwp.h"
$include sqlca;
#include "lock.h"
#include <varargs.h>
#include "mond.h"
#include "db.h"
#include "advice_parser.h"

#define VICE 0
#define VENUS 1

#define LOG_SIZE_HIST 1
#define LOG_MAX_HIST 2
#define SUCC_HIER_HIST 3
#define FAIL_HIER_HIST 4
#define VAR_LOG_HIST 5
#define RVM_RES_LOG_HIST 6
#define SQL_LENGTH 512

extern char DataBaseName[];
extern int LogLevel;
extern FILE *LogFile;

PRIVATE int CheckSQL();
PRIVATE long GetVenusIndex();
PRIVATE long GetVenusInitIndex();
PRIVATE long GetViceIndex();
PRIVATE int InsertCallArray();
PRIVATE int InsertMultiArray();
PRIVATE long GetCallIndex();
PRIVATE long GetMultiIndex(); 
PRIVATE int InsertResOpArray();
/*PRIVATE int InsertHistogram();*/

/* bogus C++ linking problems */

/* The following is the g++ name-mangled version of LogMsg */
extern void LogMsg__FiiP6_iobufPce(int,int,FILE*,char*,...);
#define LogMsg LogMsg__FiiP6_iobufPce

/* The following is the AT&T C++ name-mangled version of LogMsg */
/*
 * extern void LogMsg__FiT1P6_iobufPce(int,int,FILE*,char*,...);
 * #define LogMsg LogMsg__FiT1P6_iobufPce
 */

int ReportSession(Venus, Session, Volume, User, AVSG, StartTime,
		   EndTime, CETime, Events, Stats, CacheStats)
VmonVenusId *Venus;
VmonSessionId Session;
VolumeId Volume;
UserId User;
VmonAVSG *AVSG;
RPC2_Unsigned StartTime;
RPC2_Unsigned EndTime;
RPC2_Unsigned CETime;
VmonSessionEventArray *Events;
SessionStatistics *Stats;
CacheStatistics *CacheStats;

{

    int code = 0;
    $ long venusindex;
    $ long session;
    $ long volume;
    $ long uid;
    $ long avsgmem1;
    $ long avsgmem2;
    $ long avsgmem3;
    $ long avsgmem4;
    $ long avsgmem5;
    $ long avsgmem6;
    $ long avsgmem7;
    $ long avsgmem8;
    $ long starttime;
    $ long endtime;
    $ long cetime;
    $ long sesstime;
    $ long inserttime;
    $ long otherinsertt;
    $ long sessionindex;
    $ int opcode;
    $ long succ_count;
    $ long sigma_t;
    $ long sigma_t_squared;
    $ long fail_count;
    VmonSessionEvent *Event;    
    int i;

    LogMsg(1000,LogLevel,LogFile,
	   "SpoolSession: Venus = [%x %d], Session = %d, Volume = %x, User = %d, Time = [%d %d]",
	      Venus->IPAddress, Venus->BirthTime, Session, Volume, User, 
	      StartTime, EndTime);
    
    inserttime = 0;
    venusindex = GetVenusIndex(Venus);

    /* Record the session. */
    session = Session;
    volume = Volume;
    uid = User;
    starttime = StartTime;
    endtime = EndTime;
    cetime = CETime;
    avsgmem1 = AVSG->Member0;
    avsgmem2 = AVSG->Member1;
    avsgmem3 = AVSG->Member2;
    avsgmem4 = AVSG->Member3;
    avsgmem5 = AVSG->Member4;
    avsgmem6 = AVSG->Member5;
    avsgmem7 = AVSG->Member6;
    avsgmem8 = AVSG->Member7;
    $ select end_time,comm_event_time,session_index
	into $inserttime,$otherinsertt,$sessionindex
	    from sessions
	where venus_index = $venusindex
	    and session = $session
	    and volume = $volume
	    and uid = $uid;

    sesstime = (cetime > endtime) ? cetime : endtime;
    inserttime = (otherinsertt > inserttime) ?
	otherinsertt : inserttime;

    if (sqlca.sqlcode != SQLNOTFOUND && sesstime <= inserttime)
    {
	LogMsg(100,LogLevel,LogFile,
	       "Duplicate session: venus_index (%d) session (%d) volume (%x) uid (%d)",
		venusindex, session, volume, uid);
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND)
	{
	    $ begin work;
	    code = CheckSQL("Starting transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from sessions
		where venus_index = $venusindex
		    and session = $session
		    and volume = $volume
		    and uid = $uid;
	    code += CheckSQL("Delete from session",1);
	    $ delete from normal_events
		where session_index = $sessionindex;
	    code += CheckSQL("Delete form normal events",1);
	    $ delete from session_stats
		where session_index = $sessionindex;
	    code += CheckSQL("Delete from session stats",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    } else {
		$ commit work;
		LogMsg(100,LogLevel,LogFile,
			"Obsolete session deleted successfully");
	    }
	}
    }
    $ begin work;
    code = CheckSQL("Begin transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }
    $ insert into sessions
	(venus_index, session, volume, avsgmem1, avsgmem2, avsgmem3,
	 avsgmem4, avsgmem5, avsgmem6, avsgmem7, avsgmem8, uid, 
	 start_time, end_time, comm_event_time)	
	    values($venusindex, $session, $volume, $avsgmem1, $avsgmem2,
		   $avsgmem3, $avsgmem4, $avsgmem5, $avsgmem6, $avsgmem7,
		   $avsgmem8, $uid, $starttime, $endtime, $cetime);
    code += CheckSQL("Insert into session", 1);
    $ select session_index into $sessionindex
	from sessions
      where venus_index = $venusindex
	  and session = $session
          and volume = $volume
          and uid = $uid;
    code += CheckSQL("Get the session index",1);

    /* bail out here if we don't have a proper session index */
    if (code != 0) {
	$rollback work;
	return code;
    }

    /* insert sessions */
    Event = &Events->Event0;
    for (i = 0; i < nVSEs; Event++, i++) {
	/* only insert a normal event if there is data for it */
	if ((Event->SuccessCount != 0)
	    || (Event->FailureCount != 0))
	{
	    opcode = Event->Opcode;
	    succ_count = Event->SuccessCount;
	    sigma_t = Event->SigmaT;
	    sigma_t_squared = Event->SigmaTSquared;
	    fail_count = Event->FailureCount;
	    $ insert into normal_events
		(session_index, opcode,
		 succ_count, sigma_t, sigma_t_squared, fail_count)
	      values($sessionindex, $opcode,
		     $succ_count, $sigma_t, $sigma_t_squared, 
		     $fail_count);
	    code += CheckSQL("Insert into normal_events", 1);
	}
    }
    code += InsertSessionStats(sessionindex,Stats);
    code += InsertCacheStats(sessionindex,CacheStats);
    if (code != 0)
	$ rollback work;
    else
	$ commit work;
    return code;
}

int InsertSessionStats(index,Stats)
long index;
SessionStatistics *Stats;
{
    int code =0;
    $ long sessionindex;
    $ long bytesstart;
    $ long bytesend;
    $ long byteshigh;
    $ long entriesstart;
    $ long entriesend;
    $ long entrieshigh;
    $ long recordscan;
    $ long recordscomm;
    $ long recordsab;
    $ long fidsreall;
    $ long bytesbf;
    $ long systemcpu;
    $ long usercpu;
    $ long idlecpu;
    $ long cachehighwater;

    sessionindex = index;
    bytesstart = Stats->BytesStart;
    bytesend = Stats->BytesEnd;
    byteshigh = Stats->BytesHighWater;
    entriesstart = Stats->EntriesStart;
    entriesend = Stats->EntriesEnd;
    entrieshigh = Stats->EntriesHighWater;
    recordscan = Stats->RecordsCancelled;
    recordscomm = Stats->RecordsCommitted;
    recordsab = Stats->RecordsAborted;
    fidsreall = Stats->FidsRealloced;
    bytesbf = Stats->BytesBackFetched;
    systemcpu = Stats->SystemCPU;
    usercpu = Stats->UserCPU;
    idlecpu = Stats->IdleCPU;
    cachehighwater = Stats->CacheHighWater;

    $ insert into session_stats
	(session_index, cml_start, cml_end, cml_high, cml_bytes_start,
	 cml_bytes_end, cml_bytes_high, records_cancelled, records_committed,
	 records_aborted, fids_realloced, bytes_back_fetched,
	 system_cpu, user_cpu, idle_cpu, cache_highwater)
	values ($sessionindex, $entriesstart, $entriesend,
		$entrieshigh, $bytesstart, $bytesend, $byteshigh, 
		$recordscan, $recordscomm,
		$recordsab, $fidsreall, $bytesbf, $systemcpu,
		$usercpu, $idlecpu, $cachehighwater);
    return (CheckSQL("Insert into session_stats",1));
}

int InsertCacheStats(index,CacheStats)
long index;
CacheStatistics *CacheStats;
{
    int code =0;
    $ long sessionindex;
    $ long hoardattrhitc;
    $ long hoardattrhitb;
    $ long hoardattrmissc;
    $ long hoardattrmissb;
    $ long hoardattrnospcc;
    $ long hoardattrnospcb;
    $ long hoarddatahitc;
    $ long hoarddatahitb;
    $ long hoarddatamissc;
    $ long hoarddatamissb;
    $ long hoarddatanospcc;
    $ long hoarddatanospcb;
    $ long nonhoardattrhitc;
    $ long nonhoardattrhitb;
    $ long nonhoardattrmissc;
    $ long nonhoardattrmissb;
    $ long nonhoardattrnospcc;
    $ long nonhoardattrnospcb;
    $ long nonhoarddatahitc;
    $ long nonhoarddatahitb;
    $ long nonhoarddatamissc;
    $ long nonhoarddatamissb;
    $ long nonhoarddatanospcc;
    $ long nonhoarddatanospcb;
    $ long unknownhoardattrhitc;
    $ long unknownhoardattrhitb;
    $ long unknownhoardattrmissc;
    $ long unknownhoardattrmissb;
    $ long unknownhoardattrnospcc;
    $ long unknownhoardattrnospcb;
    $ long unknownhoarddatahitc;
    $ long unknownhoarddatahitb;
    $ long unknownhoarddatamissc;
    $ long unknownhoarddatamissb;
    $ long unknownhoarddatanospcc;
    $ long unknownhoarddatanospcb;

    sessionindex = index;
    hoardattrhitc = CacheStats->HoardAttrHit.Count;
    hoardattrhitb = CacheStats->HoardAttrHit.Blocks;
    hoardattrmissc = CacheStats->HoardAttrMiss.Count;
    hoardattrmissb = CacheStats->HoardAttrMiss.Blocks;
    hoardattrnospcc = CacheStats->HoardAttrNoSpace.Count;
    hoardattrnospcb = CacheStats->HoardAttrNoSpace.Blocks;
    hoarddatahitc = CacheStats->HoardDataHit.Count;
    hoarddatahitb = CacheStats->HoardDataHit.Blocks;
    hoarddatamissc = CacheStats->HoardDataMiss.Count;
    hoarddatamissb = CacheStats->HoardDataMiss.Blocks;
    hoarddatanospcc = CacheStats->HoardDataNoSpace.Count;
    hoarddatanospcb = CacheStats->HoardDataNoSpace.Blocks;
    nonhoardattrhitc = CacheStats->NonHoardAttrHit.Count;
    nonhoardattrhitb = CacheStats->NonHoardAttrHit.Blocks;
    nonhoardattrmissc = CacheStats->NonHoardAttrMiss.Count;
    nonhoardattrmissb = CacheStats->NonHoardAttrMiss.Blocks;
    nonhoardattrnospcc = CacheStats->NonHoardAttrNoSpace.Count;
    nonhoardattrnospcb = CacheStats->NonHoardAttrNoSpace.Blocks;
    nonhoarddatahitc = CacheStats->NonHoardDataHit.Count;
    nonhoarddatahitb = CacheStats->NonHoardDataHit.Blocks;
    nonhoarddatamissc = CacheStats->NonHoardDataMiss.Count;
    nonhoarddatamissb = CacheStats->NonHoardDataMiss.Blocks;
    nonhoarddatanospcc = CacheStats->NonHoardDataNoSpace.Count;
    nonhoarddatanospcb = CacheStats->NonHoardDataNoSpace.Blocks;
    unknownhoardattrhitc = CacheStats->UnknownHoardAttrHit.Count;
    unknownhoardattrhitb = CacheStats->UnknownHoardAttrHit.Blocks;
    unknownhoardattrmissc = CacheStats->UnknownHoardAttrMiss.Count;
    unknownhoardattrmissb = CacheStats->UnknownHoardAttrMiss.Blocks;
    unknownhoardattrnospcc = CacheStats->UnknownHoardAttrNoSpace.Count;
    unknownhoardattrnospcb = CacheStats->UnknownHoardAttrNoSpace.Blocks;
    unknownhoarddatahitc = CacheStats->UnknownHoardDataHit.Count;
    unknownhoarddatahitb = CacheStats->UnknownHoardDataHit.Blocks;
    unknownhoarddatamissc = CacheStats->UnknownHoardDataMiss.Count;
    unknownhoarddatamissb = CacheStats->UnknownHoardDataMiss.Blocks;
    unknownhoarddatanospcc = CacheStats->UnknownHoardDataNoSpace.Count;
    unknownhoarddatanospcb = CacheStats->UnknownHoardDataNoSpace.Blocks;

    $ insert into cache_stats
	(session_index, 
	h_a_h_count, h_a_h_blocks, 
	h_a_m_count, h_a_m_blocks, 
	h_a_ns_count, h_a_ns_blocks,
	h_d_h_count, h_d_h_blocks,
	h_d_m_count, h_d_m_blocks,
	h_d_ns_count, h_d_ns_blocks,
	nh_a_h_count, nh_a_h_blocks,
	nh_a_m_count, nh_a_m_blocks,
	nh_a_ns_count, nh_a_ns_blocks,
	nh_d_h_count, nh_d_h_blocks,
	nh_d_m_count, nh_d_m_blocks,
	nh_d_ns_count, nh_d_ns_blocks,
	uh_a_h_count, uh_a_h_blocks,
	uh_a_m_count, uh_a_m_blocks,
	uh_a_ns_count, uh_a_ns_blocks,
	uh_d_h_count, uh_d_h_blocks,
	uh_d_m_count, uh_d_m_blocks,
	uh_d_ns_count, uh_d_ns_blocks
	)
	values ($sessionindex,
		$hoardattrhitc, $hoardattrhitb, 
		$hoardattrmissc, $hoardattrmissb, 
		$hoardattrnospcc, $hoardattrnospcb, 
		$hoarddatahitc, $hoarddatahitb, 
		$hoarddatamissc, $hoarddatamissb, 
		$hoarddatanospcc, $hoarddatanospcb,
		$nonhoardattrhitc, $nonhoardattrhitb, 
		$nonhoardattrmissc, $nonhoardattrmissb, 
		$nonhoardattrnospcc, $nonhoardattrnospcb,
		$nonhoarddatahitc, $nonhoarddatahitb, 
		$nonhoarddatamissc, $nonhoarddatamissb, 
		$nonhoarddatanospcc, $nonhoarddatanospcb,
		$unknownhoardattrhitc, $unknownhoardattrhitb, 
		$unknownhoardattrmissc, $unknownhoardattrmissb, 
		$unknownhoardattrnospcc, $unknownhoardattrnospcb,
		$unknownhoarddatahitc, $unknownhoarddatahitb, 
		$unknownhoarddatamissc, $unknownhoarddatamissb, 
		$unknownhoarddatanospcc, $unknownhoarddatanospcb);

    return (CheckSQL("Insert into cache_stats",1));
}

int ReportCommEvent(Venus, ServerIPAddress, SerialNumber, Time, Type) 
VmonVenusId *Venus;
RPC2_Unsigned ServerIPAddress;
RPC2_Integer SerialNumber;
RPC2_Unsigned Time;
VmonCommEventType Type;
{
    int code = 0;
    $ long venusindex;
    $ long server;
    $ long serialnum;
    $ long time;
    $ int serverup;
    $ long dummy;

    LogMsg(1000, LogLevel,LogFile,
	   "SpoolCommEvent: Venus = [%x %d], Server = %x, Time = %d, Type = %d",
	      Venus->IPAddress, Venus->BirthTime, ServerIPAddress, Time, 
	      Type);

    venusindex = GetVenusIndex(Venus);

    /* Record the communication event. */
    server = ServerIPAddress;
    time = Time;
    serialnum = SerialNumber;
    serverup = (Type == ServerUp ? 1 : 0);
    $ select timestamp into $dummy from comm_events
	where venus_index = $venusindex
	    and server = $server
	    and serial_number = $serialnum
	    and timestamp = $time
	    and serverup = $serverup;
    CheckSQL("Looking for comm event",0);
    if (sqlca.sqlcode != SQLNOTFOUND)
    {
        LogMsg(100,LogLevel,LogFile,
	       "Duplicate comm event: venus_index (%d) server (%x) time (%d)",
		venusindex, server, time);
    } else {
	$ insert into comm_events
	    (venus_index, server, serial_number, timestamp, serverup)
		values($venusindex, $server, $serialnum, $time, $serverup);
	code = CheckSQL("Insert into comm_events", 1);
    }
    return(code);
}

int ReportClientCall(Venus,Time,sc_size,SrvCount)
VmonVenusId *Venus;
long Time;
unsigned long sc_size;
CallCountEntry *SrvCount;
{
    int code = 0;
    $ long venusindex;
    $ long time;
    $ long inserttime;

    venusindex = GetVenusIndex(Venus);
    time = Time;

    $ select time into $inserttime from client_rvm_stats
	where venus_index = $venusindex;
    CheckSQL("Looking for entry in client_rvm_stats",0);
    
    if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
        LogMsg(100,LogLevel,LogFile,
	       "Duplicate client call event record: venus_index (%d), time (%d)",
	       venusindex, time);
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND) {
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from clnt_calls
		where venus_index = $venusindex;
	    code += CheckSQL("Delete from clnt_calls",1);
	    if (code !=0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	}
    }   
    $ begin work;
    code = CheckSQL("Begin transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }
    code += InsertCallArray(venusindex,sc_size,SrvCount,VENUS);
    if (code != 0) {
	$rollback work;
    } else
	$ commit work;
    return code;
}


int ReportClientMCall(Venus,Time,msc_size,MSrvCount)
VmonVenusId *Venus;
long Time;
unsigned long msc_size;
MultiCallEntry *MSrvCount;
{
    int code = 0;
    $ long venusindex;
    $ long time;
    $ long inserttime;

    venusindex = GetVenusIndex(Venus);
    time = Time;

    $ select time into $inserttime from client_rvm_stats
	where venus_index = $venusindex;
    CheckSQL("Looking for entry in client_rvm_stats",0);
    
    if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
        LogMsg(100,LogLevel,LogFile,
	       "Duplicate client call event record: venus_index (%d), time (%d)",
	       venusindex, time);
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND) {
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from clnt_mltcalls
		where venus_index = $venusindex;
	    code += CheckSQL("Delete from clnt_mltcalls",1);
	    if (code !=0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	}
    }   
    $ begin work;
    code = CheckSQL("Begin transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }
    code += InsertMultiArray(venusindex,msc_size,MSrvCount,VENUS);
    if (code != 0) {
	$rollback work;
    } else
	$ commit work;
    return code;
}


int ReportClientRVM(Venus,Time,Stats)
VmonVenusId *Venus;
long Time;
RvmStatistics *Stats;
{
    int code = 0;
    $ long venusindex;
    $ long time;
    $ long mallocnum;
    $ long freenum;
    $ long mallocbytes;
    $ long freebytes;
    $ long inserttime;

    venusindex = GetVenusIndex(Venus);
    time = Time;
    mallocnum = Stats->Malloc;
    freenum = Stats->Free;
    mallocbytes = Stats->MallocBytes;
    freebytes = Stats->FreeBytes;

    $ select time into $inserttime from client_rvm_stats
	where venus_index = $venusindex;
    CheckSQL("Looking for entry in client_rvm_stats",0);
    
    if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
        LogMsg(100,LogLevel,LogFile,
	       "Duplicate client call event record: venus_index (%d), time (%d)",
	       venusindex, time);
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND) {
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from client_rvm_stats
		where venus_index = $venusindex;
	    code += CheckSQL("Delete from client_rvm_stats",1);
	    if (code !=0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	}
    }   
    $ begin work;
    code = CheckSQL("Begin transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }
    $insert into client_rvm_stats
	(venus_index, time, malloc_num, free_num, malloc_bytes, free_bytes)
	values ($venusindex,$time,$mallocnum,$freenum,$mallocbytes,$freebytes);
    code = CheckSQL("Insert into client_rvm_stas",1);
    if (code != 0) {
	$rollback work;
    } else
	$ commit work;
    return code;
}

int ReportVCB(Venus,VenusInit,Time,Volume,Stats)
VmonVenusId *Venus;
long VenusInit;
long Time;
VolumeId Volume;
VCBStatistics *Stats;
{
    int code = 0;
    $ long inserttime;
    $ long venusindex;
    $ long time;
    $ long volume;
    $ long acquires;
    $ long acquireobjs;
    $ long acquirechecked;
    $ long acquirefailed;
    $ long acquirenoobjfails;
    $ long validates;
    $ long validateobjs;
    $ long failedvalidates;
    $ long failedvalidateobjs;
    $ long breaks;
    $ long breakobjs;
    $ long breakvolonly;
    $ long breakrefs;
    $ long clears;
    $ long clearobjs;
    $ long clearrefs;
    $ long nostamp;
    $ long nostampobjs;

    LogMsg(1000, LogLevel,LogFile,
	"SpoolVCBRecord: Venus = [%x %d], InitTime = %d, Time = %d, Volume = 0x%x",
	Venus->IPAddress, Venus->BirthTime, VenusInit, Time, Volume);

    venusindex = GetVenusInitIndex(Venus->IPAddress, VenusInit);
    time = Time;
    volume = Volume;
    acquires = Stats->Acquires;
    acquireobjs = Stats->AcquireObjs;
    acquirechecked = Stats->AcquireChecked;
    acquirefailed = Stats->AcquireFailed;
    acquirenoobjfails = Stats->AcquireNoObjFails;
    validates = Stats->Validates;
    validateobjs = Stats->ValidateObjs;
    failedvalidates = Stats->FailedValidates;
    failedvalidateobjs = Stats->FailedValidateObjs;
    breaks = Stats->Breaks;
    breakobjs = Stats->BreakObjs;
    breakvolonly = Stats->BreakVolOnly;
    breakrefs = Stats->BreakRefs;
    clears = Stats->Clears;
    clearobjs = Stats->ClearObjs;
    clearrefs = Stats->ClearRefs;
    nostamp = Stats->NoStamp;
    nostampobjs = Stats->NoStampObjs;  	

    $ select time into $inserttime from vcb_stats
	where venus_index = $venusindex
	and volume = $volume;
    CheckSQL("Looking for entry in vcb_stats",0);

    if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
        LogMsg(100,LogLevel,LogFile,
	       "Duplicate vcb event record: venus_index (%d), time (%d)",
	       venusindex, time);
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND) {
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from vcb_stats
		where venus_index = $venusindex
		and volume = $volume;
	    code += CheckSQL("Delete from vcb_stats",1);
	    if (code !=0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	}
    }   
    $ begin work;
    code = CheckSQL("Begin transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }
    $insert into vcb_stats
	(venus_index, time, volume, acquires, acquireobjs, acquirechecked,
	 acquirefailed, acquirenoobjfails, validates, validateobjs, 
	 failedvalidates, failedvalidateobjs, breaks, breakobjs, breakvolonly,
	 breakrefs, clears, clearobjs, clearrefs, nostamp, nostampobjs)
	values ($venusindex, $time, $volume, $acquires, $acquireobjs, $acquirechecked,
	 $acquirefailed, $acquirenoobjfails, $validates, $validateobjs, 
	 $failedvalidates, $failedvalidateobjs, $breaks, $breakobjs, $breakvolonly,
	 $breakrefs, $clears, $clearobjs, $clearrefs, $nostamp, $nostampobjs);

    code = CheckSQL("Insert into vcb_stats",1);
    if (code != 0) {
	$rollback work;
    } else
	$ commit work;
    return code;
}

int ReportAdviceCall(Venus,Time,User,Stats,Call_Size,Call_Stats,Result_Size,Result_Stats)
VmonVenusId *Venus;
long Time;
UserId User;
AdviceStatistics *Stats;
unsigned long Call_Size;
AdviceCalls *Call_Stats;
unsigned long Result_Size;
AdviceResults *Result_Stats;
{
    int code = 0;
    $ long inserttime;
    $ long venusindex;
    $ long time;
    $ long uid;
    $ long notenabled;
    $ long notvalid;
    $ long outstanding;
    $ long asrnotallowed;
    $ long asrinterval;
    $ long volumenull;
    $ long totalnumberattempts;
    $ long pcm_s;
    $ long pcm_f;
    $ long hwa_s;
    $ long hwa_f;
    $ long dm_s;
    $ long dm_f;
    $ long r_s;
    $ long r_f;
    $ long rp_s;
    $ long rp_f;
    $ long iasr_s;
    $ long iasr_f;
    $ long lc_s;
    $ long lc_f;
    $ long wcm_s;
    $ long wcm_f;	
    $ long rpc2_success;
    $ long rpc2_connbusy;
    $ long rpc2_fail;
    $ long rpc2_noconnection;
    $ long rpc2_timeout;
    $ long rpc2_dead;
    $ long rpc2_othererrors;

    LogMsg(1000, LogLevel,LogFile,
	"SpoolAdviceRecord: Venus = [%x %d], Time = %d, User = %d",
	Venus->IPAddress, Venus->BirthTime, Time, User);

    venusindex = GetVenusIndex(Venus);
    time = Time;
    uid = User;

    notenabled = Stats->NotEnabled;
    notvalid = Stats->NotValid;
    outstanding = Stats->Outstanding;
    asrnotallowed = Stats->ASRnotAllowed;
    asrinterval = Stats->ASRinterval;
    volumenull = Stats->VolumeNull;
    totalnumberattempts = Stats->TotalNumberAttempts;

    /* Tedious! */
    pcm_s = Call_Stats[0].success;
    pcm_f = Call_Stats[0].failures;
    hwa_s = Call_Stats[1].success;
    hwa_f = Call_Stats[1].failures;
    dm_s =  Call_Stats[2].success;
    dm_f =  Call_Stats[2].failures;
    r_s = Call_Stats[3].success;
    r_f = Call_Stats[3].failures;
    rp_s = Call_Stats[4].success;
    rp_f = Call_Stats[4].failures;
    iasr_s = Call_Stats[5].success;
    iasr_f = Call_Stats[5].failures;
    lc_s = Call_Stats[6].success;
    lc_f = Call_Stats[6].failures;
    wcm_s = Call_Stats[7].success;
    wcm_f = Call_Stats[7].failures;

    rpc2_success = Result_Stats[0].count;
    rpc2_connbusy = Result_Stats[1].count;
    rpc2_fail = Result_Stats[2].count;
    rpc2_noconnection = Result_Stats[3].count;
    rpc2_timeout = Result_Stats[4].count;
    rpc2_dead = Result_Stats[5].count;
    rpc2_othererrors = Result_Stats[6].count;


    $ select time into $inserttime from advice_stats
	where venus_index = $venusindex 
	  and uid = $uid;
    CheckSQL("Looking for entry in advice_stats", 0);

    if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
	LogMsg(100,LogLevel,LogFile,
	       "Duplicate advice statistics record: venus_index (%d), time (%d)",
		venusindex, time);
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND) {
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	$ delete from advice_stats
	    where venus_index = $venusindex
	      and uid = $uid;
	code += CheckSQL("Delete from advice_stats",1);
	if (code !=0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	}
    }

    $ begin work;
    code = CheckSQL("Begin transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }

    $insert into advice_stats
	(venus_index, time, uid, not_enabled, not_valid, outstanding, 
	 asr_not_allowed, asr_interval, volume_null, total_attempts,
	 pcm_successes, pcm_failures, hwa_successes, hwa_failures,
	 dm_successes, dm_failures, r_successes, r_failures,
	 rp_successes, rp_failures, iasr_successes, iasr_failures, 
	 lc_successes, lc_failures, wcm_successes, wcm_failures,
	 rpc2_success, rpc2_connbusy, rpc2_fail, rpc2_noconnection, 
	 rpc2_timeout, rpc2_dead, rpc2_othererrors)
		values ($venusindex, $time, $uid, $notenabled, $notvalid, $outstanding,
			$asrnotallowed, $asrinterval, $volumenull, $totalnumberattempts,
			$pcm_s, $pcm_f, $hwa_s, $hwa_f, $dm_s, $dm_f, $r_s, $r_f,
			$rp_s, $rp_f, $iasr_s, $iasr_f, $lc_s, $lc_f, $wcm_s, $wcm_f, 
			$rpc2_success, $rpc2_connbusy, $rpc2_fail, $rpc2_noconnection,
			$rpc2_timeout, $rpc2_dead, $rpc2_othererrors);

    code += CheckSQL("Insert into advice_stats", 1);
    if (code != 0)
	$ rollback work;
    else
	$ commit work;
    return code;
}


int ReportMiniCache(Venus,Time,vn_size,vn_stats,vfs_size,vfs_stats)
VmonVenusId *Venus;
long Time;
unsigned long vn_size;
VmonMiniCacheStat *vn_stats;
unsigned long vfs_size;
VmonMiniCacheStat *vfs_stats;
{
    int code = 0;
    int i=0;
    $ long venusindex;
    $ long time;
    $ long inserttime;
    $ long event_index;
    $ long opc;
    $ long entries;
    $ long sat_intrn;
    $ long unsat_intrn;
    $ long gen_intrn;

    venusindex = GetVenusIndex(Venus);
    time = Time;

    $ select time into $inserttime from mcache_events
	where venus_index = $venusindex;
    CheckSQL("Looking for entry in mcache_events",0);
    
    if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
        LogMsg(100,LogLevel,LogFile,
	       "Duplicate mini cache event record: venus_index (%d), time (%d)",
	       venusindex, time);
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND) {
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from mcache_events
		where venus_index = $venusindex;
	    code += CheckSQL("Delete from mcache_events",1);
	    $ delete from mcache_vnode
		where venus_index = $venusindex;
	    code += CheckSQL("Delete from mcache_vnode",1);
	    $ delete from mcache_vfs
		where venus_index = $venusindex;
	    code += CheckSQL("Delete from mcache_vfs",1);
	    if (code !=0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	}
    }   
    $ begin work;
    code = CheckSQL("Begin transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }
    $insert into mcache_events
	(venus_index, time)
	values ($venusindex,$time);
    code = CheckSQL("Insert into mcache_events",1);
    for (i=0; i< vn_size; i++) {
	opc = vn_stats[i].Opcode;
	entries = vn_stats[i].Entries;
	sat_intrn = vn_stats[i].SatIntrn;
	unsat_intrn = vn_stats[i].UnsatIntrn;
	gen_intrn = vn_stats[i].GenIntrn;
	$ insert into mcache_vnode
	    (venus_index, opcode, entries, sat_intrn, unsat_intrn,
	     gen_intrn)
	    values ($venusindex, $opc, $entries, $sat_intrn, $unsat_intrn,
		    $gen_intrn);
	code += CheckSQL("Insert into mcache_vnode",1);
    }
    for (i=0; i< vfs_size; i++) {
	opc = vfs_stats[i].Opcode;
	entries = vfs_stats[i].Entries;
	sat_intrn = vfs_stats[i].SatIntrn;
	unsat_intrn = vfs_stats[i].UnsatIntrn;
	gen_intrn = vfs_stats[i].GenIntrn;
	$ insert into mcache_vfs
	    (venus_index, opcode, entries, sat_intrn, unsat_intrn,
	     gen_intrn)
	    values ($venusindex, $opc, $entries, $sat_intrn, $unsat_intrn,
		    $gen_intrn);
	code += CheckSQL("Insert into mcache_vfs",1);
    }
    if (code != 0) {
	$ rollback work;
    } else
	$ commit work;
    return code;
}




int ReportOverflow(Venus, VMStartTime, VMEndTime, VMCount,
		   RVMStartTime, RVMEndTime, RVMCount) 
VmonVenusId *Venus;
RPC2_Unsigned VMStartTime;
RPC2_Unsigned VMEndTime;
RPC2_Integer VMCount;
RPC2_Unsigned RVMStartTime;
RPC2_Unsigned RVMEndTime;
RPC2_Integer RVMCount;

{

    int code = 0;

    $ long venusindex;
    $ long vmstarttime;
    $ long vmendtime;
    $ long vmcnt;
    $ long rvmstarttime;
    $ long rvmendtime;
    $ long rvmcnt;
    $ long dummy;

    LogMsg(1000,LogLevel,LogFile,
	   "SpoolOverflow: Venus = [%x %d], VMTime = [%d %d], VMCount = %d",
	       Venus->IPAddress, Venus->BirthTime, VMStartTime, VMEndTime, VMCount);
    LogMsg(1000,LogLevel,LogFile,
	   "SpoolOverflow: Venus = [%x %d], RVMTime = [%d %d], RVMCount = %d",
	       Venus->IPAddress, Venus->BirthTime, RVMStartTime, RVMEndTime, RVMCount);

    venusindex = GetVenusIndex(Venus);

    /* Record the overflow event. */
    vmstarttime = VMStartTime;
    vmendtime = VMEndTime;
    vmcnt =  VMCount;
    rvmstarttime = RVMStartTime;
    rvmendtime = RVMEndTime;
    rvmcnt =  RVMCount;
    $ select vm_cnt into $dummy from overflows
	where venus_index = $venusindex
	    and vm_end_time = $vmendtime
	    and vm_start_time = $vmstarttime
            and vm_cnt = $vmcnt
	    and rvm_end_time = $rvmendtime
	    and rvm_start_time = $rvmstarttime
            and rvm_cnt = $rvmcnt;

    CheckSQL("Looking for overflow event",0);
    if (!(sqlca.sqlcode == SQLNOTFOUND))
    {
	LogMsg(100,LogLevel,LogFile,
	       "Duplicate overflow event: venus_index (%d) vm end (%d) rvm end (%d)",
		venusindex, vmendtime, rvmendtime);
    } else {
	$ insert into overflows
	    (venus_index, vm_start_time, vm_end_time, vm_cnt,
	     rvm_start_time, rvm_end_time, rvm_cnt)
		values($venusindex, $vmstarttime, $vmendtime, $vmcnt,
		       $rvmstarttime, $rvmendtime, $rvmcnt);
	code = CheckSQL("Insert into overflows", 1);
    }
    return(code);
}

PRIVATE long GetVenusIndex(Venus)
VmonVenusId *Venus;
{
    $ long host;
    $ long birth;
    $ long venusindex;
    
    host = Venus->IPAddress;
    birth = Venus->BirthTime;
    $ select instance_index into $venusindex
	from venus_instances
	    where host = $host
		and birth_time = $birth;
    if (CheckSQL("First venus lookup",1))
	return -1;
    if (sqlca.sqlcode == SQLNOTFOUND) {
	$ insert into venus_instances
	    (host, birth_time)
		values($host, $birth);
	if (CheckSQL("Insert into venus_instances", 1))
	    return -1;
	$ select instance_index into $venusindex
	    from venus_instances
		where host = $host
	      and birth_time = $birth;
	if (CheckSQL("Second venusindex lookup", 1))
	    return -1;
    }
    return(venusindex);
}

PRIVATE long GetVenusInitIndex(Host, InitTime)
unsigned long Host;
long InitTime;
{
    $ long host;
    $ long init;
    $ long venusindex;
    
    host = Host;
    init = InitTime;
    $ select instance_index into $venusindex
	from venus_init_insts
	    where host = $host
		and init_time = $init;
    if (CheckSQL("First venus lookup",1))
	return -1;
    if (sqlca.sqlcode == SQLNOTFOUND) {
	$ insert into venus_init_insts
	    (host, init_time)
		values($host, $init);
	if (CheckSQL("Insert into venus_init_insts", 1))
	    return -1;
	$ select instance_index into $venusindex
	    from venus_init_insts
		where host = $host
	      and init_time = $init;
	if (CheckSQL("Second venusindex lookup", 1))
	    return -1;
    }
    return(venusindex);
}

int ReportSrvCall(Vice,Time,CBSize,CBCount,ResSize,ResCount,SmonSize,
		   SmonCount,VolDSize,VolDCount,MultiSize,MultiCount,
		   Stats)
SmonViceId *Vice;
unsigned long Time;
unsigned long CBSize;
CallCountEntry *CBCount;
unsigned long ResSize;
CallCountEntry *ResCount;
unsigned long SmonSize;
CallCountEntry *SmonCount;
unsigned long VolDSize;
CallCountEntry *VolDCount;
unsigned long MultiSize;
MultiCallEntry *MultiCount;
SmonStatistics *Stats;

{
    int code =0;
    $long viceindex;
    $long time;
    $long systemcpu;
    $long usercpu;
    $long idlecpu;
    $long boottime;
    $long totalio;
    $long inserttime;

    viceindex = GetViceIndex(Vice);
    time = Time;
    systemcpu = Stats->SystemCPU;
    usercpu = Stats->UserCPU;
    idlecpu = Stats->IdleCPU;
    boottime = Stats->BootTime;
    totalio = Stats->TotalIO;

    $ select time into $inserttime
	from server_stats
      where vice_index = $viceindex;
    CheckSQL("Looking for serverstats record",0);
    
    if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
	LogMsg(100,LogLevel,LogFile,
           "Duplicate server call event record: vice_index (%ld) time (%ld)",
	   viceindex,time);     
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND)
	{
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from server_stats
		where vice_index = $viceindex;
	    code += CheckSQL("Delete from server_stats",1);
	    $delete from srvr_calls
		where vice_index = $viceindex;
	    code += CheckSQL("Delete from srvr_calls",1);
	    $delete from srvr_mltcalls
		where vice_index = $viceindex;
	    code += CheckSQL("Delete from srvr_mltcalls",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	}
    }
    $ begin work;
    code = CheckSQL("Begin transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }
    $insert into server_stats
	(vice_index, time, system_cpu, user_cpu, idle_cpu, boot_time,
	 total_io)
	    values($viceindex,$time,$systemcpu,$usercpu,$idlecpu,
		   $boottime,$totalio);
    code += CheckSQL("Insert into server_stats",1);
    code += InsertCallArray(viceindex,CBSize,CBCount,VICE);
    code += InsertCallArray(viceindex,ResSize,ResCount,VICE);
    code += InsertCallArray(viceindex,SmonSize,SmonCount,VICE);
    code += InsertCallArray(viceindex,VolDSize,VolDCount,VICE);
    code += InsertMultiArray(viceindex,MultiSize,MultiCount,VICE);
    if (code != 0)
	$ rollback work;
    else
	$ commit work;
    return code;
}

int ReportResEvent(Vice,Time,Volid,HighWaterMark,AllocNumber,
		    DeallocNumber,ResOpSize,ResOp)
SmonViceId *Vice;
unsigned long Time;
VolumeId Volid;
long HighWaterMark;
long AllocNumber;
long DeallocNumber;
long ResOpSize;
ResOpEntry ResOp[];
{
    int code =0;
    $long viceindex;
    $long time;
    $long volid;
    $long highwater;
    $long alloc;
    $long dealloc;
    $long inserttime;
    $long resindex;
    
    viceindex = GetViceIndex(Vice);
    time = Time;
    volid = Volid;
    highwater = HighWaterMark;
    alloc = AllocNumber;
    dealloc = DeallocNumber;
    
    $select time, res_index
	into $inserttime, $resindex
        from res_stats
        where vice_index = $viceindex
          and volume = $volid;
    CheckSQL("Looking for resolution stats record",0);
    if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
	LogMsg(100,LogLevel,LogFile,	
		 "Duplicate resolution stat record: vice_index (%ld) volid (%ld) time (%ld)",
		 viceindex,volid,time);
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND) {
	    LogMsg(1000,LogLevel,LogFile,"Starting transaction");	
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    LogMsg(1000,LogLevel,LogFile,"Deleteing from res_stats");
	    $ delete from res_stats
		where res_index = $resindex;
	    code += CheckSQL("Deleting from res_stats",1);
	    LogMsg(1000,LogLevel,LogFile,"Deleteing from res_ops");
	    $ delete from res_ops
		where res_index = $resindex;
	    code += CheckSQL("Deleting from res_ops",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    } else
		$commit work;
	}
    }
    LogMsg(1000,LogLevel,LogFile,"Starting transaction");	
    $ begin work;
    code = CheckSQL("Start transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }
    LogMsg(1000,LogLevel,LogFile,"Inserting into res_stats");
    $ insert into res_stats
	(vice_index, time, volume, high_water, alloc_number, 
	 dealloc_number)
	values ($viceindex,$time,$volid,$highwater,
		$alloc,$dealloc);
    code += CheckSQL("Insert into res_stats",1);
    LogMsg(1000,LogLevel,LogFile,"Getting res_index");
    $ select res_index into $resindex
	from res_stats
	where vice_index = $viceindex
          and volume = $volid;
    code += CheckSQL("Getting res_index",1);
    LogMsg(1000,LogLevel,LogFile,"Inserting res op array");
    code += InsertResOpArray(resindex,ResOpSize,ResOp);
    if (code != 0)
	$ rollback work;
    else
	$ commit work;
    LogMsg(1000,LogLevel,LogFile,"Finished res record");
    return code;
}

int ReportRvmResEvent(Vice, Time, VolID, FileRes, DirRes, LSH_Size,
		      LogSizeHisto, LMH_Size, LogMaxHisto, Conflicts,
		      SHH_Size, SuccHierHist, FHH_Size, FailHierHist,
		      ResLog, VLH_Size, VarLogHisto, LS_Size,
		      LogSize)
SmonViceId *Vice;
unsigned long Time;
unsigned long VolID;
FileResStats *FileRes;
DirResStats *DirRes;
long LSH_Size;
HistoElem *LogSizeHisto;
long LMH_Size;
HistoElem *LogMaxHisto;
ResConflictStats *Conflicts;
long SHH_Size;
HistoElem *SuccHierHist;
long FHH_Size;
HistoElem *FailHierHist;
ResLogStats *ResLog;
long VLH_Size;
HistoElem *VarLogHisto;
long LS_Size;
HistoElem *LogSize;
{
    int code = 0;
    $long viceindex;
    $long time;
    $long volid;
    $long lshsize;
    $long lmhsize;
    $long shhsize;
    $long fhhsize;
    $long vlhsize;
    $long lssize;
    $long fileresolve;
    $long filesucc;
    $long fileconf;
    $long filerunt;
    $long fileweakeq;
    $long filereg;
    $long fileusrres;
    $long filesuccusrres;
    $long filepartial;
    $long dirresolve;
    $long dirsucc;
    $long dirconf;
    $long dirnowork;
    $long dirprob;
    $long dirpartial;
    $long nnconf;
    $long ruconf;
    $long uuconf;
    $long mvconf;
    $long lwconf;
    $long otherconf;
    $long wraps;
    $long admgrows;
    $long vallocs;
    $long vfrees;
    $long highest;
    $long resindex;
    $long inserttime;

    LogMsg(1000,LogLevel,LogFile,
		    "SpoolRvmResEntry: [0x%x:0x%x] @ %d",
		    Vice->IPAddress, VolID, Time);

    inserttime = 0;
    viceindex = GetViceIndex(Vice);
    time = Time;
    volid = VolID;
    
    /* Check to see if we need to outdate old records */
    volid = VolID;
    $select time, rvm_res_index
	into $inserttime, $resindex
	    from rvm_res_entry
	where vice_index = $viceindex
	    and volume = $volid;

    if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime)
    {
	LogMsg(100,LogLevel,LogFile,
	       "Duplicate RvmResEntry");
	return code;
    } else {
	if (sqlca.sqlcode != SQLNOTFOUND)
	{
	    /* remove old rvmres records */
	    $ begin work;
	    code = CheckSQL("Starting transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $delete from rvm_res_entry
		where rvm_res_index = $resindex;
	    code += CheckSQL("Delete form rvm_res_entry",1);
	    $delete from rvm_res_stats
		where rvm_res_index = $resindex;
	    code += CheckSQL("Delete from rvm_res_stats",1);
	    $delete from log_size_hist
		where rvm_res_index = $resindex;
	    code += CheckSQL("Delete from log_size_hist",1);
	    $delete from log_max_hist
		where rvm_res_index = $resindex;
	    code += CheckSQL("Delete from log_max_hist",1);
	    $delete from succ_hier_hist
		where rvm_res_index = $resindex;
	    code += CheckSQL("Delete from succ_hier_hist",1);
	    $delete from fail_hier_hist
		where rvm_res_index = $resindex;
	    code += CheckSQL("Delete from fail_hier_hist",1);
	    $delete from var_log_hist
		where rvm_res_index = $resindex;
	    code += CheckSQL("Delete from var_log_hist",1);
	    $delete from rvm_res_log_hist
		where rvm_res_index = $resindex;
	    code += CheckSQL("Delete from rvm_res_log_hist",1);
	    if (code != 0) {
		$rollback work;
		return code;
	    } else {
		$commit work;
		LogMsg(100,LogLevel,LogFile,
			"Obsolete rvm res information deleted successfully");
	    }
	}
    }
    /* insert new rvmres records */
    lshsize = LSH_Size;
    lmhsize = LMH_Size;
    shhsize = SHH_Size;
    fhhsize = FHH_Size;
    vlhsize = VLH_Size;
    lssize = LS_Size;
    $insert into rvm_res_entry
	(vice_index, time, volume, lsh_size, lmh_size, shh_size,
	 fhh_size, vlh_size, ls_size)
     values
	 ($viceindex, $time, $volid, $lshsize, $lmhsize, $shhsize,
	  $fhhsize, $vlhsize, $lssize);
    code += CheckSQL("Insert into rvm_res_entry",1);
    /* get the index assigned */
    $ select rvm_res_index into $resindex
      from rvm_res_entry
      where vice_index = $viceindex
        and volume = $volid;
    code += CheckSQL("Get the rvm_res_index",1);
    /* bail out if we don't have a good session index */
    if (code != 0) {
	$rollback work;
	return code;
    }
    /* Insert the rvm_stats */
    fileresolve = FileRes->Resolves;
    filesucc = FileRes->NumSucc;
    fileconf = FileRes->NumConf;
    filerunt = FileRes->RuntForce;
    fileweakeq = FileRes->WeakEq;
    filereg = FileRes->NumReg;
    fileusrres = FileRes->UsrResolver;
    filesuccusrres = FileRes->SuccUsrResolver;
    filepartial = FileRes->PartialVSG;
    dirresolve = DirRes->Resolves;
    dirsucc = DirRes->NumSucc;
    dirconf = DirRes->NumConf;
    dirnowork = DirRes->NumNoWork;
    dirprob = DirRes->Problems;
    dirpartial = DirRes->PartialVSG;
    nnconf = Conflicts->NameName;
    ruconf = Conflicts->RemoveUpdate;
    uuconf = Conflicts->UpdateUpdate;
    mvconf = Conflicts->Rename;
    lwconf = Conflicts->LogWrap;
    otherconf = Conflicts->Other;
    wraps = ResLog->NumWraps;
    admgrows = ResLog->NumAdmGrows;
    vallocs = ResLog->NumVAllocs;
    vfrees = ResLog->NumVFrees;
    highest = ResLog->Highest;
    /* &^%$ ESQL */
    $insert into rvm_res_stats
	(rvm_res_index, file_resolves, file_success, file_conflict,
	 file_runtforce, file_weak_eq, file_regular, file_usrres,
	 file_succ_userres, file_partial_vsg, dir_resolves,
	 dir_success, dir_conflict, dir_no_work, dir_problems,
	 dir_partial_vsg, nn_conf, ru_conf, uu_conf, mv_conf, lw_conf,
	 other_conf, wraps, adm_grows, vallocs, vfrees, highest)
     values
	 ($resindex, $fileresolve, $filesucc, $fileconf, $filerunt,
	  $fileweakeq, $filereg, $fileusrres, $filesuccusrres,
	  $filepartial, $dirresolve, $dirsucc, $dirconf, $dirnowork,
	  $dirprob, $dirpartial, $nnconf, $ruconf, $uuconf, $mvconf, $lwconf,
	  $otherconf, $wraps, $admgrows, $vallocs, $vfrees, $highest);
    code += CheckSQL("Insert into rvm_res_stats",1);

    code += InsertHistogram(resindex,LOG_SIZE_HIST,
			    LSH_Size,LogSizeHisto);
    code += InsertHistogram(resindex,LOG_MAX_HIST,
			    LMH_Size,LogSizeHisto);
    code += InsertHistogram(resindex,SUCC_HIER_HIST,
			    SHH_Size,SuccHierHist);
    code += InsertHistogram(resindex,FAIL_HIER_HIST,
			    FHH_Size,FailHierHist);
    code += InsertHistogram(resindex,VAR_LOG_HIST,
			    VLH_Size,VarLogHisto);
    code += InsertHistogram(resindex,RVM_RES_LOG_HIST,
			    LS_Size,LogSize);

    if (code !=0)
	$rollback work;
    else
	$commit work;
    return code;
}
	 
int ReportSrvOvrflw(Vice,Time,StartTime,EndTime,Count)
SmonViceId *Vice;
unsigned long Time;
unsigned long StartTime;
unsigned long EndTime;
long Count;
{
    int code = 0;

    $long viceindex;
    $long time;
    $long starttime;
    $long endtime;
    $long count;
    $long inserttime;

    LogMsg(100,LogLevel,LogFile,
	    "Spooling Server overflow");

    viceindex = GetViceIndex(Vice);
    time = Time;
    starttime = StartTime;
    endtime = EndTime;
    count = Count;

    $ select time into $inserttime from srv_overflow
	where vice_index = $viceindex
	  and start_time = $starttime
	  and end_time = $endtime
	  and cnt = $count;
    CheckSQL("Looking for server overflow event",0);
    if (!(sqlca.sqlcode == SQLNOTFOUND))
    {
	LogMsg(100,LogLevel,LogFile,
	       "Duplicate overflow event: vice_index (%d) time (%d)",
	       viceindex, time);
    } else {
	$insert into srv_overflow
	    (vice_index, time, start_time, end_time, cnt)
		values($viceindex, $starttime, $endtime, $count);
	code = CheckSQL("Insert into srv_overflow",1);
    }
    return code;
}

int ReportDiscoQ(Questionnaire)
DiscoMissQ *Questionnaire;
{
    int code = 0;
    $ long hostid;
    $ long user;
    $ long venus_major;
    $ long venus_minor;
    $ long advice_version_num;
    $ long adsrv_version;
    $ long admon_version;
    $ long q_version_num;
    $ long disco_time;
    $ long cachemiss_time;
    $ long fid_volume;
    $ long fid_vnode;
    $ long fid_uniquifier;
    $ long practice_session;
    $ long expected_affect;
    $ long comments_id;

    hostid = Questionnaire->hostid;
    user = Questionnaire->uid;
    venus_major = Questionnaire->venusmajorversion;
    venus_minor = Questionnaire->venusminorversion;
    advice_version_num = Questionnaire->advicemonitorversion;
    adsrv_version = Questionnaire->adsrv_version;
    admon_version = Questionnaire->admon_version;
    q_version_num = Questionnaire->q_version;
    disco_time = Questionnaire->disco_time;
    cachemiss_time = Questionnaire->cachemiss_time;
    fid_volume = Questionnaire->fid_volume;
    fid_vnode = Questionnaire->fid_vnode;
    fid_uniquifier = Questionnaire->fid_uniquifier;
    practice_session = Questionnaire->practice_session;
    expected_affect = Questionnaire->expected_affect;
    comments_id = Questionnaire->comment_id;

    $ begin work;
    code = CheckSQL("Start transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }

    $insert into advice_discomiss
	(hostid, uid, venus_major, venus_minor, admon_version_num, 
	 adsrv_version, admon_version, q_version_num, 
	 disco_time, cachemiss_time, 
	 fid_volume, fid_vnode, fid_uniquifier, 
	 practice_session, expected_affect, comments)
		values ($hostid, $user, $venus_major, $venus_minor, $advice_version_num,
			$adsrv_version, $admon_version, $q_version_num,
			$disco_time, $cachemiss_time, 
			$fid_volume, $fid_vnode, $fid_uniquifier,
			$practice_session, $expected_affect, $comments_id);

    code += CheckSQL("Insert into advice_discomiss", 1);
    if (code != 0)
	$ rollback work;
    else
	$ commit work;
    return code;
}

int ReportReconnQ(Questionnaire)
ReconnQ *Questionnaire;
{
    int code = 0;
    $ long hostid;
    $ long user;
    $ long venus_major;
    $ long venus_minor;
    $ long admon_version_num;
    $ long adsrv_version;
    $ long admon_version;
    $ long q_version_num;
    $ long volume_id;
    $ long cml_count;
    $ long disco_time;
    $ long reconn_time;
    $ long demand_walk_time;
    $ long num_reboots;
    $ long num_cache_hits;
    $ long num_cache_misses;
    $ long num_unique_hits;
    $ long num_unique_misses;
    $ long num_objs_not_refd;
    $ long aware_of_disco;
    $ long voluntary_disco;
    $ long practice_disco;
    $ long codacon;
    $ long sluggish;
    $ long observed_miss;
    $ long known_comments;
    $ long suspected_comments;
    $ long no_preparation;
    $ long hoardwalk;
    $ long num_pseudo_disco;
    $ long num_practice_disco;
    $ long prep_comments;
    $ long overall_impression;
    $ long final_comments;

    hostid = Questionnaire->hostid;
    user = Questionnaire->uid;
    venus_major = Questionnaire->venusmajorversion;
    venus_minor = Questionnaire->venusminorversion;
    admon_version_num = Questionnaire->advicemonitorversion;
    adsrv_version = Questionnaire->adsrv_version;
    admon_version = Questionnaire->admon_version;
    q_version_num = Questionnaire->q_version;
    volume_id = Questionnaire->volume_id;
    cml_count = Questionnaire->cml_count;
    disco_time = Questionnaire->disco_time;
    reconn_time = Questionnaire->reconn_time;
    demand_walk_time = Questionnaire->walk_time;
    num_reboots = Questionnaire->reboots;
    num_cache_hits = Questionnaire->hits;
    num_cache_misses = Questionnaire->misses;
    num_unique_hits = Questionnaire->unique_hits;
    num_unique_misses = Questionnaire->unique_misses;
    num_objs_not_refd = Questionnaire->not_referenced;
    aware_of_disco = Questionnaire->awareofdisco;
    voluntary_disco = Questionnaire->voluntary;
    practice_disco = Questionnaire->practicedisco;
    codacon = Questionnaire->codacon;
    sluggish = Questionnaire->sluggish;
    observed_miss = Questionnaire->observed_miss;
    known_comments = Questionnaire->known_other_comments;
    suspected_comments = Questionnaire->suspected_other_comments;
    no_preparation = Questionnaire->nopreparation;
    hoardwalk = Questionnaire->hoard_walk;
    num_pseudo_disco = Questionnaire->num_pseudo_disco;
    num_practice_disco = Questionnaire->num_practice_disco;
    prep_comments = Questionnaire->prep_comments;
    overall_impression = Questionnaire->overall_impression;
    final_comments = Questionnaire->final_comments;

    $insert into advice_reconn
	(hostid, uid, venus_major, venus_minor, admon_version_num,
	 adsrv_version, admon_version, q_version_num,
	 volume_id, cml_count, 
	 disco_time, reconn_time, demand_walk_time,
	 num_reboots, num_cache_hits, num_cache_misses,
	 num_unique_hits, num_unique_misses, num_objs_not_refd,
	 aware_of_disco, voluntary_disco, practice_disco,
	 codacon, sluggish, observed_miss, 
	 known_comments, suspected_comments,
	 no_preparation, hoardwalk, num_pseudo_disco, num_practice_disco,
	 prep_comments, overall_impression, final_comments)
		values ($hostid, $user, $venus_major, $venus_minor, $admon_version_num,
			$adsrv_version, $admon_version, $q_version_num,
			$volume_id, $cml_count,
			$disco_time, $reconn_time, $demand_walk_time,
			$num_reboots, $num_cache_hits, $num_cache_misses,
			$num_unique_hits, $num_unique_misses, $num_objs_not_refd,
			$aware_of_disco, $voluntary_disco, $practice_disco,
			$codacon, $sluggish, $observed_miss, 
			$known_comments, $suspected_comments,
			$no_preparation, $hoardwalk, $num_pseudo_disco, $num_practice_disco,
			$prep_comments, $overall_impression, $final_comments);

    code += CheckSQL("Insert into advice_reconn", 1);
    if (code != 0)
	$ rollback work;
    else
	$ commit work;
    return code;
}

PRIVATE long GetViceIndex(Vice)
SmonViceId *Vice;
{
    $ long host;
    $ long birth;
    $ long viceindex;
    
    host = Vice->IPAddress;
    birth = Vice->BirthTime;
    $ select instance_index into $viceindex
	from vice_instances
	    where host = $host
		and birth_time = $birth;
    if (CheckSQL("First vice lookup",1))
	return -1;
    if (sqlca.sqlcode == SQLNOTFOUND) {
	$ insert into vice_instances
	    (host, birth_time)
		values($host, $birth);
	if (CheckSQL("Insert into vice_instances", 1))
	    return -1;
	$ select instance_index into $viceindex
	    from vice_instances
		where host = $host
	      and birth_time = $birth;
	if (CheckSQL("Second viceindex lookup", 1))
	    return -1;
    }
    return(viceindex);
}

PRIVATE int InsertCallArray(index,size,array,type)
long index;
unsigned long size;
CallCountEntry *array;
int type;
{
    $ long idx;
    $ long callindex;
    $ long countentry;
    $ long countexit;
    $ long tsec;
    $ long tusec;
    $ long counttime;
    int i;
    int code =0;
    
    idx = index;

    /* check to make sure there are any valid entries */
    if (size <= 1)
	return code;

    for(i=1;i<size;i++) {
	/* only insert if there were some attempted calls */
	if (array[i].countent != 0) {
	    callindex = GetCallIndex(array[i].name,type);
	    countentry = array[i].countent;
	    countexit = array[i].countexit;
	    tsec = array[i].tsec;
	    tusec = array[i].tusec;
	    counttime = array[i].counttime;
	    if (type == VICE) {
		$ insert into srvr_calls
		    (vice_index, call_index, count_entry, count_exit, tsec,
		     tusec, counttime)
			values ($idx,$callindex,$countentry,$countexit,
				$tsec,$tusec,$counttime);
		code += CheckSQL("Insert into srvr_calls",1);
	    } else {
		$ insert into clnt_calls
		    (venus_index, call_index, count_entry, count_exit, tsec,
		     tusec, counttime)
			values ($idx,$callindex,$countentry,$countexit,
				$tsec,$tusec,$counttime);
		code += CheckSQL("Insert into clnt_calls",1);
	    }
	}
    }
    return code;
}

PRIVATE int InsertMultiArray(index,size,array,type)
long index;
unsigned long size;
MultiCallEntry *array;
int type;
{
    $ long idx;
    $ long callindex;
    $ long countentry;
    $ long countexit;
    $ long tsec;
    $ long tusec;
    $ long counttime;
    $ long counthost;
    int i;
    int code =0;
    
    idx = index;

    /* check to make sure there are any valid entries */
    if (size <= 1)
	return code;

    for(i=1;i<size;i++) {
	/* only insert if there were some attempted calls */
	if (array[i].countent != 0) {
	    callindex = GetMultiIndex(array[i].name,type);
	    countentry = array[i].countent;
	    countexit = array[i].countexit;
	    tsec = array[i].tsec;
	    tusec = array[i].tusec;
	    counttime = array[i].counttime;
	    counthost = array[i].counthost;
	    if (type == VICE) {
		$ insert into srvr_mltcalls
		    (vice_index, call_index, count_entry, count_exit, tsec,
		     tusec, counttime, counthost)
			values ($idx,$callindex,$countentry,$countexit,
				$tsec,$tusec,$counttime,$counthost);
		code += CheckSQL("Insert into srvr_mltcalls",1);
	    } else {
		$ insert into clnt_mltcalls
		    (venus_index, call_index, count_entry, count_exit, tsec,
		     tusec, counttime, counthost)
			values ($idx,$callindex,$countentry,$countexit,
				$tsec,$tusec,$counttime,$counthost);
		code += CheckSQL("Insert into srvr_mltcalls",1);
	    }		
	}
    }
    return code;
}

PRIVATE int GetCallIndex(CallName,type)
char *CallName;
int type;
{
    $ long callindex;
    $ char *callname;

    callname = CallName;

    if (type == VICE) {
	$ select call_index into $callindex
	    from srvr_call_names
		where call_name = $callname;
	if (CheckSQL("First srvr call index lookup",1))
	    return -1;
    } else {
	$ select call_index into $callindex
	    from clnt_call_names
		where call_name = $callname;
	if (CheckSQL("First clnt call index lookup",1))
	    return -1;
    }
    if (sqlca.sqlcode == SQLNOTFOUND) {
	if (type == VICE) {
	    $ insert into srvr_call_names
		(call_name) values ($callname);
	    if (CheckSQL("Insert into srvr_call_names",1))
		return -1;
	    $ select call_index into $callindex
		from srvr_call_names
		    where call_name = $callname;
	    if (CheckSQL("Second srvr_call index lookup",1))
		return -1;
	} else {
	    $ insert into clnt_call_names
		(call_name) values ($callname);
	    if (CheckSQL("Insert into clnt_call_names",1))
		return -1;
	    $ select call_index into $callindex
		from clnt_call_names
		    where call_name = $callname;
	    if (CheckSQL("Second clnt_call index lookup",1))
		return -1;
	}
    }
    return callindex;
}

PRIVATE int GetMultiIndex(CallName,type)
char *CallName;
int type;
{
    $ long callindex;
    $ char *callname;

    callname = CallName;

    if (type == VICE) {
	$ select call_index into $callindex
	    from srvr_mltcall_names
		where call_name = $callname;
	if (CheckSQL("First srvr_mltcall index lookup",1))
	    return -1;
    } else {
	$ select call_index into $callindex
	    from clnt_mltcall_names
		where call_name = $callname;
	if (CheckSQL("First clnt_mltcall index lookup",1))
	    return -1;
    }
    if (sqlca.sqlcode == SQLNOTFOUND) {
	if (type == VICE) {
	    $ insert into srvr_mltcall_names
		(call_name) values ($callname);
	    if (CheckSQL("Insert into srvr_mltcall_names",1))
		return -1;
	    $ select call_index into $callindex
		from srvr_mltcall_names
		    where call_name = $callname;
	    if (CheckSQL("Second srvr_mltcall index lookup",1))
		return -1;
	} else {
	    $ insert into clnt_mltcall_names
		(call_name) values ($callname);
	    if (CheckSQL("Insert into clnt_mltcall_names",1))
		return -1;
	    $ select call_index into $callindex
		from clnt_mltcall_names
		    where call_name = $callname;
	    if (CheckSQL("Second clnt_mltcall index lookup",1))
		return -1;
	}
    }
    return callindex;
}


PRIVATE int InsertResOpArray(ResIndex,ResOpSize,ResOp)
long ResIndex;
long ResOpSize;
ResOpEntry *ResOp;
{
    $ long alloc;
    $ long dealloc;
    $ long resindex;
    $ long opindex;
    int i;
    int code =0;

    resindex = ResIndex;
    for (i=0;i<ResOpSize;i++) {
	alloc = ResOp[i].alloccount;
	dealloc = ResOp[i].dealloccount;
	opindex = i;
	if (alloc != 0 || dealloc != 0) {
	    $ insert into res_ops
		(res_index, res_op_index, alloc_number, dealloc_number)
		    values($resindex,$opindex,$alloc,$dealloc);
	    code += CheckSQL("Inserting into res_ops",1);
	}
    }
    return code;
}

int InsertHistogram(index, histType, histSize, histo)
long index;
int histType;
long histSize;
HistoElem *histo;
{
    $long bucketnum;
    $long value;
    $char insquery[512];   /* esql doesn't use cpp #defines */
    char *tableName;
    char resindStr[SQL_LENGTH];
    int result=0;

    strcpy(insquery,"insert into ");
    switch (histType) {
    case LOG_SIZE_HIST:
	tableName = "log_size_hist";
	break;
    case LOG_MAX_HIST:
	tableName = "log_max_hist";
	break;
    case SUCC_HIER_HIST:
	tableName = "succ_hier_hist";
	break;
    case FAIL_HIER_HIST:
	tableName = "fail_hier_hist";
	break;
    case VAR_LOG_HIST:
	tableName = "var_log_hist";
	break;
    case RVM_RES_LOG_HIST:
	tableName = "rvm_res_log_hist";
	break;
    }
    strcat(insquery,tableName);
    strcat(insquery," (rvm_res_index, bucketnum, value) values (");
    sprintf(resindStr,"%ld",index);
    strcat(insquery,resindStr);
    strcat(insquery,", ?, ?)");
    $ prepare insq from $insquery;

    for (bucketnum = 0; bucketnum < histSize; bucketnum++) {
	value = histo[bucketnum].bucket;
	$ execute insq using $bucketnum, $value;
	result += CheckSQL("Inserting a histogram bucket",1);
    }
    return result;

}

void UpdateDB()
{
    $update statistics;
}

int InitDB(database)
char * database;
{
$   char *dbname;

    setenv("DATABASES","/afs/cs.cmu.edu/project/coda/scylla/databases",1);
    dbname = database;

$   database $dbname;
    return (CheckSQL("InitDB", 1));
}

PRIVATE int CheckSQL(where,fatal) 
char *where;
int fatal;
{
    LogMsg(1000,LogLevel,LogFile,
	     "Checking: %s",where);
    if (sqlca.sqlcode == 0 || sqlca.sqlcode == SQLNOTFOUND) return 0;

    if (fatal) 
	LogMsg(0,LogLevel,LogFile,
	       "%s: Fatal SQL Error %d (%s)", where, sqlca.sqlcode,
						       sqlca.sqlerrm);
    else
        LogMsg(1,LogLevel,LogFile,
	       "%s: SQL Error %d (%s)", where, sqlca.sqlcode,
                                                 sqlca.sqlerrm);
    return -1;
}

int CloseDB() {
$   close database;
    return(CheckSQL("CloseDB", 0));
}

int ReportIotInfoCall(Venus, Info, AppNameLen, AppName)
VmonVenusId *Venus;
IOT_INFO *Info;
RPC2_Integer AppNameLen;
RPC2_String *AppName;
{
    int code = 0;
    $ long venus_index;
    $ char app_name[10];
    $ long tid;
    $ long res_opt;
    $ long elapsed_time;
    $ long readset_size;
    $ long writeset_size;
    $ long readvol_num;
    $ long writevol_num;
    $ long validation;
    $ long invalid_size;
    $ long backup_obj_num;
    $ long life_cycle;
    $ long pred_num;
    $ long succ_num;
    $ long dummy;

    LogMsg(100, LogLevel,LogFile, "SpoolIotInfoRecord: Venus = [%x %d]",
			    Venus->IPAddress, Venus->BirthTime);
    LogMsg(100, LogLevel,LogFile, "SpoolIotInfoRecord: AppName = %d AppLen = %d\n",
			    AppName, AppNameLen);
    LogMsg(100, LogLevel,LogFile, "SpoolIotInfoRecord: Tid = %d ResOpt = %d ElapsedTime = %d ReadSetSize = %d WriteSetSize = %d ReadVolNum = %d WriteVolNum = %d Validation = %d InvalidSize = %d BackupObjNum = %d LifeCycle = %d PredNum = %d SuccNum = %d\n", Info->Tid, Info->ResOpt, Info->ElapsedTime, Info->ReadSetSize, Info->WriteSetSize, Info->ReadVolNum, Info->WriteVolNum, Info->Validation, Info->InvalidSize, Info->BackupObjNum, Info->LifeCycle, Info->PredNum, Info->SuccNum);

    venus_index = GetVenusIndex(Venus);
    strncpy(app_name, AppName, 10);
    tid = Info->Tid;
    res_opt = Info->ResOpt;
    elapsed_time = Info->ElapsedTime;
    readset_size = Info->ReadSetSize;
    writeset_size = Info->WriteSetSize;
    readvol_num = Info->ReadVolNum;
    writevol_num = Info->WriteVolNum;
    validation = Info->Validation;
    invalid_size = Info->InvalidSize;
    backup_obj_num = Info->BackupObjNum;
    life_cycle = Info->LifeCycle;
    pred_num = Info->PredNum;
    succ_num = Info->SuccNum;

    $ select tid into $dummy from iot_info
	where venus_index = $venus_index
	    and tid = $tid;

    if (sqlca.sqlcode != SQLNOTFOUND) {
	LogMsg(100, LogLevel,LogFile, "SpoolIotInfoRecord: Duplicate IotInfo Record");
	return 0;
    }

    $ begin work;
    code = CheckSQL("Begin transaction",1);
    if (code != 0) {
	$ rollback work;
	return code;
    }
	
    $ insert into iot_info
      (venus_index, app_name, tid, res_opt, elapsed_time,
       readset_size, writeset_size, readvol_num, writevol_num,
       validation, invalid_size, backup_obj_num, 
       life_cycle, pred_num, succ_num)
	values ($venus_index, $app_name, $tid, $res_opt, $elapsed_time,
		$readset_size, $writeset_size, $readvol_num, $writevol_num,
		$validation, $invalid_size, $backup_obj_num, 
		$life_cycle, $pred_num, $succ_num);
	  code += CheckSQL("Insert into iot_info", 1);
    if (code != 0)
      $ rollback work;
    else
      $ commit work;
    return code;
}

int ReportIotStatsCall(Venus, Time, Stats)
VmonVenusId *Venus;
RPC2_Integer Time;
IOT_STAT *Stats;
{
	int code = 0;
	$ long time;
	$ long inserttime;
	$ long venus_index;
	$ long max_elapsed_time;
	$ long avg_elapsed_time;
	$ long max_readset_size;
	$ long avg_readset_size;
	$ long max_writeset_size;
	$ long avg_writeset_size;
	$ long max_readvol_num;
	$ long avg_readvol_num;
	$ long max_writevol_num;
	$ long avg_writevol_num;
	$ long commited;
	$ long pending;
	$ long resolved;
	$ long repaired;

	LogMsg(100, LogLevel,LogFile,
		"SpoolIotStatsRecord: Venus = [%x %d], Time = %d",
		Venus->IPAddress, Venus->BirthTime, Time);
	
	LogMsg(100, LogLevel,LogFile, "SpoolIotStatsRecord: MaxElapsedTime = %d AvgElapsedTime = %d MaxReadSetSize = %d AvgReadSetSize = %d MaxWriteSetSize = %d AvgWriteSetSize = %d MaxReadVolNum = %d AvgReadVolNum = %d MaxWriteVolNum = %d AvgWriteVolNum = %d Committed = %d Pending = %d Resolved = %d Repaired = %d\n", Stats->MaxElapsedTime, Stats->AvgElapsedTime, Stats->MaxReadSetSize, Stats->AvgReadSetSize, Stats->MaxWriteSetSize, Stats->AvgWriteSetSize, Stats->MaxReadVolNum, Stats->AvgReadVolNum, Stats->MaxWriteVolNum, Stats->AvgWriteVolNum, Stats->Committed, Stats->Pending, Stats->Resolved, Stats->Repaired);

	time = Time;
	venus_index = GetVenusIndex(Venus);
	max_elapsed_time = Stats->MaxElapsedTime;
	avg_elapsed_time = Stats->AvgElapsedTime;
	max_readset_size = Stats->MaxReadSetSize;
	avg_readset_size = Stats->AvgReadSetSize;
	max_writeset_size = Stats->MaxWriteSetSize;
	avg_writeset_size = Stats->AvgWriteSetSize;
	max_readvol_num = Stats->MaxReadVolNum;
	avg_readvol_num = Stats->AvgReadVolNum;	
	max_writevol_num = Stats->MaxWriteVolNum;
	avg_writevol_num = Stats->AvgWriteVolNum;
	commited = Stats->Committed;
	pending = Stats->Pending;
	resolved = Stats->Resolved;
	repaired = Stats->Repaired;

	$ select time into $inserttime from iot_stats
	  where venus_index = $venus_index;
	CheckSQL("Looking for entry in iot_stats", 0);

	if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
	    LogMsg(100,LogLevel,LogFile,
	       "Duplicate iot statistics record: venus_index (%d), time (%d)",
		venus_index, time);
	    return code;
	} else {
	    if (sqlca.sqlcode != SQLNOTFOUND) {
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from iot_stats
	      where venus_index = $venus_index;
            code += CheckSQL("Delete from iot_stats",1);
	    if (code !=0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	    }
        }

        $ begin work;
        code = CheckSQL("Begin transaction",1);
        if (code != 0) {
	    $ rollback work;
	    return code;
    	}
	
	$ insert into iot_stats
	  (venus_index, time, max_elapsed_time, avg_elapsed_time,
	   max_readset_size, avg_readset_size, 
	   max_writeset_size, avg_writeset_size,
	   max_readvol_num, avg_readvol_num,
	   max_writevol_num, avg_writevol_num,
	   commited, pending, resolved, repaired)
	    values ($venus_index, $time, $max_elapsed_time, $avg_elapsed_time,
		    $max_readset_size, $avg_readset_size, 
		    $max_writeset_size, $avg_writeset_size,
		    $max_readvol_num, $avg_readvol_num,
		    $max_writevol_num, $avg_writevol_num,
		    $commited, $pending, $resolved, $repaired);
	code += CheckSQL("Insert into iot_stats", 1);
	if (code != 0)
	  $ rollback work;
	else
	  $ commit work;
	return code;
}

int ReportSubtreeCall(Venus, Time, Stats)
VmonVenusId *Venus;
RPC2_Integer Time;
LocalSubtreeStats *Stats;
{
	int code = 0;
	$ long time;
	$ long inserttime;
	$ long venus_index;
	$ long subtree_num;
	$ long max_subtree_size;
	$ long avg_subtree_size;
	$ long max_subtree_hgt;
	$ long avg_subtree_hgt;
	$ long max_mutation_num;
	$ long avg_mutation_num;

	LogMsg(100, LogLevel,LogFile,
		"SpoolSubtreeStatsRecord: Venus = [%x %d], Time = %d",
		Venus->IPAddress, Venus->BirthTime, Time);

	LogMsg(100, LogLevel,LogFile, "SpoolSubtreeStatsRecord: SubtreeNum = %d MaxSubtreeSize = %d AvgSubtreeSize = %d MaxSubtreeHgt = %d AvgSubtreeHgt = %d MaxMutationNum = %d AvgMutationNum = %d\n", Stats->SubtreeNum, Stats->MaxSubtreeSize, Stats->AvgSubtreeSize, Stats->MaxSubtreeHgt, Stats->AvgSubtreeHgt, Stats->MaxMutationNum, Stats->AvgMutationNum);

	time = Time;
	venus_index = GetVenusIndex(Venus);
	subtree_num = Stats->SubtreeNum;
	max_subtree_size = Stats->MaxSubtreeSize;
	avg_subtree_size = Stats->AvgSubtreeSize;
	max_subtree_hgt = Stats->MaxSubtreeHgt;
	avg_subtree_hgt = Stats->AvgSubtreeHgt;
	max_mutation_num = Stats->MaxMutationNum;
	avg_mutation_num = Stats->AvgMutationNum;
	
	$ select time into $inserttime from subtree_stats
	  where venus_index = $venus_index;
	CheckSQL("Looking for entry in subtree_stats", 0);

	if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
	    LogMsg(100,LogLevel,LogFile,
	       "Duplicate subtree statistics record: venus_index (%d), time (%d)",
		venus_index, time);
	    return code;
	} else {
	    if (sqlca.sqlcode != SQLNOTFOUND) {
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from subtree_stats
	      where venus_index = $venus_index;
	    code += CheckSQL("Delete from subtree_stats",1);
	    if (code !=0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	    }
        }

        $ begin work;
        code = CheckSQL("Begin transaction",1);
        if (code != 0) {
	    $ rollback work;
	    return code;
    	}
	
	$ insert into subtree_stats
	  (venus_index, time, subtree_num,
	   max_subtree_size, avg_subtree_size,
	   max_subtree_hgt, avg_subtree_hgt,
	   max_mutation_num, avg_mutation_num)
	    values 
	      ($venus_index, $time, $subtree_num,
	       $max_subtree_size, $avg_subtree_size,
	       $max_subtree_hgt, $avg_subtree_hgt,
	       $max_mutation_num, $avg_mutation_num);
	code += CheckSQL("Insert into subtree_stats", 1);
	if (code != 0)
	  $ rollback work;
	else
	  $ commit work;
	return code;
}

int ReportRepairCall(Venus, Time, Stats)
VmonVenusId *Venus;
RPC2_Integer Time;
RepairSessionStats *Stats;
{
	int code = 0;
	$ long time;
	$ long inserttime;
	$ long venus_index;
	$ long session_num;
	$ long commit_num;
	$ long abort_num;
	$ long check_num;
	$ long preserve_num;
	$ long discard_num;
	$ long remove_num;
	$ long global_view_num;
	$ long local_view_num;
	$ long keep_local_num;
	$ long list_local_num;
	$ long rep_mutation_num;
	$ long miss_target_num;
	$ long miss_parent_num;
	$ long acl_deny_num;
	$ long update_update_num;
	$ long name_name_num;
	$ long remove_update_num;


	LogMsg(100, LogLevel,LogFile,
		"SpoolRepairStatsRecord: Venus = [%x %d], Time = %d",
		Venus->IPAddress, Venus->BirthTime, Time);

	LogMsg(100, LogLevel,LogFile, "SpoolRepairStatsRecord: SessionNum = %d CommitNum = %d AbortNum = %d CheckNum = %d PreserveNum = %d DiscardNum = %d RemoveNum = %d GlobalViewNum = %d LocalViewNum = %d KeepLocalNum = %d ListLocalNum = %d NewCommand1Num = %d NewCommand2Num = %d NewCommand3Num = %d NewCommand4Num = %d NewCommand5Num = %d NewCommand6Num = %d NewCommand7Num = %d NewCommand8Num = %d MissTargetNum = %d MissParentNum = %d AclDenyNum = %d UpdateUpdateNum = %d NameNameNum = %d RemoveUpdateNum = %d\n", Stats->SessionNum, Stats->CommitNum, Stats->AbortNum, Stats->CheckNum, Stats->PreserveNum, Stats->DiscardNum, Stats->RemoveNum, Stats->GlobalViewNum, Stats->LocalViewNum, Stats->KeepLocalNum, Stats->ListLocalNum, Stats->NewCommand1Num, Stats->NewCommand2Num, Stats->NewCommand3Num, Stats->NewCommand4Num, Stats->NewCommand5Num, Stats->NewCommand6Num, Stats->NewCommand7Num, Stats->NewCommand8Num, Stats->MissTargetNum, Stats->MissParentNum, Stats->AclDenyNum, Stats->UpdateUpdateNum, Stats->NameNameNum, Stats->RemoveUpdateNum);

	time = Time;
	venus_index = GetVenusIndex(Venus);
	session_num = Stats->SessionNum;
	commit_num = Stats->CommitNum;
	abort_num = Stats->AbortNum;	
	check_num = Stats->CheckNum;
	preserve_num = Stats->PreserveNum;
	discard_num = Stats->DiscardNum;
	remove_num = Stats->RemoveNum;
	global_view_num = Stats->GlobalViewNum;
	local_view_num = Stats->LocalViewNum;
	keep_local_num = Stats->KeepLocalNum;
	list_local_num = Stats->ListLocalNum;
	rep_mutation_num = Stats->RepMutationNum;
	miss_target_num = Stats->MissTargetNum;
	miss_parent_num = Stats->MissParentNum;
	acl_deny_num = Stats->AclDenyNum;
	update_update_num = Stats->UpdateUpdateNum;
	name_name_num = Stats->NameNameNum;
	remove_update_num = Stats->RemoveUpdateNum;
	
	$ select time into $inserttime from repair_stats
	  where venus_index = $venus_index;
	CheckSQL("Looking for entry in repair_stats", 0);

	if (sqlca.sqlcode != SQLNOTFOUND && time <= inserttime) {
	    LogMsg(100,LogLevel,LogFile,
	       "Duplicate repair statistics record: venus_index (%d), time (%d)",
		venus_index, time);
	    return code;
	} else {
	    if (sqlca.sqlcode != SQLNOTFOUND) {
	    $ begin work;
	    code = CheckSQL("Start transaction",1);
	    if (code != 0) {
		$ rollback work;
		return code;
	    }
	    $ delete from repair_stats
	      where venus_index = $venus_index;
            code += CheckSQL("Delete from repair_stats",1);
	    if (code !=0) {
		$ rollback work;
		return code;
	    } else
		$ commit work;
	    }
        }

        $ begin work;
        code = CheckSQL("Begin transaction",1);
        if (code != 0) {
	    $ rollback work;
	    return code;
    	}
	
	$ insert into repair_stats
	  (venus_index, time, session_num, commit_num, abort_num,
	   check_num, preserve_num, discard_num, remove_num,	
	   global_view_num, local_view_num, keep_local_num, list_local_num,
	   rep_mutation_num, miss_target_num, miss_parent_num, acl_deny_num,
	   update_update_num, name_name_num, remove_update_num)
	    values 
	      ($venus_index, $time, $session_num, $commit_num, $abort_num,
	       $check_num, $preserve_num, $discard_num, $remove_num,	
	       $global_view_num, $local_view_num, $keep_local_num, $list_local_num,
	       $rep_mutation_num, $miss_target_num, $miss_parent_num, $acl_deny_num,
	       $update_update_num, $name_name_num, $remove_update_num);

	code += CheckSQL("Insert into repair_stats", 1);
	if (code != 0)
	  $ rollback work;
	else
	  $ commit work;
	return code;
}

int ReportRwsStatsCall(Venus, Time, Stats)
VmonVenusId *Venus;
RPC2_Integer Time;
ReadWriteSharingStats *Stats;
{
	int code = 0;
	$ long time;
	$ long venus_index;
	$ long volume_id;
	$ long rw_sharing_count;
	$ long disc_read_count;
	$ long disc_duration;
	$ long dummy;

	LogMsg(100, LogLevel,LogFile,
		"SpoolRwsStatsRecord: Venus = [%x %d], Time = %d",
		Venus->IPAddress, Venus->BirthTime, Time);
	
	LogMsg(100, LogLevel,LogFile, "SpoolRwsStatsRecord: Vid = %d RwSharingCount = %d DiscReadCount = %d DiscDuration = %d\n", Stats->Vid, Stats->RwSharingCount, Stats->DiscReadCount, Stats->DiscDuration);

	time = Time;
	venus_index = GetVenusIndex(Venus);
	volume_id = Stats->Vid;
	rw_sharing_count = Stats->RwSharingCount;
	disc_read_count = Stats->DiscReadCount;
	disc_duration = Stats->DiscDuration;

	$ select volume_id into $dummy from rws_stats
	    where venus_index = $venus_index
	    and time = $time
	    and volume_id = $volume_id
	    and rw_sharing_count = $rw_sharing_count
	    and disc_read_count = $disc_read_count
	    and disc_duration = $disc_duration;

        if (sqlca.sqlcode != SQLNOTFOUND) {
	    LogMsg(100, LogLevel,LogFile, "SpoolRwsStatsRecord: Duplicate RwsStats Record\n");
	    return 0;
        }

        $ begin work;
        code = CheckSQL("Begin transaction",1);
        if (code != 0) {
	    $ rollback work;
	    return code;
    	}
	
	$ insert into rws_stats
	  (venus_index, time, volume_id, rw_sharing_count,
	   disc_read_count, disc_duration)
	    values ($venus_index, $time, $volume_id, $rw_sharing_count,
		    $disc_read_count, $disc_duration);
	code += CheckSQL("Insert into rws_stats", 1);
	if (code != 0)
	  $ rollback work;
	else
	  $ commit work;
	return code;
}
