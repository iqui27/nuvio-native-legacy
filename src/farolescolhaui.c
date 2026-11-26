// Modo perguntas do Farol (P5 sem LLM #50 + assistente por frase #54 + multi-TV #55).
// Ver farolescolhaui.h. Sem estado por fio (TLS), sem log de frase/titulo/perfil/token.
#include "farolescolhaui.h"
#include "farolacha.h"
#include "farolcompara.h"
#include "farolescolha_rede.h"
#include "faroltexto.h"
#include "ajustes.h"
#include "anim.h"
#include "arteurl.h"
#include "botoes.h"
#include "catalogo.h"
#include "celular.h"
#include "entrada_texto.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "perfilinfantil.h"
#include "ponteiro.h"
#include "posterprov.h"
#include "qr.h"
#include "sistexto.h"
#include "tex_cache.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(RESOLVER_TESTE) && !defined(NV_VOZ_TELA_SHOT) && \
    !defined(NV_ACHA_TELA_SHOT) && !defined(NV_FRASE_TV_SHOT) && \
    !defined(NV_ESCOLHA_TV_SHOT) && !defined(NV_LINUX_DESKTOP)
#define FESCUI_SEM_GL 1
#else
#define FESCUI_SEM_GL 0
#include "qrtex.h"
#endif

// Ganchos fracos para testes isolados que nao linkam celular.c / qr.c / entrada_texto.c.
__attribute__((weak)) int celular_disponivel(void) { return 0; }
__attribute__((weak)) int celular_abrir_digitar(const char *t) { (void)t; return 0; }
__attribute__((weak)) void celular_fechar(void) {}
__attribute__((weak)) int celular_estado(void) { return CEL_PARADO; }
__attribute__((weak)) const char *celular_url(void) { return ""; }
__attribute__((weak)) int celular_pegar(char *dst, size_t n) { (void)dst; (void)n; return 0; }
__attribute__((weak)) int qr_gerar(Qr *q, const char *texto) { (void)q; (void)texto; return 0; }
__attribute__((weak)) int texto_sistema_voz_pelo_teclado(void) { return 0; }

#define ESC_MARGEM    120.0f
#define ESC_LARG      1680.0f
#define ESC_PILULA_Y  326.0f
#define ESC_PILULA_MIN 260.0f
#define ESC_PILULA_GAP 32.0f
#define ESC_CARD_W    280.0f
#define ESC_CARD_H    420.0f
#define ESC_CARD_GAP   48.0f
#define ESC_VOLTAR_Y  888.0f
#define ESC_PAINEL_Y  290.0f
#define ESC_PAINEL_H  320.0f
#define ESC_VOZ_BTN_Y 680.0f
#define ESC_PULSO_MS  1400.0f
#define ESC_QR_LADO   248.0f
#define ESC_QR_MOLD    12.0f
#define ESC_SEMENTE   0x5e5d0050u   // fixa: mesmo catalogo = mesmas perguntas

enum {
  FESC_ENT_NENHUM = 0,
  FESC_ENT_VOZ,
  FESC_ENT_IME,
  FESC_ENT_QR
};

static int forcaVoz = -1;
static int forcaIme = -1;
static int forcaCel = -1;

// FALTA_TEXTO: "Nada exato…" (P5, 0 titulos) e "Achamos este" / "Achamos
// estes" ainda nao tem chave nas tabelas src/idioma_*.h (fora da fronteira
// desta issue): i18n() devolve o portugues ate a traducao entrar.
#define FESCUI_NADA_EXATO  "Nada exato…"
#define FESCUI_NADA_L2     "Volte uma pergunta ou escolha outra opção."
#define FESCUI_ACHAMOS_1   "Achamos este"
#define FESCUI_ACHAMOS_N   "Achamos estes"

typedef struct {
  int  indice;          // no catalogo da revisao `rev`
  long tmdb;
  char imdb[64];
  char tipo[8];
} Ident;

static FarolAchado   achados[FESC_CAND_MAX];
static Ident         ident[FESC_CAND_MAX];
static FarolEscCand  cand[FESC_CAND_MAX];

static struct {
  int      modo;
  int      nCand;
  unsigned rev;
  FarolEsc motor;
  int      foco;                    // opcao/cartaz ou FESCUI_FOCO_*
  int      focoAntesVoltar;         // para o ↑ voltar de onde saiu
  int      escolhido[FESC_PERG_MAX];   // opcao oferecida escolhida em cada passo
  float    anim[FESC_OPC_MAX];
  float    animVoltar;
  float    animVoz;
  float    animConf;
  float    animAlt;
  // Assistente por frase (#54/#55).
  int      entradaModo;
  int      celAberto;
  int      qrPronto;
  GLuint   qrTex;
  char     qrUrl[QR_MAX_BYTES + 1];
  float    nivelVoz;
  float    pulsoMs;
  uint64_t redeGen;
  char     parcial[FAROL_ESCOLHA_FRASE_BYTES];
  char     frase[FAROL_ESCOLHA_FRASE_BYTES];
  char     aviso[200];
  // Resultado (modo RESULTADO).
  FarolEscResultado res;
  int      saida;
  int      nRes;
  int      resIdx[FESC_RES_MAX];
  int32_t  resPontos[FESC_RES_MAX];
  unsigned resRev;
  FarolAchaPedido resPed[FESC_RES_MAX];
  int      pedidoTem;
  FarolAchaPedido pedido;
  int      fecharTem;
  int      testeHora;               // -2 = relogio real
  int      testeInfantil;           // -1 = perfil real
} S = { .testeHora = -2, .testeInfantil = -1 };

// ------------------------------------------------------------ candidatos --
static int ehSerieTipo(const char *t) {
  return t && (!strcmp(t, "series") || !strcmp(t, "show"));
}

static int anoDe(const char *meta) {
  const char *p;
  if (!meta) return 0;
  for (p = meta; *p; p++) {
    if (p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9' &&
        p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9' &&
        !(p[4] >= '0' && p[4] <= '9') && (p == meta || !(p[-1] >= '0' && p[-1] <= '9'))) {
      int a = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
      if (a >= 1900 && a <= 2100) return a;
    }
  }
  return 0;
}

static int horaAgora(void) {
  time_t t;
  struct tm tmv;
  if (S.testeHora != -2) return S.testeHora;
  t = time(NULL);
#if defined(_WIN32)
  if (localtime_s(&tmv, &t) != 0) return -1;
#else
  if (!localtime_r(&t, &tmv)) return -1;
#endif
  return tmv.tm_hour;
}

static int infantilAgora(void) {
  return S.testeInfantil >= 0 ? S.testeInfantil : perfil_infantil_ativo();
}

// Pontos (so ordenam o resultado): nota, salvo, nao visto, em andamento.
static int32_t pontosDe(const CatItem *c, int naoVi, int andamento) {
  int32_t p = (int32_t)(c->nota > 0 ? c->nota : 0) * 10;
  if (c->naLista) p += 300;
  if (naoVi) p += 200;
  if (andamento) p += 100;
  return p;
}

static void montarCandidatos(void) {
  int n, i, emAnd = 0;
  unsigned rev = 0;
  int infantil = infantilAgora();
  n = farolacha_listar_rev(FA_SURPRESA, 0, achados, FESC_CAND_MAX, ESC_SEMENTE, &rev);
  S.nCand = 0;
  S.rev = rev;
  for (i = 0; i < n; i++) {
    const CatItem *c = farolacha_item(&achados[i], rev);
    FarolEscFatos f;
    Ident *id;
    int andamento, naoVi;
    if (!c) continue;
    // Perfil infantil: a lista do farolacha NAO vem filtrada por classificacao
    // (a guarda e de cada modulo: farol.c, faroldetalhe.c). Descarta o que
    // nao e livre/ate 10 antes de qualquer pergunta — sem classificacao
    // conhecida tambem fica de fora (a promessa e "so").
    if (infantil && !perfil_infantil_classif_ok(c->classificacao)) continue;
    if (!c->imdb[0] && c->tmdb <= 0) continue;   // sem identidade nao reabre
    id = &ident[S.nCand];
    memset(id, 0, sizeof *id);
    id->indice = achados[i].indice;
    id->tmdb = c->tmdb;
    snprintf(id->imdb, sizeof id->imdb, "%s", c->imdb);
    snprintf(id->tipo, sizeof id->tipo, "%s", c->tipo);
    // Em andamento = comecou e nao terminou (a mesma faixa de farolacha.c:
    // progresso >= 90 ja conta como visto; retomadoMs so com progresso 0).
    andamento = (c->progresso > 0 && c->progresso < 90) ||
                (c->progresso == 0 && c->retomadoMs > 0);
    naoVi = (achados[i].motivo & FA_MOT_NAO_VI) != 0;
    memset(&f, 0, sizeof f);
    f.id = S.nCand;
    f.pontos = pontosDe(c, naoVi, andamento);
    f.minutos = achados[i].minutos;
    f.ano = anoDe(c->meta);
    f.serie = ehSerieTipo(c->tipo);
    f.visto = !naoVi;
    f.andamento = andamento;
    f.idiomas = 0;                 // o catalogo nao diz o audio: a pergunta nao vem
    f.genero = c->genero[0] ? c->genero : NULL;
    farolescolha_cand_de(&cand[S.nCand], &f);
    emAnd |= andamento;
    S.nCand++;
  }
  {
    FarolEscCtx ctx;
    ctx.hora = horaAgora();
    ctx.infantil = infantilAgora();
    ctx.emAndamento = emAnd;
    farolescolha_iniciar(&S.motor, cand, S.nCand, &ctx);
  }
}

