/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _OPERATIONS_H_
#define _OPERATIONS_H_

typedef int (*VCP)(int, VnodeType, void *, void *);

extern int ValidateParms(RPC2_Handle, ClientEntry **, int *ReplicatedOp,
			 VolumeId *, RPC2_CountedBS *, int *Nservers);
extern int AllocVnode(Vnode **, Volume *, ViceDataType,
		       ViceFid *, ViceFid *, UserId, RPC2_Unsigned, int *);
extern int CheckFetchSemantics(ClientEntry *, Vnode **, Vnode **,
				Volume **, Rights *, Rights *);
extern int CheckGetAttrSemantics(ClientEntry *, Vnode **, Vnode **,
				  Volume **, Rights *, Rights *);
extern int CheckGetACLSemantics(ClientEntry *, Vnode **, Volume **, Rights *,
				 Rights *, RPC2_BoundedBS *, RPC2_String *);
extern int CheckStoreSemantics(ClientEntry *, Vnode **, Vnode **, Volume **, int, VCP,
				ViceVersionVector *, FileVersion, Rights *, Rights *);
extern int CheckSetAttrSemantics(ClientEntry *, Vnode **, Vnode **, Volume **,
				 int, VCP, RPC2_Integer, Date_t, UserId,
				 RPC2_Unsigned, RPC2_Integer, ViceVersionVector
				 *, FileVersion,  Rights *, Rights *);
extern int CheckSetACLSemantics(ClientEntry *, Vnode **, Volume **, int, VCP,
				  ViceVersionVector *, FileVersion, Rights *, Rights *,
				  RPC2_CountedBS *, AL_AccessList **);
extern int CheckCreateSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, int, VCP,
				  ViceStatus *, ViceStatus *, Rights *, Rights *, int =1);
extern int CheckRemoveSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, int, VCP,
				  ViceStatus *, ViceStatus *, Rights *, Rights *, int =1);
extern int CheckLinkSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, int, VCP,
				ViceStatus *, ViceStatus *, Rights *, Rights *, int =1);
extern int CheckRenameSemantics(ClientEntry *, Vnode **, Vnode **, Vnode **, char *,
				Vnode **, char *, Volume **, int, VCP, ViceStatus *,
				ViceStatus *, ViceStatus *, ViceStatus *,
				Rights *, Rights *, Rights *, Rights *, Rights *, Rights *, 
				int =1, int =0, dlist * =NULL);
extern int CheckMkdirSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, int, VCP,
				 ViceStatus *, ViceStatus *, Rights *, Rights *, int =1);
extern int CheckRmdirSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, int, VCP,
				 ViceStatus *, ViceStatus *, Rights *, Rights *, int =1);
extern int CheckSymlinkSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **, int, VCP,
				   ViceStatus *, ViceStatus *, Rights *, Rights *, int =1);
extern void PerformFetch(ClientEntry *, Volume *, Vnode *);
extern int FetchBulkTransfer(RPC2_Handle, ClientEntry *, Volume *, Vnode *,
			     RPC2_Unsigned Offset, ViceVersionVector *VV);
extern void PerformGetAttr(ClientEntry *, Volume *, Vnode *);
extern void PerformGetACL(ClientEntry *, Volume *, Vnode *, RPC2_BoundedBS *, RPC2_String);
extern void PerformStore(ClientEntry *, VolumeId, Volume *, Vnode *,
			  Inode, int, RPC2_Integer, Date_t, ViceStoreId *, RPC2_Integer * =NULL);
extern int StoreBulkTransfer(RPC2_Handle, ClientEntry *, Volume *, Vnode *, Inode, RPC2_Integer);
extern void PerformSetAttr(ClientEntry *, VolumeId, Volume *, Vnode *, int,
			   RPC2_Integer, Date_t, UserId, RPC2_Unsigned,
			   RPC2_Integer, ViceStoreId *, Inode *,
			   RPC2_Integer * =NULL);
extern void PerformSetACL(ClientEntry *, VolumeId, Volume *,
			   Vnode *, int, ViceStoreId *, AL_AccessList *, RPC2_Integer * =NULL);
extern void PerformCreate(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *,
			   char *, Date_t, RPC2_Unsigned, int, ViceStoreId *, 
			  DirInode **, int *, RPC2_Integer * =NULL);
extern void PerformRemove(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *,
			   char *, Date_t, int, ViceStoreId *, DirInode **, int *, 
			   RPC2_Integer * =NULL);
extern void PerformLink(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *,
			 char *, Date_t, int, ViceStoreId *, DirInode **, int *, 
			 RPC2_Integer * =NULL);
extern void PerformRename(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *,
			   Vnode *, Vnode *, char *, char *, Date_t, int, ViceStoreId *,
			   DirInode **, DirInode **, DirInode **, int * = NULL,
			   RPC2_Integer * =NULL);
extern void PerformMkdir(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *,
			  char *, Date_t, RPC2_Unsigned, int, ViceStoreId *, 
			 DirInode **, int *, RPC2_Integer * =NULL);
extern void PerformRmdir(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *,
			  char *, Date_t, int, ViceStoreId *, DirInode **, int *, 
			  RPC2_Integer * = NULL);
extern void PerformSymlink(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *,
			    char *, Inode, RPC2_Unsigned, Date_t, RPC2_Unsigned,
			    int, ViceStoreId *, DirInode **, int *, 
			    RPC2_Integer * =NULL);
extern void PerformSetQuota(ClientEntry *, VolumeId, Volume *, Vnode *, 
			    ViceFid *, int, int, ViceStoreId *);

extern void PutObjects(int, Volume *, int, dlist *, int, int, int =0);

extern void SpoolRenameLogRecord(int, vle *, vle *, vle *, vle *, Volume *, char *, 
				 char *, ViceStoreId *);
    
#endif _OPERATIONS_H_
