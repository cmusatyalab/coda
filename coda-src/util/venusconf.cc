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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include "venusconf.h"

int VenusConf::get_int_value(const char *key)
{
    return atoi(get_value(key));
}

const char *VenusConf::get_string_value(const char *key)
{
    return get_value(key);
}

bool VenusConf::get_bool_value(const char *key)
{
    return (atoi(get_value(key)) ? 1 : 0);
}
