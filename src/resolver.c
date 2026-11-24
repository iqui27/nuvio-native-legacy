// O FAROL (antes "Resolver um problema"). Ver resolver.h.
//
// AS FASES na mesma tela (PLANO 3.2, issue 19): Inicio = escolher o sintoma
// (os seis de sempre mais "Cores estranhas", a faixa da falha recente e a
// coluna "Achar o que ver" reservada para a #25); Cadeia = as seis etapas
// acendem na ordem em que o video passa e a luz do Farol desce a coluna no
// acento ate onde quebrou (com a sonda ativa no ar a linha respira em
// "testando…" ate 5 s); Veredito = o selo, as duas linhas da regra e a acao
// concreta, com o foco nascendo nela; Envio = o registro (codigo, o que vai e
// a lista dos enviados). O FOCO E GEOMETRICO como em comecar.c: cada fase monta
// uma lista de retangulos (montar) e CIMA/BAIXO/ESQUERDA/DIREITA vao para o
// mais proximo na direcao. Desenho, foco por teclado e alvos do ponteiro usam
// a mesma lista.
//
// O DIAGNOSTICO E DA FACHADA (farol.h): farol_julgar le os fatos, julga e
// guarda o veredito; aqui so a copia dele vira cartao. Reset da sessao
// (farol_sessao_reiniciar), leitura e julgamento rodam todos no fio principal
// (resolver_evento / resolver_atualizar), nunca de outro fio.
#include "resolver.h"
#include "resolverregra.h"
#include "farol.h"
#include "farolconsent.h"
#include "farolconsentui.h"
#include "farolenvioui.h"
#include "farolachaui.h"
#include "farolvozui.h"
#include "farolescolhaui.h"
#include "farolsonda.h"
#include "farolanel.h"
#include "farolregra.h"
#include "faroltexto.h"
#include "farolacao_addon.h"
#include "addons.h"
#include "ajustes.h"
#include "anim.h"
#include "avisos.h"
#include "botoes.h"
#include "dados.h"
#include "debrid.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "memlog.h"
#include "player.h"
#include "ponteiro.h"
#include "redesaude.h"
#include "segurar.h"
#include "ilha.h"
#include "streams.h"
#include "text.h"
#include "video.h"
#include "webosver.h"
#define NV_ESCALA_TELA_ATIVA
#include "escala.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define Q_W        1920.0f
#define Q_H        1080.0f
#define MARGEM      120.0f
#define ABRIR_MS    320.0f
#define FECHAR_MS   180.0f
#define MAX_ITENS    24
#define VISIVEIS      3     // linhas de "Registros enviados" na tela
#define PAINEL_X    960.0f
#define PAINEL_Y    290.0f
#define PAINEL_W    840.0f
#define PAINEL_PAD   44.0f
#define LUZ_MS      190.0f  // a luz leva isto por etapa da Cadeia
#define SONDA_MS   5000.0f  // "testando…" segura a Cadeia no maximo isto
#define RESPIRA_MS 1600.0f  // um ciclo da linha que respira
#define GRADE_W    1140.0f  // coluna dos sintomas no Inicio
#define ACHAR_X    1300.0f  // coluna "Achar o que ver" (#25)
#define ACHAR_W     500.0f
#define SINT_H      104.0f
#define SINT_GAP     18.0f

// O assistente P5 (#50). Os harnesses isolados do Farol (-DRESOLVER_TESTE com
// lista explicita de fontes) nao ligam farolescolhaui: la os simbolos sao
// fracos e o 5o item some, com o Inicio de sempre. No app ele sempre liga.
#ifdef RESOLVER_TESTE
#pragma weak farolescolhaui_reset
#pragma weak farolescolhaui_fechar
#pragma weak farolescolhaui_abrir
#pragma weak farolescolhaui_evento
#pragma weak farolescolhaui_atualizar
#pragma weak farolescolhaui_desenhar
#pragma weak farolescolhaui_pediu_abrir
#pragma weak farolescolhaui_pediu_fechar
#define ESCOLHA_LIGADA() (farolescolhaui_abrir != NULL)
#else
#define ESCOLHA_LIGADA() 1
#endif
// FALTA_TEXTO: "Me ajude a escolher" (entrada do P5) ainda sem chave em
// src/idioma_*.h; i18n() devolve o portugues ate a traducao entrar.
#define ROT_ESCOLHA "Me ajude a escolher"

// Ganchos fracos da camada de voz (#31) para manter os testes isolados do
// Resolver e da Tela 7 sem puxar farolvozui.c / farolbusca.c / sistexto.c.
__attribute__((weak)) int  farolvozui_disponivel(void) { return 0; }
__attribute__((weak)) const char *farolvozui_rotulo_botao(void) { return FT_ACAO_FALAR; }
__attribute__((weak)) const char *farolvozui_icone_botao(void) { return "aj_mic"; }
__attribute__((weak)) int  farolvozui_ativo(void) { return 0; }
__attribute__((weak)) int  farolvozui_foco(void) { return 0; }
__attribute__((weak)) int  farolvozui_volta_fase(void) { return RESOLVER_FASE_SINTOMA; }
__attribute__((weak)) void farolvozui_reset(void) {}
__attribute__((weak)) int  farolvozui_abrir(int voltaFase) { (void)voltaFase; return 0; }
__attribute__((weak)) void farolvozui_fechar(void) {}
__attribute__((weak)) void farolvozui_evento(const SDL_Event *e, int *pediuVoltar, int *abriuAchados) {
  (void)e; if (pediuVoltar) *pediuVoltar = 0; if (abriuAchados) *abriuAchados = 0;
}
__attribute__((weak)) void farolvozui_atualizar(float dt, Uint32 agora, int *abriuAchados, int *pediuVoltar) {
  (void)dt; (void)agora; if (abriuAchados) *abriuAchados = 0; if (pediuVoltar) *pediuVoltar = 0;
}
__attribute__((weak)) void farolvozui_desenhar(float ox, float oy, float alpha, Uint32 agora) {
  (void)ox; (void)oy; (void)alpha; (void)agora;
}

// Ids de foco. Os cartoes de sintoma usam o RES_S_* (RES_S_CORES inclusive);
// o resto vale por fase (resolver.h).
enum {
  B_BANNER = RESOLVER_B_BANNER, B_ENVIAR = RESOLVER_B_ENVIAR, B_LISTA = RESOLVER_B_LISTA,
  B_PRINC = RESOLVER_B_PRINC, B_SEC = RESOLVER_B_SEC, B_OUTRO = RESOLVER_B_OUTRO,
  B_PRONTO = RESOLVER_B_PRONTO, B_REPETIR = RESOLVER_B_REPETIR, B_ACHAR = RESOLVER_B_ACHAR,
  B_ACHAR_0 = RESOLVER_B_ACHAR_CURTO, B_ACHAR_1 = RESOLVER_B_ACHAR_NAO_VI,
  B_ACHAR_2 = RESOLVER_B_ACHAR_RETOMAR, B_ACHAR_3 = RESOLVER_B_ACHAR_SURPRESA,
  B_ACHAR_VOLTAR = RESOLVER_B_ACHAR_VOLTAR,
  B_ACHAR_VOZ = RESOLVER_B_ACHAR_VOZ,
  B_ACHAR_ESCOLHA = RESOLVER_B_ACHAR_ESCOLHA,
  B_LIN0 = RESOLVER_B_LIN0
};
// O que um botao do painel faz. A_DEBRID = login da conta de cache (a tela/QR
// do addonguia); A_PLAYER_EXT = a fonte falha sai para o player externo.
// A_FAROL = a acao FAROL_A_* sai como pedido para o app executar com
// farolacao_rede_executar (issue #34 — as 4 acoes da Onda 2).
enum { A_NADA = 0, A_REPETIR, A_FONTES, A_ENVIAR, A_DEBRID, A_PLAYER_EXT, A_ISOLAR, A_FAROL };
// O que a fase 2 mostra.
enum { M_NADA = 0, M_ENVIANDO, M_ENVIADO, M_FALHOU, M_LISTA };

typedef struct { int id; GfxRect r; } Item;

static int   aberto, fase, foco, sint, pedido, voltaFase, acharVoltaFoco;
// PEDIU_FAROL: acao + modo do pedido de execucao (o persistir so vale em
// FAROL_A_DESLIGAR_HDR). Consumidos junto com `pedido` em resolver_pedido().
static int   pedidoAcao, pedidoPersistir;
// OK afundado na celula do "Desligar o HDR" (a unica com versao longa):
// toque executa a acao desta reproducao; segurar executa a "sempre nesta TV".
static Uint32 okDesde;
static int   okArmado, okLongo;
static float entrada;
static ResolverFatos fatos;
static ResolverResultado res;
static ResolverFalha falha;
static int   temFalha;
static uint64_t diagTentativa, pedidoTentativa;
static int   diagUsaTentativa;
// O motor de playbooks: `ff` sao os fatos com que a fachada julgou e `vered`
// a copia do veredito da sessao (farol_veredito) na ultima diagnosticar().
static FarolFatos ff;
static FarolVeredito vered;
// A Cadeia: `luz` anda em etapas ate `luzAlvo` (a que quebrou + 1, ou o fim);
// `segura` = a sonda ativa no ar prende a luz ("testando…") ha `seguraMs`.
// `sondaCancelada` = uma sonda cancelada ainda esta encerrando: nao herda.
static float luz, luzAlvo, seguraMs, respiraMs;
static int   segura, sondaCancelada;
// ADENDO C: a acao despachada fica pendente ate o proximo julgamento do mesmo
// sintoma, que diz se ela consertou (farol_registrar_resultado). `falhouCod` =
// o veredito cuja acao ja falhou: com escala SE_FALHAR ele ganha "Mandar
// registro". `envDoFarol` = o envio em curso saiu do Veredito.
static int   pendAcao, pendCod = -1, pendSint, falhouCod = -1, envDoFarol;
// Painel do Veredito (montado por preparar()).
static char  pTitulo[200], pCorpo[640], pMarca[64];
static int   pAcheou, pPrim, pSec, pPrimF, pSecF, pTvAviso, pFarol;
static float pHeadH, pBodyH;
// Registros enviados e o envio em curso.
static ResolverEnviado env[RES_ENVIADOS_MAX];
static int   envN, envModo, envSint, envMotivo, envHttp, rolo;
static long  envInicio, envQuando;
static char  envCodigo[8];
static int   envNoAr;   // um envio foi pedido e ainda nao chegou: segue valendo com a tela fechada ou em outra fase
#ifdef RESOLVER_TESTE
static ResolverFatos fatosFixos;
static FarolFatos    ffFixos;
static int   usaFixos, usaFFixos;
void resolver_teste_fatos(const ResolverFatos *f) { usaFixos = f != NULL; if (f) fatosFixos = *f; }
void resolver_teste_fatos_farol(const FarolFatos *f) {
  usaFFixos = f != NULL;
  if (f) ffFixos = *f;
  farol_teste_fatos(f);
}
int          resolver_teste_acao_pendente(void) { return pendAcao; }
int          resolver_teste_veredito(void) { return vered.regra ? vered.regra->codigo : -1; }
const int   *resolver_teste_estados(void) { return vered.estado; }
const char  *resolver_teste_titulo(void) { return pTitulo; }
const char  *resolver_teste_corpo(void) { return pCorpo; }
int          resolver_teste_prim(void) { return pPrimF; }
int          resolver_teste_sec(void) { return pSecF; }
#endif

int resolver_aberto(void) { return aberto; }
int resolver_fase(void) { return fase; }
int resolver_foco(void) {
  if (farolconsentui_aberto()) return farolconsentui_foco();
  if (farolvozui_ativo()) return farolvozui_foco();
  if (fase == RESOLVER_FASE_ACHAR) {
    int af = farolachaui_foco();
    if (af == FAROL_ACHA_FOCO_VOZ) return B_ACHAR_VOZ;
    return af < 0 ? B_ACHAR_VOLTAR : (B_ACHAR_0 + af);
  }
  if (fase == RESOLVER_FASE_ENVIO && farolenvioui_ativo())
    return farolenvioui_foco();
  if (fase == RESOLVER_FASE_ESCOLHA) return B_ACHAR_ESCOLHA;
  return foco;
}
int resolver_sintoma(void) { return sint; }
int resolver_enviados_n(void) { return envN; }
float resolver_luz(void) { return luz; }
int resolver_testando(void) { return fase == RESOLVER_FASE_CADEIA && segura; }
int resolver_mandar_registro(void) { return fase == RESOLVER_FASE_VEREDITO && pSec == A_ENVIAR && pFarol; }

