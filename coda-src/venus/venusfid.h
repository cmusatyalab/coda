#ifndef _VENUSFID_H_
#define _VENUSFID_H_

#include <codadir.h>

typedef struct {
    RealmId  Realm;
    VolumeId Volume;
    VnodeId  Vnode;
    Unique_t Unique;
} VenusFid;

typedef struct {
    RealmId  Realm;
    VolumeId Volume;
} VolFid;

inline CodaFid *VenusToKernelFid(VenusFid *fid)
{
    return (CodaFid *)(&fid->Volume);
}

inline void KernelToVenusFid(VenusFid *fid, CodaFid *kfid)
{
    fid->Realm = 0;
    memcpy(&fid->Volume, kfid, sizeof(CodaFid));
}

inline ViceFid *MakeViceFid(VenusFid *fid)
{
    return (ViceFid *)(&fid->Volume);
}

inline VolFid *MakeVolFid(VenusFid *fid)
{
    return (VolFid *)(&fid->Realm);
}

inline int FID_EQ(const VenusFid *a, const VenusFid *b)
{
    return (a->Realm == b->Realm && a->Volume == b->Volume &&
	    a->Vnode == b->Vnode && a->Unique == b->Unique);
}

inline int FID_VolEQ(const VenusFid *a, const VenusFid *b)
{
	return (a->Realm == b->Realm && a->Volume == b->Volume);
}

inline int FID_VolIsLocal(VenusFid *fid)
{
    return FID_VolIsLocal(MakeViceFid(fid));
}

inline int FID_IsVolRoot(VenusFid *fid)
{
    return FID_IsVolRoot(MakeViceFid(fid));
}

inline char *FID_(const VenusFid *fid)
{
    static char buf[2][28];
    static int i = 0;
    i = 1 - i;
    sprintf(buf[i], "%x.%x.%x.%x",
	    fid->Realm, fid->Volume, fid->Vnode, fid->Unique);
    return buf[i];
}

inline void MakeVenusFid(VenusFid *vf, const u_int32_t realm, const ViceFid *fid)
{
    vf->Realm  = realm;
    vf->Volume = fid->Volume;
    vf->Vnode  = fid->Vnode;
    vf->Unique = fid->Unique;
}

#endif /* _VENUSFID_H_ */

