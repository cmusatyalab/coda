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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/







/*
 *  Hoard database front-end.
 *
 *  ToDo:
 *      1. Clarify attribute meanings.
 *      2. Eliminate Modify command.
 *      3. Allow List command to have outfile in Coda.
 *      4. New attribute for cross-volume inheritance?
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
extern FILE *_findiop();
#include <libc.h>
extern int execvp(const char *, const char **);
#include <sysent.h>
#include <stdarg.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <olist.h>
#include <venusioctl.h>
#include <vice.h>
#include <hdb.h>



/* Manifest Constants. */
#define	CODA_ROOT   "/coda"
#define	STREQ(a, b) (strcmp(a, b) == 0)
#define	FATAL	1
#define	MAXCMDLEN   (MAXPATHLEN + 1024)
#define	ALL_UIDS    ((vuid_t)-1)


/* Global Variables. */
char cwd[MAXPATHLEN];
vuid_t ruid;
vuid_t euid;
int Debug = 0;
int Verbose = 0;


#define	DEBUG(stmt) { if (Debug) { stmt ; fflush(stdout); } }

/* Private routines. */
PRIVATE FILE *ParseCommandLine(int, char *[]);
PRIVATE void ParseHoardCommands(FILE *, olist&, olist&, olist&, olist&, olist&, olist&, olist&, olist&);
PRIVATE char *my_fgets(char *, int, FILE *);
PRIVATE char *GetToken(char *, char *, char **);
PRIVATE int my_atoi(char *, int *);
PRIVATE int canonicalize(char *, VolumeId *, char *, char *, VolumeId *, char (*)[MAXPATHLEN]);
PRIVATE char *vol_getwd(VolumeId *, char *, char *);
PRIVATE VolumeId GetVid(char *);
PRIVATE void DoClears(olist&);
PRIVATE void DoAdds(olist&);
PRIVATE void DoModifies(olist&);
PRIVATE void DoDeletes(olist&);
PRIVATE void DoLists(olist&);
PRIVATE void DoWalks(olist&);
PRIVATE void DoVerifies(olist&);
PRIVATE void DoEnables(olist&);
PRIVATE void DoDisables(olist&);
PRIVATE void MetaExpand(olist&, char *, int, int);
PRIVATE void ExpandNode(char *, VolumeId, char *, olist&, int, int);
PRIVATE int CreateOutFile(char *, char *);
PRIVATE void RenameOutFile(char *, char *);
PRIVATE void error(int, char * ...);
PRIVATE void usage();
PRIVATE void parse_error(char *);

/* *****  Wrapper classes for HDB pioctl messages.  ***** */

class clear_entry : public olink {
  public:
    hdb_clear_msg msg;

    clear_entry(vuid_t cuid) {
	msg.cuid = cuid;
	msg.ruid = ruid;
    }
};

class add_entry : public olink {
  public:
    hdb_add_msg msg;

    add_entry(VolumeId volno, char *name, int priority, int attributes) {
	msg.volno = volno;
	strcpy(msg.name, name);
	msg.priority = priority;
	msg.attributes = attributes;
	msg.ruid = ruid;
    }
};

class delete_entry : public olink {
  public:
    hdb_delete_msg msg;

    delete_entry(VolumeId volno, char *name) {
	msg.volno = volno;
	strcpy(msg.name, name);
	msg.ruid = ruid;
    }
};

class list_entry : public olink {
  public:
    hdb_list_msg msg;
    char tname[MAXPATHLEN];

    list_entry(char *lname, vuid_t luid) {
	sprintf(msg.outfile, "/usr/coda/tmp/XXXXXX");
	(void)mktemp(msg.outfile);

	strcpy(tname, lname);
	msg.luid = luid;
	msg.ruid = ruid;
    }
};

class walk_entry : public olink {
  public:
    hdb_walk_msg msg;

    walk_entry() {
	msg.ruid = ruid;
    }
};

/* hdb_walk_msg is the simplest kind. Don't need another type of msg. */
class enable_entry : public olink {
  public:
    hdb_walk_msg msg;

    enable_entry() {
	msg.ruid = ruid;
    }
};

class disable_entry : public olink {
  public:
    hdb_walk_msg msg;

    disable_entry() {
	msg.ruid = ruid;
    }
};

class verify_entry : public olink {
  public:
    hdb_verify_msg msg;
    char tname[MAXPATHLEN];

    verify_entry(char *vname, vuid_t vuid, int verbosity) {
	sprintf(msg.outfile, "/usr/coda/tmp/XXXXXX");
	(void)mktemp(msg.outfile);

	strcpy(tname, vname);
	msg.luid = vuid;
	msg.ruid = ruid;
	msg.verbosity = verbosity;
    }
};

