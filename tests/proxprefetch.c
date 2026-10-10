// Posplay + cache + addons reais; transporte controlado, sem rede/video/debrid.
#define main teste_vod_existente
#define rede_baixar transporte_fixture
#include "fontecache_vod.c"
#undef rede_baixar
#undef main
#include "posplay.h"
#include "catalogo.h"
#include "intro.h"
#include "idbase.h"

static CatItem titulo;
static CatEp episodios[2];
static int preparar = 1, atualT = 1, atualE = 1, velocidade = 100;
static double posicao, duracao = 1200, credito;
static _Atomic int bloquear, entrou;
char *rede_baixar(const char *url, int timeout) {
  atomic_store(&entrou, 1);
  while (atomic_load(&bloquear)) usleep(1000);
  return transporte_fixture(url, timeout);
}
const CatItem *cat_item(int i) { (void)i; return &titulo; }
int cat_indice_vivo(int i, const char *id) { (void)id; return i; }
int cat_n_episodios(int i) { (void)i; return 2; }
const CatEp *cat_episodio(int i, int e) { (void)i; return e >= 0 && e < 2 ? &episodios[e] : NULL; }
int cat_id_stream(int i, int t, int e, char *dst, unsigned tam) {
  char base[64]; (void)i;
  if (e == 2 && episodios[1].vid[0]) { snprintf(dst, tam, "%s", episodios[1].vid); return 1; }
  idbase_copiar(titulo.imdb, base, sizeof base);
  snprintf(dst, tam, "%s:%d:%d", base, t, e); return 1;
}
int cat_indice_por_imdb(const char *id) { (void)id; return -1; }
void player_episodio_atual(int *t, int *e) { *t = atualT; *e = atualE; }
int player_velocidade_efetiva(void) { return velocidade; }
void player_aprender_creditos(void) {}
int ajustes_fonte_preparar(void) { return preparar; }
double video_creditos(void) { return credito; }
// A regra temporal real e exercitada tambem em tests/posplay.sh.
int player_janela_proximo(double antecedencia) {
  return posicao + antecedencia * velocidade / 100.0 >= (credito > 1 ? credito : duracao - 50);
}
int extras_n_relacionados(void) { return 0; }
const char *extras_relacionado_imdb(int i) { (void)i; return ""; }
void desc_pedir_titulo(const char *id) { (void)id; }

static void quadro(double p, int serie) {
  posicao = p;
  posplay_atualizar(1, SDL_GetTicks(), p, duracao, serie, 0, player_janela_proximo(0));
}
static void iniciar(void) {
  posplay_fechar(); fontecache_vod_limpar();
  stream_definir_lista(NULL, 0); // cada cenario comeca no episodio anterior
  for (int i = 0; i < 10; i++) quadro(0, 1);
}
static void esperarPedido(int n) {
  for (int i = 0; i < 2000 && pedidos < n; i++) usleep(1000);
  assert(pedidos == n);
}
static void esperarFim(void) {
  int v = 1;
  for (int i = 0; i < 4000 && v; i++) {
    pthread_mutex_lock(&trava); v = vivo; pthread_mutex_unlock(&trava);
    if (v) usleep(1000);
  }
  assert(!v);
}
static int cacheProximo(void) {
  FontecacheEscopo e = escopo(); Stream *l = NULL; int n = 0; Uint32 idade;
  int r = fontecache_vod_pegar("tt42:1:2", "series", "", &e, &l, &n, &idade);
  free(l); return r == FC_ACERTO ? n : 0;
}
static void dispensar(int tecla) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = tecla;
  assert(posplay_evento(&e));
}
static void iniciarBloqueado(void) {
  iniciar(); atomic_store(&entrou, 0); atomic_store(&bloquear, 1);
  quadro(1140, 1);
  for (int i = 0; i < 2000 && !atomic_load(&entrou); i++) usleep(1000);
  assert(entrou);
}
static int extraModo;
static int extraSilenciosaModo, extraSilenciosaChamadas;
static int extraSilenciosaAtiva(void) { return extraSilenciosaModo != 0; }
static int extraSilenciosa(const char *id, const char *tipo, int (*c)(void *), void *ctx,
                          OrigemAviso aviso, void *u, void *saida) {
  Stream **l = saida;
  (void)id; (void)tipo; (void)c; (void)ctx;
  extraSilenciosaChamadas++;
  *l = NULL;
  if (extraSilenciosaModo == 1) return 0; // falha HTTP/TMDB, sem callbacks
  *l = calloc(1, sizeof **l); assert(*l);
  aviso(u, 2, "Recuperada", 1, NULL, 0);
  aviso(u, 2, "Recuperada", 2, *l, 1);
  return 1;
}
static int extraAtiva(void) { return extraModo != 0; }
static int extraFixture(const char *id, const char *tipo, int (*c)(void *), void *ctx,
                        OrigemAviso aviso, void *u, void *saida) {
  Stream **l = saida;
  (void)id; (void)tipo; (void)c; (void)ctx;
  *l = calloc(1, sizeof **l); assert(*l);
  aviso(u, 0, "Rapido", 1, NULL, 0);
  aviso(u, 1, "Lento", 1, NULL, 0);
  aviso(u, 0, "Rapido", 2, *l, 1);
  // Corte deixa Lento pendente (1), falha explicita (3), ou sucesso vazio (2).
  if (extraModo != 1) aviso(u, 1, "Lento", extraModo == 2 ? 3 : 2, NULL, 0);
  return 1;
}

