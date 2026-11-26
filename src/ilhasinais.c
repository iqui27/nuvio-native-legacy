// Sinais da ilha do relogio — ver ilhasinais.h.
#include "ilhasinais.h"
#if defined(__APPLE__) && (defined(__GNUC__) || defined(__clang__))
extern int ajustes_farol_avisos(void) __attribute__((weak_import));
#elif defined(__GNUC__) || defined(__clang__)
extern int ajustes_farol_avisos(void) __attribute__((weak));
#else
int ajustes_farol_avisos(void);
#endif
#include "dormiu.h"
#include "diarionuvem.h"
#include "ilha.h"
#include "ilhasalvar.h"
#include "listasescrita.h"
#include "extplayer.h"
#include "ilhacart.h"
#include "avisodisp.h"
#include "enquete.h"
#include "juntos.h"
#include "juntosui.h"
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
#include "dados.h"
#include "farol.h"
#include "farolanel.h"
#include "farolilha.h"
#include "farolregra.h"
#include "farolsonda.h"
#include "faroltexto.h"
#include "player.h"
#include "plrilha.h"
#include "resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SONDA_MS 2000u
// "Sincronizando a conta…" so depois de 3 s rodando (mockup): o ciclo normal
// leva menos que isso e piscaria a pilula a cada abertura.
#define SYNC_ATIVIDADE_MS 3000u

static int pediuTrakt;
static int pedOpPend;
static char pedPubPend[16];
static uint64_t pedSessao;
static int pediuAmigos;
static RecContato amizadesBase[REC_CONTATOS_MAX];
static int nAmizadesBase = -1;
static Uint32 amizadesDesde, ultSonda;
static int traktAvisado, debridDitos;
static SyncEstado syncAntes = SYNC_PARADO;
static Uint32 syncRodandoDesde;
static int syncFalhaDita;
static unsigned addonForaVisto;
static void responderPedido(void);

// --- o Farol na ilha (issue 20) ---------------------------------------------------
// A politica (quando falar) e farolilha.c, pura; aqui so a juncao: os fatos do
// Farol (farol_fatos_ler), a pilula e o disco das dispensas de 24 h.
#define FAROL_CHAVE      "farol"
#define FAROL_CONS_CHAVE "farol-conserto"
#define FAROL_ILHA_ARQ   "farol-ilha.txt"
// Um FE_FAROL_ACAO da mesma acao ate 2 min antes do conserto = foi a pessoa
// (um botao do Farol), e o conserto nao e "calado".
#define FAROL_ACAO_PESSOA_MS 120000u
static int farolPediu = -1;        // sintoma RES_S_* pedido pela AZUL; -1 = nada
static int farolSintomaVez = RES_S_NAO_COMECA;
static uint64_t farolConsSeq;      // eventos do anel com seq >= este sao novos
static int farolConsVisto;         // 0 = o anel ainda nao foi olhado
static char farolConsFrase[200];   // a pilula do conserto dentro do player
static Uint32 farolConsAte;
static void farolGravar(void) {
  char buf[FAROLILHA_DISPENSAS * 32 + 1];
  farolilha_exportar(buf, sizeof buf);
  dados_gravar(FAROL_ILHA_ARQ, buf);
}
static unsigned farolMs(void) { return SDL_GetTicks(); }

void ilhasinais_iniciar(void) {
  char *s;
  rede_avisar_saude(rede_saude_nota_desde); rede_avisar_host(rede_hosts_nota);
  farolilha_relogio(NULL, farolMs);
  if (ajustes_farol_avisos) farolilha_definir_ativo(ajustes_farol_avisos());
  s = dados_ler(FAROL_ILHA_ARQ);
  farolilha_importar(s);
  free(s);
}

int ilhasinais_pediu_farol(void) { int p = farolPediu; farolPediu = -1; return p; }

int ilhasinais_pediu_trakt(void) { int p = pediuTrakt; pediuTrakt = 0; return p; }

