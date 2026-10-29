#pragma once
#include <stddef.h>
// #205: read-only probe. Finds the mount holding `path` in `mounts` (a
// /proc/mounts-format file) and returns its filesystem type and whether it is
// mounted ro. 0 = found, -1 = unreadable file or no match. Never writes.
int usbsonda_montagem(const char *mounts, const char *path, char *fs, size_t nfs, int *somente_leitura);
