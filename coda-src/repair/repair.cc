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

#include "repair.h"

#define HELPDIR "/coda/project/coda/doc/cihelp/repair"
#define ISDIR(vnode) ((vnode) & 1)  /* directory vnodesare odd */

extern int  IsCreatedEarlier (struct listhdr **, int, long, long);
extern void SetDefaultPaths();
extern int  compareVV(int, char **, struct repvol *);
extern int  getcompareargs(int, char **, char *, char *, char **, char **, char **, char **);
extern int  getremoveargs(int, char **, char *);
extern void getremovelists(int, resreplica *, struct listhdr **);
extern int  getrepairargs(int, char **, char *, char *, char *);
extern int  makedff(char *extfile, char *intfile);
extern int  doCompare(int, struct repvol *, char **, char *, char *, ViceFid *, char *, char *, char *, char *);
extern int  compareStatus(int, resreplica *);
extern int  compareQuotas(int , char **);
extern int  compareOwner(int, resreplica *);
extern void printAcl(struct Acl *);
extern int  compareAcl(int, resreplica *);
extern int  GetTokens();

#ifdef REPAIR_STATS
#define DOREPAIRDATADIR "/to/be/defined/by/anyone/needs/to/collect/repair/stats"
#endif REPAIR_STATS

int interactive = 1, allowclear = 0, session = NOT_IN_SESSION, repair_DebugFlag = 0;
struct repvol *RepairVol = NULL;
extern char compDirDefault[MAXPATHLEN];
extern char compOutputFile[MAXPATHLEN]; /* filename for output of last docompare */
extern char beginRepDefault[MAXPATHLEN];
extern char doRepDefault[MAXPATHLEN];
extern struct stat compOutputStatBuf;	/* file information for the repair commands file */
extern struct stat doInputStatBuf;

#define INITHELPMSG 	\
"This repair tool can be used to manually repair server/server \n\
or local/global conflicts on files and directories. \n\
You will first need to do a \"beginrepair\" to start a repair\n\
session where messages about the nature of the conflict and\n\
the commands that should be used to repair the conflict will\n\
be displayed. Help message on individual commands can also be\n\
obtained by using the \"help\" facility. Finally, you can use the\n\
\"endrepair\" or \"quit\" to terminate the current repair session.\n"

/* Relax, command parser allows abbreviations of command names */
command_t list[] = {
    {"beginrepair",      rep_BeginRepair,      0, ""}, /* <reppathname> */
    {"endrepair",        rep_EndRepair,        0, ""}, /* no args */
    {"dorepair",         rep_DoRepair,         0, ""}, /* <reppathname> <fixfilename> */
    {"comparedirs",      rep_CompareDirs,      0, ""}, /* <reppathname> <fixfile> */
    {"clearinc",         rep_ClearInc,         0, ""}, /* <reppathname> */
    {"removeinc",        rep_RemoveInc,        0, ""}, /* <reppathname> */
    {"quit",             rep_Exit,             0, ""}, /* no args */
    {"checklocal",       rep_CheckLocal,       0, ""}, /* no args */
    {"listlocal",        rep_ListLocal,        0, ""}, /* no args */
    {"preservelocal",    rep_PreserveLocal,    0, ""}, /* no args */
    {"preservealllocal", rep_PreserveAllLocal, 0, ""}, /* no args */
    {"discardlocal",     rep_DiscardLocal,     0, ""}, /* no args */
    {"discardalllocal",  rep_DiscardAllLocal,  0, ""}, /* no args */
    {"setglobalview",    rep_SetGlobalView,    0, ""}, /* no args */
    {"setmixedview",     rep_SetMixedView,     0, ""}, /* no args */
    {"setlocalview",     rep_SetLocalView,     0, ""}, /* no args */
    {"help",             rep_Help,             0, ""}, /* no args */
    { NULL, NULL, 0, ""},
};

