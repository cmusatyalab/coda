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
                           none currently

#*/

/* main for rpc2portmap daemon */

#include <unistd.h>
#include <sys/param.h>
#include "coda_assert.h"
#include <netinet/in.h>
#include <stdlib.h>

#include <ports.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>

#include "portmapper.h"
#include "map.h"
#include "codaconf.h"
#include "vice_file.h"

#ifdef __CYGWIN32__
extern char *optarg;
#endif

FILE *portmaplog = NULL;
#define PORTMAPLOG vice_sharedfile("misc/portmaplog")

static char *serverconf = SYSCONFDIR "/server"; /* ".conf" */
static char *vicedir = NULL;

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



void
ReadConfigFile()
{
    char confname[MAXPATHLEN];

    /* don't complain if config files are missing */
    codaconf_quiet = 1;

    /* Load configuration file to get vice dir. */
    sprintf (confname, "%s.conf", serverconf);
    (void) conf_init(confname);

    CONF_STR(vicedir,		"vicedir",	   "/vice");

    vice_dir_init(vicedir, 0);
}

int main(int argc, char **argv)
{
	RPC2_PacketBuffer *reqbuffer;
	RPC2_Handle cid;
	int rc;

	/* fork and detach */

	rc = getopt(argc, argv, "d:");

	if ( rc != EOF && rc != 'd' ) {
		fprintf(stderr, "Usage: %s [ -d debuglevel ]\n", argv[0]);
	}

	ReadConfigFile();

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
