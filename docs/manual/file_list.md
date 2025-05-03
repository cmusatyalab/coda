---
hide:
 - toc
---

# System Configuration Files

| `File` | Where | Created By | Edited By | Purpose |
| ------ | ----- | ---------- | --------- | ------- |
| `auth2.pw` | `/vice/db` | *initpw* | *auth2* | User password database used by *auth2*. |
| `auth2.tk` | `/vice/db` | *vice-setup* |  | Used for secure communication among Coda servers. |
| `dumplist` | `/vice/db` | *vice-setup* | *createvol_rep* | Specifies which volumes to backup and when. |
| `files` | `/vice/db` | *vice-setup* | hand | Update uses this as a list of files to keep up to date on all Coda servers. |
| `servers` | `/vice/db` | *vice-setup* | hand | Server number.  Used as part of a volume number. |
| `prot_users.db` `prot_index.db` | `/vice/db` | *pdbtool* | *pdbtool* | User and group database used by Coda file servers |
| `volutil.tk` | `/vice/db` | *vice-setup* |  | Used for secure communication among Coda servers. |
| `VLDB` | `/vice/db` | *buildvldb.sh* |  | Volume Location Database.  Used by clients to locate volumes. |
| `AllVolumes` | `/vice/vol` | *bldvldb.sh* |  | Human readable version of VLDB. |
| `VRDB` | `/vice/db` | *volutil* *makevrdb* | *volutil* *makevrdb* | Map group volume names and numbers from a replicated volume to a VSG and the read-write volumes that make up the replicated volume. |
| `VRlist` | `/vice/vol` | *volutil* | *volutil* | Human readable version of VRDB. |
| `VSGDB` | `/vice/db` | *vice-setup* | hand | Used by *createvol_rep* to map a VSG to a set of servers. |
| `ROOTVOLUME` | `/vice/db` | vice-setup | hand | Used by *codasrv* to tell clients what volume should be mounted at the root of our Coda tree |
| `maxgroupid` | `/vice/vol` | *vice-setup* | *createvol_rep* | Used by *creatvol_rep* when allocating group ids. |
| `partitions` | `/vice/vol` | *bldvldb.sh* |  | Lists vice partitions for all servers. |
| `RWlist` | `/vice/vol` | *createvol_rep* |  | Lists all read/write volumes. |
| `VolumeList` | `/vice/vol` | *createvol_rep* |  | List of volumes on a server. |
| `SrvLog` | `/vice/srv` | *codasrv* |  | Log of server activity. |
| `SrvLog-<?>` | `/vice/srv` | *volutil* |  | Old SrvLog. |
| `update.tk` | `/vice/db` | *vice-setup* | | Used for securing the communication between update daemons. |
| `UpdateClntLog` | `/vice/misc` | *updateclnt* | *updateclnt* | Log of *updateclnt* activity. |
| `UpdateSrvLog` | `/vice/misc` | *updatesrv* | *updatesrv* | Log of *updatesrv* activity. |
| `AuthLog` | `/vice/auth2` | *auth2* |  | Log of authentication server activity. |
| `venus.conf` | `/etc/coda` (Client) | *venus-setup* | hand | Venus configuration file. |
