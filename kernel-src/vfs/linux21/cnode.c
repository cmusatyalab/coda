/* cnode related routines for the coda kernel code
   Peter Braam, Sep 1996.
   */

#include <linux/types.h>
#include <linux/time.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_psdev.h>
#include <linux/coda_cnode.h>

extern int coda_debug;
extern int coda_print_entry;
extern int coda_fetch_inode(struct inode *inode, struct coda_vattr *attr);

/* cnode.c */
struct cnode *coda_cnode_alloc(void);
void coda_cnode_free(struct cnode *);
int  coda_cnode_make(struct inode **inode, ViceFid *fid, struct super_block *sb);

/* return pointer to new empty cnode */
struct cnode *coda_cnode_alloc(void)
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
void coda_cnode_free(struct cnode *cinode)
{
        CODA_FREE(cinode, sizeof(struct cnode));
}
              
/* this is effectively coda_iget:
   - get attributes (might be cached)
   - get the inode for the fid using vfs iget
   - link the two up if this is needed
   - fill in the attributes
*/
int coda_cnode_make(struct inode **inode, ViceFid *fid, struct super_block *sb)
{
        struct cnode *cnp;
        struct coda_vattr attr;
        int error;
	ino_t ino;
	char str[50];
        
        ENTRY;

        /* 
         * We get inode numbers from Venus -- see venus source
         */

	error = venus_getattr(sb, fid, &attr);
	if ( error ) {
	    printk("coda_cnode_make: coda_getvattr returned %d for %s.\n", 
		   error, coda_f2s(fid, str));
	    *inode = NULL;
	    return error;
	} 

	ino = attr.va_fileid;
        *inode = iget(sb, ino);
        if ( !*inode ) {
                printk("coda_cnode_make: iget failed\n");
                return -ENOMEM;
        }

	/* link the cnode and the vfs inode 
	   if this inode is not linked yet
	*/
	if ( !(*inode)->u.generic_ip ) {
        	cnp = coda_cnode_alloc();
        	if ( !cnp ) {
               		printk("coda_cnode_make: coda_cnode_alloc failed.\n");
			clear_inode(*inode);
                	return -ENOMEM;
        	}
        	cnp->c_fid = *fid;
        	cnp->c_magic = CODA_CNODE_MAGIC;
		cnp->c_flags = C_VATTR;
        	cnp->c_vnode = *inode;
        	(*inode)->u.generic_ip = (void *) cnp;
		CDEBUG(D_CNODE, "LINKING: ino %ld, count %d  at 0x%x with cnp 0x%x, cnp->c_vnode 0x%x, in->u.generic_ip 0x%x\n", (*inode)->i_ino, (*inode)->i_count, (int) (*inode), (int) cnp, (int)cnp->c_vnode, (int) (*inode)->u.generic_ip);
	} else {
	    cnp = (struct cnode *)(*inode)->u.generic_ip;
	    CDEBUG(D_CNODE, "FOUND linked: ino %ld, count %d, at 0x%x with cnp 0x%x, cnp->c_vnode 0x%x\n", (*inode)->i_ino, (*inode)->i_count, (int) (*inode), (int) cnp, (int)cnp->c_vnode);
	}
	CHECK_CNODE(cnp);

	/* refresh the attributes */
        error = coda_fetch_inode(*inode, &attr);
        if ( error ) {
                printk("coda_cnode_make: fetch_inode returned %d\n", error);
		clear_inode(*inode);
		coda_cnode_free(cnp);
                return -error;
        }
		CDEBUG(D_CNODE, "Done linking: ino %ld,  at 0x%x with cnp 0x%x, cnp->c_vnode 0x%x\n", (*inode)->i_ino, (int) (*inode), (int) cnp, (int)cnp->c_vnode);

        EXIT;
        return 0;
}


inline int coda_fideq(ViceFid *fid1, ViceFid *fid2)
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

/* the CONTROL inode is made without asking attributes from Venus */
int coda_cnode_makectl(struct inode **inode, struct super_block *sb)
{
    int error = 0;

    *inode = iget(sb, CTL_INO);
    if ( *inode ) {
	(*inode)->i_op = &coda_ioctl_inode_operations;
	(*inode)->i_mode = 00444;
	error = 0;
    } else { 
	error = -ENOMEM;
    }
    
    return error;
}
