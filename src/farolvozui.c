// Camada de ditado e revisao de frase do Farol (ADENDO-2.1.md #31).
// Ver farolvozui.h. Sem _Thread_local, sem printf/log de frase/perfil.
#include "farolvozui.h"
#include "farolachaui.h"
#include "farolbusca.h"
#include "farolbuscaparse.h"
#include "faroltexto.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "ponteiro.h"
#include "sistexto.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FV_MARGEM   120.0f
#define FV_PAINEL_Y 290.0f
#define FV_PAINEL_W 1680.0f
#define FV_PAINEL_H 320.0f
#define FV_BOTOES_Y 680.0f
#define FV_PULSO_MS 1400.0f

static struct {
  FarolVozEstado estado;
  int            voltaFase;
  int            foco;
  uint64_t       buscaGeracao;
  float          nivelVoz;
  float          pulsoMs;
  float          animConf;
  float          animFalar;
  float          animVoltar;
  char           parcial[FAROL_BUSCA_FRASE_BYTES];
  char           frase[FAROL_BUSCA_FRASE_BYTES];
  char           aviso[200];
} V;

static void zerarFrases(void) {
  memset(V.parcial, 0, sizeof V.parcial);
  memset(V.frase, 0, sizeof V.frase);
  memset(V.aviso, 0, sizeof V.aviso);
}

static void aplicarAnimacoesImediatas(void) {
  V.animConf   = (V.foco == FV_FOCO_CONFIRMAR) ? 1.0f : 0.0f;
  V.animFalar  = (V.foco == FV_FOCO_FALAR)     ? 1.0f : 0.0f;
  V.animVoltar = (V.foco == FV_FOCO_VOLTAR)    ? 1.0f : 0.0f;
}

static int montarBotoesFoco(int ids[3]) {
  int n = 0;
  if (V.estado == FV_OUVINDO || V.estado == FV_BUSCANDO) {
    ids[n++] = FV_FOCO_VOLTAR;
  } else if (V.estado == FV_REVISAO) {
    ids[n++] = FV_FOCO_CONFIRMAR;
    if (st_voz_disponivel()) ids[n++] = FV_FOCO_FALAR;
    ids[n++] = FV_FOCO_VOLTAR;
  } else if (V.estado == FV_AVISO) {
    if (st_voz_disponivel()) ids[n++] = FV_FOCO_FALAR;
    ids[n++] = FV_FOCO_VOLTAR;
  }
  return n;
}

static void moverFoco(int dir) {
  int ids[3], n, i, cur = -1;
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
  int tentativa;
  for (tentativa = 0; tentativa < 3; tentativa++) {
    FarolBuscaInfo info1, info2;
    FarolBuscaItem itens[FAROL_ACHA_UI_MAX];
    unsigned rev = 0;
    if (farolbusca_estado(&info1) != FB_PRONTO) return 0;
    if (V.buscaGeracao != 0 && info1.geracao != V.buscaGeracao) return 0;
    (void)farolbusca_resolver(itens, FAROL_ACHA_UI_MAX, &rev);
    if (farolbusca_estado(&info2) != FB_PRONTO) return 0;
    // R8: garante que cabecalho (info) e resultados vem da mesma geracao.
    if (info1.geracao != info2.geracao) continue;
    farolachaui_abrir_busca(V.frase);
    return 1;
  }
  return 0;
}

static void entrarAviso(const char *msg) {
  V.estado = FV_AVISO;
  snprintf(V.aviso, sizeof V.aviso, "%s",
           (msg && msg[0]) ? msg : "O ditado não respondeu. Tente de novo ou digite.");
  V.foco = st_voz_disponivel() ? FV_FOCO_FALAR : FV_FOCO_VOLTAR;
  if (ajustes_animacoes_reduzidas()) aplicarAnimacoesImediatas();
}

