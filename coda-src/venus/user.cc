/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
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
 *	1/ There is currently no way of reclaiming user entries!
 *	   Need some GC mechanism!
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <struct.h>
#ifndef __FreeBSD__
#include <utmp.h>
#endif
#include <pwd.h>

#include <rpc2/rpc2.h>
/* interfaces */
#include <auth2.h>
#include <vice.h>
#include <lka.h>

#ifdef __cplusplus
}
#endif

#include <coda_getservbyname.h>
#include "comm.h"
#include "hdb.h"
#include "mariner.h"
#include "user.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vsg.h"
#include "worker.h"

#define CLOCK_SKEW 120 /* seconds */

olist *userent::usertab;

void UserInit()
{
    /* Initialize static members. */
    userent::usertab = new olist;

    USERD_Init();
}

userent *Realm::GetUser(uid_t uid)
{
    LOG(100, ("Realm::GetUser local uid '%d' for realm '%s'\n", uid, name));

    user_iterator next;
    userent *u;

    while ((u = next())) {
        if (Id() == u->realmid && uid == u->GetUid())
            return u;
    }

    /* allocate a new user entry */
    u = new userent(Id(), uid);
    userent::usertab->insert(&u->tblhandle);
    return u;
}

int Realm::NewUserToken(uid_t uid, SecretToken *secretp, ClearToken *clearp)
{
    LOG(100,
        ("Realm::NewUserToken local uid '%d' for realm '%s'\n", uid, name));
    userent *u;
    int ret;

    u = GetUser(uid);

    ret = u->SetTokens(secretp, clearp);

    PutUser(&u);

    return ret ? 0 : EPERM;
}

void PutUser(userent **upp)
{
    LOG(100, ("PutUser: \n"));
}

void UserPrint()
{
    UserPrint(stdout);
}

void UserPrint(FILE *fp)
{
    fflush(fp);
    UserPrint(fileno(fp));
}

void UserPrint(int fd)
{
    if (userent::usertab == 0)
        return;

    fdprint(fd, "Users: count = %d\n", userent::usertab->count());

    user_iterator next;
    userent *u;
    while ((u = next()))
        u->print(fd);

    fdprint(fd, "\n");
}

/* 
 *  An authorized user is either:
 *    logged into the console, or 
 *    the primary user of this machine (as defined by a run-time switch).
 */
int AuthorizedUser(uid_t thisUser)
{
    /* If this user is the primary user of this machine, then this user is
     * authorized */
    if (PrimaryUser != UNSET_PRIMARYUSER) {
        if (PrimaryUser == thisUser) {
            LOG(100,
                ("AuthorizedUser: User (%d) --> authorized as primary user.\n",
                 thisUser));
            return (1);
        }
        /* When primary user is set, this overrides console user checks */
        LOG(100, ("AuthorizedUser: User (%d) --> is not the primary user.\n",
                  thisUser));
        return (0);
    }

    /* If this user is logged into the console, then this user is authorized */
    if (ConsoleUser(thisUser)) {
        LOG(100, ("AuthorizedUser: User (%d) --> authorized as console user.\n",
                  thisUser));
        return (1);
    }

    /* Otherwise, this user is not authorized */
    LOG(100, ("AuthorizedUser: User (%d) --> NOT authorized.\n", thisUser));
    return (0);
}

int ConsoleUser(uid_t user)
{
#if defined(__CYGWIN32__) || defined(__FreeBSD__)
    return (1);

#elif defined(__linux__)
#define CONSOLE "tty1"

    struct utmp w, *u;
    struct passwd *pw;
    int found = 0;

    setutent();

    strcpy(w.ut_line, CONSOLE);
    while (!found && (u = getutline(&w))) {
        pw = getpwnam(u->ut_name);
        if (pw)
            found = (user == pw->pw_uid);
    }
    endutent();

    return (found);

#else /* Look up console user in utmp. */

#define CONSOLE "console"

#ifndef UTMP_FILE
#define UTMP_FILE "/etc/utmp"
#endif

    uid_t uid = ANYUSER_UID;

    FILE *fp = fopen(UTMP_FILE, "r");
    if (fp == NULL)
        return (0);
    struct utmp u;
    while (fread((char *)&u, (int)sizeof(struct utmp), 1, fp) == 1) {
        if (STREQ(u.ut_line, CONSOLE)) {
            struct passwd *pw = getpwnam(u.ut_name);
            if (pw)
                uid = pw->pw_uid;
            break;
        }
    }
    if (fclose(fp) == EOF)
        CHOKE("ConsoleUser: fclose(%s) failed", UTMP_FILE);

    return (uid == user);
#endif
}

