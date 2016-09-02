/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <struct.h>
#include <codadir.h>

#ifdef __cplusplus
}
#endif

#include <volume.h>
#include <srv.h>
#include <util.h>
#include "vrdb.h"

vrtab VRDB("VRDB");

/* hash function for the vrdb hash table - just return the id */
static intptr_t vrtabHashfn(void *id)
{
    return (intptr_t)id;
}

static intptr_t nametabHashfn(void *p)
{
    char *name = (char *)p;
    int length = strlen(name);
    int hash = 0;
    for (int i = 0; i < length; i++)
	hash += (int)(name[i]);
    return(hash);
}

vrtab::vrtab(const char *n) : ohashtab(VRTABHASHSIZE, vrtabHashfn),
    namehtb(VRTABHASHSIZE, nametabHashfn)
{
    name = strdup(n);
}


vrtab::~vrtab() {
    clear();
    free(name);
}


void vrtab::add(vrent *vre) {
  ohashtab::insert((void *)(intptr_t)vre->volnum, vre);
  namehtb.insert(vre->key, &vre->namehtblink);
}


void vrtab::remove(vrent *vre) {
    ohashtab::remove((void *)(intptr_t)vre->volnum, vre);
    namehtb.remove(vre->key, &vre->namehtblink);
      
    delete vre;
}


vrent *vrtab::find(VolumeId grpvolnum) {
    ohashtab_iterator next(*this, (void *)(intptr_t)grpvolnum);
    vrent *vre;

    while ((vre = (vrent *)next()))
	if (vre->volnum == grpvolnum) return(vre);

    return(0);
}


vrent *vrtab::find(char *grpname) {
    ohashtab_iterator next(namehtb, grpname);
    vrent *vre;
    olink *l;
    
    while ((l = next())) {
	vre = strbase(vrent, l, namehtblink);
	if (STREQ(vre->key, grpname)) return(vre);
    }
    return(0);
}


vrent *vrtab::ReverseFind(VolumeId rwvolnum, int *idx)
{
    if (rwvolnum == 0) return(0);

    ohashtab_iterator next(*this, (void *) -1);
    vrent *vre;

    while ((vre = (vrent *)next())) {
	for (int i = 0; i < vre->nServers; i++) {
	    if (vre->ServerVolnum[i] == rwvolnum) {
		if (idx) *idx = i;
		return(vre);
	    }
	}
    }

    return(0);
}