__attribute__((weak)) int  farolenvioui_presente(void) { return 0; }
__attribute__((weak)) int  farolenvioui_ativo(void) { return 0; }
__attribute__((weak)) void farolenvioui_abrir(int s, int v, int df, const FarolVeredito *vd,
                                              const FarolFatos *f, const int *est, const char *uc) {
  (void)s; (void)v; (void)df; (void)vd; (void)f; (void)est; (void)uc;
}
__attribute__((weak)) void farolenvioui_fechar(void) {}
__attribute__((weak)) void farolenvioui_esquecer(void) {}
__attribute__((weak)) void farolenvioui_evento(const SDL_Event *e) { (void)e; }
__attribute__((weak)) int  farolenvioui_passo(float dt, int *op, int *of, int *ov, int *or_, int *odf) {
  (void)dt;
  if (op) *op = RESOLVER_PEDIU_NADA;
  if (of) *of = 0;
  if (ov) *ov = 0;
  if (or_) *or_ = 0;
  if (odf) *odf = -1;
  return 0;
}
__attribute__((weak)) void farolenvioui_desenhar(float ox_, float oy_, float a) {
  (void)ox_; (void)oy_; (void)a;
}
__attribute__((weak)) int  farolenvioui_foco(void) { return 0; }
__attribute__((weak)) int  farolconsent_deve_perguntar(void) { return 0; }
__attribute__((weak)) int  farolconsentui_aberto(void) { return 0; }
__attribute__((weak)) void farolconsentui_abrir(void) {}
__attribute__((weak)) void farolconsentui_fechar(void) {}
__attribute__((weak)) void farolconsentui_esquecer(void) {}
__attribute__((weak)) void farolconsentui_evento(const SDL_Event *e) { (void)e; }
__attribute__((weak)) void farolconsentui_atualizar(float dt) { (void)dt; }
__attribute__((weak)) void farolconsentui_desenhar(float ox_, float oy_, float a) {
  (void)ox_; (void)oy_; (void)a;
}
__attribute__((weak)) int  farolconsentui_foco(void) { return 0; }
__attribute__((weak)) void ajustes_abrir_no_trakt(void) {}

void resolver_teste_esquecer(void) {
  farolconsentui_esquecer();
  farolenvioui_esquecer();
  aberto = 0; entrada = 0.0f; fase = 0; foco = 0; envN = 0; envModo = 0; envNoAr = 0; rolo = 0; temFalha = 0;
  acharVoltaFoco = 0; farolachaui_reset(); farolvozui_reset();
  if (ESCOLHA_LIGADA()) farolescolhaui_reset();
  diagTentativa = 0; pedidoTentativa = 0; diagUsaTentativa = 0;
  luz = luzAlvo = seguraMs = respiraMs = 0.0f; segura = 0; sondaCancelada = 0;
  pendAcao = 0; pendCod = -1; pendSint = 0; falhouCod = -1; envDoFarol = 0; pFarol = 0;
  memset(&fatos, 0, sizeof fatos);
  memset(&vered, 0, sizeof vered);
  memset(&ff, 0, sizeof ff);
#ifdef RESOLVER_TESTE
  usaFixos = 0; usaFFixos = 0;
  farol_teste_fatos(NULL);
#endif
  // Fronteira nova depois do anel zerado pelo harness: o que vier e desta sessao.
  farol_sessao_reiniciar();
}

int resolver_pedido(void) {
  int p = pedido;
  pedido = RESOLVER_PEDIU_NADA;
  if (p != RESOLVER_PEDIU_FAROL) { pedidoAcao = 0; pedidoPersistir = 0; }
  return p;
}
// Validos so no turno em que resolver_pedido() devolve RESOLVER_PEDIU_FAROL.
int resolver_pedido_acao(void) { return pedidoAcao; }
int resolver_pedido_persistir(void) { return pedidoPersistir; }

int resolver_pediu_abrir(FarolAchaPedido *pedido) {
  if (farolachaui_pediu_abrir(pedido)) return 1;
  return ESCOLHA_LIGADA() && farolescolhaui_pediu_abrir(pedido);
}

int resolver_resolver_pedido(const FarolAchaPedido *pedido, unsigned *revisaoOut) {
  return farolachaui_resolver_pedido(pedido, revisaoOut);
}

int resolver_pedido_bate(const CatItem *c, const FarolAchaPedido *pedido) {
  return farolachaui_pedido_bate(c, pedido);
}

static void fechar(void);

void resolver_fechar(void) {
  fechar();
}

void resolver_recarregar_achar(void) {
  farolachaui_recarregar();
}

uint64_t resolver_tentativa_externo(void) {
  return pedidoTentativa;
}

// ------------------------------------------------------------------ enviados
static void enviados_ler(void) {
  char *s = dados_ler(RESOLVER_ARQ);
  envN = resolverregra_enviados_ler(s, (long)time(NULL), env, RES_ENVIADOS_MAX);
  free(s);
}

static void enviados_gravar(void) {
  char buf[RES_ENVIADOS_MAX * 40];
  // Lista vazia grava o arquivo vazio: sem isso o ultimo registro apagado volta.
  if (resolverregra_enviados_serializar(env, envN, buf, sizeof buf) || envN == 0) dados_gravar(RESOLVER_ARQ, buf);
}

// ------------------------------------------------------------------- fatos
static void fatos_ler(void) {
  char buf[96];
  int m;
  const char *fr;
#ifdef RESOLVER_TESTE
  if (usaFixos) {
    fatos = fatosFixos;
    temFalha = resolverregra_falha_recente(&falha, (long)time(NULL));
    fatos.haFalha = temFalha;
    return;
  }
#endif
  memset(&fatos, 0, sizeof fatos);
  fatos.offline = rede_saude_offline();
  fatos.addonsQtd = addons_n();
  if (fatos.addonsQtd > 0 && !addons_ocupado() &&
      !addons_motivo_vazio(fatos.addonsMotivo, sizeof fatos.addonsMotivo))
    fatos.addonsMotivo[0] = 0;
  m = debrid_sem_plano();
  fr = m ? debrid_sem_plano_frase(m) : NULL;
  if (fr) snprintf(fatos.debridMotivo, sizeof fatos.debridMotivo, "%s", i18n(fr));
  else if (debrid_recusa(buf, sizeof buf))
    snprintf(fatos.debridMotivo, sizeof fatos.debridMotivo, i18n("Conta de debrid recusada: %s"), buf);
  if (video_registro_negado())
    snprintf(fatos.tvMotivo, sizeof fatos.tvMotivo, "%s", i18n("A TV recusou abrir o player: o sistema negou a permissão de vídeo."));
  temFalha = resolverregra_falha_recente(&falha, (long)time(NULL));
  fatos.haFalha = temFalha;
}

// ------------------------------------------------------------- fatos farol
// Os fatos do motor saem da fachada (farol_fatos_ler, dentro de farol_julgar):
// o erro nativo normalizado, a idade dele, as contagens do anel e a fronteira
// da sessao. Aqui so o que o despacho do player externo precisa conferir de
// novo no OK: a tentativa mudou ou ja tocou depois do erro.
static int anel_pronto_apos_erro(const FarolEvento *ev, int n) {
  int i, iPronto = -1, iErro = -1;
  for (i = 0; i < n; i++) {
    if (ev[i].ev == FE_PRONTO) iPronto = i;
    else if (ev[i].ev == FE_ERRO_FONTE || ev[i].ev == FE_RECUSA_SINC) iErro = i;
  }
  return iPronto >= 0 && (iErro < 0 || iPronto > iErro);
}

// ------------------------------------------------------------------- textos
typedef struct { const char *tit, *desc, *icone; } SintInfo;
static const SintInfo SINT[RES_S_N] = {
  { "O vídeo não começa", "Tela preta, erro ou volta para a lista", "aj_play" },
  { "Trava ou fica carregando", "Para no meio, engasga, demora a abrir", "aj_rotate-cw" },
  { "Sem som ou som errado", "Mudo, idioma trocado, fora de sincronia", "aj_volume-2" },
  { "Legenda", "Não aparece, atrasada ou com letras quebradas", "aj_captions" },
  { "Continuar assistindo errado", "Episódio trocado, progresso que volta", "aj_clock" },
  { "Contas e sincronização", "Trakt, Simkl ou Conta Nuvio não entram", "aj_users" },
};
// "Cores estranhas" (ADENDO C): sintoma do Farol, fora da tabela antiga.
static const SintInfo SINT_CORES = { FT_SINTOMA_CORES, FT_SINTOMA_CORES_DESC, "aj_palette" };

static const SintInfo *sint_info(int s) {
  if (s >= 0 && s < RES_S_N) return &SINT[s];
  if (s == RES_S_CORES) return &SINT_CORES;
  return NULL;
}
static const struct { const char *nome, *icone; } TESTE[RES_T_N] = {
  { "Internet", "aj_wifi" }, { "Add-ons", "aj_puzzle" }, { "Debrid", "aj_key" },
  { "Fonte", "aj_link" }, { "Esta TV", "aj_tv-minimal-play" }, { "Áudio", "aj_volume-2" },
};

static const char *sint_titulo(int s) {
  const SintInfo *si = sint_info(s);
  return si ? i18n(si->tit) : i18n("Registro sem problema escolhido");
}

// "agora", "há 4 min", "há 3 h", "há 2 dias" (o rotulo curto das linhas).
static void texto_ha(long seg, char *dst, size_t tam) {
  int n = 0;
  switch (resolverregra_ha(seg, &n)) {
    case RES_HA_AGORA: snprintf(dst, tam, "%s", i18n("agora")); break;
    case RES_HA_MIN:   snprintf(dst, tam, i18n("há %d min"), n); break;
    case RES_HA_H:     snprintf(dst, tam, i18n("há %d h"), n); break;
    default:
      if (n == 1) snprintf(dst, tam, "%s", i18n("há 1 dia"));
      else snprintf(dst, tam, i18n("há %d dias"), n);
      break;
  }
}

static const char *detalhe_teste(int t, char *buf, size_t tam) {
  int st = res.estado[t];
  if (!res.testado[t]) return i18n("Este problema não passa por aqui.");
  if (st == RES_NAO_TESTADO) return i18n(FT_NAO_TESTADO);
  if (t == RES_T_AUDIO && st == RES_OK) return i18n("Ainda não dá para ver o som da TV por aqui.");
  switch (t) {
    case RES_T_INTERNET: return st == RES_OK ? i18n("A TV está conectada.") : i18n("A TV está sem internet.");
    case RES_T_ADDONS:
      if (st == RES_OK) { snprintf(buf, tam, i18n("%d add-ons instalados, sem erro na última busca."), fatos.addonsQtd); return buf; }
      if (fatos.addonsQtd <= 0) return i18n("Nenhum add-on instalado.");
      return fatos.addonsMotivo[0] ? fatos.addonsMotivo : pTitulo;
    case RES_T_DEBRID:
      if (st == RES_OK) return debrid_ativo() ? i18n("Conta de debrid sem recusa nesta sessão.") : i18n("Nenhuma conta de debrid ligada.");
      return fatos.debridMotivo[0] ? fatos.debridMotivo : pTitulo;
    case RES_T_FONTE:
      if (st == RES_OK) return i18n("Sem erro de fonte recente.");
      return falha.titulo[0] ? falha.titulo : i18n("A última fonte falhou.");
    case RES_T_TV:
      if (st == RES_OK) return i18n("A TV não recusou abrir o player.");
      return fatos.tvMotivo[0] ? fatos.tvMotivo : pTitulo;
    case RES_T_AUDIO: return pTitulo;
  }
  return "";
}

static void juntar(char *dst, size_t tam, const char *a, const char *b) {
  snprintf(dst, tam, "%s%s%s", a, a[0] && b[0] ? "\n" : "", b);
}

// -------------------------------------------------------- acoes do veredito
// FAROL_A_* -> acao local do cartao. A_NADA = ainda sem executor (a issue que
// fecha cada uma esta em lotes/revisao/correcoes/CORRECAO-FAROL6B.md): o
// cartao cai na alternativa e, sem ela, no envio do registro. Nada de botao
// que nao faz o que promete.
static int acao_farol_map(int a) {
  switch (a) {
    case FAROL_A_TENTAR_DE_NOVO:    return A_REPETIR;
    case FAROL_A_ABRIR_FONTES:      return A_FONTES;
    case FAROL_A_ABRIR_DEBRID:
    case FAROL_A_RECONECTAR_DEBRID: return A_DEBRID;
    case FAROL_A_RECONECTAR_REDE:
    case FAROL_A_LIMPAR_CACHE:
    case FAROL_A_BUFFER_MAIOR:
    case FAROL_A_DESLIGAR_HDR:      return A_FAROL;   // issue #34: executa
    case FAROL_A_PLAYER_EXTERNO:    return ff.externoDisponivel > 0 ? A_PLAYER_EXT : A_NADA;
    case FAROL_A_ENVIAR_REGISTRO:   return A_ENVIAR;
    case FAROL_A_ISOLAR_ADDON:      return farolacao_addon_pode_isolar() ? A_ISOLAR : A_NADA;
    default:                        return A_NADA;
  }
}

