/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "coda_wait.h"
#include <ctype.h>
#include <errno.h>
extern FILE *_findiop();
#include <stdarg.h>
#include <inodeops.h>
#include <unistd.h>
#include <stdlib.h>

#include <time.h>
#include <coda.h>

#ifdef __cplusplus
}
#endif

#include <olist.h>
#include <venusioctl.h>
#include <vice.h>
#include <hdb.h>
#include <codaconf.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Manifest Constants. */
#ifndef STREQ
#define	STREQ(a, b) (strcmp(a, b) == 0)
#endif
#define	FATAL	1
#define	MAXCMDLEN   (MAXPATHLEN + 1024)
#define	ALL_UIDS    ((vuid_t)-1)


/* Global Variables. */
char cwd[MAXPATHLEN];
vuid_t ruid;
vuid_t euid;
int Debug = 0;
int Verbose = 0;
const char *mountpoint = NULL;


#define	DEBUG(stmt) { if (Debug) { stmt ; fflush(stdout); } }

/* Private routines. */
static FILE *ParseCommandLine(int, char *[]);
static void ParseHoardCommands(FILE *, olist&, olist&, olist&, olist&, olist&, olist&, olist&, olist&);
static char *my_fgets(char *, int, FILE *);
static char *GetToken(char *, char *, char **);
static int my_atoi(char *, int *);
static int canonicalize(char *path, VolumeId *vid, char *realm, char *name,
			char *fullname, VolumeId *svid,
			char (*srealm)[MAXHOSTNAMELEN+1],
			char (*spath)[MAXPATHLEN]);
static char *vol_getwd(VolumeId *vid, char *realm, char *head, char *tail);
static int  GetVid(VolumeId *, char *, const char *);
static void DoClears(olist&);
static void DoAdds(olist&);
static void DoDeletes(olist&);
static void DoLists(olist&);
static void DoWalks(olist&);
static void DoVerifies(olist&);
static void DoEnables(olist&);
static void DoDisables(olist&);
static void MetaExpand(olist&, char *, int, int);
static void ExpandNode(char *, VolumeId, char *, char *, olist&, int, int);
static void RenameOutFile(char *, char *);
static void error(int, const char * ...);
static void usage();
static void parse_error(char *);

/* *****  Wrapper classes for HDB pioctl messages.  ***** */

class clear_entry : public olink {
  public:
    hdb_clear_msg msg;

    clear_entry(vuid_t cuid) {
	msg.cuid = cuid;
    }
};

class add_entry : public olink {
  public:
    hdb_add_msg msg;

    add_entry(VolumeId vid, char *realm, char *name, int priority, int attributes)
    {
	msg.vid = vid;
	strcpy(msg.realm, realm);
	strcpy(msg.name, name);
	msg.priority = priority;
	msg.attributes = attributes;
    }
};

class delete_entry : public olink {
  public:
    hdb_delete_msg msg;

    delete_entry(VolumeId vid, char *realm, char *name) {
	msg.vid = vid;
	strcpy(msg.realm, realm);
	strcpy(msg.name, name);
    }
};

class listentry : public olink {
  public:
    hdb_list_msg msg;
    char tname[MAXPATHLEN];

    listentry(char *lname, vuid_t luid) {
	tmpnam(msg.outfile);
	strcpy(tname, lname);
	msg.luid = luid;
    }
};

class walk_entry : public olink {
  public:
    hdb_walk_msg msg;

    walk_entry() {
    }
};

/* hdb_walk_msg is the simplest kind. Don't need another type of msg. */
class enable_entry : public olink {
  public:
    hdb_walk_msg msg;

    enable_entry() {
    }
};

class disable_entry : public olink {
  public:
    hdb_walk_msg msg;

    disable_entry() {
    }
};

class verify_entry : public olink {
  public:
    hdb_verify_msg msg;
    char tname[MAXPATHLEN];

    verify_entry(const char *vname, vuid_t vuid, int verbosity) {
	tmpnam(msg.outfile);
	strcpy(tname, vname);
	msg.luid = vuid;
	msg.verbosity = verbosity;
    }
};

/*  ****************************************  */


