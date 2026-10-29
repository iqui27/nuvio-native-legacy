// TRAVA JUNTOS: o episodio do player vira marca nos outros membros, com o
// Diario de verdade; conta, ilha e idioma sao dubles.
#include "travajuntos.h"
#include "trava.h"
#include "diario.h"
#include "perfis.h"
#include "catalogo.h"
int cat_indice_titulo(const char *id, int preferido) { (void)id; (void)preferido; return -1; }
int cat_n_episodios(int ix) { (void)ix; return 0; }
const CatEp *cat_episodio(int ix, int i) { (void)ix; (void)i; return 0; }
#include "contapend.h"
#include "ilhaacao.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char pasta[256];
int perfis_ativo(void) { return 1; }
char *dados_ler(const char *nome) {
  char c[512]; FILE *f; long n; char *b;
  snprintf(c, sizeof c, "%s/%s", pasta, nome);
  f = fopen(c, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *c) {
  char p[512]; FILE *f;
  snprintf(p, sizeof p, "%s/%s", pasta, nome);
  f = fopen(p, "wb"); if (!f) return 0; fputs(c, f); fclose(f); return 1;
}
int dados_apagar(const char *nome) { char p[512]; snprintf(p, sizeof p, "%s/%s", pasta, nome); return remove(p) == 0; }

const char *i18n(const char *s) { return s; }
static ContaPerfil pf[3];
int perfis_n(void) { return 3; }
const ContaPerfil *perfis_item(int i) { return &pf[i]; }

static int nPush, ultPerfil, ultVisto, nChute;
int contapend_episodios_perfil(int perfil, const char *imdb, const char *tipo, const VistoPar *pares, int n, int visto) {
  nPush++; ultPerfil = perfil; ultVisto = visto; (void)imdb; (void)tipo; (void)pares; return n;
}
void contapend_chutar(void) { nChute++; }

static IlhaAcao ult; static char frase[200], ctxCopia[512]; static int nIlha;
void ilhaacao_feita(const IlhaAcao *a) {
  nIlha++; ult = *a; snprintf(frase, sizeof frase, "%s", a->frase); ult.frase = frase;
  assert(a->ctxN <= sizeof ctxCopia); memcpy(ctxCopia, a->ctx, a->ctxN); ult.ctx = ctxCopia;
}

extern long long diario_relogio_teste;

static void viu(int perfil, int t, int e, char origem, unsigned flags) {
  DiarioEntrada d;
  memset(&d, 0, sizeof d);
  snprintf(d.id, sizeof d.id, "tt1"); d.tipo = DIARIO_SERIE; d.origem = origem; d.flags = (unsigned short)flags;
  d.temporada = (short)t; d.episodio = (short)e; d.hora = -1; d.quando = 1790000000LL + e * 100000LL;
  assert(diario_adicionar(perfil, &d) >= 0);
  diario_passo();
}

int main(void) {
  int p3[3] = {1, 2, 3}, id;
  char idTrava[TRAVA_ID_MAX];
  TravaPerfil perfis[3] = {0};
  long long q;
  char cmd[600];

  snprintf(pf[0].nome, 64, "Ana"); pf[0].indice = 1;
  snprintf(pf[1].nome, 64, "Beto"); pf[1].indice = 2;
  snprintf(pf[2].nome, 64, "Caio"); pf[2].indice = 3;
  snprintf(pasta, sizeof pasta, "%s/nuvio-travajuntos-teste-%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
  snprintf(cmd, sizeof cmd, "rm -rf '%s' && mkdir -p '%s'", pasta, pasta); assert(system(cmd) == 0);
  diario_relogio_teste = 1790000000LL;
  diario_dono("conta-a"); trava_definir_conta("conta-a");
  for (int i = 0; i < 3; i++) {
    perfis[i].indice = pf[i].indice;
    snprintf(perfis[i].nome, sizeof perfis[i].nome, "%s", pf[i].nome);
  }
  trava_definir_perfis(perfis, 3, 1);
  diario_iniciar(); travajuntos_iniciar();

  // sem trava: nada
  viu(1, 1, 1, DIARIO_ORIG_PLAYER, 0);
  assert(nIlha == 0 && nPush == 0 && !diario_episodio(2, "tt1", 1, 1, &q));

  id = trava_criar("tt1", "Severance", "", TRAVA_JUNTOS, 0, p3, 3);
  assert(id >= 0);
  snprintf(idTrava, sizeof idTrava, "%s", trava_i(id)->id);
  // parcial, conta e especial nao propagam
  viu(1, 1, 2, DIARIO_ORIG_PLAYER, DIARIO_F_PARCIAL);
  viu(1, 1, 3, DIARIO_ORIG_CONTA, 0);
  viu(1, 0, 1, DIARIO_ORIG_PLAYER, 0);
  assert(nIlha == 0 && !diario_episodio(2, "tt1", 1, 2, &q));

  // player termina o E4: os dois outros ganham
  viu(1, 1, 4, DIARIO_ORIG_PLAYER, 0);
  assert(diario_episodio(2, "tt1", 1, 4, &q) && diario_episodio(3, "tt1", 1, 4, &q));
  assert(nPush == 2 && ultVisto == 1 && nIlha == 1 && nChute >= 1);
  assert(!strcmp(frase, "Marcado para Beto e Caio") && !strcmp(ult.titulo, "Severance") && ult.desfazer);
  {
    DiarioEntrada d;
    assert(diario_por_titulo(2, "tt1", &d, 1) == 1 && d.origem == DIARIO_ORIG_MANUAL && (d.flags & DIARIO_F_JUNTOS) && d.comPerfis == 7);
    assert(diario_por_titulo(1, "tt1", NULL, 0) >= 1);
  }
  // sem eco: o evento das entradas novas nao dispara de novo
  diario_passo();
  assert(nIlha == 1 && nPush == 2);

  // o Beto ja tinha visto o E5: so o Caio ganha
  viu(2, 1, 5, DIARIO_ORIG_CONTA, 0);
  nPush = 0; nIlha = 0;
  viu(1, 1, 5, DIARIO_ORIG_PLAYER, 0);
  assert(nPush == 1 && ultPerfil == 3 && !strcmp(frase, "Marcado para Caio"));

  // Desfazer: tira dos alvos, nao do que o Beto viu por conta propria
  ult.desfazer(ult.ctx);
  assert(!diario_episodio(3, "tt1", 1, 5, &q) && diario_episodio(2, "tt1", 1, 5, &q) && ultVisto == 0);

  // trava pausada nao propaga
  trava_definir_pausada(idTrava, 1); nIlha = 0;
  viu(1, 1, 6, DIARIO_ORIG_PLAYER, 0);
  assert(nIlha == 0 && !diario_episodio(2, "tt1", 1, 6, &q));
  // Separados nao propaga
  trava_definir_pausada(idTrava, 0); trava_definir_modo(idTrava, TRAVA_SEPARADOS);
  viu(1, 1, 7, DIARIO_ORIG_PLAYER, 0);
  assert(nIlha == 0 && !diario_episodio(2, "tt1", 1, 7, &q));
  assert(trava_por_id(idTrava));

  snprintf(cmd, sizeof cmd, "rm -rf '%s'", pasta); (void)!system(cmd);
  puts("travajuntos: ok");
  return 0;
}
