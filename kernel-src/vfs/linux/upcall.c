/*
 * Mostly platform independent upcall operations to Venus:
 *  -- upcalls
 *  -- upcall routines
 *
 * Linux 2.0 version
 * Copyright (C) 1996 Peter J. Braam <braam@maths.ox.ac.uk>, 
 * Michael Callahan <callahan@maths.ox.ac.uk> 
 * 
 * Redone for Linux 2.1
 * Copyright (C) 1997 Carnegie Mellon University
 *
 * Carnegie Mellon University encourages users of this code to contribute
 * improvements to the Coda project. Contact Peter Braam <coda@cs.cmu.edu>.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <asm/segment.h>
#include <linux/string.h>

#include <linux/coda.h>
#include <cfs_linux.h>
#include <psdev.h>
#include "super.h"
#include <upcall.h>
#include <cnode.h>
#include "namecache.h"

#define UPARG(op)\
do {\
  	CODA_ALLOC(inp, union inputArgs *, insize);\
	outp = (union outputArgs *) (inp);\
        inp->ih.opcode = (op);\
	inp->ih.pid = current->pid;\
	inp->ih.pgid = current->gid;\
	coda_load_creds(&(inp->ih.cred));\
        outsize = insize;\
} while (0)

static inline int max(int a, int b) 
{
	return (( a > b ) ? a : b);
}

#define INSIZE(tag) sizeof(struct cfs_ ## tag ## _in)
#define OUTSIZE(tag) sizeof(struct cfs_ ## tag ## _out)
#define SIZE(tag)  max(INSIZE(tag), OUTSIZE(tag))


/* the upcalls */
int venus_rootfid(struct super_block *sb, ViceFid *fidp)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
ENTRY;

        insize = SIZE(root);
        UPARG(CFS_ROOT);
	error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);
	
	if (error) {
	        printk("coda_get_rootfid: error %d\n", error);
	} else {
	        *fidp = (ViceFid) outp->cfs_root.VFid;
		CDEBUG(D_SUPER, "VolumeId: %lx, VnodeId: %lx.\n",
		       fidp->Volume, fidp->Vnode);
	}

	if (inp)  CODA_FREE(inp, insize);
        EXIT;
	return -error;
}

int venus_getattr(struct super_block *sb, struct ViceFid *fid, 
		     struct coda_vattr *attr) 
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
ENTRY;
        insize = SIZE(getattr); 
	UPARG(CFS_GETATTR);
        inp->cfs_getattr.VFid = *fid;
        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);
	
	if ( !error )
	        *attr = outp->cfs_getattr.attr;

        if (inp) CODA_FREE(inp, insize);
        EXIT;
        return -error;
}

int  venus_setattr(struct super_block *sb, struct ViceFid *fid, 
		      struct coda_vattr *vattr)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
	
	insize= SIZE(setattr);
	UPARG(CFS_SETATTR);

        inp->cfs_setattr.VFid = *fid;
	inp->cfs_setattr.attr = *vattr;

        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

        CDEBUG(D_SUPER, " result %d\n", error); 
        if ( inp ) CODA_FREE(inp, insize);
        return -error;
}

int venus_lookup(struct super_block *sb, struct ViceFid *fid, 
		    const char *name, int length, int * type, 
		    struct ViceFid *resfid)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
	int offset;

	offset = INSIZE(lookup);
        insize =  max(offset + length +1, OUTSIZE(lookup));
	UPARG(CFS_LOOKUP);

        inp->cfs_lookup.VFid = *fid;
	inp->cfs_lookup.name = offset;
        /* send Venus a null terminated string */
        memcpy((char *)(inp) + offset, name, length);
        *((char *)inp + offset + length) = '\0';

        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

	if ( !error ) {
	        *resfid = outp->cfs_lookup.VFid;
		*type = outp->cfs_lookup.vtype;
	}
	if (inp) CODA_FREE(inp, insize);
		
	return -error;
}


