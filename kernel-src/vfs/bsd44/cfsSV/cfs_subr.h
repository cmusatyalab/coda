struct cnode *cfs_alloc(void);
void  cfs_free(struct cnode *cp);
struct cnode *cfs_find(ViceFid *fid);
void cfs_flush(enum dc_status dcstat);
void cfs_testflush(void);
int  cfs_checkunmounting(struct mount *mp);
int  cfs_cacheprint(struct mount *whoIam);
void cfs_debugon(void);
void cfs_debugoff(void);
int  cfs_kill(struct mount *whoIam, enum dc_status dcstat);
void cfs_save(struct cnode *cp);
void cfs_unsave(struct cnode *cp);


