// FAROL — telas 4, 5, 6 e estados §3.3 do envio do registro (issue 22).
// Zero _Thread_local; zero log com URL/token/titulo/perfil.
#include "farolenvioui.h"
#include "farolvai.h"
#include "farol.h"
#include "farolpacote.h"
#include "farolperfil.h"
#include "faroltexto.h"
#include "resolver.h"
#include "resolverregra.h"
#include "addons.h"
#include "ajustes.h"
#include "atualizacao.h"
#include "botoes.h"
#include "debrid.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "nvbackend.h"
#include "ponteiro.h"
#include "qr.h"
#include "qrtex.h"
#include "redesaude.h"
#include "simkl.h"
#include "streams.h"
#include "text.h"
#include "trakt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FEUI_MARGEM   120.0f
#define FEUI_MAX_IT    12
#define FEUI_RESP_MS  1600.0f
#define FEUI_QR_LADO  360.0f

/* Ganchos fracos para testes unitarios sem o app inteiro linkado;
 * na build do app, as definicoes fortes dos modulos reais vencem. */
__attribute__((weak)) size_t nv_rec_url_copiar(char *dst, size_t tam) {
  if (dst && tam) dst[0] = '\0';
  return 0;
}
__attribute__((weak)) const char *atualizacao_nova(void) { return ""; }
__attribute__((weak)) void atualizacao_abrir(void) {}
__attribute__((weak)) void atualizacao_procurar_agora(void) {}
__attribute__((weak)) int  rede_saude_offline(void) { return 0; }
__attribute__((weak)) int  addons_n(void) { return 0; }
__attribute__((weak)) int  addons_ativo(int i) { (void)i; return 0; }
__attribute__((weak)) int  addons_fornece(int i, int oque) { (void)i; (void)oque; return 0; }
__attribute__((weak)) const char *debrid_servico_id(int s) { (void)s; return ""; }
__attribute__((weak)) const char *debrid_chave_mascarada(const char *s, char *d, unsigned n) {
  (void)s; if (d && n) d[0] = '\0'; return d;
}
__attribute__((weak)) int  trakt_ativo(void) { return 0; }
__attribute__((weak)) int  simkl_ativo(void) { return 0; }
__attribute__((weak)) int  stream_atual(void) { return -1; }
__attribute__((weak)) const Stream *stream_item(int i) { (void)i; return NULL; }
__attribute__((weak)) int  ajustes_idioma_ingles(void) { return 0; }

typedef struct { int id; GfxRect r; } FeuiItem;
typedef struct { int r, g, b; } FeuiTinta;

static FarolEnvioUiTela telaAtual = FEUI_FECHADO;
static int              focoAtual = FEUI_B_MANDAR;
static int              incluirLog;
static int              envIniciado;
static int              fecharPendente, voltarPendente, pedidoPendente;
static int              recarregarPendente, doFarolPendente, doFarolOkPendente = -1;
static int              envioPendenteSubmeter;
static int              temUltGeracao;
static uint64_t         ultGeracao;
static float            respiraMs;

/* Contexto capturado ao abrir a fase de envio */
static int              ctxSintoma = RES_S_GERAL;
static int              ctxVolta = RESOLVER_FASE_SINTOMA;
static int              ctxDoFarol;
static FarolVeredito    ctxVered;
static int              ctxTemVered;
static FarolFatos       ctxFatos;
static int              ctxEstados[RES_T_N];
static int              ctxTemEstados;
static char             ctxUltCodigo[8];

/* Ultimo snapshot de farolenvio_info() para tela/QR */
static FarolEnvioInfo   envInfo;
static GLuint           texQr;
static char             qrUrl[256];
static char             qrUrlTex[256];
static char             urlOverride[384];
static int              temUrlOverride;

/* ------------------------------------------------------------------- QR --- */
static void liberar_qr(void) {
  if (texQr) {
    gfx_tex_esquecer(texQr);
    glDeleteTextures(1, &texQr);
    texQr = 0;
  }
  qrUrlTex[0] = '\0';
}

static void obter_base_url(char *dst, size_t cap) {
  size_t n;
  if (!dst || !cap) return;
  dst[0] = '\0';
  if (temUrlOverride) snprintf(dst, cap, "%s", urlOverride);
  else nv_rec_url_copiar(dst, cap);
  n = strlen(dst);
  while (n > 0 && dst[n - 1] == '/') dst[--n] = '\0';
}

static void atualizar_qr_para(const char *codigo) {
  char base[384];
  size_t i;
  if (!codigo || strlen(codigo) != 6) {
    qrUrl[0] = '\0';
    liberar_qr();
    return;
  }
  for (i = 0; i < 6; i++) {
    char c = codigo[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))) {
      qrUrl[0] = '\0';
      liberar_qr();
      return;
    }
  }
  obter_base_url(base, sizeof base);
  if (!base[0]) {
    qrUrl[0] = '\0';
    liberar_qr();
    return;
  }
  if (snprintf(qrUrl, sizeof qrUrl, "%s/v1/farol/r/%s", base, codigo) >= (int)sizeof qrUrl) {
    qrUrl[0] = '\0';
    liberar_qr();
    return;
  }
  if (strcmp(qrUrlTex, qrUrl) != 0) {
    liberar_qr();
    if (qr_textura(&texQr, qrUrl)) snprintf(qrUrlTex, sizeof qrUrlTex, "%s", qrUrl);
    else texQr = 0;
  }
}

