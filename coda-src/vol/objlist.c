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
	CODA_ASSERT(FID_VolEQ(&a->fid, &b->fid));
	return FID_Cmp(&a->fid, &b-fid);
}

struct objlist *OBJ_NewList()
{
	struct objlist *l;
	l = (struct objlist *) malloc(sizeof(*l));
	CODA_ASSERT(l);
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
	CODA_ASSERT(obj);

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
	CODA_ASSERT(list_empty(&obj->obj_chain));
	CODA_ASSERT(obj->obj_vptr == 0);
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
