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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/


/************************************************************************/
/*									*/
/*  clientproc.c - Maintain the FileServer user structure		*/
/*									*/
/*  Function	- A set of routines to build the user structure for	*/
/*		  the File Server.					*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <util.h>
#include <callback.h>
#include <prs.h>
#include <al.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <srv.h>
#include <vice.private.h>


/* *****  Private variables  ***** */

static HostTable hostTable[MAXHOSTTABLEENTRIES];
static int maxHost = 0;


/* *****  Private routines  ***** */

static HostTable *client_GetVenusId(RPC2_Handle);
static void client_RemoveClients(HostTable *);
static char *client_SLDecode(RPC2_Integer);
static void client_SetUserName(ClientEntry *);


int CLIENT_Build(RPC2_Handle RPCid, char *User, RPC2_Integer sl, 
		 ClientEntry **client) 
{
    long errorCode;

    /* Translate from textual representation of uid to user name (if necessary). */
    char username[PRS_MAXNAMELEN + 1];
    if (STRNEQ(User, "UID=", 4)) {
	if (AL_IdToName(atoi(User + 4), username))
		strcpy(username,"System:AnyUser");
    } else {
	    strcpy(username, User);
    }

    /* Get the private pointer; it will be used to hold a reference to
       the new client entry. */
    errorCode = RPC2_GetPrivatePointer(RPCid, (char **)client);
    if(errorCode != RPC2_SUCCESS)
	    return(errorCode);
    if (*client) {
	    SLog(0, "Someone left garbage in the client pointer. Freeing.");
	    /* I added this; couldn't see any reason not to free it. -JJK */
	    free((char *)*client);
	    *client = NULL;
    }

    /* Get a free client table entry and initialize it. */
    *client = (ClientEntry *)malloc(sizeof(ClientEntry));
    CODA_ASSERT(*client);
    (*client)->RPCid = RPCid;
    (*client)->NextClient = 0;
    (*client)->DoUnbind = 0;
    (*client)->LastOp = 0;
    (*client)->SecurityLevel = sl;
    strcpy((*client)->UserName, username);
    (*client)->VenusId = client_GetVenusId(RPCid);

    /* Stash a reference to the new entry in the connection's private
       pointer. */
    errorCode = RPC2_SetPrivatePointer(RPCid, (char *)*client);
    if(errorCode != RPC2_SUCCESS) {
	    free(*client);
	    return(errorCode);
    }

    /* Link this client entry into the chain for the host. */
    ObtainWriteLock(&((*client)->VenusId->lock));
    (*client)->NextClient = (ClientEntry *)((*client)->VenusId->FirstClient);
    (*client)->VenusId->FirstClient = *client;
    ReleaseWriteLock(&((*client)->VenusId->lock));

    /* Get the id and then CPS for this client */
    client_SetUserName(*client);

    CurrentConnections++;

    return(0);
}


void CLIENT_Delete(ClientEntry *clientPtr) 
{
    if (clientPtr == 0) {
	    SLog(0, "Client pointer is zero in CLIENT_Delete");
	    return;
    }

    SLog(1, "Deleting client entry for user %s at %s.%d",
	 clientPtr->UserName, clientPtr->VenusId->HostName, 
	 ntohs(clientPtr->VenusId->port));

    if(clientPtr->DoUnbind) {
	    SLog(0, "DoUnbind is TRUE in CLIENT_Delete");
	    return;
    }

    if (clientPtr->VenusId && clientPtr->VenusId->FirstClient) {
	    if (clientPtr->VenusId->FirstClient == clientPtr) {
		    clientPtr->VenusId->FirstClient = clientPtr->NextClient;
	} else {
		for (ClientEntry *searchPtr = clientPtr->VenusId->FirstClient;
		     searchPtr; searchPtr = searchPtr->NextClient)
			if (searchPtr->NextClient == clientPtr)
				searchPtr->NextClient = clientPtr->NextClient;
	}
    }

    CurrentConnections--;

    /* Free the ClientEntry. */
    RPC2_SetPrivatePointer(clientPtr->RPCid, (char *)0);
    if(clientPtr->LastOp) {
	    clientPtr->DoUnbind = 1;
    } else {
	    SLog(0, "Unbinding RPC2 connection %d", clientPtr->RPCid);

	    RPC2_Unbind(clientPtr->RPCid);
	    clientPtr->RPCid = 0;
	    AL_FreeCPS(&(clientPtr->CPS));
	    clientPtr->VenusId = 0;
	    free((char *)clientPtr);
    }
}


