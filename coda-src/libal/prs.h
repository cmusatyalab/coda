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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/libal/prs.h,v 4.1 1997/01/08 21:49:47 rvb Exp $";
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


#ifndef _PRS_
#define _PRS_


#define PRS_VERSION "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/libal/prs.h,v 4.1 1997/01/08 21:49:47 rvb Exp $"

#define PRS_MAXNAMELEN 100    /*Maximum length of group and user names*/

#define PRS_SYSTEMID 777      /*Userid of System*/

#define PRS_ANONYMOUSID 776   /*Userid of the fake user Anonymous*/

#define PRS_ANYUSERID -101    /*GroupId of System:AnyUser*/
#define PRS_SYSTEMADMINID -204    /*GroupId of System:AnyUser*/

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