/* ------------------------------------------------------- estado / transicao */
static int foco_padrao_para(FarolEnvioUiTela t) {
  switch (t) {
    case FEUI_TELA4_CONFIRMA:  return FEUI_B_MANDAR;
    case FEUI_ENVIANDO:        return FEUI_B_VOLTAR;
    case FEUI_TELA5_ENVIADO:   return FEUI_B_PRONTO;
    case FEUI_TELA6_CORRIGIDO: return FEUI_B_ATUALIZAR;
    case FEUI_PENDENTE:        return FEUI_B_PRONTO;
    case FEUI_FALHOU_RECUSOU:  return FEUI_B_TENTAR;
    case FEUI_FALHOU_429:      return FEUI_B_FECHAR;
    case FEUI_FALHOU_SEM_CONTA:return FEUI_B_CONTAS;
    default:                   return FEUI_B_PRONTO;
  }
}

static FarolEnvioUiTela tela_de_info(const FarolEnvioInfo *inf) {
  switch (inf->estado) {
    case FAROLENVIO_ENVIANDO:     return FEUI_ENVIANDO;
    case FAROLENVIO_OK:           return FEUI_TELA5_ENVIADO;
    case FAROLENVIO_JA_CORRIGIDO: return FEUI_TELA6_CORRIGIDO;
    case FAROLENVIO_PENDENTE:     return FEUI_PENDENTE;
    case FAROLENVIO_FALHOU:
      if (inf->motivo == FAROLENVIO_M_LIMITE) return FEUI_FALHOU_429;
      if (inf->motivo == FAROLENVIO_M_CONTA)  return FEUI_FALHOU_SEM_CONTA;
      return FEUI_FALHOU_RECUSOU;
    default:
      return FEUI_TELA4_CONFIRMA;
  }
}

static void ir_tela(FarolEnvioUiTela t) {
  if (t != FEUI_TELA5_ENVIADO) {
    liberar_qr();
    qrUrl[0] = '\0';
  }
  telaAtual = t;
  focoAtual = foco_padrao_para(t);
  if (t == FEUI_TELA5_ENVIADO) atualizar_qr_para(envInfo.codigo);
}

/* -------------------------------------------------------- montar pacote --- */
static const char *map_container(const char *s) {
  static const char *const OK[] = { "mkv", "mp4", "ts", "hls", "dash", "avi", "webm", "outro" };
  size_t i;
  if (!s || !*s) return "x";
  for (i = 0; i < sizeof OK / sizeof *OK; i++)
    if (strcmp(s, OK[i]) == 0) return OK[i];
  return "outro";
}

static void montar_perfil_envio(FarolPerfil *p) {
  int i, nAdd, ativos = 0;
  unsigned tipos = 0, debMask = 0, contas = 0;
  char masc[32];
  farolperfil_obter(p);

  p->aparelho.rede.offline = rede_saude_offline() ? 1 : 0;
  p->cliente.ajustes.glass_ui = ajustes_vidro() ? 1 : 0;

  nAdd = addons_n();
  if (nAdd < 0) nAdd = 0;
  for (i = 0; i < nAdd; i++) {
    if (addons_ativo(i)) ativos++;
    if (addons_fornece(i, ADD_CATALOGO)) tipos |= FAROL_ADDON_CATALOGO;
    if (addons_fornece(i, ADD_STREAM))   tipos |= FAROL_ADDON_FONTES;
    if (addons_fornece(i, ADD_LEGENDA))  tipos |= FAROL_ADDON_LEGENDAS;
    if (addons_fornece(i, ADD_META))     tipos |= FAROL_ADDON_META;
  }
  p->cliente.addons.total = nAdd;
  p->cliente.addons.ativos = ativos;
  if (p->cliente.addons.isolados < 0) p->cliente.addons.isolados = 0;
  p->cliente.addons.tipos_mask = tipos;

  for (i = 0; i < DEBRID_N; i++) {
    const char *id = debrid_servico_id(i);
    if (!id || !id[0]) continue;
    masc[0] = '\0';
    debrid_chave_mascarada(id, masc, sizeof masc);
    if (!masc[0]) continue;
    if (strcmp(id, "realdebrid") == 0 || strcmp(id, "real-debrid") == 0) debMask |= FAROL_DEBRID_REALDEBRID;
    else if (strcmp(id, "torbox") == 0) debMask |= FAROL_DEBRID_TORBOX;
    else if (strcmp(id, "premiumize") == 0) debMask |= FAROL_DEBRID_PREMIUMIZE;
    else if (strcmp(id, "alldebrid") == 0 || strcmp(id, "all-debrid") == 0) debMask |= FAROL_DEBRID_ALLDEBRID;
  }
  p->cliente.debrid_mask = debMask;

  if (trakt_ativo()) contas |= FAROL_CONTA_TRAKT;
  if (simkl_ativo()) contas |= FAROL_CONTA_SIMKL;
  p->cliente.contas_mask = contas;
}

