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

#endif /*_BLURB_*/


/* Standard Coda PW Support Routines

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <sys/param.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <prs.h>
#include <al.h>
#include "auth2.h"
#include "auth2.common.h"
#include "pwdefines.h"

extern int GetViceId(RPC2_CountedBS *cIdent);

/* prototypes */
int IsAdministrator(struct UserInfo *pU);

char *GetVname(int id, char *s);
void InitPW(int firsttime);
int BogusKey(RPC2_EncryptionKey x);
void BuildPWArray(char *fileBuf);
void EnlargePW(int newSize);
int IsAUser(int viceId);
int IsADeletedUser(int viceId);

char DefKey[RPC2_KEYSIZE] = {'\144','\162','\163','\145','\165','\163','\163','\040'};
RPC2_EncryptionKey FileKey;     /* unsigned char causes initialization probs */
RPC2_EncryptionKey NullKey;     /* always full of zeros */
RPC2_EncryptionKey DeleteKey;   /* always full of ones: set in InitAl()
*/  

int AdminID;    /* group Id of system administrators */

char *PWFile   = "/vice/db/auth2.pw";     /* name of password file */
char *LockFile = "/auth2.lock";         /* lock file */
RPC2_EncryptionKey *PWArray = NULL;     /* pointer to array of passwords indexed by ViceId */
int PWTime = 0;                 /* used to tell if pw file has changed */
int PWLen = 0;  /* no of entries in PWArray */
int PWCount = 0;
int CheckOnly = 0;      /* only allow password checking at this server */


/* Functions */

/* cIdent:  body assumed to be an integer representing a ViceId in network
            order; converted to host order
   hKey:    key to do handshake
   sKey:    key to use for remainder of connection's duration */

/*  Coughs up keys for authentication handshake between auth server and client.
    Looks up PWArray for this. Makes sure user exists and has not been
      deleted (i.e., excluded from logging in)
    The cIdent field may specify the client in Vice user name form or
      as an integer, ViceId.  The SeqBody field of cIdent is always a
      string.  If it is a number, it is interpreted as a ViceId.
    Else it is interpreted as a Vice user name. */

long PWGetKeys(RPC2_CountedBS *cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey)
{

	int vid;
	register int i;
	struct stat statbuff;

	LogMsg(0, AuthDebugLevel, stdout, "In PWGetKeys()");

	vid = GetViceId(cIdent);

	LogMsg(0, AuthDebugLevel, stdout, "\tvid = %d", vid);

	if (vid < 0) return(-1);
	if (!IsAUser(vid) || IsADeletedUser(vid)) return(-1);

	if (!stat(PWFile, &statbuff)) {
		if(PWTime != statbuff.st_mtime)
		{
			InitPW(PWNOTFIRSTTIME);
		}
	} else
		LogMsg(0, AuthDebugLevel, stdout, "Problem statting auth2.pw");


	memcpy(hKey, PWArray[vid], RPC2_KEYSIZE);
	rpc2_Decrypt((char *)hKey, (char *)hKey, RPC2_KEYSIZE, (unsigned char *)FileKey, RPC2_XOR);
	for (i=0; i < RPC2_KEYSIZE; i++)
		sKey[i] = rpc2_NextRandom(NULL);	/* random session key */

	return(0);
}

int LockParent(char *fName, int lockType)
{
	/* lockType:    LOCK_SH or LOCK_EX
	   Locks parent directory of fName and returns fd on parent.
	   Used for syncing with mvdb */

	char parent[MAXPATHLEN];
	int pfd;

	if (fName[0] != '/')
	{
		getcwd(parent, sizeof(parent-1));
		strcat(parent, "/");
		strcat(parent, fName);
	}
	else strcpy(parent, fName);
	*rindex(parent, '/') = 0;
		/* parent surely has at least one '/' by now */

	strcat(parent, LockFile);
	if ((pfd = open(parent, O_CREAT | O_RDONLY, 0)) < 0
		|| flock(pfd, lockType) < 0)
	{
		perror(parent);
		abort();
	}

	return(pfd);
}