// Fixtures que compilam o Resolver sem farolacao_addon.c (farol_android_erro):
// sem isolamento o cartao cai na alternativa. O app e tests/farol_resolver.sh
// linkam a implementacao de verdade, que prevalece sobre estas.
__attribute__((weak)) int farolacao_addon_pode_isolar(void) { return 0; }
__attribute__((weak)) int farolacao_addon_isolar_lento(void) { return FAAD_NENHUM; }

// O botao principal: a acao da regra quando ela executa; senao a alternativa;
// senao o registro. Devolve o FAROL_A_* EFETIVO e grava o A_* local em `*a`.
static int acao_efetiva(const FarolRegra *r, int *a) {
  int m = acao_farol_map(r->acao);
  if (m != A_NADA) { *a = m; return r->acao; }
  m = acao_farol_map(r->acaoAlt);
  if (m != A_NADA) { *a = m; return r->acaoAlt; }
  *a = A_ENVIAR;
  return FAROL_A_ENVIAR_REGISTRO;
}

// "Mandar registro" (ADENDO C, gancho da #22) so quando a regra deixa: escala
// JUNTO sempre; SE_FALHAR depois que a acao deste mesmo veredito ja falhou
// (farol_registrar_resultado com ok = 0 nesta sessao da tela).
static int pode_mandar(const FarolRegra *r) {
  return r->escala >= FAROL_ESC_JUNTO ||
         (r->escala == FAROL_ESC_SE_FALHAR && falhouCod == (int)r->codigo);
}

// O botao secundario: "Mandar registro" quando a escala permite; senao a
// alternativa com executor; senao "Tentar de novo" como nos outros cartoes.
// FAROL_A_* efetivo na devolucao, A_* em `*a`.
static int acao_secundaria(const FarolRegra *r, int primF, int *a) {
  int m;
  if (primF != FAROL_A_ENVIAR_REGISTRO && pode_mandar(r)) {
    *a = A_ENVIAR; return FAROL_A_ENVIAR_REGISTRO;
  }
  if (r->acaoAlt != primF && (m = acao_farol_map(r->acaoAlt)) != A_NADA && m != A_ENVIAR) {
    *a = m; return r->acaoAlt;
  }
  if (primF == FAROL_A_TENTAR_DE_NOVO) { *a = A_NADA; return 0; }
  *a = A_REPETIR; return FAROL_A_TENTAR_DE_NOVO;
}

// A segunda linha promete a acao EFETIVA — quando o cartao caiu para a
// alternativa ou para o registro, a frase original (que prometia a acao da
// regra) nao pode continuar.
static const char *corpo_farol(const FarolRegra *r, int acaoF) {
  if (acaoF == r->acao) return r->l2;
  if (acaoF == FAROL_A_PLAYER_EXTERNO) return FT_L2_PLAYER_EXTERNO;
  return FT_NADA_ACHADO_L2;
}

// O cartao do que o motor decidiu: as duas linhas da regra e os botoes das
// acoes efetivas.
static void farol_painel(const FarolRegra *r) {
  pAcheou = 1;
  pFarol = 1;
  snprintf(pMarca, sizeof pMarca, "%s", i18n(FT_A_CAUSA_ENCONTRADA));
  snprintf(pTitulo, sizeof pTitulo, "%s", i18n(r->l1));
  pPrimF = acao_efetiva(r, &pPrim);
  pSecF = acao_secundaria(r, pPrimF, &pSec);
  snprintf(pCorpo, sizeof pCorpo, "%s", i18n(corpo_farol(r, pPrimF)));
}

// Monta o painel da direita da fase 1 a partir do resultado.
static void preparar(void) {
  int p = res.primeira;
  unsigned m = resolverregra_mascara(sint);
  pTitulo[0] = pCorpo[0] = pMarca[0] = 0;
  pAcheou = 0; pSec = A_NADA; pPrimF = pSecF = 0; pTvAviso = 0; pFarol = 0;
  if (!m) {                                                    // Legenda / Continuar errado
    snprintf(pMarca, sizeof pMarca, "%s", i18n("Passo a passo"));
    pPrim = A_ENVIAR;
    if (sint == RES_S_LEGENDA) {
      snprintf(pTitulo, sizeof pTitulo, "%s", i18n("Três coisas para tentar na legenda."));
      snprintf(pCorpo, sizeof pCorpo, "%s\n%s\n%s\n%s",
               i18n("1. No player, abra o menu de legendas e escolha outra."),
               i18n("2. Se estiver atrasada, ajuste o atraso no mesmo menu."),
               i18n("3. Se as letras quebram, troque o tamanho ou a fonte da legenda."),
               i18n("Continua igual? Envie um registro."));
    } else {
      snprintf(pTitulo, sizeof pTitulo, "%s", i18n("Três coisas para tentar no Continuar."));
      snprintf(pCorpo, sizeof pCorpo, "%s\n%s\n%s\n%s",
               i18n("1. Veja de onde vem o progresso: Trakt, Simkl ou só esta TV."),
               i18n("2. Marque o episódio como visto à mão, na página do título."),
               i18n("3. Se voltar a errar, anote o título e o episódio."),
               i18n("Continua igual? Envie um registro."));
    }
    return;
  }
  // O Farol decide primeiro. NADA_ACHADO devolve o painel da cadeia antiga —
  // ele e a reserva onde o motor ainda nao tem playbook.
  if (vered.regra && vered.regra->codigo != FAROL_C_NADA_ACHADO) {
    farol_painel(vered.regra);
    return;
  }
  if (p < 0) {
    snprintf(pMarca, sizeof pMarca, "%s", i18n("Nada encontrado"));
    pPrim = A_ENVIAR; pSec = sint == RES_S_CONTAS ? A_NADA : A_REPETIR;
    if (sint == RES_S_CONTAS) {
      snprintf(pTitulo, sizeof pTitulo, "%s", i18n("A internet está bem."));
      snprintf(pCorpo, sizeof pCorpo, "%s", i18n("Se o Trakt, o Simkl ou a Conta Nuvio não entram, saia e entre de novo em Ajustes › Contas. Se não resolver, envie um registro."));
    } else {
      snprintf(pTitulo, sizeof pTitulo, "%s", i18n("Os testes não acharam erro."));
      snprintf(pCorpo, sizeof pCorpo, "%s", (m >> RES_T_AUDIO) & 1u
               ? i18n("O app ainda não consegue ver o som da TV. O resto está sem erro agora. Se continua, envie um registro: ele leva o que o app viu.")
               : i18n("Tudo o que o app consegue ver está sem erro agora. Se continua, envie um registro: ele leva o que o app viu."));
    }
    return;
  }
  pAcheou = 1;
  snprintf(pMarca, sizeof pMarca, "%s", i18n("Achamos o motivo"));
  switch (p) {
    case RES_T_INTERNET:
      snprintf(pTitulo, sizeof pTitulo, "%s", i18n("A TV está sem internet."));
      snprintf(pCorpo, sizeof pCorpo, "%s", i18n("Sem rede nada abre: nem add-ons, nem fontes. Confira o Wi-Fi ou o cabo e tente de novo."));
      pPrim = A_REPETIR;
      break;
    case RES_T_ADDONS:
      snprintf(pTitulo, sizeof pTitulo, "%s", fatos.addonsQtd <= 0 ? i18n("Nenhum add-on instalado.") : i18n("Os add-ons não trouxeram fontes."));
      snprintf(pCorpo, sizeof pCorpo, "%s", fatos.addonsQtd <= 0
               ? i18n("Sem add-on de fontes o Nuvio não tem o que tocar. Instale um em Ajustes › Fontes e addons.")
               : fatos.addonsMotivo);
      pPrim = A_FONTES; pSec = A_REPETIR;
      break;
    case RES_T_DEBRID:
      snprintf(pTitulo, sizeof pTitulo, "%s", i18n("O debrid está fora."));
      juntar(pCorpo, sizeof pCorpo, fatos.debridMotivo, i18n("Confira a conta ou a chave em Ajustes › Fontes e addons."));
      pPrim = A_FONTES; pSec = A_REPETIR;
      break;
    case RES_T_FONTE:
      snprintf(pTitulo, sizeof pTitulo, "%s", falha.titulo[0] ? falha.titulo : i18n("A última fonte falhou."));
      snprintf(pCorpo, sizeof pCorpo, "%s", falha.dica[0] ? falha.dica
               : i18n("Escolha outra fonte na folha de Fontes ou envie um registro."));
      pPrim = A_ENVIAR; pSec = A_REPETIR;
      break;
    default:   // TV (e o audio, quando ganhar dado)
      snprintf(pTitulo, sizeof pTitulo, "%s", p == RES_T_TV ? fatos.tvMotivo : fatos.audioMotivo);
      snprintf(pCorpo, sizeof pCorpo, "%s", i18n("Nenhuma fonte abre enquanto isto durar. Envie um registro para quem ajuda ver o que o sistema negou."));
      pPrim = A_ENVIAR; pSec = A_REPETIR;
      pTvAviso = p == RES_T_TV;
      break;
  }
}

// `fa` e a acao do veredito que gerou o botao (0 = cartao da cadeia antiga):
// o rotulo sai do vocabulario do Farol para texto e despacho combinarem.
static const char *rotulo_acao(int a, int fa) {
  if (fa > 0) return i18n(farolregra_acao_texto(fa));
  switch (a) {
    case A_REPETIR:    return i18n("Tentar de novo");
    case A_FONTES:     return i18n("Abrir Fontes e addons");
    case A_ENVIAR:     return i18n("Enviar um registro");
    case A_DEBRID:     return i18n("Abrir a conta de cache");
    case A_PLAYER_EXT: return i18n("Player externo");
  }
  return "";
}

static const char *icone_acao_farol(int fa) {
  switch (fa) {
    case FAROL_A_RECONECTAR_REDE:   return "aj_wifi";
    case FAROL_A_LIMPAR_CACHE:      return "aj_rotate-cw";
    case FAROL_A_BUFFER_MAIOR:      return "aj_volume-2";
    case FAROL_A_DESLIGAR_HDR:      return "aj_tv-minimal-play";
    default:                        return "aj_wrench";
  }
}

static const char *icone_acao(int a, int fa) {
  if (a == A_FAROL && fa > 0) return icone_acao_farol(fa);
  switch (a) {
    case A_REPETIR:    return "aj_rotate-cw";
    case A_FONTES:     return "aj_puzzle";
    case A_ENVIAR:     return "aj_send";
    case A_DEBRID:     return "aj_key";
    case A_PLAYER_EXT: return "aj_external-link";
    case A_ISOLAR:     return "aj_pause";
  }
  return NULL;
}

// ------------------------------------------------------------------- layout
static float ox, oy;

static void origem(void) {
  ox = (NV_TELA_W - Q_W) * 0.5f;
  oy = (NV_TELA_H - Q_H) * 0.5f;
}

static void poe(Item *it, int *n, int id, float x, float y, float w, float h) {
  if (*n >= MAX_ITENS) return;
  it[*n].id = id;
  it[*n].r = (GfxRect){ ox + x, oy + y, w, h };
  (*n)++;
}

static void poe_botao(Item *it, int *n, int id, float x, float y, const char *rot, const char *ic, int prim) {
  poe(it, n, id, x, y, botao_largura(rot, ic, prim), prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO);
}

// Altura que o titulo e o corpo do painel ocupam, quebrados na largura dele (o
// desenho usa os mesmos numeros em desenha_testes). Alpha 0 so mede.
static void medir_painel(void) {
  float w = PAINEL_W - 2.0f * PAINEL_PAD;
  pHeadH = pTitulo[0] ? txt_bloco(TXT_ILHA_TITULO, pTitulo, 0, 0, 0, 0, 0, w, 48.0f, 0.0f, 2) : 0.0f;
  pBodyH = pCorpo[0] ? txt_bloco(TXT_V2_26, pCorpo, 0, 0, 0, 0, 0, w, 32.0f, 0.0f, 6) : 0.0f;
}

static float painel_corpo_y(void) { return PAINEL_Y + 100.0f + pHeadH + 22.0f; }
static float painel_botoes_y(void) { return painel_corpo_y() + pBodyH + 36.0f; }
static float painel_h(void) {
  float h = painel_botoes_y() + BOTAO_H_PRIMARIO + PAINEL_PAD - PAINEL_Y;
  return pTvAviso ? h + 24.0f + 76.0f : h;
}