int main(int argc, char *argv[])
{
    /* Derive the stream of hoard commands. */
    FILE *fp = ParseCommandLine(argc, argv);

    if (getcwd(cwd, MAXPATHLEN) == NULL)
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

    codaconf_init("venus.conf");
    CODACONF_STR(mountpoint, "mountpoint", "/coda");

    /* Execute each list.  N.B. The execution order is significant. */
    DoClears(Clear);
    DoAdds(Add);
    DoDeletes(Delete);
    DoLists(List);
    DoWalks(Walk);
    DoVerifies(Verify);
    DoEnables(Enable);
    DoDisables(Disable);
    exit(EXIT_SUCCESS);
}

static FILE *ParseCommandLine(int argc, char **argv) {
    if (argc == 1)
	usage();

    FILE *fp = NULL;
    while (argc > 1) {
	argc--;
	argv++;

	if (argv[0][0] != '-') break;

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
	int fd[2];
	if (pipe(fd)<0)
	    error(FATAL, "open pipe error") ;
	while(argc) {
	    if (write(fd[1], *argv, strlen(*argv)) < 0)
		error(FATAL, "pipe writing error");
	    if (write(fd[1], " ", 1) < 0)
		error(FATAL, "pipe writing error");
	    argc--;
	    argv++;
	}
	if (write(fd[1], "\n", 1) < 0)
	    error(FATAL, "pipe writing error");
	if (close(fd[1])<0)
	    error(FATAL, "closing pipe error");
	if ((fp = fdopen(fd[0],"r"))==NULL)
	    error(FATAL, "fdopen error");
    }

    return(fp);
}


