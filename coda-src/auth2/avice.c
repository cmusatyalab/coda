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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/auth2/avice.c,v 4.2 1998/08/05 23:49:13 braam Exp $";
#endif /*_BLURB_*/





/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


/*
  Routines used by Vice file servers to do authentication
*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <util.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "auth2.h"

static int Key1IsValid = FALSE;
static int Key2IsValid = FALSE;
static RPC2_EncryptionKey Key1;
static RPC2_EncryptionKey Key2;

extern void ntoh_SecretToken(SecretToken *);

/* prototype */
long GetKeysFromToken(INOUT RPC2_CountedBS *cIdent,
                      OUT RPC2_EncryptionKey hKey,
                      OUT RPC2_EncryptionKey sKey);


/*  Wrapper function when multiple authentication type support was added;
    fits new calling parameters.  We only allow by-token authentication to
    vice
*/

long GetKeys(RPC2_Integer *AuthenticationType, RPC2_CountedBS *cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey)

{
	switch (*AuthenticationType)
	{
		case	AUTH_METHOD_NULL:
			/* we don't like this */
				return -1;
#ifdef PWCODADB
		case	AUTH_METHOD_CODAUSERNAME:
#ifdef VICEPWCODADB
/* to support direct login to the Vice server, you will need to link in
   libpwsupport, and define both of these.  Note that Vice was NOT designed
   to have direct password support, and it has not been tested.  Further
   more this may be a disaster, as libpwsupport requires some init functions
   to be called that are currently not called by Vice.

                   !!!!! DO NOT ENABLE VICEPWCODADB !!!!!
*/

			/* use coda password database */
			return PWGetKeys(cIdent, hKey, sKey);

#else	/* VICEPWCODADB */

			/* we don't like this either */
				return -1;

#endif	/* VICEPWCODADB */
#endif	/* PWCODADB */

		case	AUTH_METHOD_CODATOKENS:
			/* this is a good way to auth to Vice */
				return GetKeysFromToken(cIdent, hKey, sKey);

		case	AUTH_METHOD_PK:
			/* just a reserved constant, thanks */
				return -1;

		case	AUTH_METHOD_KERBEROS4:
			/* do not ever allow this -- does not provide a VID
			   mapping */
				return -1;

		case	AUTH_METHOD_KERBEROS5:
			/* ditto */
				return -1;

		default:
			/* unknown auth type */
				return -1;
	}
}

/*  Use a pointer to this routine in RPC2_GetRequest().  Derives hKey
    and sKey from ClientIdent using one or the other of the global
    server keys to decrypt the secret token in cIdent.  Returns 0 if
    the handshake is to be continued, -1 if not.  The sKey is
    synthesized.  If 0 is returned, cIdent's body has been decrypted
    and converted to host order --- this will be passed on to the new
    connection routine, which can access the fields of SecretToken.  
*/

long GetKeysFromToken(IN RPC2_Integer AuthenticationType, 
		      INOUT RPC2_CountedBS *cIdent, 
		      OUT RPC2_EncryptionKey hKey, 
		      OUT RPC2_EncryptionKey sKey)
{
    SecretToken st;
    register int i;
    struct timeval t;

    /* XXX check that Authentication type is what we want here */

    if (cIdent->SeqLen != sizeof(SecretToken)) {
	LogMsg(-1, 0, stdout, "Invalid length token in GetKeysFromToken");
	return(-1);
    }

    if (Key1IsValid) {
	rpc2_Decrypt((char *)cIdent->SeqBody, (char *)&st, cIdent->SeqLen, 
		     (unsigned char *)Key1, RPC2_XOR);
	if (strncmp((char *)st.MagicString, AUTH_MAGICVALUE, 
		    sizeof(AuthMagic)) == 0) goto GotIt;
    }
    if (Key2IsValid) {
	rpc2_Decrypt((char *)cIdent->SeqBody, (char *)&st, cIdent->SeqLen, 
		     (unsigned char *)Key2, RPC2_XOR);
	if (strncmp((char *)st.MagicString, AUTH_MAGICVALUE, 
		    sizeof(AuthMagic)) == 0) goto GotIt;
    }
    LogMsg(-1, 0, stdout, "Could not get a valid key in GetKeysFromToken");
    return(-1);	/* no valid key did the job */
    
GotIt:
    ntoh_SecretToken(&st);
    
    gettimeofday(&t, 0);
    if (t.tv_sec < st.BeginTimestamp || t.tv_sec > st.EndTimestamp) {
	LogMsg(10, SrvDebugLevel, stdout,
	       "End time stamp %d > time %d for user %d",
		st.EndTimestamp,t.tv_sec,st.ViceId);
	return(-1);
    }
    bcopy(st.HandShakeKey, hKey, sizeof(RPC2_EncryptionKey));
    for (i = 0; i < sizeof(RPC2_EncryptionKey); i++)
	sKey[i] = rpc2_NextRandom(NULL) & 0xff; 	/* new session key */
    bcopy((void *)&st, cIdent->SeqBody, sizeof(SecretToken));   /* to be passed back
						as new connection packet */
    return(0);
}

/* Sets the global server keys to the specified value.  Either of the
   keys may be NULL, in which case the corresponding key is merely
   marked invalid */

void SetServerKeys(IN RPC2_EncryptionKey serverKey1, 
		   IN RPC2_EncryptionKey serverKey2)
{
    bzero(Key1, sizeof(Key1));
    bzero(Key2, sizeof(Key2));

    if (serverKey1 == NULL) 
	    Key1IsValid = FALSE;
    else {
	bcopy(serverKey1, Key1, sizeof(RPC2_EncryptionKey));
	Key1IsValid = TRUE;
    }
    if (serverKey2 == NULL) 
	    Key2IsValid = FALSE;
    else {
	bcopy(serverKey2, Key2, sizeof(RPC2_EncryptionKey));
	Key2IsValid = TRUE;
    }
}
