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


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

#define FIRSTLINE "#ifndef _BLURB_\n"
#define FIRSTLINE_RPC "%{#ifndef _BLURB_\n"
#define FIRSTLINE_LEXYACC "%{\n"
#define SECONDLINE "#define _BLURB_\n"
#define SECONDLINE_LEXYACC "#ifndef _BLURB_\n#define _BLURB_\n"
#define LASTLINE "#endif /*_BLURB_*/\n"
#define LASTLINE_RPC "#endif /*_BLURB_*/%}\n"

#define MAXLINELEN 1024  /* max len of line in input file */

int NoRenameFlag;  /* if set to 1, original file is not removed */
int VerboseFlag;    /* Set to one to hear ball by ball description */


char *BlurbFile = "/usr/coda/include/blurb.doc";;  


/* set based on file name or contents */
enum {CFILE, MAKEFILE, ASMFILE, RPCFILE, LEXFILE, YACCFILE, SHELLSCRIPT, UNKNOWN} FileType;  


/* Function decls */
void ProcessOneFile(char *name);
int FixFile(FILE *fin, FILE *fout, FILE *fblurb);
int EatArgs(int argc, char *argv[]);
void DetermineFileType(char *name);


main(int argc, char *argv[])
    {
    register int i, file1;

    file1 = EatArgs(argc, argv);
    
    for (i=file1; i < argc; i++)
	{
	ProcessOneFile(argv[i]);
	}
    }


void ProcessOneFile(char *name)
    {
    FILE *fin, *fout, *fblurb;
    char tmpfile[MAXPATHLEN];

    fin = fout= fblurb = 0;

    DetermineFileType(name);
    if (FileType == UNKNOWN)
	{
	printf("%s: unknown file type ... ignoring\n", name);
	goto QUIT;
	}

    /* Play safe initially */
    fblurb = fopen (BlurbFile, "r");
    if (!fblurb) {perror(BlurbFile); goto QUIT;}

    fin = fopen (name, "r");
    if (!fin) {perror(name); goto QUIT;}
    
    if (SafeStrCpy(tmpfile, name, sizeof(tmpfile))) goto QUIT;
    if (SafeStrCat(tmpfile, ".b", sizeof(tmpfile))) goto QUIT;
    
    fout = fopen (tmpfile, "w");
    if (!fout) {perror(tmpfile); goto QUIT;}

    if (VerboseFlag) printf("%s ...", name);
    if (FixFile(fin, fout, fblurb)) goto QUIT;
    
    if (NoRenameFlag) goto QUIT;

    /* Now do risky part -- here's where transactions would help! */
    if (unlink(name)) {perror(name); goto QUIT;}
    if (rename(tmpfile, name)) {perror(tmpfile); goto QUIT;}

QUIT:
    if (fin) fclose(fin);
    if (fout) fclose(fout);
    if (fblurb) fclose(fblurb);
    }

int FixFile(FILE *fin, FILE *fout, FILE *fblurb)
    {
    char nextline[MAXLINELEN+1], blurbline[MAXLINELEN+1], shellcommand[MAXLINELEN+1];
    char *s, *firstline, *secondline, *lastline;

    switch (FileType)
	{
	case RPCFILE:
	                firstline = FIRSTLINE_RPC;
			secondline = SECONDLINE;
			lastline = LASTLINE_RPC;
			break;
			
	case LEXFILE:
	case YACCFILE:
	                firstline = FIRSTLINE_LEXYACC;
			secondline = SECONDLINE_LEXYACC;
			lastline = LASTLINE;
			break;


	default:	firstline = FIRSTLINE;
			secondline = SECONDLINE;
			lastline = LASTLINE;
			break;
		
	}
    
    /* Strip off existing blurb, if any */

    /* Get the first line */
    s = fgets(nextline, MAXLINELEN, fin);
    if (!s) return(-1);

    /* Save it if this is a shell script */
    if (FileType == SHELLSCRIPT)
	{
	strcpy(shellcommand, nextline);
	s = fgets(nextline, MAXLINELEN, fin); 
	if (!s) return(-1);
	}

    if (strcmp(nextline, firstline) == 0) 
	{/* Found first line of old blurb */
	do 
	    {
	    s = fgets(nextline, MAXLINELEN, fin);
	    if (!s) {printf("Premature end of file\n"); return(-1);}
	    }
	while (strcmp(nextline, lastline) != 0);
	s = fgets(nextline, MAXLINELEN, fin); 
	if (!s) nextline[0] = 0;  /* so writing it out will be ok */
	}
    
    /* At this point nextline contains the first non-blurb line of the input */

    /* Write preamble */
    if (FileType == SHELLSCRIPT) fputs(shellcommand, fout);
    fputs(firstline, fout);
    fputs(secondline, fout);
    if (FileType == ASMFILE) fputs("#ifdef undef\n", fout);

    /* Copy blurb */
    for(s = fgets(blurbline, MAXLINELEN, fblurb); s != NULL;
		s = fgets(blurbline, MAXLINELEN, fblurb))
	{
	if (FileType == MAKEFILE || FileType == SHELLSCRIPT) fputc('#', fout);
	fputs(blurbline, fout);
	}
    
    
    /* Write epilogue */
    if (FileType == ASMFILE) fputs("#endif undef\n", fout);
    fputs(lastline, fout);
    fputc('\n', fout);

    /* Copy rest of input file to output */
    do
	{
	fputs(nextline, fout);
	s = fgets(nextline, MAXLINELEN, fin);
	}
    while (s);
    
    if (VerboseFlag) printf("OK\n");
    return(0);
    }