void InitPW(int firsttime) 
{

	int fd, pfd, i;
	struct stat stbuf;
	char *fbuf;
	char *admin = "system:administrators";	/* CODA_ASSERT dies on literals */

	if (firsttime == PWFIRSTTIME)
	{
		memcpy(FileKey, DefKey, RPC2_KEYSIZE);

		/* moved from void main in auth2: */
		PWLen = 100;	/* length of PW array; this is an initial guess,
				   may increase below */
		PWArray = (RPC2_EncryptionKey *)malloc(PWLen*RPC2_KEYSIZE);
		CODA_ASSERT(PWArray != NULL);
	}
	/* now back to the old InitPW */

	/* Reads in contents of PWFile and builds a sorted list of passwords
	   in PWArray.  Frees existing storage coor to PWArray.
	*/

	pfd = LockParent(PWFile, LOCK_SH);

	if ((fd = open(PWFile, O_RDONLY, 0)) < 0
		|| flock(fd, LOCK_SH) < 0	/* locking is superstitious */
		|| fstat(fd, &stbuf))
	{
		perror(PWFile);
		abort();
	}

	CODA_ASSERT((fbuf = (char *)malloc(1+stbuf.st_size)) != NULL);
	CODA_ASSERT(stbuf.st_size == read(fd, fbuf, stbuf.st_size));	/* entirefile */

	PWTime = stbuf.st_mtime;	/* time on file to check for changes */
	fbuf[stbuf.st_size] = 0;	/* sentinel to stop sscanf() later */
	flock(fd, LOCK_UN);	close(fd);
	flock(pfd, LOCK_UN);	close(pfd);

	CODA_ASSERT(AL_NameToId(admin, &AdminID) == 0);
	for (i = 0; i < RPC2_KEYSIZE; i++)  
		DeleteKey[i] = 0xff;

	BuildPWArray(fbuf);
	free(fbuf);
}

void BuildPWArray(char *fileBuf)
{
	/* fileBuf: pointer to in-core contents of PWFile
	   parses the file buffer and builds up PWArray; sets PWLen and
	   PWCount */
	
	char *nextline, *kk;
	int thisid, i;
	RPC2_EncryptionKey thiskey;
	char holder[3];
	
	PWCount = 0;	/* no of valid entries in PWArray */
	bzero((char *)PWArray, PWLen*RPC2_KEYSIZE);
	nextline = fileBuf;	/* index into start of next line  in fbuf */

	while (TRUE)
	{
		if (index(nextline, '\t') == NULL) break;
		thisid = atoi(nextline);
		if ( thisid < 0 ) 
			goto nextline;
		CODA_ASSERT((kk = index(nextline, '\t')) != NULL);
		kk++;
		for (i = 0; i < RPC2_KEYSIZE; i++)
		{
			int x;
			holder[0] = *(kk++);
			holder[1] = *(kk++);
			holder[2] = '\0';
			CODA_ASSERT( 1 == sscanf(holder, "%2x", &x));
			thiskey[i] = x;
		}

		if (AuthDebugLevel)
		{
			printf("%d\t", thisid);
			for (i = 0; i < RPC2_KEYSIZE; i++)
				printf("%02x", thiskey[i]);
			printf("\n"); fflush(stdout);
		}

		if (thisid >= PWLen)
			EnlargePW(2*thisid);
			/*factor of 2 to prevent frequent realloc() */

		memcpy(PWArray[thisid], thiskey, RPC2_KEYSIZE);
		PWCount++;
	nextline:
		CODA_ASSERT((nextline = index(nextline, '\n')) != NULL);
		nextline++;	/* safe, since fileBuf is NULL-terminated */
	}
}

void EnlargePW(int newSize)
{
	/* Makes PWArray capable of holding at least newSize entries */
	if (newSize < PWLen) return;	/* turkey! */
	LogMsg(0, AuthDebugLevel, stdout, "Reallocing from %d to %d", PWLen,
		newSize);
	PWArray = (RPC2_EncryptionKey *)realloc((char *)PWArray,
		newSize*RPC2_KEYSIZE);
	CODA_ASSERT(PWArray != NULL);
	memset(PWArray[PWLen], 0, (newSize-PWLen)*RPC2_KEYSIZE);
	PWLen = newSize;
}

