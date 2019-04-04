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
    quiet       = true;
    table       = NULL;
    alias_table = NULL;
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

void StringKeyValueStore::set_key_alias(const char *key, const char *key_alias)
{
    item_t n;

    n = (item_t)malloc(sizeof(struct _item));
    assert(n != NULL);

    n->name = strdup(key_alias);
    assert(n->name != NULL);

    n->value = strdup(key);
    assert(n->value != NULL);

    n->next     = alias_table;
    alias_table = n;
}

const char *StringKeyValueStore::translate_alias_into_key(const char *key_alias)
{
    item_t cp;

    for (cp = alias_table; cp; cp = cp->next) {
        if (strcmp(key_alias, cp->name) == 0) {
            return cp->value;
        }
    }

    return key_alias;
}

void StringKeyValueStore::replace(const char *name, const char *value)
{
    item_t cp;
    if (!value)
        return;

    cp = find(name);
    if (cp) {
        free(cp->value);
        cp->value = strdup(value);
        assert(cp->value != NULL);
    }
}

item_t StringKeyValueStore::find(const char *name)
{
    item_t cp;
    const char *store_key = translate_alias_into_key(name);

    for (cp = table; cp; cp = cp->next) {
        if (strcmp(store_key, cp->name) == 0) {
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

    while ((cp = alias_table) != NULL) {
        alias_table = cp->next;
        free(cp->name);
        free(cp->value);
        free(cp);
    }
}
