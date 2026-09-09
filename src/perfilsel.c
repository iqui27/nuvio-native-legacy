#include "perfilsel.h"
#include "idioma.h"
#include "perfis.h"
#include "sync.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "sessao.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

// --- MEDIDAS -----------------------------------------------------------------
//
// UMA FILEIRA, nunca uma grade. A grade 4x4 que estava aqui obrigava o D-pad a
// ter quatro direcoes numa tela cuja pergunta e linear ("qual destes?"), e com
// 5 perfis deixava um orfao sozinho na segunda linha. Numa fileira o CIMA e o
// BAIXO ficam livres — e e por isso que o teclado do PIN pode nascer embaixo
// sem disputar tecla com nada.
//
// O diametro do avatar SAI DA CONTA, nao de uma constante: com 2 perfis a tela
// tem espaco para circulos enormes e com 8 nao tem. O teto de 288 existe porque
// acima disso o nome embaixo comeca a parecer legenda de foto; o piso de 168 e
// o menor circulo em que a inicial ainda se le a 3 m.
#define PS_AV_MAX      288.0f
#define PS_AV_MIN      168.0f
#define PS_VAO_RAZAO     0.34f   // vao entre avatares, em fracao do diametro
#define PS_VAO_MIN       28.0f
// O VAO CRESCE QUANDO SOBRA LARGURA. Com dois perfis o diametro bate no teto de
// 288 e sobram mais de 1100 px de tela vazia — mas o vao continuava em 0.34*d, e
// os dois circulos ficavam encostados no meio com o halo do focado invadindo o
// vizinho. Deixar o vao usar parte da folga separa os dois sem mexer no tamanho.
// O teto de 0.62 e o ponto em que a fileira comeca a ler como dois elementos
// soltos em vez de uma lista.
#define PS_VAO_RAZAO_MAX 0.62f
#define PS_TITULO_Y     148.0f
#define PS_SUB_Y        258.0f
#define PS_FILA_Y       396.0f   // topo do avatar SEM foco
#define PS_NOME_GAP      36.0f   // base do avatar -> topo do nome
#define PS_SELO_GAP      12.0f   // base do nome -> topo do selo de PIN
#define PS_DICA_Y       948.0f
#define PS_ARTE_H       648.0f   // faixa da arte de fundo do perfil focado
#define PS_HALO_N            5   // aneis do brilho atras do avatar focado
#define PS_HALO_ATE      0.78f   // quanto o ultimo anel passa do avatar
#define PS_HALO_ALFA     0.055f

// --- PIN ---------------------------------------------------------------------
//
// Teclado 3x4, na ordem do telefone. O que havia aqui era 5 colunas com 12
// teclas: as duas ultimas (apagar e OK) sobravam sozinhas numa terceira linha
// encostada a esquerda, e o olho procurava o OK no canto errado toda vez.
#define PS_PIN_MAX       8
#define PS_TECLA        96.0f
#define PS_TECLA_GAP    18.0f
#define PS_TECLA_COLS       3
#define PS_TECLA_LINS       4
#define PS_PIN_APAGAR       9
#define PS_PIN_ZERO        10
#define PS_PIN_OK          11
#define PS_PONTO        22.0f    // diametro do ponto que mascara um digito
#define PS_PONTO_PASSO  40.0f

static int foco;
static int concluido, sair, repetir;
static float animFoco[CONTA_PERFIL_MAX];
static float animEntrada;        // 0..1: a tela sobe e aparece uma vez so
static float animPin;            // 0..1: o veu e o teclado do PIN

// Arte de fundo do perfil focado. Nao ha crossfade de DUAS artes ao mesmo
// tempo: seria uma segunda camada de tela cheia, e a nota no topo de gfx.c diz
// que duas delas derrubam esta Mali para 40fps. Aqui a de saida apaga e a de
// entrada acende, o que custa uma camada e le igual em movimento.
static char fundoNaTela[300];
static float fundoAlfa;