main(int argc, char **argv) {
    int i, ret;
    struct repvol *repv;
    char msgbuf[DEF_BUF];
    VolumeId vid;

    /* parse args */
    SetDefaultPaths();

    signal(SIGINT, (void (*)(int))INT); /* catch SIGINT */

    /* check if help is available and accessible */
    /*    if (access(HELPDIR, R_OK|X_OK) < 0)
     *	  printf("The help directory \"%s\" is not accessible\n", HELPDIR); */

    if ((argc > 1) &&
	((strcmp(argv[1], "-clear") == 0) 
	 || (strcmp(argv[1], "-remove") == 0))) {
	interactive = 0;
	if (argc != 3) {
	    fprintf(stderr, "Usage:  %s [-clear <pathname> | -remove <pathname>]\n", argv[0]);
	    exit(1);
	}
	else {
	    if ((ret = BeginRepair(argv[2], &repv, msgbuf, sizeof(msgbuf))) < 0) {
		fprintf(stderr, "%s\nError beginning repair\n", msgbuf);
		exit(1);
	    }
	    if (repv->local) { /* local/global conflict */
		if (DiscardAllLocal(repv, msgbuf, sizeof(msgbuf)) < 0)
		    fprintf(stderr, "%s\nError discarding local mutations\n", msgbuf);
	    }
	    else { /* server/server conflict */
		if (RemoveInc(repv, msgbuf, sizeof(msgbuf)) < 0)
		    fprintf(stderr, "%s\nError removing inconsistency\n", msgbuf);
	    }
	    if (EndRepair(repv, 1, msgbuf, sizeof(msgbuf)) < 0)
		fprintf(stderr, "%s\nError ending repair\n", msgbuf);
	    Parser_exit(0, NULL);
	    return(0);
	}
    }

    /* Sit in command loop */
    if ( argc >= 3 ) {
	rep_BeginRepair(2, &argv[0]);
	rep_CompareDirs(argc - 1 , &argv[1]);
	rep_DoRepair(3, &argv[0]);
	rep_Exit(0, NULL);
    }
    else if ( argc != 3 ) {
	printf(INITHELPMSG);
	if (GetTokens()) /* warn user if no tokens are found */
	    fprintf(stderr, "\n\n\nWARNING: YOU DON'T HAVE TOKENS.  YOU MIGHT WANT TO AUTHENTICATE FIRST\n\n\a");
	GetArgs(argc, argv);
	Parser_init("repair > ", list);
	Parser_commands();
    } 
    else {
	fprintf(stderr, "Usage: %s { object fixfile }\n", argv[0]);
	exit(1);
    }
}

void GetArgs(int argc, char *argv[]) {
    int i;

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-d") == 0)
	    repair_DebugFlag = 1;
	else if (strcmp(argv[i], "-allowclear") == 0)
	    allowclear = 1;
	else {
	    printf("Usage: repair [-d]\n");
	    exit(-1);
	}
    }
}

void INT(int, int, struct sigcontext *) {
    /* force an end to the current repair session when ^C is hit */
    printf("abnormal exit of repair tool\n");
    fflush(stdout);
    rep_Exit(0, NULL);
}

void rep_BeginRepair(int largc, char **largv) {
    char userpath[MAXPATHLEN], msgbuf[DEF_BUF];
    VolumeId vid;
    struct repvol *repv;
    int rc;

    switch (session) {
    case SERVER_SERVER:
	printf("Server/server repair session already in progress.\n");
	return;
	break;
    case LOCAL_GLOBAL:
	printf("Local/global repair session already in progress.\n");
	return;
	break;
    }

    if (largc == 1)
	Parser_getstr("Pathname of object in conflict?", beginRepDefault, userpath, MAXPATHLEN);
    else if (largc == 2)
	strncpy(userpath, largv[1], MAXPATHLEN);
    else {
	printf("beginrepair <reppathname>\n");
	return;
    }
    strcpy(beginRepDefault, userpath);
    strcpy(doRepDefault, userpath);

    /* Begin the repair */
    if ((rc = BeginRepair(userpath, &repv, msgbuf, sizeof(msgbuf))) < 0) {
	fprintf(stderr, "%s\nbeginrepair failed.\n", msgbuf);
	return;
    }
    RepairVol = repv;
    session = (repv->local) ? LOCAL_GLOBAL : SERVER_SERVER;
    if (session == LOCAL_GLOBAL) {
	printf("Local-global repair session started.\n");
	printf("Available Commands:\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n",
	       "checklocal", "listlocal", "preservelocal", "preservealllocal", "discardlocal",
	       "discardalllocal", "setglobalview", "setmixedview", "setlocalview");
	printf("A list of local mutations is available in the .cml file in the coda spool directory\n");
    }
    else { /* (session == SERVER_SERVER) */
	printf("Server-server repair session started.\n");
	printf("Available commands:\n\t%s\n\t%s\n\t%s\n", 
	       "comparedirs", "removeinc", "dorepair");
    }
    fflush(stdout);
}

