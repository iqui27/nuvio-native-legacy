#include "cenas.h"
#include "ajustes.h"
#include "catalogo.h"
#include "dados.h"
#include "idioma.h"
#include "perfis.h"
#include "player.h"
#include "recenviar.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CENAS_REENVIO_MS 8000u
#define CENAS_ARMADA_MS  (15u * 60u * 1000u)

static MarcaCena lista[MARCACENA_MAX];
static int nLista, perfilLista = -1;
static MarcaCena ultima;          // a marca do ultimo toque, para o segundo toque
static Uint32 ultimaEm;
static struct { char imdb[24]; int t, e, seg, dur, ativa; Uint32 em; } armada;
static int conferirDur, conferirSeg;   // duracao de origem a comparar com a local

static void arquivo(char *dst, size_t tam, int perfil) {
  snprintf(dst, tam, "cenas_%d.txt", perfil);
}

static void carregar(void) {
  char nome[32], *txt;
  int p = perfis_ativo();
  if (p == perfilLista) return;
  perfilLista = p; nLista = 0;
  arquivo(nome, sizeof nome, p);
  txt = dados_ler(nome);
  if (txt) { nLista = marcacena_carregar(txt, lista, MARCACENA_MAX); free(txt); }
}

static void gravar(void) {
  char nome[32];
  size_t tam = (size_t)MARCACENA_MAX * 80 + 1;
  char *buf = malloc(tam);
  if (!buf) return;
  marcacena_serializar(lista, nLista, buf, tam);
  arquivo(nome, sizeof nome, perfilLista);
  dados_gravar(nome, buf);
  free(buf);
}

int cenas_n(void) { carregar(); return nLista; }
int cenas_item(int i, MarcaCena *saida) {
  carregar();
  if (i < 0 || i >= nLista || !saida) return 0;
  *saida = lista[i];
  return 1;
}

void cenas_tecla(void) {
  const CatItem *ci = cat_item(player_indice());
  char tempo[16], aviso[160], texto[MARCACENA_TEXTO_MAX + 1];
  MarcaCena m;
  int t = 0, e = 0;
  double pos = player_posicao_seg(), dur = 0.0;
  Uint32 agora = SDL_GetTicks();
  if (!ajustes_marcar_cena() || !ci || !ci->imdb[0] || strlen(ci->imdb) >= sizeof m.id || !(pos >= 0.0)) return;
  memset(&m, 0, sizeof m);
  memcpy(m.id, ci->imdb, strlen(ci->imdb) + 1);
  m.tipo = !strcmp(ci->tipo, "movie") ? 'm' : 's';
  player_episodio_atual(&t, &e);
  if (m.tipo == 's' && t >= 0 && e > 0) { m.temporada = (short)t; m.episodio = (short)e; }
  m.seg = (int)pos;
  if (!player_duracao_midia(&dur)) dur = 0.0;
  m.durSeg = dur > 0.0 && dur < 99.0 * 3600 ? (int)dur : 0;
  m.quando = (long long)time(NULL);
  if (!marcacena_valida(&m)) return;

  // SEGUNDO TOQUE: envia a cena que acabou de ser marcada, e nao uma nova.
  if (ultimaEm && agora - ultimaEm < CENAS_REENVIO_MS && !strcmp(ultima.id, m.id) &&
      ultima.episodio == m.episodio && ultima.temporada == m.temporada) {
    ultimaEm = 0;
    if (marcacena_texto(&ultima, texto, sizeof texto) && recenviar_abrir_cena(ci, texto)) return;
    player_toast(i18n("Entre na conta e adicione um amigo para enviar cenas"), 3500);
    return;
  }
  carregar();
  if (marcacena_inserir(lista, &nLista, MARCACENA_MAX, &m) < 0) return;
  gravar();
  ultima = m; ultimaEm = agora ? agora : 1;
  marcacena_tempo(m.seg, tempo, sizeof tempo);
  snprintf(aviso, sizeof aviso, i18n("Cena marcada em %s · aperte de novo para enviar"), tempo);
  player_toast(aviso, 3500);
}

void cenas_armar(const char *imdb, int temporada, int episodio, int seg, int durOrigem) {
  memset(&armada, 0, sizeof armada);
  if (!imdb || !imdb[0] || seg < 0) return;
  snprintf(armada.imdb, sizeof armada.imdb, "%s", imdb);
  armada.t = temporada; armada.e = episodio; armada.seg = seg; armada.dur = durOrigem;
  armada.em = SDL_GetTicks(); armada.ativa = 1;
}

int cenas_aplicar_ao_abrir(void) {
  const CatItem *ci = cat_item(player_indice());
  char tempo[16], aviso[96];
  int t = 0, e = 0;
  conferirDur = 0;
  if (!armada.ativa) return 0;
  if (SDL_GetTicks() - armada.em > CENAS_ARMADA_MS) { armada.ativa = 0; return 0; }
  if (!ci || strcmp(ci->imdb, armada.imdb)) return 0;
  player_episodio_atual(&t, &e);
  if (armada.e > 0 && (t != armada.t || e != armada.e)) return 0;
  armada.ativa = 0;
  player_do_inicio();
  player_definir_retomada_fonte((double)armada.seg);
  conferirDur = armada.dur; conferirSeg = armada.seg;
  marcacena_tempo(armada.seg, tempo, sizeof tempo);
  snprintf(aviso, sizeof aviso, i18n("Abrindo na cena marcada: %s"), tempo);
  player_toast(aviso, 3500);
  return 1;
}

void cenas_atualizar(void) {
  int incerto = 0;
  double local = 0.0;
  if (!conferirDur) return;
  if (!player_aberto()) { conferirDur = 0; return; }
  if (!player_duracao_midia(&local) || local <= 0.0) return;
  // -1: the marked second is past the end of this version.
  if (marcacena_ancorar(conferirSeg, conferirDur, (int)local, &incerto) < 0) incerto = 1;
  conferirDur = 0;
  if (incerto) player_toast(i18n("Esta versão tem outra duração: a cena pode estar um pouco antes ou depois"), 5000);
}
