// #205 "VISTOS FALSOS": a duracao do TRAILER nao pode valer pela do titulo.
//
// O caminho, provado em log de TV (tcl-agora.log, 412.txt) e no #385: o detalhe
// toca o trailer do IMDb pela MESMA pipeline de video do player (trailer.c:
// video_tocar), o prepare do trailer demora, e o evento PRONTO dele — 129 s —
// chega DEPOIS de o dono pedir Reproduzir, sem selo de sessao
// (nv_tpk_video_evento: "tpk evento 1 (129036)"). Ele cai na sessao nova e
// vira a duracao do TITULO (duracaoSeg = 129 s num filme de 106 min). O filme
// real toca; a 69 s de reproducao real a regra da conclusao (pos >= dur-60 e
// pos >= dur/2) da o filme por VISTO, pos := duracaoSeg grava 100% no
// catalogo, o trakt.c manda /scrobble/stop 100%, e visto_titulo marca o filme
// no Simkl e na conta. "Quase todos os filmes da fileira Trending Movies"
// marcados sem ninguem ter visto nada.
//
// Aqui o pipeline e falso (mesmo truque de player_fonte_duracao.c: nenhuma
// janela, TV ou rede) e o teste fabrica o veneno na mao: video_duracao()
// devolve os 129 s do trailer enquanto a "pessoa" assiste segundos reais do
// filme. Os destinos da marcacao viram contadores (renome no .sh): sem token,
// sem rede — e a prova e que NENHUM destino e chamado. Os dois ultimos casos
// sao os controles ao contrario: titulo real assistido de verdade tem que
// continuar marcando, e o filme abandonado no comeco tem que continuar
// gravando o progresso REAL.
#include "player.h"
#include "catalogo.h"
#include "dados.h"
#include "linguas.h"
#include "trakt.h"
#include "visto.h"
#include "vistoep.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "trailer.h"
#include "video.h"
#include "plrui.h"
#include "text.h"
#include "layout.h"
#include "ajustes.h"
#include "trailerfonte.h"
#include "progresso.h"
#include "intro.h"
#include "perfis.h"
#include "descoberta.h"
#include "conquistas.h"
#include "conquistasdados.h"

// Episode metadata stays under host-test control, without background HTTP.
static int carregando;
void desc_episodios(int idx, int temporada) { (void)idx; (void)temporada; }
void desc_episodios_garantir(int idx) { (void)idx; }
void desc_episodios_pendente(void) {}
int desc_episodios_carregando(int idx) { (void)idx; return carregando; }

// Relogio monotono controlado: seek/retomada nao ganham tempo assistido.
static double agoraMono;
int nv_test_clock_gettime(clockid_t id, struct timespec *ts) {
  (void)id;
  ts->tv_sec = (time_t)agoraMono;
  ts->tv_nsec = (long)((agoraMono - ts->tv_sec) * 1e9);
  return 0;
}

// Pipeline falso: pos e dur sob controle total do teste.
static double dur, pos, creditos;
static int ativo, pausado, buffer, terminou;
static char urlAtual[4096];
int video_tocar(const char *url) {
  snprintf(urlAtual, sizeof urlAtual, "%s", url);
  ativo = 1; pausado = buffer = terminou = 0; pos = 0; return 1;
}
void video_parar(void) { ativo = 0; }
void video_pausar(int p) { pausado = p; }
void video_buscar(double p) { pos = p; }
int video_pausa_confirmada(void) { return pausado; }
int video_tocando(void) { return ativo && !pausado; }
unsigned video_bufferando_ms(void) { return (unsigned)buffer; }
int video_pronto(void) { return ativo; }
int video_ativo(void) { return ativo; }
int video_falhou(void) { return 0; }
int video_terminou(void) { return terminou; }
int video_conflito_recurso(void) { return 0; }
double video_pos(void) { return pos; }
double video_duracao(void) { return dur; }
const char *video_url_atual(void) { return urlAtual; }
static const char *const fonte = "https://example.invalid/teste.mp4";
int video_iniciar_auto(void) { return 1; }
double video_creditos(void) { return creditos; }

