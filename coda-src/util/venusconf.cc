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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef __cplusplus
}
#endif

#include "venusconf.h"
#include "dlist.h"

VenusConf::~VenusConf()
{
    dlist_iterator next(on_off_pairs_list);
    on_off_pair *curr_pair = NULL;

    while (curr_pair = (on_off_pair *)next()) {
        delete curr_pair;
    }
}

int VenusConf::get_int_value(const char *key)
{
    const char *value = get_value(key);
    CODA_ASSERT(value);
    return atoi(value);
}

const char *VenusConf::get_string_value(const char *key)
{
    return get_value(key);
}

bool VenusConf::get_bool_value(const char *key)
{
    return (get_int_value(key) ? 1 : 0);
}

int VenusConf::add_on_off_pair(const char *on_key, const char *off_key,
                               bool on_value)
{
    if (has_key(unalias_key(on_key)) || has_key(unalias_key(off_key))) {
        return EEXIST;
    }

    add(on_key, on_value ? "1" : "0");
    add(off_key, on_value ? "0" : "1");

    on_off_pairs_list.insert((dlink *)new on_off_pair(on_key, off_key));
}

void VenusConf::set(const char *key, const char *value)
{
    VenusConf::on_off_pair *pair = find_on_off_pair(key);

    if (pair) {
        bool bool_val = atoi(value) ? true : false;
        StringKeyValueStore::set(pair->on_val, bool_val ? "1" : "0");
        StringKeyValueStore::set(pair->off_val, bool_val ? "0" : "1");
    } else {
        StringKeyValueStore::set(key, value);
    }
}

#ifndef itoa
static char *itoa(int value, char *str, int base)
{
    sprintf(str, "%d", value);
    return str;
}
#endif

void VenusConf::load_default_config()
{
    char tmp[256];
    static bool already_loaded = false;

    if (already_loaded)
        return;
    already_loaded = true;

    add("cachesize", MIN_CS);
    add_int("cacheblocks", 0);
    add_int("cachefiles", 0);
    add("cachechunkblocksize", "32KB");
    add("wholefilemaxsize", "50MB");
    add("wholefileminsize", "4MB");
    add("wholefilemaxstall", "10");
    add("partialcachefilesratio", "1");
    add("cachedir", DFLT_CD);
    add("checkpointdir", "/usr/coda/spool");
    add("logfile", DFLT_LOGFILE);
    add("errorlog", DFLT_ERRLOG);
    add("kerneldevice", "/dev/cfs0,/dev/coda/0");
    add_int("mapprivate", 0);
    add("marinersocket", "/usr/coda/spool/mariner");
    add_int("masquerade_port", 0);
    add_int("allow_backfetch", 0);
    add("mountpoint", DFLT_VR);
    add_int("primaryuser", UNSET_PRIMARYUSER);
    add("realmtab", "/etc/coda/realms");
    add("rvm_log", "/usr/coda/LOG");
    add("rvm_data", "/usr/coda/DATA");
    add_int("RPC2_timeout", DFLT_TO);
    add_int("RPC2_retries", DFLT_RT);
    add_int("serverprobe", 150);
    add_int("reintegration_age", 0);
    add_int("reintegration_time", 15);
    add_int("dontuservm", 0);
    add_int("cml_entries", 0);
    add_int("hoard_entries", 0);
    add("pid_file", DFLT_PIDFILE);
    add("run_control_file", DFLT_CTRLFILE);
    add("asrlauncher_path", "");
    add("asrpolicy_path", "");
    add_int("validateattrs", 15);
    add_int("isr", 0);
    add_on_off_pair("codafs", "no-codafs", true);
    add_on_off_pair("9pfs", "no-9pfs", true);
    add_on_off_pair("codatunnel", "no-codatunnel", true);
    add_int("onlytcp", 0);
    add_int("detect_reintegration_retry", 1);
    add("checkpointformat", "newc");

    //Newly added
    add("initmetadata", "0");
    add("loglevel", "0");
    add("rpc2loglevel", "0");
    add("lwploglevel", "0");
    add("rdstrace", "0");
    add("copmodes", "6");
    add_int("maxworkers", UNSET_MAXWORKERS);
    add_int("maxcbservers", UNSET_MAXCBSERVERS);
    add_int("maxprefetchers", UNSET_MAXWORKERS);
    add_int("sftp_windowsize", UNSET_WS);
    add_int("sftp_sendahead", UNSET_SA);
    add_int("sftp_ackpoint", UNSET_AP);
    add_int("sftp_packetsize", UNSET_PS);
    add_int("rvmtype", UNSET);
    add_int("rvm_log_size", UNSET_VLDS);
    add_int("rvm_data_size", UNSET_VDDS);
    add_int("rds_chunk_size", UNSET_RDSCS);
    add_int("rds_list_size", UNSET_RDSNL);
    add("log_optimization", "1");

    add_int("swt", UNSET_SWT);
    add_int("mwt", UNSET_MWT);
    add_int("ssf", UNSET_SSF);
    add_on_off_pair("von", "no-voff", false);
    add_on_off_pair("vmon", "vmoff", false);
    add("SearchForNOreFind", "0");
    add("noasr", "0");
    add("novcb", "0");
    add("nowalk", "0");
#if defined(HAVE_SYS_UN_H) && !defined(__CYGWIN32__)
    add_on_off_pair("MarinerTcp", "noMarinerTcp", false);
#else
    add_on_off_pair("MarinerTcp", "noMarinerTcp", true);
#endif
    add("allow-reattach", "0");
    add("nofork", "0");
}

