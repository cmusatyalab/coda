/* BLURB lgpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _CODACONFDB_H_
#define _CODACONFDB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "codaconf.h"

#ifdef __cplusplus
}
#endif

class StringKeyValueStore {
private:
    item_t table;
    item_t alias_table;

    const char *unalias_key(const char *key_alias);

protected:
    bool quiet;
    item_t find(const char *key);
    item_t find_alias(const char *key_alias);

public:
    StringKeyValueStore();
    ~StringKeyValueStore();

    int add(const char *key, const char *value);
    int add_key_alias(const char *key, const char *key_alias);
    bool has_key(const char *key);
    bool is_key_alias(const char *key);
    const char *get_value(const char *key);
    void replace(const char *key, const char *value);
    void purge();
};

#endif /* _CODACONFDB_H_ */
