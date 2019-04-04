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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#include <sys/param.h>
#include <stdio.h>
#include "coda_string.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "stringkeyvaluestore.h"

StringKeyValueStore::StringKeyValueStore()
{
    quiet = true;
    table = NULL;
}

StringKeyValueStore::~StringKeyValueStore()
{
    purge();
}

void StringKeyValueStore::add(const char *name, const char *value)
{
    item_t n;

    n = (item_t)malloc(sizeof(struct _item));
    assert(n != NULL);

    n->name = strdup(name);
    assert(n->name != NULL);

    n->value = strdup(value);
    assert(n->value != NULL);

    n->next = table;
    table   = n;
}

void StringKeyValueStore::replace(const char *name, const char *value)
{
    item_t cp;
    if (!value)
        return;

    for (cp = table; cp; cp = cp->next) {
        if (strcmp(name, cp->name) == 0) {
            if (value) {
                free(cp->value);
                cp->value = strdup(value);
                assert(cp->value != NULL);
            }
            break;
        }
    }
}

item_t StringKeyValueStore::find(const char *name)
{
    item_t cp;

    for (cp = table; cp; cp = cp->next) {
        if (strcmp(name, cp->name) == 0) {
            return cp;
        }
    }

    return NULL;
}

const char *StringKeyValueStore::get_value(const char *name)
{
    item_t cp;

    cp = find(name);

    return cp ? cp->value : NULL;
}

void StringKeyValueStore::purge(void)
{
    item_t cp;

    while ((cp = table) != NULL) {
        table = cp->next;
        free(cp->name);
        free(cp->value);
        free(cp);
    }
}
