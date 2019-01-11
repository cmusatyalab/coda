/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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
#include <unistd.h>
#include "codaconf.h"
#include "coda_config.h"

/* some hardcoded options, probably both undef'd for normal use      */
/* CONFDEBUG: annoying debugging related messages                    */
#undef CONFDEBUG
#undef CONFWRITE

/* default configuration file search path used by codaconf_init */
static const char *default_codaconfpath = SYSCONFDIR
    ":/usr/local/etc/coda:/etc/coda:";

/* buffer to read lines of config data */
#define MAXLINELEN 256
static char line[MAXLINELEN];
static char conffile[MAXPATHLEN + 1];

/* this global is exported to surpress codaconf_init_one verbosity */
int codaconf_quiet = 0;

/* nobody outside of this file needs to be exposed to these structures. */
typedef struct _item {
    struct _item *next;
    char *name;
    char *value;
} * item_t;

static item_t codaconf_table = NULL;

/* Add a name=value pair to the codaconf_table. */
/* The passed name and value strings are copied. */
static item_t codaconf_add(const char *name, const char *value)
{
    item_t n;

    n = (item_t)malloc(sizeof(struct _item));
    assert(n != NULL);

    n->name = strdup(name);
    assert(n->name != NULL);

    n->value = strdup(value);
    assert(n->value != NULL);

    n->next        = codaconf_table;
    codaconf_table = n;

    return (n);
}

/* Return the value associated with a name. */
/* If value is specified and replace is true and the found entry is
 * replaced with the new value. If a value is given, the entry is not
 * found, and CONFWRITE is defined, the name=value pair is appended
 * to the last read configuration file. */
static item_t codaconf_find(const char *name, const char *value, int replace)
{
    item_t cp;
#ifdef CONFWRITE
    FILE *conf;
#endif

    for (cp = codaconf_table; cp; cp = cp->next) {
        if (strcmp(name, cp->name) == 0) {
            if (replace && value) {
                free(cp->value);
                cp->value = strdup(value);
                assert(cp->value != NULL);
            }
            return (cp);
        }
    }

    if (!value)
        return (NULL);

#ifdef CONFWRITE
    /* append the new value to the last read configuration file, but only if
     * we are being called from codaconf_lookup (i.e. replace is false) and a
     * default value was given */
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
#endif

    return (codaconf_add(name, value));
}

/* parse a configuration line */
static void codaconf_parse_line(char *line, int lineno, char **name,
                                char **value)
{
    char *eon, *eov, *val;

    *name = *value = NULL;

    /* skip blanks at the beginning of the line */
    while (*line == ' ' || *line == '\t')
        line++;
    /* ignore comments and empty lines */
    if (*line == '#' || *line == '\0' || *line == '\n')
        return;

    /* find the beginning of the value */
    eon = val = strchr(line, '=');
    if (!eon) {
        fprintf(stderr,
                "Configuration error in line %d, "
                "missing '='.\n",
                lineno);
        return;
    }

    /* strip trailing blanks from the name */
    eon--;
    while (*eon == ' ' || *eon == '\t')
        eon--;
    eon[1] = '\0';

    /* blanks before the value are an error (for bash, so also for us) */
    val++;
    if (*val == ' ' || *val == '\t') {
        fprintf(stderr,
                "Configuration error in line %d, "
                "no blanks allowed after the '='.\n",
                lineno);
        return;
    }
    /* sort of handle quoting */
    if (*val == '"' || *val == '\'')
        val++;

    /* find the end of the line */
    eov = val;
    while (*eov && *eov != '\n')
        eov++;

    /* strip trailing blanks from the value */
    eov--;
    while (*eov == ' ' || *eov == '\t')
        eov--;

    /* sort of handle quoting */
    if (*eov == '"' || *eov == '\'')
        eov--;

    eov[1] = '\0';

    /* got all the pointers, now allocate the strings */
    *name = strdup(line);
    assert(*name != NULL);

    *value = strdup(val);
    assert(*value != NULL);
}

/* codaconf_init_one reads (or merges) the name=value tuples from the conffile.
 * If a name is seen multiple times, only the last value is remembered. Empty
 * lines and lines starting with '#' are ignored. */
