/* cnode related routines for the coda kernel code
   Peter Braam, Sep 1996.
   */


#include "cfs.h"
#include "cnode.h"
#include "super.h" 

extern int coda_debug;
extern int coda_print_entry;
extern int coda_fetch_inode(struct inode *inode, struct vattr *attr);
extern int coda_getvattr(ViceFid *, struct vattr *, struct coda_sb_info *); 

/* cnode.c */
struct cnode *coda_cnode_alloc(void);
void coda_cnode_free(struct cnode *);
struct inode *coda_cnode_make(ViceFid *fid, struct super_block *sb);

/* return pointer to new empty cnode */
struct cnode *
coda_cnode_alloc(void)
{
        struct cnode *result = NULL;

        CODA_ALLOC(result, struct cnode *, sizeof(struct cnode));
        if ( !result ) {
                printk("coda_cnode_alloc: kmalloc returned NULL.\n");
                return result;
        }

        memset(result, 0, (int) sizeof(struct cnode));
        return result;
}

/* release cnode memory */
void 
coda_cnode_free(struct cnode *cinode)
{
        CODA_FREE(cinode, sizeof(struct cnode));
}
              
/* this is effectively coda_iget:
   - get attributes (might be cached)
   - get the inode for the fid using vfs iget
   - link the two up if this is needed
   - fill in the attributes
*/
struct inode *
coda_cnode_make(ViceFid *fid, struct super_block *sb)
{
        struct cnode *cnp;
        struct inode *result;
        struct vattr attr;
        int error;
        
        ENTRY;

        /* 
         * We get inode numbers from Venus -- see venus source
         */
        
        error = coda_getvattr(fid, &attr, coda_sbp(sb));
        
        if ( error ) {
                printk("coda_cnode_make: coda_getvattr returned %d\n", error);
                return NULL;
        }

        result = iget(sb, attr.va_fileid);

        if ( !result ) {
                printk("coda_cnode_make: iget failed\n");
                return NULL;
        }

	/* link the cnode and the vfs inode 
	   if this inode is not linked yet
	*/
	if ( !result->u.generic_ip ) {
        	cnp = coda_cnode_alloc();
        	if ( !cnp ) {
               		printk("coda_cnode_make: coda_cnode_alloc failed.\n");
                	return NULL;
        	}
        	cnp->c_fid = *fid;
        	cnp->c_magic = CODA_CNODE_MAGIC;
		cnp->c_flags = C_VATTR;
        	cnp->c_vnode = result;
        	result->u.generic_ip = (void *) cnp;
		CDEBUG(D_CNODE, "LINKING: ino %ld,  at 0x%x with cnp 0x%x, cnp->c_vnode 0x%x, in->u.generic_ip 0x%x\n", result->i_ino, (int) result, (int) cnp, (int)cnp->c_vnode, (int) result->u.generic_ip);
	} else {
		cnp = (struct cnode *)result->u.generic_ip;
		CDEBUG(D_CNODE, "FOUND linked: ino %ld,  at 0x%x with cnp 0x%x, cnp->c_vnode 0x%x\n", result->i_ino, (int) result, (int) cnp, (int)cnp->c_vnode);
	}
	CHECK_CNODE(cnp);

	/* refresh the attributes */
        error = coda_fetch_inode(result, &attr);
        if ( error ) {
                printk("coda_cnode_make: fetch_inode returned %d\n", error);
                return NULL;
        }
		CDEBUG(D_CNODE, "Done linking: ino %ld,  at 0x%x with cnp 0x%x, cnp->c_vnode 0x%x\n", result->i_ino, (int) result, (int) cnp, (int)cnp->c_vnode);

        EXIT;
        return result;
}

inline int
coda_fideq(ViceFid *fid1, ViceFid *fid2)
{
	int eq;
	eq =   ( (fid1->Vnode == fid2->Vnode) &&
		 (fid1->Volume == fid2->Volume) &&
		 (fid1->Unique == fid2->Unique) );
	return eq;
}

 
/* compute the inode number from the FID
   same routine as in vproc.cc (venus)
   XXX look at the exceptional case of root fids etc
*/
static ino_t
coda_fid2ino(ViceFid *fid)
{
	u_long ROOT_VNODE = 1;
	u_long ROOT_UNIQUE = 1;
	ViceFid nullfid = { 0, 0, 0};

	if ( coda_fideq(fid, &nullfid) ) {
		printk("coda_fid2ino: called with NULL Fid!\n");
		return 0;
	}

	/* what do we return for the root fid */

	/* Other volume root.  We need the relevant mount point's 
	fid, but we don't know what that is! */
	if (fid->Vnode == ROOT_VNODE && fid->Unique == ROOT_UNIQUE) {
		return(0);
	}

	/* Non volume root. */
	return(fid->Unique + (fid->Vnode << 10) + (fid->Volume << 20));
}     

/* convert a fid to an inode. Avoids having a hash table
   such as present in the Mach minicache */
struct inode *
coda_fid2inode(ViceFid *fid, struct super_block *sb) {
	ino_t nr;
	struct inode *inode;
	struct cnode *cnp;
	
	nr = coda_fid2ino(fid);
	inode = iget(sb, nr);

	/* check if this inode is linked to a cnode */
	cnp = (struct cnode *) inode->u.generic_ip;
	if ( cnp == NULL ) {
		iput(inode);
		return NULL;
	}
	/* make sure fid is the one we want */
	if ( !coda_fideq(fid, &(cnp->c_fid)) ) {
		printk("coda_fid2inode: bad cnode! Tell Peter.\n");
		iput(inode);
		return NULL;
	}

	iput(inode);	
	return inode;
}

		
