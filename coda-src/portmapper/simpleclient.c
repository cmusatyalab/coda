/*
 *      $Id: simpleclient.c,v 1.6 1998/11/24 15:34:29 jaharkes Exp $   
 */

/* Simple client to excercise the RPC2 procedure calls */

#include "coda_assert.h"

#ifdef __BSD44__
#include <sys/types.h>
#endif
#if !defined(CYGWIN32) & !defined(DJGPP)
#include <arpa/nameser.h>
#endif
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#include <lwp.h>
#include <rpc2.h>
#include <se.h>

#include <ports.h>
#include "portmapper.h"

void main(void)
{
	RPC2_BindParms bp;
	RPC2_HostIdent hident;
	RPC2_PortIdent pident;
	RPC2_SubsysIdent sident;
	RPC2_CountedBS cident;
	long	rc;
	RPC2_Handle	cid;
	RPC2_Integer	port;
	struct timeval	timeout;

	PROCESS mylpid;

	CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);
	/* lwp_debug = 1; */
	CODA_ASSERT(RPC2_Init(RPC2_VERSION, 0, NULL, -1, NULL) > RPC2_ELIMIT);

	hident.Tag = RPC2_HOSTBYNAME;
	strcpy(hident.Value.Name, "localhost");

	pident.Tag = RPC2_PORTBYINETNUMBER;
	pident.Value.InetPortNumber = ntohs(PORT_rpc2portmap);

	sident.Tag = RPC2_SUBSYSBYID;
	sident.Value.SubsysId = htonl(PORTMAPPER_SUBSYSID);

	cident.SeqBody = 0;
	cident.SeqLen = 0;

	timeout.tv_sec = 20;
	timeout.tv_usec = 0;

	bp.SecurityLevel = RPC2_OPENKIMONO;
	bp.EncryptionType = 0;
	/*	bp.timeout = (struct timeval *) NULL; */
	bp.SharedSecret = 0;
	bp.ClientIdent = &cident;
	bp.SideEffectType = 0;
	bp.Color = 0;

	CODA_ASSERT(!(rc = RPC2_NewBinding(&hident, &pident, &sident, &bp, &cid)));

	rc = portmapper_client_lookup_pbynvp(cid, "hithereservice", 0, 17, &port);

	fprintf(stderr, "After initial lookup, rc=%ld, port=%ld\n", rc, port);

	/* register the port */
	rc = portmapper_client_register_excl(cid, "scraw", 0, 17, 12345);

	fprintf(stderr, "After register, rc=%ld\n", rc);

	rc = portmapper_client_lookup_pbynvp(cid, "scraw", 0, 17, &port);

	fprintf(stderr, "After second lookup, rc=%ld, port=%ld\n", rc, port);

	rc = portmapper_client_delete(cid, "scraw", 0, 17);

	fprintf(stderr, "After delete, rc=%ld\n", rc);

	rc = portmapper_client_register_excl(cid, "scraw", 0, 17, 12345);

	fprintf(stderr, "After second register, rc=%ld\n", rc);

	rc = portmapper_client_register_excl(cid, "scraw", 0, 17, 12345);

	fprintf(stderr, "After third register, rc=%ld\n", rc);

	rc = portmapper_client_register_sqsh(cid, "scraw", 0, 17, 12346);

	fprintf(stderr, "After fourth register (sqsh), rc=%ld\n", rc);

	rc= portmapper_client_lookup_pbynvp(cid, "scraw", 0, 17, &port);

	fprintf(stderr, "After third lookup, rc=%ld, port=%ld\n", rc, port);
}
