// #557: some hosts ignore Range and answer 200 with the whole file; the TV
// player then cannot seek and fails silently. One Range: bytes=0-1 probe per
// source tells the player to say so.
#ifndef NV_SEEKPROBE_H
#define NV_SEEKPROBE_H

// Blocking probe. 1 = host ignored Range (HTTP 200); 0 = ranges work or unknown
// (error, private/loopback origin, HLS playlist): never warn on doubt.
int seekprobe_sem_range(const char *url, const char *headers);

// Async, once per url. seekprobe_resultado: -1 pending/none, 0 ok, 1 no range.
void seekprobe_iniciar(const char *url, const char *headers);
int  seekprobe_resultado(const char *url);

#endif
