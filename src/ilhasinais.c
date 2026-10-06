// Sinais da ilha do relogio — ver ilhasinais.h.
#include "ilhasinais.h"
#include "dormiu.h"
#include "diarionuvem.h"
#include "ilha.h"
#include "ilhasalvar.h"
#include "ilhacart.h"
#include "avisodisp.h"
#include "enquete.h"
#include "ilhaacao.h"
#include "addons.h"
#include "avisos.h"
#include "debrid.h"
#include "idioma.h"
#include "perfis.h"
#include "recomenda.h"
#include "redesaude.h"
#include "rede.h"
#include "sync.h"
#include "traktauth.h"
#include <stdio.h>
#include <string.h>

#define SONDA_MS 2000u
// "Sincronizando a conta…" so depois de 3 s rodando (mockup): o ciclo normal
// leva menos que isso e piscaria a pilula a cada abertura.
#define SYNC_ATIVIDADE_MS 3000u

static int pediuTrakt;
static int pedOpPend;
static char pedPubPend[16];
static int pediuAmigos;
static void responderPedido(void);

void ilhasinais_iniciar(void) { rede_avisar_saude(rede_saude_nota); rede_avisar_host(rede_hosts_nota); }

int ilhasinais_pediu_trakt(void) { int p = pediuTrakt; pediuTrakt = 0; return p; }

// --- os botoes dos modais --------------------------------------------------------
static void acoes(void) {
  char chave[96];
  int b = ilha_aviso_pediu(chave, sizeof chave);
  if (!b) return;
  if (!strncmp(chave, "av:", 3)) { avisos_ilha_acao(chave, b); return; }
  if (!strcmp(chave, ILHA_ACAO_CHAVE)) { ilhaacao_botao(b); return; }
  if (!strncmp(chave, "salvar:", 7)) { ilhasalvar_acao(chave, b); return; }
  if (!strncmp(chave, "enquete:", 8)) { enquete_acao(chave, b); return; }
  if (!strncmp(chave, "dormiu:", 7)) { dormiu_acao(chave, b); return; }
  if (!strcmp(chave, "diario-cofre")) { diarionuvem_acao(b); return; }
  if (!strncmp(chave, "pedido:", 7)) {
    // O fio social pode estar ocupado (uma releitura em voo): a resposta fica
    // guardada e sai na proxima volta, em vez de se perder em silencio.
    if (b == 1 || b == 2) {
      pedOpPend = b;
      snprintf(pedPubPend, sizeof pedPubPend, "%s", chave + 7);
      responderPedido();
    }
    return;
  }
  if (!strcmp(chave, "pedidos")) { pediuAmigos = 1; return; }
  if (!strncmp(chave, "vendo:", 6)) { ilhacart_vendo_acao(chave); return; }
  if (!strcmp(chave, "trakt") && b == 1) pediuTrakt = 1;
}

// --- social: pedido de amizade e amizade nova ----------------------------------
// PEDIDO DE AMIZADE NA ILHA (06/10, relato do dono): o aviso e PASSAGEIRO e
// nao e o lugar de responder. Ele diz o assunto uma vez — um pedido novo com
// o modal Aceitar/Recusar, varios juntos num "3 pedidos de amizade" que leva a
// aba Amigos — e some sozinho. Antes ele voltava a cada arranque (a lista de
// ja vistos era so da sessao) e, perdidos os 10 s, nao havia onde aceitar
// alem de uma pagina dentro de Encontrar pessoas. Agora o ja dito vai para o
// disco (avisodisp, chave "pedido:<pub>") e os pedidos ficam na aba Amigos
// (salvospainel.c) e na lista de Avisos ate a resposta.
#define PEDIDO_ILHA_MS 6000u
// pedOpPend: 1 aceitar, 2 recusar ainda nao entregue (fio social ocupado).

int ilhasinais_pediu_amigos(void) { int p = pediuAmigos; pediuAmigos = 0; return p; }