static int tem_banner(void) { return temFalha; }
static float grade_y(void) { return tem_banner() ? 430.0f : 316.0f; }
static float grade_h(void) {
  int linhas = (RES_S_FAROL_N + 1) / 2;
  return (float)linhas * SINT_H + (float)(linhas - 1) * SINT_GAP;
}
// "Achar o que ver" e da #25: ate ela ligar, a coluna aparece reservada e fora
// do foco (nada de cartao que nao faz o que promete, nem beco sem saida).
static int achar_disponivel(void) { return 1; }

// Linhas da coluna "Achar o que ver": as 4 intencoes e, com o assistente, o
// 5o item (mais justas para caber no painel da grade).
static int   achar_n(void) { return ESCOLHA_LIGADA() ? 5 : 4; }
static float achar_y(int i) {
  return ESCOLHA_LIGADA() ? 148.0f + (float)i * 60.0f : 160.0f + (float)i * 64.0f;
}

static int envio_visiveis(void) { return envN - rolo < VISIVEIS ? envN - rolo : VISIVEIS; }

static float voz_botao_largura(void) {
  float w = botao_largura(i18n(farolvozui_rotulo_botao()), farolvozui_icone_botao(), 0);
  float maxW = ACHAR_W - 28.0f - 28.0f - 56.0f - 16.0f;
  return w > maxW ? maxW : w;
}

static int montar(Item *it) {
  int n = 0, i;
  origem();
  if (fase == RESOLVER_FASE_SINTOMA) {
    float w = (GRADE_W - 24.0f) / 2.0f, y0 = grade_y();
    char lista[96];
    if (tem_banner()) poe(it, &n, B_BANNER, MARGEM, 300.0f, 1680.0f, 108.0f);
    for (i = 0; i < RES_S_FAROL_N; i++)
      poe(it, &n, resolverregra_sintoma_farol(i), MARGEM + (float)(i % 2) * (w + 24.0f),
          y0 + (float)(i / 2) * (SINT_H + SINT_GAP), w, SINT_H);
    if (achar_disponivel()) {
      if (farolvozui_disponivel()) {
        float vw = voz_botao_largura();
        poe(it, &n, B_ACHAR_VOZ, ACHAR_X + ACHAR_W - 28.0f - vw, y0 + 28.0f,
            vw, BOTAO_H_SECUNDARIO);
      }
      for (i = 0; i < achar_n(); i++)
        poe(it, &n, i < 4 ? B_ACHAR_0 + i : B_ACHAR_ESCOLHA, ACHAR_X + 28.0f,
            y0 + achar_y(i), ACHAR_W - 56.0f, 52.0f);
    }
    snprintf(lista, sizeof lista, i18n(FT_REGISTROS_ENVIADOS), envN);
    poe_botao(it, &n, B_ENVIAR, MARGEM, 944.0f, i18n("Enviar um registro"), "aj_send", 0);
    poe_botao(it, &n, B_LISTA, MARGEM + botao_largura(i18n("Enviar um registro"), "aj_send", 0) + BOTAO_GAP, 944.0f, lista, NULL, 0);
  } else if (fase == RESOLVER_FASE_CADEIA) {
    // Sem botao: a luz anda sozinha. OK adianta (sem sonda no ar), Voltar sai.
  } else if (fase == RESOLVER_FASE_VEREDITO) {
    float by, x = PAINEL_X + PAINEL_PAD, cy;
    medir_painel();
    by = painel_botoes_y();
    cy = PAINEL_Y + painel_h() + 28.0f;
    poe_botao(it, &n, B_PRINC, x, by, rotulo_acao(pPrim, pPrimF), icone_acao(pPrim, pPrimF), 1);
    if (pSec) poe_botao(it, &n, B_SEC, x + botao_largura(rotulo_acao(pPrim, pPrimF), icone_acao(pPrim, pPrimF), 1) + BOTAO_GAP, by + 8.0f,
                        rotulo_acao(pSec, pSecF), icone_acao(pSec, pSecF), 0);
    x = PAINEL_X + 190.0f;
    // O veredito do Farol so oferece o registro quando a escala deixa (B_SEC);
    // o cartao antigo (reserva da cadeia) segue com o atalho de sempre.
    if (pPrim != A_ENVIAR && !pFarol) {
      poe_botao(it, &n, B_ENVIAR, x, cy, i18n("Enviar um registro"), "aj_send", 0);
      x += botao_largura(i18n("Enviar um registro"), "aj_send", 0) + BOTAO_GAP;
    }
    poe_botao(it, &n, B_OUTRO, x, cy, i18n("Testar outro problema"), NULL, 0);
  } else {
    float x = MARGEM;
    if (envModo == M_FALHOU && !(envMotivo == AVISOS_ENVIO_INDISPONIVEL || envMotivo == AVISOS_ENVIO_CONTA)) {
      poe_botao(it, &n, B_REPETIR, x, 504.0f, i18n("Tentar de novo"), "aj_rotate-cw", 1);
      x += botao_largura(i18n("Tentar de novo"), "aj_rotate-cw", 1) + BOTAO_GAP;
      poe_botao(it, &n, B_PRONTO, x, 512.0f, i18n("Voltar"), NULL, 0);
    } else {
      poe_botao(it, &n, B_PRONTO, x, 504.0f, envModo == M_ENVIANDO ? i18n("Voltar") : envModo == M_LISTA ? i18n("Voltar") : i18n("Pronto"), NULL, 1);
    }
    for (i = 0; i < envio_visiveis(); i++)
      poe(it, &n, B_LIN0 + rolo + i, MARGEM, 790.0f + (float)i * 68.0f, 1680.0f, 64.0f);
  }
  return n;
}

// ----------------------------------------------------------- foco geometrico
static int foco_padrao(void) {
  if (fase == RESOLVER_FASE_SINTOMA) return tem_banner() ? B_BANNER : resolverregra_sintoma_farol(0);
  if (fase == RESOLVER_FASE_CADEIA || fase == RESOLVER_FASE_VEREDITO) return B_PRINC;
  return B_PRONTO;
}

static void mover(int dx, int dy) {
  Item it[MAX_ITENS];
  int n = montar(it), i, cur = -1, melhor = -1;
  float cx, cy, bs = 1e9f;
  if (fase == RESOLVER_FASE_CADEIA) return;   // sem foco ate o veredito
  if (fase == RESOLVER_FASE_ENVIO && foco >= B_LIN0 && dx == 0) {   // a lista rola
    int k = foco - B_LIN0;
    if (dy > 0 && k == rolo + VISIVEIS - 1 && rolo + VISIVEIS < envN) { rolo++; foco++; return; }
    if (dy < 0 && k == rolo && rolo > 0) { rolo--; foco--; return; }
  }
  for (i = 0; i < n; i++) if (it[i].id == foco) cur = i;
  if (cur < 0) { foco = foco_padrao(); return; }
  cx = it[cur].r.x + it[cur].r.w * 0.5f;
  cy = it[cur].r.y + it[cur].r.h * 0.5f;
  for (i = 0; i < n; i++) {
    float ax, ay, along, cross, s;
    if (i == cur) continue;
    ax = it[i].r.x + it[i].r.w * 0.5f - cx;
    ay = it[i].r.y + it[i].r.h * 0.5f - cy;
    along = ax * (float)dx + ay * (float)dy;
    cross = dx ? fabsf(ay) : fabsf(ax);
    if (along < 1.0f) continue;
    s = along + 3.0f * cross;
    if (s < bs) { bs = s; melhor = i; }
  }
  if (melhor >= 0) foco = it[melhor].id;
}

static void atualizar_falha_recente(void) {
  if (temFalha && !resolverregra_falha_recente(NULL, (long)time(NULL))) {
    temFalha = 0;
    fatos.haFalha = 0;
    if (fase == RESOLVER_FASE_SINTOMA && foco == B_BANNER) foco = foco_padrao();
  }
}

// ------------------------------------------------------------------- fluxo
static void ir_fase(int f) {
  fase = f;
  if (f == RESOLVER_FASE_SINTOMA) atualizar_falha_recente();
  foco = foco_padrao();
}

// O resultado da acao despachada antes (ADENDO C): o proximo julgamento do
// mesmo sintoma mede. O mesmo veredito de volta = a acao nao consertou (e com
// escala SE_FALHAR o cartao passa a oferecer "Mandar registro"); outro
// veredito = consertou.
static void medir_acao_pendente(int s, int cod) {
  int ok;
  if (!pendAcao || s != pendSint) return;
  ok = cod != pendCod;
  farol_registrar_resultado(pendAcao, ok);
  if (!ok) falhouCod = cod;
  else if (falhouCod == pendCod) falhouCod = -1;
  pendAcao = 0;
  pendCod = -1;
}

// Le e julga pela fachada (fio principal): farol_julgar le os fatos, julga,
// guarda o veredito da sessao e grava FE_FAROL_VEREDITO; a tela fica com a
// copia. NADA_ACHADO devolve a cadeia antiga (resolverregra) — a reserva onde
// o motor ainda nao tem playbook.
static void diagnosticar(int s) {
  const FarolVeredito *v;
  int etapa, cod;
  unsigned mascara = resolverregra_mascara(s);
  sint = s;
  fatos_ler();
  cod = farol_julgar(s);
  v = farol_veredito();
  if (v) vered = *v;
  else memset(&vered, 0, sizeof vered);
  if (!farol_veredito_copiar(NULL, &ff)) memset(&ff, 0, sizeof ff);
  // A tentativa do erro diagnosticado: o OK no "Player externo" confere de
  // novo que ela segue elegivel (outra fonte tocou ou entrou = alternativa).
  diagTentativa = 0;
  diagUsaTentativa = 0;
#ifdef RESOLVER_TESTE
  if (!usaFFixos)
#endif
  {
    diagUsaTentativa = 1;
    if (ff.erroIdadeS >= 0) diagTentativa = player_ultimo_erro_tentativa();
  }
  if (!vered.regra || vered.regra->codigo == FAROL_C_NADA_ACHADO) {
    resolverregra_testar(s, &fatos, &res);
  } else {
    res.primeira = -1;
    memcpy(res.estado, vered.estado, sizeof res.estado);
    for (etapa = 0; etapa < RES_T_N; etapa++)
      res.testado[etapa] = (mascara >> etapa) & 1u;
  }
  medir_acao_pendente(s, cod);
  preparar();
}

// ------------------------------------------------------------------- cadeia
// Onde a luz para: depois da etapa que quebrou, ou da ultima testada.
static float alvo_luz(void) {
  int t, ult = -1;
  for (t = 0; t < RES_T_N; t++) {
    if (!res.testado[t]) continue;
    if (res.estado[t] == RES_FALHOU) return (float)(t + 1);
    if (res.estado[t] == RES_OK) ult = t;
  }
  return ult >= 0 ? (float)(ult + 1) : 0.0f;
}

// A linha que a sonda ativa segura: a Fonte, se a cadeia passa por ela antes
// de parar; senao a ultima linha que a luz alcanca.
static int linha_sonda(void) {
  int alvo = (int)luzAlvo - 1;
  if (res.testado[RES_T_FONTE] && RES_T_FONTE <= alvo) return RES_T_FONTE;
  return alvo >= 0 ? alvo : 0;
}

static int sonda_no_ar(void) { return !farolsonda_estado(NULL); }

static void sonda_sincronizar(void) {
  if (sondaCancelada && !sonda_no_ar()) sondaCancelada = 0;
}

// Inicia a sonda ativa para a ultima fonte VOD (sem logar nem por a URL no
// anel). Sem URL valida, passa NULL e farolsonda_iniciar devolve 0.
static int sonda_disparar(void) {
  const Stream *st = stream_atual() >= 0 ? stream_item(stream_atual()) : NULL;
  const char *url = (st && st->url[0]) ? st->url : NULL;
  const char *cab[33];
  char copia[sizeof ((Stream *)0)->cabecalhos];
  int nc = 0;
  if (url && st->cabecalhos[0]) {
    char *p = copia;
    snprintf(copia, sizeof copia, "%s", st->cabecalhos);
    while (*p && nc < 32) {
      char *fim = strchr(p, '\n');
      size_t len;
      if (fim) *fim = 0;
      len = strlen(p);
      while (len > 0 && p[len - 1] == '\r') p[--len] = 0;
      if (*p) cab[nc++] = p;
      if (!fim) break;
      p = fim + 1;
    }
  }
  cab[nc] = NULL;
  return farolsonda_iniciar(url, nc ? cab : NULL);
}

