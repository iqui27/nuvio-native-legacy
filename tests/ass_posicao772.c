// #772/F83: a guarda de divergencia do renderer (assrender.c, "parado") tem
// de comparar a leitura do relogio ANTES do mapa legsync com o numero cru do
// video. Em 17a20ba8f ela comparava o posSeg ja mapeado: com escala/trechos
// do AutoSync (25 vs 23,976 fps => 104,27 s aos 100 s de video) a diferenca
// passa de 1,5 s sem salto nenhum — pedidos e promocao ficam bloqueados, a
// fala valida some em ASS e SRT via plainass e, ao cruzar o limiar, a fala
// anterior congela em intervalo vazio.
//
// Sonda sol-f70 transformada em teste do repo:
//   escala — AutoSync real (primaria 25 fps, referencia 23,976 fps, offset
//            0), fala 103-110 s, reproducao continua 99->103 s: 210/210
//            quadros com imagem (17a20ba8f: 0/210).
//   cruza  — mesmo mapa; fala termina em 36,70 s do tempo da legenda e o
//            intervalo 36,80-37,00 s nao tem falas: 0 quadros com imagem
//            (17a20ba8f: 12/12, a fala anterior congelada).
//   bordas — sem mapa: saltos de 1,49/1,50/1,51 s alinhados mostram a fala
//            do destino; pausa e retomada mantem 24/24; offset de busca de
//            +2000 ms e troca de faixa na mesma modalidade entram (o que a
//            F70 reparou continua valendo).
// Tudo em ASS authored e em SRT via plainass_document.
//
// O teste informa ao renderer as duas posicoes como o player faz:
// assrender_posicao_video(numero cru do pipeline) e
// assrender_posicao_legenda(leitura do relogio antes do mapa); o desenhar
// continua recebendo a posicao ja mapeada, que o libass usa p/ achar a fala.
// Em 17a20ba8f assrender_posicao_legenda nao existe — o simbolo fica fraco,
// a leitura nao e registrada e a guarda velha reprova o caso.

extern void assrender_posicao_video(double) __attribute__((weak));
extern void assrender_posicao_legenda(double) __attribute__((weak));
#define main main_fixture_ass
#include "ass_salto772.c"
#undef main
#define main main_fixture_autosync
#include "autosync.c"
#undef main
#include "../src/plainass.h"

static const char *ass_longa = CAB
  "Dialogue: 0,0:01:43.00,0:01:50.00,Default,,0,0,0,,Fala com FPS corrigido\n";
static const char *ass_troca = CAB
  "Dialogue: 0,0:00:02.20,0:00:02.80,Default,,0,0,0,,Outra faixa\n";
static const char *ass_cruza = CAB
  "Dialogue: 0,0:00:35.90,0:00:36.70,Default,,0,0,0,,Fala que termina\n"
  "Dialogue: 0,0:00:37.10,0:00:38.80,Default,,0,0,0,,Fala seguinte\n";

static void add_font(const char *name, const char *path) {
  size_t bytes;
  char *font = ler(path, &bytes);
  assert(font);
  assrender_adicionar_fonte(name, font, bytes);
  free(font);
}

static void preparar(unsigned g, int srt, int fixture) {
  assrender_geracao(g);
  assrender_limpar_fontes();
  add_font("InterDisplay-Regular.ttf", "deploy/app/fonts/InterDisplay-Regular.ttf");
  if (srt) {
    LegendaCue cues[] = {
      {.inicio = 1, .fim = 3, .texto = "كلام البداية"},
      {.inicio = 20, .fim = 22, .texto = "كلام المشهد التالي"},
    };
    if (fixture == 1) {
      cues[0].inicio = 103; cues[0].fim = 110;
      cues[1].inicio = 115; cues[1].fim = 117;
    } else if (fixture == 2) {
      cues[0].inicio = 2.2; cues[0].fim = 2.8;
      cues[1].inicio = 24; cues[1].fim = 25;
    } else if (fixture == 3) {
      cues[0].inicio = 35.90; cues[0].fim = 36.70;
      cues[1].inicio = 37.10; cues[1].fim = 38.80;
    }
    PlainAssStyle estilo = {.size = 28, .rgb = 0xffffff, .marginV = 20};
    snprintf(estilo.font, sizeof estilo.font, "Noto Naskh Arabic");
    add_font("NotoNaskhArabic-Regular.ttf", "deploy/app/fonts/NotoNaskhArabic-Regular.ttf");
    char *doc = plainass_document(cues, 2);
    assert(doc);
    assert(assrender_carregar_texto(doc, strlen(doc), g));
    free(doc);
    assrender_definir_texto_estilo(&estilo);
  } else {
    const char *docs[] = {DOC, ass_longa, ass_troca, ass_cruza};
    const char *doc = docs[fixture];
    assert(assrender_carregar(doc, strlen(doc), g));
  }
  assert(assrender_ativo());
}

static void video(double t) {
  if (assrender_posicao_video) assrender_posicao_video(t);
}

/* A leitura do relogio ANTES do mapa — o que posLegenda() entrega ao player
 * e o que o renderer precisa para medir a divergencia com o video. */
static void leitura(double t) {
  if (assrender_posicao_legenda) assrender_posicao_legenda(t);
}

static int draw(double t, int atraso) {
  return assrender_desenhar(t, atraso, 1.0f, 0, 0, 1920, 1080);
}

static int obter(double t, int atraso) {
  for (int q = 1; q <= 90; ++q) {
    if (draw(t, atraso) > 0) return q;
    quadro();
  }
  return 0;
}

