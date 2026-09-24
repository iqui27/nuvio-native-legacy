// Issue #132: "so 1 fonte listada, e os addons tem mais".
//
// A folha de Fontes lista TUDO o que os addons mandaram (a lista de
// stream_definir_lista). O que pode tirar linha dela e so o que a pessoa liga
// na propria folha (provedor, "MP4") e o descarte de torrent sem url quando nao
// ha debrid nenhum (uma linha que nunca tocaria). Nada do automatico entra
// aqui: nem "Fonte automatica" (Melhor fonte / Primeira da lista), nem a
// "Qualidade maxima", nem as candidatas que a verificacao recusou, nem a marca
// de fora de cache do TorBox.
//
// Sem rede e sem SDL de video: so a lista em memoria e a contagem da folha.
#include "streams.h"
#include "ajustes.h"
#include "debrid.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N_A 7   // "AIOStreams": direta, com fora de cache no meio
#define N_B 9   // "Torrentio": so infoHash (torrent sem url)
#define N_C 4   // "MediaFusion": direta, uma 4K acima do teto
#define N_TOTAL (N_A + N_B + N_C)

static void direta(Stream *s, const char *prov, int k, int altura, int mp4) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s %dp #%d", prov, altura, k);
  snprintf(s->url, sizeof s->url, "https://%s.invalid/p/%d.%s", prov, k, mp4 ? "mp4" : "mkv");
  s->altura = altura;
  s->mp4 = mp4;
  s->fileIdx = -1;
}
static void torrent(Stream *s, const char *prov, int k, int altura) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s %dp #%d", prov, altura, k);
  snprintf(s->infoHash, sizeof s->infoHash, "%040d", k + 1);
  s->altura = altura;
  s->fileIdx = k;
}

static int montar(Stream *l) {
  int q = 0, k;
  for (k = 0; k < N_A; k++) {
    direta(&l[q], "AIOStreams", k, k % 2 ? 1080 : 2160, k == 3);
    l[q].foraCache = (k == 1 || k == 4);   // o "⏳" do AIOStreams
    q++;
  }
  for (k = 0; k < N_B; k++) torrent(&l[q++], "Torrentio", k, 1080);
  for (k = 0; k < N_C; k++) direta(&l[q++], "MediaFusion", k, k ? 720 : 2160, 0);
  return q;
}

// Ajustes do teste num diretorio proprio: "Fonte automatica" e "Qualidade
// maxima" sao lidas de ajustes.txt como na TV.
static void ajustes(const char *dir, int primeira, int qualidade) {
  char caminho[512];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "fonteAutoLocal %d\nqualidade %d\n", primeira, qualidade);
  fclose(f);
  ajustes_dir(dir);
  assert(ajustes_fonte_primeira() == primeira);
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  stream_folha_evento(&e);
}

// A folha inteira, com o que o automatico faz com a lista de verdade: a
// preferida lembrada, a fonte tocando, e as candidatas que falharam.
static void conferirFolhaCheia(const Stream *l, int n, const char *cenario) {
  int i;
  stream_definir_lista(l, n);
  assert(stream_n() == N_TOTAL);
  // O que a verificacao em serie (fonteauto) faz com quem nao serviu, e o
  // que tentarProximaFonteVOD faz com a que travou: sai da FILA, nao da lista.
  for (i = 0; i < N_TOTAL; i += 2) stream_automatico_excluir(i);
  stream_preferir(5);
  stream_definir_atual(N_A + 2);
  stream_folha_abrir();
  if (stream_folha_n() != N_TOTAL) {
    printf("fontes_lista: %s: a folha mostra %d de %d fontes\n", cenario,
           stream_folha_n(), N_TOTAL);
    exit(1);
  }
  // Toda linha continua escolhivel pela folha, inclusive as recusadas pelo
  // automatico e as fora de cache: descer ate o fim e dar OK escolhe a ultima.
  for (i = 0; i < N_TOTAL + 3; i++) tecla(SDLK_DOWN);
  tecla(SDLK_RETURN);
  { int esc = -1;
    assert(stream_folha_escolheu(&esc));
    assert(esc == N_TOTAL - 1); }
  printf("fontes_lista: %s: %d de %d na folha\n", cenario, stream_folha_n(), N_TOTAL);
}

int main(void) {
  Stream l[N_TOTAL];
  char dir[] = "/tmp/nuvio-fontes-lista-XXXXXX";
  int n;
  assert(mkdtemp(dir));
  n = montar(l);
  assert(n == N_TOTAL);

  // Com debrid: os torrents sem url ficam (debrid_resolver os resolve).
  debrid_definir_chave("torbox", "chave-de-teste");
  assert(debrid_ativo());

  ajustes(dir, 0, 0);   // Melhor fonte, sem teto
  conferirFolhaCheia(l, n, "melhor fonte");
  ajustes(dir, 1, 0);   // Primeira da lista
  conferirFolhaCheia(l, n, "primeira da lista");
  ajustes(dir, 1, 2);   // Primeira da lista + teto 1080p: teto e preferencia
  conferirFolhaCheia(l, n, "primeira da lista, teto 1080p");
  ajustes(dir, 0, 3);   // Melhor fonte + teto 720p
  conferirFolhaCheia(l, n, "melhor fonte, teto 720p");

  // OS FILTROS DA FOLHA sao a pessoa escolhendo: esses tiram linha, e so
  // enquanto ligados. Provedor: CIMA vai as pilulas, DIREITA = 1o addon.
  stream_definir_lista(l, n);
  stream_folha_abrir();
  assert(stream_folha_n() == N_TOTAL);
  tecla(SDLK_UP);
  tecla(SDLK_RIGHT);
  assert(stream_folha_n() == N_A);
  tecla(SDLK_RIGHT);
  assert(stream_folha_n() == N_B);
  tecla(SDLK_RIGHT);
  assert(stream_folha_n() == N_C);
  tecla(SDLK_ESCAPE);
  stream_folha_abrir();   // reabrir volta a "Todos"
  assert(stream_folha_n() == N_TOTAL);
  tecla(SDLK_ESCAPE);
  puts("fontes_lista: filtro de provedor e escolha da pessoa, e reabrir volta a todas");

  // SEM DEBRID NENHUM os torrents sem url saem (nunca tocariam), e o log diz
  // quantos. As diretas dos outros dois addons ficam todas.
  debrid_esquecer();
  assert(!debrid_ativo());
  stream_definir_lista(l, n);
  stream_folha_abrir();
  assert(stream_n() == N_A + N_C);
  assert(stream_folha_n() == N_A + N_C);
  tecla(SDLK_ESCAPE);
  puts("fontes_lista: sem debrid so saem os torrents sem url");
  return 0;
}
