/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <sys/param.h>
#include <stdio.h>
#include "coda_string.h"
#include <assert.h>
#include <stdlib.h>
#include "codaconf.h"

/* some hardcoded options, probably both undef'd for normal use      */
/* CONFDEBUG: annoying debugging related messages                    */
/* CONFWRITE: write unfound config entries to the last-read conffile */
#undef CONFDEBUG
#undef CONFWRITE

/* buffer to read lines of config data */
#define MAXLINELEN 256
static char line[MAXLINELEN];
static char conffile[MAXPATHLEN];

/* nobody outside of this file needs to be exposed to these structures. */
typedef char *string_t;
typedef struct _item {
    struct _item *next;
    string_t name;
    string_t value;
} *item_t;

static item_t conf_table = NULL;

/* Add a name=value pair to the conf_table. */
/* The passed name and value strings are copied. */
static item_t conf_add(string_t name, string_t value)
{
    item_t n;

    n = (item_t)malloc(sizeof(struct _item));
    assert(n != NULL);

    n->name  = strdup(name);
    assert(n->name != NULL);

    n->value = strdup(value);
    assert(n->value != NULL);

    n->next  = conf_table;
    conf_table = n;

    return(n);
}

/* Return the value associated with a name. */
/* If value is specified and replace is true and the found entry is
 * replaced with the new value. If a value is given, the entry is not
 * found, and CONFWRITE is defined, the name=value pair is appended to
 * the last read configuration file. */
static item_t conf_find(string_t name, string_t value, int replace)
{
    item_t cp;
#ifdef CONFWRITE
    FILE  *conf;
#endif

    for(cp = conf_table; cp; cp = cp->next) {
        if (strcmp(name, cp->name) == 0) {
            if (replace && value) {
                free(cp->value);
                cp->value = strdup(value);
                assert(cp->value != NULL);
            }
            return(cp);
        }
    }

    if (!value) return(NULL);

#ifdef CONFWRITE
    /* append the new value to the last read configuration file, but only if
     * we are being called from conf_lookup (i.e. replace is false) */
    if (!replace) {
        conf = fopen(conffile, "a");
        if (conf) {
            fputs(name, conf);
            fputs("=\"", conf);
            fputs(value, conf);
            fputs("\"\n", conf);
            fclose(conf);
        }
    }
#endif /* CONFWRITE */

    return(conf_add(name, value));
}

/* parse a configuration line */
static void conf_parse_line(char *line, int lineno,
                          string_t *name, string_t *value)
{
    char *eon, *eov, *val;

    *name = *value = NULL;

    /* skip blanks at the beginning of the line */
    while (*line == ' ' || *line == '\t') line++;
    /* ignore comments and empty lines */
    if (*line == '#' || *line == '\0' || *line == '\n') return;

    /* find the beginning of the value */
    eon = val = strchr(line, '=');
    if (!eon) {
        fprintf(stderr, "Configuration error in line %d, "
		        "missing '='.\n", lineno);
        return;
    }

    /* strip trailing blanks from the name */
    eon--; while (*eon == ' ' || *eon == '\t') eon--;
    eon[1] = '\0';

    /* blanks before the value are an error (for bash, so also for us) */
    val++;
    if (*val == ' ' || *val == '\t') {
        fprintf(stderr, "Configuration error in line %d, "
                        "no blanks allowed after the '='.\n", lineno);
        return;
    }
    /* sort of handle quoting */
    if (*val == '"' || *val == '\'') val++;

    /* find the end of the line */
    eov = val; while (*eov && *eov != '\n') eov++;

    /* strip trailing blanks from the value */
    eov--; while (*eov == ' ' || *eov == '\t') eov--;

    /* sort of handle quoting */
    if (*eov == '"' || *eov == '\'') eov--;

    eov[1] = '\0';

    /* got all the pointers, now allocate the string_t's */
    *name  = strdup(line);
    assert(*name != NULL);

    *value = strdup(val);
    assert(*value != NULL);
}

/* conf_init reads (or merges) the name=value tuples from the conffile. If a
 * name is seen multiple times, only the last value is remembered. Empty lines
 * and lines starting with '#' are ignored. */
int conf_init(char *cf)
{
    FILE *conf;
    int lineno = 0;
    string_t name, value;
    item_t item;

    /* remember the last read configuration file */
    strcpy(conffile, cf);

    conf = fopen(cf, "r");
    if (!conf) {
	fprintf(stderr, "Cannot read configuration file '%s', "
			"will use default values.\n", conffile);
        return(-1);
    }
    
    while(fgets(line, MAXLINELEN, conf)) {
        lineno++;
        conf_parse_line(line, lineno, &name, &value);
        if (name == NULL) continue; /* skip comments and blank lines */
        
        item = conf_find(name, value, 1);

#ifdef CONFDEBUG
        printf("line: %d, name: '%s', value: '%s'", lineno, name, value);
        if (item)
            printf("stored-value: '%s'\n", item->value);
        else
            printf("not found?\n");
#endif
        free(name); free(value);
    }
    fclose(conf);

    return(0);
}

/* conf_lookup returns the value associated with name, or NULL on error. */
char *conf_lookup(char *name, char *defaultvalue)
{
    item_t cp;
    
    cp = conf_find(name, defaultvalue, 0);

    return cp ? cp->value : NULL;
}

/* release all allocated resources */
void conf_free(void)
{
    item_t cp;

    while((cp = conf_table) != NULL) {
        conf_table = cp->next;
        free(cp->name);
        free(cp->value);
        free(cp);
    }
}

#ifdef TESTCONF
char configpath[MAXPATHLEN]=TESTCONF;

int main(int argc, char *argv[])
{
    char *var, *val = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <conffile>.<confvar> [defaultvalue]\n",
                argv[0]);
        fprintf(stderr, "e.g. %s venus.cachesize\n", argv[0]);
        exit(-1);
    }

    var = strchr(argv[1], '.');
    if (!var) {
        fprintf(stderr, "Didn't find the '.' separator\n");
        exit(-1);
    }
    *(var++) = '\0';
    
    if (argc == 3) val = argv[2];

    strcat(configpath, argv[1]);
    strcat(configpath, ".conf");

    conf_init(configpath);

    val = conf_lookup(var, val);
    
    conf_free();

    if (!val) {
        fprintf(stderr, "Couldn't find a value for '%s'\n", var);
        exit(-2);
    }
    
    fputs(val, stdout);
    fputc('\n', stdout);
    
    exit(0);
}
#endif