static void veredito_entrar(void) {
  segura = 0;
  luz = luzAlvo;
  ir_fase(RESOLVER_FASE_VEREDITO);
}

// Um passo da Cadeia (`dt` em ms). Animacoes reduzidas: a luz salta direto
// ao fim (ou a linha em teste); sem respiro.
static void cadeia_passo(float dt) {
  float teto;
  sonda_sincronizar();
  if (fase != RESOLVER_FASE_CADEIA) return;
  respiraMs = fmodf(respiraMs + dt, RESPIRA_MS);
  if (segura) {
    seguraMs += dt;
    if (!sonda_no_ar() || seguraMs >= SONDA_MS) {
      if (sonda_no_ar()) {
        farolsonda_cancelar();   // 5 s: segue sem ela
        sondaCancelada = sonda_no_ar();
      }
      segura = 0;
      diagnosticar(sint);                          // a sonda trouxe fatos novos
      luzAlvo = alvo_luz();
      if (luz > luzAlvo) luz = luzAlvo;
    }
  }
  teto = segura ? (float)linha_sonda() + 1.0f : luzAlvo;
  if (teto > luzAlvo) teto = luzAlvo;
  if (ajustes_animacoes_reduzidas()) luz = teto;
  else {
    luz += dt / LUZ_MS;
    if (luz > teto) luz = teto;
  }
  if (!segura && luz >= luzAlvo) veredito_entrar();
}

// Inicio -> Cadeia. Inaugura o diagnostico DESTE sintoma sobre a sessao
// corrente (o erro que trouxe a pessoa ate aqui ainda vale); a fronteira nova
// (farol_sessao_reiniciar) vem quando o Farol despacha um conserto.
static void cadeia_iniciar(int s) {
  int iniciou = 0;
  sonda_sincronizar();
  luz = 0.0f; seguraMs = 0.0f; respiraMs = 0.0f;
  fase = RESOLVER_FASE_CADEIA;
  foco = B_PRINC;
  if (resolverregra_mascara(s)) {
    iniciou = sonda_disparar();
    if (iniciou) sondaCancelada = 0;
  }
  diagnosticar(s);
  luzAlvo = alvo_luz();
  if (!resolverregra_mascara(s)) { veredito_entrar(); return; }   // passo a passo
  segura = iniciou || (!sondaCancelada && sonda_no_ar());
  cadeia_passo(0.0f);
}

static void cadeia_cancelar(void) {
  if (segura || sonda_no_ar()) {
    farolsonda_cancelar();
    sondaCancelada = sonda_no_ar();
  }
  segura = 0;
  ir_fase(RESOLVER_FASE_SINTOMA);
}

static void enviar_registro(int s, int volta) {
  if (farolenvioui_presente()) {
    const char *ult = envN > 0 ? env[0].codigo : "";
    envSint = s;
    voltaFase = volta;
    envModo = M_NADA;
    envDoFarol = 0;
    rolo = 0;
    farolenvioui_abrir(s, volta, 0, &vered, &ff, res.estado, ult);
    ir_fase(RESOLVER_FASE_ENVIO);
    return;
  }
  {
  AvisosEnvio i;
  avisos_envio_info(&i);
  envSint = s;
  voltaFase = volta;
  envModo = M_ENVIANDO;
  envNoAr = 1;
  envDoFarol = 0;
  envInicio = i.estado == 1 ? (long)i.quando : (long)time(NULL);
  envCodigo[0] = 0;
  envMotivo = envHttp = 0;
#ifndef RESOLVER_TESTE
  avisos_enviar_registro_atual();
#endif
  rolo = 0;
  ir_fase(RESOLVER_FASE_ENVIO);
  }
}

// "Mandar registro" do Veredito (#22): abre a Tela 4 com espelho e farol_enviar().
static void mandar_registro(void) {
  if (farolenvioui_presente()) {
    const char *ult = envN > 0 ? env[0].codigo : "";
    envSint = sint;
    voltaFase = RESOLVER_FASE_VEREDITO;
    envModo = M_NADA;
    envDoFarol = 1;
    rolo = 0;
    farolenvioui_abrir(sint, RESOLVER_FASE_VEREDITO, 1, &vered, &ff, res.estado, ult);
    ir_fase(RESOLVER_FASE_ENVIO);
    return;
  }
  enviar_registro(sint, RESOLVER_FASE_VEREDITO);
  envDoFarol = 1;
}

static void abrir(void) {
  aberto = 1;
  entrada = 0.0f;
  pedido = RESOLVER_PEDIU_NADA;
  pedidoTentativa = 0;
  diagTentativa = 0;
  diagUsaTentativa = 0;
  acharVoltaFoco = 0;
  farolachaui_reset();
  farolvozui_reset();
  if (ESCOLHA_LIGADA()) farolescolhaui_reset();
  segura = 0;
  sonda_sincronizar();
  envModo = M_NADA;
  rolo = 0;
  enviados_ler();
  fatos_ler();
  ir_fase(RESOLVER_FASE_SINTOMA);
}

void resolver_abrir(void) {
  abrir();
  if (farolconsent_deve_perguntar() == 1) farolconsentui_abrir();
}
void resolver_abrir_sintoma(int s) {
  abrir();
  farolconsentui_fechar();
  if (s >= 0 && s < RES_S_N) cadeia_iniciar(s);
}

static void fechar(void) {
  farolconsentui_fechar();
  farolenvioui_fechar();
  farolvozui_fechar();
  if (ESCOLHA_LIGADA()) farolescolhaui_fechar();
  if (segura || sonda_no_ar()) {
    farolsonda_cancelar();
    sondaCancelada = sonda_no_ar();
  }
  segura = 0;
  aberto = 0;
}

// O conserto sai da tela (ADENDO C): a acao vai ao anel, fica pendente para o
// proximo julgamento medir e, como a proxima tentativa e outra, a sessao de
// diagnostico recomeca AGORA — com a tentativa ja copiada para o despacho do
// player externo (pedidoTentativa), que segue valendo para ela.
static void despachar(int fa, int ped) {
  if (fa > 0) {
    farol_registrar_acao(fa);
    pendAcao = fa;
    pendCod = vered.regra ? (int)vered.regra->codigo : -1;
    pendSint = sint;
  }
  farol_sessao_reiniciar();
  pedido = ped;
  fechar();
}

// `fa` e a acao do veredito que gerou o botao (0 no cartao antigo): vai para
// o anel como FE_FAROL_ACAO — o resultado do conserto se mede depois. A_FAROL
// sai como pedido para o app executar com farolacao_rede_executar (issue
// #34): e o executor quem registra acao/resultado no anel, entao aqui o
// FE_FAROL_ACAO local NAO se repete. `longo` e o OK segurado: em
// FAROL_A_DESLIGAR_HDR pede a versao persistente ("sempre nesta TV"); nas
// outras acoes nao muda nada.
static void aplicar(int a, int fa, int longo) {
  pedidoTentativa = 0;
  // Farol #17: isola o add-on lento por 24 h (anel, arquivo e ilha com Desfazer
  // ficam com farolacao_addon). Recusado no OK (outro fio, teto): testa de novo.
  if (a == A_ISOLAR) {
    if (farolacao_addon_isolar_lento() == FAAD_OK) fechar();
    else aplicar(A_REPETIR, 0, 0);
    return;
  }
  if (a == A_PLAYER_EXT && diagUsaTentativa) {
    FarolEvento ev[FAROL_ANEL_N];
    int n = farolanel_ler(ev, FAROL_ANEL_N, 0);
    if (!diagTentativa || anel_pronto_apos_erro(ev, n) ||
        !player_externo_disponivel_tentativa(diagTentativa)) {
      diagnosticar(sint);   // a tentativa mudou: o cartao cai na alternativa
      luzAlvo = alvo_luz();
      veredito_entrar();
      return;
    }
  }
  if (a == A_FAROL) {
    if (fa > 0) {
      pendAcao = fa;
      pendCod = vered.regra ? (int)vered.regra->codigo : -1;
      pendSint = sint;
    }
    farol_sessao_reiniciar();
    pedidoAcao = fa;
    pedidoPersistir = fa == FAROL_A_DESLIGAR_HDR && longo ? 1 : 0;
    pedido = RESOLVER_PEDIU_FAROL;
    fechar();
    return;
  }
  if (a == A_REPETIR) {
    if (fa > 0) {
      farol_registrar_acao(fa);
      pendAcao = fa;
      pendCod = vered.regra ? (int)vered.regra->codigo : -1;
      pendSint = sint;
    }
    cadeia_iniciar(sint);
  } else if (a == A_FONTES) despachar(fa, RESOLVER_PEDIU_FONTES);
  else if (a == A_DEBRID) despachar(fa, RESOLVER_PEDIU_DEBRID);
  else if (a == A_PLAYER_EXT) {
    uint64_t t = diagUsaTentativa ? diagTentativa : 0;
    despachar(fa, RESOLVER_PEDIU_PLAYER_EXT);
    pedidoTentativa = t;
  } else if (a == A_ENVIAR) {
    if (fa > 0) {
      farol_registrar_acao(fa);
      mandar_registro();
    } else enviar_registro(sint, RESOLVER_FASE_VEREDITO);
  }
}

#ifdef RESOLVER_TESTE
void resolver_teste_testar(int s) {
  diagnosticar(s);
  luzAlvo = alvo_luz();
  veredito_entrar();
}
#endif

// O Desligar o HDR tem a versao duravel: toque = esta reproducao, segurar =
// "sempre nesta TV" (FAROL34B). O segurar arma no KEYDOWN do OK com foco na
// celula dessa acao, vence no passo (segurar_venceu) e executa no KEYUP.
#define RES_OK_TOQUE_MS 150u   // convencao do ctxhold: passou disto, ja vira dica
static int foco_desliga_hdr(void) {
  return fase == RESOLVER_FASE_VEREDITO &&
         ((foco == B_PRINC && pPrim == A_FAROL && pPrimF == FAROL_A_DESLIGAR_HDR) ||
          (foco == B_SEC  && pSec  == A_FAROL && pSecF  == FAROL_A_DESLIGAR_HDR));
}

static void ok(int longo) {
  if (fase == RESOLVER_FASE_SINTOMA) {
    if (resolverregra_sintoma_valido(foco)) cadeia_iniciar(foco);
    else if (foco == B_BANNER) cadeia_iniciar(RES_S_NAO_COMECA);
    else if (foco == B_ENVIAR) enviar_registro(RES_S_GERAL, RESOLVER_FASE_SINTOMA);
    else if (foco == B_LISTA) {
      envModo = M_LISTA; voltaFase = RESOLVER_FASE_SINTOMA; rolo = 0;
      ir_fase(RESOLVER_FASE_ENVIO);
      if (envN > 0) foco = B_LIN0;
    } else if (foco >= B_ACHAR_0 && foco <= B_ACHAR_3) {
      int k = foco - B_ACHAR_0;
      acharVoltaFoco = foco;
      farolachaui_abrir((FarolAchaTipo)k, FAROL_ACHA_CURTO_PADRAO, 0u);
      fase = RESOLVER_FASE_ACHAR;
    } else if (foco == B_ACHAR_ESCOLHA && ESCOLHA_LIGADA()) {
      acharVoltaFoco = foco;
      fase = RESOLVER_FASE_ESCOLHA;
      farolescolhaui_abrir();
      if (farolescolhaui_pediu_fechar()) fechar();   // ja terminou no P4
    } else if (foco == B_ACHAR_VOZ && farolvozui_disponivel()) {
      acharVoltaFoco = B_ACHAR_VOZ;
      farolvozui_abrir(RESOLVER_FASE_SINTOMA);
    }
  } else if (fase == RESOLVER_FASE_CADEIA) {
    if (!segura) veredito_entrar();   // OK adianta a luz; a sonda no ar segura
  } else if (fase == RESOLVER_FASE_VEREDITO) {
    if (foco == B_PRINC) aplicar(pPrim, pPrimF, longo);
    else if (foco == B_SEC) aplicar(pSec, pSecF, longo);
    else if (foco == B_ENVIAR) enviar_registro(sint, RESOLVER_FASE_VEREDITO);
    else if (foco == B_OUTRO) ir_fase(RESOLVER_FASE_SINTOMA);
  } else {
    if (foco == B_PRONTO) { int v = voltaFase; envModo = M_NADA; ir_fase(v); }
    else if (foco == B_REPETIR) { int df = envDoFarol; enviar_registro(envSint, voltaFase); envDoFarol = df; }
    else if (foco >= B_LIN0) {
      int k = foco - B_LIN0;
      if (k >= 0 && k < envN) {
        envN = resolverregra_enviados_remover(env, envN, env[k].codigo);
        enviados_gravar();
        if (rolo > 0 && rolo + VISIVEIS > envN) rolo = envN > VISIVEIS ? envN - VISIVEIS : 0;
        foco = envN > 0 ? B_LIN0 + (k < envN ? k : envN - 1) : B_PRONTO;
        if (envN > 0 && foco - B_LIN0 < rolo) foco = B_LIN0 + rolo;
      }
    }
  }
}

