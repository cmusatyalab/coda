/* Make a "big file" directory in coda.
 * Phil Nelson,  June 27, 2006
 *
 * Arguments:  large file, big file directory name
 *
 *     mkcodabf large.mov /coda/realm/dir/bfname
 *
 * Switches:
 *     -s  --  hunk size in megabytes
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

const off_t megabyte = 1048576;

/* Configurable via switches */
long hunksize = 1;  /* in Megabytes -- 1024*1024 */
off_t hunkbytes; 
long filesperdir = 100;
int dirdigits = 2;
int verbose = 0;

static void mkbigfile (int fd, struct stat fsb, char *dirname)
{
    long levels;		/* Number of directory levels */
    long files;		/* Total number of files */
    long lastfilesize;	/* bytes in the last file */
    long fileno;		/* File number */
    long ix;		/* index for creating dirs */
    long temp;		/* Temp data */
    long hfd;		/* Hunk's fd */
    char name[MAXPATHLEN];  /* Names of files and dirs */

    /* Calculate items. */
    files = fsb.st_size / hunkbytes;
    lastfilesize = fsb.st_size % hunkbytes;
    if (lastfilesize > 0)
	files++;
    temp = (files-1) / filesperdir;
    levels = 0;
    while (temp > 0) {
	levels++;
	temp /= filesperdir;
    }


    /* Final check */
    if (levels > 2) {
	fprintf (stderr, "Input file too big, increase hunk size "
		 "or files per directory.\n"); 
	exit(1);
    }

    /* Build the dir structure */
    switch (levels) {
    case 1:
	for (ix=0; ix <= files/filesperdir; ix++) {
	    snprintf (name, MAXPATHLEN, "%s/%0*ld", dirname,
		      dirdigits, ix);
	    if (verbose) printf ("creating directory %s\n", name);
	    if (mkdir (name, 0777) < 0) {
		fprintf (stderr, "Could not create %s: %s\n", name,
			 strerror(errno));
		exit(1);
	    }
	}
	break;
	    
    case 2:
	for (ix=0; ix <= files/filesperdir; ix++) {
	    if ((ix % filesperdir) == 0) {
		snprintf (name, MAXPATHLEN, "%s/%0*ld", dirname,
			  dirdigits, ix/filesperdir);
		if (verbose) printf ("creating directory %s\n", name);
		if (mkdir (name, 0777) < 0) {
		    fprintf (stderr, "Could not create %s: %s\n", name,
			     strerror(errno));
		    exit(1);
		}
	    }
	    snprintf (name, MAXPATHLEN, "%s/%0*ld/%0*ld", dirname,
		      dirdigits, ix/filesperdir, dirdigits, ix%filesperdir);
	    if (verbose) printf ("creating directory %s\n", name);
	    if (mkdir (name, 0777) < 0) {
		fprintf (stderr, "Could not create %s: %s\n", name,
			 strerror(errno));
		exit(1);
	    }
	}
	break;
    }

    /* Create the data files */
    for (fileno = 0; fileno < files; fileno++) {
	/* Create the file */
	switch (levels) {
	case 0:
	    snprintf (name, MAXPATHLEN, "%s/%0*ld", dirname, dirdigits,
		      fileno);
	    break;
	case 1:
	    snprintf (name, MAXPATHLEN, "%s/%0*ld/%0*ld", dirname,
		      dirdigits, fileno/filesperdir, dirdigits,
		      fileno%filesperdir);
	    break;
	case 2:
	    snprintf (name, MAXPATHLEN, "%s/%0*ld/%0*ld/%0*ld", dirname,
		      dirdigits, fileno/(filesperdir*filesperdir),
		      dirdigits, fileno/filesperdir % filesperdir,
		      dirdigits, fileno%filesperdir);
	}
	if (verbose) printf ("writing file %s\n", name);
	/* Copy the Data */
	hfd = open (name, O_WRONLY|O_CREAT, 0444);
	if (hfd < 0) {
	    fprintf (stderr, "Could not create %s: %s\n", name,
		     strerror(errno));
	    exit(1);
	}
	temp  = (fileno == files-1 ? lastfilesize : hunkbytes);
	while (temp > 0) {
	    ssize_t ret;
	    ssize_t wret;
	    char buff[8192];   /* XXX fix */
	    ret = read(fd, buff, (temp > 8192 ? 8192 : temp));
	    if (ret < 0) {
		fprintf (stderr, "Source file read problem: %s\n",
			 strerror(errno));
		exit(1);
	    }
	    wret = write (hfd, buff, ret);
	    if (wret != ret) {
		fprintf (stderr, "Write problem: unwritten data\n");
		exit(1);
	    }
	    temp -= ret;
	}
	
	/* Close the file */
	close(hfd);
    }

    /* Write the _Coda_BigFile_ file */
    {
	char data[1024];
	int chars;
        ssize_t wret;

	snprintf (name, MAXPATHLEN, "%s/_Coda_BigFile_", dirname);
	if (verbose) printf ("Creating %s\n", name);
	fd = open (name, O_WRONLY|O_CREAT, 0444);
	if (fd < 0) {
	    fprintf (stderr, "Could not create %s: %s\n", name,
		     strerror(errno));
	    exit(1);
	}
        chars = snprintf (data, 1024, "CDBGFL00 %lld %ld %ld %ld\n",
			  (long long)fsb.st_size, hunksize,
			  filesperdir, files);
	wret = write(fd, data, chars);
	if (wret != chars) {
	    fprintf (stderr, "Write problems on %s: %s\n", name,
		     strerror(errno));
	    exit(1);
	}
	close(fd);
    }
}

