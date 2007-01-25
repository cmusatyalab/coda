/* BLURB lgpl

			   Coda File System
			      Release 6

	    Copyright (c) 2006 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
#*/

#include "rpc2.private.h"

#ifdef USE_LUA

#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static char *lua_script = "/etc/rpc2.lua";

/* Keep the state in a global variable, that way the rest of RPC2 doesn't have
 * to know anything about lua */
static lua_State *L;

/********************************************************************
 * exported functions for scripts
 ********************************************************************/
static int print(lua_State *L)
{
    int i, n = lua_gettop(L);

    for(i = 1; i <= n; i++)
    {
	if (i > 1)
	    fprintf(rpc2_logfile, "\t");
	if (lua_isnil(L, i))
	    fprintf(rpc2_logfile, "nil");
	else if (lua_isboolean(L, i))
	    fprintf(rpc2_logfile, "%s", lua_toboolean(L, i) ? "true" : "false");
	else if (lua_isstring(L, i))
	    fprintf(rpc2_logfile, "%s", lua_tostring(L, i));
	else if (luaL_callmeta(L, i, "__tostring")) {
	    fprintf(rpc2_logfile, "%s", lua_tostring(L, -1));
	    lua_pop(L, 1);
	} else
	    fprintf(rpc2_logfile, "%s:%p", luaL_typename(L, i),
		    lua_topointer(L, i));
    }
    fprintf(rpc2_logfile, "\n");
    fflush(rpc2_logfile);
    return 0;
}


/********************************************************************
 * timeval object
 *
 * By making the following 3 functions non-static, this could also
 * be part of a separate library.
 ********************************************************************/
#define RPC2_TIMEVAL "RPC2.timeval" /* type identifier */

static int l2c_timeval_init(lua_State *L);
static int l2c_pushtimeval(lua_State *L, struct timeval *tv);
static void l2c_totimeval(lua_State *L, int index, struct timeval *tv);

static int timeval_eq(lua_State *L)
{
    struct timeval a, b;
    l2c_totimeval(L, 1, &a);
    l2c_totimeval(L, 2, &b);
    lua_pushboolean(L, (a.tv_sec == b.tv_sec) && (a.tv_usec == b.tv_usec));
    return 1;
}

static int timeval_le(lua_State *L)
{
    struct timeval a, b;
    l2c_totimeval(L, 1, &a);
    l2c_totimeval(L, 2, &b);
    lua_pushboolean(L, (a.tv_sec < b.tv_sec) ||
		    ((a.tv_sec == b.tv_sec) && (a.tv_usec <= b.tv_usec)));
    return 1;
}

static int timeval_lt(lua_State *L)
{
    struct timeval a, b;
    l2c_totimeval(L, 1, &a);
    l2c_totimeval(L, 2, &b);
    lua_pushboolean(L, (a.tv_sec < b.tv_sec) ||
		    ((a.tv_sec == b.tv_sec) && (a.tv_usec < b.tv_usec)));
    return 1;
}

static int timeval_add(lua_State *L)
{
    struct timeval a, b, res;
    l2c_totimeval(L, 1, &a);
    l2c_totimeval(L, 2, &b);

    res.tv_sec = a.tv_sec + b.tv_sec;
    res.tv_usec = a.tv_usec + b.tv_usec;
    if (res.tv_usec >= 1000000) { res.tv_usec -= 1000000; res.tv_sec++; }
    l2c_pushtimeval(L, &res);
    return 1;
}

static int timeval_sub(lua_State *L)
{
    struct timeval a, b, res;
    l2c_totimeval(L, 1, &a);
    l2c_totimeval(L, 2, &b);

    res.tv_sec = a.tv_sec - b.tv_sec;
    res.tv_usec = a.tv_usec - b.tv_usec;
    if (res.tv_usec < 0) { res.tv_usec += 1000000; res.tv_sec--; }
    l2c_pushtimeval(L, &res);
    return 1;
}

static int timeval_umn(lua_State *L)
{
    struct timeval a, res;
    l2c_totimeval(L, 1, &a);

    res.tv_sec = -a.tv_sec - 1;
    res.tv_usec = 1000000 - a.tv_usec;
    if (res.tv_usec == 1000000) { res.tv_usec -= 1000000; res.tv_sec++; }
    l2c_pushtimeval(L, &res);
    return 1;
}

