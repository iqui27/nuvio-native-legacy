// CATREV — indices de cat_item guardados em copias estaticas sem guarda de
// cat_revisao(). Exercita o codigo de producao real (mapa.c, explorar.c,
// catalogo.c): monta os climas sobre um catalogo, TROCA o catalogo inteiro
// (cat_definir_tudo, que sobe cat_revisao) e confere que nenhum catIndice
// copiado/publicado aponta para a obra errada.
//
//   (a) troca entre a montagem (mapa_climas_pedir) e a copia
//       (mapa_climas_copiar): os indices publicados tem de ser da revisao
//       nova, resolvidos pela identidade da obra;
//   (b) troca entre quadros do Explorar (explorar_iniciar -> atualizar ->
//       abrir_titulo -> Voltar -> explorar_pediu_abrir): o indice pedido
//       para abrir a pagina tem de ser a mesma obra, nao a mesma posicao;
//   (c) a serie do card de Continuar, "tt...:T:E" no catalogo e "tt..." na
//       mao — a comparacao e pelo id base MESMO sem troca (CATREVB);
//   (d) id cru sem IMDb/TMDB ("kitsu:..."): republicacao identica nao apaga
//       o catIndice nem impede o cartaz de abrir (CATREVB);
//   (e) homonimo sem IMDb (id "tmdb:N") na posicao velha: a vizinhanca
//       re-resolve pela identidade em vez de adotar o item errado (CATREVB).
//
// Sem rede (rede_baixar -> NULL, sem chave do TMDB): so o caminho local roda.
// O fio do clima sobe de verdade (a fusao republica), entao cada copiar antes
// da assercao drena as publicacoes ate estabilizar.
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/catalogo.h"
#include "../src/mapa.h"
#include "../src/explorar.h"
#include "../src/text.h"
#include "../src/posterprov.h"
#include "../src/farolgosto.h"
#include "../src/progresso.h"
#include "../src/salvos.h"
#include "../src/amigostitulo.h"
#include "../src/socialvis.h"