static void voltar(void) {
  if (fase == RESOLVER_FASE_SINTOMA) fechar();
  else if (fase == RESOLVER_FASE_CADEIA) cadeia_cancelar();   // Voltar cancela a sonda na hora
  else if (fase == RESOLVER_FASE_VEREDITO) ir_fase(RESOLVER_FASE_SINTOMA);
  else { int v = voltaFase; envModo = M_NADA; ir_fase(v); }
}

static int tecla_ok(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

static void ir_fase(int f);

static void voz_voltar_fase(void) {
  int vf = farolvozui_volta_fase();
  if (vf == RESOLVER_FASE_ACHAR) {
    fase = RESOLVER_FASE_ACHAR;
  } else {
    ir_fase(RESOLVER_FASE_SINTOMA);
    if ((acharVoltaFoco >= B_ACHAR_0 && acharVoltaFoco <= B_ACHAR_3) ||
        acharVoltaFoco == B_ACHAR_VOZ)
      foco = acharVoltaFoco;
  }
}

static void sincronizar_envioui(float dt) {
  int ped = RESOLVER_PEDIU_NADA, fech = 0, volt = 0, recar = 0, dfOk = -1;
  if (!farolenvioui_presente()) return;
  farolenvioui_passo(dt, &ped, &fech, &volt, &recar, &dfOk);
  if (recar) enviados_ler();
  if (dfOk >= 0) farol_registrar_resultado(FAROL_A_ENVIAR_REGISTRO, dfOk);
  if (ped != RESOLVER_PEDIU_NADA) pedido = ped;
  if (fech) fechar();
  else if (volt && fase == RESOLVER_FASE_ENVIO) ir_fase(voltaFase);
}

void resolver_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return;
  if (farolconsentui_aberto()) {
    if (e->type == SDL_KEYDOWN) farolconsentui_evento(e);
    return;
  }
  if (farolvozui_ativo()) {
    int pediuVoltar = 0, abriuAchados = 0;
    farolvozui_evento(e, &pediuVoltar, &abriuAchados);
    if (abriuAchados) fase = RESOLVER_FASE_ACHAR;
    else if (pediuVoltar) voz_voltar_fase();
    return;
  }
  if (fase == RESOLVER_FASE_ACHAR) {
    if (e->type == SDL_KEYDOWN) {
      int pediuVoltar = 0, pediuAbrir = 0;
      farolachaui_evento(e, &pediuVoltar, &pediuAbrir);
      (void)pediuAbrir;
      if (farolachaui_pediu_voz() && farolvozui_disponivel()) {
        farolvozui_abrir(RESOLVER_FASE_ACHAR);
        return;
      }
      if (pediuVoltar) {
        ir_fase(RESOLVER_FASE_SINTOMA);
        if ((acharVoltaFoco >= B_ACHAR_0 && acharVoltaFoco <= B_ACHAR_3) ||
            acharVoltaFoco == B_ACHAR_VOZ)
          foco = acharVoltaFoco;
      }
    }
    return;
  }
  if (fase == RESOLVER_FASE_ESCOLHA) {
    if (e->type == SDL_KEYDOWN) {
      int pediuSair = 0, pediuAbrir = 0;
      farolescolhaui_evento(e, &pediuSair, &pediuAbrir);
      (void)pediuAbrir;   // app.c consome por resolver_pediu_abrir
      if (farolescolhaui_pediu_fechar()) { fechar(); return; }   // o P4 abriu
      if (pediuSair) {
        ir_fase(RESOLVER_FASE_SINTOMA);
        foco = B_ACHAR_ESCOLHA;
      }
    }
    return;
  }
  if (fase == RESOLVER_FASE_ENVIO && farolenvioui_ativo()) {
    if (e->type == SDL_KEYDOWN) {
      farolenvioui_evento(e);
      sincronizar_envioui(0.0f);
    }
    return;
  }
  // O OK segurado executa no KEYUP (toque vs longo); o KEYDOWN so arma.
  if (e->type == SDL_KEYUP) {
    k = e->key.keysym.sym;
    if (tecla_ok(k) && okArmado) {
      int l = okLongo;
      okArmado = okLongo = 0; okDesde = 0;
      ok(l);
    }
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (tecla_ok(k)) {
    // KEYDOWN repetido do mesmo OK segurando: mantem armado.
    if (!e->key.repeat) {
      okArmado = foco_desliga_hdr();
      okLongo = 0;
      { Uint32 t = SDL_GetTicks();   // 0 = nao armado para segurar_venceu;
        if (!t) t = 1;               // ticks reais podem ser 0 no 1o quadro
        okDesde = t; }               // (tests/farol_resolver, GCC/Clang)
      if (!okArmado) ok(0);   // fora da celula longa, o OK continua no KEYDOWN
    }
    return;
  }
  okArmado = 0; okDesde = 0; okLongo = 0;
  if (k == SDLK_LEFT)  { mover(-1, 0); return; }
  if (k == SDLK_RIGHT) { mover(1, 0); return; }
  if (k == SDLK_UP)    { mover(0, -1); return; }
  if (k == SDLK_DOWN)  { mover(0, 1); return; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) voltar();
}

// Segue o envio ate o fim, ainda que a tela ja tenha sido fechada: o codigo
// que chega depois entra na lista do mesmo jeito.
static void sondar_envio(void) {
  AvisosEnvio i;
  if (!envNoAr) return;
  avisos_envio_info(&i);
  if ((long)i.quando < envInicio || i.estado == 1 || i.estado == 0) return;
  if (i.estado == 2) {
    envQuando = (long)time(NULL);
    snprintf(envCodigo, sizeof envCodigo, "%s", i.codigo);
    if (envCodigo[0]) {
      envN = resolverregra_enviados_add(env, envN, RES_ENVIADOS_MAX, envCodigo, envQuando, envSint);
      enviados_gravar();
    }
  } else {
    envMotivo = i.motivo;
    envHttp = i.http;
  }
  envNoAr = i.estado == 3 && i.motivo == AVISOS_ENVIO_OFFLINE;
  if (envDoFarol && !envNoAr) {   // "Mandar registro" do Veredito chegou ao fim
    farol_registrar_resultado(FAROL_A_ENVIAR_REGISTRO, i.estado == 2);
    envDoFarol = 0;
  }
  if (envModo != M_ENVIANDO) return;   // a pessoa ja voltou: o codigo ficou so na lista
  envModo = i.estado == 2 ? M_ENVIADO : M_FALHOU;
  if (aberto && fase == RESOLVER_FASE_ENVIO) foco = foco_padrao();
}

void resolver_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
  sonda_sincronizar();
  farolconsentui_atualizar(dt);
  sincronizar_envioui(dt);
  if (!farolenvioui_presente()) sondar_envio();
  if (aberto) {
    atualizar_falha_recente();
    cadeia_passo(dt > 0.0f ? dt * 1000.0f : 0.0f);
    // Segurar OK em "Desligar o HDR": a dica sobe depois do toque e a acao
    // duravel vence em NV_HOLD_MS (convencao segurar.h/ctxhold). A execucao e
    // no KEYUP — segurar alem do vencido nao dispara duas vezes.
    if (okArmado) {
      if (okLongo) ilha_atividade(i18n(FT_ACAO_DESLIGAR_HDR_SEMPRE), 1.0f);
      else if (agora - okDesde >= RES_OK_TOQUE_MS)
        ilha_atividade(i18n(FT_ACAO_DESLIGAR_HDR_SEMPRE),
                       (float)(agora - okDesde) / (float)NV_HOLD_MS);
      if (!okLongo && segurar_venceu(agora, okDesde, "resolver")) okLongo = 1;
    }
    if (farolvozui_ativo()) {
      int abriuAchados = 0, pediuVoltar = 0;
      farolvozui_atualizar(dt, agora, &abriuAchados, &pediuVoltar);
      if (abriuAchados) fase = RESOLVER_FASE_ACHAR;
      else if (pediuVoltar) voz_voltar_fase();
    } else if (fase == RESOLVER_FASE_ACHAR) {
      farolachaui_atualizar(dt, agora);
    } else if (fase == RESOLVER_FASE_ESCOLHA) {
      farolescolhaui_atualizar(dt, agora);
      if (farolescolhaui_pediu_fechar()) fechar();
    }
  }
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (red) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? ABRIR_MS : FECHAR_MS);
}

#ifdef RESOLVER_TESTE
// Toque (0) ou OK segurado (1) no botao em foco, sem fabricar SDL_Event.
void resolver_teste_ok(int persistir) { ok(persistir != 0); }
#endif

// ------------------------------------------------------------------- desenho
typedef struct { int r, g, b; } Tinta;

static void txt_em(TxtEstilo est, const char *s, float x, float y, float maxW, int r, int g, int b, float a) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static void pt_foco(int id, int nada) { (void)nada; foco = id; }

static void alvo(GfxRect r, int id) {
  if (aberto) ponteiro_alvo(r.x, r.y, r.w, r.h, pt_foco, NULL, id, 0);
}

static void icone_t(GfxRect r, const char *nome, Tinta t, float a) {
  gfx_icone(r, nome, (float)t.r / 255.0f, (float)t.g / 255.0f, (float)t.b / 255.0f, a);
}

// Superficie de um cartao/linha: repouso cinza tingido, foco na cor de realce.
static void superficie(GfxRect r, float raioPx, int focado, float a, Tinta *pri, Tinta *sec) {
  if (focado) {
    float fr, fg, fb, t = botao_cor_foco(&fr, &fg, &fb);
    int p = (int)(t * 255.0f + 0.5f);
    botao_luz(r, 1.0f, a);
    gfx_cor(r, raioPx / r.h, fr, fg, fb, a);
    *pri = (Tinta){ p, p, p };
    *sec = p > 128 ? (Tinta){ 238, 238, 238 } : (Tinta){ 60, 60, 60 };
  } else {
    gfx_cor(r, raioPx / r.h, 0.14f, 0.15f, 0.17f, 0.92f * a);
    *pri = (Tinta){ 243, 242, 239 };
    *sec = (Tinta){ 186, 192, 204 };
  }
}

static void bola(float x, float y, float lado, const char *icone, Tinta t, int focado, float a) {
  gfx_cor((GfxRect){ x, y, lado, lado }, 0.31f, 1, 1, 1, (focado ? 0.16f : 0.07f) * a);
  icone_t((GfxRect){ x + lado * 0.28f, y + lado * 0.28f, lado * 0.44f, lado * 0.44f }, icone, t, a);
}

static void painel_fundo(GfxRect r, float raioPx, float a) {
  gfx_cor(r, raioPx / r.h, 0.12f, 0.125f, 0.145f, 0.9f * a);
  gfx_anel(r, raioPx / r.h, 1.0f, 1, 1, 1, 0.08f * a);
}

// Painel do Farol: o vidro 2.1 (gfx_vidro_painel, sem desfoque de fundo) com a
// Interface de vidro ligada; o solido de sempre com ela desligada.
static void painel(GfxRect r, float raioPx, float a) {
  if (ajustes_vidro()) gfx_vidro_painel(r, raioPx / r.h, 0.78f, a);
  else painel_fundo(r, raioPx, a);
}

static void cabecalho(const char *kick, const char *tit, const char *sub, float a) {
  txt_em(TXT_CAPTION, kick, ox + MARGEM, oy + 96.0f, 1500.0f, 150, 156, 168, a);
  txt_em(TXT_NOV_TITULO, tit, ox + MARGEM, oy + 128.0f, 1680.0f, 248, 249, 252, a);
  txt_bloco(TXT_V2_26, sub, 186, 192, 204, ox + MARGEM, oy + 206.0f, 1500.0f, 34.0f, 0.9f * a, 2);
}

static void chip(Item *it, int id, const char *rot, const char *ic, float a) {
  botao_pilula(it->r, rot, ic, foco == id ? 1.0f : 0.0f, 0, 0, a);
  alvo(it->r, id);
}

