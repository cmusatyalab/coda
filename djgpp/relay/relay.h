#ifndef _DJGPP_RELAY_H_
#define _DJGPP_RELAY_H_ 1

int write_relay(char *buffer, int n);
int read_relay(char *buffer);
int init_relay();
int unmount_relay();
int mount_relay();

#endif not _DJGPP_RELAY_H_