// ------------------------------------------------------------ resultado --
static void pedidoDe(FarolAchaPedido *p, const Ident *id, unsigned rev) {
  memset(p, 0, sizeof *p);
  p->rev = rev;
  p->indice = id->indice;
  p->tmdb = id->tmdb;
  snprintf(p->imdb, sizeof p->imdb, "%s", id->imdb);
  snprintf(p->tipo, sizeof p->tipo, "%s", id->tipo);
}

// Resolve os titulos do resultado no catalogo vivo (pela identidade). O que
// sumiu sai da lista; o resto guarda indice + revisao + identidade exibida.
static void resolverResultado(void) {
  int i, k = 0;
  unsigned revVivo = cat_revisao();
  for (i = 0; i < S.res.n; i++) {
    int pos = S.res.id[i];
    FarolAchaPedido p;
    unsigned r = 0;
    int idx;
    const CatItem *c;
    if (pos < 0 || pos >= S.nCand) continue;
    pedidoDe(&p, &ident[pos], S.rev);
    idx = farolachaui_resolver_pedido(&p, &r);
    if (idx < 0) continue;
    c = cat_item(idx);
    if (!c) continue;
    revVivo = r;
    p.rev = r;
    p.indice = idx;
    snprintf(p.titulo, sizeof p.titulo, "%s", c->titulo);
    snprintf(p.meta, sizeof p.meta, "%s", c->meta);
    S.resIdx[k] = idx;
    S.resPontos[k] = S.res.pontos[i];
    S.resPed[k] = p;
    k++;
  }
  S.nRes = k;
  S.resRev = revVivo;
}

static void garantirResultado(void) {
  int i, k = 0;
  if (S.modo != FESCUI_RESULTADO || S.nRes <= 0 || cat_revisao() == S.resRev) return;
  for (i = 0; i < S.nRes; i++) {
    unsigned r = 0;
    int idx = farolachaui_resolver_pedido(&S.resPed[i], &r);
    if (idx < 0) continue;
    S.resPed[k] = S.resPed[i];
    S.resPontos[k] = S.resPontos[i];
    S.resPed[k].rev = r;
    S.resPed[k].indice = idx;
    S.resIdx[k] = idx;
    S.resRev = r;
    k++;
  }
  if (k == 0) S.resRev = cat_revisao();
  S.nRes = k;
  if (S.foco >= S.nRes) S.foco = S.nRes > 0 ? S.nRes - 1 : FESCUI_FOCO_VOLTAR;
}

static int modoTecladoPorVoz(void) {
  if (forcaVoz >= 0) return 0;
#if defined(NV_ANDROID)
  return 0;
#elif defined(NV_TPK) || defined(__EMSCRIPTEN__)
  return (st_voz_disponivel() &&
          (!st_ime_disponivel() || texto_sistema_voz_pelo_teclado())) ? 1 : 0;
#else
  return (st_voz_disponivel() && texto_sistema_voz_pelo_teclado()) ? 1 : 0;
#endif
}

static int modoVoz(void) {
  if (forcaVoz >= 0) return forcaVoz ? 1 : 0;
  if (modoTecladoPorVoz()) return 0;
  return (st_voz_disponivel() && !texto_sistema_voz_pelo_teclado()) ? 1 : 0;
}

static int modoIme(void) {
  if (forcaIme >= 0) return forcaIme ? 1 : 0;
  return (modoTecladoPorVoz() || st_ime_disponivel()) ? 1 : 0;
}

static int modoCel(void) {
  if (forcaCel >= 0) return forcaCel ? 1 : 0;
#if defined(NV_DESKTOP_UI) || defined(NV_LINUX_DESKTOP) || defined(__APPLE__) || defined(_WIN32)
  return 0;
#else
  return celular_disponivel() ? 1 : 0;
#endif
}

static int modoPrimario(void) {
  if (modoVoz()) return FESC_ENT_VOZ;
  if (modoIme()) return FESC_ENT_IME;
  if (modoCel()) return FESC_ENT_QR;
  return FESC_ENT_NENHUM;
}

static int modoDisponivel(int m) {
  if (m == FESC_ENT_VOZ) return modoVoz();
  if (m == FESC_ENT_IME) return modoIme();
  if (m == FESC_ENT_QR)  return modoCel();
  return 0;
}

static int modoRefazer(void) {
  if (S.entradaModo != FESC_ENT_NENHUM && modoDisponivel(S.entradaModo)) return S.entradaModo;
  return modoPrimario();
}

static int modoAlternativo(void) {
  int cur;
  if (modoVoz()) return FESC_ENT_NENHUM;
  if (!modoIme() || !modoCel()) return FESC_ENT_NENHUM;
  cur = modoRefazer();
  if (cur == FESC_ENT_IME) return FESC_ENT_QR;
  if (cur == FESC_ENT_QR)  return FESC_ENT_IME;
  return FESC_ENT_NENHUM;
}

static const char *rotuloEntrada(int m) {
  if (m == FESC_ENT_IME) return FT_ACAO_TECLADO_TV;
  if (m == FESC_ENT_QR)  return FT_ACAO_DIGITAR_CELULAR;
  return FT_ACAO_FALAR;
}

static const char *iconeEntrada(int m) {
  if (m == FESC_ENT_IME) return "aj_keyboard";
  if (m == FESC_ENT_QR)  return "aj_smartphone";
  return "aj_mic";
}

