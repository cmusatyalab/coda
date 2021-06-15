---
title: example_client.c
---
``` c
/* exclient.c -- Trivial client to demonstrate basic RPC2 functionality */

#include <stdio.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <assert.h>
#include <pwd.h>
#include <lwp.h>
#include <rpc2.h>
#include "auth.h"
#include "comp.h"

void main()
{
    int a;
    char buf[100];

    printf("Debug Level? (0) ");
    gets(buf);
    RPC2_DebugLevel = atoi(buf);

    InitRPC();
    while (1) {
        printf("Action? (1 = New Conn, 2 = Auth Request, 3 = Comp Request) ");
        gets(buf);
        a = atoi(buf);
        switch (a) {
        case 1:
            NewConn();
            continue;
        case 2:
            Auth();
            continue;
        case 3:
            Comp();
            continue;
        default:
            continue;
        }
    }
}

void NewConn()
{
    char hname[100], buf[100];
    int newcid, rc;
    RPC2_HostIdent hident;
    RPC2_PortalIdent pident;
    RPC2_SubsysIdent sident;

    printf("Remote host name? ");
    gets(hident.Value.Name);

    hident.Tag = RPC2_HOSTBYNAME;
    printf("Subsystem? (Auth = %d, Comp = %d) ", AUTHSUBSYSID, COMPSUBSYSID);
    gets(buf);
    sident.Value.SubsysId = atoi(buf);

    sident.Tag                  = RPC2_SUBSYSBYID;
    pident.Tag                  = RPC2_PORTALBYINETNUMBER;
    pident.Value.InetPortNumber = htons(AUTHPORTAL);
    /* same as COMPPORTAL */
    rc = RPC2_Bind(RPC2_OPENKIMONO, NULL, &hident, &pident, &sident, NULL, NULL,
                   NULL, &newcid);
    if (rc == RPC2_SUCCESS)
        printf("Binding succeeded, this connection id is %d\n", newcid);
    else
        printf("Binding failed: %s\n", RPC2_ErrorMsg(rc));
}

void Auth()
{
    RPC2_Handle cid;
    int op, rc, uid;
    char name[100], buf[100];
    AuthInfo ainfo;
    RPC2_BoundedBS bbs;

    printf("Connection id? ");
    gets(buf);
    cid = atoi(buf);
    printf("Operation? (1 = Id, 2 = Name, 3 = Info, 4 = Quit) ");
    gets(buf);
    op = atoi(buf);
    switch (op) {
    case 1:
        printf("Name? ");
        gets(name);
        rc = AuthUserId(cid, name, &uid);
        if (rc == AUTHSUCCESS)
            printf("Id = %d\n", uid);
        else if (rc == AUTHFAILED)
            printf("Bogus user name\n");
        else
            printf("Call failed --> %s\n", RPC2_ErrorMsg(rc));
        break;

    case 2:
        printf("Id? ");
        gets(buf);
        uid           = atoi(buf);
        bbs.MaxSeqLen = sizeof(name);
        bbs.SeqLen    = 0;
        bbs.SeqBody   = (RPC2_ByteSeq)name;
        rc            = AuthUserName(cid, uid, &bbs);
        if (rc == AUTHSUCCESS)
            printf("Name = %s\n", bbs.SeqBody);
        else if (rc == AUTHFAILED)
            printf("Bogus user id\n");
        else
            printf("Call failed --> %s\n", RPC2_ErrorMsg(rc));
        break;

    case 3:
        printf("Id? ");
        gets(buf);
        uid = atoi(buf);
        rc  = AuthUserInfo(cid, uid, &ainfo);
        if (rc == AUTHSUCCESS)
            printf("Group = %d   Home = %s\n", ainfo.GroupId, ainfo.HomeDir);
        else if (rc == AUTHFAILED)
            printf("Bogus user id\n");
        else
            printf("Call failed --> %s\n", RPC2_ErrorMsg(rc));
        break;

    case 4:
        rc = AuthQuit(cid);
        if (rc != AUTHSUCCESS)
            printf("Call failed --> %s\n", RPC2_ErrorMsg(rc));
        RPC2_Unbind(cid);
        break;
    }
}

void Comp()
{
    RPC2_Handle cid;
    int op, rc, x;
    char buf[100];

    printf("Connection id? ");
    gets(buf);
    cid = atoi(buf);
    printf("Operation? (1 = Square, 2 = Cube, 3 = Age, 4 = Quit) ");
    gets(buf);
    op = atoi(buf);
    switch (op) {
    case 1:
        printf("x? ");
        gets(buf);
        x  = atoi(buf);
        rc = CompSquare(cid, x);
        if (rc > 0)
            printf("x**2 = %d\n", rc);
        else
            printf("Call failed --> %s\n", RPC2_ErrorMsg(rc));
        break;

    case 2:
        printf("x? ");
        gets(buf);
        x  = atoi(buf);
        rc = CompCube(cid, x);
        if (rc > 0)
            printf("x**3 = %d\n", rc);
        else
            printf("Call failed --> %s\n", RPC2_ErrorMsg(rc));
        break;

    case 3:
        rc = CompAge(cid);
        if (rc > 0)
            printf("Age of connection = %d seconds\n", rc);
        else
            printf("Call failed --> %s\n", RPC2_ErrorMsg(rc));
        break;

    case 4:
        rc = CompQuit(cid);
        if (rc < 0)
            printf("Call failed --> %s\n", RPC2_ErrorMsg(rc));
        RPC2_Unbind(cid);
        break;
    }
}

/* RPC Initialization and Error handling */
void InitRPC()
{
    int mylpid = -1;

    assert(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) == LWP_SUCCESS);
    assert(RPC2_Init(RPC2_VERSION, 0, NULL, 1, -1, NULL) == RPC2_SUCCESS);
}
```