int venus_release(struct super_block *sb, struct ViceFid *fid, int flags)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
	
	insize = SIZE(close);
	UPARG(CFS_CLOSE);

        inp->cfs_close.VFid = *fid;
        inp->cfs_close.flags = flags;

        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

        if (inp) 
		CODA_FREE(inp, insize);
        return -error;
}

int venus_open(struct super_block *sb, struct ViceFid *fid,
		  int flags, ino_t *ino, dev_t *dev)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
       
	insize = SIZE(open);
	UPARG(CFS_OPEN);

        inp->cfs_open.VFid = *fid;
        inp->cfs_open.flags = flags;

        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

	if ( !error ) {
	        *ino = outp->cfs_open.inode;
	        *dev = outp->cfs_open.dev;
	} else {
	        *ino = 0;
		*dev = 0;
	}

        if (inp) 
                CODA_FREE(inp, insize);

	return -error;
}	

int venus_mkdir(struct super_block *sb, struct ViceFid *dirfid, 
		   const char *name, int length, 
		   struct ViceFid *newfid, struct coda_vattr *attrs)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset;

	offset = INSIZE(mkdir);
	insize = max(offset + length + 1, OUTSIZE(mkdir));
	UPARG(CFS_MKDIR);

        inp->cfs_mkdir.VFid = *dirfid;
        inp->cfs_mkdir.attr = *attrs;
	inp->cfs_mkdir.name = offset;
        /* Venus must get null terminated string */
        memcpy((char *)(inp) + offset, name, length);
        *((char *)inp + offset + length) = '\0';
        
        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

	*attrs = outp->cfs_mkdir.attr;
	*newfid = outp->cfs_mkdir.VFid;

	if (inp) 
	        CODA_FREE(inp, insize);
	return -error;        
}


int venus_rename(struct super_block *sb, struct ViceFid *old_fid, 
		 struct ViceFid *new_fid, size_t old_length, 
		 size_t new_length, const char *old_name, 
		 const char *new_name)
{
	union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error; 
	int offset, s;
	
	offset = INSIZE(rename);
	insize = max(offset + new_length + old_length + 8,
		     OUTSIZE(rename)); 
 	UPARG(CFS_RENAME);

        inp->cfs_rename.sourceFid = *old_fid;
        inp->cfs_rename.destFid =  *new_fid;
        inp->cfs_rename.srcname = offset;

        /* Venus must receive an null terminated string */
        s = ( old_length & ~0x3) +4; /* round up to word boundary */
        memcpy((char *)(inp) + offset, old_name, old_length);
        *((char *)inp + offset + old_length) = '\0';

        /* another null terminated string for Venus */
        offset += s;
        inp->cfs_rename.destname = offset;
        s = ( new_length & ~0x3) +4; /* round up to word boundary */
        memcpy((char *)(inp) + offset, new_name, new_length);
        *((char *)inp + offset + new_length) = '\0';

        CDEBUG(D_INODE, "destname in packet: %s\n", 
              (char *)inp + (int) inp->cfs_rename.destname);
        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

	if (inp) CODA_FREE(inp, insize);
	return -error;
}

int venus_create(struct super_block *sb, struct ViceFid *dirfid, 
		    const char *name, int length, int excl, int mode, 
		    struct ViceFid *newfid, struct coda_vattr *attrs) 
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset;
ENTRY;

        offset = INSIZE(create);
	insize = max(offset + length + 1, OUTSIZE(create));
	UPARG(CFS_CREATE);

        inp->cfs_create.VFid = *dirfid;
        inp->cfs_create.attr.va_mode = mode;
	inp->cfs_create.excl = excl;
        inp->cfs_create.mode = mode;
        inp->cfs_create.name = offset;

	printk("insize %d, offset %d\n", insize, offset);
        /* Venus must get null terminated string */
        memcpy(((char *)inp) + offset, name, length);
        *(((char *)inp) + offset + length) = '\0';
                
        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

	*attrs = outp->cfs_create.attr;
	*newfid = outp->cfs_create.VFid;

	if (inp) 
	        CODA_FREE(inp, insize);
	return -error;        
}

