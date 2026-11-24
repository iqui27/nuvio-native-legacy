// tests/farol_compara.c — tela P4 "Comparar" (lote sense, issue 49).
//
// Liga contra o app inteiro (menos main.c): a tela usa o catalogo, as
// fontes e a casca 2.1 reais. Parte 1 (logica, sem GL): abertura, cena
// real farolcena_comparar + farolpeca_ler, omissao de eixos, regra da
// qualidade (sem consulta ao abrir, "Ver qualidade" sob demanda, Voltar
// cancela) e D-pad em 2 e 3 colunas. Parte 2 (GLES2 1920x1080): capturas
// PNG de 3 e 2 colunas no diretorio do run, validadas e apagadas pelo .sh.
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "farolcena.h"
#include "farolcompara.h"
#include "ajustes.h"
#include "catalogo.h"
#include "dados.h"
#include "fonteantecipa.h"
#include "gfx.h"
#include "streams.h"
#include "tex_cache.h"
#include "text.h"

#ifdef NV_LINUX_DESKTOP
#include <GLES2/gl2.h>
#endif

// A base onda D chama testar() em src/resolver.c:838 sem defini-la (fora da
// fronteira desta issue); o app inteiro nao liga sem ela. Este teste nunca
// abre o resolver.
void testar(int s);
void testar(int s) { (void)s; }

// NV_SOFT_ASSERT (so a prova de falha-na-base usa): registra e segue, para
// provar TODAS as casas novas num unico binario da base. Sem o define,
// CHECK e assert comum.
static int gFalhas;
#ifdef NV_SOFT_ASSERT
#define NV_CHECK(x) do { if (!(x)) { gFalhas++; \
  fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)
#else
#define NV_CHECK(x) assert(x)
#endif

// ------------------------------------------------------------ ganchos
static atomic_int gChamadas, gLiberar, gCanceladas, gSeq;
static atomic_int gBar[8];          // modo 3: barreira por chamada (ordem global)
static char gIds[8][80];            // ids na ordem das chamadas
static int gModo;   // 0 = espera liberar, 1 = devolve na hora, 3 = barreira/seq

static int consultaTeste(const char *id, const char *tipo, int (*cancelado)(void *),
                         void *ctx, FarolInfoFonte *out, int max) {
  int s;
  (void)tipo;
  s = atomic_fetch_add(&gSeq, 1);
  if (s >= 0 && s < 8) snprintf(gIds[s], sizeof gIds[0], "%s", id ? id : "");
  atomic_fetch_add(&gChamadas, 1);
  if (gModo == 3 && (s == 0 || s == 3)) {   // A(s=0) e o ULTIMO de B(s=3) na barreira;
    while (!atomic_load(&gBar[s ? 1 : 0])) SDL_Delay(1);  // ignora o cancela:
    if (cancelado(ctx)) { atomic_fetch_add(&gCanceladas, 1); return -1; }
  } else {                          // o fio velho tem de sair da consulta por ultimo
    while (gModo == 0 && !atomic_load(&gLiberar)) {
      if (cancelado(ctx)) { atomic_fetch_add(&gCanceladas, 1); return -1; }
      SDL_Delay(1);
    }
    if (cancelado(ctx)) { atomic_fetch_add(&gCanceladas, 1); return -1; }
  }
  if (max < 1) return 0;
  memset(out, 0, sizeof *out);
  out->altura = strstr(id, "2") ? 1080 : 2160;
  out->dv = strstr(id, "2") ? 0 : 1;
  out->toca = 1;
  snprintf(out->audio, sizeof out->audio, "pt");
  return 1;
}

static int gPlayer;
static int playerTeste(void) { return gPlayer; }
static long long epochTeste(void) { return 1775077200LL; }

// ------------------------------------------------------------ itens
static FarolCompItem IT[3];

static void montarItens(void) {
  static const char *const REF[3] = { "tt0000001", "tt0000002", "tt0000003" };
  static const char *const TIT[3] = { "Alfa", "Beta", "Gama" };
  static const int MIN[3] = { 90, 150, 45 };
  static const int NOTA[3] = { 80, 90, 0 };
  static const int32_t PTS[3] = { 20000, 30000, 10000 };
  int i;
  memset(IT, 0, sizeof IT);
  for (i = 0; i < 3; i++) {
    snprintf(IT[i].ref, sizeof IT[i].ref, "%s", REF[i]);
    snprintf(IT[i].tipo, sizeof IT[i].tipo, "movie");
    snprintf(IT[i].titulo, sizeof IT[i].titulo, "%s", TIT[i]);
    snprintf(IT[i].meta, sizeof IT[i].meta, "2024 · Filme");
    IT[i].catIndice = 100 + i;
    IT[i].minutos = MIN[i];
    IT[i].notaPct = NOTA[i];
    IT[i].pontos = PTS[i];
    IT[i].nFontes = -1;
  }
  snprintf(IT[0].clima, sizeof IT[0].clima, "Drama");
  snprintf(IT[0].onde, sizeof IT[0].onde, "Netflix");
}

