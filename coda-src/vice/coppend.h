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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/





/*
 *
 * Specification of the Cop Pending table entries
 *
 */

#ifndef _VICE_COPPEND_H_
#define _VICE_COPPEND_H_ 1


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <lock.h>
#include <vice.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus


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
    int func(int);
    
    friend void InitCopPendingTable();
    friend cpent;
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
#endif not _VICE_COPPEND_H
