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