/*  ****************************************  */


main(int argc, char *argv[]) {
    /* Derive the stream of hoard commands. */
    FILE *fp = ParseCommandLine(argc, argv);

    if (getwd(cwd) == NULL)
	error(FATAL, "%s", cwd);
    DEBUG(printf("cwd = %s\n", cwd);)

    ruid = (vuid_t)getuid();
    euid = geteuid();
    DEBUG(printf("ruid = %d, euid = %d\n", ruid, euid);)

    /* Parse stream into lists of each type of hoard commands. */
    olist Clear;
    olist Add;
    olist Delete;
    olist List;
    olist Walk;
    olist Verify;
    olist Enable;
    olist Disable;
    ParseHoardCommands(fp, Clear, Add, Delete, List, Walk, Verify, Enable, Disable);

    /* Execute each list.  N.B. The execution order is significant. */
    DoClears(Clear);
    DoAdds(Add);
    DoDeletes(Delete);
    DoLists(List);
    DoWalks(Walk);
    DoVerifies(Verify);
    DoEnables(Enable);
    DoDisables(Disable);
    exit(0);
}


PRIVATE FILE *ParseCommandLine(int argc, char **argv) {
    if (argc == 1)
	usage();

    FILE *fp = NULL;
    while (argc > 1) {
	argc--;
	argv++;

	if (STREQ(argv[0], "-d")) {
	    Debug = 1;
	    continue;
	}
	if (STREQ(argv[0], "-v")) {
	    Verbose = 1;
	    continue;
	}
	if (STREQ(argv[0], "-f")) {
	    fp = fopen(argv[1], "r");
	    if (fp == NULL)
		error(FATAL, "can't open %s", argv[1]);
	    argc--;
	    argv++;
	    break;
	}
	if (STREQ(argv[0], "-")) {
	    fp = stdin;
	    break;
	}
	if (argv[0][0] == '-') {
	    error(!FATAL, "bad option: %s", argv[0]);
	    usage();
	}
    }
    if (fp == NULL) {
	/* Assign fp to argv[0]. */
	fp = _findiop();
	if (fp == NULL)
	    error(FATAL, "no I/O buffers");

	fp->_flag = _IOREAD | _IOSTRG;
	fp->_file = -1;

	int len = (int) strlen(argv[0]) + 1;
	setbuffer(fp, argv[0], len);
	fp->_cnt = len;
    }

    return(fp);
}


