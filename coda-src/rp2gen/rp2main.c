#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rp2gen/rp2main.c,v 4.3 1998/04/14 20:59:47 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/

#include <stdio.h>
#include <sys/param.h>
#include "rp2.h"

int32_t yydebug;

#ifdef	__linux__
extern char * basename(char * name);
#endif

extern no_storage();
extern init_lex(), init_table(), yyparse();

struct subsystem subsystem;	/* Holds global subsystem information */
char *server_prefix, *client_prefix;

FILE *file;
FILE *cfile, *sfile, *hfile, *mfile, *pfile;
char *cfile_name, *sfile_name, *hfile_name, *mfile_name, *pfile_name;
char *file_name;
char define_name[MAXPATHLEN]; /* value of __XXX__ */

int32_t HeaderOnlyFlag;  /* set to one if only .h file is to be produced */

static LANGUAGE clanguage, slanguage, mlanguage;


static char *client_includes[] = {
	/* NONE */	"Can't happen",
	/* C */		"#ifdef __cplusplus\nextern \"C\" {\n#endif __cplusplus\n#include <sys/time.h>\n#ifdef __cplusplus\n}\n#endif __cplusplus\n",
	/* PASCAL */	"Can't happen",
	/* F77 */	"Can't happen"
};

static char *server_includes[] = {
	/* NONE */	"Can't happen",
	/* C */		"#include <sys/time.h>",
	/* PASCAL */	"Can't happen",
	/* F77 */	"Can't happen"
};

static char *h_includes[] = {
	/* NONE */	"Can't happen",
	/* C */		"#ifdef __cplusplus\nextern \"C\" {\n#endif __cplusplus\n#include \"rpc2.h\"\n#include \"se.h\"\n#include \"errors.h\"\n#ifdef __cplusplus\n}\n#endif __cplusplus\n",
	/* PASCAL */	"Can't happen",
	/* F77 */	"Can't happen"
};

static char *multi_includes[] = {
	/* NONE */	"Can't happen",
	/* C */		"",
	/* PASCAL */	"Can't happen",
	/* F77 */	"Can't happen"
};

rp2_bool testing;
rp2_bool strictproto = 1;
rp2_bool cplusplus = 0;
rp2_bool tcpdump = 0;
rp2_bool ansi; 
rp2_bool neterrors;

char **cpatharray;  /* array of strings indicating search paths for 
                  included files (defined by -I flag) */
int32_t  cpathcnt; /* no of elements in cpath, initially 0 */

unsigned versionnumber;	/* used to check version */

/* forward decls */
static int32_t GetArgs();
static int32_t h_hack_begin();
static int32_t h_hack_end();
static int32_t header();
static int32_t do_procs();


main(argc, argv)
    int32_t argc;
    char *argv[];
    {
    init_lex();
    init_table();
    GetArgs(argc, argv);
    SetupFiles();

    yyparse();
    do_procs();

    fclose(cfile);
    fclose(sfile);
    h_hack_end(hfile);
    fclose(hfile);
    fclose(mfile);

    exit(0);
    }

static int32_t GetArgs(argc, argv)
    int32_t argc;
    char *argv[];
    {
    register int32_t i;

    testing = RP2_FALSE;
    strictproto = RP2_TRUE;   /* generate strict prototypes */
    cplusplus = RP2_FALSE; /* by default generate .c not .cc files */
    ansi = RP2_FALSE;     /* generate ## paste tokens rather than double-comment */
    neterrors = RP2_FALSE; /* exchange errors in OS independent fashion */
    /* Wire-in client, server and multi languages to be C.
       Should be settable on command line when other languages are
       supported */
    clanguage = C;  
    slanguage = C;
    mlanguage = C;
    pfile_name == NULL;

    if (argc < 2) badargs();
    for (i = 1; i < argc - 1; i++)
	{
	if (strcmp(argv[i], "-s") == 0)
	    {
	    if (++i >= argc) badargs();
	    sfile_name = argv[i];
	    continue;
	    }
	if (strcmp(argv[i], "-c") == 0)
	    {
	    if (++i >= argc) badargs();
	    cfile_name = argv[i];
	    continue;
	    }
	if (strcmp(argv[i], "-h") == 0)
	    {
	    if (++i >= argc) badargs();
	    hfile_name = argv[i];
	    continue;
	    }
	if (strcmp(argv[i], "-m") == 0)
	    {
	    if (++i >= argc) badargs();
	    mfile_name = argv[i];
	    continue;
	    }
	if (strcmp(argv[i], "-p") == 0)
	    {
	    if (++i >= argc) badargs();
	    pfile_name = argv[i];
	    continue;
	    }
	if ((strcmp(argv[i], "-t") == 0) || (strcmp(argv[i],"-tcpdump")==0))
		{tcpdump = RP2_TRUE; continue;}
	if ((strcmp(argv[i], "-e") == 0) || (strcmp(argv[i],"-neterrors")==0))
	    {neterrors = RP2_TRUE; continue;}
	if (strcmp(argv[i], "-cplusplus") == 0)
	    {cplusplus = RP2_TRUE; continue;}
	if (strcmp(argv[i], "-I") == 0)
	    {
	    if (++i >= argc) badargs();
	    if (cpathcnt == 0) cpatharray = (char **)malloc(sizeof(char *));
	    else cpatharray = (char **)realloc(cpatharray, (cpathcnt+1)*sizeof(char *));
	    cpatharray[cpathcnt] = argv[i];
	    cpathcnt++;
	    continue;
	    }
	badargs();
	}
    file_name = argv[argc - 1];
    return(0);
    }

