// Ver reacao.h.
#include "reacao.h"
#include "diario.h"
#include "atividade.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "perfis.h"
#include "player.h"
#include "posplay.h"
#include "text.h"
#include "trakt.h"
#include "plrui.h"
#include "recomenda.h"
#include "recresp.h"
#include "teclado.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- medidas do cartao ---------------------------------------------------------
// A margem direita e a do player (PLR_MARGEM, 96): o cartao alinha com o fim
// da barra de tempo, que e onde o olho ja vai no fim do filme.
#define RX_MARGEM     96.0f
#define RX_MAX        400    // reacoes guardadas por perfil

static const char *const ROTULO[3] = { "Gostei", "Mais ou menos", "Não gostei" };
static const int VALOR[3] = { REACAO_GOSTEI, REACAO_MAIS_MENOS, REACAO_NAO };
// O SEGUNDO PASSO, so quando o titulo veio de uma recomendacao (dono, 03/10:
// "responder para a pessoa se gostou ou nao e mandar uma msg ou nao"). O
// MESMO cartao, as mesmas pilulas: nada de um segundo cartao empilhado.
static const char *const ROTULO_MSG[3] = { "Valeu pela dica!", "Escrever mensagem", "Agora não" };
// O que viaja com "Valeu pela dica!": no alfabeto que o servidor guarda
// (a-z0-9 e espaco, recresp.h), e a frase traduzida e so a pilula.
#define MSG_RAPIDA "valeu pela dica"
#define MSG_ALFABETO "abcdefghijklmnopqrstuvwxyz0123456789 "

// --- regras puras ----------------------------------------------------------------

int reacao_regra_perguntar(int ehSerie, int temProximo, int proxOutraTemporada,
                           double pos, double dur, double cred) {
  if (dur <= 1.0) return 0;
  if (!ehSerie) return posplay_regra_filme(pos, dur, cred);
  // Meio da temporada: o "A seguir" manda, a pergunta espera o fim dela.
  if (temProximo && !proxOutraTemporada) return 0;
  return player_regra_proximo(pos, dur, cred);
}

int reacao_nota_trakt(int r) {
  return r == REACAO_GOSTEI ? 8 : r == REACAO_MAIS_MENOS ? 5 : r == REACAO_NAO ? 2 : 0;
}

// --- arquivo, por perfil ------------------------------------------------------------
//
// Uma linha por titulo: imdb TAB estado TAB epoch TAB rec TAB midia TAB nome
// TAB titulo. O titulo por ULTIMO, como em salvos.c (pode ter qualquer coisa
// menos TAB). Estado 9 = pendente.
typedef struct {
  char imdb[24];
  int  estado;
  long long quando, rec;
  char midia[8];
  char nome[64];
  char titulo[160];
} Reacao;

static Reacao lista[RX_MAX];
static int    nLista;
static int    perfilLido = -999;

static const char *arquivo(void) {
  static char nome[40];
  int p = perfis_ativo();
  if (p <= 0) snprintf(nome, sizeof nome, "reacoes.txt");
  else snprintf(nome, sizeof nome, "reacoes-p%d.txt", p);
  return nome;
}

static void semTab(char *s) { for (; *s; s++) if (*s == '\t' || *s == '\n' || *s == '\r') *s = ' '; }

static char *campo(char **p) {
  char *ini = *p, *t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else *p = ini + strlen(ini);
  return ini;
}

static void carregar(void) {
  char *b, *p;
  if (perfilLido == perfis_ativo()) return;
  perfilLido = perfis_ativo();
  nLista = 0;
  b = dados_ler(arquivo());
  if (!b) return;
  for (p = b; *p && nLista < RX_MAX;) {
    char *fim = strchr(p, '\n'), *q;
    Reacao r;
    if (fim) *fim = 0;
    memset(&r, 0, sizeof r);
    q = p;
    if (*q && *q != '#') {
      snprintf(r.imdb, sizeof r.imdb, "%s", campo(&q));
      r.estado = atoi(campo(&q));
      r.quando = atoll(campo(&q));
      r.rec = atoll(campo(&q));
      snprintf(r.midia, sizeof r.midia, "%s", campo(&q));
      snprintf(r.nome, sizeof r.nome, "%s", campo(&q));
      snprintf(r.titulo, sizeof r.titulo, "%s", q);
      if (!strncmp(r.imdb, "tt", 2) &&
          (r.estado == REACAO_PENDENTE || (r.estado >= -1 && r.estado <= 1)))
        lista[nLista++] = r;
    }
    if (!fim) break;
    p = fim + 1;
  }
  free(b);
}

