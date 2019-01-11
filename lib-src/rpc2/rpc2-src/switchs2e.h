// clang-format off
/* This file was generated from errordb.txt at Thu Jan 10 15:42:30 EST 2019 */
/* It translates from system (and Coda) errno values to error messages */

/* Translations for common UNIX errno values */
  case EPERM:	txt = "Operation not permitted"; break;
  case ENOENT:	txt = "No such file or directory"; break;
  case ESRCH:	txt = "No such process"; break;
  case EINTR:	txt = "Interrupted system call"; break;
  case EIO:	txt = "Input/output error"; break;
  case ENXIO:	txt = "Device not configured"; break;
  case E2BIG:	txt = "Argument list too long"; break;
  case ENOEXEC:	txt = "Exec format error"; break;
  case EBADF:	txt = "Bad file descriptor"; break;
  case ECHILD:	txt = "No child processes"; break;
  case EDEADLK:	txt = "Resource deadlock avoided"; break;
  case ENOMEM:	txt = "Cannot allocate memory"; break;
  case EACCES:	txt = "Permission denied"; break;
  case EFAULT:	txt = "Bad address"; break;
  case ENOTBLK:	txt = "Not a block device"; break;
  case EBUSY:	txt = "Device busy"; break;
  case EEXIST:	txt = "File exists"; break;
  case EXDEV:	txt = "Cross-device link"; break;
  case ENODEV:	txt = "Operation not supported by device"; break;
  case ENOTDIR:	txt = "Not a directory"; break;
  case EISDIR:	txt = "Is a directory"; break;
  case EINVAL:	txt = "Invalid argument"; break;
  case ENFILE:	txt = "Too many open files in system"; break;
  case EMFILE:	txt = "Too many open files"; break;
  case ENOTTY:	txt = "Inappropriate ioctl for device"; break;
  case ETXTBSY:	txt = "Text file busy"; break;
  case EFBIG:	txt = "File too large"; break;
  case ENOSPC:	txt = "No space left on device"; break;
  case ESPIPE:	txt = "Illegal seek"; break;
  case EROFS:	txt = "Read-only file system"; break;
  case EMLINK:	txt = "Too many links"; break;
  case EPIPE:	txt = "Broken pipe"; break;
  case EDOM:	txt = "Numerical argument out of domain"; break;
  case ERANGE:	txt = "Result too large"; break;
  case EAGAIN:	txt = "Resource temporarily unavailable"; break;
  case EALREADY:	txt = "Operation already in progress"; break;
  case ENOSYS:	txt = "Function not implemented"; break;
  case EPROTONOSUPPORT:	txt = "Protocol not supported"; break;
  case EOPNOTSUPP:	txt = "Operation not supported"; break;
  case ENOBUFS:	txt = "No buffer space available"; break;
  case ENOTCONN:	txt = "Socket is not connected"; break;
  case ESHUTDOWN:	txt = "Can't send after socket shutdown"; break;
  case ETOOMANYREFS:	txt = "Too many references: can't splice"; break;
  case ETIMEDOUT:	txt = "Operation timed out"; break;
  case ELOOP:	txt = "Too many levels of symbolic links"; break;
  case ENAMETOOLONG:	txt = "File name too long"; break;
  case ENOTEMPTY:	txt = "Directory not empty"; break;
  case EDQUOT:	txt = "Disc quota exceeded"; break;

/* Coda client <-> server specific errors */
  case VSALVAGE:	txt = "Volume needs salvage"; break;
  case VNOVNODE:	txt = "Bad vnode number quoted"; break;
  case VNOVOL:	txt = "Volume does not exist or not online"; break;
  case VVOLEXISTS:	txt = "Volume already exists"; break;
  case VNOSERVICE:	txt = "Volume is not in service"; break;
  case VOFFLINE:	txt = "Volume is off line"; break;
  case VONLINE:	txt = "Volume is already on line"; break;
  case VBUSY:	txt = "Volume temporarily unavailable"; break;
  case VMOVED:	txt = "Volume has moved"; break;
  case VNOSERVER:	txt = "File server lwp is not running"; break;
  case VLOGSTALE:	txt = "CML head previously reintegrated"; break;
  case EVOLUME:	txt = "Volume error"; break;
  case EINCOMPATIBLE:	txt = "Version vectors are incompatible"; break;
  case EINCONS:	txt = "File is inconsistent"; break;
  case VFAIL:	txt = "Unknown Coda error"; break;

/* Advice monitor related errors, we should really not use these anymore */
// #define CAEFAIL		300	/* Unknown error related to the Advice Monitor */
// #define CAEVERSIONSKEW	301	/* Version skew between Venus and the Advice Monitor */
  case CAENOSUCHUSER:	txt = "Advice Monitor attempted to connect on behalf of an unknown user"; break;
  case CAENOTVALID:	txt = "Advice Monitor attempted to test liveness of an invalid connection"; break;
  case CAENOASR:	txt = "Advice Monitor returned the result of an ASR, but not ASR is pending"; break;
  case CAEUNEXPECTEDASR:	txt = "Advice Monitor returned the result of the wrong ASR"; break;
// #define CAEASRINPROGRESS	306	/* Venus requested an ASR while another one is in progress */
  case CAEADVICEPENDING:	txt = "Advice Monitor requested a new connection while a request is pending"; break;
  case CAENOSERVERS:	txt = "No servers are known to Venus"; break;

