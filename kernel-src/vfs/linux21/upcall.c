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

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/signal.h>
#include <linux/signal.h>

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <asm/segment.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_psdev.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_cache.h>
#include <linux/coda_proc.h> 


static int  coda_upcall(struct coda_sb_info *mntinfo, int inSize, int *outSize, 
		       union inputArgs *buffer);

#define UPARG(op)\
do {\
  	CODA_ALLOC(inp, union inputArgs *, insize);\
        if ( !inp ) { return -ENOMEM; }\
        outp = (union outputArgs *) (inp);\
        inp->ih.opcode = (op);\
	inp->ih.pid = current->pid;\
	inp->ih.pgid = current->pgrp;\
	coda_load_creds(&(inp->ih.cred));\
        outsize = insize;\
} while (0)

static inline int max(int a, int b) 
{
	if ( a > b )
		return a; 
	else
		return b;
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
	return error;
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

        if (inp) 
		CODA_FREE(inp, insize);
        EXIT;
        return error;
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
        return error;
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
		
	return error;
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
        return error;
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

	return error;
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
	return error;        
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
	return error;
}

int venus_create(struct super_block *sb, struct ViceFid *dirfid, 
		    const char *name, int length, int excl, int mode, int rdev,
		    struct ViceFid *newfid, struct coda_vattr *attrs) 
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset;

        offset = INSIZE(create);
	insize = max(offset + length + 1, OUTSIZE(create));
	UPARG(CFS_CREATE);

        inp->cfs_create.VFid = *dirfid;
        inp->cfs_create.attr.va_mode = mode;
        inp->cfs_create.attr.va_rdev = rdev;
	inp->cfs_create.excl = excl;
        inp->cfs_create.mode = mode;
        inp->cfs_create.name = offset;

        /* Venus must get null terminated string */
        memcpy((char *)(inp) + offset, name, length);
        *((char *)inp + offset + length) = '\0';
                
        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

	*attrs = outp->cfs_create.attr;
	*newfid = outp->cfs_create.VFid;

	if (inp) 
	        CODA_FREE(inp, insize);
	return error;        
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
	return error;
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
	return error;
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
        return error;
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
        return error;
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
        return error;
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
	return error;
}

int venus_access(struct super_block *sb, struct ViceFid *fid, int mask)
{
        union inputArgs *inp;
        union outputArgs *outp; 
	int insize, outsize, error;

	insize = SIZE(access);
	UPARG(CFS_ACCESS);

        inp->cfs_access.VFid = *fid;
        inp->cfs_access.flags = mask;

	error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);

	if (inp) CODA_FREE(inp, insize);
        EXIT;
	return error;
}


int venus_pioctl(struct super_block *sb, struct ViceFid *fid,
		 unsigned int cmd, struct PioctlData *data)
{
        union inputArgs *inp;
        union outputArgs *outp;  
	int insize, outsize, error;
	int iocsize;

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
        if ( copy_from_user((char*)inp + (int)inp->cfs_ioctl.data,
			    data->vi.in, data->vi.in_size) ) {
	        error = EINVAL;
	        goto exit;
	}

        error = coda_upcall(coda_sbp(sb), insize, &outsize, inp);
        
        if (error) {
	        printk("coda_pioctl: Venus returns: %d for %s\n", 
		       error, coda_f2s(fid));
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

		if (copy_to_user(data->vi.out, 
				 (char *)outp + (int)outp->cfs_ioctl.data, 
				 data->vi.out_size)) {
		        error = EINVAL;
			goto exit;
		}
        }

 exit:
        if (inp) 
	        CODA_FREE(inp, insize);
	return error;
}

/*
 * coda_upcall and coda_downcall routines.
 * 
 */