// ------------------------------------------------------------ teclas
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  farolcompara_evento(&e);
  if (k == SDLK_RETURN) { e.type = SDL_KEYUP; farolcompara_evento(&e); }
}

static void quadro(void) { farolcompara_atualizar(0.016f, 1000); }

static void reabrir(int n, int tempoMin) {
  farolcompara_teste_reset();
  assert(farolcompara_abrir(IT, n, tempoMin) == 1);
  quadro();
}

// ------------------------------------------------------------ provas
static void provaAbertura(void) {
  FarolCompItem dup[2];
  montarItens();
  assert(farolcompara_abrir(NULL, 0, 0) == 0);
  assert(farolcompara_abrir(IT, 0, 0) == 0);
  assert(farolcompara_abrir(IT, 1, 0) == 0);
  assert(!farolcompara_aberto());
  dup[0] = IT[0]; dup[1] = IT[0];
  assert(farolcompara_abrir(dup, 2, 0) == 0);           // repetidos caem
  gPlayer = 1;
  assert(farolcompara_abrir(IT, 3, 0) == 0);            // player aberto
  gPlayer = 0;
  assert(farolcompara_abrir(IT, 2, 0) == 1 && farolcompara_n_colunas() == 2);
  assert(farolcompara_abrir(IT, 3, 0) == 1 && farolcompara_n_colunas() == 3);
  {
    FarolCompItem quatro[4];
    memcpy(quatro, IT, sizeof IT);
    quatro[3] = IT[1];
    snprintf(quatro[3].ref, sizeof quatro[3].ref, "tt0000004");
    assert(farolcompara_abrir(quatro, 4, 0) == 1 && farolcompara_n_colunas() == 3);
  }
  farolcompara_fechar();
  assert(!farolcompara_aberto());
  puts("  abertura: recusa 0/1/repetidos/player, abre 2 e 3, corta em 3");
}

// O veredito/escolha da tela tem de ser o da cena real.
static void conferirCena(int tempoMin, int escolhaEsperada, int temVeredito) {
  static const char *const REFS[3] = { "c0", "c1", "c2" };
  static FarolCena cena;
  static char json[FAROL_CENA_MAX + 1];
  FarolCenaCtx ctx;
  FarolCenaCmp cmp;
  char v[256];
  int i;
  memset(&ctx, 0, sizeof ctx); ctx.nCand = 3; ctx.perfil = -1;
  memset(&cmp, 0, sizeof cmp);
  cmp.nItens = 3; cmp.tempoMin = tempoMin;
  for (i = 0; i < 3; i++) {
    cmp.itens[i] = REFS[i]; cmp.pontos[i] = IT[i].pontos; cmp.minutos[i] = (uint16_t)IT[i].minutos;
  }
  for (i = 0; i < FAROL_EIXO_MAX; i++) cmp.eixos[i] = (uint8_t)i;
  cmp.nEixos = FAROL_EIXO_MAX;
  assert(farolcena_comparar(json, sizeof json, &ctx, &cmp) > 0);
  assert(farolpeca_ler(&cena, json, 3) == 1 && cena.nRecusadas == 0);
  assert(cena.pecas[0].tipo == FP_COMPARAR);
  assert(cena.pecas[0].u.comparar.escolha == escolhaEsperada);
  assert(farolpeca_texto_tem(&cena.pecas[0].u.comparar.veredito) == temVeredito);

  reabrir(3, tempoMin);
  assert(farolcompara_escolha() == escolhaEsperada);
  assert(farolcompara_foco() == escolhaEsperada && !farolcompara_foco_botao());
  assert(farolcompara_veredito(v, sizeof v) == temVeredito);
  if (temVeredito) assert(strstr(v, IT[escolhaEsperada].titulo) && !strstr(v, "c0"));
  else assert(v[0] == 0);
}