static void gravar(void) {
  static char buf[RX_MAX * 300 + 64];
  size_t k = 0;
  int i;
  k += (size_t)snprintf(buf, sizeof buf, "# nuvio reacoes v1\n");
  for (i = 0; i < nLista && k < sizeof buf; i++)
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%s\t%d\t%lld\t%lld\t%s\t%s\t%s\n",
                          lista[i].imdb, lista[i].estado, lista[i].quando, lista[i].rec,
                          lista[i].midia, lista[i].nome, lista[i].titulo);
  dados_gravar(arquivo(), buf);
}

static Reacao *achar(const char *imdb) {
  char id[24];
  int i;
  carregar();
  atividade_id_puro(id, sizeof id, imdb);
  for (i = 0; i < nLista; i++) if (!strcmp(lista[i].imdb, id)) return &lista[i];
  return NULL;
}

// Cria ou devolve a linha. Cheia: sai a mais velha (pendentes primeiro).
static Reacao *linhaDe(const char *imdb) {
  Reacao *r = achar(imdb);
  if (r) return r;
  if (nLista >= RX_MAX) {
    int i, sai = 0;
    for (i = 0; i < nLista; i++)
      if (lista[i].estado == REACAO_PENDENTE) { sai = i; break; }
    memmove(lista + sai, lista + sai + 1, sizeof lista[0] * (size_t)(nLista - sai - 1));
    nLista--;
  }
  r = &lista[nLista++];
  memset(r, 0, sizeof *r);
  atividade_id_puro(r->imdb, sizeof r->imdb, imdb);
  r->estado = REACAO_NENHUMA;
  return r;
}

int reacao_estado(const char *imdb) {
  Reacao *r = achar(imdb);
  if (!r) return REACAO_NENHUMA;
  if (r->estado == REACAO_PENDENTE &&
      (long long)time(NULL) - r->quando > (long long)REACAO_PENDENTE_DIAS * 86400LL)
    return REACAO_NENHUMA;
  return r->estado;
}

static void marcarPendente(const char *imdb, const char *midia, const char *titulo,
                           long long rec, const char *nome) {
  Reacao *r = achar(imdb);
  if (r && r->estado != REACAO_PENDENTE) return;   // ja respondida: nao rebaixa
  r = linhaDe(imdb);
  r->estado = REACAO_PENDENTE;
  r->quando = (long long)time(NULL);
  r->rec = rec;
  snprintf(r->midia, sizeof r->midia, "%s", midia);
  snprintf(r->nome, sizeof r->nome, "%s", nome ? nome : "");
  snprintf(r->titulo, sizeof r->titulo, "%s", titulo ? titulo : "");
  semTab(r->nome); semTab(r->titulo);
  gravar();
}

// --- o cartao -------------------------------------------------------------------------
static struct {
  int    aberto;
  int    modoDetalhe;            // aberto pela pagina: sem contagem
  char   imdb[24], midia[8], titulo[160], poster[512], nome[64];
  long long rec;
  int    envia;                  // "vai ver sua resposta" so com envio de verdade
  int    foco;
  // 0 = gostei/nao gostei; 1 = "mandar uma mensagem?" (so com rec de origem).
  int    passo;
  int    escrevendo;             // o teclado de tela esta aberto por este cartao
  // ABERTO PELA ABA AMIGOS ("Ja assisti", salvospainel.c): modal, sem
  // contagem, centrado em `cx` (o painel) em vez do canto do player.
  int    modoPainel;
  float  cx;
  float  anim, focoA[3];
  Uint32 desde;                  // ultima tecla (ou abertura), para os 8 s
} c;
static char  oferecido[24];      // titulo ja perguntado nesta sessao do player
// O RELOGIO DO PLAYER (o `agora` de reacao_player_atualizar). A contagem dos
// 8 s e a tecla que a reinicia usam o mesmo relogio que a confere.
static Uint32 ultimoAgora;
static double durVista, durEstavel;

