// FAROL 20B — o Voltar do aviso dispensavel (o do Farol, issue 20) so pode
// agir com a pilula visivel DE FATO e so depois de todas as camadas que se
// desenham por cima dela (painel de Salvos, cartao da atualizacao, spotlight,
// avisos, cartoes, folhas e o player). Ver src/ilha.h (ilha_evento_dispensar)
// e a ordem de app_evento em src/app.c.
//
// Sem janela e sem GL: ilha.c e ilhasinais.c ligados com stubs no-op do
// desenho e do relogio injetado (o proprio SDL_GetTicks, que ilha.c le para o
// guarda de 250 ms). farolilha.c e farolregra.c sao os de producao.
//
// O QUE COBRE (a revisao farol20-r1 reprovou nestes pontos):
//   1. aviso dispensavel visivel   -> Voltar consome, fecha e entrega a chave;
//   2. ilha coberta (ilha_coberta) -> Voltar NAO consome nem dispensa;
//   3. ultimo desenho velho (player/tela opaca > 250 ms) -> idem;
//   4. player_mini_ativo() (PiP)   -> suprime o aviso proativo;
//   5. ordem em app.c              -> ilha_evento_dispensar DEPOIS das
//      camadas (conferido em tests/farol_ilha.sh).
#include "ilha.h"
#include "ilhasinais.h"
#include "anim.h"
#include "ponteiro.h"
#include "farolilha.h"
#include "farolregra.h"
#include "farol.h"
#include "resolverregra.h"
#include "ilha_voo.h"
#include "desempenho.h"
#include "plrilha.h"
#include "catalogo.h"
#include "recomenda.h"
#include "sync.h"
#include "perfis.h"
#include "traktauth.h"
#include "farolsonda.h"
#include "farolanel.h"
#include "rede.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Em c4f3d39f5b (antes da correcao) ilha_evento_dispensar nao existe: o weak
// fica NULL e o teste cai no ilha_evento antigo, que engole o Voltar — e ai
// os CHECA de "nao dispensa" FALHAM, provando a falha.
int ilha_evento_dispensar(const SDL_Event *e) __attribute__((weak));

// ---------------------------------------------------------------- relogio --
static unsigned msAgora;
Uint32 SDL_GetTicks(void) { return msAgora; }   // ilha.c le isto no guarda
static long long segAgora = 1800000000LL;
static long long relSeg(void) { return segAgora; }
static unsigned  relMs(void)  { return msAgora; }

// ------------------------------------------------------------- cenario ---
static int stubPlayerAberto, stubPlayerRetido, stubPlayerMini, stubResolver;
int player_aberto(void)       { return stubPlayerAberto; }
int player_retido(void)       { return stubPlayerRetido; }
int player_mini_ativo(void)   { return stubPlayerMini; }
int player_ultimo_titulo(CatItem *out, int *temporada, int *episodio) {
  (void)out; (void)temporada; (void)episodio; return 0;
}
int resolver_aberto(void)     { return stubResolver; }

static int stubFalhas, stubParadas;
void farol_fatos_ler(FarolFatos *out) {
  farolregra_fatos_iniciar(out);
  out->falhasSeguidas = stubFalhas;
  out->paradas5min    = stubParadas;
}
void resolverregra_testar(int sintoma, const ResolverFatos *f, ResolverResultado *r) {
  (void)sintoma; (void)f; memset(r, 0, sizeof *r);
}
int  farolsonda_estado(FarolSonda *out) { if (out) memset(out, 0, sizeof *out); return 0; }
uint64_t farolanel_seq(void) { return 0; }
int  farolanel_ler_seq(FarolEvento *dst, int max, uint32_t desde_ms, uint64_t *seqFim) {
  (void)dst; (void)max; (void)desde_ms; if (seqFim) *seqFim = 0; return 0;
}
void plrilha_pedir(const PlrIlhaPedido *p) { (void)p; }
char *dados_ler(const char *nome)      { (void)nome; return NULL; }
int   dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 0; }
const char *i18n(const char *s)        { return s; }

