// AS EXIBICOES DA PAGINA DE COLECAO (vertudo.c), sem janela e sem rede.
//
// Pedido do dono (06/10): cada colecao escolhe como se mostra — Grade, Linha do
// tempo, Ranking, Saga, Decadas, Mural. O que este teste cobra:
//   1. o Automatico pelo conteudo (Diretores, ranking, franquia, o resto);
//   2. toda exibicao cabe na largura util da grade e nenhuma celula encosta
//      noutra (a tabela de celulas e o unico lugar que posiciona);
//   3. as linhas sao contiguas e crescentes (a navegacao depende disso);
//   4. a ordem cronologica de Saga/Decadas/Linha (sem ano vai para o fim) e os
//      cabecalhos de decada antes dos titulos deles;
//   5. o D-pad: cima/baixo pelo centro mais proximo no Mural, a esquerda no
//      primeiro da linha pede a barra, cima na primeira linha vai ao botao;
//   6. OK no botao troca a exibicao e a escolha fica gravada POR PERFIL.
//
// Inclui src/vertudo.c (o estado e estatico) e troca a lista que a descoberta
// leu por uma semeada aqui.
#include "gfx.h"
#include "nvtmp.h"
#include "tex_cache.h"
#include "text.h"
TxtLinha shot_text(TxtEstilo style, const char *s, int r, int g, int b, int a, float width);
static int pedidosArte, sombrasFluxo, pedidosPagina;
static GfxRect anelRect; static int anelN; static float anelC[3];   // no acento: o anel da Home
static GLuint fluxo_tex(const char *p, float w) { (void)p; (void)w; pedidosArte++; return 0; }
static void fluxo_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)raio; (void)ca;
  if (r.h > 100 && cr == 0 && cg == 0 && cb == 0) sombrasFluxo++;
  if (cr == anelC[0] && cg == anelC[1] && cb == anelC[2]) { anelRect = r; anelN++; }
}
static void fluxo_rect(GfxRect r, GLuint t, GfxModo m, float f, float x, float y,
                       float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)t; (void)m; (void)f; (void)x; (void)y; (void)raio;
  (void)cr; (void)cg; (void)cb; (void)ca;
}
static void fluxo_recorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
static void fluxo_sem_recorte(void) {}
static void fluxo_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca;
}
#define tex_obter_larg fluxo_tex
#define gfx_cor fluxo_cor
#define gfx_rect fluxo_rect
#define gfx_recorte fluxo_recorte
#define gfx_sem_recorte fluxo_sem_recorte
#define gfx_anel fluxo_anel
#define ajustes_ocultar_filmes_vistos shot_hide_watched
#define desc_filme_visto shot_watched
#define desc_vertudo_carregando shot_loading
#define desc_vertudo_fim shot_end
#define desc_vertudo_erro shot_error
#define desc_vertudo_mais shot_more
#define desc_vertudo_filtro_desde shot_filter_from
#define desc_vertudo_fonte_desde shot_provider_from
#define desc_chave_tmdb shot_tmdb_key
#define nuvem_trakt_cliente shot_trakt_client
#define desc_vertudo_pagina shot_page
#define desc_colecao_inicial shot_collection
#define fundo_desenhar_modo shot_background
#define txt_linha_corta shot_text
#define desc_vertudo_n shot_vt_n
#define desc_vertudo_item shot_vt_item
#define desc_vertudo_n_bruto shot_vt_n
#define desc_vertudo_item_bruto shot_vt_item
#include "../src/vertudo.c"
#undef desc_colecao_inicial
#undef desc_vertudo_n
#undef desc_vertudo_item
#undef desc_vertudo_n_bruto
#undef desc_vertudo_item_bruto
#include <assert.h>
#include "ponteiro.h"
#include "perfis.h"

void shot_background(int mode, GfxRect area, float radius, const char *url, float a) {
  (void)mode; (void)area; (void)radius; (void)url; (void)a;
}
static int hideWatched, allWatched, moreCalls, emptyTexts, exhausted = 1, loading, watchedOlder;
int shot_hide_watched(void) { return hideWatched; }
int shot_watched(const CatItem *it) {
  return !strcmp(it->tipo, "movie") && (allWatched || (watchedOlder && nv_ano_meta(it->meta) < 2020));
}
int shot_loading(void) { return loading; }
int shot_end(void) { return exhausted; }
int shot_error(void) { return 0; }
void shot_more(void) { moreCalls++; pedidosPagina++; loading = 1; }
static int openedSources, resumedPage, openedProviderSources;
static char openedBase[NV_ADDON_URL_MAX], openedType[8], openedId[96], openedGenre[96];
static ColSource openedProvider;
int shot_page(void) { return 300; }
void shot_filter_from(const char *base, const char *type, const char *id,
                      const char *genre, int page) {
  snprintf(openedBase, sizeof openedBase, "%s", base);
  snprintf(openedType, sizeof openedType, "%s", type);
  snprintf(openedId, sizeof openedId, "%s", id);
  snprintf(openedGenre, sizeof openedGenre, "%s", genre);
  openedSources++; resumedPage = page; loading = 1;
}
void shot_provider_from(const ColSource *s, int page) {
  assert(s);
  openedProvider = *s; openedProviderSources++; resumedPage = page; loading = 1;
}
const char *shot_tmdb_key(void) { return "collection-test-key"; }
const char *shot_trakt_client(void) { return "collection-test-client"; }
#undef txt_linha_corta
TxtLinha shot_text(TxtEstilo style, const char *s, int r, int g, int b, int a, float width) {
  if (!strcmp(s, i18n("Nenhum título nesta lista."))) emptyTexts++;
  return txt_linha_corta(style, s, r, g, b, a, width);
}

static CatItem semente[301];
static int nSemente;
int shot_vt_n(void) { return nSemente; }
int shot_vt_item(int i, CatItem *dst) {
  if (i < 0 || i >= nSemente) return 0;
  if (dst) *dst = semente[i];
  return 1;
}

// Each request receives an immutable page. Completion gates test the actual
// scheduler without sharing the singleton page or mutable seed across workers.
static struct {
  CatItem itens[2 * VIT_ITENS];
  int n, mais, pagina, iniciou, liberada;
} shotSources[2];
static pthread_mutex_t shotMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t shotCond = PTHREAD_COND_INITIALIZER;
static int initialSources;
static Uint32 shotNow = 900;
static void shot_prepare(int r) {
  pthread_mutex_lock(&shotMutex);
  memset(&shotSources[r], 0, sizeof shotSources[r]);
  shotSources[r].n = nSemente < 2 * VIT_ITENS ? nSemente : 2 * VIT_ITENS;
  memcpy(shotSources[r].itens, semente, sizeof(CatItem) * (size_t)shotSources[r].n);
  shotSources[r].mais = !exhausted;
  shotSources[r].pagina = 300;
  pthread_mutex_unlock(&shotMutex);
}
int shot_collection(const ColSource *source, const char *base, NvPerfilSessao epoch,
                    CatItem *out, int limit, DescColecaoResultado *result) {
  assert(source && base && *base && limit > 0 && out && result);
  int r = !strcmp(source->catId, "source1");
  pthread_mutex_lock(&shotMutex);
  shotSources[r].iniciou = 1; initialSources++;
  pthread_cond_broadcast(&shotCond);
  while (!shotSources[r].liberada) pthread_cond_wait(&shotCond, &shotMutex);
  int n = shotSources[r].n < limit ? shotSources[r].n : limit;
  memcpy(out, shotSources[r].itens, sizeof(CatItem) * (size_t)n);
  if (!strncmp(source->catId, "catalog", 7) && n)
    snprintf(out[0].imdb, sizeof out[0].imdb, "tt%d", 8000000 + atoi(source->catId + 7));
  *result = (DescColecaoResultado){.n=n, .pagina=shotSources[r].pagina,
                                  .mais=shotSources[r].mais};
  pthread_mutex_unlock(&shotMutex);
  return nv_perfil_sessao_atual(epoch);
}
static void shot_release(int r) {
  pthread_mutex_lock(&shotMutex); shotSources[r].liberada = 1;
  pthread_cond_broadcast(&shotCond); pthread_mutex_unlock(&shotMutex);
}
static void shot_pump(unsigned mask, int started) {
  Uint32 end = SDL_GetTicks() + 3000;
  for (;;) {
    vitCarregar(shotNow += 16); vitProjetar();
    unsigned ready = 0;
    for (int r = 0; r < 2; r++) {
      if (started) {
        pthread_mutex_lock(&shotMutex);
        if (shotSources[r].iniciou) ready |= 1u << r;
        pthread_mutex_unlock(&shotMutex);
      } else if (vitLin[r].estado == VIT_PRONTA || vitLin[r].estado == VIT_VAZIA)
        ready |= 1u << r;
    }
    if ((ready & mask) == mask) return;
    assert((Sint32)(end - SDL_GetTicks()) > 0); usleep(1000);
  }
}

