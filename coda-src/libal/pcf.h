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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/libal/pcf.h,v 1.1.1.1 1996/11/22 19:08:36 rvb Exp";
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



/* For use in creating/interpreting .pcf files using the pcfprocs routines */

#define PCF_PCFNAME "test.pcf"
#define PCF_MAGIC 1823145	/* Magic number for .pcf files */
#define PCF_FIRSTLINELEN   50	/* in ASCII in .pcf file */

extern unsigned PDBCheckSum;	/* Checksum of the .pdb file from which this .pcf file was created */
extern int HighestUID;	/* encountered in corresponding .pdb file  */
extern int HighestGID;	/* encountered in corresponding .pdb file  */
extern int SeekOfEmptyEntry;	/* where to lseek() to, to extend .pdb file */

extern char *LitPool; 	/* Text of UserNames and GroupNames: a series of C-strings */
extern int LitPoolSize;	/* LitPool[0]:LitPool[LitPoolSize-1] are the bytes in use */
extern int *Uoffsets;   /* Uoffsets[i] is the index of the place in LitPool where the Username of UID i starts.
			-1 if such a UID doesn't exist */
extern int *Usorted;	/* i < j iff UserName{LitPool+Uoffsets[Usorted[i]]} < UserName{LitPool+Uoffsets[Usorted[j]]} */
extern int *Useeks;	/* Useeks[i] gives the position in protection data base to lseek() to begin reading
				the entry for UID i */
extern int *Goffsets;	/* Goffsets[i] is the index of the place in LitPool where the Groupname of GID -i starts.
			-1 if such a GID doesn't exist*/
extern int *Gsorted;	/* i < j iff GroupName{LitPool+Goffsets[Gsorted[i]]} < GroupName{LitPool+Goffsets[Gsorted[j]]} */
extern int *Gseeks;	/* Gseeks[i] gives the position in protection data base to lseek() to begin reading
				the entry for GID i */


extern int CmpUn(IN int *u1, IN int *u2);
		    /* Takes a pair of pointers to elements in Usorted[] and returns -1, 0, 1 depending 
		    whether the username corresponding to the  first element is less than, equal to,
		    or greater than the second  */
extern int CmpGn(int *g1, int *g2);	/* similar to CmpUn(), for Gsorted */
extern int pcfWrite(char *pcfile);  /* Write a .pcf file from the filled-in globals */
extern int pcfRead(char *pcfile);   /* Read a .pcf file into the globals */
extern unsigned ComputeCheckSum(FILE *ffd);
