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

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <struct.h>
#include <codadir.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <srv.h>
#include <util.h>
#include "vsg.h"
#include "vrdb.h"

const char *VRDB_PATH = "/vice/db/VRDB";
const char *VRDB_TEMP = "/vice/db/VRDB.new";
vrtab VRDB("VRDB");
int canonicalize = 0;

/* hash function for the vrdb hash table - just return the id */
int vrtabHashfn(void *id) {
    return((int)id);
}

int nametabHashfn(void *p) {
    int length = strlen((char *)p);
    int hash = 0;
    for (int i = 0; i < length; i++)
	hash += (int)(((char *)p)[i]);
    return(hash);
}

vrtab::vrtab(char *n) : ohashtab(VRTABHASHSIZE, vrtabHashfn), 
    namehtb(VRTABHASHSIZE, nametabHashfn) {
	
	name = new char[strlen(n) + 1];
	strcpy(name, n);
}


vrtab::~vrtab() {
    delete name;
}


void vrtab::add(vrent *vre) {
  ohashtab::insert((void *)vre->volnum, vre);
  namehtb.insert((void *)vre->key, &vre->namehtblink);

#ifdef MULTICAST
    if (vre->index(ThisHostAddr) != -1) 
	JoinedVSGs.join(vre->addr);
#endif MULTICAST
}


void vrtab::remove(vrent *vre) {
    ohashtab::remove((void *)vre->volnum, vre);
    namehtb.remove(vre->key, &vre->namehtblink);
      
    delete vre;
}


vrent *vrtab::find(VolumeId grpvolnum) {
    ohashtab_iterator next(*this, (void *)grpvolnum);
    vrent *vre;

    while ((vre = (vrent *)next()))
	if (vre->volnum == grpvolnum) return(vre);

    return(0);
}


vrent *vrtab::find(char *grpname) {
    ohashtab_iterator next(namehtb, (void *)grpname);
    vrent *vre;
    olink *l;
    
    while ((l = next())) {
	vre = strbase(vrent, l, namehtblink);
	if (STREQ(vre->key, grpname)) return(vre);
    }
    return(0);
}


vrent *vrtab::ReverseFind(VolumeId rwvolnum) {
    if (rwvolnum == 0) return(0);

    ohashtab_iterator next(*this, (void *) -1);
    vrent *vre;

    while ((vre = (vrent *)next()))
	for (int i = 0; i < vre->nServers; i++)
	    if (vre->ServerVolnum[i] == rwvolnum) return(vre);

    return(0);
}


void vrtab::clear() {
    ohashtab_iterator next(*this, (void *)-1);
    vrent *vre;

    while ((vre = (vrent *)next())) {
	ohashtab::remove((void *)vre->volnum, vre);
	namehtb.remove(vre->key, &(vre->namehtblink));
	delete vre;
    }
}


void vrtab::print() {
    print(stdout);
}


void vrtab::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void vrtab::print(int afd) {
    char buf[40];
    sprintf(buf, "%#08lx : %-16s\n", (long)this, name);
    write(afd, buf, strlen(buf));

    ohashtab_iterator next(*this, (void *)-1);
    vrent *vre;
    while ((vre = (vrent *)next())) 
	    vre->print(afd);
}


void CheckVRDB() {
    int VRDB_fd = open(VRDB_PATH, O_RDONLY, 0);
    if (VRDB_fd < 0) {
	LogMsg(0, VolDebugLevel, stdout, "CheckVRDB: could not open VRDB");
	return;
    }

    VRDB.clear();
#ifdef MULTICAST
    JoinedVSGs.UnMark();
#endif MULTICAST

    /* Build the new VRDB. */
    vrent vre;
    while (read(VRDB_fd, &vre, sizeof(vrent)) == sizeof(vrent)) {
        vre.ntoh();
	VRDB.add(new vrent(vre));
    }

    close(VRDB_fd);

#ifdef MULTICAST
    JoinedVSGs.GarbageCollect();
#endif MULTICAST

}


int XlateVid(VolumeId *vidp) {
    int count, pos;
    return(XlateVid(vidp, &count, &pos));
}


int XlateVid(VolumeId *vidp, int *count, int *pos) 
{
    vrent *vre = VRDB.find(*vidp);
    if (!vre) return(0);

    int ix;

    if ((ix = vre->index(ThisHostAddr)) == -1) return(0);


    *vidp = vre->ServerVolnum[ix];
    *count = vre->nServers;
    *pos = ix;
    return(1);
}


int ReverseXlateVid(VolumeId *vidp) {
    vrent *vre = VRDB.ReverseFind(*vidp);
    if (!vre) return(0);

    *vidp = vre->volnum;
    return(1);
}

unsigned long XlateVidToVSG(VolumeId vid) {
    vrent *vre = VRDB.find(vid);
    if (!vre) return(0);

    return(vre->addr);
}


vrent::vrent() {
    bzero(key, sizeof(key));
    volnum = 0;
    nServers = 0;
    bzero((void *)ServerVolnum, sizeof(ServerVolnum));
    addr = 0;
}


vrent::vrent(vrent& vre) {
    strcpy(key, vre.key);
    volnum = vre.volnum;
    nServers = vre.nServers;
    bcopy((const void *)vre.ServerVolnum, (void *) ServerVolnum, sizeof(ServerVolnum));
    addr = vre.addr;
}


