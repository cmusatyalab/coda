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

/*
 *
 * Implementation of the Venus User abstraction.
 *
 */

/*
 *  ToDo:
 *	1/ There is currently no way of reclaiming user entries!  Need some GC mechanism!
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <struct.h>
#ifndef DJGPP
#include <utmp.h>
#endif
#include <pwd.h>

#include <rpc2.h>
/* interfaces */
#include <auth2.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "comm.h"
#include "hdb.h"
#include "mariner.h"
#include "user.h"
#include "venus.private.h"
#include "worker.h"


#define	CLOCK_SKEW  120	    /* seconds */

olist *userent::usertab;

void UserInit() {
    /* Initialize static members. */
    userent::usertab = new olist;

    USERD_Init();
}


userent *FindUser(vuid_t vuid) {
    user_iterator next;
    userent *u;

    while ((u = next()))
	if (u->uid == vuid) return(u);

    return(0);
}


userent *FindUserByAdviceHandle(RPC2_Handle handle) {
  user_iterator next;
  userent *u;

  while ((u = next()))
    if (u->admon.IsAdviceHandle(handle) == TRUE) return(u);

  return(0);
}


void GetUser(userent **upp, vuid_t vuid) {
    LOG(100, ("GetUser: uid = %d\n", vuid));

    userent *u = FindUser(vuid);
    if (u) {
	*upp = u;
	return;
    }

    /* Create a new entry and initialize it. */
    u = new userent(vuid);

    /* Stick it in the table. */
    userent::usertab->insert(&u->tblhandle);

    *upp = u;
}


void PutUser(userent **upp) {
    LOG(100, ("PutUser: \n"));

}


void UserPrint() {
    UserPrint(stdout);
}


void UserPrint(FILE *fp) {
    fflush(fp);
    UserPrint(fileno(fp));
}


void UserPrint(int fd) {
    if (userent::usertab == 0) return;

    fdprint(fd, "Users: count = %d\n", userent::usertab->count());

    user_iterator next;
    userent *u;
    while ((u = next())) u->print(fd);

    fdprint(fd, "\n");
}

/* 
 *  An authorized user is either:
 *    logged into the console, or 
 *    the primary user of this machine (as defined by a run-time switch).
 */
int AuthorizedUser(vuid_t thisUser) {

    /* If this user is the primary user of this machine, then this user is
     * authorized */
    if (PrimaryUser != UNSET_PRIMARYUSER) {
	if (PrimaryUser == thisUser) {
	    LOG(100, ("AuthorizedUser: User (%d) --> authorized as primary user.\n", thisUser));
	    return(1);
	}
	/* When primary user is set, this overrides console user checks */
	LOG(100, ("AuthorizedUser: User (%d) --> is not the primary user.\n", thisUser));
	return(0);
    }

  /* If this user is logged into the console, then this user is authorized */
  if (ConsoleUser(thisUser)) {
    LOG(100, ("AuthorizedUser: User (%d) --> authorized as console user.\n", thisUser));
    return(1);
  }

  /* Otherwise, this user is not authorized */
  LOG(100, ("AuthorizedUser: User (%d) --> NOT authorized.\n", thisUser));
  return(0);
}

int ConsoleUser(vuid_t user)
{
#ifdef DJGPP
    return(1);

#elif __linux__
#define	CONSOLE	    "tty1"

    struct utmp w, *u;
    struct passwd *pw;
    int found = 0;

    setutent();

    strcpy(w.ut_line, CONSOLE);
    while (!found && (u = getutline(&w))) {
	pw = getpwnam(u->ut_name);
	if (pw) found = (user == pw->pw_uid);
    }
    endutent();

    return(found);

#else /* Look up console user in utmp. */

#define	CONSOLE	    "console"

#ifndef UTMP_FILE
#define	UTMP_FILE   "/etc/utmp"
#endif

    vuid_t vuid = ALL_UIDS;

    FILE *fp = fopen(UTMP_FILE, "r");
    if (fp == NULL) return(vuid);
    struct utmp u;
    while (fread((char *)&u, (int)sizeof(struct utmp), 1, fp) == 1) {
	if (STREQ(u.ut_line, CONSOLE)) {
	    struct passwd *pw = getpwnam(u.ut_name);
	    if (pw) vuid = pw->pw_uid;
	    break;
	}
    }
    if (fclose(fp) == EOF)
	CHOKE("ConsoleUser: fclose(%s) failed", UTMP_FILE);

    return(vuid == user);
#endif
}


