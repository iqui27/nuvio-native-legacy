// PEGOU NO SONO NA ILHA: a pergunta e a retomada (dormiu.h, parte de tela).
//
// QUANDO: a pagina do titulo assentada num titulo com suspeita aberta no
// Diario. Uma vez por titulo por sessao do app (ignorar = pergunta de novo na
// proxima abertura do app, nunca em loop).
// O QUE: "Voce pegou no sono em <Titulo>?" com o ponto ("Voltar para ~1:12").
// [Voltar] poe a retomada no ponto (progresso local + conta, o mesmo caminho
// do player) e confirma o sono. [Nao dormi] apaga a suspeita e ensina o limiar.
#include "dormiu.h"
#include "catalogo.h"
#include "detail.h"
#include "idioma.h"
#include "ilha.h"
#include "progresso.h"
#include <stdio.h>
#include <string.h>

#define PERGUNTADOS_MAX 32

static char perguntados[PERGUNTADOS_MAX][DIARIO_ID_MAX];
static int nPerg;
static struct { int ativo; DiarioEntrada e; } aberta;

static int jaPerguntou(const char *id) {
  int i;
  for (i = 0; i < nPerg; i++) if (!strcmp(perguntados[i], id)) return 1;
  return 0;
}

static void anotar(const char *id) {
  if (nPerg == PERGUNTADOS_MAX) { memmove(perguntados, perguntados + 1, sizeof perguntados[0] * (PERGUNTADOS_MAX - 1)); nPerg--; }
  snprintf(perguntados[nPerg++], sizeof perguntados[0], "%s", id);
}

void dormiu_perguntar(const char *titulo, const char *poster, const char *arte, const DiarioEntrada *e) {
  static IlhaModal m;     // a ilha copia; estatico so para nao pesar a pilha
  IlhaAvisoEx a;
  char chave[48], forte[200], texto[320], rot[16], ep[24];
  memset(&m, 0, sizeof m);
  memset(&a, 0, sizeof a);
  dormiu_rotulo(e->dormiuSeg, rot, sizeof rot);
  ep[0] = 0;
  if (e->temporada || e->episodio) snprintf(ep, sizeof ep, "T%dE%d · ", e->temporada, e->episodio);
  snprintf(chave, sizeof chave, "dormiu:%s", e->id);
  snprintf(texto, sizeof texto, i18n("Você pegou no sono em %s?"), ilha_forte(forte, sizeof forte, titulo));
  snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Pegou no sono"));
  snprintf(m.titulo, sizeof m.titulo, "%s", titulo);
  snprintf(m.linha, sizeof m.linha, "%s%s ~%s", ep, i18n("Você parou de mexer no controle por volta de"), rot);
  snprintf(m.texto, sizeof m.texto, "%s", i18n("Voltar põe a retomada ali. Só você vê isto."));
  snprintf(m.arte, sizeof m.arte, "%s", arte && arte[0] ? arte : (poster ? poster : ""));
  m.tipo = ILHA_ACENTO;
  m.nBotoes = 2;
  snprintf(m.botao[0], sizeof m.botao[0], "%s ~%s", i18n("Voltar para"), rot);
  snprintf(m.botaoIcone[0], sizeof m.botaoIcone[0], "aj_rotate-ccw-clock");
  snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Não dormi"));
  a.chave = chave; a.tipo = ILHA_ACENTO; a.icone = "aj_moon";
  a.texto = texto; a.ms = 12000u; a.modal = &m; a.capa = poster;
  ilha_avisar_ex(&a);
  aberta.ativo = 1; aberta.e = *e;
}

void dormiu_passo(void) {
  static int idxVisto = -1;
  const CatItem *ci;
  DiarioEntrada e;
  char id[DIARIO_ID_MAX];
  int idx;
  if (!detail_aberto() || !detail_assentado()) { idxVisto = -1; return; }
  idx = detail_indice();
  if (idx == idxVisto) return;            // um teste por titulo aberto
  idxVisto = idx;
  ci = cat_item(idx);
  if (!ci || !ci->imdb[0]) return;
  diario_separar_id(ci->imdb, id, sizeof id, NULL, NULL);
  if (jaPerguntou(id) || !dormiu_suspeita(id, &e) || e.dormiuSeg <= 0) return;
  anotar(id);
  dormiu_perguntar(ci->titulo, ci->poster, ci->backdrop, &e);
}

void dormiu_acao(const char *chave, int botao) {
  char rot[16], txt[160];
  DiarioEntrada e;
  if (!aberta.ativo || strncmp(chave, "dormiu:", 7) || strcmp(chave + 7, aberta.e.id)) return;
  e = aberta.e;
  aberta.ativo = 0;
  if (botao == 1) {
    int idx = detail_aberto() ? detail_indice() : -1;
    const CatItem *ci = idx >= 0 ? cat_item(idx) : NULL;
    char base[DIARIO_ID_MAX];
    double dur = e.durSeg > 1 ? e.durSeg : 0;
    if (ci) diario_separar_id(ci->imdb, base, sizeof base, NULL, NULL);
    if (dur > 1) {
      if (ci && !strcmp(base, e.id)) cat_salvar_progresso_ep(idx, e.dormiuSeg, dur, e.temporada, e.episodio);
      else prog_gravar_local(e.id, e.temporada, e.episodio, e.dormiuSeg, dur);
    }
    dormiu_confirmar(&e);
    dormiu_rotulo(e.dormiuSeg, rot, sizeof rot);
    snprintf(txt, sizeof txt, i18n("Retomada em ~%s"), rot);
    ilha_avisar("dormiu-ok", ILHA_OK, "aj_rotate-ccw-clock", txt, 4000u, 0);
  } else if (botao == 2) {
    dormiu_negar(&e);
    ilha_avisar("dormiu-ok", ILHA_INFO, "aj_moon", i18n("Certo, sem marca de sono."), 3000u, 0);
  }
}
