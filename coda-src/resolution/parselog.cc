/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 * compops.c
 *	routines for computing list of 
 *	compensating operations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <olist.h>
#include <srv.h>
#include "rsle.h"
#include "parselog.h"

// *********** Private Routines *************

static void ReadOpsFromBuf(char *, int, int, rsle **);

/* ParseRemoteLogs
 *	Parse the byte stream of log entries shipped 
 *	by the coordinator.
 *	Separate out entries by host and by vnode number
 */
void ParseRemoteLogs(char *buf, int bufsize, int nentries, 
		     olist **hlist, rsle **RemoteLogEntries) {

    ReadOpsFromBuf(buf, bufsize, nentries, RemoteLogEntries);

    *hlist = new olist();

    int currenthost = 0;
    he *currenthe = NULL;

    VnodeId vnode = 0;
    Unique_t unique = 0;
    remoteloglist *loglist = NULL;
    
    for (int i = 0; i < nentries; i++) {
	rsle *r = &((*RemoteLogEntries)[i]);

	// index overloaded to hold hostid when shipped 
	if ((r->index != (int)currenthost)) {
	    // new host, add new entry to host list
	    currenthe = new he(r->index);
	    (*hlist)->append(currenthe);

	    currenthost = r->index;
	    vnode = 0;
	    unique = 0;
	    loglist = NULL;
	}

	// find this vnode's list of log entries 
	if (r->dvn != vnode || r->du != unique) {
	    vnode = r->dvn;
	    unique = r->du;
	    loglist = FindLogList(currenthe, vnode, unique);
	    if (loglist == NULL) {
		loglist = new remoteloglist(vnode, unique);
		CODA_ASSERT(loglist);
		currenthe->vlist.append(loglist);
	    }
	}
	CODA_ASSERT(loglist);

	// append entry to vnode's list of log entries 
	loglist->slelist.append(r);
    }
}

void DeallocateRemoteLogs(olist *AllLogs) {
    he *nexthe = NULL;
    while ((nexthe = (he *)(AllLogs->get()))) {
	remoteloglist *rll;
	while ((rll = (remoteloglist *)(nexthe->vlist.get()))) 
	    delete rll;			// remove all links and free rll
	delete nexthe;
    }
    delete AllLogs;
}

// ReadOpsFromBuf: 
//	Parse buf into individual log entries
//	Log entries are allocated in an array - RemoteLogEntries
static void ReadOpsFromBuf(char *buf, int bufsize, 
			    int nentries, rsle **RemoteLogEntries) {
    *RemoteLogEntries = new rsle[nentries];
    char *tmpptr = buf;
    int index = 0;
    while (tmpptr < (buf + bufsize)) {
	CODA_ASSERT(index < nentries);
	rsle *r = &((*RemoteLogEntries)[index]);
	r->InitFromRecleBuf(&tmpptr);
	index++;
    }
    CODA_ASSERT(index == nentries);
}

// FindLogList
//  Find the log entries corresponding to a vnode/unique pair
remoteloglist *FindLogList(he *hostent, VnodeId vn, Unique_t un) {
    olist_iterator next(hostent->vlist);
    remoteloglist *r;
    while ((r = (remoteloglist *)next())) 
	if (r->vnode == vn && r->unique == un) 
	    return(r);
    return(NULL);
}

olist *FindRemoteLog(olist *AllLogs, unsigned long hostid, 
				 ViceFid *Fid) {
    CODA_ASSERT(hostid != ThisHostAddr);
    he *rhe = FindHE(AllLogs, hostid);
    CODA_ASSERT(rhe);
    remoteloglist *rllp = FindLogList(rhe, Fid->Vnode, Fid->Unique);
    if (rllp) return(&rllp->slelist);
    else return(NULL);
}

