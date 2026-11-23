#include "farolregra.h"
#include "faroltexto.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static FarolFatos base_fatos(FarolPlat p, int os) {
  FarolFatos f;
  farolregra_fatos_iniciar(&f);
  f.plat = p;
  f.osMaior = os;
  f.addonsQtd = 3;
  return f;
}

typedef struct {
  int alvo;
  FarolPlat plat;
  int os;
  int sintoma;
  void (*armar)(FarolFatos *);
  uint16_t esp_cod;
  uint8_t esp_acao;
  uint8_t esp_alt;
  uint8_t esp_esc;
} LinhaMatriz;

static void m_rede_fora(FarolFatos *f)         { f->offline = 1; }
static void m_rede_nao_chega(FarolFatos *f)    { f->offline = 0; f->erro.classe = FC_REDE; }
static void m_addon_nenhum(FarolFatos *f)      { f->addonsQtd = 0; }
static void m_addon_lento_1(FarolFatos *f)     { f->addonsLentos = 1; }
static void m_addon_lento_3(FarolFatos *f)     { f->addonsLentos = 2; f->addonsMudos = 1; }
static void m_debrid_sem_plano(FarolFatos *f)  { f->debridSemPlano = 1; }
static void m_debrid_desc_rec(FarolFatos *f)   { f->debridRecusa = 1; }
static void m_debrid_desc_403(FarolFatos *f)   { f->fonteDebrid = 1; f->erro.classe = FC_HTTP; f->erro.http = 403; }
static void m_fonte_recusou(FarolFatos *f)     { f->erro.classe = FC_HTTP; f->erro.http = 404; }
static void m_fonte_sem_range(FarolFatos *f)   { f->sondaRange = 200; }
static void m_rede_curta(FarolFatos *f)        { f->paradas5min = 2; f->fonteGBx10 = 200; f->vazaoMbps = 12; }
static void m_tv_sem_memoria(FarolFatos *f)    { f->memLivreMB = 96; f->erro.classe = FC_FORMATO; }
static void m_tv_negou(FarolFatos *f)          { f->tvNegou = 1; }
static void m_audio_dts_conv(FarolFatos *f)    { f->audioRecusado = 1; f->faixaDts = 1; f->conversaoPossivel = 1; }
static void m_audio_outra(FarolFatos *f)       { f->audioRecusado = 1; f->outraFaixa = 1; }
static void m_audio_sem_saida(FarolFatos *f)   { f->audioRecusado = 1; }
static void m_audio_sem_ext(FarolFatos *f)     { f->audioRecusado = 1; f->externoDisponivel = 1; }
static void m_imagem_dv(FarolFatos *f)         { f->fonteDv = 1; f->tvDv = 0; }
static void m_tv_formato(FarolFatos *f)        { f->erro.classe = FC_FORMATO; f->falhasSeguidas = 1; }
static void m_tv_formato_ext(FarolFatos *f)    { f->erro.classe = FC_FORMATO; f->falhasSeguidas = 1; f->externoDisponivel = 1; }
static void m_tv_rep(FarolFatos *f)            { f->erro.classe = FC_FORMATO; f->falhasSeguidas = 2; }
static void m_tv_rep_ext(FarolFatos *f)        { f->erro.classe = FC_FORMATO; f->falhasSeguidas = 2; f->externoDisponivel = 1; }
static void m_player_parou(FarolFatos *f)      { f->erro.classe = FC_PARADO; f->fonteGBx10 = 100; f->vazaoMbps = 60; }
static void m_nada(FarolFatos *f)              { (void)f; }

#define TODAS_PLATS(alvo, sint, fn, cod, acao, alt, esc) \
  { alvo, FP_WEBOS,     6,  sint, fn, cod, acao, alt, esc }, \
  { alvo, FP_TIZEN_WGT, 6,  sint, fn, cod, acao, alt, esc }, \
  { alvo, FP_TIZEN_TPK, 6,  sint, fn, cod, acao, alt, esc }, \
  { alvo, FP_ANDROID,   33, sint, fn, cod, acao, alt, esc }, \
  { alvo, FP_OUTRA,     0,  sint, fn, cod, acao, alt, esc }

