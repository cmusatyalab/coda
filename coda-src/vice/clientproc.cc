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
#endif

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <util.h>
#include <callback.h>
#include <prs.h>
#include <al.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

#include <srv.h>
#include <vice.private.h>

/* *****  Private variables  ***** */

static HostTable hostTable[MAXHOSTTABLEENTRIES];

/* *****  Private routines  ***** */

static void client_GetVenusId(RPC2_Handle, ClientEntry *);
static void client_RemoveClients(HostTable *);
static const char *client_SLDecode(RPC2_Integer);
static void client_SetUserName(ClientEntry *);


int CLIENT_Build(RPC2_Handle RPCid, char *User, RPC2_Integer sl,
		 SecretToken *st, ClientEntry **client) 
{
    long errorCode;

    /* get the client's username */
    if (sl == RPC2_OPENKIMONO) {
	if (STRNEQ(User, "UID=", 4) && AL_IdToName(atoi(User+4), User) == -1)
	    strcpy(User, PRS_ANYUSERGROUP);
    } else {
	if (!st)
	    return RPC2_NOTAUTHENTICATED;

	/* The token length, magic, and expiration times have already
	 * been validated by GetKeysFromToken */
	if (AL_IdToName((int) st->ViceId, User) == -1)
	    strcpy(User, PRS_ANYUSERGROUP);

	SLog(1, "Authorized Connection for user %s, uid %d, "
		"Start %d, end %d, time %d",
	     User, st->ViceId, st->BeginTimestamp, st->EndTimestamp, time(0));
    }

    /* Get the private pointer; it will be used to hold a reference to
       the new client entry. */
    errorCode = RPC2_GetPrivatePointer(RPCid, (char **)client);
    if(errorCode != RPC2_SUCCESS) return(errorCode);

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
    list_head_init(&(*client)->Clients);
    (*client)->DoUnbind = 0;
    (*client)->LastOp = 0;
    (*client)->SecurityLevel = sl;
    strcpy((*client)->UserName, User);
    (*client)->EndTimestamp = st ? st->EndTimestamp : 0;

    client_GetVenusId(RPCid, *client);

    /* Stash a reference to the new entry in the connection's private
       pointer. */
    errorCode = RPC2_SetPrivatePointer(RPCid, (char *)*client);
    if(errorCode != RPC2_SUCCESS) {
	    free(*client);
	    return(errorCode);
    }

    /* Get the id and then CPS for this client */
    client_SetUserName(*client);

    CurrentConnections++;

    return(0);
}


void CLIENT_Delete(ClientEntry *clientPtr) 
{
    if (!clientPtr) {
	    SLog(0, "Client pointer is zero in CLIENT_Delete");
	    return;
    }

    SLog(1, "Unbinding client entry for rpcid %d", clientPtr->RPCid);

    if (!list_empty(&clientPtr->Clients)) {
	list_del(&clientPtr->Clients);
	CurrentConnections--;
    }

    /* Free the ClientEntry. */
    RPC2_SetPrivatePointer(clientPtr->RPCid, (char *)0);

    /* Delay destroying our own connection to the client */
    if(clientPtr->LastOp) {
	clientPtr->DoUnbind = 1;
	return;
    }

    SLog(0, "Deleting client entry for user %s at %s.%d rpcid %d",
	 clientPtr->UserName, inet_ntoa(clientPtr->VenusId->host), 
	 ntohs(clientPtr->VenusId->port), clientPtr->RPCid);

    RPC2_Unbind(clientPtr->RPCid);
    clientPtr->RPCid = 0;
    AL_FreeCPS(&(clientPtr->CPS));
    free(clientPtr);
}


void CLIENT_InitHostTable(void) 
{
    for (int i = 0; i < MAXHOSTTABLEENTRIES; i++) {
	memset(&hostTable[i], 0, sizeof(struct HostTable));
	Lock_Init(&hostTable[i].lock);
	list_head_init(&hostTable[i].Clients);
    }
}

