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

#include "urlquote.h"

int quote(char *dest, char *src, size_t n)
{
    char c;

    n--;
    for (; n & *src; n--, src++, dest++) {
#if 0
	/* simply copy the following `safe' characters */
	if ((*src >= 'a' && *src <= 'z') ||
	    (*src >= 'A' && *src <= 'Z') ||
	    (*src >= '0' && *src <= '9') ||
	    (*src == '_' || *src == ',' || *src == '.' || *src == '-'))
#else
        /* only do the minimally required characters to keep as much
         * backward compatibility for now */
        if (*src != ' ' && *src != '%')
#endif
        {
	    *dest = *src;
	    continue;
	}

	/* avoid buffer overflows */
	if (n < 3) { *dest = '\0'; return(-1); }

	/* encode all other characters */
	*dest = '%';

	c = (*src >> 4) & 0xf;
	c += '0';
	if (c > '9') c += 'a' - '9' - 1;
	*(++dest) = c; n--;

	c = *src & 0xf;
	c += '0';
	if (c > '9') c += 'a' - '9' - 1;
	*(++dest) = c; n--;
    }
    /* and null-terminate the destination */
    *dest = '\0';
    return (*src == '\0');
}

int unquote(char *dest, char *src, size_t n)
{
    char c;

    n--;
    for (; n && *src; n--, src++, dest++) {
	if (*src == '+') { *dest = ' '; continue; }
	if (*src != '%') { *dest = *src; continue; }

	/* very sloppy hex decoder follows... */
	c = *(++src);
	if (!c) { *dest = '?'; continue; }
	c -= '0';
	if (c > 9)  c -= 'A' - '0' - 10;
	/* lowercase a-z is handled as a sideeffect of the following '& 0xf' */
	*dest = (c & 0xf) << 4;

	c = *(++src);
	if (!c) { *dest = '?'; continue; }
	c -= '0';
	if (c > 9)  c -= 'A' - '0' - 10;
	*dest |= c & 0xf;
    }
    /* and null-terminate the destination */
    *dest = '\0';

    return (*src == '\0' ? 0 : -1);
}

