/*
 * ifs.h: definitions for the inodefs system calls 
 */

#ifndef _IFS_H_
#define _IFS_H_

#if   defined(KERNEL) || defined(_KERNEL)

int getroot(int dev, struct vnode **rootvp);
struct vnode *igetvnode(dev_t dev, ino_t i_number);

struct icreate_args {
    int	dev;
    int	near_inode;	/* ignored */
    int	param1;
    int	param2;
    int	param3;
    int	param4;
};
int sys_icreate(struct proc *p, struct icreate_args *uap, int *retval);

struct iopen_args {
    int	dev;
    int	inode;
    int	usermode;
};
int sys_iopen(struct proc *p, struct iopen_args *uap, int *retval);

int 
readwritevnode(struct proc *p, enum uio_rw rw, struct vnode *vp,
	       caddr_t base, int len, off_t offset, int segflg,
	       int *aresid);
struct ireadwrite_args {
    int			dev;
    int			inode;
    long		inode_p1;
    unsigned int 	offset;
    char		*cbuf;
    unsigned int 	count;
};
int ireadwrite(struct proc *p, struct ireadwrite_args *uap,
	       int *retval, enum uio_rw rw);
int sys_iread(struct proc *p, struct ireadwrite_args *uap, int *retval);
int sys_iwrite(struct proc *p, struct ireadwrite_args *uap, int *retval);

struct iincdec_args {
    int		dev;
    int		inode;
    long	inode_p1;
};
int iincdec(struct proc *p, struct iincdec_args *uap, int *retval, int amount);
int sys_iinc(struct proc *p, struct iincdec_args *uap, int *retval);
int sys_idec(struct proc *p, struct iincdec_args *uap, int *retval);

struct pioctl_args {
    char	*path;
    int		com;
    caddr_t	comarg;
    int		follow;
};
int sys_pioctl(struct proc *p, struct pioctl_args *uap, int *retval);

#ifdef	NetBSD1_3
#define i_xxx_nlink	i_ffs_nlink
#define i_xxx_uid	i_ffs_uid
#define i_xxx_gid	i_ffs_gid
#define i_xxx_mode	i_ffs_mode
#define i_xxx_vicep1	i_ffs_vicep1
#define i_xxx_vicep2	i_ffs_vicep2
#define i_xxx_vicep3	i_ffs_vicep3
#define i_xxx_vicep4	i_ffs_vicep4
#endif
  
#ifdef	NetBSD1_2
#define i_xxx_nlink	i_nlink
#define i_xxx_uid	i_uid
#define i_xxx_gid	i_gid
#define i_xxx_mode	i_mode
#define i_xxx_vicep1	i_vicep1
#define i_xxx_vicep2	i_vicep2
#define i_xxx_vicep3	i_vicep3
#define i_xxx_vicep4	i_vicep4
#endif

#ifdef	__FreeBSD__
#define	i_xxx_nlink	i_nlink
#define	i_xxx_uid	i_uid
#define	i_xxx_gid	i_gid
#define	i_xxx_mode	i_mode

#define	i_xxx_vicep1	i_vicep1
#define	i_xxx_vicep2	i_vicep2
#define	i_xxx_vicep3	i_vicep3
#define	i_xxx_vicep4	i_vicep4
#endif

extern struct mount *devtomp(dev_t);

#else /* _KERNEL */

extern int icreate __P((int, int, int, int, int, int));
extern int iopen   __P((int, int, int));
extern int iread   __P((int, int, long, unsigned int, char *, unsigned int));
extern int iwrite  __P((int, int, long, unsigned int, char *, unsigned int));
extern int iinc    __P((int, int, long));
extern int idec    __P((int, int, long));
extern int pioctl  __P((char *, int, caddr_t, int));

#endif /* _KERNEL */

#endif /* _IFS_H_ */
