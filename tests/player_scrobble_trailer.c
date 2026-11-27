// B6: trailer (PRONTO de 129 s sobre um titulo de 1 h 46) nao scrobbla ao
// vivo — nem start nem pause, Trakt ou Simkl. Titulo real continua falando.
// Contadores POR EVENTO E SERVICO, checados ANTES do player_encerrar: o
// scrobble de SAIDA so satisfaz um contador unico, nunca o evento certo.
#define main main_base
#define trakt_scrobble_alvo nv_fixture_trakt_scrobble_alvo
#include "player_visto_trailer.c"
#undef trakt_scrobble_alvo
#undef main
#include "traktscrobble.h"   // SCR_EV_TOCANDO / SCR_EV_PAUSOU / SCR_EV_SAIU
static double posTk[SCR_EV_PONTO + 1], durTk[SCR_EV_PONTO + 1], posSk[SCR_EV_PONTO + 1], durSk[SCR_EV_PONTO + 1];
static int nTk[SCR_EV_PONTO + 1], nSk[SCR_EV_PONTO + 1];   // [SCR_EV_TOCANDO, SCR_EV_PAUSOU, SCR_EV_SAIU]
static void zera(void) { memset(nTk, 0, sizeof nTk); memset(nSk, 0, sizeof nSk); }
int nv_cnt_trakt_scrobble(int ev, const char *id, double p, double d, double a, int x) {
  (void)id; (void)a; (void)x; assert(ev > 0 && ev <= SCR_EV_PONTO); posTk[ev] = p; durTk[ev] = d; nTk[ev]++; return 0;
}
int nv_cnt_trakt_scrobble_alvo(int ev, const TraktScrobbleAlvo *alvo,
                               double p, double d, double a, int x) {
  assert(alvo && alvo->id[0]);
  return nv_cnt_trakt_scrobble(ev, alvo->id, p, d, a, x);
}
int nv_cnt_simkl_scrobble(int ev, const char *id, double p, double d, double a, int x) {
  (void)id; (void)a; (void)x; assert(ev > 0 && ev <= SCR_EV_PONTO); posSk[ev] = p; durSk[ev] = d; nSk[ev]++; return 0;
}
// Abre o titulo `imdb` ("1 h 46 min" no catalogo) com a pipeline dizendo `d`.
static void ao_vivo(const char *imdb, double d) {
  CatItem c = {0};
  strcpy(c.titulo, "Fixture"); strcpy(c.imdb, imdb); strcpy(c.tipo, "movie");
  strcpy(c.meta, "1 h 46 min");
  cat_definir(&c, 1); dur = d; zera();
  player_abrir(0, NULL); player_definir_fonte(fonte); quadro();
}
// `seg` SEGUNDOS DE DT (16 ms por quadro): >= 5 s cruza o limiar do start —
// 60 quadros so acumulam 0,96 s e nao provam nada do ramo de start.
static void tocar_dt(double seg) {
  for (int i = 0, n = (int)(seg / .016) + 1; i < n; i++) { agoraMono += 1; pos += .05; quadro(); }
}
// Canonical episode tracking must retain upstream's whole-movie coordinates
// when the source is one file from a multipart release (rebase regression).
#include "partes.h"
static void multipart_scrobble(void) {
  ParteArq arquivos[2] = {{0}};
  PartesLista lista = {0};
  double vp, vd;
  arquivos[0].bytes = arquivos[1].bytes = 1000;
  arquivos[0].id = 1; arquivos[1].id = 2;
  lista.estado = PARTES_PRONTO; lista.n = 2; lista.arq = arquivos;
  ao_vivo("tt22687804", 3200);
  partes_sessao_iniciar("tt22687804", &lista, 1);
  tocar_dt(5.2);
  assert(partes_conta_virtual(&lista, 1, pos, dur, 6360, &vp, &vd));
  assert(nTk[SCR_EV_TOCANDO] && nSk[SCR_EV_TOCANDO]);
  assert(fabs(posTk[SCR_EV_TOCANDO] - vp) < 1.0);
  assert(fabs(durTk[SCR_EV_TOCANDO] - vd) < 1.0);
  assert(fabs(posSk[SCR_EV_TOCANDO] - vp) < 1.0);
  pausado = 1; quadro(); quadro();
  assert(nTk[SCR_EV_PAUSOU] && nSk[SCR_EV_PAUSOU]);
  assert(fabs(posTk[SCR_EV_PAUSOU] - vp) < 1.0);
  assert(fabs(durTk[SCR_EV_PAUSOU] - vd) < 1.0);
  player_encerrar(); pausado = 0;
  assert(nTk[SCR_EV_SAIU] && nSk[SCR_EV_SAIU]);
  assert(fabs(posTk[SCR_EV_SAIU] - vp) < 1.0);
  assert(fabs(durTk[SCR_EV_SAIU] - vd) < 1.0);
  assert(fabs(durSk[SCR_EV_SAIU] - vd) < 1.0);
  partes_sessao_parar("fixture done");
}
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0); dados_iniciar(""); ling_local_legenda("none");

  // (1) TRAILER: PRONTO de 129 s num titulo de 1 h 46, mais de 5 s de dt.
  // Sem a guarda de START o /scrobble/start disparava aqui mesmo.
  ao_vivo("tt22687801", 129.036);
  tocar_dt(5.2);
  assert(nTk[SCR_EV_TOCANDO] == 0 && nSk[SCR_EV_TOCANDO] == 0);
  pausado = 1; quadro(); quadro();
  assert(nTk[SCR_EV_PAUSOU] == 0 && nSk[SCR_EV_PAUSOU] == 0);   // guarda de PAUSE
  player_encerrar(); pausado = 0;
  assert(nTk[SCR_EV_TOCANDO] == 0 && nSk[SCR_EV_TOCANDO] == 0 &&
         nTk[SCR_EV_PAUSOU] == 0 && nSk[SCR_EV_PAUSOU] == 0);

  // (2) CONTROLE: titulo real (6357 s) scrobbla start e pause nos dois servicos.
  ao_vivo("tt22687802", 6357.0);
  tocar_dt(5.2);
  assert(nTk[SCR_EV_TOCANDO] >= 1 && nSk[SCR_EV_TOCANDO] >= 1);
  pausado = 1; quadro(); quadro();
  assert(nTk[SCR_EV_PAUSOU] >= 1 && nSk[SCR_EV_PAUSOU] >= 1);
  player_encerrar(); pausado = 0;

  // (3) O PRONTO ATRASADO chega DEPOIS de um start valido: a duracao vira a do
  // trailer com o start em pe (tocouS > 0). Nenhum start novo, nenhum pause —
  // e o primeiro quadro suspeito so nao vira pause por causa da guarda.
  ao_vivo("tt22687803", 6357.0);
  tocar_dt(5.2);
  assert(nTk[SCR_EV_TOCANDO] >= 1 && nSk[SCR_EV_TOCANDO] >= 1);
  { int t1 = nTk[SCR_EV_TOCANDO], s1 = nSk[SCR_EV_TOCANDO];
    dur = 129.036;                 // a pipeline troca a duracao pela do trailer
    tocar_dt(5.2);                 // continua tocando: start nao fala de novo
    assert(nTk[SCR_EV_TOCANDO] == t1 && nSk[SCR_EV_TOCANDO] == s1);
    assert(nTk[SCR_EV_PAUSOU] == 0 && nSk[SCR_EV_PAUSOU] == 0);
    pausado = 1; quadro(); quadro();
    assert(nTk[SCR_EV_PAUSOU] == 0 && nSk[SCR_EV_PAUSOU] == 0); }
  player_encerrar(); pausado = 0;
  assert(nTk[SCR_EV_PAUSOU] == 0 && nSk[SCR_EV_PAUSOU] == 0);

  multipart_scrobble();

  puts("ok");
  return 0;
}