static void provaCena(void) {
  montarItens();
  conferirCena(100, 0, 1);   // cabem Alfa (90) e Gama (45): Alfa pontua mais
  conferirCena(0, 1, 0);     // sem tempo: maior pontuacao, sem veredito
  conferirCena(30, 1, 0);    // nenhum cabe: idem
  conferirCena(200, 1, 1);   // todos cabem: Beta
  // Abertura direta por cena P4 com refs "tt" e eixos escolhidos.
  {
    static const char *const R[2] = { "tt0000003", "tt0000001" };
    static char json[FAROL_CENA_MAX + 1];
    FarolCenaCtx ctx; FarolCenaCmp cmp;
    memset(&ctx, 0, sizeof ctx); ctx.perfil = -1;
    memset(&cmp, 0, sizeof cmp);
    cmp.nItens = 2; cmp.itens[0] = R[0]; cmp.itens[1] = R[1];
    cmp.pontos[0] = 5; cmp.pontos[1] = 9;
    cmp.eixos[0] = FP_EIXO_NOTA; cmp.eixos[1] = FP_EIXO_DURACAO; cmp.eixos[2] = FP_EIXO_PORQUE;
    cmp.nEixos = 3;
    assert(farolcena_comparar(json, sizeof json, &ctx, &cmp) > 0);
    farolcompara_teste_reset();
    assert(farolcompara_abrir_cena(json, IT, 3) == 1);
    assert(farolcompara_n_colunas() == 2);
    assert(farolcompara_escolha() == 1 && farolcompara_foco() == 1);
    // Gama nao tem nota, Alfa tem: a linha sobe. Porque: ninguem tem -> some.
    assert(farolcompara_n_eixos() == 2);
    assert(farolcompara_eixo(0) == FP_EIXO_NOTA && farolcompara_eixo(1) == FP_EIXO_DURACAO);
    assert(farolcompara_eixo(2) == -1);
    assert(!farolcompara_celula(0, FP_EIXO_NOTA, NULL, 0));
    assert(farolcompara_celula(1, FP_EIXO_NOTA, NULL, 0));
    assert(!farolcompara_botao_visivel());   // sem eixo qualidade na cena
    // Cena de outra tela ou json quebrado: recusa.
    assert(farolcompara_abrir_cena("{\"v\":1,\"tela\":\"home\",\"pecas\":[]}", IT, 3) == 0);
    assert(farolcompara_abrir_cena("{", IT, 3) == 0);
  }
  puts("  cena: veredito/escolha/foco = farolcena_comparar + farolpeca_ler");
}

static void provaEixos(void) {
  char c[FCMP_CEL_TAM];
  montarItens();
  reabrir(3, 0);
  assert(farolcompara_tem_eixo(FP_EIXO_DURACAO));
  assert(farolcompara_tem_eixo(FP_EIXO_NOTA));
  assert(farolcompara_tem_eixo(FP_EIXO_CLIMA));
  assert(farolcompara_tem_eixo(FP_EIXO_TERMINA));
  assert(farolcompara_tem_eixo(FP_EIXO_ONDE));
  assert(!farolcompara_tem_eixo(FP_EIXO_PORQUE));
  assert(!farolcompara_tem_eixo(FP_EIXO_AMIGOS));
  assert(!farolcompara_tem_eixo(FP_EIXO_QUALIDADE));
  assert(farolcompara_n_eixos() == 5);
  assert(farolcompara_celula(0, FP_EIXO_DURACAO, c, sizeof c) && !strcmp(c, "1 h 30 min"));
  assert(farolcompara_celula(2, FP_EIXO_DURACAO, c, sizeof c) && !strcmp(c, "45 min"));
  assert(farolcompara_celula(1, FP_EIXO_NOTA, c, sizeof c) && !strcmp(c, "90%"));
  assert(!farolcompara_celula(2, FP_EIXO_NOTA, c, sizeof c) && c[0] == 0);   // vazia, sem "nao sei"
  assert(!farolcompara_celula(1, FP_EIXO_CLIMA, c, sizeof c));
  assert(farolcompara_celula(0, FP_EIXO_TERMINA, c, sizeof c) && strchr(c, ':'));
  // Um so dado faz a linha subir.
  snprintf(IT[2].amigos, sizeof IT[2].amigos, "Ana terminou.");
  reabrir(3, 0);
  assert(farolcompara_tem_eixo(FP_EIXO_AMIGOS) && farolcompara_n_eixos() == 6);
  assert(!farolcompara_celula(0, FP_EIXO_AMIGOS, c, sizeof c));
  // Ordem das linhas = ordem da lista fechada.
  assert(farolcompara_eixo(0) == FP_EIXO_DURACAO && farolcompara_eixo(5) == FP_EIXO_AMIGOS);
  montarItens();
  puts("  eixos: sem dado em nenhuma coluna some; um dado faz a linha subir");
}

