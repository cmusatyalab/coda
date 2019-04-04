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
#include "codaconfcmdlineparser.h"
#include "coda_config.h"

bool CodaConfCmdLineParser::option_has_value(int i)
{
    int next_value = i + 1;
    if (next_value >= _argc)
        return false;

    if (is_option(next_value))
        return false;

    return true;
}

bool CodaConfCmdLineParser::is_value(int i)
{
    return !is_option(i);
}

bool CodaConfCmdLineParser::is_option(int i)
{
    if (_argv[i][0] == '-')
        return true;
    return false;
}

const char *CodaConfCmdLineParser::get_value(int i)
{
    int next_value = i + 1;
    if (!option_has_value(i))
        return "1";

    return _argv[next_value];
}

const char *CodaConfCmdLineParser::get_option(int i)
{
    return _argv[i];
}

int CodaConfCmdLineParser::get_next_option_index(int i)
{
    if (option_has_value(i))
        return i + 2;

    return i + 1;
}

void CodaConfCmdLineParser::parse()
{
    int i = 1;
    for (i = 1; i < _argc; i = get_next_option_index(i)) {
        store.set(get_option(i), get_value(i));
    }
}

void CodaConfCmdLineParser::set_args(int argc, char **argv)
{
    _argc = argc;
    _argv = argv;
}
