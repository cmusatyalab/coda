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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/rvmres/recle.h,v 1.1.1.1 1996/11/22 19:13:27 rvb Exp";
#endif /*_BLURB_*/





#ifndef _RECLE_H
#define _RECLE_H 1
#include <rec_dlist.h>
#include "recvarl.h"

#define DUMP_ENTRY_BEGIN_STAMP 0xfb32ea84
#define DUMP_ENTRY_END_STAMP 0x989fd4ae

class rsle;

class recle : public rec_dlink {	/* Recoverable resolution Log Entry - fixed length part */
//friend class rsle;
  public:
    unsigned long	serverid;
    ViceStoreId 	storeid;
    RPC2_Unsigned 	opcode;
    VnodeId		dvnode;
    Unique_t		dunique;
    unsigned		size:16;	/* size of variable part - to simplify
					   calculating directory log size */
    recvarl		*vle;		/* pointer to variable length part */

    /* varl v; 				variable length class varl goes here */
    
    int GetDumpSize(); 			// size of buffer for dumping entry
  // public:		
    unsigned long	index;		/* index of entry in the log */
    int			seqno;		/* monotonically increasing 
					   number for log records */
    recle();
    ~recle();
    int FreeVarl();			/* free variable length part */
    rec_dlist *HasList();		/* returns head of list of removed child's log */
    void InitFromsle(rsle *);		/* from another already initialized rsle */
    char *DumpToBuf(int*);
    void print();
    void print(FILE *);
    void print(int);
};

/* definition of the variable parts of each record */
/* there are no constructors for these classes because
   they are all members of a union */
#define	STSTORE	1
#define	ACLSTORE 2
class aclstore {
  public:
    int		 	type;
    char		acl[SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE];

    void init(char *a);
    void print(int);
};

class ststore {
  public:
    int 	 	type;
    UserId		owner;
    RPC2_Unsigned	mode;
    UserId		author;
    Date_t		mtime;
    ViceVersionVector	vv;	// at time of setattr

    void init(UserId, RPC2_Unsigned, UserId, Date_t, ViceVersionVector *);
    void print(int);
};

class newstore {
  public:
    int                 type;
    UserId		owner;
    RPC2_Unsigned	mode;
    UserId		author;
    Date_t		mtime;
    RPC2_Integer        mask;
    ViceVersionVector	vv;	// at time of setattr

    void init(UserId, RPC2_Unsigned, UserId, Date_t, RPC2_Integer, ViceVersionVector *);
    void print(int);
};

class create_rle {
  public:
    VnodeId 	cvnode;
    Unique_t 	cunique;
    UserId	owner;
    char    	name[1];		/* begining of null terminated name 
					   of child created */
    void init(VnodeId, Unique_t, UserId, char *);
    void print(int);
};

class symlink_rle {
  public:
    VnodeId 	cvnode; 
    Unique_t 	cunique;
    UserId	owner;
    char	name[1];	/* begining of null terminated name 
				   of child created */
    void init(VnodeId, Unique_t, UserId, char *);
    void print(int);
};

class link_rle {
  public:
    VnodeId 	cvnode;
    Unique_t 	cunique;
    ViceVersionVector	cvv;
    char	name[1];	/* begining of null terminated name 
				   of child created */
    void init(VnodeId, Unique_t, ViceVersionVector *, char *);
    void print(int);
};

class mkdir_rle {
  public:
    VnodeId 	cvnode;
    Unique_t 	cunique;
    UserId	owner;
    char	name[1];	/* begining of null terminated name 
				   of child created */
    void init(VnodeId, Unique_t, UserId, char *);
    void print(int);
};

class rm_rle {
  public:
    VnodeId	cvnode;
    Unique_t	cunique;
    ViceVersionVector cvv; 	/* version vector for child when deleted */
    char	name[1];	/* beginning of null terminated name 
				   of child removed */
    void init(VnodeId v, Unique_t u, ViceVersionVector *vv, char *s);
    void print(int);
};

class rmdir_rle {
  public:
    VnodeId 	cvnode;
    Unique_t 	cunique;
    rec_dlist	*childlist;	/* deleted child's log */
    ViceStoreId childLCP; 	/* childs lcp in log with other reps */
    ViceStoreId	csid;		/* storeid of directory when deleted */
    char	name[1];

    void init(VnodeId, Unique_t, rec_dlist *, ViceStoreId *lcp, 
	      ViceStoreId *, char *);
    void print(int);
};

#define	SOURCE	0	/* source or target log record */
#define TARGET	1	

class rename_rle {
  public:
    unsigned short	type;	/* is this directory SOURCE or TARGET */
    VnodeId 	otherdirv;	/* src/tgt dir spec */
    Unique_t 	otherdiru;
    VnodeId	svnode;
    Unique_t	sunique;
    ViceVersionVector	svv;	/* source's version vector when renamed */
    VnodeId	tvnode;		/* target objects id: 0 if didn't exist */
    Unique_t	tunique;
    ViceVersionVector	tvv;	/* target's vv when deleted */
    rec_dlist *tlist;		/* target's log if deleted directory */
    unsigned short newname_offset; /* newname starts this many bytes away */
    char	oldname[1];	/* name of child before rename */
//    char	newname[1];	/* name of child after rename - 
				/* gets appended to oldname field */

    void init(unsigned short, VnodeId, Unique_t, VnodeId, Unique_t ,
	      ViceVersionVector *, char *old, char *, 
	      VnodeId tv =0, Unique_t tu =0, ViceVersionVector *tgtvv =NULL, 
	      rec_dlist *list =NULL);
    void print(int);
};


class setquota_rle {
  public:
    int    oldquota;
    int    newquota;

    void init(int, int);
    void print(int);
};

#endif _RECLE_H