static void disparar_envio(void) {
  FarolPacoteEntrada in;
  FarolPerfil perfil;
  FarolEnvioOpcoes op;
  const Stream *st;
  int i;

  farolpacote_entrada_padrao(&in);
  if (ctxSintoma == RES_S_GERAL) in.gatilho = FAROL_G_MANUAL;
  else if (ctxFatos.falhasSeguidas >= 2) in.gatilho = FAROL_G_FALHA_REPETIDA;
  else in.gatilho = FAROL_G_FALHA_SEM_CONSERTO;

  in.sintoma = farolpacote_sintoma_de_resolver(ctxSintoma);
  if (ctxTemVered && ctxVered.regra) {
    in.codigo = farolregra_codigo_nome((int)ctxVered.regra->codigo);
    if (ctxVered.regra->codigo != FAROL_C_NADA_ACHADO &&
        ctxVered.regra->estagio < RES_T_N)
      in.estagio = (int)ctxVered.regra->estagio;
  }
  if (ctxFatos.haFalha) in.erro = ctxFatos.erro;

  if (ctxSintoma != RES_S_GERAL && resolverregra_mascara(ctxSintoma) != 0 && ctxTemEstados) {
    in.tem_cadeia = 1;
    for (i = 0; i < RES_T_N; i++) in.cadeia[i].estado = ctxEstados[i];
  }

  st = stream_atual() >= 0 ? stream_item(stream_atual()) : NULL;
  if (st) {
    if (st->container[0]) in.midia.container = map_container(st->container);
    else if (st->mp4) in.midia.container = "mp4";
    if (st->dolbyVision > 0) in.midia.hdr = "dv";
    if (st->altura >= 2160) in.midia.res = "2160p";
    else if (st->altura >= 1080) in.midia.res = "1080p";
    else if (st->altura >= 720) in.midia.res = "720p";
    else if (st->altura >= 480) in.midia.res = "480p";
    if (st->tamanhoBytes > 0) in.midia.gb = (double)st->tamanhoBytes / 1000000000.0;
    if (st->debridServico > 0) in.midia.entrega = "debrid";
    else if (st->infoHash[0]) in.midia.entrega = "torrent";
    else if (st->url[0]) in.midia.entrega = "http";
  }
  if (strcmp(in.midia.hdr, "x") == 0) {
    if (ctxFatos.fonteDv > 0) in.midia.hdr = "dv";
    else if (ctxFatos.fonteHdr > 0) in.midia.hdr = "hdr10";
  }
  if (ctxFatos.sondaRange > 0) in.midia.http = ctxFatos.sondaRange;

  montar_perfil_envio(&perfil);
  in.perfil = &perfil;

  farolenvio_opcoes_padrao(&op);
  op.incluir_log = incluirLog ? 1 : 0;
  op.res_sintoma = ctxSintoma;

  doFarolPendente = ctxDoFarol ? 1 : 0;
  envioPendenteSubmeter = 0;
  if (!farol_enviar(&in, &op)) {
    farolenvio_info(&envInfo);
    if (farolenvio_em_voo() && envInfo.estado != FAROLENVIO_ENVIANDO) {
      /* J_CARREGAR/J_DESCARTAR em curso: submete no passo assim que encerrar. */
      envioPendenteSubmeter = 1;
      ir_tela(FEUI_ENVIANDO);
      return;
    }
  }
  farolenvio_info(&envInfo);
  if (!temUltGeracao) { ultGeracao = envInfo.geracao; temUltGeracao = 1; }
  else ultGeracao = envInfo.geracao;
  ir_tela(tela_de_info(&envInfo));
}

/* ------------------------------------------------------------- layout/foco */
static void poe(FeuiItem *it, int *n, int id, float ox, float oy, float x, float y, float w, float h) {
  if (*n >= FEUI_MAX_IT) return;
  it[*n].id = id;
  it[*n].r = (GfxRect){ ox + x, oy + y, w, h };
  (*n)++;
}

static void poe_botao(FeuiItem *it, int *n, int id, float ox, float oy, float x, float y,
                      const char *rot, const char *ic, int prim) {
  poe(it, n, id, ox, oy, x, y, botao_largura(rot, ic, prim),
      prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO);
}