PRIVATE void ParseHoardCommands(FILE *fp, olist& Clear, olist& Add,
				olist& Delete, olist& List, olist& Walk, 
				olist& Verify, olist& Enable, olist &Disable) {
/*
    DEBUG(printf("Entering ParseHoardCommands\n"););
*/

    for (;;) {
next_cmd:
	char line[MAXCMDLEN];
	if (my_fgets(line, MAXCMDLEN, fp) == NULL) return;

	char *cp = line;
	char token[MAXCMDLEN];
	if (GetToken(cp, token, &cp) == NULL) continue;
	switch(token[0]) {
	    case 'c':
		{
		/* Clear command. */
		if (!STREQ(token, "c") && !STREQ(token, "clear")) {
		    parse_error(line);
		    continue;
		}

		/* Parse <clear-uid>. */
		vuid_t cuid;
		if (GetToken(cp, token, &cp) == NULL) {
		    cuid = ALL_UIDS;
		}
		else {
		   if (!my_atoi(token, (int *)&cuid)) {
		       parse_error(line);
		       continue;
		   }
		   if (GetToken(cp, token, &cp) != NULL) {
		       parse_error(line);
		       continue;
		   }
		}

		Clear.append(new clear_entry(cuid));
		break;
		}

	    case 'a':
		{
		/* Add command. */
		if (!STREQ(token, "a") && !STREQ(token, "add")) {
		    parse_error(line);
		    continue;
		}

		/* Parse <volume-number> <hoard-filename>. */
		/* Also record <fullname> for later meta-expansion. */
		VolumeId volno, svollist[MAXSYMLINKS];
		char name[MAXPATHLEN], snamelist[MAXSYMLINKS][MAXPATHLEN];
		char fullname[MAXPATHLEN];
		if (GetToken(cp, token, &cp) == NULL) {
		    parse_error(line);
		    continue;
		}
		if (!canonicalize(token, &volno, name, fullname, svollist, snamelist))
		    continue;

		/* Parse <priority/attributes> string. */
		int priority = H_DFLT_PRI;
		int attributes = H_DFLT_ATTRS;
		if (GetToken(cp, token, &cp) != NULL) {
		    /* Format is: <#:c:c+:d:d+>, where each field is optional and order insensitive. */
		    char *tp = token;
		    while (*tp) {
			/* Get next field out of the token. */
			char field[MAXCMDLEN];
			char *xp = field;
			while (*tp && *tp != ':')
			    *xp++ = *tp++;
			*xp = '\0';
			while (*tp == ':') tp++;
			if (field[0] == '\0') continue;

			switch(field[0]) {
			    case 'c':
			    case 'd':
				if (STREQ(field, "c"))
				    { attributes |= H_CHILDREN; break; }
				if (STREQ(field, "d"))
				    { attributes |= H_DESCENDENTS; break; }
				if (STREQ(field,"c+"))
				    { attributes |= (H_CHILDREN | H_INHERIT); break;} 
				if (STREQ(field, "d+"))
				    { attributes |= (H_DESCENDENTS | H_INHERIT); break; }
				parse_error(line);
				goto next_cmd;

			    default:
				/* Parse as a number. */
				if (!my_atoi(field, (int *)&priority)) {
				    parse_error(line);
				    continue;
				}
				if (priority < H_MIN_PRI || priority > H_MAX_PRI) {
				    error(!FATAL, "priority out of range (%d - %d): %s",
					  H_MIN_PRI, H_MAX_PRI, line);
				    continue;
				}
				break;
			}
		    }

		    if (GetToken(cp, token, &cp) != NULL) {
			parse_error(line);
			continue;
		    }
		}

		if ((attributes & (H_CHILDREN | H_DESCENDENTS)) &&
		    !(attributes & H_INHERIT))
		    MetaExpand(Add, fullname, priority, attributes);
		else
		    Add.append(new add_entry(volno, name, priority, attributes));

		/* add symlink entries */
		for (int i = 0; i < MAXSYMLINKS; i++) 
		    if (svollist[i] != 0) {
			DEBUG(printf("adding symlink entry <%x, %s>\n",	
				     svollist[i], snamelist[i]));
			Add.append(new add_entry(svollist[i], snamelist[i], 
						 priority, attributes));
		    }
		break;
	    }

	    case 'd':
		{
		/* Delete command. */
		if (!STREQ(token, "d") && !STREQ(token, "delete")) {
		    parse_error(line);
		    continue;
		}

		/* Parse <volume-number> <hoard-filename>. */
		VolumeId volno;
		char name[MAXPATHLEN];
		if (GetToken(cp, token, &cp) == NULL) {
		    parse_error(line);
		    continue;
		}
		if (!canonicalize(token, &volno, name, 0, 0, 0))
		    continue;
		if (GetToken(cp, token, &cp) != NULL) {
		    parse_error(line);
		    continue;
		}

		Delete.append(new delete_entry(volno, name));
		break;
		}

	    case 'l':
		{/* hoard l [fname [uid]] */
		char lname[MAXPATHLEN];
		vuid_t luid;

		/* Set defaults */
		strcpy(lname, "/dev/tty");
		luid = ALL_UIDS;

		/* List command. */
		if (!STREQ(token, "l") && !STREQ(token, "list")) {
		    parse_error(line);
		    continue;
		}

		/* Parse <list-filename> <list-uid>. */
		if (GetToken(cp, token, &cp) != NULL) {
		    strcpy(lname, token);
		    if (GetToken(cp, token, &cp) != NULL) {
			if (!my_atoi(token, (int *)&luid)) {
			    parse_error(line);
			    continue;
			}
			if (GetToken(cp, token, &cp) != NULL) {
			    parse_error(line);
			    continue;
			}
		    }
		}

		List.append(new list_entry(lname, luid));
		break;
		}

	    case 'w':
		{
		/* Walk command. */
		if (!STREQ(token, "w") && !STREQ(token, "walk")) {
		    parse_error(line);
		    continue;
		}

		/* No arguments (at present). */
		if (GetToken(cp, token, &cp) != NULL) {
		    parse_error(line);
		    continue;
		}

		Walk.append(new walk_entry());

		/* Do a "hoard verify" to confirm success */
		Verify.append(new verify_entry("/dev/tty", ALL_UIDS, 0));
		break;
		}

	    case 'v':
		{/* hoard v [fname [verbosity [uid]]] */
		int verbosity;
		char vname[MAXPATHLEN];
		vuid_t vuid;

		/* Set defaults */
		verbosity = 0;
		strcpy(vname, "/dev/tty");
		vuid = ALL_UIDS;
		

		/* Verify command. */
		if (!STREQ(token, "v") && !STREQ(token, "verify")) {
		    parse_error(line);
		    continue;
		}

		/* Parse <filename> <verbosity> <uid>. */
		if (GetToken(cp, token, &cp) != NULL) {
		    strcpy(vname, token);
		    if (GetToken(cp, token, &cp) != NULL) {
			if (!my_atoi(token, (int *)&verbosity)) {
			    parse_error(line);
			    continue;
			}
			if (GetToken(cp, token, &cp) != NULL) {
			    if (!my_atoi(token, (int *)&vuid)) {
				parse_error(line);
				continue;
			    }
			    if (GetToken(cp, token, &cp) != NULL) {
				parse_error(line);
				continue;
			    }
			}
		    }
		}

		Verify.append(new verify_entry(vname, vuid, verbosity));
		break;
		}

	    case 'o':
		{
		/* Enable periodic hoard walks */
		if (STREQ(token, "on")) {
		    /* No arguments (at present). */
		    if (GetToken(cp, token, &cp) != NULL) {
			parse_error(line);
			continue;
		    }

		    Enable.append(new enable_entry());
		} else
		if (STREQ(token, "off")) {
		    /* No arguments (at present). */
		    if (GetToken(cp, token, &cp) != NULL) {
			parse_error(line);
			continue;
		    }

		    Disable.append(new disable_entry());
		} else {
		    parse_error(line);
		    continue; /* ??? -DCS */
		}
		    
		break;
	    }


	    default:
		{
		parse_error(line);
		continue;
		}
	}
    }
}


		 