static void copiarUtf8Seguro(char *dst, size_t cap, const char *src) {
  size_t n = 0;
  if (!dst || !cap) return;
  if (!src) { dst[0] = '\0'; return; }
  while (src[n] && n + 1 < cap) n++;
  if (src[n]) {
    while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
}

static void destruirQrTex(void) {
#if !FESCUI_SEM_GL
  if (S.qrTex) {
    glDeleteTextures(1, &S.qrTex);
    S.qrTex = 0;
  }
#else
  S.qrTex = 0;
#endif
  S.qrUrl[0] = '\0';
  S.qrPronto = 0;
}

static void fecharCelular(void) {
  if (S.celAberto) {
    celular_fechar();
    S.celAberto = 0;
  }
  destruirQrTex();
}

static void zerarFrases(void) {
  memset(S.parcial, 0, sizeof S.parcial);
  memset(S.frase, 0, sizeof S.frase);
  memset(S.aviso, 0, sizeof S.aviso);
}

static int podeUsarVoz(void) {
  return S.modo == FESCUI_PERGUNTA &&
         farolescolha_feitas(&S.motor) == 0 &&
         S.motor.nPre == 0 &&
         modoPrimario() != FESC_ENT_NENHUM;
}

static void zerarAnim(void) {
  int i;
  for (i = 0; i < FESC_OPC_MAX; i++) S.anim[i] = 0.0f;
  S.animVoltar = 0.0f;
  S.animVoz = 0.0f;
  S.animConf = 0.0f;
  S.animAlt = 0.0f;
  if (ajustes_animacoes_reduzidas()) {
    if (S.foco >= 0 && S.foco < FESC_OPC_MAX) S.anim[S.foco] = 1.0f;
    if (S.foco == FESCUI_FOCO_VOLTAR) S.animVoltar = 1.0f;
    if (S.foco == FESCUI_FOCO_VOZ) S.animVoz = 1.0f;
    if (S.foco == FESCUI_FOCO_CONFIRMAR) S.animConf = 1.0f;
    if (S.foco == FESCUI_FOCO_ALT) S.animAlt = 1.0f;
  }
}

// O motor terminou: P4 com 2-3, cartaz com 1, "Nada exato…" com 0.
static void concluir(void) {
  int todosImdb = 1, i;
  fecharCelular();
  farolescolha_resultado(&S.motor, &S.res);
  S.saida = S.res.saida;
  resolverResultado();
  // farolcompara_abrir_titulos identifica por c->imdb: um vencedor so-TMDB
  // entraria na lista e sumiria em silencio. So vai ao P4 quando TODOS os
  // vencedores tem imdb; senao o conjunto fica aqui, nos cartazes (que abrem
  // por FarolAchaPedido, com identidade TMDB ou IMDb).
  for (i = 0; i < S.nRes; i++) {
    const CatItem *c = cat_item(S.resIdx[i]);
    if (!c || !c->imdb[0]) { todosImdb = 0; break; }
  }
  if (S.nRes >= 2 && todosImdb) {
    if (farolcompara_abrir_titulos(S.resIdx, S.nRes, S.resPontos, S.res.tempoMin)) {
      S.modo = FESCUI_COMPARAR;
      S.fecharTem = 1;
      return;
    }
  }
  S.modo = FESCUI_RESULTADO;
  S.foco = S.nRes > 0 ? 0 : FESCUI_FOCO_VOLTAR;
  S.focoAntesVoltar = 0;
  zerarAnim();
}

// ------------------------------------------------------------ fluxo --
void farolescolhaui_reset(void) {
  int th = S.testeHora, ti = S.testeInfantil;
  if (S.modo == FESCUI_OUVINDO || S.modo == FESCUI_DIGITANDO) st_fechar(ST_BUSCA);
  fecharCelular();
  if (S.modo == FESCUI_ESPERA || S.redeGen != 0) farolescolha_rede_cancelar();
  memset(&S, 0, sizeof S);
  S.testeHora = th;
  S.testeInfantil = ti;
  S.modo = FESCUI_FECHADO;
}

static void entrarPergunta(int foco) {
  int n = farolescolha_n_opcoes(&S.motor);
  S.modo = FESCUI_PERGUNTA;
  S.entradaModo = FESC_ENT_NENHUM;
  S.foco = (foco >= 0 && foco < n) ? foco : 0;
  S.focoAntesVoltar = S.foco;
  zerarAnim();
}

void farolescolhaui_fechar(void) {
  if (S.modo == FESCUI_OUVINDO || S.modo == FESCUI_DIGITANDO) st_fechar(ST_BUSCA);
  fecharCelular();
  if (S.modo == FESCUI_ESPERA || S.redeGen != 0) {
    farolescolha_rede_cancelar();
    S.redeGen = 0;
  }
  zerarFrases();
  if (S.modo == FESCUI_OUVINDO || S.modo == FESCUI_DIGITANDO ||
      S.modo == FESCUI_QR || S.modo == FESCUI_PREVIA ||
      S.modo == FESCUI_ESPERA || S.modo == FESCUI_AVISO)
    entrarPergunta(0);
}

static void seguir(int foco) {
  if (farolescolha_terminou(&S.motor)) concluir();
  else entrarPergunta(foco);
}

void farolescolhaui_abrir(void) {
  farolescolhaui_reset();
  montarCandidatos();
  seguir(0);
}

int farolescolhaui_pre_responder(const char *perguntaId, const char *opcaoId) {
  int p = farolescolha_pergunta_por_id(perguntaId);
  if (S.modo != FESCUI_PERGUNTA && S.modo != FESCUI_RESULTADO) return 0;
  if (farolescolha_feitas(&S.motor) > 0) return 0;
  if (!farolescolha_pre_responder(&S.motor, p, opcaoId)) return 0;
  S.nRes = 0;
  seguir(0);
  return 1;
}

static void entrarAviso(const char *msg) {
  int ref = modoRefazer();
  S.modo = FESCUI_AVISO;
  snprintf(S.aviso, sizeof S.aviso, "%s",
           (msg && msg[0]) ? msg : "O ditado não respondeu. Tente de novo ou digite.");
  if (ref != FESC_ENT_NENHUM) S.foco = FESCUI_FOCO_VOZ;
  else if (modoAlternativo() != FESC_ENT_NENHUM) S.foco = FESCUI_FOCO_ALT;
  else S.foco = FESCUI_FOCO_VOLTAR;
  zerarAnim();
}

int farolescolhaui_aplicar_frase(const char *frase) {
  char norm[FAROL_ESCOLHA_FRASE_BYTES];
  int eraOuvindo = (S.modo == FESCUI_OUVINDO);
  int eraDigitando = (S.modo == FESCUI_DIGITANDO);
  int eraQr = (S.modo == FESCUI_QR);
  if (S.modo != FESCUI_PERGUNTA && !eraOuvindo && !eraDigitando && !eraQr) return 0;
  if (farolescolha_feitas(&S.motor) > 0 || S.motor.nPre > 0) return 0;
  if (!farolescolha_rede_normalizar_frase(frase, norm, sizeof norm)) {
    if (eraOuvindo) {
      st_fechar(ST_BUSCA);
      zerarFrases();
      entrarPergunta(0);
    } else if (eraDigitando) {
      st_fechar(ST_BUSCA);
      zerarFrases();
      entrarAviso("Não entendi. Tente de novo.");
    } else if (eraQr) {
      fecharCelular();
      zerarFrases();
      entrarAviso("Não entendi. Tente de novo.");
    }
    return 0;
  }
  if (eraOuvindo || eraDigitando) st_fechar(ST_BUSCA);
  if (eraQr) fecharCelular();
  zerarFrases();
  snprintf(S.frase, sizeof S.frase, "%s", norm);
  memset(norm, 0, sizeof norm);
  if (S.entradaModo == FESC_ENT_NENHUM) S.entradaModo = modoPrimario();
  S.modo = FESCUI_PREVIA;
  S.foco = FESCUI_FOCO_CONFIRMAR;
  zerarAnim();
  return 1;
}

static void iniciarVoz(void) {
  if (!modoVoz()) return;
  fecharCelular();
  if (S.modo == FESCUI_DIGITANDO || S.modo == FESCUI_OUVINDO) st_fechar(ST_BUSCA);
  zerarFrases();
  S.entradaModo = FESC_ENT_VOZ;
  S.nivelVoz = 0.0f;
  S.pulsoMs = 0.0f;
  st_voz_teclado_inicial("");
  if (st_voz_iniciar(ST_BUSCA)) {
    S.modo = FESCUI_OUVINDO;
    S.foco = FESCUI_FOCO_VOLTAR;
    zerarAnim();
  } else {
    const char *av = st_aviso();
    entrarAviso(av && av[0] ? av : "O ditado não respondeu. Tente de novo ou digite.");
  }
}

static void iniciarIme(void) {
  int ok;
  if (!modoIme()) return;
  fecharCelular();
  if (S.modo == FESCUI_OUVINDO || S.modo == FESCUI_DIGITANDO) st_fechar(ST_BUSCA);
  zerarFrases();
  S.entradaModo = FESC_ENT_IME;
  S.nivelVoz = 0.0f;
  S.pulsoMs = 0.0f;
  if (modoTecladoPorVoz()) {
    st_voz_teclado_inicial("");
    ok = st_voz_iniciar(ST_BUSCA);
  } else {
    st_ime_tipo(ST_IME_TEXTO);
    ok = st_ime_abrir(ST_BUSCA, "", FAROL_ESCOLHA_FRASE_MAX);
  }
  if (ok) {
    S.modo = FESCUI_DIGITANDO;
    S.foco = FESCUI_FOCO_VOLTAR;
    zerarAnim();
  } else {
    const char *av = st_aviso();
    entrarAviso(av && av[0] ? av : "O teclado do sistema não abriu.");
  }
}

static void iniciarQr(void) {
  const char *u;
  Qr q;
  if (!modoCel()) return;
  if (S.modo == FESCUI_OUVINDO || S.modo == FESCUI_DIGITANDO) st_fechar(ST_BUSCA);
  fecharCelular();
  zerarFrases();
  S.entradaModo = FESC_ENT_QR;
  S.nivelVoz = 0.0f;
  S.pulsoMs = 0.0f;
  if (!celular_abrir_digitar(FT_DIGITE_CELULAR)) {
    entrarAviso("Sem rede local. Conecte a TV ao Wi-Fi e aperte OK.");
    return;
  }
  S.celAberto = 1;
  u = celular_url();
  if (!u || !u[0] || !qr_gerar(&q, u)) {
    fecharCelular();
    entrarAviso("Sem rede local. Conecte a TV ao Wi-Fi e aperte OK.");
    return;
  }
  S.qrPronto = 1;
  S.modo = FESCUI_QR;
  S.foco = FESCUI_FOCO_VOLTAR;
  zerarAnim();
}

static void iniciarEntrada(int m) {
  if (m == FESC_ENT_VOZ) iniciarVoz();
  else if (m == FESC_ENT_IME) iniciarIme();
  else if (m == FESC_ENT_QR) iniciarQr();
}

static void confirmarPrevia(void) {
  uint64_t gen;
  if (!S.frase[0] || infantilAgora() || !farolescolha_rede_autorizado()) {
    zerarFrases();
    entrarPergunta(0);
    return;
  }
  gen = farolescolha_rede_pedir(S.frase, &S.motor, NULL);
  if (gen == 0) {
    zerarFrases();
    entrarPergunta(0);
    return;
  }
  S.redeGen = gen;
  S.modo = FESCUI_ESPERA;
  S.foco = FESCUI_FOCO_VOLTAR;
  S.pulsoMs = 0.0f;
  zerarAnim();
}

static int montarBotoesFluxo(int ids[4]) {
  int n = 0;
  if (S.modo == FESCUI_OUVINDO || S.modo == FESCUI_ESPERA) {
    ids[n++] = FESCUI_FOCO_VOLTAR;
  } else if (S.modo == FESCUI_DIGITANDO || S.modo == FESCUI_QR) {
    ids[n++] = FESCUI_FOCO_VOLTAR;
    if (modoAlternativo() != FESC_ENT_NENHUM) ids[n++] = FESCUI_FOCO_ALT;
  } else if (S.modo == FESCUI_PREVIA) {
    ids[n++] = FESCUI_FOCO_CONFIRMAR;
    if (modoRefazer() != FESC_ENT_NENHUM) ids[n++] = FESCUI_FOCO_VOZ;
    if (modoAlternativo() != FESC_ENT_NENHUM) ids[n++] = FESCUI_FOCO_ALT;
    ids[n++] = FESCUI_FOCO_VOLTAR;
  } else if (S.modo == FESCUI_AVISO) {
    if (modoRefazer() != FESC_ENT_NENHUM) ids[n++] = FESCUI_FOCO_VOZ;
    if (modoAlternativo() != FESC_ENT_NENHUM) ids[n++] = FESCUI_FOCO_ALT;
    ids[n++] = FESCUI_FOCO_VOLTAR;
  }
  return n;
}

static void moverFocoFluxo(int dir) {
  int ids[4], n = montarBotoesFluxo(ids), i, cur = -1;
  if (n <= 0) return;
  for (i = 0; i < n; i++) if (ids[i] == S.foco) { cur = i; break; }
  if (cur < 0) { S.foco = ids[0]; return; }
  cur += dir;
  if (cur < 0) cur = 0;
  if (cur >= n) cur = n - 1;
  S.foco = ids[cur];
}

// Voltar: desfaz um passo (foco na opcao escolhida nele) ou pede a saida.
static void voltarUmPasso(int *pediuSair) {
  if (farolescolha_voltar(&S.motor)) {
    int passo = farolescolha_feitas(&S.motor);
    int f = (passo >= 0 && passo < FESC_PERG_MAX) ? S.escolhido[passo] : 0;
    S.nRes = 0;
    seguir(f);
    return;
  }
  if (pediuSair) *pediuSair = 1;
}

static int nFocaveis(void) {
  if (S.modo == FESCUI_PERGUNTA) return farolescolha_n_opcoes(&S.motor);
  if (S.modo == FESCUI_RESULTADO) return S.nRes;
  return 0;
}

static int okNoFoco(int *pediuSair) {
  if (S.foco == FESCUI_FOCO_VOLTAR) { voltarUmPasso(pediuSair); return 1; }
  if (S.foco == FESCUI_FOCO_VOZ) {
    if (podeUsarVoz()) iniciarEntrada(modoPrimario());
    return 1;
  }
  if (S.modo == FESCUI_PERGUNTA) {
    int passo = farolescolha_feitas(&S.motor), f = S.foco;
    if (!farolescolha_responder(&S.motor, f)) return 1;
    if (passo >= 0 && passo < FESC_PERG_MAX) S.escolhido[passo] = f;
    seguir(0);
    return 1;
  }
  return 1;
}

int farolescolhaui_evento(const SDL_Event *e, int *pediuSair, int *pediuAbrir) {
  SDL_Keycode k;
  int n, okTem = 0, okVoltar = 0;
  FarolAchaPedido okExibido;
  if (pediuSair) *pediuSair = 0;
  if (pediuAbrir) *pediuAbrir = 0;
  if (!e) return 0;
  if ((S.modo == FESCUI_OUVINDO || S.modo == FESCUI_DIGITANDO) && st_evento(e)) return 1;
  if (S.modo == FESCUI_DIGITANDO &&
      (e->type == SDL_TEXTINPUT || e->type == SDL_TEXTEDITING))
    return 1;
  if (e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  if (S.modo == FESCUI_DIGITANDO && k == SDLK_BACKSPACE) return 1;

  // Estados de voz, digitacao, QR, previa, aviso e espera (#54/#55): ignorar key.repeat em OK e Voltar.
  if (S.modo == FESCUI_OUVINDO) {
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
        k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
        k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
      if (e->key.repeat != 0) return 1;
      st_fechar(ST_BUSCA);
      zerarFrases();
      entrarPergunta(0);
      return 1;
    }
    return 1;
  }
  if (S.modo == FESCUI_DIGITANDO || S.modo == FESCUI_QR) {
    if (k == SDLK_LEFT || k == SDLK_UP) { moverFocoFluxo(-1); return 1; }
    if (k == SDLK_RIGHT || k == SDLK_DOWN) { moverFocoFluxo(1); return 1; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER ||
        (k == SDLK_SPACE && S.modo != FESCUI_DIGITANDO)) {
      if (e->key.repeat != 0) return 1;
      if (S.foco == FESCUI_FOCO_ALT && modoAlternativo() != FESC_ENT_NENHUM) {
        iniciarEntrada(modoAlternativo());
      } else {
        if (S.modo == FESCUI_DIGITANDO) st_fechar(ST_BUSCA);
        fecharCelular();
        zerarFrases();
        entrarPergunta(0);
      }
      return 1;
    }
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_DELETE ||
        e->key.keysym.scancode == NV_SCANCODE_BACK ||
        (k == SDLK_BACKSPACE && S.modo != FESCUI_DIGITANDO)) {
      if (e->key.repeat != 0) return 1;
      if (S.modo == FESCUI_DIGITANDO) st_fechar(ST_BUSCA);
      fecharCelular();
      zerarFrases();
      entrarPergunta(0);
      return 1;
    }
    return 1;
  }
  if (S.modo == FESCUI_PREVIA || S.modo == FESCUI_AVISO) {
    if (k == SDLK_LEFT || k == SDLK_UP) { moverFocoFluxo(-1); return 1; }
    if (k == SDLK_RIGHT || k == SDLK_DOWN) { moverFocoFluxo(1); return 1; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      if (e->key.repeat != 0) return 1;
      if (S.modo == FESCUI_PREVIA && S.foco == FESCUI_FOCO_CONFIRMAR) {
        confirmarPrevia();
      } else if (S.foco == FESCUI_FOCO_VOZ && modoRefazer() != FESC_ENT_NENHUM) {
        iniciarEntrada(modoRefazer());
      } else if (S.foco == FESCUI_FOCO_ALT && modoAlternativo() != FESC_ENT_NENHUM) {
        iniciarEntrada(modoAlternativo());
      } else {
        zerarFrases();
        entrarPergunta(0);
      }
      return 1;
    }
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
        k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
      if (e->key.repeat != 0) return 1;
      zerarFrases();
      entrarPergunta(0);
      return 1;
    }
    return 1;
  }
  if (S.modo == FESCUI_ESPERA) {
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
        k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
        k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
      if (e->key.repeat != 0) return 1;
      farolescolha_rede_cancelar();
      S.redeGen = 0;
      zerarFrases();
      entrarPergunta(0);
      return 1;
    }
    return 1;
  }

  if (S.modo != FESCUI_PERGUNTA && S.modo != FESCUI_RESULTADO) return 0;
  // TOCTOU: o OK vale para o cartaz EXIBIDO sob o foco (o que a pessoa viu),
  // nao para o que ocupar o slot depois de garantirResultado() recontar o
  // catalogo vivo — captura ANTES de qualquer recomposicao.
  if (S.modo == FESCUI_RESULTADO && S.foco >= 0 && S.foco < S.nRes) {
    okExibido = S.resPed[S.foco];
    okTem = 1;
  }
  okVoltar = (S.modo == FESCUI_RESULTADO && S.foco == FESCUI_FOCO_VOLTAR);
  garantirResultado();
  n = nFocaveis();
  if (k == SDLK_LEFT) {
    if (S.foco > 0) S.foco--;
    else if (S.foco == FESCUI_FOCO_VOZ) S.foco = FESCUI_FOCO_VOLTAR;
    return 1;
  }
  if (k == SDLK_RIGHT) {
    if (S.foco >= 0 && S.foco + 1 < n) S.foco++;
    else if (S.foco == FESCUI_FOCO_VOLTAR && podeUsarVoz()) S.foco = FESCUI_FOCO_VOZ;
    return 1;
  }
  if (k == SDLK_DOWN) {
    if (S.foco >= 0) { S.focoAntesVoltar = S.foco; S.foco = FESCUI_FOCO_VOLTAR; }
    return 1;
  }
  if (k == SDLK_UP) {
    if ((S.foco == FESCUI_FOCO_VOLTAR || S.foco == FESCUI_FOCO_VOZ) && n > 0)
      S.foco = (S.focoAntesVoltar >= 0 && S.focoAntesVoltar < n) ? S.focoAntesVoltar : 0;
    return 1;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    unsigned r = 0;
    int idx;
    const CatItem *ci;
    if (e->key.repeat != 0) return 1;     // segurar o OK nunca responde duas
    if (okTem) {
      // O cartaz exibido ainda existe no catalogo vivo e continua o mesmo?
      // Sumiu ou trocou de obra: so atualiza a tela (garantirResultado ja
      // correu acima) e consome — NUNCA abre outro titulo nem dispara Voltar.
      idx = farolachaui_resolver_pedido(&okExibido, &r);
      ci = idx >= 0 ? cat_item(idx) : NULL;
      if (ci && farolachaui_pedido_bate(ci, &okExibido)) {
        okExibido.rev = r;
        okExibido.indice = idx;
        S.pedido = okExibido;
        S.pedidoTem = 1;
        if (pediuAbrir) *pediuAbrir = 1;
      }
      return 1;
    }
    // Voltar so quando o foco JA estava nele antes do OK — nunca porque o
    // ultimo cartaz sumiu no instante da tecla.
    if (okVoltar && S.foco == FESCUI_FOCO_VOLTAR) {
      voltarUmPasso(pediuSair);
      return 1;
    }
    return okNoFoco(pediuSair);
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    if (e->key.repeat != 0) return 1;     // segurar o Voltar desfaz um passo so
    voltarUmPasso(pediuSair);
    return 1;
  }
  return 0;
}