static int montar(FeuiItem *it, float ox, float oy) {
  int n = 0;
  float x = FEUI_MARGEM;
  if (telaAtual == FEUI_TELA4_CONFIRMA) {
    float wMandar = botao_largura(i18n(FT_ACAO_MANDAR), "aj_send", 1);
    poe(it, &n, FEUI_B_TOGGLE_LOG, ox, oy, x, 466.0f, 740.0f, 108.0f);
    poe_botao(it, &n, FEUI_B_MANDAR, ox, oy, x, 610.0f, i18n(FT_ACAO_MANDAR), "aj_send", 1);
    poe_botao(it, &n, FEUI_B_AGORA_NAO, ox, oy, x + wMandar + BOTAO_GAP, 618.0f, i18n(FT_ACAO_AGORA_NAO), NULL, 0);
  } else if (telaAtual == FEUI_ENVIANDO) {
    poe_botao(it, &n, FEUI_B_VOLTAR, ox, oy, x, 540.0f, i18n(FT_ACAO_VOLTAR), NULL, 1);
  } else if (telaAtual == FEUI_TELA5_ENVIADO) {
    poe_botao(it, &n, FEUI_B_PRONTO, ox, oy, x, 600.0f, i18n(FT_ACAO_PRONTO), NULL, 1);
  } else if (telaAtual == FEUI_TELA6_CORRIGIDO) {
    float wAt = botao_largura(i18n("Atualizar agora"), "aj_download", 1);
    poe_botao(it, &n, FEUI_B_ATUALIZAR, ox, oy, x, 560.0f, i18n("Atualizar agora"), "aj_download", 1);
    poe_botao(it, &n, FEUI_B_FECHAR, ox, oy, x + wAt + BOTAO_GAP, 568.0f, i18n(FT_ACAO_FECHAR), NULL, 0);
  } else if (telaAtual == FEUI_PENDENTE) {
    poe_botao(it, &n, FEUI_B_PRONTO, ox, oy, x, 560.0f, i18n(FT_ACAO_PRONTO), NULL, 1);
  } else if (telaAtual == FEUI_FALHOU_RECUSOU) {
    float wTen = botao_largura(i18n(FT_ACAO_TENTAR), "aj_rotate-cw", 1);
    poe_botao(it, &n, FEUI_B_TENTAR, ox, oy, x, 560.0f, i18n(FT_ACAO_TENTAR), "aj_rotate-cw", 1);
    poe_botao(it, &n, FEUI_B_FECHAR, ox, oy, x + wTen + BOTAO_GAP, 568.0f, i18n(FT_ACAO_FECHAR), NULL, 0);
  } else if (telaAtual == FEUI_FALHOU_429) {
    poe_botao(it, &n, FEUI_B_FECHAR, ox, oy, x, 560.0f, i18n(FT_ACAO_FECHAR), NULL, 1);
  } else if (telaAtual == FEUI_FALHOU_SEM_CONTA) {
    float wCon = botao_largura(i18n("Conta e serviços"), "aj_user-round", 1);
    poe_botao(it, &n, FEUI_B_CONTAS, ox, oy, x, 560.0f, i18n("Conta e serviços"), "aj_user-round", 1);
    poe_botao(it, &n, FEUI_B_FECHAR, ox, oy, x + wCon + BOTAO_GAP, 568.0f, i18n(FT_ACAO_FECHAR), NULL, 0);
  }
  return n;
}

static void mover(int dx, int dy) {
  FeuiItem it[FEUI_MAX_IT];
  int n = montar(it, 0.0f, 0.0f), i, cur = -1, melhor = -1;
  float cx, cy, bs = 1e9f;
  for (i = 0; i < n; i++) if (it[i].id == focoAtual) cur = i;
  if (cur < 0) { focoAtual = foco_padrao_para(telaAtual); return; }
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
  if (melhor >= 0) focoAtual = it[melhor].id;
}

static void acionar_ok(void) {
  if (telaAtual == FEUI_TELA4_CONFIRMA) {
    if (focoAtual == FEUI_B_TOGGLE_LOG) {
      incluirLog = !incluirLog;
    } else if (focoAtual == FEUI_B_MANDAR) {
      disparar_envio();
    } else if (focoAtual == FEUI_B_AGORA_NAO) {
      voltarPendente = 1;
      farolenvioui_fechar();
    }
  } else if (telaAtual == FEUI_ENVIANDO) {
    envioPendenteSubmeter = 0;
    farolenvio_cancelar();
    if (doFarolPendente) {
      doFarolOkPendente = 0;
      doFarolPendente = 0;
    }
    voltarPendente = 1;
    farolenvioui_fechar();
  } else if (telaAtual == FEUI_TELA5_ENVIADO || telaAtual == FEUI_PENDENTE ||
             telaAtual == FEUI_FALHOU_429) {
    voltarPendente = 1;
    farolenvioui_fechar();
  } else if (telaAtual == FEUI_TELA6_CORRIGIDO) {
    if (focoAtual == FEUI_B_ATUALIZAR) {
      const char *nova = atualizacao_nova();
      if (nova && nova[0]) atualizacao_abrir();
      else atualizacao_procurar_agora();
      fecharPendente = 1;
      farolenvioui_fechar();
    } else {
      voltarPendente = 1;
      farolenvioui_fechar();
    }
  } else if (telaAtual == FEUI_FALHOU_RECUSOU) {
    if (focoAtual == FEUI_B_TENTAR) {
      farolenvio_info(&envInfo);
      if (envInfo.temPendente && farolenvio_retentar()) {
        farolenvio_info(&envInfo);
        ir_tela(tela_de_info(&envInfo));
      } else {
        disparar_envio();
      }
    } else {
      voltarPendente = 1;
      farolenvioui_fechar();
    }
  } else if (telaAtual == FEUI_FALHOU_SEM_CONTA) {
    if (focoAtual == FEUI_B_CONTAS) {
      pedidoPendente = RESOLVER_PEDIU_CONTAS;
      fecharPendente = 1;
      farolenvioui_fechar();
    } else {
      voltarPendente = 1;
      farolenvioui_fechar();
    }
  }
}

static void acionar_voltar(void) {
  if (telaAtual == FEUI_ENVIANDO) {
    envioPendenteSubmeter = 0;
    farolenvio_cancelar();
    if (doFarolPendente) {
      doFarolOkPendente = 0;
      doFarolPendente = 0;
    }
  }
  voltarPendente = 1;
  farolenvioui_fechar();
}

/* ---------------------------------------------------------------- API ----- */
int farolenvioui_presente(void) { return 1; }
int farolenvioui_ativo(void) { return telaAtual != FEUI_FECHADO; }
FarolEnvioUiTela farolenvioui_tela(void) { return telaAtual; }
int farolenvioui_foco(void) { return focoAtual; }
int farolenvioui_incluir_log(void) { return incluirLog; }
const char *farolenvioui_qr_url(void) { return qrUrl; }
int farolenvioui_qr_ativo(void) { return texQr != 0; }
const char *farolenvioui_codigo_exibir(void) { return envInfo.codigoExibir; }

