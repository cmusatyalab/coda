struct mount;

int cfs_mount(struct mount *, char *, caddr_t, struct nameidata *, 
		       struct proc *);
int cfs_start(struct mount *, int, struct proc *);
int cfs_unmount(struct mount *, int, struct proc *);
int cfs_root(struct mount *, struct vnode **);
int cfs_quotactl(struct mount *, int, uid_t, caddr_t, struct proc *);
int cfs_sync(struct mount *, int, struct ucred *, struct proc *);
int cfs_vget(struct mount *, ino_t, struct vnode **);
int cfs_fhtovp(struct mount *, struct fid *, struct mbuf *, struct vnode **,
		       int *, struct ucred **);
int cfs_vptofh(struct vnode *, struct fid *);
void cfs_init(void);