static const LinhaMatriz matriz[] = {
  TODAS_PLATS(FAROL_C_REDE_FORA, RES_S_NAO_COMECA, m_rede_fora,
              FAROL_C_REDE_FORA, FAROL_A_TENTAR_DE_NOVO, FAROL_A_RECONECTAR_REDE, 0),
  TODAS_PLATS(FAROL_C_REDE_NAO_CHEGA, RES_S_NAO_COMECA, m_rede_nao_chega,
              FAROL_C_REDE_NAO_CHEGA, FAROL_A_RECONECTAR_REDE, FAROL_A_PROXIMA_FONTE, 1),
  TODAS_PLATS(FAROL_C_ADDON_NENHUM, RES_S_NAO_COMECA, m_addon_nenhum,
              FAROL_C_ADDON_NENHUM, FAROL_A_ABRIR_FONTES, FAROL_A_NENHUMA, 0),
  TODAS_PLATS(FAROL_C_ADDON_LENTO, RES_S_TRAVA, m_addon_lento_1,
              FAROL_C_ADDON_LENTO, FAROL_A_ISOLAR_ADDON, FAROL_A_NENHUMA, 0),
  TODAS_PLATS(FAROL_C_ADDON_LENTO, RES_S_TRAVA, m_addon_lento_3,
              FAROL_C_ADDON_LENTO, FAROL_A_ISOLAR_ADDON, FAROL_A_LIMPAR_CACHE, 1),
  TODAS_PLATS(FAROL_C_DEBRID_SEM_PLANO, RES_S_NAO_COMECA, m_debrid_sem_plano,
              FAROL_C_DEBRID_SEM_PLANO, FAROL_A_ABRIR_DEBRID, FAROL_A_NENHUMA, 0),
  TODAS_PLATS(FAROL_C_DEBRID_DESCONECTOU, RES_S_NAO_COMECA, m_debrid_desc_rec,
              FAROL_C_DEBRID_DESCONECTOU, FAROL_A_RECONECTAR_DEBRID, FAROL_A_NENHUMA, 0),
  TODAS_PLATS(FAROL_C_DEBRID_DESCONECTOU, RES_S_NAO_COMECA, m_debrid_desc_403,
              FAROL_C_DEBRID_DESCONECTOU, FAROL_A_RECONECTAR_DEBRID, FAROL_A_NENHUMA, 0),
  TODAS_PLATS(FAROL_C_FONTE_RECUSOU, RES_S_NAO_COMECA, m_fonte_recusou,
              FAROL_C_FONTE_RECUSOU, FAROL_A_PROXIMA_FONTE, FAROL_A_NENHUMA, 1),
  TODAS_PLATS(FAROL_C_FONTE_SEM_RANGE, RES_S_NAO_COMECA, m_fonte_sem_range,
              FAROL_C_FONTE_SEM_RANGE, FAROL_A_PROXIMA_FONTE, FAROL_A_NENHUMA, 0),
  TODAS_PLATS(FAROL_C_REDE_CURTA, RES_S_TRAVA, m_rede_curta,
              FAROL_C_REDE_CURTA, FAROL_A_FONTE_MENOR, FAROL_A_BUFFER_MAIOR, 0),

  // TV_SEM_MEMORIA: vale em webOS, TPK e Android; no WASM e FP_OUTRA cai em TV_FORMATO.
  { FAROL_C_TV_SEM_MEMORIA, FP_WEBOS,     6,  RES_S_NAO_COMECA, m_tv_sem_memoria,
    FAROL_C_TV_SEM_MEMORIA, FAROL_A_ALIVIAR_REABRIR, FAROL_A_LIMPAR_CACHE, 1 },
  { FAROL_C_TV_SEM_MEMORIA, FP_TIZEN_WGT, 6,  RES_S_NAO_COMECA, m_tv_sem_memoria,
    FAROL_C_TV_FORMATO, FAROL_A_PROXIMA_FONTE, FAROL_A_NENHUMA, 1 },
  { FAROL_C_TV_SEM_MEMORIA, FP_TIZEN_TPK, 6,  RES_S_NAO_COMECA, m_tv_sem_memoria,
    FAROL_C_TV_SEM_MEMORIA, FAROL_A_ALIVIAR_REABRIR, FAROL_A_LIMPAR_CACHE, 1 },
  { FAROL_C_TV_SEM_MEMORIA, FP_ANDROID,   33, RES_S_NAO_COMECA, m_tv_sem_memoria,
    FAROL_C_TV_SEM_MEMORIA, FAROL_A_ALIVIAR_REABRIR, FAROL_A_LIMPAR_CACHE, 1 },
  { FAROL_C_TV_SEM_MEMORIA, FP_OUTRA,     0,  RES_S_NAO_COMECA, m_tv_sem_memoria,
    FAROL_C_TV_FORMATO, FAROL_A_PROXIMA_FONTE, FAROL_A_NENHUMA, 1 },

  // TV_NEGOU_PLAYER: so webOS (LS2) e TPK (ResourceConflict); demais caem em NADA_ACHADO.
  { FAROL_C_TV_NEGOU_PLAYER, FP_WEBOS,     6,  RES_S_NAO_COMECA, m_tv_negou,
    FAROL_C_TV_NEGOU_PLAYER, FAROL_A_ENVIAR_REGISTRO, FAROL_A_NENHUMA, 2 },
  { FAROL_C_TV_NEGOU_PLAYER, FP_TIZEN_WGT, 6,  RES_S_NAO_COMECA, m_tv_negou,
    FAROL_C_NADA_ACHADO, FAROL_A_ENVIAR_REGISTRO, FAROL_A_NENHUMA, 2 },
  { FAROL_C_TV_NEGOU_PLAYER, FP_TIZEN_TPK, 6,  RES_S_NAO_COMECA, m_tv_negou,
    FAROL_C_TV_NEGOU_PLAYER, FAROL_A_ENVIAR_REGISTRO, FAROL_A_NENHUMA, 2 },
  { FAROL_C_TV_NEGOU_PLAYER, FP_ANDROID,   33, RES_S_NAO_COMECA, m_tv_negou,
    FAROL_C_NADA_ACHADO, FAROL_A_ENVIAR_REGISTRO, FAROL_A_NENHUMA, 2 },
  { FAROL_C_TV_NEGOU_PLAYER, FP_OUTRA,     0,  RES_S_NAO_COMECA, m_tv_negou,
    FAROL_C_NADA_ACHADO, FAROL_A_ENVIAR_REGISTRO, FAROL_A_NENHUMA, 2 },

  // AUDIO_DTS_SEM_SAIDA: converte em webOS e TPK 6+; indisponivel em WASM, TPK 4-5, Android, Outra.
  { FAROL_C_AUDIO_DTS_SEM_SAIDA, FP_WEBOS,     6,  RES_S_SOM, m_audio_dts_conv,
    FAROL_C_AUDIO_DTS_SEM_SAIDA, FAROL_A_CONVERTER_AUDIO, FAROL_A_FONTE_SEM_DTS, 1 },
  { FAROL_C_AUDIO_DTS_SEM_SAIDA, FP_TIZEN_WGT, 6,  RES_S_SOM, m_audio_dts_conv,
    FAROL_C_AUDIO_SEM_SAIDA, FAROL_A_FONTE_SEM_DTS, FAROL_A_NENHUMA, 1 },
  { FAROL_C_AUDIO_DTS_SEM_SAIDA, FP_TIZEN_TPK, 6,  RES_S_SOM, m_audio_dts_conv,
    FAROL_C_AUDIO_DTS_SEM_SAIDA, FAROL_A_CONVERTER_AUDIO, FAROL_A_FONTE_SEM_DTS, 1 },
  { FAROL_C_AUDIO_DTS_SEM_SAIDA, FP_TIZEN_TPK, 4,  RES_S_SOM, m_audio_dts_conv,
    FAROL_C_AUDIO_SEM_SAIDA, FAROL_A_FONTE_SEM_DTS, FAROL_A_NENHUMA, 1 },
  { FAROL_C_AUDIO_DTS_SEM_SAIDA, FP_TIZEN_TPK, 5,  RES_S_SOM, m_audio_dts_conv,
    FAROL_C_AUDIO_SEM_SAIDA, FAROL_A_FONTE_SEM_DTS, FAROL_A_NENHUMA, 1 },
  { FAROL_C_AUDIO_DTS_SEM_SAIDA, FP_ANDROID,   33, RES_S_SOM, m_audio_dts_conv,
    FAROL_C_AUDIO_SEM_SAIDA, FAROL_A_FONTE_SEM_DTS, FAROL_A_NENHUMA, 1 },
  { FAROL_C_AUDIO_DTS_SEM_SAIDA, FP_OUTRA,     0,  RES_S_SOM, m_audio_dts_conv,
    FAROL_C_AUDIO_SEM_SAIDA, FAROL_A_FONTE_SEM_DTS, FAROL_A_NENHUMA, 1 },

  TODAS_PLATS(FAROL_C_AUDIO_OUTRA_FAIXA, RES_S_SOM, m_audio_outra,
              FAROL_C_AUDIO_OUTRA_FAIXA, FAROL_A_TROCAR_FAIXA, FAROL_A_FONTE_SEM_DTS, 0),
  TODAS_PLATS(FAROL_C_AUDIO_SEM_SAIDA, RES_S_SOM, m_audio_sem_saida,
              FAROL_C_AUDIO_SEM_SAIDA, FAROL_A_FONTE_SEM_DTS, FAROL_A_NENHUMA, 1),
  { FAROL_C_AUDIO_SEM_SAIDA, FP_ANDROID, 33, RES_S_SOM, m_audio_sem_ext,
    FAROL_C_AUDIO_SEM_SAIDA, FAROL_A_FONTE_SEM_DTS, FAROL_A_PLAYER_EXTERNO, 1 },

  TODAS_PLATS(FAROL_C_IMAGEM_DV, FAROL_S_CORES, m_imagem_dv,
              FAROL_C_IMAGEM_DV, FAROL_A_FONTE_SEM_DV, FAROL_A_DESLIGAR_HDR, 0),

  TODAS_PLATS(FAROL_C_TV_FORMATO, RES_S_NAO_COMECA, m_tv_formato,
              FAROL_C_TV_FORMATO, FAROL_A_PROXIMA_FONTE, FAROL_A_NENHUMA, 1),
  { FAROL_C_TV_FORMATO, FP_ANDROID, 33, RES_S_NAO_COMECA, m_tv_formato_ext,
    FAROL_C_TV_FORMATO, FAROL_A_PROXIMA_FONTE, FAROL_A_PLAYER_EXTERNO, 1 },

  TODAS_PLATS(FAROL_C_TV_FORMATO_REPETE, RES_S_NAO_COMECA, m_tv_rep,
              FAROL_C_TV_FORMATO_REPETE, FAROL_A_FONTE_OUTRO_FORMATO, FAROL_A_LIMPAR_CACHE, 2),
  { FAROL_C_TV_FORMATO_REPETE, FP_ANDROID, 33, RES_S_NAO_COMECA, m_tv_rep_ext,
    FAROL_C_TV_FORMATO_REPETE, FAROL_A_PLAYER_EXTERNO, FAROL_A_FONTE_OUTRO_FORMATO, 2 },

  TODAS_PLATS(FAROL_C_PLAYER_PAROU, RES_S_TRAVA, m_player_parou,
              FAROL_C_PLAYER_PAROU, FAROL_A_BUFFER_MAIOR, FAROL_A_PROXIMA_FONTE, 2),

  TODAS_PLATS(FAROL_C_NADA_ACHADO, RES_S_NAO_COMECA, m_nada,
              FAROL_C_NADA_ACHADO, FAROL_A_ENVIAR_REGISTRO, FAROL_A_NENHUMA, 2),
};

