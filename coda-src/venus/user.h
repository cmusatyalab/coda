/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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
 * Specification of the Venus User abstraction.
 *
 */

#ifndef _VENUS_USER_H_
#define _VENUS_USER_H_ 1

/* Forward declarations. */
class userent;
class user_iterator;
class srvent;

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <rpc2/rpc2.h>
#ifdef __cplusplus
}
#endif

/* interfaces */
#include <auth2.h>

/* from util */
#include <olist.h>

/* from venus */
#include "comm.h"
#include "venus.private.h"

class userent {
    friend void UserInit();
    friend void PutUser(userent **);
    friend void UserPrint(int);
    friend int AuthorizedUser(uid_t thisUser);
    friend class user_iterator;
    friend class fsdb;
    friend class Realm; /* ~Realm, ResetTransient, GetUser, NewUserToken */

    /* The user list. */
    static olist *usertab;
    static uid_t PrimaryUser;

    /* Transient members. */
    olink tblhandle;
    RealmId realmid;
    uid_t uid;
    int tokensvalid;
    int told_you_so;
    SecretToken secret;
    ClearToken clear;
    int waitforever : 1;

    long DemandHoardWalkTime; /* time of last demand hoard walk for this user */

    /* Constructors, destructors, and private utility routines. */
    userent(RealmId realmid, uid_t userid);
    userent(userent &); /* not supported! */
    int operator=(userent &); /* not supported! */
    ~userent();

public:
    long SetTokens(SecretToken *, ClearToken *);
    long GetTokens(SecretToken *, ClearToken *);
    int TokensValid();
    void CheckTokenExpiry();
    void Invalidate();
    void Reset();
    int CheckFetchPartialSupport(RPC2_Handle *cid, srvent *sv, int *retry_cnt);
    int Connect(RPC2_Handle *, int *, struct in_addr *);
    int GetWaitForever();
    void SetWaitForever(int);

    uid_t GetUid() { return (uid); }

    void print();
    void print(FILE *);
    void print(int);
};

class user_iterator : public olist_iterator {
public:
    user_iterator();
    userent *operator()();
};

/*  *****  Functions/Routines  *****  */

/* user.c */
void UserInit();
void PutUser(userent **);
void UserPrint();
void UserPrint(FILE *);
void UserPrint(int);
int AuthorizedUser(uid_t);
int ConsoleUser(uid_t user);

/* user_daemon.c */
void USERD_Init(void);
void UserDaemon(void);

#endif /* _VENUS_USER_H_ */