static inline unsigned long coda_waitfor_upcall(struct vmsg *vmp)
{
	struct wait_queue	wait = { current, NULL };
	unsigned long posttime;

	vmp->vm_posttime = jiffies;
	posttime = jiffies;

	add_wait_queue(&vmp->vm_sleep, &wait);
	for (;;) {
		if ( coda_hard == 0 ) 
			current->state = TASK_INTERRUPTIBLE;
		else
			current->state = TASK_UNINTERRUPTIBLE;

		/* got a reply */
		if ( vmp->vm_flags & VM_WRITE )
			break;

		if ( !coda_hard && signal_pending(current) ) {
			/* if this process really wants to die, let it go */
			if ( sigismember(&(current->signal), SIGKILL) ||
			     sigismember(&(current->signal), SIGINT) )
				break;
			/* signal is present: after timeout always return 
			   really smart idea, probably useless ... */
			if ( jiffies > vmp->vm_posttime + coda_timeout * HZ )
				break; 
		}
		schedule();

	}
	remove_wait_queue(&vmp->vm_sleep, &wait);
	current->state = TASK_RUNNING;

	CDEBUG(D_SPECIAL, "posttime: %ld, returned: %ld\n", posttime, jiffies-posttime);
	return 	(jiffies - posttime);

}


/* 
 * coda_upcall will return an error in the case of 
 * failed communication with Venus _or_ will peek at Venus
 * reply and return Venus' error.
 *
 * As venus has 2 types of errors, normal errors (positive) and internal
 * errors (negative), normal errors are negated, while internal errors
 * are all mapped to -EINTR, while showing a nice warning message. (jh)
 * 
 */
static int coda_upcall(struct coda_sb_info *sbi, 
		int inSize, int *outSize, 
		union inputArgs *buffer) 
{
	unsigned long runtime; 
	struct vcomm *vcommp;
	union outputArgs *out;
	struct vmsg *vmp;
	int error = 0;

ENTRY;

	if (sbi->sbi_vcomm == NULL) {
                return -ENODEV;
	}
	vcommp = sbi->sbi_vcomm;


	if (!vcomm_open(vcommp))
                return -ENXIO;

	/* Format the request message. */
	CODA_ALLOC(vmp,struct vmsg *,sizeof(struct vmsg));
	vmp->vm_data = (void *)buffer;
	vmp->vm_flags = 0;
	vmp->vm_inSize = inSize;
	vmp->vm_outSize = *outSize ? *outSize : inSize;
	vmp->vm_opcode = ((union inputArgs *)buffer)->ih.opcode;
	vmp->vm_unique = ++vcommp->vc_seq;
        vmp->vm_sleep = NULL;
	
	/* Fill in the common input args. */
	((union inputArgs *)buffer)->ih.unique = vmp->vm_unique;

	/* Append msg to pending queue and poke Venus. */
	coda_q_insert(&(vmp->vm_chain), &(vcommp->vc_pending));
	CDEBUG(D_UPCALL, 
	       "Proc %d wake Venus for(opc,uniq) =(%d,%d) msg at %x.zzz.\n",
	       current->pid, vmp->vm_opcode, vmp->vm_unique, (int)vmp);

	wake_up_interruptible(&vcommp->vc_waitq);
	/* We can be interrupted while we wait for Venus to process
	 * our request.  If the interrupt occurs before Venus has read
	 * the request, we dequeue and return. If it occurs after the
	 * read but before the reply, we dequeue, send a signal
	 * message, and return. If it occurs after the reply we ignore
	 * it. In no case do we want to restart the syscall.  If it
	 * was interrupted by a venus shutdown (psdev_close), return
	 * ENODEV.  */

	/* Go to sleep.  Wake up on signals only after the timeout. */
	runtime = coda_waitfor_upcall(vmp);
	coda_upcall_stats(((union inputArgs *)buffer)->ih.opcode, runtime);