static void responderPedido(void) {
  int ok;
  if (!pedOpPend) return;
  ok = pedOpPend == 1 ? recomenda_aceitar(pedPubPend) : recomenda_recusar(pedPubPend);
  if (!ok) return;               // ocupado: tenta de novo na proxima volta
  printf("[amigos] pedido %s pela ilha\n", pedOpPend == 1 ? "aceito" : "recusado");
  if (pedOpPend == 2) ilha_avisar("pedido-resp", ILHA_INFO, "aj_x", i18n("Pedido recusado"), 3000u, 0);
  pedOpPend = 0;
}

static void pedidosDeAmizade(void) {
  int i, n = recomenda_n_pedidos(), novos = 0;
  RecPessoa p, primeiro;
  char chave[40];
  for (i = 0; i < n; i++) {
    if (!recomenda_pedido(i, &p) || !p.pub[0]) continue;
    snprintf(chave, sizeof chave, "pedido:%s", p.pub);
    if (avisodisp_tem(chave)) continue;
    avisodisp_por(chave);        // dito: nao volta nem depois de reiniciar
    if (!novos) primeiro = p;
    novos++;
  }
  if (!novos) return;
  if (novos == 1) {
    static IlhaModal m;
    IlhaAvisoEx e;
    char txt[200], f[120];
    int g, k = 0;
    const char *nome = primeiro.apelido[0] ? primeiro.apelido : "?";
    memset(&m, 0, sizeof m);
    memset(&e, 0, sizeof e);
    snprintf(chave, sizeof chave, "pedido:%s", primeiro.pub);
    snprintf(txt, sizeof txt, i18n("%s quer ser seu amigo"), ilha_forte(f, sizeof f, nome));
    snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Pedido de amizade"));
    snprintf(m.titulo, sizeof m.titulo, "%s", primeiro.apelido);
    snprintf(m.texto, sizeof m.texto, "%s", primeiro.bio);
    for (g = 0; g < REC_GENEROS_N && k < 3; g++)
      if (primeiro.generos & (1u << g)) snprintf(m.chips[k++], sizeof m.chips[0], "%s", i18n(rec_genero_rotulo(g)));
    snprintf(m.rosto, sizeof m.rosto, "%s", primeiro.avatar);
    snprintf(m.rostoNome, sizeof m.rostoNome, "%s", nome);
    snprintf(m.rodape, sizeof m.rodape, "%s", i18n("Recusar não avisa a pessoa. O pedido também fica em Amigos."));
    m.nBotoes = 2;
    snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Aceitar"));
    snprintf(m.botaoIcone[0], sizeof m.botaoIcone[0], "check");
    snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Recusar"));
    snprintf(m.botaoIcone[1], sizeof m.botaoIcone[1], "aj_x");
    e.chave = chave; e.tipo = ILHA_ACENTO; e.texto = txt; e.ms = PEDIDO_ILHA_MS;
    e.rosto = primeiro.avatar; e.rostoNome = nome;
    e.modal = &m;
    ilha_avisar_ex(&e);
  } else {
    // VARIOS DE UMA VEZ: um aviso so, com o rosto do mais novo; OK leva a
    // aba Amigos, onde cada um tem Aceitar e Recusar.
    IlhaAvisoEx e;
    char txt[120];
    memset(&e, 0, sizeof e);
    snprintf(txt, sizeof txt, i18n("%d pedidos de amizade"), novos);
    e.chave = "pedidos"; e.tipo = ILHA_ACENTO; e.texto = txt; e.ms = PEDIDO_ILHA_MS;
    e.rosto = primeiro.avatar; e.rostoNome = primeiro.apelido[0] ? primeiro.apelido : "?";
    e.acao = 1;
    ilha_avisar_ex(&e);
  }
  printf("[ilha] pedidos de amizade novos: %d\n", novos);
}

