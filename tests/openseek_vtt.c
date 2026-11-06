// OPENSEEK: leitura do VTT (so a origem pareada, t= vira rotulo e sai da chave
// da folha), regra [ini, fim) com a tolerancia de +-5 s, e prova de que as
// variantes com base deixam a saida do Seekr identica (mesmas cargas do
// tests/seekr.c, comparadas campo a campo com a funcao antiga).
//
//   bash tests/openseek_vtt.sh
#include "../src/seekrvtt.h"
#include "../src/openseekproto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define B "https://nas.local:8787"
#define CAP "/v1/c/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/0123456789abcdef0123456789abcdef"

// Contrato: cue [p-5 s, p+5 s), cortada no meio entre vizinhos publicados.
// Quadros em p = 4797,96 s (k=480), 4810 s (k=481) e 4900 s (k=490, isolado).
static const char *VTT_OS =
  "WEBVTT\n\n"
  "01:19:52.960 --> 01:20:02.960\n"
  B CAP "/tiles/480.jpg?exp=1999999999&t=4797960#xywh=0,0,320,180\n\n"
  "01:20:05.000 --> 01:20:15.000\n"
  B CAP "/tiles/481.jpg?exp=1999999999&t=4810000#xywh=0,0,320,180\n\n"
  "01:21:35.000 --> 01:21:42.500\n"
  B CAP "/sheets/0.jpg?exp=1999999999&v=3&t=4900000#xywh=320,180,320,180\n\n"
  "01:21:42.500 --> 01:21:50.000\n"
  B CAP "/sheets/0.jpg?exp=1999999999&v=3&t=4905000#xywh=640,180,320,180\n\n"
  // Outra origem (o pino nao a cobre): descartada.
  "01:30:00.000 --> 01:30:10.000\n"
  "https://evil.example" CAP "/tiles/540.jpg?exp=1999999999&t=5400000#xywh=0,0,320,180\n\n"
  // Mesmo host, outra porta: descartada.
  "01:31:00.000 --> 01:31:10.000\n"
  "https://nas.local:8788" CAP "/tiles/546.jpg?exp=1999999999&t=5460000#xywh=0,0,320,180\n\n"
  // Prefixo enganoso do host: descartada.
  "01:32:00.000 --> 01:32:10.000\n"
  "https://nas.local:87870" CAP "/tiles/552.jpg?exp=1&t=5520000#xywh=0,0,320,180\n\n"
  // Sem t=: descartada (o rotulo seria inventado).
  "01:33:00.000 --> 01:33:10.000\n"
  B CAP "/tiles/558.jpg?exp=1999999999#xywh=0,0,320,180\n\n";

// As cargas de tests/seekr.c.
static const char *VTT_SK =
  "WEBVTT\r\n\r\n"
  "00:00:10.000 --> 00:00:20.000\r\n"
  "https://sprites.seekr.tv/s/603/sheet_000.jpg?sig=abc#xywh=320,0,320,180\r\n\r\n"
  "00:00:00.000 --> 00:00:10.000\r\n"
  "https://sprites.seekr.tv/s/603/sheet_000.jpg?sig=abc#xywh=0,0,320,180\r\n\r\n"
  "00:00:20.000 --> 00:00:30.000\r\n"
  "/s/603/sheet_001.jpg?sig=def#xywh=0,0,320,180\r\n\r\n"
  "01:00:00.000 --> 01:00:10.500\n"
  "\n"
  "/s/603/sheet_001.jpg?sig=def#xywh=640,180,320,180\n"
  "00:00:40.000 --> 00:00:50.000\n"
  "sem fragmento\n"
  "lixo --> tambem\n"
  "00:01:00.000 --> 00:01:10.000\n"
  "relativa/sem/barra.jpg#xywh=0,0,320,180\n";

static void mesmo(const SeekrVtt *a, const SeekrVtt *b) {
  int i;
  assert(a->nCues == b->nCues && a->nFolhas == b->nFolhas);
  for (i = 0; i < a->nFolhas; i++) assert(!strcmp(a->folhas[i], b->folhas[i]));
  for (i = 0; i < a->nCues; i++) {
    const SeekrCue *x = &a->cues[i], *y = &b->cues[i];
    assert(x->ini == y->ini && x->fim == y->fim && x->folha == y->folha && x->x == y->x &&
           x->y == y->y && x->w == y->w && x->h == y->h && x->rotulo == y->rotulo);
  }
}