void AppendPW(int vId, RPC2_EncryptionKey eKey, char *otherInfo, int agentId)
{
	/* eKey:	not yet encrypted with FileKey!!
	   Appends a line to the PWFile for user whose ViceId is vID.
	   Logs each change with the ViceId and the timestamp of the agent
	     performing the change.
	   Also updates PWArray, enlarging it if necessary.
	   A periodic offline program should be run to squish old entries. */

	int fd, pfd, i;
	char buf[100], *bnext;
	RPC2_EncryptionKey tempkey;
	struct stat buff;
	long cl;

	/* Encrypt the key first */
	rpc2_Encrypt((char *)eKey, (char *)tempkey, RPC2_KEYSIZE,
		(char *)FileKey, RPC2_XOR);

	/* Update PWArray after enlarging it */
	if (vId >= PWLen) EnlargePW(2*vId);
	memcpy(PWArray[vId], tempkey, RPC2_KEYSIZE);	/* overwrite existing key */

	/* Build an image of the line to be appended */
	sprintf(buf, "%d\t", vId);
	bnext = &buf[strlen(buf)];
	for (i = 0; i < RPC2_KEYSIZE; i++)
	{
		int x = tempkey[i];
		sprintf(bnext, "%02x", x);
		bnext += 2;
	}
	sprintf(bnext, "\t%s", otherInfo);
	bnext += strlen(otherInfo);
	cl = time(0);
	sprintf(bnext, "\t# By %d at %s", agentId, ctime(&cl));

	/* Now lock parent and append the line */
	pfd = LockParent(PWFile, LOCK_EX);
	if ((fd = open(PWFile, O_WRONLY|O_APPEND, 0)) < 0
		|| flock(fd, LOCK_EX) < 0
		|| write(fd, buf, strlen(buf)) < 0)
	{
		perror(PWFile);
		abort();
	}

	/* Update PWTime because we have the current version of the file */
	fstat(fd, &buff);
	PWTime = buff.st_mtime;

	/* Unlock and quit */
	flock(fd, LOCK_UN); close(fd);
	flock(pfd, LOCK_UN); close(pfd);
}


/* Add a user to the PW database */
long PWNewUser(RPC2_Handle cid, RPC2_Integer viceId, RPC2_EncryptionKey initKey, RPC2_String otherInfo)
{
	struct UserInfo *p;

	/* Do not allow if this is a read only server */
	if(CheckOnly)
		return(AUTH_READONLY);

	/* make sure it's a system administrator */
	RPC2_GetPrivatePointer(cid, (char **)&p);
	if (p == NULL || p->HasQuit == TRUE) return(AUTH_FAILED);
	p->LastUsed = time(0);
	if (!IsAdministrator(p))
	{
		char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
		LogMsg(-1, 0, stdout, "AuthNewUser() attempt on  %s by %s denied", GetVname(viceId, buf1), GetVname(p->ViceId, buf2));
		return(AUTH_DENIED);
	}

	if (IsAUser(viceId)) return(AUTH_FAILED);

	/* make the change */
	if (BogusKey(initKey)) return(AUTH_BADKEY);
	AppendPW(viceId, initKey, (char *)otherInfo, p->ViceId);
	return(AUTH_SUCCESS);
}

/* remove a user from the password file */
long PWDeleteUser(RPC2_Handle cid, RPC2_Integer viceId)
{
	struct UserInfo *p;

	/* Do not allow if this is a read only server */
	if (CheckOnly)
		return(AUTH_READONLY);

	/* make sure it's a system administrator */
	RPC2_GetPrivatePointer(cid, (char **)&p);
	if (p == NULL || p->HasQuit == TRUE) 
		return(AUTH_FAILED);
	p->LastUsed = time(0);
	if (!IsAdministrator(p))
	{
		char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
		LogMsg(-1, 0, stdout, "AuthDeleteUser() attempt on  %s by %s denied", GetVname(viceId, buf1), GetVname(p->ViceId, buf2));
		return(AUTH_DENIED);
	}
	if (!IsAUser(viceId)) return(AUTH_FAILED);

	/* make the change */

	AppendPW(viceId, DeleteKey, "", p->ViceId);
	return(AUTH_SUCCESS);
}

/* change user info string, etc */
long PWChangeUser(RPC2_Handle cid, RPC2_Integer viceId, RPC2_EncryptionKey newKey, RPC2_String otherInfo)
{
	int i;
	struct UserInfo *p;

	if (AuthDebugLevel)
	{
		printf("AuthChangeUser(%ld, \"", viceId);
		for (i = 0; i < RPC2_KEYSIZE; i++)
			printf("%c", newKey[i]);
		printf("\", \"%s\")\n", (char *)otherInfo);
	}

	/* Do not allow if this is a read only server */
	if(CheckOnly)
		return(AUTH_READONLY);

	/* make sure it's a system administrator */
	RPC2_GetPrivatePointer(cid, (char **)&p);
	if (p == NULL || p->HasQuit == TRUE) return(AUTH_FAILED);
	p->LastUsed = time(0);
	if (!IsAdministrator(p))
	{
		char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
		LogMsg(-1, 0, stdout, "AuthChangeUser() attempt on  %s by %s denied", GetVname(viceId, buf1), GetVname(p->ViceId, buf2));
		return(AUTH_DENIED);
	}

	if (!IsAUser(viceId)) return(AUTH_FAILED);

	/* make the change */
	if (BogusKey(newKey)) return(AUTH_BADKEY);
	AppendPW(viceId, newKey, (char *)otherInfo, p->ViceId);
	return(AUTH_SUCCESS);
}
		