static int frase_em_faroltexto(const char *s) {
  static const char *const todas[] = {
#define EXP_FT(k) FT_##k,
    FAROL_FRASES(EXP_FT)
#undef EXP_FT
  };
  size_t i;
  if (!s || !*s) return 0;
  for (i = 0; i < sizeof todas / sizeof todas[0]; i++) {
    if (!strcmp(s, todas[i])) return 1;
  }
  return 0;
}

static int token_acao_no_schema(const char *tok) {
  static const char *const validos[] = {
    "proxima_fonte", "reconexao", "sem_dv", "sem_dts", "fonte_menor",
    "converter_ac3", "isolar_addon", "aliviar_memoria", "reconectar_rede",
    "limpar_cache", "buffer_maior", "desligar_hdr", "login_debrid",
    "login_addon", "player_externo",
  };
  size_t i;
  for (i = 0; i < sizeof validos / sizeof validos[0]; i++) {
    if (!strcmp(tok, validos[i])) return 1;
  }
  return 0;
}

static void testar_invariantes_tabela(void) {
  int n = 0, i, c;
  int vistos[FAROL_C_N] = {0};
  const FarolRegra *t = farolregra_tabela(&n);

  assert(t != NULL && n >= 19);
  assert(farolregra_tabela(NULL) == t);

  for (i = 0; i < n; i++) {
    assert(t[i].codigo > FAROL_C_NENHUM && t[i].codigo < FAROL_C_N);
    assert(t[i].estagio < RES_T_N);
    assert(t[i].plats != 0 && (t[i].plats & ~FAROL_P_TODAS) == 0);
    assert(t[i].casa != NULL);
    assert(t[i].l1 != NULL && frase_em_faroltexto(t[i].l1));
    if (t[i].l2 != NULL) assert(frase_em_faroltexto(t[i].l2));
    assert(t[i].acao > FAROL_A_NENHUMA && t[i].acao < FAROL_A_N);
    assert(t[i].acaoAlt < FAROL_A_N);
    assert(t[i].escala <= FAROL_ESC_JUNTO);
    assert(frase_em_faroltexto(farolregra_acao_texto(t[i].acao)));
    if (t[i].acaoAlt != FAROL_A_NENHUMA) {
      assert(frase_em_faroltexto(farolregra_acao_texto(t[i].acaoAlt)));
      assert(token_acao_no_schema(farolregra_acao_token(t[i].acaoAlt)));
    }
    if (i > 0) assert(t[i - 1].estagio <= t[i].estagio);
    if (i < n - 1) {
      assert(t[i].codigo != FAROL_C_NADA_ACHADO);
      // ADENDO-2.1 C: nenhum playbook com conserto possivel pode so pedir registro.
      if (t[i].codigo != FAROL_C_TV_NEGOU_PLAYER) {
        assert(t[i].acao != FAROL_A_ENVIAR_REGISTRO);
        assert(token_acao_no_schema(farolregra_acao_token(t[i].acao)));
      }
    }
    vistos[t[i].codigo] = 1;
  }
  assert(t[n - 1].codigo == FAROL_C_NADA_ACHADO);
  assert(t[n - 1].plats == FAROL_P_TODAS);

  assert(!strcmp(farolregra_codigo_nome(FAROL_C_NENHUM), "NENHUM"));
  assert(!strcmp(farolregra_codigo_nome(FAROL_C_AUDIO_DTS_SEM_SAIDA), "AUDIO_DTS_SEM_SAIDA"));
  assert(!strcmp(farolregra_codigo_nome(-1), "?"));
  assert(!strcmp(farolregra_codigo_nome(FAROL_C_N), "?"));
  for (c = FAROL_C_REDE_FORA; c < FAROL_C_N; c++) {
    assert(vistos[c]);
    assert(strcmp(farolregra_codigo_nome(c), "?") != 0);
  }
  assert(!strcmp(farolregra_acao_texto(FAROL_A_NENHUMA), ""));
  assert(!strcmp(farolregra_acao_texto(-1), ""));
  assert(!strcmp(farolregra_acao_texto(FAROL_A_N), ""));
  assert(!strcmp(farolregra_acao_token(FAROL_A_NENHUMA), ""));
  assert(!strcmp(farolregra_acao_token(FAROL_A_ENVIAR_REGISTRO), ""));
  assert(!strcmp(farolregra_acao_token(-1), ""));
  assert(!strcmp(farolregra_acao_token(FAROL_A_N), ""));
  for (c = FAROL_A_TENTAR_DE_NOVO; c < FAROL_A_N; c++) {
    assert(frase_em_faroltexto(farolregra_acao_texto(c)));
    if (c != FAROL_A_ENVIAR_REGISTRO) {
      assert(token_acao_no_schema(farolregra_acao_token(c)));
    }
  }
}

