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



/* This is the directory salvager.  It consists of two routines.  The first, DirOK, checks to see if the directory looks good.  If the directory does NOT look good, the approved procedure is to then call Salvage, which copies all the good entries from the damaged dir into a new directory. */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include "coda_string.h"
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "coda_dir.h"
#include "dir.private.h"


/* This routine is called with two parameters.  The first is the id of
   the original, currently suspect, directory.  The second is the file
   id of the place the salvager should place the new, fixed,
   directory. */

/*int DirSalvage (File *fromFile, File *toFile){*/
int DirSalvage (long *fromFile, long *toFile)
{	/* corrected referencing level (ehs 10/87) */
	/* First do a MakeDir on the target. */
	long dot[3], dotdot[3], code, usedPages;
	register int i;
	struct DirHeader *dhp;
	struct DirEntry *ep;
	int entry;

    memset((char *)dot, 0, sizeof(dot));
    memset((char *)dotdot, 0, sizeof(dotdot));
    MakeDir(toFile, dot, dotdot);	/* Returns no error code. */
    code = Delete(toFile, ".");
    if (code) printf("makedir screwup on '.', code %d.\n", code);
    code = Delete(toFile, "..");
    if (code) printf("makedir screwup on '..', code %d.\n", code);

    /* Find out how many pages are valid, using stupid heuristic since DRead never returns null. */
    dhp = (struct DirHeader *) DRead(fromFile, 0);
    if (!dhp)
        {printf("Failed to read first page of fromDir!\n");
        return 0;
        }

    usedPages = 0;
    for(i=0; i<MAXPAGES; i++)
        {if (dhp->alloMap[i] == EPP)
            {usedPages = i;
            break;
            }
        }
    if (usedPages == 0) usedPages = MAXPAGES;

    /* Finally, enumerate all the entries, doing a create on them. */
    for(i=0; i<NHASH; i++)
        {entry = ntohs(dhp->hashTable[i]);
        while(1)
            {if (!entry) break;
            if (entry < 0 || entry >= usedPages*EPP)
                {printf("Warning: bogus hash table entry encountered, ignoring.\n");
                break;
                }
            ep = GetBlob(fromFile, entry);
            if (!ep)
                {printf("Warning: bogus hash chain encountered, switching to next.\n");
                break;
                }
            entry = ntohs(ep->next);
/* XXXXX - Create expects fid to be an array of 3 longs.  
  Here we are passing just an array of 2 (3rd param) */
/*            code = Create(toFile, ep->name, (long *)(&ep->fid)); */
/* XXXXX - Subtracting 1 from the pointer to make it look 
  like an array of 3 longs - VERY BAD PRACTICE XXXXXXX */
            code = Create(toFile, ep->name, ((long *)(&ep->fid)) - 1 );
            if (code) printf("Create of %s returned code %d, continuing.\n", ep->name, code);
            DRelease((buffer *)ep, 0);
            }
        }

    /* Clean up things. */
    DRelease((buffer *)dhp, 0);
    return 0;
    }

/* the end */