static HostTable *client_GetVenusId(RPC2_Handle RPCid) 
{

	/* Look up the Peer info corresponding to the given RPC
           handle. */
	RPC2_PeerInfo peer;
	int i;

	CODA_ASSERT(RPC2_GetPeerInfo(RPCid, &peer) == 0);
	CODA_ASSERT(peer.RemoteHost.Tag == RPC2_HOSTBYINETADDR);
	CODA_ASSERT(peer.RemotePort.Tag == RPC2_PORTBYINETNUMBER);

	/* Look for a corresponding host entry. */
	for (i = 0; i < maxHost; i++)
		if ((hostTable[i].host ==
		     peer.RemoteHost.Value.InetAddress.s_addr) &&
		    (hostTable[i].port ==
		     peer.RemotePort.Value.InetPortNumber))
			break;
	
	/* Not found.  Make a new host entry. */
	CODA_ASSERT(maxHost < MAXHOSTTABLEENTRIES-1);
	if (i == maxHost) {
		hostTable[i].host = peer.RemoteHost.Value.InetAddress.s_addr;
		hostTable[i].port = peer.RemotePort.Value.InetPortNumber;
		hostTable[i].FirstClient = 0;
		hostTable[i].id = 0;
		/* the funky redirection is to fool the C++ compiler into
		 * converting the long into a struct. The correct solution is
		 * typing  hostTable.host as struct in_addr!!! --JH */
		sprintf(hostTable[i].HostName, "%s", inet_ntoa(*(struct in_addr*)&hostTable[i].host));
		Lock_Init(&hostTable[i].lock);
		maxHost++;
	}

	/* Lock the host entry. */
	ObtainWriteLock(&hostTable[i].lock);

	/* Unlock the host entry. */
	ReleaseWriteLock(&hostTable[i].lock);

	return(&hostTable[i]);
}


/* look up a host entry given the (callback) connection id */
HostTable *CLIENT_FindHostEntry(RPC2_Handle CBCid) 
{
    HostTable *he = NULL;

    /* Look for a corresponding host entry. */
    for (int i = 0; i < maxHost; i++)
	if (hostTable[i].id == CBCid) 
	    he = &hostTable[i];

    return(he);
}


int CLIENT_MakeCallBackConn(ClientEntry *Client) 
{

    RPC2_PeerInfo peer;
    RPC2_SubsysIdent sid;
    RPC2_CountedBS cbs;
    RPC2_BindParms bp;
    HostTable *HostEntry;

    /* Look up the Peer info corresponding to the given RPC handle. */
    CODA_ASSERT(RPC2_GetPeerInfo(Client->RPCid, &peer) == 0);
    CODA_ASSERT(peer.RemoteHost.Tag == RPC2_HOSTBYINETADDR);
    CODA_ASSERT(peer.RemotePort.Tag == RPC2_PORTBYINETNUMBER);

    /* Subsystem identifier. */
    sid.Tag = RPC2_SUBSYSBYID;
    sid.Value.SubsysId = SUBSYS_CB;

    /* Dummy argument. */
    cbs.SeqLen = 0;

    /* Bind parameters */
    bp.SecurityLevel = RPC2_OPENKIMONO;
    bp.EncryptionType = RPC2_XOR;
    bp.SideEffectType = SMARTFTP;
    bp.ClientIdent = &cbs;
    bp.SharedSecret = NULL;

    CODA_ASSERT(Client);
    HostEntry = Client->VenusId;
    ObtainWriteLock(&HostEntry->lock);

    /* Attempt the bind. */
    long errorCode = RPC2_NewBinding(&peer.RemoteHost, 
				     &peer.RemotePort, &sid, &bp, 
				     &HostEntry->id);

    if (errorCode <= RPC2_ELIMIT) {
	SLog(0, "RPC2_Bind to %s port %d for callback failed %s",
	     HostEntry->HostName, ntohs(HostEntry->port), 
	     ViceErrorMsg((int) errorCode));
	goto exit_makecallbackconn;
    }

    if (errorCode != 0) {
	SLog(0, "RPC2_Bind to %s port %d for callback got %s",
	     HostEntry->HostName, ntohs(HostEntry->port), 
	     ViceErrorMsg((int) errorCode));
    }

    /* Make a gratuitous callback. */
    errorCode = CallBack(HostEntry->id, &NullFid);
    if (errorCode != 0) {
	SLog(0, "Callback message to %s port %d failed %s",
	     HostEntry->HostName, ntohs(HostEntry->port), 
	     ViceErrorMsg((int) errorCode));
    }

exit_makecallbackconn:
    if (errorCode <= RPC2_ELIMIT)
	CLIENT_CleanUpHost(HostEntry);

    if (HostEntry->id == 0) 
	    errorCode = EPIPE;  /* don't return an RPC2 error */

    ReleaseWriteLock(&HostEntry->lock);
    
    return((int) errorCode);
}


