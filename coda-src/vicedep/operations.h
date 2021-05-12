/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2021 Carnegie Mellon University
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

int ValidateParms(RPC2_Handle, ClientEntry **, int *ReplicatedOp, VolumeId *,
                  RPC2_CountedBS *, int *Nservers);
int AllocVnode(Vnode **, Volume *, ViceDataType, ViceFid *, ViceFid *, UserId,
               int *) EXCLUDES_TRANSACTION;
int CheckFetchSemantics(ClientEntry *, Vnode **, Vnode **, Volume **, Rights *,
                        Rights *);
int CheckGetAttrSemantics(ClientEntry *, Vnode **, Vnode **, Volume **,
                          Rights *, Rights *);
int CheckGetACLSemantics(ClientEntry *, Vnode **, Volume **, Rights *, Rights *,
                         RPC2_BoundedBS *, RPC2_String *);
int CheckStoreSemantics(ClientEntry *, Vnode **, Vnode **, Volume **, int, VCP,
                        ViceVersionVector *, FileVersion, Rights *, Rights *);
int CheckSetAttrSemantics(ClientEntry *, Vnode **, Vnode **, Volume **, int,
                          VCP, RPC2_Integer, Date_t, UserId, RPC2_Unsigned,
                          RPC2_Integer, ViceVersionVector *, FileVersion,
                          Rights *, Rights *);
int CheckSetACLSemantics(ClientEntry *, Vnode **, Volume **, int, VCP,
                         ViceVersionVector *, FileVersion, Rights *, Rights *,
                         RPC2_CountedBS *, AL_AccessList **);
int CheckCreateSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                         int, VCP, void *, void *, Rights *, Rights *, int = 1);
int CheckRemoveSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                         int, VCP, void *, void *, Rights *, Rights *, int = 1);
int CheckLinkSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                       int, VCP, void *, void *, Rights *, Rights *, int = 1);
int CheckRenameSemantics(ClientEntry *, Vnode **, Vnode **, Vnode **, char *,
                         Vnode **, char *, Volume **, int, VCP, void *, void *,
                         void *, void *, Rights *, Rights *, Rights *, Rights *,
                         Rights *, Rights *, int = 1, int = 0,
                         dlist * = NULL) EXCLUDES_TRANSACTION;
int CheckMkdirSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                        int, VCP, void *, void *, Rights *, Rights *, int = 1);
int CheckRmdirSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                        int, VCP, void *, void *, Rights *, Rights *, int = 1);
int CheckSymlinkSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                          int, VCP, void *, void *, Rights *, Rights *,
                          int = 1);
void PerformFetch(ClientEntry *, Volume *, Vnode *);
int FetchBulkTransfer(RPC2_Handle, ClientEntry *, Volume *, Vnode *,
                      RPC2_Unsigned Offset, RPC2_Integer Count,
                      ViceVersionVector *VV);
void PerformGetAttr(ClientEntry *, Volume *, Vnode *);
void PerformGetACL(ClientEntry *, Volume *, Vnode *, RPC2_BoundedBS *,
                   RPC2_String);
void PerformStore(ClientEntry *, VolumeId, Volume *, Vnode *, Inode, int,
                  RPC2_Integer, Date_t, ViceStoreId *, RPC2_Integer * = NULL);
int StoreBulkTransfer(RPC2_Handle, ClientEntry *, Volume *, Vnode *, Inode,
                      RPC2_Integer);
void PerformSetAttr(ClientEntry *, VolumeId, Volume *, Vnode *, int,
                    RPC2_Integer, Date_t, UserId, RPC2_Unsigned, RPC2_Integer,
                    ViceStoreId *, Inode *, RPC2_Integer * = NULL);
void PerformSetACL(ClientEntry *, VolumeId, Volume *, Vnode *, int,
                   ViceStoreId *, AL_AccessList *, RPC2_Integer * = NULL);
int PerformCreate(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, char *,
                  Date_t, RPC2_Unsigned, int, ViceStoreId *, DirInode **, int *,
                  RPC2_Integer * = NULL);
void PerformRemove(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, char *,
                   Date_t, int, ViceStoreId *, DirInode **, int *,
                   RPC2_Integer * = NULL);
int PerformLink(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, char *,
                Date_t, int, ViceStoreId *, DirInode **, int *,
                RPC2_Integer * = NULL);
void PerformRename(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, Vnode *,
                   Vnode *, char *, char *, Date_t, int, ViceStoreId *,
                   DirInode **, DirInode **, DirInode **, int * = NULL,
                   RPC2_Integer * = NULL);
int PerformMkdir(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, char *,
                 Date_t, RPC2_Unsigned, int, ViceStoreId *, DirInode **, int *,
                 RPC2_Integer * = NULL);
void PerformRmdir(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, char *,
                  Date_t, int, ViceStoreId *, DirInode **, int *,
                  RPC2_Integer * = NULL);
int PerformSymlink(ClientEntry *, VolumeId, Volume *, Vnode *, Vnode *, char *,
                   Inode, RPC2_Unsigned, Date_t, RPC2_Unsigned, int,
                   ViceStoreId *, DirInode **, int *, RPC2_Integer * = NULL);
void PerformSetQuota(ClientEntry *, VolumeId, Volume *, Vnode *, ViceFid *, int,
                     int, ViceStoreId *);

void PutObjects(int, Volume *, int, dlist *, int, int,
                int = 0) EXCLUDES_TRANSACTION;

void SpoolRenameLogRecord(int, vle *, vle *, vle *, vle *, Volume *, char *,
                          char *, ViceStoreId *);

#endif /* _OPERATIONS_H_ */
