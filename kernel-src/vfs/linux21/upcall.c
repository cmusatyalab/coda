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
#include <linux/coda_cnode.h>
#include <linux/coda_namecache.h>


static vcsize = (sizeof(struct inputArgs) > sizeof(struct outputArgs)) ?
		 sizeof(struct inputArgs): sizeof(struct outputArgs);

/* the upcalls */
int venus_rootfid(struct super_block *sb, ViceFid *fidp)
{
        struct inputArgs *inp;
	struct outputArgs *outp;
	int error=0;
	int size;
ENTRY;

        UPARG(vcsize, CFS_ROOT);
	error = coda_upcall(coda_sbp(sb), VC_IN_NO_DATA, &size, inp);
	
	if (error) {
	        printk("coda_get_rootfid: error %d\n", error);
	} else {
	        *fidp = (ViceFid) outp->d.cfs_root.VFid;
		CDEBUG(D_SUPER, "VolumeId: %lx, VnodeId: %lx.\n",
		       fidp->Volume, fidp->Vnode);
	}

	if (inp)  CODA_FREE(inp, VC_IN_NO_DATA);
        EXIT;
	return -error;
}

int venus_getattr(struct super_block *sb, struct ViceFid *fid, 
		     struct coda_vattr *attr) 
{
        struct inputArgs *inp;
        struct outputArgs *outp;
        int size, error;
ENTRY;

	UPARG(vcsize, CFS_GETATTR);
        inp->d.cfs_getattr.VFid = *fid;
        error = coda_upcall(coda_sbp(sb), vcsize, &size, inp);
	
	if ( !error )
	        *attr = (struct coda_vattr) outp->d.cfs_getattr.attr;

        if (inp) CODA_FREE(inp, sizeof(struct inputArgs));
        EXIT;
        return -error;
}

int  venus_setattr(struct super_block *sb, struct ViceFid *fid, 
		      struct coda_vattr *vattr)
{
        struct inputArgs *inp;
        struct outputArgs *outp;
        int error, size;
	
	UPARG(vcsize, CFS_SETATTR);

        inp->d.cfs_setattr.VFid = *fid;
	inp->d.cfs_setattr.attr = *vattr;

        error = coda_upcall(coda_sbp(sb), vcsize, &size, inp);

        CDEBUG(D_SUPER, " result %ld\n", outp->result); 
        if ( inp ) CODA_FREE(inp, vcsize);
        return -error;
}

int venus_lookup(struct super_block *sb, struct ViceFid *fid, 
		    const char *name, int length, int * type, 
		    struct ViceFid *resfid)
{
        struct inputArgs *inp;
	struct outputArgs *outp;
	int insize, size, error=0, payload_offset;

        insize =  VC_INSIZE(cfs_lookup_in) + CFS_MAXNAMLEN +1;
	UPARG(insize, CFS_LOOKUP);

        inp->d.cfs_lookup.VFid = *fid;
        /* send Venus a null terminated string */
        payload_offset = VC_INSIZE(cfs_lookup_in);
        inp->d.cfs_lookup.name = (char *) payload_offset;
        memcpy((char *)inp + payload_offset, name, length);
        *((char *)inp + payload_offset + length) = '\0';

        size = payload_offset + length + 1;
        error = coda_upcall(coda_sbp(sb), size, &size, inp);

	if ( !error ) {
	        *resfid = outp->d.cfs_lookup.VFid;
		*type = outp->d.cfs_lookup.vtype;
	}
	if (inp) CODA_FREE(inp, insize);
		
	return -error;
}


int venus_release(struct super_block *sb, struct ViceFid *fid, int flags)
{
        struct inputArgs *inp;
	struct outputArgs *outp;
	int size = sizeof(struct outputArgs);
	int error = 0;

	CODA_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
        outp = (struct outputArgs *)inp;
        INIT_IN(inp, CFS_CLOSE);
        coda_load_creds(&(inp->cred));

        inp->d.cfs_close.VFid = *fid;
        inp->d.cfs_close.flags = flags;

        error = coda_upcall(coda_sbp(sb), size, &size, inp);

        if (inp) CODA_FREE(inp, sizeof(struct inputArgs));
        return -error;
}