static int timeval_mul(lua_State *L)
{
    struct timeval tv;
    lua_Number x;
    struct timeval res;

    if (lua_isnumber(L, 2)) {
	l2c_totimeval(L, 1, &tv);
	x = lua_tonumber(L, 2);
    } else {
	l2c_totimeval(L, 2, &tv);
	x = luaL_checknumber(L, 1);
    }

    res.tv_sec = tv.tv_sec * x;
    res.tv_usec = tv.tv_usec * x;
    while (res.tv_usec < 0)	   { res.tv_usec += 1000000; res.tv_sec--; }
    while (res.tv_usec >= 1000000) { res.tv_usec -= 1000000; res.tv_sec++; }
    l2c_pushtimeval(L, &res);
    return 1;
}

static int timeval_div(lua_State *L)
{
    struct timeval tv;
    lua_Number x, val;
    lua_Integer y;
    struct timeval res;

    if (lua_isnumber(L, 1)) {
	l2c_totimeval(L, 2, &tv);
	x = luaL_checknumber(L, 1);
	val = (lua_Number)tv.tv_sec + (lua_Number)tv.tv_usec / 1000000;
	lua_pushnumber(L, x / val);
    } else {
	l2c_totimeval(L, 1, &tv);
	y = luaL_checkinteger(L, 2);
	res.tv_usec = ((tv.tv_sec % y) * 1000000 + tv.tv_usec) / y;
	res.tv_sec = tv.tv_sec / y;
	while (res.tv_usec < 0)	       { res.tv_usec += 1000000; res.tv_sec--; }
	while (res.tv_usec >= 1000000) { res.tv_usec -= 1000000; res.tv_sec++; }
	l2c_pushtimeval(L, &res);
    }
    return 1;
}

static int timeval_tostring(lua_State *L)
{
    struct timeval tv;
    char buf[20];
    l2c_totimeval(L, 1, &tv);

    snprintf(buf, 19, "%ld.%06lus", (long)tv.tv_sec, (unsigned long)tv.tv_usec);
    lua_pushstring(L, buf);
    return 1;
}

static int timeval_tonumber(lua_State *L)
{
    struct timeval tv;
    l2c_totimeval(L, 1, &tv);
    lua_Number val = (lua_Number)tv.tv_sec + (lua_Number)tv.tv_usec / 1000000;
    lua_pushnumber(L, val);
    return 1;
}

static const struct luaL_reg timeval_m [] = {
    { "__eq", timeval_eq },
    { "__le", timeval_le },
    { "__lt", timeval_lt },
    { "__add", timeval_add },
    { "__sub", timeval_sub },
    { "__umn", timeval_umn },
    { "__mul", timeval_mul },
    { "__div", timeval_div },
    { "__tostring", timeval_tostring },
    { "__call", timeval_tonumber },
    { NULL, NULL }
};

static int timeval_new(lua_State *L)
{
    struct timeval tv;

    if (!lua_gettop(L) || lua_isnil(L, 1))
	gettimeofday(&tv, NULL);
    else
	l2c_totimeval(L, 1, &tv);

    l2c_pushtimeval(L, &tv);
    return 1;
}

static int l2c_pushtimeval(lua_State *L, struct timeval *tv)
{
    struct timeval *ud = lua_newuserdata(L, sizeof(struct timeval));
    ud->tv_sec = tv->tv_sec;
    ud->tv_usec = tv->tv_usec;
    luaL_getmetatable(L, RPC2_TIMEVAL);
    lua_setmetatable(L, -2);
    return 1;
}

/* similar to checkudata, but doesn't raise an error but returns NULL */
void *l2c_getudata(lua_State *L, int index, char *type)
{
    void *p = lua_touserdata(L, index);
    if (p == NULL || !lua_getmetatable(L, index))
	return NULL;

    lua_getfield(L, LUA_REGISTRYINDEX, type);
    if (!lua_rawequal(L, -1, -2))
	p = NULL;

    lua_pop(L, 2);
    return p;
}

