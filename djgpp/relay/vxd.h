#ifndef _DJGPP_VXD_H_
#define _DJGPP_VXD_H_ 1


struct far_ptr {
  unsigned int offset; 
  unsigned short segment;
};

int open_vxd(char *vxdname, struct far_ptr *api);
int DeviceIoControl(struct far_ptr *api, int func, void *inBuf, int inCount,
                    void *outBuf, int outCount);
int load_vxd(char *vxdname, char *vxdfilename, struct far_ptr *api);
int unload_vxd(char *vxdname);
#endif	not _DJGPP_VXD_H_