static void iniciarEscuta(void) {
  zerarFrases();
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

static void cancelarTudo(void) {
  if (V.estado == FV_OUVINDO) st_fechar(ST_BUSCA);
  else if (V.estado == FV_BUSCANDO) farolbusca_cancelar();
  V.estado = FV_INATIVO;
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

int farolvozui_disponivel(void) {
  return st_voz_disponivel() ? 1 : 0;
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
  memset(&V, 0, sizeof V);
}

int farolvozui_abrir(int voltaFase) {
  if (!st_voz_disponivel()) return 0;
  farolvozui_reset();
  V.voltaFase = voltaFase;
  iniciarEscuta();
  return V.estado != FV_INATIVO;
}

int farolvozui_abrir_texto(const char *texto, int voltaFase) {
  char norm[FAROL_BUSCA_FRASE_BYTES];
  if (!farolbusca_normalizar_frase(texto, norm, sizeof norm)) return 0;
  farolvozui_reset();
  V.voltaFase = voltaFase;
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
  if (pediuVoltar) *pediuVoltar = 0;
  if (abriuAchados) *abriuAchados = 0;
  if (V.estado == FV_INATIVO || !e) return;
  if (st_evento(e)) return;
  if (e->type != SDL_KEYDOWN) return;

  k = e->key.keysym.sym;
  if (k == SDLK_LEFT || k == SDLK_UP) {
    moverFoco(-1);
    return;
  }
  if (k == SDLK_RIGHT || k == SDLK_DOWN) {
    moverFoco(1);
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (e->key.repeat) return;
    if (V.estado == FV_OUVINDO || V.estado == FV_BUSCANDO) {
      cancelarTudo();
      if (pediuVoltar) *pediuVoltar = 1;
    } else if (V.estado == FV_REVISAO) {
      if (V.foco == FV_FOCO_CONFIRMAR) {
        confirmarBusca(pediuVoltar, abriuAchados);
      } else if (V.foco == FV_FOCO_FALAR && st_voz_disponivel()) {
        iniciarEscuta();
      } else {
        cancelarTudo();
        if (pediuVoltar) *pediuVoltar = 1;
      }
    } else if (V.estado == FV_AVISO) {
      if (V.foco == FV_FOCO_FALAR && st_voz_disponivel()) {
        iniciarEscuta();
      } else {
        cancelarTudo();
        if (pediuVoltar) *pediuVoltar = 1;
      }
    }
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
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
    float aVol  = (V.foco == FV_FOCO_VOLTAR)    ? 1.0f : 0.0f;
    V.animConf   = anim_mola(V.animConf, aConf, dt, aConf > V.animConf ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    V.animFalar  = anim_mola(V.animFalar, aFal, dt, aFal > V.animFalar ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    V.animVoltar = anim_mola(V.animVoltar, aVol, dt, aVol > V.animVoltar ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    V.pulsoMs    = fmodf(V.pulsoMs + (dt > 0.0f ? dt * 1000.0f : 0.0f), FV_PULSO_MS);
  }

  if (V.estado == FV_OUVINDO) {
    char buf[FAROL_BUSCA_FRASE_BYTES];
    int valorRec = 0;
    int r = st_ler_valor(ST_BUSCA, buf, sizeof buf, &valorRec);
    float nv = st_nivel();
    V.nivelVoz = red ? nv : anim_mola(V.nivelVoz, nv, dt, 18.0f);

    if (r == ST_TEXTO) {
      if (valorRec) memcpy(V.parcial, buf, sizeof V.parcial);
    } else if (r == ST_FIM) {
      char norm[FAROL_BUSCA_FRASE_BYTES];
      st_fechar(ST_BUSCA);
      if (farolbusca_normalizar_frase(buf, norm, sizeof norm)) {
        snprintf(V.frase, sizeof V.frase, "%s", norm);
        V.parcial[0] = '\0';
        V.estado = FV_REVISAO;
        V.foco = FV_FOCO_CONFIRMAR;
        if (red) aplicarAnimacoesImediatas();
      } else {
        entrarAviso("Não entendi. Tente de novo.");
      }
    } else if (r == ST_PEDE_TECLADO) {
      if (st_ime_disponivel() && st_ime_abrir(ST_BUSCA, V.parcial, FAROL_BUSCA_FRASE_MAX)) {
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
        zerarFrases();
        if (pediuVoltar) *pediuVoltar = 1;
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
        zerarFrases();
        if (abriuAchados) *abriuAchados = 1;
      }
    } else if (st == FB_CANCELADO && info.geracao == V.buscaGeracao) {
      V.estado = FV_INATIVO;
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

void farolvozui_desenhar(float ox, float oy, float alpha, Uint32 agora) {
  char kick[160];
  const char *tit = "";
  const char *sub = "";
  GfxRect pr = { ox + FV_MARGEM, oy + FV_PAINEL_Y, FV_PAINEL_W, FV_PAINEL_H };
  float ar, ag, ab;
  float bx = ox + FV_MARGEM;
  float by = oy + FV_BOTOES_Y;
  (void)agora;

  if (V.estado == FV_INATIVO) return;
  ajustes_acento(&ar, &ag, &ab);
  snprintf(kick, sizeof kick, "%s › %s", i18n(FT_NOME), i18n(FT_BUSCA_FRASE));

  if (V.estado == FV_OUVINDO) {
    tit = i18n(FT_OUVINDO);
    sub = i18n(FT_BUSCA_FRASE_PROMPT);
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
  } else if (V.estado == FV_REVISAO) {
    GfxRect micBox = { pr.x + 48.0f, pr.y + 52.0f, 72.0f, 72.0f };
    gfx_cor(micBox, 0.36f, 1, 1, 1, 0.08f * alpha);
    gfx_icone((GfxRect){ micBox.x + 18.0f, micBox.y + 18.0f, 36.0f, 36.0f },
              "aj_mic", 0.95f, 0.95f, 0.96f, alpha);
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
              "aj_mic", 0.95f, 0.95f, 0.94f, alpha);
    txt_bloco(TXT_ILHA_TITULO, i18n(V.aviso), 243, 242, 239,
              pr.x + 144.0f, pr.y + 56.0f, pr.w - 196.0f, 48.0f, alpha, 3);
  }

  if (V.estado == FV_OUVINDO || V.estado == FV_BUSCANDO) {
    const char *rotC = i18n("Cancelar");
    GfxRect rC = { bx, by, botao_largura(rotC, "aj_arrow-left", 1), BOTAO_H_PRIMARIO };
    botao_pilula(rC, rotC, "aj_arrow-left", V.animVoltar, 1, 0, alpha);
    ponteiro_alvo(rC.x, rC.y, rC.w, rC.h, pt_foco_voz, NULL, FV_FOCO_VOLTAR, 0);
  } else if (V.estado == FV_REVISAO) {
    const char *rotOk = i18n("Buscar");
    const char *rotFal = i18n(FT_ACAO_FALAR);
    const char *rotVol = i18n(FT_ACAO_VOLTAR);
    GfxRect rOk = { bx, by, botao_largura(rotOk, "aj_search", 1), BOTAO_H_PRIMARIO };
    float x = rOk.x + rOk.w + BOTAO_GAP;
    botao_pilula(rOk, rotOk, "aj_search", V.animConf, 1, 0, alpha);
    ponteiro_alvo(rOk.x, rOk.y, rOk.w, rOk.h, pt_foco_voz, NULL, FV_FOCO_CONFIRMAR, 0);
    if (st_voz_disponivel()) {
      GfxRect rFal = { x, by + 8.0f, botao_largura(rotFal, "aj_mic", 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rFal, rotFal, "aj_mic", V.animFalar, 0, 0, alpha);
      ponteiro_alvo(rFal.x, rFal.y, rFal.w, rFal.h, pt_foco_voz, NULL, FV_FOCO_FALAR, 0);
      x = rFal.x + rFal.w + BOTAO_GAP;
    }
    {
      GfxRect rVol = { x, by + 8.0f, botao_largura(rotVol, "aj_arrow-left", 0), BOTAO_H_SECUNDARIO };
      botao_pilula(rVol, rotVol, "aj_arrow-left", V.animVoltar, 0, 0, alpha);
      ponteiro_alvo(rVol.x, rVol.y, rVol.w, rVol.h, pt_foco_voz, NULL, FV_FOCO_VOLTAR, 0);
    }
  } else {
    float x = bx;
    const char *rotFal = i18n(FT_ACAO_FALAR);
    const char *rotVol = i18n(FT_ACAO_VOLTAR);
    int temVoz = st_voz_disponivel();
    if (temVoz) {
      GfxRect rFal = { x, by, botao_largura(rotFal, "aj_mic", 1), BOTAO_H_PRIMARIO };
      botao_pilula(rFal, rotFal, "aj_mic", V.animFalar, 1, 0, alpha);
      ponteiro_alvo(rFal.x, rFal.y, rFal.w, rFal.h, pt_foco_voz, NULL, FV_FOCO_FALAR, 0);
      x = rFal.x + rFal.w + BOTAO_GAP;
    }
    {
      GfxRect rVol = {
        x, temVoz ? by + 8.0f : by,
        botao_largura(rotVol, "aj_arrow-left", temVoz ? 0 : 1),
        temVoz ? BOTAO_H_SECUNDARIO : BOTAO_H_PRIMARIO
      };
      botao_pilula(rVol, rotVol, "aj_arrow-left", V.animVoltar, temVoz ? 0 : 1, 0, alpha);
      ponteiro_alvo(rVol.x, rVol.y, rVol.w, rVol.h, pt_foco_voz, NULL, FV_FOCO_VOLTAR, 0);
    }
  }
}