// --- os botoes dos modais --------------------------------------------------------
static void acoes(void) {
  char chave[96];
  int b = ilha_aviso_pediu(chave, sizeof chave);
  if (!b) return;
  if (!strncmp(chave, "av:", 3)) { avisos_ilha_acao(chave, b); return; }
  if (!strcmp(chave, ILHA_ACAO_CHAVE)) { ilhaacao_botao(b); return; }
  if (!strncmp(chave, "salvar:", 7)) { ilhasalvar_acao(chave, b); return; }
#ifdef NV_ANDROID
  if (!strncmp(chave, "extplayer:", 10)) { extplayer_acao(chave, b); return; }
#endif
  if (!strncmp(chave, "enquete:", 8)) { enquete_acao(chave, b); return; }
  if (!strncmp(chave, "juntos:", 7)) { juntos_acao(chave, b); return; }
  if (!strncmp(chave, "dormiu:", 7)) { dormiu_acao(chave, b); return; }
  if (!strcmp(chave, "diario-cofre")) { diarionuvem_acao(b); return; }
  if (!strncmp(chave, "pedido:", 7)) {
    // O fio social pode estar ocupado (uma releitura em voo): a resposta fica
    // guardada e sai na proxima volta, em vez de se perder em silencio.
    if (b == 1 || b == 2) {
      pedOpPend = b;
      pedSessao = perfis_sessao_geracao();
      snprintf(pedPubPend, sizeof pedPubPend, "%s", chave + 7);
      responderPedido();
    }
    return;
  }
  if (!strcmp(chave, "pedidos")) { pediuAmigos = 1; return; }
  if (!strncmp(chave, "vendo:", 6)) { ilhacart_vendo_acao(chave); return; }
  if (!strcmp(chave, FAROL_CHAVE)) { farolPediu = farolSintomaVez; return; }
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
  if (!pedOpPend || !perfis_sessao_atual(pedSessao)) return;
  ok = pedOpPend == 1 ? recomenda_aceitar(pedPubPend) : recomenda_recusar(pedPubPend);
  if (!ok) return;               // ocupado: tenta de novo na proxima volta
  printf("[amigos] pedido %s pela ilha\n", pedOpPend == 1 ? "aceito" : "recusado");
  if (pedOpPend == 2) ilha_avisar("pedido-resp", ILHA_INFO, "aj_x", i18n("Pedido recusado"), 3000u, 0);
  pedOpPend = 0;
}

// "DITO" E QUANDO APARECEU, NAO QUANDO FOI MANDADO (06/10, relato do dono:
// "nao ta mostrando na ilha"). A versao anterior gravava "pedido:<pub>" no
// disco no mesmo instante em que chamava ilha_avisar_ex. Se o aviso nao
// chegasse a aparecer — a fila cheia de erros o descartou, o painel de Salvos
// estava por cima e o prazo venceu por baixo dele — o pedido ficava marcado
// como dito para sempre e nunca mais passava pela ilha. Agora o anuncio fica
// PENDENTE (so na RAM) ate a ilha dizer que mostrou (ilha_mostrou); some da
// fila sem ter aparecido, ele e anunciado de novo na proxima sondagem.
#define PED_PEND_MAX REC_PEDIDOS_MAX
static char pendPub[PED_PEND_MAX][16];
static char pendChave[PED_PEND_MAX][40];
static int nPend;

static int pendIndice(const char *pub) {
  int i;
  for (i = 0; i < nPend; i++) if (!strcmp(pendPub[i], pub)) return i;
  return -1;
}
static void pendTirar(int i) {
  memmove(pendPub[i], pendPub[i + 1], sizeof pendPub[0] * (size_t)(nPend - i - 1));
  memmove(pendChave[i], pendChave[i + 1], sizeof pendChave[0] * (size_t)(nPend - i - 1));
  nPend--;
}
static void pendPor(const char *pub, const char *chave) {
  if (nPend == PED_PEND_MAX) pendTirar(0);
  snprintf(pendPub[nPend], sizeof pendPub[0], "%s", pub);
  snprintf(pendChave[nPend], sizeof pendChave[0], "%s", chave);
  nPend++;
}

