/* $Id: rpc2portmap.c,v 1.2 98/08/05 23:49:33 braam Exp $ */

/* main for rpc2portmap daemon */

#include <unistd.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdlib.h>


#include <ports.h>
#include <lwp.h>
#include <rpc2.h>
#include <se.h>

#include "portmapper.h"
#include "map.h"

#ifdef __CYGWIN32__
extern char *optarg;
#endif

FILE *portmaplog = NULL;
#define PORTMAPLOG "/vice/srv/portmaplog"

void InitRPC2(void)
{
	PROCESS mylpid;
	RPC2_Integer rc;
	RPC2_PortalIdent portalid;
	RPC2_SubsysIdent subsysid;

	assert(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);
	portalid.Tag = RPC2_PORTALBYINETNUMBER;
	portalid.Value.InetPortNumber = ntohs(PORT_rpc2portmap);

	if ((rc = RPC2_Init(RPC2_VERSION, 0, &portalid, -1, NULL)) != RPC2_SUCCESS)
	{
		fprintf(portmaplog, "InitRPC: RPC2_Init() failed with (%ld) %s\n", rc, RPC2_ErrorMsg(rc));
		exit(-1);
	}

	subsysid.Tag = RPC2_SUBSYSBYID;
	subsysid.Value.SubsysId = ntohl(PORTMAPPER_SUBSYSID);
	assert(RPC2_Export(&subsysid) == RPC2_SUCCESS);
}


void main(int argc, char **argv)
{
	RPC2_PacketBuffer *reqbuffer;
	RPC2_Handle cid;
	int rc;

	/* fork and detach */

	rc = getopt(argc, argv, "d:");

	if ( rc != EOF && rc != 'd' ) {
		fprintf(stderr, "Usage: %s [ -d debuglevel ]\n", argv[0]);
	}

	portmaplog = fopen(PORTMAPLOG, "a+");
	if ( ! portmaplog ) { 
		perror("opening portmaplog");
		exit(1);
	}

	if ( rc == 'd' ) {
		RPC2_SetLog(portmaplog, atoi(optarg));
	} else {
		if (fork())
			{
				/* parent */
				exit(0);
			}
		rc = setsid();
		if ( rc < 0 ) {
			fprintf(portmaplog, "Error detaching from terminal.\n");
			exit(1);
		}
	}
	/* child */

	InitRPC2();

	initnamehashtable();

	while (1)
	{
		cid = 0;
		rc = RPC2_GetRequest(0, &cid, &reqbuffer, 0, 0, RPC2_XOR, 0);
		if (rc < RPC2_WLIMIT)
		{
			fprintf(stderr, "main: (%d) %s\n", rc,
				RPC2_ErrorMsg(rc));
		}
		else
		{
			/* got a request */
			rc = portmapper_ExecuteRequest(cid, reqbuffer, (SE_Descriptor *)0);
			if (rc < RPC2_WLIMIT)
			{
				fprintf(stderr, "main: (%d) %s\n", rc,
					RPC2_ErrorMsg(rc));
			}
		}
	}
}
