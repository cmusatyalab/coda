---
title: rtime_clnt.c
---
``` c
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <assert.h>
#include "lwp.h"
#include "rpc2.h"
#include "rtime.h"

void main(int argc, char *argv[])
{
    RPC2_Handle cid;
    RPC2_Integer tv_sec, tv_usec;
    int rc;
    char msg[200];

    if (argc != 2)
        error_report("usage: rtime <machine name>");

    Init_RPC();

    cid = connect_to_machine(argv[1]);

    /* client makes a remote procedure call to get the time on the server
       machine */
    rc = GetRTime(cid, &tv_sec, &tv_usec);
    if (rc != RPC2_SUCCESS) {
        sprintf(msg, "%s\nGet remote time on machine %s failed",
                RPC2_ErrorMsg(rc), argv[1]);
        error_report(msg);
    } else {
        printf("The remote time on machine %s is\n", argv[1]);
        printf("tv_sec = %d and tv_usec = %d\n", tv_sec, tv_usec);
    };

    rc = RPC2_Unbind(cid);
    if (rc != RPC2_SUCCESS)
        error_report("%s\nCant' close the connection!", RPC2_ErrorMsg(rc));
}

void error_report(char *message)
{
    fprintf(stderr, message);
    fprintf(stderr, "\n");
    exit(1);
}

void Init_RPC()
{
    PROCESS mylpid;
    int rc;

    if (LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) != LWP_SUCCESS)
        error_report("Can't Initialize LWP"); /* Initialize LWP package */

    rc = RPC2_Init(RPC2_VERSION, NULL, NULL, 0, -1, NULL);
    if (rc != RPC2_SUCCESS)
        error_report("%s\nCan't Initialize RPC2", RPC2_ErrorMsg(rc));
    /* Initialize RPC2 package */
}

/* This routine tries to establish a connection to the server running on
   machine machine_name */
RPC2_Handle connect_to_machine(char *machine_name)
{
    RPC2_HostIdent hid;
    RPC2_PortalIdent pid;
    RPC2_SubsysIdent sid;
    RPC2_Handle cid;
    char msg[100];
    int rc;
    RPC2_BindParms bp;

    hid.Tag = RPC2_HOSTBYNAME;
    if (strlen(machine_name) >= 64) {
        sprintf(msg, "Machine name %s too long!", machine_name);
        error_report(msg);
    };
    strcpy(hid.Value.Name, machine_name);

    pid.Tag                  = RPC2_PORTALBYINETNUMBER;
    pid.Value.InetPortNumber = htons(RTIMEPORTAL);

    sid.Tag            = RPC2_SUBSYSBYID;
    sid.Value.SubsysId = RTIMESUBSYSID;

    bp.SecurityLevel  = RPC2_OPENKIMONO;
    bp.EncryptionType = NULL;
    bp.SideEffectType = NULL;
    bp.ClientIdent    = NULL;
    bp.SharedSecret   = NULL;

    rc = RPC2_NewBinding(&hid, &pid, &sid, &bp, &Gcid);
    if (rc != RPC2_SUCCESS) {
        sprintf(msg, "%s\nCan't connect to machine %s", RPC2_ErrorMsg(rc),
                machine_name);
        error_report(msg);
    };
    return cid;
}
```