int venus_open(struct super_block *sb, struct ViceFid *fid,
		  int flags, ino_t *ino, dev_t *dev)
{
        struct inputArgs *inp = NULL;
	struct outputArgs *outp = NULL;
	int size = sizeof(struct inputArgs);
	int error = 0;

        CODA_ALLOC(inp, struct inputArgs *, sizeof(struct inputArgs));
        outp = (struct outputArgs *)inp;
        INIT_IN(inp, CFS_OPEN);
        coda_load_creds(&(inp->cred));

        inp->d.cfs_open.VFid = *fid;
        inp->d.cfs_open.flags = flags;

        error = coda_upcall(coda_sbp(sb), size, &size, inp);

	if ( !error ) {
	        *ino = outp->d.cfs_open.inode;
	        *dev = outp->d.cfs_open.dev;
	} else {
	        *ino = 0;
		*dev = 0;
	}

        if (inp) 
                CODA_FREE(inp, sizeof(struct inputArgs));

	return -error;
}	

int venus_mkdir(struct super_block *sb, struct ViceFid *dirfid, 
		   const char *name, int length, 
		   struct ViceFid *newfid, struct coda_vattr *attrs)
{
        struct inputArgs *inp;
        struct outputArgs *outp;
        int error=0, size, payload_offset;

        payload_offset = VC_INSIZE(cfs_mkdir_in);
	size = CFS_MAXNAMLEN + payload_offset;
	UPARG(size, CFS_MKDIR);

        inp->d.cfs_mkdir.VFid = *dirfid;
        inp->d.cfs_mkdir.attr = *attrs;
        inp->d.cfs_mkdir.name = (char *) payload_offset;

        /* Venus must get null terminated string */
        memcpy((char *)inp + payload_offset, name, length);
        *((char *)inp + payload_offset + length) = '\0';
        size = payload_offset + length + 1;
        
        error = coda_upcall(coda_sbp(sb), size, &size, inp);

	*attrs = outp->d.cfs_mkdir.attr;
	*newfid = outp->d.cfs_mkdir.VFid;

	if (inp) 
	        CODA_FREE(inp, size);
	return -error;        
}


int venus_rename(struct super_block *sb, struct ViceFid *old_fid, 
		 struct ViceFid *new_fid, size_t old_length, 
		 size_t new_length, const char *old_name, const char *new_name)
{
        struct inputArgs *inp;
        struct outputArgs *outp;
	int error, offset, size, s;
	
	size = 2*CFS_MAXNAMLEN + VC_INSIZE(cfs_rename_in) +8;
	UPARG(size, CFS_RENAME);

        inp->d.cfs_rename.sourceFid = *old_fid;
        inp->d.cfs_rename.destFid =  *new_fid;

        offset = VC_INSIZE(cfs_rename_in);

        /* Venus must receive an null terminated string */
        inp->d.cfs_rename.srcname = (char *)offset;
        s = ( old_length & ~0x3) +4; /* round up to word boundary */
        memcpy((char *)inp + offset, old_name, old_length);
        *((char *)inp + offset + old_length) = '\0';

        /* another null terminated string for Venus */
        offset += s;
        inp->d.cfs_rename.destname = (char *)offset;
        s = ( new_length & ~0x3) +4; /* round up to word boundary */
        memcpy((char *)inp + offset, new_name, new_length);
        *((char *)inp + offset + new_length) = '\0';

        size += s;
        CDEBUG(D_INODE, "destname in packet: %s\n", 
              (char *)inp + (int) inp->d.cfs_rename.destname);
        error = coda_upcall(coda_sbp(sb), size, &size, inp);

	if (inp) CODA_FREE(inp, size);
	return -error;
}

int venus_create(struct super_block *sb, struct ViceFid *dirfid, 
		    const char *name, int length, int excl, int mode, 
		    struct ViceFid *newfid, struct coda_vattr *attrs) 
{
        struct inputArgs *inp;
        struct outputArgs *outp;
        int error=0, size, payload_offset;

        payload_offset = VC_INSIZE(cfs_create_in);
	size = CFS_MAXNAMLEN + payload_offset;
	UPARG(size, CFS_CREATE);

