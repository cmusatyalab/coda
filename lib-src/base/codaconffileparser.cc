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

#include <errno.h>

#ifdef __cplusplus
}
#endif

#include <sys/param.h>
#include <stdio.h>
#include "coda_string.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "codaconffileparser.h"
#include "coda_config.h"

/* some hardcoded options, probably both undef'd for normal use      */
/* CONFDEBUG: annoying debugging related messages                    */
#undef CONFDEBUG
#undef CONFWRITE

static const char *default_codaconfpath = SYSCONFDIR
    ":/usr/local/etc/coda:/etc/coda:";

void CodaConfFileParser::parse_line(char *line, int lineno, char **name,
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

FILE *CodaConfFileParser::open_conffile()
{
    FILE *conf = fopen(conffile, "r");
    if (conf)
        return conf;

    if (!quiet)
        fprintf(stderr,
                "Cannot read configuration file '%s', "
                "will use default values.\n",
                conffile);
}

int CodaConfFileParser::parse()
{
    FILE *conf;
    int lineno = 0;
    char *name, *value;
    const char *stored_value = NULL;
    int ret_code             = 0;

    conf = open_conffile();
    if (!conf)
        return EIO;

    while (fgets(line, MAXLINELEN, conf)) {
        lineno++;
        parse_line(line, lineno, &name, &value);
        if (name == NULL)
            continue; /* skip comments and blank lines */

        if (store.has_key(name)) {
            store.replace(name, value);
            replace_in_file(name, value);
            ret_code = EEXIST;
        } else {
            store.add(name, value);
        }

        stored_value = store.get_value(name);

#ifdef CONFDEBUG
        printf("line: %d, name: '%s', value: '%s'", lineno, name, value);
        if (stored_value)
            printf("stored-value: '%s'\n", stored_value);
        else
            printf("not found?\n");
#endif
        free(name);
        free(value);
    }
    fclose(conf);

    return ret_code;
}

char *CodaConfFileParser::format_conffile_full_path(const char *confname)
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

void CodaConfFileParser::set_conffile(const char *confname)
{
    format_conffile_full_path(confname);
}

void CodaConfFileParser::replace_in_file(const char *name, const char *value)
{
#ifdef CONFWRITE
    FILE *conf;

    conf = fopen(conffile, "a");
    if (conf) {
        fputs(name, conf);
        fputs("=\"", conf);
        fputs(value, conf);
        fputs("\"\n", conf);
        fclose(conf);
    }
#endif
}
