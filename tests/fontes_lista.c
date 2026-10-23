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
#include "nvtmp.h"
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
    // #205 (38f3dd35): fora de cache fica no fim do proprio grupo;
    // a ultima linha e a ultima MediaFusion 720p, abaixo de 4K e 1080p.
    assert(esc == N_TOTAL - 1); }
  printf("fontes_lista: %s: %d de %d na folha\n", cenario, stream_folha_n(), N_TOTAL);
}

// #552: classify the parsed hostname, never userinfo or a hostname prefix.
static void conferirHostP2P(void) {
  static const struct { const char *url; int p2p; } cases[] = {
    {"http://LOCALHOST:11470/hash/0", 1},
    {"https://LocalHost/hash/0", 1},
    {"http://localhost", 1},
    {"http://127.0.0.1:11470/hash/0", 1},
    {"http://127.255.1.2/hash/0", 1},
    {"http://0.0.0.0?hash=0", 1},
    {"http://[::1]:11470/hash/0", 1},
    {"http://[0:0:0:0:0:0:0:1]/hash/0", 1},
    {"http://user:pass@LOCALHOST:11470/hash/0", 1},
    {"http://user:pass@[::1]:11470/hash/0", 1},
    {"HTTPS://user:pass@127.5.6.7:11470/hash/0", 1},
    {"http://localhost.example.org/hash/0", 0},
    {"http://LOCALHOST.example.org:80/hash/0", 0},
    {"http://127.0.0.1.example.org/hash/0", 0},
    {"http://0.0.0.0.example.org/hash/0", 0},
    {"http://0.0.0.01/hash/0", 0},
    {"http://localhost@remote.example.org/hash/0", 0},
    {"http://127.0.0.1:pass@remote.example.org/hash/0", 0},
    {"HTTP://remote.example.org:80/hash/0", 0},
    {"https://remote.example.org?next=@localhost", 0},
    {"https://remote.example.org/#@localhost", 0},
    {"http://128.0.0.1/hash/0", 0},
    {"http://[::10]/hash/0", 0},
    {"httpx://remote.example.org/hash/0", 1},
    {"ftp://remote.example.org/hash/0", 1}
  };
  for (int ligado = 0; ligado <= 1; ligado++) {
    ajustes_definir_p2p_ligado(ligado);
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
      Stream s;
      direta(&s, "hostname", 0, 1080, 0);
      snprintf(s.url, sizeof s.url, "%s", cases[i].url);
      s.notWebReady = 1;
      int hidden = !ligado && cases[i].p2p;
      for (int incremental = 0; incremental <= 1; incremental++) {
        stream_definir_lista(NULL, 0);
        if (incremental) stream_lista_acrescentar(&s, 1, 0);
        else stream_definir_lista(&s, 1);
        if (stream_n() != !hidden || stream_torrents_para_p2p() != hidden) {
          fprintf(stderr, "hostname classification failed: %s (P2P=%d, incremental=%d)\n",
                  cases[i].url, ligado, incremental);
          exit(1);
        }
      }
    }
  }
}