// Estado do PIN: -1 = nenhum perfil pedindo PIN.
static int pinDe = -1;
static char pin[PS_PIN_MAX + 1];
static int pinFoco;              // indice na grade 3x4; ver PS_PIN_*
static int pinErrado, pinRede;
static pthread_t fioPin;
static int verificando;
static _Atomic int resultadoPin; // 0 pendente, 1 ok, -1 PIN incorreto, -2 rede
static _Atomic unsigned pinGeracao;
typedef struct { unsigned geracao; int slot, indice; char valor[PS_PIN_MAX + 1]; } PinTarefa;

static int corDe(const char *hex, float *r, float *g, float *b) {
  unsigned v = 0;
  if (!hex || hex[0] != '#' || strlen(hex) < 7) return 0;
  if (sscanf(hex + 1, "%6x", &v) != 1) return 0;
  *r = ((v >> 16) & 255) / 255.0f;
  *g = ((v >> 8) & 255) / 255.0f;
  *b = (v & 255) / 255.0f;
  return 1;
}

// A cor da conta pode vir escura demais para ser vista contra o #0D0D0D do
// fundo (MEDIDO: ha perfis com #1A1A1A no servidor). Sem um piso de
// luminancia, o avatar desses perfis some e a tela mostra um buraco no lugar
// da pessoa. Clareia proporcionalmente, preservando o matiz.
static void corLegivel(float *r, float *g, float *b) {
  float lum = 0.2126f * *r + 0.7152f * *g + 0.0722f * *b;
  if (lum >= 0.16f) return;
  { float k = lum > 0.001f ? 0.16f / lum : 0.0f;
    if (k > 6.0f) k = 6.0f;
    *r = anim_clamp(*r * k + 0.10f, 0.0f, 1.0f);
    *g = anim_clamp(*g * k + 0.10f, 0.0f, 1.0f);
    *b = anim_clamp(*b * k + 0.10f, 0.0f, 1.0f); }
}

// Primeiro CARACTERE, nao primeiro byte: "Álvaro" tem dois bytes na primeira
// letra e cortar no byte produz um glifo invalido.
static void inicialDe(const char *nome, char *dst, size_t tam) {
  size_t z = 1;
  if (tam < 5) { if (tam) dst[0] = 0; return; }
  if (!nome || !nome[0]) { dst[0] = '?'; dst[1] = 0; return; }
  while (z < 4 && (nome[z] & 0xc0) == 0x80) z++;
  memcpy(dst, nome, z);
  dst[z] = 0;
}

static float diametro(int m) {
  float util = NV_TELA_W - 2.0f * NV_MARGEM_X;
  float d;
  if (m <= 0) return PS_AV_MAX;
  d = util / ((float)m + PS_VAO_RAZAO * (float)(m - 1));
  return anim_clamp(d, PS_AV_MIN, PS_AV_MAX);
}

static float vaoDe(int m, float d) {
  float util = NV_TELA_W - 2.0f * NV_MARGEM_X;
  float g = d * PS_VAO_RAZAO;
  if (m <= 1) return g;
  { float sobra = (util - (float)m * d) / (float)(m - 1);
    // Sobrando espaco, o vao se estica ate PS_VAO_RAZAO_MAX; faltando, ele
    // encolhe ate PS_VAO_MIN. E a mesma conta nos dois sentidos.
    float teto = d * PS_VAO_RAZAO_MAX;
    if (sobra > g) g = sobra > teto ? teto : sobra;
    else g = sobra; }
  return g < PS_VAO_MIN ? PS_VAO_MIN : g;
}

void perfilsel_iniciar(void) {
  int i;
  concluido = sair = repetir = 0;
  pinDe = -1;
  pin[0] = 0;
  pinFoco = PS_PIN_OK;
  pinErrado = 0;
  pinRede = 0;
  verificando = 0;
  animEntrada = 0.0f;
  animPin = 0.0f;
  fundoNaTela[0] = 0;
  fundoAlfa = 0.0f;
  atomic_fetch_add(&pinGeracao, 1);
  atomic_store(&resultadoPin, 0);
  // O cursor nasce no perfil ativo. A regra vive em perfis.c porque e ela que
  // um teste sem SDL consegue provar.
  foco = perfis_indice_sugerido();
  for (i = 0; i < CONTA_PERFIL_MAX; i++) animFoco[i] = (i == foco) ? 1.0f : 0.0f;
}

