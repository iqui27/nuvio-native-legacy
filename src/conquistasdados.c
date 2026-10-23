#include "conquistasdados.h"
#include "dados.h"
#include "perfis.h"
#include "sessao.h"
#include "catalogo.h"
#include "progresso.h"
#include "ajustes.h"
#include <time.h>
#include "trilhasdados.h"
#include "conquistas_oscar.inc"
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

// Conclusao por titulo, separada da posicao de retomada. O catalogo das duas
// trilhas limita o arquivo a ~169 linhas; sem cache, conta/perfil nunca herdam
// memoria.
static void atualizar(const char *concluido) {
  CqEstado s, antes;
  ProgRegistro regs[PROG_MAX];
  long long fim[sizeof OSCAR_FILMES / sizeof OSCAR_FILMES[0]] = {0};
  char *txt, *linha, *ctx, buf[8192];
  int n, mudou = 0, vistos[2] = {0, 0}, totais[2];
  totais[0] = (int)(sizeof OSCAR_FILMES / sizeof OSCAR_FILMES[0]);
  totais[1] = TRILHAS_G_N > 1 ? TRILHAS_G[1].n : 0;
  if (!ajustes_conquistas()) return;
  txt = dados_ler(arquivo("-completed"));
  for (linha = txt ? strtok_r(txt, "\n", &ctx) : NULL; linha; linha = strtok_r(NULL, "\n", &ctx)) {
    char id[24]; long long ms;
    if (sscanf(linha, "%23s %lld", id, &ms) != 2 || ms <= 0) continue;
    for (int i = 0; i < totais[0]; i++)
      if (!strcmp(id, OSCAR_FILMES[i]) && ms > fim[i]) fim[i] = ms;
  }
  free(txt);
  n = prog_ler(regs, PROG_MAX);
  for (int ti = 0; ti < TRILHAS_G_N; ti++) {
    const CqTrilha *t = &TRILHAS_G[ti];
    for (int i = 0; i < t->n; i++) {
      long long marcaMs = 0;
      int h = cat_historico_estado_tempo(t->p[i].imdb, "movie", &marcaMs);
      if (ti == 0) {
        if (h == 1 && marcaMs > fim[i]) { fim[i] = marcaMs; mudou = 1; }
        if (concluido && !strcmp(concluido, t->p[i].imdb)) {
          long long ms = prog_agora_ms();
          if (ms > fim[i]) { fim[i] = ms; mudou = 1; }
        }
        for (int j = 0; j < n; j++) {
          const ProgRegistro *r = &regs[j];
          if (!strcmp(r->contentId, t->p[i].imdb) && !strcmp(r->tipo, "movie") &&
              r->episodio == 0 && r->tocou != 0 && r->durSeg >= 120 && r->posSeg >= r->durSeg) {
            // Linha migrada sem horario ainda prova conclusao, mas perde para
            // qualquer desmarcacao conhecida.
            long long ms = r->lastWatchedMs > 0 ? r->lastWatchedMs : 1;
            if (ms > fim[i]) { fim[i] = ms; mudou = 1; }
          }
        }
      }
      vistos[ti] += h == 1 || (ti == 0 && fim[i] > 0 && (h < 0 || fim[i] > marcaMs));
    }
  }
  if (mudou) {
    size_t off = 0;
    buf[0] = 0;
    for (int i = 0; i < totais[0]; i++)
      if (fim[i]) off += (size_t)snprintf(buf + off, sizeof buf - off, "%s %lld\n", OSCAR_FILMES[i], fim[i]);
    dados_gravar(arquivo("-completed"), buf);
  }
  cqd_carregar(&s);
  antes = s;
  if (!vistos[0] && !cqregra_buscar(&s, "trilha:oscar-melhor-filme")) return;
  int subiu = cqregra_registrar(&s, "trilha:oscar-melhor-filme", CQ_FAM_PREMIO,
                    "Os vencedores do Oscar de Melhor Filme", cqregra_nivel(vistos[0], totais[0], 0),
                    vistos[0], totais[0], (long)time(NULL));
  if (!concluido && subiu > 0) {
    // So a subida retroativa e silenciosa; nao consome outros avisos reais.
    for (int i = 0; i < s.n; i++)
      if (!strcmp(s.e[i].chave, "trilha:oscar-melhor-filme")) s.e[i].avisado = s.e[i].nivel;
  }
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
  int i, n;
  if (!t) return 0;
  n = t->n < max ? t->n : max;
  for (i = 0; i < n; i++)
    visto[i] = cat_historico_estado_id(t->p[i].imdb, "movie") == 1;
  return n;
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