int reacao_aberta(void) { return c.aberto; }

static void abrir(const char *imdb, const char *midia, const char *titulo,
                  const char *poster, int detalhe, Uint32 agora) {
  memset(&c, 0, sizeof c);
  c.aberto = 1;
  c.modoDetalhe = detalhe;
  atividade_id_puro(c.imdb, sizeof c.imdb, imdb);
  snprintf(c.midia, sizeof c.midia, "%s", midia && !strcmp(midia, "series") ? "series" : "movie");
  snprintf(c.titulo, sizeof c.titulo, "%s", titulo ? titulo : "");
  snprintf(c.poster, sizeof c.poster, "%s", poster ? poster : "");
  c.rec = atividade_origem(c.imdb, c.nome, sizeof c.nome);
  c.envia = atividade_envia();
  c.desde = agora;
  c.cx = -1.0f;
}

// A RESPOSTA A QUEM MANDOU e direta (POST /v1/rec/resposta, recresp.h): ela
// sai com o servico compilado, mesmo com a atividade automatica desligada. E
// isso que torna verdadeira a frase "Ana vai ver sua resposta".
static int respondeAmigo(void) { return c.rec > 0 && recomenda_ativo(); }

// Fecha o teclado de tela aberto por este cartao (Voltar sintetico: e o unico
// fechamento que teclado.h oferece, e devolve TECLADO_CANCELOU).
static void fecharTeclado(void) {
  if (c.escrevendo && teclado_aberto()) {
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_ESCAPE;
    teclado_evento(&ev);
    (void)teclado_resultado();
  }
  c.escrevendo = 0;
}

void reacao_fechar(void) {
  fecharTeclado();
  c.aberto = 0;
  oferecido[0] = 0;
  durVista = durEstavel = 0.0;
}

void reacao_responder(const char *imdb, int v) {
  Reacao *r;
  int nota;
  if (v < -1 || v > 1) return;
  r = linhaDe(imdb);
  r->estado = v;
  r->quando = (long long)time(NULL);
  // Dados do cartao aberto, quando e dele a resposta; senao o que ja havia.
  if (c.imdb[0] && !strcmp(c.imdb, r->imdb)) {
    r->rec = c.rec;
    snprintf(r->midia, sizeof r->midia, "%s", c.midia);
    snprintf(r->nome, sizeof r->nome, "%s", c.nome);
    snprintf(r->titulo, sizeof r->titulo, "%s", c.titulo);
    semTab(r->nome); semTab(r->titulo);
  }
  gravar();
  // A RESPOSTA A QUEM MANDOU: a rec vai para "Assistidas" com a reacao.
  if (r->rec > 0) recresp_responder(r->rec, v, NULL);
  atividade_reacao(r->imdb, r->midia, r->titulo,
                   c.imdb[0] && !strcmp(c.imdb, r->imdb) ? c.poster : "", v, r->rec);
  nota = reacao_nota_trakt(v);
  // O DIARIO guarda a nota sempre (8/5/2, a escala de 1 a 10 das estrelas):
  // e a mesma resposta, so que local e por perfil, com ou sem Trakt.
  diario_avaliar(0, r->imdb, !strcmp(r->midia, "series") ? DIARIO_SERIE : DIARIO_FILME,
                 diario_nota_da_reacao(v));
  // SO COM O TRAKT LIGADO. Sem ele a resposta fica nesta TV (e no servico,
  // se a pessoa permitiu) e o Trakt nao fica sabendo — e e o certo.
  if (nota && trakt_ativo()) trakt_avaliar(r->imdb, r->midia, nota);
  printf("[reacao] %s -> %d%s\n", r->imdb, v, nota && trakt_ativo() ? " (nota no Trakt)" : "");
  fflush(stdout);
}

