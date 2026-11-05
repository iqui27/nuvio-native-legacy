#define _POSIX_C_SOURCE 200809L
#include "logexport.h"
#include "rangediag.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void hides(const char *line, const char *secret) {
  char out[2048]; logexport_redact(line, out, sizeof out);
  assert(!strstr(out, secret));
}
int main(void) {
  char out[4096], source[] = "/tmp/nuvio-logtest-XXXXXX", path[128];
  char host[NV_RANGE_DIAG_HOST_MAX];
  nv_range_diag_host("https://person:shortpw@LONG-CDN-DELIVERY.example.net:443/private/shortpw?token=shortpw", host);
  assert(!strcmp(host, "long-cdn-delivery.example.net"));
  nv_range_diag_host("https://127.0.0.1:443/secret", host); assert(!strcmp(host, "address"));
  nv_range_diag_host("https://[2001:db8::1]/secret", host); assert(!strcmp(host, "address"));
  nv_range_diag_host("https://evil%0ahost.example/secret", host); assert(!strcmp(host, "address"));
  nv_range_diag_host("https://example.net./secret", host); assert(!strcmp(host, "example.net"));
  nv_range_diag_host(NULL, host); assert(!strcmp(host, "-"));
  const char *trace = "[range-diag] v=1 id=2 kind=chunk monoMs=1000 origHost=torrentio.strem.fun host=long-cdn-delivery.example.net nextHost=- http=429 retrySeen=1 retrySec=30 server=nginx";
  logexport_redact(trace, out, sizeof out); assert(!strcmp(trace, out));
  hides("[range-diag] host=abcdefghijklmnopqrstuvwxyz", "abcdefghijklmnopqrstuvwxyz");
  hides("[net] host=long-cdn-delivery.example.net", "long-cdn-delivery.example.net");
  hides("[range-diag] host=long-cdn-delivery.example.net/shortpw", "shortpw");
  hides("[range-diag] host=person:shortpw@long-cdn-delivery.example.net", "shortpw");
  hides("[range-diag] host=long-cdn-delivery.example.net token=shortpw", "shortpw");
  int fd = mkstemp(source); assert(fd >= 0);
  FILE *f = fdopen(fd, "wb"); assert(f);
  const char *safe = "[playback-diag] tick=123 generation=2 pos=123.456 readyBytes=268435456 availableKiB=245760";
  logexport_redact(safe, out, sizeof out); assert(!strcmp(safe, out));
  const char *range_logs[]={
    "[video-packet] epoch=2 sample=1 bytes=286395 ptsNs=7536918000000 dtsNs=7536800000000 durNs=41708000 vcl=21 irap=1 vps=1 sps=1 pps=1 rpu=0 el=0 scanned=4096 clipped=1 headerErr=0 feed=1",
    "[audio-route] profile=7 rpu=1 nativeDv=0 controllerDv=1 known=0 ordinal=-1 count=0 target=7534.103 paused=0 resumePct=0.000",
    "[dv-state] generation=3 profile=7 probe=1 path=1 loaded=1 confirmed=1 playing=1 failed=0 refusal=0 audio=0",
    "[dts-rebuffer] event=end ms=812 aheadA=1.85 aheadV=1.88 contiguousKiB=262567 reason=native-full",
    "[range-progress] start=4194303 waitMs=3012 got=2031616 gained=2031616 kept=1",
    "[dts-rebuffer] event=start reason=seek target=9282.482 contiguousKiB=4095 readyKiB=135167",
    "[dts-rebuffer] event=end ms=6430 aheadA=2.10 aheadV=2.00 contiguousKiB=20480",
    "[dts-rebuffer] event=timeout ms=20050 contiguousKiB=0 readyKiB=163840",
    "[http-setting] path=conversion old=1 selected=4 applies=next-video saved=1",
    "[http-setting] event=restore oldNative=3 native=1 oldConversion=1 conversion=4 applies=next-video saved=1",
    "[http-session] generation=17 event=stop-start converter=1 playing=1 pending=0 nativeSelected=1 conversionSelected=4",
    "[http-session] generation=17 event=stop-done nextGeneration=18 elapsedMs=250 converter=0",
    "[http-session] generation=18 event=conversion-start selected=4 dv=1 audio=0 target=12.500 paused=0",
    "[http-engine] id=3 event=range-start selected=4 workers=4 active=4 queued=12 ready=0 readyKiB=0 cooldownMs=0 elapsedMs=0",
    "[http-engine] id=3 event=media-close-done selected=4 workers=0 active=0 queued=0 ready=0 readyKiB=0 cooldownMs=0 elapsedMs=70",
    "[mp4-bd-test] P8 compatibility verified: input=0 output=1; HDR10 SPS and P8 RPU confirmed",
    "[mp4-bd-test] seek anchor: target=3835.660 BL-dts-ns=3830000000000 BL-pts-ns=3831000000000 probePackets=155 probeBytes=63349",
    "[mp4-bd-test] single P8 track=0 videos=1 version=1.0 compat=1 bl=1 el=0 rpu=1 size=3840x2160 hdr=9/16/9 hvcc=23",
    "[mp4-bd-test] first P8 RPU validated: bytes=160; metadata unchanged",
    "[mp4-seek] deferred target=12.000 pending=2.000",
    "[mp4-seek] latest target=12.000 reload=1 audio=0",
    "[mp4-bd-test] native error code=204 enabled=1 started=0 converter=0 tried=0",
    "[mp4-bd-test] BL track=0 size=3840x2160 hdr=2/2/2 fps=24000/1001 timebase=1/480000 hvcc=23; EL track=1 size=1920x1080 fps=24000/1001 timebase=1/480000 hvcc=23",
    "[mp4-bd-test] base SPS: size=3840x2160 depth=10 hdr=9/16/9; from=in-band",
    "[mp4-bd-test] restored HDR10 colour metadata from base SPS; no video decode",
    "[dts] stage=native-buffered-reposition ",
    "[dts] stage=native-seek-progress-confirmed ",
    "[dts] stage=native-seek-accepted 1234567",
    "[dts] stage=native-seek-fallback no presentation progress",
    "[dts] stage=startup-audio-reuse ",
    "[dts] dv-seek-prime ms=2100 kind=1 pts=4645.563 bytes=327759",
    "[dts] stage=dv-audio-prepared-before-unload ",
    "[dts] stage=audio-format-reload ",
    "[dts] dv-audio-reload-prime target=3241.822 ms=4540 packet=1 kind=1 bytes=327759",
    "[dts] dv-fixed-audio mode=1 input=eac3 output=aac channels=2 rate=48000",
    "[dts] audio-policy requested=1 fixed=1 dv=1 seamless=1 compiled=1",
    "[dts] audio-policy requested=0 fixed=0 dv=1 seamless=1 compiled=1",
    "[range-reuse] target=70617346365 retained=60 retainedBytes=251658180",
    "[range-reuse] target=123 retained=2 retainedBytes=8388606 interrupted=1",
    "[range-reuse] target=67108971 retained=12 retainedBytes=50331636 interrupted=0 refillSlots=4 recycled=3",
    "[dts] dv-audio-hold-release reason=ready ms=1200 aheadA=2.10 aheadV=2.00 contiguousKiB=20480",
    "[dts] native-seek-fallback reason=no-progress target=7786.988 waitMs=4000 totalMs=12000",
    "[dts] dv-fixed-audio mode=2 input=ac3 output=ac3 channels=6 rate=48000",
    "[dv] file: profile=7 level=6 rpu=1 el=1 bl=1 compat=6",
    "[dts] stage=dv-native-load profile=8 compatibility=1 bl=1 el=0 rpu=1 reused=1 target=123.000",
    "[video] HDR do pipeline: HDR10 (fonte afirmava DV=1)",
    "[video] HDR do pipeline: DolbyVision (fonte afirmava DV=1)",
    "[range-stall] start=2456400780 idleMs=3000 got=65536",
    "[range-http] start=10737418240 end=10738466815 curl=28 http=206 got=524288 valid=1 overflow=0 responseStart=10737418240 responseEnd=10738466815 total=30000000000",
    "[range-result] start=4194304 end=5242879 attempts=2 ok=1 http=206 bytes=1048576 ms=15020",
    "[range-read] pos=4194304 waitMs=2150 http=206 ok=1 readyBytes=64000000 contiguousBytes=64000000",
    "[range-reset] reason=seek pos=100000000 ready=32 readyBytes=33554432 active=4",
    "[range-route] cached=1 sdr=1 cross=1",
    "[range-cooldown] http=429 retrySec=28 waitMs=28000 lanes=shared",
    "[range-gap] start=49129332191 later=18 laterBytes=75497454 budgetMs=3000",
    "[range-budget] start=49129332191 waitMs=3023 got=1208320 budgetMs=3000"};
  for(size_t i=0;i<sizeof range_logs/sizeof range_logs[0];i++) {
    logexport_redact(range_logs[i],out,sizeof out);assert(!strcmp(out,range_logs[i]));
  }
  hides("[dts] stage=native-seek-progress-confirmedSECRETVALUE", "SECRETVALUE");
  hides("[net] native-seek-progress-confirmedSECRETVALUE", "SECRETVALUE");
  hides("[dts] stage=native-seek-progress-confirmed token=shortpw", "shortpw");
  hides("[dts] stage=dv-audio-prepared-before-unloadSECRETVALUE", "SECRETVALUE");
  hides("[net] dv-audio-prepared-before-unload", "dv-audio-prepared-before-unload");
  hides("[dts] stage=dv-audio-prepared-before-unload token=shortpw", "shortpw");
  hides("[net] https://person:shortpw@host.example/key/movie?api=x#y", "person");
  hides("[net] HtTpS://private.example/shortpw", "shortpw");
  hides("[net] https:\\/\\/private.example/key", "private.example");
  hides("[net] url:https://a/shortpw", "shortpw");
  hides("[addon] authorization: Basic YTpi", "YTpi");
  hides("[addon] Cookie: a=shortpw; b=123", "shortpw");
  hides("[addon] apiKey=shortpw", "shortpw");
  hides("[addon] accessToken=shortpw", "shortpw");
  hides("[addon] {\"password\":\"shortpw\"}", "shortpw");
  hides("[addon] url=http%3A%2F%2Fprivate.example%2Fshortpw", "shortpw");
  hides("[auth] pin=1234", "1234");
  hides("[auth] username=someone", "someone");
  hides("[net] contact foo@example.test", "foo@example.test");
  hides("[net] 192.168.1.100", "192.168.1.100");
  hides("[net] aa:bb:cc:dd:ee:ff", "aa:bb:cc:dd:ee:ff");
  hides("[net] 2001:db8:1234::abcd", "2001:db8:1234::abcd");
  hides("[net] 1234567890abcdef1234567890abcdef", "1234567890abcdef1234567890abcdef");
  hides("[net] /media/private-user/file.mkv", "private-user");
  hides("[net] eyJabc.def.ghi", "eyJabc");
  char tiny[3] = {1,2,3}; logexport_redact("password=shortpw", tiny, sizeof tiny); assert(!tiny[2]);
  fprintf(f, "%s\n[auth] password:  \nshortpw\n  \"secondpw\"\n[video] error=300\n", safe);
  /* An oversized line must be omitted as a whole, never export its chunks. */
  for (int i = 0; i < 70000; i++) fputc('x', f);
  fputs("ENDSECRET\n[video] recovery=1\n", f); fclose(f);
  assert(logexport_create(source, path, sizeof path));
  struct stat st; assert(!stat(path, &st) && (st.st_mode & 0777) == 0600);
  f = fopen(path, "rb"); assert(f); size_t n = fread(out, 1, sizeof out - 1, f); out[n] = 0; fclose(f);
  assert(strstr(out, safe) && strstr(out, "[video] error=300") && strstr(out, "recovery=1"));
  assert(!strstr(out,"Experimental playback build:"));
  assert(!strstr(out, "shortpw") && !strstr(out, "secondpw") && !strstr(out, "ENDSECRET"));
  assert(strstr(out, "oversized log line")); unlink(path);
  /* Taking a tail cannot leak the partial first line. */
  f = fopen(source, "wb"); assert(f);
  for (int i = 0; i < 17 * 1024 * 1024; i++) fputc('x', f);
  fputs("TRUNCATEDSECRET\n[video] tail=1\n", f); fclose(f);
  assert(logexport_create(source, path, sizeof path));
  f = fopen(path, "rb"); assert(f); n = fread(out, 1, sizeof out - 1, f); out[n] = 0; fclose(f);
  assert(strstr(out, "tail=1") && !strstr(out, "TRUNCATEDSECRET"));
  unlink(path); unlink(source);
  assert(!logexport_create("/nonexistent/nuvio.log", path, sizeof path) && !path[0]);
  puts("PASS: log export redaction, numeric diagnostics, multiline secrets, bounds, private permissions");
}