// Sem sinais de sonda: nada de avisos estranhos na fila.
int  rede_saude_offline(void) { return 0; }
unsigned rede_saude_seq(void) { return 0; }
void rede_avisar_saude(void (*f)(int codigo, const char *url, unsigned primeiraMs)) { (void)f; }
void rede_avisar_host(void (*f)(const char *url, int codigo, int http, unsigned ms)) { (void)f; }
void rede_saude_nota_desde(int codigo, const char *url, unsigned primeiraMs) { (void)codigo; (void)url; (void)primeiraMs; }
void rede_hosts_nota(const char *url, int codigo, int http, unsigned ms) { (void)url; (void)codigo; (void)http; (void)ms; }
unsigned addons_fora_do_ar(char *nome, unsigned tam) { (void)nome; (void)tam; return 0; }
SyncEstado sync_estado(void) { return SYNC_PARADO; }
int  sync_manual_addons_estado(int *a, int *r, int *m) { (void)a; (void)r; (void)m; return 0; }
int  sync_addons_fora(void) { return 0; }
int  sync_servidor_fora(void) { return 0; }
int  debrid_sem_plano(void) { return 0; }
const char *debrid_sem_plano_nome(int m) { (void)m; return NULL; }
const char *debrid_sem_plano_frase(int m) { (void)m; return NULL; }
TraEstado traktauth_estado(void) { return 0; }
int  recomenda_ativo(void) { return 0; }
int  recomenda_n_pedidos(void) { return 0; }
int  recomenda_pedido(int i, RecPessoa *s) { (void)i; (void)s; return 0; }
int  recomenda_aceitar(const char *p) { (void)p; return 0; }
int  recomenda_recusar(const char *p) { (void)p; return 0; }
int  recomenda_contatos(RecContato *s, int max) { (void)s; (void)max; return 0; }
const char *rec_genero_rotulo(int i) { (void)i; return ""; }
int  perfis_sessao_atual(uint64_t g) { (void)g; return 1; }
uint64_t perfis_sessao_geracao(void) { return 1; }
const ContaPerfil *perfis_item_ativo(void) { return NULL; }

void ilhasalvar_acao(const char *c, int b) { (void)c; (void)b; }
void ilhasalvar_passo(Uint32 a)          { (void)a; }
void lstesc_passo(void)                  {}
void enquete_passo(Uint32 a)             { (void)a; }
void enquete_acao(const char *c, int b)  { (void)c; (void)b; }
void juntos_passo(Uint32 a)              { (void)a; }
void juntos_acao(const char *c, int b)   { (void)c; (void)b; }
void juntosui_atualizar(Uint32 a)        { (void)a; }
void dormiu_acao(const char *c, int b)   { (void)c; (void)b; }
void diarionuvem_acao(int b)             { (void)b; }
void avisos_ilha_acao(const char *c, int b) { (void)c; (void)b; }
void ilhaacao_botao(int b)               { (void)b; }
void ilhacart_vendo_acao(const char *c)  { (void)c; }
void avisodisp_por(const char *c)        { (void)c; }
int  avisodisp_tem(const char *c)        { (void)c; return 0; }

