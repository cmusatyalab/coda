/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/


/* segment loader private declarations */

#ifndef _RVM_SEGMENT_PRIVATE_H_
#define _RVM_SEGMENT_PRIVATE_H_
/* Worker definitions */

extern rvm_return_t allocate_vm();
extern rvm_return_t deallocate_vm();

extern int overlap();

/* Macro definitions for the segment header */

/* The follwing version string must be changed if any change is made in
   the segment header (rvm_segment_hdr_t)
*/
#define RVM_SEGMENT_VERSION "RVM Segment Loader Release 0.1  15 Nov. 1990"

/* Moved this to rvm_segment.h, as rdsinit needs it. */
/* #define RVM_SEGMENT_HDR_SIZE RVM_PAGE_SIZE   * length of segment header */
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

#endif /* _RVM_SEGMENT_PRIVATE_H_ */
