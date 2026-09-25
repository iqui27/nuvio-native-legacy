// O GIF DO CARTAZ DE COLECAO (#141): de onde sai e por que nao anima.
//
// O relato: Netflix anima, Apple TV / Paramount+ / Prime Video / Crunchyroll
// nao, e o log nao dizia nada sobre os parados. Ver src/gifcolecao.h.
//
// O QUE ESTE TESTE PROVA:
//   1. CAPA GIF SEM focusGifUrl anima pela capa — decidido pelos BYTES (GIF8),
//      nao pela URL; capa WebP/JPEG nao vira fonte; capa que ainda nao chegou
//      e "ainda nao se sabe", nao "nao e GIF"; focusGif, quando ha, ganha;
//   2. um "GIF" que chegou em outro formato (WebP com nome giphy.gif) e dito
//      COM A MAGICA no log; GIF de um quadro e dito como tal; GIF animado de
//      verdade passa;
//   3. URL com query (a do giphy traz cid de sessao) sai saneada: host +
//      ultimo trecho, sem query, fragmento nem usuario:senha;
//   4. TROCAR O FOCO entre duas pastas com GIF zera a pergunta ao arquivo e o
//      relogio do atraso — e voltar para a primeira tambem; a mesma pasta
//      ganhando fonte (a capa chegou e era GIF) conta como troca;
//   5. o registro e UMA linha por cartaz por sessao: provisorio pode ser
//      seguido de um definitivo, definitivo cala o cartaz.
//
//   bash tests/gifcolecao.sh
#include "../src/gifcolecao.h"
#include "../src/gif.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARQ "/tmp/nuvio-gifcolecao.gif"
#define LOG "/tmp/nuvio-gifcolecao.log"

static const unsigned char GIF8[4] = {'G', 'I', 'F', '8'};
static const unsigned char WEBP[4] = {'R', 'I', 'F', 'F'};
static const unsigned char JPEG[4] = {0xFF, 0xD8, 0xFF, 0xE0};

static void gravar(const unsigned char *b, size_t n) {
  FILE *f = fopen(ARQ, "wb");
  assert(f && fwrite(b, 1, n, f) == n);
  fclose(f);
}

// GIF de 1x1 com `quadros` quadros (paleta global de 2 cores, LZW minimo 2).
static size_t montarGif(unsigned char *b, int quadros) {
  static const unsigned char cab[] = {
    'G','I','F','8','9','a', 1,0, 1,0, 0x80, 0, 0,  0,0,0, 255,255,255 };
  static const unsigned char quadro[] = {
    0x21,0xF9,4, 0x04, 10,0, 0, 0,            // controle grafico: 100 ms
    0x2C, 0,0, 0,0, 1,0, 1,0, 0,              // descritor 1x1
    2, 2, 0x44, 0x01, 0 };                    // LZW
  size_t n = 0;
  memcpy(b, cab, sizeof cab); n += sizeof cab;
  for (int i = 0; i < quadros; i++) { memcpy(b + n, quadro, sizeof quadro); n += sizeof quadro; }
  b[n++] = 0x3B;
  return n;
}

// stdout para um arquivo, e de volta, para conferir a linha do registro.
static int salvo = -1;
static void capturar(void) {
  fflush(stdout);
  salvo = dup(1);
  assert(freopen(LOG, "w", stdout));
}
static char lido[4096];
static const char *soltar(void) {
  FILE *f;
  size_t n;
  fflush(stdout);
  dup2(salvo, 1); close(salvo);
  f = fopen(LOG, "r"); assert(f);
  n = fread(lido, 1, sizeof lido - 1, f); lido[n] = 0; fclose(f);
  return lido;
}