void farolenvioui_definir_url(const char *baseUrl) {
  temUrlOverride = baseUrl != NULL;
  snprintf(urlOverride, sizeof urlOverride, "%s", baseUrl ? baseUrl : "");
}

void farolenvioui_abrir(int sintoma, int voltaFase, int doFarol,
                        const FarolVeredito *vered, const FarolFatos *ff,
                        const int *estados, const char *ultCodigo) {
  if (!envIniciado) { farolenvio_iniciar(); envIniciado = 1; }
  incluirLog = 0; /* SEMPRE nasce desligado (opt-in) e reseta ao reabrir */
  fecharPendente = 0;
  voltarPendente = 0;
  pedidoPendente = RESOLVER_PEDIU_NADA;
  ctxSintoma = sintoma;
  ctxVolta = voltaFase;
  ctxDoFarol = doFarol ? 1 : 0;
  ctxTemVered = vered != NULL;
  if (vered) ctxVered = *vered; else memset(&ctxVered, 0, sizeof ctxVered);
  if (ff) ctxFatos = *ff; else memset(&ctxFatos, 0, sizeof ctxFatos);
  ctxTemEstados = estados != NULL;
  if (estados) memcpy(ctxEstados, estados, sizeof ctxEstados);
  else memset(ctxEstados, 0, sizeof ctxEstados);
  snprintf(ctxUltCodigo, sizeof ctxUltCodigo, "%s", ultCodigo ? ultCodigo : "");

  farolenvio_info(&envInfo);
  if (!temUltGeracao) { ultGeracao = envInfo.geracao; temUltGeracao = 1; }
  if (envInfo.estado == FAROLENVIO_ENVIANDO) ir_tela(FEUI_ENVIANDO);
  else ir_tela(FEUI_TELA4_CONFIRMA);
}

void farolenvioui_fechar(void) {
  envioPendenteSubmeter = 0;
  liberar_qr();
  qrUrl[0] = '\0';
  telaAtual = FEUI_FECHADO;
}

void farolenvioui_esquecer(void) {
  farolenvioui_fechar();
  envioPendenteSubmeter = 0;
  incluirLog = 0;
  envIniciado = 0;
  fecharPendente = 0;
  voltarPendente = 0;
  pedidoPendente = RESOLVER_PEDIU_NADA;
  recarregarPendente = 0;
  doFarolPendente = 0;
  doFarolOkPendente = -1;
  temUltGeracao = 0;
  ultGeracao = 0;
  respiraMs = 0.0f;
  ctxSintoma = RES_S_GERAL;
  ctxVolta = RESOLVER_FASE_SINTOMA;
  ctxDoFarol = 0;
  ctxTemVered = 0;
  ctxTemEstados = 0;
  ctxUltCodigo[0] = '\0';
  memset(&ctxVered, 0, sizeof ctxVered);
  memset(&ctxFatos, 0, sizeof ctxFatos);
  memset(ctxEstados, 0, sizeof ctxEstados);
  memset(&envInfo, 0, sizeof envInfo);
}

void farolenvioui_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (telaAtual == FEUI_FECHADO || !e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT)  { mover(-1, 0); return; }
  if (k == SDLK_RIGHT) { mover(1, 0); return; }
  if (k == SDLK_UP)    { mover(0, -1); return; }
  if (k == SDLK_DOWN)  { mover(0, 1); return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { acionar_ok(); return; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    acionar_voltar();
  }
}

int farolenvioui_passo(float dt, int *outPedido, int *outFechar,
                       int *outVolta, int *outRecarregar,
                       int *outDoFarolOk) {
  FarolEnvioEstado st;
  int mudou = 0;

  if (!envIniciado) { farolenvio_iniciar(); envIniciado = 1; }
  farolenvio_passo();
  if (envioPendenteSubmeter && telaAtual == FEUI_ENVIANDO && !farolenvio_em_voo()) {
    disparar_envio();
  }

  if (dt > 0.0f && telaAtual == FEUI_ENVIANDO && !ajustes_animacoes_reduzidas()) {
    respiraMs = fmodf(respiraMs + dt * 1000.0f, FEUI_RESP_MS);
  } else if (ajustes_animacoes_reduzidas()) {
    respiraMs = 0.0f;
  }

  st = farolenvio_info(&envInfo);
  if (!temUltGeracao || envInfo.geracao != ultGeracao) {
    temUltGeracao = 1;
    ultGeracao = envInfo.geracao;
    if (st == FAROLENVIO_OK && envInfo.codigoNovo) {
      recarregarPendente = 1;
      snprintf(ctxUltCodigo, sizeof ctxUltCodigo, "%s", envInfo.codigo);
    }
    if (doFarolPendente && st != FAROLENVIO_ENVIANDO && st != FAROLENVIO_PENDENTE &&
        st != FAROLENVIO_OCIOSO) {
      doFarolOkPendente = (st == FAROLENVIO_OK) ? 1 : 0;
      doFarolPendente = 0;
    }
    if (telaAtual == FEUI_ENVIANDO || telaAtual == FEUI_PENDENTE) {
      if (st != FAROLENVIO_OCIOSO && st != FAROLENVIO_CANCELADO) {
        ir_tela(tela_de_info(&envInfo));
      }
    }
  }

  if (outPedido) { *outPedido = pedidoPendente; if (pedidoPendente) mudou = 1; }
  if (outFechar) { *outFechar = fecharPendente; if (fecharPendente) mudou = 1; }
  if (outVolta)  { *outVolta = voltarPendente;  if (voltarPendente) mudou = 1; }
  if (outRecarregar) { *outRecarregar = recarregarPendente; if (recarregarPendente) mudou = 1; }
  if (outDoFarolOk) { *outDoFarolOk = doFarolOkPendente; if (doFarolOkPendente >= 0) mudou = 1; }

  pedidoPendente = RESOLVER_PEDIU_NADA;
  fecharPendente = 0;
  voltarPendente = 0;
  recarregarPendente = 0;
  doFarolOkPendente = -1;
  return mudou;
}

