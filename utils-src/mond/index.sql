create index i_vein_index on venus_instances (instance_index);
create index i_vein_host on venus_instances (host);
create index i_viin_index on vice_instances (instance_index);
create index i_viin_host on vice_instances (host);
create index i_sess_venindex on sessions (venus_index);
create index i_sess_sesindex on sessions (session_index);
create index i_sess_volume on sessions (volume);
create index i_sess_cetime on sessions (comm_event_time);
create index i_sess_uid on sessions (uid);
create index i_ssst_sesindex on session_stats (session_index);
create index i_clrvst_venusidx on client_rvm_stats (venus_index);
create index i_noev_sesindex on normal_events (session_index);
create index i_mcev_venindex on mcache_events (venus_index);
create index i_ncvn_venindex on mcache_vnode (venus_index);
create index i_ncvf_venindex on mcache_vfs (venus_index);
create index i_coev_venindex on comm_events (venus_index);
create index i_ovev_venindex on overflows (venus_index);
create index i_sest_viceindex on server_stats (vice_index);
create index i_srcana_callindex on srvr_call_names (call_index);
create index i_srmlna_callindex on srvr_mltcall_names (call_index);
create index i_clcana_callindex on clnt_call_names (call_index);
create index i_clmlna_callindex on clnt_mltcall_names (call_index);
create index i_srca_viceindex on srvr_calls (vice_index);
create index i_srca_callindex on srvr_calls (call_index);
create index i_srml_viceindex on srvr_mltcalls (vice_index);
create index i_srml_callindex on srvr_mltcalls (call_index);
create index i_clca_venusindex on clnt_calls (venus_index);
create index i_clca_callindex on clnt_calls (call_index);
create index i_clml_venusindex on clnt_mltcalls (venus_index);
create index i_clml_callindex on clnt_mltcalls (call_index);
create index i_rvreen_viceindex on rvm_res_entry (vice_index);
create index i_rvreen_resindex on rvm_res_entry (rvm_res_index);
create index i_rvrest_resindex on rvm_res_stats (rvm_res_index);
create index i_lsh_resindex on log_size_hist (rvm_res_index);
create index i_lmh_resindex on log_max_hist (rvm_res_index);
create index i_shh_resindex on succ_hier_hist (rvm_res_index);
create index i_fhh_resindex on fail_hier_hist (rvm_res_index);
create index i_vlh_resindex on var_log_hist (rvm_res_index);
create index i_rrlh_resindex on rvm_res_log_hist (rvm_res_index);
create index i_srov_time on srv_overflow (time);
create index i_sessid on sessions (venus_index, session, volume, uid);
create index i_commid on comm_events (venus_index, server, serial_number,
                                      timestamp, serverup);
create index i_ovrflid on overflows (venus_index, vm_end_time,
                                     vm_start_time, vm_cnt, rvm_end_time,
                                     rvm_start_time, rvm_cnt);
create index i_venusid on venus_instances (host, birth_time);
create index i_resid on rvm_res_entry (vice_index, volume);
create index i_srvovrflid on srv_overflow (vice_index, start_time,
                                           end_time, cnt);
create index i_viceid on vice_instances (host, birth_time);
create index i_adviceid on advice_stats (venus_index, uid);
create index i_vcbid on vcb_stats (venus_index, volume);
create index i_iotinfoid on iot_info (venus_index, tid);
create index i_iotstatsid on iot_stats (venus_index);
create index i_subtreestatsid on subtree_stats (venus_index);
create index i_repairid on repair_stats (venus_index);
create index i_rwsstatsid on rws_stats (venus_index, time, volume_id, rw_sharing_count, disc_read_count, disc_duration);

