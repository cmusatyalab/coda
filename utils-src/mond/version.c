#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1995 Carnegie Mellon University
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

static char *rcsid = "$Header$";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <libc.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "lwp.h"
#include "rpc2.h"

#ifdef __cplusplus
}
#endif __cplusplus

#include "mond.h"
#include "olist.h"
#include "ohash.h"
#include "version.h"
#include "util.h"

/* the table */
connection_table *ClientTable = NULL;

extern int LogLevel;
extern FILE *LogFile;

char *HostNameOfConn(RPC2_Handle);

connection_entry::connection_entry(RPC2_Handle _cid, long _clientType)
{
    myCid = _cid;
    myClientType = _clientType;
}

void connection_entry::print(void)
{
    print(0,LogFile);
}

void connection_entry::print(FILE *fp)
{
    print(0,fp);
}

void connection_entry::print(int fd)
{
    print(0,fdopen(fd,"a+"));
}

void connection_entry::print(int level,FILE *file)
{
    char *conname;
    conname = HostNameOfConn(myCid);
    if (conname == NULL) return;
    LogMsg(level,LogLevel,file,"connentry: %s [%s]",
	   conname,
	   (myClientType == MOND_VICE_CLIENT) ? "vice" : "venus");
    delete [] conname;
}

PRIVATE inline int hashfn(RPC2_Handle *cid)
{
    return ((int) *cid);
}

connection_entry *connection_table::GetConnection(RPC2_Handle cid)
{
    ohashtab_iterator next(*table, &cid);
    connection_entry *theEntry;
    while (theEntry = (connection_entry*) next()) {
	if (theEntry->cid() == cid)
	    return (connection_entry *) theEntry;
    }
    return NULL;
}
    

int connection_table::ConnectionValid(RPC2_Handle cid, long clientType)
{
    connection_entry *theEntry = GetConnection(cid);
    
    if (theEntry == NULL) return MOND_NOTCONNECTED;
    
    if (theEntry->client_type() == clientType)
	return MOND_OK;
    else return MOND_BADCONNECTION;
}

int connection_table::RemoveConnection(RPC2_Handle cid)
{
    connection_entry *theEntry = GetConnection(cid);
    if (theEntry == NULL) return MOND_NOTCONNECTED;
    
    table->remove(&cid,theEntry);
    return MOND_OK;
}

void connection_table::LogConnections(int importance, FILE *LogFile)
{
    ohashtab_iterator next(*table);
    connection_entry *theEntry;

    LogMsg(importance,LogLevel,LogFile,"Current connections");
    while (theEntry = (connection_entry *)next()) {
	theEntry->print(importance,LogFile);
    }
}

int connection_table::PurgeConnections()
{
    ohashtab_iterator next(*table);
    connection_entry *theEntry;
    int theResult = MOND_OK;

    while (theEntry = (connection_entry *)next()) {
	int result = (int)RPC2_Unbind(theEntry->cid());
	if (result != RPC2_SUCCESS && result != RPC2_NOCONNECTION) {
	    theResult = (theResult == MOND_OK) ? result : theResult;
	}
    }
    if (theResult == MOND_OK)
	table->clear();
    return theResult;
}	

connection_table::connection_table(int tablesize)
{
    typedef int (*XXX) (void *);
    int realsize = 1;
    while (realsize*2 < tablesize) {
	realsize *= 2;
    }
    table = new ohashtab(realsize, (XXX)hashfn);
}

connection_table::~connection_table()
{
    this->PurgeConnections();
    delete table;
}

long MondEstablishConn(RPC2_Handle cid, unsigned long Version,
		       long clientType, long spare_size, SpareEntry spare[])
{
    // check for valid version 
    // (this should be changed to allow for old, but compatible)
    char *hostname = HostNameOfConn(cid);
    assert (hostname != NULL);
    LogMsg(10,LogLevel,LogFile,
	   "Beginning MondEstablishConn for %s as a %s",
	   hostname, ((clientType == MOND_VICE_CLIENT) ? "vice" : "venus"));
    delete [] hostname;
    if (Version != MOND_CURRENT_VERSION
	|| (clientType != MOND_VENUS_CLIENT && clientType != MOND_VICE_CLIENT)) {
	char *badhost = HostNameOfConn(cid);
	assert(hostname != NULL);
	LogMsg(0,LogLevel,LogFile,
	       "Incompatible Version request: host %s [%s]; version %ld",
	       badhost,
	       (clientType == MOND_VENUS_CLIENT) ? "venus" :
	           (clientType == MOND_VICE_CLIENT) ? "vice" : "???",
	       Version);
	delete [] badhost;
	return MOND_INCOMPATIBLE;
    }

    // does and entry for this RPC2_Handle exist?
    connection_entry *theEntry = ClientTable->GetConnection(cid);
    if (theEntry != NULL) {
	LogMsg(5,LogLevel,LogFile,
	       "Found entry for RPC2_Handle %d",cid);
	if (theEntry->client_type() == clientType)
	    return MOND_CONNECTED;
	else
	    return MOND_BADCONNECTION;
    }

    // enter the connection
    theEntry = new connection_entry(cid, clientType);
    ClientTable->table->insert((void*)&cid,theEntry);
    LogMsg(10,LogLevel,LogFile,
	   "Entered connection for RPC2_Handle %d",cid);
    return MOND_OK;
}



char *HostNameOfConn(RPC2_Handle cid)
// returns a string that *must* be freed later
{
    RPC2_PeerInfo PeerInfo;
    if (RPC2_GetPeerInfo(cid,&PeerInfo) != RPC2_SUCCESS) {
	return NULL;
    }

    char *newconname;
    struct in_addr addr;
    struct hostent *hostentry;

    const int CNSIZE = 256;
    newconname = new char[CNSIZE];
    if (PeerInfo.RemoteHost.Tag == RPC2_HOSTBYNAME)
	strncpy(newconname,PeerInfo.RemoteHost.Value.Name,CNSIZE-1);
    else {
	addr.s_addr = PeerInfo.RemoteHost.Value.InetAddress;
	hostentry = gethostbyaddr((char *)&addr.s_addr,
				  sizeof(addr.s_addr),AF_INET);
	if (hostentry == NULL)
	    strncpy(newconname,inet_ntoa(addr),CNSIZE-1);
	else
	    strncpy(newconname,hostentry->h_name,CNSIZE-1);
    }
    newconname[CNSIZE-1] ='\0';
    return newconname;
}

