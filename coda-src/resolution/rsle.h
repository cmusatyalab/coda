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
