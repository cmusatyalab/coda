#ifndef __CODA_PSDEV_H
#define __CODA_PSDEV_H

#define CODA_PSDEV_MAJOR 67
#define MAX_CODADEVS  5	   /* how many do we allow */

extern struct vcomm psdev_vcomm[];

/* queue stuff; the rest is static to psdev.c */
struct queue {
    struct queue *forw, *back;
};
void coda_q_insert(struct queue *el, struct queue *q);
void coda_q_remove(struct queue *q);


#define CODA_SUPER_MAGIC	0x73757245

struct coda_sb_info
{
	struct inode *      sbi_psdev;     /* /dev/cfs? Venus/kernel device */
	struct inode *      sbi_ctlcp;     /* control magic file */
	int                 sbi_refct;
	struct vcomm *      sbi_vcomm;
	struct inode *      sbi_root;
	struct list_head    sbi_cchead;
	struct list_head    sbi_volroothead;
};

/* communication pending/processing queues queues */
struct vcomm {
	u_long		    vc_seq;
	struct wait_queue  *vc_waitq; /* Venus wait queue */
	struct queue	    vc_pending;
	struct queue	    vc_processing;
	struct super_block *vc_sb;
	int                 vc_inuse;
};

static inline int vcomm_open(struct vcomm *vcp)
{
        return ((vcp)->vc_pending.forw != NULL);
}

static inline void mark_vcomm_closed(struct vcomm *vcp)
{
        (vcp)->vc_pending.forw = NULL;
}

static inline struct coda_sb_info *coda_sbp(struct super_block *sb)
{
    return ((struct coda_sb_info *)((sb)->u.generic_sbp));
}



extern void coda_psdev_detach(int unit);
extern int  init_coda_psdev(void);


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
		    const char *name, int length, int excl, int mode, int rdev,
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
int coda_downcall(int opcode, union outputArgs *out, struct super_block *sb);
int venus_fsync(struct super_block *sb, struct ViceFid *fid);


/* messages between coda filesystem in kernel and Venus */
extern int coda_hard;
extern unsigned long coda_timeout;
struct vmsg {
	struct queue        vm_chain;
	caddr_t	        vm_data;
	u_short	        vm_flags;
	u_short             vm_inSize;  /* Size is at most 5000 bytes */
	u_short	        vm_outSize;
	u_short	        vm_opcode;  /* copied from data to save lookup */
	int		        vm_unique;
	struct wait_queue  *vm_sleep;   /* process' wait queue */
	unsigned long       vm_posttime;
};


/*
 * Statistics
 */
struct coda_upcallstats {
	int	ncalls;			/* client requests */
	int	nbadcalls;		/* upcall failures */
	int	reqs[CFS_NCALLS];	/* count of each request */
} ;

extern struct coda_upcallstats coda_callstats;

static inline void clstats(int opcode)
{
    coda_callstats.ncalls++;
    if ( (0 <= opcode) && (opcode <= CFS_NCALLS) )
	coda_callstats.reqs[opcode]++;
    else
	printk("clstats called with bad opcode %d\n", opcode); 
}

static inline void badclstats(void)
{
    coda_callstats.nbadcalls++;
}

#endif