int main(void) {
  SeekrVtt v, w;
  char u[2400], k1[240], k2[240];
  int n, i;

  // --- Seekr inalterado ----------------------------------------------------------
  n = seekr_vtt_ler(VTT_SK, &v);
  assert(seekr_vtt_ler_base(VTT_SK, "https://sprites.seekr.tv", &w) == n);
  mesmo(&v, &w);
  assert(n == 5 && v.nFolhas == 3);
  assert(!strcmp(v.folhas[1], "https://sprites.seekr.tv/s/603/sheet_001.jpg?sig=def"));
  assert(!strcmp(v.folhas[2], "relativa/sem/barra.jpg"));     // verbatim, como sempre
  for (i = 0; i < n; i++) assert(v.cues[i].rotulo == -1);
  // A escolha "mais proxima" do Seekr nao muda.
  assert(seekr_vtt_cue(&v, -5) == 0 && seekr_vtt_cue(&v, 18500) == 2 &&
         seekr_vtt_cue(&v, 1000000) == 4 && seekr_vtt_cue(&v, 99999999) == 4);
  seekr_vtt_liberar(&v); seekr_vtt_liberar(&w);
  {
    const char *j = "{\"vtt_url\":\"/sprites/1/2/t.vtt?sig=x\"}";
    char a[512], b[512];
    assert(seekr_ler_lookup(j, a, sizeof a) && seekr_ler_lookup_base(j, "https://sprites.seekr.tv", b, sizeof b));
    assert(!strcmp(a, b) && !strcmp(a, "https://sprites.seekr.tv/sprites/1/2/t.vtt?sig=x"));
    assert(seekr_ler_lookup_base(j, B, b, sizeof b) && !strcmp(b, B "/sprites/1/2/t.vtt?sig=x"));
    assert(!seekr_ler_lookup("{\"vtt_url\":\"javascript:x\"}", a, sizeof a));
    assert(!seekr_ler_lookup_base("{\"vtt_url\":\"javascript:x\"}", B, a, sizeof a));
  }

  // --- OpenSeek -------------------------------------------------------------------
  n = seekr_vtt_ler_openseek(VTT_OS, B, &v);
  assert(n == 4);
  // Folhas: tiles/480, tiles/481 e UMA so sheet/0 (o t sai da chave).
  assert(v.nFolhas == 3);
  assert(!strcmp(v.folhas[0], B CAP "/tiles/480.jpg?exp=1999999999"));
  assert(!strcmp(v.folhas[2], B CAP "/sheets/0.jpg?exp=1999999999&v=3"));
  assert(v.cues[2].folha == v.cues[3].folha);
  // O rotulo e o pts real (t=), nao o inicio da cue.
  assert(v.cues[0].rotulo == 4797960 && v.cues[0].ini == 4792960);
  assert(v.cues[1].rotulo == 4810000 && v.cues[3].rotulo == 4905000);
  assert(v.cues[3].x == 640 && v.cues[3].y == 180);
  // Regra [ini, fim): dentro de +-5 s do pts ou nada.
  assert(seekr_vtt_cue_estrita(&v, 4797960) == 0);
  assert(seekr_vtt_cue_estrita(&v, 4792960) == 0);        // ini inclusivo
  assert(seekr_vtt_cue_estrita(&v, 4792959) == -1);       // antes de tudo: nada
  assert(seekr_vtt_cue_estrita(&v, 4802959) == 0);
  assert(seekr_vtt_cue_estrita(&v, 4802960) == -1);       // fim exclusivo; buraco de 2 s
  assert(seekr_vtt_cue_estrita(&v, 4804999) == -1);
  assert(seekr_vtt_cue_estrita(&v, 4805000) == 1);
  assert(seekr_vtt_cue_estrita(&v, 4814999) == 1);
  assert(seekr_vtt_cue_estrita(&v, 4815000) == -1);       // buraco: sem previa
  assert(seekr_vtt_cue_estrita(&v, 4850000) == -1);
  assert(seekr_vtt_cue_estrita(&v, 4895000) == 2);
  assert(seekr_vtt_cue_estrita(&v, 4902500) == 3);       // corte no meio entre vizinhos
  assert(seekr_vtt_cue_estrita(&v, 4910000) == -1);       // depois da ultima: nada
  // A regra antiga escolheria uma cue em todos esses pontos; a nova nao.
  assert(seekr_vtt_cue(&v, 4850000) >= 0 && seekr_vtt_cue(&v, 1) == 0);
  // Tolerancia: em toda posicao com cue, |rotulo - pos| <= 5 s.
  for (long p = 4790000; p < 4912000; p += 37) {
    int c = seekr_vtt_cue_estrita(&v, p);
    if (c >= 0) { long d = v.cues[c].rotulo - p; assert(d <= 5000 && d >= -5000); }
  }
  // Chaves: a mesma imagem com exp/t/cap novos tem a mesma chave (troca de VTT
  // sem perder textura); imagens diferentes nunca colidem.
  assert(openseek_chave(B CAP "/tiles/480.jpg?exp=1999999999&t=4797960#xywh=0,0,320,180", k1, sizeof k1));
  assert(openseek_chave(B "/v1/c/BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB/0123456789abcdef0123456789abcdef"
                        "/tiles/480.jpg?exp=2000003600&t=4797960", k2, sizeof k2));
  assert(!strcmp(k1, k2));
  assert(!strcmp(k1, B "/v1/c/_/0123456789abcdef0123456789abcdef/tiles/480.jpg"));
  assert(openseek_chave(B CAP "/sheets/0.jpg?exp=1&v=3&t=4900000", k1, sizeof k1) &&
         openseek_chave(B CAP "/sheets/0.jpg?v=3&t=4905000&exp=9", k2, sizeof k2) && !strcmp(k1, k2));
  assert(openseek_chave(B CAP "/sheets/0.jpg?exp=1&v=4", k2, sizeof k2) && strcmp(k1, k2)); // v novo
  assert(openseek_chave(B CAP "/tiles/481.jpg?exp=1", k2, sizeof k2) && strcmp(k1, k2));
  // URL com exp.
  assert(openseek_url_exp(v.folhas[0]) == 1999999999LL && openseek_url_exp("https://x/a?b=1") == 0);
  seekr_vtt_liberar(&v);

  // Sem origem nada passa; VTT vazio (preparing) = 0 cues, sem erro.
  assert(seekr_vtt_ler_openseek(VTT_OS, "", &v) == 0); seekr_vtt_liberar(&v);
  assert(seekr_vtt_ler_openseek("WEBVTT\n\n", B, &v) == 0 && v.nCues == 0); seekr_vtt_liberar(&v);
  assert(seekr_vtt_cue_estrita(&v, 0) == -1);
  (void)u;
  printf("openseek_vtt: ok\n");
  return 0;
}
