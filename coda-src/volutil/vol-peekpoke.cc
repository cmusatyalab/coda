/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/





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

#include <unistd.h>
#include <stdlib.h>

#include <struct.h>

#include <ctype.h>
#ifdef __MACH__
#include <nlist.h>
/* nlist.h defines this function but it isnt getting included because it is
   guarded by an ifdef of CMU which isnt getting defined.  XXXXX pkumar 6/13/95 */ 
extern int nlist(const char*, struct nlist[]);
#endif
    
#include <string.h>
#include <sys/param.h>
#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <volutil.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "cvnode.h"
#include "volume.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"

static	char	*srvname;


void setmyname(char *s)
{
	char	buf[MAXPATHLEN];
	if (*s == '/')
		srvname = strdup(s);
	else if (getcwd(buf, MAXPATHLEN) != NULL)
		srvname = strdup(strcat(strcat(buf, "/"), s));
	else	printf("%s: unable to find the current directory\n", s);
}

#ifdef __MACH__

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

/* Not ported yet to Linux or BSD44; die horribly.... */

#define vm_address_t  unsigned int
#define vm_size_t     unsigned int
#define vm_prot_t     int
#define VM_PROT_READ 1
#define VM_PROT_WRITE 2

static
long okaddr(vm_address_t *pm, RPC2_String s, vm_size_t sz, vm_prot_t perm)
{
  LogMsg(0, VolDebugLevel, stdout, "Arrrghhh....okaddress() not ported yet\n");
  CODA_ASSERT(0);
  return 0;
}
#endif
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

