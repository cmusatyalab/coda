/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2026 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "rp2.h"

int32_t yydebug;

static int32_t SetupFiles();
static void badargs(void);

struct subsystem subsystem; /* Holds global subsystem information */
char *server_prefix, *client_prefix;

FILE *file;
FILE *cfile = NULL, *sfile = NULL, *hfile = NULL, *mfile = NULL, *pfile = NULL,
     *libfile = NULL;
char *cfile_name, *sfile_name, *hfile_name, *mfile_name, *pfile_name,
    *libfile_name;
;
char *file_name;
char *outdir_name; /* write generated files to this directory, default "." */
char define_name[MAXPATHLEN]; /* value of __XXX__ */

int32_t HeaderOnlyFlag; /* set to one if only .h file is to be produced */

static LANGUAGE clanguage, slanguage, mlanguage;

static char *client_includes[] = {
    /* NONE */ "Can't happen",
    /* C */ "",
    /* PASCAL */ "Can't happen",
    /* F77 */ "Can't happen"
};

static char *server_includes[] = {
    /* NONE */ "Can't happen",
    /* C */ "",
    /* PASCAL */ "Can't happen",
    /* F77 */ "Can't happen"
};

static char *h_includes[] = {
    /* NONE */ "Can't happen",
    /* C */
    "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n#include "
    "<rpc2/rpc2.h>\n#include <rpc2/se.h>\n#include <rpc2/errors.h>\n#include "
    "<rpc2/pack_helper.h>\n\n#ifdef __cplusplus\n}\n#endif\n",
    /* PASCAL */ "Can't happen",
    /* F77 */ "Can't happen"
};

static char *helper_includes[] = {
    /* NONE */ "Can't happen",
    /* C */
    "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n#include "
    "<stdlib.h>\n\n#include <rpc2/pack_helper.h>\n\n#ifdef "
    "__cplusplus\n}\n#endif\n",
    /* PASCAL */ "Can't happen",
    /* F77 */ "Can't happen"
};

static char *multi_includes[] = {
    /* NONE */ "Can't happen",
    /* C */ "",
    /* PASCAL */ "Can't happen",
    /* F77 */ "Can't happen"
};

rp2_bool testing;
rp2_bool strictproto = 1;
rp2_bool cplusplus   = 0;
rp2_bool tcpdump     = 0;
rp2_bool ansi;
rp2_bool neterrors;

char **cpatharray; /* array of strings indicating search paths for
                  included files (defined by -I flag) */
int32_t cpathcnt; /* no of elements in cpath, initially 0 */

time_t versionnumber; /* used to check version */

/* forward decls */
static int32_t GetArgs(int argc, char **argv);
static int32_t h_hack_begin(FILE *hfile, char *name);
static int32_t h_hack_end(FILE *hfile);
static int32_t header(FILE *hfile, char *name);
static int32_t do_procs();
static int32_t SetupFiles();

int main(int argc, char *argv[])
{
    init_lex();
    init_table();
    GetArgs(argc, argv);
    SetupFiles();

    yyparse();
    do_procs();

    if (cfile) {
        fclose(cfile);
        cfile = NULL;
    }
    if (sfile) {
        fclose(sfile);
        sfile = NULL;
    }
    if (hfile) {
        h_hack_end(hfile);
        fclose(hfile);
        hfile = NULL;
    }
    if (mfile) {
        fclose(mfile);
        mfile = NULL;
    }

    if (libfile) {
        fclose(libfile);
        libfile = NULL;
    }

    exit(EXIT_SUCCESS);
}

