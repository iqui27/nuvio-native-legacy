#include "../src/login.c"
#include <assert.h>

static Uint32 tick;
static SesEstado estado;
static const char *codigo;
static int redeFalhou, pedidos, mostrouPendente, mostrouFalha;
static float focoNovo;
static PonteiroFn alvoNovo;
static int indiceNovo;

Uint32 SDL_GetTicks(void) { return tick; }
SesEstado sessao_estado(void) { return estado; }
int sessao_logada(void) { return 0; }
int nuvem_pronta(void) { return 1; }
const char *sessao_url_login(void) { return "https://conta.exemplo/tv"; }
void sessao_passo(unsigned agora) { (void)agora; }
void sessao_login_comecar(void) {}
void sessao_login_email(const char *usuario, const char *segredo) { (void)usuario; (void)segredo; }
int ajustes_login_codigo(void) { return 1; }
void parear_rede_passo(const char *url, int ligado) { (void)url; (void)ligado; }
const char *parear_rede_codigo(void) { return codigo; }
const char *parear_rede_pagina(void) { return "parear.exemplo/c"; }
void parear_rede_esquecer(void) {}
int parear_rede_falhou(void) { return redeFalhou; }
int parear_rede_renovar(void) {
  pedidos++;
  codigo = "";
  redeFalhou = 0;
  return 1;
}
void teclado_abrir_com(const char *titulo, const char *dica, int max, const char *alfa,
                      const char *inicial) {
  (void)titulo; (void)dica; (void)max; (void)alfa; (void)inicial;
}
void teclado_tipo(int tipo) { (void)tipo; }
void teclado_mascarar(int mascara) { (void)mascara; }
int teclado_mascarado(void) { return 0; }
int teclado_aberto(void) { return 0; }
void teclado_evento(const SDL_Event *evento) { (void)evento; }
void teclado_atualizar(float dt, Uint32 agora) { (void)dt; (void)agora; }
int teclado_resultado(void) { return TECLADO_NADA; }
const char *teclado_texto(void) { return ""; }
void teclado_esquecer(void) {}
const char *i18n(const char *texto) { return texto; }

TxtLinha txt_linha(TxtEstilo estilo, const char *texto, int r, int g, int b, int a) {
  (void)estilo; (void)a;
  if (!strcmp(texto, "Gerando um código novo…")) mostrouPendente = 1;
  if (!strcmp(texto, "--- ---")) {
    assert(r == 163 && g == 161 && b == 156);
    mostrouFalha = 1;
  }
  return (TxtLinha){ .h = 60.0f };
}

TxtLinha txt_linha_corta(TxtEstilo estilo, const char *texto, int r, int g, int b, int a, float larg) {
  (void)larg;
  return txt_linha(estilo, texto, r, g, b, a);
}

void txt_desenhar_alpha(TxtLinha linha, float x, float y, float alpha) {
  (void)linha; (void)x; (void)y; (void)alpha;
}

float txt_bloco_corta(TxtEstilo estilo, const char *texto, int r, int g, int b,
                     float x, float y, float larg, float leading, float alpha, int maxLinhas) {
  (void)estilo; (void)texto; (void)r; (void)g; (void)b; (void)x; (void)y; (void)larg;
  (void)alpha; (void)maxLinhas;
  return leading;
}

void botao_pilula_sem_aro(GfxRect rect, const char *rotulo, const char *icone,
                         float foco, int primario, int alinhar, float alpha) {
  (void)rect; (void)icone; (void)primario; (void)alinhar; (void)alpha;
  if (!strcmp(rotulo, "Novo código")) focoNovo = foco;
}

void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn focar, PonteiroFn rolar,
                   int a, int b) {
  (void)x; (void)y; (void)w; (void)h; (void)rolar; (void)b;
  alvoNovo = focar;
  indiceNovo = a;
}

static void tecla(SDL_Keycode codigoTecla) {
  SDL_Event evento = { .key = { .type = SDL_KEYDOWN, .keysym.sym = codigoTecla } };
  login_evento(&evento);
}

static void conferirColuna(int pendente, int falhou) {
  mostrouPendente = mostrouFalha = 0;
  focoNovo = 0.0f;
  alvoNovo = NULL;
  desenharCodigoAjuda(0.0f);
  assert(alvoNovo && focoNovo == 1.0f);
  assert(mostrouPendente == pendente && mostrouFalha == falhou);
}

static void conferirRenovacao(Uint32 inicio, int status) {
  estado = SES_AGUARDANDO;
  codigo = "111 111";
  pedidos = redeFalhou = 0;
  anim_politica_reduzida = 1;
  login_iniciar();
  tick = inicio;
  tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  assert(pedidos == 1);
  login_atualizar(0.0f, tick);
  conferirColuna(1, 0);
  login_atualizar(0.0f, tick - 1);
  conferirColuna(1, 0);

  if (status == 503) {
    redeFalhou = 1;
    tick = inicio + 4000;
  } else {
    login_atualizar(0.0f, inicio + 20000);
    conferirColuna(1, 0);
    tick = inicio + 20001;
  }
  login_atualizar(0.0f, tick);
  conferirColuna(0, 1);
  tecla(SDLK_RETURN);
  assert(pedidos == 2);
  login_atualizar(0.0f, tick);
  conferirColuna(1, 0);

  redeFalhou = 1;
  tick += 4000;
  login_atualizar(0.0f, tick);
  conferirColuna(0, 1);
  tecla(SDLK_LEFT);
  alvoNovo(indiceNovo, 0);
  tecla(SDLK_RETURN);
  assert(pedidos == 3);
  codigo = "222 222";
  login_atualizar(0.0f, tick + 1);
  conferirColuna(0, 0);
}

int main(void) {
  conferirRenovacao(1000, 0);
  conferirRenovacao(1001, 503);
  puts("parear login: ok");
  return 0;
}