static void l2c_totimeval(lua_State *L, int index, struct timeval *tv)
{
    struct timeval *ud;
    lua_Number val;

    tv->tv_sec = tv->tv_usec = 0;
    if (lua_isnumber(L, index)) {
	val = lua_tonumber(L, index);

	tv->tv_sec = (int)floor(val);
	val -= tv->tv_sec;
	tv->tv_usec = (int)floor(1000000 * val);
	if (tv->tv_usec < 0) { tv->tv_usec += 1000000; tv->tv_sec--; };
    } else if (lua_isuserdata(L, index)) {
	ud = l2c_getudata(L, index, RPC2_TIMEVAL);
	if (ud) {
	    tv->tv_sec = ud->tv_sec;
	    tv->tv_usec = ud->tv_usec;
	}
    }
}

static int l2c_timeval_init(lua_State *L)
{
    luaL_newmetatable(L, RPC2_TIMEVAL);
    luaL_openlib(L, NULL, timeval_m, 0);
    lua_register(L, "time", timeval_new);
    return 1;
}


/********************************************************************
 * internal helpers
 ********************************************************************/

/* destroy state when we encounter an error during script execution */
static void badscript(void)
{
    fprintf(rpc2_logfile, "-- disabling %s: %s\n",
	    lua_script, lua_tostring(L, -1));
    fflush(rpc2_logfile);
    lua_close(L);
    L = NULL;
}

static int setup_function(const char *func)
{
    if (!L) return -1;

    lua_getglobal(L, func);
    if (!lua_isnil(L, -1))
	return 0;

    lua_pop(L, 1);
    return -1;
}

static int push_hosttable(struct HEntry *he)
{
    char addr[RPC2_ADDRSTRLEN];

    lua_pushlightuserdata(L, he);
    lua_rawget(L, LUA_REGISTRYINDEX);
    if (!lua_isnil(L, -1)) return 0;
    lua_pop(L, 1);

    /* create a new 'host' table */
    lua_newtable(L);

    /* set host.name = he->Addr */
    RPC2_formataddrinfo(he->Addr, addr, RPC2_ADDRSTRLEN);
    lua_pushstring(L, addr);
    lua_setfield(L, -2, "name");

    /* make sure we can find the table again */
    lua_pushlightuserdata(L, he);
    lua_pushvalue(L, -2); /* push host */
    lua_rawset(L, LUA_REGISTRYINDEX);

    /* run custom initializer */
    if (setup_function("rtt_init")) return 0;
    lua_pushvalue(L, -2); /* push host */
    if (lua_pcall(L, 1, 0, 0)) { badscript(); return -1; }

    return 0;
}

/********************************************************************
 * functions called by librpc2
 ********************************************************************/
/* Initialization */
void LUA_init(void)
{
    char *c = getenv("RPC2_LUA_SCRIPT");
    if (c) lua_script = c;
    /* check if the script exists */
    LUA_clocktick();
}