/* I created my own fgets() because the 'cmds' style of input didn't work when I used the CMUCS version. */
/* I simply copied this code from the /lib/libc.a source.  -JJK */
PRIVATE char *my_fgets(char *s, int n, FILE *iop) {
/*
    DEBUG(printf("my_fgets: cnt = %d, ptr = %x, base = %x, bufsiz = %d, flag = %d, file = %d\n",
		  iop->_cnt, iop->_ptr, iop->_base, iop->_bufsiz, iop->_flag, iop->_file);)
*/
    register c;
    register char *cs;

    cs = s;
    while (--n>0 && (c = getc(iop)) != EOF) {
	*cs++ = c;
	if (c=='\n')
	    break;
    }
    if (c == EOF && cs==s)
	return(NULL);
    *cs++ = '\0';
    return(s);
}


/* Copies next token out of first argument and into second.  Token is null-terminated. */
/* Function returns first argument or NULL if no token was found. */
/* Third argument is set to point at first char after this token (or NULL). */
PRIVATE char *GetToken(char *buf, char *token, char **nextp) {
/*
    DEBUG(printf("Entering GetToken\n"););
*/

    *nextp = NULL;
    if (buf == NULL) return(NULL);

    char *bp = buf;
    char *tp = token;
    int intoken = 0;
    char c;
    while ((c = *bp++) && c != '\n') {
	if (isspace(c)) {
	    if (!intoken) continue;
	    break;
	}
	if (!intoken) intoken = 1;
	*tp++ = c;
    }

    if (!intoken) return(NULL);
    *tp = '\0';
    *nextp = bp;
    return(buf);
}


/* Simply ensures that all characters are digits. */
PRIVATE int my_atoi(char *token, int *ip) {
/*
    DEBUG(printf("Entering my_atoi\n"););
*/

    char c;
    int i = 0;
    while (c = token[i++])
	if (!isdigit(c)) return(0);

    *ip = atoi(token);
    return(1);
}


