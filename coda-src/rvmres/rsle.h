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





#ifndef _RSLE_H
#define _RSLE_H 	1
/* 
 * class SpoolListEntry(rsle - r stands for rvm to 
 *			distinguish this from the sle 
 *			entry spooled for vm resolution)
 * 	log records spooled for a directory vnode.
 *	These are allocated in VM; only a slot has been reserved in RVM.
 *	all these records are copied to the RVM directory log during
 *	transaction commmit time.
 */
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdarg.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <olist.h>
#include "ops.h"
#include "recle.h"

class rsle : public olink {
  public:
    int index;		/* index of log entry when it will be placed in rvm */
    int seqno;
    ViceStoreId storeid;
    VnodeId dvn;
    Unique_t du;
    RPC2_Unsigned opcode;
    union {
	aclstore	acl;
	ststore		st;
	newstore        newst;
	create_rle	create;
	symlink_rle	slink;
	link_rle	link;
	mkdir_rle	mkdir;
	rm_rle		rm;
	rmdir_rle	rmdir;
	rename_rle	mv;
	setquota_rle	sq;
    } u;
    /* keep upto two names in a separate structure */
    char *name1;
    char *name2;
    int namesalloced;	// flag used to free space at destruction
  //public:
    rsle(ViceStoreId *, VnodeId, Unique_t, int op, int index =-1, int sno = -1);
    rsle();
    ~rsle();
    void init(int op ...);
    void init(int op, va_list ap);
    void CommitInRVM(Volume *, Vnode *); 
    void Abort(Volume *);	
    void InitFromRecleBuf(char **);
    void print();
    void print(FILE *);
    void print(int);
};

extern int ExtractVNTypeFromrsle(rsle *);
extern void ExtractChildFidFromrsle(rsle *, ViceFid *);
extern char *ExtractNameFromrsle(rsle *);
#endif _RSLE_H