// OS DESTINOS, virados contadores pelo renome do .sh. A marcacao de vistos
// parte daqui (fecharSessao) para o Trakt (scrobble stop) e para o
// Simkl/conta (visto_titulo) — nenhuma rede aqui, so a contagem.
static int nTrakt, nVisto, concluido = 90, manter, pularAbertura;
int ajustes_cw_concluido(void) { return concluido; }
int ajustes_manter_video(void) { return manter; }
int ajustes_pular_abertura(void) { return pularAbertura; }
static double tkPos, tkDur;   // o que o Trakt recebeu por ultimo
static int tkAssistiu;
void trakt_marcar(const char *imdb, double posSeg, double durSeg, double assistidoSeg, int assistiu) {
  (void)imdb; (void)assistidoSeg; tkPos = posSeg; tkDur = durSeg; tkAssistiu = assistiu; nTrakt++;
}
int visto_titulo(const char *imdb, const char *tipo, const int *temporadas,
                 int nt, int visto, int destinos) {
  (void)imdb; (void)tipo; (void)temporadas; (void)nt; (void)visto; (void)destinos;
  nVisto++;
  return 1;
}
int visto_episodios(const char *imdb, const char *tipo, const VistoPar *pares,
                    int n, int visto, int destinos) {
  (void)imdb; (void)tipo; (void)pares; (void)n; (void)visto; (void)destinos;
  nVisto++;
  return 1;
}

// Mantem os headers do SDK macOS; seleciona so a implementacao nativa real.
#undef __APPLE__
#include "../src/trailer.c"
static void quadro(void) { player_atualizar(.016f, SDL_GetTicks()); }

// Abre, assiste `p` segundos de reproducao real com o pipeline informando `d`
// de duracao (a do trailer, quando e o veneno), e sai — o momento em que
// fecharSessao decide o que grava.
static void abrir(int filme, const char *imdb, const char *meta,
                  const char *durEp, double d, double p) {
  CatItem c = {0}; CatEp ep = {0};
  strcpy(c.titulo, "Fixture"); strcpy(c.imdb, imdb);
  strcpy(c.tipo, filme ? "movie" : "series");
  if (meta) snprintf(c.meta, sizeof c.meta, "%s", meta);
  cat_definir(&c, 1);
  if (durEp) {
    ep.temporada = ep.episodio = 1;
    snprintf(ep.duracao, sizeof ep.duracao, "%s", durEp);
    cat_definir_episodios(0, &ep, 1);
  }
  nTrakt = nVisto = 0;
  player_abrir(0, NULL);
  if (durEp) player_definir_episodio(1, 1);
  dur = d;
  player_definir_fonte(fonte);
  quadro();
  // Dois quadros: o primeiro pode buscar o ponto salvo (retomada); depois
  // disso a posicao e a que a pessoa assistiu de verdade.
  // Reproducao continua real, amostrada uma vez por segundo.
  for (int i = 0; i < (int)p; i++) { agoraMono += 1; pos += 1; quadro(); }
  pos = p; quadro(); quadro();
  player_encerrar();
}