static void desenha_sintoma(float a) {
  Item it[MAX_ITENS];
  int n = montar(it), i;
  Tinta p, s;
  cabecalho(i18n(FT_AJUDA_KICKER), i18n(FT_NOME),
            i18n("Escolha o que mais parece. O Nuvio testa sozinho e diz o que achou."), a);
  for (i = 0; i < n; i++) {
    GfxRect r = it[i].r;
    int id = it[i].id, f = foco == id;
    const SintInfo *si = sint_info(id);
    if (id == B_BANNER) {
      char d[360], ha[40];
      texto_ha((long)time(NULL) - falha.agora, ha, sizeof ha);
      if (falha.midia[0]) snprintf(d, sizeof d, "%s · %s · %s", ha, falha.midia, falha.titulo);
      else snprintf(d, sizeof d, "%s · %s", ha, falha.titulo);
      superficie(r, 28.0f, f, a, &p, &s);
      bola(r.x + 26.0f, r.y + 22.0f, 64.0f, "aj_triangle-alert", p, f, a);
      txt_em(TXT_ILHA_NOME, i18n(FT_BANNER_FALHA), r.x + 116.0f, r.y + 18.0f, r.w - 220.0f, p.r, p.g, p.b, a);
      txt_em(TXT_CAPTION, d, r.x + 116.0f, r.y + 58.0f, r.w - 220.0f, s.r, s.g, s.b, a);
      icone_t((GfxRect){ r.x + r.w - 28.0f - 26.0f, r.y + (r.h - 26.0f) * 0.5f, 26.0f, 26.0f }, "aj_chevron-right", s, a);
      alvo(r, id);
    } else if (si) {
      superficie(r, 24.0f, f, a, &p, &s);
      bola(r.x + 24.0f, r.y + (r.h - 56.0f) * 0.5f, 56.0f, si->icone, p, f, a);
      txt_em(TXT_ILHA_NOME, i18n(si->tit), r.x + 100.0f, r.y + 20.0f, r.w - 124.0f, p.r, p.g, p.b, a);
      txt_em(TXT_CAPTION, i18n(si->desc), r.x + 100.0f, r.y + 60.0f, r.w - 124.0f, s.r, s.g, s.b, a);
      alvo(r, id);
    } else if (id == B_ENVIAR) chip(&it[i], id, i18n("Enviar um registro"), "aj_send", a);
    else if (id == B_LISTA) {
      char l[96];
      snprintf(l, sizeof l, i18n(FT_REGISTROS_ENVIADOS), envN);
      chip(&it[i], id, l, NULL, a);
    }
  }
  // Coluna direita: "Achar o que ver" (#25) com as 4 intencoes focaveis.
  { static const FarolAchaTipo TIPO[4] = { FA_CURTO, FA_NAO_VI, FA_RETOMAR, FA_SURPRESA };
    GfxRect r = { ox + ACHAR_X, oy + grade_y(), ACHAR_W, grade_h() };
    int k;
    painel(r, 28.0f, a);
    bola(r.x + 28.0f, r.y + 28.0f, 56.0f, "aj_compass", (Tinta){ 243, 242, 239 }, 0, a);
    if (farolvozui_disponivel()) {
      float vw = voz_botao_largura();
      GfxRect rVoz = { r.x + r.w - 28.0f - vw, r.y + 28.0f, vw, BOTAO_H_SECUNDARIO };
      botao_pilula(rVoz, i18n(farolvozui_rotulo_botao()), farolvozui_icone_botao(),
                   foco == B_ACHAR_VOZ ? 1.0f : 0.0f, 0, 0, a);
      alvo(rVoz, B_ACHAR_VOZ);
    }
    txt_em(TXT_ILHA_NOME, i18n(FT_ACHAR_O_QUE_VER), r.x + 28.0f, r.y + 104.0f, r.w - 56.0f, 243, 242, 239, a);
    for (k = 0; k < achar_n(); k++) {
      GfxRect c = { r.x + 28.0f, r.y + achar_y(k), r.w - 56.0f, 52.0f };
      int id = k < 4 ? B_ACHAR_0 + k : B_ACHAR_ESCOLHA;
      int f = foco == id;
      if (f) {
        superficie(c, c.h * 0.5f, 1, a, &p, &s);
      } else {
        gfx_cor(c, 0.5f, 1, 1, 1, 0.06f * a);
        p = (Tinta){ 243, 242, 239 };
        s = (Tinta){ 186, 192, 204 };
      }
      txt_em(TXT_V2_26, i18n(k < 4 ? farolacha_rotulo(TIPO[k]) : ROT_ESCOLHA), c.x + 22.0f, c.y + 10.0f,
             c.w - 64.0f, p.r, p.g, p.b, a);
      icone_t((GfxRect){ c.x + c.w - 38.0f, c.y + (c.h - 20.0f) * 0.5f, 20.0f, 20.0f },
              "aj_chevron-right", f ? p : s, (f ? 0.95f : 0.65f) * a);
      alvo(c, id);
    } }
  txt_em(TXT_CAPTION, i18n("Nenhum teste muda seus Ajustes."), ox + 1800.0f - 560.0f, oy + 962.0f, 560.0f, 150, 156, 168, a);
}

// Respiro da linha em teste: 0,45..1 num ciclo de RESPIRA_MS; parado em
// animacoes reduzidas.
static float respiro(void) {
  if (ajustes_animacoes_reduzidas()) return 1.0f;
  return 0.725f + 0.275f * cosf(respiraMs / RESPIRA_MS * 6.2831853f);
}

static int linha_acesa(int t) {
  return fase != RESOLVER_FASE_CADEIA || luz >= (float)(t + 1) - 0.001f;
}

static void linha_teste(int t, float x, float y, float w, float a) {
  char buf[96];
  int testando = fase == RESOLVER_FASE_CADEIA && segura && t == linha_sonda() && res.testado[t];
  int acesa = linha_acesa(t) && !testando;
  int st = res.estado[t];
  int sem = !res.testado[t] || st == RES_NAO_TESTADO || (t == RES_T_AUDIO && st == RES_OK) || !acesa;
  const char *det = !res.testado[t] ? detalhe_teste(t, buf, sizeof buf)
                  : testando ? i18n(FT_TESTANDO)
                  : !acesa ? (st == RES_NAO_TESTADO ? i18n(FT_NAO_TESTADO) : "")
                  : detalhe_teste(t, buf, sizeof buf);
  GfxRect c = { x, y + 6.0f, 44.0f, 44.0f };
  Tinta cl = { 243, 242, 239 }, dim = { 120, 126, 138 };
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(c, 0.5f, 0.051f, 0.051f, 0.051f, a);          // tapa o trilho atras do icone
  if (testando) {
    float rp = respiro();
    gfx_cor(c, 0.5f, ar, ag, ab, 0.22f * rp * a);
    gfx_anel(c, 0.5f, 2.0f, ar, ag, ab, rp * a);
    icone_t((GfxRect){ c.x + 11.0f, c.y + 11.0f, 22.0f, 22.0f }, TESTE[t].icone, cl, rp * a);
  } else if (acesa && st == RES_FALHOU) {
    float ti = ajustes_acento_tinta(&ar, &ag, &ab);
    int v = (int)(ti * 255.0f + 0.5f);
    gfx_cor(c, 0.5f, ar, ag, ab, a);
    icone_t((GfxRect){ c.x + 11.0f, c.y + 11.0f, 22.0f, 22.0f }, "aj_triangle-alert", (Tinta){ v, v, v }, a);
  } else if (sem) {
    gfx_anel(c, 0.5f, 2.0f, 1, 1, 1, 0.22f * a);
    icone_t((GfxRect){ c.x + 11.0f, c.y + 11.0f, 22.0f, 22.0f }, TESTE[t].icone, dim, a);
  } else {
    gfx_cor(c, 0.5f, 1, 1, 1, 0.10f * a);
    icone_t((GfxRect){ c.x + 11.0f, c.y + 11.0f, 22.0f, 22.0f }, "aj_check", cl, a);
  }
  txt_em(TXT_ILHA_NOME, i18n(TESTE[t].nome), x + 68.0f, y, w - 68.0f, sem && !testando ? 186 : 243,
         sem && !testando ? 192 : 242, sem && !testando ? 204 : 239, a);
  if (det[0]) txt_bloco(TXT_CAPTION, det, 186, 192, 204, x + 68.0f, y + 36.0f, w - 68.0f, 26.0f, (testando ? respiro() : 0.95f) * a, 2);
  gfx_cor((GfxRect){ x, y + 90.0f, w, 1.0f }, 0, 1, 1, 1, 0.07f * a);
}

// A luz do Farol (ALMA 4): um trilho fino no acento desce pela coluna dos
// icones ate onde a luz esta, com a ponta mais viva. Sem orbe, sem FBO e sem
// desfoque — dois gfx_cor.
static void trilho_luz(float x, float y0, float a) {
  float ar, ag, ab, topo = y0 + 28.0f, fim = y0 + 28.0f + (float)(RES_T_N - 1) * 96.0f, ponta;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ x + 21.0f, topo, 2.0f, fim - topo }, 0.0f, 1, 1, 1, 0.08f * a);
  if (luz <= 0.0f) return;
  ponta = topo + (luz - 1.0f) * 96.0f;
  if (ponta < topo) return;                 // a luz ainda nao chegou ao primeiro icone
  if (ponta > fim) ponta = fim;
  if (ponta > topo) gfx_cor((GfxRect){ x + 21.0f, topo, 2.0f, ponta - topo }, 0.0f, ar, ag, ab, 0.55f * a);
  if (fase == RESOLVER_FASE_CADEIA) {       // a ponta viva, so enquanto anda
    float y = ponta - 24.0f < topo ? topo : ponta - 24.0f;
    gfx_cor((GfxRect){ x + 20.0f, y, 4.0f, ponta - y + 1.0f }, 0.5f, ar, ag, ab, a);
  }
}

static void desenha_testes(float a) {
  Item it[MAX_ITENS];
  char kick[160], tit[200];
  int n = montar(it), i, cad = fase == RESOLVER_FASE_CADEIA;
  float py = PAINEL_Y;
  snprintf(kick, sizeof kick, "%s › %s", i18n(FT_NOME), sint_titulo(sint));
  if (sint == RES_S_NAO_COMECA && falha.midia[0] && temFalha) snprintf(tit, sizeof tit, i18n("Testamos %s"), falha.midia);
  else snprintf(tit, sizeof tit, "%s", sint_titulo(sint));
  cabecalho(kick, tit, resolverregra_mascara(sint)
            ? i18n("Só lemos o último erro e o estado de agora. Nenhum teste toca vídeo.")
            : i18n("Este problema não tem teste. Aqui vão os passos e, se precisar, o envio do registro."), a);
  if (resolverregra_mascara(sint)) {
    trilho_luz(ox + MARGEM, oy + 300.0f, a);
    for (i = 0; i < RES_T_N; i++) linha_teste(i, ox + MARGEM, oy + 300.0f + (float)i * 96.0f, 780.0f, a);
  }
  // O Veredito (tambem visivel durante a Cadeia/Sondando: selo e as duas linhas
  // ja ficam na tela, enquanto na Cadeia a faixa dos botoes pulsa o estado).
  medir_painel();
  { float x = ox + PAINEL_X, y = oy + py, w = PAINEL_W, h = painel_h(), ar, ag, ab;
    GfxRect pr = { x, y, w, h };
    painel(pr, 32.0f, a);
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor((GfxRect){ x + PAINEL_PAD, y + 48.0f, 14.0f, 14.0f }, 0.5f, pAcheou ? ar : 0.6f, pAcheou ? ag : 0.62f, pAcheou ? ab : 0.66f, a);
    txt_em(TXT_CAPTION, pMarca, x + PAINEL_PAD + 26.0f, y + 40.0f, 600.0f, pAcheou ? 243 : 186, pAcheou ? 242 : 192, pAcheou ? 239 : 204, a);
    txt_bloco(TXT_ILHA_TITULO, pTitulo, 248, 249, 252, x + PAINEL_PAD, y + 80.0f, w - 2 * PAINEL_PAD, 48.0f, a, 2);
    txt_bloco(TXT_V2_26, pCorpo, 200, 205, 215, x + PAINEL_PAD, oy + painel_corpo_y(), w - 2 * PAINEL_PAD, 32.0f, 0.92f * a, 6);
    if (cad) {
      float rp = segura ? respiro() : 1.0f, sy = oy + painel_botoes_y() + (BOTAO_H_PRIMARIO - 32.0f) * 0.5f;
      gfx_cor((GfxRect){ x + PAINEL_PAD, sy + 10.0f, 12.0f, 12.0f }, 0.5f, ar, ag, ab, rp * a);
      txt_em(TXT_V2_26, segura ? i18n(FT_SONDANDO_FONTE) : i18n(FT_SONDANDO),
             x + PAINEL_PAD + 24.0f, sy, w - 2 * PAINEL_PAD - 24.0f, 186, 192, 204, rp * a);
    }
    if (pTvAviso) {
      GfxRect r = { x + PAINEL_PAD, oy + painel_botoes_y() + BOTAO_H_PRIMARIO + 24.0f, w - 2 * PAINEL_PAD, 76.0f };
      gfx_cor(r, 20.0f / r.h, 1, 1, 1, 0.05f * a);
      txt_em(TXT_V2_26, i18n("Esconder fontes que esta TV não toca"), r.x + 24.0f, r.y + 8.0f, r.w - 200.0f, 186, 192, 204, 0.6f * a);
      txt_em(TXT_CAPTION, i18n("Ainda não dá para ligar isto por aqui."), r.x + 24.0f, r.y + 44.0f, r.w - 200.0f, 150, 156, 168, 0.6f * a);
      { TxtLinha l = txt_linha(TXT_CAPTION, i18n("Em breve"), 186, 192, 204, 255);
        txt_desenhar_alpha(l, r.x + r.w - 24.0f - (float)l.w, r.y + (r.h - (float)l.h) * 0.5f, 0.8f * a); }
    } }
  if (cad) return;
  for (i = 0; i < n; i++) {
    int id = it[i].id;
    if (id == B_PRINC) {
      // A acao concreta: foco nasce nela (cheia no acento); sem foco, mantem
      // o anel no acento no modo solido (no vidro, botao_superficie ja poe).
      botao_pilula(it[i].r, rotulo_acao(pPrim, pPrimF), icone_acao(pPrim, pPrimF), foco == id ? 1.0f : 0.0f, 1, 0, a);
      if (foco != id && !ajustes_vidro()) {
        float ar, ag, ab;
        ajustes_acento(&ar, &ag, &ab);
        gfx_anel(it[i].r, 0.5f, 2.0f, ar, ag, ab, a);
      }
      alvo(it[i].r, id);
    }
    else if (id == B_SEC) { botao_pilula(it[i].r, rotulo_acao(pSec, pSecF), icone_acao(pSec, pSecF), foco == id ? 1.0f : 0.0f, 0, 0, a); alvo(it[i].r, id); }
    else if (id == B_ENVIAR) chip(&it[i], id, i18n("Enviar um registro"), "aj_send", a);
    else if (id == B_OUTRO) chip(&it[i], id, i18n("Testar outro problema"), NULL, a);
  }
  { float cy = oy + PAINEL_Y + painel_h() + 28.0f;
    txt_em(TXT_V2_26, i18n("Não resolveu?"), ox + PAINEL_X, cy + 12.0f, 170.0f, 150, 156, 168, a); }
}

