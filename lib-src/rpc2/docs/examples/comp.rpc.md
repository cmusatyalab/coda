---
title: comp.rpc
---
``` rpc2
/*
RPC interface specification for a trivial computational subsystem.
Finds squares and cubes of given numbers.
*/

Server Prefix "S";
Subsystem  "comp";

#define COMPSUBSYSID    200 /* The subsysid for comp subsystem */

#define COMPSUCCESS 1
#define COMPFAILED 2

CompNewConn (IN RPC2_Integer seType, IN RPC2_Integer secLevel,
             IN RPC2_Integer encType, IN RPC2_CountedBS cIdent) NEW_CONNECTION;

CompSquare (IN RPC2_Integer X); /* returns square of x */

CompCube (IN RPC2_Integer X);   /* returns cube of x */

CompAge();  /* returns the age of this connection in  seconds */

CompQuit();
```
