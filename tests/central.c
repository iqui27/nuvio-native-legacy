// Central de controle: CH+ segurado x tocado (chsegura.h), a lista de atalhos
// (centrallista.h) e a porta de Ajustes (ajustes_rapido_*). Sem GL.
#include "chsegura.h"
#include "centrallista.h"
#include "ajustes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); \
  printf(__VA_ARGS__); printf("\n"); } } while (0)

static void teclaSegurada(void) {
  ChSegura s;
  int i, r;
  memset(&s, 0, sizeof s);
  // Toque com KEYUP de verdade: o curto sai quando a emenda vence.
  chs_desce(&s, 0);
  CONFERE(chs_ocupado(&s), "toque: engolido enquanto embaixo");
  chs_sobe(&s, 120);
  CONFERE(chs_quadro(&s, 150) == CHS_NADA, "toque: ainda na emenda");
  CONFERE(chs_quadro(&s, 210) == CHS_CURTO, "toque: curto depois da emenda");
  CONFERE(!chs_ocupado(&s) && chs_quadro(&s, 400) == CHS_NADA, "toque: uma vez so");
  // Segurado sem repeticao, com KEYUP ja provado: abre pelo relogio.
  chs_desce(&s, 1000);
  CONFERE(chs_quadro(&s, 1599) == CHS_NADA, "segurado: antes do prazo");
  CONFERE(chs_quadro(&s, 1600) == CHS_LONGO, "segurado: abre no prazo");
  CONFERE(chs_quadro(&s, 1700) == CHS_NADA && chs_ocupado(&s), "segurado: engole ate soltar");
  chs_sobe(&s, 2000);
  CONFERE(chs_quadro(&s, 2100) == CHS_NADA && !chs_ocupado(&s), "segurado: solto, sem toque curto");
  // Repeticao em pares Up/Down (X11/EFL): o par nao vira toque.
  chs_desce(&s, 3000);
  r = CHS_NADA;
  for (i = 500; i <= 700 && r == CHS_NADA; i += 33) {
    chs_sobe(&s, 3000 + i);
    r = chs_quadro(&s, 3000 + i);
    if (r == CHS_NADA) r = chs_desce(&s, 3000 + i);
  }
  CONFERE(r == CHS_LONGO, "pares Up/Down: segurado (r=%d)", r);
  chs_sobe(&s, 4000); chs_quadro(&s, 4200);
  CONFERE(!chs_ocupado(&s), "pares: livre depois");

  // LG HIPOTETICO: KEYUP "quase junto" de cada KEYDOWN, nunca um de verdade.
  memset(&s, 0, sizeof s);
  chs_desce(&s, 0); chs_sobe(&s, 4);
  CONFERE(chs_quadro(&s, 100) == CHS_NADA, "fantasma: KEYUP imediato nao solta");
  CONFERE(chs_quadro(&s, 449) == CHS_NADA, "fantasma: espera o silencio");
  CONFERE(chs_quadro(&s, 450) == CHS_CURTO, "fantasma: curto pelo silencio");
  CONFERE(!s.viuSolta, "fantasma: nao conta como KEYUP de verdade");
  chs_desce(&s, 1000); chs_sobe(&s, 1003);
  chs_desce(&s, 1400); chs_sobe(&s, 1403);
  CONFERE(chs_desce(&s, 1600) == CHS_LONGO, "fantasma: repeticao abre");
  CONFERE(chs_quadro(&s, 1700) == CHS_NADA && chs_ocupado(&s), "fantasma: segurado");
  CONFERE(chs_quadro(&s, 2050) == CHS_NADA && !chs_ocupado(&s), "fantasma: solta pelo silencio");

  // Sem KEYUP nenhum e sem repeticao: o curto sai pelo silencio, nunca abre.
  memset(&s, 0, sizeof s);
  chs_desce(&s, 0);
  for (i = 0, r = CHS_NADA; i <= 2000 && r == CHS_NADA; i += 16) r = chs_quadro(&s, (unsigned)i);
  CONFERE(r == CHS_CURTO, "sem KEYUP: curto (r=%d)", r);
}

static void lista(void) {
  CentralLista l, m;
  char buf[512];
  int i, n;
  centrallista_padrao(&l);
  CONFERE(l.n == 7, "padrao: 7 atalhos (%d)", l.n);
  for (i = 0; i < l.n; i++) CONFERE(central_catalogo(l.item[i]) != NULL, "padrao: item %d no catalogo", i);
  centrallista_escrever(&l, buf, sizeof buf);
  centrallista_ler(&m, buf);
  CONFERE(m.n == l.n && !memcmp(m.item, l.item, sizeof(int) * (size_t)l.n), "ida e volta");
  centrallista_ler(&m, NULL);
  CONFERE(m.n == 7, "sem arquivo: padrao");
  m.n = 0;
  centrallista_escrever(&m, buf, sizeof buf);
  CONFERE(!strcmp(buf, "-\n"), "vazia grava '-' (%s)", buf);
  centrallista_ler(&m, buf);
  CONFERE(m.n == 0, "'-' le vazia, nao o padrao");
  centrallista_ler(&m, "dolbyAtmos\r\nchaveDoFuturo\n dolbyAtmos\nvidroLocal");
  CONFERE(m.n == 2 && m.item[0] == central_catalogo_achar("dolbyAtmos") &&
          m.item[1] == central_catalogo_achar("vidroLocal"), "desconhecida e repetida puladas");
  CONFERE(centrallista_alternar(&m, central_catalogo_achar("dolbyAtmos")) == 0 && m.n == 1, "tira");
  CONFERE(centrallista_alternar(&m, central_catalogo_achar("dolbyAtmos")) == 1 &&
          m.item[m.n - 1] == central_catalogo_achar("dolbyAtmos"), "poe no fim");
  m.n = 0;
  n = central_catalogo_n();
  for (i = 0; i < n && m.n < CENTRAL_MAX; i++) centrallista_alternar(&m, i);
  CONFERE(m.n == CENTRAL_MAX && centrallista_alternar(&m, CENTRAL_MAX) == -1, "cheia recusa");
}