        inp->d.cfs_create.VFid = *dirfid;
        inp->d.cfs_create.attr.va_mode = mode;
	inp->d.cfs_create.excl = excl;
        inp->d.cfs_create.mode = mode;
        inp->d.cfs_create.name = (char *) payload_offset;

        /* Venus must get null terminated string */
        memcpy((char *)inp + payload_offset, name, length);
        *((char *)inp + payload_offset + length) = '\0';
        size = payload_offset + length + 1;
        
        error = coda_upcall(coda_sbp(sb), size, &size, inp);

	*attrs = outp->d.cfs_create.attr;
	*newfid = outp->d.cfs_create.VFid;

	if (inp) 
	        CODA_FREE(inp, size);
	return -error;        
}

int venus_rmdir(struct super_block *sb, struct ViceFid *dirfid, 
		    const char *name, int length)
{
        struct inputArgs *inp;
        struct outputArgs *outp;
        int error=0, size, payload_offset;

        payload_offset = VC_INSIZE(cfs_rmdir_in);
	size = CFS_MAXNAMLEN + payload_offset;
	UPARG(size, CFS_RMDIR);

        inp->d.cfs_rmdir.VFid = *dirfid;
        inp->d.cfs_rmdir.name = (char *) payload_offset;
        memcpy((char *)inp + payload_offset, name, size);
        
        error = coda_upcall(coda_sbp(sb), size, &size, inp);
	if ( inp ) 
	        CODA_FREE(inp, size);
	return -error;
}

int venus_remove(struct super_block *sb, struct ViceFid *dirfid, 
		    const char *name, int length)
{
        struct inputArgs *inp;
        struct outputArgs *outp;
        int error=0, size, payload_offset;

        payload_offset = VC_INSIZE(cfs_remove_in);
	size = CFS_MAXNAMLEN + payload_offset;
	UPARG(size, CFS_REMOVE);

        inp->d.cfs_remove.VFid = *dirfid;
        inp->d.cfs_remove.name = (char *)payload_offset;
        memcpy((char *)inp + payload_offset, name, size);
        
        error = coda_upcall(coda_sbp(sb), size, &size, inp);
	if ( inp ) 
	        CODA_FREE(inp, size);
	return -error;
}

int venus_readlink(struct super_block *sb, struct ViceFid *fid, 
		      char *buffer, int *length)
{ 
        int error, size, retlen;
        char *result;
        struct inputArgs *inp;
        struct outputArgs *outp;
        char *buf=NULL; /*[CFS_MAXNAMLEN + VC_INSIZE(cfs_readlink_in)];*/
        
	size = CFS_MAXPATHLEN + VC_INSIZE(cfs_readlink_in);
	UPARG(size, CFS_READLINK);
        inp->d.cfs_readlink.VFid = *fid;
    
        error =  coda_upcall(coda_sbp(sb), size, &size, inp);
	
	if (! error) {
                retlen = outp->d.cfs_readlink.count;
		if ( retlen > *length )
		        retlen = *length;
		*length = retlen;
		result =  (char *)outp + (int)outp->d.cfs_readlink.data;
		memcpy(buffer, result, retlen);
	}
        
        if (inp) CODA_FREE(buf, size);
        CDEBUG(D_INODE, " result %d\n",error);
        EXIT;
        return -error;
}

int venus_link(struct super_block *sb, struct ViceFid *fid, 
		  struct ViceFid *dirfid, const char *name, int len )
{
        int error, payload_offset, size;
        struct inputArgs *inp;
        struct outputArgs *outp;

	size = CFS_MAXNAMLEN + sizeof(struct inputArgs);
        UPARG(size, CFS_LINK);

        payload_offset = (VC_INSIZE(cfs_link_in));
        inp->d.cfs_link.sourceFid = *fid;
        inp->d.cfs_link.destFid = *dirfid;
        inp->d.cfs_link.tname = (char *)payload_offset;

        /* make sure strings are null terminated */
        memcpy((char *)inp + payload_offset, name, len);
        *((char *)inp + payload_offset + len) = '\0';
        size = payload_offset + len + 1;
        
        error = coda_upcall(coda_sbp(sb), size, &size, inp);

