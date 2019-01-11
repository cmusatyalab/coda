/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

/*
 *
 *  Definitions of Venus Statistics
 *
 */

#ifndef _VENUS_STATS_H_
#define _VENUS_STATS_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <rpc2/rpc2.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

#define NVFSOPS 40 /* XXX -JJK */
#define VFSSTATNAMELEN 13
typedef struct VFSStat {
    char name[VFSSTATNAMELEN]; /* XXX -JJK */
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

#define RPCOPSTATNAMELEN 20

typedef struct RPCOpStat {
    char name[RPCOPSTATNAMELEN]; /* XXX -JJK */
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

#endif /* _VENUS_STATS_H_ */