static int32_t GetArgs(int argc, char *argv[])
{
    register int32_t i;

    testing     = RP2_FALSE;
    strictproto = RP2_TRUE; /* generate strict prototypes */
    cplusplus   = RP2_FALSE; /* by default generate .c not .cc files */
    ansi = RP2_FALSE; /* generate ## paste tokens rather than double-comment */
    neterrors   = RP2_FALSE; /* exchange errors in OS independent fashion */
    outdir_name = NIL; /* write generated files to current directory */
    /* Wire-in client, server and multi languages to be C.
       Should be settable on command line when other languages are supported */
    clanguage  = C;
    slanguage  = C;
    mlanguage  = C;
    pfile_name = NULL;

    if (argc < 2)
        badargs();
    for (i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            if (++i >= argc)
                badargs();
            sfile_name = argv[i];
            continue;
        }
        if (strcmp(argv[i], "-c") == 0) {
            if (++i >= argc)
                badargs();
            cfile_name = argv[i];
            continue;
        }
        if (strcmp(argv[i], "-h") == 0) {
            if (++i >= argc)
                badargs();
            hfile_name = argv[i];
            continue;
        }
        if (strcmp(argv[i], "-m") == 0) {
            if (++i >= argc)
                badargs();
            mfile_name = argv[i];
            continue;
        }
        if (strcmp(argv[i], "-p") == 0) {
            if (++i >= argc)
                badargs();
            pfile_name = argv[i];
            continue;
        }
        if ((strcmp(argv[i], "-t") == 0) ||
            (strcmp(argv[i], "-tcpdump") == 0)) {
            tcpdump = RP2_TRUE;
            continue;
        }
        if ((strcmp(argv[i], "-e") == 0) ||
            (strcmp(argv[i], "-neterrors") == 0)) {
            neterrors = RP2_TRUE;
            continue;
        }
        if (strcmp(argv[i], "-cplusplus") == 0) {
            cplusplus = RP2_TRUE;
            continue;
        }
        if (strcmp(argv[i], "-I") == 0) {
            if (++i >= argc)
                badargs();
            if (cpathcnt == 0)
                cpatharray = (char **)malloc(sizeof(char *));
            else
                cpatharray = (char **)realloc(cpatharray,
                                              (cpathcnt + 1) * sizeof(char *));
            assert(cpatharray != NULL);
            cpatharray[cpathcnt] = argv[i];
            cpathcnt++;
            continue;
        }
        if (strcmp(argv[i], "-C") == 0) {
            if (++i >= argc)
                badargs();
            outdir_name = argv[i];
            continue;
        }
        badargs();
    }
    file_name = argv[argc - 1];
    return (0);
}

/* Build the path to a generated file, prefixed with the output
   directory given by -C, if any, we'll just leak the allocations */
static char *outpath(char *name)
{
    if (outdir_name == NIL) {
        // return concat("", name);
        return name;
    }
    return concat3elem(outdir_name, "/", name);
}

static int32_t SetupFiles()
{
    char *base;

    /* Set up files */
    if (file_name == NIL)
        badargs();
    if (!include2(file_name, "INPUT"))
        exit(EXIT_FAILURE);

    /* Get base name of input file */
    base = coda_rp2_basename(file_name);

    if (hfile_name == NIL)
        hfile_name = concat(base, ".h");
    hfile = fopen(outpath(hfile_name), "w");
    if (hfile == NIL) {
        perror(hfile_name);
        exit(EXIT_FAILURE);
    }
    /* Special include hack for .h file */
    h_hack_begin(hfile, hfile_name);
    header(hfile, h_includes[(int32_t)clanguage]);

    if (cplusplus) {
        if (cfile_name == NIL)
            cfile_name = concat(base, ".client.cc");
    } else {
        if (cfile_name == NIL)
            cfile_name = concat(base, ".client.c");
    }
    cfile = fopen(outpath(cfile_name), "w");
    if (cfile == NIL) {
        perror(cfile_name);
        exit(EXIT_FAILURE);
    }
    header(cfile, client_includes[(int32_t)clanguage]);
    fprintf(cfile, "#include \"%s\"\n\n", hfile_name);

    if (cplusplus) {
        if (sfile_name == NIL)
            sfile_name = concat(base, ".server.cc");
    } else {
        if (sfile_name == NIL)
            sfile_name = concat(base, ".server.c");
    }
    sfile = fopen(outpath(sfile_name), "w");
    if (sfile == NIL) {
        perror(sfile_name);
        exit(EXIT_FAILURE);
    }
    header(sfile, server_includes[(int32_t)slanguage]);
    fprintf(sfile, "#include \"%s\"\n\n", hfile_name);

    if (cplusplus) {
        if (mfile_name == NIL)
            mfile_name = concat(base, ".multi.cc");
    } else {
        if (mfile_name == NIL)
            mfile_name = concat(base, ".multi.c");
    }
    mfile = fopen(outpath(mfile_name), "w");
    if (mfile == NIL) {
        perror(mfile_name);
        exit(EXIT_FAILURE);
    }
    header(mfile, multi_includes[(int32_t)mlanguage]);
    fprintf(mfile, "#include \"%s\"\n\n", hfile_name);

    if (cplusplus) {
        if (pfile_name == NIL)
            pfile_name = concat(base, ".print.cc");
    } else {
        if (pfile_name == NIL)
            pfile_name = concat(base, ".print.c");
    }
    pfile = fopen(outpath(pfile_name), "w");
    if (pfile == NIL) {
        perror(pfile_name);
        exit(EXIT_FAILURE);
    }

    libfile_name = concat(base, ".helper.c");
    libfile      = fopen(outpath(libfile_name), "w");
    if (libfile == NIL) {
        perror(libfile_name);
        exit(EXIT_FAILURE);
    }
    header(libfile, helper_includes[(int32_t)clanguage]);
    fprintf(libfile, "#include \"%s\"\n\n", hfile_name);

    free(base);

    return -1;
}