        if (inp) 
	        CODA_FREE(inp, size);
        CDEBUG(D_INODE, " result %d\n",error);
        EXIT;
        return -error;
}

int venus_symlink(struct super_block *sb, struct ViceFid *fid,
		     const char *name, int len,
		     const char *symname, int symlen)
{
        int error, payload_offset, size, s;
        struct inputArgs *inp;
        struct outputArgs *outp;
        

        /* 
         * allocate space for regular input, 
         * plus 1 path and 1 name, plus padding 
         */        
	size = sizeof(struct inputArgs) + CFS_MAXNAMLEN + CFS_MAXNAMLEN + 8;
	UPARG(size, CFS_SYMLINK);
        
        /*        inp->d.cfs_symlink.attr = *tva; XXXXXX */ 
        inp->d.cfs_symlink.VFid = *fid;

        payload_offset = VC_INSIZE(cfs_symlink_in);
        inp->d.cfs_symlink.srcname =(char*) payload_offset;
    
        s = ( symlen  & ~0x3 ) + 4; /* Round up to word boundary. */
    
        /* don't forget to copy out the null termination */
        memcpy((char *)inp + payload_offset, symname, symlen);
        *((char *)inp + payload_offset + symlen) = '\0';
        
        payload_offset += s;
        inp->d.cfs_symlink.tname = (char *) payload_offset;
        s = (len & ~0x3) + 4;	/* Round up to word boundary. */
        memcpy((char *)inp + payload_offset, name, len);
        *((char *)inp + payload_offset + len) = '\0';

        size = payload_offset + s;
        error = coda_upcall(coda_sbp(sb), size, &size, inp);

        if (inp) 
	        CODA_FREE(inp, size);
        CDEBUG(D_INODE, " result %d\n",error);
        EXIT;
        return -error;
}

int venus_access(struct super_block *sb, struct ViceFid *fid, int mask)
{
        struct inputArgs *inp;
        struct outputArgs *outp;
        int size;
        int error;

	size = sizeof(struct inputArgs);
	UPARG(size, CFS_ACCESS);

        inp->d.cfs_access.VFid = *fid;
        inp->d.cfs_access.flags = mask << 6;

	error = coda_upcall(coda_sbp(sb), size, &size, inp);

	if (inp) CODA_FREE(inp, sizeof(struct inputArgs));
        EXIT;
	return -error;
}


int venus_pioctl(struct super_block *sb, struct ViceFid *fid,
		 unsigned int cmd, struct PioctlData *data)
{
        struct inputArgs *inp;
	struct outputArgs *outp;
	int size, error = 0;
	int iocsize;
	char str[50];

	size = VC_MAXMSGSIZE;
	UPARG(size, CFS_IOCTL);

        /* build packet for Venus */
        if (data->vi.in_size > VC_DATASIZE) {
	        error = EINVAL;
		goto exit;
        }

        inp->d.cfs_ioctl.VFid = *fid;
    
        /* the cmd field was mutated by increasing its size field to
         * reflect the path and follow args. We need to subtract that
         * out before sending the command to Venus.  */
        inp->d.cfs_ioctl.cmd = (cmd & ~(IOCPARM_MASK << 16));	
        iocsize = ((cmd >> 16) & IOCPARM_MASK) - sizeof(char *) - sizeof(int);
        inp->d.cfs_ioctl.cmd |= (iocsize & IOCPARM_MASK) <<	16;	
    
        /* in->d.cfs_ioctl.rwflag = flag; */
        inp->d.cfs_ioctl.len = data->vi.in_size;
        inp->d.cfs_ioctl.data = (char *)(VC_INSIZE(cfs_ioctl_in));
     
        /* get the data out of user space */
        if ( copy_from_user((char*)inp + (int)inp->d.cfs_ioctl.data,
			    data->vi.in, data->vi.in_size) ) {
	        error = EINVAL;
	        goto exit;
	}
        error = coda_upcall(coda_sbp(sb), size, &size, inp);
        
        if (error) {
	        printk("coda_pioctl: Venus returns: %d for %s\n", 
		       error, coda_f2s(fid, str));
		goto exit; 
	}
        
	/* Copy out the OUT buffer. */
        if (outp->d.cfs_ioctl.len > data->vi.out_size) {
                CDEBUG(D_FILE, "return len %d <= request len %d\n",
                      outp->d.cfs_ioctl.len, 
                      data->vi.out_size);
                error = EINVAL;
        } else {
                if (copy_to_user(data->vi.out, 
				 (char *)outp + (int)outp->d.cfs_ioctl.data, 
				 data->vi.out_size)) {
		        error = EINVAL;
			goto exit;
		}
        }

 exit:
        if (inp) 
	        CODA_FREE(inp, VC_MAXMSGSIZE);
	return -error;
}

