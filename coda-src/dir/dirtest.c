#include "coda_assert.h"
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>

#include <lwp.h>
#include <lock.h>
#include <rvm.h>
#include <rvmlib.h>
#include <util.h>
#include <parser.h>
#include <cfs/coda.h>
#include <codadir.h>
#include <dirbody.h>

void dt_init(int argc, char **argv);
void dt_ok(int argc, char **argv);
void dt_mdir(int argc, char **argv);
void dt_hash(int argc, char **argv);
void dt_printchain(int argc, char **argv);
void dt_create(int argc, char **argv);
void dt_delete(int argc, char **argv);
void dt_list(int argc, char **argv);
void dt_length(int argc, char **argv);
void dt_convert(int argc, char **argv);
void dt_empty(int argc, char **argv);
void dt_lookup(int argc, char **argv);
void dt_fidlookup(int argc, char **argv);
void dt_rdsfree(int argc, char **argv);
void dt_free(int argc, char **argv);
void dt_compare(int argc, char **argv);
void dt_vdir(int argc, char **argv);
void dt_readargs(int argc, char **argv);
void dt_quit(int argc, char **argv);
struct dirdata *dt_initrvm(char *log, char *data);
void dt_bulktest(int arc, char **argv);

#define FLUSH no_flush
#define RESTORE no_restore

command_t dtcmds[] =
{
        {"init", dt_init, 0, "make rvm clean"},
	{"ok", dt_ok, 0, "check a directory"},
	{"mdir", dt_mdir, 0, "create an empty directory"},
	{"free", dt_free, 0, "delete directories"},
	{"bulk", dt_bulktest, 0, "bulk test on directories"},
	{"create", dt_create, 0, "create a dir entry"},
	{"list", dt_list, 0, "list a directory"},
	{"vdir", dt_vdir, 0, "list a venus BSD format directory"},
	{"rdsfree", dt_rdsfree, 0, "tracer for the rdsfee problem"},
	{"delete", dt_delete, 0, "delete dirno name (deletes a dir entry)"},
	{"empty", dt_empty, 0, "delete dirno (test if dir is empty)"},
	{"length", dt_length, 0, "length dirno (print size in bytes)"},
	{"compare", dt_compare, 0, "dirno1 dirno1 (compare dirs)"},
	{"convert", dt_convert, 0, "convert dirno file (convert to BSD format in file)"},
	{"lookup", dt_lookup, 0, "lookup dirno name (lookup a dir entry)"},
	{"fidlookup", dt_fidlookup, 0, "fidlookup dirno vnode unique (lookup a dir entry by fid)"},
	{"hash", dt_hash, 0, "hash name (print hash value of a name)"},
	{"printchain", dt_printchain, 0, "printchain dirno chainno (print a dirhash chain)"},
	{"quit", dt_quit, 0, "quit this program"},
	{ 0, 0, 0, NULL }
};

void dt_quit(int argc, char **argv)
{
	exit(0);
}


#define NDIRS 100


struct dirdata {
	struct DirHandle dd_dh[NDIRS];
};
struct dirdata *dd;

void printtime(void) 
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	printf("TIME: %ld.%ld", tv.tv_sec, tv.tv_usec);
}




PDirHandle dt_dh(int i)
{
	CODA_ASSERT( i >= 0 && i <= NDIRS);
	return &dd->dd_dh[i];
}

void dt_init(int argc, char **argv) 
{
	rvm_return_t status;

	rvmlib_begin_transaction(RESTORE);
		
        rvmlib_set_range(dd, sizeof(struct dirdata));
	bzero(dd, sizeof(struct dirdata));

	rvmlib_end_transaction(FLUSH, &status);

	if ( status != RVM_SUCCESS) {
		printf("error initializing dirdata\n");
		abort();
	}
}

void printit(struct venus_dirent *de)
{
	fprintf(stdout, "fileno: %ld, reclen %hd, type %i, namelen: %d, name: \n%*s\n",
		de->d_fileno, de->d_reclen, de->d_type, de->d_namlen, de->d_namlen, de->d_name);
}

