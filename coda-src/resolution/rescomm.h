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







/* rescomm.h
 * Communication management for resolution subsystem
 * Created Puneet Kumar, June 1990
 */
#ifndef _RES_COMM_H_
#define _RES_COMM_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <olist.h>
#include <dlist.h>
#include <vice.h>
#include <res.h>
#define VSG_MEMBERS 8
#include "resutil.h"

/* forward declarations */
class res_mgrpent;
class srvent;
class srv_iterator;
class RepResCommCtxt;
class resmgrp_iterator;
class conninfo;
class conninfo_iterator;
class pdlist;
class VNResLog;
class rlent;

extern int GetResMgroup(res_mgrpent **, unsigned long, unsigned long *);
extern int PutResMgroup(res_mgrpent **);
extern void ResCommInit();
extern srvent *FindServer(unsigned long);
extern void GetServer(srvent **, unsigned long);
extern void PutServer(srvent **);
extern void ServerPrint(int);
extern void ServerPrint();
extern void ServerPrint(FILE *);
extern  void ResMgrpPrint(int);
extern void ResMgrpPrint();
extern void ResMgrpPrint(FILE *);
extern long ViceResolve(RPC2_Handle, ViceFid *);
extern conninfo *GetConnectionInfo(RPC2_Handle);

class srvent {
  friend void ResCommInit();
  friend srvent *FindServer(unsigned long);
  friend void GetServer(srvent **, unsigned long);
  friend void ServerPrint(int);
  friend class srv_iterator;
  friend void ResCheckServerLWP_worker();
    /* The server list. */
    static olist *srvtab;
    static struct condition srvtab_sync;

    /* Other shared data. */
    static int servers;

    /* Transient members. */
    olink tblhandle;
    char *name;
    unsigned long host;
    unsigned binding : 1;	/* 1 --> BINDING, 0 --> NOT_BINDING */
    enum state {up, down, unknown} srvrstate;

    /* Constructors, destructors, and private utility routines. */
    srvent(unsigned long);
    ~srvent();
  public:
    int Connect(RPC2_Handle *, int);
    void Reset();
    int	ServerIsDown();
    int ServerIsUp();
    void ServerError(int *);
    void print();
    void print(FILE *);
    void print(int);
};

class srv_iterator : public olist_iterator {

  public:
    srv_iterator();
    srvent *operator()();
};

class RepResCommCtxt {
  public:
    RPC2_Integer HowMany;
    RPC2_Handle handles[VSG_MEMBERS];
    unsigned long hosts[VSG_MEMBERS];
    RPC2_Integer retcodes[VSG_MEMBERS];	
    unsigned long primaryhost;
    RPC2_Multicast *MIp;
    unsigned dying[VSG_MEMBERS];
    
    RepResCommCtxt();
    ~RepResCommCtxt();
    
    void print();
    void print(FILE *);
    void print(int);
};
  
class res_mgrpent {
  public:
    /* the mgrp list */
    static dlist *ResMgrpTab;
    static struct condition TabSync;
    
    /* shared data */
    static int resmgrps;
    
    /* transient members */
    dlink tblhandle;
    
    /* Static state; immutable after construction */
    unsigned long VSGAddr;
    RPC2_Multicast McastInfo;
    unsigned long Hosts[VSG_MEMBERS];	/* All VSG hosts in canonical order */
    
    /* Dynamic state */
    unsigned inuse  : 1;
    unsigned dying  : 1;
    RepResCommCtxt  rrcc;

    /* Constructors, Destructors */
    res_mgrpent(unsigned long, RPC2_Handle);
    ~res_mgrpent();

    int CreateMember(unsigned long);
    void KillMember(unsigned long, int);
    int GetHostSet(unsigned long *);
    void PutHostSet();
    int CheckResult();
    int IncompleteVSG();
    int GetIndex(unsigned long);
    void print();
    void print(FILE *);
    void print(int);
};

const unsigned long ALL_VSGS = (unsigned long)-1;

class resmgrp_iterator : public dlist_iterator {
    unsigned long VSGaddr;

  public:
    resmgrp_iterator(unsigned long = ALL_VSGS);
    res_mgrpent *operator()();
};

class conninfo {
  friend class conninfo_iterator;
  friend void srvent::Reset();
  friend void ResCommInit();
  friend long RS_NewConnection(RPC2_Handle , RPC2_Integer, RPC2_Integer,RPC2_Integer, RPC2_Integer, RPC2_CountedBS *);
    /* globals state */
    static  olist *CInfoTab;
    static  int ncinfos;

    olink   tblhandle;
    /* immutable info once created */
    unsigned long RemoteAddr;
    unsigned short RemotePortNum;
    int	SecLevel;
    RPC2_Handle	cid;

    /* constructors and destructors */
    conninfo(RPC2_Handle, int);
    ~conninfo();

  public:
    unsigned long GetRemoteHost();
    int	GetSecLevel();
    unsigned short GetRemotePort();
};

class conninfo_iterator : public olist_iterator {
    RPC2_Handle key;
  public:
    conninfo_iterator(RPC2_Handle = 0);
    conninfo *operator()();
};

#define RESCOMM_MAXBSLEN 2048

#endif not _RES_COMM_H_
