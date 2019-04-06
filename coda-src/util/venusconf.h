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
#include "dlist.h"
#include <stdlib.h>

class VenusConf : public StringKeyValueStore {
private:
    dlist on_off_pairs_list;

    class on_off_pair : dlink {
    public:
        const char *on_val;
        const char *off_val;

        bool is_in_pair(const char *val)
        {
            if (strcmp(val, on_val) == 0)
                return true;
            if (strcmp(val, off_val) == 0)
                return true;
            return false;
        }

        on_off_pair(const char *on, const char *off)
            : dlink()
        {
            on_val  = strdup(on);
            off_val = strdup(off);
        }

        ~on_off_pair()
        {
            free((void *)on_val);
            free((void *)off_val);
        }
    };
    on_off_pair *find_on_off_pair(const char *key);

public:
    ~VenusConf();
    int get_int_value(const char *key);
    const char *get_string_value(const char *key);
    bool get_bool_value(const char *key);
    int add_on_off_pair(const char *on_key, const char *off_key, bool on_value);
    void set(const char *key, const char *value);
};

#endif /* _VENUSCONF_H_ */
