/*
 * Cnode definitions for Coda.
 * Original version: (C) 1996 Peter Braam 
 * Rewritten for Linux 2.1: (C) 1997 Carnegie Mellon University
 *
 * Carnegie Mellon encourages users of this code to contribute improvements
 * to the Coda project. Contact Peter Braam <coda@cs.cmu.edu>.
 */


#ifndef	_CNODE_H_
#define	_CNODE_H_
#include <linux/coda.h>


#define CODA_CNODE_MAGIC        0x47114711

/* defintion of cnode, which combines ViceFid with inode information */
struct cnode {
        struct inode      *c_vnode;     /*  inode associated with cnode */
        ViceFid	           c_fid;	/* Coda identifier */
        u_short	           c_flags;     /* flags (see below) */
        int                c_magic;     /* to verify the data structure */
        u_short	           c_ocount;    /* count of openers */
        u_short            c_owrite;    /* count of open for write */
        u_short            c_mmcount;   /* count of mmappers */
        struct inode      *c_ovp;       /* open vnode pointer */
        struct list_head   c_cnhead;    /* head of cache entries */
};

/* flags */
#define C_VATTR       0x1         /* Validity of vattr in the cnode */
#define C_SYMLINK     0x2         /* Validity of symlink pointer in the cnode */
#define C_DYING       0x4	  /* Set for outstanding cnodes from venus (which died) */
#define C_ZAPFID      0x8
#define C_ZAPDIR      0x10

void coda_cnode_free(struct cnode *);
struct cnode *coda_cnode_alloc(void);
int coda_cnode_make(struct inode **, struct ViceFid *, struct super_block *);
int coda_cnode_makectl(struct inode **inode, struct super_block *sb);
struct inode *coda_fid_to_inode(ViceFid *fid, struct super_block *sb);

/* inode to cnode */
static inline struct cnode *ITOC(struct inode *inode)
{
	return ((struct cnode *)inode->u.generic_ip);
}

/* cnode to inode */
static inline struct inode *CTOI(struct cnode *cnode)
{
	return (cnode->c_vnode);
}

#endif	

