/* BLURB lgpl

                           Coda File System
                              Release 6

	      Copyright (c) 2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _CODAENV_H_
#define _CODAENV_H_

#ifdef __cplusplus
extern "C" {
#endif

char * codaenv_find(const char * var_name);

int codaenv_int(const char *var_name, const int prev_val);

const char *codaenv_str(const char *var_name,
                        const char *prev_val);

#ifdef __cplusplus
}
#endif

#endif /* _CODAENV_H_ */
