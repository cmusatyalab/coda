
/* revamped cnode.h file: platform dependent, kernel only! */


#ifndef	_CNODE_H_
#define	_CNODE_H_
#include <linux/coda.h>


#define CODA_CNODE_MAGIC        0x47114711

/* defintion of cnode, which combines ViceFid with inode information */
struct cnode {
        struct inode    *c_vnode;    /* linux inode associated with cnode */
        ViceFid	         c_fid;	     /* Coda identifier */
        u_short	         c_flags;     /* flags (see below) */
        int             c_magic;     /* to verify the data structure */
        u_short	        c_ocount;    /* count of openers */
        u_short         c_owrite;    /* count of open for write */
        u_short         c_mmcount;   /* count of mmappers */
        struct inode    *c_ovp;	     /* open vnode pointer */
        struct dentry   c_odentry;
};

/* flags */
#define C_VATTR       0x1         /* Validity of vattr in the cnode */
#define C_SYMLINK     0x2         /* Validity of symlink pointer in the cnode */
#define C_DYING	      0x4	  /* Set for outstanding cnodes from venus (which died) */

struct cnode *coda_cnode_alloc(void);
void coda_cnode_free(struct cnode *cinode);
int coda_cnode_make(struct inode **inode, ViceFid *fid, struct super_block *sb);
struct inode *coda_fid2inode(ViceFid *fid, struct super_block *sb);
int coda_cnode_makectl(struct inode **inode, struct super_block *sb);



#endif	

