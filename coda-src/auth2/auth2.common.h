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
void Krb4Init(char *);
long Krb4GetSecret(char *hostname, char **identity, int *identitylen, 
		   char **secret,   int *secretlen);
long Krb4DoKinit();
long Krb4GetKeys(RPC2_CountedBS * cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey);
#endif

#ifdef KERBEROS5
long Krb5Init(char *);
long Krb5DoKinit();
long Krb5GetSecret(char *hostname, char **identity, int *identitylen, 
		   char **secret,   int *secretlen);
long Krb5GetKeys(RPC2_CountedBS * cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey);
#endif


#endif
