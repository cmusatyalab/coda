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

#ifndef _AUTH2_COMMON_
#define _AUTH2_COMMON_

/* per-connection info */
struct UserInfo
{
        int ViceId;     /* from NewConnection */
        int HasQuit;    /* TRUE iff Quit() was received on this connection */
        PRS_InternalCPS *UserCPS;
        int LastUsed;   /* timestamped at each RPC call; for gc'ing */
};

#ifdef KERBEROS4
void Krb4Init(void);
long Krb4GetSecret(char *hostname, char **identity, int *identitylen, 
		   char **secret,   int *secretlen);
long Krb4DoKinit();
long Krb4GetKeys(RPC2_CountedBS * cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey);
#endif

#ifdef KERBEROS5
long Krb5Init(char *, char *);
long Krb5DoKinit();
long Krb5GetSecret(char *hostname, char **identity, int *identitylen, 
		   char **secret,   int *secretlen);
long Krb5GetKeys(RPC2_CountedBS * cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey);
#endif

#ifdef sun
/* XXXXX --- should convert to mem functions .... */
char *index(const char *s, int c);
char *rindex(const char *s, int c);
void  bzero(void *b, size_t len);
void  bcopy(const void *src, void *dst, size_t len);

int   inet_aton(const char *cp, struct in_addr *addr);
#endif

#endif
