/* This file was generated from errordb.txt at Mon May 21 22:31:27 EDT 2007 */
/* It translates from system errno values to on-the-wire RPC2 errors */
// clang-format off
/* Translations for common UNIX errno values */
  case EPERM:	rpc2_err = 1; break;
  case ENOENT:	rpc2_err = 2; break;
  case ESRCH:	rpc2_err = 3; break;
  case EINTR:	rpc2_err = 4; break;
  case EIO:	rpc2_err = 5; break;
  case ENXIO:	rpc2_err = 6; break;
  case E2BIG:	rpc2_err = 7; break;
  case ENOEXEC:	rpc2_err = 8; break;
  case EBADF:	rpc2_err = 9; break;
  case ECHILD:	rpc2_err = 10; break;
  case EDEADLK:	rpc2_err = 11; break;
  case ENOMEM:	rpc2_err = 12; break;
  case EACCES:	rpc2_err = 13; break;
  case EFAULT:	rpc2_err = 14; break;
  case ENOTBLK:	rpc2_err = 15; break;
  case EBUSY:	rpc2_err = 16; break;
  case EEXIST:	rpc2_err = 17; break;
  case EXDEV:	rpc2_err = 18; break;
  case ENODEV:	rpc2_err = 19; break;
  case ENOTDIR:	rpc2_err = 20; break;
  case EISDIR:	rpc2_err = 21; break;
  case EINVAL:	rpc2_err = 22; break;
  case ENFILE:	rpc2_err = 23; break;
  case EMFILE:	rpc2_err = 24; break;
  case ENOTTY:	rpc2_err = 25; break;
  case ETXTBSY:	rpc2_err = 26; break;
  case EFBIG:	rpc2_err = 27; break;
  case ENOSPC:	rpc2_err = 28; break;
  case ESPIPE:	rpc2_err = 29; break;
  case EROFS:	rpc2_err = 30; break;
  case EMLINK:	rpc2_err = 31; break;
  case EPIPE:	rpc2_err = 32; break;
  case EDOM:	rpc2_err = 33; break;
  case ERANGE:	rpc2_err = 34; break;
  case EAGAIN:	rpc2_err = 35; break;
  case EALREADY:	rpc2_err = 37; break;
  case ENOSYS:	rpc2_err = 38; break;
  case EPROTONOSUPPORT:	rpc2_err = 43; break;
  case EOPNOTSUPP:	rpc2_err = 45; break;
  case ENOBUFS:	rpc2_err = 55; break;
  case ENOTCONN:	rpc2_err = 57; break;
  case ESHUTDOWN:	rpc2_err = 58; break;
  case ETOOMANYREFS:	rpc2_err = 59; break;
  case ETIMEDOUT:	rpc2_err = 60; break;
  case ELOOP:	rpc2_err = 62; break;
  case ENAMETOOLONG:	rpc2_err = 63; break;
  case ENOTEMPTY:	rpc2_err = 66; break;
  case EDQUOT:	rpc2_err = 69; break;

/* Coda client <-> server specific errors */
  case VSALVAGE:	rpc2_err = 101; break;
  case VNOVNODE:	rpc2_err = 102; break;
  case VNOVOL:	rpc2_err = 103; break;
  case VVOLEXISTS:	rpc2_err = 104; break;
  case VNOSERVICE:	rpc2_err = 105; break;
  case VOFFLINE:	rpc2_err = 106; break;
  case VONLINE:	rpc2_err = 107; break;
  case VBUSY:	rpc2_err = 110; break;
  case VMOVED:	rpc2_err = 111; break;
  case VNOSERVER:	rpc2_err = 112; break;
  case VLOGSTALE:	rpc2_err = 113; break;
  case EVOLUME:	rpc2_err = 158; break;
  case EINCOMPATIBLE:	rpc2_err = 198; break;
  case EINCONS:	rpc2_err = 199; break;
  case VFAIL:	rpc2_err = 200; break;

/* Advice monitor related errors, we should really not use these anymore */
// #define CAEFAIL		300	/* Unknown error related to the Advice Monitor */
// #define CAEVERSIONSKEW	301	/* Version skew between Venus and the Advice Monitor */
  case CAENOSUCHUSER:	rpc2_err = 302; break;
  case CAENOTVALID:	rpc2_err = 303; break;
  case CAENOASR:	rpc2_err = 304; break;
  case CAEUNEXPECTEDASR:	rpc2_err = 305; break;
// #define CAEASRINPROGRESS	306	/* Venus requested an ASR while another one is in progress */
  case CAEADVICEPENDING:	rpc2_err = 307; break;
  case CAENOSERVERS:	rpc2_err = 308; break;