/* Canonicalize a pathname. */
/* Caller may ask for either "volume" or "full" canonicalization, or both. */
/* "Volume" canonicalization splits the result into a <volid, canonical-name-from-volroot> pair. */
/* Returns 1 on success, 0 on failure. */
PRIVATE int canonicalize(char *path, VolumeId *vp, char *vname, char *fullname,
			 VolumeId *svp, char (*sname)[MAXPATHLEN]) {
/*
    DEBUG(printf("Entering canonicalize (%s)\n", path););
*/
    /*  Strategy:
     *      1. "chdir" to lowest directory component, and get its canonical name
     *      2. validate and append trailing, non-directory component (if any) to canonical name
     *      3. don't forget to "chdir" back to cwd before returning!
     */

    int rc = 0;
    if (vp) {
	*vp = 0;
	vname[0] = '\0';
    }
    if (fullname)
	fullname[0] = '\0';

    int ix = 0;
    if (svp) 
	for (int i = 0; i < MAXSYMLINKS; i++) {
	    svp[i] = 0;
	    sname[i][0] = '\0';
	}

    char tpath[MAXPATHLEN];
    strcpy(tpath, path);
    char *p = tpath;

    /* In case of absolute pathname chdir to "/" and strip leading slashes. */
    if (*p == '/') {
	if (chdir("/") < 0) {
	    error(!FATAL, "canonicalize: can't chdir(/) (%s)", sys_errlist[errno]);
	    goto done;
	}
	while (*p == '/') p++;
    }

    for (;;) {
	/* Get component into next_comp. */
	/* Advance p past this component (and any trailing slashes). */
	char next_comp[MAXPATHLEN];
	char *cp = next_comp;
	char c;
	while ((c = *p) && c != '/') { p++; *cp++ = c;}
	*cp = '\0';
	while (*p == '/') p++;

	/* If next_comp exists, try to "cd" there. */
	if (next_comp[0] != '\0') {

	    /* If next_comp is a symlink, save it */
	    struct stat tbuf;
	    char contents[MAXPATHLEN];
	    contents[0] = '\0';
	    if (lstat(next_comp, &tbuf) == 0 &&
		(tbuf.st_mode & S_IFMT) == S_IFLNK) {
		/* Make sure we can read the link contents */
		int cc = readlink(next_comp, contents, MAXPATHLEN);
		if (cc <= 0)
		    error(FATAL, "canonicalize: readlink(%s) failed (%s)", sys_errlist[errno]);
		contents[cc] = '\0';

		/* save symlink name if possible. */
		if (svp && (vol_getwd(&svp[ix], NULL, sname[ix]) != NULL)) {
		    /* tack on current component */
		    strcat(sname[ix], "/");
		    strcat(sname[ix], next_comp);
		    ix++;
		}
	    }

	    if (chdir(next_comp) == 0) continue;

	    if (errno != ENOTDIR && errno != ENOENT) {
		error(!FATAL, "canonicalize: chdir(%s) failed (%s)",
		      next_comp, sys_errlist[errno]);
		goto done;
	    }

	    /* translate symlink contents */
	    if (contents[0] != '\0') {
		/* Tack on trailing component(s). */
		if (*p != '\0') {
		    strcat(contents, "/");
		    strcat(contents, p);
		}

		/* Reset buffer and pointer. */
		strcpy(tpath, contents);
		p = tpath;

		/* In case of absolute pathname chdir to "/" and strip leading slashes. */
		if (*p == '/') {
		    if (chdir("/") < 0) {
			error(!FATAL, "canonicalize: can't chdir(/) (%s)", sys_errlist[errno]);
			goto done;
		    }
		    while (*p == '/') p++;
		}

		continue;
	    }
	}

	/* We're at lowest existing component.  Get its canonical name. */
	if (vp) {
	    if (vol_getwd(vp, NULL, vname) == NULL) {
		error(!FATAL, "canonicalize: %s", vname);
		goto done;
	    }
	}
	if (fullname) {
	    if (getwd(fullname) == NULL) {
		error(!FATAL, "canonicalize: %s", fullname);
		goto done;
	    }
	}

	/* Tack on the trailing component(s). */
	if (next_comp[0] != '\0') {
	    if (vp) {
		strcat(vname, "/");
		strcat(vname, next_comp);
		if (*p != '\0') {
		    strcat(vname, "/");
		    strcat(vname, p);
		}
	    }
	    if (fullname) {
		strcat(fullname, "/");
		strcat(fullname, next_comp);
		if (*p != '\0') {
		    strcat(fullname, "/");
		    strcat(fullname, p);
		}
	    }
	}

	break;
    }

    /* Canonicalization has succeeded. */
    rc = 1;

done:
    if (chdir(cwd) < 0)
	error(FATAL, "canonicalize: chdir(%s) failed (%s)", cwd, sys_errlist[errno]);
    DEBUG(printf("canonicalize: %s -> %d, <%x, %s>, %s\n",
		  path, rc, vp ? *vp : 0, vname ? vname : "", fullname ? fullname : "");)
    return(rc);
}


/* Like getwd, except that the returned path is in two parts: */
/*     head:  path from root to volume_root */
/*     tail:  path from volume_root to working directory */
/* The volume number is also returned. */
/* The caller may pass in NULL for vp, head or both. */
PRIVATE char *vol_getwd(VolumeId *vp, char *head, char *tail) {
    if (vp) *vp = 0;
    tail[0] = '\0';
    if (head) head[0] = '\0';

    char fullname[MAXPATHLEN];
    if (getwd(fullname) == NULL) {
	strcpy(tail, fullname);
	return(NULL);
    }

    VolumeId vid = GetVid(".");
    if (vid == 0){
	sprintf(tail, "vol_getwd: can't get volid for %s (%s)",
		fullname, sys_errlist[errno]);
	return(NULL);
    }

    /* Check ancestor components for the first in a different volume. */
    /* "tname" holds the (relative) path to the component in question. */
    /* p1 and p2 are cursors pointing respectively at the separators  */
    /* following the component in question and its successor. */
    char tname[MAXPATHLEN];
    strcpy(tname, ".");
    char *p1 = fullname + strlen(fullname);
    char *p2;
    for (;;) {
	/* "tname/.." is the component in question in this iteration. */
	strcat(tname, "/..");

	/* Move the cursors. */
	p2 = p1;
	while (p1 >= fullname && *--p1 != '/')
	    ;

	VolumeId tvid = GetVid(tname);
	if (tvid != vid) {
	    if (tvid == 0) {
		/* Only some value(s) of errno here should allow us to continue! -JJK */
	    }
	    break;
	}
    }

    /* p2 is now the path to the volume root. */
    if (vp) *vp = vid;
    strcpy(tail, ".");
    if (p2[0] != '\0') {
	if (p2[0] != '/') strcat(tail, "/");
	strcat(tail, p2);
    }
/*
    strcpy(tail, p2);
    if (tail[0] == '\0')
	strcpy(tail, "/");
*/
    if (head) {
	int len = p2 - fullname;
	strncpy(head, fullname, len);
	head[len] = '\0';
    }

    DEBUG(printf("vol_getwd: %s -> %x, %s, %s\n",
		  fullname, vp ? *vp : 0, head ? head : "", tail);)
    return(tail);
}