void reacao_player_atualizar(float dt, Uint32 agora, const CatItem *ci, int ehSerie,
                             double pos, double dur, double cred, int temProximo,
                             int proxOutraTemporada) {
  char id[24];
  float alvo = c.aberto ? 1.0f : 0.0f;
  int i;
  ultimoAgora = agora;
  c.anim = anim_mola(c.anim, alvo, dt, NV_MOLA_TELA);
  for (i = 0; i < 3; i++)
    c.focoA[i] = anim_mola(c.focoA[i], (c.aberto && c.foco == i) ? 1.0f : 0.0f, dt,
                           c.foco == i ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  // A CONTAGEM: 8 s desde a abertura ou da ultima tecla. So no player.
  if (c.escrevendo) { teclado_atualizar(dt, agora); c.desde = agora; }
  if (c.aberto && !c.modoDetalhe && agora - c.desde >= REACAO_TIMEOUT_MS) c.aberto = 0;
  // DURACAO ESTAVEL, a mesma guarda do posplay: a duracao provisoria do
  // primeiro instante nao pode fazer a "metade do filme" chegar aos 15 s.
  if (dur - durVista > 2.0 || durVista - dur > 2.0) { durVista = dur; durEstavel = 0.0; }
  else durEstavel += dt;
  if (!ci || !ci->imdb[0] || c.aberto || !ajustes_reacao_creditos()) return;
  atividade_id_puro(id, sizeof id, ci->imdb);
  if (strncmp(id, "tt", 2) || !strcmp(oferecido, id)) return;
  if (durEstavel < 8.0) return;
  // VEIO DE UM AMIGO E AINDA NAO FOI RESPONDIDO (dono, 03/10: "se eu ver uma
  // serie ou um episodio que a pessoa mandou, no final ... aparecer a msg se
  // gostei ou nao"): pergunta mesmo com o titulo ja avaliado e, na serie, no
  // fim de QUALQUER episodio — nao so no fim da temporada.
  { long long rec = recomenda_ativo() ? atividade_origem(id, NULL, 0) : 0;
    int peloAmigo = rec > 0 && !recresp_respondida(rec);
    int st = reacao_estado(id);
    if (st >= -1 && st <= 1 && !peloAmigo) return;
    if (peloAmigo) temProximo = proxOutraTemporada = 0; }
  if (!reacao_regra_perguntar(ehSerie, temProximo, proxOutraTemporada, pos, dur, cred)) return;
  snprintf(oferecido, sizeof oferecido, "%s", id);
  abrir(id, ehSerie ? "series" : "movie", ci->titulo, ci->poster, 0, agora);
  // PENDENTE DESDE JA: se a pessoa sair do player com o cartao no ar, a
  // pergunta continua de pe na pagina do titulo.
  marcarPendente(id, c.midia, c.titulo, c.rec, c.nome);
  printf("[reacao] pergunta de %s em %.0fs de %.0fs%s\n", id, pos, dur,
         c.rec ? " (veio de recomendacao)" : "");
  fflush(stdout);
}

// O teclado do passo 2 respondeu. Chamado por quem anima o cartao.
static void tecladoResultado(void) {
  int r;
  if (!c.escrevendo) return;
  r = teclado_resultado();
  if (r == TECLADO_PRONTO) {
    const char *t = teclado_texto();
    c.escrevendo = 0;
    if (t && t[0]) recresp_responder(c.rec, RECRESP_SEM_REACAO, t);
    else recresp_pular(c.rec);
    c.aberto = 0;
  } else if (r == TECLADO_CANCELOU) {
    c.escrevendo = 0;           // volta as tres pilulas, com o foco em "Escrever"
    c.desde = ultimoAgora;
  }
}

static void escolherMensagem(void) {
  if (c.foco == 0) { recresp_responder(c.rec, RECRESP_SEM_REACAO, MSG_RAPIDA); c.aberto = 0; }
  else if (c.foco == 1) {
    char tit[160];
    snprintf(tit, sizeof tit, i18n("Mensagem para %s"), c.nome);
    teclado_abrir_com(tit, "Curta, sem acentos. Vazio não manda nada.",
                      RECRESP_TEXTO_MAX, MSG_ALFABETO, NULL);
    c.escrevendo = 1;
  } else { recresp_pular(c.rec); c.aberto = 0; }
}

int reacao_evento(const SDL_Event *e, int controlesVisiveis) {
  SDL_Keycode k;
  // O TECLADO DO PASSO 2 E DONO DE TODA TECLA, com ou sem a barra do player.
  if (c.aberto && c.escrevendo) {
    if (teclado_aberto()) teclado_evento(e);
    tecladoResultado();
    return 1;
  }
  if (!c.aberto || e->type != SDL_KEYDOWN) return 0;
  if (controlesVisiveis && !c.modoDetalhe) return 0;
  k = e->key.keysym.sym;
  if (e->key.repeat && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) return 1;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    c.aberto = 0;               // pula; a pendencia fica
    return 1;
  }
  if (k == SDLK_LEFT)  { if (c.foco > 0) c.foco--; c.desde = ultimoAgora; return 1; }
  if (k == SDLK_RIGHT) { if (c.foco < 2) c.foco++; c.desde = ultimoAgora; return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (c.passo == 1) { escolherMensagem(); c.desde = ultimoAgora; return 1; }
    reacao_responder(c.imdb, VALOR[c.foco]);
    // COM AMIGO NA ORIGEM o cartao continua: "mandar uma mensagem?". A
    // reacao ja foi (recresp_responder dentro de reacao_responder).
    if (respondeAmigo() && c.nome[0]) {
      c.passo = 1; c.foco = 0; c.desde = ultimoAgora;
      return 1;
    }
    c.aberto = 0;
    return 1;
  }
  // Na pagina o cartao e modal: o resto das teclas nao vaza para baixo dele.
  return c.modoDetalhe;
}

