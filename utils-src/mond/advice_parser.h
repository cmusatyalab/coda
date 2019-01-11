#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/
#endif /*_BLURB_*/

#ifndef _ADVICE_PARSER_H_
#define _ADVICE_PARSER_H_

typedef struct {
    int hostid;
    int uid;
    int venusmajorversion;
    int venusminorversion;
    int advicemonitorversion;
    int adsrv_version;
    int admon_version;
    int q_version;
    int disco_time;
    int cachemiss_time;
    int fid_volume;
    int fid_vnode;
    int fid_uniquifier;
    int practice_session;
    int expected_affect;
    int comment_id;
} DiscoMissQ;

typedef struct {
    int hostid;
    int uid;
    int venusmajorversion;
    int venusminorversion;
    int advicemonitorversion;
    int adsrv_version;
    int admon_version;
    int q_version;
    int volume_id;
    int cml_count;
    int disco_time;
    int reconn_time;
    int walk_time;
    int reboots;
    int hits;
    int misses;
    int unique_hits;
    int unique_misses;
    int not_referenced;
    int awareofdisco;
    int voluntary;
    int practicedisco;
    int codacon;
    int sluggish;
    int observed_miss;
    int known_other_comments;
    int suspected_other_comments;
    int nopreparation;
    int hoard_walk;
    int num_pseudo_disco;
    int num_practice_disco;
    int prep_comments;
    int overall_impression;
    int final_comments;
} ReconnQ;

extern int ParseDisconQfile(char *, DiscoMissQ *);
extern int ParseReconnQfile(char *, ReconnQ *);

#endif _ADVICE_PARSER_H_
