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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <fcntl.h>


#include "coda_assert.h"
#include <cfs/coda.h>
#include "codadir.h"
#include "util.h"

/*
 * Public FID routines: to be taken elsewhere.
 */

void FID_PrintFid(struct DirFid *fid)
{
	printf("vnode: %ld, unique %ld\n", fid->df_vnode, fid->df_unique);
	return;
}

void FID_CpyVol(struct ViceFid *target, struct ViceFid *source)
{
	CODA_ASSERT(target && source);
	target->Volume = source->Volume;
}


void FID_Int2DFid(struct DirFid *fid, int vnode, int unique)
{
	CODA_ASSERT(fid);

	fid->df_vnode = vnode;
	fid->df_unique = unique;
	return;
}

void FID_NFid2Int(struct DirNFid *fid, VnodeId *vnode, Unique_t *unique)
{
	*vnode = ntohl(fid->dnf_vnode);
	*unique = ntohl(fid->dnf_unique);
	return;
}

void FID_VFid2DFid(struct ViceFid *vf, struct DirFid *df)
{
	CODA_ASSERT( vf && df );
	df->df_vnode = vf->Vnode;
	df->df_unique = vf->Unique;

}

void FID_DFid2VFid(struct DirFid *df, struct ViceFid *vf)
{
	CODA_ASSERT( vf && df );
	vf->Vnode = df->df_vnode;
	vf->Unique = df->df_unique;
}

char *FID_(struct ViceFid *vf)
{
	static char str1[50];
	snprintf(str1, 50, "(0x%lx.0x%lx.0x%lx)", 
		 vf->Volume, vf->Vnode, vf->Unique);
	return str1;
}

char *FID_2(struct ViceFid *vf)
{
	static char str1[50];
	snprintf(str1, 50, "(0x%lx.0x%lx.0x%lx)", 
		 vf->Volume, vf->Vnode, vf->Unique);
	return str1;
}


int FID_Cmp(struct ViceFid *fa, struct ViceFid *fb) 
{
	if ((fa->Volume) < (fb->Volume)) 
		return(-1);
	if ((fa->Volume) > (fb->Volume)) 
		return(1);
	if ((fa->Vnode) < (fb->Vnode)) 
		return(-1);
	if ((fa->Vnode) > (fb->Vnode)) 
		return(1);
	if ((fa->Unique) < (fb->Unique)) 
		return(-1);
	if ((fa->Unique) > (fb->Unique)) 
		return(1);
	return(0);
}

int FID_EQ(struct ViceFid *fa, struct ViceFid *fb)
{
	if  (fa->Volume != fb->Volume) 
		return 0;
	if  (fa->Vnode != fb->Vnode) 
		return 0;
	if  (fa->Unique != fb->Unique) 
		return 0;
	return 1;
}

int FID_VolEQ(struct ViceFid *fa, struct ViceFid *fb)
{
	if  (fa->Volume != fb->Volume) 
		return 0;
	return 1;
}

/* to determine if the volume is the local copy during a repair/conflict */
static VolumeId LocalFakeVid = 0xffffffff;
inline int  FID_VolIsLocal(struct ViceFid *x) 
{
	return (x->Volume == LocalFakeVid);
}

inline void FID_MakeVolFake(VolumeId *id)
{
	*id = LocalFakeVid;
}

inline int FID_VolIsFake(VolumeId id)
{
	return(id == LocalFakeVid);
}



/* was this fid created during a disconnection */
static VnodeId LocalFileVnode = 0xfffffffe;
static VnodeId LocalDirVnode  = 0xffffffff;
inline int FID_IsDisco(struct ViceFid *x)
{
  return  ( (x->Vnode == LocalFileVnode) || (x->Vnode == LocalDirVnode));
}

inline int FID_IsLocalDir(struct ViceFid *fid)
{
	return fid->Vnode == LocalDirVnode;
}

inline int FID_IsLocalFile(struct ViceFid *fid)
{
	return fid->Vnode == LocalFileVnode;
}

inline void FID_MakeDiscoFile(struct ViceFid *fid, VolumeId vid, 
			      Unique_t unique)
{
	fid->Volume = vid;
	fid->Vnode = LocalFileVnode;
	fid->Unique = unique;

}

inline void FID_MakeDiscoDir(struct ViceFid *fid, VolumeId vid,
			     Unique_t unique)
{
	fid->Volume = vid;
	fid->Vnode = LocalDirVnode;
	fid->Unique = unique;

}

static VnodeId FakeVnode = 0xfffffffc;

inline void FID_MakeSubtreeRoot(struct ViceFid *fid, VolumeId vid, 
				Unique_t unique)
{
	fid->Volume = vid;
	fid->Vnode = FakeVnode;
	fid->Unique = unique;

}
				

/* Local stuff is for the repair tree arising from client copies */

inline void FID_MakeLocalDir(struct ViceFid *fid, Unique_t unique)
{
	fid->Volume = LocalFakeVid;
	fid->Vnode = LocalDirVnode;
	fid->Unique = unique;

}



inline void FID_MakeLocalFile(struct ViceFid *fid, Unique_t unique)
{
	fid->Volume = LocalFakeVid;
	fid->Vnode = LocalFileVnode;
	fid->Unique = unique;

}


/* directory vnode number for dangling links during conflicts:
   top of the local subtree expanded by repair.*/
inline int FID_IsFakeRoot(struct ViceFid *fid) 
{
	return ((fid)->Vnode == FakeVnode);
}

inline void FID_MakeLocalSubtreeRoot(struct ViceFid *fid, Unique_t unique)
{
	fid->Volume = LocalFakeVid;
	fid->Vnode = FakeVnode;
	fid->Unique = unique;

}


/* Roots of volumes */
static VnodeId ROOT_VNODE = 1;
static Unique_t ROOT_UNIQUE = 1;

inline void FID_MakeRoot(struct ViceFid *fid)
{
	fid->Vnode = ROOT_VNODE;
	fid->Unique = ROOT_UNIQUE;
}

inline int FID_IsVolRoot(struct ViceFid *fid)
{
	return ((fid->Vnode == ROOT_VNODE) && (fid->Unique == ROOT_UNIQUE));

}
