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





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <struct.h>
#include <stdio.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <util.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include "vsg.h"
#include "coda_assert.h"

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
    sprintf(buf, "VSGAddr = 0x%lx ", VSGaddr);
    write(fd, buf, strlen(buf));
    for (int i = 0; i < nhosts; i++)
	if (Hosts[i]){
	    sprintf(buf, "0x%lx ", Hosts[i]);
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
    while ((ol = next())) {
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
    while ((l = next())) {
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
    while ((l = next())) {
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
	int i = sscanf(string, "%lx %s %s %s %s %s %s %s %s\n",
		       &vsgaddr, Host[0], Host[1], 
		       Host[2], Host[3], Host[4], 
		       Host[5], Host[6], Host[7]);

	/* skip lines that cannot be used to construct a vsgent */
	if (i < 2) continue;

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
    while ((l = next())) {
	v = strbase(vsgent, l, vsgtabhandle);
        v->print();
    }
}

void CheckVSGDB() {
    ClearVSGDB();
    InitVSGDB();
}

