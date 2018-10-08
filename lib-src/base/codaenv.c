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
#include "codaenv.h"


char * codaenv_find(const char * var_name) {
    char env_var[256];
    char * val = NULL;

    snprintf(env_var, sizeof(env_var), "CODA_%s", var_name);

    val = getenv(env_var);

    if (!val) {
        return NULL;
    }

    return strdup(val);
}

int codaenv_int(const char * var_name, const int prev_val)
{
    char * val = codaenv_find(var_name);

    if (!val) {
        return prev_val;
    }

    return atoi(val);
}


const char * codaenv_str(const char * var_name, const char * prev_val)
{
    char * val = codaenv_find(var_name);

    if (!val) {
        return prev_val;
    }

    return val;
}