void dt_vdir(int argc, char **argv)
{
	int fd;
	char *buf;
	struct stat statb;
	int offset, len;
	struct venus_dirent *de;

	if ( argc != 2 ) {
		printf("Usage %s file\n", argv[0]);
		return;
	}

	fd = open(argv[1], O_RDONLY);
	if ( fd < 0 || fstat(fd, &statb) != 0 ) {
		printf("File %s not accessible!\n", argv[1]);
		return;
	}
	     
	len = statb.st_size;
	buf = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	CODA_ASSERT( (int )buf != -1 );

	offset = 0;
	while( 1 ) {
		de = (struct venus_dirent *)(buf + offset);
		if ( de->d_reclen != 0 ) 
			printit(de);
		if ( offset + de->d_reclen == len || de->d_namlen == 0) 
			break;
		offset += de->d_reclen;
	}
	return;

}

void dt_free(int argc, char **argv)
{
	int i = 0;
	rvm_return_t err;
	PDirHandle dh;

	if ( argc != 2 ) {
		printf("Usage %s {dirno, all}\n", argv[0]);
		return;
	}

	if (strcmp(argv[1], "all") == 0) {
		rvmlib_begin_transaction(RESTORE);
		while ( i < NDIRS ) {
			dh = dt_dh(i);
			DH_FreeData(dh);
			free(dh);
			i++;
		}
		rvmlib_end_transaction(FLUSH, &err);
		if ( err ) printf("Error in rvmlib_end_transaction\n");
		return;
	}
	i = atoi(argv[1]);

	if ( i < 0 || i >= NDIRS ) {
		printf("Directory %d out of range!\n", i);
		return;
	}
	dh = dt_dh(i);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", i);
		return ;
	}

	rvmlib_begin_transaction(RESTORE);
	DH_FreeData(dh);
	rvmlib_end_transaction(FLUSH, &err);

	if ( err ) 
		printf("Error in rvmlib_end_transaction\n");
	return;
}
	
