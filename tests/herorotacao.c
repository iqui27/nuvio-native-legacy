// ROTACAO AUTOMATICA DO DESTAQUE: o que a segura e o que a desliga.
//
// Dono, 30/09: "se eu voltar um titulo no destaque, a rotacao para" e "a
// rotacao nao troca o titulo com o trailer tocando". Este teste dirige
// home_evento/home_atualizar como o laco principal (sem janela nem TV, como
// tests/fimfileira.c: inclui src/home.c) e olha o DESEJO de troca
// (`heroDesejado`), que e o que o carrossel escreve — quem o efetiva e o
// desenho, e o desenho precisa de GL.
//
// O que fica de fora: zerar heroAutoDesligado na volta a home mora em
// home_desenhar (o `reentra`), que tambem precisa de GL; conferido no
// codigo, nao aqui.
#include <assert.h>
int ctx_aberto(void) { return 0; }   // home.c asks whether the context menu is open
#include "../src/home.c"
// heroAquecerVizinhos (home_atualizar) pede a arte dos vizinhos: sem textura aqui.
unsigned long tex_hash_public(const char *c) { (void)c; return 0; }
GLuint tex_obter_hero_quente(const char *c) { (void)c; return 0; }
// O destaque pede a sinopse dos candidatos assim que monta o conjunto.
static int sinPedidos, sinUltimoN;
void desc_sinopse_hero(const int *idx, int n) { (void)idx; sinPedidos++; sinUltimoN = n; }

void cachearte_marcar_grupo(int grupo, const char *url, int variante, int essencial, int emUso) {
  (void)grupo; (void)url; (void)variante; (void)essencial; (void)emUso;
}
void cachearte_limpar_referencias_grupo(int grupo) { (void)grupo; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int grupo, const char *url, float larg, int essencial, int emUso) {
  (void)grupo; (void)url; (void)larg; (void)essencial; (void)emUso;
}
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
int   dados_gravar_leve(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
int   perfis_ativo(void) { return 1; }
void dados_marcar_sujo(int leve) { (void)leve; }
const char *ling_legenda(void) { return ""; }
const char *ling_audio(void) { return ""; }
const char *ling_conta_legenda_valor(void) { return ""; }
const char *ling_conta_legenda2_valor(void) { return ""; }
const char *ling_conta_audio_valor(void) { return ""; }
void ling_conta_legenda(const char *v) { (void)v; }
void ling_conta_legenda2(const char *v) { (void)v; }
void ling_conta_audio(const char *v) { (void)v; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }
int  tex_falhou(const char *u) { (void)u; return 0; }
int  tex_largura_fonte(const char *u) { (void)u; return 0; }
float tex_aspecto(const char *u) { (void)u; return 0.0f; }
const char *tex_arquivo(const char *u) { (void)u; return NULL; }
int  player_aberto(void) { return 0; }
int  detail_aberto(void) { return 0; }
// O trailer e controlado pelo teste: aberto+tocando = o hero nao pode girar.
static int trAberto, trTocando;
int  trailer_aberto(void) { return trAberto; }
int  trailer_tocando(void) { return trAberto && trTocando; }
void ctx_abrir(int indice) { (void)indice; }
void ctx_fileira(const char *c, const char *t) { (void)c; (void)t; }
void ctx_dispensar_retomar(int on) { (void)on; }
void ctx_abrir_fileira(const char *c, const char *t) { (void)c; (void)t; }
void vertudo_abrir(const char *b, const char *t, const char *c, const char *ti) {
  (void)b; (void)t; (void)c; (void)ti;
}
void vertudo_colecao(const ColFolder *f) { (void)f; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }

// O relogio do teste parte do SDL_GetTicks: as teclas gravam heroTrocaEm e
// heroUltTecla com o relogio de verdade, o laco recebe o do teste.
static Uint32 relogio;
static void quadro(int quantos) {
  for (int i = 0; i < quantos; i++) { relogio += 16; home_atualizar(0.016f, relogio); }
}
static void avanca_s(int s) { quadro(s * 1000 / 16); }
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
  quadro(1);
}