static void *fioVerificar(void *u) {
  PinTarefa *t = u;
  // A verificacao mora em perfis.c, que e o unico lugar que monta o corpo da
  // RPC. Aqui havia uma segunda copia com snprintf, e ela nao escapava o PIN:
  // uma aspa digitada quebrava o JSON e o servidor recusava tudo.
  // Tres respostas: 1 aceitou, 0 recusou, -1 nao deu para perguntar. O ultimo
  // caso vira -2 aqui (o codigo de "sem conexao" desta tela), e nao -1: dizer
  // "PIN incorreto" a quem esta sem rede e acusar a pessoa do erro do aparelho.
  int v = perfis_verificar_pin(t->indice, t->valor);
  // O PIN sai da memoria assim que deixa de ser necessario. Nao ha log dele em
  // lugar nenhum deste arquivo, e nao pode passar a haver.
  memset(t->valor, 0, sizeof t->valor);
  if (t->geracao == atomic_load(&pinGeracao) && pinDe == t->slot && verificando)
    atomic_store(&resultadoPin, v > 0 ? 1 : (v < 0 ? -2 : -1));
  free(t);
  return NULL;
}

static void escolher(int i) {
  const ContaPerfil *p = perfis_item(i);
  switch (perfis_acao(i)) {
    case PERFIL_ACAO_PIN:
      pinDe = i; pin[0] = 0; pinFoco = PS_PIN_OK; pinErrado = pinRede = 0;
      return;
    case PERFIL_ACAO_ENTRAR:
      if (p) perfis_definir_ativo(p->indice);
      concluido = 1;
      return;
    default:
      return;
  }
}

static void eventoPin(SDL_Keycode k) {
  if (verificando) {
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE) {
      atomic_fetch_add(&pinGeracao, 1); atomic_store(&resultadoPin, 0);
      verificando = 0; pinRede = 0; pinErrado = 0;
    }
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE) {
    if (pin[0]) { pin[strlen(pin) - 1] = 0; pinErrado = pinRede = 0; }
    else pinDe = -1;
    return;
  }
  if (k == SDLK_LEFT)  { if (pinFoco % PS_TECLA_COLS > 0) pinFoco--; return; }
  if (k == SDLK_RIGHT) { if (pinFoco % PS_TECLA_COLS < PS_TECLA_COLS - 1) pinFoco++; return; }
  if (k == SDLK_UP)    { if (pinFoco >= PS_TECLA_COLS) pinFoco -= PS_TECLA_COLS; return; }
  if (k == SDLK_DOWN)  { if (pinFoco + PS_TECLA_COLS < PS_TECLA_COLS * PS_TECLA_LINS)
                           pinFoco += PS_TECLA_COLS; return; }
  if (k != SDLK_RETURN && k != SDLK_KP_ENTER) return;

  if (pinFoco == PS_PIN_APAGAR) { if (pin[0]) pin[strlen(pin) - 1] = 0; return; }
  if (pinFoco == PS_PIN_OK) {
    PinTarefa *t;
    const ContaPerfil *p = perfis_item(pinDe);
    if (!pin[0]) return;
    t = malloc(sizeof *t);
    if (!t || !p) { free(t); pinRede = 1; return; }
    t->geracao = atomic_load(&pinGeracao); t->slot = pinDe; t->indice = p->indice;
    snprintf(t->valor, sizeof t->valor, "%s", pin);
    verificando = 1;
    pinErrado = pinRede = 0;
    atomic_store(&resultadoPin, 0);
    // Verificar BLOQUEIA (uma viagem ao servidor). Num fio, para a tela nao
    // congelar por um segundo a cada tentativa.
    if (pthread_create(&fioPin, NULL, fioVerificar, t) == 0) pthread_detach(fioPin);
    else { memset(t->valor, 0, sizeof t->valor); free(t); verificando = 0; pinRede = 1; }
    return;
  }
  { size_t z = strlen(pin);
    int digito = (pinFoco == PS_PIN_ZERO) ? 0 : pinFoco + 1;
    if (z < PS_PIN_MAX) { pin[z] = (char)('0' + digito); pin[z + 1] = 0; } }
}

