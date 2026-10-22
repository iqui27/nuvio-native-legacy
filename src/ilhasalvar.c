// Salvar na ilha — ver ilhasalvar.h.
#include "ilhasalvar.h"
#include "sessao.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "ajustes.h"
#include "atividade.h"
#include "dados.h"
#include "descoberta.h"
#include "idioma.h"
#include "salvos.h"
#include "simkl.h"
#include "trakt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQ_PERGUNTADO "salvar-perguntado.txt"
#define ARQ_ANTIGO     "salvos-intro.txt"   // o explicador de salvosintro.c
#define CHAVE_ONDE     "salvar:onde"

extern int trakt_watchlist_tipo(const char *imdb, const char *tipo, int adicionar);

static int perguntaAberta;
static Uint32 perguntaDesde;
static int opcao[3];            // AJ_SALVOS_* de cada botao, na ordem em que saem
static int nOpcao;
static CatItem pendente;
static int pendenteEntrar;

int ilhasalvar_pergunta_aberta(void) { return perguntaAberta; }

static int jaFoiPerguntado(void) {
  char *s = dados_ler(ARQ_PERGUNTADO);
  if (s) { free(s); return 1; }
  s = dados_ler(ARQ_ANTIGO);
  if (s) { free(s); return 1; }
  return salvos_n() > 0;   // quem ja tem lista usa o "+" faz tempo
}

const char *ilhasalvar_destino(void) {
  if (ajustes_salvos_no_trakt() && trakt_ativo()) return i18n("Watchlist do Trakt");
  if (ajustes_salvos_no_simkl() && simkl_ativo()) return i18n("Plan to Watch do Simkl");
  return i18n("Lista do Nuvio");
}

typedef struct { CatItem ci; int entrou; } SalvarCtx;

static void gravarLista(const CatItem *ci, int entrar) {
  salvos_definir(ci, entrar);
  atividade_salvo(ci, entrar);
  if (ajustes_salvos_no_trakt() && trakt_ativo()) trakt_watchlist_tipo(ci->imdb, ci->tipo, entrar);
  else if (ajustes_salvos_no_simkl() && simkl_ativo()) simkl_lista_tipo(ci->imdb, ci->tipo, entrar);
  cat_definir_na_lista_imdb(ci->imdb, entrar);
  if (entrar) {
    if (ajustes_salvos_no_trakt() && trakt_ativo()) cat_marcar_fonte_imdb(ci->imdb, CAT_FONTE_TRAKT);
    else if (ajustes_salvos_no_simkl() && simkl_ativo()) cat_marcar_fonte_imdb(ci->imdb, CAT_FONTE_SIMKL);
  }
  desc_remontar_fileiras();
}

// DESFAZER do salvar/remover: o inverso exato, pelo mesmo caminho de gravacao.
static void desfazerSalvar(const void *p) {
  const SalvarCtx *c = p;
  gravarLista(&c->ci, !c->entrou);
}

void ilhasalvar_aviso(const CatItem *ci, int entrou) {
  char txt[200];
  static SalvarCtx c;
  IlhaAcao a;
  if (!ci) return;
  if (entrou) snprintf(txt, sizeof txt, i18n("Salvo em %s"), ilhasalvar_destino());
  else snprintf(txt, sizeof txt, "%s", i18n("Removido da lista"));
  c.ci = *ci; c.entrou = entrou;
  memset(&a, 0, sizeof a);
  a.icone = "aj_bookmark"; a.frase = txt; a.titulo = ci->titulo;
  a.onde = "";   // a frase ja diz onde
  a.thumb = ci->poster; a.arte = ci->backdrop; a.tipo = entrou ? ILHA_ACENTO : ILHA_INFO; a.voar = entrou;
  a.desfazer = desfazerSalvar; a.ctx = &c; a.ctxN = sizeof c;
  ilhaacao_feita(&a);
}

void ilhasalvar_executar(const CatItem *ci, int entrar) {
  if (!ci || !ci->imdb[0]) return;
  gravarLista(ci, entrar);
  ilhasalvar_aviso(ci, entrar);
}