void rep_ClearInc(int largc, char **largv) {
    char msgbuf[DEF_BUF];
    VolumeId vid;
    int rc, i, nreplicas;
    struct repvol *repv;
    char **names, *user, *rights, *owner, *mode;

    user = rights = owner = mode = NULL;

    switch (session) {
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
	break;
    }
  
    if (!allowclear) {
	printf("Clear Inconsistency: This command is obsolete.");
	printf("You don't need to use this anymore\n");
    }
    else if (ClearInc(RepairVol, msgbuf, sizeof(msgbuf)) < 0)
	fprintf(stderr, "Error clearing inconsistency: %s\n", msgbuf);
}

void rep_DiscardAllLocal(int largc, char **largv) {
    char msgbuf[DEF_BUF];

    switch (session) {
    case SERVER_SERVER:
	printf("\"discardalllocal\" can only be used to repair a local/global conflict\n");
	return;
	break;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
	break;
    }
    if (DiscardAllLocal(RepairVol, msgbuf, sizeof(msgbuf)) < 0)
	fprintf(stderr, "%s\ndiscardalllocal failed\n", msgbuf);
}

void rep_EndRepair(int largc, char **largv) {
    int commit = 0;
    struct repvol *repv;
    char msgbuf[DEF_BUF];

    switch (session) {
    case NOT_IN_SESSION:
	printf("There is no repair session to end\n");
	return;
	break;
    case LOCAL_GLOBAL:
	if (!interactive) commit = 1;
	else commit = (Parser_getbool("Commit the local-global repair session?", 1)) ? 1 : 0;
    case SERVER_SERVER: 
	if (EndRepair(RepairVol, commit, msgbuf, sizeof(msgbuf)) < 0) {
	    fprintf(stderr, "%s\nError ending repair session\n", msgbuf);
	    exit(2);
	}
	break;
    default:
	fprintf(stderr, "Unknown session type\n");
	exit(1);
	break;
    }
    session = NOT_IN_SESSION;
    printf("Repair session completed.\n");
    fflush(stdout);
}

void rep_Exit(int largc, char **largv) {
    char msgbuf[DEF_BUF];

    /* terminate the current session, if there is one */
    if (session != NOT_IN_SESSION) {
	if (EndRepair(RepairVol, 1, msgbuf, sizeof(msgbuf)) < 0)
	    fprintf(stderr, "%s\nError ending repair\n", msgbuf);
    }

    Parser_exit(0, NULL); /* exit the repair tool */
}

void rep_Help(int largc, char **largv) {
    printf("See the Coda manual or repair.1 for help\n");
    fflush(stdout);
}

void rep_RemoveInc(int largc, char **largv) {
    int rc, dirconf;
    char msgbuf[DEF_BUF];

    switch (session) {
    case NOT_IN_SESSION:
	printf("please use \"beginrepair\" first to determine the nature of the conflict\n");
	return;
	break;
    case LOCAL_GLOBAL:
	printf("\"removeinc\" can only be used to repair a server/server conflict\n");
	return;
	break;
    case SERVER_SERVER:
	printf("\"removeinc\" will terminate the current repair session\n");
	break;
    }

    dirconf = RepairVol->dirconf; /* remember conflict type (since Endrepair will free it) */

    /* remove the inconsistency */
    if ((rc = RemoveInc(RepairVol, msgbuf, sizeof(msgbuf))) < 0)
	fprintf(stderr, "%s\nError removing inconsistency\n", msgbuf);
    /* end the repair session */
    else if ((rc = EndRepair(RepairVol, 0, msgbuf, sizeof(msgbuf))) < 0)
	fprintf(stderr, "%s\nError ending repair session.\n", msgbuf);
    else session = NOT_IN_SESSION;

    if (!rc) { /* no error - try to remove the object */
	if (((dirconf) ? rmdir(RepairVol->rodir) : unlink(RepairVol->rodir)) < 0)
	    fprintf(stderr, "%s\nCould not remove %s\n", strerror(errno), RepairVol->rodir);
    }
    else exit(2);
}