// Linhas de texto do cartao, montadas uma vez por quadro.
static void linhaOrigem(char *dst, size_t n) {
  dst[0] = 0;
  if (!c.rec || !c.nome[0]) return;
  snprintf(dst, n, i18n(!strcmp(c.midia, "series") ? "%s mandou esta série"
                                                     : "%s mandou este filme"), c.nome);
}

// GLASS UI (mockup de 03/10, "reacao"): a ilha de 760 a 96 da borda direita,
// no material da ilha (o vidro antigo era um gfx_vidro_superficie a 5%, e o
// harness mostrava o fundo em faixas): rosto + "Ana mandou este filme", a
// pergunta 36/700, as tres respostas em pilulas (a focada cheia no acento) e
// a contagem de 8 s como trilho DENTRO da ilha, ao lado de "Ana vai ver sua
// resposta" — nao mais um fio colado na borda.
static void reacao_desenharCorpo_(Uint32 agora, float baseY);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void reacao_desenhar(Uint32 agora, float baseY) {
  { ESCALA_INI();
    reacao_desenharCorpo_(agora, baseY);
    ESCALA_FIM(); }
  // O teclado do passo 2 por cima do cartao (ele cuida da propria escala).
  if (c.aberto && c.escrevendo && teclado_aberto()) teclado_desenhar(agora);
}
static void reacao_desenharCorpo_(Uint32 agora, float baseY) {
  float a = c.anim, w = 760.0f, h, x, y, lead, hp, pw = 0.0f;
  char perg[256], orig[160], ver[160];
  static const char *const ICONE[3] = { "pl_thumbs-up", NULL, "pl_thumbs-down" };
  int i;
  if (a < 0.01f || !c.imdb[0]) return;
  const char *const *rot = c.passo == 1 ? ROTULO_MSG : ROTULO;
  static const char *const ICONE_MSG[3] = { NULL, NULL, NULL };
  const char *const *ico = c.passo == 1 ? ICONE_MSG : ICONE;
  if (c.passo == 1) snprintf(perg, sizeof perg, i18n("Mandar uma mensagem para %s?"), c.nome);
  else snprintf(perg, sizeof perg, i18n("O que achou de %s?"), c.titulo);
  linhaOrigem(orig, sizeof orig);
  ver[0] = 0;
  if (orig[0] && (c.envia || respondeAmigo()))
    snprintf(ver, sizeof ver, i18n("%s vai ver sua resposta"), c.nome);
  for (i = 0; i < 3; i++) pw += plrui_botao_largura(rot[i], ico[i]) + (i ? 12.0f : 0.0f);
  if (pw + 72.0f > w) w = pw + 72.0f;
  lead = (float)txt_linha(TXT_ILHA_PERGUNTA, "Ág", 245, 246, 248, 255).h + 6.0f;
  hp = txt_bloco_corta(TXT_ILHA_PERGUNTA, perg, 0, 0, 0, -4000.0f, -4000.0f, w - 72.0f, lead, 0.0f, 2);
  h = 34.0f + (orig[0] ? 30.0f + 8.0f : 0.0f) + hp + 26.0f + 60.0f + 24.0f + 22.0f + 30.0f;
  x = NV_TELA_W - RX_MARGEM - w;
  y = baseY - h + (1.0f - a) * 24.0f;
  if (c.modoPainel) {
    // Sobre o painel: centrado nele e na altura da tela, como o menu do
    // cartaz aberto pelo painel (ctx_centro_dica).
    x = (c.cx >= 0.0f ? c.cx : NV_TELA_W * 0.5f) - w * 0.5f;
    if (x + w > NV_TELA_W - 24.0f) x = NV_TELA_W - 24.0f - w;
    if (x < 24.0f) x = 24.0f;
    y = (NV_TELA_H - h) * 0.5f + (1.0f - a) * 24.0f;
  }
  plrui_material((GfxRect){ x, y, w, h }, 36.0f, 0, a);
  { float ty = y + 34.0f, tx = x + 36.0f;
    if (orig[0]) {
      TxtLinha lo = txt_linha_corta(TXT_G18M, orig, 243, 242, 239, 158, w - 72.0f - 42.0f);
      rec_avatar_estilo((GfxRect){ tx, ty, 30.0f, 30.0f }, "", c.nome, c.nome, a, TXT_MINI);
      txt_desenhar_alpha(lo, tx + 30.0f + 12.0f, ty + (30.0f - (float)lo.h) * 0.5f, a);
      ty += 30.0f + 8.0f;
    }
    txt_bloco_corta(TXT_ILHA_PERGUNTA, perg, 243, 242, 239, tx, ty, w - 72.0f, lead, a, 2);
    ty += hp + 26.0f;
    { float bx = tx;
      for (i = 0; i < 3; i++) bx += plrui_botao(bx, ty, rot[i], ico[i], c.focoA[i], a) + 12.0f; }
    ty += 60.0f + 24.0f;
    { float yc = ty + 11.0f, tw = 0.0f;
      if (ver[0]) {
        TxtLinha lv = txt_linha_corta(TXT_ILHA_GENERO, ver, 243, 242, 239, 128, w * 0.5f);
        txt_desenhar_alpha(lv, tx, yc - (float)lv.h * 0.5f, a);
        tw = (float)lv.w + 18.0f;
      }
      // A CONTAGEM VISIVEL: sem ela o cartao sumiria "do nada".
      if (!c.modoDetalhe && c.aberto && !c.escrevendo) {
        float resta = 1.0f - (float)(agora - c.desde) / (float)REACAO_TIMEOUT_MS;
        if (resta < 0.0f) resta = 0.0f;
        plrui_trilho((GfxRect){ tx + tw, yc - 2.0f, w - 72.0f - tw, 4.0f }, resta, 0.953f, 0.949f, 0.937f, a * 0.55f);
      } } }
}