/* cleanup lua state that was associated with a removed hostentry */
void LUA_drop_hosttable(struct HEntry *he)
{
    if (!L) return;
    lua_pushlightuserdata(L, he);
    lua_pushnil(L);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

/* check if the script has been updated or removed */
void LUA_clocktick(void)
{
    static ino_t script_inode = 0;
    static time_t script_mtime = 0;
    struct stat st;
    int rc;

    rc = stat(lua_script, &st);
    if (rc != 0 || !S_ISREG(st.st_mode)) {
	/* was the script removed? */
	if (L) {
	    lua_pushstring(L, "file not found (or not a file)");
	    badscript();
	}
	/* make sure we'll reload if the script was moved away temporarily */
	script_inode = 0;
	script_mtime = 0;
	return;
    }

    if (script_mtime == st.st_mtime && script_inode == st.st_ino) {
	/* do we need to run the gc, or will it run automatically? */
	// if (L) lua_gc(L, LUA_GCSTEP, 10);
	return;
    }

    script_inode = st.st_ino;
    script_mtime = st.st_mtime;

    if (L) lua_close(L);

    L = luaL_newstate();

    /* Load default libraries. Maybe this is a bit too much, we probably
     * really only need math and string. */
    luaL_openlibs(L);

    /* make sure print sends it's output to rpc2_logfile */
    lua_register(L, "print", print);
    l2c_timeval_init(L);

    l2c_pushtimeval(L, &KeepAlive);
    lua_setglobal(L, "RPC2_TIMEOUT");

    lua_pushinteger(L, (lua_Integer)Retry_N);
    lua_setglobal(L, "RPC2_RETRIES");

    if (luaL_dofile(L, lua_script)) {
	badscript();
	return;
    }
    if (RPC2_DebugLevel)
	fprintf(rpc2_logfile, "-- loaded %s --\n", lua_script);
}

void LUA_rtt_update(struct HEntry *he, uint32_t rtt, uint32_t tx, uint32_t rx)
{
    struct timeval tv;
    if (setup_function("rtt_update")) return;
    if (push_hosttable(he)) return;

    tv.tv_sec = rtt / 1000000;
    tv.tv_usec = rtt % 1000000;
    l2c_pushtimeval(L, &tv);

    lua_pushinteger(L, (lua_Integer)tx);
    lua_pushinteger(L, (lua_Integer)rx);
    if (lua_pcall(L, 4, 0, 0)) badscript();
}

int LUA_rtt_getrto(struct HEntry *he, uint32_t tx, uint32_t rx)
{
    struct timeval tv;
    uint32_t rtt;

    if (setup_function("rtt_getrto")) return 0;
    if (push_hosttable(he)) return 0;
    lua_pushinteger(L, (lua_Integer)tx);
    lua_pushinteger(L, (lua_Integer)rx);
    if (lua_pcall(L, 3, 1, 0)) { badscript(); return 0; }

    l2c_totimeval(L, -1, &tv);
    rtt = tv.tv_sec * 1000000 + tv.tv_usec;
    lua_pop(L, 1);
    return rtt;
}

int LUA_rtt_retryinterval(struct HEntry *he, uint32_t n, uint32_t tx, uint32_t rx)
{
    struct timeval tv;
    uint32_t rtt = 0;

    if (setup_function("rtt_retryinterval")) return 0;
    if (push_hosttable(he)) return 0;
    lua_pushinteger(L, (lua_Integer)n);
    lua_pushinteger(L, (lua_Integer)tx);
    lua_pushinteger(L, (lua_Integer)rx);
    if (lua_pcall(L, 4, 1, 0)) { badscript(); return 0; }

    if (!lua_isnil(L, -1)) {
	l2c_totimeval(L, -1, &tv);
	rtt = tv.tv_sec * 1000000 + tv.tv_usec;
    }
    lua_pop(L, 1);
    return rtt;
}

int LUA_rtt_getbandwidth(struct HEntry *he, uint32_t *bw_tx, uint32_t *bw_rx)
{
    if (setup_function("rtt_getbandwidth")) return 0;
    if (push_hosttable(he)) return 0;
    if (lua_pcall(L, 1, 2, 0)) { badscript(); return 0; }

    if (bw_tx) *bw_tx = (uint32_t)lua_tointeger(L, -2);
    if (bw_rx) *bw_rx = (uint32_t)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return 1;
}

int LUA_fail_delay(struct RPC2_addrinfo *Addr, RPC2_PacketBuffer *pb, int out,
		   struct timeval *tv)
{
    char addr[RPC2_ADDRSTRLEN];
    int rc, color;

    if (out) rc = setup_function("fail_delay_tx");
    else     rc = setup_function("fail_delay_rx");
    if (rc) return 0;

    ntohPktColor(pb);
    color = GetPktColor(pb);

    RPC2_formataddrinfo(Addr, addr, RPC2_ADDRSTRLEN);
    lua_pushstring(L, addr);
    lua_pushinteger(L, pb->Prefix.LengthOfPacket);
    lua_pushinteger(L, color);
    if (lua_pcall(L, 3, 2, 0)) { badscript(); return 0; }

    if (!lua_isnil(L, -2)) {
	l2c_totimeval(L, -2, tv); /* delay packet */
	rc = (tv->tv_sec >= 0);
    } else
	rc = -1;		  /* drop packet */

    if (lua_isnumber(L, -1)) { /* not nil, set new color value */
	color = lua_tointeger(L, -1);
	SetPktColor(pb, color);
    }
    lua_pop(L, 2);

    htonPktColor(pb);
    return rc;
}
#endif

