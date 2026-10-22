// TOCAR ENQUANTO CONFERE (2.0.2): os recuos, sem rede e sem TV. A conferencia
// e a mesma de streams.c (uma por vez, fonteauto_primeira); aqui a primeira da
// fila e "aberta no player" antes do veredito, e este teste prova o que
// acontece quando o veredito, ou o player, diz que ela nao serve.
//
//   bash tests/fonteantecipa.sh
#include "fonteantecipa.h"
#include "fonteauto.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHOU %s:%d: ", __FILE__, __LINE__); \
  printf(__VA_ARGS__); printf("\n"); } } while (0)

#define N 8
typedef struct {
  unsigned rodada;
  int antecipada;        // a candidata publicada (-1 = nenhuma)
  int boa[N];            // o que a conferencia diria
  int tocou[N];
  int exclui[N];
  int playerFalhaEm;     // erro do player ANTES do veredito desta candidata (-1 = nao)
  int (*aoConferir)(int, void *);   // gancho: o fio principal age no meio da conferencia
  void *u;
} Cena;

// O mesmo que verificarOuParar de streams.c.
static int verificar(int i, void *u) {
  Cena *c = u;
  int ok = c->boa[i];
  c->tocou[i]++;
  if (c->aoConferir) c->aoConferir(i, c);
  if (i == c->antecipada) {
    if (ok && fa_player_falhou_foi(c->rodada, i)) ok = 0;
    fa_concluir(c->rodada, i, ok);
  }
  return ok;
}
static void falhou(int i, void *u) { ((Cena *)u)->exclui[i] = 1; }

// O fio principal, um quadro: o mesmo que vigiarAntecipada de app.c.
typedef struct { int aberta; int desfeitas; int abriu; } Tela;
static void quadro(Tela *t, int playerFalhou) {
  int est, i = fa_ver(&est, NULL);
  switch (fa_acao(t->aberta, i, est, playerFalhou)) {
    case FA_ACAO_ABRIR: t->aberta = i; t->abriu++; break;
    case FA_ACAO_DESFAZER_VEREDITO: t->aberta = -1; t->desfeitas++; break;
    case FA_ACAO_DESFAZER_PLAYER: fa_player_falhou(t->aberta); t->aberta = -1; t->desfeitas++; break;
    default: break;
  }
}

static Cena nova(void) {
  Cena c;
  memset(&c, 0, sizeof c);
  c.antecipada = -1; c.playerFalhaEm = -1;
  c.rodada = fa_nova_rodada();
  return c;
}
static void publicar(Cena *c, int i) { c->antecipada = i; fa_publicar(c->rodada, i, 7); }

static Tela *gT;
static int gPlayerFalha;
static int gancho(int i, void *u) {   // o fio principal roda um quadro no meio da conferencia
  (void)u;
  quadro(gT, gPlayerFalha && i == 0);
  return 0;
}

