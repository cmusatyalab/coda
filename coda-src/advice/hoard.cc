/*
 * The routines in this file were modified from those in vtools/hoard.cc.  They have
 * not been tested.  They are not used by the advice monitor, though they probably will
 * be eventually.
 */


int canonicalize(char *, VolumeId *, char *, char *, VolumeId *, char (*)[MAXPATHLEN]);
char *vol_getwd(VolumeId *, char *, char *);
VolumeId GetVid(char *);



int AddElement(char *pathname, int priority, char meta) {
  VolumeId volno, svollist[MAXSYMLINKS];
  char name[MAXPATHLEN], snamelist[MAXSYMLINKS][MAXPATHLEN];
  char fullname[MAXPATHLEN];

  /* Assure ourselves that the priority is within range */
  assert((priority >= H_MIN_PRI) && (priority <= H_MAX_PRI));

  /* Setup the meta-information */
  int attributes = H_DFLT_ATTRS;
  switch(meta) {
    case 'c':
      attributes |= (H_CHILDREN | H_INHERIT);
      break;
    case 'd':
      attributes |= (H_DESCENDENTS | H_INHERIT); 
      break; 
  }    

  /* Canonicalize the absolute pathname into volume ID and relative path */
  if (!canonicalize(pathname, &volno, name, fullname, svollist, snamelist)) {
    LogMsg(100, LogLevel, LogFile, "Can't canonicalize %s\n", pathname);
    return(-1);
  }
    
  Add.append(new add_entry(volno, name, priority, attributes));
  
  /* add symlink entries */
  for (int i = 0; i < MAXSYMLINKS; i++) 
    if (svollist[i] != 0) {
      LogMsg(10, LogLevel, LogFile, "adding symlink entry <%x, %s>\n",	
		   svollist[i], snamelist[i]);
      Add.append(new add_entry(svollist[i], snamelist[i], 
			       priority, attributes));
    }		

  return(0);
}




/* Canonicalize a pathname. */
/* Caller may ask for either "volume" or "full" canonicalization, or both. */
/* "Volume" canonicalization splits the result into a <volid, canonical-name-from-volroot> pair. */
/* Returns 1 on success, 0 on failure. */
static int canonicalize(char *path, VolumeId *vp, char *vname, char *fullname,
			 VolumeId *svp, char (*sname)[MAXPATHLEN]) {

    LogMsg(100, LogLevel, LogFile, "Entering canonicalize (%s)\n", path);

    /*  Strategy:
     *      1. "chdir" to lowest directory component, and get its canonical name
     * 		Go component by component, examining each component for the dreaded symlinks!
     *      2. validate and append trailing, non-directory component (if any) to canonical name
     *      3. don't forget to "chdir" back to cwd before returning!
     */

    int rc = 0;
    if (vp) {
	*vp = 0;
	vname[0] = '\0';
    }
    if (fullname)
	fullname[0] = '\0';

    int ix = 0;
    if (svp) 
	for (int i = 0; i < MAXSYMLINKS; i++) {
	    svp[i] = 0;
	    sname[i][0] = '\0';
	}

    char tpath[MAXPATHLEN];
    strcpy(tpath, path);
    char *p = tpath;

    /* In case of absolute pathname chdir to "/" and strip leading slashes. */
    if (*p == '/') {
	if (chdir("/") < 0) {
	    LogMsg(0, LogLevel, LogFile, "canonicalize: can't chdir(/) (%s)", sys_errlist[errno]);
	    goto done;
	}
	while (*p == '/') p++;
    }

    for (;;) {
	/* Get component into next_comp. */
	/* Advance p past this component (and any trailing slashes). */
	char next_comp[MAXPATHLEN];
	char *cp = next_comp;
	char c;
	while ((c = *p) && c != '/') { p++; *cp++ = c;}
	*cp = '\0';
	while (*p == '/') p++;

	/* If next_comp exists, try to "cd" there. */
	if (next_comp[0] != '\0') {

	    /* If next_comp is a symlink, save it */
	    struct stat tbuf;
	    char contents[MAXPATHLEN];
	    contents[0] = '\0';
	    if (lstat(next_comp, &tbuf) == 0 &&
		(tbuf.st_mode & S_IFMT) == S_IFLNK) {
		/* Make sure we can read the link contents */
		int cc = readlink(next_comp, contents, MAXPATHLEN);
		if (cc <= 0) {
		    LogMsg(0, LogLevel, LogFile, "canonicalize: readlink(%s) failed (%s)", sys_errlist[errno]);
		    exit(-1);
		}
		contents[cc] = '\0';

		/* save symlink name if possible. */
		if (svp && (vol_getwd(&svp[ix], NULL, sname[ix]) != NULL)) {
		    /* tack on current component */
		    strcat(sname[ix], "/");
		    strcat(sname[ix], next_comp);
		    ix++;
		}
	    }

	    if (chdir(next_comp) == 0) continue;

	    if (errno != ENOTDIR && errno != ENOENT) {
	        LogMsg(0, LogLevel, LogFile, "canonicalize: chdir(%s) failed (%s)",
		       next_comp, sys_errlist[errno]);
		goto done;
	    }

	    /* translate symlink contents */
	    if (contents[0] != '\0') {
		/* Tack on trailing component(s). */
		if (*p != '\0') {
		    strcat(contents, "/");
		    strcat(contents, p);
		}

		/* Reset buffer and pointer. */
		strcpy(tpath, contents);
		p = tpath;

		/* In case of absolute pathname chdir to "/" and strip leading slashes. */
		if (*p == '/') {
		    if (chdir("/") < 0) {
			LogMsg(0, LogLevel, LogFile, "canonicalize: can't chdir(/) (%s)", sys_errlist[errno]);
			goto done;
		    }
		    while (*p == '/') p++;
		}

		continue;
	    }
	}

	/* We're at lowest existing component.  Get its canonical name. */
	if (vp) {
	    if (vol_getwd(vp, NULL, vname) == NULL) {
		LogMsg(0, LogLevel, LogFile, "canonicalize: %s", vname);
		goto done;
	    }
	}
	if (fullname) {
	    if (getwd(fullname) == NULL) {
		LogMsg(0, LogLevel, LogFile, "canonicalize: %s", fullname);
		goto done;
	    }
	}

	/* Tack on the trailing component(s). */
	if (next_comp[0] != '\0') {
	    if (vp) {
		strcat(vname, "/");
		strcat(vname, next_comp);
		if (*p != '\0') {
		    strcat(vname, "/");
		    strcat(vname, p);
		}
	    }
	    if (fullname) {
		strcat(fullname, "/");
		strcat(fullname, next_comp);
		if (*p != '\0') {
		    strcat(fullname, "/");
		    strcat(fullname, p);
		}
	    }
	}

	break;
    }

    /* Canonicalization has succeeded. */
    rc = 1;

done:
    if (chdir(cwd) < 0) {
	LogMsg(0, LogLevel, LogFile, "canonicalize: chdir(%s) failed (%s)", cwd, sys_errlist[errno]);
	exit(-1);
    }
    LogMsg(0, LogLevel, LogFile, "canonicalize: %s -> %d, <%x, %s>, %s\n",
		  path, rc, vp ? *vp : 0, vname ? vname : "", fullname ? fullname : "");
    return(rc);
}


