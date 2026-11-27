// TorBox burn-in (tbburn.h): the pure helpers. Stubs stand in for settings,
// debrid, the account lookup and the network.
#include "tbburn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <time.h>

static int falhas;
#define CHECA(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

// ---- stubs -----------------------------------------------------------------
static int pro = 1, ligado = 1, chave = 1;
int ajustes_tb_pro(void) { return pro; }
int ajustes_leg_queimar(void) { return ligado && pro; }
int debrid_tb_disponivel(void) { return chave; }
int debrid_eh_sem_plano(int st, const char *c) { return st == 403 && c && strstr(c, "PLAN"); }
void debrid_tb_redigir(char *t) {
  char *p;
  while (t && (p = strstr(t, "SECRETKEY")) != NULL) { memcpy(p, "***", 3); memmove(p + 3, p + 9, strlen(p + 9) + 1); }
}
static char ultRota[512];
static int sumido500, criacoes;   // createstream: item 500 is gone (log 12)
static int planoRecusa;           // createstream: TorBox refuses the plan
static int pronto900;             // createstream for the re-added item 900 gives a playlist
static int sumido900;             // ...or ITEM_NOT_FOUND while TorBox registers it
static int falha950;              // createstream for item 950 fails this many times (not playable)
char *debrid_tb_api_get(const char *r, int p, int *st) {
  (void)p; snprintf(ultRota, sizeof ultRota, "%s", r ? r : ""); if (st) *st = 0;
  if (r && strstr(r, "createstream")) criacoes++;
  if (planoRecusa && r && strstr(r, "createstream")) {
    if (st) *st = 403;
    return strdup("{\"success\":false,\"error\":\"PLAN_RESTRICTED_FEATURE\"}");
  }
  if (sumido500 && r && strstr(r, "createstream?id=500&")) {
    if (st) *st = 404;
    return strdup("{\"success\":false,\"error\":\"ITEM_NOT_FOUND\",\"detail\":\"Item not found.\",\"data\":null}");
  }
  if (sumido900 && !pronto900 && r && strstr(r, "createstream?id=900&")) {
    if (st) *st = 404;
    return strdup("{\"success\":false,\"error\":\"ITEM_NOT_FOUND\",\"detail\":\"Item not found.\",\"data\":null}");
  }
  if (r && strstr(r, "createstream?id=950&")) {
    if (falha950 > 0) { falha950--; if (st) *st = 500; return strdup("{\"success\":false,\"error\":\"UNKNOWN_ERROR\"}"); }
    if (st) *st = 200;
    return strdup("{\"success\":true,\"data\":{\"hls_url\":\"https://h.tb-cdn.io/s/950/playlist.m3u8\"}}");
  }
  if (pronto900 && r && strstr(r, "createstream?id=900&")) {
    if (st) *st = 200;
    return strdup("{\"success\":true,\"data\":{\"hls_url\":\"https://h.tb-cdn.io/s/900/playlist.m3u8\"}}");
  }
  return NULL;
}
/* Re-add (owner 19/11/2026): createtorrent, cached only. */
static char adicionouHash[48]; static int adicionarDa = -1, adicoes;
int debrid_tb_adicionar(const char *h) {
  adicoes++; snprintf(adicionouHash, sizeof adicionouHash, "%s", h ? h : "");
  return adicionarDa;
}
/* Owner 24/11/2026 (logs 19-20): a stuck item is removed before the re-add. */
static int apagou, apagouItem, apagarDa = 1;
int debrid_tb_apagar(int item) { apagou++; apagouItem = item; return apagarDa; }
/* Only a cached hash is ever removed (tbabrir.c's rule): 1 = cached. */
static int emCache = 1;
int debrid_tb_em_cache(const char *h) { (void)h; return emCache; }
/* The cached-first lookup with the item's hash: item 500, hash "ab..". */
static int hashAchaItem = 500;
int partes_achar_exato_hash(const char *u, const char *h, const char *a, uint64_t b, int *i, int *f, int *un, char *c, unsigned n, char hi[41]) {
  (void)u; (void)h; (void)a; (void)b;
  if (un) *un = 0;
  if (hi) hi[0] = 0;
  if (!hashAchaItem) return 0;
  if (i) *i = hashAchaItem;
  if (f) *f = 7;
  if (c && n) snprintf(c, n, "filename");
  if (hi) snprintf(hi, 41, "%s", "abababababababababababababababababababab");
  return 1;
}
static char viuArquivo[512]; static uint64_t viuBytes;
// The account lookup: misses unless partesAchar; partesAtrasoMs = how long it takes.
static int partesAchar, partesUsenet, partesAtrasoMs, conta = 1;
static char partesHashItem[41];   // the found item's hash the lookup gives ("" = none)
static volatile int partesN;
unsigned debrid_tb_geracao(void) { return (unsigned)conta; }
void rede_fio_fechar(void) {}
int partes_achar_exato(const char *u, const char *h, const char *a, uint64_t b, int *i, int *f, int *un, char *c, unsigned n, char *hi) {
  (void)u; (void)h;
  if (hi) hi[0] = 0;
  snprintf(viuArquivo, sizeof viuArquivo, "%s", a ? a : ""); viuBytes = b;
  if (partesAtrasoMs) { struct timespec t = {partesAtrasoMs / 1000, (long)(partesAtrasoMs % 1000) * 1000000L}; nanosleep(&t, NULL); }
  __atomic_add_fetch(&partesN, 1, __ATOMIC_SEQ_CST);
  if (un) *un = 0;
  if (!partesAchar) return 0;
  if (un) *un = partesUsenet;
  if (i) *i = 500;
  if (f) *f = 7;
  if (c && n) snprintf(c, n, "hash");
  if (hi) snprintf(hi, 41, "%s", partesHashItem);
  return 1;
}
// Fresh lists only (after ITEM_NOT_FOUND): what the test says.
static int frescoItem, frescoN, frescoAPartir;   // frescoAPartir: found only from that call on
static char frescoHash[48];
int partes_achar_exato_fresco(const char *u, const char *h, const char *a, uint64_t b, int *i, int *f, int *un, char *c, unsigned n) {
  (void)u; (void)a; (void)b;
  frescoN++;
  snprintf(frescoHash, sizeof frescoHash, "%s", h ? h : "");
  if (un) *un = 0;
  if (!frescoItem || frescoN < frescoAPartir) return 0;
  if (i) *i = frescoItem;
  if (f) *f = 7;
  if (c && n) snprintf(c, n, "filename");
  return 1;
}
/* Owner 24/11/2026 (log 24): pegarIds and the looks after an add go through
 * partes_achar_exato_x. semPrimeiro / semFresco: the hash's torrent is listed
 * without the playing file (item id). Every fresh look after an add must come
 * right after partes_listas_renovar (a list from before the add is never
 * reused): listaVelha counts the ones that didn't. */
