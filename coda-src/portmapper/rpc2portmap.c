/* $Id: rpc2portmap.c,v 1.4 98/11/02 16:44:59 rvb Exp $ */

/* main for rpc2portmap daemon */

#include <unistd.h>
#include "coda_assert.h"
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
	RPC2_PortIdent portid;
	RPC2_SubsysIdent subsysid;

	CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);
	portid.Tag = RPC2_PORTBYINETNUMBER;
	portid.Value.InetPortNumber = ntohs(PORT_rpc2portmap);

	if ((rc = RPC2_Init(RPC2_VERSION, 0, &portid, -1, NULL)) != RPC2_SUCCESS)
	{
		fprintf(portmaplog, "InitRPC: RPC2_Init() failed with (%ld) %s\n", rc, RPC2_ErrorMsg(rc));
		exit(-1);
	}

	subsysid.Tag = RPC2_SUBSYSBYID;
	subsysid.Value.SubsysId = ntohl(PORTMAPPER_SUBSYSID);
	CODA_ASSERT(RPC2_Export(&subsysid) == RPC2_SUCCESS);
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
