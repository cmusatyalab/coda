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

#include <lwp/lock.h>
#include <bitvect.h>

#define RESOURCEDB "FTREEDB"
struct part_ftree_opts {
    int depth;
    int width;
    int logwidth;
    int resource;
    int next;
    Bitv freebm;
    Lock lock;
};