	CDEBUG(D_TIMING, "opc: %d time: %ld uniq: %d size: %d\n",
	       vmp->vm_opcode, jiffies - vmp->vm_posttime, 
	       vmp->vm_unique, vmp->vm_outSize);
	CDEBUG(D_UPCALL, 
	       "..process %d woken up by Venus for vmp at 0x%x, data at %x\n", 
	       current->pid, (int)vmp, (int)vmp->vm_data);
	if (vcomm_open(vcommp)) {      /* i.e. Venus is still alive */
	    /* Op went through, interrupt or not... */
	    if (vmp->vm_flags & VM_WRITE) {
		out = (union outputArgs *)vmp->vm_data;
		/* here we map positive Venus errors to kernel errors */
		if ( out->oh.result < 0 ) {
			printk("Tell Peter: Venus returns negative error %ld, for oc %ld!\n",
			       out->oh.result, out->oh.opcode);
			out->oh.result = EINTR;
		}
		error = -out->oh.result;
		CDEBUG(D_UPCALL, 
		       "upcall: (u,o,r) (%ld, %ld, %ld) out at %p\n", 
		       out->oh.unique, out->oh.opcode, out->oh.result, out);
		*outSize = vmp->vm_outSize;
		goto exit;
	    }
	    if ( !(vmp->vm_flags & VM_READ) && signal_pending(current)) { 
		/* Interrupted before venus read it. */
		CDEBUG(D_UPCALL, 
		       "Interrupted before read:(op,un) (%d.%d), flags = %x\n",
		       vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags);
		coda_q_remove(&(vmp->vm_chain));
		/* perhaps the best way to convince the app to
		   give up? */
		error = -EINTR;
		goto exit;
	    } 
	    if ( (vmp->vm_flags & VM_READ) && signal_pending(current) ) {
		    /* interrupted after Venus did its read, send signal */
		    union inputArgs *dog;
		    struct vmsg *svmp;
		    
		    CDEBUG(D_UPCALL, 
			   "Sending Venus a signal: op = %d.%d, flags = %x\n",
			   vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags);
		    
		    coda_q_remove(&(vmp->vm_chain));
		    error = -EINTR;
		    CODA_ALLOC(svmp, struct vmsg *, sizeof (struct vmsg));
		    CODA_ALLOC((svmp->vm_data), char *, sizeof(struct cfs_in_hdr));
		    
		    dog = (union inputArgs *)svmp->vm_data;
		    dog->ih.opcode = CFS_SIGNAL;
		    dog->ih.unique = vmp->vm_unique;
		    
		    svmp->vm_flags = 0;
		    svmp->vm_opcode = dog->ih.opcode;
		    svmp->vm_unique = dog->ih.unique;
		    svmp->vm_inSize = sizeof(struct cfs_in_hdr);
		    svmp->vm_outSize = sizeof(struct cfs_in_hdr);
		    CDEBUG(D_UPCALL, 
			   "coda_upcall: enqueing signal msg (%d, %d)\n",
			   svmp->vm_opcode, svmp->vm_unique);
		    
		    /* insert at head of queue! */
		    coda_q_insert(&(svmp->vm_chain), vcommp->vc_pending.forw);
		    wake_up_interruptible(&vcommp->vc_waitq);
	    } else {
		    printk("Coda: Strange interruption..\n");
		    error = -EINTR;
	    }
	} else {	/* If venus died i.e. !VC_OPEN(vcommp) */
	        printk("coda_upcall: Venus dead on (op,un) (%d.%d) flags %d\n",
		       vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags);
		error = -ENODEV;
	}

 exit:
	CODA_FREE(vmp, sizeof(struct vmsg));
	if (error) 
	        badclstats();
	return error;
}

/*  
    The statements below are part of the Coda opportunistic
    programming -- taken from the Mach/BSD kernel code for Coda. 
    You don't get correct semantics by stating what needs to be
    done without guaranteeing the invariants needed for it to happen.
    When will be have time to find out what exactly is going on?  (pjb)
*/


/* 
 * There are 7 cases where cache invalidations occur.  The semantics
 *  of each is listed here:
 *
 * CFS_FLUSH     -- flush all entries from the name cache and the cnode cache.
 * CFS_PURGEUSER -- flush all entries from the name cache for a specific user
 *                  This call is a result of token expiration.
 *
 * The next arise as the result of callbacks on a file or directory.
 * CFS_ZAPFILE   -- flush the cached attributes for a file.

 * CFS_ZAPDIR    -- flush the attributes for the dir and
 *                  force a new lookup for all the children
                    of this dir.

 *
 * The next is a result of Venus detecting an inconsistent file.
 * CFS_PURGEFID  -- flush the attribute for the file
 *                  purge it and its children from the dcache
 *
 * The last  allows Venus to replace local fids with global ones
 * during reintegration.
 *
 * CFS_REPLACE -- replace one ViceFid with another throughout the name cache */

