/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2025 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 * resutil.h
 */
#ifndef _RESUTIL_H_
#define _RESUTIL_H_ 1

#include <res.h>

/*
 * class he:
 * Elements of a linked list.
 * One element / host in the log buffer.
 * Each element has a list of vnode-res log headers(rmtle)
 */

struct he : public olink {
    olist vlist;
    long hid;

    he(long id) { hid = id; }
};
he *FindHE(olist *list, long hostaddress);

#define DIROPNAMESIZE 256

/* define opcodes for log records spooling */
/* The weird numbering is for compatibility with pre-coda-5.2 servers */
#define RES_Remove_OP 4
#define RES_Create_OP 5
#define RES_Rename_OP 6
#define RES_SymLink_OP 7
#define RES_Link_OP 8
#define RES_MakeDir_OP 9
#define RES_RemoveDir_OP 10
#define RES_SetVolumeStatus_OP 18
#define RES_Repair_OP 33
#define RES_NewStore_OP 52

#define RESOLVE_OPERATION_BASE 128
#define ResolveStoreAcl_OP RESOLVE_OPERATION_BASE + 2
#define ResolveViceRemove_OP RESOLVE_OPERATION_BASE + 3
#define ResolveViceCreate_OP RESOLVE_OPERATION_BASE + 4
#define ResolveViceRename_OP RESOLVE_OPERATION_BASE + 5
#define ResolveViceSymLink_OP RESOLVE_OPERATION_BASE + 6
#define ResolveViceLink_OP RESOLVE_OPERATION_BASE + 7
#define ResolveViceMakeDir_OP RESOLVE_OPERATION_BASE + 8
#define ResolveViceRemoveDir_OP RESOLVE_OPERATION_BASE + 9
#define ResolveNULL_OP RESOLVE_OPERATION_BASE + 10
#define ResolveViceSetVolumeStatus_OP RESOLVE_OPERATION_BASE + 11

#define ResolveViceNewStore_OP RESOLVE_OPERATION_BASE + 12

#define ResolveAfterCrash_OP RESOLVE_OPERATION_BASE - 1

#define PRINTOPCODE(op)                                                        \
    ((op) == RES_Create_OP                 ? "Create" :                        \
     (op) == ResolveViceCreate_OP          ? "ResolveCreate" :                 \
     (op) == RES_Remove_OP                 ? "Remove" :                        \
     (op) == ResolveViceRemove_OP          ? "ResolveRemove" :                 \
     (op) == RES_Link_OP                   ? "Link" :                          \
     (op) == ResolveViceLink_OP            ? "ResolveLink" :                   \
     (op) == RES_Rename_OP                 ? "Rename" :                        \
     (op) == ResolveViceRename_OP          ? "ResolveRename" :                 \
     (op) == RES_MakeDir_OP                ? "Mkdir" :                         \
     (op) == ResolveViceMakeDir_OP         ? "ResolveMkdir" :                  \
     (op) == RES_RemoveDir_OP              ? "Rmdir" :                         \
     (op) == ResolveViceRemoveDir_OP       ? "ResolveRmdir" :                  \
     (op) == RES_SymLink_OP                ? "Symlink" :                       \
     (op) == ResolveViceSymLink_OP         ? "ResolveSymlink" :                \
     (op) == ResolveNULL_OP                ? "ResolveNULL_OP" :                \
     (op) == RES_Repair_OP                 ? "Repair_OP" :                     \
     (op) == ResolveViceSetVolumeStatus_OP ? "ResolveViceSetVolumeStatus_OP" : \
     (op) == RES_SetVolumeStatus_OP        ? "SetVolumeStatus_OP" :            \
     (op) == RES_NewStore_OP               ? "NewStore" :                      \
     (op) == ResolveViceNewStore_OP        ? "ResolveNewStore" :               \
                                             "???")

#define ISNONRESOLVEOP(a) \
    (((a) < RESOLVE_OPERATION_BASE) || ((a) == ResolveNULL_OP))
#define FormFid(fid, vol, vn, un) \
    {                             \
        (fid).Volume = (vol);     \
        (fid).Vnode  = (vn);      \
        (fid).Unique = (un);      \
    }

// for validating a resolution operation
#define PERFORMOP 0
#define NULLOP 1
#define MARKPARENTINC 2
#define MARKOBJINC 3
#define CREATEINCOBJ 4

/* defined by Kudo in rp2gen - this is the max size of an array that can be
 * passed as arg in a rpc call */
#define MAXCOMPS 100

/**************** ilink class functions *******************/
/* declaration of functions that parse the list of inconsistencies
 * at end of phase 1 during resolution and create a unique entry list
 */
#define SIZEOF_INCFID 5 * sizeof(long)

#include "dlist.h"

struct ilink : public dlink {
    char *name;
    long vnode;
    long unique;
    long pvnode;
    long punique;
    long type;

    ilink(const char *c, long vn, long unq, long pvn, long punq, long t)
    {
        name    = strdup(c);
        vnode   = vn;
        unique  = unq;
        pvnode  = pvn;
        punique = punq;
        type    = t;
    }
    ~ilink()
    {
        if (name)
            free(name);
    }
};

void GetResStatus(unsigned long *, ResStatus **, ViceStatus *);
void AllocStoreId(ViceStoreId *);
long CheckRetCodes(RPC2_Integer *, unsigned long *, unsigned long *);
long CheckResRetCodes(RPC2_Integer *, unsigned long *, unsigned long *);
void BSToDlist(RPC2_BoundedBS *, dlist *);
void DlistToBS(dlist *, RPC2_BoundedBS *);
void ParseIncBSEntry(char **, char **, long *, long *, long *, long *, long *);
void AllocIncBSEntry(RPC2_BoundedBS *, char *, ViceFid *, ViceFid *, long);
int CompareIlinkEntry(ilink *, ilink *);
ilink *AddILE(dlist &, const char *, long, long, long, long, long);
void CleanIncList(dlist *);
void MarkObjInc(ViceFid *, Vnode *);
int CreateObjToMarkInc(Volume *, ViceFid *, ViceFid *, char *, int, dlist *,
                       int *) EXCLUDES_TRANSACTION;
void ObtainResStatus(ResStatus *, VnodeDiskObjectStruct *);

int GetPhase2Objects(ViceFid *, dlist *, dlist *,
                     Volume **) EXCLUDES_TRANSACTION;
int CreateResPhase2Objects(ViceFid *, dlist *, dlist *, Volume *, VolumeId,
                           int *) EXCLUDES_TRANSACTION;
void GetRemoteRemoveStoreId(ViceStoreId *, olist *, unsigned long, ViceFid *,
                            ViceFid *, char *);
ViceStoreId *GetRemoteRemoveStoreId(olist *, unsigned long, ViceFid *,
                                    ViceFid *, char *);
int GetNameInParent(Vnode *, dlist *, Volume *, char *) EXCLUDES_TRANSACTION;

void *Dir_n_ACL(struct Vnode *vn, int *size);

/* declarations from rescoord.c */
class res_mgrpent;
long OldDirResolve(res_mgrpent *, ViceFid *, ViceVersionVector **);

/* from resfile.c */
long FileResolve(res_mgrpent *, ViceFid *,
                 ViceVersionVector **) EXCLUDES_TRANSACTION;

#endif /* _RESUTIL_H_ */