static int bordas(int srt) {
  unsigned g = 1;
  int erros = 0;
  const double saltos[] = {1.49, 1.50, 1.51};
  for (int i = 0; i < 3; ++i) {
    preparar(g++, srt, 0);
    video(18.75); leitura(18.75);
    assert(draw(18.75, 0) == 0);
    video(18.75 + saltos[i]); leitura(18.75 + saltos[i]);
    int q = obter(18.75 + saltos[i], 0);
    printf("seek %.2fs alinhado: primeiro quadro com fala=%d\n", saltos[i], q);
    erros += !q;
  }
  preparar(g++, srt, 0);
  video(2.25); leitura(2.25);
  int q = obter(2.25, 0), pausa = 0, retomada = 0;
  for (int i = 0; i < 24; ++i) {
    quadro();
    video(2.25); leitura(2.25);
    pausa += draw(2.25, 0) > 0;
  }
  for (int i = 1; i <= 24; ++i) {
    double t = 2.25 + i / 60.0;
    quadro();
    video(t); leitura(t);
    retomada += draw(t, 0) > 0;
  }
  printf("fala inicial=%d pausa=%d/24 retomada=%d/24\n", q, pausa, retomada);
  erros += !q || pausa != 24 || retomada != 24;

  preparar(g++, srt, 0);
  video(18.5); leitura(18.5);
  int atrasada = obter(18.5, 2000);
  printf("offset de busca +2000 ms: primeiro quadro=%d\n", atrasada);
  erros += !atrasada;

  preparar(g++, srt, 2);
  video(2.5); leitura(2.5);
  int imediato = draw(2.5, 0), troca = obter(2.5, 0);
  printf("troca na mesma modalidade: imediato=%d, primeiro quadro da faixa nova=%d\n", imediato, troca);
  erros += imediato != 0 || !troca;
  return erros != 0;
}

static int cruzamento(AutoSync *sync, int srt) {
  preparar(1, srt, 3);
  assert(assrender_quadro_cpu(36.4) > 0);
  int primeira = 0, intervalo = 0, contagem = 0;
  for (int q = 0; q < 150; ++q) {
    double raw = 34.5 + q / 60.0;
    double t = autosync_posicao(sync, 0, raw);
    video(raw);
    leitura(raw);
    int desenhado = draw(t, 0) > 0;
    if (t >= 36.15 && t <= 36.40) primeira += desenhado;
    if (t >= 36.80 && t <= 37.00) {
      assert(t - raw > 1.5);
      intervalo += desenhado;
      ++contagem;
    }
    quadro();
  }
  printf("cruzamento em play: fala inicial visivel em %d quadros, intervalo sem falas com imagem=%d/%d\n",
         primeira, intervalo, contagem);
  assert(primeira > 0 && contagem > 0);
  return intervalo != 0;
}

static int escala(int srt, int cruzar) {
  criarCorpus();
  const unsigned flags = LEGENDA_DOC_COMPLETO;
  LegendaDocumento *ref = documento("embedded", "en", 0, 1, flags, 11, 0);
  LegendaDocumento *alvo = documento("fps", "en", 0, 25.0 / 23.976, flags, 11, 0);
  AutoSync *sync = autosync_criar();
  assert(sync && ref && alvo);
  autosync_iniciar(sync, 11);
  assert(autosync_selecionar(sync, 0, alvo) && autosync_solicitar(sync, 0, ref, NULL));
  AutoSyncResultado e = esperar(sync, 0);
  assert(e.estado == AUTOSYNC_ACCEPTED && e.tipo == AUTOSYNC_T_ESCALA);
  int offset = autosync_offset_ms(sync, 0);
  double raw = 100.0, mapeada = autosync_posicao(sync, 0, raw);
  printf("AutoSync aceito em slot primario: escala=%.9f offset=%d bruto=%.3f mapeado=%.3f\n",
         e.escala, offset, raw, mapeada);
  assert(offset == 0 && mapeada > 103 && mapeada < 110 && fabs(mapeada - raw) > 1.5);
  if (cruzar) {
    int rc = cruzamento(sync, srt);
    autosync_destruir(sync);
    legenda_documento_liberar(alvo);
    legenda_documento_liberar(ref);
    return rc;
  }
  preparar(1, srt, 1);
  assert(assrender_quadro_cpu(mapeada) > 0);
  Relogio rel;
  relogio_zerar(&rel);
  relogio_taxa(&rel, 1);
  int visiveis = 0;
  double erro_max = 0;
  for (int q = 0; q < 240; ++q) {
    double mono = 200 + q / 60.0;
    raw = 99 + q / 60.0;
    video(raw);
    relogio_amostra(&rel, raw, mono, 1);
    double leituraRelogio = relogio_ler(&rel, mono);
    leitura(leituraRelogio);
    erro_max = fmax(erro_max, fabs(leituraRelogio - raw));
    mapeada = autosync_posicao(sync, 0, leituraRelogio);
    assert(mapeada > 103 && mapeada < 110);
    quadro();
    if (q >= 30) visiveis += draw(mapeada, offset) > 0;
    else draw(mapeada, offset);
  }
  printf("play continuo: 210 quadros apos aquecimento, visiveis=%d; erro max relogio/video=%.6fs\n",
         visiveis, erro_max);
  autosync_destruir(sync);
  legenda_documento_liberar(alvo);
  legenda_documento_liberar(ref);
  return visiveis != 210;
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  assert(argc == 3);
  int srt = strcmp(argv[2], "srt") == 0;
  if (strcmp(argv[1], "escala") == 0) return escala(srt, 0);
  if (strcmp(argv[1], "cruza") == 0) return escala(srt, 1);
  assert(strcmp(argv[1], "bordas") == 0);
  return bordas(srt);
}
