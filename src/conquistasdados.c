#include "conquistasdados.h"
#include "dados.h"
#include "perfis.h"
#include "sessao.h"
#include "catalogo.h"
#include "progresso.h"
#include "ajustes.h"
#include <time.h>
#include "conquistas_oscar.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Conta (sub do JWT, ate 79 bytes em sessao.c) e perfil. Hex evita caminhos
// e colisoes por remover caracteres; arquivos antigos sem conta nao migram.
static const char *arquivo(void) {
  static char nome[192];
  char conta[160];
  const unsigned char *u = (const unsigned char *)sessao_usuario();
  size_t n = 0;
  while (*u && n + 2 < sizeof conta) {
    snprintf(conta + n, 3, "%02x", *u++); n += 2;
  }
  conta[n] = 0;
  snprintf(nome, sizeof nome, "conquistas-%s-p%d.txt", n ? conta : "anon", perfis_ativo());
  return nome;
}

void cqd_carregar(CqEstado *s) {
  char *txt = dados_ler(arquivo());
  cqregra_ler(txt, s);
  free(txt);
}

int cqd_gravar(const CqEstado *s) {
  static char buf[CQ_MAX * (CQ_CHAVE_MAX + CQ_NOME_MAX + 64)];
  if (!cqregra_serializar(s, buf, sizeof buf)) return 0;
  return dados_gravar(arquivo(), buf);
}

// Historico conhecido manda (inclusive desmarcado); sem ele, so filme com
// conclusao tocada. Nao usa um episodio nem seek como prova do filme inteiro.
void cqd_atualizar_local(void) {
  CqEstado s, antes;
  ProgRegistro regs[PROG_MAX];
  int n, vistos = 0, total = (int)(sizeof OSCAR_FILMES / sizeof OSCAR_FILMES[0]);
  if (!ajustes_conquistas()) return;
  n = prog_ler(regs, PROG_MAX);
  for (int i = 0; i < total; i++) {
    int h = cat_historico_estado_id(OSCAR_FILMES[i], "movie");
    if (h >= 0) { vistos += h == 1; continue; }
    for (int j = 0; j < n; j++) {
      const ProgRegistro *r = &regs[j];
      if (!strcmp(r->contentId, OSCAR_FILMES[i]) && !strcmp(r->tipo, "movie") &&
          r->episodio == 0 && r->tocou != 0 && r->durSeg >= 120 && r->posSeg >= r->durSeg) {
        vistos++; break;
      }
    }
  }
  cqd_carregar(&s);
  antes = s;
  if (!vistos && !cqregra_buscar(&s, "trilha:oscar-melhor-filme")) return;
  cqregra_registrar(&s, "trilha:oscar-melhor-filme", CQ_FAM_PREMIO,
                    "Os vencedores do Oscar de Melhor Filme", cqregra_nivel(vistos, total, 0),
                    vistos, total, (long)time(NULL));
  if (memcmp(&s, &antes, sizeof s)) cqd_gravar(&s);
}

int cqd_raridade(const char *chave) { (void)chave; return -1; }

void cqd_mapa(CqMapa *m) { memset(m, 0, sizeof *m); }

int cqd_amigos_na_trilha(const char *chave) { (void)chave; return 0; }

int cqd_comunidade(void) { return 0; }
int cqd_temporada(void) { return 0; }
