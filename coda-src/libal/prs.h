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


#ifndef _PRS_
#define _PRS_


#define PRS_VERSION "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/libal/Attic/prs.h,v 4.4 1998/12/21 22:25:58 jaharkes Exp $"

#define PRS_MAXNAMELEN 100    /*Maximum length of group and user names*/

#define PRS_SYSTEMID 777      /*Userid of System*/

#define PRS_ANONYMOUSID 776   /*Userid of the fake user Anonymous*/

#define PRS_ANYUSERID -101    /*GroupId of System:AnyUser*/
#define PRS_SYSTEMADMINID -204    /*GroupId of System:Administrators*/

#define PRS_PDBNAME "vice.pdb"
                        /*default location of protection data base*/

#define PRS_PCFNAME "pro.db"
                        /*default location of configuration file*/



typedef
    struct
        {
        int InclEntries; /* Leading part of IdList */
	int ExclEntries; /* Trailing part of IdList */
        int  IdList[1];  /* Actual  bound  is InclEntries+ExclEntries.
			    The first InclEntries entries are currently 
			    	included in this CPS: sorted in ascending order.
			    The last ExclEntries have been excluded from this CPS: not sorted.
			*/
			    
        }
    PRS_InternalCPS;
/*
Used only in VICE. Typically obtained via access list package routine AL_GetInternalCPS.
*/


typedef
	char *PRS_ExternalCPS;
/*
An ASCII representation of a CPS.  Consists of a decimal integer in format "%d\n" followed by
a list of blank separated names.
*/


#endif
