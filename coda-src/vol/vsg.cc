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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/vsg.cc,v 4.5 98/11/02 16:46:57 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <struct.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_assert.h"
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <util.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include "vsg.h"

/* vsg.c 
 * Implementation of vsg - host ip address mapping
 * Created Puneet Kumar, Jan 1992
 */

/* Declaration of static class variables in vsg.h */
ohashtab *vsgent::hosttab = 0;
ohashtab *vsgent::vsgaddrtab = 0;
int vsgent::nvsgs = 0;

int cmpHost(long *a, long *b) {
    if (*a < *b) return(-1);
    else if (*a > *b) return(1);
    else return(0);
}

vsgent::vsgent(unsigned long vsgaddr, 
	       unsigned long *hosts,
	       int nh) {
    VSGaddr = vsgaddr;
    nhosts = nh;
    /* put hosts in canonical order */
    qsort((char *)hosts, nh, sizeof(long), 
	(int (*)(const void *, const void *))cmpHost);
	  
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (i < nh)
	    Hosts[i] = hosts[i];
	else 
	    Hosts[i] = 0;
    /* the olinks are automatically initialized */
}

vsgent::~vsgent(){
}

void vsgent::print(){
    print(stdout);
}
void vsgent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}
void vsgent::print(int fd) {
    char buf[80];
/*    sprintf(buf, "%#08x : VSGAddr = 0x%x\n", (long)this, VSGaddr); */
    sprintf(buf, "VSGAddr = 0x%x ", VSGaddr);
    write(fd, buf, strlen(buf));
    for (int i = 0; i < nhosts; i++)
	if (Hosts[i]){
	    sprintf(buf, "0x%x ", Hosts[i]);
	    write(fd, buf, strlen(buf));
	}
    sprintf(buf, "\n");
    write(fd, buf, strlen(buf));
}

int VSGAddHashfn(long *address) {
    return((int )address);
}

/* assumes there are max VSG_MEMBERS hosts and unused are marked with zero */
int HostTabHashfn(long *hosts) {
    long retval = 0;
    for (int i = 0; i < VSG_MEMBERS; i++)
	retval += hosts[i];
    return((int) retval);
}

/* returns 1 if it finds a VSG entry corresponding to vsgaddr */
int GetHosts(unsigned long vsgaddr, unsigned long *Haddr, int *nh) {
    ohashtab_iterator	next(*vsgent::vsgaddrtab, (void *)vsgaddr);
    vsgent *rv;
    olink  *ol;

    *nh = 0;
    while (ol = next()) {
	rv = strbase(vsgent, ol, vsgtabhandle);
	LogMsg(10, VolDebugLevel, stdout, "GetHosts: comparing one more entry ");
	if (rv->VSGaddr == vsgaddr){
	    *nh = rv->nhosts;
	    for (int i = 0; i < rv->nhosts; i++)
		Haddr[i] = rv->Hosts[i];
	    return 1;
	}
    }
    return(0);
}

/* returns 1 if everything goes fine */
int AddMember(vsgent *v) {
    unsigned long Haddr[VSG_MEMBERS];
    int nh;

    if (GetHosts(v->VSGaddr, Haddr, &nh)) return 0;
    vsgent::vsgaddrtab->insert((void *)v->VSGaddr, &(v->vsgtabhandle));
    vsgent::hosttab->insert(v->Hosts, &(v->htabhandle));
    vsgent::nvsgs ++;
    return 1;
}

unsigned long GetVSGAddress(unsigned long *hosts, int nh) {
    
    /* first make the host list canonical */
    qsort((char *)hosts, nh, sizeof(long), 
	(int (*)(const void *, const void *))cmpHost);
    
    long CanHosts[VSG_MEMBERS];
    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (i < nh) CanHosts[i] = hosts[i];
	else CanHosts[i] = 0;
    }
    ohashtab_iterator next(*vsgent::hosttab, CanHosts);
    olink *l;
    while (l = next()) {
	vsgent *v = strbase(vsgent, l, htabhandle);
	LogMsg(10, VolDebugLevel, stdout, "GetVSGAddress: Comparing one more entry");
	if (!bcmp((const void *)CanHosts,  (const void *)v->Hosts, sizeof(long) * VSG_MEMBERS) &&
	    nh == v->nhosts)
	    return(v->VSGaddr);
    }
    
    /* entry not found */
    return(0);
}