/* ---------------------------------------------------------------- desenho - */
static void txt_em(TxtEstilo est, const char *s, float x, float y, float maxW,
                   int r, int g, int b, float a) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static void pt_foco(int id, int nada) { (void)nada; focoAtual = id; }

static void alvo(GfxRect r, int id) {
  ponteiro_alvo(r.x, r.y, r.w, r.h, pt_foco, NULL, id, 0);
}

static void painel(GfxRect r, float raioPx, float a) {
  if (ajustes_vidro()) gfx_vidro_painel(r, raioPx / r.h, 0.78f, a);
  else {
    gfx_cor(r, raioPx / r.h, 0.12f, 0.125f, 0.145f, 0.9f * a);
    gfx_anel(r, raioPx / r.h, 1.0f, 1, 1, 1, 0.08f * a);
  }
}

static void cabecalho(float ox, float oy, const char *kick, const char *tit, const char *sub, float a) {
  txt_em(TXT_CAPTION, kick, ox + FEUI_MARGEM, oy + 96.0f, 1500.0f, 150, 156, 168, a);
  txt_em(TXT_NOV_TITULO, tit, ox + FEUI_MARGEM, oy + 128.0f, 1680.0f, 248, 249, 252, a);
  if (sub && sub[0]) {
    txt_bloco(TXT_V2_26, sub, 186, 192, 204, ox + FEUI_MARGEM, oy + 206.0f, 1500.0f, 34.0f, 0.9f * a, 2);
  }
}

static float respiro_fator(void) {
  if (ajustes_animacoes_reduzidas()) return 1.0f;
  return 0.725f + 0.275f * cosf(respiraMs / FEUI_RESP_MS * 6.2831853f);
}

static void desenha_espelho_tela4(float ox, float oy, float a) {
  size_t nVai = 0, nNunca = 0, i;
  const char *const *vai = farolvai_vai_junto(&nVai);
  const char *const *nunca = farolvai_nunca_vai(&nNunca);
  float px = ox + 900.0f, py = oy + 290.0f, pw = 900.0f, ph = 668.0f, colw = 396.0f;
  float ar, ag, ab;

  ajustes_acento(&ar, &ag, &ab);
  painel((GfxRect){ px, py, pw, ph }, 32.0f, a);
  txt_em(TXT_CAPTION, i18n(FT_VAI_JUNTO), px + 36.0f, py + 28.0f, colw, 150, 156, 168, a);
  txt_em(TXT_CAPTION, i18n(FT_NUNCA_VAI), px + 36.0f + colw + 36.0f, py + 28.0f, colw, 150, 156, 168, a);

  for (i = 0; i < nVai; i++) {
    float ry = py + 74.0f + (float)i * 60.0f;
    gfx_icone((GfxRect){ px + 36.0f, ry + 3.0f, 22.0f, 22.0f }, "aj_check", ar, ag, ab, a);
    txt_bloco(TXT_CAPTION, i18n(vai[i]), 225, 228, 236, px + 36.0f + 34.0f, ry, colw - 34.0f, 26.0f, a, 2);
  }
  for (i = 0; i < nNunca; i++) {
    float ry = py + 74.0f + (float)i * 60.0f;
    gfx_icone((GfxRect){ px + 36.0f + colw + 36.0f, ry + 3.0f, 22.0f, 22.0f }, "aj_x", 0.6f, 0.62f, 0.66f, a);
    txt_bloco(TXT_CAPTION, i18n(nunca[i]), 170, 176, 188, px + 36.0f + colw + 36.0f + 34.0f, ry, colw - 34.0f, 26.0f, a, 2);
  }
  txt_em(TXT_CAPTION, i18n(FT_SOME_30), px + 36.0f, py + ph - 44.0f, pw - 72.0f, 150, 156, 168, a);
}