// ------------------------------------------------------------------ gfx ---
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float f, float raio, float s,
              float a, float cr, float cg, float cb, float ca) {
  (void)r; (void)t; (void)m; (void)f; (void)raio; (void)s; (void)a;
  (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_anel_fora(GfxRect p, float raio, float folga, float esp,
                   float cr, float cg, float cb, float ca) {
  (void)p; (void)raio; (void)folga; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_chrome(GfxRect r, float raio, float a)       { (void)r; (void)raio; (void)a; }
void gfx_dissolver_tela(float a)                      { (void)a; }
int  gfx_efeitos_leves(void)                          { return 1; }
int  gfx_efeitos_minimos(void)                        { return 1; }
float gfx_escala_entrar(void)                         { return 1.0f; }
void  gfx_escala_sair(float a)                        { (void)a; }
float gfx_escala_txt(void)                            { return 1.0f; }
float gfx_escala_ui(void)                             { return 1.0f; }
void  gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) {
  (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_luz_canto(GfxRect r, float raio, float cx, float cy, float alc,
                   float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cx; (void)cy; (void)alc; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_recorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_sem_recorte(void)                           {}
void gfx_vidro_chip(GfxRect r, float raio, float f, float a) { (void)r; (void)raio; (void)f; (void)a; }
void gfx_vidro_material(GfxRect r, float raio, float d, float a) { (void)r; (void)raio; (void)d; (void)a; }
void gfx_vidro_miolo(GfxRect r, float raio, float cr, float cg, float cb, float ca, float a) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca; (void)a;
}
float gfx_tex_aspect_atual;       // dados citados pelo desenho (nao usado aqui)
float gfx_card_forcar_cover_atual;
int   gfx_n_rect;
double gfx_ms_rect;

// ------------------------------------------------------------------ txt ---
int txt_pendentes;
int   txt_largura(TxtEstilo e, const char *s)       { (void)e; return s ? (int)strlen(s) * 8 : 0; }
int   txt_largura_familia(TxtEstilo e, const char *s, TxtFamilia f) { (void)e; (void)f; return s ? (int)strlen(s) * 8 : 0; }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  (void)e; (void)r; (void)g; (void)b; (void)a;
  TxtLinha l; memset(&l, 0, sizeof l); l.w = s ? (int)strlen(s) * 8 : 0; l.h = 20; return l;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float w) {
  (void)w; return txt_linha(e, s, r, g, b, a);
}
TxtLinha txt_linha_corta_familia(TxtEstilo e, const char *s, int r, int g, int b, int a, float w, TxtFamilia f) {
  (void)f; return txt_linha_corta(e, s, r, g, b, a, w);
}
void  txt_desenhar(TxtLinha l, float x, float y)                 { (void)l; (void)x; (void)y; }
void  txt_desenhar_alpha(TxtLinha l, float x, float y, float a)  { (void)l; (void)x; (void)y; (void)a; }
void  txt_desenhar_peso(TxtLinha l, float x, float y, float a, float p) { (void)l; (void)x; (void)y; (void)a; (void)p; }
void  txt_desenhar_borda(TxtLinha l, float x, float y, int b, float a)  { (void)l; (void)x; (void)y; (void)b; (void)a; }
void  txt_desenhar_tom(TxtLinha l, float x, float y, float cr, float cg, float cb, float a) {
  (void)l; (void)x; (void)y; (void)cr; (void)cg; (void)cb; (void)a;
}
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b,
                      float x, float y, float larg, float leading, float alpha, int maxLinhas) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)larg;
  (void)leading; (void)alpha; (void)maxLinhas; return 0.0f;
}
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float a, float tr) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)a; (void)tr; return 0.0f;
}
float sinopse_bloco_corta(TxtEstilo o, const char *t, int r, int g, int b,
                          float x, float y, float w, float leading, float a, int maxL) {
  (void)o; (void)t; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w;
  (void)leading; (void)a; (void)maxL; return 0.0f;
}
float sinopse_entrelinha(TxtEstilo o, float leading) { (void)o; return leading; }
int   tex_marca_escura(const char *c) { (void)c; return 0; }
float tex_aspecto(const char *c)      { (void)c; return 0.0f; }
GLuint tex_obter_larg(const char *c, float l)        { (void)c; (void)l; return 0; }
GLuint tex_obter_larg_qualquer(const char *c, float l) { (void)c; (void)l; return 0; }

// ------------------------------------------------------------------ ui ----
void  ajustes_acento(float *r, float *g, float *b) { if (r) *r = 0.4f; if (g) *g = 0.7f; if (b) *b = 1.0f; }
int   ajustes_relogio_12h(void)   { return 0; }
int   ajustes_relogio_ligado(void){ return 1; }
int   ajustes_animacoes_reduzidas(void) { return 0; }
int   ajustes_vidro(void)         { return 1; }
float botao_largura(const char *rotulo, const char *icone, int prim) {
  (void)rotulo; (void)icone; (void)prim; return 120.0f;
}
void  central_segurar_desenhar(GfxRect r, float raio, float a, Uint32 agora) {
  (void)r; (void)raio; (void)a; (void)agora;
}
float central_segurar_progresso(Uint32 agora) { (void)agora; return 0.0f; }
void  desempenho_corpo(int forma, GfxRect r, float a)          { (void)forma; (void)r; (void)a; }
void  desempenho_corpo_tam(int forma, float *w, float *h)      { (void)forma; if (w) *w = 0; if (h) *h = 0; }
int   desempenho_forma(void)                                   { return DS_DESLIGADO; }
void  desempenho_linha(int forma, float x, float yc, float a)  { (void)forma; (void)x; (void)yc; (void)a; }
float desempenho_linha_w(int forma)                            { (void)forma; return 0.0f; }
void  menu_avatar(float x, float y, float size, float alpha)   { (void)x; (void)y; (void)size; (void)alpha; }
float plrui_botao(float x, float y, const char *rotulo, const char *icone, float foco, float a) {
  (void)x; (void)y; (void)rotulo; (void)icone; (void)foco; (void)a; return 0.0f;
}
float plrui_botao_largura(const char *rotulo, const char *icone) { (void)rotulo; (void)icone; return 0.0f; }
void  plrui_respira(float cx, float cy, float d, Uint32 agora, float a) { (void)cx; (void)cy; (void)d; (void)agora; (void)a; }
void  plrui_trilho(GfxRect r, float frac, float cr, float cg, float cb, float a) {
  (void)r; (void)frac; (void)cr; (void)cg; (void)cb; (void)a;
}
void  ponteiro_alvo(float x, float y, float w, float h, PonteiroFn f, PonteiroFn a, int b, int c) {
  (void)x; (void)y; (void)w; (void)h; (void)f; (void)a; (void)b; (void)c;
}
void  ponteiro_alvo_faixa(float x, float y, float w, float h, float y0, float y1,
                          PonteiroFn f, PonteiroFn a, int b, int c) {
  (void)x; (void)y; (void)w; (void)h; (void)y0; (void)y1; (void)f; (void)a; (void)b; (void)c;
}
void  ponteiro_alvo_arrastavel(void)      {}
void  ponteiro_alvo_segurar(PonteiroFn f) { (void)f; }
void  ponteiro_camada(void)               {}
void  rec_avatar_estilo(GfxRect a, const char *url, const char *nome, const char *id,
                        float alfa, int estilo) {
  (void)a; (void)url; (void)nome; (void)id; (void)alfa; (void)estilo;
}
void  sintro_tecla_atalho(float x, float y, float lado, float a) { (void)x; (void)y; (void)lado; (void)a; }
int   trailer_tocando(void) { return 0; }