int codaconf_init_one(const char *cf)
{
    FILE *conf;
    int lineno = 0;
    char *name, *value;
    item_t item __attribute__((unused));

    conf = fopen(cf, "r");
    if (!conf) {
        if (!codaconf_quiet)
            fprintf(stderr,
                    "Cannot read configuration file '%s', "
                    "will use default values.\n",
                    cf);
        return (-1);
    }

    /* remember the last read configuration file */
    if (cf != conffile)
        strcpy(conffile, cf);

    while (fgets(line, MAXLINELEN, conf)) {
        lineno++;
        codaconf_parse_line(line, lineno, &name, &value);
        if (name == NULL)
            continue; /* skip comments and blank lines */

        item = codaconf_find(name, value, 1);

#ifdef CONFDEBUG
        printf("line: %d, name: '%s', value: '%s'", lineno, name, value);
        if (item)
            printf("stored-value: '%s'\n", item->value);
        else
            printf("not found?\n");
#endif
        free(name);
        free(value);
    }
    fclose(conf);

    return (0);
}

/* codaconf_file searches all directories specified by the environment variable
 * CODACONFPATH for 'confname'
 *
 * If the CODACONFPATH is not present the search defaults to,
 *	@sysconfdir@:/usr/local/etc/coda:/etc/coda
 */
char *codaconf_file(const char *confname)
{
    const char *codaconfpath, *end;
    int pathlen, filelen = strlen(confname);

    codaconfpath = getenv("CODACONFPATH");
    if (!codaconfpath)
        codaconfpath = default_codaconfpath;

    while (1) {
        end = strchr(codaconfpath, ':');
        if (!end)
            pathlen = strlen(codaconfpath);
        else
            pathlen = end - codaconfpath;

        /* don't overflow the buffer */
        if ((pathlen + filelen + 1) <= MAXPATHLEN) {
            memcpy(conffile, codaconfpath, pathlen);

            /* don't append an additional one if the path ends in a '/' */
            if (conffile[pathlen - 1] != '/')
                conffile[pathlen++] = '/';

            strcpy(conffile + pathlen, confname);

            /* we should be done as soon as we find a readable file */
            if (access(conffile, R_OK) == 0)
                return conffile;
        }

        if (!end)
            break;
        codaconfpath = end + 1;
    }
    return NULL;
}

/* codaconf_init tries to load the first file that matches 'confname' in
 * CODACONFPATH */
int codaconf_init(const char *confname)
{
    char *cf = codaconf_file(confname);

    if (!cf || codaconf_init_one(cf) != 0)
        return -1;

    return 0;
}

/* codaconf_lookup returns the value associated with name, or NULL on error. */
const char *codaconf_lookup(const char *name, const char *defaultvalue)
{
    item_t cp;

    cp = codaconf_find(name, defaultvalue, 0);

    return cp ? cp->value : NULL;
}

/* release all allocated resources */
void codaconf_free(void)
{
    item_t cp;

    while ((cp = codaconf_table) != NULL) {
        codaconf_table = cp->next;
        free(cp->name);
        free(cp->value);
        free(cp);
    }
}

#ifdef TESTCONF
char configpath[MAXPATHLEN] = TESTCONF;

int main(int argc, char *argv[])
{
    char *var, *val = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <conffile>.<confvar> [defaultvalue]\n",
                argv[0]);
        fprintf(stderr, "e.g. %s venus.cachesize\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    var = strchr(argv[1], '.');
    if (!var) {
        fprintf(stderr, "Didn't find the '.' separator\n");
        exit(EXIT_FAILURE);
    }
    *(var++) = '\0';

    if (argc == 3)
        val = argv[2];

    strcat(configpath, argv[1]);
    strcat(configpath, ".conf");

    codaconf_init(configpath);

    val = codaconf_lookup(var, val);

    codaconf_free();

    if (!val) {
        fprintf(stderr, "Couldn't find a value for '%s'\n", var);
        exit(EXIT_FAILURE);
    }

    fputs(val, stdout);
    fputc('\n', stdout);

    exit(EXIT_SUCCESS);
}
#endif