static void badargs(void)
{
    printf(
        "Usage: rp2gen [-neterrors,-n] [-I incldir] [-C outdir] "
        "[-s srvstub]\n");
    printf("              [-c clntstub]\n");
    printf("              [-h header] [-m multistub] [-p printstub] \n");
    printf("              [-t tcpdump prettyprint]   file\n");
    exit(EXIT_FAILURE);
}

static char uc(char c)
{
    return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
}

static int32_t h_hack_begin(FILE *where, char *name)
{
    char *c;

    c = coda_rp2_basename(name);
    strcpy(define_name, c);
    free(c);
    for (c = define_name; *c != '\0'; c++)
        if (*c == '.' || *c == '-')
            *c = '_';
        else
            *c = uc(*c);
    fprintf(where, "#ifndef _%s_\n", define_name);
    fprintf(where, "#define _%s_\n\n", define_name);
    return -1;
}

static int32_t h_hack_end(FILE *where)
{
    fprintf(where, "\n#endif /* _%s_ */\n", define_name);
    return -1;
}

static int32_t header(FILE *f, char *prefix)
{
    fprintf(f, "/* DO NOT EDIT: generated by rp2gen from %s */\n", file_name);
    fputs(prefix, f);
    fputc('\n', f);
    return -1;
}

/****************************\
* 			     *
*  Language specific stuff   *
* 			     *
\****************************/

static void cant_happen(ENTRY *type, WHO who, FILE *where)
{
    puts("RP2GEN [can't happen]: no specified language");
    abort();
    return;
}

static void no_support(ENTRY *type, WHO who, FILE *where);

static struct {
    char *name; /* Name for printing */
    void (*include)(char *filename, WHO who, FILE *where); /* Routine for
                                                             outputting
                                                             include to file */
    void (*define)(char *id, char *value, WHO who, FILE *where); /* Routine
                                                                   for
                                                                   outputting
                                                                   define to
                                                                   file */
    void (*type)(ENTRY *e, WHO who, FILE *where); /* Routine for outputting
                                                   type to file */
    void (*proc)(PROC *head, WHO who, FILE *where); /* Routine for outputting
                                                    procedure to file */
    void (*op_codes)(PROC *head, WHO who, FILE *where); /* Routine for
                                                         generating op codes
                                                         in .h file */
} lang_struct[] = {

    /* NONE */
    { "N/A", (void (*)(char *, WHO, FILE *))cant_happen,
      (void (*)(char *, char *, WHO, FILE *))cant_happen,
      (void (*)(ENTRY *, WHO, FILE *))cant_happen,
      (void (*)(PROC *, WHO, FILE *))cant_happen,
      (void (*)(PROC *, WHO, FILE *))cant_happen },
    /* C */ { "C", cinclude, cdefine, ctype, cproc, copcodes },
    /* PASCAL */
    { "PASCAL", (void (*)(char *, WHO, FILE *))no_support,
      (void (*)(char *, char *, WHO, FILE *))no_support,
      (void (*)(ENTRY *, WHO, FILE *))no_support,
      (void (*)(PROC *, WHO, FILE *))no_support,
      (void (*)(PROC *, WHO, FILE *))no_support },
    /* F77 */
    { "FORTRAN 77", (void (*)(char *, WHO, FILE *))no_support,
      (void (*)(char *, char *, WHO, FILE *))no_support,
      (void (*)(ENTRY *, WHO, FILE *))no_support,
      (void (*)(PROC *, WHO, FILE *))no_support,
      (void (*)(PROC *, WHO, FILE *))no_support }
};

