/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <test/rvm/recov.h>

/* Transaction handling */
void _Recov_BeginTrans(const char file[], int line)
{
    _rvmlib_begin_transaction(no_restore, file, line);
}

void Recov_EndTrans(int time)
{
    rvmlib_end_transaction(no_flush, 0);
}

void Recov_AbortTrans() {
    rvmlib_abort(0);
}
