#ifndef _AUTH2_COMMON_
#define _AUTH2_COMMON_

/* per-connection info */
struct UserInfo
{
        int ViceId;     /* from NewConnection */
        int HasQuit;    /* TRUE iff Quit() was received on this connection */
        PRS_InternalCPS *UserCPS;
        int LastUsed;   /* timestamped at each RPC call; for gc'ing */
};

#endif
