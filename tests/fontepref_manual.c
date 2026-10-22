// BINGE GROUP NA ABERTURA MANUAL DE OUTRO EPISODIO (issue #310, Namer03).
//
// O relato: o app nao usa o binge group ao escolher outro episodio a mao, no
// seletor de episodios ou no carrossel do titulo. A suspeita era que so o
// automatico / proximo episodio aplicasse a fonte lembrada. A leitura do codigo
// refuta: os tres caminhos manuais (card de episodio no detalhe, folha de
// episodios do player, carrossel/proximo) entram em app.c por
// `aguardandoFonte = 1` e passam todos pelo MESMO bloco, que chama
// fontepref_escolher(base) -> stream_preferir. Este teste TRAVA isso:
//   - o ID que cada caminho manual entrega (tt..:T:E, kitsu:N:T:E, tt.. de
//     filme) cai na mesma chave de titulo;
//   - a fonte do binge group X, gravada no S1E1, e a escolhida no S1E2 mesmo
//     com uma nao-X melhor ranqueada na lista;
//   - outra serie nao herda; sem grupo lembrado nada muda; filme nao muda.
// Mesmo harness de tests/fontepref.c (NUVIO_DADOS temporario, recusa rodar fora).
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/fontepref.c"

#include "dados.h"

static int falhas;
#define CONFERE(cond, ...) do { \
    if (cond) { printf("ok    "); } else { printf("FALHA "); falhas++; } \
    printf(__VA_ARGS__); printf("\n"); } while (0)

static void fonteB(Stream *s, const char *prov, const char *rot, const char *desc,
                   const char *url, int altura, int mp4, const char *binge) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rot);
  snprintf(s->descricao, sizeof s->descricao, "%s", desc);
  snprintf(s->url, sizeof s->url, "%s", url);
  s->altura = altura;
  s->mp4 = mp4;
  snprintf(s->bingeGroup, sizeof s->bingeGroup, "%s", binge);
}

// Lista de um episodio: [0] 4K MP4 sem grupo (a que a pontuacao poe na frente),
// [1] 1080p do grupo X, [2] 1080p de outro grupo.
static int lista(Stream *v, int ep) {
  char d[120], u[120];
  snprintf(d, sizeof d, "Serie.S01E%02d.2160p.mp4", ep);
  snprintf(u, sizeof u, "https://exemplo.invalido/4k/%d.mp4", ep);
  fonteB(&v[0], "Torrentio", "Torrentio\n4k", d, u, 2160, 1, "");
  snprintf(d, sizeof d, "Serie.S01E%02d.1080p.GRUPOX.mkv", ep);
  snprintf(u, sizeof u, "https://exemplo.invalido/x/%d.mkv", ep);
  fonteB(&v[1], "Outro Addon", "Outro 1080p", d, u, 1080, 0, "addon|grupoX");
  snprintf(d, sizeof d, "Serie.S01E%02d.1080p.OUTRO.mkv", ep);
  snprintf(u, sizeof u, "https://exemplo.invalido/y/%d.mkv", ep);
  fonteB(&v[2], "Terceiro", "Terceiro 1080p", d, u, 1080, 0, "addon|grupoY");
  return 3;
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  Stream v[4];
  int n, k, i;
  // Os ids que os caminhos manuais entregam a idBaseDoTitulo/fontepref_id_base.
  const char *manuais[] = {
    "tt2000001:1:2",    // card de episodio no detalhe (alvo do Reproduzir)
    "tt2000001:1:7",    // folha de episodios do player (episodios_escolheu)
    "tt2000001:2:1",    // carrossel / proximo da outra temporada
    "tt2000001",        // id base puro (detalhe sem episodio resolvido)
  };

  if (!dir || !dir[0]) { printf("fontepref_manual: NUVIO_DADOS ausente; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("fontepref_manual: dados_dir diverge; recusando\n"); return 2; }

  fontepref_definir_perfil(1);
  n = lista(v, 1);
  stream_definir_lista(v, n);
  CONFERE(stream_automatico() == 0, "a pontuacao poe a 4K sem grupo na frente (%d)", stream_automatico());
  CONFERE(fontepref_guardar("tt2000001:1:1", stream_item(1)) == 1, "S1E1: fonte do grupo X lembrada");

  for (i = 0; i < 4; i++) {
    n = lista(v, 2 + i);
    stream_definir_lista(v, n);
    k = fontepref_escolher(manuais[i]);
    CONFERE(k == 1, "abrir %s a mao prefere o grupo X (%d) sobre a nao-X melhor ranqueada", manuais[i], k);
    stream_preferir(k);
    CONFERE(stream_preferida() == 1, "  e o indice preferido do caminho manual e o do grupo X");
  }

  // O grupo X em outra posicao da lista (ordem de addon nao se repete).
  n = lista(v, 3);
  { Stream t = v[1]; v[1] = v[2]; v[2] = t; }
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt2000001:1:3");
  CONFERE(k == 2 && !strcmp(stream_item(k)->bingeGroup, "addon|grupoX"),
          "a posicao muda, o grupo X continua sendo achado (%d)", k);

  // Outra serie nao herda a preferencia.
  n = lista(v, 2);
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("tt2000002:1:2") == -1, "outra serie: nada lembrado, automatico");
  CONFERE(fontepref_escolher("tt2000002") == -1, "outra serie (id base): idem");

  // Anime (id kitsu:N:T:E): a chave e kitsu:N, o grupo vale entre episodios.
  n = lista(v, 1);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("kitsu:41370:1:1", stream_item(1)) == 1, "anime: grupo X lembrado");
  n = lista(v, 2);
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("kitsu:41370:1:2") == 1, "anime: S1E2 a mao prefere o grupo X");
  CONFERE(fontepref_escolher("kitsu:99999:1:2") == -1, "outro anime do mesmo addon nao herda");

  // Sem grupo lembrado: a fonte escolhida sem bingeGroup cai na heuristica de
  // provedor+trilha, exatamente como antes (aqui, o 4K da Torrentio).
  fontepref_definir_perfil(2);
  n = lista(v, 1);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt2000003:1:1", stream_item(0)) == 1, "sem grupo: 4K lembrada");
  n = lista(v, 2);
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("tt2000003:1:2") == 0, "sem grupo: S1E2 segue por provedor+trilha, como antes");

  // Perfil sem nada: tudo -1.
  fontepref_definir_perfil(3);
  CONFERE(fontepref_escolher("tt2000001:1:2") == -1, "outro perfil: nada lembrado");

  printf(falhas ? "fontepref_manual: %d falha(s)\n" : "fontepref_manual: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}