int EatArgs(int argc, char *argv[])
    /* Returns index of first uneaten arg in argv[] */
    {
    register int i;

    if (argc == 1) goto BadArgs;

    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-n") == 0)
	    {
	    NoRenameFlag = 1;
	    continue;
	    }

	if (strcmp(argv[i], "-v") == 0)
	    {
	    VerboseFlag = 1;
	    continue;
	    }

	if (strcmp(argv[i], "-f") == 0 && i < argc)
	    {
	    BlurbFile = argv[++i];
	    continue;
	    }

	if (*argv[i] == '-') goto BadArgs;
	return(i);
	}
    
    BadArgs:
    fprintf(stderr, "Usage: blurb [-v] [-n] [-f blurbfile] file1 file2 ....\n");
    exit(-1);
    }

int TestFileContents(char *fname, char *match)
    /* Opens file fname and reads first line;
       returns 0 if the beginning of this line matches the string match; 
       returns -1 otherwise */
    {
    FILE *f;
    char buf[MAXLINELEN+1];

    f = fopen(fname, "r");
    if (!f)
	{
	perror(fname);
	return(-1);
	}
    if (!fgets(buf, MAXLINELEN, f))
	{/* probably zero-length file */
	fclose(f);
	return(-1);
	}
    else fclose(f);

    if (strncmp(buf, match, strlen(match)) == 0) return(0);
    else return(-1);
    }



void DetermineFileType(char *name)
    {
    int ll;
    char *lc; /* last pathname component of name */
    

    ll = strlen(name);
    lc = rindex(name, '/');
    if (lc) lc++;
    else lc = name;


    /* Have to check for .lex and .yacc endings before makefile for SynRGen sources */
    if (ll > 3 && strcmp(&name[ll-4], ".lex") == 0)
	{
	FileType = LEXFILE;
	return;
        }

    if (ll > 4 && strcmp(&name[ll-5], ".yacc") == 0)
	{
	FileType = YACCFILE;
	return;
        }

    if (strncmp(lc, "Makefile", strlen("Makefile")) == 0  
	    || strncmp(lc, "makefile", strlen("makefile")) == 0
	    || strncmp(lc, "makefile.in", strlen("makefile.in")) == 0
	    || strncmp(lc, "GNUmakefile.in", strlen("GNUmakefile.in")) == 0
	    || strncmp(lc, "Makefile.in", strlen("Makefile.in")) == 0
	    || strncmp(lc, "GNUmakefile", strlen("GNUmakefile")) == 0
	    || strncmp(lc, "Makeconf", strlen("Makeconf")) == 0)
	{
	FileType = MAKEFILE;
	return;
	}

    if (ll > 1 && name[ll-2] == '.' && (name[ll-1] == 'h' || name[ll-1] == 'c'))
	{
	FileType = CFILE;
	return;
	}

    if (ll > 2  && strcmp(&name[ll-3], ".cc") == 0 )
	{
	FileType = CFILE;
	return;
	}


    if (ll > 1 && name[ll-2] == '.' && name[ll-1] == 's')
	{
	FileType = ASMFILE;
	return;
	}

    if ((ll > 3 && strcmp(&name[ll-4], ".rpc") == 0)
    	|| (ll > 4 && strcmp(&name[ll-5], ".rpc2") == 0))
	{
	FileType = RPCFILE;
	return;
	}

    /* Name doesn't help; look inside the file */
    if (TestFileContents(name, "#!") == 0)
	{
	FileType = SHELLSCRIPT;
	return;
	}


    /* Hopeless! */    
    FileType = UNKNOWN;
    }


