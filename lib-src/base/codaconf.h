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

#ifndef _CODACONF_H_
#define _CODACONF_H_

#ifdef __cplusplus
extern "C" {
#endif

/* conf_init reads (or merges) the name=value tuples from the conffile. If a
 * name is seen multiple times, only the last value is remembered. Empty lines
 * and lines starting with '#' are ignored. */
int conf_init(char *conffile);

/* make conf_init shut up about not finding the configuration file */
extern int codaconf_quiet;

/* conf_lookup returns the value associated with name, or NULL on error. */
char *conf_lookup(char *name, char *defaultvalue);

/* conf_free releases all resources allocated for the configuration data */
void conf_free(void);

/* helpers */
#include "coda_string.h"

#define CONF_STR(var, key, defval) \
    if (var == NULL || *var == '\0') { var = conf_lookup(key, defval); }
#define CONF_INT(var, key, defval) \
    { char t[256]; snprintf(t, 255, "%d", defval); t[255] = '\0'; \
    if (var == 0) { var = atoi(conf_lookup(key, t)); } }

#ifdef __cplusplus
}
#endif

#endif /* _CODACONF_H_ */

