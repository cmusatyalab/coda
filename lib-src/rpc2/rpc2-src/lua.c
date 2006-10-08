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
#include <stdio.h>

#include "lua.h"
#include "lauxlib.h"

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
	else if (lua_isstring(L, i))
	    fprintf(rpc2_logfile, "%s", lua_tostring(L, i));
	else if (lua_isboolean(L, i))
	    fprintf(rpc2_logfile, "%s", lua_toboolean(L, i) ? "true" : "false");
	else
	    fprintf(rpc2_logfile, "%s:%p", luaL_typename(L, i),
		    lua_topointer(L, i));
    }
    fprintf(rpc2_logfile, "\n");
    fflush(rpc2_logfile);
    return 0;
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
    lua_register(L, "print", print);

    if (luaL_dofile(L, lua_script)) {
	badscript();
	return;
    }
    if (RPC2_DebugLevel)
	fprintf(rpc2_logfile, "-- loaded %s --\n", lua_script);
}

void LUA_rtt_update(struct HEntry *he, uint32_t rtt, uint32_t tx, uint32_t rx)
{
    if (setup_function("rtt_update")) return;
    if (push_hosttable(he)) return;
    lua_pushinteger(L, (lua_Integer)rtt);
    lua_pushinteger(L, (lua_Integer)tx);
    lua_pushinteger(L, (lua_Integer)rx);
    if (lua_pcall(L, 4, 0, 0)) badscript();
}

int LUA_rtt_getrto(struct HEntry *he, uint32_t tx, uint32_t rx)
{
    lua_Integer rtt = 0;

    if (setup_function("rtt_getrto")) return 0;
    if (push_hosttable(he)) return 0;
    lua_pushinteger(L, (lua_Integer)tx);
    lua_pushinteger(L, (lua_Integer)rx);
    if (lua_pcall(L, 3, 1, 0)) { badscript(); return 0; }

    rtt = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return (uint32_t)rtt;
}

int LUA_rtt_retryinterval(struct HEntry *he, uint32_t n, uint32_t tx, uint32_t rx)
{
    lua_Integer rtt = 0;

    if (setup_function("rtt_retryinterval")) return 0;
    if (push_hosttable(he)) return 0;
    lua_pushinteger(L, (lua_Integer)n);
    lua_pushinteger(L, (lua_Integer)tx);
    lua_pushinteger(L, (lua_Integer)rx);
    if (lua_pcall(L, 4, 1, 0)) { badscript(); return 0; }

    rtt = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return (uint32_t)rtt;
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
#endif