static void testar_matriz_codigo_plataforma(void) {
  size_t i, total = sizeof matriz / sizeof matriz[0];
  int cobertos[FAROL_C_N][5] = {{0}};

  for (i = 0; i < total; i++) {
    const LinhaMatriz *m = &matriz[i];
    FarolFatos f = base_fatos(m->plat, m->os);
    FarolVeredito v;
    m->armar(&f);
    farolregra_julgar(m->sintoma, &f, &v);
    assert(v.regra != NULL);
    if (v.regra->codigo != m->esp_cod || v.regra->acao != m->esp_acao ||
        v.regra->acaoAlt != m->esp_alt || v.regra->escala != m->esp_esc) {
      fprintf(stderr, "FALHA matriz[%zu] alvo=%s plat=%d os=%d: got (%s acao=%d alt=%d esc=%d), "
              "esperado (%s acao=%d alt=%d esc=%d)\n",
              i, farolregra_codigo_nome(m->alvo), (int)m->plat, m->os,
              farolregra_codigo_nome(v.regra->codigo), v.regra->acao, v.regra->acaoAlt, v.regra->escala,
              farolregra_codigo_nome(m->esp_cod), m->esp_acao, m->esp_alt, m->esp_esc);
      assert(0);
    }
    cobertos[m->alvo][m->plat] = 1;
  }

  for (int c = FAROL_C_REDE_FORA; c < FAROL_C_N; c++) {
    for (int p = FP_WEBOS; p <= FP_OUTRA; p++) {
      assert(cobertos[c][p]);
    }
  }
}

static void testar_ordem_primeira_vence(void) {
  FarolFatos f = base_fatos(FP_WEBOS, 6);
  FarolVeredito v;

  // Tudo quebrado ao mesmo tempo: a cadeia desce na ordem fisica e vai destravando uma a uma.
  f.offline = 1;
  f.addonsQtd = 0;
  f.addonsLentos = 4;
  f.debridSemPlano = 1;
  f.debridRecusa = 1;
  f.erro.classe = FC_REDE;
  f.sondaRange = 200;
  f.paradas5min = 3;
  f.fonteGBx10 = 200;
  f.vazaoMbps = 10;
  f.memLivreMB = 80;
  f.tvNegou = 1;
  f.fonteDv = 1;
  f.tvDv = 0;
  f.falhasSeguidas = 3;
  f.audioRecusado = 1;
  f.faixaDts = 1;
  f.conversaoPossivel = 1;
  f.outraFaixa = 1;

  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_REDE_FORA);

  f.offline = 0;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_ADDON_NENHUM);

  f.addonsQtd = 3;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_ADDON_LENTO && v.regra->escala == 1);

  // Fora do sintoma "demora", ADDON_LENTO nao atravessa a frente de DEBRID/FONTE/TV.
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_DEBRID_SEM_PLANO);

  f.debridSemPlano = 0;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_DEBRID_DESCONECTOU);

  f.debridRecusa = 0;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_REDE_NAO_CHEGA);

  // HTTP 401/403 em host do debrid vence FONTE_RECUSOU; fora do debrid vira FONTE_RECUSOU.
  f.erro.classe = FC_HTTP;
  f.erro.http = 401;
  f.fonteDebrid = 1;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_DEBRID_DESCONECTOU);

  f.fonteDebrid = 0;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_FONTE_RECUSOU);

  f.erro.classe = FC_NENHUMA;
  f.erro.http = 0;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_FONTE_SEM_RANGE);

  f.sondaRange = 206;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_REDE_CURTA);

  // TV_NEGOU_PLAYER (ls2_permissao) nao e mascarado por memLivreMB < 150.
  f.paradas5min = 0;
  f.erro.classe = FC_FORMATO;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_TV_NEGOU_PLAYER);

  // Sem tvNegou, memLivreMB < 150 vence TV_FORMATO_REPETE e TV_FORMATO.
  f.tvNegou = 0;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_TV_SEM_MEMORIA);

  // Memoria boa + sintoma "Cores estranhas": IMAGEM_DV vence TV_FORMATO_REPETE.
  f.memLivreMB = 500;
  farolregra_julgar(FAROL_S_CORES, &f, &v);
  assert(v.regra->codigo == FAROL_C_IMAGEM_DV);

  // Em sintoma normal: TV_FORMATO_REPETE vence TV_FORMATO quando falhasSeguidas >= 2.
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_TV_FORMATO_REPETE);

  f.falhasSeguidas = 1;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_TV_FORMATO);

  // Sem erro de video: cadeia de audio respeita conversao > outra faixa > fonte sem DTS.
  f.erro.classe = FC_NENHUMA;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_DTS_SEM_SAIDA);

  f.conversaoPossivel = 0;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_OUTRA_FAIXA);

  f.outraFaixa = 0;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_SEM_SAIDA);

  f.audioRecusado = 0;
  f.faixaDts = 0;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
}

