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
#include <linux/string.h>

#include <linux/coda.h>
#include <cfs_linux.h>
#include <psdev.h>
#include <cnode.h>
#include <namecache.h>

/* initialize the debugging variables */
int coda_debug = 0;
int coda_print_entry = 0; 
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