int venus_rmdir(struct super_block *sb, struct ViceFid *dirfid, 
		    const char *name, int length)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset;

        offset = INSIZE(rmdir);
	insize = max(offset + length + 1, OUTSIZE(rmdir));
	UPARG(CFS_RMDIR);

        inp->cfs_rmdir.VFid = *dirfid;
        inp->cfs_rmdir.name = offset;
        memcpy((char *)(inp) + offset, name, length);
	*((char *)inp + offset + length) = '\0';
        
        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);
	if ( inp ) 
	        CODA_FREE(inp, insize);
	return -error;
}

int venus_remove(struct super_block *sb, struct ViceFid *dirfid, 
		    const char *name, int length)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int error=0, insize, outsize, offset;

        offset = INSIZE(remove);
	insize = max(offset + length + 1, OUTSIZE(remove));
	UPARG(CFS_REMOVE);

        inp->cfs_remove.VFid = *dirfid;
        inp->cfs_remove.name = offset;
        memcpy((char *)(inp) + offset, name, length);
	*((char *)inp + offset + length) = '\0';
        
        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);
	if ( inp ) 
	        CODA_FREE(inp, insize);
	return -error;
}

int venus_readlink(struct super_block *sb, struct ViceFid *fid, 
		      char *buffer, int *length)
{ 
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int retlen;
        char *result;
        
	insize = max(INSIZE(readlink), OUTSIZE(readlink)+ *length + 1);
	UPARG(CFS_READLINK);

        inp->cfs_readlink.VFid = *fid;
    
        error =  coda_upcall(coda_sbp(sb), insize, &outsize, inp);
	
	if (! error) {
                retlen = outp->cfs_readlink.count;
		if ( retlen > *length )
		        retlen = *length;
		*length = retlen;
		result =  (char *)outp + (int)outp->cfs_readlink.data;
		memcpy(buffer, result, retlen);
		*(buffer + retlen) = '\0';
	}
        
        if (inp) CODA_FREE(inp, insize);
        CDEBUG(D_INODE, " result %d\n",error);
        EXIT;
        return -error;
}



int venus_link(struct super_block *sb, struct ViceFid *fid, 
		  struct ViceFid *dirfid, const char *name, int len )
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset;

	offset = INSIZE(link);
	insize = max(offset  + len + 1, OUTSIZE(link));
        UPARG(CFS_LINK);

        inp->cfs_link.sourceFid = *fid;
        inp->cfs_link.destFid = *dirfid;
        inp->cfs_link.tname = offset;

        /* make sure strings are null terminated */
        memcpy((char *)(inp) + offset, name, len);
        *((char *)inp + offset + len) = '\0';
        
        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

        if (inp) 
	        CODA_FREE(inp, insize);
        CDEBUG(D_INODE, " result %d\n",error);
        EXIT;
        return -error;
}

int venus_symlink(struct super_block *sb, struct ViceFid *fid,
		     const char *name, int len,
		     const char *symname, int symlen)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset, s;

        offset = INSIZE(symlink);
	insize = max(offset + len + symlen + 8, OUTSIZE(symlink));
	UPARG(CFS_SYMLINK);
        
        /*        inp->cfs_symlink.attr = *tva; XXXXXX */ 
        inp->cfs_symlink.VFid = *fid;

	/* Round up to word boundary and null terminate */
        inp->cfs_symlink.srcname = offset;
        s = ( symlen  & ~0x3 ) + 4; 
        memcpy((char *)(inp) + offset, symname, symlen);
        *((char *)inp + offset + symlen) = '\0';
        
	/* Round up to word boundary and null terminate */
        offset += s;
        inp->cfs_symlink.tname = offset;
        s = (len & ~0x3) + 4;
        memcpy((char *)(inp) + offset, name, len);
        *((char *)inp + offset + len) = '\0';

	error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

        if (inp) 
	        CODA_FREE(inp, insize);
        CDEBUG(D_INODE, " result %d\n",error);
        EXIT;
        return -error;
}

