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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/auth2/oldtokens.cc,v 1.1 1996/11/22 19:09:52 braam Exp $";
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


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#if defined(__linux__) || defined(__NetBSD__)
#include  <stdlib.h>
#include  <unistd.h>
#endif /* __NetBSD__ || LINUX */


#ifdef __cplusplus
}
#endif __cplusplus

#include "auth2.h"

int main(int argc, char **argv);
PRIVATE void readfile(char *path, char *what, char len);
PRIVATE void writefile(char *path, char *what, char len);


int main(int argc, char **argv)
{
	char	path[128], path2[128];
	SecretToken sToken;
	ClearToken cToken;
	int	rc;

	U_InitRPC();
	argc--,argv++;
	while (argc > 0)
	{
		if (strcmp(*argv,"g") == 0 && argc > 1)
		{
			argc--,argv++;
			sprintf(path,"%s.clear",*argv);
			sprintf(path2,"%s.secret",*argv);
			argc--,argv++;
			if (rc = U_GetLocalTokens(&cToken,(EncryptedSecretToken)&sToken))
			{
				fprintf(stderr,"U_GetLocalTokens = %d\n",rc);
				fprintf(stderr,"\terrno = %d\n",errno);
			}
			U_HostToNetClearToken(&cToken);
			writefile(path,(char *)&cToken,sizeof(cToken));
			writefile(path2,(char *)&sToken,sizeof(sToken));
			continue;
		}
		if (strcmp(*argv,"m") == 0 && argc > 2)
		{
			char *pass, *name;
			pass = getpass("Password:");
			argc--,argv++;
			sprintf(path,"%s.clear",*argv);
			sprintf(path2,"%s.secret",*argv);
			argc--,argv++;
			name = *argv;
			argc--,argv++;
			if (rc = U_Authenticate(name,pass,&cToken,(EncryptedSecretToken)&sToken))
			{
				fprintf(stderr,"U_Authenticate = %d\n",rc);
				fprintf(stderr,"\terrno = %d\n",errno);
			}
			U_HostToNetClearToken(&cToken);
			writefile(path,(char *)&cToken,sizeof(cToken));
			writefile(path2,(char *)&sToken,sizeof(sToken));
			continue;
		}
		if (strcmp(*argv,"s") == 0 && argc > 1)
		{
			argc--,argv++;
			sprintf(path,"%s.clear",*argv);
			sprintf(path2,"%s.secret",*argv);
			readfile(path,(char *)&cToken,sizeof(cToken));
			readfile(path2,(char *)&sToken,sizeof(sToken));
			U_NetToHostClearToken(&cToken);
			if (rc = U_SetLocalTokens(1,&cToken,(EncryptedSecretToken)&sToken))
			{
				fprintf(stderr,"U_SetLocalTokens = %d\n",rc);
				fprintf(stderr,"\terrno = %d\n",errno);
			}
			argc--,argv++;
			continue;
		}
		if (strcmp(*argv,"x") == 0 && argc > 1)
		{
			argc--,argv++;
			sprintf(path,"%s.clear",*argv);
			readfile(path,(char *)&cToken,sizeof(cToken));
			fprintf(stderr,"net order:\n");		
			fprintf(stderr,"\tAuthHandle = %d\n",cToken.AuthHandle);
			fprintf(stderr,"\tViceId = %d\n",cToken.ViceId);
			fprintf(stderr,"\tBeginTimestamp = %d\n",cToken.BeginTimestamp);
			fprintf(stderr,"\tEndTimestamp = %d\n",cToken.EndTimestamp);
			U_NetToHostClearToken(&cToken);
			fprintf(stderr,"host order:\n");
			fprintf(stderr,"\tAuthHandle = %d\n",cToken.AuthHandle);
			fprintf(stderr,"\tViceId = %d\n",cToken.ViceId);
			fprintf(stderr,"\tBeginTimestamp = %d\n",cToken.BeginTimestamp);
			fprintf(stderr,"\tEndTimestamp = %d\n",cToken.EndTimestamp);
			argc--,argv++;
			continue;
		}
		fprintf(stderr,
		 "Usage: tokens [x prefix] [g prefix] [m prefix user] [s prefix]\n");
		exit(1);
	}
    return(0); /* dummy to keep g++ happy */
}


PRIVATE void readfile(char *path, char *what, char len)
{
	int	fd;
	if ((fd = open(path,O_RDONLY,0)) < 0)
	{
		perror(path);
		exit(1);
	}
	if (read(fd,what,len) != len)
	{
		perror(path);
		exit(1);
	}
	if (close(fd) < 0)
	{
		perror(path);
		exit(1);
	}
}


PRIVATE void writefile(char *path, char *what, char len)
{
	int	fd;
	if ((fd = open(path,O_WRONLY|O_CREAT,0666)) < 0)
	{
		perror(path);
		exit(1);
	}
	if (write(fd,what,len) != len)
	{
		perror(path);
		exit(1);
	}
	if (close(fd) < 0)
	{
		perror(path);
		exit(1);
	}
}
