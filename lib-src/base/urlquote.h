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

#*/

#ifndef _URLQUOTE_H_
#define _URLQUOTE_H_

/* Functions for escaping unsafe characters in a string. The worst case
 * behaviour when `quoting', is that destination string is 3 times larger
 * compared to the source string. The used encoding is similar to the one used
 * for URL's, [a-zA-Z0-9_,.-] are considered safe. All the other characters
 * are converted to %XX, where XX is the 2digit hexadecimal representation of
 * the quoted character. In addition, '+' is also interpreted as a space. */

/* The most important ones if you are hand editing some place where encoded
 * strings are used (like the filenames in the repair fixfile) are probably:
 * ' ' = %20 or '+'
 * '%' = %25
 * '+' = %2b
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>

int quote  (char *dest, char *src, size_t n);
int unquote(char *dest, char *src, size_t n);

#ifdef __cplusplus
}
#endif __cplusplus


#endif /* _URLQUOTE_H_ */
