#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/fail/Attic/heal.c,v 4.6 1998/08/05 23:49:25 braam Exp $";
#endif /*_BLURB_*/






#include <assert.h>
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

static int HealParseArgs(int argc, char ** argv) ;
static char *host1 = NULL;
static char *host2 = NULL;
static short port1 = 0;
static short port2 = 0;

#define HOSTADDRESSEQUAL(he, ip1, ip2, ip3, ip4)	\
    (((((unsigned char *)(he)->h_addr)[0]) == (ip1)) &&	\
     ((((unsigned char *)(he)->h_addr)[1]) == (ip2)) &&	\
     ((((unsigned char *)(he)->h_addr)[2]) == (ip3)) &&	\
     ((((unsigned char *)(he)->h_addr)[3]) == (ip4)))
int heal(int argc, char ** argv)
{
    int i;
    unsigned long cid1, cid2;
    struct hostent *he1;
    struct hostent *he2;
    FailFilter filter;
    FailFilter filters[32];
    int rc;


    HealParseArgs(argc, argv);
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
    
    /* get the filters and remove them */
    {
	/* get filters from recv Side for client 1 */
	RPC2_BoundedBS filtersBS;
	FailFilterSide side;	
	int j;

	filtersBS.MaxSeqLen = sizeof(filters);
	filtersBS.SeqLen = sizeof(filters);
	filtersBS.SeqBody = (RPC2_ByteSeq) filters;
	
	for (i = 0; i < 2; i++) {
	    if (i == 0) side = sendSide;
	    else side = recvSide;
	    if (rc = GetFilters(cid1, side, &filtersBS)) {
		PrintError("Couldn't GetFilters for host 1", rc);
		exit(-1);
	    }
	    rc = CountFilters(cid1, side);
	    if (rc < 0) {
		PrintError("Couldn't CountFilters for host1", rc);
		exit(-1);
	    }
	    for (j = 0; j < rc; j++) 
		ntohFF(&filters[j]);
	    he2 = gethostbyname(host2);
	    assert(he2);
	    for (j = 0; j < rc; j++) {
		/* if filter matches then  remove it */
		int code;
		if (HOSTADDRESSEQUAL(he2, filters[j].ip1, filters[j].ip2, 
				     filters[j].ip3, filters[j].ip4)) {
		    printf("removing filter %d from host1\n", filters[j].id);
		    if (code = RemoveFilter(cid1, side, filters[j].id)) {
			PrintError("Couldn't remove filter\n", code);
			break;
		    }
		}
	    }
	}

	filtersBS.MaxSeqLen = sizeof(filters);
	filtersBS.SeqBody = (RPC2_ByteSeq) filters;
	
	for (i = 0; i < 2; i++) {
	    if (i == 0) side = sendSide;
	    else side = recvSide;
	    if (rc = GetFilters(cid2, side, &filtersBS)) {
		PrintError("Couldn't GetFilters for host2", rc);
		exit(-1);
	    }
	    rc = CountFilters(cid2, side);
	    if (rc < 0) {
		PrintError("Couldn't CountFilters for host 2", rc);
		exit(-1);
	    }
	    for (j = 0; j < rc; j++) 
		ntohFF(&filters[j]);
	    he1 = gethostbyname(host1);
	    assert(he1);
	    for (j = 0; j < rc; j++) {
		/* if filter matches then  remove it */
		int code;
		if (HOSTADDRESSEQUAL(he1, filters[j].ip1, filters[j].ip2, 
				     filters[j].ip3, filters[j].ip4)) {
		    printf("removing filter %d from host2\n", filters[j].id);
		    if (code = RemoveFilter(cid2, side, filters[j].id)) {
			PrintError("Couldn't remove filter from host 2\n", code);
			break;
		    }
		}
	    }
	}
	
    }
    RPC2_Unbind(cid1);
    RPC2_Unbind(cid2);
}



int HealParseArgs(int argc, char ** argv) 
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
}
		
