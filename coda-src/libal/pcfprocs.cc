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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/libal/Attic/pcfprocs.cc,v 4.1 1997/01/08 21:49:46 rvb Exp $";
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
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "pcf.h"	/* To ensure that shared global declarations are consistent */

/*------------------------------ Shared Globals ------------------------------------------*/
unsigned PDBCheckSum;
int HighestUID,  HighestGID, LitPoolSize, SeekOfEmptyEntry;

char *LitPool;
int *Uoffsets, *Useeks, *Usorted;
int *Gsorted, *Goffsets, *Gseeks;
/*------------------------------End of Shared Globals ------------------------------------------*/

extern int errno;
PRIVATE void BigSwap(INOUT int where[], IN int HowMany);
PRIVATE void BigUnswap(INOUT int where[], IN int HowMany);
PRIVATE int fullread(IN int fd, OUT char *buff, IN int nbytes);
PRIVATE int fullwrite(IN int fd, INOUT char *buff, IN int nbytes);




/* ----------------------------------------------------------------------*/
int pcfRead(IN char *pcfile)
    /*  On exit, the globals are allocated and filled from the file pcfile.
	Returns 0 on success, -1 on failure of any kind. */
    {
    int pcfFD, howmanybytes, dobyteswap, buf[4];
    unsigned magic;
    char firstline[PCF_FIRSTLINELEN];

#define	ERROR {flock(pcfFD, LOCK_UN); close(pcfFD); return(-1);}	/* Local to pcfRead */

		
    dobyteswap = 0; 
    if (htonl(1) != 1)	dobyteswap = 1;

    if ((pcfFD= open(pcfile, O_RDONLY, 0)) < 0)
	{
	perror(pcfile);
	ERROR;
	}
	
    if (flock(pcfFD, LOCK_SH) < 0)	
	{
	perror("pcfRead");
	ERROR;
	}
    
    /* Read the fixed length line containing the .pdb checksum and creation timestamp in ASCII */
    if (fullread(pcfFD, (char *) firstline, PCF_FIRSTLINELEN) != PCF_FIRSTLINELEN)
	{
	perror("Header: fullread");
	ERROR;
	}
    if (sscanf(firstline, "%d%u", &magic, &PDBCheckSum) != 2 || magic != PCF_MAGIC)
	{
	/* Assume this is an old style .pcf file for now */
	fprintf(stderr, "WARNING: BAD MAGIC NUMBER .... ASSUMING OLD FORMAT .PCF FILE\n");
	fflush(stderr);
	lseek(pcfFD, 0, L_SET);	/* rewind file to beginning */
	PDBCheckSum = 0;	/* indicates .pcf file had no check sum */
	}
    
    /* Now read header */
    if (fullread(pcfFD, (char *) buf, sizeof(buf)) != sizeof(buf))
	{
	perror("Header: fullread");
	ERROR;
	}
    HighestUID = ntohl(buf[0]);
    HighestGID = ntohl(buf[1]);
    LitPoolSize = ntohl(buf[2]);
    SeekOfEmptyEntry = ntohl(buf[3]);
    /* Allocate tables */
    if ( (Uoffsets = (int *)calloc(HighestUID+1, sizeof(int))) == 0)
	ERROR;
    if ( (Usorted = (int *)calloc(HighestUID+1, sizeof(int))) == 0)
	ERROR;
    if ( (Useeks = (int *)calloc(HighestUID+1, sizeof(int))) == 0)
	ERROR;

    if ( (Goffsets = (int *)calloc(1-HighestGID, sizeof(int))) == 0)
	ERROR;
    if ( (Gsorted = (int *)calloc(1-HighestGID, sizeof(int))) == 0)
	ERROR;
    if ( (Gseeks = (int *)calloc(1-HighestGID, sizeof(int))) == 0)
	ERROR;

    if ( (LitPool = (char *)malloc(LitPoolSize)) == 0)
	ERROR;
	
    /* Read User tables */
    howmanybytes = sizeof(int)*(HighestUID+1);
    if (fullread(pcfFD, (char *) Uoffsets, howmanybytes) != howmanybytes)
	{
	perror("Uoffsets: fullread");
	ERROR;
	}

    if (fullread(pcfFD, (char *) Usorted, howmanybytes) != howmanybytes)
	{
	perror("Usorted: fullread");
	ERROR;
	}

    if (fullread(pcfFD, (char *) Useeks, howmanybytes) != howmanybytes)
	{
	perror("Useeks: fullread");
	ERROR;
	}

    if (dobyteswap)
	{
	BigUnswap(Uoffsets, HighestUID+1);
	BigUnswap(Usorted, HighestUID+1);
	BigUnswap(Useeks, HighestUID+1);
	}

    /* Read Group tables */
    howmanybytes = sizeof(int)*(1-HighestGID);
    if (fullread(pcfFD, (char *) Goffsets, howmanybytes) != howmanybytes)
	{
	perror("Goffsets: fullread");
	ERROR;
	}

    if (fullread(pcfFD, (char *) Gsorted, howmanybytes) != howmanybytes)
	{
	perror("Gsorted: fullread");
	ERROR;
	}

    if (fullread(pcfFD, (char *) Gseeks, howmanybytes) != howmanybytes)
	{
	perror("Gseeks: fullread");
	ERROR;
	}

    if (dobyteswap)
	{
	BigUnswap(Goffsets, 1-HighestGID);
	BigUnswap(Gsorted, 1-HighestGID);
	BigUnswap(Gseeks, 1-HighestGID);
	}

    /* Write literal pool */
    
    if (fullread(pcfFD, (char *) LitPool, LitPoolSize) != LitPoolSize)
	{
	perror("LitPool: fullread");
	ERROR;
	}


    if (flock(pcfFD, LOCK_UN) < 0)
	{
	perror("pcfRead");
	return(-1);
	}
    close(pcfFD);
    return(0);

#undef ERROR
    }



