#define _POSIX_C_SOURCE 200809L
#include "logexport.h"
#include "rangediag.h"
#ifndef NV_VERSAO
#define NV_VERSAO "2.0.8"
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define SNAPSHOT_MAX (16L * 1024 * 1024)
#define LINE_MAXIMUM 65536

static int wordchar(unsigned char c) { return isalnum(c) || c == '_' || c == '-'; }
static int sensitive(const char *s) {
  static const char *words[] = {
    "authorization", "cookie", "token", "password", "passwd", "secret",
    "apikey", "api_key", "api-key", "access_key", "credential", "bearer",
    "email", "e-mail", "username", "user_id", "userid", "account", "login",
    "deviceid", "device_id", "machineid", "chave", "senha", "pin", "key",
    "client_id", "clientid", "client_secret", "privatekey", "auth", "authkey",
    "user", "device", "uuid", "serial", "mac", "ip", "session_id", "sessionid", "pairing", "jwt"
  };
  char lower[LINE_MAXIMUM + 1];
  size_t n = strlen(s), i;
  if (n > LINE_MAXIMUM) return 1;
  for (i = 0; i <= n; i++) lower[i] = (char)tolower((unsigned char)s[i]);
  for (i = 0; i < sizeof words / sizeof words[0]; i++) {
    const char *p = lower;
    size_t m = strlen(words[i]);
    while ((p = strstr(p, words[i]))) {
      if ((p == lower || !wordchar((unsigned char)p[-1])) &&
          !wordchar((unsigned char)p[m])) return 1;
      p++;
    }
  }
  if (strstr(lower, "token") || strstr(lower, "secret") || strstr(lower, "password") ||
      strstr(lower, "apikey") || strstr(lower, "api_key")) return 1;
  /* Encoded/escaped URLs and JSON payloads can hide credentials across fields.
   * Omit that payload line rather than trying to parse arbitrary wire formats. */
  if (strstr(lower, "%3a") || strstr(lower, "\\u00") ||
      strstr(lower, "\\\"") || strstr(lower, "\":") ||
      strstr(lower, "magnet:")) return 1;
  { const char *p = lower;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '"' || *p == '\'') return 1;
  }
  return 0;
}

/* Exact built-in event names only. A long arbitrary token after stage= must
 * still be hidden, even if it resembles one of these public diagnostic names. */
static int public_stage(const char *line,const char *p,size_t len) {
  static const char *names[]={"native-buffered-reposition","native-seek-progress-confirmed",
    "native-seek-accepted","native-seek-unavailable","native-seek-refused",
    "native-seek-fallback","startup-audio-reuse","startup-audio-reopen",
    "dv-audio-prepared-before-unload","audio-format-reload"};
  if(strncmp(line,"[dts] stage=",12) || p!=line+12)return 0;
  for(size_t i=0;i<sizeof names/sizeof names[0];i++)
    if(strlen(names[i])==len && !memcmp(p,names[i],len))return 1;
  return 0;
}

/* Only the host-only fields emitted by the range diagnostic may retain long
 * DNS names. URLs, addresses and arbitrary long identifiers stay redacted. */
static int public_range_host(const char *line, const char *p, size_t len) {
  static const char *fields[] = {" origHost=", " host=", " nextHost="};
  if (strncmp(line, "[range-diag] ", 13) || !nv_range_diag_dns(p, len)) return 0;
  for (size_t i = 0; i < sizeof fields / sizeof *fields; i++) {
    size_t n = strlen(fields[i]);
    if ((size_t)(p - line) >= n && !memcmp(p - n, fields[i], n)) return 1;
  }
  return 0;
}

static void append(char *out, size_t cap, size_t *n, const char *s, size_t len) {
  if (!cap) return;
  if (len > cap - 1 - *n) len = cap - 1 - *n;
  memcpy(out + *n, s, len); *n += len; out[*n] = 0;
}

