/*
 *	Super-block structure for user-level filesystem support.
 */

#define CODA_SUPER_MAGIC	0x73757245


#include "cfs.h"

struct coda_sb_info
{
  struct inode *      s_psdev;	/* /dev/cfsN Venus/kernel device */
  struct inode *      s_ctlcp;	/* control magic file */
  int                 mi_refct;
  struct vcomm        mi_vcomm;
  struct inode *      mi_rootvp;
};

#define coda_sb(sb)   (*((struct coda_sb_info *)((sb)->u.generic_sbp)))
#define coda_sbp(sb)  ((struct coda_sb_info *)((sb)->u.generic_sbp))
#define vtomi(vp)     (coda_sb((vp)->i_sb).cfs_mntinfo)