userent::userent(RealmId rid, uid_t userid)
{
    LOG(100, ("userent::userent: uid = %d\n", userid));

    realmid     = rid;
    uid         = userid;
    tokensvalid = 0;
    told_you_so = 0;
    memset((void *)&secret, 0, (int)sizeof(SecretToken));
    memset((void *)&clear, 0, (int)sizeof(ClearToken));
    waitforever = 0;
}

/* we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
userent::userent(userent &u)
{
    abort();
}
int userent::operator=(userent &u)
{
    abort();
    return (0);
}

userent::~userent()
{
    LOG(100, ("userent::~userent: uid = %d\n", uid));
    Invalidate();
}

long userent::SetTokens(SecretToken *asecret, ClearToken *aclear)
{
    LOG(100, ("userent::SetTokens: uid = %d\n", uid));

    /* grab an extra reference on the realm until the token expires */
    if (!tokensvalid) {
        Realm *realm = REALMDB->GetRealm(realmid);
        realm->GetRef();
        realm->PutRef();
    }

    /* N.B. Using direct assignment to the Token structs rather than the
     * bcopys (now memcpy) doesn't seem to work!
     * XXX Bogus comment? Phil Nelson*/
    memcpy(&secret, asecret, sizeof(SecretToken));
    memcpy(&clear, aclear, sizeof(ClearToken));
    tokensvalid = 1;

    LOG(100, ("SetTokens calling Reset\n"));
    Reset();

    /* Inform the advice monitor that user now has tokens. */
    LOG(100, ("calling TokensAcquired with %d\n",
              (clear.EndTimestamp - CLOCK_SKEW)));

    /* Make dirty volumes "owned" by this user available for reintegration. */
    repvol_iterator next;
    repvol *v;
    while ((v = next())) {
        ClientModifyLog *CML = v->GetCML();
        if (CML->Owner() == uid && CML->count())
            v->flags.transition_pending = 1;
    }

    return (1);
}

long userent::GetTokens(SecretToken *asecret, ClearToken *aclear)
{
    LOG(100,
        ("userent::GetTokens: uid = %d, tokensvalid = %d\n", uid, tokensvalid));

    if (!tokensvalid)
        return (ENOTCONN);

    if (asecret)
        memcpy(asecret, &secret, sizeof(SecretToken));
    if (aclear)
        memcpy(aclear, &clear, sizeof(ClearToken));

    return (0);
}

int userent::TokensValid()
{
    LOG(100, ("userent %d tokensvalid = %d\n", uid, tokensvalid));
    return (tokensvalid);
}

void userent::CheckTokenExpiry()
{
    if (!tokensvalid)
        return;

    time_t curr_time = Vtime(), timeleft;

    /* We don't invalidate the tokens anymore. The server will disconnect us
     * if we try to use an expired one, and we can continue accessing files
     * during disconnections (when we cannot possibly obtain a new token)
     * The only thing we do here is warn the user of the impending doom. --JH */

    if (curr_time >= clear.EndTimestamp) {
        if (!told_you_so) {
            eprint("Coda token for user %d has expired", uid);
            told_you_so = 1;
        }
        //Invalidate();
    } else if (curr_time >= clear.EndTimestamp - 3600) {
        timeleft = ((clear.EndTimestamp - curr_time) / 60) + 1;
        eprint(
            "Coda token for user %d will be rejected by the servers in +/- %d minutes",
            uid, timeleft);
    }
}