static void testarContagem(void) {
  for (velocidade = 50; velocidade <= 200; velocidade *= 2) {
    for (int marcador = 0; marcador < 2; marcador++) {
      credito = marcador ? 1050 : 0;
      iniciar();
      int antes = pedidos, t, e;
      double inicio = (marcador ? 1050 : 1150) - 10 * velocidade / 100.0;
      quadro(inicio, 1); esperarFim(); assert(cacheProximo() == 2);
      for (double p = inicio + velocidade / 100.0; p <= 1200; p += velocidade / 100.0) {
        atomic_fetch_add(&relogio, 1000);
        quadro(p, 1); esperarFim();
      }
      assert(posplay_pediu_episodio(&t, &e) && t == 1 && e == 2);
      assert(cacheProximo() == 2); // a resposta original expirou ha >30 s
      int antesBusca = pedidos;
      posplay_fechar(); buscarTitulo("tt42:1:2", "series");
      assert(pedidos == antesBusca && nLista == 2);
      assert(pedidos == antes + 2); // so uma renovacao perto do fim, nao polling
    }
  }
  velocidade = 100; credito = 0;
  // Seek direto ao fim: a primeira resposta ja cobre a transicao, sem renovar.
  iniciar();
  int antes = pedidos;
  for (int p = 1190; p <= 1200; p++) {
    atomic_fetch_add(&relogio, 1000);
    quadro(p, 1); esperarFim();
  }
  assert(cacheProximo() == 2 && pedidos == antes + 1);
  puts("PASS R2 TTL: contagem com/sem marcador, 0.5x/1x/2x e preparo tardio sem HTTP redundante");
}
static void testarExtras(void) {
  addons_definir_origem_extra(extraFixture, extraAtiva);
  addons_definir_origem_extra(extraSilenciosa, extraSilenciosaAtiva);
  for (int semAddon = 0; semAddon < 2; semAddon++) {
    if (semAddon) { addons_esquecer(); assert(addons_n() == 0); }
    for (extraModo = 1; extraModo <= 3; extraModo++) {
      iniciar(); quadro(1140, 1); esperarFim();
      if (extraModo == 3) assert(cacheProximo() == (semAddon ? 1 : 3));
      else assert(!cacheProximo()); // Rapido + Lento pendente nao e resposta completa
      if (extraModo != 3) {
        // A busca real ainda publica as fontes parciais, mas nunca as cacheia.
        buscarTitulo("tt42:1:2", "series");
        assert(nLista == (semAddon ? 1 : 3) && !cacheProximo());
      }
    }
    extraModo = 3; // outra origem conclui: nao pode encobrir a falha silenciosa
    extraSilenciosaModo = 1; extraSilenciosaChamadas = 0;
    iniciar(); quadro(1140, 1); esperarFim();
    if (cacheProximo()) {
      fputs("FAIL R3: origem extra retornou 0 sem callbacks e cache parcial foi aceito\n", stderr);
      abort();
    }
    assert(extraSilenciosaChamadas == 1);
    buscarTitulo("tt42:1:2", "series");
    assert(extraSilenciosaChamadas == 2 && nLista == (semAddon ? 1 : 3) && !cacheProximo());
    extraSilenciosaModo = 2; // recupera dentro do TTL, sem adiantar o relogio
    buscarTitulo("tt42:1:2", "series");
    assert(extraSilenciosaChamadas == 3 && nLista == (semAddon ? 2 : 4));
    assert(cacheProximo() == nLista);
    extraSilenciosaModo = 0;
  }
  extraModo = 0; listaAddon();
  puts("PASS R2 extras: corte/falha nao viram cache; resposta completa, inclusive vazia, permite cache");
  puts("PASS R3: retorno 0 sem callbacks nao cacheia; busca real recupera a origem dentro do TTL, com/sem addons");
}

