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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/rvm-src/seg/rvm_segment_private.h,v 1.1.1.1 1996/11/22 19:17:17 rvb Exp";
#endif _BLURB_


/* segment loader private declarations */

#ifndef _RVM_SEGMENT_PRIVATE_H_
#define _RVM_SEGMENT_PRIVATE_H_

/* Worker definitions */

extern rvm_return_t allocate_vm();
extern rvm_return_t deallocate_vm();

extern overlap();

/* Macro definitions for the segment header */

/* The follwing version string must be changed if any change is made in
   the segment header (rvm_segment_hdr_t)
*/
#define RVM_SEGMENT_VERSION "RVM Segment Loader Release 0.1  15 Nov. 1990"

#define RVM_SEGMENT_HDR_SIZE RVM_PAGE_SIZE /* length of segment header */
#define RVM_MAX_REGIONS                 /* maximum regions in seg  hdr */ \
        ((RVM_SEGMENT_HDR_SIZE/sizeof(rvm_region_def_t))-1)

typedef enum { rvm_segment_hdr_id = 1 } rvm_seg_struct_id_t;

/* segment header: rvm_segment_hdr_t */
typedef struct
    {
    rvm_seg_struct_id_t     struct_id;      /* self-identifier, do not change */

    char                version[RVM_VERSION_MAX]; /* version string */
    rvm_length_t        nregions;                 /* number of regions defined */
    rvm_region_def_t    regions[1];     /* region definition array  -- length
                                           actually determined by
                                           RVM_MAX_REGIONS */
    }
rvm_segment_hdr_t;

#endif  _RVM_SEGMENT_PRIVATE_H_