static void ajustesRapido(const char *dir) {
  int i, op, n = 0;
  char caminho[600], *t = NULL;
  FILE *f;
  long tam;
  char antes[64];
  ajustes_dir(dir);
  for (i = 0; i < central_catalogo_n(); i++) {
    const CentralItem *c = central_catalogo(i);
    op = ajustes_rapido_op(c->chave);
    // A sincronia por audio so existe no Android (escondida nos outros builds).
    if (op < 0 && !strcmp(c->chave, "legendaSyncAudioLocal")) continue;
    CONFERE(op >= 0, "catalogo: %s nao vira atalho neste build", c->chave);
    if (op >= 0) { n++; CONFERE(ajustes_rapido_rotulo(op)[0] && ajustes_rapido_valor(op)[0], "%s sem texto", c->chave); }
  }
  printf("catalogo: %d de %d chaves viram atalho\n", n, central_catalogo_n());
  CONFERE(ajustes_rapido_op("idioma") < 0, "idioma: lista longa fica fora");
  CONFERE(ajustes_rapido_op("itensFileiraLocal") < 0, "itens por fileira: folha de risco fica fora");
  CONFERE(ajustes_rapido_op("-versao") < 0 && ajustes_rapido_op("naoExiste") < 0, "chave invalida");
  CONFERE(ajustes_rapido_ligado(ajustes_rapido_op("qualidade")) == -1, "qualidade nao e interruptor");
  op = ajustes_rapido_op("dolbyVision");
  CONFERE(op >= 0 && ajustes_rapido_ligado(op) >= 0, "dolbyVision e interruptor");
  // A cor de destaque: 18 fixos, o atual muda e persiste como em Ajustes.
  CONFERE(ajustes_rapido_op("corDestaque") == AJ_RAPIDO_COR && ajustes_rapido_cor_n() == 18, "corDestaque");
  CONFERE(ajustes_rapido_ligado(AJ_RAPIDO_COR) == -1, "cor nao e interruptor");
  CONFERE(ajustes_rapido_cor_definir(15) == 1 && ajustes_rapido_cor_atual() == 15 &&
          !strcmp(ajustes_rapido_valor(AJ_RAPIDO_COR), "Oceano"), "cor: Oceano gravada");
  CONFERE(ajustes_rapido_passo(AJ_RAPIDO_COR, 1) && ajustes_rapido_cor_atual() == 16, "cor: passo adiante");
  CONFERE(ajustes_rapido_cor_definir(18) == 0 && ajustes_rapido_cor_definir(-1) == 0, "cor: indice invalido");
  ajustes_rapido_cor_definir(0);
  snprintf(antes, sizeof antes, "%s", ajustes_rapido_valor(op));
  { int l0 = ajustes_rapido_ligado(op);
    CONFERE(ajustes_rapido_passo(op, 1) == 1, "passo grava");
    CONFERE(ajustes_rapido_ligado(op) == !l0 && strcmp(antes, ajustes_rapido_valor(op)), "passo troca o valor");
    CONFERE(ajustes_dolby_vision() == (ajustes_rapido_ligado(op) == 1), "o resto do app le o mesmo valor"); }
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "rb");
  CONFERE(f != NULL, "ajustes.txt gravado");
  if (f) {
    fseek(f, 0, SEEK_END); tam = ftell(f); rewind(f);
    t = calloc(1, (size_t)tam + 1);
    if (t && fread(t, 1, (size_t)tam, f) != (size_t)tam) t[0] = 0;
    fclose(f);
    CONFERE(t && strstr(t, "dolbyVision "), "a chave esta no arquivo");
    free(t);
  }
  op = ajustes_rapido_op("qualidade");
  { int k; char v0[64];
    snprintf(v0, sizeof v0, "%s", ajustes_rapido_valor(op));
    for (k = 0; k < 4; k++) ajustes_rapido_passo(op, 1);
    CONFERE(!strcmp(v0, ajustes_rapido_valor(op)), "quatro passos dao a volta em 4 valores (%s/%s)",
            v0, ajustes_rapido_valor(op)); }
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  teclaSegurada();
  lista();
  if (dir && *dir) ajustesRapido(dir);
  else { printf("FALHA: NUVIO_DADOS\n"); falhas++; }
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  puts("PASS: central de controle");
  return 0;
}
