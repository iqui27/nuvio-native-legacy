// FAROL — testes da issue #53 (Consentimento do gosto):
//   1. Fonte unica da verdade: lista da tela == lista do corpo JSON real de
//      farolcompor_montar_corpo() (100% cobertura, 0 linhas orfas, bloqueio de
//      canarios e campos proibidos).
//   2. 3 estados (NAO_PERGUNTADO / SO_TV / SIM), persistencia atomica por
//      perfil, isolamento entre perfis, bloqueio em perfil_infantil_ativo() e
//      perfil_ajustes_travados(), e limpeza em farolconsent_esquecer() /
//      farol_esquecer().
//   3. D-pad no Resolver (key.repeat no OK ignorado, So nesta TV nao volta a
//      perguntar, Permitir liga LLM, Voltar fecha sem ligar LLM, sem becos).
//   4. (-DFAROL_CONSENT_SHOT) Captura PNG 1920x1080 no FBO GLES2 do app inteiro.
#ifndef FAROL_CONSENT_SHOT

#define _POSIX_C_SOURCE 200809L
#define RESOLVER_TESTE 1
#include "farolconsent.h"
#include "farolconsentui.h"
#include "farolcompor.h"
#include "farolgosto.h"
#include "farolpeca.h"
#define FarolMotivo FarolRecMotivo
#include "farolrec.h"
#undef FarolMotivo
#include "faroltexto.h"
#include "farol.h"
#include "farolcena.h"
#include "farolsonda.h"
#include "resolver.h"
#include "resolverregra.h"
#include "addonmodo.h"
#include "addons.h"
#include "atualizacao.h"
#include "avisos.h"
#include "debrid.h"
#include "gfx.h"
#include "js.h"
#include "layout.h"
#include "ponteiro.h"
#include "streams.h"
#include "text.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int falhas;
#define CONFERE(c) do { if (!(c)) { fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

/* ----------------------------------------------------- estado controlado -- */
static char dadosDir[512];
static int  c_perfil = 1;
static int  c_infantil = 0;
static int  c_travado[16];
static int  c_recomendar = 1;
static int  c_vidro = 1;
static int  c_reduzidas = 1;
static char c_txt[256][200];
static int  c_txtN;

static void anotar_txt(const char *s) {
  if (s && c_txtN < (int)(sizeof c_txt / sizeof c_txt[0]))
    snprintf(c_txt[c_txtN++], sizeof c_txt[0], "%s", s);
}
static int escreveu(const char *s) {
  for (int i = 0; i < c_txtN; i++) if (strstr(c_txt[i], s)) return 1;
  return 0;
}