extern char *basename(), *concat();

static int32_t SetupFiles()
    {
    char *base;

    /* Set up files */
    if (file_name == NIL) badargs();
    if (!include2(file_name, "INPUT")) exit(1);

    /* Get base name of input file */
    base = basename(file_name);


    if (hfile_name == NIL) hfile_name = concat(base, ".h");
    hfile = fopen(hfile_name, "w");
    if (hfile == NIL) {perror(hfile_name); exit(-1);}
    /* Special include hack for .h file */
    h_hack_begin(hfile, hfile_name);
    header(hfile, h_includes[(int32_t) clanguage]);

    if ( cplusplus ) {
            if (cfile_name == NIL) cfile_name = concat(base, ".client.cc");
    } else {
            if (cfile_name == NIL) cfile_name = concat(base, ".client.c");
    }
    cfile = fopen(cfile_name, "w");
    if (cfile == NIL) {perror(cfile_name); exit(-1);}
    header(cfile, client_includes[(int32_t) clanguage]);
    fprintf(cfile, "#include \"%s\"\n", hfile_name);

    if ( cplusplus ) {
            if (sfile_name == NIL) sfile_name = concat(base, ".server.cc");
    } else {
            if (sfile_name == NIL) sfile_name = concat(base, ".server.c");
    }
    sfile = fopen(sfile_name, "w");
    if (sfile == NIL) {perror(sfile_name); exit(-1);}
    header(sfile, server_includes[(int32_t) slanguage]);
    fprintf(sfile, "#include \"%s\"\n", hfile_name);

    if ( cplusplus ) { 
            if (mfile_name == NIL) mfile_name = concat(base, ".multi.cc"); 
    } else {
            if (mfile_name == NIL) mfile_name = concat(base, ".multi.c");
    }
    mfile = fopen(mfile_name, "w");
    if (mfile == NIL) {perror(mfile_name); exit(-1);}
    header(mfile, multi_includes[(int32_t) mlanguage]);
    fprintf(mfile, "#include \"%s\"\n", hfile_name);

    if ( cplusplus ) { 
            if (pfile_name == NIL) pfile_name = concat(base, ".print.cc"); 
    } else {
            if (pfile_name == NIL) pfile_name = concat(base, ".print.c");
    }
    pfile = fopen(pfile_name, "w");
    if (pfile == NIL) {perror(pfile_name); exit(-1);}

    free(base);
    }

badargs()
    {
    printf("Usage: rp2gen [-neterrors,-n] [-I incldir] [-s srvstub] [-c clntstub]\n");
    printf("              [-h header] [-m multistub] [-p printstub] \n");
    printf("              [-t tcpdump prettyprint]   file\n");
    exit(-1);
    }

static char uc(c)
    register char c;
{
    return (c >= 'a' && c <= 'z') ? c - ('a'-'A') : c;
}

static int32_t h_hack_begin(where, name)
    FILE *where;
    char *name;
{
    register char *c;

    strcpy(define_name, basename(name));
    for (c=define_name; *c!='\0'; c++)
	if (*c == '.' || *c == '-')
	    *c = '_';
	else
	    *c = uc(*c);
    fprintf(where, "\n#ifndef _%s_\n", define_name);
    fprintf(where, "#define _%s_\n", define_name);
}