/* Like getwd, except that the returned path is in two parts: */
/*     head:  path from root to volume_root */
/*     tail:  path from volume_root to working directory */
/* The volume number is also returned. */
/* The caller may pass in NULL for vp, head or both. */
static char *vol_getwd(VolumeId *vp, char *head, char *tail) {
    if (vp) *vp = 0;
    tail[0] = '\0';
    if (head) head[0] = '\0';

    char fullname[MAXPATHLEN];
    if (getwd(fullname) == NULL) {
	strcpy(tail, fullname);
	return(NULL);
    }

    VolumeId vid = GetVid(".");
    if (vid == 0){
	sprintf(tail, "vol_getwd: can't get volid for %s (%s)",
		fullname, sys_errlist[errno]);
	return(NULL);
    }

    /* Check ancestor components for the first in a different volume. */
    /* "tname" holds the (relative) path to the component in question. */
    /* p1 and p2 are cursors pointing respectively at the separators  */
    /* following the component in question and its successor. */
    char tname[MAXPATHLEN];
    strcpy(tname, ".");
    char *p1 = fullname + strlen(fullname);
    char *p2;
    for (;;) {
	/* "tname/.." is the component in question in this iteration. */
	strcat(tname, "/..");

	/* Move the cursors. */
	p2 = p1;
	while (p1 >= fullname && *--p1 != '/')
	    ;

	VolumeId tvid = GetVid(tname);
	if (tvid != vid) {
	    if (tvid == 0) {
		/* Only some value(s) of errno here should allow us to continue! -JJK */
	    }
	    break;
	}
    }

    /* p2 is now the path to the volume root. */
    if (vp) *vp = vid;
    strcpy(tail, ".");
    if (p2[0] != '\0') {
	if (p2[0] != '/') strcat(tail, "/");
	strcat(tail, p2);
    }
/*
    strcpy(tail, p2);
    if (tail[0] == '\0')
	strcpy(tail, "/");
*/
    if (head) {
	int len = p2 - fullname;
	strncpy(head, fullname, len);
	head[len] = '\0';
    }

    LogLevel(100, LogLevel, LogFile, "vol_getwd: %s -> %x, %s, %s\n",
		  fullname, vp ? *vp : 0, head ? head : "", tail);
    return(tail);
}


static VolumeId GetVid(char *name) {
    LogMsg(100, LogLevel, LogFile, "Entering GetVid (%s)\n", name);

    VolumeId vid = 0;

/*
    struct getvolstat_msg {
	VolumeStatus volstat;
	char strings[544];
    } gvs_msg;

    struct ViceIoctl vi;
    vi.in = 0;
    vi.in_size = 0;
    vi.out = (char *)&gvs_msg;
    vi.out_size = sizeof(struct getvolstat_msg);
    if (pioctl(name, VIOCGETVOLSTAT, &vi, 1) == 0)
	vid = gvs_msg.volstat.Vid;
*/

    struct getfid_msg {
	ViceFid fid;
	ViceVersionVector vv;
    } gf_msg;

    struct ViceIoctl vi;
    vi.in = 0;
    vi.in_size = 0;
    vi.out = (char *)&gf_msg;
    vi.out_size = sizeof(struct getfid_msg);
    if (pioctl(name, VIOC_GETFID, &vi, 1) == 0)
	vid = gf_msg.fid.Volume;
    LogLevel(100, LogLevel, LogFile, "GetVid: %s -> %x\n", name, vid);
    return(vid);
}