// Os anuncios pendentes: os que apareceram viram "dito" no disco; os que a
// fila perdeu voltam a ser novos; os de um pedido que ja nao esta na caixa
// (respondido na aba Amigos, ou cancelado por quem pediu) saem da pilula.
static void pedidosPendentes(void) {
  int i, j, n = recomenda_n_pedidos();
  char vistas[PED_PEND_MAX][40];
  int nVistas = 0;
  RecPessoa p;
  // ilha_mostrou consome: pergunta UMA vez por chave (o agregado "pedidos"
  // cobre varias pessoas).
  for (i = 0; i < nPend; i++) {
    for (j = 0; j < nVistas; j++) if (!strcmp(vistas[j], pendChave[i])) break;
    if (j < nVistas) continue;
    if (ilha_mostrou(pendChave[i])) snprintf(vistas[nVistas++], sizeof vistas[0], "%s", pendChave[i]);
  }
  for (i = 0; i < nPend; ) {
    char chave[40];
    int naCaixa = 0, mostrou = 0;
    for (j = 0; j < n; j++)
      if (recomenda_pedido(j, &p) && !strcmp(p.pub, pendPub[i])) { naCaixa = 1; break; }
    for (j = 0; j < nVistas; j++) if (!strcmp(vistas[j], pendChave[i])) { mostrou = 1; break; }
    snprintf(chave, sizeof chave, "pedido:%s", pendPub[i]);
    if (mostrou) {
      avisodisp_por(chave);       // apareceu: nao volta nem depois de reiniciar
      pendTirar(i);
    } else if (!naCaixa) {
      if (!strcmp(pendChave[i], chave)) ilha_retirar(chave);
      pendTirar(i);
    } else if (!ilha_tem(pendChave[i])) {
      pendTirar(i);               // a fila perdeu: anuncia de novo abaixo
    } else i++;
  }
}

static void pedidosDeAmizade(void) {
  int i, n, novos = 0;
  RecPessoa p, primeiro;
  char chave[40];
  pedidosPendentes();
  n = recomenda_n_pedidos();
  for (i = 0; i < n; i++) {
    if (!recomenda_pedido(i, &p) || !p.pub[0]) continue;
    snprintf(chave, sizeof chave, "pedido:%s", p.pub);
    if (avisodisp_tem(chave) || pendIndice(p.pub) >= 0) continue;
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
    pendPor(primeiro.pub, chave);
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
    for (i = 0; i < n; i++) {
      if (!recomenda_pedido(i, &p) || !p.pub[0]) continue;
      snprintf(chave, sizeof chave, "pedido:%s", p.pub);
      if (avisodisp_tem(chave) || pendIndice(p.pub) >= 0) continue;
      pendPor(p.pub, "pedidos");
    }
  }
  printf("[ilha] pedidos de amizade novos: %d\n", novos);
}

