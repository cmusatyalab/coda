int
venus_root(void *mdp,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid);

int
venus_open(void *mdp, ViceFid *fid, int flag,
	struct ucred *cred, struct proc *p,
/*out*/	dev_t *dev, ino_t *inode);

int
venus_close(void *mdp, ViceFid *fid, int flag,
	struct ucred *cred, struct proc *p);

void
venus_read(void);

void
venus_write(void);

int
venus_ioctl(void *mdp, ViceFid *fid,
	int com, int flag, caddr_t data,
	struct ucred *cred, struct proc *p);

int
venus_getattr(void *mdp, ViceFid *fid,
	struct ucred *cred, struct proc *p,
/*out*/	struct vattr *vap);

int
venus_setattr(void *mdp, ViceFid *fid, struct vattr *vap,
	struct ucred *cred, struct proc *p);

int
venus_access(void *mdp, ViceFid *fid, int mode,
	struct ucred *cred, struct proc *p);

int
venus_readlink(void *mdp, ViceFid *fid,
	struct ucred *cred, struct proc *p,
/*out*/	char **str, int *len);

int
venus_fsync(void *mdp, ViceFid *fid,
	struct ucred *cred, struct proc *p);

int
venus_lookup(void *mdp, ViceFid *fid,
    	const char *nm, int len,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid, int *vtype);

int
venus_create(void *mdp, ViceFid *fid,
    	const char *nm, int len, int exclusive, int mode, struct vattr *va,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid, struct vattr *attr);

int
venus_remove(void *mdp, ViceFid *fid,
        const char *nm, int len,
	struct ucred *cred, struct proc *p);

int
venus_link(void *mdp, ViceFid *fid, ViceFid *tfid,
        const char *nm, int len,
	struct ucred *cred, struct proc *p);

int
venus_rename(void *mdp, ViceFid *fid, ViceFid *tfid,
        const char *nm, int len, const char *tnm, int tlen,
	struct ucred *cred, struct proc *p);

int
venus_mkdir(void *mdp, ViceFid *fid,
    	const char *nm, int len, struct vattr *va,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid, struct vattr *ova);

int
venus_rmdir(void *mdp, ViceFid *fid,
    	const char *nm, int len,
	struct ucred *cred, struct proc *p);

int
venus_symlink(void *mdp, ViceFid *fid,
        const char *lnm, int llen, const char *nm, int len, struct vattr *va,
	struct ucred *cred, struct proc *p);

int
venus_readdir(void *mdp, ViceFid *fid,
    	int count, int offset,
	struct ucred *cred, struct proc *p,
/*out*/	char *buffer, int *len);

int
venus_fhtovp(void *mdp, ViceFid *fid,
	struct ucred *cred, struct proc *p,
/*out*/	ViceFid *VFid, int *vtype);
