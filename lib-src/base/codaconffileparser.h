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

#ifndef _CODACONFFILEPARSER_H_
#define _CODACONFFILEPARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include "coda_config.h"

#ifdef __cplusplus
}
#endif

#include "stringkeyvaluestore.h"

class CodaConfFileParser {
private:
    /* buffer to read lines of config data */
    static const int MAXLINELEN = 256;
    char line[MAXLINELEN];
    char conffile[MAXPATHLEN + 1];

    StringKeyValueStore &store;
    bool quiet;

    void parse_line(char *line, int lineno, char **name, char **value);

    int parse_full_path_conffile(const char *conffile);

    void replace_in_file(const char *name, const char *value);

public:
    CodaConfFileParser(StringKeyValueStore &s)
        : store(s)
        , quiet(true)
    {
    }

    int parse(const char *confname);
    char *get_conffile_full_path(const char *confname);
};

#endif /* _CODACONFFILEPARSER_H_ */