PRIVATE VolumeId GetVid(char *name) {
/**/
    DEBUG(printf("Entering GetVid (%s)\n", name););
/**/

    VolumeId vid = 0;

/*
    struct getvolstat_msg {
	VolumeStatus volstat;
	char strings[544];
    } gvs_msg;

    struct ViceIoctl vi;
    vi.in = 0;
    vi.in_size = 0;
    vi.out = (char *)&gvs_msg;
    vi.out_size = sizeof(struct getvolstat_msg);
    if (pioctl(name, VIOCGETVOLSTAT, &vi, 1) == 0)
	vid = gvs_msg.volstat.Vid;
*/

    struct getfid_msg {
	ViceFid fid;
	ViceVersionVector vv;
    } gf_msg;

    struct ViceIoctl vi;
    vi.in = 0;
    vi.in_size = 0;
    vi.out = (char *)&gf_msg;
    vi.out_size = sizeof(struct getfid_msg);
    if (pioctl(name, VIOC_GETFID, &vi, 1) == 0)
	vid = gf_msg.fid.Volume;
    DEBUG(printf("GetVid: %s -> %x\n", name, vid);)
    return(vid);
}


PRIVATE void DoClears(olist& Clear) {
    olist_iterator next(Clear);
    clear_entry *c;
    while (c = (clear_entry *)next()) {
	struct ViceIoctl vi;
	vi.in = (char *)&c->msg;
	vi.in_size = sizeof(hdb_clear_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(CODA_ROOT, VIOC_HDB_CLEAR, &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Clear(%d, %d): %s",
		  c->msg.cuid, ruid, sys_errlist[errno]);
	}
    }
}


PRIVATE void DoAdds(olist& Add) {
    olist_iterator next(Add);
    add_entry *a;
    while (a = (add_entry *)next()) {
	struct ViceIoctl vi;
	vi.in = (char *)&a->msg;
	vi.in_size = sizeof(hdb_add_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(CODA_ROOT, VIOC_HDB_ADD, &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Add(%x, %s, %d, %d, %d): %s",
		  a->msg.volno, a->msg.name, a->msg.priority, a->msg.attributes, ruid, sys_errlist[errno]);
	}
    }
}


PRIVATE void DoDeletes(olist& Delete) {
    olist_iterator next(Delete);
    delete_entry *d;
    while (d = (delete_entry *)next()) {
	struct ViceIoctl vi;
	vi.in = (char *)&d->msg;
	vi.in_size = sizeof(hdb_delete_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(CODA_ROOT, VIOC_HDB_DELETE, &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Delete(%x, %s, %d): %s",
		  d->msg.volno, d->msg.name, ruid, sys_errlist[errno]);
	}
    }
}


PRIVATE void DoLists(olist& List) {
    olist_iterator next(List);
    list_entry *l;
    while (l = (list_entry *)next()) {
	struct ViceIoctl vi;
	vi.in = (char *)&l->msg;
	vi.in_size = sizeof(hdb_list_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(CODA_ROOT, VIOC_HDB_LIST, &vi, 0) == 0) {
	    RenameOutFile(l->msg.outfile, l->tname);
	}
	else {
	    error(!FATAL, "pioctl:List(%d, %s, %d): %s",
		  l->msg.luid, l->msg.outfile, ruid, sys_errlist[errno]);
	    unlink(l->msg.outfile);
	}
    }
}


PRIVATE void DoWalks(olist& Walk) {
    olist_iterator next(Walk);
    walk_entry *w;
    while (w = (walk_entry *)next()) {
	struct ViceIoctl vi;
	vi.in = (char *)&w->msg;
	vi.in_size = sizeof(hdb_walk_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(CODA_ROOT, VIOC_HDB_WALK, &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Walk(%d): %s",
		  ruid, sys_errlist[errno]);
	}

	/* Only do one walk (since they are all the same at present). */
	break;
    }
}


