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
 * Very simple program to monitor Coda servers, it only sets up an rpc2
 * connection, so it won't catch internal server problems. But it does
 * detect a crashed server.
 * 
 *   Jan Harkes, September 1999
 */

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <ports.h>
#include <coda_string.h>

void Initialize(void)
{
    PROCESS pid;
    struct timeval tv;
    long rc;

    /* initialize the subsystems LWP/RPC */
    rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &pid);
    if (rc != LWP_SUCCESS) {
	printf("LWP_Init() failed\n");
	exit(-1);
    }

    tv.tv_sec = 15;
    tv.tv_usec = 0;
    rc = RPC2_Init(RPC2_VERSION, 0, 0, -1, &tv);
    if (rc != LWP_SUCCESS) {
	printf("RPC_Init() failed\n");
	exit(-1);
    }
}

long Bind(char *host, short port, long subsys, RPC2_Handle *cid)
{
    RPC2_HostIdent   hostid;
    RPC2_PortIdent   portid;
    RPC2_SubsysIdent subsysid;
    RPC2_BindParms   bindparms;

    /* Initialize connection stuff */
    hostid.Tag = RPC2_HOSTBYNAME;
    strcpy(hostid.Value.Name, host);

    portid.Tag = RPC2_PORTBYINETNUMBER;
    portid.Value.InetPortNumber = htons(port);

    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId= subsys;

    bindparms.SideEffectType = 0;
    bindparms.SecurityLevel = RPC2_OPENKIMONO;
    bindparms.ClientIdent = 0;

    return RPC2_NewBinding(&hostid, &portid, &subsysid, &bindparms, cid);
}

int main(int argc, char *argv[])
{
    RPC2_Handle cid;
    long	subsys = 1001;
    long        rc;
    char       *host;
    short       port;

    if (argc == 1)
        goto badargs;

    if (strcmp(argv[1], "-p") == 0) {
        if (argc < 4)
            goto badargs;

        host = argv[3];
        port = atoi(argv[2]);
    } else {
        host = argv[1];
        port = PORT_codasrv; /* codasrv portnumber */
	subsys = 1001;       /* SUBSYS_SRV */
    }

    Initialize();

    rc = Bind(host, port, subsys, &cid);

    RPC2_Unbind(cid);

    if (rc != RPC2_SUCCESS) {
        printf("RPC2 connection to %s:%d failed with %s.\n",
               host, port, RPC2_ErrorMsg(rc));
        exit(2);
    }
    
    printf("RPC2 connection to %s:%d successful.\n", host, port);
    exit(0);

badargs:
    printf("Usage %s [-p port] hostname\n", argv[0]);
    exit(-1);
}

