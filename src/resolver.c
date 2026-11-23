// RESOLVER UM PROBLEMA. Ver resolver.h.
//
// TRES FASES na mesma tela: 0 = escolher o sintoma, 1 = os testes e o painel do
// que foi achado, 2 = o envio do registro (codigo, o que vai e a lista dos
// enviados). O FOCO E GEOMETRICO como em comecar.c: cada fase monta uma lista de
// retangulos (montar) e CIMA/BAIXO/ESQUERDA/DIREITA vao para o mais proximo na
// direcao. Desenho, foco por teclado e alvos do ponteiro usam a mesma lista.
#include "resolver.h"
#include "resolverregra.h"
#include "farolanel.h"
#include "farolregra.h"
#include "faroltexto.h"
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

// Ids de foco. Os cartoes de sintoma sao 0..RES_S_N-1; o resto vale por fase.
enum { B_BANNER = 50, B_ENVIAR, B_LISTA, B_PRINC, B_SEC, B_OUTRO, B_PRONTO, B_REPETIR, B_LIN0 = 200 };
// O que um botao do painel faz. A_DEBRID = login da conta de cache (a tela/QR
// do addonguia); A_PLAYER_EXT = a fonte falha sai para o player externo.
enum { A_NADA = 0, A_REPETIR, A_FONTES, A_ENVIAR, A_DEBRID, A_PLAYER_EXT };
// O que a fase 2 mostra.
enum { M_NADA = 0, M_ENVIANDO, M_ENVIADO, M_FALHOU, M_LISTA };

typedef struct { int id; GfxRect r; } Item;

static int   aberto, fase, foco, sint, pedido, voltaFase;
static float entrada;
static ResolverFatos fatos;
static ResolverResultado res;
static ResolverFalha falha;
static int   temFalha;
static uint64_t diagTentativa, pedidoTentativa;
static int   diagUsaTentativa;
// O motor de playbooks: os fatos dele saem do app e do anel de eventos;
// `vered` e o que ele decidiu na ultima chamada de testar().
static FarolFatos ff;
static FarolVeredito vered;
// Painel da fase 1 (montado por preparar()).
static char  pTitulo[200], pCorpo[640], pMarca[64];
static int   pAcheou, pPrim, pSec, pPrimF, pSecF, pTvAviso;
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
void resolver_teste_fatos_farol(const FarolFatos *f) { usaFFixos = f != NULL; if (f) ffFixos = *f; }
int          resolver_teste_veredito(void) { return vered.regra ? vered.regra->codigo : -1; }
const int   *resolver_teste_estados(void) { return vered.estado; }
const char  *resolver_teste_titulo(void) { return pTitulo; }
const char  *resolver_teste_corpo(void) { return pCorpo; }
int          resolver_teste_prim(void) { return pPrimF; }
int          resolver_teste_sec(void) { return pSecF; }
#endif

int resolver_aberto(void) { return aberto; }
int resolver_fase(void) { return fase; }
int resolver_foco(void) { return foco; }
int resolver_sintoma(void) { return sint; }
int resolver_enviados_n(void) { return envN; }
void resolver_teste_esquecer(void) {
  aberto = 0; entrada = 0.0f; fase = 0; foco = 0; envN = 0; envModo = 0; envNoAr = 0; rolo = 0; temFalha = 0;
  diagTentativa = 0; pedidoTentativa = 0; diagUsaTentativa = 0;
  memset(&fatos, 0, sizeof fatos);
  memset(&vered, 0, sizeof vered);
#ifdef RESOLVER_TESTE
  usaFixos = 0; usaFFixos = 0;
#endif
}

