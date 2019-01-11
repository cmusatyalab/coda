/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _VENUSFID_H_
#define _VENUSFID_H_

#include <codadir.h>

typedef struct {
    RealmId Realm;
    VolumeId Volume;
    VnodeId Vnode;
    Unique_t Unique;
} VenusFid;

typedef struct {
    RealmId Realm;
    VolumeId Volume;
} Volid;

inline CodaFid *VenusToKernelFid(VenusFid *fid)
{
    return (CodaFid *)fid;
}

inline void KernelToVenusFid(VenusFid *fid, CodaFid *kfid)
{
    *fid = *(VenusFid *)kfid;
}

inline ViceFid *MakeViceFid(VenusFid *fid)
{
    return (ViceFid *)(&fid->Volume);
}

inline Volid *MakeVolid(VenusFid *fid)
{
    return (Volid *)(&fid->Realm);
}

inline int FID_EQ(const VenusFid *a, const VenusFid *b)
{
    return (a->Realm == b->Realm && a->Volume == b->Volume &&
            a->Vnode == b->Vnode && a->Unique == b->Unique);
}

inline int FID_VolEQ(const Volid *a, const Volid *b)
{
    return (a->Realm == b->Realm && a->Volume == b->Volume);
}

inline int FID_VolEQ(const VenusFid *a, const VenusFid *b)
{
    return FID_VolEQ((Volid *)a, (Volid *)b);
}

inline int FID_IsVolRoot(VenusFid *fid)
{
    return FID_IsVolRoot(MakeViceFid(fid));
}

inline char *FID_(const VenusFid *fid)
{
    static char buf[2][37];
    static int i = 0;
    i            = 1 - i;
    sprintf(buf[i], "%x.%x.%x.%x", (unsigned int)fid->Realm,
            (unsigned int)fid->Volume, (unsigned int)fid->Vnode,
            (unsigned int)fid->Unique);
    return buf[i];
}

inline void MakeVenusFid(VenusFid *vf, const uint32_t realm, const ViceFid *fid)
{
    vf->Realm  = realm;
    vf->Volume = fid->Volume;
    vf->Vnode  = fid->Vnode;
    vf->Unique = fid->Unique;
}

#define FakeRootVolumeId ((VolumeId)0xff000001)
#define FakeRepairVolumeId ((VolumeId)0xffffffff)

int FID_IsLocalFake(VenusFid *fid);

inline int FID_IsExpandedDir(ViceFid *vf)
{
    return (vf->Volume == FakeRepairVolumeId) && FID_IsFakeRoot(vf);
}

inline int FID_IsExpandedDir(VenusFid *vf)
{
    return FID_IsLocalFake(vf) && FID_IsExpandedDir(MakeViceFid(vf));
}

#endif /* _VENUSFID_H_ */