/* ------------------------------------------- stubs de app / UI / texto ---- */
const char *i18n(const char *s) { return s; }
const char *dados_dir(void) { return dadosDir; }
char *dados_ler(const char *nome) {
  char c[700]; FILE *f; long n; char *b;
  snprintf(c, sizeof c, "%s/%s", dadosDir, nome);
  f = fopen(c, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = (char *)malloc((size_t)n + 1);
  if (!b) { fclose(f); return NULL; }
  if (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
  b[n] = 0; fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[700], t[720]; FILE *f;
  snprintf(c, sizeof c, "%s/%s", dadosDir, nome);
  snprintf(t, sizeof t, "%s.tmp", c);
  f = fopen(t, "wb");
  if (!f) return 0;
  fputs(conteudo, f);
  fclose(f);
  return rename(t, c) == 0;
}
int dados_apagar(const char *nome) {
  char c[700];
  snprintf(c, sizeof c, "%s/%s", dadosDir, nome);
  return unlink(c) == 0;
}

int  perfis_ativo(void) { return c_perfil; }
uint64_t perfis_sessao_geracao(void) { return 1; }
const char *sessao_usuario(void) { return "conta_teste"; }
int  perfil_infantil(int idx) { (void)idx; return c_infantil; }
int  perfil_infantil_ativo(void) { return c_infantil; }
int  perfil_ajustes_travados(int idx) {
  return (idx >= 1 && idx < 16) ? c_travado[idx] : 0;
}
int  farolsinal_recomendar(void) { return c_recomendar; }
int  seguro_perfil_ativo(void) { return 0; }
int  player_aberto(void) { return 0; }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
int  ajustes_vidro(void) { return c_vidro; }
int  ajustes_animacoes_reduzidas(void) { return c_reduzidas; }
void ajustes_acento(float *r, float *g, float *b) { *r = 0.25f; *g = 0.65f; *b = 1.0f; }
float ajustes_acento_tinta(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
void ajustes_abrir_no_trakt(void) {}
void ajustes_abrir_na_espera_fonte(void) {}
void ajustes_iniciar(void) {}

int rede_saude_offline(void) { return 0; }
int addons_n(void) { return 2; }
int addons_ocupado(void) { return 0; }
int addons_ativo(int i) { (void)i; return 1; }
int addons_fornece(int i, int q) { (void)i; (void)q; return 0; }
int addons_motivo_vazio(char *d, unsigned n) { (void)d; (void)n; return 0; }
const char *addons_base(int i) { (void)i; return ""; }
int debrid_ativo(void) { return 0; }
int debrid_sem_plano(void) { return 0; }
const char *debrid_sem_plano_frase(int m) { (void)m; return NULL; }
int debrid_recusa(char *d, unsigned n) { (void)d; (void)n; return 0; }
int stream_atual(void) { return -1; }
const Stream *stream_item(int i) { (void)i; return NULL; }
int stream_kbps_atual(void) { return 0; }
int stream_ordem_addon(int i) { (void)i; return -1; }
int video_registro_negado(void) { return 0; }
int video_audio_nao_suportado(void) { return 0; }
int video_n_audio(void) { return 0; }
typedef struct VideoFaixa VideoFaixa;
const VideoFaixa *video_audio(int i) { (void)i; return NULL; }
int video_audio_atual(void) { return -1; }
int video_faixa_dts(const VideoFaixa *f) { (void)f; return 0; }
int video_dts_estado(void) { return 0; }
int video_tem_dolby_vision(void) { return 0; }
const char *video_hdr(void) { return ""; }
int nv_webos_major(void) { return 0; }
int dts_playback_enabled(void) { return 0; }
int dts_tpk_disponivel(void) { return 0; }
long memlog_livre_mb(void) { return 512; }
int player_ultimo_erro_farol(FarolErro *o, int *d) { (void)o; (void)d; return 0; }
uint64_t player_ultimo_erro_tentativa(void) { return 0; }
uint64_t player_ultimo_erro_seq(void) { return 0; }
int player_externo_disponivel_tentativa(uint64_t t) { (void)t; return 0; }
int player_abrir_externo_tentativa(uint64_t t) { (void)t; return 0; }
uint64_t player_ultimo_addon_farol(void) { return 0; }
void stream_farol_fatos(int *a, int *b, int *c, int *d, int *e, int *f, int *g,
                        int *h, int *i, int *j, int *k, int *l) {
  *a = *b = *c = *d = *e = *f = *g = *h = *i = *j = *k = *l = -1;
}
int home_fileiras_na_tela(void) { return 0; }
const char *app_tela_nome(void) { return "resolver"; }
int recomenda_pessoa_canonica(const char *id, char *d, size_t n) { (void)id; if (d && n) d[0] = 0; return 0; }
int cat_indice_por_imdb(const char *i) { (void)i; return -1; }
int cat_copiar_item(int i, CatItem *d) { (void)i; (void)d; return 0; }
int cat_copiar_por_id(const char *i, const char *t, CatItem *d) { (void)i; (void)t; (void)d; return 0; }
int cat_fileira_imdb(const char *c, char (*o)[64], int m, int s) { (void)c; (void)o; (void)m; (void)s; return 0; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
int cat_n(void) { return 0; }
unsigned cat_revisao(void) { return 0; }
int cat_visto(const CatItem *c) { (void)c; return 0; }
int cat_remover_fileiras_prefixo(const char *d, const char *p) { (void)d; (void)p; return 0; }
int farolcena_home(char *d, size_t c, const FarolCenaCtx *x, const FarolDicaFato *a, int b) { (void)d; (void)c; (void)x; (void)a; (void)b; return 0; }
int farolsonda_iniciar(const char *u, const char *const *c) { (void)u; (void)c; return 0; }
int farolsonda_estado(FarolSonda *o) { if (o) memset(o, 0, sizeof *o); return 0; }
void farolsonda_cancelar(void) {}
int farolficha_ler_item(const CatItem *c, FarolFicha *d) { (void)c; (void)d; return 0; }
int64_t farolficha_chave(const FarolFicha *f) { (void)f; return 0; }
int farolsinal_modelo(FarolGosto *d) { (void)d; return 0; }
int farolsinal_ficha_do_item(const CatItem *c, FarolFicha *d) { (void)c; (void)d; return 0; }
void farolsinal_mostrado(const CatItem *c) { (void)c; }
void farolsinal_zerar_gosto(void) {}
void farolsinal_definir_recomendar(int l) { (void)l; }
int fil_oculta(const char *c) { (void)c; return 0; }
int diario_n(int p) { (void)p; return 0; }
int diario_entrada(int p, int i, void *d) { (void)p; (void)i; (void)d; return 0; }
int diario_titulo(int p, const char *i, void *d) { (void)p; (void)i; (void)d; return 0; }
int diario_nome(const char *i, char *t, size_t nt, int *a, char *pn, size_t np) {
  (void)i; if (t && nt) t[0] = 0; (void)a; if (pn && np) pn[0] = 0; return 0;
}
int salvos_n(void) { return 0; }
const void *salvos_item(int i) { (void)i; return NULL; }
long mapa_hash_nome(const char *x) { (void)x; return -1; }
uint32_t mapa_hash_titulo(const char *x) { (void)x; return 1; }
int avisos_envio_info(AvisosEnvio *o) { memset(o, 0, sizeof *o); return 0; }
void avisos_enviar_registro_atual(void) {}
const char *atualizacao_nova(void) { return ""; }

/* GFX / Text / Ponteiro / Botoes stubs */
int perftv_enabled;
int perftv_uploads;
float gfx_tex_aspect_atual;
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) {
  (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_vidro_painel(GfxRect r, float raio, float fundo, float a) {
  (void)r; (void)raio; (void)fundo; (void)a;
}
float gfx_escala_ui(void) { return 1.0f; }
float gfx_escala(void) { return 1.0f; }
void gfx_escala_sair(float a) { (void)a; }
float botao_largura(const char *rot, const char *ic, int pri) {
  (void)rot; (void)ic; (void)pri; return 180.0f;
}
void botao_pilula(GfxRect r, const char *rot, const char *ic, float f, int p, int al, float a) {
  (void)r; (void)ic; (void)f; (void)p; (void)al; (void)a; anotar_txt(rot);
}
float botao_cor_foco(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
void botao_luz(GfxRect r, float f, float a) { (void)r; (void)f; (void)a; }
void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn fo, PonteiroFn at, int a, int b) {
  (void)x; (void)y; (void)w; (void)h; (void)fo; (void)at; (void)a; (void)b;
}
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)r; (void)g; (void)b; (void)a;
  anotar_txt(s); return l;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float mw) {
  TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)r; (void)g; (void)b; (void)a; (void)mw;
  anotar_txt(s); return l;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) {
  (void)e; if (a > 0.0f) anotar_txt(s); (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)ml;
  return 0.0f;
}
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float a, float t) {
  (void)e; if (a > 0.0f) anotar_txt(s); (void)r; (void)g; (void)b; (void)x; (void)y; (void)t;
  return 0.0f;
}

/* --------------------- 1. Varredor JSON recursivo ------------------------ */
typedef struct {
  const char *p;
  const char *fim;
  unsigned hits[FAROLCONSENT_VAI_N];
  int totalChaves;
} JsonWalker;

static void pular_ws(JsonWalker *w) {
  while (w->p < w->fim && (*w->p == ' ' || *w->p == '\t' || *w->p == '\n' || *w->p == '\r'))
    w->p++;
}

static int ler_str(JsonWalker *w, char *dst, size_t cap) {
  size_t n = 0;
  pular_ws(w);
  if (w->p >= w->fim || *w->p != '"') return 0;
  w->p++;
  while (w->p < w->fim && *w->p != '"') {
    char c = *w->p++;
    if (c == '\\') {
      if (w->p >= w->fim) return 0;
      c = *w->p++;
    }
    if (dst && n + 1 < cap) dst[n++] = c;
  }
  if (w->p >= w->fim || *w->p != '"') return 0;
  w->p++;
  if (dst) dst[n] = '\0';
  return 1;
}

static int cobrir_caminho(JsonWalker *w, const char *path) {
  size_t nVai = 0, k;
  const char *const *vai = farolconsent_vai_junto(&nVai);
  const char *frase = farolconsent_espelho_para(path);
  CONFERE(farolconsent_campo_autorizado(path) == 1);
  if (!frase || !frase[0]) {
    fprintf(stderr, "FALHA: caminho JSON sem mapeamento na tela: %s\n", path);
    return 0;
  }
  for (k = 0; k < nVai && k < FAROLCONSENT_VAI_N; k++) {
    if (strcmp(vai[k], frase) == 0) {
      w->hits[k]++;
      w->totalChaves++;
      return 1;
    }
  }
  fprintf(stderr, "FALHA: frase %s fora de farolconsent_vai_junto()\n", frase);
  return 0;
}

static int walk_valor(JsonWalker *w, const char *prefixo);

static int walk_obj(JsonWalker *w, const char *prefixo) {
  int first = 1;
  pular_ws(w);
  if (w->p >= w->fim || *w->p != '{') return 0;
  w->p++;
  for (;;) {
    char chave[64], caminho[256];
    pular_ws(w);
    if (w->p < w->fim && *w->p == '}') { w->p++; return 1; }
    if (!first) {
      if (w->p >= w->fim || *w->p != ',') return 0;
      w->p++;
    }
    first = 0;
    if (!ler_str(w, chave, sizeof chave)) return 0;
    pular_ws(w);
    if (w->p >= w->fim || *w->p != ':') return 0;
    w->p++;
    if (prefixo && prefixo[0]) snprintf(caminho, sizeof caminho, "%s.%s", prefixo, chave);
    else snprintf(caminho, sizeof caminho, "%s", chave);
    if (!cobrir_caminho(w, caminho)) return 0;
    if (!walk_valor(w, caminho)) return 0;
  }
}

static int walk_arr(JsonWalker *w, const char *prefixo) {
  int first = 1, idx = 0;
  pular_ws(w);
  if (w->p >= w->fim || *w->p != '[') return 0;
  w->p++;
  for (;;) {
    char caminho[256];
    pular_ws(w);
    if (w->p < w->fim && *w->p == ']') { w->p++; return 1; }
    if (!first) {
      if (w->p >= w->fim || *w->p != ',') return 0;
      w->p++;
    }
    first = 0;
    snprintf(caminho, sizeof caminho, "%s[%d]", prefixo, idx++);
    if (!walk_valor(w, caminho)) return 0;
  }
}

static int walk_valor(JsonWalker *w, const char *prefixo) {
  pular_ws(w);
  if (w->p >= w->fim) return 0;
  if (*w->p == '{') return walk_obj(w, prefixo);
  if (*w->p == '[') return walk_arr(w, prefixo);
  if (*w->p == '"') return ler_str(w, NULL, 0);
  if (*w->p == '-' || isdigit((unsigned char)*w->p)) {
    while (w->p < w->fim && (isdigit((unsigned char)*w->p) ||
           *w->p == '.' || *w->p == '-' || *w->p == '+' || *w->p == 'e' || *w->p == 'E'))
      w->p++;
    return 1;
  }
  if (w->fim - w->p >= 4 && memcmp(w->p, "true", 4) == 0) { w->p += 4; return 1; }
  if (w->fim - w->p >= 5 && memcmp(w->p, "false", 5) == 0) { w->p += 5; return 1; }
  if (w->fim - w->p >= 4 && memcmp(w->p, "null", 4) == 0) { w->p += 4; return 1; }
  return 0;
}

static void testar_lista_tela_igual_corpo(void) {
  FarolComporAfin gen[2], tem[2], pes[2];
  FarolComporAncora anc[2];
  FarolComporCand cand[4];
  FarolComporFato fatos[4];
  FarolComporPedido ped;
  char corpo[FAROL_COMPOR_CORPO_MAX + 1];
  size_t nb, nGrp = 0, nVai = 0, nNunca = 0, i, k;
  const FarolConsentGrupo *grps = farolconsent_espelho_grupos(&nGrp);
  const char *const *vai = farolconsent_vai_junto(&nVai);
  const char *const *nunca = farolconsent_nunca_vai(&nNunca);

  CONFERE(nGrp > 0 && nVai == FAROLCONSENT_VAI_N && nNunca == FAROLCONSENT_NUNCA_N);
  for (i = 0; i < nGrp; i++) {
    int ok = 0;
    for (k = 0; k < nVai; k++) if (strcmp(vai[k], grps[i].frase) == 0) ok = 1;
    CONFERE(ok == 1);
  }
  CONFERE(strcmp(nunca[0], FAROLCONSENT_NUNCA_DIARIO) == 0);
  CONFERE(strcmp(nunca[6], FAROLCONSENT_NUNCA_INFANTIL) == 0);

  // Campos proibidos nunca sao autorizados.
  CONFERE(farolconsent_campo_autorizado("diario") == 0);
  CONFERE(farolconsent_campo_autorizado("perfil") == 0);
  CONFERE(farolconsent_campo_autorizado("amigos") == 0);
  CONFERE(farolconsent_campo_autorizado("addons") == 0);
  CONFERE(farolconsent_campo_autorizado("fontes") == 0);
  CONFERE(farolconsent_campo_autorizado("gosto.vetor") == 0);
  CONFERE(farolconsent_campo_autorizado("candidatos[0].diario") == 0);
  CONFERE(farolconsent_campo_autorizado("fatos[0].perfil") == 0);
  CONFERE(farolconsent_espelho_para("gosto.segredo") == NULL);

  // Montar pedido real completo com todos os campos de farolcompor.c.
  memset(gen, 0, sizeof gen);
  gen[0].id = 53; gen[0].peso = 512; snprintf(gen[0].nome, sizeof gen[0].nome, "Suspense");
  memset(tem, 0, sizeof tem);
  tem[0].id = 401; tem[0].peso = 384; snprintf(tem[0].nome, sizeof tem[0].nome, "investigacao");
  memset(pes, 0, sizeof pes);
  pes[0].id = 1001; pes[0].peso = 256; snprintf(pes[0].nome, sizeof pes[0].nome, "Pessoa Publica");
  memset(anc, 0, sizeof anc);
  snprintf(anc[0].id, sizeof anc[0].id, "tt9000001");
  snprintf(anc[0].titulo, sizeof anc[0].titulo, "Ancora Publica");

  memset(cand, 0, sizeof cand);
  snprintf(cand[0].imdb, sizeof cand[0].imdb, "tt1000001");
  cand[0].tmdb = 501;
  cand[0].tipo = 1;
  snprintf(cand[0].titulo, sizeof cand[0].titulo, "Serie Exemplo");
  cand[0].ano = 2024;
  cand[0].minutos = 48;
  cand[0].generos[0] = 53;
  cand[0].nGeneros = 1;
  cand[0].nota10 = 84;
  cand[0].pontos = 1500;
  cand[0].motivos[0].tipo = FMO_TERMINOU;
  cand[0].motivos[0].a = 0;
  cand[0].motivos[1].tipo = FMO_AMIGO;
  cand[0].motivos[1].a = 999888;
  cand[0].nMotivos = 2;
  cand[0].faltam = 3;
  cand[0].minNoite = 48;
  cand[0].cabeHoje = 1;

  memset(fatos, 0, sizeof fatos);
  snprintf(fatos[0].chave, sizeof fatos[0].chave, "perguntas");
  snprintf(fatos[0].valor, sizeof fatos[0].valor, "curto");
  snprintf(fatos[0].alvo, sizeof fatos[0].alvo, "c0");
  snprintf(fatos[1].chave, sizeof fatos[1].chave, "perfil"); // deve ser bloqueado
  snprintf(fatos[1].valor, sizeof fatos[1].valor, "PERFIL_CANARIO");

  memset(&ped, 0, sizeof ped);
  ped.modo = FC_MODO_HOME;
  ped.idioma = "pt-BR";
  ped.hora = 2;
  ped.fimSemana = 0;
  ped.nivel = 2;
  ped.infantil = 0;
  ped.consentiu = 1;
  ped.generos = gen; ped.nGeneros = 1;
  ped.temas = tem; ped.nTemas = 1;
  ped.pessoas = pes; ped.nPessoas = 1;
  ped.ancoras = anc; ped.nAncoras = 1;
  ped.candidatos = cand; ped.nCandidatos = 1;
  ped.fatos = fatos; ped.nFatos = 2;
  ped.frase = "algo curto para hoje";

  nb = farolcompor_montar_corpo(&ped, corpo, sizeof corpo);
  CONFERE(nb > 0 && nb <= FAROL_COMPOR_CORPO_MAX);
  CONFERE(strstr(corpo, "PERFIL_CANARIO") == NULL);

  {
    JsonWalker w;
    memset(&w, 0, sizeof w);
    w.p = corpo;
    w.fim = corpo + nb;
    CONFERE(walk_obj(&w, "") == 1);
    pular_ws(&w);
    CONFERE(w.p == w.fim);
    CONFERE(w.totalChaves > 20);
    for (k = 0; k < nVai; k++) {
      CONFERE(w.hits[k] > 0);
    }
  }
  puts("ok  lista da tela == lista do corpo JSON real (100% cobertura)");
}

/* --------------------- 2. Estados, persistencia e bloqueios ---------------- */
static void testar_estados_persistencia_e_bloqueios(void) {
  char path1[640], path2[640];
  struct stat st;
  FILE *f;

  farolconsent_esquecer();
  farolconsent_teste_zera();
  farolcompor_teste_zera();
  farolconsent_pasta(dadosDir);
  c_perfil = 1; c_infantil = 0; memset(c_travado, 0, sizeof c_travado); c_recomendar = 1;

  // Estado inicial: NAO_PERGUNTADO, LLM desligado, deve perguntar = 1.
  CONFERE(farolconsent_estado() == FAROL_CONSENT_NAO_PERGUNTADO);
  CONFERE(farolcompor_consentimento() == 0);
  CONFERE(farolconsent_llm_ativo() == 0);
  CONFERE(farolconsent_deve_perguntar() == 1);

  // Arquivo corrompido cai para NAO_PERGUNTADO.
  snprintf(path1, sizeof path1, "%s/farol-consent-p1.txt", dadosDir);
  f = fopen(path1, "wb");
  CONFERE(f != NULL);
  fputs("CORROMPIDO 999\n", f);
  fclose(f);
  farolconsent_teste_zera();
  farolconsent_pasta(dadosDir);
  farolconsent_trocar_perfil(1);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_NAO_PERGUNTADO);
  CONFERE(farolcompor_consentimento() == 0);

  // "So nesta TV": grava atomicamente, LLM desligado, nunca volta a perguntar.
  CONFERE(farolconsent_definir(FAROL_CONSENT_SO_TV) == 1);
  CONFERE(stat(path1, &st) == 0);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_SO_TV);
  CONFERE(farolcompor_consentimento() == 0);
  CONFERE(farolconsent_deve_perguntar() == 0);

  // Simula reinicio e recarga do disco.
  farolconsent_teste_zera();
  farolcompor_teste_zera();
  farolconsent_pasta(dadosDir);
  farolconsent_trocar_perfil(1);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_SO_TV);
  CONFERE(farolcompor_consentimento() == 0);
  CONFERE(farolconsent_deve_perguntar() == 0);

  // Isolamento entre perfis: perfil 2 comeca NAO_PERGUNTADO e pode aceitar SIM.
  c_perfil = 2;
  farolconsent_trocar_perfil(2);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_NAO_PERGUNTADO);
  CONFERE(farolconsent_deve_perguntar() == 1);
  CONFERE(farolcompor_consentimento() == 0);
  CONFERE(farolconsent_definir(FAROL_CONSENT_SIM) == 1);
  snprintf(path2, sizeof path2, "%s/farol-consent-p2.txt", dadosDir);
  CONFERE(stat(path2, &st) == 0);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_SIM);
  CONFERE(farolcompor_consentimento() == 1);
  CONFERE(farolconsent_llm_ativo() == 1);
  CONFERE(farolconsent_deve_perguntar() == 0);

  // Voltar ao perfil 1 desliga LLM na hora; voltar ao 2 religa na hora.
  c_perfil = 1;
  farolconsent_trocar_perfil(1);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_SO_TV);
  CONFERE(farolcompor_consentimento() == 0);
  c_perfil = 2;
  farolconsent_trocar_perfil(2);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_SIM);
  CONFERE(farolcompor_consentimento() == 1);

  // Perfil infantil bloqueia pergunta, bloqueia definicao e desliga LLM no compor.
  c_infantil = 1;
  farolconsent_trocar_perfil(2);
  CONFERE(farolcompor_consentimento() == 0);
  CONFERE(farolconsent_llm_ativo() == 0);
  CONFERE(farolconsent_deve_perguntar() == 0);
  CONFERE(farolconsent_definir(FAROL_CONSENT_SIM) == 0);
  c_perfil = 3;
  farolconsent_trocar_perfil(3);
  CONFERE(farolconsent_deve_perguntar() == 0);
  CONFERE(farolconsent_definir(FAROL_CONSENT_SO_TV) == 0);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_NAO_PERGUNTADO);
  c_infantil = 0;

  // Perfil com ajustes travados bloqueia pergunta e definicao.
  c_travado[3] = 1;
  farolconsent_trocar_perfil(3);
  CONFERE(farolconsent_deve_perguntar() == 0);
  CONFERE(farolconsent_definir(FAROL_CONSENT_SIM) == 0);
  c_travado[3] = 0;

  // Recomendar desligado bloqueia pergunta.
  c_recomendar = 0;
  CONFERE(farolconsent_deve_perguntar() == 0);
  c_recomendar = 1;
  CONFERE(farolconsent_deve_perguntar() == 1);

  // Esquecer no logout (via farol_esquecer) limpa arquivos e zera consentimento.
  c_perfil = 2;
  farolconsent_trocar_perfil(2);
  CONFERE(farolcompor_consentimento() == 1);
  farol_esquecer();
  CONFERE(stat(path1, &st) != 0);
  CONFERE(stat(path2, &st) != 0);
  CONFERE(farolcompor_consentimento() == 0);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_NAO_PERGUNTADO);

  puts("ok  3 estados, persistencia atomica por perfil, infantil/travado e farol_esquecer");
}