void ClearVSGDB() {
    ohashtab_iterator next(*vsgent::vsgaddrtab, (void *)-1);
    
    vsgent *v;
    olink *l;
    while (l = next()) {
	v = strbase(vsgent, l, vsgtabhandle);
        vsgent::vsgaddrtab->remove((void *)v->VSGaddr, l);
	delete v;
    }
    vsgent::hosttab = vsgent::vsgaddrtab = NULL;
    vsgent::nvsgs = 0;
}
/* check this */
void InitVSGDB() {
    char string[1024];
    unsigned long vsgaddr;
    struct hostent *he;
    unsigned long Haddr[VSG_MEMBERS];
    char Host[VSG_MEMBERS][256];

typedef int (*XXX)(void *);

    vsgent::nvsgs = 0;
    vsgent::vsgaddrtab = new ohashtab(VSGHASHTBLSIZE, (XXX)VSGAddHashfn);
    vsgent::hosttab = new ohashtab(VSGHASHTBLSIZE, (XXX)HostTabHashfn);

    FILE *fp = fopen(VSGPATH, "r");
    CODA_ASSERT(fp != NULL);

    while(1){
	if (fgets(string, 1024, fp) == NULL) break;
	int i = sscanf(string, "%x %s %s %s %s %s %s %s %s\n",
		       &vsgaddr, Host[0], Host[1], 
		       Host[2], Host[3], Host[4], 
		       Host[5], Host[6], Host[7]);
	if (i == 0) break;
	/* number of hosts = i - 1 */
	for (int j = 0; j < i - 1; j++){
	    he = gethostbyname(Host[j]);
	    Haddr[j] = ntohl(*(unsigned long *)(he->h_addr));
	}
	vsgent *newve = new vsgent(vsgaddr, Haddr, i-1);
	if (!AddMember(newve)){
	    newve->print();
	    delete newve;
	    CODA_ASSERT(0);
	}
    }
    fclose(fp);
}

void PrintVSGDB() {
    ohashtab_iterator next(*vsgent::vsgaddrtab, (void *)-1);
    
    vsgent *v;
    olink *l;
    while (l = next()) {
	v = strbase(vsgent, l, vsgtabhandle);
        v->print();
    }
}

void CheckVSGDB() {
    ClearVSGDB();
    InitVSGDB();
}

#if 0
/* this is the old vsg.c file 
 * It used to implement the multi cast vsg groups
 * Until somebody who understands this stuff in the kernel
 * we are deleting it.

#include "vsg.h"
#include "errno.h"
#include <stdlib.h>
#include <stdio.h>
#include <sysent.h>
#include "file.h"

extern int joingroup(unsigned long, unsigned long, unsigned long, unsigned int, char *);
extern int leavegroup(int, unsigned long, unsigned long);


vsgtab JoinedVSGs("JoinedVSGs");


vsgtab::vsgtab(char *n) {
    name = new char[strlen(n) + 1];
    strcpy(name, n);
}


vsgtab::vsgtab(vsgtab& vsgt) {
    abort();
}


vsgtab::operator=(vsgtab& vsgt) {
    abort();
}


vsgtab::~vsgtab() {
    vsgtab_iterator next(*this);
    vsgent *vsge;
    while (vsge = next()) remove(vsge);
    delete name;
}


void vsgtab::add(vsgent *vsge) {
    olist::insert(vsge);
}


void vsgtab::remove(vsgent *vsge) {
    olist::remove(vsge);
/*
    LogMsg(1, VolDebugLevel, stdout, "vsgtab::remove: leaving VSG %x", vsge->addr);
    if (leavegroup(vsge->fd, 0, 0) < 0)
	LogMsg(0, VolDebugLevel, stdout, "vsgtab::remove: leavegroup failed (%d)", errno);
*/
    delete vsge;
}


vsgent *vsgtab::find(unsigned long vsgaddr) {
    vsgtab_iterator next(*this);
    vsgent *vsge;
    while (vsge = next())
	if (vsge->addr == vsgaddr) return(vsge);

    return(0);
}


void vsgtab::join(unsigned long vsgaddr)  {
    vsgent *vsge;
    if (vsge = find(vsgaddr)) {
	vsge->Mark();
	return;
    }

/*
    LogMsg(1, VolDebugLevel, stdout, "vsgtab::join: joining VSG %x", vsgaddr);
    int fd = joingroup(vsgaddr, 0, 0, 0, 0);
    if (fd < 0)
	LogMsg(0, VolDebugLevel, stdout, "vsgtab::add: joingroup failed (%d)", errno);
    else
	add(new vsgent(vsgaddr, fd));
*/
}


void vsgtab::UnMark() {
    vsgtab_iterator next(*this);
    vsgent *vsge;
    while (vsge = next()) vsge->UnMark();
}


void vsgtab::GarbageCollect() {
    vsgtab_iterator next(*this);
    vsgent *vsge;
    while (vsge = next())
	if (!(vsge->IsMarked())) remove(vsge);
}


void vsgtab::print() {
    print(stdout);
}


void vsgtab::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void vsgtab::print(int afd) {
    char buf[40];
    sprintf(buf, "%#08x : %-16s\n", (long)this, name);
    write(afd, buf, strlen(buf));

    vsgtab_iterator next(*this);
    vsgent *vsge;
    while (vsge = next()) vsge->print(afd);
}


vsgtab_iterator::vsgtab_iterator(vsgtab& vtab) : (vtab) {
}


vsgent *vsgtab_iterator::operator()() {
    return((vsgent *)olist_iterator::operator()());
}


vsgent::vsgent(unsigned long vsgaddr, int fdes) {
    addr = vsgaddr;
    fd = fdes;
    mark = 1;
}


void vsgent::Mark() {
    mark = 1;
}


void vsgent::UnMark() {
    mark = 0;
}


int vsgent::IsMarked() {
    return(mark);
}


void vsgent::print() {
    print(stdout);
}


void vsgent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void vsgent::print(int afd) {
    char buf[80];
    sprintf(buf, "%#08x : addr = %x, fd = %d, mark = %d\n",
	     (long)this, addr, fd, mark);
    write(afd, buf, strlen(buf));
}


#endif 0