static void provaQualidade(void) {
  int antes;
  montarItens();
  farolcompara_teste_consulta(consultaTeste);
  atomic_store(&gChamadas, 0);
  atomic_store(&gLiberar, 0);
  gModo = 0;

  reabrir(3, 0);
  for (int i = 0; i < 30; i++) quadro();
  assert(atomic_load(&gChamadas) == 0);          // abrir nunca pergunta
  assert(farolcompara_botao_visivel() && !farolcompara_consultando());
  assert(!farolcompara_tem_eixo(FP_EIXO_QUALIDADE));

  // v + OK consulta; Voltar cancela e a tela fica.
  tecla(SDLK_DOWN);
  assert(farolcompara_foco_botao());
  tecla(SDLK_RETURN);
  assert(farolcompara_consultando());
  while (atomic_load(&gChamadas) < 1) SDL_Delay(1);
  tecla(SDLK_ESCAPE);
  assert(farolcompara_aberto() && !farolcompara_consultando());
  assert(farolcompara_teste_esperar(3000));
  assert(atomic_load(&gCanceladas) >= 1);
  quadro();
  for (int c = 0; c < 3; c++) assert(!farolcompara_qualidade_consultada(c));
  assert(!farolcompara_tem_eixo(FP_EIXO_QUALIDADE) && farolcompara_botao_visivel());
  assert(farolcompara_foco_botao());             // o foco nao foi a lugar nenhum

  // De novo, agora respondendo: so as colunas que faltam.
  gModo = 1;
  antes = atomic_load(&gChamadas);
  tecla(SDLK_RETURN);
  assert(farolcompara_teste_esperar(3000));
  quadro();
  assert(atomic_load(&gChamadas) == antes + 3);
  for (int c = 0; c < 3; c++) assert(farolcompara_qualidade_consultada(c));
  assert(farolcompara_tem_eixo(FP_EIXO_QUALIDADE));
  {
    char c0[FCMP_CEL_TAM], c1[FCMP_CEL_TAM];
    assert(farolcompara_celula(0, FP_EIXO_QUALIDADE, c0, sizeof c0));
    assert(farolcompara_celula(1, FP_EIXO_QUALIDADE, c1, sizeof c1));
    assert(strcmp(c0, c1));
  }
  assert(!farolcompara_botao_visivel() && !farolcompara_foco_botao());

  // A memoria da sessao serve a proxima abertura, sem perguntar de novo.
  antes = atomic_load(&gChamadas);
  farolcompara_fechar();
  assert(farolcompara_abrir(IT, 3, 0) == 1);
  quadro();
  assert(atomic_load(&gChamadas) == antes);
  assert(farolcompara_tem_eixo(FP_EIXO_QUALIDADE) && !farolcompara_botao_visivel());

  // Fontes passadas na entrada valem; a consulta pede so as outras.
  farolcompara_teste_reset();
  IT[1].nFontes = 0;
  assert(farolcompara_abrir(IT, 3, 0) == 1);
  assert(farolcompara_qualidade_consultada(1) && farolcompara_botao_visivel());
  antes = atomic_load(&gChamadas);
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  assert(farolcompara_teste_esperar(3000));
  quadro();
  assert(atomic_load(&gChamadas) == antes + 2);
  montarItens();

  // Fechar com consulta em curso tambem cancela.
  farolcompara_teste_reset();
  gModo = 0; atomic_store(&gLiberar, 0);
  assert(farolcompara_abrir(IT, 2, 0) == 1);
  antes = atomic_load(&gChamadas);
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  while (atomic_load(&gChamadas) < antes + 1) SDL_Delay(1);
  farolcompara_fechar();
  assert(farolcompara_teste_esperar(3000));
  quadro();
  assert(!farolcompara_aberto() && !farolcompara_consultando());
  gModo = 1;
  farolcompara_teste_reset();
  puts("  qualidade: 0 consultas ao abrir, v+OK consulta, Voltar cancela, memoria da sessao");
}

