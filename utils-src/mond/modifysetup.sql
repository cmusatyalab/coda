alter table session_stats drop (hoard_h_count);
alter table session_stats drop (hoard_h_blocks);
alter table session_stats drop (hoard_m_count);
alter table session_stats drop (hoard_m_blocks);
alter table session_stats drop (hoard_ns_count);
alter table session_stats drop (hoard_ns_blocks);
alter table session_stats drop (nonhoard_h_count);
alter table session_stats drop (nonhoard_h_blocks);
alter table session_stats drop (nonhoard_m_count);
alter table session_stats drop (nonhoard_m_blocks);
alter table session_stats drop (nonhoard_ns_count);
alter table session_stats drop (nonhoard_ns_blocks);
alter table session_stats drop (cache_ns_count);
alter table session_stats drop (cache_ns_blocks);

create table cache_stats (
	session_index integer, 
	h_a_h_count integer,
	h_a_h_blocks integer,
	h_a_m_count integer,
	h_a_m_blocks integer,
	h_a_ns_count integer,
	h_a_ns_blocks integer,
	h_d_h_count integer,
	h_d_h_blocks integer,
	h_d_m_count integer,
	h_d_m_blocks integer,
	h_d_ns_count integer,
	h_d_ns_blocks integer,
	nh_a_h_count integer,
	nh_a_h_blocks integer,
	nh_a_m_count integer,
	nh_a_m_blocks integer,
	nh_a_ns_count integer,
	nh_a_ns_blocks integer,
	nh_d_h_count integer,
	nh_d_h_blocks integer,
	nh_d_m_count integer,
	nh_d_m_blocks integer,
	nh_d_ns_count integer,
	nh_d_ns_blocks integer,
	uh_a_h_count integer,
	uh_a_h_blocks integer,
	uh_a_m_count integer,
	uh_a_m_blocks integer,
	uh_a_ns_count integer,
	uh_a_ns_blocks integer,
	uh_d_h_count integer,
	uh_d_h_blocks integer,
	uh_d_m_count integer,
	uh_d_m_blocks integer,
	uh_d_ns_count integer,
	uh_d_ns_blocks integer		
);

create table advice_stats (
	venus_index integer,
	time integer,
	uid integer,
	not_enabled integer,
	not_valid integer,
	outstanding integer,
	asr_not_allowed integer,
	asr_interval integer,
	volume_null integer,
	total_attempts integer,
	pcm_successes integer,
	pcm_failures integer,
	hwa_successes integer,
	hwa_failures integer,
	dm_successes integer,
	dm_failures integer,
	r_successes integer,
	r_failures integer,
	rp_successes integer,
	rp_failures integer,
	iasr_successes integer,
	iasr_failures integer,
	lc_successes integer,
	lc_failures integer,
	wcm_successes integer,
	wcm_failures integer,
	rpc2_success integer,
	rpc2_connbusy integer,
	rpc2_fail integer,
	rpc2_noconnection integer,
	rpc2_timeout integer,
	rpc2_dead integer,
	rpc2_othererrors integer
);

create table advice_discomiss (
	hostid integer,
	uid integer,
	venus_major integer,
	venus_minor integer,
	admon_version_num integer,
	adsrv_version integer,
	admon_version integer,
	q_version_num integer,
	disco_time integer,
	cachemiss_time integer,
	fid_volume integer,
	fid_vnode integer,
	fid_uniquifier integer,
	practice_session integer,
	expected_affect integer,
	comments integer
);

create table advice_reconn (
	hostid integer,
	uid integer,
	venus_major integer,
	venus_minor integer,
	admon_version_num integer,
	adsrv_version integer,
	admon_version integer,
	q_version_num integer,
	volume_id integer,
	cml_count integer,
	disco_time integer,
	reconn_time integer,
	demand_walk_time integer,
	num_reboots integer,
	num_cache_hits integer,
	num_cache_misses integer,
	num_unique_hits integer,
	num_unique_misses integer,
	num_objs_not_refd integer,
	aware_of_disco integer,
	voluntary_disco integer,
	practice_disco integer,
	codacon integer,
	sluggish integer,
	observed_miss integer,
	known_comments integer,
	suspected_comments integer,
	no_preparation integer,
	hoardwalk integer,
	num_pseudo_disco integer,
	num_practice_disco integer,
	prep_comments integer,
	overall_impression integer,
	final_comments integer
);

create table iot_info (
        venus_index integer,
        app_name char(10),
        tid integer,
        res_opt integer,
        elapsed_time integer,
        readset_size integer,
        writeset_size integer,
        readvol_num integer,
        writevol_num integer,
        validation integer,
        invalid_size integer,
        backup_obj_num integer,
        life_cycle integer,
        pred_num integer,
        succ_num integer
);

create table iot_stats (
        venus_index integer,
        time integer,
        max_elapsed_time integer,
        avg_elapsed_time integer,
        max_readset_size integer,
        avg_readset_size integer,
        max_writeset_size integer,
        avg_writeset_size integer,
        max_readvol_num integer,
        avg_readvol_num integer,
        max_writevol_num integer,
        avg_writevol_num integer,
        commited integer,
        pending integer,
        resolved integer,
        repaired integer
);
 
create table subtree_stats (
        venus_index integer,
        time integer,
        subtree_num integer,
        max_subtree_size integer,
        avg_subtree_size integer,
        max_subtree_hgt integer,
        avg_subtree_hgt integer,
        max_mutation_num integer,
        avg_mutation_num integer
);

create table repair_stats (
        venus_index integer,
        time integer,
        session_num integer,
        commit_num integer,
        abort_num integer,
        check_num integer,
        preserve_num integer,
        discard_num integer,
        remove_num integer,
        global_view_num integer,
        local_view_num integer,
        keep_local_num integer,
        list_local_num integer,
        new_command1_num integer,
        new_command2_num integer,
        new_command3_num integer,
        new_command4_num integer,
        new_command5_num integer,
        new_command6_num integer,
        new_command7_num integer,
        new_command8_num integer,
        rep_mutation_num integer,
        miss_target_num integer,
        miss_parent_num integer,
        acl_deny_num integer,
        update_update_num integer,
        name_name_num integer,
        remove_update_num integer
);

create table rws_stats (
        venus_index integer,
        time integer,
        volume_id integer,
        rw_sharing_count integer,
        disc_read_count integer,
        disc_duration integer
);

create table venus_init_insts (
	instance_index	serial(1),
	host		integer,
	init_time	integer
);

create table vcb_stats (
	venus_index		integer,
	time			integer,
	volume			integer,
	acquires		integer,
	acquireobjs		integer,
	acquirechecked		integer,
	acquirefailed		integer,
	acquirenoobjfails	integer,
	validates		integer,
	validateobjs		integer,
	failedvalidates		integer,
	failedvalidateobjs	integer,
	breaks			integer,
	breakobjs		integer,
	breakvolonly		integer,
	breakrefs		integer,
	clears			integer,
	clearobjs		integer,
	clearrefs		integer,
	nostamp			integer,
	nostampobjs		integer
);
