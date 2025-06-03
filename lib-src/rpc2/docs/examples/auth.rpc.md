---
title: auth.rpc
---
``` rpc2
/*
RPC interface specification for a trivial authentication subsystem.
This is only an example: all it does is name to id and id to name conversions.
*/

Server Prefix "S";
Subsystem  "auth";

/*
Internet port number; note that this is really not part of a specific
subsystem, but is part of a server; we should really have a  separate ex.h
file with this constant.  I am being lazy here
*/
#define AUTHPORTAL  5000

#define AUTHSUBSYSID    100 /* The subsysid for auth subsystem */

/*
Return codes from auth server
*/
#define AUTHSUCCESS 0
#define AUTHFAILED  1


typedef RPC2_Byte PathName[1024];

typedef RPC2_Struct
{
    RPC2_Integer GroupId;
    PathName HomeDir;
} AuthInfo;

AuthNewConn (IN RPC2_Integer seType, IN RPC2_Integer secLevel,
             IN RPC2_Integer encType, IN RPC2_CountedBS cIdent) NEW_CONNECTION;

AuthUserId (IN RPC2_String Username, OUT RPC2_Integer UserId);
        /* Returns AUTHSUCCESS or AUTHFAILED */

AuthUserName (IN RPC2_Integer UserId, IN OUT RPC2_BoundedBS Username);
         /* Returns AUTHSUCCESS or AUTHFAILED */

AuthUserInfo (IN RPC2_Integer UserId, OUT AuthInfo UInfo);
        /* Returns AUTHSUCCESS or AUTHFAILED */

AuthQuit();
```