static int semPrimeiro, semFresco, renovou, listaVelha;
static struct timespec frescoEm;
void partes_listas_renovar(void) { renovou = 1; }
int partes_achar_exato_x(const char *u, const char *h, const char *a, uint64_t b, int fresco, int *i, int *f, int *un, char *c,
                         unsigned n, char *hi, int *sem) {
  if (sem) *sem = 0;
  if (!fresco) {
    int v = partes_achar_exato(u, h, a, b, i, f, un, c, n, hi);
    if (!v && sem) *sem = semPrimeiro;
    return v;
  }
  if (!renovou) listaVelha++;
  renovou = 0;
  clock_gettime(CLOCK_MONOTONIC, &frescoEm);
  if (hi) hi[0] = 0;
  { int v = partes_achar_exato_fresco(u, h, a, b, i, f, un, c, n);
    if (!v && sem) *sem = semFresco;
    return v; }
}
int partes_hashes_texto(const char *s, char saida[][41], int max) {
  int k = 0;
  for (const char *p = s; p && *p && k < max; p++) {
    int L = 0;
    while (L < 40 && ((p[L] >= '0' && p[L] <= '9') || (p[L] >= 'a' && p[L] <= 'f'))) L++;
    if (L == 40 && !((p[40] >= '0' && p[40] <= '9') || (p[40] >= 'a' && p[40] <= 'f')) &&
        (p == s || !((p[-1] >= '0' && p[-1] <= '9') || (p[-1] >= 'a' && p[-1] <= 'f')))) {
      memcpy(saida[k], p, 40); saida[k][40] = 0; k++; p += 39;
    }
  }
  return k;
}
static unsigned msDesde(const struct timespec *t0) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)((t.tv_sec - t0->tv_sec) * 1000 + (t.tv_nsec - t0->tv_nsec) / 1000000);
}
static void dormir(int ms) { struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000L}; nanosleep(&t, NULL); }
// Waits for the burn-in request to end (no playlist in these stubs: TBB_FALHOU).
static int viuEsperando;
static int fimPedido(void) {
  int e;
  viuEsperando = 0;
  for (int k = 0; k < 500 && ((e = tbburn_resultado(NULL, 0, NULL, 0)) == TBB_BUSCANDO || e == TBB_ESPERANDO); k++) {
    if (e == TBB_ESPERANDO) viuEsperando = 1;
    dormir(10);
  }
  return tbburn_resultado(NULL, 0, NULL, 0);
}
static int partesEsperar(int alvo) { for (int k = 0; k < 500 && partesN < alvo; k++) dormir(10); dormir(30); return partesN; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; (void)st; return NULL; }
int rede_torbox_em_voo(int *p) { if (p) *p = 0; return 0; }
// In-memory data files, one profile, a two-letter language match.
static char dadoNome[64], *dado;
char *dados_ler(const char *n) { return dado && !strcmp(n, dadoNome) ? strdup(dado) : NULL; }
int dados_gravar(const char *n, const char *c) { snprintf(dadoNome, sizeof dadoNome, "%s", n); free(dado); dado = strdup(c); return 1; }
static int perfil = 1;
int perfis_ativo(void) { return perfil; }
int ling_casa(const char *a, const char *b) { return a && b && a[0] && b[0] && !strncasecmp(a, b, 2); }