int pcfWrite(IN char *pcfile)
    /*  On entry, the globals are assumed to hold the data to be written out.
	Constructs the .pcf file defined by pcfile.
	Returns 0 on success, -1 on failure of any kind.
    */
    {
    int pcfFD, howmanybytes, dobyteswap, buf[4];
    char firstline[PCF_FIRSTLINELEN];
    struct timeval t;

#define	ERROR {flock(pcfFD, LOCK_UN); close(pcfFD); return(-1);}	/* Local to pcfWrite*/

    dobyteswap = FALSE;
    if (htonl(1) != 1)	dobyteswap = TRUE;

    if ((pcfFD= open(pcfile, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0)
	{
	perror(pcfile);
	ERROR;
	}
    
    if (flock(pcfFD, LOCK_EX|LOCK_NB) < 0)
	{
	perror("pcfWrite");
	ERROR;
	}

    /* Write out first line in ASCII */
    if (PDBCheckSum != 0)
	{
	gettimeofday(&t, 0);
	bzero(firstline, PCF_FIRSTLINELEN);
	sprintf(firstline, "%d\t%u\t%s", PCF_MAGIC, PDBCheckSum, ctime((const long int *) &t.tv_sec));

	if (fullwrite(pcfFD, (char *) firstline, PCF_FIRSTLINELEN) != PCF_FIRSTLINELEN)
	    {
	    perror("fullwrite");
	    ERROR;
	    }
	}
    else
	{
	fprintf(stderr, "WARNING: PDBCheckSum is 0. Old style .pcf file will be produced.\n");
	fflush(stderr);
	}

    /* Write header */
    buf[0] = htonl(HighestUID);
    buf[1] = htonl(HighestGID);
    buf[2] = htonl(LitPoolSize);
    buf[3] = htonl(SeekOfEmptyEntry);
    if (fullwrite(pcfFD, (char *) buf, sizeof(buf)) != sizeof(buf))
	{
	perror("fullwrite");
	ERROR;
	}
	
    /* Write User tables */
    if (dobyteswap)
	{
	BigSwap(Uoffsets, HighestUID+1);
	BigSwap(Usorted, HighestUID+1);
	BigSwap(Useeks, HighestUID+1);
	}
    howmanybytes = sizeof(int)*(HighestUID+1);
    if (fullwrite(pcfFD, (char *) Uoffsets, howmanybytes) != howmanybytes)
	{
	perror("fullwrite");
	ERROR;
	}

    if (fullwrite(pcfFD, (char *) Usorted, howmanybytes) != howmanybytes)
	{
	perror("fullwrite");
	ERROR;
	}

    if (fullwrite(pcfFD, (char *) Useeks, howmanybytes) != howmanybytes)
	{
	perror("fullwrite");
	ERROR;
	}

    if (dobyteswap)
	{
	BigUnswap(Uoffsets, HighestUID+1);
	BigUnswap(Usorted, HighestUID+1);
	BigUnswap(Useeks, HighestUID+1);
	}

    /* Write Group tables */
    if (dobyteswap)
	{
	BigSwap(Goffsets, 1-HighestGID);
	BigSwap(Gsorted, 1-HighestGID);
	BigSwap(Gseeks, 1-HighestGID);
	}
    howmanybytes = sizeof(int)*(1-HighestGID);
    if (fullwrite(pcfFD, (char *) Goffsets, howmanybytes) != howmanybytes)
	{
	perror("fullwrite");
	ERROR;
	}

    if (fullwrite(pcfFD, (char *) Gsorted, howmanybytes) != howmanybytes)
	{
	perror("fullwrite");
	ERROR;
	}

    if (fullwrite(pcfFD, (char *) Gseeks, howmanybytes) != howmanybytes)
	{
	perror("fullwrite");
	ERROR;
	}

    if (dobyteswap)
	{
	BigUnswap(Goffsets, 1-HighestGID);
	BigUnswap(Gsorted, 1-HighestGID);
	BigUnswap(Gseeks, 1-HighestGID);
	}

    /* Write literal pool */
    
    howmanybytes = LitPoolSize;
    if (fullwrite(pcfFD, (char *) LitPool, howmanybytes) != howmanybytes)
	{
	perror("fullwrite");
	ERROR;
	}

    if (flock(pcfFD, LOCK_UN) < 0)
	{
	perror("pcfWrite");
	close(pcfFD);
	return(-1);
	}
    close(pcfFD);
    return(0);
    }


int CmpUn(IN int *u1, IN int *u2)
    /* For sorting user names using qsort() and in binary searches.
	u1, u2 are  pointers to elements of Usorted[]*/
    {
    if (Uoffsets[*u1] == -1 && Uoffsets[*u2] == -1) return(0);
    if (Uoffsets[*u1] == -1) return(1);
    if (Uoffsets[*u2] == -1) return(-1);
    return( CaseFoldedCmp(LitPool+Uoffsets[*u1], LitPool+Uoffsets[*u2]) );
    }

int CmpGn(IN int *g1, IN int *g2)
    /* For sorting group names using qsort() and for binary searches.
	g1, g2 are  pointers to elements of Gsorted[] */
    {
    if (Goffsets[*g1] == -1 && Goffsets[*g2] == -1) return(0);
    if (Goffsets[*g1] == -1) return(1);
    if (Goffsets[*g2] == -1) return(-1);
    return( CaseFoldedCmp(LitPool+Goffsets[*g1], LitPool+Goffsets[*g2]) );
    }

PRIVATE void BigSwap(INOUT int where[], IN int HowMany)
    {
    register int i;
    for (i = 0; i < HowMany; i++)
	where[i] = htonl(where[i]);
    }


PRIVATE void BigUnswap(INOUT int where[], IN int HowMany)
    {
    register int i;
    for (i = 0; i < HowMany; i++)
	where[i] = ntohl(where[i]);
    }


PRIVATE int fullread(IN int fd, OUT char *buff, IN int nbytes)
    /* 
	Read data from open file descriptor fd.

	Does what the Unix read(2) call really should do:
	Either successfully reads nbytes, or fails.  Similar to the Unix fread(3)
	call in this respect, except that one level of data copying is avoided.
	

	On exit, one of the following is true:
	    1. nbytes is returned, indicating a successful full read.
	    2. 0 is returned, indicating end-of-file.  All bytes upto
		end-of-file are read, but the caller has no way of 
		knowing how many bytes were actually read.
	    3. -1 is returned.  This means that a Unix read() returned
		-1, and errno has the value set by Unix.
		
	It is expected that the caller know, a priori, that there are at least
	nbytes bytes left before the end of file.  Hence the normal case is that
	the value returned is nbytes.
	
	Bogosities such as EINTR are not seen by the caller.

    NOTE:  One needs to think about timeouts in the context of this call.  Perhaps we should
	do a select() before each iteration of the loop below.   For now we use a simple approach
	ignoring timeouts inside here and hoping that a select() by the caller
	before fullread() will do.

    */
    {
    int bytesread, bytesleft;
    char *next;

    
    next = buff;
    bytesleft = nbytes;
    
    while (bytesleft > 0)
	{
	errno = 99999;
	bytesread = read(fd, next, bytesleft);
	if (bytesread == 0)return(0);
	if (bytesread == -1 && errno != EINTR)return(-1);
	bytesread = bytesread > 0 ? bytesread : 0;
	bytesleft -= bytesread;
	next += bytesread;	    
	}
    return(nbytes);
    }
    



PRIVATE int fullwrite(IN int fd, INOUT char *buff, IN int nbytes)    
    /* 
	Write data on open file descriptor fd.

	Same philosophy as fullread();
	On exit, one of the following is true:
	    1. nbytes is returned, indicating a successful full write
	    2. -1 is returned.  This means that a Unix write() returned
		-1, and errno has the value set by Unix.
		
	Bogosities such as EINTR are not seen by the caller.

	Same comment about timeouts as fullread().

	
    */
    {
    int byteswritten, bytesleft;
    char *next;
    
    next = buff;
    bytesleft = nbytes;
    
    while (bytesleft > 0)
	{
	errno = 99999;
	byteswritten = write(fd, next, bytesleft);
	if (byteswritten == -1 && errno != EINTR)return(-1);
	byteswritten = byteswritten > 0 ? byteswritten : 0;
	bytesleft -= byteswritten;
	next += byteswritten;	    
	}
    return(nbytes);
    }



unsigned ComputeCheckSum(FILE *ffd)
    /* Assumes ffd is an open file; reads the file
	computes the checksum of the bytes from the current position to the end.
	The algorithm used is the same as sum(1)
    */
    {
    register unsigned sum;
    register int c;

    sum = 0;
    while ((c = getc(ffd)) != EOF)
	{
	if (sum&01)
	    sum = (sum>>1) + 0x8000;
	else
	    sum >>= 1;
	sum += c;
	sum &= 0xFFFF;
	}
    return(sum);
    }