// --- pagina do titulo --------------------------------------------------------------

int reacao_detalhe_pendente(const CatItem *ci) {
  if (!ci || !ci->imdb[0] || !ajustes_reacao_creditos()) return 0;
  return reacao_estado(ci->imdb) == REACAO_PENDENTE;
}

int reacao_detalhe_abrir(const CatItem *ci) {
  if (!reacao_detalhe_pendente(ci)) return 0;
  abrir(ci->imdb, ci->tipo, ci->titulo, ci->poster, 1, SDL_GetTicks());
  { Reacao *r = achar(ci->imdb);
    // A ORIGEM GUARDADA na pendencia vale mais que a busca de agora: a
    // recomendacao pode ter saido da lista do servidor depois da pergunta.
    if (r && r->rec && !c.rec) { c.rec = r->rec; snprintf(c.nome, sizeof c.nome, "%s", r->nome); } }
  c.anim = 0.0f;
  return 1;
}

void reacao_detalhe_dica(const CatItem *ci, float a) {
  char perg[256];
  TxtLinha l, s;
  float x, y, w;
  // O cartao aberto (pela pagina) desenha por cima de tudo, com a mola dele.
  if (c.aberto && c.modoDetalhe && !c.modoPainel) {
    if (c.escrevendo) { teclado_atualizar(1.0f / 60.0f, SDL_GetTicks()); tecladoResultado(); }
    c.anim = anim_mola(c.anim, 1.0f, 1.0f / 60.0f, NV_MOLA_TELA);
    { int i;
      for (i = 0; i < 3; i++)
        c.focoA[i] = anim_mola(c.focoA[i], c.foco == i ? 1.0f : 0.0f, 1.0f / 60.0f,
                               c.foco == i ? NV_MOLA_FOCO : NV_MOLA_DESFOCO); }
    reacao_desenhar(SDL_GetTicks(), NV_TELA_H - 48.0f);
    return;
  }
  if (c.modoDetalhe && !c.aberto && !c.modoPainel) c.anim = 0.0f;
  if (a < 0.01f || !reacao_detalhe_pendente(ci)) return;
  snprintf(perg, sizeof perg, i18n("O que achou de %s?"), ci->titulo);
  // DISCRETA: texto pequeno e cinza, sem caixa de foco — ela nao e um botao,
  // e um lembrete de que CIMA abre a pergunta.
  s = txt_linha(TXT_CAPTION, "↑", 200, 204, 212, 255);
  l = txt_linha_corta(TXT_CAPTION, perg, 176, 180, 190, 255, 620.0f);
  w = (float)s.w + 12.0f + (float)l.w;
  x = NV_TELA_W - RX_MARGEM - w;
  y = NV_TELA_H - 48.0f - (float)l.h;
  gfx_cor((GfxRect){ x - 18.0f, y - 10.0f, w + 36.0f, (float)l.h + 20.0f }, 0.5f,
          .04f, .042f, .05f, .62f * a);
  txt_desenhar_alpha(s, x, y, a * 0.9f);
  txt_desenhar_alpha(l, x + (float)s.w + 12.0f, y, a * 0.85f);
}

