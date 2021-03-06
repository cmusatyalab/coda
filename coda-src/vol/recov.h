/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2021 Carnegie Mellon University
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
#include <coda_tsa.h>

#define HASHTABLESIZE 512 /* Number of buckets in volume hash table */

int coda_init() EXCLUDES_TRANSACTION;

int NewVolHeader(struct VolumeHeader *header, Error *err) REQUIRES_TRANSACTION;
int DeleteVolume(Volume *vp) EXCLUDES_TRANSACTION;
int DeleteRvmVolume(unsigned int, Device) EXCLUDES_TRANSACTION;
int ExtractVolHeader(VolumeId volid, struct VolumeHeader *header);
int VolHeaderByIndex(int index, struct VolumeHeader *header);
void CheckVolData(Error *ec, int volindex);
void CheckSmallVnodeHeader(Error *ec, int volindex);
void CheckLargeVnodeHeader(Error *ec, int volindex);
int ExtractVnode(int, int, VnodeId, Unique_t, VnodeDiskObject *);
int ReplaceVnode(int, int, VnodeId, Unique_t,
                 VnodeDiskObject *) REQUIRES_TRANSACTION;
void GrowVnodes(VolumeId volid, int vclass,
                unsigned short newsize) REQUIRES_TRANSACTION;
void NewVolDiskInfo(Error *ec, int volindex,
                    VolumeDiskData *vol) REQUIRES_TRANSACTION;
int VolDiskInfoById(Error *ec, VolumeId volid, VolumeDiskData *vol);
void ExtractVolDiskInfo(Error *ec, int volindex, VolumeDiskData *vol);
void ReplaceVolDiskInfo(Error *ec, int volindex,
                        VolumeDiskData *vol) REQUIRES_TRANSACTION;
VnodeDiskObject *FindVnode(rec_smolist *, Unique_t);
int ActiveVnodes(int volindex, int vclass);
int AllocatedVnodes(int volindex, int vclass);
int AvailVnode(int volindex, int vclass, VnodeId vnodeindex, Unique_t = 0);
int GetVolType(Error *ec, VolumeId volid);
void GetVolPartition(Error *, VolumeId, int, char partition[V_MAXPARTNAMELEN]);
void SetupVolCache();
VolumeId VAllocateVolumeId(Error *ec) REQUIRES_TRANSACTION;
VolumeId VGetMaxVolumeId();
void VSetMaxVolumeId(VolumeId newid) REQUIRES_TRANSACTION;

#endif /* _RECOV_H_ */
