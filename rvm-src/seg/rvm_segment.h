#ifndef _BLURB_
#define _BLURB_
/*

     RVM: an Experimental Recoverable Virtual Memory Package
			Release 1.3

       Copyright (c) 1990-1994 Carnegie Mellon University
                      All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

RVM  IS  AN  EXPERIMENTAL  SOFTWARE  PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   Such improvements and extensions
should be returned to Software.Distribution@cs.cmu.edu.

*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/rvm-src/seg/rvm_segment.h,v 4.1 1997/01/08 21:54:46 rvb Exp $";
#endif _BLURB_

/*
 * Segment Loader public definitions
 */

#ifndef _RVM_SEGMENT_H_
#define _RVM_SEGMENT_H_

#include "rvm.h"

/* region definition descriptor */
typedef struct
    {
    rvm_offset_t        offset;         /* region's offset in segment */
    rvm_length_t        length;         /* region length */
    char                *vmaddr;        /* mapping address for region */
    }
rvm_region_def_t;

/* initializer for region definition descriptor */
#define RVM_INIT_REGION(region,off,len,addr) \
    (region).length = (len); \
    (region).vmaddr = (addr); \
    (region).offset = (off);

/* error code for damaged segment header */
#define RVM_ESEGMENT_HDR 2000

/* define regions within a segment for segement loader */
extern rvm_return_t rvm_create_segment (
    char                *DevName,       /* pointer to data device name */
    rvm_offset_t        DevLength,      /* Length of dataDev if really a device */
    rvm_options_t       *options,       /* options record for RVM */
    rvm_length_t        nregions,       /* number of regions defined for segment*/
    rvm_region_def_t    *region_defs    /* array of region defs for segment */
    );

/* load regions of a segment */
extern rvm_return_t rvm_load_segment (
    char                *DevName,       /* pointer to data device name */
    rvm_offset_t        DevLength,      /* Length of dataDev if really a device */
    rvm_options_t       *options,       /* options record for RVM */
    unsigned long       *nregions,      /* returned -- number of regions mapped */
    rvm_region_def_t    *regions[]      /* returned array of region descriptors */
    );

#endif _RVM_SEGMENT_H_