/*
 * coda_upcall and coda_downcall routines.
 * 
 */

/* 
 * coda_upcall will return a POSITIVE error in the case of 
 * failed communication with Venus _or_ will peek at Venus
 * reply and return Venus' error, also POSITIVE. 
 * 
 */
int coda_upcall(struct coda_sb_info *sbi, int inSize, int *outSize, 
		struct inputArgs *buffer) 
{
	struct vcomm *vcommp;
	struct outputArgs *out;
	struct vmsg *vmp;
	int error = 0;

ENTRY;

	if (sbi->sbi_vcomm == NULL) {
                return ENODEV;
	}
	vcommp = sbi->sbi_vcomm;

	clstats(((struct inputArgs *)buffer)->opcode);

	if (!vcomm_open(vcommp))
                return(ENODEV);

	/* Format the request message. */
	CODA_ALLOC(vmp,struct vmsg *,sizeof(struct vmsg));
	vmp->vm_data = (void *)buffer;
	vmp->vm_flags = 0;
	vmp->vm_inSize = inSize;
	vmp->vm_outSize = *outSize ? *outSize : inSize;
	vmp->vm_opcode = ((struct inputArgs *)buffer)->opcode;
	vmp->vm_unique = ++vcommp->vc_seq;
        vmp->vm_sleep = NULL;
	
	/* Fill in the common input args. */
	((struct inputArgs *)buffer)->unique = vmp->vm_unique;

	/* Append msg to pending queue and poke Venus. */

	INSQUE(vmp->vm_chain, vcommp->vc_pending);
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
	 * was interrupted by a venus shutdown (vcclose), return
	 * ENODEV.  */

	/* Ignore return, We have to check anyway */


	interruptible_sleep_on(&vmp->vm_sleep);
	CDEBUG(D_UPCALL, 
	       "..process %d woken up by Venus for vmp at 0x%x, data at %x\n", 
	       current->pid, (int)vmp, (int)vmp->vm_data);
	if (vcomm_open(vcommp)) {      /* i.e. Venus is still alive */
	    /* Op went through, interrupt or not... */
	    if (vmp->vm_flags & VM_WRITE) {
		error = 0;
		out = (struct outputArgs *)vmp->vm_data;
		error = out->result;
		CDEBUG(D_UPCALL, 
		       "upcall: (u,o,r) (%ld, %ld, %ld) out at %x\n", 
		       out->unique, out->opcode, out->result, (int)out);
		*outSize = vmp->vm_outSize;
		goto exit;
	    } 
	    if (!(vmp->vm_flags & VM_READ)) { 
		/* Interrupted before venus read it. */
		CDEBUG(D_UPCALL, 
		       "Interrupted before read:(op,un) (%d.%d), flags = %x\n",
		       vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags);
		REMQUE(vmp->vm_chain);
		error = ERESTARTSYS;
		goto exit;
	    } 
	    if ( vmp->vm_flags & VM_READ) { 	
		/* interrupted after Venus did its read, send signal */
		struct inputArgs *dog;
		struct vmsg *svmp;
		
		CDEBUG(D_UPCALL, 
		       "Sending Venus a signal: op = %d.%d, flags = %x\n",
		       vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags);
		
		REMQUE(vmp->vm_chain);
		error = ERESTARTSYS;
		
		CODA_ALLOC(svmp, struct vmsg *, sizeof (struct vmsg));
		CODA_ALLOC((svmp->vm_data), char *, VC_IN_NO_DATA);

		CDEBUG(D_UPCALL, 
		       "coda_upcall: enqueing signal msg (%d, %d)\n",
		       svmp->vm_opcode, svmp->vm_unique);
		dog = (struct inputArgs *)svmp->vm_data;
		dog->opcode = CFS_SIGNAL;
		dog->unique = vmp->vm_unique;
		
		svmp->vm_flags = 0;
		svmp->vm_opcode = dog->opcode;
		svmp->vm_unique = dog->unique;
		svmp->vm_inSize = VC_IN_NO_DATA;
		svmp->vm_outSize = VC_IN_NO_DATA;
		
		/* insert at head of queue! */
		INSQUE(svmp->vm_chain, vcommp->vc_pending);
		wake_up_interruptible(&vcommp->vc_waitq);
	    }
	} else {	/* If venus died i.e. !VC_OPEN(vcommp) */
	        printk("coda_upcall: Venus dead upon (op,un) (%d.%d) flags %d\n",
		       vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags);
		
		/* if (! (vmp->vm_flags & VM_WRITE) ) */
		error = ENODEV;
	}

