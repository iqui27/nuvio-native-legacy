#include "arteurl.h"
// Ver reacao.h.
#include "reacao.h"
#include "atividade.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "ilhaacao.h"
#include "ilha.h"
#include "layout.h"
#include "perfis.h"
#include "player.h"
#include "posplay.h"
#include "text.h"
#include "tex_cache.h"
#include "corviva.h"
#include "trakt.h"
#include "simkl.h"
#include "plrui.h"
#include "recomenda.h"
#include "recresp.h"
#include "teclado.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include "telefonecartao.h"
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

extern const char *cat_tipo_por_imdb(const char *imdb);
static char *campo(char **p);
// --- nota (meia estrela, 1 a 10 = a escala do Trakt e do Simkl) ---------------------
//
// notas[-p<N>].txt, por perfil: imdb TAB nota TAB epoch. A chave e o imdb puro
// do titulo ou "tt:S:E" do episodio. Os tres botoes dos creditos sao o atalho:
// 8, 5 e 2 (4, 2,5 e 1 estrela).
typedef struct { char id[40]; int nota; long long quando; unsigned tags; } Nota;
static Nota notas[RX_MAX];
static int  nNotas, notasPerfil = -999;

static const char *arquivoNotas(void) {
  static char nome[40];
  int p = perfis_ativo();
  if (p <= 0) snprintf(nome, sizeof nome, "notas.txt");
  else snprintf(nome, sizeof nome, "notas-p%d.txt", p);
  return nome;
}

static void carregarNotas(void) {
  char *b, *p;
  if (notasPerfil == perfis_ativo()) return;
  notasPerfil = perfis_ativo();
  nNotas = 0;
  b = dados_ler(arquivoNotas());
  if (!b) return;
  for (p = b; *p && nNotas < RX_MAX;) {
    char *fim = strchr(p, '\n'), *q = p;
    if (fim) *fim = 0;
    if (!strncmp(q, "tt", 2)) {
      Nota n;
      memset(&n, 0, sizeof n);
      snprintf(n.id, sizeof n.id, "%s", campo(&q));
      n.nota = atoi(campo(&q));
      n.quando = atoll(campo(&q));
      n.tags = (unsigned)strtoul(q, NULL, 10) & 32767u;
      if (n.nota >= 1 && n.nota <= 10) notas[nNotas++] = n;
    }
    if (!fim) break;
    p = fim + 1;
  }
  free(b);
}

static Nota *achaNota(const char *id) {
  int i;
  carregarNotas();
  for (i = 0; i < nNotas; i++) if (!strcmp(notas[i].id, id)) return &notas[i];
  return NULL;
}

int reacao_nota(const char *imdb) {
  char id[40];
  Nota *n;
  if (!imdb || !imdb[0]) return 0;
  if (!strchr(imdb, ':')) atividade_id_puro(id, sizeof id, imdb);
  else snprintf(id, sizeof id, "%s", imdb);
  n = achaNota(id);
  return n ? n->nota : 0;
}

static void salvarNota(const char *id, int nota) {
  static char buf[RX_MAX * 64 + 32];
  size_t k = 0;
  int i;
  Nota *n = achaNota(id);
  if (!n) {
    if (nNotas >= RX_MAX) { memmove(notas, notas + 1, sizeof notas[0] * (size_t)(RX_MAX - 1)); nNotas--; }
    n = &notas[nNotas++];
    memset(n, 0, sizeof *n);
    snprintf(n->id, sizeof n->id, "%s", id);
  }
  n->nota = nota;
  n->quando = (long long)time(NULL);
  for (i = 0; i < nNotas && k < sizeof buf; i++)
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%s\t%d\t%lld\t%u\n", notas[i].id, notas[i].nota, notas[i].quando, notas[i].tags);
  dados_gravar(arquivoNotas(), buf);
}

int reacao_nota_para_reacao(int nota) {
  return nota >= 7 ? REACAO_GOSTEI : nota >= 4 ? REACAO_MAIS_MENOS : nota >= 1 ? REACAO_NAO : REACAO_NENHUMA;
}

unsigned reacao_tags(const char *id) {
  Nota *n = id ? achaNota(id) : NULL;
  return n ? n->tags : 0;
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
  char   imdb[24], alvo[40], midia[8], titulo[160], poster[NV_ARTE_URL_MAX], nome[64];
  uint64_t sessao;
  unsigned tags;
  int tagFoco, desfazerFoco, interagiu;
  long long rec;
  int    envia;                  // "vai ver sua resposta" so com envio de verdade
  int    foco;
  int    estrelas;               // foco na fileira de estrelas (CIMA); BAIXO volta as pilulas
  int    nota;                   // 1 a 10 em meia estrela; 0 = ainda sem escolha
  // 0 = gostei/nao gostei; 1 = "mandar uma mensagem?" (so com rec de origem).
  int    passo;
  int    escrevendo;             // o teclado de tela esta aberto por este cartao
  // ABERTO PELA ABA AMIGOS ("Ja assisti", salvospainel.c): modal, sem
  // contagem, centrado em `cx` (o painel) em vez do canto do player.
  int    modoPainel;
  float  cx;
  float  anim, focoA[3], estrelaA, notaA;
  Uint32 desde, varredura;        // key timeout and bounded star sweep
} c;
static char  oferecido[24];      // titulo ja perguntado nesta sessao do player
// O RELOGIO DO PLAYER (o `agora` de reacao_player_atualizar). A contagem dos
// 8 s e a tecla que a reinicia usam o mesmo relogio que a confere.
static Uint32 ultimoAgora;
static double durVista, durEstavel;
static int playerT, playerE;
void reacao_player_episodio(int t, int e) { playerT = t; playerE = e; }
#ifdef NV_TOUCH_UI
static TelefoneCartao reacaoTelefone;
static int reacaoTelefoneGeracao;
#endif

int reacao_aberta(void) { return c.aberto; }

static void abrir(const char *imdb, const char *midia, const char *titulo,
                  const char *poster, int detalhe, Uint32 agora) {
  memset(&c, 0, sizeof c);
#ifdef NV_TOUCH_UI
  telefonecartao_limpar(&reacaoTelefone); reacaoTelefoneGeracao++;
#endif
  c.aberto = 1;
  c.modoDetalhe = detalhe;
  atividade_id_puro(c.imdb, sizeof c.imdb, imdb);
  snprintf(c.alvo, sizeof c.alvo, "%s", imdb);
  c.sessao = perfis_sessao_geracao();
  { Nota *n = achaNota(c.alvo); c.tags = n ? n->tags : 0; }
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
  playerT = playerE = 0;
}

