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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vol/recov.h,v 4.1 1997/01/08 21:52:12 rvb Exp $";
#endif /*_BLURB_*/








/*
 * Routines for accessing volume abstractions in recoverable storage
 */


#ifndef _RECOV_H_
#define _RECOV_H_ 1

#define HASHTABLESIZE	512	/* Number of buckets in volume hash table */

extern int coda_init();

extern int NewVolHeader(struct VolumeHeader *header, Error *err);
extern int DeleteVolume(Volume *vp);
extern int DeleteRvmVolume(unsigned int, Device);
extern int ExtractVolHeader(VolumeId volid, struct VolumeHeader *header);
extern int VolHeaderByIndex(int index, struct VolumeHeader *header);
extern void CheckVolData(Error *ec, int volindex);
extern void CheckSmallVnodeHeader(Error *ec, int volindex);
/*extern void ExtractSmallVnodeList(Error *ec, int volindex, VnodeDiskObject ***vlist, int *elts);*/
extern void CheckLargeVnodeHeader(Error *ec, int volindex);
/*extern void ExtractLargeVnodeList(Error *ec, int volindex, VnodeDiskObject ***vlist, int *elts);*/
extern int ExtractVnode(Error *, int, int, VnodeId, Unique_t, 
			 VnodeDiskObject *);
extern int ReplaceVnode(int, int, VnodeId, Unique_t, VnodeDiskObject *);
extern void GrowVnodes(VolumeId volid, int vclass, short newsize);
extern void NewVolDiskInfo(Error *ec, int volindex, VolumeDiskData *vol);
extern int VolDiskInfoById(Error *ec, VolumeId volid, VolumeDiskData *vol);
extern void ExtractVolDiskInfo(Error *ec, int volindex, VolumeDiskData *vol);
extern void ReplaceVolDiskInfo(Error *ec, int volindex, VolumeDiskData *vol);
extern VnodeDiskObject *FindVnode(rec_smolist *, Unique_t);
extern int ActiveVnodes(int volindex, int vclass);
extern int AllocatedVnodes(int volindex, int vclass);
extern int AvailVnode(int volindex, int vclass, VnodeId vnodeindex, Unique_t =0);
extern void InitVV(vv_t *vv);
extern int GetVolType(Error *ec, VolumeId volid);
extern void GetVolPartition(Error *, VolumeId, int, char *);
extern void SetupVolCache();
extern VolumeId VAllocateVolumeId(Error *ec);
extern VolumeId VGetMaxVolumeId();
extern void VSetMaxVolumeId(VolumeId newid);
#endif _RECOV_H_

