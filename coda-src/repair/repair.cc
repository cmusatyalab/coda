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

#ifdef REPAIR_STATS
#define DOREPAIRDATADIR "/to/be/defined/by/anyone/needs/to/collect/repair/stats"
#endif REPAIR_STATS

extern int interactive, session;
extern char compDirDefault[MAXPATHLEN];
extern char compOutputFile[MAXPATHLEN]; /* filename for output of last docompare */
extern char beginRepDefault[MAXPATHLEN];
extern char doRepDefault[MAXPATHLEN];
extern int repair_DebugFlag;
extern struct stat compOutputStatBuf;	 /* file information for the repair commands file */
extern struct stat doInputStatBuf;

extern int  IsCreatedEarlier (struct listhdr **, int, long, long);

extern void SetDefaultPaths();
extern int  compareVV(int, char **, struct repvol *);
extern void GetArgs(int argc, char *argv[]);
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
extern int  GetReplicaNames(char **, int , char *);
extern int  GetTokens();
extern void help(int argc, char **argv);
extern void INT(int, int, struct sigcontext *);

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
  {"beginrepair", rep_BeginRepair, 0, ""},  	/* <reppathname> */
  {"endrepair", rep_EndRepair, 0, ""},  	/* no args */
  {"dorepair", doRepair, 0, ""},     		/* <reppathname> <fixfilename> */
  {"comparedirs", compareDirs, 0, ""},	 	/* <reppathname>, <fixfile> */
  {"clearinc", clearInc, 0, ""},	 	/* <reppathname> */
  {"removeinc", removeInc, 0, ""}, 	 	/* <reppathname> */
  {"quit", rep_Exit, 0, ""},         		/* no args */
  {"checklocal", checkLocal, 0, ""},	 	/* no args */
  {"listlocal", listLocal, 0, ""},	 	/* no args */
  {"preservelocal", preserveLocal, 0, ""},	/* no args */
  {"preservealllocal", preserveAllLocal, 0, ""},/* no args */
  {"discardlocal", discardLocal, 0, ""},	/* no args */
  {"discardalllocal", discardAllLocal, 0, ""},	/* no args */
  {"setglobalview", setGlobalView, 0, ""},	/* no args */
  {"setmixedview", setMixedView, 0, ""},	/* no args */
  {"setlocalview", setLocalView, 0, ""},	/* no args */
  {"help", help, 0, ""},
  { 0, 0, 0, ""},
};

void help(int largc, char **largv) {
  printf("See the Coda manual or repair.1 for help\n");
} 

main(int argc, char **argv) {
  int i;
  struct repvol *repv;

  /* parse args */
  SetDefaultPaths();

  signal(SIGINT, (void (*)(int))INT); /* catch SIGINT */

  /* check if help is available and accessible */
  /*    if (access(HELPDIR, R_OK|X_OK) < 0)	{
   *	  printf("The help directory \"%s\" is not accessible\n", HELPDIR);
   *    } 
   */

  for (i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-remove") == 0) {
      interactive = 0;
      if ((i != 2) || (argc < 3)) {
	fprintf(stderr, "Usage:  %s [-d] [<pathname> -remove]\n", argv[0]);
	exit(1);
      }
      else {
	BeginRepair(argv[1], &repv);
	switch (session) {
	  case LOCAL_GLOBAL:
	    discardAllLocal(1, &argv[0]);
	    break;
	  case SERVER_SERVER:
	    removeInc(2, &argv[0]);
	    break;
 	  case NOT_IN_SESSION:
	  default:
	    fprintf(stderr, "Error beginning repair\n");
	    exit(1);
	    break;
	}
	rep_Exit(0, NULL);	  
	return 0;
      }
    }
  }

  /* Sit in command loop */
  if ( argc >= 3 ) {
    rep_BeginRepair(2, &argv[0]);
    compareDirs(argc - 1 , &argv[1]);
    doRepair(3, &argv[0]);
    rep_Exit(0, NULL);
  } 
  else if ( argc != 3 ) {
    printf(INITHELPMSG);
    if (GetTokens()) /* warn user if no tokens are found */
      printf("\n\n\nWARNING: YOU DON'T HAVE TOKENS.  YOU MIGHT WANT TO AUTHENTICATE FIRST\n\n");
    GetArgs(argc, argv);
    Parser_init("repair > ", list);
    Parser_commands();
  } 
  else {
    printf("Usage: repair { object fixfile } \n");
    exit(1);
  }
}
