/*
   Copyright 1997-98 Michael Callahan
   Copyright 1999 Carnegie Mellon University
   This program is free software.  You may copy it according
   to the conditions of the GNU General Public License version 2.0
   or later; see the file COPYING in the source directory.
*/

#include "vxd.h"

int open_vxd(char *vxdname, struct far_ptr *api)
{
  asm ("pushw  %%es\n\t"
       "movw   %%ds, %%ax\n\t"
       "movw   %%ax, %%es\n\t"
       "movw   $0x1684, %%ax\n\t"
       "int    $0x2f\n\t"
       "movw   %%es, %%ax\n\t"
       "popw   %%es\n\t"
       : "=a" (api->segment), "=D" (api->offset) 
       : "b" (0), "D" (vxdname));
  api->offset &= 0xffff;
  return api->offset != 0;
}

int DeviceIoControl(struct far_ptr *api, int func, void *inBuf, int inCount,
                    void *outBuf, int outCount)
{
  unsigned int err;
  struct far_ptr my_api = *api;

  if (my_api.offset == 0)
    return -1;

  asm ("lcall  %1"
       : "=a" (err)
       : "m" (my_api), "a" (func), "S" (inBuf), "b" (inCount),
       "D" (outBuf), "c" (outCount), "d" (123456));
  if (err)
    printf ("DeviceIoControl %d: err %d\n", func, err);
  return err;
}

int load_vxd(char *vxdname, char *vxdfilename, struct far_ptr *api){
    int version;
    int res;
    if (!open_vxdldr())
      return 0;
    if (!vxdldr_get_version(&version)) {
      printf ("cannot get VXDLDR version\n");
      return 0;
    }
    printf ("VXDLDR version %x\n", version);
    if ((res = vxdldr_load_device(vxdfilename))) {
      printf ("cannot load %s: VXDLDR error %d\n", vxdfilename, res);
      return 0;
    }
    if (!open_vxd(vxdname, api)) {
      printf ("Loaded %s, but could not get api\n", vxdname);
      return 0;
    }
    return 1;
}

int unload_vxd(char *vxdname){
    return vxdldr_unload_device(vxdname);	
}
