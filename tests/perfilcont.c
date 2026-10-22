// "O que essa pessoa estava vendo" na escolha de perfil: a regra de escolher o
// item (mais recente, nao terminado, nao removido), PIN sem cartao, perfil sem
// nada, e a copia minima de titulo/capa. Sem rede, sem SDL, sem disco.
#include "perfilcont.h"
#include "progresso.h"
#include "catalogo.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *arquivo, *snapshot;
static int perfil = 1;
static long long agora = 1757000000000LL;
static ContaPerfil lista[4];
static int nLista;

char *dados_ler(const char *nome) {
  char **f = !strcmp(nome, "perfilcont.txt") ? &snapshot : &arquivo;
  return *f ? strdup(*f) : NULL;
}
int dados_gravar(const char *nome, const char *c) {
  char **f = !strcmp(nome, "perfilcont.txt") ? &snapshot : &arquivo;
  free(*f); *f = strdup(c); return 1;
}
int dados_apagar(const char *nome) {
  char **f = !strcmp(nome, "perfilcont.txt") ? &snapshot : &arquivo;
  free(*f); *f = NULL; return 1;
}
int perfis_ativo(void) { return perfil; }
int perfis_n(void) { return nLista; }
const ContaPerfil *perfis_item(int i) { return i >= 0 && i < nLista ? &lista[i] : NULL; }
int ajustes_cw_concluido(void) { return 90; }
int   ajustes_cw_minimo(void)             { return 1; }
static long long relogio(void) { return agora; }

// O catalogo conhece o que o teste disser.
static int catConhece;
static char posterLongo[NV_ARTE_URL_MAX];
int cat_copiar_por_id(const char *id, const char *tipo, CatItem *s) {
  (void)tipo;
  if (!catConhece || strcmp(id, "tt-b")) return 0;
  memset(s, 0, sizeof *s);
  snprintf(s->imdb, sizeof s->imdb, "%s", id);
  snprintf(s->titulo, sizeof s->titulo, "Os Bichos da Mata");
  snprintf(s->poster, sizeof s->poster, "%s", posterLongo[0] ? posterLongo : "http://x/b.jpg");
  s->temporada = 2; s->episodio = 4;
  snprintf(s->nomeEpisodio, sizeof s->nomeEpisodio, "A toca");
  return 1;
}

static void gravar(int p, const char *id, int t, int e, double pos, double dur, long long quando) {
  perfil = p; agora = quando;
  assert(prog_gravar_local(id, t, e, pos, dur, -1));
}

static void zerar(void) {
  perfilcont_esquecer();   // memoria e arquivo da copia
  free(arquivo); arquivo = NULL;
  prog_invalidar(); perfil = 1; catConhece = 0;
  memset(lista, 0, sizeof lista);
  lista[0].indice = 1; snprintf(lista[0].nome, sizeof lista[0].nome, "Henrique");
  lista[1].indice = 2; snprintf(lista[1].nome, sizeof lista[1].nome, "Alvaro");
  lista[2].indice = 3; snprintf(lista[2].nome, sizeof lista[2].nome, "Kids"); lista[2].temPin = 1;
  lista[3].indice = 4; snprintf(lista[3].nome, sizeof lista[3].nome, "Visitas");
  nLista = 4;
}