void perfilsel_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int m = perfis_n();
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (pinDe >= 0) { eventoPin(k); return; }

  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE) { sair = 1; return; }
  // Com a lista na tela, o OK escolhe. Sem ela (rede caida), o OK e a unica
  // acao que faz sentido: tentar de novo.
  if (m == 0 && sync_estado() == SYNC_FALHOU &&
      (k == SDLK_RETURN || k == SDLK_KP_ENTER)) { repetir = 1; return; }
  if (k == SDLK_RIGHT) { if (foco < m - 1) foco++; }
  else if (k == SDLK_LEFT) { if (foco > 0) foco--; }
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) escolher(foco);
}

void perfilsel_atualizar(float dt, Uint32 agora) {
  int i, reduzida = ajustes_animacoes_reduzidas();
  const ContaPerfil *pf;
  (void)agora;

  animEntrada = anim_reduzida(anim_mola(animEntrada, 1.0f, dt, NV_MOLA_TELA),
                              1.0f, reduzida);
  animPin = anim_reduzida(anim_mola(animPin, pinDe >= 0 ? 1.0f : 0.0f, dt, NV_MOLA_TELA),
                          pinDe >= 0 ? 1.0f : 0.0f, reduzida);
  for (i = 0; i < CONTA_PERFIL_MAX; i++) {
    float alvo = (i == foco && pinDe < 0) ? 1.0f : 0.0f;
    animFoco[i] = anim_mola(animFoco[i], alvo, dt,
                            alvo > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    if (reduzida) animFoco[i] = alvo;
  }

  // A arte do perfil focado APAGA antes de a proxima acender. Ver a nota do
  // fundoNaTela la em cima: e uma camada de tela cheia, e nao duas.
  pf = perfis_item(foco);
  { const char *quer = (pf && pf->fundoUrl[0]) ? pf->fundoUrl : "";
    if (strcmp(quer, fundoNaTela)) {
      fundoAlfa = anim_rampa(fundoAlfa, 0.0f, dt, 180.0f);
      if (fundoAlfa <= 0.001f) snprintf(fundoNaTela, sizeof fundoNaTela, "%s", quer);
    } else if (fundoNaTela[0]) {
      fundoAlfa = anim_rampa(fundoAlfa, 1.0f, dt, 260.0f);
    }
    if (reduzida) fundoAlfa = fundoNaTela[0] ? 1.0f : 0.0f; }

  { int resultado = atomic_load(&resultadoPin);
  if (verificando && resultado) {
    atomic_store(&resultadoPin, 0);
    verificando = 0;
    if (resultado == 1) {
      const ContaPerfil *p = perfis_item(pinDe);
      // Gravar o perfil ativo e do fio de desenho, nunca do fio da rede: e ele
      // que escreve em disco e que o resto do app le todo quadro.
      if (p) perfis_definir_ativo(p->indice);
      memset(pin, 0, sizeof pin);
      pinDe = -1;
      concluido = 1;
    } else if (resultado == -2) {
      pinRede = 1;
      memset(pin, 0, sizeof pin);
    } else {
      pinErrado = 1;
      memset(pin, 0, sizeof pin);
    }
  }
  }
  if (foco >= perfis_n()) foco = perfis_n() > 0 ? perfis_n() - 1 : 0;

  // NAO concluir enquanto o ciclo que BUSCA os perfis ainda esta rodando E a
  // lista ainda esta vazia.
  //
  // O defeito que isto conserta: app.c troca para esta tela logo depois de
  // chamar sync_iniciar(), que e assincrono. No primeiro quadro perfis_n() e 0
  // porque a resposta nao chegou — e "0 perfis" e indistinguivel de "conta de
  // uma pessoa so". A tela se dispensava sozinha ANTES de existir, e uma conta
  // de duas pessoas caia no perfil 1 em silencio: o app sincronizava e
  // ESCREVIA progresso no perfil errado, sem nunca perguntar.
  //
  // Com o cache em disco a lista costuma existir no primeiro quadro, e ai a
  // tela ja e util enquanto o ciclo confirma — por isso a guarda olha tambem o
  // perfis_n(), e nao so o estado do sync.
  if (sync_estado() == SYNC_RODANDO && perfis_n() == 0) return;

  // Terminado o ciclo, "nenhum ou um destravado" e resposta de verdade: seguir
  // direto. Um erro de rede nunca equivale a "uma conta sem perfis", entao so
  // com SYNC_PRONTO. A pergunta e perfis_sem_escolha() e nao
  // perfis_precisa_escolher(): esta tela tambem e aberta DE PROPOSITO pelo
  // "trocar de perfil" do menu, e ali a bandeira de sessao ja esta ligada — a
  // tela se fecharia sozinha no quadro seguinte.
  if (sync_estado() == SYNC_PRONTO && perfis_sem_escolha() && pinDe < 0) concluido = 1;
}

int perfilsel_quer_sair(void) { int v=sair; sair=0; return v; }
int perfilsel_pediu_repetir(void) { int v=repetir; repetir=0; return v; }

// --- DESENHO -----------------------------------------------------------------

// O circulo do perfil: soquete escuro, cor da conta por cima e, quando ha,
// a foto. Tres camadas e nao uma porque a cor precisa DIMINUIR fora do foco
// sem virar um buraco preto sobre a arte de fundo — o soquete e o que garante
// que o dimming seja igual com arte e sem arte.
static void disco(GfxRect a, const ContaPerfil *p, float f, float alfa) {
  float cr = 0.12f, cg = 0.53f, cb = 0.90f;
  float vivo = 0.55f + 0.45f * f;
  GLuint foto;
  corDe(p->corHex, &cr, &cg, &cb);
  corLegivel(&cr, &cg, &cb);
  gfx_rect(a, 0, GFX_DISCO, 0, 0, 0, 0, 0.09f, 0.09f, 0.10f, alfa);
  gfx_rect(a, 0, GFX_DISCO, 0, 0, 0, 0, cr, cg, cb, vivo * alfa);
  foto = p->avatarUrl[0] ? tex_obter_larg(p->avatarUrl, a.w) : 0;
  if (foto) {
    gfx_tex_aspect_atual = tex_aspecto(p->avatarUrl);
    gfx_rect(a, foto, GFX_AVATAR, 0, 0, 0, 0, 1, 1, 1, vivo * alfa);
    gfx_tex_aspect_atual = 0;
  } else {
    char ini[8];
    TxtLinha l;
    inicialDe(p->nome, ini, sizeof ini);
    l = txt_linha(a.w >= 230.0f ? TXT_TITULO1 : TXT_TITULO2, ini, 255, 255, 255, 255);
    txt_desenhar_alpha(l, a.x + (a.w - l.w) * 0.5f, a.y + (a.h - l.h) * 0.5f,
                       (0.80f + 0.20f * f) * alfa);
  }
}

// Brilho da cor do perfil atras do circulo focado. Aneis concentricos de alpha
// baixo em vez de um degrade: o shader nao tem um, e a alternativa (um disco
// grande e translucido) mostra a propria borda. Com PS_HALO_ALFA por anel o
// degrau entre um e o outro fica abaixo do que se ve numa TV.
static void halo(GfxRect a, const ContaPerfil *p, float f) {
  float cr = 0.12f, cg = 0.53f, cb = 0.90f;
  int i;
  if (f <= 0.02f) return;
  corDe(p->corHex, &cr, &cg, &cb);
  corLegivel(&cr, &cg, &cb);
  for (i = PS_HALO_N; i >= 1; i--) {
    float k = PS_HALO_ATE * (float)i / (float)PS_HALO_N * f;
    float cresce = a.w * k;
    GfxRect r = { a.x - cresce * 0.5f, a.y - cresce * 0.5f, a.w + cresce, a.h + cresce };
    gfx_rect(r, 0, GFX_DISCO, 0, 0, 0, 0, cr, cg, cb, PS_HALO_ALFA * f);
  }
}

static void desenhaFundo(void) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  if (fundoNaTela[0] && fundoAlfa > 0.004f) {
    GLuint t = tex_obter_hero(fundoNaTela);
    if (t) {
      GfxRect arte = { 0, 0, NV_TELA_W, PS_ARTE_H };
      // A arte ocupa a FAIXA DE CIMA e se dissolve na base, em vez de cobrir a
      // tela: sao duas camadas de preenchimento em vez de tres, e o contraste
      // do nome e da dica embaixo deixa de depender da foto que a pessoa
      // escolheu. Ver a nota de custo no topo de gfx.c.
      gfx_tex_aspect_atual = tex_aspecto(fundoNaTela);
      gfx_rect(arte, t, GFX_CARD, 0, 0, 0, 0, 1, 1, 1, fundoAlfa * 0.92f);
      gfx_tex_aspect_atual = 0;
      // A COR DO FUNDO DA PAGINA, e alfa 1 — nao preto a 0.96. Com preto a arte
      // acabava valendo 4% no ultimo pixel da faixa e o fundo abaixo era
      // #0D0D0D: uma linha reta de 1920 px atravessando a tela na altura em que
      // a arte termina, que a captura mostrou de cara. Convergindo para a cor
      // que ja esta embaixo, nao ha onde a emenda aparecer.
      gfx_rect((GfxRect){ 0, PS_ARTE_H * 0.30f, NV_TELA_W, PS_ARTE_H * 0.70f },
               0, GFX_VEU_BAIXO, 0, 0, 0, 0,
               NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, fundoAlfa);
      // O titulo fica sobre a arte; sem este veu ele depende do que a foto tem
      // no topo, e a 3 m um titulo sobre ceu claro simplesmente some.
      gfx_rect((GfxRect){ 0, 0, NV_TELA_W, 320.0f },
               0, GFX_VEU_TOPO, 0, 0, 0, 0, 0, 0, 0, 0.80f * fundoAlfa);
    }
  }
}

