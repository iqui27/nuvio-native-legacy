#include "conquistasdados.h"
#include "dados.h"
#include "perfis.h"
#include "sessao.h"
#include "catalogo.h"
#include "progresso.h"
#include "ajustes.h"
#include <time.h>
#include "trilhasdados.h"
#include "trilhas_catalogo.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Conta (sub do JWT, ate 79 bytes em sessao.c) e perfil. Hex evita caminhos
// e colisoes por remover caracteres; arquivos antigos sem conta nao migram.
static const char *arquivo(const char *sufixo) {
  static char nome[208];
  char conta[160];
  const unsigned char *u = (const unsigned char *)sessao_usuario();
  size_t n = 0;
  while (*u && n + 2 < sizeof conta) {
    snprintf(conta + n, 3, "%02x", *u++); n += 2;
  }
  conta[n] = 0;
  snprintf(nome, sizeof nome, "conquistas-%s-p%d%s.txt", n ? conta : "anon", perfis_ativo(), sufixo);
  return nome;
}

void cqd_carregar(CqEstado *s) {
  char *txt = dados_ler(arquivo(""));
  cqregra_ler(txt, s);
  free(txt);
}

int cqd_gravar(const CqEstado *s) {
  static char buf[CQ_MAX * (CQ_CHAVE_MAX + CQ_NOME_MAX + 64)];
  if (!cqregra_serializar(s, buf, sizeof buf)) return 0;
  return dados_gravar(arquivo(""), buf);
}

// The completion ledger survives resume removal; a later explicit unwatch wins.
static int ler_passos(const CqTrilha *t, const char *concluido,
                      unsigned char *visto, long long *fim) {
  ProgRegistro regs[PROG_MAX];
  char *txt = dados_ler(arquivo("-completed")), *linha, *ctx;
  int n = prog_ler(regs, PROG_MAX), vistos = 0;
  memset(fim, 0, (size_t)t->n * sizeof *fim);
  for (linha = txt ? strtok_r(txt, "\n", &ctx) : NULL; linha; linha = strtok_r(NULL, "\n", &ctx)) {
    char id[24]; long long ms;
    if (sscanf(linha, "%23s %lld", id, &ms) != 2 || ms <= 0) continue;
    for (int i = 0; i < t->n; i++)
      if (!strcmp(id, t->p[i].imdb) && ms > fim[i]) fim[i] = ms;
  }
  free(txt);
  for (int i = 0; i < t->n; i++) {
    long long marcaMs = 0;
    int h = cat_historico_estado_tempo(t->p[i].imdb, "movie", &marcaMs);
    if (h == 1 && marcaMs > fim[i]) fim[i] = marcaMs;
    if (concluido && !strcmp(concluido, t->p[i].imdb)) fim[i] = prog_agora_ms();
    for (int j = 0; j < n; j++) {
      const ProgRegistro *r = &regs[j];
      if (!strcmp(r->contentId, t->p[i].imdb) && !strcmp(r->tipo, "movie") &&
          r->episodio == 0 && r->tocou && r->durSeg >= 120 && r->posSeg >= r->durSeg) {
        long long ms = r->lastWatchedMs > 0 ? r->lastWatchedMs : 1;
        if (ms > fim[i]) fim[i] = ms;
      }
    }
    visto[i] = h == 1 || (fim[i] > 0 && (h < 0 || fim[i] > marcaMs));
    vistos += visto[i];
  }
  return vistos;
}

static void atualizar(const char *concluido) {
  CqEstado s, antes;
  char buf[16384];
  size_t off = 0;
  if (!ajustes_conquistas()) return;
  cqd_carregar(&s); antes = s; buf[0] = 0;
  for (int ti = 0; ti < TRILHAS_G_N; ti++) {
    const CqTrilha *t = &TRILHAS_G[ti];
    long long fim[CQ_TRILHA_MAX];
    unsigned char visto[CQ_TRILHA_MAX];
    char chave[CQ_CHAVE_MAX + 1];
    if (t->n > CQ_TRILHA_MAX) continue;
    int vistos = ler_passos(t, concluido, visto, fim);
    for (int i = 0; i < t->n; i++) if (fim[i]) {
      int n = snprintf(buf + off, sizeof buf - off, "%s %lld\n", t->p[i].imdb, fim[i]);
      if (n < 0 || (size_t)n >= sizeof buf - off) return;
      off += (size_t)n;
    }
    snprintf(chave, sizeof chave, "trilha:%s", t->id);
    if (!vistos && !cqregra_buscar(&s, chave)) continue;
    const char *nome = ti == 0 ? "Os vencedores do Oscar de Melhor Filme" : t->nome;
    int subiu = cqregra_registrar(&s, chave, CQ_FAM_PREMIO, nome,
                      cqregra_nivel(vistos, t->n, 0), vistos, t->n, (long)time(NULL));
    if (!concluido && subiu > 0)
      for (int i = 0; i < s.n; i++)
        if (!strcmp(s.e[i].chave, chave)) s.e[i].avisado = s.e[i].nivel;
  }
  char *antigo = dados_ler(arquivo("-completed"));
  if ((antigo && strcmp(antigo, buf)) || (!antigo && off)) dados_gravar(arquivo("-completed"), buf);
  free(antigo);
  if (memcmp(&s, &antes, sizeof s)) cqd_gravar(&s);
}

void cqd_atualizar_local(void) { atualizar(NULL); }

void cqd_concluir_local(const char *id) {
  // Baseline antes de gravar o progresso desta sessao: importar nao avisa.
  atualizar(NULL);
  if (id && *id) atualizar(id);
}

int cqd_raridade(const char *chave) { (void)chave; return -1; }

void cqd_mapa(CqMapa *m) { memset(m, 0, sizeof *m); }

int cqd_amigos_na_trilha(const char *chave) { (void)chave; return 0; }

int cqd_comunidade(void) { return 0; }
int cqd_temporada(void) { return 0; }

// ------------------------------- trilhas locais -----------------------------
int cqd_trilha_n(void) { return TRILHAS_G_N; }

const CqTrilha *cqd_trilha(int i) { return i >= 0 && i < TRILHAS_G_N ? &TRILHAS_G[i] : NULL; }

const CqTrilha *cqd_trilha_buscar(const char *chave) {
  if (!chave) return NULL;
  if (!strncmp(chave, "trilha:", 7)) chave += 7;
  for (int i = 0; i < TRILHAS_G_N; i++) if (!strcmp(chave, TRILHAS_G[i].id)) return &TRILHAS_G[i];
  return NULL;
}

int cqd_trilha_vistos(const CqTrilha *t, unsigned char *visto, int max) {
  long long fim[CQ_TRILHA_MAX];
  if (!t || !visto || t->n > CQ_TRILHA_MAX || t->n > max) return 0;
  ler_passos(t, NULL, visto, fim);
  return t->n;
}

int cqd_trilha_proximo(const CqTrilha *t, unsigned char *visto, int n) {
  if (!t || !visto) return -1;
  for (int i = 0; i < n; i++) if (!visto[i]) return i;
  return -1;
}

int cqd_trilhas_de(const char *imdb, const char *tipo, const CqTrilha **dst, int max) {
  int n = 0;
  if (!imdb || !*imdb || !tipo || strcmp(tipo, "movie")) return 0;
  for (int i = 0; i < TRILHAS_G_N && n < max; i++) {
    const CqTrilha *t = &TRILHAS_G[i];
    for (int j = 0; j < t->n; j++)
      if (!strcmp(t->p[j].imdb, imdb)) { dst[n++] = t; break; }
  }
  return n;
}