// P1-2: a geracao CANCELADA termina depois da consulta nova (o fio velho
// sai do addon ja vendo cancelado, mas so grava o carimbo de fim por
// ultimo). fcFimGen nao pode regredir nem S.consultando ficar preso em 1.
static void provaCorridaFim(void) {
  int i;
  montarItens();
  farolcompara_teste_consulta(consultaTeste);
  gModo = 3;
  atomic_store(&gSeq, 0);
  for (i = 0; i < 8; i++) atomic_store(&gBar[i], 0);

  reabrir(3, 0);
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);          // consulta A (geracao 1)
  for (i = 0; atomic_load(&gSeq) < 1 && i < 3000; i++) SDL_Delay(1);
  NV_CHECK(atomic_load(&gSeq) >= 1);              // A ja esta dentro da consulta
  NV_CHECK(farolcompara_consultando());
  tecla(SDLK_ESCAPE);                           // cancela A (geracao sobe)
  NV_CHECK(farolcompara_aberto() && !farolcompara_consultando());

  tecla(SDLK_RETURN);                           // consulta B (geracao nova)
  for (i = 0; atomic_load(&gSeq) < 4 && i < 3000; i++) SDL_Delay(1);
  NV_CHECK(atomic_load(&gSeq) >= 4);              // B chegou na ultima chamada
  atomic_store(&gBar[1], 1);                    // B termina inteira (fim=3)
  for (i = 0; farolcompara_teste_fios() > 1 && i < 3000; i++) SDL_Delay(1);
  atomic_store(&gBar[0], 1);                    // so agora A destrava
  NV_CHECK(farolcompara_teste_esperar(3000));
  quadro();
  NV_CHECK(!farolcompara_consultando());          // travava em 1: fim=1 != genAtual=3
  for (i = 0; i < 3; i++) NV_CHECK(farolcompara_qualidade_consultada(i));
  NV_CHECK(farolcompara_tem_eixo(FP_EIXO_QUALIDADE));
  farolcompara_fechar();
  gModo = 1;
  puts("  corrida: fio cancelado nao regrediu o fim; consultando zera");
}

// Item de catalogo minimo (o que farolcompara_abrir_titulos le).
static void montarCat(CatItem *c, const char *imdb, const char *tipo, int t, int e) {
  memset(c, 0, sizeof *c);
  snprintf(c->imdb, sizeof c->imdb, "%s", imdb);
  snprintf(c->tipo, sizeof c->tipo, "%s", tipo);
  snprintf(c->titulo, sizeof c->titulo, "Titulo %s", imdb);
  c->temporada = t;
  c->episodio = e;
}

// P1-3a + P2-1: ids de consulta a partir do catalogo e deduplicacao antes
// do corte em 3. P1-1: o indice guardado vale so junto com a identidade
// (o roteador em app.c revalida com cat_indice_vivo — ver farol_compara.sh).
static void provaTitulos(void) {
  CatItem cat[4];
  int idx[4];
  stream_invalidar("teste");
  montarCat(&cat[0], "tt4000001", "movie", 0, 0);
  montarCat(&cat[1], "tt4000002", "series", 0, 0);   // sem episodio
  montarCat(&cat[2], "tt4000003", "series", 2, 5);   // retomada
  montarCat(&cat[3], "tt4000004", "movie", 0, 0);
  cat_definir(cat, 4);
  idx[0] = 0; idx[1] = 1; idx[2] = 2;

  gModo = 1;
  farolcompara_teste_consulta(consultaTeste);
  farolcompara_teste_reset();
  atomic_store(&gSeq, 0);
  NV_CHECK(farolcompara_abrir_titulos(idx, 3, NULL, 0) == 1);
  NV_CHECK(farolcompara_n_colunas() == 3);
  NV_CHECK(farolcompara_botao_visivel());
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  NV_CHECK(farolcompara_teste_esperar(3000));
  quadro();
  {                                              // fios correm fora de ordem
    int filme = 0, semEp = 0, comEp = 0, i_;
    for (i_ = 0; i_ < 3; i_++) {
      filme |= !strcmp(gIds[i_], "tt4000001");      // filme: id cru
      semEp |= !strcmp(gIds[i_], "tt4000002:1:1");  // serie sem episodio
      comEp |= !strcmp(gIds[i_], "tt4000003:2:5");  // serie com episodio
    }
    NV_CHECK(filme && semEp && comEp);
  }
  farolcompara_fechar();

  // Dedup antes do corte: A A B C abre A, B e C (antes fechava em A e B).
  farolcompara_teste_reset();
  idx[0] = 0; idx[1] = 0; idx[2] = 1; idx[3] = 3;
  NV_CHECK(farolcompara_abrir_titulos(idx, 4, NULL, 0) == 1);
  NV_CHECK(farolcompara_n_colunas() == 3);
  {
    char ref[FAROL_REF_TAM];
    int cat_ = -1;
    tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);        // foco = terceira coluna
    tecla(SDLK_RETURN);
    NV_CHECK(farolcompara_pediu_abrir(&cat_, ref, sizeof ref));
    NV_CHECK(!strcmp(ref, "tt4000004") && cat_ == 3);
  }

  // O indice vale pela identidade: catalogo deslocado, mesmo imdb.
  cat_definir(cat + 1, 3);
  NV_CHECK(cat_indice_vivo(0, "tt4000002") == 0);
  NV_CHECK(cat_indice_vivo(2, "tt4000004") == 2);
  puts("  titulos: serie sem ep consulta ':1:1', dedup antes do corte");
}

