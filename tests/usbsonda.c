// #205: usbsonda_montagem finds the filesystem type + read-only flag of the
// mount holding a path, from /proc/mounts-format text (fixture here).
#include "usbsonda.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  char fs[32], f[] = "/tmp/nv-usbsonda-XXXXXX";
  int ro = -1, fd = mkstemp(f);
  FILE *o = fdopen(fd, "w");
  fputs("rootfs / ext4 rw,relatime 0 0\n"
        "/dev/sda1 /tmp/usb/sda1 ntfs ro,nosuid 0 0\n"
        "/dev/sdb1 /tmp/usb/sdb1 exfat rw,nosuid 0 0\n"
        "/dev/sdb2 /tmp/usb/sdb1x vfat rw 0 0\n"
        "/dev/sdc1 /tmp/usb/with\\040space vfat rw 0 0\n", o);
  fclose(o);
  assert(usbsonda_montagem(f, "/tmp/usb/sda1/p2p", fs, sizeof fs, &ro) == 0);
  assert(!strcmp(fs, "ntfs") && ro == 1);
  assert(usbsonda_montagem(f, "/tmp/usb/sdb1", fs, sizeof fs, &ro) == 0);
  assert(!strcmp(fs, "exfat") && ro == 0);       // not fooled by sdb1x prefix
  assert(usbsonda_montagem(f, "/tmp/usb/with space/p2p", fs, sizeof fs, &ro) == 0);
  assert(!strcmp(fs, "vfat"));                   // \040 decoded
  assert(usbsonda_montagem(f, "/data/x", fs, sizeof fs, &ro) == 0);
  assert(!strcmp(fs, "ext4"));                   // falls back to the / mount
  assert(usbsonda_montagem("/nonexistent/mounts", "/x", fs, sizeof fs, &ro) == -1);
  unlink(f);
  puts("usbsonda ok");
  return 0;
}
