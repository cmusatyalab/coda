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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/


/*
  Routines used by Vice file servers to do authentication
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <util.h>
#include <rpc2/rpc2.h>
#include <avice.h>

#ifdef __cplusplus
}
#endif

#include "auth2.h"

static int Key1IsValid = FALSE;
static int Key2IsValid = FALSE;
static RPC2_EncryptionKey Key1;
static RPC2_EncryptionKey Key2;

extern void ntoh_SecretToken(SecretToken *);

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
				return GetKeysFromToken(AuthenticationType, cIdent, hKey, sKey);

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

long GetKeysFromToken(IN RPC2_Integer *AuthenticationType,
		      INOUT RPC2_CountedBS *cIdent, 
		      OUT RPC2_EncryptionKey hKey, 
		      OUT RPC2_EncryptionKey sKey)
{
    SecretToken st;
    int i;
    struct timeval t;

    if (!cIdent) {
        /* unauthenticated (RPC2_OPENKIMONO) connections are allowed */
        return(0);
    }

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
    memcpy(hKey, st.HandShakeKey, sizeof(RPC2_EncryptionKey));
    for (i = 0; i < sizeof(RPC2_EncryptionKey); i++)
	sKey[i] = rpc2_NextRandom(NULL) & 0xff; 	/* new session key */
    memcpy(cIdent->SeqBody, &st, sizeof(SecretToken));
    /* to be passed back as new connection packet */
    return(0);
}

/* Sets the global server keys to the specified value.  Either of the
   keys may be NULL, in which case the corresponding key is merely
   marked invalid */

void SetServerKeys(IN RPC2_EncryptionKey serverKey1, 
		   IN RPC2_EncryptionKey serverKey2)
{
    memset(Key1, 0, sizeof(Key1));
    memset(Key2, 0, sizeof(Key2));

    if (serverKey1 == NULL) 
	    Key1IsValid = FALSE;
    else {
	memcpy(Key1, serverKey1, sizeof(RPC2_EncryptionKey));
	Key1IsValid = TRUE;
    }
    if (serverKey2 == NULL) 
	    Key2IsValid = FALSE;
    else {
	memcpy(Key2, serverKey2, sizeof(RPC2_EncryptionKey));
	Key2IsValid = TRUE;
    }
}