static void no_support(ENTRY *type, WHO who, FILE *where)
{
    printf("RP2GEN: no language support for %s\n",
           lang_struct[(int32_t)clanguage].name);
    exit(EXIT_FAILURE);
}

void spit_type(ENTRY *type)
{
    if (clanguage != slanguage || clanguage != mlanguage) {
        puts("RP2GEN: warning, SPIT_TYPE does not support multiple languages");
        exit(EXIT_FAILURE);
    }
    (*lang_struct[(int32_t)clanguage].type)(
        type, RP2_CLIENT, hfile); /* Types always go to .h file */

    print_struct_func(type->type, libfile, hfile, type->name);
}

void spit_include(char *filename)
{
    if (clanguage != slanguage || clanguage != mlanguage) {
        puts(
            "RP2GEN: warning, SPIT_INCLUDE does not support multiple languages");
        exit(EXIT_FAILURE);
    }
    (*lang_struct[(int32_t)clanguage].include)(filename, RP2_CLIENT, hfile);
}

void spit_define(char *id, char *value)
{
    if (clanguage != slanguage || clanguage != mlanguage) {
        puts(
            "RP2GEN: warning, SPIT_DEFINE does not support multiple languages");
        exit(EXIT_FAILURE);
    }
    (*lang_struct[(int32_t)clanguage].define)(id, value, RP2_CLIENT, hfile);
}

static int32_t do_procs()
{
    register PROC *head, *proc;
    register rp2_bool seen_new_connection;

    versionnumber = time(0);

    head = get_head();

    /* Do language-independent checks */
    seen_new_connection = RP2_FALSE;
    for (proc = head; proc != NIL; proc = proc->thread) {
        if (proc->new_connection) {
            if (seen_new_connection) {
                puts("RP2GEN: too many NEW_CONNECTION procedures specified");
                exit(EXIT_FAILURE);
            } else
                seen_new_connection = RP2_TRUE;
        }
    }

    /* Generate op codes in .h file */
    (*lang_struct[(int32_t)clanguage].op_codes)(head, RP2_CLIENT, hfile);
    (*lang_struct[(int32_t)clanguage].proc)(head, RP2_HELPER, libfile);

    /* Generate client file */
    if (HeaderOnlyFlag) {
        if (cfile) {
            fclose(cfile);
            cfile = NULL;
        }
        unlink(outpath(cfile_name));
    } else
        (*lang_struct[(int32_t)clanguage].proc)(head, RP2_CLIENT, cfile);

    /* Generate server file */
    if (HeaderOnlyFlag) {
        if (sfile) {
            fclose(sfile);
            sfile = NULL;
        }
        unlink(outpath(sfile_name));
    } else
        (*lang_struct[(int32_t)slanguage].proc)(head, RP2_SERVER, sfile);

    /* Generate multi file */
    if (HeaderOnlyFlag) {
        if (mfile) {
            fclose(mfile);
            mfile = NULL;
        }
        unlink(outpath(mfile_name));
    } else
        (*lang_struct[(int32_t)mlanguage].proc)(head, RP2_MULTI, mfile);

    if (HeaderOnlyFlag) {
        if (pfile) {
            fclose(pfile);
            pfile = NULL;
        }
        unlink(outpath(pfile_name));
    } else
        (*lang_struct[(int32_t)slanguage].proc)(head, RP2_DUMP, pfile);

    return -1;
}
