/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _CODA_ASSERT_H_
#define _CODA_ASSERT_H_ 1

#define CODA_ASSERT(pred) do { if (!(pred)) coda_assert(#pred, __FILE__, __LINE__) ; } while (0)
#define CODA_NOTE(pred)   do { if (!(pred)) coda_note  (#pred, __FILE__, __LINE__) ; } while (0)

#define CODA_ASSERT_SLEEP	1
#define CODA_ASSERT_EXIT	2
#define CODA_ASSERT_ABORT	3
#define CODA_ASSERT_CORE	3

#ifdef __cplusplus
extern "C" {
#endif

extern void (*coda_assert_cleanup)(void);
extern int coda_assert_action;

void coda_assert(char *pred, char *file, int line);
void coda_note(char *pred, char *file, int line);

#ifdef __cplusplus
}
#endif

#endif  /* _CODA_ASSERT_H_ */