// P1-3b + P2-3: a lista do alvo nao trunca nos 24 antes de priorizar quem
// toca (a confirmada pode estar na posicao 28) e stream_fonte_estado
// manda em toca (FA_OK sem cache afirmado tambem sustenta a linha).
static void provaFontesLista(void) {
  static Stream st[30];
  CatItem cat[3];
  int idx[2];
  char cel[FCMP_CEL_TAM];
  int i;
  montarCat(&cat[0], "tt4000001", "movie", 0, 0);
  montarCat(&cat[1], "tt4000005", "movie", 0, 0);
  montarCat(&cat[2], "tt4000006", "movie", 0, 0);
  cat_definir(cat, 3);
  farolcompara_teste_reset();

  for (i = 0; i < 30; i++) {
    memset(&st[i], 0, sizeof st[i]);
    snprintf(st[i].url, sizeof st[i].url, "https://cdn.test/%d.mp4", i);
    snprintf(st[i].rotulo, sizeof st[i].rotulo, "F%02d", i);
    st[i].altura = 720;
    st[i].emCache = 0;                          // inconclusiva: toca = -1
  }
  st[28].altura = 2160;
  st[28].emCache = 1;                           // a confirmada 4K
  stream_definir_alvo("tt4000001");
  stream_definir_lista(st, 30);

  idx[0] = 0; idx[1] = 1;
  NV_CHECK(farolcompara_abrir_titulos(idx, 2, NULL, 0) == 1);
  NV_CHECK(farolcompara_qualidade_consultada(0));
  NV_CHECK(farolcompara_tem_eixo(FP_EIXO_QUALIDADE));
  NV_CHECK(farolcompara_celula(0, FP_EIXO_QUALIDADE, cel, sizeof cel));
  NV_CHECK(strstr(cel, "4K"));                     // o 4K da posicao 28 subiu
  farolcompara_fechar();

  // FA_OK (sondagem real) vale sem cache afirmado na fonte.
  memset(st, 0, sizeof st[0] * 2);
  for (i = 0; i < 2; i++) {
    snprintf(st[i].url, sizeof st[i].url, "https://cdn.test/ok%d.mp4", i);
    snprintf(st[i].rotulo, sizeof st[i].rotulo, "OK%d", i);
    st[i].altura = 1080;
    st[i].conferencia = FA_OK;
  }
  stream_definir_alvo("tt4000001");
  stream_definir_lista(st, 2);
  farolcompara_teste_reset();
  NV_CHECK(farolcompara_abrir_titulos(idx, 2, NULL, 0) == 1);
  NV_CHECK(farolcompara_celula(0, FP_EIXO_QUALIDADE, cel, sizeof cel));
  NV_CHECK(strstr(cel, "1080p"));
  farolcompara_fechar();

  stream_invalidar("teste");
  puts("  fontes: 4K alem dos 24 sobe; FA_OK sem cache sustenta a linha");
}

// P2-4: fonte so com idioma (altura desconhecida) mostra o idioma na
// celula, nao uma celula vazia.
static void provaIdioma(void) {
  char cel[FCMP_CEL_TAM];
  montarItens();
  memset(IT[0].fontes, 0, sizeof IT[0].fontes);
  IT[0].fontes[0].toca = 1;
  snprintf(IT[0].fontes[0].audio, sizeof IT[0].fontes[0].audio, "pt");
  IT[0].nFontes = 1;
  reabrir(3, 0);
  NV_CHECK(farolcompara_tem_eixo(FP_EIXO_QUALIDADE));
  NV_CHECK(farolcompara_celula(0, FP_EIXO_QUALIDADE, cel, sizeof cel) && cel[0]);
  montarItens();
  puts("  idioma: fonte sem altura mostra o idioma, nunca celula vazia");
}

