/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>

#include "copyfile.h"
#include "codaconf.h"

#define MAXLINELEN 256
#define FAILIF(cond, what...) do { \
	if (cond) { fprintf(stderr, ## what); exit(-1); } \
    } while(0)

static void write_val(FILE *outf, int argc, char **argv)
{
    int i;

    /* write the variable name */
    fprintf(outf, "%s=\"", argv[2]);

    /* and dump the value */
    argc--; /* argv starts at '0' */
    for (i = 3; i < argc; i++)
	fprintf(outf, "%s ", argv[i]);
    fprintf(outf, "%s\"\n", argv[argc]);
}

static int match_var(const char *p, const char *var, int len)
{
    /* skip whitespace */
    while (*p == ' ' || *p == '\t') p++;

    /* match variable name */
    if (strncmp(p, var, len) != 0)
	return 0;
    p += len;

    /* skip more whitespace */
    while (*p == ' ' || *p == '\t') p++;

    /* final match */
    return (*p == '=');
}

static void do_rewrite(const char *conffile, int argc, char **argv)
{
    FILE *inf, *outf;
    char line[MAXLINELEN], tmpname[MAXPATHLEN+1], *p;
    int ret, len = strlen(argv[2]), lineno = 0, last_line = 0;

    inf = fopen(conffile, "r");
    FAILIF(!inf, "Failed to open '%s' for reading\n", conffile);

    /* find the last line that contains a (possibly commented) reference to our
     * variable, create a backup copy at the same time */
    snprintf(tmpname, MAXPATHLEN, "%s.bak", conffile);
    outf = fopen(tmpname, "w");
    FAILIF(!outf, "Failed to open '%s' for writing\n", tmpname);

    lineno = 0;
    while(fgets(line, MAXLINELEN, inf)) {
	fputs(line, outf);
	lineno++; p = line;
	/* skip whitespace and comment characters */
	while (*p == ' ' || *p == '\t') p++;
	if (*p == '#') p++;
	if (match_var(p, argv[2], len))
		last_line = lineno;
    }
    if (!last_line) last_line = lineno;

    fclose(outf);

    /* now create a new configuration file, commenting out all previous
     * occurences that defined a value to our variable and append a new
     * define after the last occurence. hopefully this will keep the
     * new variable definition close to the comments */
    snprintf(tmpname, MAXPATHLEN, "%s.new", conffile);
    outf = fopen(tmpname, "w");
    FAILIF(!outf, "Failed to open '%s' for writing\n", tmpname);

    rewind(inf);
    lineno = 0;
    while(fgets(line, MAXLINELEN, inf)) {
	lineno++;

	/* comment any lines that were defining this variable */
	if (match_var(line, argv[2], len))
	    fputs("# ", outf);
	fputs(line, outf);

	/* append the new value after the last found (possibly commented)
	 * definition */
	if (lineno == last_line)
	    write_val(outf, argc, argv);
    }

    fclose(outf);
    fclose(inf);

    ret = rename(tmpname, conffile);
    FAILIF(ret, "Failed to update configuration file '%s'\n", conffile);
}

static void copy_template(char *confbase)
{
    char *tmpname = NULL, *conffile;
    int ret, len;

    /* look for a template configuration file */
    tmpname = malloc(strlen(confbase) + 4);
    FAILIF(!tmpname, "Allocation failed");
    sprintf(tmpname, "%s.ex", confbase);
    conffile = codaconf_file(tmpname);
    free(tmpname);

    FAILIF(!conffile, "Configuration file template '%s.ex' not found\n",
	   confbase);

    /* strip the '.ex' */
    len = strlen(conffile);
    tmpname = malloc(len + 1);
    FAILIF(!tmpname, "Allocation failed");
    strcpy(tmpname, conffile);
    tmpname[len - 3] = '\0';

    /* copy the template configuration file */
    ret = copyfile_byname(conffile, tmpname);
    FAILIF(ret, "Failed to copy template file to '%s'\n", tmpname);
    free(tmpname);
}
    

int main(int argc, char **argv)
{
    char *conffile, *val;
    FAILIF(argc < 2, "Usage: %s <conffile> [<variable> [<value>]]\n", argv[0]);

    conffile = codaconf_file(argv[1]);

    if (argc < 3) {
	if (!conffile) {
	    fprintf(stdout, "/dev/null\n");
	    exit(-1);
	}
	fprintf(stdout, "%s\n", conffile);
	exit(0);
    }

    /* Hmm, should we really copy the template file on lookups as well. On one
     * hand it makes a 'readonly' operation 'write' data. On the other hand
     * there would otherwise be no other way to use the default template
     * without modifications */
    if (!conffile) {
	copy_template(argv[1]);
	conffile = codaconf_file(argv[1]);
	FAILIF(!conffile, "Failed to copy template file to '%s'\n", argv[1]);
    }

    if (argc < 4) {
	conf_init(conffile);
	val = conf_lookup(argv[2], NULL);
	FAILIF(!val, "Variable '%s' not found in '%s'\n", argv[2], conffile);

	fprintf(stdout, "%s\n", val);
	exit(0);
    }

    /* argc >= 4 */
    do_rewrite(conffile, argc, argv);

    exit(0);
}