PRIVATE void DoVerifies(olist& Verify) {
    olist_iterator next(Verify);
    verify_entry *v;
    while (v = (verify_entry *)next()) {
	struct ViceIoctl vi;
	vi.in = (char *)&v->msg;
	vi.in_size = sizeof(hdb_verify_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(CODA_ROOT, VIOC_HDB_VERIFY, &vi, 0) == 0) {
	    RenameOutFile(v->msg.outfile, v->tname);
	}
	else {
	    error(!FATAL, "pioctl:Verify(%d, %s, %d, %d): %s",
		  v->msg.luid, v->msg.outfile, ruid, v->msg.verbosity, sys_errlist[errno]);
	    unlink(v->msg.outfile);
	}
    }
}


PRIVATE void DoEnables(olist& Enable) {
    olist_iterator next(Enable);
    enable_entry *w;
    w = (enable_entry *)next();
    if (w) {
	struct ViceIoctl vi;
	vi.in = (char *)&w->msg;
	vi.in_size = sizeof(hdb_walk_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(CODA_ROOT, VIOC_HDB_ENABLE, &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Enable(%d): %s",
		  ruid, sys_errlist[errno]);
	}
    }
}


PRIVATE void DoDisables(olist& Disable) {
    olist_iterator next(Disable);
    disable_entry *w;
    w = (disable_entry *)next();
    if (w) {
	struct ViceIoctl vi;
	vi.in = (char *)&w->msg;
	vi.in_size = sizeof(hdb_walk_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(CODA_ROOT, VIOC_HDB_DISABLE, &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Disable(%d): %s",
		  ruid, sys_errlist[errno]);
	}
    }
}


/* Meta-expansion is only applicable to Adds. */
PRIVATE void MetaExpand(olist& Add, char *FullName, int priority, int attributes) {
    if (Verbose)
	printf("Meta expanding %s\n", FullName);

    /* Descendents > Children. */
    if (attributes & H_DESCENDENTS) attributes &= ~H_CHILDREN;

    /* Parse the FullName into <VRPath, vid, NodeName>. */
    if (chdir(FullName) < 0) {
	error(!FATAL, "MetaExpand: chdir(%s) failed (%s)", FullName, sys_errlist[errno]);
	goto done;
    }
    VolumeId vid; vid = 0;
    char VRPath[MAXPATHLEN];
    char NodeName[MAXPATHLEN];
    if (vol_getwd(&vid, VRPath, NodeName) == 0)
	error(FATAL, "MetaExpand: %s", NodeName);

    /* ExpandNode expects to be cd'ed into "FullName/..". */
    if (chdir("..") < 0)
	error(FATAL, "MetaExpand: chdir(%s/..) failed (%s)", FullName, sys_errlist[errno]);
    char mtpt[MAXPATHLEN];
    char *cp;
    if ((cp = rindex(VRPath, '/')) == NULL) cp = CODA_ROOT;
    strcpy(mtpt, cp + 1);
    ExpandNode(mtpt, vid, NodeName, Add, priority, attributes);

done:
    if (chdir(cwd) < 0)
	error(FATAL, "MetaExpand: chdir(%s) failed (%s)", cwd, sys_errlist[errno]);
}


/* N.B. Format of "name" is "./comp1/.../compn".  cwd = "name/..". */
PRIVATE void ExpandNode(char *mtpt, VolumeId vid, char *name,
			 olist& Add, int priority, int attributes) {
    DEBUG(printf("ExpandNode: %s, %x, %s, %d, %d\n",
		  mtpt, vid, name, priority, attributes););

    /* Make an entry for this node. */
    {
	if (Verbose)
	    printf("\t%x, %s\n", vid, name);
	Add.append(new add_entry(vid, name, priority, attributes));
    }

    /* Walk down its children/descendents if necessary. */
    if (attributes & (H_CHILDREN | H_DESCENDENTS)) {
	attributes &= ~H_CHILDREN;

	/* Determine the next component and attempt to cd into it. */
	{
	    char *cp = rindex(name, '/');
	    if (cp) cp++;
	    else cp = mtpt;
	    DEBUG(printf("ExpandNode: chdir(%s)\n", cp););
	    if (chdir(cp) < 0) {
		DEBUG(printf("ExpandNode: chdir(%s) failed (%s)\n", cp, sys_errlist[errno]););
		return;
	    }
	}

	/* Expand the children. */
	{
	    DIR *dirp = opendir(".");
	    if (dirp == NULL) {
		error(!FATAL, "ExpandNode: opendir(\".\") failed");
		return;
	    }

	    struct direct *dp;
	    while((dp = readdir(dirp)) != NULL) {
		DEBUG(printf("ExpandNode: d_name = %s\n", dp->d_name););
		if (STREQ(".", dp->d_name) || STREQ("..", dp->d_name)) continue;
		if (GetVid(dp->d_name) != vid) continue;

		char tname[MAXPATHLEN];
		strcpy(tname, name);
		strcat(tname, "/");
		strcat(tname, dp->d_name);
		ExpandNode(mtpt, vid, tname, Add, priority, attributes);
	    }

	    closedir(dirp);
	    if (chdir("..") < 0) {
		error(!FATAL, "ExpandNode: chdir(\"..\") failed(%s)", sys_errlist[errno]);
		return;
	    }
	}
    }
}