static void testar_nao_sei(void) {
  int n = 0, i, p;
  const FarolRegra *t = farolregra_tabela(&n);
  FarolFatos tudo_menos1;
  FarolVeredito v;

  memset(&tudo_menos1, 0, sizeof tudo_menos1);
  tudo_menos1.offline = -1;
  tudo_menos1.addonsQtd = -1;
  tudo_menos1.debridSemPlano = -1;
  tudo_menos1.debridRecusa = -1;
  tudo_menos1.tvNegou = -1;
  tudo_menos1.osMaior = -1;
  tudo_menos1.erro.http = -1;
  tudo_menos1.erroIdadeS = -1;
  tudo_menos1.falhasSeguidas = -1;
  tudo_menos1.paradas5min = -1;
  tudo_menos1.addonsLentos = -1;
  tudo_menos1.addonsMudos = -1;
  tudo_menos1.audioRecusado = -1;
  tudo_menos1.faixaDts = -1;
  tudo_menos1.conversaoPossivel = -1;
  tudo_menos1.outraFaixa = -1;
  tudo_menos1.fonteDv = -1;
  tudo_menos1.fonteHdr = -1;
  tudo_menos1.tvDv = -1;
  tudo_menos1.tvHdr = -1;
  tudo_menos1.fonteGBx10 = -1;
  tudo_menos1.vazaoMbps = -1;
  tudo_menos1.memLivreMB = -1;
  tudo_menos1.temOutraFonte = -1;
  tudo_menos1.temFonteMenor = -1;
  tudo_menos1.temFonteSemDv = -1;
  tudo_menos1.temFonteSemDts = -1;
  tudo_menos1.externoDisponivel = -1;
  tudo_menos1.sondaRange = -1;
  tudo_menos1.versaoNova = -1;
  tudo_menos1.fonteMbps = -1;
  tudo_menos1.fonteDebrid = -1;
  tudo_menos1.sintoma = -1;

  for (p = FP_WEBOS; p <= FP_OUTRA; p++) {
    static const FarolClasse classes[] = {
      FC_NENHUMA, FC_REDE, FC_HTTP, FC_FORMATO, FC_AUDIO, FC_SISTEMA, FC_PARADO
    };
    tudo_menos1.plat = (FarolPlat)p;
    for (size_t ci = 0; ci < sizeof classes / sizeof classes[0]; ci++) {
      tudo_menos1.erro.classe = classes[ci];
      tudo_menos1.erro.http = classes[ci] == FC_HTTP ? 403 : -1;
      snprintf(tudo_menos1.erro.cod, sizeof tudo_menos1.erro.cod, "%s",
               classes[ci] == FC_SISTEMA ? "ls2_permissao" : "");
      for (i = 0; i < n - 1; i++) assert(!t[i].casa(&tudo_menos1));
      farolregra_julgar(-1, &tudo_menos1, &v);
      assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
      farolregra_julgar(RES_S_NAO_COMECA, &tudo_menos1, &v);
      assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
      for (int st = 0; st < RES_T_N; st++) assert(v.estado[st] == RES_OK);
      farolregra_julgar(FAROL_S_CORES, &tudo_menos1, &v);
      assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
    }
    tudo_menos1.erro.classe = FC_NENHUMA;
    tudo_menos1.erro.http = -1;
    tudo_menos1.erro.cod[0] = 0;
  }

  // Plataforma fora do enum (ex.: -1) nao gera UB e cai em FP_OUTRA.
  tudo_menos1.plat = (FarolPlat)-1;
  farolregra_julgar(RES_S_NAO_COMECA, &tudo_menos1, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

  // Casos individuais de "nao sei" (-1 e 0 documentado) caindo na regra generica seguinte:
  {
    FarolFatos f = base_fatos(FP_WEBOS, 6);
    // offline = -1 + FC_REDE: nao pode afirmar "a internet esta de pe".
    f.offline = -1;
    f.erro.classe = FC_REDE;
    farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

    // addonsLentos = -1 mesmo com addonsMudos = 5 nao casa ADDON_LENTO.
    f = base_fatos(FP_WEBOS, 6);
    f.addonsLentos = -1;
    f.addonsMudos = 5;
    farolregra_julgar(RES_S_TRAVA, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

    // fonteDebrid = -1 com HTTP 403 cai na generica FONTE_RECUSOU, nunca DEBRID_DESCONECTOU.
    f = base_fatos(FP_WEBOS, 6);
    f.fonteDebrid = -1;
    f.erro.classe = FC_HTTP;
    f.erro.http = 403;
    farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
    assert(v.regra->codigo == FAROL_C_FONTE_RECUSOU);

    // HTTP com status -1 nao casa FONTE_RECUSOU; status 0 (m3_2004 generico) casa.
    f.erro.http = -1;
    farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
    f.erro.http = 0;
    farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
    assert(v.regra->codigo == FAROL_C_FONTE_RECUSOU);

    // sondaRange = -1 (sem resposta / CORS no WASM) e 0 nao casam FONTE_SEM_RANGE.
    f = base_fatos(FP_TIZEN_WGT, 6);
    f.sondaRange = -1;
    farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

    // REDE_CURTA exige vazao e tamanho conhecidos (> 0, nunca 0 ou -1).
    f = base_fatos(FP_WEBOS, 6);
    f.paradas5min = 3;
    f.fonteGBx10 = 200;
    f.vazaoMbps = 0;
    farolregra_julgar(RES_S_TRAVA, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
    f.vazaoMbps = 10;
    f.fonteGBx10 = 0;
    farolregra_julgar(RES_S_TRAVA, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
    f.fonteGBx10 = -1;
    f.fonteMbps = 25;
    farolregra_julgar(RES_S_TRAVA, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

    // memLivreMB = -1 com FC_FORMATO cai na generica TV_FORMATO (PLANO 4).
    f = base_fatos(FP_WEBOS, 6);
    f.memLivreMB = -1;
    f.erro.classe = FC_FORMATO;
    farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
    assert(v.regra->codigo == FAROL_C_TV_FORMATO);

    // IMAGEM_DV: tvDv = -1 ("nao sei") NAO casa IMAGEM_DV; com FC_FORMATO cai em TV_FORMATO.
    f = base_fatos(FP_WEBOS, 6);
    f.fonteDv = 1;
    f.tvDv = -1;
    farolregra_julgar(FAROL_S_CORES, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
    f.erro.classe = FC_FORMATO;
    farolregra_julgar(FAROL_S_CORES, &f, &v);
    assert(v.regra->codigo == FAROL_C_TV_FORMATO);

    // falhasSeguidas = -1 nao casa TV_FORMATO_REPETE; cai em TV_FORMATO.
    f = base_fatos(FP_WEBOS, 6);
    f.erro.classe = FC_FORMATO;
    f.falhasSeguidas = -1;
    farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
    assert(v.regra->codigo == FAROL_C_TV_FORMATO);

    // Android externoDisponivel = -1 cai na acao sem player externo.
    f = base_fatos(FP_ANDROID, 33);
    f.erro.classe = FC_FORMATO;
    f.falhasSeguidas = 2;
    f.externoDisponivel = -1;
    farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
    assert(v.regra->codigo == FAROL_C_TV_FORMATO_REPETE &&
           v.regra->acao == FAROL_A_FONTE_OUTRO_FORMATO);

    // PLAYER_PAROU: vazaoMbps = 0 ou -1 nao pode afirmar "a internet esta rapida".
    f = base_fatos(FP_ANDROID, 33);
    f.erro.classe = FC_PARADO;
    f.vazaoMbps = 0;
    farolregra_julgar(RES_S_TRAVA, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
    f.vazaoMbps = -1;
    farolregra_julgar(RES_S_TRAVA, &f, &v);
    assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

    // Audio com conversaoPossivel = -1 e outraFaixa = -1 cai na generica AUDIO_SEM_SAIDA.
    f = base_fatos(FP_WEBOS, 6);
    f.audioRecusado = 1;
    f.conversaoPossivel = -1;
    f.outraFaixa = -1;
    farolregra_julgar(RES_S_SOM, &f, &v);
    assert(v.regra->codigo == FAROL_C_AUDIO_SEM_SAIDA);

    // Overflow defensivo em addonsLentos + addonsMudos.
    f = base_fatos(FP_WEBOS, 6);
    f.addonsLentos = INT_MAX;
    f.addonsMudos = INT_MAX;
    farolregra_julgar(RES_S_TRAVA, &f, &v);
    assert(v.regra->codigo == FAROL_C_ADDON_LENTO && v.regra->escala == 1);
  }
}

static void testar_conversao_audio_por_plataforma(void) {
  FarolFatos f;
  FarolVeredito v;
  int os;

  // WASM (.wgt): DTS nao toca nativo e NUNCA oferece conversao, mesmo se conversaoPossivel=1.
  f = base_fatos(FP_TIZEN_WGT, 7);
  f.faixaDts = 1;
  f.conversaoPossivel = 1;
  f.outraFaixa = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_OUTRA_FAIXA && v.regra->acao == FAROL_A_TROCAR_FAIXA);
  f.outraFaixa = 0;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_SEM_SAIDA && v.regra->acao == FAROL_A_FONTE_SEM_DTS);

  // TPK 4.0-5.5 (osMaior 4 e 5) e osMaior desconhecido (0, -1): NUNCA oferece conversao.
  const int tpk_sem_conv[] = { -1, 0, 4, 5 };
  for (size_t i = 0; i < sizeof tpk_sem_conv / sizeof tpk_sem_conv[0]; i++) {
    f = base_fatos(FP_TIZEN_TPK, tpk_sem_conv[i]);
    farolerro_ler(FP_TIZEN_TPK, "erro AudioCodecNotSupported", 0, &f.erro);
    f.faixaDts = 1;
    f.conversaoPossivel = 1;
    f.outraFaixa = 1;
    farolregra_julgar(RES_S_SOM, &f, &v);
    assert(v.regra->codigo == FAROL_C_AUDIO_OUTRA_FAIXA);
    f.outraFaixa = 0;
    farolregra_julgar(RES_S_SOM, &f, &v);
    assert(v.regra->codigo == FAROL_C_AUDIO_SEM_SAIDA);
  }

  // TPK 6+ (6, 7, 8, 9): oferece conversao por HLS local quando conversaoPossivel=1.
  for (os = 6; os <= 9; os++) {
    f = base_fatos(FP_TIZEN_TPK, os);
    f.faixaDts = 1;
    f.conversaoPossivel = 1;
    farolregra_julgar(RES_S_SOM, &f, &v);
    assert(v.regra->codigo == FAROL_C_AUDIO_DTS_SEM_SAIDA &&
           v.regra->acao == FAROL_A_CONVERTER_AUDIO);
  }

  // LG webOS: C9 (osMaior=4) toca DTS nativo (sem erro = NADA_ACHADO), mas converte em uMS 200;
  // webOS >= 5 (2020+, toca mudo sem erro) converte direto por faixaDts=1.
  f = base_fatos(FP_WEBOS, 4);
  f.faixaDts = 1;
  f.conversaoPossivel = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  farolerro_ler(FP_WEBOS, "200 Audio Codec Not Supported", 200, &f.erro);
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_DTS_SEM_SAIDA);

  f = base_fatos(FP_WEBOS, 23);
  f.faixaDts = 1;
  f.conversaoPossivel = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_DTS_SEM_SAIDA);

  // Fronteira webOS 5 (NV_DTS_TV_PRIMEIRA_SEM_DTS = 5) vs osMaior desconhecido (0).
  f = base_fatos(FP_WEBOS, 5);
  f.faixaDts = 1;
  f.conversaoPossivel = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_DTS_SEM_SAIDA);
  f = base_fatos(FP_WEBOS, 0);
  f.faixaDts = 1;
  f.conversaoPossivel = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

  // Faixa que NAO e DTS (ex.: TrueHD/FLAC/Opus com ums_200 ou audioRecusado=1):
  // NUNCA pode afirmar "Sua TV nao aceita audio DTS direto" nem ligar conversor DTS.
  f = base_fatos(FP_WEBOS, 6);
  f.audioRecusado = 1;
  f.faixaDts = 0;
  f.conversaoPossivel = 1;
  f.outraFaixa = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_OUTRA_FAIXA &&
         v.regra->acao == FAROL_A_TROCAR_FAIXA);
  f.outraFaixa = 0;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_SEM_SAIDA &&
         v.regra->acao == FAROL_A_FONTE_SEM_DTS);

  f = base_fatos(FP_TIZEN_TPK, 6);
  farolerro_ler(FP_TIZEN_TPK, "erro AudioCodecNotSupported", 0, &f.erro);
  f.faixaDts = 0;
  f.conversaoPossivel = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_SEM_SAIDA);
}

