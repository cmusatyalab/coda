
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <sys/conf.h>


/* 
 * Using the reserved device number for local use as a free slot. 
 * (see /usr/src/sys/i386/i386/conf.c)
 */
#define VC_DEV_NO      20

/* Type of device methods. */
#define D_OPEN_T    d_open_t
#define D_CLOSE_T   d_close_t
#define D_RDWR_T    d_rdwr_t
#define D_READ_T    d_read_t
#define D_WRITE_T   d_write_t
#define D_IOCTL_T   d_ioctl_t
#define D_SELECT_T  d_select_t

/* rvb why */
D_OPEN_T    vc_nb_open;		/* was is defined in cfs_FreeBSD.h */
D_CLOSE_T   vc_nb_close;
D_READ_T    vc_nb_read;
D_WRITE_T   vc_nb_write;
D_IOCTL_T   vc_nb_ioctl;
D_SELECT_T  vc_nb_select;
void        vcattach __P((void));

static struct cdevsw vccdevsw =
{ 
  vc_nb_open,      vc_nb_close,    vc_nb_read,        vc_nb_write,
  vc_nb_ioctl,     nostop,         nullreset,         nodevtotty,
  vc_nb_select,    nommap,         NULL,              "Coda", NULL, -1 };

static dev_t vccdev;

PSEUDO_SET(vcattach, vc);

int     vcdebug = 1;
#define VCDEBUG if (vcdebug) printf

void
vcattach(void)
{
  /*
   * In case we are an LKM, set up device switch.
   */
  if (0 == (vccdev = makedev(VC_DEV_NO, 0)))
    VCDEBUG("makedev returned null\n");
  else 
    VCDEBUG("makedev OK.\n");
    
  cdevsw_add(&vccdev, &vccdevsw, NULL);
  VCDEBUG("cfs: vccdevsw entry installed at %d.\n", major(vccdev));
}



#ifdef __MAYBE_FreeBSD__

#include <sys/queue.h>
#include <sys/mount.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>

struct mount *
devtomp (dev_t dev)
{
  struct mount *mp;

  for (mp = mountlist.cqh_first; mp != NULL; mp = mp->mnt_list.cqe_next)
    if (VT_UFS == (mp->mnt_vnodelist.lh_first)->v_tag) {
      if (dev == VFSTOUFS(mp)->um_dev) return mp;
    }
  return (struct mount*)0;
}

#endif /* __FreeBSD__ */
