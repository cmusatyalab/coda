#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>

#include <ufs/ifs/ifs.h>

/* get the mount structure corresponding to a given device.  Assume 
 * device corresponds to a UFS. Return NULL if no device is found.
 */ 
struct mount *devtomp(dev)
    dev_t dev;
{
    struct mount *mp, *nmp;
    
    for (mp = mountlist.cqh_first; mp != (void*)&mountlist; mp = nmp) {
	nmp = mp->mnt_list.cqe_next;
	if (
#ifdef	__NetBSD__
	    (!strcmp(mp->mnt_op->vfs_name, MOUNT_UFS)) &&
#endif
	    ((VFSTOUFS(mp))->um_dev == (dev_t) dev)) {
	    /* mount corresponds to UFS and the device matches one we want */
	    return(mp); 
	}
    }
    /* mount structure wasn't found */ 
    return(NULL); 
}