static void usage(char *prog)
{
    fprintf (stderr,
	     "usage: %s [-f files_per_dir] [-s size] [-v]"
	     " file new_big_file\n",
	     prog);
    exit(1);
}

int main (int argc, char **argv)
{
    int err;
    struct stat fsb;
    struct stat bfsb;
    int ffd;
    char *prog = argv[0];
    int ch;
    int temp;

    /* Switch processing */
    hunkbytes = megabyte * hunksize;
    while ((ch = getopt(argc, argv, "f:s:v")) != -1) {
	switch (ch) {
	case 'f':
	    filesperdir = atoi(optarg);
	    if (filesperdir > 1000) {
		fprintf (stderr, "%s: Too many files per directory.\n", prog);
		exit(1);
	    }
	    if (filesperdir < 8) {
		fprintf (stderr, "%s: Files per directory must be 8 or larger.\n",
			 prog);
		exit(1);
	    }
	    dirdigits = 1;
	    for (temp = (filesperdir - 1)/10; temp > 0; temp /= 10)
		dirdigits ++;
	    break;
	case 's':
	    hunksize = atoi(optarg);
	    if (hunksize < 1) {
		fprintf (stderr, "%s: Hunk size must be 1 or larger.\n", prog);
		exit(1);
	    }
	    hunkbytes = megabyte * hunksize;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case '?':
	default:
	    usage(prog);
	}
    }
    argc -= optind;
    argv += optind;

    if (argc < 2) 
	usage(prog);


    /* File information */
    err = stat (argv[0], &fsb);
    if (err < 0) {
	fprintf (stderr, "%s: %s: %s\n", prog, argv[0],
		 strerror(errno));
	exit(1);
    }

    if ((fsb.st_mode & S_IFREG) != S_IFREG) {
	fprintf (stderr, "%s: %s: Must be a regular file.\n", prog,
		 argv[0]);
	exit(1);
    }

    if (fsb.st_size < hunkbytes) {
	fprintf (stderr, "%s: %s: File too small, needs only one hunk.\n",
		 prog, argv[0]);
        exit(1);
    }

    /* Big file information */
    err = stat (argv[1], &bfsb);
    if (err >= 0) {
	fprintf (stderr, "%s: %s: Must not exist.\n", prog,
                 argv[1]);
        exit(1);
    }
    if (err < 0 && errno != ENOENT) {
	fprintf (stderr, "%s: %s: %s.\n", prog, argv[1],
		 strerror(errno));
        exit(1);
    }

    /* Time to create the big file. */

    ffd = open (argv[0], O_RDONLY, 0);
    if (ffd < 0) {
	fprintf (stderr, "%s: %s: %s\n", prog, argv[0],
		 strerror(errno));
	exit(1);
    }

    if (verbose) printf ("creating directory %s\n", argv[1]);
    err = mkdir (argv[1], 0777);
    if (err < 0) {
	fprintf (stderr, "%s: %s: %s\n", prog, argv[1],
		 strerror(errno));
	exit(1);
    }

    mkbigfile (ffd, fsb, argv[1]);

    return 0;
}