// P2-5: trocar de perfil com o Comparar aberto fecha a tela — os itens e
// a memoria de fontes sao da sessao antiga.
static uint64_t gSessao;
static uint64_t sessaoTeste(void) { return gSessao; }

static void provaSessao(void) {
  montarItens();
  gSessao = 41;
  farolcompara_teste_sessao(sessaoTeste);
  reabrir(2, 0);
  NV_CHECK(farolcompara_aberto());
  gSessao = 42;
  quadro();
  NV_CHECK(!farolcompara_aberto());
  farolcompara_teste_sessao(NULL);
  gModo = 1;
  puts("  sessao: troca de perfil fecha o Comparar");
}

// Do estado (coluna, botao), as 4 setas: alcanca tudo, nada prende.
static void provaDpad(int n) {
  int alcancado[4] = { 0 }, i, k;
  char ref[FAROL_REF_TAM];
  int cat;
  montarItens();
  farolcompara_teste_consulta(consultaTeste);
  gModo = 1;

  // Script completo.
  reabrir(n, 0);
  assert(farolcompara_foco() == 1);              // escolha = Beta (maior pontuacao)
  tecla(SDLK_LEFT);  assert(farolcompara_foco() == 0);
  tecla(SDLK_LEFT);  assert(farolcompara_foco() == 0);   // borda: fica
  for (i = 0; i < 5; i++) tecla(SDLK_RIGHT);
  assert(farolcompara_foco() == n - 1);           // borda: fica
  tecla(SDLK_DOWN);  assert(farolcompara_foco_botao());
  tecla(SDLK_DOWN);  assert(farolcompara_foco_botao());
  tecla(SDLK_LEFT);  tecla(SDLK_RIGHT); assert(farolcompara_foco_botao());
  tecla(SDLK_UP);    assert(!farolcompara_foco_botao() && farolcompara_foco() == n - 1);
  tecla(SDLK_UP);    assert(!farolcompara_foco_botao() && farolcompara_foco() == n - 1);
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(!farolcompara_aberto());
  assert(farolcompara_pediu_abrir(&cat, ref, sizeof ref));
  assert(cat == IT[n - 2].catIndice && !strcmp(ref, IT[n - 2].ref));
  assert(!farolcompara_pediu_abrir(&cat, ref, sizeof ref) && !farolcompara_pediu_tocar(&cat, ref, sizeof ref));

  reabrir(n, 0);
  tecla(SDLK_LEFT);
  tecla(SDLK_AUDIOPLAY);
  assert(!farolcompara_aberto());
  assert(farolcompara_pediu_tocar(&cat, ref, sizeof ref) && cat == IT[0].catIndice);
  assert(!farolcompara_pediu_abrir(&cat, ref, sizeof ref));

  reabrir(n, 0);
  tecla(SDLK_DOWN); tecla(SDLK_AUDIOPLAY);       // Play no botao toca a coluna de onde saiu
  assert(farolcompara_pediu_tocar(&cat, ref, sizeof ref) && !strcmp(ref, IT[1].ref));

  reabrir(n, 0);
  tecla(SDLK_ESCAPE);
  assert(!farolcompara_aberto());
  assert(!farolcompara_pediu_abrir(&cat, ref, sizeof ref) && !farolcompara_pediu_tocar(&cat, ref, sizeof ref));

  // Busca exaustiva: de cada estado alcancado, cada seta leva a um estado
  // valido e Voltar sempre fecha (nenhum beco). Estado = coluna ou botao(3).
  {
    static const SDL_Keycode SETAS[4] = { SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN };
    int fila[64][8], nf = 0, ini = 0;
    fila[nf][0] = 0; nf++;                       // caminho vazio
    alcancado[1] = 1;
    while (ini < nf) {
      int *cam = fila[ini++];
      for (k = 0; k < 4; k++) {
        int est;
        reabrir(n, 0);
        for (i = 1; i <= cam[0]; i++) tecla(SETAS[cam[i]]);
        tecla(SETAS[k]);
        assert(farolcompara_aberto());
        est = farolcompara_foco_botao() ? 3 : farolcompara_foco();
        assert(est >= 0 && (est == 3 || est < n));
        if (!alcancado[est] && cam[0] < 6 && nf < 64) {
          alcancado[est] = 1;
          memcpy(fila[nf], cam, sizeof fila[nf]);
          fila[nf][0] = cam[0] + 1;
          fila[nf][cam[0] + 1] = k;
          nf++;
        }
        tecla(SDLK_ESCAPE);
        assert(!farolcompara_aberto());
      }
    }
    for (i = 0; i < n; i++) assert(alcancado[i]);
    assert(alcancado[3]);
  }

  // Sem botao (tudo consultado), v nao cria beco.
  for (i = 0; i < 3; i++) IT[i].nFontes = 0;
  reabrir(n, 0);
  assert(!farolcompara_botao_visivel());
  tecla(SDLK_DOWN); assert(!farolcompara_foco_botao() && farolcompara_foco() == 1);
  tecla(SDLK_UP);   assert(!farolcompara_foco_botao() && farolcompara_foco() == 1);
  tecla(SDLK_RIGHT); assert(farolcompara_foco() == (n == 3 ? 2 : 1));
  farolcompara_fechar();
  montarItens();
  printf("  dpad %d colunas: <- -> v ^ OK Play Voltar sem beco nem laco\n", n);
}

