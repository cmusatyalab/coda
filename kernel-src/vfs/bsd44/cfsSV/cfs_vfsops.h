/*
 * cfid structure:
 * This overlays the fid structure (see vfs.h)
 * Only used below and will probably go away.
 */
struct cfid {
    u_short	cfid_len;
    u_short     padding;
    ViceFid	cfid_fid;
};


struct mount;

int cfs_vfsopstats_init(void);
#ifdef	NetBSD1_3
int cfs_mount(struct mount *, const char *, void *, struct nameidata *, 
		       struct proc *);
#else
int cfs_mount(struct mount *, char *, caddr_t, struct nameidata *, 
		       struct proc *);
#endif
int cfs_start(struct mount *, int, struct proc *);
int cfs_unmount(struct mount *, int, struct proc *);
int cfs_root(struct mount *, struct vnode **);
int cfs_quotactl(struct mount *, int, uid_t, caddr_t, struct proc *);
int cfs_nb_statfs(struct mount *, struct statfs *, struct proc *);
int cfs_sync(struct mount *, int, struct ucred *, struct proc *);
int cfs_vget(struct mount *, ino_t, struct vnode **);
int cfs_fhtovp(struct mount *, struct fid *, struct mbuf *, struct vnode **,
		       int *, struct ucred **);
int cfs_vptofh(struct vnode *, struct fid *);
#ifdef	__NetBSD__
void cfs_init(void);
#elif defined(__FreeBSD__)
int cfs_init(void);
#endif
int getNewVnode(struct vnode **vpp);