void reacao_teste_abrir(const char *imdb, const char *titulo, const char *midia,
                        long long rec, const char *nomeRec, int envia) {
  abrir(imdb, midia, titulo, "", 0, ultimoAgora);
  c.rec = rec;
  snprintf(c.nome, sizeof c.nome, "%s", nomeRec ? nomeRec : "");
  c.envia = envia;
}

// O cartao esta (ou ainda esta saindo) na tela do player.
int reacao_visivel(void) { return c.anim > 0.01f && c.imdb[0] && !c.modoDetalhe; }

// --- aba Amigos ("Ja assisti") ------------------------------------------------------

int reacao_rec_abrir(long long rec, const char *imdb, const char *titulo, const char *midia,
                     const char *poster, const char *nome, float cx) {
  char id[24];
  if (rec <= 0 || !imdb) return 0;
  atividade_id_puro(id, sizeof id, imdb);
  if (strncmp(id, "tt", 2)) return 0;
  fecharTeclado();
  abrir(id, midia, titulo, poster, 1, SDL_GetTicks());
  // A ORIGEM E A LINHA EM QUE A PESSOA DEU OK, e nao a busca por titulo.
  c.rec = rec;
  snprintf(c.nome, sizeof c.nome, "%s", nome ? nome : "");
  c.modoPainel = 1;
  c.cx = cx;
  c.anim = 0.0f;
  ultimoAgora = SDL_GetTicks();
  return 1;
}

int reacao_painel_aberta(void) { return c.aberto && c.modoPainel; }

void reacao_painel_atualizar(float dt, Uint32 agora) {
  int i;
  ultimoAgora = agora;
  if (!c.modoPainel) return;
  if (c.escrevendo) { teclado_atualizar(dt, agora); tecladoResultado(); }
  c.anim = anim_mola(c.anim, c.aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  for (i = 0; i < 3; i++)
    c.focoA[i] = anim_mola(c.focoA[i], (c.aberto && c.foco == i) ? 1.0f : 0.0f, dt,
                           c.foco == i ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  if (!c.aberto && c.anim < 0.01f) { c.modoPainel = 0; c.imdb[0] = 0; }
}

void reacao_painel_desenhar(Uint32 agora) {
  if (!c.modoPainel || c.anim < 0.01f) return;
  { GfxRect tela = { 0, 0, 1920.0f, 1080.0f };
    gfx_cor(tela, 0.0f, 0, 0, 0, 0.55f * c.anim); }
  reacao_desenhar(agora, NV_TELA_H);
}

int reacao_passo(void) { return c.aberto ? c.passo : -1; }