static const int ANOS[] = { 2005, 1977, 1999, 2015, 1980, 0, 2002, 1983,
                            2017, 2019, 2008, 1997, 2016, 1977 };
#define NANOS (int)(sizeof ANOS / sizeof *ANOS)

static void semear(void) {
  int i;
  nSemente = NANOS;
  for (i = 0; i < nSemente; i++) {
    CatItem *c = &semente[i];
    memset(c, 0, sizeof *c);
    snprintf(c->imdb, sizeof c->imdb, "tt70%05d", i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "Capítulo %d", i);
    if (ANOS[i]) snprintf(c->meta, sizeof c->meta, "%d · 2 h", ANOS[i]);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  vertudo_evento(&e);
}
// O seletor recolhido: OK abre o trilho, a seta anda uma exibicao, OK escolhe.
static void trocar(void) { tecla(SDLK_RETURN); tecla(SDLK_RIGHT); tecla(SDLK_RETURN); }

static int sobrepoe(int a, int b) {
  return celX[a] < celX[b] + celW[b] - 0.5f && celX[b] < celX[a] + celW[a] - 0.5f &&
         celY[a] < celY[b] + celH[b] - 0.5f && celY[b] < celY[a] + celH[a] - 0.5f;
}

static ColFolder pasta;
static ColSource fontes[2];

// #549: a ultima coluna e uma acao, fora dos buffers e indices dos titulos.
static void verTudoPreparar(int limite, int provedor) {
  vitCancelar();
  free(vitBuf);
  vitBuf = calloc((size_t)VIT_LIN_MAX * VIT_ITENS, sizeof *vitBuf);
  assert(vitBuf);
  vitCap = limite;
  memset(&pasta, 0, sizeof pasta); memset(fontes, 0, sizeof fontes);
  snprintf(pasta.id, sizeof pasta.id, "showcase549");
  snprintf(pasta.title, sizeof pasta.title, "Showcase");
  snprintf(pasta.group, sizeof pasta.group, "Film Collections");
  pasta.sources = fontes; pasta.nSources = 2;
  for (int r = 0; r < 2; r++) {
    snprintf(fontes[r].title, sizeof fontes[r].title, "Popular");
    snprintf(fontes[r].base, sizeof fontes[r].base, "https://source.invalid/%d", r);
    snprintf(fontes[r].addonId, sizeof fontes[r].addonId, "addon%d", r);
    snprintf(fontes[r].type, sizeof fontes[r].type, "movie");
    snprintf(fontes[r].catId, sizeof fontes[r].catId, "source%d", r);
    snprintf(fontes[r].genre, sizeof fontes[r].genre, "Science Fiction");
  }
  if (provedor) {
    fontes[1].base[0] = 0;
    snprintf(fontes[1].prov, sizeof fontes[1].prov, "tmdb");
    snprintf(fontes[1].tmdbTipo, sizeof fontes[1].tmdbTipo, "COMPANY");
    fontes[1].tmdbId = 42; fontes[1].traktLista = 1234;
    snprintf(fontes[1].midia, sizeof fontes[1].midia, "MOVIE");
    snprintf(fontes[1].ordenar, sizeof fontes[1].ordenar, "popularity.desc");
    snprintf(fontes[1].ordem, sizeof fontes[1].ordem, "desc");
    snprintf(fontes[1].filtros, sizeof fontes[1].filtros, "{\"language\":\"en\",\"year\":2025}");
  }
  collection = &pasta; source = 0; aberta = 1; tabFocus = 0;
  lay = layEscolha = VT_LAY_VITRINE; semVitrineAuto = 0;
  hideWatched = allWatched = watchedOlder = semFonte = loading = 0;
  exhausted = 1; ddAberto = exibAberta = 0; pedAbrir = -1;
  vitIniciar();
  assert(!strcmp(vitLin[0].nome, vitLin[1].nome));   // voltar usa a identidade, nao este rotulo repetido
  for (int r = 0; r < 2; r++) {
    vitLin[r].n = vitLin[r].brutoN = limite; vitLin[r].estado = VIT_PRONTA;
    vitLin[r].mais = 1;
    for (int c = 0; c < limite; c++) {
      CatItem *it = &vitBuf[r * vitCap + c];
      memset(it, 0, sizeof *it);
      snprintf(it->imdb, sizeof it->imdb, "tt549%d%02d", r, c);
      snprintf(it->tipo, sizeof it->tipo, "movie");
      snprintf(it->titulo, sizeof it->titulo, "Title %d/%d", r, c);
      vitVis[r][c] = (short)c;
    }
  }
  vitCarga = vitN; vitOcultar = hideWatched; vitProjetoSujo = 0;
}

static void vitrineVerTudo(void) {
  for (int fraca = 0; fraca < 2; fraca++) {
    int limite = fraca ? VIT_ITENS_FRACA : VIT_ITENS;
    verTudoPreparar(limite, 0);
    gravarEscolha(&pasta, VT_LAY_VITRINE);
    char *antes = dados_ler(arquivoEscolha()); assert(antes);
    assert(vitTemVerTudo(1) && vitColunas(1) == limite + 1);
    assert(vitItem(1, limite) == NULL);   // a acao nao e um titulo no buffer
    vitFR = 1; vitFC = limite - 1; tabFocus = 0;
    tecla(SDLK_RIGHT); assert(vitFC == limite);
    tecla(SDLK_RIGHT); assert(vitFC == limite);   // sem atravessar a borda
    int pedidos = openedSources;
    tecla(SDLK_RETURN);
    assert(lay == VT_LAY_GRADE && source == 0 && pedAbrir == -1);
    assert(collection->nSources == 1 && !memcmp(collection->sources, &fontes[1], sizeof fontes[1]));
    SDL_Event repetiu = {0}; repetiu.type = SDL_KEYDOWN; repetiu.key.keysym.sym = SDLK_RETURN;
    repetiu.key.repeat = 1; vertudo_evento(&repetiu);
    repetiu.key.repeat = 0; vertudo_evento(&repetiu);
    assert(pedAbrir == -1 && lay == VT_LAY_GRADE);   // segurar OK nao abre o primeiro titulo
    repetiu.type = SDL_KEYUP; vertudo_evento(&repetiu);
    assert(openedSources == pedidos + 1 && !strcmp(openedBase, fontes[1].base));
    assert(!strcmp(openedType, fontes[1].type) && !strcmp(openedId, fontes[1].catId));
    assert(!strcmp(openedGenre, fontes[1].genre));
    assert(escolhaGravada(&pasta) == VT_LAY_VITRINE);
    tecla(SDLK_ESCAPE);
    assert(aberta && lay == VT_LAY_VITRINE && vitFR == 1 && vitFC == limite);
    assert(openedSources == pedidos + 1);   // voltar conserva a previa recebida
    char *depois = dados_ler(arquivoEscolha()); assert(depois && !strcmp(antes, depois));
    free(antes); free(depois);

    // O endereco da fonte completa inclui o provedor e todos os seus filtros.
    verTudoPreparar(limite, 1);
    ColSource esperada = fontes[1]; int pedidosProv = openedProviderSources;
    vitAbrirTudo(1);
    assert(lay == VT_LAY_GRADE && source == 0 && pedAbrir == -1);
    assert(openedProviderSources == pedidosProv + 1 && !memcmp(&openedProvider, &esperada, sizeof esperada));
    escolherExibicao(VT_LAY_MURAL);
    assert(lay == VT_LAY_MURAL && escolhaGravada(&pasta) == VT_LAY_VITRINE);
    escolherExibicao(VT_LAY_VITRINE);   // volta ao pai, nao abre outra Vitrine dentro dele
    assert(lay == VT_LAY_VITRINE && aberta);
  }

  // Uma fonte: a fileira e um grupo, nao o indice de uma fonte inexistente.
  for (int tipos = 0; tipos < 2; tipos++) {
    verTudoPreparar(VIT_ITENS, 0);
    pasta.nSources = 1; vitIniciar(); loading = 0;
    nSemente = 80;
    for (int i = 0; i < nSemente; i++) {
      memset(&semente[i], 0, sizeof semente[i]);
      snprintf(semente[i].imdb, sizeof semente[i].imdb, "tt549-group-%d", i);
      snprintf(semente[i].tipo, sizeof semente[i].tipo, "%s", tipos && (i & 1) ? "series" : "movie");
      snprintf(semente[i].meta, sizeof semente[i].meta, "%d", i & 1 ? 2024 : 2015);
    }
    vitAgrupar(nSemente); vitProjetar();
    assert(vitN == 2 && vitLin[1].n == VIT_ITENS && vitTemVerTudo(1));
    vitFR = 1; vitFC = vitLin[1].n; tabFocus = 0;
    vitAbrirTudo(1);
    assert(lay == VT_LAY_GRADE && source == 0 && nItens() == 40 && pedAbrir == -1);
    for (int i = 0; i < 40; i++) {
      CatItem it;
      assert(viewItem(i, &it));
      // Decada 2010 = indices pares; Series = impares. A ordem da fonte
      // continua intacta, inclusive para os titulos alem da previa de 20.
      assert(!strcmp(it.imdb, semente[i * 2 + tipos].imdb));
    }
    tecla(SDLK_ESCAPE);
    assert(aberta && lay == VT_LAY_VITRINE && vitFR == 1 && vitFC == VIT_ITENS);
  }
  verTudoPreparar(VIT_ITENS, 0);
  int pedidos = openedSources;
  snprintf(fontes[1].genre, sizeof fontes[1].genre, "Changed after rendering");
  assert(!vitAbrirTudo(1) && openedSources == pedidos && lay == VT_LAY_VITRINE);

  // The real collection store replaces/frees its source block while grid is open.
  verTudoPreparar(VIT_ITENS, 0);
  assert(col_extra_definir(&pasta, 1) == 1);
  collection = col_folder(col_n() - 1); assert(collection);
  vitColecoesRev = col_revisao();
  assert(vitAbrirTudo(1));
  ColFolder changed = pasta; changed.nSources = 1;
  snprintf(changed.title, sizeof changed.title, "Updated Showcase");
  assert(col_extra_definir(&changed, 1) == 1);
  assert(collection == &vitTudoPasta && !strcmp(collection->sources[0].catId, "source1"));
  tecla(SDLK_ESCAPE);
  assert(aberta && !vitTudo && collection->nSources == 1);
  assert(!strcmp(titulo, "Updated Showcase") && source == 0);
  assert(vitN == 0 && vitLin[1].n == 0); // replaced sources cannot retain old rows

  verTudoPreparar(VIT_ITENS, 0);
  assert(col_extra_definir(&pasta, 1) == 1);
  collection = col_folder(col_n() - 1); vitColecoesRev = col_revisao();
  assert(vitAbrirTudo(1));
  assert(col_extra_definir(NULL, 0) == 0);
  tecla(SDLK_ESCAPE);
  assert(!aberta && !vitTudo);

  verTudoPreparar(VIT_ITENS, 0);
  assert(vitAbrirTudo(1));
  perfis_definir_ativo(perfis_ativo() == 1 ? 2 : 1);
  vertudo_atualizar(1.0f / 60.0f, 30000);
  assert(!aberta && !vitTudo);
  puts("See all: Back, collection replacement/removal and profile switch pass");
}

static void vitrineTvPonteiro(void) {
  const PonteiroAlvo *alvos = NULL;
  int nAlvos, k, ok = 0;
  float w, h, passo, rot, x0 = 100.0f, topo = vitTopo();
  pasta.nSources = 2;
  lay = VT_LAY_VITRINE; layEscolha = VT_LAY_VITRINE; tabFocus = 0;
  vitIniciar();
  vitLin[0].n = 5; vitLin[0].brutoN = 5; vitLin[0].estado = VIT_PRONTA;
  for (int i = 0; i < 5; i++) { vitBuf[i] = semente[i]; vitVis[0][i] = (short)i; }
  home_vitrine_medida(&w, &h, &passo, &rot, vitRedonda());
  ponteiro_teste_toque(1);
  ponteiro_quadro(2000);
  for (int c = 0; c < vitColunas(0); c++) {
    float cx = x0 + c * passo - vitSx[0];
    GfxRect base = { cx, topo + VIT_TITULO_H, w, h };
    float left = fmaxf(base.x, x0), right = fminf(base.x + base.w, NV_TELA_W);
    vitRegistraAlvoTv(0, c, left, right, base.y, base.y + base.h, topo - 8.0f);
  }
  ponteiro_desenhar();
  nAlvos = ponteiro_teste_lista(&alvos);
  assert(alvos && nAlvos > 0);
  for (k = 0; k < nAlvos; k++)
    if (alvos[k].a / VIT_ITENS == 0 && alvos[k].a % VIT_ITENS == 2 && alvos[k].focar) {
      vitFR = 1; vitFC = 0; tabFocus = 1;
      alvos[k].focar(alvos[k].a, alvos[k].b);
      assert(vitFR == 0 && vitFC == 2 && !tabFocus);
      ok = 1;
    }
  assert(ok);
  ponteiro_teste_toque(0);
  puts("vitrine_tv_ponteiro: ok");
}

static void vitrineAllCatalogs(void);
int main(int argc, char **argv) {
  int L, i, j;
  char dir[600];
  snprintf(dir, sizeof dir, "%s/nv-colexib.XXXXXX", nvt_dir());
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar("deploy/app/art");
  semear();
  pasta.sources = fontes;
  pasta.nSources = 1;
  snprintf(pasta.id, sizeof pasta.id, "starwars");
  snprintf(pasta.groupId, sizeof pasta.groupId, "franquias");
  snprintf(fontes[0].title, sizeof fontes[0].title, "Star Wars");
  collection = &pasta;
  aberta = 1;

  if (argc > 1 && !strcmp(argv[1], "catalogs")) { vitrineAllCatalogs(); return 0; }
  if (argc > 1 && !strcmp(argv[1], "tvptr")) { vitrineTvPonteiro(); return 0; }

  // 1. AUTOMATICO = exatamente o que a 2.0.3 mostra: Grade para tudo, so o
  // Diretor na Linha do tempo (a 2.0.3 ja fazia). Ranking, Saga, Decadas, Mural
  // e Vitrine existem, mas SO pelo botao "Exibição" (opt-in).
  { static const char *GRUPOS[] = { "Film Collections", "TV Collections", "Awards",
                                    "Studios", "Genres", "" };
    static const char *TITULOS[] = { "Star Wars", "Trilogia do Anel", "Saga Marvel",
                                     "Franquia X", "Top 100", "Franchise" };
    int g, t, ab, r, sv;
    for (g = 0; g < 6; g++) for (t = 0; t < 6; t++)
      for (ab = 1; ab <= 3; ab++) for (r = 0; r < 2; r++) for (sv = 0; sv < 2; sv++) {
        snprintf(pasta.group, sizeof pasta.group, "%s", GRUPOS[g]);
        snprintf(pasta.title, sizeof pasta.title, "%s", TITULOS[t]);
        pasta.nSources = ab; ranked = r; semVitrineAuto = sv;
        assert(layAuto() == VT_LAY_GRADE);
        snprintf(pasta.group, sizeof pasta.group, "Directors");
        assert(layAuto() == VT_LAY_LINHA);
      } }
  ranked = 0; semVitrineAuto = 0; pasta.nSources = 1;
  snprintf(pasta.title, sizeof pasta.title, "Star Wars");
  snprintf(pasta.group, sizeof pasta.group, "Film Collections");
  collection = NULL; assert(layAuto() == VT_LAY_GRADE);
  collection = &pasta;

  // 2-4. GEOMETRIA de cada exibicao
  for (L = VT_LAY_GRADE; L < VT_LAY_VITRINE; L++) {
    lay = L; celLay = -1; orderN = -1; foco = 0;
    montar(nSemente);
    assert(celN == nSemente);
    for (i = 0; i < nSemente; i++) {
      assert(celX[i] >= 0.0f && celY[i] >= 0.0f);
      float width = L == VT_LAY_MURAL || L == VT_LAY_LINHA ? vtFim() - vtInicio() : VT_CW;
      assert(celX[i] + celW[i] <= width + 30.0f);   // ranking passes by at most 16 px
      if (i) assert(celLin[i] >= celLin[i - 1] && celLin[i] <= celLin[i - 1] + 1);
      for (j = 0; j < i; j++) if (sobrepoe(i, j)) {
        fprintf(stderr, "exibicao %d: celulas %d e %d se sobrepoem\n", L, i, j);
        return 1;
      }
    }
    if (ordenaPorAno()) {
      int ant = 0;
      CatItem it;
      for (i = 0; i < nSemente; i++) {
        int y; assert(viewItem(i, &it)); y = yearOf(&it);
        assert(y >= ant); ant = y;
      }
      assert(viewItem(nSemente - 1, &it) && !it.meta[0]);   // sem ano: no fim
    }
    printf("exibicao %-15s ok: %d celulas, %d linhas, altura %.0f\n",
           LAY_NOME[L], celN, celLin[celN - 1] + 1, celFim);
  }
  // Decadas: 1970, 1980, 1990, 2000, 2010 e "sem data" = 6 secoes, na ordem,
  // cada cabecalho acima do primeiro titulo dela.
  lay = VT_LAY_DECADAS; celLay = -1; orderN = -1; montar(nSemente);
  assert(decN == 6);
  assert(decAno[0] == 1970 && decAno[4] == 2010 && decAno[5] == 0);
  assert(decQtd[0] == 2 && decQtd[1] == 2 && decQtd[5] == 1);
  for (i = 1; i < decN; i++) assert(decY[i] > decY[i - 1]);
  assert(celY[0] == decY[0] + VT_DEC_H);

  // 5. D-PAD no Mural: o grande (0) desce para o pequeno de baixo mais perto.
  lay = VT_LAY_MURAL; celLay = -1; orderN = -1; montar(nSemente);
  tabFocus = 0; foco = 0;
  tecla(SDLK_DOWN); assert(foco == 3);           // primeiro pequeno de baixo
  tecla(SDLK_UP);   assert(foco == 1);           // o pequeno de cima, mesmo x
  tecla(SDLK_LEFT); assert(foco == 0);
  pedMenu = 0;
  tecla(SDLK_LEFT); assert(pedMenu == 1 && foco == 0);   // primeiro da linha
  foco = 2; tecla(SDLK_DOWN); assert(foco == 4);
  tecla(SDLK_DOWN); assert(celLin[foco] == 2);   // bloco seguinte, espelhado
  // Cima na primeira linha: a faixa do botao "Exibição".
  foco = 1; tecla(SDLK_UP); assert(tabFocus == 1 && tabCursor == nAbas());

  // 6. OK NO BOTAO troca e grava; outro perfil nao herda.
  salvos_perfil(1);
  layEscolha = VT_LAY_AUTO; lay = VT_LAY_GRADE;
  trocar();
  assert(lay == VT_LAY_LINHA && layEscolha == VT_LAY_LINHA);
  trocar(); assert(lay == VT_LAY_FLUXO);   // a horizontal logo depois
  trocar(); trocar();
  assert(lay == VT_LAY_SAGA);
  assert(escolhaGravada(&pasta) == VT_LAY_SAGA);
  { ColFolder outra = pasta; snprintf(outra.id, sizeof outra.id, "outra");
    assert(escolhaGravada(&outra) == VT_LAY_AUTO); }
  salvos_perfil(2);
  assert(escolhaGravada(&pasta) == VT_LAY_AUTO);
  gravarEscolha(&pasta, VT_LAY_MURAL);
  assert(escolhaGravada(&pasta) == VT_LAY_MURAL);
  salvos_perfil(1);
  assert(escolhaGravada(&pasta) == VT_LAY_SAGA);
  // Depois do Mural vem a Vitrine, e depois dela volta ao comeco (Grade).
  lay = VT_LAY_MURAL; layEscolha = VT_LAY_MURAL; tabFocus = 1; tabCursor = nAbas();
  trocar(); assert(lay == VT_LAY_VITRINE && tabFocus == 1);
  assert(nAbas() == 0 && tabCursor == 0);
  tecla(SDLK_RETURN); for (int i = 0; i < 7; i++) tecla(SDLK_LEFT);   // a Grade e a primeira do trilho
  tecla(SDLK_RETURN); assert(lay == VT_LAY_GRADE);

  // Default is a distinct selector step, regardless of the inherited layout.
  { static const int ciclo[] = {
      VT_LAY_GRADE, VT_LAY_LINHA, VT_LAY_FLUXO, VT_LAY_RANKING, VT_LAY_SAGA,
      VT_LAY_DECADAS, VT_LAY_MURAL, VT_LAY_VITRINE, VT_LAY_AUTO
    };
    for (int padrao = VT_LAY_N - 1; padrao >= VT_LAY_AUTO; padrao--) {
      char ajuste[64];
      snprintf(ajuste, sizeof ajuste, "colecaoExibicaoPadraoLocal %d\n", padrao);
      dados_gravar("ajustes.txt", ajuste);
      ajustes_dir(dir);
      assert(ajustes_colecao_exibicao() == padrao);
      layEscolha = VT_LAY_AUTO;
      resolverLayout();
      for (int passo = 0; passo < (int)(sizeof ciclo / sizeof *ciclo); passo++) {
        alternarExibicao();
        assert(layEscolha == ciclo[passo]);
      }
    }
  }

  // 7. VITRINE: fileiras semeadas direto (a fila de abas precisa de rede).
  pasta.nSources = 2;
  lay = VT_LAY_VITRINE; layEscolha = VT_LAY_VITRINE; tabFocus = 0;
  vitIniciar();
  assert(vitBuf && vitMulti && vitN == 2);
  vitLin[0].n = 5; vitLin[0].brutoN = 5; vitLin[0].estado = VIT_PRONTA;
  vitLin[1].n = 0; vitLin[1].estado = VIT_VAZIA;
  for (i = 0; i < 5; i++) { vitBuf[i] = semente[i]; vitVis[0][i] = (short)i; }
  vitCarga = vitN;
  // A fileira vazia e pulada; cima na primeira vai ao botao; esquerda na
  // primeira coluna pede a barra; direita alcanca a acao depois do ultimo titulo.
  vitFR = 0; vitFC = 0; vitMexeu = 1;
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); assert(vitFC == 2);
  for (i = 0; i < 6; i++) tecla(SDLK_RIGHT);
  assert(vitFC == 5 && vitItem(0, vitFC) == NULL);
  tecla(SDLK_DOWN); assert(vitFR == 0 && vitFC == 5);
  pedMenu = 0; vitFC = 0; tecla(SDLK_LEFT); assert(pedMenu == 1);
  tecla(SDLK_UP); assert(tabFocus == 1 && tabCursor == nAbas());
  tecla(SDLK_DOWN); assert(tabFocus == 0);
  // Segunda fileira com itens: baixo desce lembrando a coluna de cada uma.
  vitLin[1].brutoN = 3; vitLin[1].estado = VIT_PRONTA;
  vitProjetoSujo = 1;
  for (i = 0; i < 3; i++) vitBuf[vitCap + i] = semente[7 + i];
  vitProjetar();
  vitFR = 0; vitFC = 4;
  tecla(SDLK_DOWN); assert(vitFR == 1 && vitFC == 0);   // cada fileira lembra a sua coluna
  tecla(SDLK_RIGHT);
  tecla(SDLK_UP); assert(vitFR == 0 && vitFC == 4);
  // O destaque segue o foco depois do repouso.
  vitAtualizar(0.016f, 1000); vitAtualizar(0.016f, 1300);
  assert(vitHeroAtual == 0 * vitCap + 4);
  tecla(SDLK_RETURN);   // sem catalogo global neste teste: so nao pode cair
  // Sem abas (uma fonte so) as fileiras saem do catalogo: tipos misturados
  // viram Filmes e Series; so filmes, decadas da mais nova a mais velha.
  pasta.nSources = 1; vitIniciar();
  assert(!vitMulti);
  vitAgrupar(nSemente); vitProjetar();   // a Vitrine projeta no desenho; aqui, na mao
  assert(vitN == 6 && vitLin[0].n >= 1 && strstr(vitLin[0].nome, "2010"));
  assert(!strcmp(vitLin[vitN - 1].nome, i18n("Sem data")));
  snprintf(semente[1].tipo, sizeof semente[1].tipo, "series");
  vitAgrupar(nSemente); vitProjetar();
  assert(vitN == 2 && !strcmp(vitLin[0].nome, i18n("Filmes")) && !strcmp(vitLin[1].nome, i18n("Séries")));

  // Completed rows filtered to zero must draw the empty message.
  pasta.nSources = 2; vitIniciar();
  semear();
  hideWatched = allWatched = exhausted = 1; semFonte = loading = 0;
  vitGuardarPagina(0); vitProjetar();
  assert(vitLin[0].brutoN > 0 && vitLin[0].n == 0);
  exhausted = 1;
  vitDesenhar(1.0f, 100.0f);
  assert(emptyTexts == 1);

  // Start both sources while the first is blocked: watched row zero cannot
  // starve a ready second row, and the canonical order remains unchanged.
  snprintf(fontes[0].base, sizeof fontes[0].base, "https://unused.invalid/first");
  snprintf(fontes[1].base, sizeof fontes[1].base, "https://unused.invalid/second");
  for (i = 0; i < 2; i++) {
    snprintf(fontes[i].type, sizeof fontes[i].type, "movie");
    snprintf(fontes[i].catId, sizeof fontes[i].catId, "source%d", i);
  }
  semear(); exhausted = moreCalls = openedSources = initialSources = 0;
  shot_prepare(0);
  snprintf(semente[0].tipo, sizeof semente[0].tipo, "series"); shot_prepare(1);
  snprintf(semente[0].tipo, sizeof semente[0].tipo, "movie");
  vitIniciar(); shot_pump(3, 1);
  assert(initialSources == 2 && openedSources == 0);
  shot_release(1); shot_pump(2, 0);
  assert(vitLin[0].estado == VIT_CARREGANDO && !vitLin[0].n && vitLin[1].n == 1);
  shot_release(0); shot_pump(3, 0);
  assert(vitLin[1].n == 1 && vitCarga == vitN);
  vitFR = 0; vitFC = 0; tabFocus = 0;
  tecla(SDLK_DOWN); assert(vitFR == 1);
  tecla(SDLK_UP); assert(vitFR == 0);
  tecla(SDLK_RETURN); vitCarregar(1025);
  assert(source == 0 && openedSources == 1 && resumedPage == 300 && loading);
  loading = 0;
  snprintf(semente[0].imdb, sizeof semente[0].imdb, "tt-resumed");
  snprintf(semente[0].tipo, sizeof semente[0].tipo, "series");
  vitCarregar(1030); vitProjetar();
  assert(vitLin[0].n == 1 && vitLin[1].n == 1);
  assert(!strcmp(vitItem(0, 0)->imdb, "tt-resumed"));
  hideWatched = 0; vitProjetar();
  assert(vitLin[0].n == NANOS && !strcmp(vitItem(0, 0)->imdb, "tt7000000"));
  hideWatched = 1;

  // A bounded all-watched page stays available for explicit pagination.
  semear(); allWatched = 1; openedSources = 0;
  shot_prepare(0); shot_prepare(1); vitIniciar();
  shot_pump(3, 1); shot_release(0); shot_pump(1, 0);
  assert(vitCarga == 1 && temMaisVistos() && moreCalls == 0);
  tabFocus = 1;
  tecla(SDLK_DOWN); assert(!tabFocus && vitFR == 0);
  tecla(SDLK_RETURN); vitCarregar(1070);
  assert(openedSources == 1 && resumedPage == 300 && loading && vitCarga == 0);
  allWatched = loading = 0;
  vitCarregar(1100); vitProjetar();
  assert(vitLin[0].n > 0);
  shot_release(1); shot_pump(3, 0);

  // Single-source grouped rows must expose explicit continuation after 300 watched titles.
  pasta.nSources = 1; vitIniciar();
  semear();
  for (i = 1; i < 301; i++) {
    semente[i] = semente[0];
    snprintf(semente[i].imdb, sizeof semente[i].imdb, "tt-single-%d", i);
  }
  nSemente = 300;
  hideWatched = allWatched = 1; exhausted = loading = moreCalls = 0;
  vitCarregar(1200); vitProjetar();
  assert(vitLin[0].n == 0 && temMaisVistos() && moreCalls == 0);
  tabFocus = 1;
  tecla(SDLK_DOWN); assert(!tabFocus && vitFR == 0);
  tecla(SDLK_RETURN); assert(moreCalls == 1 && loading);
  vitCarregar(1210); assert(moreCalls == 1 && !temMaisVistos());
  snprintf(semente[300].tipo, sizeof semente[300].tipo, "series");
  nSemente = 301; loading = 0; exhausted = 1;
  vitCarregar(1220); vitProjetar();
  assert(vitLin[1].n == 1 && !temMaisVistos());

  // A populated newest decade must not block an empty older decade's button.
  semear();
  for (i = nSemente; i < 301; i++) semente[i] = semente[0];
  snprintf(semente[0].meta, sizeof semente[0].meta, "2025");
  nSemente = 300; allWatched = exhausted = loading = moreCalls = 0; watchedOlder = 1;
  vitIniciar(); loading = 0;
  vitCarregar(1300); vitProjetar();
  assert(vitLin[0].n == 1 && vitLin[1].n == 0 && vitLin[1].mais);
  vitFR = 0; vitFC = tabFocus = 0;
  tecla(SDLK_DOWN); assert(vitFR == 1);
  tecla(SDLK_RETURN); assert(moreCalls == 1 && loading);
  vitCarregar(1310); assert(!temMaisVistos() && moreCalls == 1);
  snprintf(semente[300].meta, sizeof semente[300].meta, "2015");
  watchedOlder = loading = 0; exhausted = 1; nSemente = 301;
  vitCarregar(1320); vitProjetar();
  assert(vitLin[1].n > 0 && !temMaisVistos());

  // An empty non-first source can resume even with watched filtering off.
  pasta.nSources = 2; hideWatched = allWatched = exhausted = loading = moreCalls = 0;
  semear(); shot_prepare(0); nSemente = 0; shot_prepare(1);
  vitIniciar(); shot_release(0); shot_release(1); shot_pump(3, 0);
  assert(vitLin[1].estado == VIT_VAZIA && vitLin[1].mais && temMaisVistos());
  openedSources = 0; vitFR = 1; vitFC = tabFocus = 0;
  tecla(SDLK_RETURN); vitCarregar(4000);
  assert(openedSources == 1 && resumedPage == 300 && loading);
  semear(); loading = 0;
  vitCarregar(4010); vitProjetar();
  assert(vitLin[1].n == NANOS);

  // Resuming a filtered non-first source preserves its original titles.
  semear(); hideWatched = allWatched = 1; moreCalls = openedSources = 0;
  shot_prepare(0); shot_prepare(1); vitIniciar();
  shot_release(0); shot_release(1); shot_pump(3, 0);
  assert(!vitLin[1].n && vitLin[1].mais);
  source = 0; vitFR = 1; vitFC = tabFocus = 0;
  tecla(SDLK_RETURN); vitCarregar(5010);
  assert(source == 1 && openedSources == 1 && resumedPage == 300 && loading);
  snprintf(semente[0].imdb, sizeof semente[0].imdb, "tt-later-source");
  snprintf(semente[0].tipo, sizeof semente[0].tipo, "series");
  loading = 0; vitCarregar(5020); vitProjetar();
  assert(vitLin[1].n == 1 && !strcmp(vitItem(1, 0)->imdb, "tt-later-source"));
  hideWatched = 0; vitProjetar();
  assert(vitLin[1].n == NANOS && !strcmp(vitItem(1, 0)->imdb, "tt7000000"));
  hideWatched = 1; allWatched = 0; vitProjetoSujo = 1; vitProjetar();
  assert(vitLin[1].n > 0 && !strcmp(vitItem(1, 0)->imdb, "tt7000000"));
  // Restore the state the sections below expect (one source, semente[1] a series).
  semear(); snprintf(semente[1].tipo, sizeof semente[1].tipo, "series");
  hideWatched = allWatched = watchedOlder = loading = moreCalls = 0; exhausted = 1;
  fontes[0].base[0] = fontes[1].base[0] = 0;
  pasta.nSources = 1; source = 0; vitIniciar();
  vitAgrupar(nSemente); vitProjetar();
  assert(vitN == 2);

  // 8. STREAMING: sem a legenda de depuracao quando a lista esta pronta, e as
  // abas esmaecem antes da borda em vez de serem cortadas.
  nSemente = NANOS; semFonte = 0;
  snprintf(pasta.group, sizeof pasta.group, "Streaming");
  assert(legendaOculta());
  snprintf(pasta.group, sizeof pasta.group, "Film Collections");
  assert(!legendaOculta());
  snprintf(pasta.group, sizeof pasta.group, "Streaming");
  semFonte = 1; assert(!legendaOculta()); semFonte = 0;
  assert(abaFade(700, 100, 100, 800, 0, 1) == 1.0f);      // longe da borda
  assert(abaFade(870, 100, 100, 800, 0, 1) > 0.0f);      // fresta entrando: visivel, esmaecida
  assert(abaFade(870, 100, 100, 800, 0, 1) < 1.0f);      // dentro da zona
  assert(abaFade(950, 100, 100, 800, 0, 1) == 0.0f);      // borda esquerda alem do fim: fora
  assert(abaFade(750, 100, 100, 800, 0, 1) == 1.0f);      // inteira dentro da faixa: opaca
  assert(abaFade(830, 100, 100, 800, 0, 1) < 1.0f);       // borda esquerda na zona: esmaece
  assert(abaFade(870, 100, 100, 800, 0, 0) == 1.0f);      // sem mais abas: normal
  assert(abaFade(136, 100, 100, 800, 1, 0) == 1.0f);      // inteira dentro: opaca
  assert(abaFade(-50, 100, 100, 800, 1, 0) < 1.0f);       // saindo: a fresta esmaece
  assert(abaFade(30, 100, 100, 800, 1, 0) > 0.0f);       // fresta saindo: visivel, esmaecida

  // 9. GRADE E MURAL COM MAIS AR (13/10): vao de 28 entre cartazes, a Grade
  // sem o nome embaixo (vao vertical menor que o antigo 60 com titulo), o
  // painel sem o cartaz na Grade e com ele no Mural.
  lay = VT_LAY_GRADE; timeline = 0; celLay = -1; montar(NANOS);
  assert(celX[1] - celX[0] - celW[0] >= 28.0f - 0.01f);
  assert(celY[VT_COLS] - celY[0] - celH[0] == VT_GRADE_GAP_Y && VT_GRADE_GAP_Y < VT_GAP_Y);
  assert(!painelComCartaz());
  lay = VT_LAY_MURAL; celLay = -1; montar(NANOS);
  assert(painelComCartaz());
  assert(celX[1] - celX[0] - celW[0] >= 28.0f - 0.01f);
  assert(celY[3] - celY[1] - celH[1] >= 28.0f - 0.01f);
  assert(fabsf(celH[0] - (celH[1] * 2.0f + VT_GAP_X)) < 0.01f);   // grande = dois pequenos + vao

  // 10. VITRINE NO LAYOUT DA HOME (13/10): o destaque e as fileiras seguem a
  // Moderna, o Padrao (fileiras rente ao banner) e a Dinamica (arte inteira,
  // a fileira em foco no pe da tela).
  for (L = HOME_LAYOUT_MODERNA; L < HOME_LAYOUT_N; L++) {
    char cam[700]; FILE *f;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    f = fopen(cam, "w"); assert(f);
    fprintf(f, "homeLayoutLocal %d\n", L); fclose(f);
    ajustes_dir(dados_dir());
    assert(ajustes_home_layout() == L && home_vitrine_layout() == L);
    if (L == HOME_LAYOUT_MODERNA) assert(vitTopo() == NV_SHELF_TOP);
    if (L == HOME_LAYOUT_PADRAO) assert(vitTopo() == NV_PAD_TOPO_FIL);
    if (L == HOME_LAYOUT_DINAMICA) assert(vitTopo() > NV_SHELF_TOP && vitTopo() + vitPasso() == NV_TELA_H);
  }

  // 11. LINHA DO TEMPO ANIMADA (13/10): o ano em foco cresce pela mola e a
  // linha se preenche ate ele; com animacao reduzida tudo chega num quadro.
  snprintf(pasta.group, sizeof pasta.group, "Film Collections");
  lay = layEscolha = VT_LAY_LINHA; timeline = 1; tabFocus = 0; celLay = -1; orderN = -1;
  montar(nSemente); foco = 3;
  assert(tlProg < 0.0f);
  for (i = 0; i < 120; i++) vertudo_atualizar(1.0f / 60.0f, 1000 + i * 16);
  assert(tlFoco[3] > 0.99f && tlFoco[0] < 0.01f);
  assert(fabsf(tlProg - (celY[3] + VT_TL_PONTO)) < 0.5f);
  foco = 7; vertudo_atualizar(1.0f / 60.0f, 4000);
  assert(tlProg < celY[7] + VT_TL_PONTO - 1.0f && tlFoco[7] < 0.9f);   // anda, nao salta
  anim_politica_reduzida = 1;
  foco = 9; vertudo_atualizar(1.0f / 60.0f, 4016);
  assert(tlProg == celY[9] + VT_TL_PONTO && tlFoco[9] == 1.0f && tlFoco[7] == 0.0f);
  anim_politica_reduzida = 0;

  // 12. LINHA DO TEMPO HORIZONTAL (Cover Flow, 13/10): no fim do enum (o
  // numero gravado das outras nao muda), em ordem de ano numa linha so; o foco
  // no centro, reto e inteiro; os vizinhos estreitos, escuros, sem cobrir ele.
  assert(VT_LAY_FLUXO == 8 && VT_LAY_VITRINE == 7 && VT_LAY_MURAL == 6);
  assert(!strcmp(LAY_NOME[VT_LAY_FLUXO], "Fluxo de capas"));
  gravarEscolha(&pasta, VT_LAY_FLUXO); assert(escolhaGravada(&pasta) == VT_LAY_FLUXO);
  lay = layEscolha = VT_LAY_FLUXO; timeline = 0; tabFocus = 0; celLay = -1; orderN = -1;
  montar(nSemente); foco = 0;
  assert(ordenaPorAno() && !painelComCartaz());
  for (i = 0; i < nSemente; i++) assert(celLin[i] == 0 && celCol[i] == i);
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); assert(foco == 2);
  if (argc > 1 && !strcmp(argv[1], "down")) {
    pedidosPagina = 0;
    tecla(SDLK_DOWN); assert(foco == 2 && !tabFocus && pedidosPagina == 0);
    foco = nSemente - 1; tecla(SDLK_DOWN); assert(foco == nSemente - 1 && pedidosPagina == 0);
    puts("fluxo down: ok"); return 0;
  }
  tecla(SDLK_LEFT); assert(foco == 1);
  tecla(SDLK_UP); assert(tabFocus == 1); tabFocus = 0;
  foco = 5;
  for (i = 0; i < 120; i++) vertudo_atualizar(1.0f / 60.0f, 6000 + i * 16);
  assert(fabsf(fluxoPos - 5.0f) < 0.01f);
  { float s0, s1; GfxRect c = fluxoRect(5, vtInicio(), &s0), v = fluxoRect(6, vtInicio(), &s1);
    GfxRect e = fluxoRect(4, vtInicio(), NULL);
    assert(fabsf(c.w - VT_FLX_W) < 0.5f && s0 < 0.01f);
    assert(fabsf(c.x + c.w * 0.5f - fluxoCentro(vtInicio())) < 0.5f);
    // Reto (mesma proporcao, sem espremer), menor, escuro e ATRAS do foco (sobreposto).
    assert(v.w < c.w && v.h < c.h && s1 > 0.3f);
    assert(fabsf(v.w / v.h - c.w / c.h) < 0.01f);
    assert(v.x > c.x && v.x < c.x + c.w && e.x + e.w > c.x && e.x < c.x);
    // Ponta a ponta: o 6o vizinho cai na borda da faixa; a faixa = tela menos a margem.
    assert(fabsf(fluxoFim() - vtInicio() - (NV_TELA_W - 48.0f - vtInicio())) < 0.5f);
    { GfxRect p = fluxoRect(11, vtInicio(), NULL); float cx = p.x + p.w * 0.5f;
      assert(cx > fluxoFim() - 40.0f && cx <= fluxoFim()); }
    // A ficha do titulo fica ABAIXO da linha do tempo.
    assert(fluxoInfoY() > fluxoLinhaY());
    // O ANEL DO FOCO E O DA HOME (focoprof.h): o cartao e NV_ANEL_FOCO em volta,
    // na cor do acento — nada de contorno proprio do Cover Flow.
    { char cam[700]; FILE *f;
      snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
      f = fopen(cam, "w"); assert(f);
      fprintf(f, "bordaFocoCartaz 0\n"); fclose(f);   // V_LIGA: 0 = Ligado
      ajustes_dir(dados_dir()); }
    assert(ajustes_borda_foco());
    ajustes_acento(&anelC[0], &anelC[1], &anelC[2]);
    anelN = 0; fluxoCartaz(5, vtInicio(), 1, 6000);
    assert(anelN == 1);
    assert(fabsf(anelRect.x - (c.x - NV_ANEL_FOCO)) < 0.01f &&
           fabsf(anelRect.y - (c.y - NV_ANEL_FOCO)) < 0.01f &&
           fabsf(anelRect.w - (c.w + 2 * NV_ANEL_FOCO)) < 0.01f &&
           fabsf(anelRect.h - (c.h + 2 * NV_ANEL_FOCO)) < 0.01f); }
  foco = 6; vertudo_atualizar(1.0f / 60.0f, 9000);
  assert(fluxoPos > 5.0f && fluxoPos < 6.0f);   // desliza, nao salta

  if (argc > 1 && !strcmp(argv[1], "cost")) {
    float x0 = vtInicio();
    for (i = 0; i < nSemente; i++) snprintf(semente[i].poster, sizeof semente[i].poster, "poster-%d", i);
    fluxoPos = 6; foco = 6;
    ptv_definir_gpu_fraca(0);
    pedidosArte = 0; fluxoDesenhar(nSemente, x0, 1, 0);
    int fortes = pedidosArte;
    assert(fortes == 13);
    ptv_definir_gpu_fraca(1);
    pedidosArte = sombrasFluxo = 0; fluxoDesenhar(nSemente, x0, 1, 0);
    assert(pedidosArte == 5 && pedidosArte < fortes && sombrasFluxo <= 4);
    pedidosArte = sombrasFluxo = 0;
    fluxoPos = 0; fluxoCartaz(nSemente - 1, x0, 1, 0);  // totalmente fora do recorte: nem textura nem veu
    assert(pedidosArte == 0 && sombrasFluxo == 0);
    fluxoPos = 6.5f; pedidosArte = 0;
    fluxoDesenhar(nSemente, x0, 1, 0); assert(pedidosArte <= 5);
    ptv_definir_gpu_fraca(0);
    puts("fluxo cost: ok"); return 0;
  }
  tecla(SDLK_DOWN); assert(foco == 6 && !tabFocus);
  // 11. ESTILO DAS ABAS (#205): Menu suspenso, Barra central e Paginas mexem so
  // no que o D-pad faz no cabecalho; o modelo (source, tabCursor) e o mesmo.
  { static ColSource f3[3];
    static const char *TIPOS[3] = { "movie", "movie", "series" };
    char cam[700]; FILE *f; int e;
    for (i = 0; i < 3; i++) {
      snprintf(f3[i].type, sizeof f3[i].type, "%s", TIPOS[i]);
      snprintf(f3[i].title, sizeof f3[i].title, "Fonte %d", i);
    }
    pasta.sources = f3; pasta.nSources = 3;
    lay = layEscolha = VT_LAY_GRADE; celLay = -1; orderN = -1; montar(nSemente);
    for (e = 0; e < 4; e++) {
      snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
      f = fopen(cam, "w"); assert(f);
      fprintf(f, "abasEstiloLocal %d\n", e); fclose(f);
      ajustes_dir(dados_dir());
      assert(ajustes_abas_estilo() == e);
      ddAberto = 0; pgSlide = 0.0f; source = 0; tabFocus = 1; tabCursor = 0;
      if (e == VT_AB_CHIPS || e == VT_AB_BARRA) {
        assert(cabN() == 3);
        tecla(SDLK_RIGHT); assert(tabCursor == 1 && source == 0);
        tecla(SDLK_RETURN); assert(source == 1 && tabFocus == 0);
      }
      if (e == VT_AB_MENU) {
        assert(cabN() == 3 && cursorInicial() == 2);   // Filmes, Séries, catalogo
        tabCursor = 2; tecla(SDLK_RETURN);
        assert(ddAberto == 1 && ddCursor == 0 && source == 0);
        tecla(SDLK_DOWN); assert(ddCursor == 1);
        tecla(SDLK_RETURN); assert(ddAberto == 0 && source == 1 && tabFocus == 0);
        tabFocus = 1; tabCursor = 2; tecla(SDLK_RETURN); tecla(SDLK_ESCAPE);
        assert(ddAberto == 0 && aberta == 1 && source == 1);   // Voltar fecha so a lista
        tabCursor = 1; tecla(SDLK_RETURN);
        assert(source == 2 && tabCursor == 2 && ddAberto == 0);   // tipo com um catalogo so
        tabFocus = 1; tabCursor = 0; tecla(SDLK_RETURN);          // Filmes: volta a um filme
        assert(source < 2 && tabCursor == 2 && ddAberto == 1);
        ddAberto = 0;
      }
      if (e == VT_AB_PAGINAS) {
        tecla(SDLK_RIGHT); assert(source == 1 && pgSlide > 0.5f);
        tecla(SDLK_LEFT);  assert(source == 0 && pgSlide < -0.5f);
        for (i = 0; i < 90; i++) vertudo_atualizar(1.0f / 60.0f, 20000 + i * 16);
        assert(fabsf(pgSlide) < 0.01f);
        tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
        assert(source == 2 && tabCursor == 3);   // a ultima pagina; depois o botao
      }
    }
    pasta.sources = fontes; pasta.nSources = 1;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    f = fopen(cam, "w"); assert(f);
    fprintf(f, "abasEstiloLocal 0\n"); fclose(f); ajustes_dir(dados_dir());
    assert(ajustes_abas_estilo() == 0);
    // Sem varias fontes nao ha abas, seja qual for o estilo.
    assert(estiloAbas() == VT_AB_CHIPS && cabN() == 0);
  }

  // 12. MENU SUSPENSO LONGO (#205): com 16 catalogos a lista nao cabe entre o
  // cabecalho e o pe da tela (parava em Y~1093, alem dos 1080). A lista se
  // limita ao espaco util e a janela segue o cursor: o foco nunca some.
  { static ColSource f16[16];
    char cam[700]; FILE *f; int vis, base;
    float py = VT_AB_Y + VT_AB_H + 8.0f;
    for (i = 0; i < 16; i++) {
      memset(&f16[i], 0, sizeof f16[i]);
      snprintf(f16[i].type, sizeof f16[i].type, "movie");
      snprintf(f16[i].title, sizeof f16[i].title, "Catálogo %d", i + 1);
      snprintf(f16[i].base, sizeof f16[i].base, "https://exemplo.invalid/%d", i);
    }
    pasta.sources = f16; pasta.nSources = 16;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    f = fopen(cam, "w"); assert(f);
    fprintf(f, "abasEstiloLocal 1\n"); fclose(f);
    ajustes_dir(dados_dir());
    lay = layEscolha = VT_LAY_GRADE; celLay = -1; orderN = -1; montar(nSemente);
    source = 0; tabFocus = 1; tabCursor = 0;
    assert(cabN() == 1 && cursorInicial() == 0);
    tecla(SDLK_RETURN);   // abre a lista suspensa
    assert(ddAberto == 1 && ddCursor == 0);
    base = ddJanela(16, ddCursor, py, &vis);
    assert(base == 0 && vis < 16);                       // rola, nao transborda
    assert(py + vis * VT_DD_RH + 12.0f <= NV_TELA_H);
    for (i = 0; i < 15; i++) tecla(SDLK_DOWN);
    assert(ddCursor == 15);
    base = ddJanela(16, ddCursor, py, &vis);
    assert(base > 0 && ddCursor >= base && ddCursor < base + vis);   // foco visivel
    assert(py + vis * VT_DD_RH + 12.0f <= NV_TELA_H);
    tecla(SDLK_RETURN);
    assert(ddAberto == 0 && source == 15 && tabFocus == 0);
    ddAberto = 0;
  }

  // 13. ABERTURA DA COLECAO NO MENU SUSPENSO (#205): o cursor do cabecalho
  // nasce no botao do catalogo, nao no indice da primeira fonte com endereco
  // (com ele em 4 e cabN()==3, o OK nascia em "Exibição"). O botao "Exibição"
  // tambem mantem o cursor na posicao dele, nao no numero de abas.
  { static ColSource f5[5];
    static ColFolder pasta5;
    char cam[700]; FILE *f;
    for (i = 0; i < 5; i++) {
      memset(&f5[i], 0, sizeof f5[i]);
      snprintf(f5[i].type, sizeof f5[i].type, i < 4 ? "movie" : "series");
      snprintf(f5[i].title, sizeof f5[i].title, "Fonte %d", i);
      snprintf(f5[i].catId, sizeof f5[i].catId, "cat%d", i);
    }
    snprintf(f5[4].base, sizeof f5[4].base, "https://exemplo.invalid/series");
    memset(&pasta5, 0, sizeof pasta5);
    snprintf(pasta5.id, sizeof pasta5.id, "cursor205");
    snprintf(pasta5.title, sizeof pasta5.title, "Cursor");
    pasta5.sources = f5; pasta5.nSources = 5;
    vertudo_colecao(&pasta5);
    assert(source == 4);                                 // a unica com endereco
    assert(tabFocus == 1);
    assert(tabCursor == cursorInicial() && tabCursor == 2);   // antes: 4
    tabFocus = 1; tabCursor = cabN();
    assert(tabCursor == 3);
    trocar();   // "Exibição": troca e o cursor segue no botao
    assert(lay != VT_LAY_GRADE);
    assert(tabCursor == cabN() && tabCursor == 3);       // antes: nAbas()==5
    // restaura o estilo e a pasta do resto do teste
    pasta.sources = fontes; pasta.nSources = 1;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    f = fopen(cam, "w"); assert(f);
    fprintf(f, "abasEstiloLocal 0\n"); fclose(f); ajustes_dir(dados_dir());
    assert(ajustes_abas_estilo() == 0);
  }

  vitrineVerTudo();
  printf("colecao_exibicao: ok\n");
  return 0;
}

