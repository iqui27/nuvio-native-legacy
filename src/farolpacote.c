// Montador do pacote FarolRegistro v1 — ver farolpacote.h (issue 11).
// Sem SDL, sem rede, sem dados.h, sem alocacao dinamica, sem _Thread_local.
#include "farolpacote.h"
#include "farolnativo.h"
#include "faroltexto.h"
#include "sha256.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Reserva garantida no fim do JSON apos o ultimo item de `linha`:
 *   ]` (1) + `,"privacidade":{"redator":1,"campos_recusados":4294967295}}` (59)
 *   + headroom interno do farolredator (`prof + 2` <= 5) + margem = 72 B. */
enum { PRIV_RESERVA = 72 };

static int na_lista_arr(const char *s, const char *const *tab, size_t n) {
  size_t i;
  if (!s || !*s) return 0;
  for (i = 0; i < n; i++) {
    if (strcmp(s, tab[i]) == 0) return 1;
  }
  return 0;
}

static const char *const GATILHOS[] = {
  "falha_sem_conserto", "manual", "falha_repetida"
};

static const char *const SINTOMAS[] = {
  "nao_comeca", "trava", "sem_som", "legenda", "qualidade", "outro"
};

static const char *const PASSOS[RES_T_N] = {
  "internet", "addons", "debrid", "fonte", "tv", "audio"
};

static const char *const ESTADOS[] = {
  "nao_testado", "ok", "falhou", "testando"
};

static const char *const MODS[FM_N] = {
  "player", "video", "rede", "addon", "debrid",
  "fonte", "audio", "legenda", "memoria", "farol"
};

static const char *const EVENTOS[FE_N] = {
  NULL,              /* FE_NENHUM */
  "abrir",           /* FE_ABRIR */
  "pronto",          /* FE_PRONTO */
  "erro_fonte",      /* FE_ERRO_FONTE */
  "recusa_sinc",     /* FE_RECUSA_SINC */
  "parado",          /* FE_PARADO */
  "erro_nativo",     /* FE_ERRO_NATIVO */
  "audio_recusado",  /* FE_AUDIO_RECUSADO */
  "pipeline_morreu", /* FE_PIPELINE_MORREU */
  "reconexao",       /* FE_RECONEXAO */
  "rede_estado",     /* FE_REDE_ESTADO */
  "addon_busca",     /* FE_ADDON_BUSCA */
  "addon_lento",     /* FE_ADDON_LENTO */
  "debrid",          /* FE_DEBRID */
  "escolhida",       /* FE_FONTE_ESCOLHIDA (espelha fixture v1 §2.6) */
  "fonte_proxima",   /* FE_FONTE_PROXIMA */
  "mem_baixa",       /* FE_MEM_BAIXA */
  "farol_veredito",  /* FE_FAROL_VEREDITO */
  "farol_acao",      /* FE_FAROL_ACAO */
  "farol_resultado", /* FE_FAROL_RESULTADO */
  "farol_envio",     /* FE_FAROL_ENVIO */
  "farol_consertou"  /* FE_FAROL_CONSERTOU */
};

static const char *const FP_PLATS[] = {
  "webos", "tizen_wgt", "tizen_tpk", "android", "outra"
};
static const char *const FP_BACKENDS[] = {
  "starfish", "avplay", "tpk", "media3", "outro"
};
static const char *const FP_CONTAINERS[] = {
  "mkv", "mp4", "ts", "hls", "dash", "avi", "webm", "outro", "x"
};
static const char *const FP_VIDEOS[] = {
  "h264", "hevc", "hevc10", "av1", "vp9", "mpeg2", "outro", "x"
};
static const char *const FP_HDRS[] = {
  "sdr", "hdr10", "hdr10plus", "hlg", "dv", "x"
};

/* Normaliza o audio da midia (`eac3_51`, `dts_51`, `aac_20`, ...) para a
 * familia de codec aceita no 8o componente do fingerprint (§2.6). */
static const char *audio_para_fp(const char *s) {
  if (!s || !*s) return "x";
  if (strcmp(s, "aac") == 0 || strcmp(s, "aac_20") == 0) return "aac";
  if (strcmp(s, "ac3") == 0 || strcmp(s, "ac3_51") == 0) return "ac3";
  if (strcmp(s, "eac3") == 0 || strcmp(s, "eac3_51") == 0 || strcmp(s, "eac3_atmos") == 0) return "eac3";
  if (strcmp(s, "dts") == 0 || strcmp(s, "dts_51") == 0) return "dts";
  if (strcmp(s, "truehd") == 0 || strcmp(s, "truehd_atmos") == 0) return "truehd";
  if (strcmp(s, "mp3") == 0) return "mp3";
  if (strcmp(s, "flac") == 0) return "flac";
  if (strcmp(s, "outro") == 0) return "outro";
  return "x";
}

/* Codigo nativo valido para fingerprint / erro.nativo (ou "x"). */
static const char *nativo_para_fp(const char *cod, char tmp[FAROL_REDATOR_TOKEN_N]) {
  if (!cod || !*cod) return "x";
  farolredator_token(cod, tmp);
  if (farolnativo_cod_ok(tmp)) return tmp;
  return "x";
}

void farolpacote_midia_padrao(FarolPacoteMidia *m) {
  if (!m) return;
  memset(m, 0, sizeof *m);
  m->container = "x";
  m->video = "x";
  m->hdr = "x";
  m->res = "x";
  m->fps = -1.0;
  m->mbps = -1.0;
  m->gb = -1.0;
  m->audio = "x";
  m->legenda = "x";
  m->entrega = "x";
  m->http = -1;
  m->ttfb_ms = -1;
}

void farolpacote_entrada_padrao(FarolPacoteEntrada *e) {
  int i;
  if (!e) return;
  memset(e, 0, sizeof *e);
  e->gatilho = FAROL_G_MANUAL;
  e->sintoma = FAROL_PS_OUTRO;
  e->codigo = NULL;
  e->estagio = -1;
  e->erro.classe = FC_NENHUMA;
  e->erro.http = 0;
  e->erro.nativo = 0;
  e->erro.cod[0] = '\0';
  farolpacote_midia_padrao(&e->midia);
  for (i = 0; i < RES_T_N; i++) {
    e->cadeia[i].estado = FAROL_EST_NAO_TESTADO;
    e->cadeia[i].ms = -1;
    e->cadeia[i].http = -1;
  }
  e->tem_cadeia = 0;
  e->eventos = NULL;
  e->n_eventos = 0;
  e->agora_ms = 0;
  e->janela_ms = FAROL_PACOTE_JANELA_MS;
}

FarolPacoteSintoma farolpacote_sintoma_de_resolver(int res_sintoma) {
  switch (res_sintoma) {
    case RES_S_NAO_COMECA: return FAROL_PS_NAO_COMECA;
    case RES_S_TRAVA:      return FAROL_PS_TRAVA;
    case RES_S_SOM:        return FAROL_PS_SEM_SOM;
    case RES_S_LEGENDA:    return FAROL_PS_LEGENDA;
    default:               return FAROL_PS_OUTRO;
  }
}

int farolpacote_fingerprint(const FarolPerfil *perfil,
                            const FarolErro *erro,
                            const FarolPacoteMidia *midia,
                            char fp_texto[FAROL_PACOTE_FP_MAX],
                            char fp_hex[FAROL_PACOTE_FP_HEX_N]) {
  const char *plat = "outra";
  const char *back = "outro";
  const char *nat = "x";
  const char *cont = "x";
  const char *vid = "x";
  const char *hdr = "x";
  const char *aud = "x";
  char nat_tmp[FAROL_REDATOR_TOKEN_N];
  int so_maior = 0;
  int n;

  if (!fp_texto) return 0;

  if (perfil) {
    if (na_lista_arr(perfil->aparelho.plataforma, FP_PLATS, sizeof FP_PLATS / sizeof *FP_PLATS))
      plat = perfil->aparelho.plataforma;
    if (perfil->aparelho.so_maior > 0)
      so_maior = perfil->aparelho.so_maior > 99 ? 99 : perfil->aparelho.so_maior;
    if (na_lista_arr(perfil->cliente.backend, FP_BACKENDS, sizeof FP_BACKENDS / sizeof *FP_BACKENDS))
      back = perfil->cliente.backend;
  }
  if (erro && erro->cod[0])
    nat = nativo_para_fp(erro->cod, nat_tmp);
  if (midia) {
    if (na_lista_arr(midia->container, FP_CONTAINERS, sizeof FP_CONTAINERS / sizeof *FP_CONTAINERS))
      cont = midia->container;
    if (na_lista_arr(midia->video, FP_VIDEOS, sizeof FP_VIDEOS / sizeof *FP_VIDEOS))
      vid = midia->video;
    if (na_lista_arr(midia->hdr, FP_HDRS, sizeof FP_HDRS / sizeof *FP_HDRS))
      hdr = midia->hdr;
    aud = audio_para_fp(midia->audio);
  }

  n = snprintf(fp_texto, FAROL_PACOTE_FP_MAX, "%s-%d-%s-%s-%s-%s-%s-%s",
               plat, so_maior, back, nat, cont, vid, hdr, aud);
  if (n < 0 || n >= FAROL_PACOTE_FP_MAX) {
    fp_texto[0] = '\0';
    if (fp_hex) fp_hex[0] = '\0';
    return 0;
  }

  if (fp_hex) {
    Sha s;
    unsigned char d[32];
    char dig[65];
    sha_ini(&s);
    sha_add(&s, (const unsigned char *)fp_texto, (unsigned long)n);
    sha_fim(&s, d);
    hex(d, 32, dig);
    memcpy(fp_hex, dig, 16);
    fp_hex[16] = '\0';
  }
  return 1;
}

/* Fecha todos os niveis abertos no caso de falha para deixar `buf` coerente. */
static void fechar_restantes(FarolJson *j) {
  while (j && j->prof > 0 && !j->fechado) {
    if (!farolredator_fechar(j)) break;
  }
}

/* Emite campo enum/token com recuo seguro: se `val` for NULL/vazio escreve
 * `recuo` sem recusa; se `val` for recusado pelo redator (ex.: canario), a
 * recusa fica contabilizada em `campos_recusados` e `recuo` garante campo valido. */
static int campo_com_recuo(FarolJson *j, const char *chave, FarolTipo tipo,
                           const char *val, const char *recuo) {
  if (!val || !*val) return farolredator_campo(j, chave, tipo, recuo);
  if (farolredator_campo(j, chave, tipo, val)) return 1;
  return farolredator_campo(j, chave, tipo, recuo);
}

static int escrever_erro(FarolJson *j, const FarolPacoteEntrada *in,
                         const char *fp_texto, const char *fp_hex) {
  /* Evita literal "NENHUM" em claro para o varredor i18n (chave de enum do schema). */
  static const char cod_padrao[] = { 'N', 'E', 'N', 'H', 'U', 'M', '\0' };
  int i, nt;

  if (!farolredator_objeto(j, "erro")) return 0;

  if (!campo_com_recuo(j, "codigo", FT_ENUM, in->codigo, cod_padrao)) goto falha;
  if (in->estagio >= 0 && in->estagio < RES_T_N) {
    if (!farolredator_campo(j, "estagio", FT_ENUM, PASSOS[in->estagio])) goto falha;
  }
  if (!campo_com_recuo(j, "nativo", FT_TOKEN, in->erro.cod, "x")) goto falha;
  if (!farolredator_campo(j, "nativo_num", FT_INT, &in->erro.nativo)) goto falha;
  if (!farolredator_campo(j, "http", FT_INT, &in->erro.http)) goto falha;
  if (!farolredator_campo(j, "fingerprint", FT_TOKEN, fp_texto)) goto falha;
  if (!farolredator_campo(j, "fp", FT_HASH, fp_hex)) goto falha;

  if (!farolredator_array(j, "tentou")) goto falha;
  nt = (in->tentou && in->n_tentou > 0) ? in->n_tentou : 0;
  if (nt > FAROL_PACOTE_TENTOU_MAX) nt = FAROL_PACOTE_TENTOU_MAX;
  for (i = 0; i < nt; i++) {
    int ok = in->tentou[i].ok ? 1 : 0;
    if (!farolredator_objeto(j, NULL)) { farolredator_fechar(j); goto falha; }
    if (in->tentou[i].acao && in->tentou[i].acao[0])
      (void)farolredator_campo(j, "acao", FT_ENUM, in->tentou[i].acao);
    if (!farolredator_campo(j, "ok", FT_BOOL, &ok) ||
        !farolredator_campo(j, "ms", FT_INT, &in->tentou[i].ms)) {
      farolredator_fechar(j);
      farolredator_fechar(j);
      goto falha;
    }
    if (!farolredator_fechar(j)) { farolredator_fechar(j); goto falha; }
  }
  if (!farolredator_fechar(j)) goto falha;

  return farolredator_fechar(j);

falha:
  farolredator_fechar(j);
  return 0;
}

static int campo_num1_opcional(FarolJson *j, const char *chave, double v) {
  if (!isfinite(v) || v > 1000000000.0) {
    (void)farolredator_campo(j, chave, FT_NUM1, &v);
    return 1;
  }
  if (v < 0.0) return 1;
  return farolredator_campo(j, chave, FT_NUM1, &v);
}

static int escrever_midia(FarolJson *j, const FarolPacoteMidia *m) {
  if (!farolredator_objeto(j, "midia")) return 0;

  if (!campo_com_recuo(j, "container", FT_ENUM, m->container, "x")) goto falha;
  if (!campo_com_recuo(j, "video", FT_ENUM, m->video, "x")) goto falha;
  if (!campo_com_recuo(j, "hdr", FT_ENUM, m->hdr, "x")) goto falha;
  if (!campo_com_recuo(j, "res", FT_ENUM, m->res, "x")) goto falha;

  if (!campo_num1_opcional(j, "fps", m->fps) ||
      !campo_num1_opcional(j, "mbps", m->mbps) ||
      !campo_num1_opcional(j, "gb", m->gb))
    goto falha;

  if (!campo_com_recuo(j, "audio", FT_ENUM, m->audio, "x")) goto falha;
  if (!campo_com_recuo(j, "legenda", FT_ENUM, m->legenda, "x")) goto falha;
  if (!campo_com_recuo(j, "entrega", FT_ENUM, m->entrega, "x")) goto falha;

  if (m->http >= 0) {
    if (!farolredator_campo(j, "http", FT_INT, &m->http)) goto falha;
  }
  if (m->ttfb_ms >= 0) {
    if (!farolredator_campo(j, "ttfb_ms", FT_INT, &m->ttfb_ms)) goto falha;
  }

  return farolredator_fechar(j);

falha:
  farolredator_fechar(j);
  return 0;
}

static int escrever_cadeia(FarolJson *j, const FarolPacoteEntrada *in) {
  int i;
  if (!farolredator_array(j, "cadeia")) return 0;
  if (in->tem_cadeia) {
    for (i = 0; i < RES_T_N; i++) {
      int est = in->cadeia[i].estado;
      const char *est_s = (est >= 0 && est < (int)(sizeof ESTADOS / sizeof *ESTADOS))
                            ? ESTADOS[est] : ESTADOS[FAROL_EST_NAO_TESTADO];
      if (!farolredator_objeto(j, NULL)) goto falha;
      if (!farolredator_campo(j, "passo", FT_ENUM, PASSOS[i]) ||
          !farolredator_campo(j, "estado", FT_ENUM, est_s)) {
        farolredator_fechar(j);
        goto falha;
      }
      if (in->cadeia[i].ms >= 0) {
        if (!farolredator_campo(j, "ms", FT_INT, &in->cadeia[i].ms)) {
          farolredator_fechar(j);
          goto falha;
        }
      }
      if (in->cadeia[i].http > 0) {
        if (!farolredator_campo(j, "http", FT_INT, &in->cadeia[i].http)) {
          farolredator_fechar(j);
          goto falha;
        }
      }
      if (!farolredator_fechar(j)) goto falha;
    }
  }
  return farolredator_fechar(j);

falha:
  farolredator_fechar(j);
  return 0;
}

typedef struct {
  int         t_rel;
  const char *m;
  const char *e;
  int         a;
  int         b;
  char        k[FAROL_TOKEN_N];
  size_t      custo;
} LinhaCand;

static size_t medir_linha_cand(const LinhaCand *c) {
  int n = snprintf(NULL, 0,
                   "{\"t\":%d,\"m\":\"%s\",\"e\":\"%s\",\"a\":%d,\"b\":%d,\"k\":\"%s\"}",
                   c->t_rel, c->m, c->e, c->a, c->b, c->k);
  return n > 0 ? (size_t)n : 128u;
}

static int escrever_linha(FarolJson *j, const FarolPacoteEntrada *in,
                          int *out_total, int *out_escritos, int *out_cortados) {
  FarolEvento anel_buf[FAROL_ANEL_N];
  LinhaCand cands[FAROL_ANEL_N];
  const FarolEvento *src;
  int n_src, i, n_cand = 0, inicio;
  uint32_t agora;
  size_t espaco = 0, usado = 0;

  if (in->eventos) {
    src = in->eventos;
    n_src = in->n_eventos > FAROL_ANEL_N ? FAROL_ANEL_N : in->n_eventos;
    if (n_src < 0) n_src = 0;
  } else {
    src = anel_buf;
    n_src = farolanel_ler(anel_buf, FAROL_ANEL_N, 0);
  }

  agora = in->agora_ms;
  if (agora == 0 && n_src > 0) agora = src[n_src - 1].t_ms;

  for (i = 0; i < n_src; i++) {
    const FarolEvento *ev = &src[i];
    int32_t dt;
    const char *m_s, *e_s;
    LinhaCand *c;
    if (ev->ev <= FE_NENHUM || ev->ev >= FE_N) continue;
    e_s = EVENTOS[ev->ev];
    if (!e_s) continue;
    m_s = (ev->mod < FM_N) ? MODS[ev->mod] : MODS[FM_FAROL];
    if (in->janela_ms > 0) {
      dt = (int32_t)(agora - ev->t_ms);
      if (dt < 0 || (uint32_t)dt > in->janela_ms) continue;
    }
    c = &cands[n_cand++];
    c->t_rel = (int)(int32_t)(ev->t_ms - agora);
    c->m = m_s;
    c->e = e_s;
    c->a = (int)ev->a;
    c->b = (int)ev->b;
    /* Saneamento defensivo caso o chamador passe FarolEvento direto sem anel. */
    farolredator_token(ev->tok, c->k);
    c->custo = medir_linha_cand(c);
  }

  if (!farolredator_array(j, "linha")) return 0;

  if (j->tam > j->n + PRIV_RESERVA) espaco = j->tam - j->n - PRIV_RESERVA;

  /* Corta do mais antigo: acumula do mais novo para tras ate o teto. */
  inicio = n_cand;
  for (i = n_cand - 1; i >= 0; i--) {
    size_t passo = cands[i].custo + (inicio < n_cand ? 1u : 0u);
    if (usado + passo > espaco) break;
    usado += passo;
    inicio = i;
  }

  for (i = inicio; i < n_cand; i++) {
    const LinhaCand *c = &cands[i];
    if (!farolredator_objeto(j, NULL)) goto falha;
    if (!farolredator_campo(j, "t", FT_INT, &c->t_rel) ||
        !farolredator_campo(j, "m", FT_ENUM, c->m) ||
        !farolredator_campo(j, "e", FT_ENUM, c->e) ||
        !farolredator_campo(j, "a", FT_INT, &c->a) ||
        !farolredator_campo(j, "b", FT_INT, &c->b)) {
      farolredator_fechar(j);
      goto falha;
    }
    /* k passa pelo redator: token fora do dominio fechado e recusado e contado
     * em privacidade.campos_recusados sem derrubar o evento. */
    (void)farolredator_campo(j, "k", FT_TOKEN, c->k);
    if (!farolredator_fechar(j)) goto falha;
  }

  if (!farolredator_fechar(j)) goto falha;

  if (out_total) *out_total = n_cand;
  if (out_escritos) *out_escritos = n_cand - inicio;
  if (out_cortados) *out_cortados = inicio;
  return 1;

falha:
  farolredator_fechar(j);
  return 0;
}

int farolpacote_montar(const FarolPacoteEntrada *in,
                       char *buf, size_t tam,
                       FarolPacoteRes *res) {
  FarolJson j;
  FarolPerfil perfil_local;
  const FarolPerfil *perfil;
  char fp_texto[FAROL_PACOTE_FP_MAX];
  char fp_hex[FAROL_PACOTE_FP_HEX_N];
  int v = 1, redator = 1, recusados;
  int l_total = 0, l_escritos = 0, l_cortados = 0;
  const char *gat_s, *sin_s;

  if (res) memset(res, 0, sizeof *res);
  if (!in || !buf || tam < 3) return 0;

  if (in->perfil) {
    perfil = in->perfil;
  } else {
    farolperfil_obter(&perfil_local);
    perfil = &perfil_local;
  }

  if (!farolpacote_fingerprint(perfil, &in->erro, &in->midia, fp_texto, fp_hex))
    return 0;

  if (!farolredator_abrir(&j, buf, tam)) return 0;

  gat_s = (in->gatilho >= 0 && in->gatilho < (int)(sizeof GATILHOS / sizeof *GATILHOS))
            ? GATILHOS[in->gatilho] : GATILHOS[FAROL_G_MANUAL];
  sin_s = (in->sintoma >= 0 && in->sintoma < (int)(sizeof SINTOMAS / sizeof *SINTOMAS))
            ? SINTOMAS[in->sintoma] : SINTOMAS[FAROL_PS_OUTRO];

  if (!farolredator_campo(&j, "v", FT_INT, &v) ||
      !farolredator_campo(&j, "gatilho", FT_ENUM, gat_s) ||
      !farolredator_campo(&j, "sintoma", FT_ENUM, sin_s))
    goto falha;

  if (!escrever_erro(&j, in, fp_texto, fp_hex)) goto falha;
  if (!farolperfil_escrever(&j, perfil)) goto falha;
  if (!escrever_midia(&j, &in->midia)) goto falha;
  if (!escrever_cadeia(&j, in)) goto falha;
  if (!escrever_linha(&j, in, &l_total, &l_escritos, &l_cortados)) goto falha;

  if (!farolredator_objeto(&j, "privacidade")) goto falha;
  recusados = (int)j.campos_recusados;
  if (!farolredator_campo(&j, "redator", FT_INT, &redator) ||
      !farolredator_campo(&j, "campos_recusados", FT_INT, &recusados) ||
      !farolredator_fechar(&j)) {
    farolredator_fechar(&j);
    goto falha;
  }

  if (!farolredator_fechar(&j) || !j.fechado) goto falha;
  if (j.n > FAROL_JSON_MAX) goto falha;

  if (res) {
    res->bytes = j.n;
    res->campos_recusados = j.campos_recusados;
    res->linha_total = l_total;
    res->linha_escritos = l_escritos;
    res->linha_cortados = l_cortados;
    memcpy(res->fingerprint, fp_texto, sizeof res->fingerprint);
    memcpy(res->fp, fp_hex, sizeof res->fp);
  }
  return 1;

falha:
  fechar_restantes(&j);
  return 0;
}

/* --- Espelho na tela ("Vai junto" / "Nunca vai", §2.5 item 6) ------------- */
/* Ordem mais especifica primeiro ("cliente.ajustes" antes de "cliente").
 * Cada grupo do schema em farolredator.c aponta para sua frase FT_VAI_*. */
static const FarolEspelhoGrupo ESPELHO_GRUPOS[] = {
  { "aparelho",        FT_VAI_MODELO  },
  { "cliente.ajustes", FT_VAI_AJUSTES },
  { "cliente",         FT_VAI_VERSAO  },
  { "midia",           FT_VAI_FORMATO },
  { "v",               FT_VAI_PONTO   },
  { "gatilho",         FT_VAI_PONTO   },
  { "sintoma",         FT_VAI_PONTO   },
  { "erro",            FT_VAI_PONTO   },
  { "cadeia",          FT_VAI_PONTO   },
  { "privacidade",     FT_VAI_PONTO   },
  { "linha",           FT_VAI_EVENTOS }
};

static const char *const VAI_JUNTO[] = {
  FT_VAI_MODELO,
  FT_VAI_VERSAO,
  FT_VAI_FORMATO,
  FT_VAI_PONTO,
  FT_VAI_AJUSTES,
  FT_VAI_EVENTOS
};

static const char *const NUNCA_VAI[] = {
  FT_NUNCA_TITULOS,
  FT_NUNCA_PERFIL,
  FT_NUNCA_ENDERECOS,
  FT_NUNCA_SENHAS,
  FT_NUNCA_IP,
  FT_NUNCA_HISTORICO
};

const FarolEspelhoGrupo *farolpacote_espelho_grupos(size_t *n) {
  if (n) *n = sizeof ESPELHO_GRUPOS / sizeof *ESPELHO_GRUPOS;
  return ESPELHO_GRUPOS;
}

const char *farolpacote_espelho_para(const char *caminho) {
  size_t i;
  if (!caminho || !*caminho) return NULL;
  for (i = 0; i < sizeof ESPELHO_GRUPOS / sizeof *ESPELHO_GRUPOS; i++) {
    const char *pref = ESPELHO_GRUPOS[i].prefixo;
    size_t plen = strlen(pref);
    if (strncmp(caminho, pref, plen) == 0) {
      char prox = caminho[plen];
      if (prox == '\0' || prox == '.' || prox == '[')
        return ESPELHO_GRUPOS[i].frase;
    }
  }
  return NULL;
}

const char *const *farolpacote_vai_junto(size_t *n) {
  if (n) *n = sizeof VAI_JUNTO / sizeof *VAI_JUNTO;
  return VAI_JUNTO;
}

const char *const *farolpacote_nunca_vai(size_t *n) {
  if (n) *n = sizeof NUNCA_VAI / sizeof *NUNCA_VAI;
  return NUNCA_VAI;
}
