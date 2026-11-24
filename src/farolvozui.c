// Camada de entrada (voz, IME da TV e QR celular) e revisao de frase do Farol
// (ADENDO-2.1.md #31 e #32). Ver farolvozui.h.
// Sem _Thread_local, sem printf/log de frase/token/perfil.
#include "farolvozui.h"
#include "farolachaui.h"
#include "farolbusca.h"
#include "farolbuscaparse.h"
#include "faroltexto.h"
#include "ajustes.h"
#include "anim.h"
#include "arteurl.h"
#include "botoes.h"
#include "celular.h"
#include "entrada_texto.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "ponteiro.h"
#include "qr.h"
#include "sistexto.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(RESOLVER_TESTE) && !defined(NV_VOZ_TELA_SHOT) && \
    !defined(NV_ACHA_TELA_SHOT) && !defined(NV_FRASE_TV_SHOT) && \
    !defined(NV_LINUX_DESKTOP)
#define FV_SEM_GL 1
#else
#define FV_SEM_GL 0
#include "qrtex.h"
#endif

// Ganchos fracos para testes isolados que nao linkam celular.c / qr.c / gfx.c.
__attribute__((weak)) int celular_disponivel(void) { return 0; }
__attribute__((weak)) int celular_abrir_digitar(const char *t) { (void)t; return 0; }
__attribute__((weak)) void celular_fechar(void) {}
__attribute__((weak)) int celular_estado(void) { return CEL_PARADO; }
__attribute__((weak)) const char *celular_url(void) { return ""; }
__attribute__((weak)) int celular_pegar(char *dst, size_t n) { (void)dst; (void)n; return 0; }
__attribute__((weak)) int qr_gerar(Qr *q, const char *texto) { (void)q; (void)texto; return 0; }
__attribute__((weak)) int texto_sistema_voz_pelo_teclado(void) { return 0; }

#define FV_MARGEM   120.0f
#define FV_PAINEL_Y 290.0f
#define FV_PAINEL_W 1680.0f
#define FV_PAINEL_H 320.0f
#define FV_BOTOES_Y 680.0f
#define FV_PULSO_MS 1400.0f
#define FV_QR_LADO  248.0f
#define FV_QR_MOLD  12.0f

// Overrides deterministicos de teste (preservados em farolvozui_reset, igual teclado_teste_modos).
static int forcaVoz = -1;
static int forcaIme = -1;
static int forcaCel = -1;

static struct {
  FarolVozEstado estado;
  FarolVozModo   modo;
  int            voltaFase;
  int            foco;
  uint64_t       buscaGeracao;
  float          nivelVoz;
  float          pulsoMs;
  float          animConf;
  float          animFalar;
  float          animAlt;
  float          animVoltar;
  int            celAberto;
  int            qrPronto;
  GLuint         qrTex;
  char           qrUrl[QR_MAX_BYTES + 1];
  char           parcial[FAROL_BUSCA_FRASE_BYTES];
  char           frase[FAROL_BUSCA_FRASE_BYTES];
  char           aviso[200];
} V;

