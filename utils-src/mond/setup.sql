create table venus_instances (
	instance_index	serial(1),
	host		integer,
	birth_time	integer
);

create table venus_init_insts (
	instance_index	serial(1),
	host		integer,
	init_time	integer
);

create table venus_names (
	hostid		integer,
	hostname	char(24),
	arch		char(8),
	primary_user	integer
);

create table vice_instances (
	instance_index	serial(1),
	host		integer,
	birth_time	integer
);

create table vice_names (
	hostid		integer,
	hostname	char(24)
);

create table sessions (
	venus_index	integer,
	session		integer,
	volume		integer,
	avsgmem1	integer,
	avsgmem2	integer,
	avsgmem3	integer,
	avsgmem4	integer,
	avsgmem5	integer,
	avsgmem6	integer,
	avsgmem7	integer,
	avsgmem8	integer,
	uid		integer,
	start_time	integer,
	end_time	integer,
	comm_event_time	integer,
	epoch           integer,
	session_index	serial(1)
);

create table volume_info (
	name		char(65),
	volid		integer,
	repfactor	integer,
	species		char(16),
	type		char(1)
);

create table session_stats (
	session_index		integer,
	cml_start		integer,
	cml_end			integer,
	cml_high		integer,
	cml_bytes_start		integer,
	cml_bytes_end		integer,
	cml_bytes_high		integer,
	records_cancelled	integer,
	records_committed	integer,
	records_aborted		integer,
	fids_realloced		integer,
	bytes_back_fetched	integer,
	system_cpu		integer,
	user_cpu		integer,
	idle_cpu		integer,
	cache_highwater		integer
);

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

create table client_rvm_stats (
	venus_index	integer,
	time		integer,
	malloc_num	integer,
	free_num	integer,
	malloc_bytes	integer,
	free_bytes	integer
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

create table normal_events (
	session_index	integer,
	opcode          integer,
	succ_count	integer,
	sigma_t		integer,
	sigma_t_squared	integer,
	fail_count	integer
);

create table normal_opcodes (
	opname		char(31),
	opcode		integer,
	kernop          integer
);

insert into normal_opcodes (opname, opcode, kernop) values ("other_ops", -1,-1);
insert into normal_opcodes (opname, opcode, kernop) values ("unused", 1, -1);
insert into normal_opcodes (opname, opcode, kernop) values ("unused", 2, -1);
insert into normal_opcodes (opname, opcode, kernop) values ("unused", 3, -1);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_open", 4, 1);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_close", 5, 2);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_ioctl", 6, 4);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_getattr", 7, 6);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_setattr", 8, 7);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_access", 9, 8);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_lookup", 10, 12);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_create", 11,13 );
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_remove", 12, 14);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_link", 13, 15);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_rename", 14, 16);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_mkdir", 15, 17);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_rmdir", 16, 18);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_readdir", 17, 20);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_symlink", 18, 21);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_readlink", 19, 8);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_fsync", 20, 10);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_inactive", 21, 11);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_vget", 22, 21);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_rdwr", 31, 3);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_resolve", 32, 22);
insert into normal_opcodes (opname, opcode, kernop) values ("vfsop_reintegrate", 33, 23);

create table mcache_events (
	venus_index	integer,
	time		integer
);

create table mcache_vnode (
	venus_index	integer,
	opcode		integer,
	entries		integer,
	sat_intrn	integer,
	unsat_intrn	integer,
	gen_intrn	integer
);
	
create table mcache_vfs (
	venus_index	integer,
	opcode		integer,
	entries		integer,
	sat_intrn	integer,
	unsat_intrn	integer,
	gen_intrn	integer
);
	
create table mcache_vnode_names (
	opcode		integer,
	opname		char(32)
);

insert into mcache_vnode_names (opname, opcode) values ("open", 0);
insert into mcache_vnode_names (opname, opcode) values ("close", 1);
insert into mcache_vnode_names (opname, opcode) values ("rdwr", 2);
insert into mcache_vnode_names (opname, opcode) values ("ioctl", 3);
insert into mcache_vnode_names (opname, opcode) values ("select", 4);
insert into mcache_vnode_names (opname, opcode) values ("getattr", 5);
insert into mcache_vnode_names (opname, opcode) values ("setattr", 6);
insert into mcache_vnode_names (opname, opcode) values ("access", 7);
insert into mcache_vnode_names (opname, opcode) values ("readlink", 8);
insert into mcache_vnode_names (opname, opcode) values ("fsync", 9);
insert into mcache_vnode_names (opname, opcode) values ("inactive", 10);
insert into mcache_vnode_names (opname, opcode) values ("lookup", 11);
insert into mcache_vnode_names (opname, opcode) values ("create", 12);
insert into mcache_vnode_names (opname, opcode) values ("remove", 13);
insert into mcache_vnode_names (opname, opcode) values ("link", 14);
insert into mcache_vnode_names (opname, opcode) values ("rename", 15);
insert into mcache_vnode_names (opname, opcode) values ("mkdir", 16);
insert into mcache_vnode_names (opname, opcode) values ("rmdir", 17);
insert into mcache_vnode_names (opname, opcode) values ("symlink", 18);
insert into mcache_vnode_names (opname, opcode) values ("readdir", 19);
insert into mcache_vnode_names (opname, opcode) values ("vget", 20);
insert into mcache_vnode_names (opname, opcode) values ("resolve", 21);
insert into mcache_vnode_names (opname, opcode) values ("reintegrate", 22);