/* Creating the output file must be done with an euid of the real user! */
/* Return 0 on success, -1 on failure with errno set appropriately. */
PRIVATE int CreateOutFile(char *in, char *out) {
    int child = fork();
    if (child == -1)
	error(FATAL, "CreateOutFile: fork failed(%s)", sys_errlist[errno]);

    if (child == 0) {
	/* Attempt to create/truncate the target file. */
	if (setreuid(ruid, ruid) < 0) exit(errno);
	int fd = open(in, (O_TRUNC | O_CREAT), 0666);
	if (fd < 0) exit(errno);
	exit(close(fd) < 0 ? errno : 0);
    }
    else {
	/* Wait for child to finish. */
	union wait status;
	int rc;
	while ((rc = wait(&status)) != child)
	    if (rc < 0) return(-1);
	if (status.w_retcode != 0) {
	    errno = status.w_retcode;
	    return(-1);
	}

	/* Canonicalize the name of the file child just created/truncated. */
	if (!canonicalize(in, (VolumeId *)0, 0, out, 0, 0)) {
	    errno = EINVAL;
	    return(-1);
	}

	/* If the file lives in Coda, convert it to "Fid-form." */
	if (GetVid(out) != 0) {
	    struct GetFid {
		ViceFid fid;
		ViceVersionVector vv;
	    } gf;
	    bzero(&gf, sizeof(struct GetFid));

	    struct ViceIoctl vi;
	    vi.in = 0;
	    vi.in_size = 0;
	    vi.out = (char *)&gf;
	    vi.out_size = sizeof(struct GetFid);

	    if (pioctl(out, VIOC_GETFID, &vi, 0) < 0)
		return(-1);

	    sprintf(out, "@%x.%x.%x", gf.fid.Volume, gf.fid.Vnode, gf.fid.Unique);
	}

	return(0);
    }
}


PRIVATE void RenameOutFile(char *from, char *to) {
    int child = fork();
    if (child == -1)
	error(FATAL, "RenameOutFile: fork failed(%s)", sys_errlist[errno]);

    if (child == 0) {
	/* Open the source file. */
	int infd = open(from, O_RDONLY, 0);
	if (infd < 0) {
	    error(!FATAL, "RenameOutFile: open(%s) failed(%s)", from, sys_errlist[errno]);
	    exit(errno);
	}

	/* Open the target file. */
	if (setreuid(ruid, ruid) < 0) {
	    error(!FATAL, "RenameOutFile: setreuid(%d, %d) failed(%s)", ruid, ruid, sys_errlist[errno]);
	    exit(errno);
	}
	int outfd = open(to, (O_TRUNC | O_CREAT | O_WRONLY), 0666);
	if (outfd < 0) {
	    error(!FATAL, "RenameOutFile: open(%s) failed(%s)", to, sys_errlist[errno]);
	    exit(errno);
	}

	/* Set-up stdin and stdout and invoke "cat". */
	if (dup2(infd, 0) < 0 || dup2(outfd, 1) < 0) {
	    error(!FATAL, "RenameOutFile: dup2() failed(%s)", sys_errlist[errno]);
	    exit(errno);
	}
	char *argv[2]; argv[0] = "cat"; argv[1] = 0;
	if (execvp("cat", argv) < 0) {
	    error(!FATAL, "RenameOutFile: execvp(\"cat\") failed(%s)", sys_errlist[errno]);
	    exit(errno);
	}
    }
    else {
	/* Wait for child to finish. */
	union wait status;
	::wait(&status);

	if (!Debug)
	    unlink(from);
    }    
}


PRIVATE void error(int fatal, char *fmt ...) {
    va_list ap;

    char msg[240];

    /* Copy the message. */
    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    va_end(ap);

    fprintf(stderr, msg);
    fprintf(stderr, "\n");

    if (fatal)
	exit(-1);
}


PRIVATE void usage() {
    error(FATAL, "Usage: hoard [-f source | 'cmds']");
}


PRIVATE void parse_error(char *line) {
    error(!FATAL, "parse error: %s", line);
}