// ------------------------------------------------------------ GL
#ifdef NV_LINUX_DESKTOP
static void salvarPng(const char *path, const unsigned char *rgba, int w, int h) {
  int y, pitch = w * 4;
  unsigned char *flip = malloc((size_t)pitch * (size_t)h);
  SDL_Surface *s;
  assert(flip);
  for (y = 0; y < h; y++)
    memcpy(flip + (size_t)y * (size_t)pitch, rgba + (size_t)(h - 1 - y) * (size_t)pitch,
           (size_t)pitch);
  s = SDL_CreateRGBSurfaceFrom(flip, w, h, 32, pitch, 0x000000ffu, 0x0000ff00u,
                               0x00ff0000u, 0xff000000u);
  assert(s);
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s);
  free(flip);
}

static void capturar(const char *dir, const char *nome, int n, unsigned char *px, int W, int H) {
  char path[1024];
  SDL_Surface *lida;
  int i, claros = 0;
  size_t b;
  montarItens();
  reabrir(n, 100);
  for (i = 0; i < 40; i++) {
    farolcompara_atualizar(0.016f, 2000u + (Uint32)i * 16u);
    txt_novo_quadro();
    gfx_novo_quadro();
    glViewport(0, 0, W, H);
    glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    farolcompara_desenhar(2000u + (Uint32)i * 16u);
    glFinish();
  }
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (b = 0; b < (size_t)W * (size_t)H * 4u; b += 4)
    if (px[b] > 200 && px[b + 1] > 200 && px[b + 2] > 200) claros++;
  assert(claros > 2000);                          // texto desenhado
  snprintf(path, sizeof path, "%s/%s.png", dir, nome);
  salvarPng(path, px, W, H);
  lida = IMG_Load(path);
  assert(lida && lida->w == W && lida->h == H);
  SDL_FreeSurface(lida);
  farolcompara_fechar();
  printf("  captura %s: %dx%d ok\n", nome, W, H);
}

static void provaGl(const char *dir) {
  const int W = 1920, H = 1080;
  SDL_Window *win;
  SDL_GLContext gl;
  unsigned char *px = malloc((size_t)W * (size_t)H * 4u);
  assert(px);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  win = SDL_CreateWindow("farol-49", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  glViewport(0, 0, W, H);
  gfx_tamanho_alvo(W, H);
  assert(gfx_iniciar());
  { char ic[4096]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);
  capturar(dir, "comparar-3", 3, px, W, H);
  capturar(dir, "comparar-2", 2, px, W, H);
  free(px);
  tex_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
}
#endif

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : NULL;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  farolcompara_teste_player(playerTeste);
  farolcompara_teste_relogio(epochTeste);
  farolcompara_teste_caps(1, 1);
  farolcompara_teste_consulta(consultaTeste);
  gModo = 1;

  provaAbertura();
  provaCena();
  provaEixos();
  provaQualidade();
  provaTitulos();
  provaFontesLista();
  provaIdioma();
  provaSessao();
  provaCorridaFim();
  provaDpad(3);
  provaDpad(2);
#ifdef NV_LINUX_DESKTOP
  if (dir) provaGl(dir);
#endif
  (void)dir;
  assert(!gFalhas);
  assert(farolcompara_teste_esperar(3000));
  SDL_Quit();
  puts("farol_compara: ok");
  return 0;
}