static void testar_idade_erro_e_disjuncoes(void) {
  FarolFatos f;
  FarolVeredito v;

  // Regra de 6 h (RES_REJEITA_S = 21600, tests/resolver.c): recente (<= 6h) conta; > 6h nao.
  f = base_fatos(FP_WEBOS, 6);
  f.erro.classe = FC_HTTP;
  f.erro.http = 404;
  f.erroIdadeS = 240;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_FONTE_RECUSOU && v.estado[RES_T_FONTE] == RES_FALHOU);

  f.erroIdadeS = RES_REJEITA_S;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_FONTE_RECUSOU && v.estado[RES_T_FONTE] == RES_FALHOU);

  f.erroIdadeS = RES_REJEITA_S + 1;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  for (int t = 0; t < RES_T_N; t++) assert(v.estado[t] == RES_OK);

  f.erroIdadeS = 7 * 3600;
  f.erro.classe = FC_REDE;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO && v.estado[RES_T_FONTE] == RES_OK);

  f.erro.classe = FC_FORMATO;
  f.falhasSeguidas = 3;
  f.memLivreMB = 80;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO && v.estado[RES_T_TV] == RES_OK);

  f.erro.classe = FC_PARADO;
  f.fonteGBx10 = 100;
  f.vazaoMbps = 80;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO && v.estado[RES_T_TV] == RES_OK);

  f.erro.classe = FC_AUDIO;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO && v.estado[RES_T_AUDIO] == RES_OK);

  // Estado ao vivo (offline, tvNegou, debridRecusa, audioRecusado) independe de erroIdadeS.
  f.audioRecusado = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_SEM_SAIDA && v.estado[RES_T_AUDIO] == RES_FALHOU);

  // Disjuncoes independentes: -1 em um ramo nao bloqueia outro ramo positivo conhecido.
  // 1) debridRecusa = -1 mas fonteDebrid = 1 + HTTP 401/403 recente -> DEBRID_DESCONECTOU.
  f = base_fatos(FP_WEBOS, 6);
  f.debridRecusa = -1;
  f.fonteDebrid = 1;
  f.erro.classe = FC_HTTP;
  f.erro.http = 401;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_DEBRID_DESCONECTOU);
  f.erro.http = 403;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_DEBRID_DESCONECTOU);
  f.erro.http = 404;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_FONTE_RECUSOU);

  // 2) audioRecusado = -1 mas erro FC_AUDIO recente ou DTS sem suporte na TV -> AUDIO_*.
  f = base_fatos(FP_WEBOS, 6);
  f.audioRecusado = -1;
  farolerro_ler(FP_WEBOS, "200 Audio Codec Not Supported", 200, &f.erro);
  f.faixaDts = 1;
  f.conversaoPossivel = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_DTS_SEM_SAIDA);

  f = base_fatos(FP_WEBOS, 23);
  f.audioRecusado = -1;
  f.faixaDts = 1;
  f.conversaoPossivel = 1;
  farolregra_julgar(RES_S_SOM, &f, &v);
  assert(v.regra->codigo == FAROL_C_AUDIO_DTS_SEM_SAIDA);

  // 3) tvNegou = -1 mas erro recente ls2_permissao / tpk_conflito -> TV_NEGOU_PLAYER (mesmo com pouca RAM).
  f = base_fatos(FP_WEBOS, 6);
  f.tvNegou = -1;
  f.memLivreMB = 90;
  farolerro_ler(FP_WEBOS, NULL, -1027, &f.erro);
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_TV_NEGOU_PLAYER);

  f = base_fatos(FP_TIZEN_TPK, 6);
  f.tvNegou = -1;
  f.memLivreMB = 90;
  farolerro_ler(FP_TIZEN_TPK, "interrompido: ResourceConflict", 0, &f.erro);
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_TV_NEGOU_PLAYER);
}