int main(void) {
  Stream l[N_TOTAL];
  char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-fontes-lista-XXXXXX");
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

  // P2P EXPERIMENTAL (p2p.h): ligado E com endereco, o torrent sem url deixa de
  // ser descartado — mesmo sem debrid — e a folha o lista. Ligado SEM endereco,
  // ou com endereco e DESLIGADO, nada muda (a fabrica e desligado).
  assert(!ajustes_p2p_ligado());
  ajustes_definir_p2p_ligado(1);
  stream_definir_lista(l, n);
  assert(stream_n() == N_A + N_C);                 // ligado, mas sem endereco
  assert(!ajustes_definir_p2p_url("ftp://x"));     // endereco torto e recusado
  assert(ajustes_definir_p2p_url("192.168.1.5"));
  assert(!strcmp(ajustes_p2p_url(), "http://192.168.1.5:11470"));
  stream_definir_lista(l, n);
  assert(stream_n() == N_TOTAL);
  stream_folha_abrir();
  assert(stream_folha_n() == N_TOTAL);
  tecla(SDLK_ESCAPE);
  assert(stream_qtd_torrents() == N_B);
  ajustes_definir_p2p_ligado(0);                   // desligado: volta ao de hoje
  stream_definir_lista(l, n);
  assert(stream_n() == N_A + N_C);
  assert(stream_qtd_torrents() == 0);
  ajustes_definir_p2p_url("");                     // vazio esquece o endereco
  assert(!ajustes_p2p_url()[0]);
  puts("fontes_lista: P2P so mantem os torrents ligado e com endereco");

  // #552: URL http(s) marcada notWebReady e oferecida com P2P desligado (os
  // players nativos a tocam); so o torrent sem url e o servidor local pedem P2P. Conferir resposta inteira e chegada incremental.
  char json[4096];
  FILE *f = fopen("tests/fixtures/streams_notwebready.json", "r");
  assert(f);
  size_t bytes = fread(json, 1, sizeof json - 1, f);
  assert(!ferror(f) && feof(f));
  fclose(f); json[bytes] = 0;
  Stream *v = NULL;
  int count = stream_extrair(json, "fixture", &v);
  assert(count == 10);
  debrid_definir_chave("torbox", "chave-de-teste");
  // #505: as duas fontes notWebReady descartadas com o P2P desligado contam
  // para a dica "ligue o P2P" — nunca uma folha vazia calada. Sem debrid e
  // com um endereco P2P configurado a dica existe (o valor real e o de
  // stream_torrents_para_p2p; aqui ele devolve 0 porque o debrid esta ativo).
  debrid_esquecer();
  assert(ajustes_definir_p2p_url("192.168.1.5"));
  ajustes_definir_p2p_ligado(0);
  stream_definir_lista(v, count);
  assert(stream_torrents_para_p2p() == 2);
  // Lumio-like infoHash-only notWebReady stream: connected debrid resolves it
  // (offered, not a P2P need); only the local-engine URL still needs P2P.
  debrid_definir_chave("torbox", "chave-de-teste");
  stream_definir_lista(v, count);
  assert(stream_n() == 9 && stream_torrents_para_p2p() == 1);
  Stream onlyP2P[2];
  int p2pCount = 0;
  for (int i = 0; i < count; i++)
    if (!v[i].url[0] || strstr(v[i].url, "127.0.0.1")) onlyP2P[p2pCount++] = v[i];
  assert(p2pCount == 2);
  stream_definir_lista(onlyP2P, p2pCount);
  assert(stream_n() == 1 && stream_torrents_para_p2p() == 1);
  stream_definir_lista(NULL, 0);
  assert(stream_torrents_para_p2p() == 0);
  stream_lista_acrescentar(onlyP2P, p2pCount, 0);
  assert(stream_n() == 1 && stream_torrents_para_p2p() == 1);
  stream_definir_lista(l, n);
  assert(stream_torrents_para_p2p() == 0);
  for (int ligado = 0; ligado <= 1; ligado++) {
    assert(ajustes_definir_p2p_url("192.168.1.5"));
    ajustes_definir_p2p_ligado(ligado);
    stream_definir_lista(v, count);
    assert(stream_n() == (ligado ? 10 : 9));
    assert(stream_torrents_para_p2p() == (ligado ? 0 : 1));
    for (int i = 0; i < stream_n(); i++)
      assert(ligado || stream_item(i)->infoHash[0] || (stream_item(i)->url[0] && !strstr(stream_item(i)->url, "127.0.0.1")));
    stream_definir_lista(NULL, 0);
    stream_lista_acrescentar(v, count, 0);
    assert(stream_n() == (ligado ? 10 : 9));
    for (int i = 0; i < stream_n(); i++)
      assert(ligado || stream_item(i)->infoHash[0] || (stream_item(i)->url[0] && !strstr(stream_item(i)->url, "127.0.0.1")));
    stream_folha_abrir();
    assert(stream_folha_n() == (ligado ? 10 : 9));
    assert(ligado || !strstr(stream_item(stream_automatico())->url, "127.0.0.1"));
    tecla(SDLK_ESCAPE);
  }
  free(v);
  conferirHostP2P();
  puts("fontes_lista: notWebReady com url http oferecido, so P2P-puro exige P2P, externalUrl oculto");
  return 0;
}