int main(void) {
  int antes;
  strcpy(titulo.imdb, "tt42:1:1"); strcpy(titulo.tipo, "series");
  titulo.temporada = titulo.episodio = 1;
  for (int i = 0; i < 2; i++) {
    episodios[i].temporada = 1; episodios[i].episodio = i + 1;
    strcpy(episodios[i].data, "2020-01-01");
  }
  listaAddon();
  const char *caso = getenv("R2_CASO");
  if (!caso || !strcmp(caso, "ttl")) testarContagem();
  if (!caso || !strcmp(caso, "extras")) testarExtras();
  if (caso) { addons_encerrar(); free(listaAtiva); return 0; }
  pedidos = 0; stream_definir_lista(NULL, 0);
  iniciar();
  quadro(1139, 1); assert(pedidos == 0);
  quadro(1140, 1); esperarPedido(1); esperarFim();
  assert(cacheProximo() == 2 && nLista == 0); // nenhuma publicacao no player
  for (int i = 0; i < 30; i++) quadro(1145, 1);
  assert(pedidos == 1);
  // O fluxo real fecha/abre o player ANTES de addons_buscar do proximo.
  quadro(1150, 1); dispensar(SDLK_RETURN);
  { int t, e; assert(posplay_pediu_episodio(&t, &e) && t == 1 && e == 2); }
  posplay_fechar(); posplay_fechar();
  buscarTitulo("tt42:1:2", "series");
  assert(pedidos == 1 && nLista == 2 && !strcmp(listaAlvo, "tt42:1:2"));
  puts("PASS: 10 s antes, uma vez, cache completo e busca real sem HTTP");

  antes = pedidos; credito = 1050; iniciar();
  quadro(1039, 1); assert(pedidos == antes);
  quadro(1040, 1); esperarPedido(antes + 1); esperarFim();
  assert(cacheProximo() == 2);
  credito = 0;
  puts("PASS: 10 s antes do marcador de creditos");

  antes = pedidos; preparar = 0; iniciar(); quadro(1140, 1);
  assert(pedidos == antes); preparar = 1;
  iniciar(); quadro(1140, 0); assert(pedidos == antes);
  iniciar(); strcpy(episodios[1].data, "2999-01-01"); quadro(1140, 1);
  assert(pedidos == antes);
  episodios[1].data[0] = 0; quadro(1140, 1); assert(pedidos == antes);
  strcpy(episodios[1].data, "2020-01-01");
  atualE = 2; iniciar(); quadro(1140, 1); assert(pedidos == antes); atualE = 1;
  iniciar(); posplay_fechar(); quadro(1140, 1); assert(pedidos == antes);
  strcpy(titulo.imdb, "jf:server:item"); iniciar(); quadro(1140, 1);
  assert(pedidos == antes); strcpy(titulo.imdb, "tt42:1:1");
  puts("PASS: ajuste, filme, futuro, data ausente, ultimo episodio, duracao instavel e servidor privado");

  // Fecha/dispensa/troca com HTTP ainda no ar: resultado tardio descartado.
  for (int modo = 0; modo < 8; modo++) {
    iniciarBloqueado();
    if (modo == 0) posplay_fechar();
    if (modo == 1 || modo == 2) { quadro(1150, 1); dispensar(modo == 1 ? SDLK_BACKSPACE : SDLK_DOWN); }
    if (modo == 3) { atualE = 2; quadro(1140, 1); }
    if (modo == 4) { addons_alternar(0); quadro(1140, 1); }
    if (modo == 5) { perfil = 2; quadro(1140, 1); }
    if (modo == 6) { preparar = 0; quadro(1140, 1); }
    if (modo == 7) { conta = "conta-b"; quadro(1140, 1); }
    atomic_store(&bloquear, 0); esperarFim();
    assert(cacheProximo() == 0);
    if (modo == 1 || modo == 2 || modo == 4 || modo == 5) {
      antes = pedidos; quadro(1150, 1); assert(pedidos == antes);
    }
    atualE = 1; perfil = 1; preparar = 1; conta = "conta-a";
    if (modo == 4) addons_alternar(0);
  }
  puts("PASS: fechamento, Voltar, Baixo, episodio, addons, perfil e ajuste cancelam resposta tardia");

  iniciar(); quadro(1140, 1); esperarFim(); assert(cacheProximo() == 2);
  quadro(1150, 1); dispensar(SDLK_BACKSPACE); assert(cacheProximo() == 0);
  antes = pedidos; quadro(1100, 1); quadro(1140, 1); assert(pedidos == antes);
  puts("PASS: dispensa remove cache pronto e seek nao duplica o gatilho");

  // Busca real nova cede o prefetch, sem adotar/trocar o alvo da UI.
  iniciarBloqueado();
  stream_definir_alvo("tt99"); addons_buscar("tt99", "movie");
  atomic_store(&bloquear, 0); esperarFim();
  for (int i = 0; i < 4000 && addons_estado() == ADD_BUSCANDO; i++) usleep(1000);
  assert(addons_estado() == ADD_PRONTO && !strcmp(listaAlvo, "tt99"));
  assert(cacheProximo() == 0);
  puts("PASS: prefetch cede a busca real sem alterar seu alvo");

  iniciar(); atomic_store(&bloquear, 1); atomic_store(&entrou, 0);
  stream_definir_alvo("tt100"); addons_buscar("tt100", "movie");
  antes = pedidos; quadro(1140, 1);
  pthread_mutex_lock(&trava); assert(!vivo); pthread_mutex_unlock(&trava);
  atomic_store(&bloquear, 0);
  for (int i = 0; i < 4000 && addons_estado() == ADD_BUSCANDO; i++) usleep(1000);
  assert(pedidos == antes + 1 && !strcmp(listaAlvo, "tt100"));
  quadro(1140, 1); esperarFim(); assert(cacheProximo() == 2);
  atomic_fetch_add(&relogio, FONTECACHE_VOD_VALIDADE_MS);
  assert(cacheProximo() == 0);
  puts("PASS: aguarda busca real ocupada; TTL original preservado");

  // O ID fornecido pelo catalogo e opaco: nao compor IMDb/T/E aqui.
  strcpy(titulo.imdb, "kitsu:42"); strcpy(episodios[1].vid, "kitsu:42:99");
  iniciar(); antes = pedidos; quadro(1140, 1); esperarFim();
  buscarTitulo("kitsu:42:99", "series");
  assert(pedidos == antes + 1 && !strcmp(listaAlvo, "kitsu:42:99"));
  episodios[1].vid[0] = 0; strcpy(titulo.imdb, "tt42:1:1");
  puts("PASS: ID opaco do catalogo reutilizado pela busca real");

  fontesRede = 0; iniciar(); quadro(1140, 1); esperarFim(); assert(!cacheProximo());
  fontesRede = FONTECACHE_VOD_BYTES / sizeof(Stream) + 1;
  iniciar(); quadro(1140, 1); esperarFim(); assert(!cacheProximo()); fontesRede = 2;
  { AddonRemoto lista[2] = {0};
    unsigned avisos = addons_fora_do_ar(NULL, 0);
    strcpy(lista[0].url, "https://addon.invalid/manifest.json"); lista[0].ativo = 1;
    strcpy(lista[1].url, "https://erro.invalid/manifest.json"); lista[1].ativo = 1;
    addons_definir_lista(lista, 2); iniciar(); quadro(1140, 1); esperarFim();
    assert(!cacheProximo() && addons_fora_do_ar(NULL, 0) == avisos);
  }
  puts("PASS: vazio, lista acima do teto e resposta parcial nao entram no cache");
  addons_encerrar(); free(listaAtiva);
  puts("proxprefetch: PASS");
  return 0;
}