static const char *const TAGS[] = {
  "Visual", "Trilha", "Atuações", "Roteiro", "Final", "Ritmo lento",
  "Chorei", "Ri muito", "Assustador", "Quero rever", "Superestimado", "Subestimado",
  "Engraçado", "Emocionante", "Previsível"
};
// Display order is independent of the persisted tag bits.
static const int TAG_ORDEM[] = {12, 13, 14, 2, 1, 4, 0, 3, 5, 6, 7, 8, 9, 10, 11};
#define TAG_N 15
// O episodio desenha os chips 3 por linha, 2 linhas por pagina, e o foco
// anda pela MESMA grade: a pagina visivel e sempre a do foco.
#define TAG_PAGINA 6
#define TAG_COLS   3
int reacao_tag_foco(void) { return c.aberto ? c.tagFoco : -1; }
static struct { char id[40], tipo[8]; int nota, estado; unsigned tags; uint64_t sessao; } anterior;

static void desfazerNota(const void *ctx) {
  (void)ctx;
  if (!perfis_sessao_atual(anterior.sessao)) return;
  salvarNota(anterior.id, anterior.nota);
  Nota *n = achaNota(anterior.id);
  if (n) n->tags = anterior.tags;
  salvarNota(anterior.id, anterior.nota);
  if (!strchr(anterior.id, ':')) {
    Reacao *r = linhaDe(anterior.id); r->estado = anterior.estado; gravar();
    if (simkl_ativo()) simkl_avaliar(anterior.id, anterior.tipo, anterior.nota);
  }
  if (trakt_ativo()) trakt_avaliar(anterior.id, anterior.tipo, anterior.nota);
}

static float notaVisivel(void) {
  if (!c.varredura || ajustes_animacoes_reduzidas_usuario()) return c.nota;
  float t = anim_clamp((float)(ultimoAgora - c.varredura) / 450.f, 0, 1);
  return anim_mistura(c.notaA, c.nota, t);
}

static float notaMola(float atual, float alvo, float dt, float rigidez) {
  if (ajustes_animacoes_reduzidas_usuario()) return alvo;
  return atual + (alvo - atual) * (1.f - expf(-rigidez * dt));
}

static void confirmarNota(int nota) {
  snprintf(anterior.id, sizeof anterior.id, "%s", c.alvo);
  snprintf(anterior.tipo, sizeof anterior.tipo, "%s", c.midia);
  anterior.nota = reacao_nota(c.alvo); anterior.tags = reacao_tags(c.alvo);
  anterior.estado = reacao_estado(c.imdb); anterior.sessao = c.sessao;
  reacao_nota_definir(c.alvo, nota);
  // Preserve the direct reply to a recommendation, even for an episode score.
  if (strchr(c.alvo, ':') && c.rec > 0) recresp_responder(c.rec, reacao_nota_para_reacao(nota), NULL);
  IlhaAcao acao = {0};
  c.nota = nota; c.notaA = nota; c.varredura = 0;
  acao.icone = "aj_star"; acao.frase = i18n("Avaliação salva"); acao.titulo = c.titulo;
  acao.tipo = ILHA_INFO; acao.desfazer = desfazerNota;
  ilhaacao_feita(&acao);
}

static void responder(const char *imdb, int v, int nota);
void reacao_responder(const char *imdb, int v) { responder(imdb, v, reacao_nota_trakt(v)); }

// Nota de 1 a 10 (meia estrela); grava, manda ao Trakt/Simkl e vale como a
// reacao do titulo. Episode scores also go to Trakt.
void reacao_nota_definir(const char *imdb, int nota) {
  char corpo[300];
  if (nota < 1 || !trakt_nota_corpo(corpo, sizeof corpo, imdb, "movie", nota)) return;
  if (strchr(imdb, ':')) {
    salvarNota(imdb, nota);
    if (trakt_ativo()) trakt_avaliar(imdb, "series", nota);
    // Simkl ratings support titles; never overwrite a show's score with an episode.
    return;
  }
  responder(imdb, reacao_nota_para_reacao(nota), nota);
}

static void responder(const char *imdb, int v, int nota) {
  Reacao *r;
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
  if (!r->midia[0]) snprintf(r->midia, sizeof r->midia, "%s", cat_tipo_por_imdb(r->imdb));
  gravar();
  // A RESPOSTA A QUEM MANDOU: a rec vai para "Assistidas" com a reacao.
  if (r->rec > 0) recresp_responder(r->rec, v, NULL);
  atividade_reacao(r->imdb, r->midia, r->titulo,
                   c.imdb[0] && !strcmp(c.imdb, r->imdb) ? c.poster : "", v, r->rec);
  salvarNota(r->imdb, nota);
  // SO COM O TRAKT LIGADO. Sem ele a resposta fica nesta TV (e no servico,
  // se a pessoa permitiu) e o Trakt nao fica sabendo — e e o certo.
  if (nota && trakt_ativo()) trakt_avaliar(r->imdb, r->midia, nota);
  if (nota && simkl_ativo()) simkl_avaliar(r->imdb, r->midia, nota);
  printf("[reacao] %s -> %d nota %d%s\n", r->imdb, v, nota, trakt_ativo() ? " (Trakt)" : "");
  fflush(stdout);
}

