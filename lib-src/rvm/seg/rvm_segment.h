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

/*
 * Segment Loader public definitions
 */

#ifndef _RVM_SEGMENT_H_
#define _RVM_SEGMENT_H_

#include "rvm.h"

/* region definition descriptor */
typedef struct {
    rvm_offset_t offset; /* region's offset in segment */
    rvm_length_t length; /* region length */
    char *vmaddr; /* mapping address for region */
} rvm_region_def_t;

/* initializer for region definition descriptor */
#define RVM_INIT_REGION(region, off, len, addr) \
    (region).length = (len);                    \
    (region).vmaddr = (addr);                   \
    (region).offset = (off);

/* error code for damaged segment header */
#define RVM_ESEGMENT_HDR 2000

/* define regions within a segment for segement loader */
rvm_return_t rvm_create_segment(
    char *DevName, /* pointer to data device name */
    rvm_offset_t DevLength, /* Length of dataDev if really a device */
    rvm_options_t *options, /* options record for RVM */
    rvm_length_t nregions, /* number of regions defined for segment*/
    rvm_region_def_t *region_defs /* array of region defs for segment */
);

/* load regions of a segment */
rvm_return_t rvm_load_segment(
    char *DevName, /* pointer to data device name */
    rvm_offset_t DevLength, /* Length of dataDev if really a device */
    rvm_options_t *options, /* options record for RVM */
    unsigned long *nregions, /* returned -- number of regions mapped */
    rvm_region_def_t *regions[] /* returned array of region descriptors */
);

#endif /* _RVM_SEGMENT_H_ */
