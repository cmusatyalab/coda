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
                           none currently

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "coda_string.h"
#include <stdlib.h>
#include <util.h>
#include "vicetab.h"


struct Partent_s{
    char part_host[VICETAB_MAXSTR];
    char part_dir[VICETAB_MAXSTR];      /* server partition directory  */
    char part_type[VICETAB_MAXSTR];     /* type of partition */
    char part_opts[VICETAB_MAXSTR];     /* options */
};


Partent Partent_new()
{
    Partent T;
    return malloc(sizeof *T);
}

void Partent_free(Partent *T)
{
    CODA_ASSERT(T && *T);
    free(*T);
}

FILE *Partent_set(const char *path, const char *mode)
{
    return fopen(path, mode); 
}

int Partent_end(FILE *f)
{
    if ( f ) 
        return fclose(f);
    else {
	eprint("NULL FILE * passed to Partent_end.\n");
        CODA_ASSERT(0);
    }
    return 0;
}

char *Partent_hasopt(Partent part, const char *opt)
{
    if ( part ) 
        return strstr(part->part_opts, opt);
    else {
	eprint("NULL passed to Partent_hasopt.\n");
        CODA_ASSERT(0);
    }
    return 0;
}

int Partent_intopt(Partent part, const char *opt, int *value)
{
    char buff[VICETAB_MAXSTR];
    char *optloc, *valloc;
    char *sep=" \t,=";
    int rc = -1;
    
    if ( part == NULL )
        CODA_ASSERT(0);

    optloc = Partent_hasopt(part, opt);
    if ( optloc ) {
	strncpy(buff, optloc, VICETAB_MAXSTR-1);
	valloc = strstr(buff, "=");
	valloc = strtok(valloc, sep);
	if ( valloc ) {
	    rc = 0; 
	    *value = atoi(valloc);
	}
    }
    return rc;
}

Partent Partent_create(char *host, char *dir, char *type, char *opts)
{
    Partent pa = Partent_new();
    CODA_ASSERT(pa && host && dir && type && opts);
    strncpy(pa->part_host, host, VICETAB_MAXSTR);
    strncpy(pa->part_dir, dir, VICETAB_MAXSTR);
    strncpy(pa->part_type, type, VICETAB_MAXSTR);
    strncpy(pa->part_opts, opts, VICETAB_MAXSTR);
    return pa;
}

int Partent_add(FILE *filep, Partent part)
{
  if ( part == NULL ) {
      eprint("NULL passed to Partent_add");
      CODA_ASSERT(0);
  }

    if (fseek(filep, 0, SEEK_END) < 0)
        return 1;

    if (fprintf(filep, "%s %s %s %s\n", part->part_host, part->part_dir, 
		part->part_type, part->part_opts)< 1)
        return 1;

  return 0;
}


Partent Partent_get(FILE *filep)
{
    char *cp, *tp, *sep = " \t\n";
    static char buff[VICETAB_MAXSTR];
    Partent part = Partent_new();

    if ( part == NULL ) {
        eprint("Could not create new Partent in Partent_get.\n");
        CODA_ASSERT(0);
    }

    /* Continue reading lines from the file */
    while((cp = fgets(buff, sizeof buff, filep)) != NULL) {
        if (buff[0] == '#' || buff[0] == '\n') continue;
        break;
    }

    /* To detect EOF we check the return value from fgets (). */
    if (cp == NULL) 
	goto error;
    
    tp = strtok(buff, sep);
    if (tp == NULL)
	goto error;
    else
	strcpy(part->part_host, tp);

    tp = strtok(NULL, sep);
    if (tp == NULL)
	goto error;
    else
	strcpy(part->part_dir, tp);

    tp = strtok(NULL, sep);
    if (tp == NULL)
	goto error;
    else
	strcpy(part->part_type, tp);

    tp = strtok(NULL, sep);
    if (tp == NULL)
	part->part_opts[0] = '\n';
    else
	strcpy(part->part_opts, tp);

    return part;
 error:
    Partent_free(&part);
    return NULL;
}

char *Partent_host(Partent p)
{
    if ( p == NULL ) {
	eprint("NULL passed!\n");
	CODA_ASSERT(0);
    }
    return p->part_host;
}

char *Partent_type(Partent p)
{
    if ( p == NULL ) {
	eprint("NULL passed!\n");
	CODA_ASSERT(0);
    }
    return p->part_type;
}

char *Partent_dir(Partent p)
{
    if ( p == NULL ) {
	eprint("NULL passed!\n");
	CODA_ASSERT(0);
    }
    return p->part_dir;
}