static void fonte(void) {
  const char *capa = "https://media4.giphy.com/media/abc/giphy.gif?cid=1&ep=v1_gifs_search&rid=giphy.gif&ct=g";
  const char *capaPng = "https://cdn.exemplo/covers/apple.png";
  int daCapa = -1;
  // Sem focusGif e capa GIF: anima pela capa.
  assert(gifcol_fonte("", capa, GIF8, &daCapa) == capa && daCapa == 1);
  assert(gifcol_fonte(NULL, capa, GIF8, &daCapa) == capa && daCapa == 1);
  // A mesma URL .gif entregando WebP ou JPEG nao e fonte.
  assert(!gifcol_fonte("", capa, WEBP, &daCapa) && daCapa == 0);
  assert(!gifcol_fonte("", capa, JPEG, &daCapa));
  assert(gifcol_sem_fonte(capa, WEBP) == GC_SEM_GIF);
  // Capa que ainda nao chegou: nao se sabe — provisorio.
  assert(!gifcol_fonte("", capa, NULL, &daCapa));
  assert(gifcol_sem_fonte(capa, NULL) == GC_CAPA_AINDA);
  // Capa do pacote (caminho local) nunca tera magica: e "sem GIF", nao espera.
  assert(gifcol_sem_fonte("colecoes/apple/cover.jpg", NULL) == GC_SEM_GIF);
  assert(gifcol_sem_fonte("", NULL) == GC_SEM_GIF);
  // PNG com magica GIF8 anima (a URL nao manda; os bytes mandam).
  assert(gifcol_fonte("", capaPng, GIF8, &daCapa) == capaPng && daCapa == 1);
  // focusGif ganha da capa, mesmo capa GIF.
  assert(!strcmp(gifcol_fonte("https://x/foco.gif", capa, GIF8, &daCapa), "https://x/foco.gif") && daCapa == 0);
  printf("ok  capa GIF sem focusGifUrl anima pela capa, decidido pelos bytes\n");
}

static void arquivo(void) {
  unsigned char b[256], m[4];
  size_t n;
  // Animado de verdade.
  n = montarGif(b, 3); gravar(b, n);
  assert(gif_animado(ARQ) == 1);
  assert(gifcol_motivo_arquivo(ARQ, gif_animado(ARQ), m) == GC_ANIMA);
  // Um quadro so.
  n = montarGif(b, 1); gravar(b, n);
  assert(gif_animado(ARQ) == 0);
  assert(gifcol_motivo_arquivo(ARQ, 0, m) == GC_UM_QUADRO && !memcmp(m, "GIF8", 4));
  // "giphy.gif" que chegou WebP: a magica sai no motivo.
  { static const unsigned char webp[] = { 'R','I','F','F', 40,0,0,0, 'W','E','B','P',
                                         'V','P','8',' ', 0,0,0,0, 0,0,0,0 };
    gravar(webp, sizeof webp); }
  assert(gif_animado(ARQ) == 0);
  assert(gifcol_motivo_arquivo(ARQ, 0, m) == GC_FORMATO && !memcmp(m, "RIFF", 4));
  // Arquivo que nao existe: formato 00000000, nao "um quadro".
  unlink(ARQ);
  assert(gifcol_motivo_arquivo(ARQ, 0, m) == GC_FORMATO && !m[0] && !m[3]);
  // gif_recusou fora do Tizen: nunca.
  assert(!gif_recusou(ARQ));
  printf("ok  GIF animado passa, GIF de 1 quadro e WebP com nome .gif saem com o motivo certo\n");
}

static void sanear(void) {
  char s[140];
  gifcol_sanear("https://media4.giphy.com/media/xT9IgG50Fb7Mi0prBC/giphy.gif?cid=790b7611&ep=v1_gifs_search&rid=giphy.gif&ct=g", s, sizeof s);
  assert(!strcmp(s, "media4.giphy.com/giphy.gif"));
  gifcol_sanear("https://usuario:senha@cdn.exemplo:8443/a/b/capa.gif#x", s, sizeof s);
  assert(!strcmp(s, "cdn.exemplo:8443/capa.gif"));
  gifcol_sanear("https://cdn.exemplo/pasta/?token=segredo", s, sizeof s);
  assert(!strcmp(s, "cdn.exemplo/pasta"));
  gifcol_sanear("https://cdn.exemplo?token=segredo", s, sizeof s);
  assert(!strcmp(s, "cdn.exemplo"));
  gifcol_sanear("", s, sizeof s);
  assert(!s[0]);
  assert(!strstr(s, "segredo"));
  // Buffer pequeno nao estoura.
  { char p[8]; gifcol_sanear("https://media4.giphy.com/media/x/giphy.gif", p, sizeof p); assert(strlen(p) == 7); }
  printf("ok  URL com query sai como host + ultimo trecho, sem query nem usuario\n");
}

