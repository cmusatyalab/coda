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

#endif /*_BLURB_*/

#include <sys/types.h>
#include <stdio.h>
#include <codadir.h>
#include <util.h>


#include <srv.h>
#include <dllist.h>
#include "objlist.h"

/* XXX map  vle     to   struct obj  */

int OBJ_Cmp(struct fsobj *a, struct fsobj *b) 
{
	assert(FID_VolEQ(&a->fid, &b->fid));
	return FID_Cmp(&a->fid, &b-fid);
}

struct objlist *OBJ_NewList()
{
	struct objlist *l;
	l = (struct objlist *) malloc(sizeof(*l));
	assert(l);
	INIT_LIST_HEAD(&l->objl_lh);

	return l;
}

void OBJ_FreeList(struct objlist *list)
{
	free(l);
}

struct  fsobj *OBJ_Find(struct fsobjlist *objlist, ViceFid *fid) 
{
	struct list_head *lh = &objlist->lh;
	struct list_chain *tmp = lh->next;
	struct fsobj *obj;

	while (tmp != lh) {
		obj = list_entry(tmp, struct fsobj, obj_chain);
		if (FID_EQ(&obj->fid, fid)) 
			return(obj);
		tmp = tmp->next;
	}
	return NULL;
}


struct fsobj *OBJ_GetFree(ViceFid *fid)
{
	struct fsobj *obj;
	
	obj = (struct fsobj *) malloc(sizeof(*obj));
	assert(obj);

	INIT_LIST_HEAD(obj->obj_chain);
	obj->obj_fid = *Fid;
	obj->obj_vptr = 0;

	if (!ISDIR(fid)) {
		obj->f_sid = NullSid;
		obj->f_sinode = 0;
		obj->f_finode = 0;
		obj->f_tinode = 0;
		obj->f_tlength = 0;
	} else {
		obj->d_cinode = 0;
		obj->d_inodemod = 0;
		obj->d_needsres = 0;
		obj->d_needslogpurge = 0;
		obj->d_needslogtrunc = 0;
		obj->d_reintupdate = 0;
		obj->d_reintstale = 0;
	}
}

void OBJ_Free(struct fsobject *obj) 
{
	assert(list_empty(&obj->obj_chain));
	assert(obj->obj_vptr == 0);
	free(obj);
};


struct fsobj *OBJ_Add(struct fsobjlist *objlist, ViceFid *fid) 
{
	struct list_head *lh = &objlist->lh;
	struct fsobj *obj;


	obj = OBJ_Find(objlist, fid);
	if (obj == 0) {
		obj = OBJ_GetFree();
		list_add(&objlist->objl_lh, &obj->obj_obj_chain);
	}
	return(obj);
}


#endif	not _VICE_OBJLIST_H_
