// A ARTE ESCOLHIDA A MAO (#142): a tabela, o disco por perfil, o logout e a
// PRECEDENCIA sobre a politica automatica de artehero.c.
//
// Sem SDL, sem GL e sem rede: arteescolha.c so fala com dados.c, e artehero.c
// recebe a tabela pelas funcoes registradas (artehero_definir_escolha), do
// mesmo jeito que o main registra.
//
// ONDE ELE ESCREVE: so em NUVIO_DADOS, e ele confere antes de gravar — sem
// isto o teste escreveria no ~/.nuvio de quem o roda.
//
//   bash tests/arteescolha.sh
#include "arteescolha.h"
#include "artehero.h"
#include "dados.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CONFERE(c) do { if (!(c)) { printf("FALHOU %s:%d  %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)
#define IGUAL(a, b) do { const char *x_ = (a), *y_ = (b); \
  if (!x_ || !y_ || strcmp(x_, y_)) { printf("FALHOU %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, \
    x_ ? x_ : "(null)", y_ ? y_ : "(null)"); falhas++; } } while (0)

// tex_falhou de mentira: a url que esta aqui "deu 404".
static char morta[512];
static int falhouDeMentira(const char *u) { return morta[0] && u && !strcmp(u, morta); }

static int arquivoExiste(const char *nome) {
  char *b = dados_ler(nome);
  if (!b) return 0;
  free(b);
  return 1;
}

static CatItem filme(void) {
  CatItem it;
  memset(&it, 0, sizeof it);
  snprintf(it.imdb, sizeof it.imdb, "tt0133093");
  snprintf(it.tipo, sizeof it.tipo, "movie");
  snprintf(it.titulo, sizeof it.titulo, "Matrix");
  snprintf(it.meta, sizeof it.meta, "1999 · 2 h 16 min");
  snprintf(it.backdrop, sizeof it.backdrop,
           "https://image.tmdb.org/t/p/w1280/catalogo.jpg");
  snprintf(it.logo, sizeof it.logo, "https://image.tmdb.org/t/p/original/logoCat.png");
  return it;
}

static void chaves(void) {
  char k[40];
  arteesc_chave("tt0903747:2:3", k, sizeof k);  IGUAL(k, "tt0903747");
  arteesc_chave("tt0903747", k, sizeof k);      IGUAL(k, "tt0903747");
  arteesc_chave("kitsu:7442:3", k, sizeof k);   IGUAL(k, "kitsu:7442");
  arteesc_chave("tmdb:m603", k, sizeof k);      IGUAL(k, "tmdb:m603");
  arteesc_chave("", k, sizeof k);               IGUAL(k, "");
  arteesc_chave(NULL, k, sizeof k);             IGUAL(k, "");
}

static void tabela(void) {
  arteesc_definir_perfil(1);
  CONFERE(arteesc_n() == 0);
  CONFERE(arteesc_fundo("tt0133093") == NULL);
  CONFERE(arteesc_definir_fundo("tt0133093", "https://image.tmdb.org/t/p/w1280/escolhido.jpg"));
  IGUAL(arteesc_fundo("tt0133093"), "https://image.tmdb.org/t/p/w1280/escolhido.jpg");
  CONFERE(arteesc_logo("tt0133093") == NULL);          // so o fundo foi escolhido
  // A MESMA escolha de novo nao muda nada (e nao grava).
  CONFERE(!arteesc_definir_fundo("tt0133093", "https://image.tmdb.org/t/p/w1280/escolhido.jpg"));
  // Episodio acha a escolha da serie.
  CONFERE(arteesc_definir_logo("tt0903747", "https://image.tmdb.org/t/p/w500/logoSerie.png"));
  IGUAL(arteesc_logo("tt0903747:5:14"), "https://image.tmdb.org/t/p/w500/logoSerie.png");
  CONFERE(arteesc_n() == 2);
  // Url estranha nao entra (linha estragada nao vira pedido de textura).
  CONFERE(!arteesc_definir_fundo("tt1", "javascript:alert(1)"));
  CONFERE(arteesc_n() == 2);
  // AUTOMATICO apaga a escolha; com as duas vazias a linha sai.
  CONFERE(arteesc_definir_logo("tt0903747", NULL));
  CONFERE(arteesc_logo("tt0903747") == NULL);
  CONFERE(arteesc_n() == 1);
  CONFERE(!arteesc_definir_logo("tt0903747", ""));     // ja era Automatico
  CONFERE(arteesc_definir_logo("tt0133093", "https://images.metahub.space/logo/medium/tt0133093/img"));
  CONFERE(arteesc_n() == 1);
  CONFERE(arquivoExiste("arte-escolhida-p1.txt"));
}

static void perfis(void) {
  // PERFIL 2 NAO VE A ESCOLHA DO PERFIL 1.
  arteesc_definir_perfil(2);
  CONFERE(arteesc_fundo("tt0133093") == NULL);
  CONFERE(arteesc_definir_fundo("tt0133093", "https://image.tmdb.org/t/p/w1280/doFilho.jpg"));
  // Voltar ao 1 RELE DO DISCO (arranque frio da tabela).
  arteesc_definir_perfil(1);
  IGUAL(arteesc_fundo("tt0133093"), "https://image.tmdb.org/t/p/w1280/escolhido.jpg");
  IGUAL(arteesc_logo("tt0133093"), "https://images.metahub.space/logo/medium/tt0133093/img");
  arteesc_definir_perfil(2);
  IGUAL(arteesc_fundo("tt0133093"), "https://image.tmdb.org/t/p/w1280/doFilho.jpg");
  // Linha estragada no arquivo nao derruba a leitura das outras.
  CONFERE(dados_gravar("arte-escolhida-p3.txt",
    "# nuvio arte v1\n"
    "tt1\tlixo sem protocolo\t\n"
    "\n"
    "tt2\thttps://image.tmdb.org/t/p/w1280/bom.jpg\t\r\n"
    "sem-campos\n"));
  arteesc_definir_perfil(3);
  CONFERE(arteesc_n() == 1);
  IGUAL(arteesc_fundo("tt2"), "https://image.tmdb.org/t/p/w1280/bom.jpg");
}

static void cheia(void) {
  char id[32], url[128];
  int i;
  arteesc_definir_perfil(4);
  for (i = 0; i < ARTEESC_MAX; i++) {
    snprintf(id, sizeof id, "tt%07d", i);
    snprintf(url, sizeof url, "https://image.tmdb.org/t/p/w1280/%d.jpg", i);
    CONFERE(arteesc_definir_fundo(id, url));
  }
  CONFERE(arteesc_n() == ARTEESC_MAX);
  // Todas achaveis pelo hash.
  for (i = 0; i < ARTEESC_MAX; i++) {
    snprintf(id, sizeof id, "tt%07d", i);
    CONFERE(arteesc_fundo(id) != NULL);
  }
  // A 161a toma o lugar da MAIS ANTIGA (tt0000000).
  CONFERE(arteesc_definir_fundo("tt9999999", "https://image.tmdb.org/t/p/w1280/nova.jpg"));
  CONFERE(arteesc_n() == ARTEESC_MAX);
  CONFERE(arteesc_fundo("tt0000000") == NULL);
  CONFERE(arteesc_fundo("tt0000001") != NULL);
  IGUAL(arteesc_fundo("tt9999999"), "https://image.tmdb.org/t/p/w1280/nova.jpg");
}

static void precedencia(void) {
  CatItem it = filme(), ep;
  const char *u;
  arteesc_definir_perfil(5);
  artehero_definir_falhou(falhouDeMentira);
  artehero_definir_escolha(arteesc_fundo, arteesc_logo);
  artehero_qualidade(1);
  // SEM ESCOLHA: a regra de sempre (a foto do catalogo).
  IGUAL(artehero_url_destaque(&it, ARTEHERO_AUTO, 0), "https://image.tmdb.org/t/p/w1280/catalogo.jpg");
  CONFERE(artehero_url_escolhida(&it) == NULL);
  CONFERE(artehero_logo_escolhido(&it) == NULL);

  CONFERE(arteesc_definir_fundo("tt0133093", "https://image.tmdb.org/t/p/w1280/escolhido.jpg"));
  CONFERE(arteesc_definir_logo("tt0133093", "https://image.tmdb.org/t/p/w500/logoEsc.png"));
  // A ESCOLHA VENCE em todo lugar de tela cheia, qualquer que seja o ajuste.
  IGUAL(artehero_url_destaque(&it, ARTEHERO_AUTO, 0), "https://image.tmdb.org/t/p/w1280/escolhido.jpg");
  IGUAL(artehero_url_destaque(&it, ARTEHERO_METAHUB, 0), "https://image.tmdb.org/t/p/w1280/escolhido.jpg");
  IGUAL(artehero_url_destaque(&it, ARTEHERO_TMDB, 1), "https://image.tmdb.org/t/p/w1280/escolhido.jpg");
  IGUAL(artehero_url(&it), "https://image.tmdb.org/t/p/w1280/escolhido.jpg");
  // O CARD NAO MUDA: a escolha e de tela cheia.
  IGUAL(artehero_url_card_fonte(&it, ARTEHERO_AUTO, 0), "https://image.tmdb.org/t/p/w1280/catalogo.jpg");
  // Tamanho pela qualidade: baixa w780, alta original (so na LG).
  artehero_qualidade(0);
  IGUAL(artehero_url_escolhida(&it), "https://image.tmdb.org/t/p/w780/escolhido.jpg");
  artehero_qualidade(2);
#ifdef __EMSCRIPTEN__
  IGUAL(artehero_url_escolhida(&it), "https://image.tmdb.org/t/p/w1280/escolhido.jpg");
#else
  IGUAL(artehero_url_escolhida(&it), "https://image.tmdb.org/t/p/original/escolhido.jpg");
#endif
  artehero_qualidade(1);
  // O LOGO escolhido, no tamanho unico de artehero_url_logo (w1280 no padrao),
  // na sessao do detalhe, na observacao do hero e no card aberto.
  IGUAL(artehero_logo_escolhido(&it), "https://image.tmdb.org/t/p/w1280/logoEsc.png");
  artehero_logo_sessao_iniciar(&it);
  IGUAL(artehero_logo_sessao(&it), "https://image.tmdb.org/t/p/w1280/logoEsc.png");
  IGUAL(artehero_logo_sessao_observar(&it), "https://image.tmdb.org/t/p/w1280/logoEsc.png");
  IGUAL(artehero_logo_sessao_larg(&it, 300.0f), "https://image.tmdb.org/t/p/w1280/logoEsc.png");
  // EPISODIO de Continuar assistindo acha a escolha da serie/filme pelo id.
  ep = it;
  snprintf(ep.imdb, sizeof ep.imdb, "tt0133093:1:2");
  ep.temporada = 1; ep.episodio = 2;
  IGUAL(artehero_url_escolhida(&ep), "https://image.tmdb.org/t/p/w1280/escolhido.jpg");
  // SUSPENSA (a miniatura "Automatico" da tela de escolha): a regra volta.
  artehero_escolha_suspender(1);
  IGUAL(artehero_url_destaque(&it, ARTEHERO_AUTO, 0), "https://image.tmdb.org/t/p/w1280/catalogo.jpg");
  CONFERE(artehero_logo_escolhido(&it) == NULL);
  artehero_escolha_suspender(0);
  // ESCOLHA QUE DEU 404 nao prende a tela: vale a regra, e o disco fica.
  snprintf(morta, sizeof morta, "https://image.tmdb.org/t/p/w1280/escolhido.jpg");
  IGUAL(artehero_url_destaque(&it, ARTEHERO_AUTO, 0), "https://image.tmdb.org/t/p/w1280/catalogo.jpg");
  CONFERE(arteesc_fundo("tt0133093") != NULL);
  morta[0] = 0;
  // AUTOMATICO pela tela: a escolha sai e a regra volta.
  CONFERE(arteesc_definir_fundo("tt0133093", NULL));
  IGUAL(artehero_url_destaque(&it, ARTEHERO_AUTO, 0), "https://image.tmdb.org/t/p/w1280/catalogo.jpg");
  // Item sem imdb, com id do TMDB: a chave e "tmdb:m<id>".
  { CatItem t = filme(); char k[64];
    t.imdb[0] = 0; t.tmdb = 603;
    artehero_id_escolha(&t, k, sizeof k);
    IGUAL(k, "tmdb:m603");
    CONFERE(arteesc_definir_fundo(k, "https://image.tmdb.org/t/p/w1280/pelotmdb.jpg"));
    IGUAL(artehero_url_escolhida(&t), "https://image.tmdb.org/t/p/w1280/pelotmdb.jpg"); }
  // Sem registro (o teste do artehero roda assim): nada muda.
  artehero_definir_escolha(NULL, NULL);
  CONFERE(artehero_url_escolhida(&it) == NULL);
  (void)u;
}

static void logout(void) {
  arteesc_esquecer();
  CONFERE(arteesc_n() == 0);
  CONFERE(!arquivoExiste("arte-escolhida-p1.txt"));
  CONFERE(!arquivoExiste("arte-escolhida-p2.txt"));
  CONFERE(!arquivoExiste("arte-escolhida-p5.txt"));
  arteesc_definir_perfil(1);
  CONFERE(arteesc_fundo("tt0133093") == NULL);
}

int main(void) {
  const char *dd;
  dados_iniciar("/nao/existe");
  dd = dados_dir();
  if (!dd || !strstr(dd, "nuvio-arteesc")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de apontar para a pasta temporaria do "
                    "teste (dados_dir = \"%s\")\n", dd ? dd : "");
    return 1;
  }
  chaves();
  tabela();
  perfis();
  cheia();
  precedencia();
  logout();
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("arteescolha: tudo certo\n");
  return 0;
}