int main(void) {
  int fila[3] = { 0, 1, 2 }, tocadas = 0, r;
  Tela t;
  Cena c;

  // 1. A primeira serve: abre antes do veredito, o veredito confirma, nada se desfaz.
  c = nova(); memset(&t, 0, sizeof t); t.aberta = -1;
  c.boa[0] = 1;
  publicar(&c, 0);
  quadro(&t, 0);
  CONFERE(t.aberta == 0 && t.abriu == 1, "abre a primeira antes do veredito");
  r = fonteauto_primeira(fila, 3, verificar, falhou, &c, &tocadas);
  quadro(&t, 0);
  CONFERE(r == 0 && tocadas == 1, "confirma a primeira (r=%d tocadas=%d)", r, tocadas);
  CONFERE(t.aberta == 0 && t.desfeitas == 0, "a aberta nao e desfeita quando serve");
  CONFERE(fa_ja_tocando(t.aberta, r), "a escolha final e a que ja toca");
  CONFERE(c.tocou[1] == 0 && c.tocou[2] == 0, "a conferencia nao passa da primeira");

  // 2. A primeira nao serve (nao e video, aviso, playlist vazia, morta): o
  //    veredito desfaz e a fila segue para a proxima — a segunda, nunca pior.
  c = nova(); memset(&t, 0, sizeof t); t.aberta = -1;
  c.boa[0] = 0; c.boa[1] = 1;
  publicar(&c, 0);
  quadro(&t, 0);
  gT = &t; gPlayerFalha = 0; c.aoConferir = gancho;
  r = fonteauto_primeira(fila, 3, verificar, falhou, &c, &tocadas);
  quadro(&t, 0);
  CONFERE(r == 1, "segue para a segunda (r=%d)", r);
  CONFERE(t.aberta == -1 && t.desfeitas == 1, "a primeira aberta foi desfeita uma vez (%d)", t.desfeitas);
  CONFERE(c.exclui[0] == 1 && c.exclui[1] == 0, "so a reprovada sai da fila");
  CONFERE(!fa_ja_tocando(t.aberta, r), "a segunda ainda tem de ser aberta");
  CONFERE(c.tocou[0] == 1 && c.tocou[1] == 1 && c.tocou[2] == 0, "uma conferencia por vez, parando na que serve");

  // 3. O player erra ANTES do veredito e a conferencia diria "serve": vale como
  //    reprovada (nao se mantem uma fonte que o player recusou).
  c = nova(); memset(&t, 0, sizeof t); t.aberta = -1;
  c.boa[0] = 1; c.boa[1] = 1;
  publicar(&c, 0);
  quadro(&t, 0);
  gT = &t; gPlayerFalha = 1; c.aoConferir = gancho;
  r = fonteauto_primeira(fila, 3, verificar, falhou, &c, &tocadas);
  CONFERE(r == 1, "o erro do player antes do veredito passa para a segunda (r=%d)", r);
  CONFERE(t.aberta == -1 && t.desfeitas == 1, "desfeita pelo player");
  CONFERE(c.exclui[0] == 1, "a recusada pelo player sai da fila");

  // 4. Todas reprovam: nada fica aberto e o resultado e -1 (erro de sempre).
  c = nova(); memset(&t, 0, sizeof t); t.aberta = -1;
  publicar(&c, 0);
  quadro(&t, 0);
  r = fonteauto_primeira(fila, 3, verificar, falhou, &c, &tocadas);
  quadro(&t, 0);
  CONFERE(r == -1 && t.aberta == -1, "nenhuma serve: nada aberto (r=%d)", r);

  // 5. Rodada velha: um fio que ainda confere a escolha anterior nao publica
  //    nem conclui nada na rodada nova.
  { unsigned velha = fa_nova_rodada(), nova_;
    int est;
    nova_ = fa_nova_rodada();
    fa_publicar(velha, 3, 1);
    CONFERE(fa_ver(&est, NULL) == -1 && est == FA_NADA, "publicacao de rodada velha e ignorada");
    fa_publicar(nova_, 2, 1);
    fa_concluir(velha, 2, 0);
    CONFERE(fa_ver(&est, NULL) == 2 && est == FA_CONFERINDO, "veredito de rodada velha e ignorado");
    fa_concluir(nova_, 2, 1);
    CONFERE(fa_ver(&est, NULL) == 2 && est == FA_OK, "veredito da rodada atual vale");
    fa_nova_rodada();
    CONFERE(fa_ver(&est, NULL) == -1 && est == FA_NADA, "rodada nova apaga a publicacao");
  }

  // 6. Torrent/P2P e o que precisa de resolucao: nada publicado, nada abre.
  c = nova(); memset(&t, 0, sizeof t); t.aberta = -1;
  c.boa[0] = 1;
  quadro(&t, 0);
  CONFERE(t.aberta == -1 && t.abriu == 0, "sem publicacao nao abre nada");

  // 7. A escolha final e OUTRA que a aberta (lista que mudou): desfaz, nao reabre a mesma.
  CONFERE(!fa_ja_tocando(2, 1) && !fa_ja_tocando(-1, 1) && fa_ja_tocando(1, 1), "fa_ja_tocando");
  CONFERE(fa_acao(0, 5, FA_CONFERINDO, 0) == FA_ACAO_DESFAZER_VEREDITO, "lista trocada desfaz");
  CONFERE(fa_acao(-1, 0, FA_OK, 0) == FA_ACAO_NADA, "ja concluida antes de abrir: nao abre");
  CONFERE(fa_acao(-1, 0, FA_RUIM, 0) == FA_ACAO_NADA, "ja reprovada antes de abrir: nao abre");

  // 8. Fonte preparada ao abrir o titulo: so serve ao MESMO Play.
  { FaPreparada p = { "tt1:1:2", 9, 3, 4, 1000 };
    FaAgora a = { "tt1:1:2", 9, 3, 12, 1, 5000 };
    CONFERE(fa_preparada_vale(&p, &a), "mesmo alvo, lista e lembrada: serve");
    a.alvo = "tt1:1:3";
    CONFERE(!fa_preparada_vale(&p, &a), "outro episodio nao serve");
    a.alvo = "tt1:1:2"; a.lista = 10;
    CONFERE(!fa_preparada_vale(&p, &a), "lista trocada nao serve");
    a.lista = 9; a.lembrada = -1;
    CONFERE(!fa_preparada_vale(&p, &a), "outra fonte lembrada nao serve");
    a.lembrada = 3; a.idxDisponivel = 0;
    CONFERE(!fa_preparada_vale(&p, &a), "descartada pelo automatico nao serve");
    a.idxDisponivel = 1; a.n = 4;
    CONFERE(!fa_preparada_vale(&p, &a), "indice fora da lista nao serve");
    a.n = 12; p.idadeMs = FA_PREPARADA_VALE_MS + 1;
    CONFERE(!fa_preparada_vale(&p, &a), "preparada velha nao serve");
    p.idadeMs = 1000; a.idadeListaMs = FA_LINK_VALE_MS + 1;
    CONFERE(!fa_preparada_vale(&p, &a), "link da lista envelhecido nao serve");
    p.idx = -1; a.idadeListaMs = 0;
    CONFERE(!fa_preparada_vale(&p, &a) && !fa_preparada_vale(NULL, &a), "sem fonte nao serve");
  }

  if (falhas) { printf("fonteantecipa: %d falha(s)\n", falhas); return 1; }
  printf("fonteantecipa: ok\n");
  return 0;
}