void userent::Invalidate()
{
    LOG(100, ("userent::Invalidate: uid = %d, tokensvalid = %d\n", uid,
              tokensvalid));

    if (!tokensvalid)
        return;

    Realm *realm = REALMDB->GetRealm(realmid);
    realm->PutRef(); /* one because we had a token */
    realm->PutRef(); /* one to compensate for the GetRealm reference */

    /* Security is not having to say you're sorry. */
    tokensvalid = 0;
    told_you_so = 0;
    memset((void *)&secret, 0, (int)sizeof(SecretToken));
    memset((void *)&clear, 0, (int)sizeof(ClearToken));

    /* Inform the user */
    eprint("Coda token for user %d has been discarded", uid);

    Reset();
}

void userent::Reset()
{
    LOG(100, ("E userent::Reset()\n"));
    /* Clear the cached access info for the user. */
    // FSDB->ResetUser(uid);

    /* Invalidate kernel data for the user. */
    k_Purge(uid);
    LOG(100, ("After k_Purge in userent::Reset\n"));

    /* Demote HDB bindings for the user. */
    HDB->ResetUser(uid);
    LOG(100, ("After HDB::ResetUser in userent::Reset\n"));

    /* Delete the user's connections. */
    {
        struct ConnKey Key;
        Key.host.s_addr = INADDR_ANY;
        Key.uid         = uid;
        conn_iterator next(&Key);
        connent *c = 0;
        while ((c = next()))
            c->Suicide();
    }

    /* Delete the user's mgrps. */
    VSGDB->KillUserMgrps(uid);

    LOG(100, ("L userent::Reset()\n"));
}

int userent::CheckFetchPartialSupport(RPC2_Handle *cid, srvent *sv,
                                      int *retry_cnt)
{
    int inconok     = 0;
    uint64_t offset = 0;
    int64_t len     = -1;
    VenusFid fid    = NullFid;
    int code        = 0;

    /* If it's known don't get it again */
    if (sv->fetchpartial_support) {
        return 0;
    }

    /* VersionVector */
    ViceVersionVector vv;
    memset((void *)&vv, 0, (int)sizeof(ViceVersionVector));

    /* Status parameters. */
    ViceStatus status;
    memset((void *)&status, 0, (int)sizeof(ViceStatus));

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen  = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* Set up the SE descriptor. */
    SE_Descriptor dummysed;
    memset(&dummysed, 0, sizeof(SE_Descriptor));
    SE_Descriptor *sed = &dummysed;

    /* SFTP setup */
    sed->Tag    = SMARTFTP;
    sed->XferCB = NULL;
    sed->userp  = this;

    struct SFTP_Descriptor *sei = &sed->Value.SmartFTPD;
    sei->TransmissionDirection  = SERVERTOCLIENT;
    sei->hashmark               = 0;
    sei->SeekOffset             = offset;
    sei->ByteQuota              = len;

    sei->Tag              = FILEBYFD;
    sei->FileInfo.ByFD.fd = -1;

    /* Perform the RPC */
    UNI_START_MESSAGE(ViceFetchPartial_OP);
    code = ViceFetchPartial(*cid, MakeViceFid(&fid), &vv, inconok, &status, 0,
                            offset, len, &PiggyBS, sed);
    UNI_END_MESSAGE(ViceFetchPartial_OP);
    MarinerLog(
        "userent::CheckFetchPartialSupport: ViceFetchPartial test returned %d\n",
        code);
    UNI_RECORD_STATS(ViceFetchPartial_OP);

    /* No doubt */
    if (code == RPC2_INVALIDOPCODE) {
        LOG(100,
            ("userent::CheckFetchPartialSupport: ViceFetchPartial Operation Not supported\n"));
        sv->fetchpartial_support = 0;
        return 0;
    }

    /* If it's not clear retry */
    if (code < 0) {
        LOG(100,
            ("userent::CheckFetchPartialSupport: ViceFetchPartial retrying\n"));
        if (--*retry_cnt)
            return ERETRY;
        return code;
    }

    /* Only EINVAL confirms presence of ViceFetchPartial().
       Conservatively interpret all other server return codes as nonexistence */
    if (code == EINVAL) {
        LOG(100,
            ("userent::CheckFetchPartialSupport: ViceFetchPartial Supported\n"));
        sv->fetchpartial_support = 1;
    } else {
        LOG(100,
            ("userent::CheckFetchPartialSupport: ViceFetchPartial Operation Not supported\n"));
        sv->fetchpartial_support = 0;
    }

    return 0;
}

