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

#include <stdlib.h>
#include "coda_string.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <coda_assert.h>
#include "pdbarray.h"

static int pdb_array_search(pdb_array *l, int32_t x)
{
	int32_t b,t,m;

	if(l->size < 1) return -1;
	b = 0;
	t = l->size - 1;
	m = (b + t) / 2;
	while(l->data[m] != x){
		if(b >= t) return -1;
		if(l->data[m] > x)
			t = m - 1;
		else
			b = m + 1;
		m = (b + t) / 2;
	}
	return m;
}


void pdb_array_init(pdb_array *l)
{
	l->size = 0;
	l->memsize = 0;
	l->data = NULL;
}


void pdb_array_free(pdb_array *l)
{
	CODA_ASSERT(l);
	if(l->memsize > 0) free(l->data);
	l->size = 0;
	l->memsize = 0;
	l->data = NULL;
}


void pdb_array_add(pdb_array *l, int32_t x) 
{
	int i;
	
	CODA_ASSERT(l);

	if(l->size + 1 > l->memsize){
		if(l->memsize != 0){
			l->memsize += 16;
			l->data = realloc(l->data, l->memsize*sizeof(int32_t));
		} else {
			l->memsize = 16;
			l->data = malloc(l->memsize*sizeof(int32_t));
		}
	}
	/* order it */
	i = l->size-1;
	while((i >= 0) && (l->data[i] > x))
		i--;
	if (i >= 0) {
	   if (l->data[i] == x)
	       return;
	    memmove(&(l->data[i+1]), &(l->data[i]),
		    (l->size - i) * sizeof(int32_t));
	}
	l->data[i+1] = x;
	l->size++;
}


void pdb_array_del(pdb_array *l, int32_t x) 
{
	int i;

	CODA_ASSERT(l);

	i = pdb_array_search(l,x);
	if (i < 0) return;
	l->size--;
	memmove(&(l->data[i]),&(l->data[i+1]),(l->size - i) * sizeof(int32_t));
	l->data[l->size] = 0; /* Just to be safe */
}


void pdb_array_copy(pdb_array *d, pdb_array *s)
{
	d->size = s->size;
	d->memsize = s->size; /* Save a little bit of space if possible */
	d->data = malloc(s->size * sizeof(int32_t));
	memcpy(d->data, s->data, s->size * sizeof(int32_t));
}


void pdb_array_merge(pdb_array *d, pdb_array *s)
{
	int i;

	CODA_ASSERT(d && s);

	/* This could be a lot faster, but this is easier */
	for(i=0;i < s->size;i++)
		pdb_array_add(d, s->data[i]);
}


int32_t pdb_array_head(pdb_array *l, pdb_array_off *off)
{
	*off = 0;
	if(l->size <= 0)
		return 0;
	return l->data[0];
}


int32_t pdb_array_next(pdb_array *l, pdb_array_off *off)
{
	if(l->size <= (++(*off)))
		return 0;
	return l->data[*off];
}


int32_t pdb_array_size(pdb_array *l)
{
	CODA_ASSERT(l);
	return l->size;
}


int pdb_array_pack(int32_t *d, pdb_array *l)
{
	int i;
	CODA_ASSERT(d && l);
	
	d[0] = htonl(l->size);
	for(i=0;i<l->size;i++)
		d[i+1] = htonl(l->data[i]);
	return (l->size + 1);
}


int pdb_array_unpack(int32_t *s, pdb_array *l)
{
	int i;
	CODA_ASSERT(s && l);
	
	l->size = l->memsize = ntohl(s[0]);
	if(l->memsize > 0)
		l->data = malloc(l->memsize * sizeof(int32_t));
	else
		l->data = NULL;
	for(i=0;i<l->size;i++)
		l->data[i] = ntohl(s[i+1]);
	return (l->size + 1);
}


int pdb_array_to_array(int32_t *out, pdb_array *l)
{
	CODA_ASSERT(out && l);

	memcpy((char *) out,(char *) l->data, l->size * sizeof(int32_t));
	return l->size;
}



int pdb_array_snprintf(char *out, pdb_array *l, int len)
{
	int32_t i;
	char tmp[80];

	CODA_ASSERT(l && out);

	if (len < 1) return -1;
	out[0]='\0';
	for (i=0; i < l->size; i++) {
		sprintf(tmp, "%d ", l->data[i]);
		if(strlen(out)+strlen(tmp) > len) return -1;
		strcat(out,tmp);
	}
	return strlen(out);
}