void dt_create(int argc, char **argv) 
{
	rvm_return_t err;
	int dirno;
	struct DirFid fid;
	PDirHandle dh;
	struct ViceFid vfid;
	
	if ( argc != 5 ) {
		printf("usage %s dirno name vnode uniq\n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno >= NDIRS ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	FID_Int2DFid(&fid, atoi(argv[3]), atoi(argv[4]));
	FID_DFid2VFid(&fid, &vfid);

	rvmlib_begin_transaction(RESTORE);
	err = DH_Create(dh, argv[2], &vfid);
	if ( err ) {
		printf("DIR_Create failed\n");
		rvmlib_end_transaction(FLUSH, &err);
		return;
	}
	rvmlib_end_transaction(FLUSH, &err);
	if ( err ) {
		printf("DIR_Create - end trans failed\n");
		return;
	}
	return;
}


void dt_bulktest(int argc, char **argv)
{
	int i , j;
	int count;
	int err;

	int rvmcount;
	int dirno;
	int *ino1;
	int *ino2;
	char *mdir_argv[5] = {"mdir", "newdir", "0", "0", "1", "1"};
	char *create_argv[4] = {"create/del", argv[1], "newdir", "0", "0"};
	char *rmdir_arg[2] = {"rmdir", "1" };



	if ( argc != 4 ) {
		printf("usage %s dirno count rvmcount\n", argv[0]);
		return;
	}
	count = atoi(argv[2]);
	rvmcount = atoi(argv[3]);

	printtime();
	rvmlib_begin_transaction(RESTORE);
	
	ino1 = rvmlib_rec_malloc(sizeof(int) * 256); 

	rvmlib_end_transaction(FLUSH, &err);

	for ( i = 0 ; i < count/rvmcount ; i++ ) {
		for ( j = 0 ; j < rvmcount ; j++ ) {
			printf("creating %d, %d\n", i, j);
			dt_mdir(5, mdir_argv);
			dt_create(5, create_argv);
			rvmlib_begin_transaction(RESTORE);
			ino2 = rvmlib_rec_malloc(sizeof(int) * 512); 
			rvmlib_set_range(ino2, 512 * sizeof(int));
			bzero(ino2, 256 *sizeof(int));
			rvmlib_set_range(ino1, sizeof(int));
			ino1[1] = 0;
			rvmlib_end_transaction(FLUSH, &err);
			dt_delete(3, create_argv);
			rvmlib_begin_transaction(RESTORE);
			rvmlib_rec_free(ino2);
			rvmlib_end_transaction(FLUSH, &err);
			dt_free(2, rmdir_arg);
		}
		rvm_flush();
	}
	printtime();
}




void dt_lookup(int argc, char **argv) 
{
	int err;
	int dirno;
	struct DirFid fid;
	PDirHandle dh;
	struct ViceFid vfid;
	
	if ( argc != 3 ) {
		printf("usage %s dirno name \n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	err = DH_Lookup(dh, argv[2], &vfid, CLU_CASE_SENSITIVE);
	if ( err ) {
		printf("DIR_Lookup failed\n");
		return;
	}
	FID_VFid2DFid(&vfid, &fid);
	FID_PrintFid(&fid);
	return;
}

void dt_fidlookup(int argc, char **argv) 
{
	int err;
	int dirno;
	struct DirFid fid;
	struct ViceFid vfid;
	PDirHandle dh;
	char name[MAXPATHLEN];
	
	if ( argc != 4 ) {
		printf("usage %s dirno vnode unique \n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	FID_Int2DFid(&fid, atoi(argv[2]), atoi(argv[3]));
	FID_DFid2VFid(&fid, &vfid);
	err = DH_LookupByFid(dh, name, &vfid);
	if ( err ) {
		printf("DH_LookupByFid failed\n");
		return;
	}

	
	FID_PrintFid(&fid);
	printf("Name: %s\n", name);
	return;
}

void dt_printchain(int argc, char **argv) 
{
	int dirno;
	PDirHandle dh;
	
	if ( argc != 3 ) {
		printf("usage %s dirno chain \n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	DIR_PrintChain(dh->dh_data, atoi(argv[2]));

	return;
}
void dt_convert(int argc, char **argv) 
{
	int dirno;
	VolumeId vol;
	PDirHandle dh;
	int fd;
	
	if ( argc != 4 ) {
		printf("usage %s dirno vol file\n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	vol = atoi(argv[2]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	fd = open(argv[3], O_CREAT | O_RDWR, 0644);
	CODA_ASSERT( fd >= 0);
	close(fd);

	DH_Convert(dh, argv[2], vol);

	return;
}

void dt_length(int argc, char **argv) 
{
	int dirno;
	PDirHandle dh;
	
	if ( argc != 2 ) {
		printf("usage %s dirno\n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	printf("size: %d\n", DH_Length(dh));

	return;
}


void dt_compare(int argc, char **argv) 
{
	int dirno;
	PDirHandle dh1;
	PDirHandle dh2;
	
	if ( argc != 3 ) {
		printf("usage %s dirno dirno\n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh1 = dt_dh(dirno);


	if ( !dh1->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	dirno = atoi(argv[2]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh2 = dt_dh(dirno);


	if ( !dh2->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	printf("comparison: %d\n", DIR_Compare(dh1->dh_data, dh2->dh_data));

	return;
}

void dt_empty(int argc, char **argv) 
{
	int dirno;
	PDirHandle dh;
	int rc;
	
	if ( argc != 2 ) {
		printf("usage %s dirno\n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	rc = DH_IsEmpty(dh);
	if ( rc ) 
		printf("empty: rc=%d\n", rc);
	else 
		printf("notempty: rc=%d\n", rc);
	return;
}

void dt_hash(int argc, char **argv) 
{
	
	if ( argc != 2 ) {
		printf("usage %s  name \n", argv[0]);
		return;
	}

	printf("name: %s, hash: %d\n", argv[1], DIR_Hash(argv[1]));
	return;
}

void dt_list(int argc, char **argv) 
{
	int dirno;
	PDirHandle dh;
	
	if ( argc != 2 ) {
		printf("usage %s dirno\n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}


	DH_Print(dh);

	return;
}

void dt_ok(int argc, char **argv) 
{
	int dirno;
	PDirHandle dh;
	
	if ( argc != 2 ) {
		printf("usage %s dirno\n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	if ( DIR_DirOK(dh->dh_data) ) {
		printf("Directory is OK!\n");
	} else {
		printf("Directory %d not OK\n", dirno);
		return ;
	}

	return;
}


void dt_delete(int argc, char **argv) 
{
	rvm_return_t err;
	int dirno;
	PDirHandle dh;
	
	if ( argc != 3 ) {
		printf("usage %s dirno name \n", argv[0]);
		return;
	}

	dirno = atoi(argv[1]);
	if ( dirno < 0 || dirno > NDIRS -1 ) {
		printf("dirno out of bounds, or dirno not allocated!\n");
		return;
	}
	dh = dt_dh(dirno);
	if ( !dh->dh_data ) {
		printf("Directory %d not allocated\n", dirno);
		return ;
	}

	rvmlib_begin_transaction(RESTORE);
	err = DH_Delete(dh, argv[2]);
	if ( err ) {
		printf("DIR_Delete failed for %s.\n", argv[2]);
		rvmlib_abort(err);
		return;
	}
	rvmlib_end_transaction(FLUSH, &err);
	if ( err ) {
		printf("DIR_Delete - end trans failed\n");
		return;
	}
	return;
}



void dt_rdsfree(int argc, char **argv)
{
	int *value;
	rvm_return_t err;

	rvmlib_begin_transaction(RESTORE);
	value = rvmlib_rec_malloc(sizeof(*value));
	rvmlib_end_transaction(FLUSH, &err);

	rvmlib_begin_transaction(RESTORE);
	rvmlib_set_range(value, sizeof(*value));
	*value = 5;
	rvmlib_rec_free(value);
	rvmlib_end_transaction(FLUSH, &err);
}
	

void dt_mdir(int argc, char **argv)
{
	rvm_return_t status;
	int err;
	PDirHandle dh;
	int i = 0;
	struct DirFid me, parent;
	struct ViceFid vme, vparent;

	if ( argc != 5 ) {
		printf("Usage makedir mevn meun parentvn parentun\n");
		return ;
	}

	FID_Int2DFid(&me, atoi(argv[1]), atoi(argv[2]));
	FID_Int2DFid(&parent, atoi(argv[3]), atoi(argv[4]));
	FID_DFid2VFid(&me, &vme);
	FID_DFid2VFid(&parent, &vparent);

	while ( i < NDIRS ) {
		dh = dt_dh(i);
		if ( ! dh->dh_data ) 
			break;
		i++;
	}
	
	if ( i == NDIRS ) {
		printf("No more directories available!\n");
		return;
	}

	rvmlib_begin_transaction(RESTORE);

	err = DH_MakeDir(dh, &vme, &vparent);
	if ( err ) {
		printf("ERROR in DIR_MakeDir\n");
		abort();
	}
#if 0
	printf("Assigned directory %d at %p\n", i, dh->dh_data);
#endif
	rvmlib_end_transaction(FLUSH, &status);

	if ( status != RVM_SUCCESS) {
		printf("error initialining dirdata\n");
		abort();
	}
}

FILE *file = 0;

void dt_readargs(int argc, char **argv)
{
	if (argc != 3 && argc != 4 ) {
		printf("Usage %s log data [file]\n", argv[0]);
		exit(1);
	}

	if ( access(argv[1], W_OK)  ) {
		perror("LOG file access: ");
		exit(1);
	}

	if ( access(argv[2], W_OK) ) {
		perror("DATA file access: ");
		exit(1);
	}

	if ( argc == 4 ) {
		file = fopen(argv[3], "r");
		if ( !file ) {
			perror("Error opening file");
			exit(1);
		}
	}
}

struct dirdata *dt_initrvm(char *log, char *data)
{
	rvm_options_t *options = rvm_malloc_options();
	rvm_return_t err;
	struct stat buf;
	struct dirdata *dd;
	size_t size;
	rvm_offset_t length;


	if ( stat(data, &buf) ) {
		perror("Statting data file");
		exit(2);
	}
	size = buf.st_size;
	length = RVM_MK_OFFSET(0, size);
	
	options->log_dev = log;
	options->flags = 0;
	options->truncate = 50;
	err = RVM_INIT(options);

	if ( err != RVM_SUCCESS ) {
		printf("error in RVM_INIT %s\n", rvm_return(err));
		exit(2);
	}

        rds_load_heap(data, length, (char **)&dd, (int *)&err);  
	if (err != RVM_SUCCESS) {
		printf("rds_load_heap error %s\n",rvm_return(err));
		abort();
	}

	rvm_free_options(options);					    
	return dd;
}

	

int main(int argc, char **argv)
{
	int rc;
	PROCESS parentPid;
	rvm_perthread_t rvmptt;
	char *nl;

	RvmType = UFS;

	dt_readargs(argc, argv);
	printf("arguments ok\n");

	rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &parentPid);
	printf("LWP_Init returning %d\n", rc);

	rvmlib_init_threaddata(&rvmptt);
	printf("Main thread just did a rvmlib_set_thread_data()\n");

	dd = dt_initrvm(argv[1], argv[2]);
	printf("rvm initialized, dd at %p\n", dd);

	DIR_Init(DIR_DATA_IN_RVM);

	
                  printtime();
	Parser_init("dirtest> ", dtcmds);
	if ( file ) {
		char line[1024];
		while ( fgets(line, 1024, file) ) {
			if ( (nl = strchr(line, '\n')) )
				*nl = '\0';
			execute_line(line);
		}
	} else {
		Parser_commands();
	}
	return 0 ;

}
