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
char *debrid_tb_api_get(const char *r, int p, int *st) { (void)r; (void)p; if (st) *st = 0; return NULL; }
static char viuArquivo[512]; static uint64_t viuBytes;
int partes_achar_exato(const char *u, const char *h, const char *a, uint64_t b, int *i, int *f, char *c, unsigned n) {
  (void)u; (void)h; (void)i; (void)f; (void)c; (void)n;
  snprintf(viuArquivo, sizeof viuArquivo, "%s", a ? a : ""); viuBytes = b;
  return 0;
}
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

  if (falhas) { printf("tbburn: %d falha(s)\n", falhas); return 1; }
  printf("tbburn: ok\n");
  return 0;
}
