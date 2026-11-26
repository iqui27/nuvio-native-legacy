// Minha noite (minhanoite.h): a grade deterministica do canal pessoal, a
// ordem de zapping com os canais de IPTV e a regra do gesto.
#include "minhanoite.h"

#include <stdio.h>
#include <string.h>

static int falhas;
#define CONFERE(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

static MnEntrada ent(const char *id, const char *tipo, int t, int e, int origem) {
  MnEntrada x;
  memset(&x, 0, sizeof x);
  snprintf(x.id, sizeof x.id, "%s", id);
  snprintf(x.tipo, sizeof x.tipo, "%s", tipo);
  snprintf(x.titulo, sizeof x.titulo, "T %s", id);
  x.temporada = t; x.episodio = e; x.origem = origem;
  return x;
}

static void montar(MnFonte *f, int inverso) {
  MnEntrada v[5];
  int i;
  v[0] = ent("tt001", "movie", 0, 0, MN_CONTINUAR);
  v[1] = ent("tt002", "series", 2, 5, MN_SERIE);
  v[2] = ent("tt003", "movie", 0, 0, MN_LISTA);
  v[3] = ent("tt004", "series", 1, 1, MN_LISTA);
  v[4] = ent("tt005", "series", 3, 9, MN_CONTINUAR);
  mn_fonte_limpar(f);
  for (i = 0; i < 5; i++) CONFERE(mn_fonte_juntar(f, &v[inverso ? 4 - i : i]));
}