int coda_downcall(int opcode, union outputArgs * out, struct super_block *sb)
{

	/* Handle invalidation requests. */
          if ( !sb ) { 
	          printk("coda_downcall: opcode %d, no sb!\n", opcode);
		  return 0; 
	  }

	  switch (opcode) {

	  case CFS_FLUSH : {
	           clstats(CFS_FLUSH);
		   CDEBUG(D_DOWNCALL, "CFS_FLUSH\n");
		   coda_cache_clear_all(sb);
		   shrink_dcache_sb(sb);
		   return(0);
	  }

	  case CFS_PURGEUSER : {
	           struct coda_cred *cred = &out->cfs_purgeuser.cred;
		   CDEBUG(D_DOWNCALL, "CFS_PURGEUSER\n");
		   if ( !cred ) {
		           printk("PURGEUSER: null cred!\n");
			   return 0;
		   }
		   clstats(CFS_PURGEUSER);
		   coda_cache_clear_cred(sb, cred);
		   return(0);
	  }

	  case CFS_ZAPDIR : {
	          struct inode *inode;
		  ViceFid *fid = &out->cfs_zapdir.CodaFid;
		  CDEBUG(D_DOWNCALL, "zapdir: fid = %s...\n", coda_f2s(fid));
		  clstats(CFS_ZAPDIR);

		  inode = coda_fid_to_inode(fid, sb);
		  if (inode) {
			  CDEBUG(D_DOWNCALL, "zapdir: inode = %ld children flagged\n", 
				 inode->i_ino);
			  coda_purge_children(inode);
			  CDEBUG(D_DOWNCALL, "zapdir: inode = %ld cache cleared\n", inode->i_ino);
	                  coda_flag_inode(inode, C_VATTR);
		  } else 
			  CDEBUG(D_DOWNCALL, "zapdir: no inode\n");
		  
		  return(0);
	  }

	  case CFS_ZAPFILE : {
	          struct inode *inode;
		  struct ViceFid *fid = &out->cfs_zapfile.CodaFid;
		  clstats(CFS_ZAPFILE);
		  CDEBUG(D_DOWNCALL, "zapfile: fid = %s\n", coda_f2s(fid));
		  inode = coda_fid_to_inode(fid, sb);
		  if ( inode ) {
			  CDEBUG(D_DOWNCALL, "zapfile: inode = %ld\n", inode->i_ino);
	                  coda_flag_inode(inode, C_VATTR);
		  } else 
			  CDEBUG(D_DOWNCALL, "zapfile: no inode\n");
		  return 0;
	  }

	  case CFS_PURGEFID : {
	          struct inode *inode;
		  ViceFid *fid = &out->cfs_purgefid.CodaFid;
		  CDEBUG(D_DOWNCALL, "purgefid: fid = %s\n", coda_f2s(fid));
		  clstats(CFS_PURGEFID);
		  inode = coda_fid_to_inode(fid, sb);
		  if ( inode ) { 
			  CDEBUG(D_DOWNCALL, "purgefid: inode = %ld\n", inode->i_ino);
			  coda_purge_children(inode);
			  coda_purge_dentries(inode);
		  }else 
			  CDEBUG(D_DOWNCALL, "purgefid: no inode\n");
		  return 0;
	  }

	  case CFS_REPLACE : {
	          struct inode *inode;
		  ViceFid *oldfid = &out->cfs_replace.OldFid;
		  ViceFid *newfid = &out->cfs_replace.NewFid;
		  clstats(CFS_REPLACE);
		  CDEBUG(D_DOWNCALL, "CFS_REPLACE\n");
		  inode = coda_fid_to_inode(oldfid, sb);
		  if ( inode ) { 
			  CDEBUG(D_DOWNCALL, "replacefid: inode = %ld\n", inode->i_ino);
			  coda_replace_fid(inode, oldfid, newfid);
		  }else 
			  CDEBUG(D_DOWNCALL, "purgefid: no inode\n");
		  
		  return 0;
	  }
	  }
	  return 0;
}


                