// ------------------------------------------------------------ o teste -----
static int falhas;
#define CHECA(c, msg) do { if (!(c)) { printf("FALHOU: %s (linha %d)\n", msg, __LINE__); falhas++; } \
                           else printf("ok: %s\n", msg); } while (0)

static void tecla(SDL_Event *e, SDL_Keycode k) {
  memset(e, 0, sizeof *e);
  e->type = SDL_KEYDOWN;
  e->key.keysym.sym = k;
}

static void avisoFarol(const char *chave) {
  IlhaAvisoEx a;
  memset(&a, 0, sizeof a);
  a.chave = chave; a.tipo = ILHA_ACENTO; a.prior = ILHA_P1;
  a.icone = "aj_farol"; a.texto = "A fonte recusou o pedido.";
  a.ms = 8000u; a.tecla = 1; a.acao = 1; a.ponto = 1; a.dispensavel = 1;
  ilha_avisar_ex(&a);
}

// `n` quadros de 16 ms da ilha, a partir de `msAgora` (anda o relogio junto).
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) { msAgora += 16; ilha_desenhar(msAgora); }
}

static void sessaoLimpa(void) {
  ilha_abrir_sessao();        // tira avisos/cartoes da vez anterior
  farolilha_zerar();
  msAgora = 100000u;
  stubPlayerAberto = stubPlayerRetido = stubPlayerMini = stubResolver = 0;
}