void CLIENT_CallBackCheck() 
{
    struct timeval tp;
    struct timezone tsp;
    TM_GetTimeOfDay(&tp, &tsp);
    unsigned int checktime = (unsigned int) tp.tv_sec - (5 * 60);

    for (int i = 0; i < maxHost; i++) {
	if ((hostTable[i].id) && (hostTable[i].LastCall < checktime)) {
	    ObtainWriteLock(&hostTable[i].lock);
	    long rc = CallBack(hostTable[i].id, &NullFid);
	    if (rc <= RPC2_ELIMIT) {
		SLog(0, "Callback failed %s for ws %s, port %d",
		     ViceErrorMsg((int) rc), hostTable[i].HostName, 
		     ntohs(hostTable[i].port));
		CLIENT_CleanUpHost(&hostTable[i]);
	    }
	    ReleaseWriteLock(&hostTable[i].lock);
	}
    }
}


void CLIENT_CleanUpHost(HostTable *ht) 
{
    SLog(1, "Cleaning up a HostTable for %s.%d", ht->HostName, ntohs(ht->port));

    client_RemoveClients(ht);	/* remove any connections for this Venus */
    DeleteVenus(ht);		/* remove all callback entries	*/
    if (ht->id) {
	SLog(1, "Unbinding RPC2 connection %d", ht->id);
	RPC2_Unbind(ht->id);
	ht->id = 0;
    }
}


static void client_RemoveClients(HostTable *ht) 
{
    for (ClientEntry *cp = ht->FirstClient; cp; cp = ht->FirstClient) {
	CLIENT_Delete(cp);
	if(cp == ht->FirstClient) {
	    SLog(0, "RemoveClients got a failure from DeleteClient");
	    break;
	}
    }
    ht->FirstClient = 0;
}


static char *client_SLDecode(RPC2_Integer sl) 
{
    if(sl == RPC2_OPENKIMONO) return("OpenKimono");
    if(sl == RPC2_AUTHONLY) return("AuthOnly");
    if(sl == RPC2_HEADERSONLY) return("HeadersOnly");
    if(sl == RPC2_SECURE) return("Secure");
    return("Unknown");
}


void CLIENT_PrintClients() 
{
    struct timeval tp;
    struct timezone tsp;
    TM_GetTimeOfDay(&tp, &tsp);
    SLog(1, "List of active users at %s", ctime(&tp.tv_sec));

    for(int i = 0; i < maxHost; i++) {
	for(ClientEntry *cp = hostTable[i].FirstClient; cp; cp=cp->NextClient) {
	    SLog(1, "user = %s at %s.%d cid %d security level %s",
		 cp->UserName, hostTable[i].HostName, ntohs(hostTable[i].port), 
		 cp->RPCid, client_SLDecode(cp->SecurityLevel));
	}
    }
}

/* Coerce name to System:AnyUser if not properly authenticated. */
static void client_SetUserName(ClientEntry *client) 
{
	char *name;
	if (Authenticate && client->SecurityLevel == RPC2_OPENKIMONO)
		name = "System:AnyUser";
	else
		name = client->UserName;

	/* Translate the name to a proper Id. */
	if (AL_NameToId(name, (int *)&(client->Id)) != 0) {
		if(!STREQ(client->UserName, NEWCONNECT))
			SLog(0, "User id %s unknown", name);
		CODA_ASSERT(AL_NameToId("System:AnyUser", 
				   (int *)&(client->Id)) == 0);
	}
	CODA_ASSERT(AL_GetInternalCPS((int) client->Id, &(client->CPS)) == 0);
	
}


/* This counts the number of workstations and the number of active
   workstations. */
/* A workstation is active if it has received a call since time. */
void CLIENT_GetWorkStats(int *num, int *active, unsigned int time) 
{
	*num = 0;
	*active = 0;
	
	for(int i = 0; i < maxHost; i++)
		if (hostTable[i].id != 0) {
			(*num)++;
			if (hostTable[i].ActiveCall > time) 
				(*active)++;
		}
}
