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
                           none currently

#*/

#ifndef VICETAB_INCLUDED
#define VICETAB_INCLUDED

#include <stdio.h>


/* this file is used for parsing routines of /vice/vicetab, the file
 * describing server partitions.
 * Its structure is similar to fstab: 
 * we have  space or tab separated fields:
 *    part_host hostname of server
 *    part_dir a pathname identifying the server directory or partition
 *    part_type the type of server partition found under part_dir 
 *    a comma separated list of options of the form:
 *         opt1=value1,opt2=value2,....
 *
 * the semantics of the functions setpartent, getpartent, endpartent
 * and addpartent are very similar to the corresponding mntent
 * functions (see getmntent (3)).
 * However, for increased sanity the structure is not exposed.
 */

#define VICETAB  "/vice/vicetab"
#define VICETAB_MAXSTR  256    /* max line length in VICETAB */


/* supported types of server partitions */
#define PTYPE_SIMPLE     "simple"
#define PTYPE_MACHUFS    "machufs"
#define PTYPE_LINUXEXT2  "linuxext2"
#define PTYPE_NBSDUFS    "nbsdufs"
#define PTYPE_FTREE      "ftree"



typedef struct Partent_s *Partent;

Partent Partent_new();
void Partent_free(Partent *f);
FILE *Partent_set(const char *file, const char *mode);
int Partent_end(FILE *__filep);
Partent Partent_get(FILE *filep);
int Partent_add(FILE *__filep, Partent mnt);
char *Partent_hasopt(Partent mnt, const char *opt);
int Partent_intopt(Partent ent, const char *opt, int *value);
void Partent_print(Partent ent);
char *Partent_dir(Partent p);
char *Partent_type(Partent p);
char *Partent_host(Partent p);
Partent Partent_create(char *host, char *dir, char *type, char *opts);


#endif /* _VICETAB_H_ */
