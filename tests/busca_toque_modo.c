// Search result rows drag-scroll only in Mobile mode; TV keeps drag-as-arrows.
// Samsung: IME=0, voice=1, phone=0 must still have a D-pad route to the mic.
#include <assert.h>
#define st_ime_disponivel teste_ime
#define st_voz_disponivel teste_voz
#define st_voz_iniciar teste_iniciar_voz
#define st_teclado_modo teste_modo_teclado
#define st_abre_sozinho teste_abre_sozinho
#define st_ime_abrir teste_abrir_ime
#define celb_disponivel teste_cel
#define desc_buscar limite_buscar
#define desc_busca_n_alvos limite_alvos
#define desc_busca_alvo_n limite_n
#define desc_busca_alvo_item limite_item
#define desc_busca_alvo_titulo limite_titulo
#define desc_busca_alvo_addon limite_addon
#define desc_busca_alvo_nuvio limite_nuvio
#define desc_busca_n_fontes limite_fontes
#define desc_busca_chegou limite_chegou
#define spotpessoa_pedir limite_pessoa_pedir
#define spotpessoa_geracao limite_pessoa_geracao
#define spotpessoa_n limite_pessoa_n
#include "../src/busca.c"
#undef st_ime_disponivel
#undef st_voz_disponivel
#undef st_voz_iniciar
#undef st_teclado_modo
#undef st_abre_sozinho
#undef st_ime_abrir
#undef celb_disponivel
#undef desc_buscar
#undef desc_busca_n_alvos
#undef desc_busca_alvo_n
#undef desc_busca_alvo_item
#undef desc_busca_alvo_titulo
#undef desc_busca_alvo_addon
#undef desc_busca_alvo_nuvio
#undef desc_busca_n_fontes
#undef desc_busca_chegou
#undef spotpessoa_pedir
#undef spotpessoa_geracao
#undef spotpessoa_n

static int limiteAtivo;
static CatItem limiteRemotos[DESC_BUSCA_POR_ALVO];
void limite_buscar(const char *t) { (void)t; }
int limite_alvos(void) { return limiteAtivo; }
int limite_n(int a, const char *t) { return limiteAtivo && a == 0 && !strcmp(t, "bigg") ? DESC_BUSCA_POR_ALVO : 0; }
int limite_item(int a, int i, CatItem *d) {
  if (a || i < 0 || i >= DESC_BUSCA_POR_ALVO) return 0;
  *d = limiteRemotos[i]; return 1;
}
const char *limite_titulo(int a) { (void)a; return "Series"; }
const char *limite_addon(int a) { (void)a; return "Fixture"; }
int limite_nuvio(int a) { (void)a; return 0; }
int limite_fontes(void) { return 1; }
int limite_chegou(const char *t) { return limiteAtivo && !strcmp(t, "bigg"); }
void limite_pessoa_pedir(const char *t, unsigned a) { (void)t; (void)a; }
unsigned limite_pessoa_geracao(void) { return 0; }
int limite_pessoa_n(const char *t) { (void)t; return 0; }

static void resultadosAte24(void) {
  memset(limiteRemotos, 0, sizeof limiteRemotos);
  for (int i = 0; i < DESC_BUSCA_POR_ALVO; i++) {
    snprintf(limiteRemotos[i].imdb, sizeof limiteRemotos[i].imdb, "tmdb:%d", 7000+i);
    snprintf(limiteRemotos[i].titulo, sizeof limiteRemotos[i].titulo, "Bigg %d", i);
    strcpy(limiteRemotos[i].tipo, "series");
  }
  strcpy(limiteRemotos[13].imdb, "tmdb:11436");
  strcpy(limiteRemotos[13].titulo, "Bigg Boss");
  strcpy(limiteRemotos[13].meta, "2006");
  limiteAtivo = 1;
  for (int mobile = 0; mobile <= 1; mobile++) {
    // Search appends into an initialized catalogue, as the browsing screen
    // does. Its existing append contract rejects an entirely empty catalogue.
    CatItem base = {0};
    strcpy(base.imdb, "tt0000555"); strcpy(base.titulo, "Unrelated");
    strcpy(base.tipo, "movie");
    cat_definir_tudo(&base, 1, NULL, 0); cat_quadro();
    layout_modo_definir(mobile);
    strcpy(consulta, "bigg"); consultaFiltrada[0] = 0;
    painel = 0; nVelho = 0;
    refiltrar();
    // The first item becomes its own Best result; all other 23 stay navigable.
    assert(nFil == 2 && fil[0].melhor && fil[0].n == 1 && fil[1].n == 23);
    assert(focoRes.nColunas[1] == 23);
    assert(!strcmp(cat_item(fil[1].itens[12])->imdb, "tmdb:11436"));
    painel = 1; focoRes.fileira = 1; focoRes.coluna = 0;
    SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RIGHT;
    for (int i = 0; i < 30; i++) busca_evento(&e);
    assert(focoRes.coluna == 22);
    assert(!strcmp(cat_item(fil[1].itens[22])->imdb, "tmdb:7023"));
    e.key.keysym.sym = SDLK_RETURN; busca_evento(&e);
    e.type = SDL_KEYUP; busca_evento(&e); e.type = SDL_KEYDOWN;
    assert(pedido == fil[1].itens[22]);
    assert(!strcmp(cat_item(pedido)->imdb, "tmdb:7023"));
    pedido = -1;
    focoRes.coluna = 12;
    e.key.keysym.sym = SDLK_RETURN; busca_evento(&e);
    e.type = SDL_KEYUP; busca_evento(&e); e.type = SDL_KEYDOWN;
    assert(pedido == fil[1].itens[12]);
    assert(!strcmp(cat_item(pedido)->imdb, "tmdb:11436"));
    pedido = -1;
    // Rebuild the same term while focused: selection follows its catalogue identity.
    refiltrar();
    assert(focoRes.fileira == 1 && focoRes.coluna == 12);
  }
  limiteAtivo = 0;
  puts("PASS: Busca retains and opens 14th/24th results in TV and Mobile");
}