int resolver_pedido(void) {
  int p = pedido;
  pedido = RESOLVER_PEDIU_NADA;
  return p;
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
// A leitura do motor de playbooks: o que a cadeia antiga ja tem (fatos) mais o
// que so o Farol ve — o erro nativo normalizado, a idade dele e as contagens
// do anel de eventos. -1 = nao sei (nenhum predicado casa com desconhecido).
static int os_maior(void) {
#if defined(NV_TPK)
  return video_tpk_tizen_maior();
#elif defined(NV_ANDROID) || defined(__EMSCRIPTEN__) || defined(NV_DESKTOP_MPV) || defined(RESOLVER_TESTE)
  return -1;
#else
  return nv_webos_major();
#endif
}

static int anel_pronto_apos_erro(const FarolEvento *ev, int n) {
  int i, iPronto = -1, iErro = -1;
  for (i = 0; i < n; i++) {
    if (ev[i].ev == FE_PRONTO) iPronto = i;
    else if (ev[i].ev == FE_ERRO_FONTE || ev[i].ev == FE_RECUSA_SINC) iErro = i;
  }
  return iPronto >= 0 && (iErro < 0 || iPronto > iErro);
}

static void farol_fatos(void) {
  FarolEvento ev[FAROL_ANEL_N];
  const Stream *ft;
  char buf[96];
  int i, n, iPronto = -1, iBusca = -1, iBuscaAnt = -1, lentos = 0;
  uint32_t agoraMs = SDL_GetTicks();
  long agora = (long)time(NULL);
  diagTentativa = 0;
  diagUsaTentativa = 0;
#ifdef RESOLVER_TESTE
  if (usaFFixos) { ff = ffFixos; return; }
#endif
  memset(&ff, 0, sizeof ff);
#if defined(NV_ANDROID)
  ff.plat = FP_ANDROID;
#elif defined(NV_TPK)
  ff.plat = FP_TIZEN_TPK;
#elif defined(__EMSCRIPTEN__)
  ff.plat = FP_TIZEN_WGT;
#elif defined(NV_DESKTOP_MPV)
  ff.plat = FP_OUTRA;
#else
  ff.plat = FP_WEBOS;
#endif
  ff.osMaior = os_maior();
  ff.offline = fatos.offline;
  ff.addonsQtd = fatos.addonsQtd;
  ff.addonsMotivo = fatos.addonsMotivo[0] ? 1 : 0;
  ff.debridSemPlano = debrid_sem_plano() ? 1 : 0;
  ff.debridRecusa = debrid_recusa(buf, sizeof buf) ? 1 : 0;
  ff.tvNegou = video_registro_negado() ? 1 : 0;
  ff.haFalha = temFalha && agora >= falha.agora && agora - falha.agora <= RES_REJEITA_S;
  // O erro normalizado fica com o player; so vale na janela da falha recente.
  ff.erroIdadeS = -1;
  ff.fonteDebrid = -1;
  if (ff.haFalha && player_ultimo_erro_farol(&ff.erro, &ff.fonteDebrid)) {
    ff.erroIdadeS = (int)(agora - falha.agora);
    diagTentativa = player_ultimo_erro_tentativa();
  }
  diagUsaTentativa = 1;
  // Anel: falhas seguidas = erros de fonte desde o ultimo primeiro quadro;
  // paradas e recusas de audio contam na janela; a ultima busca de fontes da
  // os add-ons mudos e, varrendo a busca anterior a ela, os lentos.
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  for (i = 0; i < n; i++) {
    if (ev[i].ev == FE_PRONTO) iPronto = i;
    else if (ev[i].ev == FE_ADDON_BUSCA) { iBuscaAnt = iBusca; iBusca = i; }
  }
  ff.falhasSeguidas = 0;
  for (i = iPronto + 1; i < n; i++) {
    // Recusa sincrona emite FE_RECUSA_SINC seguido de FE_ERRO_FONTE no mesmo
    // erro: contar os dois dobrava uma unica falha para TV_FORMATO_REPETE.
    if (ev[i].ev == FE_ERRO_FONTE) ff.falhasSeguidas++;
    else if (ev[i].ev == FE_RECUSA_SINC &&
             !(i + 1 < n && ev[i + 1].ev == FE_ERRO_FONTE))
      ff.falhasSeguidas++;
  }
  ff.paradas5min = farolanel_contar(FE_PARADO, 5u * 60u * 1000u, agoraMs);
  ff.audioRecusado = farolanel_contar(FE_AUDIO_RECUSADO, (uint32_t)RES_REJEITA_S * 1000u, agoraMs);
  ff.faixaDts = -1;
  ff.conversaoPossivel = (ff.plat == FP_WEBOS || ff.plat == FP_TIZEN_TPK) ? 1 : -1;
  ff.outraFaixa = -1;
  ft = stream_atual() >= 0 ? stream_item(stream_atual()) : NULL;
  if (ft) {
    int kbps = stream_kbps_atual();
    ff.fonteDv = ft->dolbyVision > 0 ? 1 : 0;
    uint64_t tamanhoGBx10 = ft->tamanhoBytes / 100000000ULL;
    ff.fonteGBx10 = !ft->tamanhoBytes ? -1 : tamanhoGBx10 > INT_MAX ? INT_MAX : (int)tamanhoGBx10;
    ff.fonteMbps = kbps > 0 ? kbps / 1000 : -1;
    if (ff.fonteDebrid < 0) ff.fonteDebrid = ft->debridServico > 0 ? 1 : 0;
  } else {
    ff.fonteDv = -1; ff.fonteGBx10 = -1; ff.fonteMbps = -1;
  }
  ff.fonteHdr = -1; ff.tvDv = -1; ff.tvHdr = -1;
  ff.vazaoMbps = -1;
  ff.memLivreMB = (int)memlog_livre_mb();
  ff.temOutraFonte = ff.temFonteMenor = ff.temFonteSemDv = ff.temFonteSemDts = -1;
  ff.sondaRange = -1;
  ff.versaoNova = -1;
  ff.addonsLentos = -1;
  ff.addonsMudos = -1;
  if (iBusca >= 0) {
    ff.addonsLentos = 0;
    ff.addonsMudos = ev[iBusca].b;
    for (i = iBuscaAnt + 1; i < iBusca; i++)
      if (ev[i].ev == FE_ADDON_LENTO && !ev[i].b) lentos++;
    ff.addonsLentos = lentos;
  }
#ifdef NV_ANDROID
  ff.externoDisponivel = (!anel_pronto_apos_erro(ev, n) && diagTentativa &&
                          player_externo_disponivel_tentativa(diagTentativa)) ? 1 : 0;
#else
  ff.externoDisponivel = 0;
#endif
}

// ------------------------------------------------------------------- textos
static const struct { const char *tit, *desc, *icone; } SINT[RES_S_N] = {
  { "O vídeo não começa", "Tela preta, erro ou volta para a lista", "aj_play" },
  { "Trava ou fica carregando", "Para no meio, engasga, demora a abrir", "aj_rotate-cw" },
  { "Sem som ou som errado", "Mudo, idioma trocado, fora de sincronia", "aj_volume-2" },
  { "Legenda", "Não aparece, atrasada ou com letras quebradas", "aj_captions" },
  { "Continuar assistindo errado", "Episódio trocado, progresso que volta", "aj_clock" },
  { "Contas e sincronização", "Trakt, Simkl ou Conta Nuvio não entram", "aj_users" },
};
static const struct { const char *nome, *icone; } TESTE[RES_T_N] = {
  { "Internet", "aj_wifi" }, { "Add-ons", "aj_puzzle" }, { "Debrid", "aj_key" },
  { "Fonte", "aj_link" }, { "Esta TV", "aj_tv-minimal-play" }, { "Áudio", "aj_volume-2" },
};

static const char *sint_titulo(int s) {
  return s >= 0 && s < RES_S_N ? i18n(SINT[s].tit) : i18n("Registro sem problema escolhido");
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
  if (st == RES_NAO_TESTADO) return i18n("Não testado: um teste antes falhou.");
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
    case FAROL_A_PLAYER_EXTERNO:    return ff.externoDisponivel > 0 ? A_PLAYER_EXT : A_NADA;
    case FAROL_A_ENVIAR_REGISTRO:   return A_ENVIAR;
    default:                        return A_NADA;
  }
}

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

