/* cnode related routines for the coda kernel code
   (C) 1996 Peter Braam
   */

#include <linux/types.h>
#include <linux/time.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_psdev.h>

extern int coda_debug;
extern int coda_print_entry;

/* cnode.c */
static void coda_fill_inode(struct inode *inode, struct coda_vattr *attr)
{
        CDEBUG(D_SUPER, "ino: %ld\n", inode->i_ino);

        if (coda_debug & D_SUPER ) 
		print_vattr(attr);

        coda_vattr_to_iattr(inode, attr);

        if (S_ISREG(inode->i_mode))
                inode->i_op = &coda_file_inode_operations;
        else if (S_ISDIR(inode->i_mode))
                inode->i_op = &coda_dir_inode_operations;
        else if (S_ISLNK(inode->i_mode))
                inode->i_op = &coda_symlink_inode_operations;
        else if (S_ISCHR(inode->i_mode)) {
                inode->i_op = &chrdev_inode_operations;
                inode->i_rdev = to_kdev_t(attr->va_rdev);
        } else if (S_ISBLK(inode->i_mode)) {
                inode->i_op = &blkdev_inode_operations;
                inode->i_rdev = to_kdev_t(attr->va_rdev);
        } else if (S_ISFIFO(inode->i_mode))
                init_fifo(inode);
	else if (S_ISSOCK(inode->i_mode))
		inode->i_op = NULL;
        else {
                printk ("coda_fill_inode: what's this? i_mode = %o\n", 
			inode->i_mode);
                inode->i_op = NULL;
        }
}

/* this is effectively coda_iget:
   - get attributes (might be cached)
   - get the inode for the fid using vfs iget
   - link the two up if this is needed
   - fill in the attributes
*/
int coda_cnode_make(struct inode **inode, ViceFid *fid, struct super_block *sb)
{
        struct coda_inode_info *cnp;
	struct coda_sb_info *sbi= coda_sbp(sb);
        struct coda_vattr attr;
        int error;
	ino_t ino;
        
        ENTRY;

        /* 
	 * We get inode numbers from Venus -- see venus source
	 */

	error = venus_getattr(sb, fid, &attr);
	if ( error ) {
	    CDEBUG(D_CNODE, 
		   "coda_cnode_make: coda_getvattr returned %d for %s.\n", 
		   error, coda_f2s(fid));
	    *inode = NULL;
	    return error;
	} 

	ino = attr.va_fileid;
        *inode = iget(sb, ino);
        if ( !*inode ) {
                printk("coda_cnode_make: iget failed\n");
                return -ENOMEM;
        }

	cnp = ITOC(*inode);
       if  ( cnp->c_magic != 0 ) {
               printk("coda_cnode make on initialized inode %ld, old %s new
%s!\n",
                      (*inode)->i_ino, coda_f2s(&cnp->c_fid), coda_f2s2(fid));
               iput(*inode);
               return -ENOENT;
        }
 
       memset(cnp, 0, (int) sizeof(struct coda_inode_info));
       cnp->c_fid = *fid;
       cnp->c_magic = CODA_CNODE_MAGIC;
       cnp->c_flags = 0;
       cnp->c_vnode = *inode;
       INIT_LIST_HEAD(&(cnp->c_cnhead));
       INIT_LIST_HEAD(&(cnp->c_volrootlist));

	/* fill in the inode attributes */
	if ( coda_f2i(fid) != ino ) {
	        if ( !coda_fid_is_weird(fid) ) 
		        printk("Coda: unknown weird fid: ino %ld, fid %s."
			       "Tell Peter.\n", ino, coda_f2s(&cnp->c_fid));
		list_add(&cnp->c_volrootlist, &sbi->sbi_volroothead);
		CDEBUG(D_UPCALL, "Added %ld ,%s to volroothead\n",
		       ino, coda_f2s(&cnp->c_fid));
	}

        coda_fill_inode(*inode, &attr);
	CDEBUG(D_DOWNCALL, "Done making inode: ino %ld,  count %d with %s\n",
	        (*inode)->i_ino, (*inode)->i_count, 
	       coda_f2s(&cnp->c_fid));

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

void coda_replace_fid(struct inode *inode, struct ViceFid *oldfid, 
		      struct ViceFid *newfid)
{
	struct coda_inode_info *cnp;
	struct coda_sb_info *sbi= coda_sbp(inode->i_sb);
	
	cnp = ITOC(inode);

	if ( ! coda_fideq(&cnp->c_fid, oldfid) )
		printk("What? oldfid != cnp->c_fid. Call 911.\n");

	cnp->c_fid = *newfid;

	list_del(&cnp->c_volrootlist);
	if ( !coda_fid_is_weird(newfid) ) 
		list_add(&cnp->c_volrootlist, &sbi->sbi_volroothead);

	return;
}


 

/* convert a fid to an inode. Mostly we can compute
   the inode number from the FID, but not for volume
   mount points: those are in a list */
struct inode *coda_fid_to_inode(ViceFid *fid, struct super_block *sb) 
{
	ino_t nr;
	struct inode *inode;
	struct coda_inode_info *cnp;
	ENTRY;


	if ( !sb ) {
		printk("coda_fid_to_inode: no sb!\n");
		return NULL;
	}

	if ( !fid ) {
		printk("coda_fid_to_inode: no fid!\n");
		return NULL;
	}
	CDEBUG(D_INODE, "%s\n", coda_f2s(fid));


	if ( coda_fid_is_weird(fid) ) {
		struct coda_inode_info *cii;
		struct list_head *lh, *le;
		struct coda_sb_info *sbi = coda_sbp(sb);
		le = lh = &sbi->sbi_volroothead;

		while ( (le = le->next) != lh ) {
			cii = list_entry(le, struct coda_inode_info, 
					 c_volrootlist);
			CDEBUG(D_DOWNCALL, "iterating, now doing %s, ino %ld\n", 
			       coda_f2s(&cii->c_fid), cii->c_vnode->i_ino);
			if ( coda_fideq(&cii->c_fid, fid) ) {
				inode = cii->c_vnode;
				CDEBUG(D_INODE, "volume root, found %ld\n", cii->c_vnode->i_ino);
				if ( cii->c_magic != CODA_CNODE_MAGIC )
					printk("%s: Bad magic in inode, tell Peter.\n", 
					       __FUNCTION__);
				return cii->c_vnode;
			}
			
		}
		return NULL;
	}

	/* fid is not weird: ino should be computable */
	nr = coda_f2i(fid);
	inode = iget(sb, nr);
	if ( !inode ) {
		printk("coda_fid_to_inode: null from iget, sb %p, nr %ld.\n",
		       sb, nr);
		return NULL;
	}

	/* check if this inode is linked to a cnode */
	cnp = ITOC(inode);
	if ( cnp->c_magic != CODA_CNODE_MAGIC ) {
		CDEBUG(D_INODE, "uninitialized inode. Return.\n");
		iput(inode);
		return NULL;
	}

	/* make sure fid is the one we want; 
	   unfortunately Venus will shamelessly send us mount-symlinks.
	   These have the same inode as the root of the volume they
	   mount, but the fid will be wrong. 
	*/
	if ( !coda_fideq(fid, &(cnp->c_fid)) ) {
		/* printk("coda_fid2inode: bad cnode (ino %ld, fid %s)"
		   "Tell Peter.\n", nr, coda_f2s(fid)); */
		iput(inode);
		return NULL;
	}

	CDEBUG(D_INODE, "found %ld\n", inode->i_ino);
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
