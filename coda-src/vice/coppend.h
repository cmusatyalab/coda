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





/*
 *
 * Specification of the Cop Pending table entries
 *
 */

#ifndef _VICE_COPPEND_H_
#define _VICE_COPPEND_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <lwp/lock.h>
#include <vice.h>
#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif


#include <ohash.h>

#define	MAXFIDS	4	/* The most fids used by any operation. */
#define CPENTMAGIC 0x202842
/* cop pending entry */
class cpent : public olink { 
    friend void AddPairToCopPendingTable(ViceStoreId *, ViceFid *);
    friend long InternalCOP2(RPC2_Handle , ViceStoreId *, ViceVersionVector *);
    friend class cpman;

    ViceStoreId StoreId;	
    ViceFid fids[MAXFIDS]; /* fids of all objects mutated in operation */ 
    long time;		   /* expiration time for entry */
    int deqing;	
    int id;

  public:
    cpent(ViceStoreId *, ViceFid *);
    ~cpent();
    void print();
    void print(FILE *);
    void print(int);
};

#define	COPHASHSIZE 128 /* size of objects hash table */

/* daemon to maintain entries for pending cop's */
class cpman {
    char *name;
    struct Lock lock;
    int pid;
    ohashtab objects;

    cpman(char * ="anonymous coppendbuster");
    ~cpman();

    friend int cpman_func(cpman *);
    int func(int);
    
    friend void InitCopPendingTable();
    friend class cpent;
    friend void AddPairToCopPendingTable(ViceStoreId *, ViceFid *);
    cpent *find(ViceStoreId *);

  public:
    void add(cpent *);
    void remove(cpent *);
    // cpent *find(ViceFid *);
    cpent *findanddeq(ViceStoreId *);
    void print();
    void print(FILE *);
    void print(int);
};

extern cpman *CopPendingMan;
extern void InitCopPendingTable();
extern void AddToCopPendingTable(ViceStoreId *, ViceFid *);
extern void AddPairToCopPendingTable(ViceStoreId *, ViceFid *);

#endif /* _VICE_COPPEND_H_ */