static void vitrineAllCatalogs(void) {
  // #661: every catalog remains reachable; scrolling recycles the row cache.
  {
    static ColSource many[52];
    static ColFolder big;
    for (int r = 0; r < 52; r++) {
      snprintf(many[r].catId, sizeof many[r].catId, "catalog%d", r);
      snprintf(many[r].title, sizeof many[r].title, "Catalog %d", r);
      strcpy(many[r].type, "movie");
      strcpy(many[r].base, "https://example.invalid");
    }
    big.sources = many; big.nSources = 52;
    collection = &big; lay = VT_LAY_VITRINE; aberta = 1;
    vitIniciar();
    assert(vitN == 52);
    shot_prepare(0); shot_release(0);
    hideWatched = allWatched = 0; tabFocus = 0;
    for (int r = 0; r < 52; r++) {
      Uint32 end = SDL_GetTicks() + 3000;
      while (!vitLin[r].n) {
        vitCarregar(shotNow += 16); vitProjetar();
        assert((Sint32)(end - SDL_GetTicks()) > 0); usleep(1000);
      }
      assert(vitFR == r && vitItem(r, 0));
      char id[32]; snprintf(id, sizeof id, "tt%d", 8000000 + r);
      assert(!strcmp(vitItem(r, 0)->imdb, id));
      assert(strstr(vitLin[r].nome, "Catalog"));
      int cached = 0;
      for (int j = 0; j < vitN; j++) cached += vitLin[j].brutoN > 0;
      assert(cached <= 12);
      if (r < 51) vitEvento(SDLK_DOWN);
    }
    for (int r = 50; r >= 0; r--) {
      vitEvento(SDLK_UP);
      assert(vitFR == r);
      Uint32 end = SDL_GetTicks() + 3000;
      while (!vitLin[r].n) {
        vitCarregar(shotNow += 16); vitProjetar();
        assert((Sint32)(end - SDL_GetTicks()) > 0); usleep(1000);
      }
      assert(vitItem(r, 0));
      char id[32]; snprintf(id, sizeof id, "tt%d", 8000000 + r);
      assert(!strcmp(vitItem(r, 0)->imdb, id));
    }
    vitCancelar();
  }
  printf("showcase: 52 catalogs reachable in both directions\n");
}
