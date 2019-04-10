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
#include <errno.h>
#include <stdio.h>
#include "coda_string.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "stringkeyvaluestore.h"

StringKeyValueStore::StringKeyValueStore()
{
    list_head_init(&table);
    list_head_init(&alias_table);
}

StringKeyValueStore::~StringKeyValueStore()
{
    purge();
}

static element_t find_in_table(const char *key, table_t *f_table)
{
    element_t cp;
    struct dllist_head *p;

    if (!key)
        return NULL;

    if (list_empty(f_table))
        return NULL;

    list_for_each(p, *f_table)
    {
        cp = list_entry(p, element, link);
        if (strcmp(key, cp->key) == 0) {
            return cp;
        }
    }

    return NULL;
}

static element_t alloc_element(const char *key, const char *value)
{
    struct element *n;

    n = (element_t)malloc(sizeof(struct element));
    assert(n != NULL);

    n->key = strdup(key);
    assert(n->key != NULL);

    n->value = strdup(value);
    assert(n->value != NULL);

    list_head_init(&n->link);

    return n;
}

static void free_element(element_t e)
{
    free(e->key);
    free(e->value);
    free(e);
}

int StringKeyValueStore::add(const char *key, const char *value)
{
    element_t n;
    if (has_key(unalias_key(key)))
        return EEXIST;

    n = alloc_element(key, value);

    list_add(&n->link, &table);

    return 0;
}

void StringKeyValueStore::set(const char *key, const char *value)
{
    if (has_key(unalias_key(key)))
        replace(key, value);
    else
        add(key, value);
}

int StringKeyValueStore::add_key_alias(const char *key, const char *key_alias)
{
    element_t n;

    if (has_key(key_alias))
        return EEXIST;

    n = alloc_element(key_alias, key);

    list_add(&n->link, &alias_table);

    return (0);
}

const char *StringKeyValueStore::unalias_key(const char *key_alias)
{
    element_t cp = find_alias(key_alias);
    return ((cp != NULL) ? cp->value : key_alias);
}

void StringKeyValueStore::replace(const char *key, const char *value)
{
    element_t cp;
    if (!value)
        return;

    cp = find(key);
    if (cp) {
        free(cp->value);
        cp->value = strdup(value);
        assert(cp->value != NULL);
    }
}

element_t StringKeyValueStore::find(const char *key)
{
    return find_in_table(unalias_key(key), &table);
}

element_t StringKeyValueStore::find_alias(const char *key_alias)
{
    return find_in_table(key_alias, &alias_table);
}

bool StringKeyValueStore::has_key(const char *key)
{
    const char *store_key = unalias_key(key);
    element_t cp          = find(store_key);

    if (cp != NULL)
        return true;

    cp = find_alias(key);
    if (cp != NULL)
        return true;

    return false;
}

bool StringKeyValueStore::is_key_alias(const char *key)
{
    element_t aliased_key = find_alias(key);
    return ((aliased_key != NULL) ? true : false);
}

const char *StringKeyValueStore::get_value(const char *key)
{
    element_t cp;

    cp = find(key);

    return cp ? cp->value : NULL;
}

static void purge_table(table_t *f_table)
{
    element_t cp;

    while (!list_empty(f_table)) {
        cp = list_entry(f_table->next, element, link);
        list_del(&cp->link);
        free_element(cp);
    }
}

void StringKeyValueStore::purge(void)
{
    purge_table(&table);
    purge_table(&alias_table);
}

void StringKeyValueStore::print()
{
    print(fileno(stdout));
}

void StringKeyValueStore::print(int fd)
{
    element_t cp;
    struct dllist_head *p;

    list_for_each(p, table)
    {
        cp = list_entry(p, element, link);
        dprintf(fd, "\"%s\" : \"%s\"\n", cp->key, cp->value);
    }
}