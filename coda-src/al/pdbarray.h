#ifndef pdb_array_H
#define pdb_array_H

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

   Copyright (C) 1998  John-Anthony Owens, Samuel Ieong, Rudi Seitz

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

*/


#include <stdio.h>
#include <sys/types.h>

#ifdef DJGPP
#define int32_t int
#endif

typedef struct pdb_array {
	int32_t size,memsize;
	int32_t *data;
} pdb_array;

typedef int pdb_array_off;

__inline__ void pdb_array_init(pdb_array *l);
__inline__ void pdb_array_free(pdb_array *l);
__inline__ void pdb_array_add(pdb_array *l, int32_t x);
__inline__ void pdb_array_del(pdb_array *l, int32_t x);
__inline__ void pdb_array_copy(pdb_array *d, pdb_array *s);
__inline__ void pdb_array_merge(pdb_array *d, pdb_array *s);
__inline__ int32_t pdb_array_head(pdb_array *l, pdb_array_off *off);
__inline__ int32_t pdb_array_next(pdb_array *l, pdb_array_off *off);
__inline__ int pdb_array_size(pdb_array *l);
__inline__ int pdb_array_pack(int32_t *d, pdb_array *l);
__inline__ int pdb_array_unpack(int32_t *s, pdb_array *l);
__inline__ int pdb_array_to_array(int32_t *d, pdb_array *l);
__inline__ int pdb_array_snprintf(char *d, pdb_array *l, int len);

#endif
