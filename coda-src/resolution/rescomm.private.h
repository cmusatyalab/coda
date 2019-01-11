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

/* rescomm.private.h
 * Created Puneet Kumar, June 1990
 */

#ifdef __cplusplus
extern "C" {
#endif

extern void ResProcWait(char *);
extern void ResProcSignal(char *, int = 0);

#define CONDITION_INIT(c)
#define CONDITION_WAIT(c, m) ResProcWait((char *)(c))
#define CONDITION_SIGNAL(c) ResProcSignal((char *)(c))

#define TRANSLATE_TO_LOWER(s)      \
    {                              \
        for (char *c = s; *c; c++) \
            if (isupper(*c))       \
                *c = tolower(*c);  \
    }

#ifdef __cplusplus
}
#endif
