/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 * Routines for accessing volume abstractions in recoverable storage
 */


#ifndef _RECOV_H_
#define _RECOV_H_ 1

#include <volume.h>

#define HASHTABLESIZE	512	/* Number of buckets in volume hash table */

extern int coda_init();

extern int NewVolHeader(struct VolumeHeader *header, Error *err);
extern int DeleteVolume(Volume *vp);
extern int DeleteRvmVolume(unsigned int, Device);
extern int ExtractVolHeader(VolumeId volid, struct VolumeHeader *header);
extern int VolHeaderByIndex(int index, struct VolumeHeader *header);
extern void CheckVolData(Error *ec, int volindex);
extern void CheckSmallVnodeHeader(Error *ec, int volindex);
extern void CheckLargeVnodeHeader(Error *ec, int volindex);
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

#endif /* _RECOV_H_ */