userent::userent(vuid_t userid) {
    LOG(100, ("userent::userent: uid = %d\n", userid));

    uid = userid;
    tokensvalid = 0;
    told_you_so = 0;
    bzero((void *)&secret, (int) sizeof(SecretToken));
    bzero((void *)&clear, (int) sizeof(ClearToken));
    waitforever = 0;
    admon.Reset();
}


/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
userent::userent(userent& u) {
    abort();
}


int userent::operator=(userent& u) {
    abort();
    return(0);
}


userent::~userent() {
    LOG(100, ("userent::~userent: uid = %d\n", uid));

    Invalidate();
}


long userent::SetTokens(SecretToken *asecret, ClearToken *aclear) {
    LOG(100, ("userent::SetTokens: uid = %d\n", uid));

    if (uid == V_UID) {
	eprint("root acquiring Coda tokens!");
    }

    /* N.B. Using direct assignment to the Token structs rather than the bcopys doesn't seem to work! */
    tokensvalid = 1;
    bcopy((const void *)asecret, (void *) &secret, (int) sizeof(SecretToken));
    bcopy((const void *)aclear, (void *) &clear, (int) sizeof(ClearToken));
LOG(100, ("SetTokens calling Reset\n"));
    Reset();

    /* Inform the advice monitor that user now has tokens. */
    LOG(100, ("calling TokensAcquired with %d\n", (clear.EndTimestamp-CLOCK_SKEW)));
    admon.TokensAcquired((clear.EndTimestamp - CLOCK_SKEW));
    LOG(100, ("returned from TokensAcquired\n"));


    /* Make dirty volumes "owned" by this user available for reintegration. */
    vol_iterator next;
    volent *v;
    while ((v = next()))
	if (v->type == REPVOL && v->state == Emulating && v->CML.Owner() == uid) {
	    v->flags.transition_pending = 1;
	    v->ClearReintegratePending();
	}

    return(1);
}


long userent::GetTokens(SecretToken *asecret, ClearToken *aclear) {
    LOG(100, ("userent::GetTokens: uid = %d, tokensvalid = %d\n", uid, tokensvalid));

    if (!tokensvalid) return(ENOTCONN);

    if (asecret) memcpy(asecret, &secret, sizeof(SecretToken));
    if (aclear)  memcpy(aclear, &clear, sizeof(ClearToken));

    return(0);
}


int userent::TokensValid() {
    return(tokensvalid);
}


void userent::CheckTokenExpiry() {
    if (!tokensvalid) return;

    time_t curr_time = Vtime(), timeleft;

    /* We don't invalidate the tokens anymore. The server will disconnect us
     * if we try to use an expired one, and we can continue accessing files
     * during disconnections (when we cannot possibly obtain a new token)
     * The only thing we do here is warn the user of the impending doom. --JH */

    if (curr_time >= clear.EndTimestamp && !told_you_so) {
	eprint("Coda token for user %d has expired", uid);
	told_you_so = 1;
	//Invalidate();
    } else if (curr_time >= clear.EndTimestamp - 3600) {
	timeleft = ((clear.EndTimestamp - curr_time) / 60) + 1;
	eprint("Coda token for user %d will be rejected by the servers in +/- %d minutes", uid, timeleft);
    }
}


void userent::Invalidate() {
    LOG(100, ("userent::Invalidate: uid = %d, tokensvalid = %d\n",
	    uid, tokensvalid));

    /* Security is not having to say you're sorry. */
    tokensvalid = 0;
    told_you_so = 0;
    bzero((void *)&secret, (int) sizeof(SecretToken));
    bzero((void *)&clear, (int) sizeof(ClearToken));

    /* Inform the user */
    eprint("Coda token for user %d has been discarded", uid);
    admon.TokensExpired();

    Reset();
}