// AMIZADE NOVA: a lista de contatos ganhou alguem entre duas leituras. A
// PRIMEIRA leitura e a base (o que ja era amigo nao vira novidade); um salto de
// mais de 3 de uma vez e a lista chegando inteira do servidor (primeira
// abertura, troca de perfil) e tambem so vira base.
static void amizadesNovas(Uint32 agora) {
  static RecContato base[REC_CONTATOS_MAX];
  static int nBase = -1;
  static Uint32 desde;
  RecContato atual[REC_CONTATOS_MAX];
  int n, i, j, novos = 0;
  if (!desde) desde = agora ? agora : 1;
  if (agora - desde < 20000u) return;   // o cache e a primeira leitura assentam
  n = recomenda_contatos(atual, REC_CONTATOS_MAX);
  if (nBase >= 0) {
    for (i = 0; i < n; i++) {
      for (j = 0; j < nBase; j++) if (!strcmp(base[j].id, atual[i].id)) break;
      if (j == nBase) novos++;
    }
    if (novos > 0 && novos <= 3) {
      for (i = 0; i < n; i++) {
        for (j = 0; j < nBase; j++) if (!strcmp(base[j].id, atual[i].id)) break;
        if (j == nBase) {
          char chave[120], txt[200], f[120];
          IlhaAvisoEx e;
          memset(&e, 0, sizeof e);
          snprintf(chave, sizeof chave, "amigo:%s", atual[i].id);
          snprintf(txt, sizeof txt, i18n("Você e %s agora são amigos"),
                   ilha_forte(f, sizeof f, atual[i].nome[0] ? atual[i].nome : "?"));
          e.chave = chave; e.tipo = ILHA_OK; e.texto = txt; e.ms = 6000u;
          e.rosto = atual[i].avatar; e.rostoNome = atual[i].nome[0] ? atual[i].nome : "?";
          ilha_avisar_ex(&e);
        }
      }
    }
  }
  memcpy(base, atual, sizeof atual[0] * (size_t)n);
  nBase = n;
}

// --- contas e servicos -----------------------------------------------------------
static void trakt(void) {
  static int avisado;
  TraEstado st = traktauth_estado();
  if (st == TRA_INVALIDO && !avisado) {
    static IlhaModal m;
    IlhaAvisoEx e;
    avisado = 1;
    memset(&m, 0, sizeof m);
    memset(&e, 0, sizeof e);
    snprintf(m.kicker, sizeof m.kicker, "Trakt");
    snprintf(m.titulo, sizeof m.titulo, "%s", i18n("A sessão do Trakt expirou"));
    snprintf(m.texto, sizeof m.texto, "%s",
             i18n("Enquanto isso, o que você assistir não vai para o Trakt. Leva um minuto: um código na TV, o celular confirma."));
    snprintf(m.icone, sizeof m.icone, "aj_link-2-off");
    m.tipo = ILHA_ERRO; m.nBotoes = 2;
    snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Reconectar"));
    snprintf(m.botaoIcone[0], sizeof m.botaoIcone[0], "aj_rotate-cw");
    snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Depois"));
    e.chave = "trakt"; e.tipo = ILHA_ERRO; e.icone = "aj_link-2-off";
    e.texto = i18n("O Trakt desconectou"); e.ms = 9000u; e.modal = &m;
    ilha_avisar_ex(&e);
  } else if (st == TRA_LIGADO) avisado = 0;   // reconectou: uma proxima queda avisa de novo
}

static void debridSemPlano(void) {
  static int ditos;
  int mask = debrid_sem_plano(), novos = mask & ~ditos;
  if (!novos) return;
  ditos |= mask;
  { const char *nome = debrid_sem_plano_nome(novos), *frase = debrid_sem_plano_frase(novos);
    char txt[240], f[64];
    if (nome) snprintf(txt, sizeof txt, i18n("Seu %s está sem plano. As fontes dele ficam de fora."),
                       ilha_forte(f, sizeof f, nome));
    else snprintf(txt, sizeof txt, "%s", frase ? i18n(frase) : "");
    if (txt[0]) ilha_avisar("debrid-plano", ILHA_ERRO, "aj_triangle-alert", txt, 8000u, 0); }
}