int main(void) {
  char out[4096], tmp[256];

  // 1) The playlist URL inside JSON, escaped slashes, after other URLs.
  CHECA(tbburn_achar_m3u8("{\"success\":true,\"data\":{\"poster\":\"https:\\/\\/x.io\\/a.jpg\","
                          "\"hls_url\":\"https:\\/\\/flux-007.wnam.tb-cdn.io\\/stream\\/abc\\/null\\/0\\/1\\/playlist.m3u8?token=SECRETKEY&scrobbling_enabled=False\"}}",
                          out, sizeof out));
  CHECA(!strcmp(out, "https://flux-007.wnam.tb-cdn.io/stream/abc/null/0/1/playlist.m3u8?token=SECRETKEY&scrobbling_enabled=False"));
  // ".m3u8" only in the query is not a playlist.
  CHECA(!tbburn_achar_m3u8("{\"u\":\"https://a.io/x?file=y.m3u8\"}", out, sizeof out) && !out[0]);
  // Plain text answer.
  CHECA(tbburn_achar_m3u8("ok https://h.tb-cdn.io/s/1/playlist.m3u8\n", out, sizeof out) &&
        !strcmp(out, "https://h.tb-cdn.io/s/1/playlist.m3u8"));
  CHECA(!tbburn_achar_m3u8(NULL, out, sizeof out));
  // Log 15 (owner 23/11/2026): createstream's flags in the "stream ready" line.
  { const char *j = "{\"success\":true,\"data\":{\"is_transcoding\":false, \"needs_transcoding\" : true,\"region\":\"wnam\"}}";
    CHECA(tbburn_json_bool(j, "needs_transcoding") == 1 && tbburn_json_bool(j, "is_transcoding") == 0);
    CHECA(tbburn_json_bool(j, "region") == -1 && tbburn_json_bool(j, "nope") == -1 && tbburn_json_bool(NULL, "x") == -1);
    CHECA(tbburn_json_bool("{\"needs_transcoding\":\"true\"}", "needs_transcoding") == -1); }
  CHECA(!tbburn_achar_m3u8("{\"data\":null}", out, sizeof out));
  // Unterminated string: no crash, nothing found.
  CHECA(!tbburn_achar_m3u8("{\"u\":\"https://a.io/p.m3u8", out, sizeof out));

  // 2) JSON string fields anywhere; non-string values skipped.
  CHECA(tbburn_json_texto("{\"data\":{\"presigned_token\":\"p\\/q\",\"token\":\"tt\"}}", "presigned_token", tmp, sizeof tmp) &&
        !strcmp(tmp, "p/q"));
  CHECA(tbburn_json_texto("{\"token\": 5, \"x\":{\"token\" : \"abc\"}}", "token", tmp, sizeof tmp) && !strcmp(tmp, "abc"));
  CHECA(!tbburn_json_texto("{\"token\":null}", "token", tmp, sizeof tmp) && !tmp[0]);
  // A key that is a suffix of another is not matched ("user_token" vs "token").
  CHECA(!tbburn_json_texto("{\"user_token\":\"u\"}", "token", tmp, sizeof tmp));

  // 2b) What goes to the log: no token value, no query string, URLs cut to
  // the host; the rest of the answer stays readable.
  snprintf(out, sizeof out, "%s",
           "{\"success\":true,\"detail\":\"Stream created successfully.\",\"data\":{\"hls_url\":"
           "\"https:\\/\\/s7.tb-cdn.st\\/hls\\/5f3a9c\\/playlist.m3u8?token=eyJhbGciOi.J9&user=12345\","
           "\"presigned_token\":\"pst_9f8e7d6c5b4a\",\"token\":\"0a1b2c3d-4e5f\",\"user_token\" : \"ut\\/77\\\"x\",\"title\":\"Show.S03E02\"}}");
  tbburn_log_mascarar(out);
  CHECA(!strstr(out, "pst_9f8e") && !strstr(out, "0a1b2c3d") && !strstr(out, "eyJhbGci") && !strstr(out, "12345") &&
        !strstr(out, "5f3a9c") && !strstr(out, "77") && strstr(out, "s7.tb-cdn.st") &&
        strstr(out, "Stream created successfully.") && strstr(out, "\"title\":\"Show.S03E02\"") &&
        strstr(out, "\"presigned_token\":\"***\""));
  snprintf(out, sizeof out, "%s", "#EXTM3U|#EXT-X-STREAM-INF:BANDWIDTH=8000000,RESOLUTION=1920x1080|video/index.m3u8?token=abcdef&e=99|"
                                  "https://user:pw@h.tb-cdn.st:8443/a/b.m3u8?x=1|");
  tbburn_log_mascarar(out);
  CHECA(!strcmp(out, "#EXTM3U|#EXT-X-STREAM-INF:BANDWIDTH=8000000,RESOLUTION=1920x1080|video/index.m3u8?***|https://***@h.tb-cdn.st:8443/...|"));
  snprintf(out, sizeof out, "%s", "token=ab https://h.io ?a=b");   // values shorter than the mask: starred, never longer
  tbburn_log_mascarar(out);
  CHECA(!strcmp(out, "token=** https://h.io ?***"));
  tbburn_log_mascarar(NULL);

  // 3) The createstream route: indices per kind, scrobbling off, never the key.
  tbburn_rota_criar(tmp, sizeof tmp, 123, 4, 0, 1, 0);
  CHECA(!strcmp(tmp, "stream/createstream?id=123&file_id=4&type=torrent&chosen_subtitle_index=1&chosen_audio_index=0&scrobbling_enabled=false"));
  tbburn_rota_criar(tmp, sizeof tmp, 9, 0, 1, 2, -1);
  CHECA(strstr(tmp, "type=usenet") && strstr(tmp, "chosen_audio_index=0"));

  // 4) Plan refusals.
  CHECA(tbburn_eh_recusa_plano(403, "{\"error\":\"PLAN_RESTRICTED_FEATURE\"}"));
  CHECA(tbburn_eh_recusa_plano(200, "{\"detail\":\"This feature is only available for Pro users.\"}"));
  CHECA(!tbburn_eh_recusa_plano(200, "{\"success\":true}"));
  CHECA(!tbburn_eh_recusa_plano(500, NULL));

  // 5) Eligibility: Settings, plan, key, playback row.
  pro = 1; ligado = 1; chave = 1;
  CHECA(tbburn_pode());
  tbburn_definir_escolha(0); CHECA(!tbburn_pode());          // playback row Off
  tbburn_definir_escolha(1); ligado = 0; CHECA(tbburn_pode()); // row On beats Settings Off
  tbburn_definir_escolha(-1); CHECA(!tbburn_pode());          // follows Settings (Off)
  ligado = 1; pro = 0; tbburn_definir_escolha(1); CHECA(!tbburn_pode());   // Standard: never
  pro = 1; chave = 0; CHECA(!tbburn_pode());                  // no key
  chave = 1;
  // The row carries to the next episode, and resets on a real exit.
  tbburn_definir_escolha(0); tbburn_manter_proximo(); tbburn_sessao_nova(); CHECA(tbburn_escolha() == 0);
  tbburn_sessao_nova(); CHECA(tbburn_escolha() == -1);

  // 6) Row visibility: needs an embedded ASS track in the playing file.
  tbburn_fonte("https://a/b.mkv", "", "b.mkv", 1);
  CHECA(!tbburn_linha_visivel());
  tbburn_arquivo_tem_ass(1); CHECA(tbburn_linha_visivel());
  pro = 0; CHECA(!tbburn_linha_visivel()); pro = 1;
  // Known file ids skip the lookup (a part picked in "files of the source").
  tbburn_ids("https://a/b.mkv", 0, 77, 3);
  CHECA(tbburn_linha_visivel());
  // Owner 19/11: the row stays after a lookup that did not find the file.
  tbburn_fonte("https://a/nao.mkv", "", "nao.mkv", 2);
  tbburn_arquivo_tem_ass(1);
  tbburn_pedir("https://a/nao.mkv", 0, 0);
  CHECA(fimPedido() == TBB_FALHOU && tbburn_linha_visivel());   // no hash: one more look 7 s into playback, then FALHOU

  // 7) Reuse last link: the player opens the saved URL with no add-on details;
  // the saved record fills them (owner's log 12/11, file=- size=0).
  tbburn_fonte_guardada("https://x/saved.mkv", "", "S01E08.mkv", 7074247003ULL);
  tbburn_fonte("https://x/saved.mkv", "", "", 0);
  tbburn_arquivo_tem_ass(1);
  CHECA(tbburn_linha_visivel());
  tbburn_pedir("https://x/saved.mkv", 0, 1);
  CHECA(fimPedido() == TBB_FALHOU);
  CHECA(!strcmp(viuArquivo, "S01E08.mkv") && viuBytes == 7074247003ULL);
  // A different URL does not take the saved record.
  tbburn_fonte("https://x/other.mkv", "", "", 0);
  viuArquivo[0] = 0; viuBytes = 1;
  tbburn_pedir("https://x/other.mkv", 0, 1);
  for (int k = 0; k < 200 && tbburn_resultado(NULL, 0, NULL, 0) == TBB_BUSCANDO; k++) { struct timespec t = {0, 10000000}; nanosleep(&t, NULL); }
  CHECA(!viuArquivo[0] && viuBytes == 0);

  // 8) Automatic choice: Signs & Songs, else dialogue; memory wins.
  { TbFaixa f[3], m;
    memset(f, 0, sizeof f);
    snprintf(f[0].idioma, 16, "eng"); snprintf(f[0].nome, 96, "Dialogue");
    snprintf(f[1].idioma, 16, "eng"); snprintf(f[1].nome, 96, "Signs & Songs"); f[1].tipo = 2;
    snprintf(f[2].idioma, 16, "spa"); snprintf(f[2].nome, 96, "Dialogo");
    CHECA(tbburn_escolher_auto(f, 3, 0, NULL, "en") == 1);           // S&S
    CHECA(tbburn_escolher_auto(f, 1, 0, NULL, "en") == 0);           // no S&S: dialogue
    CHECA(tbburn_escolher_auto(f + 2, 1, 0, NULL, "en") == 0);       // other language: still burned
    { TbFaixa g[2]; memcpy(g, f, sizeof g); g[1].tipo = 0;          // S&S by name only
      CHECA(tbburn_escolher_auto(g, 2, 0, NULL, "en") == 1); }
    CHECA(tbburn_escolher_auto(f, 3, 1, NULL, "en") == -1);          // Off remembered
    m = f[0]; CHECA(tbburn_escolher_auto(f, 3, 2, &m, "en") == 0);   // Dialogue remembered
    m = f[2]; snprintf(m.nome, 96, "Spanish full"); m.tipo = 0;     // same kind + language
    CHECA(tbburn_escolher_auto(f, 3, 2, &m, "en") == 2);
    CHECA(tbburn_escolher_auto(f, 0, 0, NULL, "en") == -1);
    CHECA(tbburn_escolher_auto(NULL, 3, 0, NULL, "en") == -1); }

  // 9) Memory per show and profile.
  { TbFaixa t, r;
    tbburn_titulo("");  CHECA(tbburn_memoria(&r) == 0);            // no show: nothing
    tbburn_titulo("tt15765670");
    CHECA(tbburn_memoria(&r) == 0);
    memset(&t, 0, sizeof t); snprintf(t.idioma, 16, "eng"); snprintf(t.nome, 96, "Dialogue\tx");
    tbburn_lembrar(&t);
    CHECA(tbburn_memoria(&r) == 2 && !strcmp(r.idioma, "eng") && !strcmp(r.nome, "Dialogue x"));
    tbburn_lembrar(NULL); CHECA(tbburn_memoria(&r) == 1);
    tbburn_titulo("tt0000001"); CHECA(tbburn_memoria(&r) == 0);    // another show
    tbburn_titulo("tt15765670"); CHECA(tbburn_memoria(&r) == 1);
    CHECA(dado && strstr(dadoNome, "-p1"));
    perfil = 2; CHECA(tbburn_memoria(&r) == 0); perfil = 1; }

  // 10) HDR (owner 13/11): burn-in On on an HDR file is remembered per show,
  // survives a later track pick, and old 7-column lines read as not allowed.
  { TbFaixa t, r;
    tbburn_titulo("tt15765670");
    CHECA(!tbburn_hdr_permitido());
    memset(&t, 0, sizeof t); snprintf(t.idioma, 16, "eng"); snprintf(t.nome, 96, "Signs");
    tbburn_lembrar(&t); tbburn_hdr_lembrar(1);
    CHECA(tbburn_hdr_permitido() && dado && strstr(dado, "\tSigns\th\n"));
    snprintf(t.nome, 96, "Dialogue"); tbburn_lembrar(&t);              // another pick keeps it
    CHECA(tbburn_hdr_permitido() && tbburn_memoria(&r) == 2 && !strcmp(r.nome, "Dialogue"));
    tbburn_titulo("tt0000001"); CHECA(!tbburn_hdr_permitido());        // per show
    tbburn_titulo("tt15765670"); tbburn_hdr_lembrar(0); CHECA(!tbburn_hdr_permitido());
    free(dado); dado = strdup("# nuvio burn-in v1\ntt7\t1\tf\teng\t0\t1\tSigns\n");   // older file
    snprintf(dadoNome, sizeof dadoNome, "tbburn-memoria-p%d.txt", 3); perfil = 3;
    tbburn_titulo("tt7"); CHECA(tbburn_memoria(&r) == 2 && !tbburn_hdr_permitido());
    perfil = 1;
    tbburn_hdr_fonte(1); CHECA(tbburn_hdr_da_fonte()); tbburn_hdr_fonte(0); CHECA(!tbburn_hdr_da_fonte()); }

  // 11) Early lookup (owner 18/11): the key, the MKV guess, then the flow.
  tbburn_chave(tmp, sizeof tmp, "https://a/x", "ABCDEF0123456789ABCDEF0123456789ABCDEF01", "/d/Show.S01E02.mkv", 42);
  CHECA(!strcmp(tmp, "h:abcdef0123456789abcdef0123456789abcdef01|n:Show.S01E02.mkv|b:42"));
  tbburn_chave(tmp, sizeof tmp, "https://a/x", "nothex", "", 0);      // no usable hash/name/size: the URL
  CHECA(!strcmp(tmp, "u:https://a/x"));
  tbburn_chave(tmp, sizeof tmp, "", "", "", 0); CHECA(!tmp[0]);
  CHECA(tbburn_parece_mkv("https://a/b.mkv", "") && tbburn_parece_mkv("https://a/b", "") &&
        tbburn_parece_mkv("https://a/play?id=1.mp4", "") && tbburn_parece_mkv("https://host", "") &&
        tbburn_parece_mkv("", "Show.S01E02.1080p.WEB") && tbburn_parece_mkv("https://a/b.mp4", "Show.MKV"));
  CHECA(!tbburn_parece_mkv("https://a/b.mp4?x=1", "") && !tbburn_parece_mkv("", "dir/Movie.2020.AVI") &&
        !tbburn_parece_mkv("https://a/b.mkv", "Movie.m2ts"));
  pro = 1; ligado = 1; chave = 1; tbburn_definir_escolha(-1);
  // a) Focused source found early; the burn-in request uses it (one lookup in all).
  partesAchar = 1; partesAtrasoMs = 0; partesN = 0;
  tbburn_procurar_cedo("https://s/1", "", "A.S01E01.mkv", 1001, "source focused 1 s");
  CHECA(partesEsperar(1) == 1);
  tbburn_procurar_cedo("https://s/1", "", "A.S01E01.mkv", 1001, "source focused 1 s");   // known: no new lookup
  dormir(50); CHECA(partesN == 1);
  tbburn_fonte("https://played/1", "", "A.S01E01.mkv", 1001);   // the played URL differs; same file
  tbburn_pedir("https://played/1", 0, 0);
  CHECA(fimPedido() == TBB_FALHOU && partesN == 1);
  // b) The request starts while the early lookup runs: it waits for it, no second lookup.
  partesN = 0; partesAtrasoMs = 300;
  tbburn_procurar_cedo("https://s/2", "", "A.S01E02.mkv", 1002, "Play");
  dormir(50);
  tbburn_fonte("https://s/2", "", "A.S01E02.mkv", 1002);
  tbburn_pedir("https://s/2", 0, 0);
  CHECA(fimPedido() == TBB_FALHOU && partesEsperar(1) == 1);
  // c) A miss: no new early lookup of that file for a while, but the request looks again.
  partesN = 0; partesAtrasoMs = 0; partesAchar = 0;
  tbburn_procurar_cedo("https://s/3", "", "A.S01E03.mkv", 1003, "source focused 1 s");
  CHECA(partesEsperar(1) == 1);
  tbburn_procurar_cedo("https://s/3", "", "A.S01E03.mkv", 1003, "Play");
  dormir(50); CHECA(partesN == 1);
  tbburn_fonte("https://s/3", "", "A.S01E03.mkv", 1003);
  tbburn_pedir("https://s/3", 0, 0);
  CHECA(fimPedido() == TBB_FALHOU && partesN == 2);
  // d) Not for an MP4, nor when burn-in is not possible.
  partesN = 0; partesAchar = 1;
  tbburn_procurar_cedo("https://s/4.mp4", "", "", 0, "Play");
  tbburn_definir_escolha(0); tbburn_procurar_cedo("https://s/5", "", "A.S01E05.mkv", 1005, "Play"); tbburn_definir_escolha(-1);
  pro = 0; tbburn_procurar_cedo("https://s/6", "", "A.S01E06.mkv", 1006, "Play"); pro = 1;
  dormir(100); CHECA(partesN == 0);
  // e) Play: tbburn_fonte arms it, the next frame starts it once; a picked file skips it.
  tbburn_cedo_bombear(); dormir(50); CHECA(partesN == 0);           // nothing armed
  tbburn_fonte("https://s/7", "", "A.S01E07.mkv", 1007);
  tbburn_cedo_bombear(); CHECA(partesEsperar(1) == 1);
  tbburn_cedo_bombear(); dormir(50); CHECA(partesN == 1);           // once
  tbburn_fonte("https://s/8", "", "A.S01E08.mkv", 1008);
  tbburn_ids("https://s/8", 0, 900, 2);
  tbburn_cedo_bombear(); dormir(50); CHECA(partesN == 1);
  // f) While one runs, only the newest waiting request is looked up after it.
  partesN = 0; partesAtrasoMs = 200;
  tbburn_procurar_cedo("https://s/9", "", "A.S01E09.mkv", 1009, "source focused 1 s");
  dormir(30);
  tbburn_procurar_cedo("https://s/10", "", "A.S01E10.mkv", 1010, "source focused 1 s");
  tbburn_procurar_cedo("https://s/11", "", "A.S01E11.mkv", 1011, "source focused 1 s");
  CHECA(partesEsperar(2) == 2 && !strcmp(viuArquivo, "A.S01E11.mkv"));
  dormir(300); CHECA(partesN == 2);
  // g) Another TorBox account: hits of the old one are not used.
  partesN = 0; partesAtrasoMs = 0; conta = 2;
  tbburn_procurar_cedo("https://s/1", "", "A.S01E01.mkv", 1001, "source focused 1 s");
  CHECA(partesEsperar(1) == 1);
  conta = 1;

  // 12) Log 10 (owner 18/11).
  { // l) the early lookup at Play is skipped while the request looks the same file up.
    partesN = 0; partesAtrasoMs = 300; partesAchar = 1;
    tbburn_fonte("https://s/12l", "", "B.S01E05.mkv", 2005);
    tbburn_pedir("https://s/12l", 1, 1);
    dormir(50);
    tbburn_cedo_bombear();
    CHECA(fimPedido() == TBB_FALHOU && partesN == 1);
    dormir(400); CHECA(partesN == 1);
    partesAtrasoMs = 0;
    // m) a usenet download: createstream asks for type=usenet (early lookup and request).
    partesN = 0; partesUsenet = 1; ultRota[0] = 0;
    tbburn_procurar_cedo("https://s/12m", "", "B.S01E06.mkv", 2006, "Play");
    CHECA(partesEsperar(1) == 1);
    tbburn_fonte("https://s/12m", "", "B.S01E06.mkv", 2006);
    tbburn_pedir("https://s/12m", 1, 1);
    CHECA(fimPedido() == TBB_FALHOU && partesN == 1 && strstr(ultRota, "type=usenet") != NULL);
    partesN = 0; ultRota[0] = 0;
    tbburn_fonte("https://s/12n", "", "B.S01E07.mkv", 2007);
    tbburn_pedir("https://s/12n", 1, 1);
    CHECA(fimPedido() == TBB_FALHOU && partesN == 1 && strstr(ultRota, "type=usenet") != NULL);
    partesUsenet = 0; ultRota[0] = 0;
    tbburn_fonte("https://s/12o", "", "B.S01E08.mkv", 2008);
    tbburn_pedir("https://s/12o", 1, 1);
    CHECA(fimPedido() == TBB_FALHOU && strstr(ultRota, "type=torrent") != NULL);
    // p) Log 12: createstream answers ITEM_NOT_FOUND for the listed item -> fresh
    //    lists once; a new item is asked for; the stale hit is forgotten.
    CHECA(tbburn_eh_item_sumido(404, "{\"success\":false,\"error\":\"ITEM_NOT_FOUND\"}") &&
          !tbburn_eh_item_sumido(200, "{\"success\":true}") && !tbburn_eh_item_sumido(0, NULL));
    sumido500 = 1; frescoItem = 600; frescoN = criacoes = 0; partesN = 0;
    tbburn_procurar_cedo("https://s/12p", "", "B.S01E09.mkv", 2009, "Play");
    CHECA(partesEsperar(1) == 1);
    tbburn_fonte("https://s/12p", "", "B.S01E09.mkv", 2009);
    tbburn_pedir("https://s/12p", 1, 1);
    CHECA(fimPedido() == TBB_FALHOU && frescoN == 1 && criacoes == 2 && strstr(ultRota, "createstream?id=600&") != NULL);
    tbburn_pedir("https://s/12p", 1, 1);   // the fresh item is used now: no list, no 404
    CHECA(fimPedido() == TBB_FALHOU && frescoN == 1 && criacoes == 3 && strstr(ultRota, "id=600&") != NULL);
    // q) the fresh list has the same item, or nothing: re-add by the hash TorBox's
    //    (stale) list gives (owner 19/11/2026). Not cached: one createtorrent,
    //    no second createstream, Nuvio draws.
    frescoItem = 500; frescoN = criacoes = adicoes = 0; adicionarDa = -1;
    tbburn_fonte("https://s/12q", "", "B.S01E10.mkv", 2010);
    tbburn_pedir("https://s/12q", 1, 1);
    CHECA(fimPedido() == TBB_FALHOU && frescoN == 1 && criacoes == 1 && adicoes == 1 &&
          !strcmp(adicionouHash, "abababababababababababababababababababab") && !viuEsperando);
    frescoItem = 0; frescoN = criacoes = adicoes = 0;
    tbburn_fonte("https://s/12r", "", "B.S01E11.mkv", 2011);
    tbburn_pedir("https://s/12r", 1, 1);
    CHECA(fimPedido() == TBB_FALHOU && frescoN == 1 && criacoes == 1 && adicoes == 1);
    // r) Re-added (torrent 900): TorBox needs a while; the request waits
    //    (TBB_ESPERANDO) and asks with the torrent itself and the file index
    //    of the old match (log 12, 23/11/2026): no fresh-list lookup needed.
    { char m[256];
      frescoItem = 900; frescoAPartir = 4; frescoN = criacoes = adicoes = 0; adicionarDa = 900; pronto900 = 1;
      tbburn_fonte("https://s/12s", "", "B.S01E12.mkv", 2012);
      tbburn_pedir("https://s/12s", 1, 1);
      // Owner 23/11/2026: the first ask is right after the add; TorBox
      // answering there means no wait at all (no TBB_ESPERANDO).
      CHECA(fimPedido() == TBB_PRONTO && !viuEsperando && adicoes == 1 && frescoN == 1 &&
            strstr(ultRota, "createstream?id=900&file_id=7") != NULL);
      CHECA(tbburn_resultado(m, sizeof m, NULL, 0) == TBB_PRONTO && strstr(m, "/s/900/playlist.m3u8") != NULL);
      // The next request for that file uses item 900 at once.
      frescoN = criacoes = adicoes = 0;
      tbburn_pedir("https://s/12s", 1, 1);
      CHECA(fimPedido() == TBB_PRONTO && adicoes == 0 && frescoN == 0 && criacoes == 1); }
    // s) Never registered: gives up after the total time, Nuvio draws. The
    //    torrent is asked directly each try; while TorBox says ITEM_NOT_FOUND
    //    for it the 13 MB lists are not read again.
    frescoItem = 0; frescoAPartir = 0; frescoN = criacoes = adicoes = 0; adicionarDa = 900; pronto900 = 0; sumido900 = 1;
    tbburn_fonte("https://s/12t", "", "B.S01E13.mkv", 2013);
    tbburn_pedir("https://s/12t", 1, 1);
    // Owner 24/11/2026 (log 24): two looks (3.5 s after the add, 7 s into
    // playback) with a remove + add between: createstream for item 500 once
    // + 2 for torrent 900; added twice, removed once.
    apagou = 0;
    CHECA(fimPedido() == TBB_FALHOU && viuEsperando && adicoes == 2 && apagou == 1 && apagouItem == 900 &&
          frescoN == 1 && criacoes == 3);
    // s1) The same, but TorBox no longer reports the hash cached: the added
    //     item is never removed from the account (and not added again).
    frescoItem = 0; frescoAPartir = 0; frescoN = criacoes = adicoes = 0; adicionarDa = 900; pronto900 = 0; sumido900 = 1;
    emCache = 0; apagou = 0;
    tbburn_fonte("https://s/12u", "", "B.S01E14.mkv", 2014);
    tbburn_pedir("https://s/12u", 1, 1);
    CHECA(fimPedido() == TBB_FALHOU && apagou == 0 && adicoes == 1);
    emCache = 1;
    // s2) TBB_ESPERANDO comes after the look 3.5 s after the add (item 500's
    //     createstream + 1 look); TorBox ready by the look 7 s into the
    //     original's playback -> PRONTO, not before that moment.
    { int c0; struct timespec t0;
      frescoN = criacoes = adicoes = 0; sumido900 = 1; pronto900 = 0;
      tbburn_fonte("https://s/12t2", "", "B.S01E13.mkv", 2013);
      tbburn_pedir("https://s/12t2", 1, 1);
      for (int k = 0; k < 400 && tbburn_resultado(NULL, 0, NULL, 0) != TBB_ESPERANDO; k++) dormir(1);
      c0 = criacoes;
      CHECA(tbburn_resultado(NULL, 0, NULL, 0) == TBB_ESPERANDO && c0 == 2);
      dormir(5);   // the remove + add right after
      pronto900 = 1;
      tbburn_original_tocando("https://s/12t2");
      clock_gettime(CLOCK_MONOTONIC, &t0);
      CHECA(fimPedido() == TBB_PRONTO && criacoes == 3 && adicoes == 2);
      CHECA(msDesde(&t0) >= 35); }   // 7 s (40 ms here) into playback, not sooner
    // s3) Log 17 (E03, owner 23/11/2026): ITEM_NOT_FOUND with the item's hash
    //     known from the lookup: re-added at once, no list download.
    { partesAchar = 1; snprintf(partesHashItem, sizeof partesHashItem, "%s", "3434343434343434343434343434343434343434");
      frescoN = criacoes = adicoes = 0; sumido900 = 0; pronto900 = 1;
      tbburn_fonte("https://s/12t3", "", "B.S01E21.mkv", 2021);
      tbburn_pedir("https://s/12t3", 1, 1);
      CHECA(fimPedido() == TBB_PRONTO && frescoN == 0 && adicoes == 1 &&
            !strcmp(adicionouHash, "3434343434343434343434343434343434343434") && strstr(ultRota, "createstream?id=900&") != NULL);
      partesHashItem[0] = 0; }
    sumido900 = 0; pronto900 = 1;
    // t) Cancelled while waiting (another pick / the player left): stops asking.
    { int n0;
      frescoN = criacoes = adicoes = 0;
      tbburn_fonte("https://s/12u", "", "B.S01E14.mkv", 2014);
      tbburn_pedir("https://s/12u", 1, 1);
      for (int k = 0; k < 200 && tbburn_resultado(NULL, 0, NULL, 0) != TBB_ESPERANDO; k++) dormir(5);
      tbburn_cancelar();
      n0 = frescoN; dormir(200);
      CHECA(frescoN <= n0 + 1 && tbburn_resultado(NULL, 0, NULL, 0) == TBB_NADA); }
    sumido500 = 0; pronto900 = 0;
    // u) Not in any list, but the add-on gave its hash: re-added the same way.
    { partesAchar = 0; frescoItem = 900; frescoAPartir = 1; frescoN = criacoes = adicoes = 0; adicionarDa = 900; pronto900 = 1;
      tbburn_fonte("https://s/12v", "CDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCD", "B.S01E15.mkv", 2015);
      tbburn_pedir("https://s/12v", 1, 1);
      CHECA(fimPedido() == TBB_PRONTO && adicoes == 1 && !strcmp(adicionouHash, "cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"));
      // ...and without a hash nothing is added (owner 24/11/2026, step 5): the
      // original plays, one more (fresh) look 7 s into playback.
      adicoes = 0; frescoItem = 0; frescoN = 0; listaVelha = 0;
      tbburn_fonte("https://s/12w", "", "B.S01E16.mkv", 2016);
      tbburn_pedir("https://s/12w", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && adicoes == 0 && viuEsperando && frescoN == 1 && listaVelha == 0);
      // ...found at that look: switches.
      frescoItem = 900; frescoAPartir = 1; frescoN = 0;
      tbburn_fonte("https://s/12w2", "", "B.S01E16.mkv", 2016);
      tbburn_pedir("https://s/12w2", 1, 1);
      tbburn_original_tocando("https://s/12w2");
      CHECA(fimPedido() == TBB_PRONTO && adicoes == 0 && viuEsperando && frescoN == 1);
      // The link's own hash is a hash too (log 24: Married Couple, "by link hash").
      frescoN = 0; frescoAPartir = 1;
      tbburn_fonte("https://s/abcdefabcdefabcdefabcdefabcdefabcdefabcd/ep.mkv", "", "B.S01E22.mkv", 2022);
      tbburn_pedir("https://s/abcdefabcdefabcdefabcdefabcdefabcdefabcd/ep.mkv", 1, 1);
      CHECA(fimPedido() == TBB_PRONTO && adicoes == 1 && !strcmp(adicionouHash, "abcdefabcdefabcdefabcdefabcdefabcdefabcd"));
      pronto900 = 0; frescoItem = 0; }
    // u2) Owner 24/11/2026: not listed 3.5 s after the add -> the original
    //     plays (TBB_ESPERANDO), the torrent is removed and the hash added
    //     again; found 7 s into playback -> PRONTO. Fresh lists every look.
    { adicoes = apagou = 0; frescoItem = 900; frescoAPartir = 2; frescoN = 0; adicionarDa = 900; pronto900 = 1; listaVelha = 0;
      tbburn_fonte("https://s/14a", "CDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCD", "D.S01E01.mkv", 4001);
      tbburn_pedir("https://s/14a", 1, 1);
      tbburn_original_tocando("https://s/14a");
      CHECA(fimPedido() == TBB_PRONTO && viuEsperando && adicoes == 2 && apagou == 1 && apagouItem == 900 &&
            frescoN == 2 && listaVelha == 0);
      // Remove + add once per file per playback: a new request for it that
      // misses again doesn't remove again.
      adicoes = apagou = 0; frescoItem = 0; frescoN = 0;
      tbburn_pedir("https://s/14a", 1, 1);   // known now (item 900): straight to createstream
      (void)fimPedido();
      tbburn_fonte("https://s/14a2", "CDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCD", "D.S01E05.mkv", 4005);
      apagou = adicoes = 0;
      tbburn_pedir("https://s/14a2", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && apagou == 1 && adicoes == 2);   // a different file: once for it
      apagou = adicoes = 0;
      tbburn_pedir("https://s/14a2", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && apagou == 0 && adicoes == 1);   // the same file again: not again
      tbburn_sessao_nova();
      apagou = adicoes = 0;
      tbburn_pedir("https://s/14a2", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && apagou == 1 && adicoes == 2);   // a new playback may
      pronto900 = 0; frescoAPartir = 0; }
    // u3) Listed 3.5 s after the add but not playable (createstream fails):
    //     the original, remove that item + add; playable 7 s into playback.
    { adicoes = apagou = 0; frescoItem = 950; frescoAPartir = 1; frescoN = 0; adicionarDa = 900; falha950 = 1;
      tbburn_fonte("https://s/14c", "CDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCD", "D.S01E02.mkv", 4002);
      tbburn_pedir("https://s/14c", 1, 1);
      tbburn_original_tocando("https://s/14c");
      CHECA(fimPedido() == TBB_PRONTO && viuEsperando && apagou == 1 && apagouItem == 950 && adicoes == 2 && frescoN == 2);
      frescoItem = 0; frescoAPartir = 0; falha950 = 0; }
    // u4) Owner OK 24/11/2026: the added torrent is listed without the playing
    //     file (the link's hash was another torrent): deleted again, stop, no
    //     remove + add loop.
    { char mt[96];
      adicoes = apagou = 0; frescoItem = 0; frescoN = 0; adicionarDa = 900; semFresco = 900;
      tbburn_fonte("https://s/14d", "CDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCD", "D.S01E03.mkv", 4003);
      tbburn_pedir("https://s/14d", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && !viuEsperando && adicoes == 1 && apagou == 1 && apagouItem == 900 && frescoN == 1);
      CHECA(tbburn_resultado(NULL, 0, mt, sizeof mt) == TBB_FALHOU && strstr(mt, "another torrent") != NULL);
      semFresco = 0;
      // ...and a hash whose torrent was already listed without the file at the
      // first look is not added at all (never deletes what was already there).
      adicoes = apagou = 0; semPrimeiro = 777; frescoN = 0;
      tbburn_fonte("https://s/14e", "CDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCD", "D.S01E04.mkv", 4004);
      tbburn_pedir("https://s/14e", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && adicoes == 0 && apagou == 0 && viuEsperando && frescoN == 1);
      semPrimeiro = 0; }
    // v) A plan refusal no longer hides burn-in (owner 19/11/2026: never goes on Pro).
    { partesAchar = 1; planoRecusa = 1;
      tbburn_fonte("https://s/12x", "", "B.S01E17.mkv", 2017);
      tbburn_arquivo_tem_ass(1);
      tbburn_pedir("https://s/12x", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && tbburn_recusado() && tbburn_linha_visivel() && tbburn_pode());
      planoRecusa = 0; }
    // w) Owner 22/11/2026 (log 1, YURASUKA): the item left TorBox's lists before
    //    the re-add, which then has no list hash; the hash kept from the lookup
    //    that found the item re-adds it.
    { partesAchar = 1; snprintf(partesHashItem, sizeof partesHashItem, "%s", "efefefefefefefefefefefefefefefefefefefef");
      sumido500 = 1; frescoItem = 0; frescoN = criacoes = adicoes = 0; adicionarDa = -1; hashAchaItem = 0;
      tbburn_fonte("https://s/12y", "", "B.S01E18.mkv", 2018);
      tbburn_pedir("https://s/12y", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && adicoes == 1 && !strcmp(adicionouHash, "efefefefefefefefefefefefefefefefefefefef"));
      sumido500 = 0; hashAchaItem = 500; }
    // x) TorBox sent no video (tbburn_readicionar): the found item is re-added by
    //    its kept hash, the request waits, then gets the stream.
    { char m[256];
      partesAchar = 1; snprintf(partesHashItem, sizeof partesHashItem, "%s", "1212121212121212121212121212121212121212");
      tbburn_fonte("https://s/12z", "", "B.S01E19.mkv", 2019);
      tbburn_pedir("https://s/12z", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU);   // found (item 500), no playlist in these stubs
      // Log 15: the item is in the account: video_tpk.c doesn't re-add then.
      CHECA(tbburn_item_na_conta("https://s/12z") == 500 && !tbburn_item_na_conta("https://s/other") && !tbburn_item_na_conta(""));
      frescoItem = 900; frescoAPartir = 1; frescoN = criacoes = adicoes = 0; adicionarDa = 900; pronto900 = 1;
      apagou = apagouItem = 0;
      tbburn_readicionar("https://s/12z", 1, 1);
      CHECA(fimPedido() == TBB_PRONTO && apagou == 1 && apagouItem == 500 &&   // removed first, then re-added
            !viuEsperando && adicoes == 1 &&
            !strcmp(adicionouHash, "1212121212121212121212121212121212121212") && strstr(ultRota, "createstream?id=900&") != NULL);
      CHECA(tbburn_resultado(m, sizeof m, NULL, 0) == TBB_PRONTO && strstr(m, "/s/900/playlist.m3u8") != NULL);
      CHECA(tbburn_item_na_conta("https://s/12z") == 900);   // the re-added torrent is the one known now
      pronto900 = 0; frescoItem = 0; frescoAPartir = 0; partesHashItem[0] = 0; }
    // x2) Log 15 (owner 23/11/2026): no video, and adding the hash gives the
    //     same item back (153924165 there): no minutes of asking, at once.
    { char mt[96];
      partesAchar = 1; snprintf(partesHashItem, sizeof partesHashItem, "%s", "3434343434343434343434343434343434343434");
      tbburn_fonte("https://s/12w", "", "B.S01E20.mkv", 2020);
      tbburn_pedir("https://s/12w", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && tbburn_item_na_conta("https://s/12w") == 500);
      frescoN = criacoes = adicoes = 0; adicionarDa = 500; apagou = 0; apagarDa = 0;
      tbburn_readicionar("https://s/12w", 1, 1);
      // The delete was refused: the add gives the same item back -> stops at once.
      CHECA(fimPedido() == TBB_FALHOU && apagou == 1 && adicoes == 1 && criacoes == 0 && frescoN == 0);
      CHECA(tbburn_resultado(NULL, 0, mt, sizeof mt) == TBB_FALHOU && strstr(mt, "still in your TorBox") != NULL);
      // Once per file per playback: a second no-video for the same file doesn't remove again.
      tbburn_pedir("https://s/12w", 1, 1); (void)fimPedido();
      apagou = 0; tbburn_readicionar("https://s/12w", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && apagou == 0);
      // A new playback may remove it again.
      tbburn_sessao_nova(); tbburn_pedir("https://s/12w", 1, 1); (void)fimPedido();
      apagou = 0; tbburn_readicionar("https://s/12w", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && apagou == 1);
      apagarDa = 1; partesHashItem[0] = 0; } }

  // y) Log 12 (Married Couple CRUCiBLE): a Burn in press while the re-added
  //    file is registering joins the wait: no second add, no list lookup, no
  //    "not in your TorBox"; it asks with the re-added torrent at once.
  { char m[256]; int n0, f0;
    partesAchar = 1; sumido500 = 1; frescoItem = 0; frescoAPartir = 0; frescoN = criacoes = adicoes = 0;
    adicionarDa = 900; sumido900 = 1; pronto900 = 0;
    tbburn_fonte("https://s/13a", "", "C.S01E08.mkv", 3008);
    tbburn_pedir("https://s/13a", 1, 1);
    for (int k = 0; k < 200 && tbburn_resultado(NULL, 0, NULL, 0) != TBB_ESPERANDO; k++) dormir(5);
    CHECA(tbburn_resultado(NULL, 0, NULL, 0) == TBB_ESPERANDO);
    dormir(5);   // the remove + add after the look 3.5 s after the add
    CHECA(adicoes == 2);
    n0 = partesN; f0 = frescoN;
    tbburn_pedir("https://s/13a", 2, 1);   // the press: another track, while registering
    dormir(15);
    { int e = tbburn_resultado(NULL, 0, NULL, 0); CHECA(e == TBB_ESPERANDO || e == TBB_BUSCANDO); }   // still waiting, not FALHOU
    pronto900 = 1;   // TorBox registered it
    CHECA(fimPedido() == TBB_PRONTO && adicoes == 2 && partesN == n0 && frescoN == f0 &&
          strstr(ultRota, "createstream?id=900&file_id=7") != NULL && strstr(ultRota, "chosen_subtitle_index=2") != NULL);
    CHECA(tbburn_resultado(m, sizeof m, NULL, 0) == TBB_PRONTO && strstr(m, "/s/900/playlist.m3u8") != NULL);
    sumido500 = sumido900 = pronto900 = 0; }

  if (falhas) { printf("tbburn: %d falha(s)\n", falhas); return 1; }
  printf("tbburn: ok\n");
  return 0;
}