int userent::Connect(RPC2_Handle *cid, int *auth, struct in_addr *host)
{
    LOG(100, ("userent::Connect: addr = %s, uid = %d, tokensvalid = %d\n",
              inet_ntoa(*host), uid, tokensvalid));

    *cid                  = 0;
    int code              = 0;
    int support_test_code = 0;

    /* This may be a request to connect either to a specific host, or to form an mgrp. */
    if (host->s_addr == INADDR_ANY) {
        /* If the user has valid tokens, we specify an authenticated mgrp.
	 * exception; when talking to a staging server */
        /* Otherwise, we specify an unauthenticated mgrp. */
        long sl;
        if (*auth && tokensvalid) {
            sl    = RPC2_AUTHONLY;
            *auth = 1;
        } else {
            sl    = RPC2_OPENKIMONO;
            *auth = 0;
        }

        /* Attempt to create the mgrp. */
        RPC2_McastIdent mcid;
        mcid.Tag               = RPC2_MGRPBYINETADDR;
        mcid.Value.InetAddress = *host;

        struct servent *s = coda_getservbyname("codasrv", "udp");
        RPC2_PortIdent pid;
        pid.Tag                  = RPC2_PORTBYINETNUMBER;
        pid.Value.InetPortNumber = s->s_port;

        RPC2_SubsysIdent ssid;
        ssid.Tag            = RPC2_SUBSYSBYID;
        ssid.Value.SubsysId = SUBSYS_SRV;
        LOG(1, ("userent::Connect: RPC2_CreateMgrp(%s)\n", inet_ntoa(*host)));
        code = (int)RPC2_CreateMgrp(cid, &mcid, &pid, &ssid, sl,
                                    clear.HandShakeKey, RPC2_XOR, SMARTFTP);
        LOG(1,
            ("userent::Connect: RPC2_CreateMgrp -> %s\n", RPC2_ErrorMsg(code)));
    } else {
        char username[16];
        if (uid == 0)
            strcpy(username, "root"); /* root */
        else
            sprintf(username, "UID=%08u", uid); /* normal user */

        /*
         * If the user has valid tokens and he is not root, we send the secret
         * token in an authenticated bind. Otherwise, we send the username in
         * an unauthenticated bind.
	 */
        RPC2_CountedBS clientident;
        RPC2_BindParms bparms;

        if (*auth && tokensvalid) {
            clientident.SeqLen   = sizeof(SecretToken);
            clientident.SeqBody  = (RPC2_ByteSeq)&secret;
            bparms.SecurityLevel = RPC2_AUTHONLY;
            *auth                = 1;
        } else {
            clientident.SeqLen   = strlen(username) + 1;
            clientident.SeqBody  = (RPC2_ByteSeq)username;
            bparms.SecurityLevel = RPC2_OPENKIMONO;
            *auth                = 0;
        }

        /* Attempt the bind. */
        RPC2_HostIdent hid;
        hid.Tag               = RPC2_HOSTBYINETADDR;
        hid.Value.InetAddress = *host;

        struct servent *s = coda_getservbyname("codasrv", "udp");
        RPC2_PortIdent pid;
        pid.Tag                  = RPC2_PORTBYINETNUMBER;
        pid.Value.InetPortNumber = s->s_port;

        RPC2_SubsysIdent ssid;
        ssid.Tag              = RPC2_SUBSYSBYID;
        ssid.Value.SubsysId   = SUBSYS_SRV;
        bparms.EncryptionType = RPC2_XOR;
        bparms.SideEffectType = SMARTFTP;
        bparms.ClientIdent    = &clientident;
        bparms.SharedSecret   = &clear.HandShakeKey;

        int retry_cnt = 3;

    RetryConnect:

        LOG(1, ("userent::Connect: RPC2_NewBinding(%s)\n", inet_ntoa(*host)));
        code = (int)RPC2_NewBinding(&hid, &pid, &ssid, &bparms, cid);
        LOG(1,
            ("userent::Connect: RPC2_NewBinding -> %s\n", RPC2_ErrorMsg(code)));

        /* Invalidate tokens on authentication failure. */
        /* Higher level software may retry unauthenticated if desired. */
        if (code == RPC2_NOTAUTHENTICATED) {
            LOG(0, ("userent::Connect: Authenticated bind failure, uid = %d\n",
                    uid));

            Invalidate();
        }

        if (code <= RPC2_ELIMIT)
            return (code);

        /* connect to the file service */
        ViceClient vc;
        vc.UserName        = (RPC2_String)username;
        vc.WorkStationName = (RPC2_String)myHostName;
        vc.VenusName       = (RPC2_String) "venus";

        /* This UUID identifies this client during it's lifetime.
	 * It is only reset when RVM is reinitialized */
        memcpy(vc.VenusUUID, &VenusGenID, sizeof(ViceUUID));

        srvent *sv  = FindServer(&hid.Value.InetAddress);
        char *sname = sv->name;
        LOG(1, ("userent::Connect: NewConnectFS(%s)\n", sname));
        MarinerLog("fetch::NewConnectFS %s\n", sname);
        UNI_START_MESSAGE(ViceNewConnectFS_OP);
        code = (int)ViceNewConnectFS(*cid, VICE_VERSION, &vc);
        UNI_END_MESSAGE(ViceNewConnectFS_OP);
        MarinerLog("fetch::newconnectfs done\n");
        UNI_RECORD_STATS(ViceNewConnectFS_OP);
        LOG(1, ("userent::Connect: NewConnectFS -> %d\n", code));

        support_test_code = CheckFetchPartialSupport(cid, sv, &retry_cnt);

        /* Check of retry */
        if (support_test_code == ERETRY) {
            /* some other RPC2 error code */
            RPC2_Unbind(*cid);
            goto RetryConnect;
        }

        /* Propagate the error */
        if (support_test_code < 0) {
            code = support_test_code;
        }

        if (code) {
            int unbind_code = (int)RPC2_Unbind(*cid);
            LOG(1, ("userent::Connect: RPC2_Unbind -> %s\n",
                    RPC2_ErrorMsg(unbind_code)));
            return (code);
        }
    }
    return (code);
}