static int32_t h_hack_end(where)
    FILE *where;
    {
    fprintf(where, "\n#endif _%s_\n", define_name);
    }

static int32_t header(f, prefix)
    register FILE *f;
    char *prefix;
{
    fprintf(f, "\n/* DO NOT EDIT: generated by rp2gen from %s */\n",
	file_name);
    fputs(prefix, f);
    fputc('\n', f);
}

/****************************\
* 			     *
*  Language specific stuff   *
* 			     *
\****************************/

static cant_happen(type, who, where)
    ENTRY *type;
    WHO who;
    FILE *where;
{
    puts("RP2GEN [can't happen]: no specified language");
    abort();
}

extern cinclude(), cdefine(), ctype(), cproc(), copcodes();

static no_support();

static struct {
    char	*name;		/* Name for printing */
    int32_t		(*include)();	/* Routine for outputting include to file */
    int32_t		(*define)();	/* Routine for outputting define to file */
    int32_t		(*type)();	/* Routine for outputting type to file */
    int32_t		(*proc)();	/* Routine for outputting procedure to file */
    int32_t		(*op_codes)();	/* Routine for generating op codes in .h file */
} lang_struct[] = {

	/* NONE */	{ "N/A",	cant_happen,	cant_happen,	cant_happen,	cant_happen,	cant_happen	},
	/* C */		{ "C",		cinclude,	cdefine,	ctype,	cproc,		copcodes	},
	/* PASCAL */	{ "PASCAL",	no_support,	no_support,	no_support,	no_support,	no_support	},
	/* F77 */	{ "FORTRAN 77",	no_support,	no_support,	no_support,	no_support,	no_support	}

};

static no_support(type, who, where)
    ENTRY *type;
    WHO who;
    FILE *where;
{
    printf("RP2GEN: no language support for %s\n", lang_struct[(int32_t) clanguage]);
    exit(1);
}

spit_type(type)
    ENTRY *type;
{
    if (clanguage != slanguage || clanguage != mlanguage) {
	puts("RP2GEN: warning, SPIT_TYPE does not support multiple languages");
	exit(1);
    }
    (*lang_struct[(int32_t) clanguage].type)(type, CLIENT, hfile);		/* Types always go to .h file */
}

spit_include(filename)
    char *filename;
{
    if (clanguage != slanguage || clanguage != mlanguage) {
	puts("RP2GEN: warning, SPIT_INCLUDE does not support multiple languages");
	exit(1);
    }
    (*lang_struct[(int32_t) clanguage].include)(filename, CLIENT, hfile);
}

spit_define(id, value)
    char *id, *value;
{
    if (clanguage != slanguage || clanguage != mlanguage) {
	puts("RP2GEN: warning, SPIT_DEFINE does not support multiple languages");
	exit(1);
    }
    (*lang_struct[(int32_t) clanguage].define)(id, value, CLIENT, hfile);
}

static int32_t do_procs()
{
    extern PROC *get_head();
    register PROC *head, *proc;
    register rp2_bool seen_new_connection;

    versionnumber = time(0);

    head = get_head();

    /* Do language-independent checks */
    seen_new_connection = RP2_FALSE;
    for (proc=head; proc!=NIL; proc=proc->thread) {
	if (proc->new_connection)
	    if (seen_new_connection) {
		puts("RP2GEN: too many NEW_CONNECTION procedures specified");
		exit(1);
	    } else
		seen_new_connection = RP2_TRUE;
    }

    /* Generate op codes in .h file */
    (*lang_struct[(int32_t) clanguage].op_codes)(head, CLIENT, hfile);

    /* Generate client file */
    if (HeaderOnlyFlag)
	    {fclose(cfile); unlink(cfile_name);}
    else (*lang_struct[(int32_t) clanguage].proc)(head, CLIENT, cfile);

    /* Generate server file */
    if (HeaderOnlyFlag)
	    {fclose(sfile); unlink(sfile_name);}
    else (*lang_struct[(int32_t) slanguage].proc)(head, SERVER, sfile);

    /* Generate multi file */
    if (HeaderOnlyFlag)
	    {fclose(mfile); unlink(mfile_name);}
    else (*lang_struct[(int32_t) mlanguage].proc)(head, MULTI, mfile);
    
    if (HeaderOnlyFlag)
	    {fclose(pfile); unlink(pfile_name);}
    else (*lang_struct[(int32_t) slanguage].proc)(head, DUMP, pfile);
}
