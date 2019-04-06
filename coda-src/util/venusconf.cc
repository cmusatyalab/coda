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
    return atoi(get_value(key));
}

const char *VenusConf::get_string_value(const char *key)
{
    return get_value(key);
}

bool VenusConf::get_bool_value(const char *key)
{
    return (atoi(get_value(key)) ? 1 : 0);
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
static char *itoa(int value, char *str, int base = 10)
{
    sprintf(str, "%d", value);
    return str;
}
#endif

void VenusConf::load_default_config()
{
    char tmp[256];

    add("cachesize", MIN_CS);
    add("cacheblocks", itoa(0, tmp));
    add("cachefiles", itoa(0, tmp));
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
    add("mapprivate", itoa(0, tmp));
    add("marinersocket", "/usr/coda/spool/mariner");
    add("masquerade_port", itoa(0, tmp));
    add("allow_backfetch", itoa(0, tmp));
    add("mountpoint", DFLT_VR);
    add("primaryuser", itoa(UNSET_PRIMARYUSER, tmp));
    add("realmtab", "/etc/coda/realms");
    add("rvm_log", "/usr/coda/LOG");
    add("rvm_data", "/usr/coda/DATA");
    add("RPC2_timeout", itoa(DFLT_TO, tmp));
    add("RPC2_retries", itoa(DFLT_RT, tmp));
    add("serverprobe", itoa(150, tmp));
    add("reintegration_age", itoa(0, tmp));
    add("reintegration_time", itoa(15, tmp));
    add("dontuservm", itoa(0, tmp));
    add("cml_entries", itoa(0, tmp));
    add("hoard_entries", itoa(0, tmp));
    add("pid_file", DFLT_PIDFILE);
    add("run_control_file", DFLT_CTRLFILE);
    add("asrlauncher_path", "");
    add("asrpolicy_path", "");
    add("validateattrs", itoa(15, tmp));
    add("isr", itoa(0, tmp));
    add_on_off_pair("codafs", "no-codafs", true);
    add_on_off_pair("9pfs", "no-9pfs", true);
    add_on_off_pair("codatunnel", "no-codatunnel", true);
    add("onlytcp", itoa(0, tmp));
    add("detect_reintegration_retry", itoa(1, tmp));
    add("checkpointformat", "newc");

    //Newly added
    add("initmetadata", "0");
    add("loglevel", "0");
    add("rpc2loglevel", "0");
    add("lwploglevel", "0");
    add("rdstrace", "0");
    add("copmodes", "6");
    add("maxworkers", itoa(UNSET_MAXWORKERS, tmp));
    add("maxcbservers", itoa(UNSET_MAXCBSERVERS, tmp));
    add("maxprefetchers", itoa(UNSET_MAXWORKERS, tmp));
    add("sftp_windowsize", itoa(UNSET_WS, tmp));
    add("sftp_sendahead", itoa(UNSET_SA, tmp));
    add("sftp_ackpoint", itoa(UNSET_AP, tmp));
    add("sftp_packetsize", itoa(UNSET_PS, tmp));
    add("rvmtype", itoa(UNSET, tmp));
    add("rvm_log_size", itoa(UNSET_VLDS, tmp));
    add("rvm_data_size", itoa(UNSET_VDDS, tmp));
    add("rds_chunk_size", itoa(UNSET_RDSCS, tmp));
    add("rds_list_size", itoa(UNSET_RDSNL, tmp));
    add("log_optimization", "1");

    add("swt", itoa(UNSET_SWT, tmp));
    add("mwt", itoa(UNSET_MWT, tmp));
    add("ssf", itoa(UNSET_SSF, tmp));
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

void VenusConf::add_cmd_line_to_config_params_mapping()
{
    static bool map_added = false;

    if (map_added)
        return;
    map_added = true;

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