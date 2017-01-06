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





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <struct.h>
#include <sys/file.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "vrdb.h"

#include <codaconf.h>
#include <vice_file.h>

static char *serverconf = SYSCONFDIR "/server"; /* ".conf" */
static char *vicedir = NULL;

void PrintVRDB() {
    ohashtab_iterator next(VRDB.namehtb, (void *)-1);

    olink *l;
    vrent *v;
    while (l = next()) {
	v = strbase(vrent, l, namehtblink);
	v->print();
    }
}

void BuildVRDB() {
    char *infile = vice_file("db/VRList");
    FILE *vrlist;
    vrlist = fopen(infile, "r");
    if (vrlist == NULL) {
	printf("MakeVRDB: unable to open file %s\n", infile);
	exit(EXIT_FAILURE);
    }
    int fd = open(vice_sharedfile("db/VRDB"), O_CREAT | O_TRUNC | O_WRONLY,
		  0644);
    vrent vre;
    char line[500];
    int lineno = 0;
    while (fgets(line, sizeof(line), vrlist) != NULL) {
	lineno++;
	int servercount;
	if (sscanf(line, "%32s %x %d %x %x %x %x %x %x %x %x %x",
		   vre.key, &vre.volnum, &servercount,
		   &vre.ServerVolnum[0], &vre.ServerVolnum[1],
		   &vre.ServerVolnum[2], &vre.ServerVolnum[3],
		   &vre.ServerVolnum[4], &vre.ServerVolnum[5],
		   &vre.ServerVolnum[6], &vre.ServerVolnum[7],
		   &vre.addr) != 12) {
	    printf("Bad input line(%d): %s\n", lineno, line);
	    printf("makevrdb aborted\n");
	    exit(EXIT_FAILURE);
	}
	vre.nServers = servercount;
	vre.hton();
	
	if (write(fd, &vre, sizeof(struct vrent)) != sizeof(struct vrent)) {
	    printf("write error on input line(%d): %s\n", lineno, line);
	    printf("makevrdb aborted\n");
	    exit(EXIT_FAILURE);
	}
    }
    close(fd);
}

void ReadConfigFile()
{
    char    confname[MAXPATHLEN];

    /* don't complain if config files are missing */
    codaconf_quiet = 1;

    /* Load configuration file to get vice dir. */
    sprintf (confname, "%s.conf", serverconf);
    (void) conf_init(confname);

    CONF_STR(vicedir,		"vicedir",	   "/vice");

    vice_dir_init(vicedir, 0);
}



int
main(int argc, char **argv) {
    char input;

    ReadConfigFile();

    BuildVRDB();
    CheckVRDB();

    while (1) {
	char buf[80];
	printf("Option(v, n, i)? "); gets(buf);
	sscanf(buf, "%c", &input);
	switch (input) {
	  case 'v': {
	      long vnum;
	      printf("volume number? "); gets(buf);
	      sscanf(buf, "%x", &vnum);
	      printf("The volume number is 0x%x\n", vnum);
	      vrent *newvre = VRDB.find(vnum);
	      if (newvre) 
		  newvre->print();
	      else 
		  printf("Couldn't find the vrdb entry!\n");
	  }
	    break;
	  case 'n': {
	      char buf[80];
	      printf("Volume name? "); gets(buf);
	      vrent *newvre = VRDB.find(buf);
	      if (newvre) newvre->print();
	      else printf("vrent  not found \n");
	  }
	    break;
	  case 'i': {
	      printf("Rereading the data base from %s",
		     vice_file("vol/VRList"));
	      BuildVRDB();
	      CheckVRDB();
	  }
	    break;

	  default: printf("unknown option\n");
	}
    }
    return 0;
}