// O botao secundario: a alternativa com executor; sem ela, "Enviar um
// registro" quando a regra escala JUNTO; senao "Tentar de novo" como nos
// outros cartoes. FAROL_A_* efetivo na devolucao, A_* em `*a`.
static int acao_secundaria(const FarolRegra *r, int primF, int *a) {
  int m;
  if (r->acaoAlt != primF && (m = acao_farol_map(r->acaoAlt)) != A_NADA) {
    *a = m; return r->acaoAlt;
  }
  if (r->escala == FAROL_ESC_JUNTO && primF != FAROL_A_ENVIAR_REGISTRO) {
    *a = A_ENVIAR; return FAROL_A_ENVIAR_REGISTRO;
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
  pAcheou = 0; pSec = A_NADA; pPrimF = pSecF = 0; pTvAviso = 0;
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

static const char *icone_acao(int a) {
  switch (a) {
    case A_REPETIR:    return "aj_rotate-cw";
    case A_FONTES:     return "aj_puzzle";
    case A_ENVIAR:     return "aj_send";
    case A_DEBRID:     return "aj_key";
    case A_PLAYER_EXT: return "aj_external-link";
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
static float grade_y(void) { return tem_banner() ? 440.0f : 316.0f; }

static int envio_visiveis(void) { return envN - rolo < VISIVEIS ? envN - rolo : VISIVEIS; }

static int montar(Item *it) {
  int n = 0, i;
  origem();
  if (fase == RESOLVER_FASE_SINTOMA) {
    float w = (1680.0f - 2.0f * 24.0f) / 3.0f, y0 = grade_y();
    char lista[96];
    if (tem_banner()) poe(it, &n, B_BANNER, MARGEM, 304.0f, 1680.0f, 108.0f);
    for (i = 0; i < RES_S_N; i++)
      poe(it, &n, i, MARGEM + (float)(i % 3) * (w + 24.0f), y0 + (float)(i / 3) * 232.0f, w, 208.0f);
    snprintf(lista, sizeof lista, i18n("Registros enviados · %d"), envN);
    poe_botao(it, &n, B_ENVIAR, MARGEM, 944.0f, i18n("Enviar um registro"), "aj_send", 0);
    poe_botao(it, &n, B_LISTA, MARGEM + botao_largura(i18n("Enviar um registro"), "aj_send", 0) + BOTAO_GAP, 944.0f, lista, NULL, 0);
  } else if (fase == RESOLVER_FASE_TESTES) {
    float by, x = PAINEL_X + PAINEL_PAD, cy;
    medir_painel();
    by = painel_botoes_y();
    cy = PAINEL_Y + painel_h() + 28.0f;
    poe_botao(it, &n, B_PRINC, x, by, rotulo_acao(pPrim, pPrimF), icone_acao(pPrim), 1);
    if (pSec) poe_botao(it, &n, B_SEC, x + botao_largura(rotulo_acao(pPrim, pPrimF), icone_acao(pPrim), 1) + BOTAO_GAP, by + 8.0f,
                        rotulo_acao(pSec, pSecF), icone_acao(pSec), 0);
    x = PAINEL_X + 190.0f;
    if (pPrim != A_ENVIAR) {
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
  if (fase == RESOLVER_FASE_SINTOMA) return tem_banner() ? B_BANNER : 0;
  if (fase == RESOLVER_FASE_TESTES) return B_PRINC;
  return B_PRONTO;
}

static void mover(int dx, int dy) {
  Item it[MAX_ITENS];
  int n = montar(it), i, cur = -1, melhor = -1;
  float cx, cy, bs = 1e9f;
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

// ------------------------------------------------------------------- fluxo
static void ir_fase(int f) {
  fase = f;
  foco = foco_padrao();
}

static void testar(int s) {
  int etapa;
  unsigned mascara = resolverregra_mascara(s);
  sint = s;
  fatos_ler();
  farol_fatos();
  farolregra_julgar(s, &ff, &vered);
  if (vered.regra->codigo == FAROL_C_NADA_ACHADO) {
    resolverregra_testar(s, &fatos, &res);
  } else {
    res.primeira = -1;
    memcpy(res.estado, vered.estado, sizeof res.estado);
    for (etapa = 0; etapa < RES_T_N; etapa++)
      res.testado[etapa] = (mascara >> etapa) & 1u;
  }
  farol_ev(FM_FAROL, FE_FAROL_VEREDITO, 0, vered.regra->codigo, 0,
           farolregra_codigo_nome(vered.regra->codigo));
  preparar();
}

static void enviar_registro(int s, int volta) {
  AvisosEnvio i;
  avisos_envio_info(&i);
  envSint = s;
  voltaFase = volta;
  envModo = M_ENVIANDO;
  envNoAr = 1;
  envInicio = i.estado == 1 ? (long)i.quando : (long)time(NULL);
  envCodigo[0] = 0;
  envMotivo = envHttp = 0;
#ifndef RESOLVER_TESTE
  avisos_enviar_registro_atual();
#endif
  rolo = 0;
  ir_fase(RESOLVER_FASE_ENVIO);
}

static void abrir(void) {
  aberto = 1;
  entrada = 0.0f;
  pedido = RESOLVER_PEDIU_NADA;
  pedidoTentativa = 0;
  diagTentativa = 0;
  diagUsaTentativa = 0;
  envModo = M_NADA;
  rolo = 0;
  enviados_ler();
  fatos_ler();
  ir_fase(RESOLVER_FASE_SINTOMA);
}

void resolver_abrir(void) { abrir(); }

static void fechar(void) { aberto = 0; }

// `fa` e a acao do veredito que gerou o botao (0 no cartao antigo): vai para
// o anel como FE_FAROL_ACAO — o resultado do conserto se mede depois.
static void aplicar(int a, int fa) {
  const char *tok = fa > 0 ? farolregra_acao_token(fa) : "";
  pedidoTentativa = 0;
  if (a == A_PLAYER_EXT && diagUsaTentativa) {
    FarolEvento ev[FAROL_ANEL_N];
    int n = farolanel_ler(ev, FAROL_ANEL_N, 0);
    if (!diagTentativa || anel_pronto_apos_erro(ev, n) ||
        !player_externo_disponivel_tentativa(diagTentativa)) {
      testar(sint);
      foco = B_PRINC;
      return;
    }
  }
  if (tok[0]) farol_ev(FM_FAROL, FE_FAROL_ACAO, 0, fa, 0, tok);
  if (a == A_REPETIR) { testar(sint); foco = B_PRINC; }
  else if (a == A_FONTES) { pedido = RESOLVER_PEDIU_FONTES; fechar(); }
  else if (a == A_DEBRID) { pedido = RESOLVER_PEDIU_DEBRID; fechar(); }
  else if (a == A_PLAYER_EXT) {
    pedidoTentativa = diagUsaTentativa ? diagTentativa : 0;
    pedido = RESOLVER_PEDIU_PLAYER_EXT;
    fechar();
  } else if (a == A_ENVIAR) enviar_registro(sint, RESOLVER_FASE_TESTES);
}

#ifdef RESOLVER_TESTE
void resolver_teste_testar(int s) { testar(s); ir_fase(RESOLVER_FASE_TESTES); }
#endif

static void ok(void) {
  if (fase == RESOLVER_FASE_SINTOMA) {
    if (foco >= 0 && foco < RES_S_N) { testar(foco); ir_fase(RESOLVER_FASE_TESTES); }
    else if (foco == B_BANNER) { testar(RES_S_NAO_COMECA); ir_fase(RESOLVER_FASE_TESTES); }
    else if (foco == B_ENVIAR) enviar_registro(RES_S_GERAL, RESOLVER_FASE_SINTOMA);
    else if (foco == B_LISTA) {
      envModo = M_LISTA; voltaFase = RESOLVER_FASE_SINTOMA; rolo = 0;
      ir_fase(RESOLVER_FASE_ENVIO);
      if (envN > 0) foco = B_LIN0;
    }
  } else if (fase == RESOLVER_FASE_TESTES) {
    if (foco == B_PRINC) aplicar(pPrim, pPrimF);
    else if (foco == B_SEC) aplicar(pSec, pSecF);
    else if (foco == B_ENVIAR) enviar_registro(sint, RESOLVER_FASE_TESTES);
    else if (foco == B_OUTRO) ir_fase(RESOLVER_FASE_SINTOMA);
  } else {
    if (foco == B_PRONTO) { int v = voltaFase; envModo = M_NADA; ir_fase(v); }
    else if (foco == B_REPETIR) enviar_registro(envSint, voltaFase);
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
  else if (fase == RESOLVER_FASE_TESTES) ir_fase(RESOLVER_FASE_SINTOMA);
  else { int v = voltaFase; envModo = M_NADA; ir_fase(v); }
}

void resolver_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT)  { mover(-1, 0); return; }
  if (k == SDLK_RIGHT) { mover(1, 0); return; }
  if (k == SDLK_UP)    { mover(0, -1); return; }
  if (k == SDLK_DOWN)  { mover(0, 1); return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { ok(); return; }
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
  if (envModo != M_ENVIANDO) return;   // a pessoa ja voltou: o codigo ficou so na lista
  envModo = i.estado == 2 ? M_ENVIADO : M_FALHOU;
  if (aberto && fase == RESOLVER_FASE_ENVIO) foco = foco_padrao();
}

void resolver_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
  (void)agora;
  sondar_envio();
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (red) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? ABRIR_MS : FECHAR_MS);
}

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
  cabecalho(i18n("Ajustes › Ajuda"), i18n("Resolver um problema"),
            i18n("Escolha o que mais parece. O Nuvio testa sozinho e diz o que achou."), a);
  for (i = 0; i < n; i++) {
    GfxRect r = it[i].r;
    int id = it[i].id, f = foco == id;
    if (id == B_BANNER) {
      char t[160], d[260];
      int hn = 0;
      long seg = (long)time(NULL) - falha.agora;
      switch (resolverregra_ha(seg, &hn)) {
        case RES_HA_AGORA: snprintf(t, sizeof t, "%s", i18n("Agora há pouco um vídeo não começou")); break;
        case RES_HA_MIN:   snprintf(t, sizeof t, i18n("Há %d min um vídeo não começou"), hn); break;
        default:           snprintf(t, sizeof t, i18n("Há %d h um vídeo não começou"), hn); break;
      }
      if (falha.midia[0]) snprintf(d, sizeof d, "%s · %s", falha.midia, falha.titulo);
      else snprintf(d, sizeof d, "%s", falha.titulo);
      superficie(r, 28.0f, f, a, &p, &s);
      bola(r.x + 26.0f, r.y + 22.0f, 64.0f, "aj_triangle-alert", p, f, a);
      txt_em(TXT_ILHA_NOME, t, r.x + 116.0f, r.y + 18.0f, r.w - 220.0f, p.r, p.g, p.b, a);
      txt_em(TXT_CAPTION, d, r.x + 116.0f, r.y + 58.0f, r.w - 220.0f, s.r, s.g, s.b, a);
      icone_t((GfxRect){ r.x + r.w - 28.0f - 26.0f, r.y + (r.h - 26.0f) * 0.5f, 26.0f, 26.0f }, "aj_chevron-right", s, a);
      alvo(r, id);
    } else if (id >= 0 && id < RES_S_N) {
      superficie(r, 28.0f, f, a, &p, &s);
      bola(r.x + 28.0f, r.y + 24.0f, 64.0f, SINT[id].icone, p, f, a);
      txt_em(TXT_ILHA_NOME, i18n(SINT[id].tit), r.x + 28.0f, r.y + 104.0f, r.w - 56.0f, p.r, p.g, p.b, a);
      txt_bloco(TXT_CAPTION, i18n(SINT[id].desc), s.r, s.g, s.b, r.x + 28.0f, r.y + 146.0f, r.w - 56.0f, 26.0f, a, 2);
      alvo(r, id);
    } else if (id == B_ENVIAR) chip(&it[i], id, i18n("Enviar um registro"), "aj_send", a);
    else if (id == B_LISTA) {
      char l[96];
      snprintf(l, sizeof l, i18n("Registros enviados · %d"), envN);
      chip(&it[i], id, l, NULL, a);
    }
  }
  txt_em(TXT_CAPTION, i18n("Nenhum teste muda seus Ajustes."), ox + 1800.0f - 560.0f, oy + 962.0f, 560.0f, 150, 156, 168, a);
}

static void linha_teste(int t, float x, float y, float w, float a) {
  char buf[96];
  const char *det = detalhe_teste(t, buf, sizeof buf);
  int st = res.estado[t], sem = !res.testado[t] || st == RES_NAO_TESTADO || (t == RES_T_AUDIO && st == RES_OK);
  GfxRect c = { x, y + 6.0f, 44.0f, 44.0f };
  Tinta cl = { 243, 242, 239 }, dim = { 120, 126, 138 };
  if (st == RES_FALHOU) {
    float ar, ag, ab, ti = ajustes_acento_tinta(&ar, &ag, &ab);
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
  txt_em(TXT_ILHA_NOME, i18n(TESTE[t].nome), x + 68.0f, y, w - 68.0f, sem ? 186 : 243, sem ? 192 : 242, sem ? 204 : 239, a);
  txt_bloco(TXT_CAPTION, det, 186, 192, 204, x + 68.0f, y + 36.0f, w - 68.0f, 26.0f, 0.95f * a, 2);
  gfx_cor((GfxRect){ x, y + 90.0f, w, 1.0f }, 0, 1, 1, 1, 0.07f * a);
}

static void desenha_testes(float a) {
  Item it[MAX_ITENS];
  char kick[160], tit[200];
  int n = montar(it), i;
  float py = PAINEL_Y;
  snprintf(kick, sizeof kick, "%s › %s", i18n("Resolver um problema"), sint_titulo(sint));
  if (sint == RES_S_NAO_COMECA && falha.midia[0] && temFalha) snprintf(tit, sizeof tit, i18n("Testamos %s"), falha.midia);
  else snprintf(tit, sizeof tit, "%s", sint_titulo(sint));
  cabecalho(kick, tit, resolverregra_mascara(sint)
            ? i18n("Só lemos o último erro e o estado de agora. Nenhum teste toca vídeo.")
            : i18n("Este problema não tem teste. Aqui vão os passos e, se precisar, o envio do registro."), a);
  if (resolverregra_mascara(sint))
    for (i = 0; i < RES_T_N; i++) linha_teste(i, ox + MARGEM, oy + 300.0f + (float)i * 96.0f, 780.0f, a);
  // O painel do que foi achado.
  { float x = ox + PAINEL_X, y = oy + py, w = PAINEL_W, h = painel_h(), ar, ag, ab;
    GfxRect pr = { x, y, w, h };
    painel_fundo(pr, 32.0f, a);
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor((GfxRect){ x + PAINEL_PAD, y + 48.0f, 14.0f, 14.0f }, 0.5f, pAcheou ? ar : 0.6f, pAcheou ? ag : 0.62f, pAcheou ? ab : 0.66f, a);
    txt_em(TXT_CAPTION, pMarca, x + PAINEL_PAD + 26.0f, y + 40.0f, 600.0f, pAcheou ? 243 : 186, pAcheou ? 242 : 192, pAcheou ? 239 : 204, a);
    txt_bloco(TXT_ILHA_TITULO, pTitulo, 248, 249, 252, x + PAINEL_PAD, y + 80.0f, w - 2 * PAINEL_PAD, 48.0f, a, 2);
    txt_bloco(TXT_V2_26, pCorpo, 200, 205, 215, x + PAINEL_PAD, oy + painel_corpo_y(), w - 2 * PAINEL_PAD, 32.0f, 0.92f * a, 6);
    if (pTvAviso) {
      GfxRect r = { x + PAINEL_PAD, oy + painel_botoes_y() + BOTAO_H_PRIMARIO + 24.0f, w - 2 * PAINEL_PAD, 76.0f };
      gfx_cor(r, 20.0f / r.h, 1, 1, 1, 0.05f * a);
      txt_em(TXT_V2_26, i18n("Esconder fontes que esta TV não toca"), r.x + 24.0f, r.y + 8.0f, r.w - 200.0f, 186, 192, 204, 0.6f * a);
      txt_em(TXT_CAPTION, i18n("Ainda não dá para ligar isto por aqui."), r.x + 24.0f, r.y + 44.0f, r.w - 200.0f, 150, 156, 168, 0.6f * a);
      { TxtLinha l = txt_linha(TXT_CAPTION, i18n("Em breve"), 186, 192, 204, 255);
        txt_desenhar_alpha(l, r.x + r.w - 24.0f - (float)l.w, r.y + (r.h - (float)l.h) * 0.5f, 0.8f * a); }
    } }
  for (i = 0; i < n; i++) {
    int id = it[i].id;
    if (id == B_PRINC) { botao_pilula(it[i].r, rotulo_acao(pPrim, pPrimF), icone_acao(pPrim), foco == id ? 1.0f : 0.0f, 1, 0, a); alvo(it[i].r, id); }
    else if (id == B_SEC) { botao_pilula(it[i].r, rotulo_acao(pSec, pSecF), icone_acao(pSec), foco == id ? 1.0f : 0.0f, 0, 0, a); alvo(it[i].r, id); }
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
  static const char *const VAI[2] = { "Os últimos 200 KB do registro desta sessão", "Endereços cortados no host" };
  static const char *const NAO[2] = { "Senhas e chaves", "O caminho completo dos endereços" };
  Item it[MAX_ITENS];
  char kick[160], buf[160], cod[16];
  const char *tit, *sub;
  int n = montar(it), i;
  float x = ox + MARGEM;
  snprintf(kick, sizeof kick, "%s › %s", i18n("Resolver um problema"), i18n("Registro"));
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
  (void)agora;
  if (entrada < 0.002f) return;
  float escAnt = gfx_escala();
  gfx_escala_sair(resolver_escala());
  origem();
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0.051f, 0.051f, 0.051f, entrada);
  if (fase == RESOLVER_FASE_SINTOMA) desenha_sintoma(a);
  else if (fase == RESOLVER_FASE_TESTES) desenha_testes(a);
  else desenha_envio(a);
  gfx_escala_sair(escAnt);
}