int main(void) {
  MnFonte f, g;
  MnPrograma p, q;
  const long fuso = -3 * 3600;
  // 2026-11-22 21:17:00 -03
  const time_t t = 1795393020;
  int i;

  // --- montagem -------------------------------------------------------------
  mn_fonte_limpar(&f);
  CONFERE(!mn_programa(&f, t, fuso, &p));                 // canal vazio
  { MnEntrada canal = ent("globo", "channel", 0, 0, MN_LISTA);
    MnEntrada semEp = ent("tt9", "series", 1, 0, MN_SERIE);
    MnEntrada semId = ent("", "movie", 0, 0, MN_LISTA);
    CONFERE(!mn_fonte_juntar(&f, &canal));                // canal ao vivo nao entra
    CONFERE(!mn_fonte_juntar(&f, &semEp));                // serie sem episodio nao entra
    CONFERE(!mn_fonte_juntar(&f, &semId)); }
  { MnEntrada a = ent("tt7", "movie", 0, 0, MN_LISTA), b = ent("tt7", "movie", 0, 0, MN_CONTINUAR);
    CONFERE(mn_fonte_juntar(&f, &a) && mn_fonte_juntar(&f, &b));
    CONFERE(f.n == 1 && f.e[0].origem == MN_CONTINUAR);   // um id, a origem mais forte
    CONFERE(mn_fonte_juntar(&f, &a) && f.e[0].origem == MN_CONTINUAR); }
  mn_fonte_limpar(&f);
  for (i = 0; i < MN_MAX + 3; i++) {
    char id[16]; MnEntrada x;
    snprintf(id, sizeof id, "tt%04d", i);
    x = ent(id, "movie", 0, 0, MN_LISTA);
    CONFERE(mn_fonte_juntar(&f, &x) == (i < MN_MAX));     // teto sem estouro
  }
  CONFERE(f.n == MN_MAX && mn_programa(&f, t, fuso, &p));

  // --- determinismo ---------------------------------------------------------
  montar(&f, 0); montar(&g, 1);
  CONFERE(mn_programa(&f, t, fuso, &p) && mn_programa(&g, t, fuso, &q));
  // A ordem em que o catalogo chegou nao muda a grade.
  CONFERE(!strcmp(f.e[p.indice].id, g.e[q.indice].id) && p.ini == q.ini && p.fim == q.fim);
  CONFERE(p.ini <= t && t < p.fim);
  CONFERE(p.fracao >= 0.0 && p.fracao <= MN_ENTRADA_MAX);
  // Mesma pergunta, mesma resposta.
  CONFERE(mn_programa(&f, t, fuso, &q) && q.indice == p.indice && q.ini == p.ini && q.fracao == p.fracao);
  // Dentro do mesmo bloco o programa e o mesmo e a fracao so cresce.
  if (p.fim - t > 60) {
    CONFERE(mn_programa(&f, t + 60, fuso, &q) && q.indice == p.indice && q.ini == p.ini);
    CONFERE(q.fracao >= p.fracao);
  }
  // Blocos em multiplos de 5 min, alinhados ao dia local.
  CONFERE(((long)(p.ini + fuso) % 300) == 0);
  // O dia inteiro e coberto sem buraco nem sobreposicao.
  { time_t ini = t - ((t + fuso) % 86400), x = ini;
    int blocos = 0;
    while (x < ini + 86400 && blocos < 400) {
      CONFERE(mn_programa(&f, x, fuso, &q) && q.ini == x && q.fim > x && q.fim <= ini + 86400);
      CONFERE(q.fracao == 0.0);
      x = q.fim; blocos++;
    }
    CONFERE(x == ini + 86400 && blocos > 5); }
  // O que esta pela metade abre o dia: o primeiro bloco e de Continuar.
  { time_t ini = t - ((t + fuso) % 86400);
    CONFERE(mn_programa(&f, ini, fuso, &q) && f.e[q.indice].origem == MN_CONTINUAR); }
  // O dia muda a ordem dentro da mesma origem em algum dia da semana.
  { int mudou = 0, d;
    time_t ini = t - ((t + fuso) % 86400);
    MnFonte h; mn_fonte_limpar(&h);
    for (i = 0; i < 6; i++) { char id[16]; MnEntrada x;
      snprintf(id, sizeof id, "tt9%02d", i); x = ent(id, "movie", 0, 0, MN_LISTA); mn_fonte_juntar(&h, &x); }
    CONFERE(mn_programa(&h, ini, fuso, &p));
    for (d = 1; d < 7; d++)
      if (mn_programa(&h, ini + d * 86400L, fuso, &q) && q.indice != p.indice) mudou = 1;
    CONFERE(mudou); }
  // O fuso desloca a virada do dia: 00:30 UTC ainda e a vespera em -03.
  CONFERE(mn_dia(86400 + 1800, 0) == 1 && mn_dia(86400 + 1800, fuso) == 0);
  CONFERE(mn_dia(-1, 0) == -1);

  // --- a seguir -------------------------------------------------------------
  CONFERE(mn_programa(&f, t, fuso, &p));
  CONFERE(mn_proximo(&f, t, fuso, 0, &q) && q.ini == p.fim && q.fracao == 0.0);
  { MnPrograma r;
    CONFERE(mn_proximo(&f, t, fuso, 1, &r) && r.ini == q.fim); }
  // Atravessa a meia-noite.
  { time_t fimDia = t - ((t + fuso) % 86400) + 86400;
    CONFERE(mn_programa(&f, fimDia - 30, fuso, &p) && p.fim == fimDia);
    CONFERE(mn_proximo(&f, fimDia - 30, fuso, 0, &q) && q.ini == fimDia); }
  CONFERE(!mn_proximo(&f, t, fuso, -1, &q));

  // --- entrada no meio ------------------------------------------------------
  { MnFonte um; MnEntrada x = ent("tt1", "movie", 0, 0, MN_LISTA);
    time_t ini = t - ((t + fuso) % 86400);
    mn_fonte_limpar(&um); x.durSeg = 100 * 60; mn_fonte_juntar(&um, &x);
    CONFERE(mn_programa(&um, ini + 50 * 60, fuso, &p) && p.fracao > 0.49 && p.fracao < 0.51);
    // Perto do fim a entrada para em MN_ENTRADA_MAX: ninguem cai nos creditos.
    CONFERE(mn_programa(&um, ini + 99 * 60, fuso, &p) && p.fracao == MN_ENTRADA_MAX);
    // Um titulo so: o canal repete, sem buraco.
    CONFERE(mn_programa(&um, ini + 101 * 60, fuso, &p) && p.ini == ini + 100 * 60); }

  // --- zapping --------------------------------------------------------------
  CONFERE(mn_zap(0, 1, 0) == MN_ZAP_IPTV);     // meio da lista: zap normal
  CONFERE(mn_zap(0, 1, 1) == MN_ZAP_MINHA);    // ponta da lista: entra a Minha noite
  CONFERE(mn_zap(0, 0, 1) == MN_ZAP_IPTV);     // canal vazio: a lista da a volta como antes
  CONFERE(mn_zap(1, 1, 0) == MN_ZAP_PONTA);    // da Minha noite: volta para a lista
  CONFERE(mn_zap(1, 0, 1) == MN_ZAP_PONTA);

  // --- o gesto --------------------------------------------------------------
  CONFERE(!mn_conta(1, MN_GESTO_NENHUM));      // aberta sozinha: nao conta
  CONFERE(mn_conta(1, MN_GESTO_DO_INICIO));
  CONFERE(mn_conta(1, MN_GESTO_DAQUI));
  CONFERE(mn_conta(0, MN_GESTO_NENHUM));       // sessao comum sempre contou

  if (falhas) { printf("minhanoite: %d falha(s)\n", falhas); return 1; }
  printf("minhanoite: ok\n");
  return 0;
}
