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

#include "dllist.h"

#ifdef __cplusplus
}
#endif

struct element {
    struct dllist_head link;
    char *key;
    char *value;
};

typedef struct element *element_t;
typedef struct dllist_head table_t;

class StringKeyValueStore {
private:
    table_t table;
    table_t alias_table;

protected:
    bool quiet;
    element_t find(const char *key);
    element_t find_alias(const char *key_alias);
    const char *unalias_key(const char *key_alias);

public:
    StringKeyValueStore();
    ~StringKeyValueStore();

    int add(const char *key, const char *value);
    void set(const char *key, const char *value);
    int add_key_alias(const char *key, const char *key_alias);
    bool has_key(const char *key);
    bool is_key_alias(const char *key);
    const char *get_value(const char *key);
    void replace(const char *key, const char *value);
    void purge();

    void print();
    void print(int fd);
};

#endif /* _CODACONFDB_H_ */