void userent::Reset() {
LOG(100, ("E userent::Reset()\n"));
    /* Clear the cached access info for the user. */
    FSDB->ResetUser(uid);

    /* Invalidate kernel data for the user. */
    k_Purge((vuid_t) uid);
LOG(100, ("After k_Purge in userent::Reset\n"));

    /* Demote HDB bindings for the user. */
    HDB->ResetUser(uid);
LOG(100, ("After HDB::ResetUser in userent::Reset\n"));

    /* Delete the user's connections. */
    {
	struct ConnKey Key; Key.host = ALL_HOSTS; Key.vuid = uid;
	conn_iterator next(&Key);
	connent *c = 0;
	connent *tc = 0;
	for (c = next(); c != 0; c = tc) {
	    tc = next();		/* read ahead */
	    (void)c->Suicide(1);
	}
    }

    /* Delete the user's mgrps. */
    {
	struct MgrpKey Key; Key.vsgaddr = ALL_VSGS; Key.vuid = uid;
	mgrp_iterator next(&Key);
	mgrpent *m = 0;
	mgrpent *tm = 0;
	for (m = next(); m != 0; m = tm) {
	    tm = next();		/* read ahead */
	    (void)m->Suicide(1);
	}
    }

LOG(100, ("L userent::Reset()\n"));
}


int userent::Connect(RPC2_Handle *cid, int *auth, unsigned long host) {
    LOG(100, ("userent::Connect: addr = %x, uid = %d, tokensvalid = %d\n",
	       host, uid, tokensvalid));

    *cid = 0;
    int code = 0;

    /* This may be a request to connect either to a specific host, or to form an mgrp. */
    if (IN_CLASSD(host)) {
	/* If the user has valid tokens and he is not root, we specify an authenticated mgrp. */
	/* Otherwise, we specify an unauthenticated mgrp. */
	long sl;
	/* root can now own tokens */
	if (/* uid != V_UID && */tokensvalid) {
	    sl = RPC2_AUTHONLY;
	    *auth = 1;
	}
	else {
	    sl = RPC2_OPENKIMONO;
	    *auth = 0;
	}

	/* Attempt to create the mgrp. */
	RPC2_McastIdent mcid;
	mcid.Tag = RPC2_MGRPBYINETADDR;
	mcid.Value.InetAddress.s_addr = htonl(host);
	RPC2_PortIdent pid;
	pid.Tag = RPC2_PORTBYNAME;
	strcpy(pid.Value.Name, "codasrv");
	RPC2_SubsysIdent ssid;
	ssid.Tag = RPC2_SUBSYSBYID;
	ssid.Value.SubsysId = SUBSYS_SRV;
	LOG(1, ("userent::Connect: RPC2_CreateMgrp(%x)\n", host));
	code = (int) RPC2_CreateMgrp(cid, &mcid, &pid, &ssid, sl,
			       clear.HandShakeKey, RPC2_XOR, SMARTFTP);
	LOG(1, ("userent::Connect: RPC2_CreateMgrp -> %s\n",
		RPC2_ErrorMsg(code)));
	return(code);
    }
    else {
	char username[16];
	if (uid	== 0) strcpy(username, "root");		/* root */
	else sprintf(username, "UID=%08u", uid);	/* normal user */

	/* 
	 * If the user has valid tokens and he is not root, we send the secret token in an
	 * authenticated bind. Otherwise, we send the username in an unauthenticated bind. 
	 */
	RPC2_CountedBS clientident;
	RPC2_BindParms bparms;

	/* root can now own tokens */
	if (/* uid != V_UID && */ tokensvalid) {
	    clientident.SeqLen = sizeof(SecretToken);
	    clientident.SeqBody = (RPC2_ByteSeq)&secret;
	    bparms.SecurityLevel = RPC2_AUTHONLY;
	    *auth = 1;
	} else {
	    clientident.SeqLen = strlen(username) + 1;
	    clientident.SeqBody = (RPC2_ByteSeq)username;
	    bparms.SecurityLevel = RPC2_OPENKIMONO;
	    *auth = 0;
	}

	/* Attempt the bind. */
	RPC2_HostIdent hid;
	hid.Tag = RPC2_HOSTBYINETADDR;
	hid.Value.InetAddress.s_addr = htonl(host);
	RPC2_PortIdent pid;
	pid.Tag = RPC2_PORTBYNAME;
	strcpy(pid.Value.Name, "codasrv");
	RPC2_SubsysIdent ssid;
	ssid.Tag = RPC2_SUBSYSBYID;
	ssid.Value.SubsysId = SUBSYS_SRV;
	bparms.EncryptionType = RPC2_XOR;
	bparms.SideEffectType = SMARTFTP;
	bparms.ClientIdent = &clientident;
	bparms.SharedSecret = &clear.HandShakeKey;

	LOG(1, ("userent::Connect: RPC2_NewBinding(%x)\n", host));
	code = (int) RPC2_NewBinding(&hid, &pid, &ssid, &bparms, cid);
	LOG(1, ("userent::Connect: RPC2_NewBinding -> %s\n", RPC2_ErrorMsg(code)));

	/* Invalidate tokens on authentication failure. */
	/* Higher level software may retry unauthenticated if desired. */
	if (code == RPC2_NOTAUTHENTICATED) {
	    LOG(0, ("userent::Connect: Authenticated bind failure, uid = %d\n", uid));

	    Invalidate();
	}

	if (code <= RPC2_ELIMIT) return(code);

	/* connect to the file service */
	ViceClient vc;
	vc.UserName = (RPC2_String) username;
	vc.WorkStationName = (RPC2_String) myHostName;
	vc.VenusName = (RPC2_String) "venus";

	/* This UUID identifies this client during it's lifetime.
	 * It is only reset when RVM is reinitialized */
	memcpy(vc.VenusUUID, &VenusGenID, sizeof(ViceUUID));

	char *sname = (FindServer(ntohl(hid.Value.InetAddress.s_addr)))->name;
	LOG(1, ("userent::Connect: NewConnectFS(%s)\n", sname)); 
	MarinerLog("fetch::NewConnectFS %s\n", sname);
	UNI_START_MESSAGE(ViceNewConnectFS_OP);
	code = (int) ViceNewConnectFS(*cid, VICE_VERSION, &vc);
	UNI_END_MESSAGE(ViceNewConnectFS_OP);
	MarinerLog("fetch::newconnectfs done\n");
	UNI_RECORD_STATS(ViceNewConnectFS_OP);
	LOG(1, ("userent::Connect: NewConnectFS -> %d\n", code)); 

	if (code) {
	    int unbind_code = (int) RPC2_Unbind(*cid);
	    LOG(1, ("userent::Connect: RPC2_Unbind -> %s\n",
		    RPC2_ErrorMsg(unbind_code)));
	    return(code);
	}
	return(0);
    }
}