static void testar_bordas_e_mutantes(void) {
  FarolFatos f;
  FarolVeredito v;

  // Overflow defensivo em taxa_mbps(fonteGBx10 = INT_MAX) em LP64 e ILP32.
  f = base_fatos(FP_WEBOS, 6);
  f.paradas5min = 2;
  f.fonteGBx10 = INT_MAX;
  f.vazaoMbps = 100;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_REDE_CURTA);

  f = base_fatos(FP_WEBOS, 6);
  f.erro.classe = FC_PARADO;
  f.fonteGBx10 = INT_MAX;
  f.vazaoMbps = INT_MAX;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_PLAYER_PAROU);

  // ADDON_LENTO: limiares exatos 0, 1, 2, 3.
  f = base_fatos(FP_WEBOS, 6);
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  f.addonsMudos = 1;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_ADDON_LENTO && v.regra->escala == FAROL_ESC_NUNCA);
  f.addonsLentos = 1;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_ADDON_LENTO && v.regra->escala == FAROL_ESC_NUNCA);
  f.addonsMudos = 2;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_ADDON_LENTO && v.regra->escala == FAROL_ESC_SE_FALHAR);

  // FONTE_RECUSOU: bordas HTTP 399, 400, 599, 600.
  f = base_fatos(FP_WEBOS, 6);
  f.erro.classe = FC_HTTP;
  f.erro.http = 399;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  f.erro.http = 400;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_FONTE_RECUSOU);
  f.erro.http = 599;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_FONTE_RECUSOU);
  f.erro.http = 600;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

  // REDE_CURTA e PLAYER_PAROU: bordas paradas5min, arredondamento GBx10 e fonteMbps explicito.
  f = base_fatos(FP_WEBOS, 6);
  f.paradas5min = 1;
  f.fonteGBx10 = 200;
  f.vazaoMbps = 5;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  f.paradas5min = 2;
  f.fonteGBx10 = 9; // (9+8)/9 = 1 Mbps
  f.vazaoMbps = 1;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  f.fonteGBx10 = 10; // (10+8)/9 = 2 Mbps
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_REDE_CURTA);
  f.fonteMbps = 30;
  f.vazaoMbps = 29;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_REDE_CURTA);
  f.vazaoMbps = 30;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

  f = base_fatos(FP_WEBOS, 6);
  f.erro.classe = FC_PARADO;
  f.vazaoMbps = 9;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  f.vazaoMbps = 10;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_PLAYER_PAROU);
  f.fonteMbps = 25;
  f.vazaoMbps = 24;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  f.vazaoMbps = 25;
  farolregra_julgar(RES_S_TRAVA, &f, &v);
  assert(v.regra->codigo == FAROL_C_PLAYER_PAROU);

  // TV_SEM_MEMORIA: limiar exato MEMLOG_PISO_MB (150) e FC_SISTEMA ums_pipeline.
  f = base_fatos(FP_WEBOS, 6);
  farolerro_ler(FP_WEBOS, "0 com.webos.pipeline.3 is not running", 0, &f.erro);
  f.memLivreMB = 0;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_TV_SEM_MEMORIA);
  f.memLivreMB = 149;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_TV_SEM_MEMORIA);
  f.memLivreMB = 150;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

  // IMAGEM_DV: tvDv = 1, fonteDv = 0, sintoma diferente nao casam.
  f = base_fatos(FP_WEBOS, 6);
  f.fonteDv = 1;
  f.tvDv = 1;
  farolregra_julgar(FAROL_S_CORES, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  f.tvDv = 0;
  f.fonteDv = 0;
  farolregra_julgar(FAROL_S_CORES, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);
  f.fonteDv = 1;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.regra->codigo == FAROL_C_NADA_ACHADO);