static void desenha_toggle(GfxRect r, int ligado, int focado, float a) {
  float ar, ag, ab;
  GfxRect tr = { r.x + r.w - 86.0f, r.y + (r.h - 36.0f) * 0.5f, 64.0f, 36.0f };
  GfxRect kn = { tr.x + (ligado ? 32.0f : 4.0f), tr.y + 4.0f, 28.0f, 28.0f };
  FeuiTinta pri = { 243, 242, 239 }, sec = { 165, 172, 184 };

  ajustes_acento(&ar, &ag, &ab);
  if (focado) {
    float fr, fg, fb, t = botao_cor_foco(&fr, &fg, &fb);
    int p = (int)(t * 255.0f + 0.5f);
    botao_luz(r, 1.0f, a);
    gfx_cor(r, 22.0f / r.h, fr, fg, fb, a);
    pri = (FeuiTinta){ p, p, p };
    sec = p > 128 ? (FeuiTinta){ 232, 232, 232 } : (FeuiTinta){ 60, 60, 60 };
  } else {
    gfx_cor(r, 22.0f / r.h, 0.14f, 0.15f, 0.17f, 0.92f * a);
  }
  txt_bloco(TXT_ILHA_NOME, i18n(FT_INCLUIR_TEXTO), pri.r, pri.g, pri.b,
            r.x + 24.0f, r.y + 18.0f, r.w - 134.0f, 28.0f, a, 2);
  if (ligado) {
    txt_em(TXT_CAPTION, i18n(FT_TEXTO_AVISO), r.x + 24.0f, r.y + r.h - 30.0f, r.w - 134.0f, sec.r, sec.g, sec.b, a);
  }
  gfx_cor(tr, 0.5f, ligado ? ar : 1.0f, ligado ? ag : 1.0f, ligado ? ab : 1.0f, (ligado ? 0.92f : 0.16f) * a);
  gfx_cor(kn, 0.5f, 1.0f, 1.0f, 1.0f, a);
}

static void desenha_botao(FeuiItem *it, const char *rot, const char *ic, int prim, float a) {
  int f = focoAtual == it->id;
  botao_pilula(it->r, rot, ic, f ? 1.0f : 0.0f, prim, 0, a);
  if (prim && !f && !ajustes_vidro()) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_anel(it->r, 0.5f, 2.0f, ar, ag, ab, a);
  }
  alvo(it->r, it->id);
}

static const char *ultimo_codigo_valido(char out[8]) {
  size_t i;
  if (envInfo.codigo[0] && strlen(envInfo.codigo) == 6) {
    snprintf(out, 8, "%s", envInfo.codigo);
    return out;
  }
  if (ctxUltCodigo[0] && strlen(ctxUltCodigo) == 6) {
    for (i = 0; i < 6; i++) {
      char c = ctxUltCodigo[i];
      if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))) return NULL;
    }
    snprintf(out, 8, "%s", ctxUltCodigo);
    return out;
  }
  return NULL;
}