void logexport_redact(const char *in, char *out, size_t cap) {
  size_t n = 0;
  const char *p = in ? in : "";
  if (!cap) return;
  out[0] = 0;
  if (sensitive(p)) {
    /* Retain only a short, non-sensitive category label. */
    const char *end = *p == '[' ? strchr(p, ']') : NULL;
    if (end && end - p < 48) {
      const char *q;
      int safe = 1;
      for (q = p + 1; q < end; q++) if (!isalpha((unsigned char)*q) && *q != '-') safe = 0;
      if (safe) { append(out, cap, &n, p, (size_t)(end - p + 1)); append(out, cap, &n, " ", 1); }
    }
    append(out, cap, &n, "[REDACTED: sensitive field or payload]", sizeof "[REDACTED: sensitive field or payload]" - 1);
    return;
  }
  while (*p) {
    const char *end = p;
    size_t len;
    int hide = 0, digits = 0, letters = 0, hexonly = 1;
    if (!strncasecmp(p, "http://", 7) || !strncasecmp(p, "https://", 8) ||
        !strncmp(p, "HTTP://", 7) || !strncmp(p, "HTTPS://", 8) ||
        !strncmp(p, "file://", 7) || !strncmp(p, "ftp://", 6)) {
      while (*end && !isspace((unsigned char)*end) && *end != '"' && *end != '\'') end++;
      append(out, cap, &n, "[URL]", 5); p = end; continue;
    }
    if (wordchar((unsigned char)*p) || *p == '@' || *p == '/' || *p == '.') {
      while (wordchar((unsigned char)*end) || *end == '.' || *end == '@' || *end == '/' || *end == '+' || *end == '%' || *end == ':') {
        if (isdigit((unsigned char)*end)) digits++;
        if (isalpha((unsigned char)*end)) letters++;
        if (!isxdigit((unsigned char)*end)) hexonly = 0;
        end++;
      }
      len = (size_t)(end - p);
      const char *scheme = strstr(p, "://");
      hide = (scheme && scheme < end) || (len >= 24 && !public_stage(in,p,len) && !public_range_host(in,p,len)) || (hexonly && len >= 16) || memchr(p, '@', len) != NULL;
      /* IP addresses (including private addresses), filesystem paths and JWTs. */
      int dots = 0, colons = 0;
      const char *q;
      for (q = p; q < end; q++) { dots += *q == '.'; colons += *q == ':'; }
      if (colons >= 2 || (digits && !letters && dots >= 3) || *p == '/' ||
          (len >= 3 && !strncmp(p, "eyJ", 3))) hide = 1;
      append(out, cap, &n, hide ? "[REDACTED]" : p, hide ? 10 : len);
      p = end; continue;
    }
    if ((unsigned char)*p >= 32 || *p == '\t') append(out, cap, &n, p, 1);
    p++;
  }
}

int logexport_create(const char *source, char *path, size_t size) {
  FILE *in = NULL, *out = NULL;
  char *line = NULL, *clean = NULL;
  long total, start, left;
  int fd, ok = 0, privateBlock = 0;
  size_t used = 0;
  if (!path || size < 32) return 0;
  path[0] = 0;
  if (!source) return 0;
  fflush(stdout); fflush(stderr);
  in = fopen(source, "rb");
  if (!in || fseek(in, 0, SEEK_END) || (total = ftell(in)) < 0) goto done;
  start = total > SNAPSHOT_MAX ? total - SNAPSHOT_MAX : 0;
  if (fseek(in, start, SEEK_SET)) goto done;
  snprintf(path, size, "/tmp/nuvio-export-XXXXXX");
  fd = mkstemp(path);
  if (fd < 0) { path[0] = 0; goto done; }
  fchmod(fd, 0600);
  out = fdopen(fd, "wb");
  if (!out) { close(fd); goto done; }
  line = malloc(LINE_MAXIMUM + 1); clean = malloc(LINE_MAXIMUM + 128);
  if (!line || !clean) goto done;
  fprintf(out, "Nuvio Legacy " NV_VERSAO " LG custom port\nSnapshot UTC epoch: %lld\n"
    "All app log categories; frozen snapshot, not live.\n"
    "Recognized credentials, URLs, personal identifiers and payloads are masked.\n"
    "Automatic redaction is best effort: review before sharing publicly.\n"
    "Source bytes: %ld; retained input limit: %ld; older bytes omitted: %ld\n\n",
    (long long)time(NULL), total, SNAPSHOT_MAX, start);
#ifdef NV_PLAYBACK_TEST_BUILD
  fprintf(out,"Experimental playback build: v%d\n\n",NV_PLAYBACK_TEST_BUILD);
#endif
  left = total - start;
  /* Discard an incomplete first line when taking the tail. */
  if (start) { int c; while (left-- > 0 && (c = fgetc(in)) != EOF && c != '\n') {} }
  while (left > 0) {
    int c = fgetc(in), oversized = 0;
    if (c == EOF) break;
    left--;
    while (c != '\n') {
      if (used < LINE_MAXIMUM) line[used++] = c ? (char)c : ' ';
      else oversized = 1;
      if (!left) break;
      c = fgetc(in); left--;
      if (c == EOF) break;
    }
    line[used] = 0;
    if (ftell(out) > SNAPSHOT_MAX - LINE_MAXIMUM - 256) {
      fputs("[log-export] Output limit reached; remaining lines omitted.\n", out); break;
    }
    if (!used || (line[0] == '[' && isalpha((unsigned char)line[1]))) privateBlock = 0;
    if (privateBlock) fputs("[REDACTED: sensitive continuation]\n", out);
    else if (oversized) fputs("[REDACTED: oversized log line]\n", out);
    else { logexport_redact(line, clean, LINE_MAXIMUM + 128); fprintf(out, "%s\n", clean); }
    size_t end = used;
    while (end && isspace((unsigned char)line[end - 1])) end--;
    if (sensitive(line) && end && strchr(":={[,", line[end - 1])) privateBlock = 1;
    used = 0;
  }
  ok = !ferror(in) && !ferror(out);
done:
  if (in) fclose(in);
  if (out && fclose(out)) ok = 0;
  free(line); free(clean);
  if (!ok && path[0]) { unlink(path); path[0] = 0; }
  return ok;
}
