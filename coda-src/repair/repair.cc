/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include "repair.h"

int allowclear = 0, session = NOT_IN_SESSION, repair_DebugFlag = 0;
struct repvol *RepairVol = NULL;
char cfix[MAXPATHLEN];

/* Relax, command parser allows abbreviations of command names */
command_t list[] = {
    {"beginrepair",      rep_BeginRepair,      0, ""}, /* <reppathname> */
    {"endrepair",        rep_EndRepair,        0, ""}, /* no args */
    {"dorepair",         rep_DoRepair,         0, ""}, /* <reppathname> <fixfilename> */
    {"comparedirs",      rep_CompareDirs,      0, ""}, /* <reppathname> <fixfile> */
    {"clearinc",         rep_ClearInc,         0, ""}, /* <reppathname> */
    {"removeinc",        rep_RemoveInc,        0, ""}, /* <reppathname> */
    {"replaceinc",       rep_ReplaceInc,       0, ""}, /* <reppathname> <mergedfile> */
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

int main(int argc, char **argv)
{
    struct repvol *repv;
    char msgbuf[DEF_BUF];

    memset(cfix, 0, sizeof(cfix));
    signal(SIGINT, (void (*)(int))INT); /* catch SIGINT */

    if ((argc > 1) && (strcmp(argv[1], "-remove") == 0)) {
	if (argc != 3) {
	    fprintf(stderr, "Usage:  %s [-remove <pathname>]\n", argv[0]);
	    exit(1);
	}
	else {
	    if ((BeginRepair(argv[2], &repv, msgbuf, sizeof(msgbuf))) < 0) {
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
	    execlp("rm", "rm", "-Rf", argv[2]);
	    fprintf(stderr, "\nError removing %s: %s\n", argv[2], strerror(errno));
	    exit(1);
	}
    }

    /* Sit in command loop */
    if ( argc >= 3 ) {
	rep_BeginRepair(2, &(argv[0]));
	rep_CompareDirs(argc - 1 , &(argv[1]));
	rep_DoRepair(2, &(argv[1]));
	rep_EndRepair(0, NULL);
	rep_Exit(0, NULL);
    }
    else if ( argc != 3 ) {
	printf(INITHELPMSG);
#if 0
	if (GetTokens()) /* warn user if no tokens are found */
	    fprintf(stderr, "\n\n\nWARNING: YOU DON'T HAVE TOKENS.  YOU MIGHT WANT TO AUTHENTICATE FIRST\n\n\a");
#endif
	GetArgs(argc, argv);
	Parser_init("repair > ", list);
	Parser_commands();
    } 
    else {
	fprintf(stderr, "Usage: %s { object fixfile }\n", argv[0]);
	exit(1);
    }
}

int checkIfLocal(char *arg) {
    switch (session) {
    case SERVER_SERVER:
	printf("\"%s\" can only be used to repair a local/global conflict\n", arg);
	return(1);
	break;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return(1);
	break;
    }
    return(0);
}

int checkserver(char *arg) {
    switch (session) {
    case LOCAL_GLOBAL:
	printf("\"%s\" can only be used to repair a server/server conflict\n", arg);
	return(1);
	break;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return(1);
	break;
    }
    return(0);
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

int getcompareargs(int largc, char **largv, char **filepath, struct repinfo *inf)
{
    int j;

    if (largc == 1) goto exit;

    inf->user = inf->rights = inf->owner = inf->mode = NULL;
    for ( j = 2; j < largc ; j++ ) {
	if ( strcmp(largv[j], "-acl") == 0 ) {
	    if ( largc < j+3 ) 
		goto exit;
	    inf->user = largv[j+1];
	    inf->rights = largv[j+2];
	    j = j + 2;
	}
	else if ( strcmp(largv[j], "-owner") == 0 ) {
	    if ( largc < j+2 ) 
		goto exit;
	    inf->owner = largv[j+1];
	    j = j+1;
	}
	else if ( strcmp(largv[j], "-mode") == 0) {
	    if ( largc < j+2 ) 
		goto exit;
	    inf->mode = largv[j+1];
	    j = j+1;
	}
	else goto exit;
    }

    *filepath = largv[1];
    if (!repair_getfid(*filepath, NULL, NULL, NULL, NULL, 0)) {
	fprintf(stderr, "%s is in /coda and cannot be used as the fix file\n", *filepath);
	return(-1); 
    }
    strncpy(cfix, *filepath, sizeof(cfix));

    return(0);

  exit:
    printf("%s  <fixfile> { -acl user rights } { -owner uid} {-mode mode}\n", largv[0]);
    return(-1);
}

int getrepairargs(int largc, char **largv, char *fixpath)
{
    if (largc == 1) Parser_getstr("Pathname of fixfile?", cfix, fixpath, MAXPATHLEN);
    else if (largc == 2) strncpy(fixpath, largv[1], MAXPATHLEN);
    else {
	fprintf(stderr, "%s {object fixfile }\n", largv[0]);
	return(-1);
    }
    if (!repair_getfid(fixpath, NULL, NULL, NULL, NULL, 0)) {
	fprintf(stderr, "%s is in /coda and cannot be used as the fix file\n", fixpath);
	return(-1); 
    }
    strncpy(cfix, fixpath, sizeof(cfix));
    return(0);
}

/* return zero if user has valid tokens */
int GetTokens()
{
    ClearToken clear;
    EncryptedSecretToken secret;
    return (U_GetLocalTokens(&clear, secret, ""));
}

void INT(int, int, struct sigcontext *) {
    /* force an end to the current repair session when ^C is hit */
    fprintf(stderr, "abnormal exit of repair tool\n");
    rep_Exit(0, NULL);
}

void rep_BeginRepair(int largc, char **largv) {
    char userpath[MAXPATHLEN], msgbuf[DEF_BUF];
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
	Parser_getstr("Pathname of object in conflict?", "", userpath, MAXPATHLEN);
    else if (largc == 2)
	strncpy(userpath, largv[1], MAXPATHLEN);
    else {
	printf("beginrepair <reppathname>\n");
	return;
    }

    /* Begin the repair */
    if ((rc = BeginRepair(userpath, &repv, msgbuf, sizeof(msgbuf))) < 0) {
	fprintf(stderr, "%s\nbeginrepair failed.\n", msgbuf);
	return;
    }
    RepairVol = repv;
    session = (repv->local) ? LOCAL_GLOBAL : SERVER_SERVER;
    if (session == LOCAL_GLOBAL) {
	printf("Local-global %s repair session started.\n", (repv->dirconf ? "directory" : "file"));
	printf("Available Commands:\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n",
	       "checklocal", "listlocal", "preservelocal", "preservealllocal", "discardlocal",
	       "discardalllocal", "setglobalview", "setmixedview", "setlocalview");
	printf("A list of local mutations is available in the .cml file in the coda spool directory\n");
    }
    else { /* (session == SERVER_SERVER) */
	printf("Server-server %s repair session started.\n", (repv->dirconf ? "directory" : "file"));
	if (repv->dirconf) /* directory conflict */
	    printf("Available commands:\n\t%s\n\t%s\n\t%s\n", "comparedirs", "removeinc", "dorepair");
	else /* file conflict */
	    printf("Available commands:\n\t%s\n\t%s\n\n", "replaceinc", "removeinc");
    }
    fflush(stdout);
}

void rep_CheckLocal(int largc, char **largv) {
    struct ViceIoctl vioc;
    int rc;
    char space[DEF_BUF];
    char buf[DEF_BUF];

    if (checkIfLocal("checklocal")) return;

    sprintf(buf, "%d", REP_CMD_CHECK);
    vioc.in = buf;
    vioc.in_size = strlen(vioc.in) + 1;
    vioc.out = space;
    vioc.out_size = DEF_BUF;

    rc = pioctl("/coda", _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0);
    if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_CHECK)");
    printf("%s\n", vioc.out);
    fflush(stdout);
}

void rep_ClearInc(int largc, char **largv) {
    char msgbuf[DEF_BUF];

    if (session == NOT_IN_SESSION) {
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    if (!allowclear) {
	printf("Clear Inconsistency: This command is obsolete.");
	printf("You don't need to use this anymore\n");
    }
    else if (ClearInc(RepairVol, msgbuf, sizeof(msgbuf)) < 0)
	fprintf(stderr, "Error clearing inconsistency: %s\n", msgbuf);
}

void rep_CompareDirs(int largc, char **largv) {
    char msgbuf[DEF_BUF];
    struct repinfo inf;
    char *fixfile = NULL;
    int ret;

    memset(&inf, 0, sizeof(inf));
    inf.interactive = 1;

    if (session == NOT_IN_SESSION) {
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    /* Obtain parameters from user */
    if (getcompareargs(largc, largv, &fixfile, &inf) < 0)
	return;

    if (session == LOCAL_GLOBAL) {
	if ((ret = CompareDirs(RepairVol, fixfile, &inf, msgbuf, sizeof(msgbuf))) == -2) {
	    if (DoRepair(RepairVol, fixfile, stdout, msgbuf, sizeof(msgbuf)) >= 0) {
		rep_PreserveAllLocal(0, NULL);
		return;
	    }
	}
    }
    else { /* session == SERVER_SERVER */
	while ((ret = CompareDirs(RepairVol, fixfile, &inf, msgbuf, sizeof(msgbuf))) == -2) {
	    if (DoRepair(RepairVol, fixfile, stdout, msgbuf, sizeof(msgbuf)) < 0)
		break;
	}
    }

    if (ret < 0)
	fprintf(stderr, "%s\n%s failed\n", msgbuf, ((ret == -2) ? "dorepair" : "comparedirs"));
}

void rep_DiscardAllLocal(int largc, char **largv) {
    char msgbuf[DEF_BUF];

    if (checkIfLocal("discardalllocal")) return;

    if (DiscardAllLocal(RepairVol, msgbuf, sizeof(msgbuf)) < 0)
	fprintf(stderr, "%s\ndiscardalllocal failed\n", msgbuf);
}

void rep_DiscardLocal(int largc, char **largv) {
    struct ViceIoctl vioc;
    int rc;
    char space[DEF_BUF];
    char buf[BUFSIZ];
    
    if (checkIfLocal("discardlocal")) return;

    vioc.out = space;
    vioc.out_size = DEF_BUF;
    sprintf(buf, "%d", REP_CMD_DISCARD);
    vioc.in = buf;    
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0);
    if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_DISCARD)");
    printf("%s\n", vioc.out);
    fflush(stdout);
}

void rep_DoRepair(int largc, char **largv) {
    char msgbuf[DEF_BUF], ufixpath[MAXPATHLEN];

    if (session == NOT_IN_SESSION) {
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    /* Obtain parameters and confirmation from user */
    if (getrepairargs(largc, largv, ufixpath) < 0) return;

    if (DoRepair(RepairVol, ufixpath, stdout, msgbuf, sizeof(msgbuf)) < 0)
      fprintf(stderr, "%s\nRepair failed.\n", msgbuf);
}

void rep_EndRepair(int largc, char **largv) {
    int commit = 0;
    char msgbuf[DEF_BUF];

    switch (session) {
    case NOT_IN_SESSION:
	printf("There is no repair session to end\n");
	return;
	break;
    case LOCAL_GLOBAL:
	commit = (Parser_getbool("Commit the local-global repair session?", 1)) ? 1 : 0;
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

void rep_Exit(int largc, char **largv)
{
    /* terminate the current session, if there is one */
    if (session != NOT_IN_SESSION)
	rep_EndRepair(0, NULL);

    Parser_exit(0, NULL); /* exit the repair tool */
}

void rep_Help(int largc, char **largv) {
    printf("See the Coda manual or repair.1 for help.\nPerhaps try doing a \"beginrepair\".\n");
    fflush(stdout);
}

void rep_ListLocal(int largc, char **largv)
{
    int fd;
    struct ViceIoctl vioc;
    int rc, n;
    char space[DEF_BUF];
    char buf[DEF_BUF];
    char filename[MAXPATHLEN];

    if (checkIfLocal("listlocal")) return;

    vioc.out = space;
    vioc.out_size = DEF_BUF;
    strcpy(filename, "/tmp/listlocal.XXXXXX");
    fd = mkstemp(filename);
    if (fd < 0) {
	perror(filename);
	return;
    }

    vioc.in = buf;
    sprintf(buf, "%d %s", REP_CMD_LIST, filename);
    vioc.in_size = (short) strlen(vioc.in) + 1;

    rc = pioctl("/coda", _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0);
    if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_LIST)");
    printf("%s\n", vioc.out);
    fflush(stdout);
    if (rc == 0) {
	while ((n = read(fd, buf, DEF_BUF)) > 0)
	    write(1, buf, n);
    }
    close(fd);
    unlink(filename);
}

void rep_PreserveAllLocal(int largc, char **largv) {
    struct ViceIoctl vioc;
    int rc;
    char space[DEF_BUF];
    char buf[BUFSIZ];

    if (checkIfLocal("preservealllocal")) return;

    /* Release volume-level locks */
    vioc.out = space;
    vioc.out_size = DEF_BUF;
    sprintf(buf, "%d", REP_CMD_PRESERVE_ALL);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0);
    if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_PRESERVE_ALL)");
    printf("%s\n", vioc.out);
    fflush(stdout);
}

void rep_PreserveLocal(int largc, char **largv) {
    struct ViceIoctl vioc;
    int rc;
    char space[DEF_BUF];
    char buf[BUFSIZ];

    if (checkIfLocal("preservelocal")) return;

    vioc.out = space;
    vioc.out_size = DEF_BUF;
    sprintf(buf, "%d", REP_CMD_PRESERVE);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0);
    if (rc < 0) perror("VIOC_REP_CMD(REP_CMU_PRESERVE)");
    printf("%s\n", vioc.out);
    fflush(stdout);
}

void rep_RemoveInc(int largc, char **largv) {
    int rc, dirconf;
    char msgbuf[DEF_BUF];

    if (checkserver("removeinc")) return;
    printf("\"removeinc\" will terminate the current repair session\n");

    dirconf = RepairVol->dirconf; /* remember conflict type (since Endrepair will free it) */

    printf("Completely remove %s?", RepairVol->rodir);
    if (!Parser_getbool("", 1)) {
	printf("Operation aborted.\n");
	return;
    }

    /* remove the inconsistency */
    if ((rc = RemoveInc(RepairVol, msgbuf, sizeof(msgbuf))) < 0)
	fprintf(stderr, "%s\nError removing inconsistency\n", msgbuf);
    /* end the repair session */
    else if ((rc = EndRepair(RepairVol, 0, msgbuf, sizeof(msgbuf))) < 0)
	fprintf(stderr, "%s\nError ending repair session.\n", msgbuf);

    if (!rc) { /* no error - try to remove the object */
	if (((dirconf) ? rmdir(RepairVol->rodir) : unlink(RepairVol->rodir)) < 0)
	    fprintf(stderr, "%s\nCould not remove %s\n", strerror(errno), RepairVol->rodir);
    }
    else exit(2);
}

void rep_ReplaceInc(int largc, char **largv)
{
    int rc;
    char fixpath[MAXPATHLEN], mergefile[MAXPATHLEN], msgbuf[DEF_BUF];
    ViceFid fixfid;
    char fixrealm[MAXHOSTNAMELEN];
    vv_t fixvv;
    struct stat sbuf;

    if (checkserver("replaceinc")) return;
    if (RepairVol->dirconf) {
	printf("\"replaceinc\" can only be used to repair file conflicts\n");
	return;
    }
    printf("\"replaceinc\" will terminate the current repair session\n");

    if (largc == 1) {
	printf("Pathname of object to replace %s ", RepairVol->rodir);
	Parser_getstr("with?", "", mergefile, MAXPATHLEN);
    }
    else if (largc == 2)
	strncpy(mergefile, largv[1], MAXPATHLEN);
    else {
	printf("replaceinc <mergedfile>\n");
	return;
    }

    if ((rc = stat(mergefile, &sbuf)) != 0) {
	fprintf(stderr, "Couldn't find %s: %s\n", mergefile, strerror(errno));
	return;
    }
    if (!(sbuf.st_mode & S_IFREG)) {
	fprintf(stderr, "File %s is not a regular file (and hence cannot be used for repair)\n", mergefile);
	return;
    }
    if (!repair_getfid(mergefile, &fixfid, fixrealm, &fixvv, msgbuf, sizeof(msgbuf)) && (fixvv.StoreId.Host != (unsigned long)-1))
	sprintf(fixpath, "@%lx.%lx.%lx@%s", fixfid.Volume, fixfid.Vnode, fixfid.Unique, fixrealm);
    else strcpy(fixpath, mergefile);

    printf("Replace %s\n   with %s?", RepairVol->rodir, mergefile);
    if (!Parser_getbool("", 1)) {
	printf("Operation aborted.\n");
	return;
    }

    if ((rc = EndRepair(RepairVol, 0, msgbuf, sizeof(msgbuf))) < 0)
	fprintf(stderr, "%s\nError ending repair session.\n", msgbuf);

    if ((dorep(RepairVol, fixpath, NULL, 0) < 0) && (errno != ETOOMANYREFS)) {
	fprintf(stderr, "Error repairing conflict: %s\n", strerror(errno));
	return;
    }
}

void rep_SetGlobalView(int largc, char **largv)
{
    struct ViceIoctl vioc;
    int rc;
    char space[DEF_BUF];
    char buf[BUFSIZ];

    if (checkIfLocal("setglobalview")) return;
    
    vioc.out = space;
    vioc.out_size = DEF_BUF;
    sprintf(buf, "%d", REP_CMD_GLOBAL_VIEW);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0);
    if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_GLOBAL_VIEW)");
    printf("%s\n", vioc.out);
    fflush(stdout);
}

void rep_SetLocalView(int largc, char **largv) {
    struct ViceIoctl vioc;
    int rc;
    char space[DEF_BUF];
    char buf[BUFSIZ];

    if (checkIfLocal("setlocalview")) return;

    vioc.out = space;
    vioc.out_size = DEF_BUF;
    sprintf(buf, "%d", REP_CMD_LOCAL_VIEW);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0);
    if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_LOCAL_VIEW)");
    printf("%s\n", vioc.out);
    fflush(stdout);
}

void rep_SetMixedView(int largc, char **largv) {
    struct ViceIoctl vioc;
    int rc;
    char space[DEF_BUF];
    char buf[BUFSIZ];

    if (checkIfLocal("setmixedview")) return;

    vioc.out = space;
    vioc.out_size = DEF_BUF;
    sprintf(buf, "%d", REP_CMD_MIXED_VIEW);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0);
    if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_MIXED_VIEW)");
    printf("%s\n", vioc.out);
    fflush(stdout);
}