int main(void) {
  PerfilCont c;
  ProgRegistro r;
  prog_definir_relogio(relogio);

  // MAIS RECENTE, NAO TERMINADO. Henrique: A (30%) e B (50%, depois); o C mais
  // novo esta a 95% e nao e "continuar".
  zerar();
  catConhece = 1;
  gravar(1, "tt-a", 0, 0, 1800, 6000, agora + 1000);
  gravar(1, "tt-b", 2, 4, 1500, 3000, agora + 5000);
  gravar(1, "tt-c", 0, 0, 5700, 6000, agora + 9000);
  perfil = 2;
  assert(perfilcont_de(&lista[0], &c));
  assert(c.serie && c.t == 2 && c.e == 4);
  assert(!strcmp(c.titulo, "Os Bichos da Mata") && !strcmp(c.epNome, "A toca"));
  assert(!strcmp(c.poster, "http://x/b.jpg"));
  assert(c.progresso > 0.49f && c.progresso < 0.51f && c.restanteMin == 25);
  puts("ok  mais recente nao terminado; terminado e ignorado");

  // PERFIL SEM NADA: so terminado, ou duracao de ruido, ou sem registro.
  zerar();
  gravar(2, "tt-b", 0, 0, 5800, 6000, agora + 1);
  gravar(2, "tt-a", 0, 0, 10, 30, agora + 2);        // < 60 s
  assert(!perfilcont_de(&lista[1], &c));
  assert(!perfilcont_de(&lista[3], &c));
  assert(!perfilcont_de(NULL, &c));
  puts("ok  perfil sem item em andamento: sem cartao");

  // PIN: nao revela nada, mesmo com progresso e titulo conhecidos.
  zerar(); catConhece = 1;
  gravar(3, "tt-b", 2, 4, 1500, 3000, agora + 1);
  assert(!perfilcont_de(&lista[2], &c));
  lista[2].temPin = 0;
  assert(perfilcont_de(&lista[2], &c));
  puts("ok  perfil com PIN: sem cartao (e com o PIN fora, aparece)");

  // PERFIS NAO SE MISTURAM.
  zerar(); catConhece = 1;
  gravar(2, "tt-b", 2, 4, 1500, 3000, agora + 1);
  assert(!perfilcont_de(&lista[0], &c));
  assert(perfilcont_de(&lista[1], &c));
  puts("ok  cada perfil ve so o seu");

  // REMOVIDO DE "CONTINUAR": sai, e volta se a pessoa assistiu de novo.
  zerar(); catConhece = 1;
  gravar(1, "tt-a", 0, 0, 1800, 6000, 1757000001000LL);
  gravar(1, "tt-b", 2, 4, 1500, 3000, 1757000002000LL);
  agora = 1757000003000LL; prog_marcar_removido("tt-b");
  assert(prog_continuar_de_perfil(1, 90, &r) && !strcmp(r.contentId, "tt-a"));
  gravar(1, "tt-b", 2, 4, 1600, 3000, 1757000004000LL);
  assert(prog_continuar_de_perfil(1, 90, &r) && !strcmp(r.contentId, "tt-b"));
  puts("ok  removido some e volta ao assistir de novo");

  // COPIA MINIMA: o catalogo de outra pessoa nao tem o titulo, mas a copia
  // guardada quando ele era conhecido ainda serve.
  zerar(); catConhece = 1;
  gravar(1, "tt-b", 2, 4, 1500, 3000, agora + 1);
  perfilcont_registrar();
  assert(snapshot && strstr(snapshot, "Os Bichos da Mata"));
  catConhece = 0;
  assert(perfilcont_de(&lista[0], &c) && !strcmp(c.titulo, "Os Bichos da Mata"));
  assert(!strcmp(c.epNome, "A toca"));
  // Passou a ver outro titulo: a copia do anterior nao serve de rosto para ele.
  gravar(1, "tt-z", 0, 0, 1000, 6000, agora + 100);
  assert(!perfilcont_de(&lista[0], &c));
  // Episodio diferente do copiado: o titulo vale, o nome do episodio nao.
  gravar(1, "tt-b", 2, 5, 600, 3000, agora + 200);
  assert(perfilcont_de(&lista[0], &c) && c.e == 5 && c.epNome[0] == 0);
  puts("ok  copia minima de titulo/capa; sem titulo conhecido, sem cartao");

  // PIN fora da copia, e logout apaga.
  zerar(); catConhece = 1;
  gravar(3, "tt-b", 2, 4, 1500, 3000, agora + 1);
  perfilcont_registrar();
  assert(!snapshot);
  gravar(1, "tt-b", 2, 4, 1500, 3000, agora + 2);
  perfilcont_registrar();
  assert(snapshot);
  perfilcont_esquecer();
  assert(!snapshot);
  puts("ok  PIN nunca vai para a copia; logout apaga");

  // #433: a copia em disco nao volta ao antigo teto de 400/800 bytes.
  for (size_t n = 1173; n <= 2000; n += 827) {
    zerar(); catConhece = 1;
    memset(posterLongo, 'a', n); memcpy(posterLongo, "https://posters.invalid/", 24); posterLongo[n] = 0;
    gravar(1, "tt-b", 2, 4, 1500, 3000, agora + 1);
    perfilcont_registrar();
    assert(snapshot && strstr(snapshot, posterLongo));
    catConhece = 0;
    assert(perfilcont_de(&lista[0], &c) && !strcmp(c.poster, posterLongo));
  }
  puts("ok  continuar por perfil conserva cartaz de 1173/2000 bytes");
  puts("perfilcont: tudo ok");
  return 0;
}