int main(void) {
  SDL_Event ev;
  char chave[96];

  anim_politica_reduzida = 1;   // tela de teste: tudo assenta no quadro
  farolilha_relogio(relSeg, relMs);
  ilha_relogio_visivel(0);      // sem relogio: so o aviso decide a pilula
  ilha_ancorar(1600.0f, 30.0f, 1);
  ilhasinais_iniciar();         // liga o relogio do farolilha via producao

  // --- 1. AVISO VISIVEL: Voltar dispensa ------------------------------------
  sessaoLimpa();
  avisoFarol("farol");
  quadros(40);                  // ~0,6 s: pílula assentada, curAte armado
  CHECA(ilha_tem("farol"), "aviso do farol na pilula");
  tecla(&ev, SDLK_AC_BACK);
  CHECA(ilha_evento(&ev) == 0, "ilha_evento nao dispensa mais (fila)");
  {
    // No app real isto e o que sobra do Voltar depois das camadas.
    int comeu;
    int (*disp)(const SDL_Event *) = ilha_evento_dispensar;
    comeu = disp ? disp(&ev) : ilha_evento(&ev);
    CHECA(comeu == 1, "visivel: Voltar dispensa o aviso");
  }
  CHECA(ilha_aviso_dispensado(chave, sizeof chave) == 1 && !strcmp(chave, "farol"),
        "visivel: ilha_aviso_dispensado entrega a chave");
  CHECA(!ilha_tem("farol"), "visivel: aviso saiu da pilula");

  // --- 2. COBERTA (painel de Salvos / cartao da atualizacao) ----------------
  sessaoLimpa();
  avisoFarol("farol");
  quadros(40);                  // visivel um instante
  // o painel que nasceu dela passa por cima: app_desenhar chama ilha_coberta
  // a CADA quadro enquanto a pilula esta debaixo dele
  for (int i = 0; i < 30; i++) { ilha_coberta(1); msAgora += 16; ilha_desenhar(msAgora); }
  tecla(&ev, SDLK_AC_BACK);
  CHECA(ilha_evento(&ev) == 0, "coberta: ilha_evento nao toca o Voltar");
  {
    int (*disp)(const SDL_Event *) = ilha_evento_dispensar;
    int comeu = disp ? disp(&ev) : ilha_evento(&ev);
    CHECA(comeu == 0, "coberta: Voltar NAO dispensa o aviso oculto");
  }
  CHECA(ilha_aviso_dispensado(chave, sizeof chave) == 0,
        "coberta: nada e dispensado");
  CHECA(ilha_tem("farol"), "coberta: o aviso continua na pilula");
  tecla(&ev, SDLK_ESCAPE);      // outra tecla de voltar, mesmo resultado
  {
    int (*disp)(const SDL_Event *) = ilha_evento_dispensar;
    int comeu = disp ? disp(&ev) : ilha_evento(&ev);
    CHECA(comeu == 0, "coberta: Escape tambem nao dispensa");
  }
  ilha_coberta(0);
  quadros(20);                  // descoberta de novo: o Voltar volta a dispensar
  tecla(&ev, SDLK_AC_BACK);
  {
    int (*disp)(const SDL_Event *) = ilha_evento_dispensar;
    int comeu = disp ? disp(&ev) : ilha_evento(&ev);
    CHECA(comeu == 1, "descoberta: Voltar dispensa de novo");
  }
  CHECA(ilha_aviso_dispensado(chave, sizeof chave) == 1,
        "descoberta: a dispensa acontece com a pilula visivel");

  // --- 3. TELA OPAQUE / PLAYER: ultimo desenho envelhecido ------------------
  // Com player_aberto() ou numa camada que cobre a ilha, app.c nao a desenha
  // mais; o guarda so aceita o Voltar se o ULTIMO quadro desenhado de fato
  // for recente (< 250 ms). Aqui a pilula some da tela por 1 s (janela do
  // jogador abrindo, tela opaca) e o Voltar nao pode dispensar um aviso que
  // ninguem ve.
  sessaoLimpa();
  avisoFarol("farol");
  quadros(40);
  stubPlayerAberto = 1;         // a tela do player abriu por cima
  msAgora += 1000u;             // 1 s sem ilha_desenhar (ela nao e chamada)
  tecla(&ev, SDLK_AC_BACK);
  {
    int (*disp)(const SDL_Event *) = ilha_evento_dispensar;
    int comeu = disp ? disp(&ev) : ilha_evento(&ev);
    CHECA(comeu == 0, "player ha 1 s: Voltar nao dispensa o aviso velho");
  }
  CHECA(ilha_aviso_dispensado(chave, sizeof chave) == 0,
        "player ha 1 s: nada dispensado");
  CHECA(ilha_tem("farol"), "player ha 1 s: aviso ainda espera a volta");
  ilha_retirar("farol");

  // --- 4. PiP (player_mini_ativo) suprime o aviso proativo ------------------
  sessaoLimpa();
  ilha_relogio_visivel(0);
  stubFalhas = 3; stubParadas = 0;
  stubPlayerRetido = 0; stubPlayerMini = 1;   // canal ao vivo no mini-player
  ilhasinais_passo(msAgora += 2100u);         // passa a sonda (SONDA_MS)
  ilhasinais_passo(msAgora += 2100u);
  CHECA(!ilha_tem("farol"), "PiP ativo: aviso proativo suprimido");
  stubPlayerMini = 0;
  ilhasinais_passo(msAgora += 2100u);
  CHECA(ilha_tem("farol"), "sem PiP nem player: aviso proativo aparece");
  {
    // e ele e DISPENSAVEL — o contrato que o Voltar de cima usa
    int (*disp)(const SDL_Event *) = ilha_evento_dispensar;
    int comeu;
    quadros(40);
    tecla(&ev, SDLK_AC_BACK);
    comeu = disp ? disp(&ev) : ilha_evento(&ev);
    CHECA(comeu == 1, "aviso proativo e dispensavel com a pilula visivel");
  }

  if (falhas) { printf("farol_ilha_voltar: %d falha(s)\n", falhas); return 1; }
  printf("farol_ilha_voltar: tudo ok\n");
  return 0;
}