static void client_GetVenusId(RPC2_Handle RPCid, ClientEntry *client) 
{
	/* Look up the Peer info corresponding to the given RPC handle. */
	RPC2_PeerInfo peer;
	int i, old = -1;
	time_t oldest = 0;

	CODA_ASSERT(RPC2_GetPeerInfo(RPCid, &peer) == 0);
	CODA_ASSERT(peer.RemoteHost.Tag == RPC2_HOSTBYINETADDR);
	CODA_ASSERT(peer.RemotePort.Tag == RPC2_PORTBYINETNUMBER);

	/* Look for a corresponding host entry. */
	for (i = 0; i < MAXHOSTTABLEENTRIES; i++) {
	    if (memcmp(&hostTable[i].host, &peer.RemoteHost.Value.InetAddress,
		       sizeof(struct in_addr)) == 0 &&
		hostTable[i].port == peer.RemotePort.Value.InetPortNumber)
	    {
		ObtainWriteLock(&hostTable[i].lock);
		goto GotIt;
	    }

	    if (old == -1 || hostTable[i].LastCall < oldest) {
		old = i;
		oldest = hostTable[i].LastCall;
	    }
	}

	/* PANIC, hosttable full, kill the oldest client */
	ObtainWriteLock(&hostTable[old].lock);

	CLIENT_CleanUpHost(&hostTable[old]);
	i = old;

	hostTable[i].id = 0;
	memcpy(&hostTable[i].host, &peer.RemoteHost.Value.InetAddress,
	       sizeof(struct in_addr));
	hostTable[i].port = peer.RemotePort.Value.InetPortNumber;
	hostTable[i].LastCall = time(0);

	SLog(0, "client_GetVenusId: got new host %s:%d",
	     inet_ntoa(hostTable[i].host), ntohs(hostTable[i].port));

GotIt:
	client->VenusId = &hostTable[i];

	/* Link this client entry into the chain for the host. */
	list_add(&client->Clients, &hostTable[i].Clients);
	ReleaseWriteLock(&hostTable[i].lock);
}

int CLIENT_MakeCallBackConn(ClientEntry *Client) 
{
    RPC2_PeerInfo peer;
    RPC2_SubsysIdent sid;
    RPC2_CountedBS cbs;
    RPC2_BindParms bp;
    HostTable *HostEntry;
    RPC2_Handle callback_id;
    long	errorCode = RPC2_SUCCESS;

    /* Create peer info corresponding to the given Client structure. */
    peer.RemoteHost.Tag			     = RPC2_HOSTBYINETADDR;
    peer.RemoteHost.Value.InetAddress.s_addr = Client->VenusId->host.s_addr;
    peer.RemotePort.Tag			     = RPC2_PORTBYINETNUMBER;
    peer.RemotePort.Value.InetPortNumber     = Client->VenusId->port;

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

    /* Check if another thread already set up the connection while we were
     * waiting for the lock, if so we're done quickly. */
    if (HostEntry->id)
	goto exit_makecallbackconn;

    /* Attempt the bind */
    errorCode = RPC2_NewBinding(&peer.RemoteHost, &peer.RemotePort, &sid, &bp,
				&callback_id);

    /* This should never happen, otherwise someone forgot to get the
     * HostEntry->lock! */
    CODA_ASSERT(HostEntry->id == 0);

    HostEntry->id = callback_id;

    if (errorCode <= RPC2_ELIMIT) {
	SLog(0, "RPC2_Bind to %s port %d for callback failed %s",
	     inet_ntoa(HostEntry->host), ntohs(HostEntry->port), 
	     ViceErrorMsg((int) errorCode));
	goto exit_makecallbackconn;
    }

    if (errorCode != 0) {
	SLog(0, "RPC2_Bind to %s port %d for callback got %s",
	     inet_ntoa(HostEntry->host), ntohs(HostEntry->port), 
	     ViceErrorMsg((int) errorCode));
    }

    /* Make a gratuitous callback. */
    errorCode = CallBack(HostEntry->id, (ViceFid *)&NullFid);
    if (errorCode != 0) {
	SLog(0, "Callback message to %s port %d failed %s",
	     inet_ntoa(HostEntry->host), ntohs(HostEntry->port), 
	     ViceErrorMsg((int) errorCode));
    }

exit_makecallbackconn:
    if (errorCode <= RPC2_ELIMIT)
	CLIENT_CleanUpHost(HostEntry);

    if (HostEntry->id == 0) 
	    errorCode = EPIPE;  /* don't return an RPC2 error */

    return((int) errorCode);
}


