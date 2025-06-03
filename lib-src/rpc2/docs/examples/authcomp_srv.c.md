---
title: authcomp_srv.c
---
``` c
#include <stdio.h>
#include <strings.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <assert.h>
#include <pwd.h>
#include <lwp.h>
#include <rpc2.h>
#include "auth.h"
#include "comp.h"

/*
This data structure provides per-connection info.  It is
created on every new connection and ceases to exist after AuthQuit().
*/
struct UserInfo {
    int Creation; /* Time at which this connection was created */
    /* other fields would go here */
}

int NewCLWP(), AuthLWP(), CompLWP(); /* bodies of LWPs */
void DebugOn(), DebugOff(); /* signal handlers */

void main()
{
    int mypid;

    signal(SIGEMT, DebugOn);
    signal(SIGIOT, DebugOff);

    InitRPC();
    LWP_CreateProcess(AuthLWP, 4096, LWP_NORMAL_PRIORITY, "AuthLWP", NULL,
                      &mypid);
    LWP_CreateProcess(CompLWP, 4096, LWP_NORMAL_PRIORITY, "CompLWP", NULL,
                      &mypid);
    LWP_WaitProcess(main); /* sleep here forever; no one will ever wake me up */
}

void AuthLWP(char *p)
/* char *p; single parameter passed to LWP_CreateProcess() */
{
    RPC2_RequestFilter reqfilter;
    RPC2_PacketBuffer *reqbuffer;
    RPC2_Handle cid;
    int rc;
    char *pp;

    /*  Set filter to accept  auth requests on new or existing connections */
    reqfilter.FromWhom              = ONESUBSYS;
    reqfilter.OldOrNew              = OLDORNEW;
    reqfilter.ConnOrSubsys.SubsysId = AUTHSUBSYSID;

    while (1) {
        cid = 0;

        rc = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, NULL, NULL, NULL,
                             NULL);
        if (rc < RPC2_WLIMIT)
            HandleRPCError(rc, cid);

        rc = auth_ExecuteRequest(cid, reqbuffer);
        if (rc < RPC2_WLIMIT)
            HandleRPCError(rc, cid);

        pp = NULL;
        if (RPC2_GetPrivatePointer(cid, &pp) != RPC2_SUCCESS || pp == NULL)
            RPC2_Unbind(cid); /* This was almost certainly an AuthQuit() call */
    }
}

void CompLWP(char *p)
{
    RPC2_RequestFilter reqfilter;
    RPC2_PacketBuffer *reqbuffer;
    RPC2_Handle cid;
    int rc;
    char *pp;

    /* Set filter  to accept  comp requests on new or existing connections */
    reqfilter.FromWhom              = ONESUBSYS;
    reqfilter.OldOrNew              = OLDORNEW;
    reqfilter.ConnOrSubsys.SubsysId = COMPSUBSYSID;

    while (1) {
        cid = 0;
        rc  = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, NULL, NULL, NULL,
                              NULL);
        if (rc < RPC2_WLIMIT)
            HandleRPCError(rc, cid);

        rc = comp_ExecuteRequest(cid, reqbuffer);
        if (rc < RPC2_WLIMIT)
            HandleRPCError(rc, cid);

        pp = NULL;
        if (RPC2_GetPrivatePointer(cid, &pp) != RPC2_SUCCESS || pp == NULL)
            RPC2_Unbind(cid); /* This was almost certainly an CompQuit() call */
    }
}

/*
Bodies of Auth RPC routines
*/

void S_AuthNewConn(RPC2_Handle cid, RPC2_Integer seType, RPC2_Integer secLevel,
                   RPC2_Integer encType, RPC2_CountedBS *cIdent)
{
    struct UserInfo *p;

    p = (struct UserInfo *)malloc(sizeof(struct UserInfo));
    RPC2_SetPrivatePointer(cid, p);
    p->Creation = time(0);
}

int S_AuthQuit(RPC2_Handle cid)
/*
Get rid of user state; note that we do not do RPC2_Unbind() here, because this
request itself has to complete.  The invoking server LWP therefore checks to
see if this connection can be unbound.
*/
{
    struct UserInfo *p;
    RPC2_GetPrivatePointer(cid, &p);
    assert(p != NULL); /* we have a bug then */
    free(p);
    RPC2_SetPrivatePointer(cid, NULL);
    return (AUTHSUCCESS);
}

int S_AuthUserId(RPC2_Handle cid, char *userName, int *userId)
{
    struct passwd *pw;
    if ((pw = getpwnam(userName)) == NULL)
        return (AUTHFAILED);

    *userId = pw->pw_uid;
    return (AUTHSUCCESS);
}

int S_AuthUserName(RPC2_Handle cid, int userId, RPC2_BoundedBS *userName)
{
    struct passwd *pw;
    if ((pw = getpwuid(userId)) == NULL)
        return (AUTHFAILED);

    strcpy(userName->SeqBody, pw->pw_name);
    /* we hope the buffer is big enough */
    userName->SeqLen = 1 + strlen(pw->pw_name);
    return (AUTHSUCCESS);
}

int S_AuthUserInfo(RPC2_Handle cid, int userId, AuthInfo *uInfo)
{
    struct passwd *pw;
    if ((pw = getpwuid(userId)) == NULL)
        return (AUTHFAILED);

    uInfo->GroupId = pw->pw_gid;
    strcpy(uInfo->HomeDir, pw->pw_dir);
    return (AUTHSUCCESS);
}

/*
Bodies of Comp RPC routines
*/
void S_CompNewConn(RPC2_Handle cid, RPC2_Integer seType, RPC2_Integer secLevel,
                   RPC2_Integer encType, RPC2_CountedBS *cIdent)
{
    struct UserInfo *p;

    p = (struct UserInfo *)malloc(sizeof(struct UserInfo));
    RPC2_SetPrivatePointer(cid, p);
    p->Creation = time(0);
}

int S_CompQuit(RPC2_Handle cid)
/*
Get rid of user state; note that we do not do RPC2_Unbind() here, because this
request itself has to complete.  The invoking server LWP therefore checks to
see if this connection can be unbound.
*/
{
    struct UserInfo *p;
    RPC2_GetPrivatePointer(cid, &p);
    assert(p != NULL); /* we have a bug then */
    free(p);
    RPC2_SetPrivatePointer(cid, NULL);
    return (0);
}

int S_CompSquare(RPC2_Handle cid, int x)
{
    return (x * x);
}

int S_CompCube(RPC2_Handle cid, int x)
{
    return (x * x * x);
}

int S_CompAge(RPC2_Handle cid, int x)
{
    struct UserInfo *p;
    assert(RPC2_GetPrivatePointer(cid, &p) == RPC2_SUCCESS);
    return (time(0) - p->Creation);
}

/*
RPC Initialization and Error handling
*/
void InitRPC()
{
    int mylpid = -1;
    RPC2_PortalIdent portalid, *portallist[1];
    RPC2_SubsysIdent subsysid;
    long rc;

    assert(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) == LWP_SUCCESS);

    portalid.Tag                  = RPC2_PORTALBYINETNUMBER;
    portalid.Value.InetPortNumber = htons(AUTHPORTAL);
    portallist[0]                 = &portalid;

    rc = RPC2_Init(RPC2_VERSION, 0, portallist, 1, -1, NULL);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "RPC2_Init: failed with %ld\n", rc);
        exit(1);
    }

    subsysid.Tag            = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = AUTHSUBSYSID;
    assert(RPC2_Export(&subsysid) == RPC2_SUCCESS);

    subsysid.Value.SubsysId = COMPSUBSYSID;
    assert(RPC2_Export(&subsysid) == RPC2_SUCCESS);
}

void HandleRPCError(int rCode, RPC2_Handle connId)
{
    fprintf(stderr, "exserver: %s\n", RPC2_ErrorMsg(rCode));
    if (rCode < RPC2_FLIMIT && connId != 0)
        RPC2_Unbind(connId);
}

void DebugOn()
{
    RPC2_DebugLevel = 100;
}

void DebugOff()
{
    RPC2_DebugLevel = 0;
}
```