static void desenhaPin(void) {
  static const char *ROT[PS_TECLA_COLS * PS_TECLA_LINS] =
    { "1","2","3", "4","5","6", "7","8","9", "←","0","OK" };
  const ContaPerfil *p = perfis_item(pinDe);
  float largura = PS_TECLA_COLS * PS_TECLA + (PS_TECLA_COLS - 1) * PS_TECLA_GAP;
  float x0 = (NV_TELA_W - largura) * 0.5f;
  float y0 = 540.0f;
  float a = animPin;
  int i;
  size_t n = strlen(pin), mostrar;

  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0.02f, 0.02f, 0.025f, 0.88f * a); }
  if (!p) return;

  // Quem esta sendo destravado, com a cara dele. Sem o avatar aqui o teclado
  // pode ser o de qualquer perfil, e num teclado numerico nao ha nada na tela
  // que diga de quem e a fechadura.
  { GfxRect av = { (NV_TELA_W - 132.0f) * 0.5f, 176.0f, 132.0f, 132.0f };
    disco(av, p, 1.0f, a); }

  { char t[128];
    TxtLinha l;
    snprintf(t, sizeof t, i18n("PIN de %s"), p->nome[0] ? p->nome : i18n("perfil"));
    l = txt_linha(TXT_TITULO3, t, 255, 255, 255, 255);
    txt_desenhar_alpha(l, (NV_TELA_W - l.w) * 0.5f, 344.0f, a); }

  // Pontos, nunca os digitos: alguem passando na sala nao precisa ler o PIN.
  // Discos e nao asteriscos — o '*' da fonte fica na ALTURA DAS MAIUSCULAS, ou
  // seja flutuando no alto da linha, e a fila lia como sujeira em vez de senha.
  mostrar = n > 4 ? n : 4;
  if (mostrar > PS_PIN_MAX) mostrar = PS_PIN_MAX;
  { float total = (float)mostrar * PS_PONTO_PASSO - (PS_PONTO_PASSO - PS_PONTO);
    float px = (NV_TELA_W - total) * 0.5f;
    size_t k;
    for (k = 0; k < mostrar; k++) {
      GfxRect d = { px + (float)k * PS_PONTO_PASSO, 434.0f, PS_PONTO, PS_PONTO };
      gfx_rect(d, 0, GFX_DISCO, 0, 0, 0, 0, 1, 1, 1, (k < n ? 0.96f : 0.20f) * a);
    } }

  { const char *aviso = NULL;
    int cr = 236, cg = 108, cb = 108;
    if (verificando)   { aviso = "verificando…"; cr = 200; cg = 202; cb = 210; }
    else if (pinRede)  { aviso = "Sem conexão. Tente novamente."; cg = 150; cb = 150; }
    else if (pinErrado){ aviso = "PIN incorreto"; }
    if (aviso) {
      TxtLinha l = txt_linha(TXT_BODY, aviso, cr, cg, cb, 255);
      txt_desenhar_alpha(l, (NV_TELA_W - l.w) * 0.5f, 486.0f, a);
    } }

  for (i = 0; i < PS_TECLA_COLS * PS_TECLA_LINS; i++) {
    int col = i % PS_TECLA_COLS, lin = i / PS_TECLA_COLS;
    GfxRect r = { x0 + col * (PS_TECLA + PS_TECLA_GAP),
                  y0 + lin * (PS_TECLA + PS_TECLA_GAP), PS_TECLA, PS_TECLA };
    int f = (i == pinFoco && !verificando);
    TxtLinha l;
    // FOCO EM SUPERFICIE: fundo ESCURO (--focus-bg #303030) com texto branco e
    // o anel de 4px por fora. Esta tela fazia o contrario — pilula branca com
    // texto preto —, que e exatamente o padrao que a nota de NV_COR_FOCO em
    // layout.h descreve como o errado e manda nao repetir.
    if (f) {
      gfx_cor(r, NV_RAIO_PILL, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B, a);
      gfx_rect((GfxRect){ r.x - NV_ANEL_FOCO, r.y - NV_ANEL_FOCO,
                          r.w + NV_ANEL_FOCO * 2, r.h + NV_ANEL_FOCO * 2 },
               0, GFX_ANEL, 0, NV_ANEL_FOCO / (r.w + NV_ANEL_FOCO * 2), 0,
               NV_RAIO_PILL, 0.96f, 0.96f, 0.98f, a);
    } else {
      gfx_cor(r, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, 0.09f * a);
    }
    l = txt_linha(TXT_TITULO3, ROT[i], f ? 255 : 214, f ? 255 : 216, f ? 255 : 224, 255);
    txt_desenhar_alpha(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a);
  }
}