void reacao_player_atualizar(float dt, Uint32 agora, const CatItem *ci, int ehSerie,
                             double pos, double dur, double cred, int temProximo,
                             int proxOutraTemporada) {
  char id[24];
  float alvo = c.aberto ? 1.0f : 0.0f;
  int i;
  ultimoAgora = agora;
  c.anim = notaMola(c.anim, alvo, dt, NV_MOLA_TELA);
  for (i = 0; i < 3; i++)
    c.focoA[i] = notaMola(c.focoA[i], (c.aberto && c.foco == i) ? 1.0f : 0.0f, dt,
                           c.foco == i ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  // A CONTAGEM: 8 s desde a abertura ou da ultima tecla. So no player.
  if (c.escrevendo) { teclado_atualizar(dt, agora); c.desde = agora; }
  if (c.aberto && !perfis_sessao_atual(c.sessao)) c.aberto = 0;
  if (c.aberto && !c.modoDetalhe && agora - c.desde >= REACAO_TIMEOUT_MS) c.aberto = 0;
  // DURACAO ESTAVEL, a mesma guarda do posplay: a duracao provisoria do
  // primeiro instante nao pode fazer a "metade do filme" chegar aos 15 s.
  if (dur - durVista > 2.0 || durVista - dur > 2.0) { durVista = dur; durEstavel = 0.0; }
  else durEstavel += dt;
  if (!ci || !ci->imdb[0] || c.aberto || !ajustes_reacao_creditos()) return;
  // Cena pos-creditos: esta pergunta tambem cobriria a imagem antes do fim.
  if (ci->posCreditos && pos < dur) return;
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
    if (ehSerie && playerE > 0) {
      char alvo[40]; snprintf(alvo, sizeof alvo, "%s:%d:%d", id, playerT, playerE);
      if (reacao_nota(alvo) && !peloAmigo) return;
      temProximo = proxOutraTemporada = 0;
    } else if (st >= -1 && st <= 1 && !peloAmigo) return;
    if (peloAmigo) temProximo = proxOutraTemporada = 0; }
  if (!reacao_regra_perguntar(ehSerie, temProximo, proxOutraTemporada, pos, dur, cred)) return;
  snprintf(oferecido, sizeof oferecido, "%s", id);
  abrir(id, ehSerie ? "series" : "movie", ci->titulo, ci->poster, 0, agora);
  if (ehSerie && playerE > 0) {
    snprintf(c.alvo, sizeof c.alvo, "%s:%d:%d", id, playerT, playerE);
    Nota *n = achaNota(c.alvo); c.tags = n ? n->tags : 0;
  }
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
  if (c.aberto && !perfis_sessao_atual(c.sessao)) { c.aberto = 0; return 0; }
  if (!c.aberto || e->type != SDL_KEYDOWN) return 0;
  if (controlesVisiveis && !c.modoDetalhe) return 0;
  k = e->key.keysym.sym;
  c.interagiu = 1;
  if (e->key.repeat && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) return 1;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    c.aberto = 0;               // pula; a pendencia fica
    return 1;
  }
  if (c.passo == 2) {
    c.desde = ultimoAgora;
    if (!strchr(c.alvo, ':')) {
      if (k == SDLK_LEFT) c.desfazerFoco = 1;
      if (k == SDLK_RIGHT) c.desfazerFoco = 0;
      if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        if (c.desfazerFoco) ilhaacao_botao(1);
        c.aberto = 0;
      }
      return 1;
    }
    if (k == SDLK_LEFT && c.tagFoco > 0) c.tagFoco--;
    if (k == SDLK_RIGHT && c.tagFoco < TAG_N) c.tagFoco++;
    if (k == SDLK_UP) {
      // A MESMA GRADE do desenho (3 chips por linha, 6 por pagina): a
      // pagina visivel e sempre a do foco, o OK nunca cai num chip oculto.
      if (c.tagFoco >= TAG_N) c.tagFoco = TAG_N - 1;
      else {
        int linha = (c.tagFoco % TAG_PAGINA) / TAG_COLS;
        if (linha > 0) c.tagFoco -= TAG_COLS;
        else if (c.tagFoco >= TAG_PAGINA) c.tagFoco = (c.tagFoco / TAG_PAGINA - 1) * TAG_PAGINA + c.tagFoco % TAG_COLS;
      }
    }
    if (k == SDLK_DOWN) {
      if (c.tagFoco < TAG_N) {
        int linha = (c.tagFoco % TAG_PAGINA) / TAG_COLS;
        if (linha == 0 && c.tagFoco + TAG_COLS < (c.tagFoco / TAG_PAGINA + 1) * TAG_PAGINA && c.tagFoco + TAG_COLS < TAG_N) c.tagFoco += TAG_COLS;
        else if ((c.tagFoco / TAG_PAGINA + 1) * TAG_PAGINA < TAG_N) c.tagFoco = (c.tagFoco / TAG_PAGINA + 1) * TAG_PAGINA + c.tagFoco % TAG_COLS;
        else c.tagFoco = TAG_N;
      }
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (c.tagFoco == TAG_N) c.aberto = 0;
      else {
        unsigned bit = 1u << TAG_ORDEM[c.tagFoco], count = 0;
        for (unsigned b = c.tags; b; b >>= 1) count += b & 1u;
        if ((c.tags & bit) || count < 3) c.tags ^= bit;
        Nota *n = achaNota(c.alvo);
        if (n) { n->tags = c.tags; salvarNota(c.alvo, n->nota); }
      }
    }
    return 1;
  }
  // A FILEIRA DE ESTRELAS (so no passo 1 da pergunta): CIMA entra, BAIXO volta.
  if (c.passo == 0 && (k == SDLK_UP || (k == SDLK_DOWN && c.modoDetalhe)) && !c.estrelas) {
    c.estrelas = 1; c.desde = ultimoAgora;
    if (!c.nota) {
      c.notaA = 0; c.varredura = ultimoAgora;
      c.nota = reacao_nota(c.alvo) ? reacao_nota(c.alvo) : reacao_nota_trakt(VALOR[c.foco]);
    }
    return 1;
  }
  if (c.estrelas) {
    c.desde = ultimoAgora;
    if (k == SDLK_DOWN) { c.estrelas = 0; return 1; }
    if (k == SDLK_LEFT || k == SDLK_RIGHT) {
      c.notaA = notaVisivel();
      if (k == SDLK_LEFT && c.nota > 1) c.nota--;
      if (k == SDLK_RIGHT && c.nota < 10) c.nota++;
      c.varredura = ultimoAgora;
      return 1;
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      confirmarNota(c.nota);
      if (respondeAmigo() && c.nome[0]) { c.passo = 1; c.foco = 0; c.estrelas = 0; return 1; }
      c.passo = 2; c.estrelas = 0; c.tagFoco = TAG_N; c.desfazerFoco = 1;
      return 1;
    }
    return c.modoDetalhe;
  }
  if (k == SDLK_LEFT)  { if (c.foco > 0) c.foco--; c.desde = ultimoAgora; return 1; }
  if (k == SDLK_RIGHT) { if (c.foco < 2) c.foco++; c.desde = ultimoAgora; return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (c.passo == 1) { escolherMensagem(); c.desde = ultimoAgora; return 1; }
    confirmarNota(reacao_nota_trakt(VALOR[c.foco]));
    // COM AMIGO NA ORIGEM o cartao continua: "mandar uma mensagem?". A
    // reacao ja foi (recresp_responder dentro de reacao_responder).
    if (respondeAmigo() && c.nome[0]) {
      c.passo = 1; c.foco = 0; c.desde = ultimoAgora;
      return 1;
    }
    c.nota = reacao_nota_trakt(VALOR[c.foco]);
    c.passo = 2; c.tagFoco = TAG_N; c.desfazerFoco = 1;
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
// PONTEIRO (#99): foco pelas MESMAS variaveis das setas (c.foco, e a contagem
// recomeca como no ← →); o OK do clique segue por reacao_evento. Idempotente;
// com o teclado do passo 2 aberto nao mexe (ele e camada propria).
static void ponteiroResposta(int i, int b) {
  (void)b;
  if (!c.aberto || c.escrevendo || i < 0 || i > 2 || c.foco == i) return;
  c.foco = i; c.desde = ultimoAgora;
}
int reacao_teste_foco(void) { return c.foco; }

// Ponteiro na ilha de avaliacao: hover so foca; o OK do clique segue por
// reacao_evento (o ativar NULL nao consome), como nas pilulas de reacao.
static void ponteiroNota(int nota, int b) {
  (void)b;
  if (!c.aberto || c.escrevendo || nota < 1 || nota > 10) return;
  c.estrelas = 1; c.nota = nota; c.notaA = nota; c.varredura = 0; c.desde = ultimoAgora;
}
static void ponteiroTag(int i, int b) {
  (void)b;
  if (!c.aberto || c.escrevendo || i < 0 || i > TAG_N) return;
  c.tagFoco = i; c.desde = ultimoAgora;
}
static void ponteiroDesfazer(int i, int b) {
  (void)b;
  if (!c.aberto || c.escrevendo) return;
  c.desfazerFoco = i; c.desde = ultimoAgora;
}

#ifdef NV_TOUCH_UI
static int reacaoTelefoneRolar(const PonteiroRolagem *e) {
  if (!c.aberto || c.escrevendo || !telefoneui_ativo()) return 0;
  return toquerol_evento(&reacaoTelefone.rolagem, e);
}
static void reacaoTelefoneEscolher(int i, int chave) {
  SDL_Event e;
  if (!telefoneui_ativo() || !c.aberto || c.escrevendo || chave != reacaoTelefoneGeracao * 2 + c.passo || i < 0 || i > 2) return;
  c.foco = i; c.desde = ultimoAgora;
  memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  reacao_evento(&e, 0);
}
static float reacaoTelefoneConteudo(const char *perg, const char *orig, const char *ver,
                                   float x, float y, float w, float a, Uint32 agora) {
  float inicio = y;
  if (orig[0]) y += telefonecartao_texto(TXT_G18M, orig, x, y, w, 28, .68f * a) + 16;
  y += telefonecartao_texto(TXT_ILHA_PERGUNTA, perg, x, y, w, 48, a) + 24;
  if (ver[0]) y += telefonecartao_texto(TXT_ILHA_GENERO, ver, x, y, w, 28, .55f * a) + 16;
  if (!c.modoDetalhe && c.aberto && !c.escrevendo) {
    float resta = fmaxf(0, 1 - (float)(agora - c.desde) / REACAO_TIMEOUT_MS);
    if (a > 0) plrui_trilho((GfxRect){x, y, w, 4}, resta, .953f, .949f, .937f, .55f * a);
    y += 20;
  }
  return y - inicio;
}
static void reacaoTelefoneDesenhar(const char *perg, const char *orig, const char *ver,
                                  const char *const *rot, Uint32 agora, float baseY, float a) {
  float total, h, y, w = NV_TELA_W - 48;
  int reg = c.aberto && !c.escrevendo && a > .99f && a < 1.01f;
  total = reacaoTelefoneConteudo(perg, orig, ver, 0, 0, w - 56, 0, agora);
  h = fminf(NV_TELA_H - 48, total + 48 + 3 * 76 + 2 * 12 + 24);
  y = c.modoPainel || c.modoDetalhe ? (NV_TELA_H - h) * .5f : baseY - h;
  y = fmaxf(24, fminf(y, NV_TELA_H - 24 - h));
  reacaoTelefone.painel = (GfxRect){24, y, w, h};
  reacaoTelefone.corpo = (GfxRect){52, y + 24, w - 56, h - 48 - 3 * 76 - 2 * 12 - 24};
  reacaoTelefone.botoes = 3; reacaoTelefone.interativo = reg;
  if (reacaoTelefone.chave != c.passo || a < .99f) {
    telefonecartao_limpar(&reacaoTelefone); reacaoTelefone.chave = c.passo;
  }
  reacaoTelefone.offset = fminf(reacaoTelefone.offset, fmaxf(0, total - reacaoTelefone.corpo.h));
  if (c.aberto && (c.modoDetalhe || c.modoPainel)) ponteiro_camada();
  if (reg) {
    ponteiro_alvo(24, y, w, h, NULL, NULL, 0, 0);
    toquerol_vincular(&reacaoTelefone.rolagem, reacaoTelefone.corpo, gfx_escala(), 0,
                     total - reacaoTelefone.corpo.h, 1, &reacaoTelefone.offset);
    ponteiro_rolagem(reacaoTelefoneRolar);
  }
  plrui_material(reacaoTelefone.painel, 36, 0, a);
  gfx_recorte(reacaoTelefone.corpo.x, reacaoTelefone.corpo.y, reacaoTelefone.corpo.w, reacaoTelefone.corpo.h);
  reacaoTelefoneConteudo(perg, orig, ver, reacaoTelefone.corpo.x,
                        reacaoTelefone.corpo.y - reacaoTelefone.offset, reacaoTelefone.corpo.w, a, agora);
  gfx_sem_recorte();
  for (int i = 0; i < 3; i++) telefonecartao_botao(&reacaoTelefone, i, rot[i], c.foco == i,
                                                 ponteiroResposta, reacaoTelefoneEscolher, reacaoTelefoneGeracao * 2 + c.passo, a);
}
#endif

// A FILEIRA DE ESTRELAS (meia estrela, 1 a 10): cinco estrelas de 44 px; a
// cheia e a meia pela mesma forma recortada. Foco = acento do tema; sem foco, o
// valor que ja existe (c.nota) em branco suave.
#define ESTRELA_LINHA 76.0f
static void corNota(float *r, float *g, float *b) {
  CorvivaPaleta p;
  ajustes_acento(r, g, b);
  tex_publicar_paleta(c.poster);
  if (corviva_paleta(c.poster, &p) || corviva_cena_paleta(&p)) {
    // Lift the cached artwork hue into the warm, readable island focus.
    *r = .72f + .25f * p.acento[0];
    *g = .62f + .25f * p.acento[1];
    *b = .53f + .25f * p.acento[2];
  }
}

static void estrelasDesenhar(float x, float y, float a) {
  const float t = 47.0f, g = 7.0f;
  int reg = c.aberto && a > .99f && a < 1.01f;
  const int saved = c.passo == 2 && !strchr(c.alvo, ':');
  float v = c.nota ? notaVisivel() : reacao_nota(c.alvo);
  int i;
  c.estrelaA = notaMola(c.estrelaA, c.estrelas ? 1 : 0, 1.f / 60, NV_MOLA_FOCO);
  float al = a;
  float ar, ag, ab;
  corNota(&ar, &ag, &ab);
  if (!saved) gfx_cor((GfxRect){x - 15, y - 12, 642, 71}, 22.f / 71, .953f, .949f, .937f,
          (.055f + .075f * c.estrelaA) * a);
  char dica[96];
  TxtLinha l;
  for (i = 0; i < 5; i++) {
    float sx = x + (float)i * (t + g);
    float ms = (float)(ultimoAgora - c.varredura) - i * 35.f;
    float pulse = c.varredura && !ajustes_animacoes_reduzidas_usuario() && ms >= 0 && ms < 110 ? sinf(ms / 110.f * 3.141593f) : 0;
    float size = t * (1 + .06f * pulse);
    GfxRect r = { sx - (size - t) * .5f, y - (size - t) * .5f, size, size };
    float fill = anim_clamp((v - 2 * i) * .5f, 0, 1);
    gfx_icone(r, "pl_star", .953f, .949f, .937f, al * .27f);
    if (reg) {
      ponteiro_alvo(sx - 8, y - 8, t * .5f + 8, t + 16, NULL, ponteiroNota, i * 2 + 1, 0);
      ponteiro_alvo(sx + t * .5f, y - 8, t * .5f + 8, t + 16, NULL, ponteiroNota, i * 2 + 2, 0);
    }
    if (fill > .001f) {
      gfx_recorte(r.x, r.y, size * fill, size);
      gfx_icone(r, "pl_star-f", ar, ag, ab, al * (.85f + .15f * pulse));
      gfx_sem_recorte();
    }
  }
  if (v) {
    int shown = (int)floorf(v + .001f);
    snprintf(dica, sizeof dica, "%d.%d", shown / 2, shown % 2 ? 5 : 0); plrui_decimal(dica);
    l = txt_linha(TXT_LOG_N44, dica, 243, 242, 239, 255);
    txt_desenhar_alpha(l, saved ? x + 287 : x + 575 - l.w, y + (t - l.h) * .5f, a);
    l = txt_linha(TXT_ILHA_SUB, "/ 5", 160, 157, 153, 255);
    txt_desenhar_alpha(l, x + (saved ? 352 : 582), y + 25, a);
  } else {
    l = txt_linha(TXT_G18R, i18n("Dar uma nota"), 185, 180, 175, 255);
    txt_desenhar_alpha(l, x + 627 - l.w, y + 15, a);
  }
}

static float notaBotao(float x, float y, const char *rotulo, const char *icone, float foco, float a, float largura) {
  float w = largura > 0 ? largura : (float)txt_largura(TXT_ILHA_ITEM, i18n(rotulo)) + 56 + (icone ? 34 : 0);
  float f = anim_clamp(foco, 0, 1), ar, ag, ab;
  GfxRect r = {x, y - 2 * f, w, 56};
  corNota(&ar, &ag, &ab);
  gfx_cor(r, .5f, .141f, .149f, .173f, a);
  // Selection remains visible after focus moves away; glass belongs to the island.
  if (icone && !strcmp(icone, "aj_check")) gfx_cor(r, .5f, ar, ag, ab, .22f * a);
  if (f > .005f) gfx_cor(r, .5f, ar, ag, ab, f * a);
  int cor = f > .5f ? plrui_tinta() : 239;
  if (icone) gfx_icone((GfxRect){x + 20, r.y + 16, 24, 24}, icone, cor / 255.f, cor / 255.f, cor / 255.f, a);
  float dx = icone ? 54 : 24;
  TxtLinha l = txt_linha_corta(TXT_ILHA_ITEM, rotulo, cor, cor, cor, 255, w - dx - 18);
  txt_desenhar_alpha(l, x + dx, r.y + (56 - l.h) * .5f, a);
  return w;
}

static void painelNota(GfxRect r, float a) {
  float ar, ag, ab;
  corNota(&ar, &ag, &ab);
  plrui_material(r, 32, 0, a);
  gfx_cor(r, 32 / r.h, .055f, .059f, .071f, (ajustes_vidro() ? .78f : .98f) * a);
  gfx_luz_canto(r, 32 / r.h, 48, -80, r.w * 1.1f, ar, ag, ab, .16f * a);
}

// Approved 01 Ilha geometry, shared by detail and episode rating.
static void textoNota(TxtEstilo estilo, const char *s, float x, float y, float a, int cor) {
  TxtLinha l = txt_linha(estilo, i18n(s), cor, cor, cor, 255);
  txt_desenhar_alpha(l, x, y, a);
}

static void ilhaNotaDesenhar(float baseY, float a) {
  const int ep = strchr(c.alvo, ':') != NULL, salvo = c.passo == 2;
  float w = 710, h = ep ? 550 : salvo ? 474 : c.estrelas ? 545 : 509;
  float x = NV_TELA_W - 48 - w, y = baseY - h + (1 - a) * 24;
  float tx = x + 34, ty = y + 30, ar, ag, ab;
  static const char *const icons[] = {"pl_thumbs-up", "pl_meh", "pl_thumbs-down"};
  corNota(&ar, &ag, &ab);
  if (c.modoPainel) {
    x = (c.cx >= 0 ? c.cx : NV_TELA_W * .5f) - w * .5f;
    y = (NV_TELA_H - h) * .5f; tx = x + 34; ty = y + 30;
  }
  if (c.aberto && (c.modoDetalhe || c.modoPainel)) ponteiro_camada();
  painelNota((GfxRect){x, y, w, h}, a);
  GfxRect thumb = {tx, ty + 10, 65, 76};
  GLuint tex = c.poster[0] ? tex_obter_larg(c.poster, 76) : 0;
  gfx_cor(thumb, 12.f / 76, ar, ag, ab, .12f * a);
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(c.poster);
    gfx_rect(thumb, tex, GFX_CARD, 0, 0, 0, 12.f / 76, 1, 1, 1, a);
  }
  plrui_kicker(ep ? i18n("Avaliar episódio") : i18n("Sua opinião"), tx + 83, ty, 184, 170, 160, a);
  textoNota(TXT_ILHA_PERGUNTA, ep ? "Como foi este episódio?" : "Valeu a sessão?", tx + 83, ty + 25, a, 243);
  char title[220];
  int t, e;
  if (ep && sscanf(strchr(c.alvo, ':'), ":%d:%d", &t, &e) == 2)
    snprintf(title, sizeof title, "%s · T%d:E%d", c.titulo, t, e);
  else snprintf(title, sizeof title, "%s", c.titulo);
  TxtLinha name = txt_linha_corta(TXT_ILHA_SUB, title, 182, 178, 174, 255, 545);
  txt_desenhar_alpha(name, tx + 83, ty + 76, a);
  ty += ep ? 152 : 124;
  if (salvo && !ep) {
    gfx_cor((GfxRect){tx, ty, 64, 64}, .5f, ar, ag, ab, .14f * a);
    gfx_icone((GfxRect){tx + 15, ty + 15, 34, 34}, "aj_check", ar, ag, ab, a);
    textoNota(TXT_ILHA_PERGUNTA, "Avaliação salva", tx + 82, ty, a, 243);
    textoNota(TXT_ILHA_SUB, "Sua opinião agora faz parte do título.", tx + 82, ty + 46, a, 185);
    ty += 100;
  } else if (!ep) {
    for (int i = 0; i < 3; i++) {
      float cx = tx + 99 + i * 222;
      float f = c.estrelas || (c.modoDetalhe && !c.interagiu) ? 0 : anim_clamp(c.focoA[i], 0, 1);
      float d = 84 + 4 * f;
      GfxRect r = {cx - d * .5f, ty - 2 * f, d, d};
      if (f > .01f) gfx_luz_canto((GfxRect){cx - 64, ty - 20, 128, 128}, .5f, 64, 64, 85, ar, ag, ab, .18f * f * a);
      gfx_cor(r, .5f, 1, 1, 1, .08f * a);
      if (f > .01f) gfx_cor(r, .5f, ar, ag, ab, f * a);
      float ink = f > .5f ? .13f : .82f;
      gfx_icone((GfxRect){cx - 17, ty + 25, 34, 34}, icons[i], ink, ink, ink, a);
      TxtLinha l = txt_linha(f > .5f ? TXT_G19M : TXT_ILHA_SUB, ROTULO[i], 220, 215, 210, 255);
      txt_desenhar_alpha(l, cx - l.w * .5f, ty + 96, a);
      if (c.aberto && a > .99f && !c.estrelas) ponteiro_alvo(cx - 90, ty, 180, 124, ponteiroResposta, NULL, i, 0);
    }
    ty += 132;
  }
  estrelasDesenhar(tx + (salvo && !ep ? 0 : 15), ty + 12, a);
  ty += 71;
  if (ep) {
    textoNota(TXT_ILHA_ITEM, "O que ficou com você?", tx, ty + 22, a, 232);
    textoNota(TXT_ILHA_NUM, "Opcional", tx + 263, ty + 25, a, 157);
    // Six chips per page; every legacy bit remains reachable and unchanged.
    int first = c.tagFoco < TAG_N ? c.tagFoco / TAG_PAGINA * TAG_PAGINA : 0;
    float bx = tx, by = ty + 62;
    for (int i = first; i < TAG_N && i < first + TAG_PAGINA; i++) {
      int bit = TAG_ORDEM[i], selected = (c.tags & (1u << bit)) != 0;
      float f = salvo && c.tagFoco == i ? 1 : 0;
      int ink = f ? 33 : selected ? 230 : 207;
      TxtLinha l = txt_linha(TXT_G18R, TAGS[bit], ink, ink, ink, 255);
      float cw = l.w + 34 + (selected ? 23 : 0);
      if (i > first && ((i - first) % TAG_COLS == 0 || bx + cw > tx + 642)) { bx = tx; by += 54; }
      gfx_cor((GfxRect){bx, by, cw, 44}, .5f, selected || f ? ar : .953f,
              selected || f ? ag : .949f, selected || f ? ab : .937f,
              (f ? 1 : selected ? .16f : .08f) * a);
      if (selected) gfx_icone((GfxRect){bx + 17, by + 13, 17, 17}, "aj_check", ar, ag, ab, a);
      txt_desenhar_alpha(l, bx + 17 + (selected ? 23 : 0), by + (44 - l.h) * .5f, a);
      if (c.aberto && a > .99f && a < 1.01f) ponteiro_alvo(bx, by, cw, 44, ponteiroTag, NULL, i, 0);
      bx += cw + 10;
    }
    ty += 160;
  } else if (c.estrelas) {
    gfx_icone((GfxRect){tx, ty + 12, 16, 16}, "pl_chevron-left", ar, ag, ab, a);
    gfx_icone((GfxRect){tx + 23, ty + 12, 16, 16}, "pl_chevron-right", ar, ag, ab, a);
    textoNota(TXT_ILHA_NUM, "Ajuste de meia em meia estrela", tx + 46, ty + 10, a, 212);
    ty += 41;
  }
  if (!salvo || ep) textoNota(TXT_ILHA_NUM, "Sua opinião ajuda a escolher a próxima sessão.", tx, ty + 20, a, 178);
  else textoNota(TXT_ILHA_NUM, "Sua nota, junto das outras opiniões.", tx, ty + 15, a, 178);
  if (trakt_ativo() || simkl_ativo()) {
    const char *service = trakt_ativo() ? simkl_ativo() ? "Trakt · Simkl" : "Trakt" : "Simkl";
    TxtLinha sv = txt_linha(TXT_ILHA_HORA, service, 178, 174, 170, 255);
    txt_desenhar_alpha(sv, tx + 642 - sv.w, ty + 20, a);
  }
  float fy = y + h - (salvo || ep ? 106 : 76);
  gfx_cor((GfxRect){tx, fy, 642, 1}, 0, 1, 1, 1, .08f * a);
  if (salvo && !ep) {
    float dw = notaBotao(tx, fy + 21, "Desfazer", "pl_rotate-ccw", c.desfazerFoco, a, 0);
    if (c.aberto && a > .99f && a < 1.01f) ponteiro_alvo(tx, fy + 21, dw, 56, ponteiroDesfazer, NULL, 1, 0);
    const char *keys[] = {"OK", "Voltar"}, *labels[] = {"Desfazer", "Fechar"};
    plrui_dicas(keys, labels, 2, tx + 642, fy + 49, 1, a);
  } else if (ep && salvo) {
    float sw = notaBotao(tx, fy + 21, "Salvar avaliação", NULL, c.tagFoco == TAG_N, a, 0);
    if (c.aberto && a > .99f && a < 1.01f) ponteiro_alvo(tx, fy + 21, sw, 56, ponteiroTag, NULL, TAG_N, 0);
    const char *keys[] = {"OK", "Voltar"}, *labels[] = {"Selecionar", "Sair"};
    plrui_dicas(keys, labels, 2, tx + 642, fy + 49, 1, a);
  } else {
    textoNota(TXT_ILHA_SUB, "Agora não", tx, fy + 24, a, 185);
    const char *keys[] = {"OK"}, *labels[] = {c.estrelas ? "Salvar" : "Avaliar"};
    float kw = plrui_dicas(keys, labels, 1, -1, 0, 0, 0);
    float hx = tx + 642 - kw;
    if (c.estrelas) {
      plrui_dicas(keys, labels, 1, tx + 642, fy + 35, 1, a);
      TxtLinha step = txt_linha(TXT_ILHA_NUM, "0,5", 185, 180, 174, 255);
      hx -= step.w + 17 + 46 + 7;
      gfx_cor((GfxRect){hx, fy + 22, 46, 26}, 6.f / 26, 1, 1, 1, .10f * a);
      gfx_icone((GfxRect){hx + 5, fy + 26, 17, 17}, "pl_chevron-left", .91f, .90f, .87f, a);
      gfx_icone((GfxRect){hx + 24, fy + 26, 17, 17}, "pl_chevron-right", .91f, .90f, .87f, a);
      txt_desenhar_alpha(step, hx + 53, fy + 25, a);
    } else {
      float ew = txt_largura(TXT_ILHA_NUM, i18n("Estrelas"));
      plrui_dicas(keys, labels, 1, tx + 642 - ew - 52, fy + 35, 1, a);
      hx = tx + 642 - ew - 35;
      gfx_cor((GfxRect){hx, fy + 22, 28, 26}, 6.f / 26, 1, 1, 1, .10f * a);
      gfx_icone((GfxRect){hx + 5, fy + 26, 18, 18}, "aj_arrow-up", .91f, .90f, .87f, a);
      textoNota(TXT_ILHA_NUM, "Estrelas", hx + 35, fy + 25, a, 185);
    }
  }
  if (!c.modoDetalhe && c.aberto)
    plrui_trilho((GfxRect){tx, y + h - 5, 642, 3}, fmaxf(0, 1.f - (float)(ultimoAgora - c.desde) / REACAO_TIMEOUT_MS), ar, ag, ab, .4f * a);
}

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
  float a = c.anim, w = 800.0f, h, x, y, lead, hp, pw = 0.0f;
  char perg[256], orig[160], ver[160];
  static const char *const ICONE[3] = { "pl_thumbs-up", NULL, "pl_thumbs-down" };
  int i;
  if (a < 0.01f || !c.imdb[0] || !perfis_sessao_atual(c.sessao)) return;
  if (c.passo != 1) { ilhaNotaDesenhar(baseY, a); return; }
  const char *const *rot = c.passo == 1 ? ROTULO_MSG : ROTULO;
  static const char *const ICONE_MSG[3] = { NULL, NULL, NULL };
  const char *const *ico = c.passo == 1 ? ICONE_MSG : ICONE;
  if (c.passo == 1) snprintf(perg, sizeof perg, i18n("Mandar uma mensagem para %s?"), c.nome);
  else snprintf(perg, sizeof perg, i18n("O que achou de %s?"), c.titulo);
  linhaOrigem(orig, sizeof orig);
  ver[0] = 0;
  if (orig[0] && (c.envia || respondeAmigo()))
    snprintf(ver, sizeof ver, i18n("%s vai ver sua resposta"), c.nome);
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { reacaoTelefoneDesenhar(perg, orig, ver, rot, agora, baseY, a); return; }
#endif
  for (i = 0; i < 3; i++) pw += (float)txt_largura(TXT_ILHA_ITEM, i18n(rot[i])) + 56 + (ico[i] ? 34 : 0) + (i ? 12 : 0);
  if (pw + 72.0f > w) w = pw + 72.0f;
  lead = (float)txt_linha(TXT_ILHA_PERGUNTA, "Ág", 245, 246, 248, 255).h + 6.0f;
  hp = txt_bloco_corta(TXT_ILHA_PERGUNTA, perg, 0, 0, 0, -4000.0f, -4000.0f, w - 72.0f, lead, 0.0f, 2);
  h = 66.0f + (orig[0] ? 30.0f + 12.0f : 0.0f) + hp + 28.0f + 56.0f + 28.0f + 22.0f + 30.0f + (c.passo == 0 ? ESTRELA_LINHA : 0.0f);
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
  // Na pagina do titulo e no painel de Salvos o cartao e modal (as teclas nao
  // vazam): camada, os alvos de tras deixam de valer. No player ele nao e
  // camada — com os controles no ar as teclas sao do player.
  if (c.aberto && (c.modoDetalhe || c.modoPainel)) ponteiro_camada();
  painelNota((GfxRect){ x, y, w, h }, a);
  plrui_kicker("Sua nota", x + 36, y + 28, 174, 179, 186, a);
  { float ty = y + 66.0f, tx = x + 36.0f;
    if (orig[0]) {
      TxtLinha lo = txt_linha_corta(TXT_CAPTION, orig, 243, 242, 239, 158, w - 72.0f - 42.0f);
      rec_avatar_estilo((GfxRect){ tx, ty, 30.0f, 30.0f }, "", c.nome, c.nome, a, TXT_MINI);
      txt_desenhar_alpha(lo, tx + 30.0f + 12.0f, ty + (30.0f - (float)lo.h) * 0.5f, a);
      ty += 30.0f + 12.0f;
    }
    txt_bloco_corta(TXT_ILHA_PERGUNTA, perg, 243, 242, 239, tx, ty, w - 72.0f, lead, a, 2);
    ty += hp + 28.0f;
    { float bx = tx;
      // So assentado (a mola da entrada parada) e sem o teclado por cima.
      int reg = c.aberto && !c.escrevendo && a > 0.99f && a < 1.01f;
      for (i = 0; i < 3; i++) {
        float bw = notaBotao(bx, ty, rot[i], ico[i], c.estrelas ? 0 : c.focoA[i], a, 0);
        if (reg) ponteiro_alvo(bx, ty, bw, 60.0f, ponteiroResposta, NULL, i, 0);
        bx += bw + 12.0f;
      } }
    ty += 56.0f + 28.0f;
    if (c.passo == 0) { estrelasDesenhar(tx, ty, a); ty += ESTRELA_LINHA; }
    { float yc = ty + 11.0f, tw = 0.0f;
      if (ver[0]) {
        TxtLinha lv = txt_linha_corta(TXT_CAPTION, ver, 243, 242, 239, 128, w * 0.5f);
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
  if (!ci || strncmp(ci->imdb, "tt", 2)) return 0;
  abrir(ci->imdb, ci->tipo, ci->titulo, ci->poster, 1, SDL_GetTicks());
  { Reacao *r = achar(ci->imdb);
    // A ORIGEM GUARDADA na pendencia vale mais que a busca de agora: a
    // recomendacao pode ter saido da lista do servidor depois da pergunta.
    if (r && r->rec && !c.rec) { c.rec = r->rec; snprintf(c.nome, sizeof c.nome, "%s", r->nome); } }
  c.nota = reacao_nota(ci->imdb);
  c.estrelas = c.nota > 0;
  c.anim = 0.0f; ultimoAgora = SDL_GetTicks();
  return 1;
}

int reacao_episodio_abrir(const CatItem *ci, int t, int e) {
  if (!ci || strncmp(ci->imdb, "tt", 2) || t < 0 || e <= 0) return 0;
  char id[40]; snprintf(id, sizeof id, "%s:%d:%d", ci->imdb, t, e);
  abrir(id, "series", ci->titulo, ci->poster, 1, SDL_GetTicks());
  c.estrelas = 1; c.nota = reacao_nota(id);
  if (!c.nota) c.nota = 8;
  ultimoAgora = SDL_GetTicks();
  return 1;
}

void reacao_detalhe_dica(const CatItem *ci, float a) {
  char perg[256];
  TxtLinha l;
  float x, y, w;
  // O cartao aberto (pela pagina) desenha por cima de tudo, com a mola dele.
  if (c.aberto && !perfis_sessao_atual(c.sessao)) c.aberto = 0;
  if (c.aberto && c.modoDetalhe && !c.modoPainel) {
    ultimoAgora = SDL_GetTicks();
    if (c.escrevendo) { teclado_atualizar(1.0f / 60.0f, SDL_GetTicks()); tecladoResultado(); }
    c.anim = notaMola(c.anim, 1.0f, 1.0f / 60.0f, NV_MOLA_TELA);
    { int i;
      for (i = 0; i < 3; i++)
        c.focoA[i] = notaMola(c.focoA[i], c.foco == i ? 1.0f : 0.0f, 1.0f / 60.0f,
                               c.foco == i ? NV_MOLA_FOCO : NV_MOLA_DESFOCO); }
    reacao_desenhar(SDL_GetTicks(), NV_TELA_H - 48.0f);
    return;
  }
  if (c.modoDetalhe && !c.aberto && !c.modoPainel) c.anim = 0.0f;
  if (a < 0.01f) return;
  // Keep the saved half-star value visible on the title page.
  if (ci && ci->imdb[0] && reacao_nota(ci->imdb) > 0) {
    int n = reacao_nota(ci->imdb);
    char tx[160];
    snprintf(tx, sizeof tx, "%s · %d.%d", i18n("Sua nota"), n / 2, (n % 2) * 5);
    plrui_decimal(tx);
    l = txt_linha(TXT_G18M, tx, 243, 242, 239, 255);
    w = 24.0f + 10.0f + (float)l.w;
    x = NV_TELA_W - RX_MARGEM - w;
    y = NV_TELA_H - 48.0f - (float)l.h;
    gfx_cor((GfxRect){ x - 18.0f, y - 10.0f, w + 36.0f, (float)l.h + 20.0f }, 0.5f,
            .094f, .098f, .11f, .96f * a);
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_icone((GfxRect){ x, y + ((float)l.h - 24.0f) * 0.5f, 24.0f, 24.0f }, "pl_star-f", ar, ag, ab, a * .95f);
    txt_desenhar_alpha(l, x + 34.0f, y, a * 0.9f);
    return;
  }
  if (!ci || strncmp(ci->imdb, "tt", 2)) return;
  snprintf(perg, sizeof perg, "%s", i18n("↑ nota em estrelas"));
  // DISCRETA: texto pequeno e cinza, sem caixa de foco — ela nao e um botao,
  // e um lembrete de que CIMA abre a pergunta.
  l = txt_linha_corta(TXT_G18M, perg, 176, 180, 190, 255, 620.0f);
  w = 24.0f + 12.0f + (float)l.w;
  x = NV_TELA_W - RX_MARGEM - w;
  y = NV_TELA_H - 48.0f - (float)l.h;
  gfx_cor((GfxRect){ x - 18.0f, y - 10.0f, w + 36.0f, (float)l.h + 20.0f }, 0.5f,
          .094f, .098f, .11f, .96f * a);
  gfx_icone((GfxRect){x, y + (l.h - 24) * .5f, 24, 24}, "aj_arrow-up", .78f, .80f, .83f, a);
  txt_desenhar_alpha(l, x + 36.0f, y, a * .85f);
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
