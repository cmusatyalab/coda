#ifndef pdb_array_H
#define pdb_array_H

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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>

typedef struct pdb_array {
	int32_t size,memsize;
	int32_t *data;
} pdb_array;

typedef int pdb_array_off;

void pdb_array_init(pdb_array *l);
void pdb_array_free(pdb_array *l);
int pdb_array_search(pdb_array *l, int32_t x);
void pdb_array_add(pdb_array *l, int32_t x);
void pdb_array_del(pdb_array *l, int32_t x);
void pdb_array_copy(pdb_array *d, pdb_array *s);
void pdb_array_merge(pdb_array *d, pdb_array *s);
int32_t pdb_array_head(pdb_array *l, pdb_array_off *off);
int32_t pdb_array_next(pdb_array *l, pdb_array_off *off);
int32_t pdb_array_size(pdb_array *l);
int pdb_array_pack(int32_t *d, pdb_array *l);
int pdb_array_unpack(int32_t *s, pdb_array *l);
int pdb_array_to_array(int32_t *d, pdb_array *l);
int pdb_array_snprintf(char *d, pdb_array *l, int len);

#endif