void VenusConf::configure_cmdline_options()
{
    static bool already_configured = false;

    if (already_configured)
        return;
    already_configured = true;

    add_key_alias("cachesize", "-c");
    add_key_alias("cachefiles", "-cf");
    add_key_alias("cachechunkblocksize", "-ccbs");
    add_key_alias("wholefilemaxsize", "-wfmax");
    add_key_alias("wholefileminsize", "-wfmin");
    add_key_alias("wholefilemaxstall", "-wfstall");
    add_key_alias("partialcachefilesratio", "-pcfr");
    add_key_alias("initmetadata", "-init");
    add_key_alias("cachedir", "-f");
    add_key_alias("checkpointdir", "-spooldir");
    add_key_alias("errorlog", "-console");
    add_key_alias("kerneldevice", "-k");
    add_key_alias("mapprivate", "-mapprivate");
    add_key_alias("primaryuser", "-primaryuser");
    add_key_alias("rvm_log", "-vld");
    add_key_alias("rvm_log_size", "-vlds");
    add_key_alias("rvm_data", "-vdd");
    add_key_alias("rvm_data_size", "-vdds");
    add_key_alias("RPC2_timeout", "-timeout");
    add_key_alias("RPC2_retries", "-retries");
    add_key_alias("cml_entries", "-mles");
    add_key_alias("hoard_entries", "-hdbes");
    add_key_alias("codafs", "-codafs");
    add_key_alias("no-codafs", "-no-codafs");
    add_key_alias("9pfs", "-9pfs");
    add_key_alias("no-9pfs", "-no-9pfs");
    add_key_alias("codatunnel", "-codatunnel");
    add_key_alias("no-codatunnel", "-no-codatunnel");
    add_key_alias("onlytcp", "-onlytcp");
    add_key_alias("loglevel", "-d");
    add_key_alias("rpc2loglevel", "-rpcdebug");
    add_key_alias("lwploglevel", "-lwpdebug");
    add_key_alias("rdstrace", "-rdstrace");
    add_key_alias("copmodes", "-m");
    add_key_alias("maxworkers", "-maxworkers");
    add_key_alias("maxcbservers", "-maxcbservers");
    add_key_alias("maxprefetchers", "-maxprefetchers");
    add_key_alias("sftp_windowsize", "-ws");
    add_key_alias("sftp_sendahead", "-sa");
    add_key_alias("sftp_ackpoint", "-ap");
    add_key_alias("sftp_packetsize", "-ps");
    add_key_alias("rvmtype", "-rvmt");
    add_key_alias("rds_chunk_size", "-rdscs");
    add_key_alias("rds_list_size", "-rdsnl");
    add_key_alias("log_optimization", "-logopts");

    add_key_alias("swt", "-swt");
    add_key_alias("mwt", "-mwt");
    add_key_alias("ssf", "-ssf");
    add_key_alias("von", "-von");
    add_key_alias("voff", "-voff");
    add_key_alias("vmon", "-vmon");
    add_key_alias("vmoff", "-vmoff");
    add_key_alias("SearchForNOreFind", "-SearchForNOreFind");
    add_key_alias("noasr", "-noasr");
    add_key_alias("novcb", "-novcb");
    add_key_alias("nowalk", "-nowalk");
    add_key_alias("MarinerTcp", "-MarinerTcp");
    add_key_alias("noMarinerTcp", "-noMarinerTcp");
    add_key_alias("allow-reattach", "-allow-reattach");
    add_key_alias("masquerade", "-masquerade");
    add_key_alias("nomasquerade", "-nomasquerade");
    add_key_alias("nofork", "-nofork");
}

void VenusConf::set_int(const char *key, int value)
{
    char buffer[256];
    itoa(value, buffer, 10);
    set(key, buffer);
}

int VenusConf::add_int(const char *key, int value)
{
    char buffer[256];
    itoa(value, buffer, 10);
    return add(key, buffer);
}

void VenusConf::handle_relative_path(const char *key)
{
    const char *TmpChar  = get_value(key);
    const char *cachedir = get_value("cachedir");
    if (*TmpChar != '/') {
        char *tmp = (char *)malloc(strlen(cachedir) + strlen(TmpChar) + 2);
        CODA_ASSERT(tmp);
        sprintf(tmp, "%s/%s", cachedir, TmpChar);
        set(key, tmp);
        free(tmp);
    }
}

VenusConf::on_off_pair *VenusConf::find_on_off_pair(const char *key)
{
    dlist_iterator next(on_off_pairs_list);
    on_off_pair *curr_pair = NULL;

    while (curr_pair = (on_off_pair *)next()) {
        if (curr_pair->is_in_pair(key))
            return curr_pair;
    }

    return NULL;
}

bool VenusConf::on_off_pair::is_in_pair(const char *val)
{
    if (strcmp(val, on_val) == 0)
        return true;
    if (strcmp(val, off_val) == 0)
        return true;
    return false;
}

VenusConf::on_off_pair::on_off_pair(const char *on, const char *off)
    : dlink()
{
    on_val  = strdup(on);
    off_val = strdup(off);
}

VenusConf::on_off_pair::~on_off_pair()
{
    free((void *)on_val);
    free((void *)off_val);
}