 exit:
	CODA_FREE(vmp, sizeof(struct vmsg));
	if (error) 
	        badclstats();
	return error;
}




/* 
 * There are 7 cases where cache invalidations occur.  The semantics
 *  of each is listed here:
 *
 * CFS_FLUSH     -- flush all entries from the name cache and the cnode cache.
 * CFS_PURGEUSER -- flush all entries from the name cache for a specific user
 *                  This call is a result of token expiration.
 *                  Linux does a cfsnc_flush since cred's are not maintained.
 *
 * The next arise as the result of callbacks on a file or directory.
 * CFS_ZAPDIR    -- flush the attributes for the dir from its cnode.
 *                  Zap all children of this directory from the namecache.
 * CFS_ZAPFILE   -- flush the cached attributes for a file.
 * CFS_ZAPVNODE  -- in linux the same as zap file (no creds).
 *
 * The next is a result of Venus detecting an inconsistent file.
 * CFS_PURGEFID  -- flush the attribute for the file
 *                  If it is a dir (odd vnode), purge its 
 *                  children from the namecache
 *                  remove the file from the namecache.
 *
 * The last  allows Venus to replace local fids with global ones
 * during reintegration.
 *
 * CFS_REPLACE -- replace one ViceFid with another throughout the name cache */

int coda_downcall(int opcode, struct outputArgs * out)
{

    /* Handle invalidate requests. */
    switch (opcode) {
      case CFS_FLUSH : {
	      clstats(CFS_FLUSH);
	      cfsnc_flush();
	      return(0);
      }
      case CFS_PURGEUSER : {
	      clstats(CFS_PURGEUSER);
	      cfsnc_flush();
	      return(0);
      }
      case CFS_ZAPDIR : {
	      ViceFid *fid = &out->d.cfs_zapdir.CodaFid;
	      clstats(CFS_ZAPDIR);
	      cfsnc_zapfid(fid);
	      cfsnc_zapParentfid(fid);     
	      CDEBUG(D_UPCALL, "zapdir: fid = (%lx.%lx.%lx), \n",fid->Volume, 
					  fid->Vnode, 
					  fid->Unique);
	      return(0);
      }
      case CFS_ZAPVNODE : {
              clstats(CFS_ZAPVNODE);
	      cfsnc_zapfid(&out->d.cfs_zapvnode.VFid);
	      return(0);
      }	
      case CFS_ZAPFILE : {
              clstats(CFS_ZAPFILE);
	      cfsnc_zapfid(&out->d.cfs_zapfile.CodaFid);
	      return 0;
      }
      case CFS_PURGEFID : {
	      ViceFid *fid = &out->d.cfs_purgefid.CodaFid;
              clstats(CFS_PURGEFID);
	      cfsnc_zapfid(fid);
	      cfsnc_zapParentfid(fid);     
	      CDEBUG(D_UPCALL, "purgefid: fid = (%lx.%lx.%lx)\n", 
                                            fid->Volume, fid->Vnode,
                                            fid->Unique);
	      return 0;
      }
      case CFS_REPLACE : {
              clstats(CFS_REPLACE);
	      cfsnc_replace(&out->d.cfs_replace.OldFid, 
			    &out->d.cfs_replace.NewFid);
	      return (0);
      }			   
    }
      return 0;
}


