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






#include "coda_assert.h"
#include <stdio.h>
#include <sys/types.h>
#include <strings.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>
#include "fail.h"

extern void ntohFF(FailFilter *);
extern void htonFF(FailFilter *);
void PrintError();
static int PartParseArgs(int argc, char ** argv) ;

static char *host1 = NULL;
static char *host2 = NULL;
static short port1 = 0;
static short port2 = 0;

int oldpartition(int argc, char ** argv)
{
    int i;
    unsigned long cid1, cid2;
    struct hostent *he1;
    struct hostent *he2;
    FailFilter filter;
    int rc;


    PartParseArgs(argc, argv);
    InitRPC();

    he1 = gethostbyname(host1);
    if ( he1 == NULL) {
	printf("invalid host %s\n", host1);
	exit(-1);
    }
    he2 = gethostbyname(host2);
    if ( he2 == NULL) {
	printf("invalid host %s\n", host2);
	exit(-1);
    }

    /* bind to each host */
    printf("Trying to bind to %s on port %d...\n", host1, port1);
    rc = NewConn(host1, port1, &cid1);
    if (rc != RPC2_SUCCESS) {
	PrintError("Can't bind", rc);
	exit(-1);
    }
    printf("Bind Succeeded \n");
    RPC2_SetColor(cid1, FAIL_IMMUNECOLOR);

    printf("Trying to bind to %s on port %d...\n", host2, port2);
    rc = NewConn(host2, port2, &cid2);
    if (rc != RPC2_SUCCESS) {
	PrintError("Can't bind", rc);
	exit(-1);
    }
    printf("Bind Succeeded \n");
    RPC2_SetColor(cid2, FAIL_IMMUNECOLOR);
    he1 = gethostbyname(host1);
    CODA_ASSERT(he1 != NULL);
    filter.ip1 = ((unsigned char *)he1->h_addr)[0];
    filter.ip2 = ((unsigned char *)he1->h_addr)[1];
    filter.ip3 = ((unsigned char *)he1->h_addr)[2];
    filter.ip4 = ((unsigned char *)he1->h_addr)[3];
    filter.color = -1;
    filter.lenmin = 0;
    filter.lenmax = 65535;
    filter.factor = 0;
    
	
    /* insert a filter on the recv  side of host 2 */
    if ((rc = InsertFilter(cid2, recvSide, 0, &filter)) < 0) {
	PrintError("Couldn't insert filter", rc);
    }
    /* insert a filter on send side of host 2 */
    if ((rc = InsertFilter(cid2, sendSide, 0, &filter)) < 0) {
	PrintError("Couldn't insert filter", rc);
    }
    he2 = gethostbyname(host2);
    CODA_ASSERT(he2 != NULL);
    filter.ip1 = ((unsigned char *)he2->h_addr)[0];
    filter.ip2 = ((unsigned char *)he2->h_addr)[1];
    filter.ip3 = ((unsigned char *)he2->h_addr)[2];
    filter.ip4 = ((unsigned char *)he2->h_addr)[3];
    if ((rc = InsertFilter(cid1, recvSide, 0, &filter)) < 0) {
	PrintError("Couldn't insert filter", rc);
    }
    /* insert a filter on send side of host 2 */
    if ((rc = InsertFilter(cid1, sendSide, 0, &filter)) < 0) {
	PrintError("Couldn't insert filter", rc);
    }
    
    RPC2_Unbind(cid1);
    RPC2_Unbind(cid2);
    return rc;
}


static int PartParseArgs(int argc, char ** argv) 
{
    int i;
    if (argc != 7) {
	printf("Usage: partition -h hostname port -h hostname port\n");
	exit(-1);
    }
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-h") == 0) {
	    if (!host1) {
		host1 = argv[i+1];
		sscanf(argv[i+2], "%hd", &port1);
		i = i + 2;
	    }
	    else if (!host2) {
		host2 = argv[i+1];
		sscanf(argv[i+2], "%hd", &port2);
		i = i + 2;
	    }
	    else {
		printf("Usage: partition -h hostname port -h hostname port\n");
		exit(-1);
	    }
	}
    }
    return 0;
}
		
