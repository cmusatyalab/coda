#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/Attic/vol-peekpoke.cc,v 4.1 1997/01/08 21:52:32 rvb Exp $";
#endif /*_BLURB_*/





/*****************************************
 * vol-peekpoke.c                           *
 * Get or set the maximum used volume id *
 *****************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>

#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */

#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif /* __NetBSD__ || LINUX */

#include <struct.h>

#include <ctype.h>
#include <nlist.h>
/* nlist.h defines this function but it isnt getting included because it is
   guarded by an ifdef of CMU which isnt getting defined.  XXXXX pkumar 6/13/95 */ 
extern int nlist(const char*, struct nlist[]);
    
#include <string.h>
#include <sys/param.h>
#ifdef	__MACH__
#include <mach.h>
#endif
#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include "cvnode.h"
#include "volume.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"

static	char	*srvname;

#ifdef	__MACH__
char *strdup(const char *s)
#else
char *strdup(char *s)
#endif
{
        char    *t;
        if ((t = (char *) malloc(((unsigned int) strlen(s)) + 1)) == NULL) {
		LogMsg(0, VolDebugLevel, stdout, "strdup failed: Out of memory\n");
                printf("strdup failed: Out of memory\n");
                return(NULL);
        }
        return(strcpy(t, s));
}

void setmyname(char *s)
{
	char	buf[MAXPATHLEN];
	if (*s == '/')
		srvname = strdup(s);
	else if (getwd(buf) != NULL)
		srvname = strdup(strcat(strcat(buf, "/"), s));
	else	printf("%s: unable to find the current directory\n", s);
}

#ifdef __MACH__
static
long checkaddress(vm_address_t addr, vm_size_t sz, vm_prot_t perm)
{
	vm_address_t	address = addr;
	vm_size_t       size;
	vm_prot_t       protection;
	vm_prot_t       max_protection;
	vm_inherit_t    inheritance;
	boolean_t       shared;
	port_t          object_name;
	vm_offset_t     offset;

	while(vm_region(task_self(), &address, &size,
			&protection, &max_protection,
			&inheritance, &shared,
			&object_name, &offset) == KERN_SUCCESS) {
		if (address > addr) return(3-10040L);
		if ((protection & perm) != perm) return(4-10040L);
		if (address + size >= addr + sz) return(RPC2_SUCCESS);
		sz = (addr + sz) - (address + size);
		addr = address += size;
		if (size == 0) break;
	}
	return(3-10040L);
}

static
long okaddr(vm_address_t *pm, RPC2_String s, vm_size_t sz, vm_prot_t perm)
{
	while(isspace(*s)) s++;
	if (*s == '*') {
		long	status;
		if ((status = okaddr(pm, ++s, sizeof(*pm), VM_PROT_READ))
		    != RPC2_SUCCESS)
			return(status);
		if (((int) *pm) % sizeof(vm_address_t) != 0)
			return(5-10040L);
		else	*pm = *((vm_address_t *) *pm);
	}
	else if (isdigit(*s)) {
		unsigned long	x;
		if (*s != '0')
			(void) sscanf((char *) s, "%uld", &x);
		else if (*++s != 'x')
			(void) sscanf((char *) s, "%lo", &x);
		else	(void) sscanf((char *) ++s, "%lx", &x);
		*pm = (vm_address_t) x;
	}
	else {
		/* symbol - use nlist */
		struct nlist nl[2];
		nl[1].n_name = NULL; /* or should it be "" ? */
		nl[0].n_name = (char *) s;
		if (srvname == NULL) return(0-10040L);
		switch(nlist(srvname, nl)) {
		default:return(1-10040L);
		case 1:	return(2-10040L);
		case 0:	*pm = (vm_address_t) nl[0].n_value;
		}
	}
	LogMsg(0, VolDebugLevel, stdout, "okaddr using address 0x%lx\n", (long) *pm);
	return(checkaddress(*pm, sz, perm));
}
#else /* MACH */

/* Not ported yet to Linux or NetBSD; die horribly.... */

#define vm_address_t  unsigned int
#define vm_size_t     unsigned int
#define vm_prot_t     int
#define VM_PROT_READ 1
#define VM_PROT_WRITE 2
static
long checkaddress(vm_address_t addr, vm_size_t sz, vm_prot_t perm)
{
  LogMsg(0, VolDebugLevel, stdout, "Arrrghhh....checkaddress() not ported yet\n");
  assert(0);
}

static
long okaddr(vm_address_t *pm, RPC2_String s, vm_size_t sz, vm_prot_t perm)
{
  LogMsg(0, VolDebugLevel, stdout, "Arrrghhh....okaddress() not ported yet\n");
  assert(0);
}
#endif /* __MACH__ */

/*
  BEGIN_HTML
  <a name="S_VolPeekInt"><strong>Service the peek request</strong></a> 
  END_HTML
*/
long S_VolPeekInt(RPC2_Handle cid, RPC2_String address, RPC2_Integer *pvalue)
{
	long	status;
	vm_address_t	x;
	if ((status = okaddr(&x, address, sizeof(RPC2_Integer), VM_PROT_READ))
	    == RPC2_SUCCESS)
		if (x % sizeof(RPC2_Integer) != 0)
			return(5-10040L);
		else	*pvalue = *((RPC2_Integer *) x);
	LogMsg(0, VolDebugLevel, stdout, "S_:VolPeekInt returning %ld\n", status);
	return(status);
}

/*
  BEGIN_HTML
  <a name="S_VolPokeInt"><strong>Service the poke request</strong></a> 
  END_HTML
*/
long S_VolPokeInt(RPC2_Handle cid, RPC2_String address, RPC2_Integer value)
{
	long	status;
	vm_address_t	x;
	if ((status = okaddr(&x, address, sizeof(RPC2_Integer), VM_PROT_WRITE))
	    == RPC2_SUCCESS)
		if (x % sizeof(RPC2_Integer) != 0)
			return(5-10040L);
		else	*((RPC2_Integer *) x) = value;
	LogMsg(0, VolDebugLevel, stdout, "S_:VolPokeInt returning %ld\n", status);
	return(status);
}

/*
  BEGIN_HTML
  <a name="S_VolPeekMem"><strong>Service the peek request for arbitrary memory</strong></a> 
  END_HTML
*/
long S_VolPeekMem(RPC2_Handle cid, RPC2_String address, RPC2_BoundedBS *buf)
{
	long	status;
	vm_address_t	x;
	buf->SeqLen = buf->MaxSeqLen;
	if ((status = okaddr(&x, address, (vm_size_t) buf->SeqLen, VM_PROT_READ))
	    == RPC2_SUCCESS)
		bcopy((char *) x, (char *) buf->SeqBody, (int) buf->SeqLen);
	LogMsg(0, VolDebugLevel, stdout, "S_:VolPeekMem returning %ld\n", status);
	return(status);
}

/*
  BEGIN_HTML
  <a name="S_VolPokeMem"><strong>Service the poke request for arbitrary memory</strong></a> 
  END_HTML
*/
long S_VolPokeMem(RPC2_Handle cid, RPC2_String address, RPC2_CountedBS *buf)
{
	long	status;
	vm_address_t	x;
	if ((status = okaddr(&x, address, (vm_size_t) buf->SeqLen, VM_PROT_WRITE))
	    == RPC2_SUCCESS)
		bcopy((char *) buf->SeqBody, (char *) x, (int) buf->SeqLen);
	LogMsg(0, VolDebugLevel, stdout, "S_:VolPokeMem returning %ld\n", status);
	return(status);
}

