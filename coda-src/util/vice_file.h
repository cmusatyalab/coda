/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 2003-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

*/

/* vice_file.h: prototypes */

#ifndef _VICE_FILE_H
#define _VICE_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

void vice_dir_init(const char *dirname);
const char *vice_config_path(const char *name);

#ifdef __cplusplus
}
#endif

#endif
