/*
 * Inode operations for Coda filesystem
 * Original version: (C) 1996 P. Braam and M. Callahan
 * Rewritten for Linux 2.1. (C) 1997 Carnegie Mellon University
 * 
 * Carnegie Mellon encourages users to contribute improvements to
 * the Coda project. Contact Peter Braam (coda@cs.cmu.edu).
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/string.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_psdev.h>
#include <linux/coda_cnode.h>
#include <linux/coda_namecache.h>

/* initialize the debugging variables */
int coda_debug =815;
int coda_print_entry = 1; 
int coda_access_cache = 1;

/* caller must allocate 36 byte string ! */
char * coda_f2s(ViceFid *f, char *s)
{
	if ( f ) {
		sprintf(s, "(%-#10lx,%-#10lx,%-#10lx)", 
			 f->Volume, f->Vnode, f->Unique);
	}
	return s;
}

int coda_isroot(struct inode *i)
{
    if ( i->i_sb->s_root->d_inode == i ) {
	return 1;
    } else {
	return 0;
    }
}
	
void coda_load_creds(struct CodaCred *cred)
{
        int i;

        cred->cr_uid = (vuid_t) current->uid;
        cred->cr_euid = (vuid_t) current->euid;
        cred->cr_suid = (vuid_t) current->suid;
        cred->cr_fsuid = (vuid_t) current->fsuid;

        cred->cr_gid = (vgid_t) current->gid;
        cred->cr_egid = (vgid_t) current->egid;
        cred->cr_sgid = (vgid_t) current->sgid;
        cred->cr_fsgid = (vgid_t) current->fsgid;

        for ( i = 0 ; i < NGROUPS ; ++i ) {
                cred->cr_groups[i] = (vgid_t) current->groups[i];
        }

}

