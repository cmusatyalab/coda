


/* upcalls */
int venus_rootfid(struct super_block *sb, ViceFid *fidp);
int venus_getattr(struct super_block *sb, struct ViceFid *fid, 
		     struct coda_vattr *attr);
int venus_setattr(struct super_block *, struct ViceFid *, 
		     struct coda_vattr *);
int venus_lookup(struct super_block *sb, struct ViceFid *fid, 
		    const char *name, int length, int *type, 
		    struct ViceFid *resfid);
int venus_release(struct super_block *sb, struct ViceFid *fid, int flags);
int venus_open(struct super_block *sb, struct ViceFid *fid,
		  int flags, ino_t *ino, dev_t *dev);
int venus_mkdir(struct super_block *sb, struct ViceFid *dirfid, 
			  const char *name, int length, 
			  struct ViceFid *newfid, struct coda_vattr *attrs);
int venus_create(struct super_block *sb, struct ViceFid *dirfid, 
		    const char *name, int length, int excl, int mode, 
		    struct ViceFid *newfid, struct coda_vattr *attrs) ;
int venus_rmdir(struct super_block *sb, struct ViceFid *dirfid, 
		    const char *name, int length);
int venus_remove(struct super_block *sb, struct ViceFid *dirfid, 
		 const char *name, int length);
int venus_readlink(struct super_block *sb, struct ViceFid *fid, 
		   char *buffer, int *length);
int venus_rename(struct super_block *, struct ViceFid *new_fid, 
		 struct ViceFid *old_fid, size_t old_length, 
		 size_t new_length, const char *old_name, 
		 const char *new_name);
int venus_link(struct super_block *sb, struct ViceFid *fid, 
		  struct ViceFid *dirfid, const char *name, int len );
int venus_symlink(struct super_block *sb, struct ViceFid *fid,
		  const char *name, int len, const char *symname, int symlen);
int venus_access(struct super_block *sb, struct ViceFid *fid, int mask);
int venus_pioctl(struct super_block *sb, struct ViceFid *fid,
		 unsigned int cmd, struct PioctlData *data);
int venus_fsync(struct super_block *sb, struct ViceFid *fid);


int coda_upcall(struct coda_sb_info *mntinfo, int inSize, 
		int *outSize, union inputArgs *buffer);