#ifdef RES_S_CORES
  // CORRECAO-FAROL6B: FAROL_S_CORES tem valor proprio — com a falha recente
  // tipica de quem abre o Resolver depois de um erro, RES_S_GERAL nao pode
  // casar IMAGEM_DV (antigamente 6 == RES_S_GERAL e casava). E o caminho ao
  // contrario tambem: sintoma "cores estranhas" casa DV mesmo sem erro
  // recente (a regra so pede fonte DV + TV sem DV).
  f = base_fatos(FP_WEBOS, 6);
  f.fonteDv = 1;
  f.tvDv = 0;
  f.erro.classe = FC_HTTP;
  f.erro.http = 403;
  farolregra_julgar(RES_S_GERAL, &f, &v);
  assert(v.regra->codigo != FAROL_C_IMAGEM_DV);
  farolregra_julgar(FAROL_S_CORES, &f, &v);
  assert(v.regra->codigo == FAROL_C_FONTE_RECUSOU);
  f.erro.classe = FC_NENHUMA;
  f.erro.http = 0;
  farolregra_julgar(FAROL_S_CORES, &f, &v);
  assert(v.regra->codigo == FAROL_C_IMAGEM_DV);
  assert(resolverregra_mascara(FAROL_S_CORES) ==
         ((1U << RES_T_FONTE) | (1U << RES_T_TV)));
  assert(resolverregra_mascara(RES_S_GERAL) == 0);

  // CORRECAO-FAROL6B: addonsMotivo e falha recente de classe desconhecida
  // (FC_NENHUMA + token seguro tipo "ums_outro") viram RES_FALHOU na tela —
  // os dois diagnosticos que a ponte resolver -> FarolFatos perdia.
  f = base_fatos(FP_WEBOS, 6);
  f.addonsMotivo = 1;
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.estado[RES_T_ADDONS] == RES_FALHOU);
  f = base_fatos(FP_WEBOS, 6);
  f.haFalha = 1;
  farolerro_ler(FP_WEBOS, "realloc: out of memory", 0, &f.erro);
  assert(f.erro.classe == FC_NENHUMA && f.erro.cod[0] != 0);
  farolregra_julgar(RES_S_NAO_COMECA, &f, &v);
  assert(v.estado[RES_T_FONTE] == RES_FALHOU);
#endif
}

static void testar_regressao_resolver(void) {
  int mask, sint, p;

  // Varredura exaustiva (2^7 combinacoes dos fatos antigos x 8 sintomas x 5
  // plataformas = 5120 casos): o vetor estado[RES_T_N] de farolregra_julgar e
  // identico ao de resolverregra_testar. O bit 6 e o addonsMotivo (antes a
  // ponte perdia), haFalha varia de classe — inclusive FC_NENHUMA com token
  // seguro — em vez de colar 403 em toda falha (o teste era cego), e o laco
  // de sintomas cobre FAROL_S_CORES, o id proprio que nao colide com GERAL.
  for (p = FP_WEBOS; p <= FP_OUTRA; p++) {
    for (sint = 0; sint <= FAROL_S_CORES; sint++) {
      for (mask = 0; mask < 128; mask++) {
        ResolverFatos rf;
        ResolverResultado rr;
        FarolFatos ff = base_fatos((FarolPlat)p, 6);
        FarolVeredito fv;

        memset(&rf, 0, sizeof rf);
        rf.offline = (mask >> 0) & 1;
        rf.addonsQtd = ((mask >> 1) & 1) ? 0 : 3;
        if ((mask >> 2) & 1) strcpy(rf.debridMotivo, "sem plano");
        rf.haFalha = (mask >> 3) & 1;
        if ((mask >> 4) & 1) strcpy(rf.tvMotivo, "recusou");
        if ((mask >> 5) & 1) strcpy(rf.audioMotivo, "sem som");
        if ((mask >> 6) & 1) strcpy(rf.addonsMotivo, "lista vazia");

        ff.offline = rf.offline;
        ff.addonsQtd = rf.addonsQtd;
        ff.debridSemPlano = ((mask >> 2) & 1);
        if (rf.haFalha) {
#ifdef RES_S_CORES
          ff.haFalha = 1;
          switch ((mask >> 1) & 3) {
            case 0:
              ff.erro.classe = FC_NENHUMA;
              snprintf(ff.erro.cod, sizeof ff.erro.cod, "ums_outro");
              break;
            case 1:
              ff.erro.classe = FC_REDE;
              break;
            default:
              ff.erro.classe = FC_HTTP;
              ff.erro.http = (mask & 2) ? 403 : 404;
              break;
          }
#else
          ff.erro.classe = FC_HTTP;
          ff.erro.http = 403;
#endif
        }
        ff.tvNegou = ((mask >> 4) & 1);
        ff.audioRecusado = ((mask >> 5) & 1);
#ifdef RES_S_CORES
        ff.addonsMotivo = ((mask >> 6) & 1);
#endif

        resolverregra_testar(sint, &rf, &rr);
        farolregra_julgar(sint, &ff, &fv);
        assert(memcmp(fv.estado, rr.estado, sizeof rr.estado) == 0);
      }
    }
  }

  // Tambem cobre FC_REDE em haFalha e NULL defensivo.
  {
    ResolverFatos rf;
    ResolverResultado rr;
    FarolFatos ff = base_fatos(FP_WEBOS, 6);
    FarolVeredito fv;
    memset(&rf, 0, sizeof rf);
    rf.addonsQtd = 3;
    rf.haFalha = 1;
    strcpy(rf.tvMotivo, "recusou");
    ff.erro.classe = FC_REDE;
    ff.tvNegou = 1;
    resolverregra_testar(RES_S_NAO_COMECA, &rf, &rr);
    farolregra_julgar(RES_S_NAO_COMECA, &ff, &fv);
    assert(memcmp(fv.estado, rr.estado, sizeof rr.estado) == 0);
    assert(fv.regra->codigo == FAROL_C_REDE_NAO_CHEGA);

    farolregra_julgar(RES_S_NAO_COMECA, NULL, &fv);
    assert(fv.regra->codigo == FAROL_C_NADA_ACHADO);
    farolregra_julgar(RES_S_NAO_COMECA, &ff, NULL);

    // Tambem cobre debridRecusa na paridade de estados.
    memset(&rf, 0, sizeof rf);
    rf.addonsQtd = 3;
    strcpy(rf.debridMotivo, "recusada");
    ff = base_fatos(FP_ANDROID, 33);
    ff.debridRecusa = 1;
    resolverregra_testar(RES_S_NAO_COMECA, &rf, &rr);
    farolregra_julgar(RES_S_NAO_COMECA, &ff, &fv);
    assert(memcmp(fv.estado, rr.estado, sizeof rr.estado) == 0);
  }
}

int main(void) {
  testar_invariantes_tabela();
  testar_matriz_codigo_plataforma();
  testar_ordem_primeira_vence();
  testar_nao_sei();
  testar_conversao_audio_por_plataforma();
  testar_idade_erro_e_disjuncoes();
  testar_bordas_e_mutantes();
  testar_regressao_resolver();
  puts("farol_regra: motor ok");
  return 0;
}