static void ParseHoardCommands(FILE *fp, olist& Clear, olist& Add,
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
		VolumeId vid, svidlist[CODA_MAXSYMLINK];
		char realm[MAXHOSTNAMELEN+1], srealmlist[CODA_MAXSYMLINK][MAXHOSTNAMELEN+1];
		char name[MAXPATHLEN], snamelist[CODA_MAXSYMLINK][MAXPATHLEN];
		char fullname[MAXPATHLEN];
		if (GetToken(cp, token, &cp) == NULL) {
		    parse_error(line);
		    continue;
		}
#ifdef __CYGWIN32__
		if (GetVid(&vid, realm, token))
		    continue;
		strcpy(name, token);
#else		
		if (!canonicalize(token, &vid, realm, name, fullname, svidlist, srealmlist, snamelist))
		    continue;
#endif
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
		    Add.append(new add_entry(vid, realm, name, priority, attributes));

		/* add symlink entries */
		for (int i = 0; i < CODA_MAXSYMLINK; i++) 
		    if (svidlist[i]) {
			DEBUG(printf("adding symlink entry <%x@%s, %s>\n",	
				     (unsigned int)svidlist[i], srealmlist[i], snamelist[i]));
			Add.append(new add_entry(svidlist[i], srealmlist[i],
						 snamelist[i], priority,
						 H_DFLT_ATTRS /* attributes */)); 
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

		/* Parse <volume-number>@<realm> <hoard-filename>. */
		VolumeId vid;
		char realm[MAXHOSTNAMELEN+1];
		char name[MAXPATHLEN];
		if (GetToken(cp, token, &cp) == NULL) {
		    parse_error(line);
		    continue;
		}
		if (!canonicalize(token, &vid, realm, name, 0, 0, 0, 0))
		    continue;
		if (GetToken(cp, token, &cp) != NULL) {
		    parse_error(line);
		    continue;
		}

		Delete.append(new delete_entry(vid, realm, name));
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

		List.append(new listentry(lname, luid));
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
static char *my_fgets(char *s, int n, FILE *iop) {
/*
    DEBUG(printf("my_fgets: cnt = %d, ptr = %x, base = %x, bufsiz = %d, flag = %d, file = %d\n",
		  iop->_cnt, iop->_ptr, iop->_base, iop->_bufsiz, iop->_flag, iop->_file);)
*/
    int c = 0;
    char *cs;

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
static char *GetToken(char *buf, char *token, char **nextp) {
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
static int my_atoi(char *token, int *ip) {
/*
    DEBUG(printf("Entering my_atoi\n"););
*/

    char c;
    int i = 0;
    while ((c = token[i++]))
	if (!isdigit(c)) return(0);

    *ip = atoi(token);
    return(1);
}


/* Canonicalize a pathname. */
/* Caller may ask for either "volume" or "full" canonicalization, or both. */
/* "Volume" canonicalization splits the result into a <volid, canonical-name-from-volroot> pair. */
/* Returns 1 on success, 0 on failure. */
static int canonicalize(char *path, VolumeId *vp, char *vrealm, char *vname,
			char *fullname, VolumeId *svp,
			char (*svrealm)[MAXHOSTNAMELEN+1],
			char (*sname)[MAXPATHLEN])
{
/*
    DEBUG(printf("Entering canonicalize (%s)\n", path););
*/
    /*  Strategy:
     *      1. "chdir" to lowest directory component, and get its canonical name
     *      2. validate and append trailing, non-directory component (if any) to canonical name
     *      3. don't forget to "chdir" back to cwd before returning!
     */

    int rc = 0;

    *vp = 0;
    vrealm[0] = '\0';
    vname[0] = '\0';

    if (fullname)
	fullname[0] = '\0';

    int ix = 0;
    if (svp) 
	for (int i = 0; i < CODA_MAXSYMLINK; i++) {
	    svp[i] = 0;
	    svrealm[i][0] = '\0';
	    sname[i][0] = '\0';
	}

    char tpath[MAXPATHLEN];
#ifdef __CYGWIN32__
    if (!memcmp(path, mountpoint, 5))
	path = path + 6;
#endif   
    strcpy(tpath, path);    
    char *p = tpath;

    /* In case of absolute pathname chdir to "/" and strip leading slashes. */
    if (*p == '/') {
	if (chdir("/") < 0) {
	    error(!FATAL, "canonicalize: can't chdir(/) (%s)", strerror(errno));
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
		    error(FATAL, "canonicalize: readlink(%s) failed (%s)",
			  strerror(errno));
		contents[cc] = '\0';

		/* save symlink name if possible. */
		if (svp && (vol_getwd(&svp[ix], svrealm[ix], NULL, sname[ix]) != NULL)) {
		    /* tack on current component */
		    strcat(sname[ix], "/");
		    strcat(sname[ix], next_comp);
		    ix++;
		} else {
		  printf("vol_getwd failed: probably link points out of coda\n");
		  fflush(stdout);
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
		      error(!FATAL, "canonicalize: can't chdir(/) (%s)",
			    strerror(errno));
		      goto done;
		    }
		    while (*p == '/') p++;
		  }

		  continue;
		}

	    }

	    if (chdir(next_comp) == 0) continue;

	    if (errno != ENOTDIR && errno != ENOENT) {
		error(!FATAL, "canonicalize: chdir(%s) failed (%s)",
		      next_comp, strerror(errno));
		goto done;
	    }

	}

	/* We're at lowest existing component.  Get its canonical name. */
	if (vol_getwd(vp, vrealm, NULL, vname) == NULL) {
	    error(!FATAL, "canonicalize: %s", vname);
	    goto done;
	}

	if (fullname) {
	    if (getcwd(fullname, MAXPATHLEN) == NULL) {
		error(!FATAL, "canonicalize: %s", fullname);
		goto done;
	    }
	}

	/* Tack on the trailing component(s). */
	if (next_comp[0] != '\0') {
	    strcat(vname, "/");
	    strcat(vname, next_comp);
	    if (*p != '\0') {
		strcat(vname, "/");
		strcat(vname, p);
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
	error(FATAL, "canonicalize: chdir(%s) failed (%s)", cwd, strerror(errno));
    DEBUG(printf("canonicalize: %s -> %d, <%x@%s, %s>, %s\n",
		  path, rc, (unsigned int)*vp, vrealm, vname ? vname : "", fullname ? fullname : "");)
    return(rc);
}


/* Like getwd, except that the returned path is in two parts: */
/*     head:  path from root to volume_root */
/*     tail:  path from volume_root to working directory */
/* The volume number is also returned. */
/* The caller may pass in NULL head or both. */
static char *vol_getwd(VolumeId *vp, char *realm, char *head, char *tail)
{
    tail[0] = '\0';
    if (head) head[0] = '\0';

    char fullname[MAXPATHLEN];
    if (getcwd(fullname, MAXPATHLEN) == NULL) {
	strcpy(tail, fullname);
	return(NULL);
    }

    if (GetVid(vp, realm, ".")) {
	sprintf(tail, "vol_getwd: can't get volid for %s (%s)",
		fullname, strerror(errno));
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

	VolumeId tvid;
	char trealm[MAXHOSTNAMELEN+1];
	int err = GetVid(&tvid, trealm, tname);
	if (err || tvid != *vp || strcmp(trealm, realm) != 0) {
	    if (err) {
		/* Only some value(s) of errno here should allow us to continue! -JJK */
	    }
	    break;
	}
    }

    /* p2 is now the path to the volume root. */
    strcpy(tail, ".");
    if (p2[0] != '\0') {
	if (p2[0] != '/') strcat(tail, "/");
	strcat(tail, p2);
    }

    if (head) {
	int len = p2 - fullname;
	strncpy(head, fullname, len);
	head[len] = '\0';
    }

    DEBUG(printf("vol_getwd: %s -> %x@%s, %s, %s\n",
		  fullname, (unsigned int)*vp, realm, head ? head : "", tail);)
    return(tail);
}


static int GetVid(VolumeId *vid, char *realm, const char *name)
{
/**/
    DEBUG(printf("Entering GetVid (%s)\n", name););
/**/
    int err;
    struct getfid_msg {
	ViceFid fid;
	ViceVersionVector vv;
	char realm[MAXHOSTNAMELEN+1];
    } gf_msg;

    struct ViceIoctl vi;
    vi.in = 0;
    vi.in_size = 0;
    vi.out = (char *)&gf_msg;
    vi.out_size = sizeof(struct getfid_msg);
    err = pioctl(name, _VICEIOCTL(_VIOC_GETFID), &vi, 1);

    *vid = 0;
    if (!err) {
	*vid = gf_msg.fid.Volume;
	strcpy(realm, gf_msg.realm);
    }

    DEBUG(printf("GetVid: %s -> %x@%s\n", name, *vid, realm);)

    return err;
}


static void DoClears(olist& Clear) {
    olist_iterator next(Clear);
    clear_entry *c;
    while ((c = (clear_entry *)next())) {
	struct ViceIoctl vi;
	vi.in = (char *)&c->msg;
	vi.in_size = sizeof(hdb_clear_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(NULL, _VICEIOCTL(_VIOC_HDB_CLEAR), &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Clear(%d, %d): %s",
		  c->msg.cuid, ruid, strerror(errno));
	}
    }
}


static void DoAdds(olist& Add) {
    olist_iterator next(Add);
    add_entry *a;
    while ((a = (add_entry *)next())) {
	struct ViceIoctl vi;
	vi.in = (char *)&a->msg;
	vi.in_size = sizeof(hdb_add_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(NULL, _VICEIOCTL(_VIOC_HDB_ADD), &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Add(%x@%s, %s, %d, %d, %d): %s",
		  a->msg.vid, a->msg.realm, a->msg.name,
		  a->msg.priority, a->msg.attributes, ruid, strerror(errno));
	}
    }
}


static void DoDeletes(olist& Delete) {
    olist_iterator next(Delete);
    delete_entry *d;
    while ((d = (delete_entry *)next())) {
	struct ViceIoctl vi;
	vi.in = (char *)&d->msg;
	vi.in_size = sizeof(hdb_delete_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(NULL, _VICEIOCTL(_VIOC_HDB_DELETE), &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Delete(%x@%s, %s, %d): %s",
		  d->msg.vid, d->msg.realm, d->msg.name, ruid,
		  strerror(errno));
	}
    }
}


static void DoLists(olist& List) {
    olist_iterator next(List);
    listentry *l;
    while ((l = (listentry *)next())) {
	struct ViceIoctl vi;
	vi.in = (char *)&l->msg;
	vi.in_size = sizeof(hdb_list_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(NULL, _VICEIOCTL(_VIOC_HDB_LIST), &vi, 0) == 0) {
	    RenameOutFile(l->msg.outfile, l->tname);
	}
	else {
	    error(!FATAL, "pioctl:List(%d, %s, %d): %s",
		  l->msg.luid, l->msg.outfile, ruid, strerror(errno));
	    unlink(l->msg.outfile);
	}
    }
}


static void DoWalks(olist& Walk) {
    olist_iterator next(Walk);
    walk_entry *w;
    while ((w = (walk_entry *)next())) {
	struct ViceIoctl vi;
	vi.in = (char *)&w->msg;
	vi.in_size = sizeof(hdb_walk_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(NULL, _VICEIOCTL(_VIOC_HDB_WALK), &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Walk(%d): %s",
		  ruid, strerror(errno));
	}

	/* Only do one walk (since they are all the same at present). */
	break;
    }
}


static void DoVerifies(olist& Verify) {
    olist_iterator next(Verify);
    verify_entry *v;
    while ((v = (verify_entry *)next())) {
	struct ViceIoctl vi;
	vi.in = (char *)&v->msg;
	vi.in_size = sizeof(hdb_verify_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(NULL, _VICEIOCTL(_VIOC_HDB_VERIFY), &vi, 0) == 0) {
	    RenameOutFile(v->msg.outfile, v->tname);
	}
	else {
	    error(!FATAL, "pioctl:Verify(%d, %s, %d, %d): %s",
		  v->msg.luid, v->msg.outfile, ruid, v->msg.verbosity, strerror(errno));
	    unlink(v->msg.outfile);
	}
    }
}


static void DoEnables(olist& Enable) {
    olist_iterator next(Enable);
    enable_entry *w;
    w = (enable_entry *)next();
    if (w) {
	struct ViceIoctl vi;
	vi.in = (char *)&w->msg;
	vi.in_size = sizeof(hdb_walk_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(NULL, _VICEIOCTL(_VIOC_HDB_ENABLE), &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Enable(%d): %s",
		  ruid, strerror(errno));
	}
    }
}


static void DoDisables(olist& Disable) {
    olist_iterator next(Disable);
    disable_entry *w;
    w = (disable_entry *)next();
    if (w) {
	struct ViceIoctl vi;
	vi.in = (char *)&w->msg;
	vi.in_size = sizeof(hdb_walk_msg);
	vi.out = 0;
	vi.out_size = 0;

	if (pioctl(NULL, _VICEIOCTL(_VIOC_HDB_DISABLE), &vi, 0) != 0) {
	    error(!FATAL, "pioctl:Disable(%d): %s",
		  ruid, strerror(errno));
	}
    }
}


/* Meta-expansion is only applicable to Adds. */
static void MetaExpand(olist& Add, char *FullName, int priority, int attributes) {
    if (Verbose)
	printf("Meta expanding %s\n", FullName);

    /* Descendents > Children. */
    if (attributes & H_DESCENDENTS) attributes &= ~H_CHILDREN;

    /* Parse the FullName into <VRPath, vid, NodeName>. */
    if (chdir(FullName) < 0) {
	error(!FATAL, "MetaExpand: chdir(%s) failed (%s)", FullName, strerror(errno));
	goto done;
    }
    VolumeId vid;
    char realm[MAXHOSTNAMELEN+1];
    char VRPath[MAXPATHLEN];
    char NodeName[MAXPATHLEN];
    if (vol_getwd(&vid, realm, VRPath, NodeName) == 0)
	error(FATAL, "MetaExpand: %s", NodeName);

    /* ExpandNode expects to be cd'ed into "FullName/..". */
    if (chdir("..") < 0)
	error(FATAL, "MetaExpand: chdir(%s/..) failed (%s)", FullName,
	      strerror(errno));
    char mtpt[MAXPATHLEN];
    const char *cp;
    if ((cp = strrchr(VRPath, '/')) == NULL) cp = mountpoint;
    strcpy(mtpt, cp + 1);
    ExpandNode(mtpt, vid, realm, NodeName, Add, priority, attributes);

done:
    if (chdir(cwd) < 0)
	error(FATAL, "MetaExpand: chdir(%s) failed (%s)", cwd, strerror(errno));
}


/* N.B. Format of "name" is "./comp1/.../compn".  cwd = "name/..". */
static void ExpandNode(char *mtpt, VolumeId vid, char *realm, char *name,
			 olist& Add, int priority, int attributes)
{
    DEBUG(printf("ExpandNode: %s, %x@%s, %s, %d, %d\n",
		  mtpt, (unsigned int)vid, realm, name, priority, attributes););

    /* Make an entry for this node. */
    {
	if (Verbose)
	    printf("\t%x@%s, %s\n", (unsigned int)vid, realm, name);
	Add.append(new add_entry(vid, realm, name, priority, attributes));
    }

    /* Walk down its children/descendents if necessary. */
    if (attributes & (H_CHILDREN | H_DESCENDENTS)) {
	attributes &= ~H_CHILDREN;

	/* Determine the next component and attempt to cd into it. */
	{
	    char *cp = strrchr(name, '/');
	    if (cp) cp++;
	    else cp = mtpt;
	    DEBUG(printf("ExpandNode: chdir(%s)\n", cp););
	    if (chdir(cp) < 0) {
		DEBUG(printf("ExpandNode: chdir(%s) failed (%s)\n", cp,
			     strerror(errno)););
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

	    struct dirent *dp;
	    while((dp = readdir(dirp)) != NULL) {
		DEBUG(printf("ExpandNode: d_name = %s\n", dp->d_name););
		if (STREQ(".", dp->d_name) || STREQ("..", dp->d_name)) continue;

		VolumeId tmp;
		char tmprealm[MAXHOSTNAMELEN+1];
		int err = GetVid(&tmp, tmprealm, dp->d_name);
		if (err || tmp != vid || strcmp(tmprealm, realm) != 0)
		    continue;

		char tname[MAXPATHLEN];
		strcpy(tname, name);
		strcat(tname, "/");
		strcat(tname, dp->d_name);
		ExpandNode(mtpt, vid, realm, name, Add, priority, attributes);
	    }

	    closedir(dirp);
	    if (chdir("..") < 0) {
		error(!FATAL, "ExpandNode: chdir(\"..\") failed(%s)",
		      strerror(errno));
		return;
	    }
	}
    }
}


static void RenameOutFile(char *from, char *to) {
    int child = fork();
    if (child == -1)
	error(FATAL, "RenameOutFile: fork failed(%s)", strerror(errno));

    if (child == 0) {
	/* Open the source file. */
	int infd = open(from, O_RDONLY | O_BINARY, 0);
	if (infd < 0) {
	    error(!FATAL, "RenameOutFile: open(%s) failed(%s)", from,
		  strerror(errno));
	    exit(EXIT_FAILURE);
	}

	/* Open the target file. */
#ifndef __CYGWIN32__
	if (setreuid(ruid, ruid) < 0) {
	    error(!FATAL, "RenameOutFile: setreuid(%d, %d) failed(%s)", ruid,
		  ruid, strerror(errno));
	    exit(EXIT_FAILURE);
	}
#endif
	int outfd = open(to, (O_TRUNC | O_CREAT | O_WRONLY | O_BINARY), 0666);
	if (outfd < 0) {
	    error(!FATAL, "RenameOutFile: open(%s) failed(%s)", to,
		  strerror(errno));
	    exit(EXIT_FAILURE);
	}

	/* Set-up stdin and stdout and invoke "cat". */
	if (dup2(infd, 0) < 0 || dup2(outfd, 1) < 0) {
	    error(!FATAL, "RenameOutFile: dup2() failed(%s)", strerror(errno));
	    exit(EXIT_FAILURE);
	}
	char *argv[2]; argv[0] = (char *)"cat"; argv[1] = NULL;
	if (execvp(argv[0], argv) < 0) {
	    error(!FATAL, "RenameOutFile: execvp(\"cat\") failed(%s)", strerror(errno));
	    exit(EXIT_FAILURE);
	}
    }
    else {
	/* Wait for child to finish. */
	int status;

	::wait(&status);

	if (!Debug)
	    unlink(from);
    }
}


static void error(int fatal, const char *fmt ...) {
    va_list ap;

    char msg[240];

    /* Copy the message. */
    va_start(ap, fmt);
    vsnprintf(msg, 240, fmt, ap);
    va_end(ap);

    fprintf(stderr, "%s\n", msg);

    if (fatal)
	exit(EXIT_FAILURE);
}


static void usage() {
    error(FATAL, "Usage: hoard [-f source | 'cmds']");
}


static void parse_error(char *line) {
    error(!FATAL, "parse error: %s", line);
}
