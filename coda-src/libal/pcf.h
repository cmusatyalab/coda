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


extern int CmpUn(int *u1, int *u2);
		    /* Takes a pair of pointers to elements in Usorted[] and returns -1, 0, 1 depending 
		    whether the username corresponding to the  first element is less than, equal to,
		    or greater than the second  */
extern int CmpGn(int *g1, int *g2);	/* similar to CmpUn(), for Gsorted */
extern int pcfWrite(char *pcfile);  /* Write a .pcf file from the filled-in globals */
extern int pcfRead(char *pcfile);   /* Read a .pcf file into the globals */
extern unsigned ComputeCheckSum(FILE *ffd);