void vrtab::clear() {
    ohashtab_iterator next(*this, (void *)-1);
    vrent *vre = (vrent *)next();

    while (vre) {
	vrent *dying = vre;
	vre = (vrent *)next();

	ohashtab::remove((void *)(intptr_t)dying->volnum, dying);
	namehtb.remove(dying->key, &(dying->namehtblink));
	delete dying;
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
    sprintf(buf, "%p : %-16s\n", this, name);
    write(afd, buf, strlen(buf));

    ohashtab_iterator next(*this, (void *)(intptr_t)-1);
    vrent *vre;
    while ((vre = (vrent *)next())) 
	    vre->print(afd);
}

int vrtab::dump(int afd)
{
    ohashtab_iterator next(*this, (void *)(intptr_t)-1);
    vrent *vre;
    while ((vre = (vrent *)next())) 
	    if (vre->dump(afd) == -1)
		return -1;
    return 0;
}

void CheckVRDB()
{
    int VRDB_fd = open(VRDB_PATH, O_RDONLY, 0);
    if (VRDB_fd < 0) {
	LogMsg(0, VolDebugLevel, stdout, "CheckVRDB: could not open VRDB");
	return;
    }

    VRDB.clear();

    /* Build the new VRDB. */
    vrent vre;
    while (read(VRDB_fd, &vre, sizeof(vrent)) == sizeof(vrent)) {
        vre.ntoh();
	VRDB.add(new vrent(vre));
    }

    close(VRDB_fd);
}

int DumpVRDB(int outfd)
{
    return VRDB.dump(outfd);
}


int XlateVid(VolumeId *vidp, int *count, int *pos) 
{
    vrent *vre = VRDB.find(*vidp);
    if (!vre) return(0);

    int ix = vre->index();
    if (ix == -1) return(0);


    *vidp = vre->ServerVolnum[ix];
    if (count) *count = vre->nServers;
    if (pos)   *pos = ix;
    return(1);
}


int ReverseXlateVid(VolumeId *vidp, int *idx)
{
    vrent *vre = VRDB.ReverseFind(*vidp, idx);
    if (!vre) return(0);

    *vidp = vre->volnum;
    return(1);
}

vrent::vrent() {
    memset(key, 0, sizeof(key));
    volnum = 0;
    nServers = 0;
    memset(ServerVolnum, 0, sizeof(ServerVolnum));
}


vrent::vrent(vrent& vre) {
    strcpy(key, vre.key);
    volnum = vre.volnum;
    nServers = vre.nServers;
    memcpy(ServerVolnum, vre.ServerVolnum, sizeof(ServerVolnum));
}


int vrent::operator=(vrent& vre) {
    abort();
    return(0);	/* keep C++ happy */
}


vrent::~vrent() {
}


void vrent::GetHosts(unsigned long *Hosts) {
    memset(Hosts, 0, VSG_MEMBERS * sizeof(unsigned long));

    for (int i = 0; i < nServers; i++)
	Hosts[i] = VolToHostAddr(ServerVolnum[i]);
}


int vrent::index_by_hostaddr(unsigned long hostaddr) {
    for (int i = 0; i < nServers; i++)
	if (hostaddr == VolToHostAddr(ServerVolnum[i])) return(i);

    return(-1);
}

int vrent::index_by_serverid(uint8_t serverid)
{
    for (int i = 0; i < nServers; i++)
	if (serverid == VolToServerId(ServerVolnum[i]))
            return(i);

    return(-1);
}

int vrent::index(void)
{
    return index_by_serverid(ThisServerId);
}


void vrent::GetCheckVV(ViceVersionVector *VV)
{
    memset((void *)VV, 0, sizeof(ViceVersionVector));

    for (int i = 0; i < nServers; i++) {
        uint8_t serverid = VolToServerId(ServerVolnum[i]);
        if (serverid)
            (&(VV->Versions.Site0))[i] = 1;
    }
}


int vrent::GetVolumeInfo(VolumeInfo *Info) {
    int i;

    if (nServers <= 0 || nServers > VSG_MEMBERS) {
	LogMsg(0, VolDebugLevel, stdout, "vrent::GetVolumeInfo: bogus nServers (%d)", nServers);
	return(VNOVOL);
    }

    memset((void *)Info, 0, sizeof(VolumeInfo));
    Info->Vid = volnum;
    Info->Type = REPVOL;
    (&Info->Type0)[REPVOL] = volnum;
    Info->ServerCount = nServers;
    for (i = 0; i < nServers; i++) {
	unsigned long hostaddr = VolToHostAddr(ServerVolnum[i]);
	if (hostaddr == 0) {
	    LogMsg(0, VolDebugLevel, stdout, "vrent::GetVolumeInfo: no hostaddr for volume (%lx)",
		    ServerVolnum[i]);
	    return(VNOVOL);
	}
	(&Info->Server0)[i] = hostaddr;
    }
    Info->VSGAddr = 0;
    for (i = 0; i < nServers; i++)
	(&Info->RepVolMap.Volume0)[i] = ServerVolnum[i];

    return(0);
}

void vrent::hton() {
    /* we won't translate the key on the hopes that strings take care of themselves */
    volnum = htonl(volnum);
    /* Don't need to translate nServers, it is a byte */

    for (int i = 0; i < VSG_MEMBERS; i++)
      ServerVolnum[i] = htonl(ServerVolnum[i]);
}

void vrent::ntoh() {
    /* we won't translate the key on the hopes that strings take care of themselves */
    volnum = ntohl(volnum);
    /* Don't need to translate nServers, it is a byte */

    for (int i = 0; i < VSG_MEMBERS; i++)
      ServerVolnum[i] = ntohl(ServerVolnum[i]);
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
    sprintf(buf, "%p : %s : %08x, %d, (x.x.x.x.x.x.x.x)\n",
	    this, key, volnum, nServers);
    write(afd, buf, strlen(buf));

}

int vrent::dump(int afd)
{
    char buf[512];
    int i, n, len;

    len = sprintf(buf, "%s %08x %d ", key, volnum, nServers);

    for (i = 0; i < VSG_MEMBERS; i++) {
	n = sprintf(buf + len, "%08x ", ServerVolnum[i]);
	len += n;
    }
    n = sprintf(buf + len, "0\n");
    len += n;

    n = write(afd, buf, len);
    if (n != len)
	return -1;

    return 0;
}
