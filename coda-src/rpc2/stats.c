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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/rpc2/Attic/stats.c,v 4.1 1997/01/08 21:50:33 rvb Exp $";
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



#include <stdio.h>
#include <math.h>

double lowCI[20] = {-1.0, 3.078, 1.886, 1.638, 1.533, 1.476, 1.440,1.415, 1.397, 1.383, 1.372,
			1.363, 1.356, 1.35, 1.345, 1.341, 1.337, 1.333, 1.33, 1.328};
			/* Magic numbers used by CIFactor(); can't make it local */

double CIFactor(dFreedom)
    int dFreedom;	/* no of degrees of freedom; == (NoOfSamples - 1) */
    {
    /* Approximate; errs conservatively; see Law & Kelton, pg 386 */
    if (dFreedom >= 100) return(1.29);
    if (dFreedom >= 50)  return(1.3);
    if (dFreedom >= 30) return(1.31);
    if (dFreedom >= 20) return (1.325);
    return(lowCI[dFreedom]);
    
    }

CumStats(count, sum, sum2)
    int count;
    double sum, sum2;
    {
    double mean, stddev, c90;

    if (count < 2) return;

    mean = sum/count;
    stddev = sqrt((count*sum2 - sum*sum)/(count*(count-1)));
    c90 = stddev*CIFactor(count - 1);
    
    printf("SAMPLES = %d    MEAN = %7.2f  STD DEV = %7.2f    90%% CI = %7.2f\n",
    	count, mean, stddev, c90);
    }

