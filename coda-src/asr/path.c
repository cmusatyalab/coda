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





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "asr.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include "coda_string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>    
#include <vcrcommon.h>


#ifdef __cplusplus
}
#endif __cplusplus



/* An implementation of path(3) which is a standard function in Mach OS
 * the behaviour is according to man page in Mach OS, which says,
 *
 *    The handling of most names is obvious, but several special
 *    cases exist.  The name "f", containing no slashes, is split
 *    into directory "." and filename "f".  The name "/" is direc-
 *    tory "/" and filename ".".  The path "" is directory "." and
 *    filename ".".
 *       -- manpage of path(3)
 */

void path(char *pathname, char *direc, char *file)
{
  char *maybebase, *tok;
  int num_char_to_be_rm;

  if (strlen(pathname)==0) {
    strcpy(direc, ".");
    strcpy(file, ".");
    return;
  }
  if (strchr(pathname, '/')==0) {
    strcpy(direc, ".");
    strcpy(file, pathname);
    return;
  } 
  if (strcmp(pathname, "/")==0) {
    strcpy(direc, "/");
    strcpy(file, ".");
    return;
  }
  strcpy(direc, pathname);
  maybebase = strtok(direc,"/");
  while (tok = strtok(0,"/")) 
    maybebase = tok;
  strcpy(file, maybebase);
  strcpy(direc, pathname);
  num_char_to_be_rm = strlen(file) + 
    (direc[strlen(pathname)-1]=='/' ? 1 : 0);/* any trailing slash ? */
  *(direc+strlen(pathname)-num_char_to_be_rm) = '\0';
    /* removing the component for file from direc */
  if (strlen(direc)==0) strcpy(direc,"."); /* this happen when pathname 
                                            * is "name/", for example */
  if (strlen(direc)>=2) /* don't do this if only '/' remains in direc */
    if (*(direc+strlen(direc)-1) == '/' )
      *(direc+strlen(direc)-1) = '\0'; 
       /* remove trailing slash in direc */
  return;
}



