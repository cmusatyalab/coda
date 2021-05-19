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
*/

// Utility routines ... i.e. don't want inline code in the gui.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "vcodacon.h"
#include "util.h"
#include <FL/fl_ask.H>
#include <sys/stat.h>
#include <codaconf.h>

char *XferLabel[3];

/* For clog/cunlog */

static char **realmlist = NULL;
static int nlist        = 0;
static int nrealm       = 0;

static const char *codadir = "/coda";

/* return the index of the name in the realmlist */

int lookup_realm(char *name)
{
    int ix;
    for (ix = 0; ix < nrealm; ix++)
        if (strcmp(realmlist[ix], name) == 0)
            return ix;
    return -1;
}

static void add_realm(const char *name)
{
    if (nrealm + 1 > nlist) {
        // allocate more!
        if (nlist == 0) {
            // Initial allocation
            realmlist = (char **)malloc(8 * sizeof(char *));
            if (!realmlist) {
                fl_alert("ABORTING ... No memory for a realm list.\n");
                abort();
            }
            nlist = 8;
        } else {
            nlist *= 2;
            realmlist = (char **)realloc(realmlist, nlist * sizeof(char *));
        }
    }

    // Add it!
    realmlist[nrealm++] = strdup(name);
}

int update_realmlist(void)
{
    DIR *d;
    struct dirent *ent;

    /* Open the root coda directory. */

    d = opendir(codadir);
    if (!d)
        return 0;

    /* Walk the directory ... */
    while ((ent = readdir(d))) {
        if (ent->d_name[0] != '.') {
            if (ent->d_name[0] == 'N' &&
                (strcmp(ent->d_name, "NOT_REALLY_CODA") == 0))
                return 0;
            //      printf ("Looking at name '%s'\n", ent->d_name);
            if (lookup_realm(ent->d_name) < 0) {
                //	printf ("Adding '%s'\n", ent->d_name);
                add_realm(ent->d_name);
            }
        }
    }
    return 1;
}

//  Do the clog ...
void do_clog()
{
    char cmd[255];
    const char *user = clogUserName->value();
    const char *pass = clogPassword->value();
    FILE *p;
    int stat;
    int ix;

    ix = clogRealm->value();

    if (ix < 0) {
        fl_alert("Please select a realm.\n");
        return;
    }

    if (strlen(user) == 0) {
        fl_alert("Please enter a user name.\n");
        return;
    }

    if (realmlist)
        snprintf(cmd, 255, "clog -pipe %s@%s", user, realmlist[ix]);
    else
        snprintf(cmd, 255, "clog -pipe %s", user);
    p = popen(cmd, "w");

    if (!p) {
        fl_alert("could not start clog");
        return;
    }

    fprintf(p, "%s\n", pass);
    stat = pclose(p);

    if (stat) {
        fl_alert("clog failed");
        return;
    }

    /* Cleanup password  and window. */
    clogPassword->value("");
    Clog->hide();
}

void do_cunlog()
{
    char cmd[255];
    FILE *p;
    int stat;
    int ix;

    ix = cunlogRealm->value();

    if (ix < 0) {
        fl_alert("Please select a realm.\n");
        return;
    }

    snprintf(cmd, 255, "cunlog @%s", realmlist[ix]);
    p = popen(cmd, "w");

    if (!p) {
        fl_alert("could not start cunlog");
        return;
    }
    stat = pclose(p);

    if (stat) {
        fl_alert("cunlog failed");
        return;
    }

    /* window. */
    Cunlog->hide();
}

void menu_clog(void)
{
    int ix;

    if (update_realmlist()) {
        clogRealm->clear();
        for (ix = 0; ix < nrealm; ix++)
            clogRealm->add(realmlist[ix], 0, NULL);
        clogRealm->value(0);
        clogPassword->value("");
        Clog->show();
    } else
        fl_alert("Could not look in directory %s for realms.\n", codadir);
}

void menu_ctokens(void)
{
    FILE *p;
    char line[100];
    char cmd[100];

    snprintf(cmd, 100, "ctokens");
    p = popen(cmd, "r");
    if (!p) {
        fl_alert("could not start ctokens");
        return;
    }
    TokenList->clear();
    CTokens->show();
    while (fgets(line, 100, p))
        TokenList->add(line, NULL);
    pclose(p);
    TokenList->show();
    TokenList->redraw();
}

void menu_cunlog(void)
{
    int ix;

    if (update_realmlist()) {
        if (!realmlist) {
            fl_alert("No known realms for cunlog.");
            return;
        }
        cunlogRealm->clear();
        for (ix = 0; ix < nrealm; ix++)
            cunlogRealm->add(realmlist[ix], 0, NULL);
        cunlogRealm->value(0);
        Cunlog->show();
    } else
        fl_alert("Could not look in directory %s for realms.\n", codadir);
}

// Stat /coda/name to find a realm

int do_findRealm(const char *realm)
{
    char fullname[258];
    int realmlen = strlen(realm);

    struct stat sb;
    int rv;

    if (realmlen > 250) {
        fl_alert("%s: name too long", realm);
        return 0;
    }

    snprintf(fullname, 258, "/coda/%s", realm);

    rv = stat(fullname, &sb);

    if (rv < 0 || ((sb.st_mode & S_IFMT) != S_IFDIR))
        return 0;

    return 1;
}

// The main program ...

void MainInit(int *argcp, char ***argvp)
{
    const char *myrealm;

    // Initialize the visual tool
    // for (int i=0; i<8; i++) Vol[i]->hide();
    for (int i = 0; i < 3; i++) {
        XferLabel[i] = NULL;
        XferProg[i]->hide();
    }

    codaconf_init("venus.conf");
    myrealm = codaconf_lookup("realm", NULL);
    if (myrealm)
        add_realm(myrealm);
}
