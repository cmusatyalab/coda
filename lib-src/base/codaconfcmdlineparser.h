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

#ifndef _CODACONFCMDLINEPARSER_H_
#define _CODACONFCMDLINEPARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include "coda_config.h"

#ifdef __cplusplus
}
#endif

#include "codaconfparser.h"

class CodaConfCmdLineParser : private CodaConfParser {
private:
    int _argc;
    char **_argv;

    bool option_has_value(int i);
    bool is_value(int i);
    bool is_option(int i);
    const char *get_value(int i);
    const char *get_option(int i);
    int get_next_option_index(int i);

public:
    CodaConfCmdLineParser(StringKeyValueStore &s)
        : CodaConfParser(s)
    {
    }

    void set_args(int argc, char **argv);

    void parse();
};

#endif /* _CODACONFCMDLINEPARSER_H_ */