// AMIZADE NOVA: a lista de contatos ganhou alguem entre duas leituras. A
// PRIMEIRA leitura e a base (o que ja era amigo nao vira novidade); um salto de
// mais de 3 de uma vez e a lista chegando inteira do servidor (primeira
// abertura, troca de perfil) e tambem so vira base.
static void amizadesNovas(Uint32 agora) {
  RecContato atual[REC_CONTATOS_MAX];
  int n, i, j, novos = 0;
  if (!amizadesDesde) amizadesDesde = agora ? agora : 1;
  if (agora - amizadesDesde < 20000u) return;   // o cache e a primeira leitura assentam
  n = recomenda_contatos(atual, REC_CONTATOS_MAX);
  if (nAmizadesBase >= 0) {
    for (i = 0; i < n; i++) {
      for (j = 0; j < nAmizadesBase; j++) if (!strcmp(amizadesBase[j].id, atual[i].id)) break;
      if (j == nAmizadesBase) novos++;
    }
    if (novos > 0 && novos <= 3) {
      for (i = 0; i < n; i++) {
        for (j = 0; j < nAmizadesBase; j++) if (!strcmp(amizadesBase[j].id, atual[i].id)) break;
        if (j == nAmizadesBase) {
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
  memcpy(amizadesBase, atual, sizeof atual[0] * (size_t)n);
  nAmizadesBase = n;
}

// --- contas e servicos -----------------------------------------------------------
static void trakt(void) {
  TraEstado st = traktauth_estado();
  if (st == TRA_INVALIDO && !traktAvisado) {
    static IlhaModal m;
    IlhaAvisoEx e;
    traktAvisado = 1;
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
  } else if (st == TRA_LIGADO) traktAvisado = 0;
}

static void debridSemPlano(void) {
  int mask = debrid_sem_plano(), novos = mask & ~debridDitos;
  if (!novos) return;
  debridDitos |= mask;
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

// O BOTAO "Sincronizar addons" (#360) da tela de Add-ons: o resultado chega
// aqui, e nao na tela, porque a pessoa pode sair dela antes de a volta acabar.
static void addonsAMao(void) {
  char txt[200];
  int add = 0, rem = 0, mud = 0, r = sync_manual_addons_estado(&add, &rem, &mud);
  if (r < 2) return;
  if (r == 3) {
    ilha_avisar("sync-addons", ILHA_ERRO, "aj_rotate-cw", i18n("Não foi possível sincronizar os addons."), 5000u, 0);
    return;
  }
  if (add || rem) snprintf(txt, sizeof txt, i18n("Addons sincronizados: %d adicionados, %d removidos"), add, rem);
  else snprintf(txt, sizeof txt, "%s", i18n(mud ? "Addons atualizados com a conta" : "Addons já estão em dia"));
  ilha_avisar("sync-addons", ILHA_OK, "aj_rotate-cw", txt, 4000u, 0);
}

static void sincronia(Uint32 agora) {
  SyncEstado st = sync_estado();
  if (st == SYNC_RODANDO) {
    if (syncAntes != SYNC_RODANDO) syncRodandoDesde = agora ? agora : 1;
    if (syncRodandoDesde && agora - syncRodandoDesde >= SYNC_ATIVIDADE_MS)
      ilha_atividade(i18n("Sincronizando a conta…"), -1.0f);
  } else if (st == SYNC_FALHOU && syncAntes != SYNC_FALHOU && !syncFalhaDita) {
    // A CONTA FORA DO AR (#215) ja tem o aviso dela (app.c) e SEM INTERNET
    // tambem: a falha do sync nesses casos e consequencia, nao assunto novo.
    if (!sync_addons_fora() && !sync_servidor_fora() && !rede_saude_offline()) {
      ilha_avisar("sync", ILHA_ERRO, "aj_rotate-cw",
                  i18n("Não deu para sincronizar a conta. Tento de novo sozinho."), 6000u, 0);
      syncFalhaDita = 1;
    }
  } else if (st == SYNC_PRONTO) syncFalhaDita = 0;
  syncAntes = st;
  addonsAMao();
}

void ilhasinais_perfil_trocado(void) {
  pediuTrakt = pediuAmigos = 0;
  // Aceitar/recusar ja enfileirados levam identidade imutavel no worker de
  // recomenda. Somente o pedido de UI ainda nao enfileirado sai daqui.
  pedOpPend = 0; pedPubPend[0] = 0; pedSessao = 0;
  memset(pendPub, 0, sizeof pendPub); memset(pendChave, 0, sizeof pendChave); nPend = 0;
  memset(amizadesBase, 0, sizeof amizadesBase); nAmizadesBase = -1;
  amizadesDesde = ultSonda = syncRodandoDesde = 0;
  syncAntes = SYNC_PARADO; syncFalhaDita = traktAvisado = debridDitos = 0;
  addonForaVisto = 0;
  farolilha_sessao_nova();        // o teto por titulo e da sessao do perfil
  farolPediu = -1; farolConsAte = 0; farolConsFrase[0] = 0;
  ilha_retirar(FAROL_CHAVE);
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
  char nome[64], chave[96], txt[200], f[96];
  unsigned s = addons_fora_do_ar(nome, sizeof nome);
  if (s == addonForaVisto) return;
  addonForaVisto = s;
  // Sem internet nenhum addon responde: o aviso e o da rede, nao o do addon.
  if (!nome[0] || rede_saude_offline()) return;
  snprintf(chave, sizeof chave, "addon:%s", nome);
  snprintf(txt, sizeof txt, i18n("O addon %s não respondeu"), ilha_forte(f, sizeof f, nome));
  ilha_avisar(chave, ILHA_ERRO, "aj_puzzle", txt, 6000u, 0);
}

// AVISO PROATIVO: ja fora do player, com 2 falhas seguidas ou 2 paradas em 5
// min, uma vez por titulo por sessao (farolilha.h). P1, 8 s, a tecla AZUL/CH+
// abre o Farol ja no veredito; Voltar dispensa o codigo por 24 h.
static void farolProativo(void) {
  static CatItem ult;
  FarolFatos ff;
  FarolVeredito v;
  FarolIlhaFatos f;
  IlhaAvisoEx a;
  int t = 0, e = 0;
  if (!farolilha_ativo() || player_aberto() || resolver_aberto() || ilha_tem(FAROL_CHAVE)) return;
  farol_fatos_ler(&ff);
  if (ff.falhasSeguidas < FAROLILHA_FALHAS_MIN && ff.paradas5min < FAROLILHA_PARADAS_MIN) return;
  memset(&f, 0, sizeof f);
  f.falhasSeguidas = ff.falhasSeguidas;
  f.paradas5min = ff.paradas5min;
  f.playerAberto = player_aberto();
  // video andando = retido (saida no meio) OU o mini-player (PiP) tocando —
  // como farolsonda, musicaui e o pre-carregar de fonte ja fazem em app.c.
  f.videoAndando = player_retido() || player_mini_ativo();
  f.titulo = player_ultimo_titulo(&ult, &t, &e) ? ult.imdb : "";
  ff.sintoma = farolilha_sintoma(&f);
  farolregra_julgar(ff.sintoma, &ff, &v);
  f.codigo = v.regra ? (int)v.regra->codigo : FAROL_C_NADA_ACHADO;
  if (!farolilha_proativo(&f)) return;
  farolSintomaVez = ff.sintoma;
  memset(&a, 0, sizeof a);
  a.chave = FAROL_CHAVE; a.tipo = ILHA_ACENTO; a.prior = ILHA_P1;
  a.icone = "aj_farol"; a.texto = i18n(FT_PILULA_FALHA); a.dica = i18n(FT_ABRE_O_FAROL);
  a.ms = FAROLILHA_AVISO_MS; a.tecla = 1; a.acao = 1;
  a.ponto = 1; a.dispensavel = 1;
  ilha_avisar_ex(&a);
  printf("[farol] ilha: aviso proativo (%s, falhas %d, paradas %d)\n",
         farolregra_codigo_nome(f.codigo), f.falhasSeguidas, f.paradas5min);
}

// CONSERTO CALADO: o app aplicou a troca sozinho (FE_FAROL_CONSERTOU sem um
// FE_FAROL_ACAO da pessoa logo antes). Pilula de 4 s, sem tecla: fora do player
// na ilha, dentro dele na pilula do player (plrilha), pedida a cada quadro.
static void farolConserto(Uint32 agora) {
  uint64_t seq = farolanel_seq();
  // O que ja estava no anel nao vira pilula; o anel zerado recomeca a conta.
  if (!farolConsVisto || seq < farolConsSeq) { farolConsVisto = 1; farolConsSeq = seq; }
  if (seq > farolConsSeq) {
    static FarolEvento ev[FAROL_ANEL_N];
    uint64_t seqFim = 0, seqIni;
    int n = farolanel_ler_seq(ev, FAROL_ANEL_N, 0, &seqFim), i, j;
    seqIni = seqFim - (uint64_t)n;
    for (i = 0; i < n; i++) {
      const char *frase;
      int sozinho = !resolver_aberto();
      if (seqIni + (uint64_t)i < farolConsSeq || ev[i].ev != FE_FAROL_CONSERTOU) continue;
      for (j = i - 1; j >= 0 && sozinho; j--)
        if (ev[j].ev == FE_FAROL_ACAO && ev[j].a == ev[i].a &&
            ev[i].t_ms - ev[j].t_ms <= FAROL_ACAO_PESSOA_MS) sozinho = 0;
      frase = farolilha_conserto(ev[i].a, sozinho);
      if (!frase) continue;
      if (player_aberto()) {
        snprintf(farolConsFrase, sizeof farolConsFrase, "%s", i18n(frase));
        // 0 e "desligado": no wrap de 32 bits (agora + 4 s = 0, ~49,7 dias de
        // uptime) a pilula do conserto apagaria na hora. Sobe para 1.
        farolConsAte = (agora ? agora : 1) + FAROLILHA_CONSERTO_MS;
        if (!farolConsAte) farolConsAte = 1;
      } else {
        ilha_avisar(FAROL_CONS_CHAVE, ILHA_OK, "aj_farol", i18n(frase), FAROLILHA_CONSERTO_MS, 0);
      }
    }
    farolConsSeq = seqFim;
  }
  if (!farolilha_ativo()) { farolConsAte = 0; farolConsFrase[0] = 0; }
  if (farolConsAte && (!player_aberto() || (Sint32)(agora - farolConsAte) >= 0)) farolConsAte = 0;
  if (farolConsAte) {
    PlrIlhaPedido p;
    memset(&p, 0, sizeof p);
    p.icone = "aj_farol"; p.corIcone = 2; p.texto = farolConsFrase;
    p.semFim = 1; p.aberta = 1; p.video = 1;
    plrilha_pedir(&p);
  }
}

// "Testando…" so depois de 300 ms (farolilha_atividade). Com o Farol aberto a
// propria tela mostra o teste; a ilha fala quando a pessoa sai dele no meio.
static void farolAtividade(void) {
  int ocupado = !farolsonda_estado(NULL);
  if (farolilha_atividade(ocupado) && !player_aberto() && !resolver_aberto())
    ilha_atividade_ex(i18n(FT_SONDANDO), -1.0f, "");
}

static void farolDispensa(void) {
  char chave[96];
  if (!ilha_aviso_dispensado(chave, sizeof chave) || strcmp(chave, FAROL_CHAVE)) return;
  farolilha_dispensar(farolilha_codigo_vez());
  farolGravar();
  printf("[farol] ilha: aviso dispensado (%s) por 24 h\n", farolregra_codigo_nome(farolilha_codigo_vez()));
}

void ilhasinais_passo(Uint32 agora) {
  if (ajustes_farol_avisos) farolilha_definir_ativo(ajustes_farol_avisos());
  if (!farolilha_ativo()) {
    if (ilha_tem(FAROL_CHAVE)) ilha_retirar(FAROL_CHAVE);
    if (ilha_tem(FAROL_CONS_CHAVE)) ilha_retirar(FAROL_CONS_CHAVE);
  }
  acoes();
  farolDispensa();
  farolConserto(agora);
  farolAtividade();
  responderPedido();
  ilhasalvar_passo(agora);
  lstesc_passo();
#ifdef NV_ANDROID
  extplayer_passo(agora);
#endif
  enquete_passo(agora);
  juntos_passo(agora);
  juntosui_atualizar(agora);
  rede();
  addonFora();
  sincronia(agora);   // a atividade e renovada por quadro
  if (ultSonda && agora - ultSonda < SONDA_MS) return;
  ultSonda = agora ? agora : 1;
  if (recomenda_ativo()) { pedidosDeAmizade(); amizadesNovas(agora); }
  trakt();
  debridSemPlano();
  farolProativo();
}
