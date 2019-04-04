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

protected:
    bool quiet;

public:
    StringKeyValueStore();
    ~StringKeyValueStore();

    void add(const char *name, const char *value);
    item_t find(const char *name);
    const char *get_value(const char *name);
    void replace(const char *name, const char *value);
    void purge(void);
};

#endif /* _CODACONFDB_H_ */
