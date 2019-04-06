/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _VENUSCONF_H_
#define _VENUSCONF_H_

#include <stringkeyvaluestore.h>

class VenusConf : public StringKeyValueStore {
public:
    int get_int_value(const char *key);
    const char *get_string_value(const char *key);
    bool get_bool_value(const char *key);
};

#endif /* _VENUSCONF_H_ */
