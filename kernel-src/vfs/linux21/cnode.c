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


              
static void coda_fill_inode (struct inode *inode, struct coda_vattr *attr)
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
        else {
                printk ("coda_read_inode: what's this? i_mode = %o\n", 
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
	    printk("coda_cnode_make: coda_getvattr returned %d for %s.\n", 
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
	if  ( cnp->c_magic == 0 ) {
		memset(cnp, 0, (int) sizeof(struct coda_inode_info));
		cnp->c_fid = *fid;
		cnp->c_magic = CODA_CNODE_MAGIC;
		cnp->c_flags = C_VATTR;
		cnp->c_vnode = *inode;
		INIT_LIST_HEAD(&(cnp->c_cnhead));
		INIT_LIST_HEAD(&(cnp->c_volrootlist));
	} else {
		printk("coda_cnode make on initialized inode %ld, %s!\n",
		       (*inode)->i_ino, coda_f2s(&cnp->c_fid));
	}

	/* fill in the inode attributes */
	if ( coda_fid_is_volroot(fid) ) 
		list_add(&cnp->c_volrootlist, &sbi->sbi_volroothead);

        coda_fill_inode(*inode, &attr);
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

 

/* convert a fid to an inode. Avoids having a hash table
   such as present in the Mach minicache */
struct inode *coda_fid_to_inode(ViceFid *fid, struct super_block *sb) 
{
	ino_t nr;
	struct inode *inode;
	struct coda_inode_info *cnp;
ENTRY;

	CDEBUG(D_INODE, "%s\n", coda_f2s(fid));

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