// --- DUBLES -----------------------------------------------------------------
// Entradas do teste, nao do ambiente — mesmo modelo de tests/catcache.c.
int ajustes_idioma(void) { return 1; }
int ajustes_idioma_ingles(void) { return 1; }
int ajustes_fonte_biblioteca(void) { return 0; }
int ajustes_animacoes_reduzidas(void) { return 1; }
int ajustes_acento(void) { return 0; }
int ajustes_borda_foco(void) { return 0; }
float ajustes_conteudo_x(void) { return 80.0f; }
int ajustes_menu_topo(void) { return 0; }
int ajustes_tinta_foco(void) { return 0; }
int ajustes_vidro(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
const char *desc_genero_pt(const char *g) { return g; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_chave_tmdb_reserva(void) { return ""; }
int desc_titulo_buscando(void) { return 0; }
void desc_pedir_titulo(const char *imdb) { (void)imdb; }
void desc_pedir_titulo_tmdb(long tmdb, const char *tipo) { (void)tmdb; (void)tipo; }
char *rede_baixar(const char *url, int timeoutS) { (void)url; (void)timeoutS; return NULL; }
const char *dados_dir(void) { static char d[512] = ""; return d; }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
void dados_gravar_leve(const char *nome, const char *txt) { (void)nome; (void)txt; }
void dados_apagar(const char *nome) { (void)nome; }
const char *sessao_usuario(void) { return "teste"; }
int perfis_ativo(void) { return 1; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
}
int salvos_n(void) { return 0; }
const SalvoItem *salvos_item(int i) { (void)i; return NULL; }
int farolsinal_recomendar(void) { return 0; }
int farolsinal_modelo(FarolGosto *dst) { (void)dst; return 0; }
void amigostitulo_atualizar(void) {}
int amigostitulo_obter(const char *imdb, AmigosTitulo *saida) { (void)imdb; (void)saida; return 0; }
void amigostitulo_primeiro_nome(const char *nome, char *dst, size_t tam) {
  (void)nome; if (dst && tam) dst[0] = 0;
}
const SvEvento *socialvis_evento(int i) { (void)i; return NULL; }
int socialvis_n_eventos(void) { return 0; }
int menu_pilula_titulo(void) { return 0; }
const char *posterprov_card(const char *imdb, long tmdb, const char *tipo,
                            const char *orig) {
  (void)imdb; (void)tmdb; (void)tipo; return orig;
}
void ponteiro_alvo(int i, int b) { (void)i; (void)b; }
void ponteiro_mouse(int x, int y) { (void)x; (void)y; }
unsigned tex_obter_larg(const char *caminho, float largLayout) {
  (void)caminho; (void)largLayout; return 0;
}
float tex_aspecto(const char *caminho) { (void)caminho; return 0.0f; }
TxtLinha txt_linha(TxtEstilo estilo, const char *s, int r, int g, int b, int a) {
  TxtLinha z; memset(&z, 0, sizeof z);
  (void)estilo; (void)s; (void)r; (void)g; (void)b; (void)a; return z;
}
TxtLinha txt_linha_corta(TxtEstilo estilo, const char *s, int r, int g, int b,
                         int a, float maxW) {
  TxtLinha z; memset(&z, 0, sizeof z);
  (void)estilo; (void)s; (void)r; (void)g; (void)b; (void)a; (void)maxW; return z;
}
void txt_desenhar(TxtLinha l, float x, float y) { (void)l; (void)x; (void)y; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) {
  (void)l; (void)x; (void)y; (void)a;
}
int txt_largura(TxtEstilo estilo, const char *s) { (void)estilo; (void)s; return 0; }
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b,
                      float x, float y, float larg, float leading,
                      float alpha, int maxLinhas) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y;
  (void)larg; (void)leading; (void)alpha; (void)maxLinhas; return 0.0f;
}
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b,
                   float x, float y, float alpha, float tracking) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y;
  (void)alpha; (void)tracking; return 0.0f;
}
void gfx_cor(int r, int g, int b, int a) { (void)r; (void)g; (void)b; (void)a; }
void gfx_rect(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_anel(float x, float y, float w, float h, float t) {
  (void)x; (void)y; (void)w; (void)h; (void)t;
}
float gfx_escala(void) { return 1.0f; }
int gfx_efeitos_leves(void) { return 1; }
void gfx_sem_recorte(void) {}
void gfx_vidro_cartao(float x, float y, float w, float h, float r) {
  (void)x; (void)y; (void)w; (void)h; (void)r;
}
void gfx_vidro_foco(float x, float y, float w, float h, float r) {
  (void)x; (void)y; (void)w; (void)h; (void)r;
}
void gfx_vidro_painel(float x, float y, float w, float h, float r) {
  (void)x; (void)y; (void)w; (void)h; (void)r;
}
void gfx_vidro_pilula_cheia(float x, float y, float w, float h) {
  (void)x; (void)y; (void)w; (void)h;
}
void gfx_vidro_tinta(int r, int g, int b, int a) { (void)r; (void)g; (void)b; (void)a; }
int gfx_snap_iniciar(int w, int h) { (void)w; (void)h; return 0; }
int gfx_snap_ok(void) { return 0; }
void gfx_snap_comecar(void) {}
void gfx_snap_terminar(void) {}
void gfx_snap_desenhar(void) {}
int gfx_snap_ativo(void) { return 0; }
unsigned gfx_snap_geracao(void) { return 0; }
float gfx_tex_aspect_atual(void) { return 0.0f; }

// --- APOIO -------------------------------------------------------------------
static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %-64s %s\n", o_que, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

// Mesma regra de idbase_mesmo (igual, ou sobra exata ":<digitos>" para id
// nao-IMDb / ":<digitos>:<digitos>"), copiada aqui para o teste compilar
// tambem contra a base — que ainda nao tem a funcao em idbase.h.
static int mesmoId(const char *a, const char *b) {
  size_t na, nb, nc;
  const char *lon, *cur, *r;
  int n = 0, anime;
  if (!a || !b) return 0;
  if (!strcmp(a, b)) return 1;
  na = strlen(a); nb = strlen(b);
  lon = na > nb ? a : b; cur = na > nb ? b : a; nc = na > nb ? nb : na;
  if (strncmp(lon, cur, nc)) return 0;
  anime = strncmp(lon, "tt", 2) || strncmp(cur, "tt", 2);
  r = lon + nc;
  if (*r != ':') return 0;
  r++;
  while (*r >= '0' && *r <= '9') { r++; n++; }
  if (!*r) return anime && n;
  if (*r != ':') return 0;
  r++; n = 0;
  while (*r >= '0' && *r <= '9') { r++; n++; }
  return n && !*r;
}

// Mesma identidade da correcao: tipo + IMDb pelo id base; sem IMDb, TMDB
// (campo ou id "tmdb:N"); sem nenhum, titulo + tipo.
static int mesmaObra(const CatItem *ci, const MapaObra *o) {
  if (!ci || strcmp(ci->tipo, o->tipo)) return 0;
  if (o->imdb[0]) return mesmoId(ci->imdb, o->imdb);
  if (o->tmdb) {
    if (ci->tmdb == o->tmdb) return 1;
    if (!strncmp(ci->imdb, "tmdb:", 5) && atol(ci->imdb + 5) == o->tmdb) return 1;
    return 0;
  }
  return !strcmp(ci->titulo, o->titulo);
}

// Nenhum catIndice copiado pode apontar para outra obra: -1 (fora do
// catalogo) ou a posicao de um item cuja identidade bate.
static int indicesBatem(const MapaClimas *cl) {
  int m, i, ruim = 0;
  for (m = 0; m < cl->n; m++)
    for (i = 0; i < cl->c[m].n; i++) {
      const MapaObra *o = &cl->c[m].itens[i];
      int idx = o->catIndice;
      if (idx < 0) continue;
      if (idx >= cat_n() || !mesmaObra(cat_item(idx), o)) ruim++;
    }
  return ruim == 0;
}

// Drena as publicacoes do fio do clima ate o retrato estabilizar.
static void drenarCopias(MapaClimas *cl, unsigned *rev) {
  int k;
  for (k = 0; k < 200; k++) {
    if (!mapa_climas_copiar(cl, rev)) break;
    usleep(10000);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  explorar_evento(&e);
}

// --- CENARIO ------------------------------------------------------------------
// Catalogo A: filmes "Crime" (clima 7, so genero) e series "Mistério"
// (clima 6). A obra-chave e "tt7770003/Titulo 3" (filme) na posicao 3.
// O catalogo B troca tudo de posicao: um HOMONIMO puro (mesmo titulo, tipo e
// genero — outro imdb) vai para a posicao 3, o original para a 6, e a serie
// da 6 some (obra fora do catalogo). Sem a guarda o indice 3 abre o
// homonimo; com a guarda a obra re-resolve para 6.
static CatItem *montarCatalogo(int *n, int troca) {
  CatItem *v = calloc(10, sizeof *v);
  int i;
  *n = 10;
  for (i = 0; i < 10; i++) {
    int serie = i >= 5;
    snprintf(v[i].tipo, sizeof v[i].tipo, "%s", serie ? "series" : "movie");
    snprintf(v[i].imdb, sizeof v[i].imdb, "tt777%04d", i);
    snprintf(v[i].titulo, sizeof v[i].titulo, "Titulo %d", i);
    snprintf(v[i].genero, sizeof v[i].genero, "%s", serie ? "Mistério" : "Crime");
    snprintf(v[i].poster, sizeof v[i].poster, "/p%d.jpg", i);
    v[i].nota = 70 + i;
  }
  if (troca) {
    snprintf(v[3].imdb, sizeof v[3].imdb, "tt7779993");   // homonimo da obra-chave
    snprintf(v[6].tipo, sizeof v[6].tipo, "movie");       // a obra-chave moveu
    snprintf(v[6].imdb, sizeof v[6].imdb, "tt7770003");
    snprintf(v[6].titulo, sizeof v[6].titulo, "Titulo 3");
    snprintf(v[6].genero, sizeof v[6].genero, "Crime");
    snprintf(v[6].poster, sizeof v[6].poster, "/p3.jpg");
  }
  return v;
}

// A obra de catIndice `pos` do catalogo A, como sairia numa copia de climas.
static MapaObra obraEm(const CatItem *a, int pos) {
  MapaObra o;
  memset(&o, 0, sizeof o);
  snprintf(o.imdb, sizeof o.imdb, "%s", a[pos].imdb);
  snprintf(o.tipo, sizeof o.tipo, "%s", a[pos].tipo);
  snprintf(o.titulo, sizeof o.titulo, "%s", a[pos].titulo);
  snprintf(o.poster, sizeof o.poster, "%s", a[pos].poster);
  o.catIndice = pos;
  return o;
}

int main(void) {
  int n;
  CatItem *a = montarCatalogo(&n, 0);
  MapaClimas cl;
  unsigned rev = 0;
  int i, j, idx, pos;

  puts("catrev: guarda de cat_revisao nos indices copiados");

  // (a) troca entre montagem e copia -----------------------------------------
  cat_definir_tudo(a, n, NULL, 0);
  mapa_climas_pedir();
  memset(&cl, 0, sizeof cl);
  drenarCopias(&cl, &rev);
  confere("montagem tem itens nos climas", cl.n > 0);
  confere("indices da montagem batem com o catalogo A", indicesBatem(&cl));
  // O fio do clima sai depois da fusao inicial. Esperar ele aqui evita a
  // corrida em que a fusao dele revalida os indices no meio do
  // cat_definir_tudo seguinte (a janela em que n ja e 0 e itens ainda e o
  // bloco velho — o retrato sairia todo -1 e ficaria ate a proxima troca).
  usleep(150000);

  { CatItem *b = montarCatalogo(&n, 1);
    unsigned revAntes = cat_revisao();
    int copiou;
    cat_definir_tudo(b, n, NULL, 0);
    confere("a troca subiu cat_revisao", cat_revisao() != revAntes);
    // A troca sem nova montagem tem de republicar indices validos: a copia
    // seguinte nao pode entregar a posicao do bloco velho.
    copiou = mapa_climas_copiar(&cl, &rev);
    confere("copiar depois da troca republica o retrato", copiou == 1);
    confere("nenhum cartaz aponta para a obra errada", indicesBatem(&cl));
    // A obra-chave nao pode ter herdado a posicao do homonimo: ou re-resolve
    // para a posicao nova (6), ou sai — nunca fica apontando para o outro.
    pos = -2;
    for (i = 0; i < cl.n; i++)
      for (j = 0; j < cl.c[i].n; j++)
        if (!strcmp(cl.c[i].itens[j].imdb, "tt7770003"))
          pos = cl.c[i].itens[j].catIndice;
    confere("tt7770003 re-resolvida para a posicao nova", pos == 6);
    free(b);
  }

  // (b) troca entre quadros do Explorar ---------------------------------------
  // explorar_iniciar copia os climas (como o app) e abrir_titulo empilha a
  // obra na trilha com o catIndice do catalogo A. Entre os dois quadros o
  // catalogo troca; o Voltar (ESC) e o ponto que lia o indice guardado sem
  // conferir — pediuIndice sairia 3, a posicao do homonimo.
  { CatItem *b;
    cat_definir_tudo(a, n, NULL, 0);
    explorar_iniciar();
    explorar_atualizar(0.016f, 16);
    { MapaObra o = obraEm(a, 3);            // tt7770003 na posicao 3 de A
      explorar_abrir_titulo(&o); }
    b = montarCatalogo(&n, 1);
    cat_definir_tudo(b, n, NULL, 0);
    explorar_atualizar(0.016f, 32);         // o quadro seguinte, como no app
    tecla(SDLK_ESCAPE);                     // Voltar: 1o degrau, ORIGEM_DETALHE
    idx = -1;
    if (explorar_pediu_abrir(&idx)) {
      const CatItem *ci = idx >= 0 && idx < cat_n() ? cat_item(idx) : NULL;
      confere("Voltar pede a pagina da MESMA obra (tt7770003)",
              ci && !strcmp(ci->imdb, "tt7770003"));
    } else {
      // Recusar a posicao velha tambem e correto: nunca abrir a obra errada.
      confere("Voltar sem obra resolvida nao abre o homonimo", 1);
    }
    free(b);
  }

  // (c) a serie do card de Continuar --------------------------------------
  // O catalogo guarda o item como "tt...:T:E" e quem procura a serie tem so
  // o id base ("tt...") — mapa_obra_do_catalogo corta o sufixo, como faz o
  // Detalhe. SEM troca de catalogo nenhuma: a comparacao tem de ser pelo id
  // base, a regra de cat_indice_por_imdb (CATREVB).
  { MapaObra o;
    CatEp eps[3];
    unsigned vrev = 0;
    static MapaVizinhos viz;
    memset(eps, 0, sizeof eps);
    memset(&viz, 0, sizeof viz);
    snprintf(a[5].imdb, sizeof a[5].imdb, "tt7770005:1:2");
    cat_definir_tudo(a, n, NULL, 0);
    for (i = 0; i < 3; i++) { eps[i].temporada = 1; eps[i].episodio = i + 1; }
    cat_definir_episodios(5, eps, 3);
    confere("catalogo acha o card pelo id base",
            cat_indice_por_imdb("tt7770005") == 5 && cat_n_episodios(5) == 3);
    confere("obra do card sai com o id base",
            mapa_obra_do_catalogo(5, &o) &&
            !strcmp(o.imdb, "tt7770005") && o.catIndice == 5);
    explorar_iniciar();
    explorar_atualizar(0.016f, 16);
    explorar_abrir_titulo(&o);
    explorar_atualizar(0.016f, 32);
    tecla(SDLK_ESCAPE);                     // Voltar: 1o degrau, ORIGEM_DETALHE
    idx = -1;
    confere("Voltar do Explorar reabre o Detalhe do card de Continuar",
            explorar_pediu_abrir(&idx) && idx == 5);
    mapa_vizinhos_pedir(&o, 0, NULL, 0);
    mapa_vizinhos_copiar(&viz, &vrev);
    confere("vizinhanca do card usa o item do catalogo",
            viz.foco.catIndice == 5);
  }

  // (d) id cru (kitsu:, servidor pessoal) ----------------------------------
  // Sem IMDb nem TMDB, a identidade e o id do item inteiro: uma republicacao
  // IDENTICA do catalogo (cat_revisao sobe igual) nao pode apagar o catIndice
  // nem impedir o cartaz de abrir (CATREVB).
  { static MapaClimas cl2;
    const MapaObra *o2 = NULL;
    unsigned rev2 = 0;
    snprintf(a[2].imdb, sizeof a[2].imdb, "kitsu:4242");
    cat_definir_tudo(a, n, NULL, 0);
    explorar_iniciar();
    explorar_atualizar(0.016f, 16);
    memset(&cl2, 0, sizeof cl2);
    drenarCopias(&cl2, &rev2);
    for (i = 0; i < cl2.n && !o2; i++)
      for (j = 0; j < cl2.c[i].n; j++)
        if (!strcmp(cl2.c[i].itens[j].titulo, "Titulo 2")) o2 = &cl2.c[i].itens[j];
    confere("cartaz kitsu: com indice antes da republicacao",
            o2 && o2->catIndice == 2);
    cat_definir_tudo(a, n, NULL, 0);        // mesmo conteudo, revisao sobe
    explorar_atualizar(0.016f, 32);
    explorar_atualizar(0.016f, 48);
    memset(&cl2, 0, sizeof cl2);
    rev2 = 0;
    drenarCopias(&cl2, &rev2);
    o2 = NULL;
    for (i = 0; i < cl2.n && !o2; i++)
      for (j = 0; j < cl2.c[i].n; j++)
        if (!strcmp(cl2.c[i].itens[j].titulo, "Titulo 2")) o2 = &cl2.c[i].itens[j];
    confere("depois da republicacao identica o cartaz kitsu: segue no catalogo",
            o2 && o2->catIndice == 2);
    if (o2) {
      explorar_abrir_titulo(o2);
      explorar_atualizar(0.016f, 64);
      tecla(SDLK_ESCAPE);
      idx = -1;
      confere("Voltar reabre a pagina do cartaz kitsu:",
              explorar_pediu_abrir(&idx) && idx == 2);
    } else confere("Voltar reabre a pagina do cartaz kitsu:", 0);
  }

  // (e) homonimo sem IMDb (id "tmdb:N") na posicao velha --------------------
  // A obra so tem TMDB e o catalogo trocou: o homonimo ficou na posicao
  // antiga. A vizinhanca nao pode adota-lo — re-resolve pela identidade
  // (CATREVB).
  { MapaObra alvo;
    CatItem *b = montarCatalogo(&n, 1);
    unsigned vrev = 0;
    static MapaVizinhos viz;
    memset(&viz, 0, sizeof viz);
    snprintf(b[3].imdb, sizeof b[3].imdb, "tmdb:9999");   // homonimo na pos. velha
    snprintf(b[6].imdb, sizeof b[6].imdb, "tmdb:3003");   // a obra foi para a 6
    alvo = obraEm(a, 3);
    alvo.imdb[0] = 0;
    alvo.tmdb = 3003;
    cat_definir_tudo(b, n, NULL, 0);
    mapa_vizinhos_pedir(&alvo, 0, NULL, 0);
    mapa_vizinhos_copiar(&viz, &vrev);
    confere("vizinhanca sem IMDb (tmdb:N) re-resolve e nao adota o homonimo",
            viz.foco.catIndice == 6);
    // Com IMDb, o caso que a r1 ja cobria: nao adota e re-resolve.
    { MapaObra t = obraEm(a, 4);
      CatItem *c2 = montarCatalogo(&n, 1);
      snprintf(c2[4].imdb, sizeof c2[4].imdb, "tt7779994");  // homonimo na 4
      c2[8] = a[4];                                          // o real foi para a 8
      cat_definir_tudo(c2, n, NULL, 0);
      mapa_vizinhos_pedir(&t, 0, NULL, 0);
      mapa_vizinhos_copiar(&viz, &vrev);
      confere("vizinhanca com IMDb nao adota o homonimo e re-resolve",
              viz.foco.catIndice == 8);
      free(c2);
    }
    free(b);
  }

  free(a);
  usleep(200000);                            // deixa o fio do clima terminar
  if (falhas) { printf("catrev: %d FALHA(S)\n", falhas); return 1; }
  puts("catrev: tudo ok");
  return 0;
}