static int temIme, temVoz, temCel, vozIniciada;
static int modoTeclado, abreSozinho, imeIniciado;
int teste_modo_teclado(void) { return modoTeclado; }
int teste_abre_sozinho(void) { return abreSozinho; }
int teste_abrir_ime(int dono, const char *texto, int max) {
  assert(texto && !texto[0] && max == BU_MAX_CONSULTA - 1);
  imeIniciado = dono; return 1;
}
int teste_ime(void) { return temIme; }
int teste_voz(void) { return temVoz; }
int teste_cel(void) { return temCel; }
int teste_iniciar_voz(int dono) { vozIniciada = dono; return 1; }
#include <assert.h>

static void arrasto(int mobile) {
  PonteiroRolagem e = { PONT_ROL_INICIO, 0, 0.0f, 0.0f, 0.0f, 0.0f };
  layout_modo_definir(mobile);
  nFil = 1; fil[0].n = 20; fil[0].pessoas = 0; fil[0].melhor = 0;
  scrollX[0] = 0.0f; toqueLivreX[0] = 0; toqueFileira = -1;
  e.x = BU_RES_X + 10.0f; e.y = BU_RES_Y + 10.0f;
  int captura = toqueBuscaRolar(&e);
  e.fase = PONT_ROL_MOVER; e.delta = -200.0f;
  toqueBuscaRolar(&e);
  if (mobile) assert(captura == 1 && scrollX[0] == 200.0f && toqueLivreX[0]);
  else assert(captura == 0 && scrollX[0] == 0.0f && !toqueLivreX[0]);
}

int main(void) {
  layout_tela_definir(1920, 1080);
  arrasto(0);
  arrasto(1);
  layout_modo_definir(0);
  toqueBuscaLimpar();
  SDL_Event e; SDL_zero(e); e.type = SDL_KEYDOWN;
  for (int modo = 0; modo < 4; modo++) {
    temIme = modo == 1; temVoz = modo == 0; temCel = modo == 2;
    painel = campoFoco = 0;
    focus_iniciar(&focoKb, 1, KB_COLUNAS);
    e.key.keysym.sym = SDLK_UP; busca_evento(&e);
    assert(campoFoco == (temIme ? 1 : temVoz ? 2 : temCel ? 3 : 0));
    if (temVoz) {
      e.key.keysym.sym = SDLK_RETURN; busca_evento(&e);
      assert(vozIniciada == ST_BUSCA);
    }
  }
  // TV/web: only "Do sistema" opens automatically; Android/Mobile keep
  // automatic IME entry, except Android's explicit "Do app" mode.
  for (int mobile = 0; mobile <= 1; mobile++) {
    layout_modo_definir(mobile);
    for (abreSozinho = 0; abreSozinho <= 1; abreSozinho++) {
      for (temIme = 0; temIme <= 1; temIme++) {
        for (modoTeclado = 0; modoTeclado <= 2; modoTeclado++) {
          imeIniciado = 0;
          busca_iniciar();
          int abre = temIme && modoTeclado != 1 &&
                     (abreSozinho || mobile || modoTeclado == 2);
          assert(imeIniciado == (abre ? ST_BUSCA : 0));
          assert(busca_foco_campo() == ((abre || (mobile && temIme)) ? 1 : 0));
          busca_encerrar();
        }
      }
    }
  }
  puts("PASS: Busca drag, mic navigation and automatic system keyboard modes");
  resultadosAte24();
  return 0;
}