/* --------------------- 3. D-pad no Resolver e consentui ------------------- */
static void tecla(SDL_Keycode k, int repeat) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.repeat = (Uint8)repeat;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}

static void desenhar(void) {
  c_txtN = 0;
  resolver_atualizar(0.016f, 0);
  resolver_desenhar(0);
}

static void testar_dpad_resolver(void) {
  size_t nVai = 0, nNunca = 0, i;
  const char *const *vai = farolconsent_vai_junto(&nVai);
  const char *const *nunca = farolconsent_nunca_vai(&nNunca);

  farolconsent_esquecer();
  farolconsent_teste_zera();
  farolcompor_teste_zera();
  resolver_teste_esquecer();
  farolconsent_pasta(dadosDir);
  c_perfil = 1; c_infantil = 0; memset(c_travado, 0, sizeof c_travado); c_recomendar = 1;
  c_vidro = 1; c_reduzidas = 1;

  // 1. Abrir Farol com NAO_PERGUNTADO exibe tela de consentimento.
  resolver_abrir();
  CONFERE(resolver_aberto() == 1);
  CONFERE(farolconsentui_aberto() == 1);
  CONFERE(resolver_foco() == FCUI_B_SO_TV);

  desenhar();
  CONFERE(escreveu(FT_VAI_JUNTO) && escreveu(FT_NUNCA_VAI));
  CONFERE(escreveu(FT_GOSTO_LLM_SO_ESTA_TV) && escreveu(FAROLCONSENT_BTN_PERMITIR));
  for (i = 0; i < nVai; i++) CONFERE(escreveu(vai[i]));
  for (i = 0; i < nNunca; i++) CONFERE(escreveu(nunca[i]));

  // 2. key.repeat != 0 no OK eh ignorado.
  tecla(SDLK_RETURN, 1);
  CONFERE(farolconsentui_aberto() == 1);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_NAO_PERGUNTADO);

  // 3. Navegacao D-pad sem beco: -> vai para Permitir, <- volta para So nesta TV.
  tecla(SDLK_RIGHT, 0);
  CONFERE(resolver_foco() == FCUI_B_PERMITIR);
  tecla(SDLK_DOWN, 0);
  CONFERE(resolver_foco() == FCUI_B_PERMITIR);
  tecla(SDLK_LEFT, 0);
  CONFERE(resolver_foco() == FCUI_B_SO_TV);

  // 4. Voltar fecha consentimento sem habilitar LLM (fica em 0).
  tecla(SDLK_ESCAPE, 0);
  CONFERE(farolconsentui_aberto() == 0);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_NAO_PERGUNTADO);
  CONFERE(farolcompor_consentimento() == 0);
  tecla(SDLK_ESCAPE, 0);
  CONFERE(resolver_aberto() == 0);

  // 5. Segunda abertura volta a perguntar (porque nao respondeu); "So nesta TV" grava e nao pergunta mais.
  resolver_abrir();
  CONFERE(farolconsentui_aberto() == 1);
  CONFERE(resolver_foco() == FCUI_B_SO_TV);
  tecla(SDLK_RETURN, 0);
  CONFERE(farolconsentui_aberto() == 0);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_SO_TV);
  CONFERE(farolcompor_consentimento() == 0);
  CONFERE(farolconsent_deve_perguntar() == 0);
  tecla(SDLK_ESCAPE, 0);
  CONFERE(resolver_aberto() == 0);

  // Terceira abertura no mesmo perfil nao abre o consentimento.
  resolver_abrir();
  CONFERE(resolver_aberto() == 1);
  CONFERE(farolconsentui_aberto() == 0);
  tecla(SDLK_ESCAPE, 0);

  // 6. Perfil 2: abre consentimento, navega -> para Permitir e confirma.
  c_perfil = 2;
  farolconsent_trocar_perfil(2);
  resolver_abrir();
  CONFERE(farolconsentui_aberto() == 1);
  tecla(SDLK_RIGHT, 0);
  CONFERE(resolver_foco() == FCUI_B_PERMITIR);
  tecla(SDLK_RETURN, 0);
  CONFERE(farolconsentui_aberto() == 0);
  CONFERE(farolconsent_estado() == FAROL_CONSENT_SIM);
  CONFERE(farolcompor_consentimento() == 1);
  CONFERE(farolconsent_deve_perguntar() == 0);
  tecla(SDLK_ESCAPE, 0);

  // Segunda abertura no perfil 2 tambem nao pergunta.
  resolver_abrir();
  CONFERE(farolconsentui_aberto() == 0);
  tecla(SDLK_ESCAPE, 0);

  farolconsent_esquecer();
  puts("ok  D-pad Resolver: key.repeat ignorado, Voltar=0, So nesta TV=1, Permitir=2");
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "";
  CONFERE(dir[0] != '\0');
  snprintf(dadosDir, sizeof dadosDir, "%s", dir);

  testar_lista_tela_igual_corpo();
  testar_estados_persistencia_e_bloqueios();
  testar_dpad_resolver();

  if (falhas) {
    fprintf(stderr, "farol_consent: %d falha(s)\n", falhas);
    return 1;
  }
  puts("farol_consent: tudo ok");
  return 0;
}