int userent::GetWaitForever() {
    return(waitforever);
}


void userent::SetWaitForever(int state) {
    LOG(1, ("userent::SetWaitForever: uid = %d, old_state = %d, new_state = %d\n",
	     uid, waitforever, state));

    if (state == waitforever) return;

    waitforever = state;
    if (!waitforever)
	/* Poke anyone who was waiting on a retry event. */
	Rtry_Signal();
}


void userent::print() {
    print(stdout);
}


void userent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void userent::print(int afd) {
    char begin_time[13];
    char end_time[13];
    if (tokensvalid) {
	char *b = ctime((time_t *)&clear.BeginTimestamp);
	strncpy(begin_time, b + 4, 12);
	begin_time[12] = '\0';
	char *e = ctime((time_t *)&clear.EndTimestamp);
	strncpy(end_time, e + 4, 12);
	end_time[12] = '\0';
    }
    else {
	begin_time[0] = '\0';
	end_time[0] = '\0';
    }

    admon.Print(afd);
    fdprint(afd, "Time of last demand hoard walk = %ld\n", DemandHoardWalkTime);

    fdprint(afd, "%#08x : uid = %d, wfe = %d, valid = %d, begin = %s, end = %s\n\n",
	     (long)this, uid, waitforever, tokensvalid, begin_time, end_time);
}


user_iterator::user_iterator() : olist_iterator((olist&)*userent::usertab) {
}


userent *user_iterator::operator()() {
    olink *o = olist_iterator::operator()();
    if (!o) return(0);

    userent *u = strbase(userent, o, tblhandle);
    return(u);
}


/* **************************************** */

/* Move this stuff to user_daemon.c! -JJK */

/* *****  Private constants  ***** */

static const int UserDaemonInterval = 300;
static const int UserDaemonStackSize = 16384;


/* ***** Private variables  ***** */

static char userdaemon_sync;

void USERD_Init() {
    (void)new vproc("UserDaemon", (PROCBODY)&UserDaemon,
    		     VPT_UserDaemon, UserDaemonStackSize);
}

void UserDaemon() {
    /* Hack!  Vproc must yield before data members become valid! */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(UserDaemonInterval, &userdaemon_sync);

    for (;;) {
	VprocWait(&userdaemon_sync);

	user_iterator next;
	userent *u;
	while ((u = next())) 
	    u->CheckTokenExpiry();

	/* Bump sequence number. */
	vp->seq++;
    }
}
