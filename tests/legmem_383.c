// #383 (Samsung S95C, .tpk 2.0.4): "remember the subtitle track I picked so it
// picks the same for next episode". A memoria existe desde a 2.0.3
// (legmemoria.c) e mesmo assim o episodio seguinte abria SEM legenda.
//
// A CAUSA: a faixa exata so volta quando o episodio seguinte tem a mesma faixa
// (embutida: mesmo numero; addon: mesmo id = provedor + URL, e a URL e de UM
// episodio, entao nunca casa no seguinte). Sem a exata sobra o IDIOMA da
// escolha — e ai a regra do #287 (audio ja no idioma da legenda = so a
// forcada) apagava a escolha feita a mao: serie em ingles, legenda em ingles,
// episodio seguinte -> "audio em 'en' = legenda 'en', sem forcada: nenhuma".
// No .tpk e o caso comum: o player da Samsung nao lista legenda de imagem
// (PGS), entao quem ve remux escolhe a legenda no addon.
//
// Roda a decisao REAL de faixas.c com o mundo falso de tests/legmemoria.c.
#define main legmemoria_roteiro
#include "legmemoria.c"
int xtream_url_limitada(const char *url) { (void)url; return 0; }
#undef main

static void addonsDoEpisodio(const char *ep) {
  int i;
  nAdds = 2;
  for (i = 0; i < 2; i++) {
    memset(&adds[i], 0, sizeof adds[i]);
    snprintf(adds[i].idioma, sizeof adds[i].idioma, "en");
    snprintf(adds[i].provedor, sizeof adds[i].provedor, i ? "Subdl" : "OpenSubtitles");
    snprintf(adds[i].url, sizeof adds[i].url, "http://s/%s-%d.srt", ep, i);
  }
}

int main(void) {
  int r;
  forcadaAuto = 1;   // #287, o padrao de Ajustes
  snprintf(aud[0].idioma, sizeof aud[0].idioma, "en");

  // Registro de builds antigos: forcada/letreiros tambem tinham id vazio.
  // A faixa 2 do episodio anterior sumiu; #287 deve escolher a forcada 8.
  dados_gravar("legmemoria-p3.txt",
               "# nuvio legendas v1\n*\ten\n"
               "tt2861424\t1\ten\te\t2\t\tEnglish forced\n");
  legmem_definir_perfil(3);
  titulo("tt2861424:1:2");
  nAdds = 0; nLeg = 2;
  faixa(0, "en", 7, LING_LEG_COMUM); faixa(1, "en", 8, LING_LEG_FORCADA);
  r = abrir(-1);
  printf("registro antigo com id vazio, episodio seguinte: %d\n", r);
  assert(r == 1);
  faixa(1, "en", 8, LING_LEG_LETREIROS);
  assert(abrir(-1) == 1);
  nLeg = 1;
  assert(abrir(-1) == -1);

  legmem_definir_perfil(1);

  // .tpk antes da sonda: a forcada escolhida ainda tem tipo/letreiro zerados.
  titulo("tt0386676:1:1");
  nAdds = 0; nLeg = 2;
  faixa(0, "en", 1, LING_LEG_COMUM); faixa(1, "en", 2, LING_LEG_COMUM);
  aberta = 1; faixas_escolher_embutida(1); aberta = 0;
  // A classificacao chega depois da escolha, sem nova escolha manual.
  leg[1].tipoLeg = LING_LEG_FORCADA; leg[1].letreiro = 1;
  legmem_definir_perfil(2);
  legmem_definir_perfil(1);
  titulo("tt0386676:1:2");
  faixa(0, "en", 7, LING_LEG_COMPLETA); faixa(1, "en", 8, LING_LEG_FORCADA);
  r = abrir(-1);
  printf("forcada escolhida antes da sonda, episodio seguinte: %d\n", r);
  assert(r == 1);
  assert(!legmem_do_titulo("tt0386676")->id[0]);

  // 1. ADDON. Remux so com PGS: o player nao lista embutida nenhuma.
  titulo("tt0903747:1:1");
  nLeg = 0; addonsDoEpisodio("s01e01");
  assert(abrir(-1) == -1);                 // nunca escolheu: nenhuma (#287)
  aberta = 1; faixas_escolher_externa(&adds[1]); aberta = 0;
  titulo("tt0903747:1:2");
  addonsDoEpisodio("s01e02");              // outras URLs: a exata nao existe
  r = abrir(-1);
  printf("episodio seguinte, legenda de addon em ingles com audio em ingles: %d (%s)\n", r, urlExterna);
  assert(r >= 0 && !strncmp(urlExterna, "http://s/s01e02-", 16));

  // 2. EMBUTIDA em outro numero no episodio seguinte (outra fonte/release).
  titulo("tt0944947:1:1");
  nAdds = 0; nLeg = 2;
  faixa(0, "es", 1, LING_LEG_COMUM); faixa(1, "en", 2, LING_LEG_COMPLETA);
  aberta = 1; faixas_escolher_embutida(1); aberta = 0;
  legmem_definir_perfil(2);
  legmem_definir_perfil(1);               // marcador da inteira sobrevive ao disco
  titulo("tt0944947:1:2");
  faixa(0, "en", 5, LING_LEG_COMUM); faixa(1, "es", 6, LING_LEG_COMUM);
  r = abrir(-1);
  printf("episodio seguinte, embutida em ingles noutro numero: %d\n", r);
  assert(r == 0);

  // 3. CONTROLE: a FORCADA escolhida a mao continua forcada (nunca a inteira).
  titulo("tt2861424:1:1");
  nLeg = 2;
  faixa(0, "en", 1, LING_LEG_COMUM); faixa(1, "en", 2, LING_LEG_FORCADA);
  aberta = 1; faixas_escolher_embutida(1); aberta = 0;
  titulo("tt2861424:1:2");
  faixa(0, "en", 7, LING_LEG_COMUM); faixa(1, "en", 8, LING_LEG_FORCADA);
  assert(abrir(-1) == 1);
  nLeg = 1;                                // sem forcada: nenhuma, como antes
  assert(abrir(-1) == -1);

  // 4. CONTROLE: OUTRO titulo segue o #287 (so a ultima escolha, nao deste).
  titulo("tt0068646");
  nLeg = 1; faixa(0, "en", 1, LING_LEG_COMUM);
  assert(abrir(-1) == -1);

  puts("#383: a escolha a mao deste titulo vence o #287 no episodio seguinte ok");
  return 0;
}