// Filme sem minutagem (so a prova de reproducao decide), ja com a fonte aberta
// e o ponto salvo (`progresso`, em %) retomado.
static void sessao(const char *imdb, int progresso, double d) {
  CatItem c = {0};
  strcpy(c.imdb, imdb); strcpy(c.tipo, "movie"); c.progresso = progresso;
  cat_definir(&c, 1); nTrakt = nVisto = 0; dur = d; creditos = 0;
  player_abrir(0, fonte); quadro(); quadro();
}
// `seg` segundos de relogio tocando a `vel` vezes a velocidade normal.
static void tocar(int seg, double vel) {
  for (int i = 0; i < seg; i++) { agoraMono += 1; pos += vel; quadro(); }
}

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0); dados_iniciar(""); ling_local_legenda("none");

  // Feed real: historico e progresso locais, sem cqregra_registrar no teste.
  cat_historico_definir_id("tt0018578", "movie", 1);
  assert(prog_gravar_local("tt0019729", 0, 0, 1000, 1000, 1));
  assert(prog_gravar_local("tt0020629", 0, 0, 1000, 1000, 0)); // seek nao conta
  assert(prog_gravar_local("tt0021746", 1, 1, 1000, 1000, 1)); // episodio nao e filme
  assert(prog_gravar_local("tt0022958", 0, 0, 1000, 1000, 1));
  cat_historico_definir_id("tt0022958", "movie", 0); // desmarcado vence progresso
  conquistas_abrir();
  {
    CqEstado s; cqd_carregar(&s);
    const CqEntrada *e = cqregra_buscar(&s, "trilha:oscar-melhor-filme");
    assert(e && e->vistos == 2 && e->total == 98 && e->nivel == 1);
  }
  conquistas_teste_esquecer();

  // A REGRA (#205): chegou = tocado+3 >= 90% do trecho base->saida; visto =
  // fim, chegou e base+tocado >= 120 s.
  { int chegou;
    assert(!player_regra_visto(0, 70, 70, 1, &chegou) && chegou);            // S1: curto
    assert(!player_regra_visto(2295, 5, 2400, 1, &chegou) && !chegou);       // S2: seek
    assert(player_regra_visto(2295, 105, 2400, 1, &chegou) && chegou);       // S4
    assert(player_regra_visto(5700, 90, 5790, 1, &chegou) && chegou);        // S5
    assert(!player_regra_visto(2295, 105, 2400, 0, &chegou) && chegou);      // sem fim
    player_regra_visto(0, 897, 1000, 0, &chegou); assert(chegou);
    player_regra_visto(0, 896, 1000, 0, &chegou); assert(!chegou);
    assert(!player_regra_visto(0, 1110, 1260, 1, &chegou));                  // pulo nao somado
    assert(player_regra_visto(0, 1260, 1260, 1, &chegou)); }

  // Sem minutagem: fonte pronta na ultima posicao, sem reproducao real.
  // Antes: fecharSessao arredonda 69/129 e chama TODOS os destinos.
  CatItem semMeta = {0};
  strcpy(semMeta.imdb, "tt90000001"); strcpy(semMeta.tipo, "movie");
  cat_definir(&semMeta, 1); dur = 129.036;
  player_abrir(0, fonte); quadro(); pos = 69; quadro();
  player_encerrar();
  assert(nTrakt == 0 && nVisto == 0 && cat_item(0)->progresso == 0);

  // (1) O RELATO DO #205, filme: meta diz 1 h 46 min, o pipeline (PRONTO do
  // trailer) diz 129 s, a pessoa assiste 69 s e sai. A regra antiga: 69 >=
  // 129-60 e 69 >= 129/2 — CONCLUIU. Nao pode concluir: 69 s nao sao 90% de
  // 106 minutos.
  abrir(1, "tt22687790", "2023 · 1 h 46 min", NULL, 129.036, 69.0);
  assert(nTrakt == 0);                   // nenhum scrobble, nem pause
  assert(nVisto == 0);                   // nem Simkl, nem conta
  assert(cat_item(0)->progresso == 0);   // 69/129 = 53% de mentira, nada.

  // (2) A MESMA SESSAO, quem ficou mais tempo: passou ate ALEM da duracao
  // falsa. Qualquer saida depois de 69 s concluia — assistir 2 min de um
  // filme de 106 min nao e assisti-lo.
  abrir(1, "tt22687791", "1 h 46 min", NULL, 129.036, 130.0);
  assert(nTrakt == 0 && nVisto == 0 && cat_item(0)->progresso == 0);

  // (3) O EPISODIO do #385, com minutagem na lista: o mesmo PRONTO atrasado.
  // A conclusao ja era segurada pela duracao suspeita, mas o PROGRESSO era
  // gravado com a duracao do trailer (69 s de 129 = 53% do episodio).
  abrir(0, "tt22687792", NULL, "43 min", 129.036, 69.0);
  assert(nTrakt == 0 && nVisto == 0);
  assert(vistoep_estado("tt22687792", 1, 1) != 1);   // sem o check local
  assert(cat_item(0)->progresso == 0);

  // (4) CONTROLE POSITIVO, filme assistido de verdade ate o fim: a duracao do
  // pipeline bate com o meta. Tem que continuar marcando em todo destino.
  abrir(1, "tt22687793", "1 h 46 min", NULL, 6357.0, 6300.0);
  assert(nTrakt == 1 && nVisto == 1);
  assert(cat_item(0)->progresso == 100);

  // (5) CONTROLE POSITIVO, episodio ate o fim.
  abrir(0, "tt22687794", NULL, "43 min", 2580.0, 2525.0);
  assert(nTrakt == 1);
  assert(vistoep_estado("tt22687794", 1, 1) == 1);
  assert(cat_item(0)->progresso == 100);

  // Conclusao anterior nao arma aviso ao cancelar o proximo antes da fonte.
  player_abrir(0, NULL);
  assert(!player_saiu_visto());
  player_encerrar();
  assert(!player_saiu_visto());
  abrir(1, "tt90000210", NULL, NULL, 180, 180);
  assert(player_saiu_visto() && !player_saiu_visto());
  abrir(1, "tt90000211", NULL, NULL, 180, 180);
  player_encerrar(); // segundo fechamento invalido tambem limpa a conclusao
  assert(!player_saiu_visto());

  // (6) CONTROLE: filme real abandonado no comeco — progresso REAL gravado,
  // scrobble de progresso (pause) enviado, nada marcado como visto.
  abrir(1, "tt22687795", "1 h 46 min", NULL, 6357.0, 200.0);
  assert(nTrakt == 1 && nVisto == 0);
  assert(cat_item(0)->progresso > 0 && cat_item(0)->progresso < 10);

  // Seek ate o fim nao e tempo assistido, mesmo com a minutagem certa.
  semMeta.progresso = 0; strcpy(semMeta.imdb, "tt90000002");
  cat_definir(&semMeta, 1); nTrakt = nVisto = 0; dur = 2580;
  player_abrir(0, fonte); quadro(); agoraMono += 200; pos = 2525; quadro();
  player_encerrar();
  assert(nTrakt == 0 && nVisto == 0 && cat_item(0)->progresso == 0);

  // Um trailer substitui a pipeline de uma sessao principal ainda aberta.
  // Passa pelo trailer_abrir REAL; ao fechar a sessao nao pode copiar seu fim.
  strcpy(semMeta.imdb, "tt90000003"); cat_definir(&semMeta, 1);
  player_abrir(0, fonte); quadro();
  for (int i = 0; i < 130; i++) { agoraMono += 1; pos += 1; quadro(); }
  dur = 180;
  trailer_abrir("https://example.invalid/trailer.mp4", (GfxRect){0,0,100,100}, 0, 0);
  assert(trailer_aberto());
  pos = 180; quadro(); player_encerrar();
  assert(nTrakt == 0 && nVisto == 0 && cat_item(0)->progresso == 0);
  trailer_fechar();

  // Trailer nativo termina, fecha e deixa os valores antigos na pipeline.
  // A sessao perdeu a fonte; mesmo restaurar a URL nao recupera sua prova.
  strcpy(semMeta.imdb, "tt90000005"); cat_definir(&semMeta, 1); dur = 2580;
  player_abrir(0, fonte); quadro();
  for (int i = 0; i < 2400; i++) { agoraMono += 1; pos += 1; quadro(); }
  trailer_abrir(fonte, (GfxRect){0,0,100,100}, 0, 0);
  dur = pos = 180; terminou = 1;
  trailer_atualizar(SDL_GetTicks()); assert(!trailer_aberto());
  quadro(); player_encerrar();
  assert(nTrakt == 0 && nVisto == 0 && cat_item(0)->progresso == 0);

  // Pausa e buffer nao contam.
  strcpy(semMeta.imdb, "tt90000004"); cat_definir(&semMeta, 1); dur = 2580;
  player_abrir(0, fonte); quadro();
  video_pausar(1);
  for (int i = 0; i < 180; i++) { agoraMono += 1; quadro(); }
  video_pausar(0); buffer = 1;
  for (int i = 0; i < 180; i++) { agoraMono += 1; pos += 1; quadro(); }
  buffer = 0; pos = 2525; quadro(); player_encerrar();
  assert(nTrakt == 0 && nVisto == 0 && cat_item(0)->progresso == 0);

  // Amostras em degraus de 200ms a 60fps: nao perder 95% do tempo real.
  strcpy(semMeta.imdb, "tt90000006"); cat_definir(&semMeta, 1); dur = 180;
  player_abrir(0, fonte); quadro(); quadro();
  for (int i = 1; i <= 170 * 60; i++) {
    agoraMono += 1.0 / 60.0; pos = (i / 12) * 0.2; quadro();
  }
  player_encerrar();
  assert(nTrakt == 1 && nVisto == 1 && cat_item(0)->progresso == 100);

  // Sem fonte (preparo/pre-busca): o video e de um trailer, nao do player.
  nTrakt = nVisto = 0; strcpy(semMeta.imdb, "tt90000007"); cat_definir(&semMeta, 1);
  player_abrir(0, NULL); dur = 180;
  trailer_abrir(fonte, (GfxRect){0,0,100,100}, 0, 0);
  for (int i = 0; i < 180; i++) { agoraMono += 1; pos += 1; quadro(); }
  player_encerrar(); trailer_fechar();
  assert(nTrakt == 0 && nVisto == 0 && cat_item(0)->progresso == 0);

  // Regressao sem catalogo: 69s reais com duracao falsa de 129s nao concluem.
  abrir(1, "tt90000008", NULL, NULL, 129.036, 69);
  assert(nTrakt == 1 && nVisto == 0 && cat_item(0)->progresso < 90);

  // --- #205 NAO PODE REGREDIR O #100 (segunda revisao) -----------------------
  //
  // A prova exigia 90% da DURACAO em tempo de relogio. Quem via tudo e saia
  // pelo caminho que o proprio app oferece ficava abaixo disso.

  // Aceitar o proximo no marcador de creditos: 2400 s de 2700 = 88,9%.
  sessao("tt90000010", 0, 2700); creditos = 2400;
  tocar(2400, 1); player_encerrar();
  assert(nTrakt == 1 && nVisto == 1 && cat_item(0)->progresso == 100);
  assert(tkPos == tkDur && tkAssistiu);

  // Abertura de 90 s pulada, saida a 60 s do fim de um episodio de 22 min.
  sessao("tt90000011", 0, 1320);
  tocar(30, 1); pos += 90; quadro(); tocar(1140, 1); player_encerrar();
  assert(nTrakt == 1 && nVisto == 1 && cat_item(0)->progresso == 100);

  // 1,5x: o relogio anda 1684 s, a midia tocada sao 2526 s.
  sessao("tt90000012", 0, 2580);
  tocar(1684, 1.5); player_encerrar();
  assert(nTrakt == 1 && nVisto == 1 && cat_item(0)->progresso == 100);

  // Retomada em 60% e o resto assistido: o load novo nao pode custar o visto.
  sessao("tt90000013", 60, 6000); assert(pos == 3600);
  tocar(2350, 1); player_encerrar();
  assert(nTrakt == 1 && nVisto == 1 && cat_item(0)->progresso == 100);

  // O mesmo, saindo a 100 s do fim (o cenario da revisao): o fim ainda nao
  // chegou, mas a POSICAO tem de ser gravada — ficava nos 60% de antes. Sem o
  // fim nao ha visto, entao o Trakt recusa a saida (>= 90% sem visto).
  sessao("tt90000014", 60, 6000);
  tocar(2300, 1); player_encerrar();
  assert(nTrakt == 1 && nVisto == 0 && cat_item(0)->progresso == 98);
  assert(tkPos == 5900 && !tkAssistiu);

  // SEEK ATE O FIM (log do C9, Dexter S1E2): 99 s tocados, busca, mais 34 s e
  // sai a 19 s do fim. Nao viu — mas esta ali: grava a posicao REAL (nem os
  // 100% do arredondamento, nem nada), e o Trakt recebe a saida sem prova,
  // que e o que ele recusa (tests/trakt_contracts.c).
  sessao("tt90000015", 0, 2843);
  tocar(99, 1); pos = 2790; quadro(); tocar(34, 1); player_encerrar();
  assert(nVisto == 0 && cat_item(0)->progresso > 90 && cat_item(0)->progresso < 100);
  assert(nTrakt == 1 && tkPos == 2824 && !tkAssistiu);

  // RETOMADA PERTO DO FIM (terceira revisao): 85% de 2700 s, mais 285 s ate os
  // creditos em 2580 s e aceita o proximo. A prova media metade ate o FIM DO
  // ARQUIVO (202,5 s) sem contar a cauda: gravava 96% e o Trakt ficava sem
  // stop. A cauda so entra depois dos 120 s reais (sexta revisao).
  sessao("tt90000016", 85, 2700); creditos = 2580; assert(pos == 2295);
  tocar(285, 1); player_encerrar();
  assert(nTrakt == 1 && nVisto == 1 && cat_item(0)->progresso == 100);
  assert(tkPos == tkDur);
  // A mesma retomada buscando o fim: o credito dos creditos nao vira prova.
  sessao("tt90000017", 85, 2700); creditos = 2400;
  tocar(10, 1); pos = 2660; quadro(); tocar(5, 1); player_encerrar();
  assert(nVisto == 0 && cat_item(0)->progresso < 100 && tkPos < tkDur);

  // Sem minutagem, duracao falsa de 129 s, 70 s reais: o resto ate o fim
  // (59 s) nao pode ser creditado como assistido — gravava 100% e visto.
  creditos = 0; abrir(1, "tt90000018", NULL, NULL, 129.036, 70);
  assert(nVisto == 0 && cat_item(0)->progresso < 90);
  assert(!tkAssistiu);

  // Retomada em 85% de 2700 s, 5 s tocados e busca direto aos creditos em
  // 2400 s: os 300 s que sobram nao viram prova de 5 s assistidos.
  sessao("tt90000019", 85, 2700); creditos = 2400;
  tocar(5, 1); pos = 2400; quadro(); player_encerrar();
  assert(nVisto == 0 && cat_item(0)->progresso < 100);
  assert(tkPos < tkDur && !tkAssistiu && prog_tocou("tt90000019") == 0);

  // Filme sem historico: 133 s tocados, busca e sai em 2824/2843 s. Sem prova
  // o progresso gravado nao pode acender o selo de visto (cat_visto >= 90%) —
  // mas a posicao gravada e a REAL: truncar em 89% (2530 s) fazia a retomada
  // voltar quase cinco minutos (sexta revisao).
  sessao("tt90000020", 0, 2843);
  tocar(133, 1); pos = 2824; quadro(); player_encerrar();
  assert(nVisto == 0 && !cat_visto(cat_item(0)));
  assert(cat_item(0)->progresso == 99 && tkPos == 2824);
  { ProgRegistro r;   // a posicao REAL tambem na pendencia do sync
    assert(prog_por_chave("tt90000020", &r) && r.pendente && r.posSeg == 2824 && r.tocou == 0); }

  // O mesmo selo depois de reiniciar o app (ou trocar de perfil e voltar): o
  // progresso volta do disco a 99%, sem historico. A prova esta no disco junto
  // com a posicao — nada em memoria segura o selo.
  prog_invalidar();
  { CatItem c = {0}; strcpy(c.imdb, "tt90000020"); strcpy(c.tipo, "movie");
    c.progresso = 99; cat_definir(&c, 1); }
  assert(cat_historico_estado_id("tt90000020", "movie") < 0 && !cat_visto(cat_item(0)));
  perfis_definir_ativo(2); prog_invalidar(); assert(prog_tocou("tt90000020") == -1);
  perfis_definir_ativo(1); prog_invalidar();
  assert(!cat_visto(cat_item(0)) && prog_tocou("tt90000020") == 0);
  { ProgRegistro r; assert(prog_por_chave("tt90000020", &r) && r.posSeg == 2824); }

  // E assistir o filme inteiro depois disso marca, selo inclusive: a conclusao
  // real substitui a falta de prova (sem conta vinculada nenhum historico vem).
  sessao("tt90000020", 99, 2843); assert(pos == 0);
  tocar(2843, 1); player_encerrar();
  assert(nVisto == 1 && cat_item(0)->progresso == 100 && cat_visto(cat_item(0)));
  assert(prog_tocou("tt90000020") == 1);

  // Sem minutagem, retomada em 1% da duracao falsa de 129,036 s, 70 s reais e
  // sai: a cauda (57 s) completava os 120 s minimos — 100%, visto e stop. A
  // prova e so segundo real tocado.
  sessao("tt90000021", 1, 129.036);
  tocar(70, 1); player_encerrar();
  assert(nVisto == 0 && cat_item(0)->progresso < 90);
  assert(tkPos < tkDur && !tkAssistiu);

  // PARAR PERTO DO FIM E TERMINAR DEPOIS (setima revisao): a prova e a soma
  // gravada. 85% de 2700 s assistidos numa sessao; noutra, depois de reiniciar,
  // a retomada toca 105 s ate os creditos e conclui.
  sessao("tt90000022", 0, 2700); creditos = 2400; tocar(2295, 1); player_encerrar();
  assert(nVisto == 0 && cat_item(0)->progresso == 85);
  prog_invalidar();
  sessao("tt90000022", 85, 2700); creditos = 2400; assert(pos == 2295);
  tocar(105, 1); player_encerrar();
  assert(nTrakt == 1 && nVisto == 1 && cat_item(0)->progresso == 100 && tkPos == tkDur);

  // O mesmo com 95% de 6000 s e mais 90 s ate os creditos (marcador em 5790 s).
  sessao("tt90000023", 0, 6000); creditos = 5790; tocar(5700, 1); player_encerrar();
  assert(nVisto == 0 && cat_item(0)->progresso == 95);
  concluido = 98; sessao("tt90000023", 95, 6000); creditos = 5790; assert(pos == 5700);
  tocar(90, 1); player_encerrar(); concluido = 90;
  assert(nTrakt == 1 && nVisto == 1 && cat_item(0)->progresso == 100 && tkPos == tkDur);

  // S1 COM REGISTRO: 133 s reais de 2843 s gravados numa sessao; na seguinte o
  // PRONTO do trailer (129 s) vira a duracao e 70 s reais chegam ao "fim". A
  // base de 2843 s nao vale contra 129 s: so os 70 s contam.
  sessao("tt90000024", 0, 2843); tocar(133, 1); player_encerrar();
  assert(prog_tocou("tt90000024") == 1);
  { int pct = cat_item(0)->progresso;
    sessao("tt90000024", pct, 129.036); }
  tocar(70, 1); player_encerrar();
  assert(nVisto == 0 && !tkAssistiu && cat_item(0)->progresso < 90);

  // S7: registro sem o bit (linha de 10 colunas, ou vindo do servidor) e base
  // confiavel — S4 e S5 continuam marcando.
  assert(prog_gravar_local("tt90000027", 0, 0, 2295, 2700, -1));
  sessao("tt90000027", 85, 2700); creditos = 2400; assert(pos == 2295);
  tocar(105, 1); player_encerrar();
  assert(nVisto == 1 && tkAssistiu && prog_tocou("tt90000027") == 1);
  assert(prog_gravar_local("tt90000028", 0, 0, 5700, 6000, -1));
  concluido = 98; sessao("tt90000028", 95, 6000); creditos = 5790; assert(pos == 5700);
  tocar(90, 1); player_encerrar(); concluido = 90;
  assert(nVisto == 1 && tkAssistiu);

  // S8: suspender e retomar (Manter o video pronto) duas vezes. A suspensao
  // grava o progresso; a base continua a da abertura e os segundos tocados
  // nao sao contados de novo. Base 600 s tocados; 510 s tocados e busca ao
  // fim (650 s de trecho): nao chegou tocando.
  sessao("tt90000025", 0, 1300); tocar(600, 1); player_encerrar();
  assert(prog_tocou("tt90000025") == 1 && cat_item(0)->progresso == 46);
  sessao("tt90000025", 46, 1300); assert(pos == 598);
  manter = 1;
  for (int k = 0; k < 2; k++) {
    tocar(170, 1);
    player_preparar_retencao(); assert(player_suspender());
    assert(player_retomar_retido("tt90000025", 0, 0));
    video_pausar(0);
  }
  tocar(170, 1); pos = 1250; quadro(); player_encerrar(); manter = 0;
  assert(nVisto == 0 && !tkAssistiu && prog_tocou("tt90000025") == 0);

  // O PULO OFICIAL da abertura conta como tocado: 30 s, pula 150 s sozinho
  // (#329) e toca ate o fim — sem o credito do pulo faltariam 21 s nos 90%.
  sessao("tt90000026", 0, 1320); pularAbertura = 1;
  { IntroTrecho t = {30, 180, INTRO_ABERTURA}; intro_definir_capitulos(&t, 1); }
  tocar(30, 1); quadro(); assert(pos > 180);
  tocar(1080, 1); player_encerrar(); pularAbertura = 0;
  intro_definir_capitulos(NULL, 0);
  assert(nVisto == 1 && tkAssistiu);

  // #379: EOF sai do filme/ultimo episodio, nunca da pausa ou do canal.
  for (int caso = 0; caso < 4; caso++) {
    CatItem c = {0}; CatEp eps[2] = {{0}};
    strcpy(c.imdb, "tt90000379");
    strcpy(c.tipo, caso == 0 ? "movie" : caso == 3 ? "channel" : "series");
    cat_definir(&c, 1);
    eps[0].temporada = eps[0].episodio = 1;
    eps[1].temporada = 1; eps[1].episodio = 2;
    if (caso == 1 || caso == 2) cat_definir_episodios(0, eps, caso == 2 ? 2 : 1);
    dur = 3600; player_abrir(0, fonte);
    if (caso == 1 || caso == 2) player_definir_episodio(1, 1);
    quadro(); quadro();
    pos = 3599; pausado = 1; quadro();
    assert(!player_quer_sair()); // pausa no ultimo segundo nao e EOF
    terminou = 1; quadro();
    assert(player_quer_sair() == (caso < 2));
    if (caso < 2) {
      for (int i = 0; i < 120 && player_aberto(); i++) quadro();
      assert(!player_aberto() && player_quer_sair());
      assert(!player_suspender()); // EOF nao conserva o decoder para retomada
    }
    player_encerrar();
    assert(!video_ativo());
  }

  // EOF de trailer enquanto o titulo espera uma fonte nao pede saida.
  player_abrir(0, NULL);
  trailer_abrir(fonte, (GfxRect){0,0,100,100}, 0, 0);
  terminou = 1; quadro();
  assert(!player_quer_sair());
  player_encerrar(); trailer_fechar();

  // Conclusao via player atualiza e persiste, mesmo sem abrir Conquistas.
  abrir(1, "tt0020629", NULL, NULL, 180, 180);
  {
    CqEstado s; cqd_carregar(&s);
    const CqEntrada *e = cqregra_buscar(&s, "trilha:oscar-melhor-filme");
    assert(e && e->vistos == 3 && e->nivel == 1);
    prog_invalidar(); cqd_carregar(&s);
    assert(cqregra_buscar(&s, "trilha:oscar-melhor-filme")->vistos == 3);
  }
  abrir(1, "tt0020629", NULL, NULL, 180, 20);
  assert(!player_saiu_visto());
  conquistas_abrir();
  {
    CqEstado s; cqd_carregar(&s);
    assert(cqregra_buscar(&s, "trilha:oscar-melhor-filme")->vistos == 3);
  }
  conquistas_teste_esquecer();

  puts("PASS: trailer, seek, pausa e buffer nao marcam; reproducao real grava progresso e visto");
  return 0;
}
