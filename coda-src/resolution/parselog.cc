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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/





/*
 * compops.c
 *	routines for computing list of 
 *	compensating operations.
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <libc.h>
#include <stdio.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <olist.h>
#include <remotelog.h>
#include <srv.h>
#include "rsle.h"
#include "parselog.h"

// *********** Private Routines *************

PRIVATE void ReadOpsFromBuf(char *, int, int, rsle **);

/* ParseRemoteLogs
 *	Parse the byte stream of log entries shipped 
 *	by the coordinator.
 *	Separate out entries by host and by vnode number
 */
void ParseRemoteLogs(char *buf, int bufsize, int nentries, 
		     olist **hlist, rsle **RemoteLogEntries) {

    ReadOpsFromBuf(buf, bufsize, nentries, RemoteLogEntries);

    *hlist = new olist();

    unsigned long currenthost = 0;
    he *currenthe = NULL;

    VnodeId vnode = 0;
    Unique_t unique = 0;
    remoteloglist *loglist = NULL;
    
    for (int i = 0; i < nentries; i++) {
	rsle *r = &((*RemoteLogEntries)[i]);

	// index overloaded to hold hostid when shipped 
	if (r->index != currenthost) {
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
		assert(loglist);
		currenthe->vlist.append(loglist);
	    }
	}
	assert(loglist);

	// append entry to vnode's list of log entries 
	loglist->slelist.append(r);
    }
}

void DeallocateRemoteLogs(olist *AllLogs) {
    he *nexthe = NULL;
    while (nexthe = (he *)(AllLogs->get())) {
	remoteloglist *rll;
	while (rll = (remoteloglist *)(nexthe->vlist.get())) 
	    delete rll;			// remove all links and free rll
	delete nexthe;
    }
    delete AllLogs;
}

// ReadOpsFromBuf: 
//	Parse buf into individual log entries
//	Log entries are allocated in an array - RemoteLogEntries
PRIVATE void ReadOpsFromBuf(char *buf, int bufsize, 
			    int nentries, rsle **RemoteLogEntries) {
    *RemoteLogEntries = new rsle[nentries];
    char *tmpptr = buf;
    int index = 0;
    while (tmpptr < (buf + bufsize)) {
	rsle *r = &((*RemoteLogEntries)[index]);
	r->InitFromRecleBuf(&tmpptr);
	index++;
    }
    assert(index == nentries);
}

// FindLogList
//  Find the log entries corresponding to a vnode/unique pair
remoteloglist *FindLogList(he *hostent, VnodeId vn, Unique_t un) {
    olist_iterator next(hostent->vlist);
    remoteloglist *r;
    while (r = (remoteloglist *)next()) 
	if (r->vnode == vn && r->unique == un) 
	    return(r);
    return(NULL);
}

olist *FindRemoteLog(olist *AllLogs, unsigned long hostid, 
				 ViceFid *Fid) {
    assert(hostid != ThisHostAddr);
    he *rhe = FindHE(AllLogs, hostid);
    assert(rhe);
    remoteloglist *rllp = FindLogList(rhe, Fid->Vnode, Fid->Unique);
    if (rllp) return(&rllp->slelist);
    else return(NULL);
}


