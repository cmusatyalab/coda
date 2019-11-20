/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/* from venus */
#include "binding.h"
#include "venus.private.h"

#ifdef VENUSDEBUG
int binding::allocs   = 0;
int binding::deallocs = 0;
#endif /* VENUSDEBUG */

binding::binding()
{
    binder         = 0;
    bindee         = 0;
    referenceCount = 0;

#ifdef VENUSDEBUG
    allocs++;
#endif
}

binding::~binding()
{
#ifdef VENUSDEBUG
    deallocs++;
#endif
    if (referenceCount != 0)
        LOG(0,
            ("binding::~binding:  somebody forgot to decrement before delete\n"));

    if (binder != 0 || bindee != 0) {
        print(GetLogFile());
        CHOKE("binding::~binding: something bogus");
    }
}

void binding::print(int fd)
{
    fdprint(fd, "%p: binder = %p, bindee = %p, refCount = %d\n", this, binder,
            bindee, referenceCount);
}