static const char *motivo_envio(char *buf, size_t tam) {
  switch (envMotivo) {
    case AVISOS_ENVIO_OFFLINE: return i18n("A TV está sem internet. Tente de novo quando a rede voltar.");
    case AVISOS_ENVIO_SERVIDOR:
      snprintf(buf, tam, i18n("O servidor recusou o registro (erro %d)."), envHttp);
      return buf;
    case AVISOS_ENVIO_PRAZO: return i18n("O servidor demorou demais. Tente de novo.");
    case AVISOS_ENVIO_CONEXAO: return i18n("A conexão caiu no meio do envio.");
    case AVISOS_ENVIO_CONTA: return i18n("Falta entrar na conta para enviar o registro.");
    case AVISOS_ENVIO_INDISPONIVEL: return i18n("Esta versão não tem servidor de registros.");
  }
  return i18n("O envio não deu certo.");
}

static void desenha_envio(float a) {
  if (farolenvioui_ativo()) {
    farolenvioui_desenhar(ox, oy, a);
    return;
  }
  static const char *const VAI[2] = { "Os últimos 200 KB do registro desta sessão", "Endereços cortados no host" };
  static const char *const NAO[2] = { "Senhas e chaves", "O caminho completo dos endereços" };
  Item it[MAX_ITENS];
  char kick[160], buf[160], cod[16];
  const char *tit, *sub;
  int n = montar(it), i;
  float x = ox + MARGEM;
  snprintf(kick, sizeof kick, "%s › %s", i18n(FT_NOME), i18n("Registro"));
  if (envModo == M_ENVIANDO) { tit = i18n("Enviando o registro"); sub = i18n("Isto leva alguns segundos. Dá para voltar: o código entra na lista quando chegar."); }
  else if (envModo == M_ENVIADO) {
    tit = envCodigo[0] ? i18n("Registro enviado") : i18n("Registro recebido");
    sub = envCodigo[0] ? i18n("Diga este código no Discord ou na issue do GitHub. Quem ajuda abre o registro por ele.")
                       : i18n("O servidor recebeu, mas não devolveu um código para citar.");
  } else if (envModo == M_FALHOU) { tit = i18n("Não deu para enviar"); sub = motivo_envio(buf, sizeof buf); }
  else {
    tit = i18n("Registros enviados");
    sub = i18n("Ficam 30 dias nesta TV e depois somem sozinhos. Cite o código quando pedir ajuda.");
  }
  cabecalho(kick, tit, sub, a);
  // Coluna da esquerda: o codigo (ou o estado).
  if (envModo == M_ENVIADO && envCodigo[0] && resolverregra_codigo_exibir(envCodigo, cod, sizeof cod)) {
    char cap[200], ha[40];
    txt_tracking(TXT_NOV_TITULO, cod, 248, 249, 252, x, oy + 330.0f, a, 6.0f);
    texto_ha(0, ha, sizeof ha);
    snprintf(cap, sizeof cap, "%s · %s", sint_titulo(envSint), i18n("enviado agora · some sozinho em 30 dias"));
    txt_bloco(TXT_V2_26, cap, 186, 192, 204, x, oy + 410.0f, 760.0f, 32.0f, 0.95f * a, 2);
  } else if (envModo == M_ENVIANDO) {
    txt_em(TXT_ILHA_TITULO, i18n("Enviando…"), x, oy + 340.0f, 760.0f, 248, 249, 252, a);
  } else if (envModo == M_LISTA) {
    snprintf(buf, sizeof buf, i18n("%d registros nesta TV"), envN);
    txt_em(TXT_ILHA_TITULO, envN ? buf : i18n("Nenhum registro enviado ainda."), x, oy + 340.0f, 760.0f, 248, 249, 252, a);
  }
  // O que vai e o que nao vai.
  { float px = ox + 900.0f, py = oy + 290.0f, pw = 900.0f, ph = 282.0f, colw = 380.0f;
    painel_fundo((GfxRect){ px, py, pw, ph }, 32.0f, a);
    txt_em(TXT_CAPTION, i18n("Vai junto"), px + 36.0f, py + 30.0f, colw, 150, 156, 168, a);
    txt_em(TXT_CAPTION, i18n("Não vai"), px + 36.0f + colw + 36.0f, py + 30.0f, colw, 150, 156, 168, a);
    for (i = 0; i < 2; i++) {
      float ry = py + 84.0f + (float)i * 90.0f, ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      gfx_icone((GfxRect){ px + 36.0f, ry + 2.0f, 24.0f, 24.0f }, "aj_check", ar, ag, ab, a);
      txt_bloco(TXT_V2_26, i18n(VAI[i]), 220, 224, 232, px + 36.0f + 38.0f, ry, colw - 38.0f, 30.0f, a, 2);
      gfx_icone((GfxRect){ px + 36.0f + colw + 36.0f, ry + 2.0f, 24.0f, 24.0f }, "aj_x", 0.6f, 0.62f, 0.66f, a);
      txt_bloco(TXT_V2_26, i18n(NAO[i]), 170, 176, 188, px + 36.0f + colw + 36.0f + 38.0f, ry, colw - 38.0f, 30.0f, a, 2);
    } }
  // Botoes e linhas.
  for (i = 0; i < n; i++) {
    int id = it[i].id;
    if (id == B_PRONTO || id == B_REPETIR) {
      int prim = envModo == M_FALHOU ? id == B_REPETIR : 1;
      const char *rot = id == B_REPETIR ? i18n("Tentar de novo") : (envModo == M_ENVIADO ? i18n("Pronto") : i18n("Voltar"));
      botao_pilula(it[i].r, rot, id == B_REPETIR ? "aj_rotate-cw" : NULL, foco == id ? 1.0f : 0.0f, prim, 0, a);
      alvo(it[i].r, id);
    }
  }
  // "Mandar o codigo pro celular": ainda nao existe, aparece apagado.
  if (envModo == M_ENVIADO && envCodigo[0]) {
    TxtLinha l = txt_linha(TXT_CAPTION, i18n("Mandar o código pro celular · em breve"), 150, 156, 168, 255);
    txt_desenhar_alpha(l, x, oy + 604.0f, 0.8f * a);
  }
  // Lista dos enviados.
  { float ly = oy + 724.0f;
    txt_em(TXT_CAPTION, i18n("Registros enviados"), x + 22.0f, ly, 600.0f, 150, 156, 168, a);
    gfx_cor((GfxRect){ x, ly + 36.0f, 1680.0f, 1.0f }, 0, 1, 1, 1, 0.08f * a);
    if (envN == 0) txt_em(TXT_V2_26, i18n("Nenhum registro enviado ainda."), x + 22.0f, ly + 62.0f, 800.0f, 150, 156, 168, a);
    for (i = 0; i < n; i++) {
      GfxRect r = it[i].r;
      int id = it[i].id, f = foco == id, k = id - B_LIN0;
      Tinta p, s;
      char disp[16], ha[40], dir[96];
      if (id < B_LIN0 || k < 0 || k >= envN) continue;
      superficie(r, 20.0f, f, a, &p, &s);
      if (!resolverregra_codigo_exibir(env[k].codigo, disp, sizeof disp)) snprintf(disp, sizeof disp, "%s", env[k].codigo);
      texto_ha((long)time(NULL) - env[k].quando, ha, sizeof ha);
      snprintf(dir, sizeof dir, "%s · %s", ha, i18n("apagar"));
      txt_em(TXT_ILHA_NOME, disp, r.x + 22.0f, r.y + (r.h - 30.0f) * 0.5f, 260.0f, p.r, p.g, p.b, a);
      txt_em(TXT_V2_26, sint_titulo(env[k].sintoma), r.x + 310.0f, r.y + (r.h - 30.0f) * 0.5f, r.w - 310.0f - 300.0f, s.r, s.g, s.b, a);
      { TxtLinha l = txt_linha(TXT_CAPTION, dir, s.r, s.g, s.b, 255);
        txt_desenhar_alpha(l, r.x + r.w - 22.0f - (float)l.w, r.y + (r.h - (float)l.h) * 0.5f, a); }
      alvo(r, id);
    }
    if (envN > VISIVEIS) {
      char mais[64];
      snprintf(mais, sizeof mais, i18n("%d de %d"), rolo + envio_visiveis(), envN);
      txt_em(TXT_CAPTION, mais, x + 1680.0f - 200.0f, ly, 200.0f, 150, 156, 168, a);
    } }
}

// Fator da tela: o tamanho da interface se a grade 1920x1080 cabe; senao encolhe
// ate caber (celular em retrato), nunca amplia.
float resolver_escala(void) {
  float ui = gfx_escala_ui(), fw = NV_LAYOUT_REAL_W / Q_W, fh = NV_LAYOUT_REAL_H / Q_H;
  float fit = fw < fh ? fw : fh;
  if (Q_W * ui <= NV_LAYOUT_REAL_W - 32.0f && Q_H * ui <= NV_LAYOUT_REAL_H - 32.0f) return ui;
  return fit < 1.0f ? fit : 1.0f;
}

void resolver_desenhar(Uint32 agora) {
  float a = anim_suave(entrada);
  if (entrada < 0.002f) return;
  float escAnt = gfx_escala();
  gfx_escala_sair(resolver_escala());
  origem();
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0.051f, 0.051f, 0.051f, entrada);
  if (farolconsentui_aberto()) {
    farolconsentui_desenhar(ox, oy, a);
    gfx_escala_sair(escAnt);
    return;
  }
  if (farolvozui_ativo()) farolvozui_desenhar(ox, oy, a, agora);
  else if (fase == RESOLVER_FASE_SINTOMA) desenha_sintoma(a);
  else if (fase == RESOLVER_FASE_CADEIA || fase == RESOLVER_FASE_VEREDITO) desenha_testes(a);
  else if (fase == RESOLVER_FASE_ACHAR) farolachaui_desenhar(ox, oy, a, agora);
  else if (fase == RESOLVER_FASE_ESCOLHA) farolescolhaui_desenhar(ox, oy, a, agora);
  else desenha_envio(a);
  gfx_escala_sair(escAnt);
}
