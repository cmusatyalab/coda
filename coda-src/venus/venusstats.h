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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/venus/venusstats.h,v 1.1 1996/11/22 19:11:52 braam Exp $";
#endif /*_BLURB_*/








/*
 *
 *  Definitions of Venus Statistics
 *
 */


#ifndef _VENUS_STATS_H_
#define _VENUS_STATS_H_ 1


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <rpc2.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus



#define	NVFSOPS	40	/* XXX -JJK */

typedef struct VFSStat {
    char name[12];	    /* XXX -JJK */
    int success;
    int retry;
    int timeout;
    int failure;
    double time;
    double time2;
} VFSStat;


typedef struct VFSStatistics {
    struct VFSStat VFSOps[NVFSOPS];
} VFSStatistics;


typedef struct FSOStatistics {
    int FSO1;
    int FSO2;
    int FSO3;
    int FSO4;
} FSOStatistics;


typedef struct VolStatistics {
    int Vol1;
    int Vol2;
    int Vol3;
    int Vol4;
} VolStatistics;


typedef struct ConnStatistics {
    int Conn1;
    int Conn2;
    int Conn3;
    int Conn4;
} ConnStatistics;


typedef struct MgrpStatistics {
    int Mgrp1;
    int Mgrp2;
    int Mgrp3;
    int Mgrp4;
} MgrpStatistics;


typedef struct ServerStatistics {
    int Server1;
    int Server2;
    int Server3;
    int Server4;
} ServerStatistics;


typedef struct VSGStatistics {
    int VSG1;
    int VSG2;
    int VSG3;
    int VSG4;
} VSGStatistics;


#define RPCOPSTATNAMELEN 16

typedef struct RPCOpStat {
    char name[RPCOPSTATNAMELEN];	    /* XXX -JJK */
    int good;
    int bad;
    float time;
    int Mgood;
    int Mbad;
    float Mtime;
    int rpc_retries;
    int Mrpc_retries;
} RPCOpStat;


typedef struct RPCOpStatistics {
    RPCOpStat RPCOps[srvOPARRAYSIZE];
} RPCOpStatistics;


typedef struct RPCPktStatistics {
    struct SStats RPC2_SStats_Uni;
    struct SStats RPC2_SStats_Multi;
    struct RStats RPC2_RStats_Uni;
    struct RStats RPC2_RStats_Multi;
    struct sftpStats SFTP_SStats_Uni;
    struct sftpStats SFTP_SStats_Multi;
    struct sftpStats SFTP_RStats_Uni;
    struct sftpStats SFTP_RStats_Multi;
} RPCPktStatistics;


typedef struct CommStatistics {
    ConnStatistics ConnStats;
    MgrpStatistics MgrpStats;
    ServerStatistics ServerStats;
    VSGStatistics VSGStats;
    RPCOpStatistics RPCOpStats;
    RPCPktStatistics RPCPktStats;
} CommStatistics;


typedef struct VenusStatistics {
    VFSStatistics VFSStats;
    FSOStatistics FSOStats;
    VolStatistics VolStats;
    CommStatistics CommStats;
} VenusStatistics;

#endif	not _VENUS_STATS_H_