int userent::GetWaitForever()
{
    return (waitforever);
}

void userent::SetWaitForever(int state)
{
    LOG(1,
        ("userent::SetWaitForever: uid = %d, old_state = %d, new_state = %d\n",
         uid, waitforever, state));

    if (state == waitforever)
        return;

    waitforever = state;
    if (!waitforever)
        /* Poke anyone who was waiting on a retry event. */
        Rtry_Signal();
}

void userent::print()
{
    print(stdout);
}

void userent::print(FILE *fp)
{
    fflush(fp);
    print(fileno(fp));
}

void userent::print(int afd)
{
    char begin_time[13];
    char end_time[13];
    time_t timestamp;
    char *tmp;
    if (tokensvalid) {
        timestamp = (time_t)clear.BeginTimestamp;
        tmp       = ctime(&timestamp);
        strncpy(begin_time, tmp + 4, 12);
        begin_time[12] = '\0';

        timestamp = (time_t)clear.EndTimestamp;
        tmp       = ctime(&timestamp);
        strncpy(end_time, tmp + 4, 12);
        end_time[12] = '\0';
    } else {
        begin_time[0] = '\0';
        end_time[0]   = '\0';
    }

    fdprint(afd, "Time of last demand hoard walk = %ld\n", DemandHoardWalkTime);

    fdprint(afd,
            "%#08x : uid = %d, wfe = %d, valid = %d, begin = %s, end = %s\n\n",
            (long)this, uid, waitforever, tokensvalid, begin_time, end_time);
}

user_iterator::user_iterator()
    : olist_iterator((olist &)*userent::usertab)
{
}

userent *user_iterator::operator()()
{
    olink *o = olist_iterator::operator()();
    if (!o)
        return (0);

    userent *u = strbase(userent, o, tblhandle);
    return (u);
}

/* **************************************** */

/* Move this stuff to user_daemon.c! -JJK */

/* *****  Private constants  ***** */

static const int UserDaemonInterval  = 300;
static const int UserDaemonStackSize = 16384;

/* ***** Private variables  ***** */

static char userdaemon_sync;

void USERD_Init(void)
{
    (void)new vproc("UserDaemon", UserDaemon, VPT_UserDaemon,
                    UserDaemonStackSize);
}

void UserDaemon(void)
{
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
