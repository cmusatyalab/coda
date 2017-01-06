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

/*
 * Very simple program query the volume databases on Coda servers, it sets up
 * an rpc2 connection and performs a ViceGetVolumeInfo query.
 * 
 *   Jan Harkes, July 2001
 */

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <coda_string.h>
#include <coda_getservbyname.h>

#include "vice.h"

static void Initialize(void)
{
    RPC2_Options options;
    PROCESS pid;
    struct timeval tv;
    long rc;

    /* initialize the subsystems LWP/RPC */
    rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &pid);
    if (rc != LWP_SUCCESS) {
	printf("LWP_Init() failed\n");
	exit(EXIT_FAILURE);
    }

    tv.tv_sec = 15;
    tv.tv_usec = 0;
    
    memset(&options, 0, sizeof(options));
    options.Flags = RPC2_OPTION_IPV6;

    rc = RPC2_Init(RPC2_VERSION, &options, NULL, -1, &tv);
    if (rc != LWP_SUCCESS) {
	printf("RPC_Init() failed\n");
	exit(EXIT_FAILURE);
    }
}

static long Bind(char *host, short port, long subsys, RPC2_Handle *cid)
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
    bindparms.ClientIdent = NULL;

    return RPC2_NewBinding(&hostid, &portid, &subsysid, &bindparms, cid);
}

static char *viceaddr(unsigned long srvaddr)
{
    struct in_addr host;
    host.s_addr = htonl(srvaddr);
    return inet_ntoa(host);
}

int main(int argc, char *argv[])
{
    RPC2_Handle cid;
    long        rc;
    char       *host, *volume;
    short       port;
    long	subsys;
    VolumeInfo  volinfo;

    if (argc <= 2)
        goto badargs;

    subsys = SUBSYS_SRV;

    if (strcmp(argv[1], "-p") == 0) {
        if (argc <= 4)
            goto badargs;

        host = argv[3];
        volume = argv[4];
        port = atoi(argv[2]);
    } else {
	struct servent *s = coda_getservbyname("codasrv", "udp");
	host = argv[1];
	volume = argv[2];
	port = ntohs(s->s_port);
    }

    Initialize();

    rc = Bind(host, port, subsys, &cid);

    if (rc != RPC2_SUCCESS) {
        printf("RPC2 connection to %s:%d failed with %s.\n",
               host, port, RPC2_ErrorMsg(rc));
        exit(EXIT_FAILURE);
    }
    printf("RPC2 connection to %s:%d successful.\n", host, port);

    rc = ViceGetVolumeInfo(cid, (RPC2_String)volume, &volinfo);

    if (rc != RPC2_SUCCESS) {
        printf("ViceGetVolumeInfo for %s to %s:%d failed with %s.\n",
               volume, host, port, RPC2_ErrorMsg(rc));
	RPC2_Unbind(cid);
        exit(EXIT_FAILURE);
    }

    printf("Returned volume information for %s\n", volume);
    printf("\tVolumeId %08x\n", volinfo.Vid);
    switch(volinfo.Type) {
    case ReadOnly:
	printf("\tReadonly clone (type %d)\n", volinfo.Type);
	break;

    case ReadWrite:
	printf("\tReadwrite underlying volume replica (type %d)\n", volinfo.Type);
	break;

    case Backup:
	printf("\tBackup volume (type %d)\n", volinfo.Type);
	break;

    case Replicated:
	printf("\tReplicated volume (type %d)\n", volinfo.Type);
	break;

    default:
	printf("\tUnknown volume type %d\n", volinfo.Type);
	break;
    }
    printf("\n");
    printf("\tType0 id %x\n", volinfo.Type0);
    printf("\tType1 id %x\n", volinfo.Type1);
    printf("\tType2 id %x\n", volinfo.Type2);
    printf("\tType3 id %x\n", volinfo.Type3);
    printf("\tType4 id %x\n", volinfo.Type4);
    printf("\n");
    printf("\tServerCount %d\n", volinfo.ServerCount);
    printf("\tReplica0 id %08x, Server0 %s\n",
	   volinfo.RepVolMap.Volume0, viceaddr(volinfo.Server0));
    printf("\tReplica1 id %08x, Server1 %s\n",
	   volinfo.RepVolMap.Volume1, viceaddr(volinfo.Server1));
    printf("\tReplica2 id %08x, Server2 %s\n",
	   volinfo.RepVolMap.Volume2, viceaddr(volinfo.Server2));
    printf("\tReplica3 id %08x, Server3 %s\n",
	   volinfo.RepVolMap.Volume3, viceaddr(volinfo.Server3));
    printf("\tReplica4 id %08x, Server4 %s\n",
	   volinfo.RepVolMap.Volume4, viceaddr(volinfo.Server4));
    printf("\tReplica5 id %08x, Server5 %s\n",
	   volinfo.RepVolMap.Volume5, viceaddr(volinfo.Server5));
    printf("\tReplica6 id %08x, Server6 %s\n",
	   volinfo.RepVolMap.Volume6, viceaddr(volinfo.Server6));
    printf("\tReplica7 id %08x, Server7 %s\n",
	   volinfo.RepVolMap.Volume7, viceaddr(volinfo.Server7));
    printf("\n");
    printf("\tVSGAddr %x\n", volinfo.VSGAddr);
    printf("\n");

    RPC2_Unbind(cid);
    exit(EXIT_SUCCESS);

badargs:
    printf("Usage %s [-p port] hostname volumename/id\n", argv[0]);
    exit(EXIT_FAILURE);
}