static int modoTecladoPorVoz(void) {
  if (forcaVoz >= 0) return 0;
#if defined(NV_ANDROID)
  return 0;
#elif defined(NV_TPK) || defined(__EMSCRIPTEN__)
  return st_voz_disponivel() ? 1 : 0;
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

static FarolVozModo modoPrimario(void) {
  if (modoVoz()) return FV_MODO_VOZ;
  if (modoIme()) return FV_MODO_IME;
  if (modoCel()) return FV_MODO_QR;
  return FV_MODO_NENHUM;
}

static int modoDisponivel(FarolVozModo m) {
  if (m == FV_MODO_VOZ) return modoVoz();
  if (m == FV_MODO_IME) return modoIme();
  if (m == FV_MODO_QR)  return modoCel();
  return 0;
}

static FarolVozModo modoRefazer(void) {
  if (V.modo != FV_MODO_NENHUM && modoDisponivel(V.modo)) return V.modo;
  return modoPrimario();
}

static FarolVozModo modoAlternativo(void) {
  FarolVozModo cur;
  if (modoVoz()) return FV_MODO_NENHUM;
  if (!modoIme() || !modoCel()) return FV_MODO_NENHUM;
  cur = modoRefazer();
  if (cur == FV_MODO_IME) return FV_MODO_QR;
  if (cur == FV_MODO_QR)  return FV_MODO_IME;
  return FV_MODO_NENHUM;
}

static const char *rotuloModo(FarolVozModo m) {
  if (m == FV_MODO_IME) return FT_ACAO_TECLADO_TV;
  if (m == FV_MODO_QR)  return FT_ACAO_DIGITAR_CELULAR;
  return FT_ACAO_FALAR;
}

static const char *iconeModo(FarolVozModo m) {
  if (m == FV_MODO_IME) return "aj_keyboard";
  if (m == FV_MODO_QR)  return "aj_smartphone";
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

static int normalizarFrase200(const char *src, char *dst, size_t cap) {
  char tmp[FAROL_BUSCA_FRASE_BYTES];
  size_t n;
  if (dst && cap) dst[0] = '\0';
  if (!dst || cap < 2) return 0;
  if (!farolbusca_normalizar_frase(src, tmp, sizeof tmp)) return 0;
  n = strlen(tmp);
  if (n + 1 > cap) {
    n = cap - 1;
    while (n > 0 && ((unsigned char)tmp[n] & 0xC0) == 0x80) n--;
    while (n > 0 && tmp[n - 1] == ' ') n--;
    tmp[n] = '\0';
    if (n == 0) return 0;
  }
  memcpy(dst, tmp, n + 1);
  return 1;
}

static void zerarFrases(void) {
  memset(V.parcial, 0, sizeof V.parcial);
  memset(V.frase, 0, sizeof V.frase);
  memset(V.aviso, 0, sizeof V.aviso);
}

static void destruirQrTex(void) {
#if !FV_SEM_GL
  if (V.qrTex) {
    glDeleteTextures(1, &V.qrTex);
    V.qrTex = 0;
  }
#else
  V.qrTex = 0;
#endif
  V.qrUrl[0] = '\0';
  V.qrPronto = 0;
}

static void fecharCelular(void) {
  if (V.celAberto) {
    celular_fechar();
    V.celAberto = 0;
  }
  destruirQrTex();
}

static void aplicarAnimacoesImediatas(void) {
  V.animConf   = (V.foco == FV_FOCO_CONFIRMAR) ? 1.0f : 0.0f;
  V.animFalar  = (V.foco == FV_FOCO_FALAR)     ? 1.0f : 0.0f;
  V.animAlt    = (V.foco == FV_FOCO_ALT)       ? 1.0f : 0.0f;
  V.animVoltar = (V.foco == FV_FOCO_VOLTAR)    ? 1.0f : 0.0f;
}

static int montarBotoesFoco(int ids[4]) {
  int n = 0;
  if (V.estado == FV_OUVINDO || V.estado == FV_BUSCANDO) {
    ids[n++] = FV_FOCO_VOLTAR;
  } else if (V.estado == FV_DIGITANDO || V.estado == FV_QR) {
    ids[n++] = FV_FOCO_VOLTAR;
    if (modoAlternativo() != FV_MODO_NENHUM) ids[n++] = FV_FOCO_ALT;
  } else if (V.estado == FV_REVISAO) {
    ids[n++] = FV_FOCO_CONFIRMAR;
    if (modoRefazer() != FV_MODO_NENHUM) ids[n++] = FV_FOCO_FALAR;
    if (modoAlternativo() != FV_MODO_NENHUM) ids[n++] = FV_FOCO_ALT;
    ids[n++] = FV_FOCO_VOLTAR;
  } else if (V.estado == FV_AVISO) {
    if (modoRefazer() != FV_MODO_NENHUM) ids[n++] = FV_FOCO_FALAR;
    if (modoAlternativo() != FV_MODO_NENHUM) ids[n++] = FV_FOCO_ALT;
    ids[n++] = FV_FOCO_VOLTAR;
  }
  return n;
}

static void moverFoco(int dir) {
  int ids[4], n, i, cur = -1;
  n = montarBotoesFoco(ids);
  if (n <= 0) return;
  for (i = 0; i < n; i++) {
    if (ids[i] == V.foco) { cur = i; break; }
  }
  if (cur < 0) {
    V.foco = ids[0];
    return;
  }
  cur += dir;
  if (cur < 0) cur = 0;
  if (cur >= n) cur = n - 1;
  V.foco = ids[cur];
}

static int abrirAchadosCoerente(void) {
  if (V.buscaGeracao == 0) return 0;
  return farolachaui_abrir_busca_geracao(V.frase, V.buscaGeracao);
}

static void entrarAviso(const char *msg) {
  FarolVozModo ref = modoRefazer();
  V.estado = FV_AVISO;
  snprintf(V.aviso, sizeof V.aviso, "%s",
           (msg && msg[0]) ? msg : "O ditado não respondeu. Tente de novo ou digite.");
  if (ref != FV_MODO_NENHUM) V.foco = FV_FOCO_FALAR;
  else if (modoAlternativo() != FV_MODO_NENHUM) V.foco = FV_FOCO_ALT;
  else V.foco = FV_FOCO_VOLTAR;
  if (ajustes_animacoes_reduzidas()) aplicarAnimacoesImediatas();
}

static void iniciarEscuta(void) {
  fecharCelular();
  if (V.estado == FV_DIGITANDO) st_fechar(ST_BUSCA);
  zerarFrases();
  V.modo = FV_MODO_VOZ;
  V.nivelVoz = 0.0f;
  V.pulsoMs = 0.0f;
  st_voz_teclado_inicial("");
  if (st_voz_iniciar(ST_BUSCA)) {
    V.estado = FV_OUVINDO;
    V.foco = FV_FOCO_VOLTAR;
    if (ajustes_animacoes_reduzidas()) aplicarAnimacoesImediatas();
  } else {
    const char *av = st_aviso();
    entrarAviso(av && av[0] ? av : "O ditado não respondeu. Tente de novo ou digite.");
  }
}

static void iniciarIme(void) {
  int ok;
  fecharCelular();
  if (V.estado == FV_OUVINDO || V.estado == FV_DIGITANDO) st_fechar(ST_BUSCA);
  zerarFrases();
  V.modo = FV_MODO_IME;
  V.nivelVoz = 0.0f;
  V.pulsoMs = 0.0f;
  if (modoTecladoPorVoz()) {
    st_voz_teclado_inicial("");
    ok = st_voz_iniciar(ST_BUSCA);
  } else {
    st_ime_tipo(ST_IME_TEXTO);
    ok = st_ime_abrir(ST_BUSCA, "", FAROL_BUSCA_FRASE_MAX);
  }
  if (ok) {
    V.estado = FV_DIGITANDO;
    V.foco = FV_FOCO_VOLTAR;
    if (ajustes_animacoes_reduzidas()) aplicarAnimacoesImediatas();
  } else {
    const char *av = st_aviso();
    entrarAviso(av && av[0] ? av : "O teclado do sistema não abriu.");
  }
}

static void iniciarQr(void) {
  const char *u;
  Qr q;
  if (V.estado == FV_OUVINDO || V.estado == FV_DIGITANDO) st_fechar(ST_BUSCA);
  fecharCelular();
  zerarFrases();
  V.modo = FV_MODO_QR;
  V.nivelVoz = 0.0f;
  V.pulsoMs = 0.0f;
  if (!celular_abrir_digitar(FT_DIGITE_CELULAR)) {
    entrarAviso("Sem rede local. Conecte a TV ao Wi-Fi e aperte OK.");
    return;
  }
  V.celAberto = 1;
  u = celular_url();
  if (!u || !u[0] || !qr_gerar(&q, u)) {
    fecharCelular();
    entrarAviso("Sem rede local. Conecte a TV ao Wi-Fi e aperte OK.");
    return;
  }
  V.qrPronto = 1;
  V.estado = FV_QR;
  V.foco = FV_FOCO_VOLTAR;
  if (ajustes_animacoes_reduzidas()) aplicarAnimacoesImediatas();
}

static void iniciarModo(FarolVozModo m) {
  if (m == FV_MODO_VOZ) iniciarEscuta();
  else if (m == FV_MODO_IME) iniciarIme();
  else if (m == FV_MODO_QR)  iniciarQr();
}

static void cancelarTudo(void) {
  if (V.estado == FV_OUVINDO || V.estado == FV_DIGITANDO) st_fechar(ST_BUSCA);
  else if (V.estado == FV_BUSCANDO) farolbusca_cancelar();
  fecharCelular();
  V.estado = FV_INATIVO;
  V.modo = FV_MODO_NENHUM;
  V.buscaGeracao = 0;
  V.nivelVoz = 0.0f;
  zerarFrases();
}

static void confirmarBusca(int *pediuVoltar, int *abriuAchados) {
  FarolBuscaInfo info;
  if (!V.frase[0]) {
    cancelarTudo();
    if (pediuVoltar) *pediuVoltar = 1;
    return;
  }
  if (!farolbusca_pedir(V.frase)) {
    entrarAviso("Não entendi. Tente de novo.");
    return;
  }
  farolbusca_estado(&info);
  V.buscaGeracao = info.geracao;
  if (info.estado == FB_PRONTO) {
    if (abrirAchadosCoerente()) {
      V.estado = FV_INATIVO;
      V.modo = FV_MODO_NENHUM;
      zerarFrases();
      if (abriuAchados) *abriuAchados = 1;
      return;
    }
  }
  V.estado = FV_BUSCANDO;
  V.pulsoMs = 0.0f;
  V.foco = FV_FOCO_VOLTAR;
  if (ajustes_animacoes_reduzidas()) aplicarAnimacoesImediatas();
}

void farolvozui_teste_modos(int voz, int ime, int cel) {
  forcaVoz = voz < 0 ? -1 : (voz ? 1 : 0);
  forcaIme = ime < 0 ? -1 : (ime ? 1 : 0);
  forcaCel = cel < 0 ? -1 : (cel ? 1 : 0);
}

int farolvozui_disponivel(void) {
  return (modoVoz() || modoIme() || modoCel()) ? 1 : 0;
}

const char *farolvozui_rotulo_botao(void) {
  return rotuloModo(modoPrimario());
}

const char *farolvozui_icone_botao(void) {
  return iconeModo(modoPrimario());
}

FarolVozModo farolvozui_modo_atual(void) {
  return V.modo;
}

int farolvozui_ativo(void) {
  return V.estado != FV_INATIVO;
}

FarolVozEstado farolvozui_estado(void) {
  return V.estado;
}

int farolvozui_foco(void) {
  return V.foco;
}

int farolvozui_volta_fase(void) {
  return V.voltaFase;
}

const char *farolvozui_frase(void) {
  return V.frase;
}

const char *farolvozui_parcial(void) {
  return V.parcial;
}

const char *farolvozui_aviso(void) {
  return V.aviso;
}

void farolvozui_reset(void) {
  st_fechar(ST_BUSCA);
  if (V.estado == FV_BUSCANDO) farolbusca_cancelar();
  fecharCelular();
  memset(&V, 0, sizeof V);
}

int farolvozui_abrir(int voltaFase) {
  FarolVozModo m = modoPrimario();
  if (m == FV_MODO_NENHUM) return 0;
  farolvozui_reset();
  V.voltaFase = voltaFase;
  iniciarModo(m);
  return V.estado != FV_INATIVO;
}

int farolvozui_abrir_texto(const char *texto, int voltaFase) {
  char norm[FAROL_BUSCA_FRASE_BYTES];
  if (!normalizarFrase200(texto, norm, sizeof norm)) return 0;
  farolvozui_reset();
  V.voltaFase = voltaFase;
  V.modo = modoPrimario();
  snprintf(V.frase, sizeof V.frase, "%s", norm);
  V.estado = FV_REVISAO;
  V.foco = FV_FOCO_CONFIRMAR;
  if (ajustes_animacoes_reduzidas()) aplicarAnimacoesImediatas();
  return 1;
}

void farolvozui_fechar(void) {
  cancelarTudo();
}

void farolvozui_evento(const SDL_Event *e, int *pediuVoltar, int *abriuAchados) {
  SDL_Keycode k;
  int ehVoltar;
  if (pediuVoltar) *pediuVoltar = 0;
  if (abriuAchados) *abriuAchados = 0;
  if (V.estado == FV_INATIVO || !e) return;
  if (st_evento(e)) return;
  if (V.estado == FV_DIGITANDO &&
      (e->type == SDL_TEXTINPUT || e->type == SDL_TEXTEDITING))
    return;
  if (e->type != SDL_KEYDOWN) return;

  k = e->key.keysym.sym;
  if (V.estado == FV_DIGITANDO && k == SDLK_BACKSPACE) return;

  if (k == SDLK_LEFT || k == SDLK_UP) {
    moverFoco(-1);
    return;
  }
  if (k == SDLK_RIGHT || k == SDLK_DOWN) {
    moverFoco(1);
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER ||
      (k == SDLK_SPACE && V.estado != FV_DIGITANDO)) {
    if (e->key.repeat) return;
    if (V.estado == FV_OUVINDO || V.estado == FV_BUSCANDO) {
      cancelarTudo();
      if (pediuVoltar) *pediuVoltar = 1;
    } else if (V.estado == FV_DIGITANDO || V.estado == FV_QR) {
      if (V.foco == FV_FOCO_ALT && modoAlternativo() != FV_MODO_NENHUM) {
        iniciarModo(modoAlternativo());
      } else {
        cancelarTudo();
        if (pediuVoltar) *pediuVoltar = 1;
      }
    } else if (V.estado == FV_REVISAO) {
      if (V.foco == FV_FOCO_CONFIRMAR) {
        confirmarBusca(pediuVoltar, abriuAchados);
      } else if (V.foco == FV_FOCO_FALAR && modoRefazer() != FV_MODO_NENHUM) {
        iniciarModo(modoRefazer());
      } else if (V.foco == FV_FOCO_ALT && modoAlternativo() != FV_MODO_NENHUM) {
        iniciarModo(modoAlternativo());
      } else {
        cancelarTudo();
        if (pediuVoltar) *pediuVoltar = 1;
      }
    } else if (V.estado == FV_AVISO) {
      if (V.foco == FV_FOCO_FALAR && modoRefazer() != FV_MODO_NENHUM) {
        iniciarModo(modoRefazer());
      } else if (V.foco == FV_FOCO_ALT && modoAlternativo() != FV_MODO_NENHUM) {
        iniciarModo(modoAlternativo());
      } else {
        cancelarTudo();
        if (pediuVoltar) *pediuVoltar = 1;
      }
    }
    return;
  }
  ehVoltar = (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_DELETE ||
              e->key.keysym.scancode == NV_SCANCODE_BACK ||
              (k == SDLK_BACKSPACE && V.estado != FV_DIGITANDO));
  if (ehVoltar) {
    if (e->key.repeat) return;
    cancelarTudo();
    if (pediuVoltar) *pediuVoltar = 1;
  }
}

void farolvozui_atualizar(float dt, Uint32 agora, int *abriuAchados, int *pediuVoltar) {
  int red = ajustes_animacoes_reduzidas();
  (void)agora;
  if (abriuAchados) *abriuAchados = 0;
  if (pediuVoltar) *pediuVoltar = 0;
  if (V.estado == FV_INATIVO) return;

  if (red) {
    aplicarAnimacoesImediatas();
    V.pulsoMs = 0.0f;
  } else {
    float aConf = (V.foco == FV_FOCO_CONFIRMAR) ? 1.0f : 0.0f;
    float aFal  = (V.foco == FV_FOCO_FALAR)     ? 1.0f : 0.0f;
    float aAlt  = (V.foco == FV_FOCO_ALT)       ? 1.0f : 0.0f;
    float aVol  = (V.foco == FV_FOCO_VOLTAR)    ? 1.0f : 0.0f;
    V.animConf   = anim_mola(V.animConf, aConf, dt, aConf > V.animConf ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    V.animFalar  = anim_mola(V.animFalar, aFal, dt, aFal > V.animFalar ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    V.animAlt    = anim_mola(V.animAlt, aAlt, dt, aAlt > V.animAlt ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    V.animVoltar = anim_mola(V.animVoltar, aVol, dt, aVol > V.animVoltar ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    V.pulsoMs    = fmodf(V.pulsoMs + (dt > 0.0f ? dt * 1000.0f : 0.0f), FV_PULSO_MS);
  }

  if (V.estado == FV_OUVINDO) {
    char buf[NV_ARTE_URL_MAX + 1];
    int valorRec = 0;
    int r = st_ler_valor(ST_BUSCA, buf, sizeof buf, &valorRec);
    float nv = st_nivel();
    V.nivelVoz = red ? nv : anim_mola(V.nivelVoz, nv, dt, 18.0f);

    if (r == ST_TEXTO) {
      if (valorRec) copiarUtf8Seguro(V.parcial, sizeof V.parcial, buf);
    } else if (r == ST_FIM) {
      char norm[FAROL_BUSCA_FRASE_BYTES];
      st_fechar(ST_BUSCA);
      if (normalizarFrase200(buf, norm, sizeof norm)) {
        snprintf(V.frase, sizeof V.frase, "%s", norm);
        V.parcial[0] = '\0';
        V.estado = FV_REVISAO;
        V.foco = FV_FOCO_CONFIRMAR;
        if (red) aplicarAnimacoesImediatas();
      } else {
        entrarAviso("Não entendi. Tente de novo.");
      }
    } else if (r == ST_PEDE_TECLADO) {
      st_ime_tipo(ST_IME_TEXTO);
      if (st_ime_disponivel() && st_ime_abrir(ST_BUSCA, V.parcial, FAROL_BUSCA_FRASE_MAX)) {
        V.estado = FV_DIGITANDO;
        V.modo = FV_MODO_IME;
        V.foco = FV_FOCO_VOLTAR;
        if (red) aplicarAnimacoesImediatas();
        return;
      }
      st_fechar(ST_BUSCA);
      entrarAviso(st_aviso());
    } else if (r == ST_CANCELOU) {
      const char *av = st_aviso();
      st_fechar(ST_BUSCA);
      if (av && av[0]) {
        entrarAviso(av);
      } else {
        V.estado = FV_INATIVO;
        V.modo = FV_MODO_NENHUM;
        zerarFrases();
        if (pediuVoltar) *pediuVoltar = 1;
      }
    }
    return;
  }

  if (V.estado == FV_DIGITANDO) {
    char buf[NV_ARTE_URL_MAX + 1];
    int valorRec = 0;
    int r = st_ler_valor(ST_BUSCA, buf, sizeof buf, &valorRec);
    if (r == ST_TEXTO) {
      if (valorRec) copiarUtf8Seguro(V.parcial, sizeof V.parcial, buf);
    } else if (r == ST_FIM) {
      char norm[FAROL_BUSCA_FRASE_BYTES];
      st_fechar(ST_BUSCA);
      if (normalizarFrase200(buf, norm, sizeof norm)) {
        snprintf(V.frase, sizeof V.frase, "%s", norm);
        V.parcial[0] = '\0';
        V.estado = FV_REVISAO;
        V.foco = FV_FOCO_CONFIRMAR;
        if (red) aplicarAnimacoesImediatas();
      } else {
        entrarAviso("Não entendi. Tente de novo.");
      }
    } else if (r == ST_CANCELOU) {
      const char *av = st_aviso();
      st_fechar(ST_BUSCA);
      if (av && av[0]) {
        entrarAviso(av);
      } else {
        V.estado = FV_INATIVO;
        V.modo = FV_MODO_NENHUM;
        zerarFrases();
        if (pediuVoltar) *pediuVoltar = 1;
      }
    }
    return;
  }

  if (V.estado == FV_QR) {
    char buf[CEL_CORPO_MAX + 1];
    if (celular_pegar(buf, sizeof buf)) {
      char norm[FAROL_BUSCA_FRASE_BYTES];
      int ok = normalizarFrase200(buf, norm, sizeof norm);
      memset(buf, 0, sizeof buf);
      fecharCelular();
      if (ok) {
        snprintf(V.frase, sizeof V.frase, "%s", norm);
        V.parcial[0] = '\0';
        V.estado = FV_REVISAO;
        V.foco = FV_FOCO_CONFIRMAR;
        if (red) aplicarAnimacoesImediatas();
      } else {
        entrarAviso("Não entendi. Tente de novo.");
      }
      return;
    }
    {
      int est = celular_estado();
      if (est == CEL_EXPIROU) {
        fecharCelular();
        entrarAviso("O endereço expirou. Aperte OK para gerar outro.");
      } else if (est == CEL_FALHOU) {
        fecharCelular();
        entrarAviso("Endereço bloqueado por tentativas erradas. Aperte OK para gerar outro.");
      } else if (est == CEL_PARADO) {
        fecharCelular();
        entrarAviso("Sem rede local. Conecte a TV ao Wi-Fi e aperte OK.");
      }
    }
    return;
  }

  if (V.estado == FV_BUSCANDO) {
    FarolBuscaInfo info;
    FarolBuscaEstado st = farolbusca_estado(&info);
    if (st == FB_PRONTO && info.geracao == V.buscaGeracao) {
      if (abrirAchadosCoerente()) {
        V.estado = FV_INATIVO;
        V.modo = FV_MODO_NENHUM;
        zerarFrases();
        if (abriuAchados) *abriuAchados = 1;
      }
    } else if (st == FB_CANCELADO && info.geracao == V.buscaGeracao) {
      V.estado = FV_INATIVO;
      V.modo = FV_MODO_NENHUM;
      zerarFrases();
      if (pediuVoltar) *pediuVoltar = 1;
    }
  }
}

static void pt_foco_voz(int id, int nada) {
  (void)nada;
  V.foco = id;
}

static void txt_em(TxtEstilo est, const char *s, float x, float y, float maxW,
                   int r, int g, int b, float a) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static void painel_voz(GfxRect r, float raioPx, float a) {
  if (ajustes_vidro()) {
    gfx_vidro_painel(r, raioPx / r.h, 0.78f, a);
  } else {
    gfx_cor(r, raioPx / r.h, 0.12f, 0.125f, 0.145f, 0.90f * a);
    gfx_anel(r, raioPx / r.h, 1.0f, 1, 1, 1, 0.08f * a);
  }
}

static float pulso_voz(void) {
  if (ajustes_animacoes_reduzidas()) return 1.0f;
  return 0.65f + 0.35f * cosf(V.pulsoMs / FV_PULSO_MS * 6.2831853f);
}

static int prepararQrTextura(const char *url) {
  if (!url || !url[0]) return 0;
  if (V.qrTex && !strcmp(V.qrUrl, url)) return 1;
#if !FV_SEM_GL
  if (!qr_textura(&V.qrTex, url)) return 0;
#else
  {
    Qr q;
    if (!qr_gerar(&q, url)) return 0;
    V.qrTex = 1u;
  }
#endif
  snprintf(V.qrUrl, sizeof V.qrUrl, "%s", url);
  return 1;
}

void farolvozui_desenhar(float ox, float oy, float alpha, Uint32 agora) {
  char kick[160];
  const char *tit = "";
  const char *sub = "";
  GfxRect pr = { ox + FV_MARGEM, oy + FV_PAINEL_Y, FV_PAINEL_W, FV_PAINEL_H };
  float ar, ag, ab;
  float bx = ox + FV_MARGEM;
  float by = oy + FV_BOTOES_Y;
  FarolVozModo ref = modoRefazer();
  FarolVozModo alt = modoAlternativo();
  (void)agora;

  if (V.estado == FV_INATIVO) return;
  ajustes_acento(&ar, &ag, &ab);
  snprintf(kick, sizeof kick, "%s › %s", i18n(FT_NOME), i18n(FT_BUSCA_FRASE));

  if (V.estado == FV_OUVINDO) {
    tit = i18n(FT_OUVINDO);
    sub = i18n(FT_BUSCA_FRASE_PROMPT);
  } else if (V.estado == FV_DIGITANDO) {
    tit = i18n(FT_ACAO_TECLADO_TV);
    sub = i18n(FT_BUSCA_FRASE_PROMPT);
  } else if (V.estado == FV_QR) {
    tit = i18n(FT_DIGITE_CELULAR);
    sub = i18n("Aponte a câmera do celular. Mesma rede Wi-Fi, vale por 5 minutos.");
  } else if (V.estado == FV_REVISAO) {
    tit = i18n(FT_VOCE_DISSE);
    sub = i18n(FT_CONFIRMA_DESCARTA);
  } else if (V.estado == FV_BUSCANDO) {
    tit = i18n(FT_BUSCA_FRASE_PROCURANDO);
    sub = i18n(FT_CONFIRMA_DESCARTA);
  } else {
    tit = i18n(FT_BUSCA_FRASE);
    sub = i18n(FT_BUSCA_FRASE_EXEMPLO);
  }

  txt_em(TXT_CAPTION, kick, ox + FV_MARGEM, oy + 96.0f, 1500.0f, 150, 156, 168, alpha);
  txt_em(TXT_NOV_TITULO, tit, ox + FV_MARGEM, oy + 128.0f, 1680.0f, 248, 249, 252, alpha);
  if (sub[0])
    txt_em(TXT_V2_26, sub, ox + FV_MARGEM, oy + 206.0f, 1500.0f, 186, 192, 204, alpha);

  painel_voz(pr, 28.0f, alpha);

  if (V.estado == FV_OUVINDO) {
    float p = pulso_voz();
    float nv = V.nivelVoz > 0.05f ? V.nivelVoz : 0.25f * p;
    GfxRect micBox = { pr.x + 48.0f, pr.y + (pr.h - 84.0f) * 0.5f, 84.0f, 84.0f };
    if (nv > 0.02f) {
      GfxRect halo = { micBox.x - 10.0f, micBox.y - 10.0f, micBox.w + 20.0f, micBox.h + 20.0f };
      gfx_rect(halo, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.45f * nv * alpha);
    }
    gfx_cor(micBox, 0.5f, ar, ag, ab, 0.22f * alpha);
    gfx_anel(micBox, 0.5f, 2.0f, ar, ag, ab, p * alpha);
    gfx_icone((GfxRect){ micBox.x + 22.0f, micBox.y + 22.0f, 40.0f, 40.0f },
              "aj_mic", 0.96f, 0.96f, 0.98f, alpha);
    if (V.parcial[0]) {
      txt_bloco(TXT_ILHA_TITULO, V.parcial, 248, 249, 252,
                pr.x + 168.0f, pr.y + 84.0f, pr.w - 220.0f, 48.0f, alpha, 3);
    } else {
      txt_em(TXT_V2_26, i18n(FT_BUSCA_FRASE_EXEMPLO),
             pr.x + 168.0f, pr.y + (pr.h - 32.0f) * 0.5f, pr.w - 220.0f,
             186, 192, 204, alpha);
    }
  } else if (V.estado == FV_DIGITANDO) {
    float p = pulso_voz();
    const char *av = st_aviso();
    GfxRect kbBox = { pr.x + 48.0f, pr.y + (pr.h - 84.0f) * 0.5f, 84.0f, 84.0f };
    gfx_cor(kbBox, 0.36f, ar, ag, ab, 0.18f * alpha);
    gfx_anel(kbBox, 0.36f, 2.0f, ar, ag, ab, p * alpha);
    gfx_icone((GfxRect){ kbBox.x + 22.0f, kbBox.y + 22.0f, 40.0f, 40.0f },
              "aj_keyboard", 0.96f, 0.96f, 0.98f, alpha);
    if (V.parcial[0]) {
      txt_bloco(TXT_ILHA_TITULO, V.parcial, 248, 249, 252,
                pr.x + 168.0f, pr.y + 72.0f, pr.w - 220.0f, 48.0f, alpha, 3);
    } else {
      txt_em(TXT_V2_26, i18n(FT_BUSCA_FRASE_EXEMPLO),
             pr.x + 168.0f, pr.y + (av && av[0] ? 104.0f : (pr.h - 32.0f) * 0.5f),
             pr.w - 220.0f, 186, 192, 204, alpha);
    }
    if (av && av[0]) {
      txt_em(TXT_CAPTION, i18n(av),
             pr.x + 168.0f, pr.y + pr.h - 60.0f, pr.w - 220.0f,
             186, 192, 204, alpha);
    }
  } else if (V.estado == FV_QR) {
    const char *u = celular_url();
    float moldLado = FV_QR_LADO + 2.0f * FV_QR_MOLD;
    float qx = pr.x + 48.0f;
    float qy = pr.y + (pr.h - moldLado) * 0.5f;
    float tx = qx + moldLado + 48.0f;
    float tw = pr.x + pr.w - tx - 48.0f;
    if (u && u[0] && prepararQrTextura(u)) {
      gfx_cor((GfxRect){ qx, qy, moldLado, moldLado }, 0.06f, 1.0f, 1.0f, 1.0f, alpha);
      gfx_tex_aspect_atual = 0.0f;
      gfx_rect((GfxRect){ qx + FV_QR_MOLD, qy + FV_QR_MOLD, FV_QR_LADO, FV_QR_LADO },
               V.qrTex, GFX_SNAP, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, alpha);
    }
    gfx_icone((GfxRect){ tx, pr.y + 58.0f, 32.0f, 32.0f },
              "aj_smartphone", ar, ag, ab, alpha);
    txt_em(TXT_ILHA_TITULO, i18n(FT_DIGITE_CELULAR),
           tx + 44.0f, pr.y + 52.0f, tw - 44.0f, 248, 249, 252, alpha);
    if (u && u[0]) {
      txt_em(TXT_V2_26, u, tx, pr.y + 132.0f, tw, 232, 236, 244, alpha);
    }
    txt_em(TXT_V2_26, i18n(FT_BUSCA_FRASE_EXEMPLO),
           tx, pr.y + 196.0f, tw, 186, 192, 204, alpha);
  } else if (V.estado == FV_REVISAO) {
    const char *ic = iconeModo(ref);
    GfxRect micBox = { pr.x + 48.0f, pr.y + 52.0f, 72.0f, 72.0f };
    gfx_cor(micBox, 0.36f, 1, 1, 1, 0.08f * alpha);
    gfx_icone((GfxRect){ micBox.x + 18.0f, micBox.y + 18.0f, 36.0f, 36.0f },
              ic, 0.95f, 0.95f, 0.96f, alpha);
    txt_bloco(TXT_ILHA_TITULO, V.frase, 248, 249, 252,
              pr.x + 156.0f, pr.y + 52.0f, pr.w - 208.0f, 52.0f, alpha, 3);
    txt_em(TXT_CAPTION, i18n(FT_CONFIRMA_DESCARTA),
           pr.x + 156.0f, pr.y + pr.h - 54.0f, pr.w - 208.0f, 166, 172, 184, alpha);
  } else if (V.estado == FV_BUSCANDO) {
    float p = pulso_voz();
    GfxRect dot = { pr.x + 56.0f, pr.y + 72.0f, 20.0f, 20.0f };
    gfx_cor(dot, 0.5f, ar, ag, ab, p * alpha);
    txt_em(TXT_ILHA_TITULO, i18n(FT_BUSCA_FRASE_PROCURANDO),
           pr.x + 98.0f, pr.y + 54.0f, pr.w - 150.0f, 248, 249, 252, alpha);
    txt_bloco(TXT_V2_26, V.frase, 186, 192, 204,
              pr.x + 98.0f, pr.y + 132.0f, pr.w - 150.0f, 34.0f, alpha, 3);
  } else {
    GfxRect box = { pr.x + 48.0f, pr.y + 56.0f, 64.0f, 64.0f };
    gfx_cor(box, 0.32f, 1, 1, 1, 0.07f * alpha);
    gfx_icone((GfxRect){ box.x + 16.0f, box.y + 16.0f, 32.0f, 32.0f },
              iconeModo(ref), 0.95f, 0.95f, 0.94f, alpha);
    txt_bloco(TXT_ILHA_TITULO, i18n(V.aviso), 243, 242, 239,
              pr.x + 144.0f, pr.y + 56.0f, pr.w - 196.0f, 48.0f, alpha, 3);
  }

  if (V.estado == FV_OUVINDO || V.estado == FV_BUSCANDO) {
    const char *rotC = i18n("Cancelar");
    GfxRect rC = { bx, by, botao_largura(rotC, "aj_arrow-left", 1), BOTAO_H_PRIMARIO };
    botao_pilula(rC, rotC, "aj_arrow-left", V.animVoltar, 1, 0, alpha);
    ponteiro_alvo(rC.x, rC.y, rC.w, rC.h, pt_foco_voz, NULL, FV_FOCO_VOLTAR, 0);
  } else if (V.estado == FV_DIGITANDO || V.estado == FV_QR) {
    const char *rotC = i18n("Cancelar");
    GfxRect rC = { bx, by, botao_largura(rotC, "aj_arrow-left", 1), BOTAO_H_PRIMARIO };
    float x = rC.x + rC.w + BOTAO_GAP;
    botao_pilula(rC, rotC, "aj_arrow-left", V.animVoltar, 1, 0, alpha);
    ponteiro_alvo(rC.x, rC.y, rC.w, rC.h, pt_foco_voz, NULL, FV_FOCO_VOLTAR, 0);
    if (alt != FV_MODO_NENHUM) {
      const char *rotA = i18n(rotuloModo(alt));
      const char *icA = iconeModo(alt);
      GfxRect rA = { x, by + 8.0f, botao_largura(rotA, icA, 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rA, rotA, icA, V.animAlt, 0, 0, alpha);
      ponteiro_alvo(rA.x, rA.y, rA.w, rA.h, pt_foco_voz, NULL, FV_FOCO_ALT, 0);
    }
  } else if (V.estado == FV_REVISAO) {
    const char *rotOk = i18n("Buscar");
    const char *rotVol = i18n(FT_ACAO_VOLTAR);
    GfxRect rOk = { bx, by, botao_largura(rotOk, "aj_search", 1), BOTAO_H_PRIMARIO };
    float x = rOk.x + rOk.w + BOTAO_GAP;
    botao_pilula(rOk, rotOk, "aj_search", V.animConf, 1, 0, alpha);
    ponteiro_alvo(rOk.x, rOk.y, rOk.w, rOk.h, pt_foco_voz, NULL, FV_FOCO_CONFIRMAR, 0);
    if (ref != FV_MODO_NENHUM) {
      const char *rotRef = i18n(rotuloModo(ref));
      const char *icRef = iconeModo(ref);
      GfxRect rFal = { x, by + 8.0f, botao_largura(rotRef, icRef, 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rFal, rotRef, icRef, V.animFalar, 0, 0, alpha);
      ponteiro_alvo(rFal.x, rFal.y, rFal.w, rFal.h, pt_foco_voz, NULL, FV_FOCO_FALAR, 0);
      x = rFal.x + rFal.w + BOTAO_GAP;
    }
    if (alt != FV_MODO_NENHUM) {
      const char *rotAlt = i18n(rotuloModo(alt));
      const char *icAlt = iconeModo(alt);
      GfxRect rAlt = { x, by + 8.0f, botao_largura(rotAlt, icAlt, 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rAlt, rotAlt, icAlt, V.animAlt, 0, 0, alpha);
      ponteiro_alvo(rAlt.x, rAlt.y, rAlt.w, rAlt.h, pt_foco_voz, NULL, FV_FOCO_ALT, 0);
      x = rAlt.x + rAlt.w + BOTAO_GAP;
    }
    {
      GfxRect rVol = { x, by + 8.0f, botao_largura(rotVol, "aj_arrow-left", 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rVol, rotVol, "aj_arrow-left", V.animVoltar, 0, 0, alpha);
      ponteiro_alvo(rVol.x, rVol.y, rVol.w, rVol.h, pt_foco_voz, NULL, FV_FOCO_VOLTAR, 0);
    }
  } else {
    float x = bx;
    const char *rotVol = i18n(FT_ACAO_VOLTAR);
    int temAcao = (ref != FV_MODO_NENHUM);
    if (temAcao) {
      const char *rotRef = i18n(rotuloModo(ref));
      const char *icRef = iconeModo(ref);
      GfxRect rFal = { x, by, botao_largura(rotRef, icRef, 1), BOTAO_H_PRIMARIO };
      botao_pilula(rFal, rotRef, icRef, V.animFalar, 1, 0, alpha);
      ponteiro_alvo(rFal.x, rFal.y, rFal.w, rFal.h, pt_foco_voz, NULL, FV_FOCO_FALAR, 0);
      x = rFal.x + rFal.w + BOTAO_GAP;
    }
    if (alt != FV_MODO_NENHUM) {
      const char *rotAlt = i18n(rotuloModo(alt));
      const char *icAlt = iconeModo(alt);
      int prim = !temAcao;
      GfxRect rAlt = {
        x, prim ? by : by + 8.0f,
        botao_largura(rotAlt, icAlt, prim),
        prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO
      };
      botao_pilula(rAlt, rotAlt, icAlt, V.animAlt, prim, 0, alpha);
      ponteiro_alvo(rAlt.x, rAlt.y, rAlt.w, rAlt.h, pt_foco_voz, NULL, FV_FOCO_ALT, 0);
      x = rAlt.x + rAlt.w + BOTAO_GAP;
      temAcao = 1;
    }
    {
      GfxRect rVol = {
        x, temAcao ? by + 8.0f : by,
        botao_largura(rotVol, "aj_arrow-left", temAcao ? 0 : 1),
        temAcao ? BOTAO_H_SECUNDARIO : BOTAO_H_PRIMARIO
      };
      botao_pilula(rVol, rotVol, "aj_arrow-left", V.animVoltar, temAcao ? 0 : 1, 0, alpha);
      ponteiro_alvo(rVol.x, rVol.y, rVol.w, rVol.h, pt_foco_voz, NULL, FV_FOCO_VOLTAR, 0);
    }
  }
}