void CLIENT_CallBackCheck() 
{
    dllist_head *head, *curr, *next;
    ClientEntry *cp;
    int		 i;
    long	 rc;

    time_t now = time(0);
    time_t checktime = now - (5 * 60);
    time_t deadtime  = now - (15 * 60);

    for (i = 0; i < MAXHOSTTABLEENTRIES; i++) {
	if (hostTable[i].id) {
	    if (hostTable[i].LastCall < checktime) {
		ObtainWriteLock(&hostTable[i].lock);
		/* recheck, the connection may have been destroyed while
		 * waiting for the lock */
		if (hostTable[i].id)
		     rc = CallBack(hostTable[i].id, (ViceFid *)&NullFid);
		else rc = RPC2_ABANDONED;

		if (rc <= RPC2_ELIMIT) {
		    SLog(0, "Callback failed %s for ws %s:%d",
			 ViceErrorMsg((int) rc), inet_ntoa(hostTable[i].host), 
			 ntohs(hostTable[i].port));

		    CLIENT_CleanUpHost(&hostTable[i]);
		}
		ReleaseWriteLock(&hostTable[i].lock);
	    }
	} else {
	    /* Clean up client structures that never obtained a callback
	     * connection, and have been quiet for more than 15 minutes */ 
	    ObtainWriteLock(&hostTable[i].lock);
	    head = &hostTable[i].Clients;
	    for (curr = head->next; curr != head; curr = next) {
		next = curr->next;
		cp = list_entry(curr, ClientEntry, Clients);

		if (cp->LastCall < deadtime)
		    CLIENT_Delete(cp);
	    }
	    ReleaseWriteLock(&hostTable[i].lock);
	}
    }
}


/* This needs to be called with ht->lock taken!! */
void CLIENT_CleanUpHost(HostTable *ht) 
{
    SLog(1, "Cleaning up a HostTable for %s.%d",
	 inet_ntoa(ht->host), ntohs(ht->port));

    client_RemoveClients(ht);	/* remove any connections for this Venus */
    DeleteVenus(ht);		/* remove all callback entries	*/
    if (ht->id) {
	SLog(1, "Unbinding RPC2 connection %d", ht->id);
	RPC2_Unbind(ht->id);
	ht->id = 0;
    }
    ht->host.s_addr = INADDR_ANY;
    ht->port = 0;
}


/* This needs to be called with ht->lock taken!! */
static void client_RemoveClients(HostTable *ht) 
{
    dllist_head *head, *curr, *next;
    ClientEntry *cp;

    head = &ht->Clients;
    for (curr = head->next; curr != head; curr = next) {
	next = curr->next;
	cp = list_entry(curr, ClientEntry, Clients);

	CLIENT_Delete(cp);
	if(next->prev == curr) {
	    SLog(0, "RemoveClients got a failure from DeleteClient");
	    break;
	}
    }
}


static const char *client_SLDecode(RPC2_Integer sl) 
{
    if(sl == RPC2_OPENKIMONO)  return "OpenKimono";
    if(sl == RPC2_AUTHONLY)    return "AuthOnly";
    if(sl == RPC2_HEADERSONLY) return "HeadersOnly";
    if(sl == RPC2_SECURE)      return "Secure";
    return "Unknown";
}


void CLIENT_PrintClients() 
{
    struct timeval tp;
    struct timezone tsp;
    TM_GetTimeOfDay(&tp, &tsp);
    SLog(1, "List of active users at %s", ctime((const time_t *)&tp.tv_sec));
    struct dllist_head *curr;
    ClientEntry *cp;
    int i;

    for(i = 0; i < MAXHOSTTABLEENTRIES; i++) {
	list_for_each(curr, hostTable[i].Clients) {
	    cp = list_entry(curr, ClientEntry, Clients);
	    SLog(1, "user = %s at %s:%d cid %d security level %s",
		 cp->UserName, inet_ntoa(hostTable[i].host),
		 ntohs(hostTable[i].port), cp->RPCid,
		 client_SLDecode(cp->SecurityLevel));
	}
    }
}

/* Coerce name to System:AnyUser if not properly authenticated. */
static void client_SetUserName(ClientEntry *client) 
{
	const char *name;
	if (Authenticate && client->SecurityLevel == RPC2_OPENKIMONO)
		name = PRS_ANYUSERGROUP;
	else
		name = client->UserName;

	/* Translate the name to a proper Id. */
	if (AL_NameToId(name, (int *)&(client->Id)) != 0) {
		if(!STREQ(client->UserName, NEWCONNECT))
			SLog(0, "User id %s unknown", name);

		/* assign the id of System:AnyUser */
		client->Id = AnyUserId;
	}
	CODA_ASSERT(AL_GetInternalCPS((int) client->Id, &(client->CPS)) == 0);
}


/* This counts the number of workstations and the number of active
   workstations. */
/* A workstation is active if it has received a call since time. */
void CLIENT_GetWorkStats(int *num, int *active, unsigned int time) 
{
    int i;

    *num = 0;
    *active = 0;

    for(i = 0; i < MAXHOSTTABLEENTRIES; i++) {
	if (hostTable[i].id) {
	    (*num)++;
	    if (hostTable[i].ActiveCall > (time_t)time) 
		(*active)++;
	}
    }
}