static void concluir(int escolha /* AJ_SALVOS_* ou -1 = manter o padrao */) {
  perguntaAberta = 0;
  if (escolha == AJ_SALVOS_LOCAL || escolha == AJ_SALVOS_TRAKT || escolha == AJ_SALVOS_SIMKL)
    ajustes_definir_salvos_destino(escolha);
  dados_gravar(ARQ_PERGUNTADO, "1\n");
  printf("[ilhasalvar] pergunta respondida: destino %d\n", escolha);
  ilhasalvar_executar(&pendente, pendenteEntrar);
}

int ilhasalvar_perguntar(const CatItem *ci, int entrar) {
  static IlhaModal m;
  IlhaAvisoEx e;
  int i;
  if (!ci || !ci->imdb[0] || !entrar || perguntaAberta) return perguntaAberta;
  if (jaFoiPerguntado()) return 0;
  // Sem pasta de dados nao ha como gravar a marca: perguntaria a cada "+".
  if (!dados_dir()[0]) return 0;
  nOpcao = 0;
  opcao[nOpcao++] = AJ_SALVOS_LOCAL;
  if (trakt_ativo()) opcao[nOpcao++] = AJ_SALVOS_TRAKT;
  if (simkl_ativo()) opcao[nOpcao++] = AJ_SALVOS_SIMKL;
  memset(&m, 0, sizeof m);
  snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Primeira vez na lista"));
  snprintf(m.titulo, sizeof m.titulo, "%s", i18n("Onde o + salva?"));
  // Com conta, a Lista do Nuvio tambem vai para o perfil (contapend.c).
  if (sessao_logada())
    snprintf(m.texto, sizeof m.texto, "%s", nOpcao > 1
             ? i18n("A Lista do Nuvio fica nesta TV e no seu perfil da conta. O Trakt e o Simkl guardam também nas contas deles. Dá para mudar em Ajustes.")
             : i18n("O título fica na Lista do Nuvio, nesta TV e no seu perfil da conta."));
  else
  snprintf(m.texto, sizeof m.texto, "%s", nOpcao > 1
           ? i18n("A Lista do Nuvio fica só nesta TV. O Trakt e o Simkl guardam na sua conta, e a lista desta TV também é gravada. Dá para mudar em Ajustes.")
           : i18n("O título fica na Lista do Nuvio, nesta TV. Conecte o Trakt ou o Simkl em Ajustes para guardar também na sua conta."));
  snprintf(m.icone, sizeof m.icone, "aj_bookmark");
  m.tipo = ILHA_ACENTO;
  m.nBotoes = nOpcao > 1 ? nOpcao : 1;
  for (i = 0; i < nOpcao; i++) {
    const char *r = opcao[i] == AJ_SALVOS_LOCAL ? (nOpcao > 1 ? i18n("Lista do Nuvio") : i18n("Salvar na Lista do Nuvio"))
                  : opcao[i] == AJ_SALVOS_TRAKT ? i18n("Trakt") : i18n("Simkl");
    snprintf(m.botao[i], sizeof m.botao[i], "%s", r);
  }
  snprintf(m.rodape, sizeof m.rodape, "%s", i18n("Voltar mantém o padrão."));
  memset(&e, 0, sizeof e);
  e.chave = CHAVE_ONDE; e.tipo = ILHA_ACENTO; e.icone = "aj_bookmark";
  e.texto = i18n("Onde o + salva?"); e.ms = 60000u; e.modal = &m; e.abrir = 1;
  ilha_avisar_ex(&e);
  pendente = *ci;
  pendenteEntrar = entrar;
  perguntaAberta = 1;
  perguntaDesde = SDL_GetTicks();
  printf("[ilhasalvar] primeira vez: pergunta na ilha (%d opcoes)\n", nOpcao);
  return 1;
}

void ilhasalvar_acao(const char *chave, int botao) {
  if (!chave || strcmp(chave, CHAVE_ONDE) || !perguntaAberta) return;
  // Com uma opcao so o botao confirma o padrao: nao troca o que esta em Ajustes
  // (o padrao e o Trakt, que sem conta cai na lista local e segue valendo
  // quando ela for ligada).
  concluir(nOpcao > 1 && botao >= 1 && botao <= nOpcao ? opcao[botao - 1] : -1);
}

void ilhasalvar_passo(Uint32 agora) {
  // VOLTAR: o modal recolheu sem entregar botao. Mantem o padrao e salva.
  if (perguntaAberta && agora - perguntaDesde > 600u && !ilha_tem(CHAVE_ONDE) &&
      !ilha_modal_aberto() && !ilha_modal_visivel())
    concluir(-1);
}