create table mcache_vfs_names (
	opcode		integer,
	opname		char(32)
);

insert into mcache_vfs_names (opname, opcode) values ("mount", 0);
insert into mcache_vfs_names (opname, opcode) values ("umount", 1);
insert into mcache_vfs_names (opname, opcode) values ("root", 2);
insert into mcache_vfs_names (opname, opcode) values ("statfs", 3);
insert into mcache_vfs_names (opname, opcode) values ("sync", 4);
insert into mcache_vfs_names (opname, opcode) values ("vget", 5);

create table comm_events (
	venus_index	integer,
	server		integer,
	serial_number	integer,
	timestamp	integer,
	serverup	smallint
);

create table overflows (
	venus_index	integer,
	vm_start_time	integer,
	vm_end_time	integer,
	vm_cnt		integer,
	rvm_start_time	integer,
	rvm_end_time	integer,
	rvm_cnt		integer
);

create table server_stats (
	vice_index	integer,
	time		integer,
	system_cpu	integer,
	user_cpu	integer,	
	idle_cpu	integer,	
	boot_time	integer,	
	total_io	integer
);

create table srvr_call_names (
	call_name	char(31),
	call_index	serial(1)
);

create table srvr_mltcall_names (
	call_name	char(31),
	call_index	serial(1)
);

create table clnt_call_names (
	call_name	char(31),
	call_index	serial(1)
);

create table clnt_mltcall_names (
	call_name	char(31),
	call_index	serial(1)
);

create table srvr_calls (
	vice_index	integer,
	call_index	integer,
	count_entry	integer,
	count_exit	integer,
	tsec		integer,
	tusec		integer,	
	counttime	integer
);

create table srvr_mltcalls (
	vice_index	integer,
	call_index	integer,
	count_entry	integer,
	count_exit	integer,
	tsec		integer,
	tusec		integer,	
	counttime	integer,
	counthost	integer
);

create table clnt_calls (
	venus_index	integer,
	call_index	integer,
	count_entry	integer,
	count_exit	integer,
	tsec		integer,
	tusec		integer,	
	counttime	integer
);

create table clnt_mltcalls (
	venus_index	integer,
	call_index	integer,
	count_entry	integer,
	count_exit	integer,
	tsec		integer,
	tusec		integer,	
	counttime	integer,
	counthost	integer
);

create table rvm_res_entry (
	vice_index	integer,
	time		integer,
	volume		integer,
	lsh_size	integer,
	lmh_size	integer,
	shh_size	integer,
	fhh_size	integer,
	vlh_size	integer,
	ls_size		integer,
	rvm_res_index	serial(1)
);

create table rvm_res_stats (
	rvm_res_index		integer,
	file_resolves		integer,
	file_success		integer,
	file_conflict		integer,
	file_runtforce		integer,
	file_weak_eq		integer,
	file_regular		integer,
	file_usrres		integer,
	file_succ_userres	integer,
	file_partial_vsg	integer,
	dir_resolves		integer,
	dir_success		integer,
	dir_conflict		integer,
	dir_no_work		integer,
	dir_problems		integer,
	dir_partial_vsg		integer,
	nn_conf			integer,
	ru_conf			integer,
	uu_conf			integer,
	mv_conf			integer,
	lw_conf			integer,
	other_conf		integer,
	wraps			integer,
	adm_grows		integer,
	vallocs			integer,
	vfrees			integer,
	highest			integer
);

create table log_size_hist (
	rvm_res_index	integer,
	bucketnum	integer,
	value		integer
);

create table log_max_hist (
	rvm_res_index	integer,
	bucketnum	integer,
	value		integer
);

create table succ_hier_hist (
	rvm_res_index	integer,
	bucketnum	integer,
	value		integer
);

create table fail_hier_hist (
	rvm_res_index	integer,
	bucketnum	integer,
	value		integer
);

create table var_log_hist (
	rvm_res_index	integer,
	bucketnum	integer,
	value		integer
);

create table rvm_res_log_hist (
	rvm_res_index	integer,
	bucketnum	integer,
	value		integer
);

create table srv_overflow (
	vice_index	integer,
	time		integer,
	start_time	integer,
	end_time	integer,
	cnt		integer
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
	max_readvol_num	integer,
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

