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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/res/Attic/resforce.h,v 4.1 1997/01/08 21:50:03 rvb Exp $";
#endif /*_BLURB_*/






#ifndef _RES_FORCE_H
#define _RES_FORCE_H 1
#include "olist.h"
typedef enum {CreateD=0, CreateF=1, CreateS=2, CreateL=3} dirop_t;

#define DIROPNAMESIZE	255
 
class diroplink : public olink {
  public:
    dirop_t	op;
    long	vnode;
    long	unique;
    char	name[DIROPNAMESIZE];

    diroplink(dirop_t, long, long, char *);
    ~diroplink();
    void hton();
    void ntoh();
    int write(int);
};

/* 
class commitlink : public olink {
  public:
    ViceFid	Fid;
    Vnode 	*vptr;

    commitlink(ViceFid *, Vnode *);
    ~commitlink();
};
*/

/* getdiropParm - struct to pack all the arguments to the enumerate dir proc*/
struct  getdiropParm {
    Volume	*volptr;
    olist	*oplist;
}; 

extern void UpdateRunts(res_mgrpent *, ViceVersionVector **, ViceFid *);
extern int RuntExists(ViceVersionVector **, int, int *, int *);
#endif _RES_FORCE_H