#else /* FAROL_CONSENT_SHOT */

#define RESOLVER_TESTE 1
#include "farolconsent.h"
#include "farolconsentui.h"
#include "farolsinal.h"
#include "resolver.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "gl_compat.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;

static void quadro(float dt) {
  SDL_PumpEvents();
  resolver_atualizar(dt, SDL_GetTicks());
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.06f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  resolver_desenhar(SDL_GetTicks());
}

static void captura(const char *nome, int nQuadros) {
  unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y, variou = 0;
  assert(pix);
  for (int i = 0; i < nQuadros; i++) { quadro(1.0f / 60.0f); SDL_Delay(2); }
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 1; y < 1920 * 1080 && !variou; y += 997)
    if (memcmp(pix + (size_t)y * 4, pix, 3)) variou = 1;
  assert(variou);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
}

int main(int argc, char **argv) {
  const char *outDir = argc > 1 ? argv[1] : "/tmp";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;

  assert(dir && dir[0]);
  dados_iniciar(dir);
  ajustes_dir(dir);
  farolsinal_iniciar();

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  w = SDL_CreateWindow("farol-consent-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_tex_esquecer(0);
  { char ic[4096]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }

  farolconsent_esquecer();
  ajustes_definir_vidro(1);
  resolver_abrir();
  assert(farolconsentui_aberto() == 1);
  snprintf(nome, sizeof nome, "%s/consent-vidro.png", outDir);
  captura(nome, 24);

  ajustes_definir_vidro(0);
  snprintf(nome, sizeof nome, "%s/consent-solido.png", outDir);
  captura(nome, 8);

  farolsinal_sair();
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  return 0;
}

#endif /* FAROL_CONSENT_SHOT */