int main(void) {
  static CatItem it[12];
  static CatFileira fs[2];
  int i;
  for (i = 0; i < 12; i++) {
    snprintf(it[i].imdb, sizeof it[i].imdb, "tt%07d", 2000 + i);
    snprintf(it[i].titulo, sizeof it[i].titulo, "T%d", i);
    snprintf(it[i].tipo, sizeof it[i].tipo, "movie");
    snprintf(it[i].backdrop, sizeof it[i].backdrop, "https://b.invalid/%d.jpg", i);
  }
  snprintf(fs[0].chave, sizeof fs[0].chave, "continue_watching");
  snprintf(fs[0].titulo, sizeof fs[0].titulo, "Continuar");
  fs[0].ini = 0; fs[0].n = 6;
  snprintf(fs[1].chave, sizeof fs[1].chave, "catalogo_1");
  snprintf(fs[1].titulo, sizeof fs[1].titulo, "Lista");
  snprintf(fs[1].base, sizeof fs[1].base, "https://example.invalid/addon");
  snprintf(fs[1].tipo, sizeof fs[1].tipo, "movie");
  snprintf(fs[1].catId, sizeof fs[1].catId, "id1");
  fs[1].ini = 6; fs[1].n = 6;
  cat_definir_tudo(it, 12, fs, 2);
  relogio = SDL_GetTicks() + 1000;
  quadro(2);
  assert(focoHero && heroNLista() >= 3);

  assert(sinPedidos >= 1 && sinUltimoN == heroNLista());
  puts("ok  conjunto do destaque montado: a sinopse dos candidatos e pedida");

  // (1) Parada, a rotacao gira: 12 s de ocio + o intervalo de 7 s.
  heroDesejado = -1;
  avanca_s(20);
  assert(heroDesejado >= 0);
  puts("ok  destaque parado: o carrossel escolhe o proximo titulo");

  // (2) AVANCAR a mao NAO desliga: o carrossel volta depois do ocio.
  heroDesejado = -1;
  tecla(SDLK_RIGHT);
  assert(!heroAutoDesligado);
  heroDesejado = -1;
  avanca_s(20);
  assert(heroDesejado >= 0);
  puts("ok  avancar (direita) so reinicia os relogios: a rotacao volta");

  // (3) VOLTAR um titulo (esquerda, fora da primeira posicao) desliga a visita.
  tecla(SDLK_RIGHT);          // sai da posicao 0, senao a esquerda abre o menu
  tecla(SDLK_LEFT);
  assert(heroAutoDesligado);
  heroDesejado = -1;
  avanca_s(120);
  assert(heroDesejado < 0);
  puts("ok  voltar um titulo desliga a rotacao (2 min parado, nada troca)");

  // (4) Uma nova visita (o que home_desenhar faz no `reentra`) religa.
  heroAutoDesligado = 0;
  heroDesejado = -1;
  avanca_s(20);
  assert(heroDesejado >= 0);
  puts("ok  nova visita a home religa a rotacao");

  // (5) NAO TROCA COM O TRAILER TOCANDO: a mesma situacao, com o trailer no ar.
  ajustes_aplicar_blob("{\"trailerHero\":true}");
  heroTrailerItem = heroAtual;
  { const CatItem *ci = cat_item_exato(heroAtual);
    assert(ci);
    snprintf(heroTrailerImdb, sizeof heroTrailerImdb, "%s", ci->imdb); }
  trAberto = 1; trTocando = 1;
  heroDesejado = -1;
  heroTrocaEm = 0;
  avanca_s(60);
  if (ajustes_trailer_hero()) {
    assert(heroDesejado < 0);
    puts("ok  trailer tocando: o carrossel nao troca o titulo");
  } else puts("--  ajuste do trailer do destaque desligado neste ambiente: caso (5) nao se aplica");
  trTocando = 0;   // acabou/fechou: libera
  trAberto = 0;
  heroTrailerTentado = 1;
  avanca_s(20);
  assert(heroDesejado >= 0);
  puts("ok  trailer terminado: a rotacao segue");

  // (6) PRE-BUSCA COM O TRAILER SENDO PROCURADO (C9, 08/10, 2.0.3). Cada titulo
  // do carrossel tem a busca do trailer, e durante ela heroTrailerSegurando e
  // verdadeiro: a pre-busca (3 s antes da troca) nunca saia e o log da TV nao
  // tinha uma unica linha `pre-busca`. Agora a busca nao a impede; o trailer
  // TOCANDO continua impedindo (pode durar minutos).
  if (ajustes_trailer_hero()) {
    const CatItem *ci;
    heroAutoDesligado = 0;
    heroTrailerItem = heroAtual;
    ci = cat_item_exato(heroAtual);
    assert(ci);
    snprintf(heroTrailerImdb, sizeof heroTrailerImdb, "%s", ci->imdb);
    heroTrailerTentado = 0;
    heroTrailerDesde = relogio;
    trAberto = 0; trTocando = 0;            // so procurando, nada no ar
    heroDesejado = -1;
    heroTrocaEm = relogio + 2000;           // dentro dos NV_HERO_PRE_MS finais
    quadro(1);
    assert(heroTrailerSegurando(relogio));  // o carrossel esta segurado...
    assert(heroPreItem >= 0 && heroPreItem != heroAtual);   // ...e a pre-busca sai
    puts("ok  busca do trailer segurando o carrossel: a pre-busca do proximo sai");
    trAberto = 1; trTocando = 1;            // agora o trailer toca
    heroTrocaEm = relogio + 2000;
    quadro(1);
    assert(heroPreItem < 0);
    puts("ok  trailer tocando: sem pre-busca (arte ficaria parada)");
    trAberto = 0; trTocando = 0;
    heroTrailerTentado = 1;
  } else puts("--  ajuste do trailer do destaque desligado: caso (6) nao se aplica");

  // (7) HOME ESCONDIDA POR TRAS DA ESCOLHA DE PERFIL (C9, 08/10, 2.0.3): app.c
  // chama home_atualizar com a tela de escolha na frente, o desenho NUNCA roda e
  // quem efetiva a troca e o desenho. O carrossel marcava heroDesejado a cada
  // 7 s e ele ficava preso (log: 38 min, 0 `[hero] espera`, 327 `desejado=322`).
  // Escondida, a home nao escolhe nada e a diagnostica nao fala; ao voltar, o
  // carrossel recomeca do relogio rearmado.
  heroAutoDesligado = 0;
  heroTrailerTentado = 1;
  trAberto = 0; trTocando = 0;
  heroDesejado = -1;
  heroPendente = heroAtual;
#ifndef SEM_OCULTA
  home_oculta(1);
#endif
  avanca_s(60);
  assert(heroDesejado < 0);
  puts("ok  home escondida (escolha de perfil): o carrossel nao marca troca");
#ifndef SEM_OCULTA
  home_oculta(0);
#endif
  avanca_s(30);
  assert(heroDesejado >= 0);
  puts("ok  home de volta: a rotacao recomeca");
  puts("herorotacao: PASS");
  return 0;
}
