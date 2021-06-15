---
title: rcat.rpc
---
``` rpc2
Subsystem  "rcat";

#define RCAT_FAILED 1 /* return code for the server */
#define RCAT_SUBSYSID 200
#define RCAT_PORTAL 4000

Fetch_Remote_File(IN RPC2_String Filename, IN OUT SE_Descriptor Sed);
```