void farolescolhaui_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas(), i;
  (void)agora;
  garantirResultado();
  if (S.modo == FESCUI_PERGUNTA && S.foco == FESCUI_FOCO_VOZ && !podeUsarVoz())
    S.foco = FESCUI_FOCO_VOLTAR;

  if (S.modo == FESCUI_OUVINDO) {
    char buf[NV_ARTE_URL_MAX + 1];
    int valorRec = 0;
    int r = st_ler_valor(ST_BUSCA, buf, sizeof buf, &valorRec);
    float nv = st_nivel();
    S.nivelVoz = red ? nv : anim_mola(S.nivelVoz, nv, dt, 18.0f);
    if (r == ST_TEXTO) {
      if (valorRec) copiarUtf8Seguro(S.parcial, sizeof S.parcial, buf);
    } else if (r == ST_FIM) {
      if (!farolescolhaui_aplicar_frase(buf)) {
        st_fechar(ST_BUSCA);
        zerarFrases();
        entrarPergunta(0);
      }
    } else if (r == ST_PEDE_TECLADO) {
      st_ime_tipo(ST_IME_TEXTO);
      if (st_ime_disponivel() && st_ime_abrir(ST_BUSCA, S.parcial, FAROL_ESCOLHA_FRASE_MAX)) {
        S.modo = FESCUI_DIGITANDO;
        S.entradaModo = FESC_ENT_IME;
        S.foco = FESCUI_FOCO_VOLTAR;
        zerarAnim();
      } else {
        st_fechar(ST_BUSCA);
        zerarFrases();
        entrarPergunta(0);
      }
    } else if (r == ST_CANCELOU) {
      st_fechar(ST_BUSCA);
      zerarFrases();
      entrarPergunta(0);
    }
  } else if (S.modo == FESCUI_DIGITANDO) {
    char buf[NV_ARTE_URL_MAX + 1];
    int valorRec = 0;
    int r = st_ler_valor(ST_BUSCA, buf, sizeof buf, &valorRec);
    if (r == ST_TEXTO) {
      if (valorRec) copiarUtf8Seguro(S.parcial, sizeof S.parcial, buf);
    } else if (r == ST_FIM) {
      (void)farolescolhaui_aplicar_frase(buf);
    } else if (r == ST_CANCELOU) {
      const char *av = st_aviso();
      st_fechar(ST_BUSCA);
      zerarFrases();
      if (av && av[0]) entrarAviso(av);
      else entrarPergunta(0);
    }
  } else if (S.modo == FESCUI_QR) {
    char buf[CEL_CORPO_MAX + 1];
    if (celular_pegar(buf, sizeof buf)) {
      (void)farolescolhaui_aplicar_frase(buf);
      memset(buf, 0, sizeof buf);
    } else {
      int est = celular_estado();
      if (est == CEL_EXPIROU) {
        fecharCelular();
        zerarFrases();
        entrarAviso("O endereço expirou. Aperte OK para gerar outro.");
      } else if (est == CEL_FALHOU) {
        fecharCelular();
        zerarFrases();
        entrarAviso("Endereço bloqueado por tentativas erradas. Aperte OK para gerar outro.");
      } else if (est == CEL_PARADO) {
        fecharCelular();
        zerarFrases();
        entrarAviso("Sem rede local. Conecte a TV ao Wi-Fi e aperte OK.");
      }
    }
  } else if (S.modo == FESCUI_ESPERA) {
    FarolEscRedeInfo info;
    FarolEscRedeEstado st = farolescolha_rede_estado(&info);
    if (S.redeGen == 0) {
      zerarFrases();
      entrarPergunta(0);
    } else if (st == FER_PRONTO && info.geracao == S.redeGen) {
      FarolEscRedeResp resp;
      int ok = farolescolha_rede_resposta(S.redeGen, &resp);
      S.redeGen = 0;
      zerarFrases();
      if (!ok) {
        entrarPergunta(0);
      } else {
        int aplicados = 0;
        for (i = 0; i < resp.nRespostas && aplicados < FESC_PERG_MAX; i++) {
          if (farolescolha_pre_responder(&S.motor, resp.respostas[i].perg,
                                         resp.respostas[i].opcaoId))
            aplicados++;
        }
        if (!farolescolha_terminou(&S.motor) && resp.temFalta)
          (void)farolescolha_priorizar(&S.motor, resp.faltaPerg);
        S.nRes = 0;
        seguir(0);
      }
    } else if (st != FER_BUSCANDO || info.geracao != S.redeGen) {
      S.redeGen = 0;
      zerarFrases();
      entrarPergunta(0);
    }
  }

  for (i = 0; i < FESC_OPC_MAX; i++) {
    float alvo = S.foco == i ? 1.0f : 0.0f;
    S.anim[i] = red ? alvo : anim_mola(S.anim[i], alvo, dt,
                                       alvo > S.anim[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  {
    float aVol  = S.foco == FESCUI_FOCO_VOLTAR    ? 1.0f : 0.0f;
    float aVoz  = S.foco == FESCUI_FOCO_VOZ       ? 1.0f : 0.0f;
    float aConf = S.foco == FESCUI_FOCO_CONFIRMAR ? 1.0f : 0.0f;
    float aAlt  = S.foco == FESCUI_FOCO_ALT       ? 1.0f : 0.0f;
    S.animVoltar = red ? aVol  : anim_mola(S.animVoltar, aVol,  dt, aVol  > S.animVoltar ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    S.animVoz    = red ? aVoz  : anim_mola(S.animVoz,    aVoz,  dt, aVoz  > S.animVoz    ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    S.animConf   = red ? aConf : anim_mola(S.animConf,   aConf, dt, aConf > S.animConf   ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    S.animAlt    = red ? aAlt  : anim_mola(S.animAlt,    aAlt,  dt, aAlt  > S.animAlt    ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    S.pulsoMs    = red ? 0.0f : fmodf(S.pulsoMs + (dt > 0.0f ? dt * 1000.0f : 0.0f), ESC_PULSO_MS);
  }
}

// ------------------------------------------------------------ desenho --
static void txt_em(TxtEstilo est, const char *s, float x, float y, float maxW,
                   int r, int g, int b, float a) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static void pt_foco(int id, int nada) {
  (void)nada;
  if (id == FESCUI_FOCO_VOLTAR || id == FESCUI_FOCO_VOZ ||
      id == FESCUI_FOCO_CONFIRMAR || id == FESCUI_FOCO_ALT) {
    if (S.foco >= 0) S.focoAntesVoltar = S.foco;
    S.foco = id;
  } else if (id >= 0 && id < nFocaveis()) {
    S.foco = id;
  }
}

static void ponto_luz(float x, float y, float a) {
  float r, g, b;
  ajustes_acento(&r, &g, &b);
  gfx_cor((GfxRect){ x, y, 10.0f, 10.0f }, 0.5f, r, g, b, a);
}

static void desenha_card(int i, const CatItem *ci, float x, float y, float a) {
  float f = S.anim[i < FESC_OPC_MAX ? i : 0];
  float esc = 1.0f + 0.05f * f;
  GfxRect p = {
    x - ESC_CARD_W * (esc - 1.0f) * 0.5f,
    y - ESC_CARD_H * (esc - 1.0f) * 0.5f - 6.0f * f,
    ESC_CARD_W * esc,
    ESC_CARD_H * esc
  };
  float raio = 18.0f / p.h;
  const char *arte;
  GLuint tex;

  ponteiro_alvo(x, y, ESC_CARD_W, ESC_CARD_H + 84.0f, pt_foco, NULL, i, 0);
  if (f > 0.01f) botao_luz(p, f, a);
  arte = posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, ci->poster);
  if (!arte || !arte[0]) arte = ci->backdrop[0] ? ci->backdrop : NULL;
  tex = (arte && arte[0]) ? tex_obter_larg(arte, p.w) : 0;
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(p, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    if (arte && arte[0] && !tex_falhou(arte))
      gfx_esqueleto(p, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
    else
      gfx_cor(p, raio, 0.14f, 0.15f, 0.17f, 0.92f * a);
    gfx_icone((GfxRect){ p.x + p.w * 0.5f - 28.0f, p.y + p.h * 0.5f - 28.0f, 56.0f, 56.0f },
              ehSerieTipo(ci->tipo) ? "aj_tv-minimal-play" : "aj_film",
              0.72f, 0.74f, 0.78f, 0.45f * a);
  }
  if (f > 0.01f) {
    float fr, fg, fb;
    botao_cor_foco(&fr, &fg, &fb);
    gfx_anel(p, raio, 3.0f, fr, fg, fb, f * a);
  } else {
    gfx_anel(p, raio, 1.0f, 1, 1, 1, 0.08f * a);
  }
  txt_em(TXT_ILHA_NOME, ci->titulo[0] ? ci->titulo : ci->imdb, x, p.y + p.h + 16.0f,
         ESC_CARD_W, S.foco == i ? 248 : 220, S.foco == i ? 249 : 224,
         S.foco == i ? 252 : 232, a);
  if (ci->meta[0])
    txt_em(TXT_CAPTION, ci->meta, x, p.y + p.h + 50.0f, ESC_CARD_W, 166, 172, 184, a);
}

static void desenha_pergunta(float ox, float oy, float a) {
  int n = farolescolha_n_opcoes(&S.motor), i, perg = farolescolha_pergunta(&S.motor);
  float w[FESC_OPC_MAX], total = 0.0f, x;
  char passo[48];

  snprintf(passo, sizeof passo, i18n(FT_ESC_PASSO), farolescolha_passo(&S.motor),
           farolescolha_de(&S.motor));
  ponto_luz(ox + ESC_MARGEM, oy + 220.0f, a);
  txt_em(TXT_V2_26, passo, ox + ESC_MARGEM + 22.0f, oy + 206.0f, 600.0f, 186, 192, 204, a);
  txt_em(TXT_NOV_TITULO, i18n(farolescolha_pergunta_rotulo(perg)), ox + ESC_MARGEM,
         oy + 128.0f, ESC_LARG, 248, 249, 252, a);

  for (i = 0; i < n; i++) {
    const char *rot = i18n(farolescolha_opcao_rotulo(perg, farolescolha_opcao(&S.motor, i)));
    w[i] = botao_largura(rot, NULL, 1);
    if (w[i] < ESC_PILULA_MIN) w[i] = ESC_PILULA_MIN;
    total += w[i];
  }
  total += (float)(n > 0 ? n - 1 : 0) * ESC_PILULA_GAP;
  if (total > ESC_LARG && total > 0.0f) {   // nunca sai da grade: encolhe por igual
    float k = (ESC_LARG - (float)(n - 1) * ESC_PILULA_GAP) /
              (total - (float)(n - 1) * ESC_PILULA_GAP);
    for (i = 0; i < n; i++) w[i] *= k;
  }
  x = ox + ESC_MARGEM;
  for (i = 0; i < n; i++) {
    const char *rot = i18n(farolescolha_opcao_rotulo(perg, farolescolha_opcao(&S.motor, i)));
    GfxRect r = { x, oy + ESC_PILULA_Y, w[i], BOTAO_H_PRIMARIO };
    botao_pilula(r, rot, NULL, S.anim[i], 1, 0, a);
    ponteiro_alvo(r.x, r.y, r.w, r.h, pt_foco, NULL, i, 0);
    x += w[i] + ESC_PILULA_GAP;
  }
}

static void desenha_resultado(float ox, float oy, float a) {
  int i;
  if (S.nRes == 0) {
    GfxRect pr = { ox + ESC_MARGEM, oy + ESC_PILULA_Y, 820.0f, 160.0f };
    txt_em(TXT_NOV_TITULO, i18n(FESCUI_NADA_EXATO), ox + ESC_MARGEM, oy + 128.0f, ESC_LARG,
           248, 249, 252, a);
    if (ajustes_vidro()) gfx_vidro_painel(pr, 28.0f / pr.h, 0.78f, a);
    else {
      gfx_cor(pr, 28.0f / pr.h, 0.12f, 0.125f, 0.145f, 0.90f * a);
      gfx_anel(pr, 28.0f / pr.h, 1.0f, 1, 1, 1, 0.08f * a);
    }
    gfx_cor((GfxRect){ pr.x + 32.0f, pr.y + 52.0f, 56.0f, 56.0f }, 0.31f, 1, 1, 1, 0.07f * a);
    gfx_icone((GfxRect){ pr.x + 48.0f, pr.y + 68.0f, 24.0f, 24.0f }, "aj_compass",
              0.95f, 0.95f, 0.94f, a);
    txt_em(TXT_V2_26, i18n(FESCUI_NADA_L2), pr.x + 112.0f, pr.y + 64.0f, pr.w - 144.0f,
           232, 235, 242, a);
    return;
  }
  txt_em(TXT_NOV_TITULO, i18n(S.nRes == 1 ? FESCUI_ACHAMOS_1 : FESCUI_ACHAMOS_N),
         ox + ESC_MARGEM, oy + 128.0f, ESC_LARG, 248, 249, 252, a);
  for (i = 0; i < S.nRes; i++) {
    const CatItem *ci = cat_item(S.resIdx[i]);
    if (!ci || !farolachaui_pedido_bate(ci, &S.resPed[i])) continue;
    desenha_card(i, ci, ox + ESC_MARGEM + (float)i * (ESC_CARD_W + ESC_CARD_GAP),
                 oy + ESC_PILULA_Y, a);
  }
}

static void painel_esc(GfxRect r, float raioPx, float a) {
  if (ajustes_vidro()) {
    gfx_vidro_painel(r, raioPx / r.h, 0.78f, a);
  } else {
    gfx_cor(r, raioPx / r.h, 0.12f, 0.125f, 0.145f, 0.90f * a);
    gfx_anel(r, raioPx / r.h, 1.0f, 1, 1, 1, 0.08f * a);
  }
}

static float pulso_esc(void) {
  if (ajustes_animacoes_reduzidas()) return 1.0f;
  return 0.65f + 0.35f * cosf(S.pulsoMs / ESC_PULSO_MS * 6.2831853f);
}

static int prepararQrTextura(const char *url) {
  if (!url || !url[0]) return 0;
  if (S.qrTex && !strcmp(S.qrUrl, url)) return 1;
#if !FESCUI_SEM_GL
  if (!qr_textura(&S.qrTex, url)) return 0;
#else
  {
    Qr q;
    if (!qr_gerar(&q, url)) return 0;
    S.qrTex = 1u;
  }
#endif
  snprintf(S.qrUrl, sizeof S.qrUrl, "%s", url);
  return 1;
}

static void desenha_voz_estado(float ox, float oy, float a) {
  GfxRect pr = { ox + ESC_MARGEM, oy + ESC_PAINEL_Y, ESC_LARG, ESC_PAINEL_H };
  float ar, ag, ab;
  float bx = ox + ESC_MARGEM, by = oy + ESC_VOZ_BTN_Y;
  const char *tit = "";
  const char *sub = "";
  int ref = modoRefazer();
  int alt = modoAlternativo();

  ajustes_acento(&ar, &ag, &ab);
  if (S.modo == FESCUI_OUVINDO) {
    tit = i18n(FT_OUVINDO);
    sub = i18n(FT_BUSCA_FRASE_PROMPT);
  } else if (S.modo == FESCUI_DIGITANDO) {
    tit = i18n(FT_ACAO_TECLADO_TV);
    sub = i18n(FT_BUSCA_FRASE_PROMPT);
  } else if (S.modo == FESCUI_QR) {
    tit = i18n(FT_DIGITE_CELULAR);
    sub = i18n("Aponte a câmera do celular. Mesma rede Wi-Fi, vale por 5 minutos.");
  } else if (S.modo == FESCUI_PREVIA) {
    tit = i18n(FT_VOCE_DISSE);
    sub = i18n(FT_CONFIRMA_DESCARTA);
  } else if (S.modo == FESCUI_ESPERA) {
    tit = i18n(FT_ESCOLHENDO);
    sub = i18n(FT_CONFIRMA_DESCARTA);
  } else {
    tit = i18n(FT_BUSCA_FRASE);
    sub = i18n(FT_BUSCA_FRASE_EXEMPLO);
  }

  txt_em(TXT_NOV_TITULO, tit, ox + ESC_MARGEM, oy + 128.0f, ESC_LARG, 248, 249, 252, a);
  if (sub[0])
    txt_em(TXT_V2_26, sub, ox + ESC_MARGEM, oy + 206.0f, 1500.0f, 186, 192, 204, a);

  painel_esc(pr, 28.0f, a);

  if (S.modo == FESCUI_OUVINDO) {
    float p = pulso_esc();
    float nv = S.nivelVoz > 0.05f ? S.nivelVoz : 0.25f * p;
    GfxRect micBox = { pr.x + 48.0f, pr.y + (pr.h - 84.0f) * 0.5f, 84.0f, 84.0f };
    if (nv > 0.02f) {
      GfxRect halo = { micBox.x - 10.0f, micBox.y - 10.0f, micBox.w + 20.0f, micBox.h + 20.0f };
      gfx_rect(halo, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.45f * nv * a);
    }
    gfx_cor(micBox, 0.5f, ar, ag, ab, 0.22f * a);
    gfx_anel(micBox, 0.5f, 2.0f, ar, ag, ab, p * a);
    gfx_icone((GfxRect){ micBox.x + 22.0f, micBox.y + 22.0f, 40.0f, 40.0f },
              "aj_mic", 0.96f, 0.96f, 0.98f, a);
    if (S.parcial[0]) {
      txt_bloco(TXT_ILHA_TITULO, S.parcial, 248, 249, 252,
                pr.x + 168.0f, pr.y + 84.0f, pr.w - 220.0f, 48.0f, a, 3);
    } else {
      txt_em(TXT_V2_26, i18n(FT_BUSCA_FRASE_EXEMPLO),
             pr.x + 168.0f, pr.y + (pr.h - 32.0f) * 0.5f, pr.w - 220.0f,
             186, 192, 204, a);
    }
  } else if (S.modo == FESCUI_DIGITANDO) {
    float p = pulso_esc();
    const char *av = st_aviso();
    GfxRect kbBox = { pr.x + 48.0f, pr.y + (pr.h - 84.0f) * 0.5f, 84.0f, 84.0f };
    gfx_cor(kbBox, 0.36f, ar, ag, ab, 0.18f * a);
    gfx_anel(kbBox, 0.36f, 2.0f, ar, ag, ab, p * a);
    gfx_icone((GfxRect){ kbBox.x + 22.0f, kbBox.y + 22.0f, 40.0f, 40.0f },
              "aj_keyboard", 0.96f, 0.96f, 0.98f, a);
    if (S.parcial[0]) {
      txt_bloco(TXT_ILHA_TITULO, S.parcial, 248, 249, 252,
                pr.x + 168.0f, pr.y + 72.0f, pr.w - 220.0f, 48.0f, a, 3);
    } else {
      txt_em(TXT_V2_26, i18n(FT_BUSCA_FRASE_EXEMPLO),
             pr.x + 168.0f, pr.y + (av && av[0] ? 104.0f : (pr.h - 32.0f) * 0.5f),
             pr.w - 220.0f, 186, 192, 204, a);
    }
    if (av && av[0]) {
      txt_em(TXT_CAPTION, i18n(av),
             pr.x + 168.0f, pr.y + pr.h - 60.0f, pr.w - 220.0f,
             186, 192, 204, a);
    }
  } else if (S.modo == FESCUI_QR) {
    const char *u = celular_url();
    float moldLado = ESC_QR_LADO + 2.0f * ESC_QR_MOLD;
    float qx = pr.x + 48.0f;
    float qy = pr.y + (pr.h - moldLado) * 0.5f;
    float tx = qx + moldLado + 48.0f;
    float tw = pr.x + pr.w - tx - 48.0f;
    if (u && u[0] && prepararQrTextura(u)) {
      gfx_cor((GfxRect){ qx, qy, moldLado, moldLado }, 0.06f, 1.0f, 1.0f, 1.0f, a);
      gfx_tex_aspect_atual = 0.0f;
      gfx_rect((GfxRect){ qx + ESC_QR_MOLD, qy + ESC_QR_MOLD, ESC_QR_LADO, ESC_QR_LADO },
               S.qrTex, GFX_SNAP, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, a);
    }
    gfx_icone((GfxRect){ tx, pr.y + 58.0f, 32.0f, 32.0f },
              "aj_smartphone", ar, ag, ab, a);
    txt_em(TXT_ILHA_TITULO, i18n(FT_DIGITE_CELULAR),
           tx + 44.0f, pr.y + 52.0f, tw - 44.0f, 248, 249, 252, a);
    if (u && u[0]) {
      txt_em(TXT_V2_26, u, tx, pr.y + 132.0f, tw, 232, 236, 244, a);
    }
    txt_em(TXT_V2_26, i18n(FT_BUSCA_FRASE_EXEMPLO),
           tx, pr.y + 196.0f, tw, 186, 192, 204, a);
  } else if (S.modo == FESCUI_PREVIA) {
    GfxRect micBox = { pr.x + 48.0f, pr.y + 52.0f, 72.0f, 72.0f };
    gfx_cor(micBox, 0.36f, 1, 1, 1, 0.08f * a);
    gfx_icone((GfxRect){ micBox.x + 18.0f, micBox.y + 18.0f, 36.0f, 36.0f },
              iconeEntrada(ref), 0.95f, 0.95f, 0.96f, a);
    txt_bloco(TXT_ILHA_TITULO, S.frase, 248, 249, 252,
              pr.x + 156.0f, pr.y + 52.0f, pr.w - 208.0f, 52.0f, a, 3);
    txt_em(TXT_CAPTION, i18n(FT_CONFIRMA_DESCARTA),
           pr.x + 156.0f, pr.y + pr.h - 54.0f, pr.w - 208.0f, 166, 172, 184, a);
  } else if (S.modo == FESCUI_ESPERA) {
    float p = pulso_esc();
    GfxRect dot = { pr.x + 56.0f, pr.y + 72.0f, 20.0f, 20.0f };
    gfx_cor(dot, 0.5f, ar, ag, ab, p * a);
    txt_em(TXT_ILHA_TITULO, i18n(FT_ESCOLHENDO),
           pr.x + 98.0f, pr.y + 54.0f, pr.w - 150.0f, 248, 249, 252, a);
    txt_bloco(TXT_V2_26, S.frase, 186, 192, 204,
              pr.x + 98.0f, pr.y + 132.0f, pr.w - 150.0f, 34.0f, a, 3);
  } else {
    GfxRect box = { pr.x + 48.0f, pr.y + 56.0f, 64.0f, 64.0f };
    gfx_cor(box, 0.32f, 1, 1, 1, 0.07f * a);
    gfx_icone((GfxRect){ box.x + 16.0f, box.y + 16.0f, 32.0f, 32.0f },
              iconeEntrada(ref), 0.95f, 0.95f, 0.94f, a);
    txt_bloco(TXT_ILHA_TITULO, i18n(S.aviso), 243, 242, 239,
              pr.x + 144.0f, pr.y + 56.0f, pr.w - 196.0f, 48.0f, a, 3);
  }

  if (S.modo == FESCUI_OUVINDO || S.modo == FESCUI_ESPERA) {
    const char *rotV = i18n(FT_ACAO_VOLTAR);
    GfxRect rV = { bx, by, botao_largura(rotV, "aj_arrow-left", 1), BOTAO_H_PRIMARIO };
    botao_pilula(rV, rotV, "aj_arrow-left", S.animVoltar, 1, 0, a);
    ponteiro_alvo(rV.x, rV.y, rV.w, rV.h, pt_foco, NULL, FESCUI_FOCO_VOLTAR, 0);
  } else if (S.modo == FESCUI_DIGITANDO || S.modo == FESCUI_QR) {
    const char *rotV = i18n(FT_ACAO_VOLTAR);
    GfxRect rV = { bx, by, botao_largura(rotV, "aj_arrow-left", 1), BOTAO_H_PRIMARIO };
    float x = rV.x + rV.w + BOTAO_GAP;
    botao_pilula(rV, rotV, "aj_arrow-left", S.animVoltar, 1, 0, a);
    ponteiro_alvo(rV.x, rV.y, rV.w, rV.h, pt_foco, NULL, FESCUI_FOCO_VOLTAR, 0);
    if (alt != FESC_ENT_NENHUM) {
      const char *rotA = i18n(rotuloEntrada(alt));
      const char *icA = iconeEntrada(alt);
      GfxRect rA = { x, by + 8.0f, botao_largura(rotA, icA, 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rA, rotA, icA, S.animAlt, 0, 0, a);
      ponteiro_alvo(rA.x, rA.y, rA.w, rA.h, pt_foco, NULL, FESCUI_FOCO_ALT, 0);
    }
  } else if (S.modo == FESCUI_PREVIA) {
    const char *rotOk = i18n(FT_ACAO_PRONTO);
    const char *rotVol = i18n(FT_ACAO_VOLTAR);
    GfxRect rOk = { bx, by, botao_largura(rotOk, "aj_check", 1), BOTAO_H_PRIMARIO };
    float x = rOk.x + rOk.w + BOTAO_GAP;
    botao_pilula(rOk, rotOk, "aj_check", S.animConf, 1, 0, a);
    ponteiro_alvo(rOk.x, rOk.y, rOk.w, rOk.h, pt_foco, NULL, FESCUI_FOCO_CONFIRMAR, 0);
    if (ref != FESC_ENT_NENHUM) {
      const char *rotRef = i18n(rotuloEntrada(ref));
      const char *icRef = iconeEntrada(ref);
      GfxRect rFal = { x, by + 8.0f, botao_largura(rotRef, icRef, 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rFal, rotRef, icRef, S.animVoz, 0, 0, a);
      ponteiro_alvo(rFal.x, rFal.y, rFal.w, rFal.h, pt_foco, NULL, FESCUI_FOCO_VOZ, 0);
      x = rFal.x + rFal.w + BOTAO_GAP;
    }
    if (alt != FESC_ENT_NENHUM) {
      const char *rotAlt = i18n(rotuloEntrada(alt));
      const char *icAlt = iconeEntrada(alt);
      GfxRect rAlt = { x, by + 8.0f, botao_largura(rotAlt, icAlt, 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rAlt, rotAlt, icAlt, S.animAlt, 0, 0, a);
      ponteiro_alvo(rAlt.x, rAlt.y, rAlt.w, rAlt.h, pt_foco, NULL, FESCUI_FOCO_ALT, 0);
      x = rAlt.x + rAlt.w + BOTAO_GAP;
    }
    {
      GfxRect rVol = { x, by + 8.0f, botao_largura(rotVol, "aj_arrow-left", 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rVol, rotVol, "aj_arrow-left", S.animVoltar, 0, 0, a);
      ponteiro_alvo(rVol.x, rVol.y, rVol.w, rVol.h, pt_foco, NULL, FESCUI_FOCO_VOLTAR, 0);
    }
  } else {
    float x = bx;
    const char *rotVol = i18n(FT_ACAO_VOLTAR);
    int temAcao = (ref != FESC_ENT_NENHUM);
    if (temAcao) {
      const char *rotRef = i18n(rotuloEntrada(ref));
      const char *icRef = iconeEntrada(ref);
      GfxRect rFal = { x, by, botao_largura(rotRef, icRef, 1), BOTAO_H_PRIMARIO };
      botao_pilula(rFal, rotRef, icRef, S.animVoz, 1, 0, a);
      ponteiro_alvo(rFal.x, rFal.y, rFal.w, rFal.h, pt_foco, NULL, FESCUI_FOCO_VOZ, 0);
      x = rFal.x + rFal.w + BOTAO_GAP;
    }
    if (alt != FESC_ENT_NENHUM) {
      const char *rotAlt = i18n(rotuloEntrada(alt));
      const char *icAlt = iconeEntrada(alt);
      int prim = !temAcao;
      GfxRect rAlt = {
        x, prim ? by : by + 8.0f,
        botao_largura(rotAlt, icAlt, prim),
        prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO
      };
      botao_pilula(rAlt, rotAlt, icAlt, S.animAlt, prim, 0, a);
      ponteiro_alvo(rAlt.x, rAlt.y, rAlt.w, rAlt.h, pt_foco, NULL, FESCUI_FOCO_ALT, 0);
      x = rAlt.x + rAlt.w + BOTAO_GAP;
      temAcao = 1;
    }
    {
      GfxRect rVol = {
        x, temAcao ? by + 8.0f : by,
        botao_largura(rotVol, "aj_arrow-left", temAcao ? 0 : 1),
        temAcao ? BOTAO_H_SECUNDARIO : BOTAO_H_PRIMARIO
      };
      botao_pilula(rVol, rotVol, "aj_arrow-left", S.animVoltar, temAcao ? 0 : 1, 0, a);
      ponteiro_alvo(rVol.x, rVol.y, rVol.w, rVol.h, pt_foco, NULL, FESCUI_FOCO_VOLTAR, 0);
    }
  }
}

void farolescolhaui_desenhar(float ox, float oy, float alpha, Uint32 agora) {
  char kick[160];
  const char *rotV = i18n(FT_ACAO_VOLTAR);
  int primario = S.modo == FESCUI_RESULTADO && S.nRes == 0;
  GfxRect rV;
  (void)agora;
  if (S.modo != FESCUI_PERGUNTA && S.modo != FESCUI_RESULTADO &&
      S.modo != FESCUI_OUVINDO && S.modo != FESCUI_DIGITANDO &&
      S.modo != FESCUI_QR && S.modo != FESCUI_PREVIA &&
      S.modo != FESCUI_ESPERA && S.modo != FESCUI_AVISO)
    return;
  garantirResultado();
  snprintf(kick, sizeof kick, "%s › %s", i18n(FT_NOME), i18n(FT_ACHAR_O_QUE_VER));
  txt_em(TXT_CAPTION, kick, ox + ESC_MARGEM, oy + 96.0f, 1500.0f, 150, 156, 168, alpha);
  if (S.modo == FESCUI_OUVINDO || S.modo == FESCUI_DIGITANDO ||
      S.modo == FESCUI_QR || S.modo == FESCUI_PREVIA ||
      S.modo == FESCUI_ESPERA || S.modo == FESCUI_AVISO) {
    desenha_voz_estado(ox, oy, alpha);
    return;
  }
  if (S.modo == FESCUI_PERGUNTA) desenha_pergunta(ox, oy, alpha);
  else desenha_resultado(ox, oy, alpha);
  rV = (GfxRect){ ox + ESC_MARGEM, oy + ESC_VOLTAR_Y, botao_largura(rotV, "aj_arrow-left", primario),
                  primario ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO };
  botao_pilula(rV, rotV, "aj_arrow-left", S.animVoltar, primario, 0, alpha);
  ponteiro_alvo(rV.x, rV.y, rV.w, rV.h, pt_foco, NULL, FESCUI_FOCO_VOLTAR, 0);
  if (podeUsarVoz()) {
    int prim = modoPrimario();
    const char *rotFal = i18n(rotuloEntrada(prim));
    const char *icFal = iconeEntrada(prim);
    GfxRect rMic = {
      rV.x + rV.w + BOTAO_GAP,
      oy + ESC_VOLTAR_Y,
      botao_largura(rotFal, icFal, 0),
      BOTAO_H_SECUNDARIO
    };
    botao_pilula(rMic, rotFal, icFal, S.animVoz, 0, 0, alpha);
    ponteiro_alvo(rMic.x, rMic.y, rMic.w, rMic.h, pt_foco, NULL, FESCUI_FOCO_VOZ, 0);
  }
}

// ------------------------------------------------------------ pedidos --
int farolescolhaui_pediu_abrir(FarolAchaPedido *pedido) {
  if (!S.pedidoTem) return 0;
  if (pedido) *pedido = S.pedido;
  S.pedidoTem = 0;
  memset(&S.pedido, 0, sizeof S.pedido);
  return 1;
}

int farolescolhaui_pediu_fechar(void) {
  int f = S.fecharTem;
  S.fecharTem = 0;
  return f;
}

// ------------------------------------------------------------ inspecao --
int farolescolhaui_modo(void) { return S.modo; }
int farolescolhaui_foco(void) { return S.foco; }
int farolescolhaui_n_candidatos(void) { return S.nCand; }
const FarolEsc *farolescolhaui_motor(void) { return &S.motor; }
int farolescolhaui_n_resultados(void) { return S.modo == FESCUI_RESULTADO ? S.nRes : 0; }
int farolescolhaui_resultado_indice(int i) {
  return (S.modo == FESCUI_RESULTADO && i >= 0 && i < S.nRes) ? S.resIdx[i] : -1;
}
int farolescolhaui_saida(void) { return S.saida; }
int farolescolhaui_voz_visivel(void) { return podeUsarVoz(); }
const char *farolescolhaui_rotulo_botao(void) { return rotuloEntrada(modoPrimario()); }
const char *farolescolhaui_icone_botao(void) { return iconeEntrada(modoPrimario()); }
const char *farolescolhaui_frase(void) { return S.frase; }
const char *farolescolhaui_parcial(void) { return S.parcial; }
const char *farolescolhaui_aviso(void) { return S.aviso; }

void farolescolhaui_teste_hora(int hora) { S.testeHora = hora; }
void farolescolhaui_teste_infantil(int infantil) { S.testeInfantil = infantil; }
void farolescolhaui_teste_modos(int voz, int ime, int cel) {
  forcaVoz = voz < 0 ? -1 : (voz ? 1 : 0);
  forcaIme = ime < 0 ? -1 : (ime ? 1 : 0);
  forcaCel = cel < 0 ? -1 : (cel ? 1 : 0);
}
unsigned int farolescolhaui_teste_qr_tex(void) { return (unsigned int)S.qrTex; }
