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

#include "codaconfdb.h"

#ifdef __cplusplus
}
#endif

/* nobody outside of this file needs to be exposed to these structures. */

class CodaConfFileParser : public CodaConfDB {
private:
    void parse_line(char *line, int lineno, char **name, char **value);

public:
    /* conf_init reads (or merges) the name=value tuples from the conffile. If a
    * name is seen multiple times, only the last value is remembered. Empty lines
    * and lines starting with '#' are ignored. */
    int init_one(const char *conffile);

    /* helpers */

    /* file searches all directories specified by the environment variable
    *		 CODACONFPATH for 'confname'.
    * init uses file to find a configuration file and then calls
    *		 conf_init on this file.
    *
    * If the CODACONFPATH is not present the search defaults to,
    *	@sysconfdir@:/usr/local/etc/coda:/etc/coda
    */
    int init(const char *confname);
    char *file(const char *confname);
};

#endif /* _CODACONFFILEPARSER_H_ */