int vrent::operator=(vrent& vre) {
    abort();
    return(0);	/* keep C++ happy */
}


vrent::~vrent() {
}


void vrent::GetHosts(unsigned long *Hosts) {
    bzero((void *)Hosts, VSG_MEMBERS * sizeof(unsigned long));

    for (int i = 0; i < nServers; i++)
	Hosts[i] = VolToHostAddr(ServerVolnum[i]);

}


int vrent::index(unsigned long hostaddr) {
    for (int i = 0; i < nServers; i++)
	if (hostaddr == VolToHostAddr(ServerVolnum[i])) return(i);

    return(-1);
}


void vrent::HostListToVV(unsigned long *Hosts, vv_t *VV) {
    bzero((void *)VV, sizeof(vv_t));
    for (int i = 0; i < VSG_MEMBERS; i++)
	if (Hosts[i]) {
	    int ix = index(Hosts[i]);
	    CODA_ASSERT(ix != -1);
	    (&(VV->Versions.Site0))[ix] = 1;
	}
}


int vrent::GetVolumeInfo(VolumeInfo *Info) {
    int i;

    if (nServers <= 0 || nServers > VSG_MEMBERS) {
	LogMsg(0, VolDebugLevel, stdout, "vrent::GetVolumeInfo: bogus nServers (%d)", nServers);
	return(VNOVOL);
    }

    bzero((void *)Info, sizeof(VolumeInfo));
    Info->Vid = volnum;
    Info->Type = REPVOL;
    (&Info->Type0)[REPVOL] = volnum;
    Info->ServerCount = nServers;
    for (i = 0; i < nServers; i++) {
	unsigned long hostaddr = VolToHostAddr(ServerVolnum[i]);
	if (hostaddr == 0) {
	    LogMsg(0, VolDebugLevel, stdout, "vrent::GetVolumeInfo: no hostaddr for volume (%u)",
		    ServerVolnum[i]);
	    return(VNOVOL);
	}
	(&Info->Server0)[i] = hostaddr;
    }
    if (canonicalize) {
	long tmpvsgaddr = GetVSGAddress(&(Info->Server0), Info->ServerCount);
	CODA_ASSERT(tmpvsgaddr == (long)addr);
    }
    Info->VSGAddr = addr;
    for (i = 0; i < nServers; i++)
	(&Info->RepVolMap.Volume0)[i] = ServerVolnum[i];

    return(0);
}

// keep the volume ids in an order that corresponds to the 
// canonical (sorted) order of the hosts where they belong
void vrent::Canonicalize() {
    VolumeId CopySrvVolNum[VSG_MEMBERS];
    unsigned long VolHostAddr[VSG_MEMBERS];
    unsigned long CopyVolHostAddr[VSG_MEMBERS];
    int i, j;
    for (i = 0; i < VSG_MEMBERS; i++) {
	VolHostAddr[i] = 0;
	CopyVolHostAddr[i] = 0;
	CopySrvVolNum[i] = ServerVolnum[i];
    }
    // get the host addresses for each volume id
    for (i = 0; i < nServers; i++) {
	VolHostAddr[i] = VolToHostAddr(ServerVolnum[i]);
	CopyVolHostAddr[i] = VolToHostAddr(ServerVolnum[i]);
    }
    extern int cmpHost(long *, long *);
    qsort((char *)VolHostAddr, nServers, sizeof(long), 
	(int (*)(const void *, const void *))cmpHost);
    
    // now compute the volume id array corresponding to this sorted
    // order using the copy of the VolHostAddr array
    for (i = 0; i < nServers; i++) {
	// find the hosts real position in the sorted array
	for (j = 0; j < nServers; j++) 
	    if (CopyVolHostAddr[i] == VolHostAddr[j]) break;
	CODA_ASSERT(j < nServers);
	ServerVolnum[j] = CopySrvVolNum[i];
    }
    char buf[512], *c;
    c = buf;
    for (i = 0; i < VSG_MEMBERS; i++) {
	sprintf(c, "0x%lx ", ServerVolnum[i]);
	c += strlen(c);
    }
    LogMsg(10, VolDebugLevel, stdout, 
	   "vrent:: canonicalize volumeids are : %s\n", buf);
}
void vrent::hton() {
    /* we won't translate the key on the hopes that strings take care of themselves */
    this->volnum = htonl(this->volnum);
    /* Don't need to translate nServers, it is a byte */

    for (int i = 0; i < VSG_MEMBERS; i++)
      this->ServerVolnum[i] = htonl(this->ServerVolnum[i]);

    this->addr = htonl(this->addr);
}

void vrent::ntoh() {
    /* we won't translate the key on the hopes that strings take care of themselves */
    this->volnum = ntohl(this->volnum);
    /* Don't need to translate nServers, it is a byte */

    for (int i = 0; i < VSG_MEMBERS; i++)
      this->ServerVolnum[i] = ntohl(this->ServerVolnum[i]);

    this->addr = ntohl(this->addr);
}

void vrent::print() {
    print(stdout);
}


void vrent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void vrent::print(int afd) {
    char buf[512];
    sprintf(buf, "%p : %s : 0x%lx, %d, (x.x.x.x.x.x.x.x), 0x%lx\n",
	    this, key, volnum, nServers, addr);
    write(afd, buf, strlen(buf));

}