void ilhasinais_debrid_baixando(const char *servico, const char *titulo) {
  char txt[240], f1[64], f2[200];
  if (!titulo || !titulo[0]) return;
  snprintf(txt, sizeof txt, i18n("O %s está baixando %s. Volte em alguns minutos."),
           servico && servico[0] ? servico : "debrid", ilha_forte(f2, sizeof f2, titulo));
  (void)f1;
  ilha_avisar("debrid-baixa", ILHA_INFO, "aj_download", txt, 9000u, 0);
}

static void sincronia(Uint32 agora) {
  static SyncEstado antes = SYNC_PARADO;
  static Uint32 rodandoDesde;
  static int falhaDita;
  SyncEstado st = sync_estado();
  if (st == SYNC_RODANDO) {
    if (antes != SYNC_RODANDO) rodandoDesde = agora ? agora : 1;
    if (rodandoDesde && agora - rodandoDesde >= SYNC_ATIVIDADE_MS)
      ilha_atividade(i18n("Sincronizando a conta…"), -1.0f);
  } else if (st == SYNC_FALHOU && antes != SYNC_FALHOU && !falhaDita) {
    // A CONTA FORA DO AR (#215) ja tem o aviso dela (app.c) e SEM INTERNET
    // tambem: a falha do sync nesses casos e consequencia, nao assunto novo.
    if (!sync_addons_fora() && !sync_servidor_fora() && !rede_saude_offline()) {
      ilha_avisar("sync", ILHA_ERRO, "aj_rotate-cw",
                  i18n("Não deu para sincronizar a conta. Tento de novo sozinho."), 6000u, 0);
      falhaDita = 1;
    }
  } else if (st == SYNC_PRONTO) falhaDita = 0;
  antes = st;
}

void ilhasinais_perfil_trocado(void) {
  const ContaPerfil *p = perfis_item_ativo();
  char txt[160], f[96];
  IlhaAvisoEx e;
  if (!p || !p->nome[0]) return;
  memset(&e, 0, sizeof e);
  snprintf(txt, sizeof txt, i18n("Agora no perfil %s"), ilha_forte(f, sizeof f, p->nome));
  e.chave = "perfil"; e.tipo = ILHA_INFO; e.texto = txt; e.ms = 3000u;
  e.rosto = p->avatarUrl; e.rostoNome = p->nome;
  ilha_avisar_ex(&e);
}

// --- rede e addons -----------------------------------------------------------------
static void rede(void) {
  static unsigned visto;
  unsigned s = rede_saude_seq();
  if (s == visto) return;
  visto = s;
  // A MESMA CHAVE nos dois: "de volta" troca o "sem internet" no lugar se ele
  // ainda estiver na pilula, em vez de entrar na fila atras dele.
  if (rede_saude_offline())
    ilha_avisar("rede", ILHA_ERRO, "aj_wifi-off",
                i18n("Sem internet. Mostrando o que já estava salvo."), 60u * 60u * 1000u, 0);
  else
    ilha_avisar("rede", ILHA_OK, "aj_wifi", i18n("Internet de volta"), 3000u, 0);
}

static void addonFora(void) {
  static unsigned visto;
  char nome[64], chave[96], txt[200], f[96];
  unsigned s = addons_fora_do_ar(nome, sizeof nome);
  if (s == visto) return;
  visto = s;
  // Sem internet nenhum addon responde: o aviso e o da rede, nao o do addon.
  if (!nome[0] || rede_saude_offline()) return;
  snprintf(chave, sizeof chave, "addon:%s", nome);
  snprintf(txt, sizeof txt, i18n("O addon %s não respondeu"), ilha_forte(f, sizeof f, nome));
  ilha_avisar(chave, ILHA_ERRO, "aj_puzzle", txt, 6000u, 0);
}

void ilhasinais_passo(Uint32 agora) {
  static Uint32 ultSonda;
  acoes();
  responderPedido();
  ilhasalvar_passo(agora);
  enquete_passo(agora);
  rede();
  addonFora();
  sincronia(agora);   // a atividade e renovada por quadro
  if (ultSonda && agora - ultSonda < SONDA_MS) return;
  ultSonda = agora ? agora : 1;
  if (recomenda_ativo()) { pedidosDeAmizade(); amizadesNovas(agora); }
  trakt();
  debridSemPlano();
}
