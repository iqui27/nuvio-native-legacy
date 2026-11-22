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
static volatile int partesN;
unsigned debrid_tb_geracao(void) { return (unsigned)conta; }
void rede_fio_fechar(void) {}
int partes_achar_exato(const char *u, const char *h, const char *a, uint64_t b, int *i, int *f, int *un, char *c, unsigned n) {
  (void)u; (void)h;
  snprintf(viuArquivo, sizeof viuArquivo, "%s", a ? a : ""); viuBytes = b;
  if (partesAtrasoMs) { struct timespec t = {partesAtrasoMs / 1000, (long)(partesAtrasoMs % 1000) * 1000000L}; nanosleep(&t, NULL); }
  __atomic_add_fetch(&partesN, 1, __ATOMIC_SEQ_CST);
  if (un) *un = 0;
  if (!partesAchar) return 0;
  if (un) *un = partesUsenet;
  if (i) *i = 500;
  if (f) *f = 7;
  if (c && n) snprintf(c, n, "hash");
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
  for (int k = 0; k < 300 && tbburn_resultado(NULL, 0, NULL, 0) == TBB_BUSCANDO; k++) { struct timespec t = {0, 10000000L}; nanosleep(&t, NULL); }
  CHECA(tbburn_resultado(NULL, 0, NULL, 0) == TBB_FALHOU && tbburn_linha_visivel());

  // 7) Reuse last link: the player opens the saved URL with no add-on details;
  // the saved record fills them (owner's log 12/11, file=- size=0).
  tbburn_fonte_guardada("https://x/saved.mkv", "", "S01E08.mkv", 7074247003ULL);
  tbburn_fonte("https://x/saved.mkv", "", "", 0);
  tbburn_arquivo_tem_ass(1);
  CHECA(tbburn_linha_visivel());
  tbburn_pedir("https://x/saved.mkv", 0, 1);
  for (int k = 0; k < 200 && tbburn_resultado(NULL, 0, NULL, 0) == TBB_BUSCANDO; k++) { struct timespec t = {0, 10000000}; nanosleep(&t, NULL); }
  CHECA(tbburn_resultado(NULL, 0, NULL, 0) == TBB_FALHOU);
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
    //    (TBB_ESPERANDO), finds it in the fresh lists on the 3rd look (by the
    //    hash) and gets a playlist.
    { char m[256];
      frescoItem = 900; frescoAPartir = 4; frescoN = criacoes = adicoes = 0; adicionarDa = 900; pronto900 = 1;
      tbburn_fonte("https://s/12s", "", "B.S01E12.mkv", 2012);
      tbburn_pedir("https://s/12s", 1, 1);
      CHECA(fimPedido() == TBB_PRONTO && viuEsperando && adicoes == 1 && frescoN == 4 &&
            !strcmp(frescoHash, "abababababababababababababababababababab") && strstr(ultRota, "createstream?id=900&") != NULL);
      CHECA(tbburn_resultado(m, sizeof m, NULL, 0) == TBB_PRONTO && strstr(m, "/s/900/playlist.m3u8") != NULL);
      // The next request for that file uses item 900 at once.
      frescoN = criacoes = adicoes = 0;
      tbburn_pedir("https://s/12s", 1, 1);
      CHECA(fimPedido() == TBB_PRONTO && adicoes == 0 && frescoN == 0 && criacoes == 1); }
    // s) Never registered: gives up after the total time, Nuvio draws.
    frescoItem = 0; frescoAPartir = 0; frescoN = criacoes = adicoes = 0; adicionarDa = 900;
    tbburn_fonte("https://s/12t", "", "B.S01E13.mkv", 2013);
    tbburn_pedir("https://s/12t", 1, 1);
    CHECA(fimPedido() == TBB_FALHOU && viuEsperando && adicoes == 1 && frescoN >= 3 && criacoes == 1);
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
      // ...and without a hash from the add-on nothing is added.
      adicoes = 0;
      tbburn_fonte("https://s/12w", "", "B.S01E16.mkv", 2016);
      tbburn_pedir("https://s/12w", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && adicoes == 0);
      pronto900 = 0; frescoItem = 0; }
    // v) A plan refusal no longer hides burn-in (owner 19/11/2026: never goes on Pro).
    { partesAchar = 1; planoRecusa = 1;
      tbburn_fonte("https://s/12x", "", "B.S01E17.mkv", 2017);
      tbburn_arquivo_tem_ass(1);
      tbburn_pedir("https://s/12x", 1, 1);
      CHECA(fimPedido() == TBB_FALHOU && tbburn_recusado() && tbburn_linha_visivel() && tbburn_pode());
      planoRecusa = 0; } }

  if (falhas) { printf("tbburn: %d falha(s)\n", falhas); return 1; }
  printf("tbburn: ok\n");
  return 0;
}
