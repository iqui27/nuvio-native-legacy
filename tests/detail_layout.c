// Regression: unknown logo language and fullscreen carousel edge navigation.
#define desc_pedir_titulo_semente capturar_rota_tmdb
#define extras_relacionado_imdb relacionado_tmdb_fixture
#include "../src/notasui.h"
#define notasui_fontes_n notas_n_fixture
int notas_n_fixture(const NotasSecao *s);
#include "../src/detail.c"
#undef desc_pedir_titulo_semente
#undef extras_relacionado_imdb
#include <assert.h>
static long rotaId;
static char rotaTipo[16];
// detail.c now opens a TMDB recommendation through desc_pedir_titulo_semente
// (click seed: title/year/poster); the stub keeps it free of I/O.
void capturar_rota_tmdb(const char *imdb, long id, const char *tipo, const char *titulo,
                        const char *ano, const char *poster) {
  (void)imdb; (void)titulo; (void)ano; (void)poster;
  rotaId = id;
  snprintf(rotaTipo, sizeof rotaTipo, "%s", tipo);
}
// Five sources with a rating: the section must still be ONE focus stop.
int notas_n_fixture(const NotasSecao *s) { (void)s; return 5; }
const char *relacionado_tmdb_fixture(int i) {
  return i == 0 ? "tmdb:42" : "";
}
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  // Detail geometry is the published1.7.4 layout; global Glass remains.
  assert(NV_DETP_G_TEMP == 1080 && NV_DETP_TEMP_Y == 1160);
  assert(NV_DETP_G_EP == 1194 && NV_DETP_EP_Y == 1286);
  assert(NV_DETP_EP_W == 640 && NV_DETP_EP_H == 414);
  assert(CAR_TEXTO_SOBE == 64);
  // Ratings first below the hero on movies; right after the episodes on series,
  // followed by the season numbers block (SEC_NUMEROS) before the tabs.
  assert(ORDEM_FILME[0] == SEC_NOTAS);
  assert(ORDEM_SERIE[1] == SEC_EPISODIOS && ORDEM_SERIE[2] == SEC_PROGTEMP &&
         ORDEM_SERIE[3] == SEC_NOTAS && ORDEM_SERIE[4] == SEC_NUMEROS && ORDEM_SERIE[5] == SEC_ABAS_INFO);
  CatItem c = {0};
  const char *u = "https://image.tmdb.org/t/p/w500/image.png";
  assert(!mostrarNomeLogo(&c, u, 1, "pt-BR"));
  assert(!mostrarNomeLogo(&c, u, 0, "pt-BR"));
  assert(mostrarNomeLogo(&c, NULL, 0, "pt-BR"));
  snprintf(c.logoIdiomaUrl, sizeof c.logoIdiomaUrl, "%s", u);
  snprintf(c.logoIdioma, sizeof c.logoIdioma, "pt");
  assert(!mostrarNomeLogo(&c, u, 1, "pt-BR"));
  snprintf(c.logoIdioma, sizeof c.logoIdioma, "en");
  assert(mostrarNomeLogo(&c, u, 1, "pt-BR"));
  assert(mostrarNomeLogo(&c, u, 0, "pt-BR")); // Known foreign art does not delay the name.
  assert(mostrarNomeLogo(&c, "https://image.tmdb.org/t/p/w300/image.png", 1, "pt-BR"));
  assert(!mostrarNomeLogo(&c, "https://other.example/image.png", 1, "pt-BR"));
  assert(!mostrarNomeLogo(&c, "https://image.tmdb.org/t/p/w500/other.png", 1, "pt-BR"));
  snprintf(c.logoIdioma, sizeof c.logoIdioma, "und");
  assert(!mostrarNomeLogo(&c, u, 1, "pt-BR"));
  snprintf(c.tipo, sizeof c.tipo, "movie");
  cat_definir_tudo(&c, 1, NULL, 0); idx = 0;
  // Ratings: focus stops only on the big score, never per source chip.
  assert(secaoN(SEC_NOTAS) == 1);
  carro = 1; carCheia = 1; carN = 3; carPos = carAplicado = 1;
  nivel = 0; saindo = 0;
  SDL_Event e = {0}; e.type = SDL_KEYDOWN;
  botao = nBotoes() - 1; e.key.keysym.sym = SDLK_RIGHT;
  detail_evento(&e); assert(carPos == 1 && !saindo);
  botao = 0; e.key.keysym.sym = SDLK_LEFT;
  detail_evento(&e); assert(carPos == 1 && !saindo);
  carPasso(1); assert(carPos == 1);
  carCheia = 0;
  botao = nBotoes() - 1; e.key.keysym.sym = SDLK_RIGHT;
  detail_evento(&e); assert(carPos == 2);
  carCheia = 1; carPos = 1; maisAcoes = 0; botao = 0;
  assert(nBotoes() == 2);
  e.key.keysym.sym = SDLK_RIGHT; detail_evento(&e);
  assert(maisAcoes && acaoEm(botao) == ACAO_LISTA && nBotoes() > 2);
  assert(acaoEm(0) == ACAO_PRIMARIO);
  pedMarcar = 0;
  SDL_Delay(2);
  e.key.keysym.sym = SDLK_RETURN; e.type = SDL_KEYDOWN; detail_evento(&e);
  e.type = SDL_KEYUP; detail_evento(&e); assert(pedMarcar);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = SDLK_RIGHT; detail_evento(&e);
  assert(acaoEm(botao) == ACAO_ASSISTIDO);
  e.key.keysym.sym = SDLK_LEFT; detail_evento(&e); detail_evento(&e);
  assert(!maisAcoes && botao == 0 && nBotoes() == 2);
  e.key.keysym.sym = SDLK_RIGHT; detail_evento(&e);
  e.key.keysym.sym = SDLK_ESCAPE; detail_evento(&e);
  assert(!maisAcoes && !saindo && botao == 0);
  carro=0; maisAcoes=0; nivel=0; botao=0;
  assert(ajustes_home_layout()==HOME_LAYOUT_MODERNA);
  assert(acoesAgrupadas() && nBotoes()==2);
  e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RIGHT;
  detail_evento(&e); assert(maisAcoes && acaoEm(botao)==ACAO_LISTA);
  e.key.keysym.sym=SDLK_ESCAPE; detail_evento(&e);
  assert(!maisAcoes && !saindo);
  carro=0; pessoaAberta=1; saindo=0;
  e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_ESCAPE;
  detail_evento(&e); assert(!pessoaAberta && !saindo);
  pessoaAberta=1; e.key.keysym.sym=SDLK_AC_BACK;
  detail_evento(&e); assert(!pessoaAberta && !saindo);
  pessoaAberta=1; e.key.keysym.sym=SDLK_BACKSPACE;
  detail_evento(&e); assert(!pessoaAberta && !saindo);
  // Dedicated recommendations are focusable for both media kinds. Route
  // opaque TMDB ids using the current media kind, without performing I/O.
  for (int serie = 0; serie < 2; serie++) {
    snprintf(c.tipo, sizeof c.tipo, "%s", serie ? "series" : "movie");
    cat_definir_tudo(&c, 1, NULL, 0); idx = 0;
    carro = 0; nivel = 1; foco.fileira = SEC_RELACIONADOS; foco.coluna = 0;
    rotaId = 0; rotaTipo[0] = 0;
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
    detail_evento(&e);
    e.type = SDL_KEYUP; detail_evento(&e);
    assert(rotaId == 42);
    assert(!strcmp(rotaTipo, serie ? "tv" : "movie"));
  }
  // MENU OVER THE TITLE PAGE (owner 03/10, "pode abrir por cima"): Left at the
  // start of a row asks for the side menu WITHOUT closing the page, and the
  // menu being open on top keeps focus/scroll as they were.
  snprintf(c.tipo, sizeof c.tipo, "movie");
  cat_definir_tudo(&c, 1, NULL, 0); idx = 0; aberto = 1;
  carro = 0; maisAcoes = 0; nivel = 0; botao = 0; saindo = 0; pediuMenu = 0;
  pessoaAberta = 0; colListaAberta = 0;
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_LEFT;
  detail_evento(&e);
  assert(!saindo && detail_pediu_menu() && !detail_pediu_menu());
  nivel = 1; foco.fileira = SEC_RELACIONADOS; foco.coluna = 0; scrollY = 321.0f;
  detail_evento(&e);
  assert(!saindo && detail_pediu_menu());
  detail_sob_menu(1); assert(detail_sob_menu_ativo());
  detail_sob_menu(0); assert(!detail_sob_menu_ativo());
  assert(!saindo && nivel == 1 && foco.fileira == SEC_RELACIONADOS &&
         foco.coluna == 0 && scrollY == 321.0f);
  // Person filmography grid: column 0 asks for the menu, other columns walk.
  pessoaAberta = 1; pessoaFoco = PES_POR_LINHA;   // row 2, column 0
  detail_evento(&e);
  assert(pessoaAberta && pessoaFoco == PES_POR_LINHA && !saindo && detail_pediu_menu());
  pessoaFoco = 1; detail_evento(&e);
  assert(pessoaFoco == 0 && !detail_pediu_menu());
  pessoaAberta = 0;
  // Collection list (vertical): Left asks for the menu, the list stays open.
  colListaAberta = 1; detail_evento(&e);
  assert(colListaAberta && !saindo && detail_pediu_menu());
  colListaAberta = 0;
  // #407: CIMA no alto da pagina de titulo (sobre os botoes) NAO abre o painel
  // de Salvos/Avisos. O atalho existia para controles sem tecla AZUL, mas na
  // Samsung o CH+ ja abre o painel (e o relogio continua clicavel); a seta
  // disparando um menu era o que o relato pedia para sair.
  nivel = 0; focoAmigos = 0; botao = 0; saindo = 0;
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_UP;
  detail_evento(&e);
  assert(!detail_pediu_social() && !saindo && nivel == 0 && botao == 0);
  // Back never carries a menu request.
  nivel = 0; e.key.keysym.sym = SDLK_ESCAPE; detail_evento(&e);
  assert(saindo && !detail_pediu_menu());
  // "MAIS OPCOES" (06/10/2026): Trailer, Explorar, Trocar arte and Recomendar
  // left the row. The LAST circular is "...", OK on it opens the island; the
  // row of a movie is Play, +, watched, sources, "..." (5, no resume).
  { CatItem m = {0};
    int i, achou = -1;
    snprintf(m.tipo, sizeof m.tipo, "movie");
    snprintf(m.titulo, sizeof m.titulo, "Prisoners");
    snprintf(m.imdb, sizeof m.imdb, "tt1392214");
    cat_definir_tudo(&m, 1, NULL, 0); idx = 0;
    carro = 0; saindo = 0; nivel = 0; pessoaAberta = 0; maisAcoes = 1;
    assert(temExplorar() && temMais());
    assert(nBotoesTodos() == 5 && acaoEm(nBotoesTodos() - 1) == ACAO_MAIS);
    for (i = 0; i < nBotoesTodos(); i++) {
      int ac = acaoEm(i);
      assert(ac != ACAO_TRAILER && ac != ACAO_EXPLORAR && ac != ACAO_ARTE && ac != ACAO_RECOMENDAR);
    }
    botao = nBotoesTodos() - 1;
    SDL_Delay(2);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; detail_evento(&e);
    e.type = SDL_KEYUP; detail_evento(&e);
    assert(detmais_aberto() && !detail_pediu_explorar());
    for (i = 0; i < detmais_n(); i++) if (detmais_acao(i) == DMAIS_EXPLORAR) achou = i;
    // Fixed order, only what this title has: no Recomendar without the service.
    assert(achou >= 0 && detmais_acao(detmais_n()) == -1);
    for (i = 1; i < detmais_n(); i++) assert(detmais_acao(i) > detmais_acao(i - 1));
    assert(temArte() && detmais_n() == (temTrailer() ? 1 : 0) + 2 + (temRecomendar() ? 1 : 0));
    // Back closes the island and keeps the focus on "..." (the page stays).
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; detail_evento(&e);
    assert(!detmais_aberto() && !saindo && botao == nBotoesTodos() - 1);
    // Reopen, walk down to Explorar, OK: the router gets one request.
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; detail_evento(&e);
    e.type = SDL_KEYUP; detail_evento(&e);
    // The KEYUP of the OK that opened it must not pick a row.
    assert(detmais_aberto() && detmais_foco() == 0);
    for (i = 0; i < achou; i++) { e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN; detail_evento(&e); }
    assert(detmais_foco() == achou);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; detail_evento(&e);
    e.type = SDL_KEYUP; detail_evento(&e);
    assert(!detmais_aberto());
    assert(detail_pediu_explorar() && !detail_pediu_explorar());
    snprintf(m.tipo, sizeof m.tipo, "tv");
    cat_definir_tudo(&m, 1, NULL, 0);
    assert(!temExplorar() && !temMais()); }
  puts("PASS: title/navigation and focus-only action group");
  return 0;
}
