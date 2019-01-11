// clang-format off
/* This file was generated from errordb.txt at Thu Jan 10 15:42:30 EST 2019 */
/* It translates from on-the-wire RPC2 errors to system errno values */

/* Translations for common UNIX errno values */
  case 1:	sys_err = EPERM; break;
  case 2:	sys_err = ENOENT; break;
  case 3:	sys_err = ESRCH; break;
  case 4:	sys_err = EINTR; break;
  case 5:	sys_err = EIO; break;
  case 6:	sys_err = ENXIO; break;
  case 7:	sys_err = E2BIG; break;
  case 8:	sys_err = ENOEXEC; break;
  case 9:	sys_err = EBADF; break;
  case 10:	sys_err = ECHILD; break;
  case 11:	sys_err = EDEADLK; break;
  case 12:	sys_err = ENOMEM; break;
  case 13:	sys_err = EACCES; break;
  case 14:	sys_err = EFAULT; break;
  case 15:	sys_err = ENOTBLK; break;
  case 16:	sys_err = EBUSY; break;
  case 17:	sys_err = EEXIST; break;
  case 18:	sys_err = EXDEV; break;
  case 19:	sys_err = ENODEV; break;
  case 20:	sys_err = ENOTDIR; break;
  case 21:	sys_err = EISDIR; break;
  case 22:	sys_err = EINVAL; break;
  case 23:	sys_err = ENFILE; break;
  case 24:	sys_err = EMFILE; break;
  case 25:	sys_err = ENOTTY; break;
  case 26:	sys_err = ETXTBSY; break;
  case 27:	sys_err = EFBIG; break;
  case 28:	sys_err = ENOSPC; break;
  case 29:	sys_err = ESPIPE; break;
  case 30:	sys_err = EROFS; break;
  case 31:	sys_err = EMLINK; break;
  case 32:	sys_err = EPIPE; break;
  case 33:	sys_err = EDOM; break;
  case 34:	sys_err = ERANGE; break;
  case 35:	sys_err = EAGAIN; break;
  case 37:	sys_err = EALREADY; break;
  case 38:	sys_err = ENOSYS; break;
  case 43:	sys_err = EPROTONOSUPPORT; break;
  case 45:	sys_err = EOPNOTSUPP; break;
  case 55:	sys_err = ENOBUFS; break;
  case 57:	sys_err = ENOTCONN; break;
  case 58:	sys_err = ESHUTDOWN; break;
  case 59:	sys_err = ETOOMANYREFS; break;
  case 60:	sys_err = ETIMEDOUT; break;
  case 62:	sys_err = ELOOP; break;
  case 63:	sys_err = ENAMETOOLONG; break;
  case 66:	sys_err = ENOTEMPTY; break;
  case 69:	sys_err = EDQUOT; break;

/* Coda client <-> server specific errors */
  case 101:	sys_err = VSALVAGE; break;
  case 102:	sys_err = VNOVNODE; break;
  case 103:	sys_err = VNOVOL; break;
  case 104:	sys_err = VVOLEXISTS; break;
  case 105:	sys_err = VNOSERVICE; break;
  case 106:	sys_err = VOFFLINE; break;
  case 107:	sys_err = VONLINE; break;
  case 110:	sys_err = VBUSY; break;
  case 111:	sys_err = VMOVED; break;
  case 112:	sys_err = VNOSERVER; break;
  case 113:	sys_err = VLOGSTALE; break;
  case 158:	sys_err = EVOLUME; break;
  case 198:	sys_err = EINCOMPATIBLE; break;
  case 199:	sys_err = EINCONS; break;
  case 200:	sys_err = VFAIL; break;

/* Advice monitor related errors, we should really not use these anymore */
// #define CAEFAIL		300	/* Unknown error related to the Advice Monitor */
// #define CAEVERSIONSKEW	301	/* Version skew between Venus and the Advice Monitor */
  case 302:	sys_err = CAENOSUCHUSER; break;
  case 303:	sys_err = CAENOTVALID; break;
  case 304:	sys_err = CAENOASR; break;
  case 305:	sys_err = CAEUNEXPECTEDASR; break;
// #define CAEASRINPROGRESS	306	/* Venus requested an ASR while another one is in progress */
  case 307:	sys_err = CAEADVICEPENDING; break;
  case 308:	sys_err = CAENOSERVERS; break;