void farolenvioui_desenhar(float ox, float oy, float a) {
  FeuiItem it[FEUI_MAX_IT];
  int n, i;
  char kick[160], buf[320], codDisp[16];
  float x = ox + FEUI_MARGEM;

  if (telaAtual == FEUI_FECHADO) return;
  n = montar(it, ox, oy);
  snprintf(kick, sizeof kick, "%s › %s", i18n(FT_NOME), i18n("Registro"));

  if (telaAtual == FEUI_TELA4_CONFIRMA) {
    cabecalho(ox, oy, kick, i18n(FT_ACAO_ENVIAR_REGISTRO), i18n(FT_REGISTRO_SEM_DADOS), a);
    txt_bloco(TXT_V2_26, i18n(FT_SOME_30), 186, 192, 204, x, oy + 320.0f, 740.0f, 34.0f, 0.95f * a, 2);
    desenha_espelho_tela4(ox, oy, a);
    for (i = 0; i < n; i++) {
      int id = it[i].id;
      if (id == FEUI_B_TOGGLE_LOG) {
        desenha_toggle(it[i].r, incluirLog, focoAtual == id, a);
        alvo(it[i].r, id);
      } else if (id == FEUI_B_MANDAR) {
        desenha_botao(&it[i], i18n(FT_ACAO_MANDAR), "aj_send", 1, a);
      } else if (id == FEUI_B_AGORA_NAO) {
        desenha_botao(&it[i], i18n(FT_ACAO_AGORA_NAO), NULL, 0, a);
      }
    }
  } else if (telaAtual == FEUI_ENVIANDO) {
    float rp = respiro_fator(), ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    cabecalho(ox, oy, kick, i18n(FT_ENVIANDO), "", a);
    gfx_cor((GfxRect){ x, oy + 340.0f, 20.0f, 20.0f }, 0.5f, ar, ag, ab, rp * a);
    txt_em(TXT_ILHA_TITULO, i18n(FT_ENVIANDO), x + 38.0f, oy + 326.0f, 800.0f, 248, 249, 252, rp * a);
    for (i = 0; i < n; i++) {
      if (it[i].id == FEUI_B_VOLTAR) desenha_botao(&it[i], i18n(FT_ACAO_VOLTAR), NULL, 1, a);
    }
  } else if (telaAtual == FEUI_TELA5_ENVIADO) {
    const char *disp = resolverregra_codigo_exibir(envInfo.codigo, codDisp, sizeof codDisp)
                         ? codDisp : envInfo.codigo;
    snprintf(buf, sizeof buf, i18n(FT_REGISTRO_ENVIADO_L1), disp);
    cabecalho(ox, oy, kick, i18n(FT_ACAO_PRONTO), buf, a);
    txt_tracking(TXT_AJ_NUM64, disp, 248, 249, 252, x, oy + 320.0f, a, 6.0f);
    txt_bloco(TXT_V2_26, i18n(FT_REGISTRO_ENVIADO_L2), 186, 192, 204, x, oy + 430.0f, 860.0f, 34.0f, 0.95f * a, 2);
    txt_em(TXT_CAPTION, i18n(FT_SOME_30), x, oy + 516.0f, 860.0f, 150, 156, 168, a);

    if (envInfo.codigo[0]) {
      atualizar_qr_para(envInfo.codigo);
      if (texQr) {
        float qx = ox + 1260.0f, qy = oy + 310.0f;
        GfxRect moldura = { qx - 20.0f, qy - 20.0f, FEUI_QR_LADO + 40.0f, FEUI_QR_LADO + 40.0f };
        GfxRect r = { qx, qy, FEUI_QR_LADO, FEUI_QR_LADO };
        painel((GfxRect){ qx - 56.0f, qy - 52.0f, FEUI_QR_LADO + 112.0f, FEUI_QR_LADO + 148.0f }, 32.0f, a);
        gfx_cor(moldura, 0.06f, 1.0f, 1.0f, 1.0f, a);
        gfx_tex_aspect_atual = 0.0f;
        gfx_rect(r, texQr, GFX_SNAP, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0, a);
      }
    }
    for (i = 0; i < n; i++) {
      if (it[i].id == FEUI_B_PRONTO) desenha_botao(&it[i], i18n(FT_ACAO_PRONTO), NULL, 1, a);
    }
  } else if (telaAtual == FEUI_TELA6_CORRIGIDO) {
    const char *ver = envInfo.conhecidoVersao[0] ? envInfo.conhecidoVersao : "?";
    const char *nota = ajustes_idioma_ingles()
                         ? (envInfo.notaEn[0] ? envInfo.notaEn : envInfo.notaPt)
                         : (envInfo.notaPt[0] ? envInfo.notaPt : envInfo.notaEn);
    snprintf(buf, sizeof buf, i18n(FT_JA_CORRIGIDO_L1), ver);
    cabecalho(ox, oy, kick, buf, i18n(FT_JA_CORRIGIDO_L2), a);
    if (nota && nota[0]) {
      painel((GfxRect){ x, oy + 310.0f, 920.0f, 180.0f }, 28.0f, a);
      txt_bloco(TXT_V2_26, nota, 220, 224, 232, x + 32.0f, oy + 344.0f, 856.0f, 34.0f, a, 3);
    }
    for (i = 0; i < n; i++) {
      if (it[i].id == FEUI_B_ATUALIZAR) desenha_botao(&it[i], i18n("Atualizar agora"), "aj_download", 1, a);
      else if (it[i].id == FEUI_B_FECHAR) desenha_botao(&it[i], i18n(FT_ACAO_FECHAR), NULL, 0, a);
    }
  } else if (telaAtual == FEUI_PENDENTE) {
    cabecalho(ox, oy, kick, i18n(FT_REDE_FORA_L1), i18n(FT_SEM_NET_ENVIO_L2), a);
    painel((GfxRect){ x, oy + 310.0f, 920.0f, 180.0f }, 28.0f, a);
    txt_bloco(TXT_V2_26, i18n(FT_SEM_NET_ENVIO_L2), 220, 224, 232, x + 32.0f, oy + 344.0f, 856.0f, 34.0f, a, 2);
    txt_em(TXT_CAPTION, i18n(FT_PENDENTE_24H), x + 32.0f, oy + 428.0f, 856.0f, 150, 156, 168, a);
    for (i = 0; i < n; i++) {
      if (it[i].id == FEUI_B_PRONTO) desenha_botao(&it[i], i18n(FT_ACAO_PRONTO), NULL, 1, a);
    }
  } else if (telaAtual == FEUI_FALHOU_RECUSOU) {
    cabecalho(ox, oy, kick, i18n(FT_RECUSOU_L1), i18n(FT_RECUSOU_L2), a);
    if (envInfo.http > 0) {
      snprintf(buf, sizeof buf, "HTTP %d", envInfo.http);
      txt_em(TXT_CAPTION, buf, x, oy + 340.0f, 700.0f, 150, 156, 168, a);
    }
    for (i = 0; i < n; i++) {
      if (it[i].id == FEUI_B_TENTAR) desenha_botao(&it[i], i18n(FT_ACAO_TENTAR), "aj_rotate-cw", 1, a);
      else if (it[i].id == FEUI_B_FECHAR) desenha_botao(&it[i], i18n(FT_ACAO_FECHAR), NULL, 0, a);
    }
  } else if (telaAtual == FEUI_FALHOU_429) {
    char ult[8];
    const char *cod = ultimo_codigo_valido(ult);
    cabecalho(ox, oy, kick, i18n(FT_LIMITE_L1), i18n(FT_LIMITE_L2), a);
    if (cod && resolverregra_codigo_exibir(cod, codDisp, sizeof codDisp)) {
      txt_tracking(TXT_AJ_NUM64, codDisp, 248, 249, 252, x, oy + 330.0f, a, 6.0f);
    }
    for (i = 0; i < n; i++) {
      if (it[i].id == FEUI_B_FECHAR) desenha_botao(&it[i], i18n(FT_ACAO_FECHAR), NULL, 1, a);
    }
  } else if (telaAtual == FEUI_FALHOU_SEM_CONTA) {
    cabecalho(ox, oy, kick, i18n(FT_SEM_CONTA_L1), i18n(FT_SEM_CONTA_L2), a);
    for (i = 0; i < n; i++) {
      if (it[i].id == FEUI_B_CONTAS) desenha_botao(&it[i], i18n("Conta e serviços"), "aj_user-round", 1, a);
      else if (it[i].id == FEUI_B_FECHAR) desenha_botao(&it[i], i18n(FT_ACAO_FECHAR), NULL, 0, a);
    }
  }
}