int venus_fsync(struct super_block *sb, struct ViceFid *fid)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
	
	insize=SIZE(fsync);
	UPARG(CFS_FSYNC);

        inp->cfs_fsync.VFid = *fid;
        error = coda_upcall(coda_sbp(sb), sizeof(union inputArgs), 
                            &outsize, inp);

	if ( inp ) 
		CODA_FREE(inp, insize);
	return -error;
}

int venus_access(struct super_block *sb, struct ViceFid *fid, int mask)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;

	insize = SIZE(access);
	UPARG(CFS_ACCESS);

        inp->cfs_access.VFid = *fid;
        inp->cfs_access.flags = mask << 6;

	error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

	if (inp) CODA_FREE(inp, insize);
        EXIT;
	return -error;
}


int venus_pioctl(struct super_block *sb, struct ViceFid *fid,
		 unsigned int cmd, struct PioctlData *data)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
	int iocsize;
	char str[50];

	insize = VC_MAXMSGSIZE;
	UPARG(CFS_IOCTL);

        /* build packet for Venus */
        if (data->vi.in_size > VC_MAXDATASIZE) {
	        error = EINVAL;
		goto exit;
        }

        inp->cfs_ioctl.VFid = *fid;
    
        /* the cmd field was mutated by increasing its size field to
         * reflect the path and follow args. We need to subtract that
         * out before sending the command to Venus.  */
        inp->cfs_ioctl.cmd = (cmd & ~(PIOCPARM_MASK << 16));	
        iocsize = ((cmd >> 16) & PIOCPARM_MASK) - sizeof(char *) - sizeof(int);
        inp->cfs_ioctl.cmd |= (iocsize & PIOCPARM_MASK) <<	16;	
    
        /* in->cfs_ioctl.rwflag = flag; */
        inp->cfs_ioctl.len = data->vi.in_size;
        inp->cfs_ioctl.data = (char *)(INSIZE(ioctl));
     
        /* get the data out of user space */
#ifdef L20
        memcpy_fromfs((char*)inp + (int)inp->cfs_ioctl.data,
                      data->vi.in, data->vi.in_size);
#else
        if ( copy_from_user((char*)inp + (int)inp->cfs_ioctl.data,
			    data->vi.in, data->vi.in_size) ) {
	        error = EINVAL;
	        goto exit;
	}
#endif
        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);
        
        if (error) {
	        printk("coda_pioctl: Venus returns: %d for %s\n", 
		       error, coda_f2s(fid, str));
		goto exit; 
	}
        
	/* Copy out the OUT buffer. */
        if (outp->cfs_ioctl.len > data->vi.out_size) {
                CDEBUG(D_FILE, "return len %d <= request len %d\n",
                      outp->cfs_ioctl.len, 
                      data->vi.out_size);
                error = EINVAL;
        } else {
		error = verify_area(VERIFY_WRITE, data->vi.out, 
                                    data->vi.out_size);
		if ( error ) goto exit;
#ifdef L20
                memcpy_tofs(data->vi.out, 
                            (char *)outp + (int)outp->cfs_ioctl.data, 
                            data->vi.out_size);                
#else
		if (copy_to_user(data->vi.out, 
				 (char *)outp + (int)outp->cfs_ioctl.data, 
				 data->vi.out_size)) {
		        error = EINVAL;
			goto exit;
		}
#endif
        }

 exit:
        if (inp) 
	        CODA_FREE(inp, insize);
	return -error;
}

void coda_load_creds(struct coda_cred *cred)
{
        cred->cr_uid = (vuid_t) current->uid;
        cred->cr_euid = (vuid_t) current->euid;
        cred->cr_suid = (vuid_t) current->suid;
        cred->cr_fsuid = (vuid_t) current->fsuid;

        cred->cr_gid = (vgid_t) current->gid;
        cred->cr_egid = (vgid_t) current->egid;
        cred->cr_sgid = (vgid_t) current->sgid;
        cred->cr_fsgid = (vgid_t) current->fsgid;

}
