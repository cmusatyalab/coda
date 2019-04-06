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
#include <errno.h>

#ifdef __cplusplus
}
#endif

#include "venusconf.h"
#include "dlist.h"

VenusConf::~VenusConf()
{
    dlist_iterator next(on_off_pairs_list);
    on_off_pair *curr_pair = NULL;

    while (curr_pair = (on_off_pair *)next()) {
        delete curr_pair;
    }
}

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
int VenusConf::add_on_off_pair(const char *on_key, const char *off_key,
                               bool on_value)
{
    if (has_key(unalias_key(on_key)) || has_key(unalias_key(off_key))) {
        return EEXIST;
    }

    add(on_key, on_value ? "1" : "0");
    add(off_key, on_value ? "0" : "1");

    on_off_pairs_list.insert((dlink *)new on_off_pair(on_key, off_key));
}

void VenusConf::set(const char *key, const char *value)
{
    VenusConf::on_off_pair *pair = find_on_off_pair(key);

    if (pair) {
        bool bool_val = atoi(value) ? true : false;
        StringKeyValueStore::set(pair->on_val, bool_val ? "1" : "0");
        StringKeyValueStore::set(pair->off_val, bool_val ? "0" : "1");
    } else {
        StringKeyValueStore::set(key, value);
    }
}

VenusConf::on_off_pair *VenusConf::find_on_off_pair(const char *key)
{
    dlist_iterator next(on_off_pairs_list);
    on_off_pair *curr_pair = NULL;

    while (curr_pair = (on_off_pair *)next()) {
        if (curr_pair->is_in_pair(key))
            return curr_pair;
    }

    return NULL;
}