/* Change user's password in the PW database */
long PWChangePasswd(RPC2_Handle cid, RPC2_Integer viceId, RPC2_String Passwd)
{
	int i;
	struct UserInfo *p;
	RPC2_EncryptionKey newPasswd;
	int len;

	bzero(newPasswd, sizeof(newPasswd));
	if (strlen((char *) Passwd) < RPC2_KEYSIZE)
		len = strlen((char *) Passwd);
	else
		len = RPC2_KEYSIZE;
	memcpy(newPasswd, Passwd, len);

	if (AuthDebugLevel)
	{
		printf("AuthChangePasswd(%ld, \"", viceId);
		for (i = 0; i < RPC2_KEYSIZE; i++)
			printf("%c", newPasswd[i]);
		printf("\")");
	}

	/* Do not allow if this is a read only server       */
	if(CheckOnly)
		return(AUTH_READONLY);

	/* Ensure it's a system administrator or the user himself */
	RPC2_GetPrivatePointer(cid, (char **)&p);
	if (p == NULL || p->HasQuit == TRUE) return(AUTH_FAILED);
	p->LastUsed = time(0);
	if (viceId != p->ViceId && !IsAdministrator(p))
	{
		char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
		LogMsg(-1, 0, stdout, "AuthChangePassd() attempt on %s by %s denied", GetVname(viceId, buf1), GetVname(p->ViceId, buf2));
		return(AUTH_DENIED);
	}

	if (!IsAUser(viceId) || IsADeletedUser(viceId)) return(AUTH_FAILED);

	/* Make the change */
	if (BogusKey(newPasswd)) return(AUTH_BADKEY);
	AppendPW(viceId, newPasswd, "", p->ViceId);
	return (AUTH_SUCCESS);
}

/* Returns TRUE iff viceId corr to a Vice user (==> non-zero key) */
int IsAUser(int viceId)
{
	if (viceId < 0 || viceId >= PWLen) return (FALSE);  
	if (bcmp(PWArray[viceId], NullKey, RPC2_KEYSIZE) == 0) return (FALSE);
	return(TRUE);
}

/* Returns TRUE iff viceId corr to a deleted Vice user(==> key of all 1's) */
int IsADeletedUser(int viceId)
{
	if (viceId < 0 || viceId >= PWLen) return (FALSE);
	if (bcmp(PWArray[viceId], DeleteKey, RPC2_KEYSIZE) == 0) return (TRUE);
	return(FALSE);
}

/* Returns TRUE iff the CPS corr to pU is that of a system administrator.
   Obtains the CPS for pU if necessary */
int IsAdministrator(struct UserInfo *pU)
{
	if (pU->UserCPS == NULL)
		if (AL_GetInternalCPS(pU->ViceId, &pU->UserCPS) != 0)
			return(FALSE);
	if (AL_IsAMember(AdminID, pU->UserCPS) != 0)
		return(FALSE);
	return(TRUE);
}

/* Returns TRUE iff x would result in NullKey or DeleteKey when encrypted with
   FileKey.  FALSE otherwise */
int BogusKey(RPC2_EncryptionKey x)
{
	RPC2_EncryptionKey temp;
	rpc2_Encrypt((char *)x, (char *)temp, RPC2_KEYSIZE, (char *)FileKey, RPC2_XOR);
	if (bcmp(temp, NullKey, RPC2_KEYSIZE) == 0) return(TRUE);
	if (bcmp(temp, DeleteKey, RPC2_KEYSIZE) == 0) return(TRUE);
	return(FALSE);
}

/* given an id, get a name */
char *GetVname(int id, char *s) 
{
	if (AL_IdToName(id, s) < 0)
		sprintf(s, "%d (unknown Vice id)", id);
	return(s);
}