static void trocaDeFoco(void) {
  GcFoco e;
  const char *gifA = "https://a/netflix.gif", *gifB = "https://b/appletv.gif";
  gifcol_foco_iniciar(&e);
  assert(e.ultimo == -1 && e.anima == -1);
  // Foco em A: troca; mesma pasta no quadro seguinte: nao.
  assert(gifcol_focar(&e, 1, gifA, 1000) == 1 && e.desde == 1000);
  e.anima = 1;
  assert(gifcol_focar(&e, 1, gifA, 1016) == 0 && e.anima == 1 && e.desde == 1000);
  // Para B: a resposta de A NAO pode valer para B.
  assert(gifcol_focar(&e, 2, gifB, 2000) == 1);
  assert(e.anima == -1 && e.desde == 2000 && !strcmp(e.fonte, gifB));
  e.anima = 0; e.motivoArq = GC_UM_QUADRO;
  // E de volta para A: pergunta de novo, e o motivo de B nao fica.
  assert(gifcol_focar(&e, 1, gifA, 3000) == 1 && e.anima == -1 && e.motivoArq == GC_ANIMA);
  // A mesma pasta ganhando fonte (a capa chegou e era GIF): e troca.
  assert(gifcol_focar(&e, 3, NULL, 4000) == 1);
  assert(gifcol_focar(&e, 3, NULL, 4016) == 0);
  assert(gifcol_focar(&e, 3, "https://c/capa.gif", 4500) == 1 && e.desde == 4500);
  printf("ok  trocar o foco entre duas pastas com GIF zera a pergunta e o atraso\n");
}

static void registro(void) {
  const char *txt;
  const char *url = "https://media4.giphy.com/media/abc/giphy.gif?cid=segredo123&ct=g";
  gifcol_registros_zerar();
  capturar();
  // Definitivo com magica: uma linha, saneada, com a magica.
  assert(gifcol_registrar("c1|apple", "Apple TV", GC_FORMATO, url, 0, WEBP) == 1);
  assert(gifcol_registrar("c1|apple", "Apple TV", GC_FORMATO, url, 0, WEBP) == 0);
  assert(gifcol_registrar("c1|apple", "Apple TV", GC_ARQ_AINDA, url, 0, NULL) == 0);
  // Provisorio, e depois UM definitivo.
  assert(gifcol_registrar("c1|prime", "Prime Video", GC_CAPA_AINDA, url, 1, NULL) == 1);
  assert(gifcol_registrar("c1|prime", "Prime Video", GC_CAPA_AINDA, url, 1, NULL) == 0);
  assert(gifcol_registrar("c1|prime", "Prime Video", GC_SEM_GIF, url, 1, NULL) == 1);
  assert(gifcol_registrar("c1|prime", "Prime Video", GC_SEM_GIF, url, 1, NULL) == 0);
  // Anima nao e motivo.
  assert(gifcol_registrar("c1|netflix", "Netflix", GC_ANIMA, url, 0, NULL) == 0);
  assert(gifcol_registrar("c1|netflix", "Netflix", GC_APARELHO, url, 0, NULL) == 1);
  txt = soltar();
  assert(strstr(txt, "[gif] cartaz \"Apple TV\" nao anima: o arquivo nao e GIF (magica 52494646) | focusGif media4.giphy.com/giphy.gif"));
  assert(strstr(txt, "\"Prime Video\" nao anima: sem focusGifUrl e a capa ainda nao chegou | capa"));
  assert(strstr(txt, "\"Prime Video\" nao anima: sem focusGifUrl e a capa nao e GIF | capa"));
  assert(strstr(txt, "\"Netflix\" nao anima: este aparelho nao anima GIF"));
  assert(!strstr(txt, "segredo123") && !strstr(txt, "cid="));
  { int linhas = 0; for (const char *p = txt; *p; p++) linhas += *p == '\n'; assert(linhas == 4); }
  printf("ok  uma linha por cartaz por sessao, saneada, com a magica quando o formato e outro\n");
}

int main(void) {
  fonte();
  arquivo();
  sanear();
  trocaDeFoco();
  registro();
  unlink(ARQ); unlink(LOG);
  printf("gifcolecao: tudo ok\n");
  return 0;
}
