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
#include <asm/errno.h>
#include <stdio.h>
#include "coda_string.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "stringkeyvaluestore.h"

StringKeyValueStore::StringKeyValueStore()
{
    table       = NULL;
    alias_table = NULL;
}

StringKeyValueStore::~StringKeyValueStore()
{
    purge();
}

int StringKeyValueStore::add(const char *key, const char *value)
{
    item_t n;

    if (has_key(unalias_key(key)))
        return EEXIST;

    n = (item_t)malloc(sizeof(struct _item));
    assert(n != NULL);

    n->name = strdup(key);
    assert(n->name != NULL);

    n->value = strdup(value);
    assert(n->value != NULL);

    n->next = table;
    table   = n;

    return 0;
}

int StringKeyValueStore::add_key_alias(const char *key, const char *key_alias)
{
    item_t n;

    if (has_key(key_alias))
        return EEXIST;
    if (is_key_alias(key_alias))
        return EEXIST;

    n = (item_t)malloc(sizeof(struct _item));
    assert(n != NULL);

    n->name = strdup(key_alias);
    assert(n->name != NULL);

    n->value = strdup(key);
    assert(n->value != NULL);

    n->next     = alias_table;
    alias_table = n;

    return (0);
}

const char *StringKeyValueStore::unalias_key(const char *key_alias)
{
    item_t cp = find_alias(key_alias);
    return ((cp != NULL) ? cp->value : key_alias);
}

void StringKeyValueStore::replace(const char *key, const char *value)
{
    item_t cp;
    if (!value)
        return;

    cp = find(key);
    if (cp) {
        free(cp->value);
        cp->value = strdup(value);
        assert(cp->value != NULL);
    }
}

item_t StringKeyValueStore::find(const char *key)
{
    item_t cp;
    const char *store_key = unalias_key(key);

    for (cp = table; cp; cp = cp->next) {
        if (strcmp(store_key, cp->name) == 0) {
            return cp;
        }
    }

    return NULL;
}

item_t StringKeyValueStore::find_alias(const char *key_alias)
{
    item_t cp;

    for (cp = alias_table; cp; cp = cp->next) {
        if (strcmp(key_alias, cp->name) == 0) {
            return cp;
        }
    }

    return NULL;
}

bool StringKeyValueStore::has_key(const char *key)
{
    const char *store_key = unalias_key(key);
    item_t cp             = find(store_key);
    return ((cp == NULL) ? false : true);
}

bool StringKeyValueStore::is_key_alias(const char *key)
{
    item_t aliased_key = find_alias(key);
    return ((aliased_key != NULL) ? true : false);
}

const char *StringKeyValueStore::get_value(const char *key)
{
    item_t cp;

    cp = find(key);

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