void perfilsel_desenhar(Uint32 agora) {
  int i, m = perfis_n();
  float d, vao, largura, x, subida, a;
  (void)agora;

  desenhaFundo();

  // A tela inteira SOBE alguns pixels ao aparecer. E o mesmo gesto do resto do
  // app (mola NV_MOLA_TELA) e o que impede a troca de login/home para esta tela
  // de ler como corte de video.
  subida = (1.0f - animEntrada) * 28.0f;
  a = animEntrada;

  { TxtLinha t = txt_linha(TXT_TITULO1, "Quem está assistindo?", 255, 255, 255, 255);
    txt_desenhar_alpha(t, (NV_TELA_W - t.w) * 0.5f, PS_TITULO_Y + subida, a); }

  // Enquanto a lista nao chega, dizer isso. Uma tela com titulo e nada abaixo
  // le como travamento.
  if (m == 0) {
    const char *msg = sync_estado() == SYNC_FALHOU
      ? "Não foi possível carregar os perfis. OK: tentar novamente"
      : "Carregando os perfis da sua conta…";
    TxtLinha e = txt_linha(TXT_BODY, msg, 176, 179, 190, 255);
    txt_desenhar_alpha(e, (NV_TELA_W - e.w) * 0.5f, 470.0f + subida, a);
    return;
  }

  { TxtLinha s = txt_linha(TXT_CALLOUT,
                           "Cada perfil tem sua própria lista e seu progresso.",
                           166, 169, 180, 255);
    txt_desenhar_alpha(s, (NV_TELA_W - s.w) * 0.5f, PS_SUB_Y + subida, a); }

  d = diametro(m);
  vao = vaoDe(m, d);
  largura = (float)m * d + (float)(m - 1) * vao;
  x = (NV_TELA_W - largura) * 0.5f;

  for (i = 0; i < m; i++) {
    const ContaPerfil *p = perfis_item(i);
    float f = animFoco[i];
    float px = x + (float)i * (d + vao);
    float cresce = d * NV_FOCO_ESCALA_P * f;
    float y = PS_FILA_Y + subida;
    GfxRect av = { px - cresce * 0.5f, y - cresce * 0.5f - NV_FOCO_LIFT * f,
                   d + cresce, d + cresce };
    TxtLinha nome;
    int c;
    if (!p) continue;

    halo(av, p, f);
    // ANEL CONCENTRICO, e nao um contorno pintado por cima: um disco branco
    // atras, ligeiramente maior. E o padrao que a home ja usa no avatar social,
    // e e o unico que mantem a espessura uniforme em qualquer tamanho.
    if (f > 0.02f) {
      float e = NV_ANEL_FOCO * f;
      gfx_rect((GfxRect){ av.x - e, av.y - e, av.w + e * 2, av.h + e * 2 },
               0, GFX_DISCO, 0, 0, 0, 0, 0.97f, 0.97f, 0.99f, f * a);
    }
    disco(av, p, f, a);

    // 176 e nao 140 no estado sem foco: a nota de contraste vale a 3 m, e
    // cinza-escuro sobre quase-preto e ilegivel do sofa.
    c = 176 + (int)(79.0f * f);
    nome = txt_linha_corta(d >= 230.0f ? TXT_TITULO3 : TXT_HEADLINE, p->nome,
                           c, c, c + 6 > 255 ? 255 : c + 6, 255, d + vao * 0.9f);
    txt_desenhar_alpha(nome, px + (d - nome.w) * 0.5f,
                       y + d + PS_NOME_GAP - NV_FOCO_LIFT * f * 0.5f, a);

    if (p->temPin) {
      // A PALAVRA numa pilula, nao um cadeado. O emoji U+1F512 nao existe na
      // fonte embarcada e sai como retangulo vazio — a mesma armadilha que
      // gfx.h ja registra sobre o U+25B6 ("depender do glifo da fonte e
      // loteria"). Texto que a fonte tem sempre desenha.
      TxtLinha sel = txt_linha(TXT_CAPTION, "PIN", 226, 228, 236, 255);
      float sy = y + d + PS_NOME_GAP + (float)nome.h + PS_SELO_GAP
                 - NV_FOCO_LIFT * f * 0.5f;
      GfxRect pilula = { px + (d - ((float)sel.w + 32.0f)) * 0.5f, sy,
                         (float)sel.w + 32.0f, (float)sel.h + 10.0f };
      gfx_cor(pilula, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, (0.10f + 0.10f * f) * a);
      txt_desenhar_alpha(sel, pilula.x + 16.0f, pilula.y + 5.0f, a);
    }
  }

  // A dica DIZ O QUE O VOLTAR FAZ. A tela agora aparece a cada arranque, e sem
  // esta linha o Voltar e uma tecla que ou fecha o app ou nao faz nada — as
  // duas leituras erradas. Quando ha um perfil de ontem, ele e nomeado: um
  // clique no controle e a pessoa esta na home dela.
  { char dica[160];
    const ContaPerfil *at = perfis_item_ativo();
    TxtLinha l;
    if (perfis_pode_dispensar() && at && at->nome[0])
      snprintf(dica, sizeof dica,
               i18n("Setas: mover  ·  OK: entrar  ·  Voltar: seguir como %s"), at->nome);
    else
      snprintf(dica, sizeof dica, "%s", i18n("Setas: mover  ·  OK: entrar"));
    l = txt_linha(TXT_CAPTION, dica, 172, 175, 186, 255);
    txt_desenhar_alpha(l, (NV_TELA_W - l.w) * 0.5f, PS_DICA_Y, a); }

  if (animPin > 0.004f) desenhaPin();
}

int perfilsel_concluido(void) { return concluido; }
