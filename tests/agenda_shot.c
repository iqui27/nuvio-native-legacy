// CAPTURAS DA AGENDA, sem interacao e sem rede.
//
// Existe pelo motivo que tests/ajustes_shot.c e tests/social_shot.c ja
// registram: interface de TV julgada so por codigo sai ilegivel a 3 m. Duas
// armadilhas ja pegas neste repositorio SO aparecem olhando: o raio do
// gfx_cor e FRACAO DA ALTURA (passar pixel desenha capsula), e texto claro
// sobre superficie clara some.
//
// O que cada foto prova:
//   -vazia          o estado de quem nao acompanha nada — a tela tem de
//                   explicar o que fazer, nao ficar preta;
//   -hoje           a LINHA DO TEMPO no topo: a faixa de origem "HOJE", o no com
//                   aura da estreia de hoje, os numerais alinhados contra o eixo
//                   e a sinopse que a linha focada abre;
//   -curta          a MESMA coluna da direita com uma sinopse de UMA linha: e o
//                   caso que fazia a laje antiga ficar com o terco de baixo
//                   vazio, e agora a linha tem a mesma altura das outras;
//   -mes            a faixa de mes quando o calendario vira, a espera longa
//                   ("em N semanas") sem a data por extenso engolindo a coluna,
//                   e a linha focada SEM sinopse — a coluna da direita fica
//                   vazia de proposito, em vez de receber enchimento;
//   -mesvazio       hoje em AGOSTO com a primeira estreia em SETEMBRO. Prova o
//                   defeito que o dono fotografou: saiam DOIS cabecalhos de mes
//                   e o de cima nao tinha uma linha embaixo. Tem de sair UM.
//   -semdata        o separador, o eixo TRACEJADO e as series encerradas /
//                   canceladas com a SITUACAO no lugar do numeral;
//   -reduzido       a mesma tela com "reduzir animacoes": o despertador nao
//                   treme e as ondas ficam paradas e opacas — o estado do
//                   lembrete NAO pode depender de animar;
//   -aviso          o cartao do lembrete vencido, que e o unico aviso que uma
//                   TV sem push consegue dar;
//   -menu           a barra lateral aberta com o item Agenda e o icone novo;
//   -semchave       a Agenda SEM chave do TMDB (a Samsung do dono, 22/09):
//                   series que so o progresso/lembrete conhece, preenchidas
//                   pelo Cinemeta FALSO desta captura — nome, cartaz, data e
//                   genero no lugar de "Serie" e do retangulo cinza —, o
//                   aviso ambar de fonte sob a legenda e "Sem data
//                   confirmada" na serie que nenhuma fonte datou;
//   -tmdbdesligado  o mesmo com a chave no pacote e o ajuste TMDB desligado:
//                   o aviso passa a apontar Ajustes > Integracoes > TMDB;
//   -fx-*           SEM REDE NENHUMA (fixtures): a linha focada com a citacao,
//                   o MODAL que o OK abre (acoes + historico de lancamentos
//                   desde o lembrete, com assistido / nao assistido /
//                   desconhecido), o cartao de leitura da manchete em foco, a
//                   noticia aberta com capa e o fallback com QR; e as mesmas
//                   linhas em alemao e russo com textos longos (-fx-de-*,
//                   -fx-ru-*), que e onde o texto estourava o cartao;
//   -lembrete-*     os quatro estados do botao circular do lembrete no hero:
//                   desligado/ligado, em repouso/em foco. O ligado em repouso e
//                   o circulo ESMERALDA; o focado e a superficie clara com o
//                   relogio escuro, e a linha acima dele vira a legenda do
//                   botao — que e o unico nome que um circular mudo tem.
//
// OS DADOS ENTRAM PELO DISCO (agenda-p1.txt e lembretes-p1.txt dentro de
// NUVIO_DADOS), como em social_shot.c: assim a foto prova tambem o FORMATO do
// arquivo — se ele mudar e a leitura nao acompanhar, a tela sai vazia.
#include "agenda.h"
#include "nvtmp.h"
#include "agendaui.h"
#include "noticias.h"
#include "layout.h"
#include "agendaviso.h"
#include "menu.h"
#include "detail.h"
#include "home.h"
#include "catalogo.h"
#include "dados.h"
#include "ajustes.h"
#include "rail_shot.h"
#include "perfis.h"
#include "shot_arte.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "descoberta.h"
#include "noticia.h"
#include "leitura.h"
#include "vistoep.h"
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { DES_AGENDA = 0, DES_AVISO, DES_MENU, DES_DETALHE };
// Ajustes gravados em disco e lidos por ajustes_dir: e o caminho publico para
// escolher idioma e "reduzir animacoes" sem setter de teste. As chaves sao as
// mesmas de ajustes.txt no aparelho.
// NUVIO_SHOT_THEME e opcional para provar os estados accent sem mudar o padrao.
// IDIOMA POR VARIAVEL DE AMBIENTE, com o padrao em portugues.
//
// As capturas deste harness servem a DOIS publicos: a conferencia do trabalho,
// que e feita em portugues como o resto do repositorio, e o album do post em
// ingles. Recompilar para trocar a lingua e o tipo de atrito que faz alguem
// publicar a captura errada — NUVIO_SHOT_EN=1 resolve sem tocar no codigo.
// FONTE DA INTERFACE POR VARIAVEL: NUVIO_SHOT_FONTE=3 (Montserrat, a da TV do
// dono) grava a mesma chave "fonteInterface" do ajustes.txt do aparelho.
static void fonteDeTeste(FILE *f) {
  const char *fo = getenv("NUVIO_SHOT_FONTE");
  if (fo && *fo) fprintf(f, "fonteInterface %d\n", atoi(fo));
}
static void ajustesDeTeste(int idiomaIngles, int animReduzidas) {
  char caminho[600];
  const char *temaEnv = getenv("NUVIO_SHOT_THEME");
  FILE *f;
  { const char *en = getenv("NUVIO_SHOT_EN");
    if (en && *en && *en != '0') idiomaIngles = 1; }
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  if (!f) return;
  fprintf(f, "idioma %d\nanimacoes %d\n", idiomaIngles, animReduzidas);
  fonteDeTeste(f);
  shot_arte_material(f);   // NUVIO_SHOT_VIDRO=0: o material solido
  if (temaEnv && *temaEnv) {
    char *fim;
    long tema = strtol(temaEnv, &fim, 10);
    if (*fim == '\0' && tema >= 0 && tema < 12)
      fprintf(f, "selected_theme %ld\n", tema);
  }
  fclose(f);
  ajustes_dir(dados_dir());
}
static int oQue;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  if (oQue == DES_MENU) menu_evento(&e);
  else { agendaui_evento(&e); e.type = SDL_KEYUP; agendaui_evento(&e); }
}
// OK SEGURADO na Agenda: KEYDOWN, espera passar NV_HOLD_MS, KEYUP — e no
// KEYUP que agendaui decide entre lembrete (toque) e menu de contexto.
static void segurarOk(void) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  agendaui_evento(&e);
  SDL_Delay(NV_HOLD_MS + 100);
  e.type = SDL_KEYUP;
  agendaui_evento(&e);
}

static void teclaDet(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  detail_evento(&e);
  e.type = SDL_KEYUP;
  detail_evento(&e);
}

// A ARTE DO MOCKUP ATRAS (so com NUVIO_SHOT_ARTE): o fundo do quadro 8 e a arte
// com o veu em gradiente do .fundo do glass-ilha.html (preto a 70% no topo, 92%
// dos 60% para baixo). Sem a variavel, o fundo liso de sempre.
static void arteDeFundo(void) {
  int y;
  if (!getenv("NUVIO_SHOT_ARTE") || !*getenv("NUVIO_SHOT_ARTE")) return;
  shot_arte_desenhar(0.0f);
  for (y = 0; y < 1080; y += 6) {
    float t = y / 648.0f, a;
    a = t >= 1.0f ? 0.92f : 0.70f + 0.22f * t;
    gfx_cor((GfxRect){ 0, (float)y, 1920, 6 }, 0, 0.0235f, 0.0275f, 0.035f, a);
  }
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  rail_shot_aplicar();
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    // ORCAMENTO LARGO de decodificacao por quadro. Com 6 (o valor do aparelho,
    // onde a arte chega ao longo de varios segundos) as capturas do hero saiam
    // com o fundo vazio ou nao, conforme a sorte da corrida — e uma foto que
    // muda de uma rodada para a outra nao serve para julgar nada.
    tex_bombear(32);
    gfx_novo_quadro();
    switch (oQue) {
      case DES_AVISO:   agendaviso_atualizar(1.0f / 60.0f, SDL_GetTicks()); break;
      case DES_MENU:    menu_atualizar(1.0f / 60.0f, SDL_GetTicks());       break;
      case DES_DETALHE: detail_atualizar(1.0f / 60.0f, SDL_GetTicks());     break;
      default:          agendaui_atualizar(1.0f / 60.0f, SDL_GetTicks());   break;
    }
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    arteDeFundo();
    switch (oQue) {
      case DES_AVISO:   agendaviso_desenhar(SDL_GetTicks()); break;
      case DES_MENU:    menu_desenhar(SDL_GetTicks());       break;
      case DES_DETALHE: detail_desenhar(SDL_GetTicks());     break;
      default:          agendaui_desenhar(SDL_GetTicks());   break;
    }
    if (oQue != DES_MENU && oQue != DES_DETALHE) rail_shot_desenhar(MENU_AGENDA);
    if (i == 89) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
  // CUSTO DE TEXTURA DA TELA, por captura. A Agenda desenha um cartaz por linha
  // e o teto de decodificacao sai da LARGURA de desenho (tex_obter_larg), entao
  // mexer no tamanho do cartaz mexe na memoria da tela — e sem numero a
  // discussao vira gosto. `quentes` e o que foi desenhado neste quadro ou no
  // anterior, que e exatamente o conjunto que a tela precisa.
  { int it, pend, q; long b, bq;
    tex_estatisticas(&it, &pend, &b, &q, &bq);
    printf("[tex] %s: quentes=%d %ld KB (cache %d itens, %ld KB)\n",
           nome, q, bq / 1024, it, b / 1024); }
}

// Uma linha de agenda-p1.txt, no formato que agenda.c le:
// imdb TAB titulo TAB poster TAB situacao TAB temp TAB ep TAB nomeEp TAB
// dataProx TAB dataUlt TAB visto TAB sinopse TAB tipoEp TAB rede TAB duracao
// TAB temporadas
//
// Os cinco ultimos entraram com a linha do tempo. Escrever por AQUI, e nao por
// agenda_registrar_extra, e o que faz a foto provar tambem o FORMATO do
// arquivo: se a gravacao e a leitura discordarem, a tela sai sem sinopse e a
// captura mostra.
static void poeLinha(char *dst, size_t tam, const char *imdb, const char *titulo,
                     const char *poster, int sit, int t, int e,
                     const char *nomeEp, const char *prox, const char *ult,
                     const char *sinopse, const char *tipoEp, const char *rede,
                     int duracao, int temporadas) {
  size_t n = strlen(dst);
  snprintf(dst + n, tam - n,
           "%s\t%s\t%s\t%d\t%d\t%d\t%s\t%s\t%s\t%lld\t%s\t%s\t%s\t%d\t%d\n",
           imdb, titulo, poster, sit, t, e, nomeEp, prox, ult, 4000000000LL,
           sinopse, tipoEp, rede, duracao, temporadas);
}

// O CINEMETA FALSO da captura -semchave. Os cartazes sao arquivos locais: a
// captura nao tem rede, e tex_obter_larg le caminho de disco do mesmo jeito.
// tt5550004 nao existe (404): e a serie que fica com o nome de reserva.
static char *cinemetaFalso(const char *url, int segundos, const char *const *cab, int *st) {
  static const struct { const char *id, *json; } R[] = {
    { "tt5550001", "{\"meta\":{\"name\":\"Slow Horses\",\"status\":\"Continuing\","
      "\"poster\":\"deploy/app/art/07.jpg\",\"genres\":[\"Thriller\"],\"runtime\":\"48 min\","
      "\"videos\":[{\"season\":5,\"episode\":2,\"name\":\"Missing Persons\",\"released\":\"2026-09-10T05:00:00.000Z\"},"
      "{\"season\":5,\"episode\":3,\"name\":\"Uncle Sam\",\"released\":\"2026-09-18T05:00:00.000Z\","
      "\"overview\":\"Lamb descobre quem mandou seguir River.\"}]}}" },
    { "tt5550002", "{\"meta\":{\"name\":\"The Diplomat\",\"status\":\"Continuing\","
      "\"poster\":\"deploy/app/art/08.jpg\",\"genres\":[\"Drama\"],\"runtime\":\"50 min\","
      "\"videos\":[{\"season\":3,\"episode\":1,\"name\":\"Estreia\",\"released\":\"2026-10-02T08:00:00.000Z\"}]}}" },
    { "tt5550003", "{\"meta\":{\"name\":\"Only Murders in the Building\","
      "\"poster\":\"deploy/app/art/09.jpg\",\"genres\":[\"Comedy\"],"
      "\"videos\":[{\"season\":6,\"episode\":1,\"name\":\"Sem dia\"}]}}" },
    { "tt5550005", "{\"meta\":{\"name\":\"Mindhunter\",\"status\":\"Ended\","
      "\"poster\":\"deploy/app/art/10.jpg\",\"genres\":[\"Crime\"],\"runtime\":\"60 min\","
      "\"videos\":[{\"season\":2,\"episode\":9,\"name\":\"Final\",\"released\":\"2019-08-16T07:00:00.000Z\"}]}}" },
  };
  size_t i;
  (void)segundos; (void)cab;
  *st = 404;
  if (!strstr(url, "v3-cinemeta.strem.io/meta/series/")) return NULL;
  for (i = 0; i < sizeof R / sizeof R[0]; i++)
    if (strstr(url, R[i].id)) { *st = 200; return strdup(R[i].json); }
  return NULL;
}

// --- AS FIXTURES DO MODAL E DAS NOTICIAS ------------------------------------
//
// A grade do Cinemeta da serie da captura: T3E5..T3E9, com o lembrete ligado em
// 20/08 (lembretes-p4.txt) e "hoje" em 16/09 — cinco episodios na janela.
static char *cinemetaModal(const char *url, int segundos, const char *const *cab, int *st) {
  (void)segundos; (void)cab;
  *st = 404;
  if (!strstr(url, "v3-cinemeta.strem.io/meta/series/tt777000")) return NULL;
  *st = 200;
  // A serie ALEMA tem grade propria (T2), com nomes de episodio compostos e
  // longos: e o que testa o corte da coluna de nome no historico.
  if (strstr(url, "tt7770002"))
    return strdup("{\"meta\":{\"name\":\"Die Schule\",\"videos\":["
      "{\"season\":2,\"episode\":5,\"name\":\"Weltraumbahnhofsicherheitsbeauftragtenversammlung\",\"released\":\"2026-09-04T07:00:00.000Z\"},"
      "{\"season\":2,\"episode\":6,\"name\":\"Die Rückkehr der Donaudampfschifffahrtsgesellschaftskapitänswitwe\",\"released\":\"2026-09-11T07:00:00.000Z\"},"
      "{\"season\":2,\"episode\":7,\"name\":\"Donaudampfschifffahrtsgesellschaftskapitänswitwe\",\"released\":\"2026-09-18T07:00:00.000Z\"}]}}");
  return strdup("{\"meta\":{\"name\":\"Foundation\",\"videos\":["
    "{\"season\":3,\"episode\":5,\"name\":\"The Pleasure of Your Company\",\"released\":\"2026-08-22T07:00:00.000Z\"},"
    "{\"season\":3,\"episode\":6,\"name\":\"Shadows in the Math\",\"released\":\"2026-08-29T07:00:00.000Z\"},"
    "{\"season\":3,\"episode\":7,\"name\":\"A Song for the End of Everything\",\"released\":\"2026-09-05T07:00:00.000Z\"},"
    "{\"season\":3,\"episode\":8,\"name\":\"The Paths That Choose Us\",\"released\":\"2026-09-12T07:00:00.000Z\"},"
    "{\"season\":3,\"episode\":9,\"name\":\"The Last Empress\",\"released\":\"2026-09-16T07:00:00.000Z\"},"
    "{\"season\":3,\"episode\":10,\"name\":\"Sem dia\"}]}}");
}

static char *lerTudo(const char *nome) {
  FILE *f = fopen(nome, "rb"); long n; char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  if (b) { size_t k = fread(b, 1, (size_t)n, f); b[k] = 0; }
  fclose(f);
  return b;
}

// Uma string para JSON (aspas e barras escapadas; o resto passa em UTF-8).
static void jsonStr(char *dst, size_t tam, const char *s) {
  size_t n = 0;
  if (n + 1 < tam) dst[n++] = '"';
  for (; *s && n + 3 < tam; s++) {
    if (*s == '"' || *s == '\\') dst[n++] = '\\';
    dst[n++] = *s;
  }
  if (n + 1 < tam) dst[n++] = '"';
  dst[n] = 0;
}

// A REDE DA NOTICIA, falsa. "https://worker.test/arc" e o caminho da Samsung (JSON do
// worker, o mesmo parse de noticia.c) montado com o que o EXTRATOR tirou da
// fixture real tests/fixtures/noticia-arc.html — so a capa troca por uma arte
// local, porque a captura nao tem rede nem cache de disco de imagem. O IMDb
// devolve 202 vazio, que e o que ele devolve de verdade a um cliente sem
// JavaScript (medido 29/09/2026): e o fallback com QR.
static char *noticiaFalsa(const char *url, const char *corpo, int *st) {
  (void)corpo;
  *st = 404;
  if (strstr(url, "/v1/noticia?u=")) {
    static char js[16384];
    char *html = lerTudo("tests/fixtures/noticia-arc.html");
    Leitura L;
    char t[1400];
    size_t n = 0;
    int i;
    if (!html) return NULL;
    leitura_extrair(html, "https://www.estadao.com.br/minha-serie/play/233083-foundation-2-temporada/", &L);
    free(html);
    n += (size_t)snprintf(js + n, sizeof js - n, "{\"url\":\"https://www.estadao.com.br/minha-serie/play/233083-foundation-2-temporada/\",");
    jsonStr(t, sizeof t, L.titulo); n += (size_t)snprintf(js + n, sizeof js - n, "\"titulo\":%s,", t);
    jsonStr(t, sizeof t, L.resumo); n += (size_t)snprintf(js + n, sizeof js - n, "\"resumo\":%s,", t);
    n += (size_t)snprintf(js + n, sizeof js - n, "\"imagem\":\"deploy/app/art/12.jpg\",");
    jsonStr(t, sizeof t, L.site); n += (size_t)snprintf(js + n, sizeof js - n, "\"site\":%s,\"paragrafos\":[", t);
    for (i = 0; i < L.n; i++) {
      jsonStr(t, sizeof t, L.par[i]);
      n += (size_t)snprintf(js + n, sizeof js - n, "%s%s", i ? "," : "", t);
    }
    snprintf(js + n, sizeof js - n, "]}");
    *st = 200;
    return strdup(js);
  }
  if (strstr(url, "imdb.com")) { *st = 202; return strdup(""); }
  return NULL;
}

// As manchetes da serie da captura, pelo DISCO (noticias2-<imdb>-<lingua>.txt,
// o formato que noticias.c grava): "agora" no topo para a validade de 6 h
// valer, e os instantes relativos a agora para "há 3 h", "ontem", "há 4 dias".
static void noticiasFixture(const char *imdb, const char *lingua, const char *const *manch, int nm) {
  static const char *const FONTE[] = { "Estadão", "IMDb", "Omelete", "Variety", "Collider" };
  static const int HORAS[] = { 3, 26, 96, 150, 400 };
  static const char *const LINK[] = { "https://worker.test/arc", "https://www.imdb.com/pt/news/ni64147012/",
                                      "https://worker.test/arc", "https://worker.test/arc", "https://worker.test/arc" };
  char nome[120], txt[8192];
  size_t n = 0;
  long long agora = (long long)time(NULL);
  int i;
  n += (size_t)snprintf(txt + n, sizeof txt - n, "%lld\n", agora);
  for (i = 0; i < nm && i < 5; i++)
    n += (size_t)snprintf(txt + n, sizeof txt - n, "%ld\t%s\t%s\t%s\t%lld\t%s\n",
                          20260916L - i, i == 4 ? "30 ago" : "16 set", FONTE[i], manch[i],
                          agora - (long long)HORAS[i] * 3600LL, LINK[i]);
  snprintf(nome, sizeof nome, "noticias2-%s-%s.txt", imdb, lingua);
  dados_gravar_leve(nome, txt);
}

static void idiomaDeTeste(int idioma) {
  char caminho[600];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  if (!f) return;
  fprintf(f, "idioma %d\nanimacoes 0\n", idioma);
  fonteDeTeste(f);
  fclose(f);
  ajustes_dir(dados_dir());
}

static void esperaHistorico(const char *imdb) {
  AgEp h[8];
  int k, est = AG_HIST_BUSCANDO;
  for (k = 0; k < 300; k++) {
    agenda_historico(imdb, h, 8, &est);
    if (est == AG_HIST_PRONTO || est == AG_HIST_FALHOU) break;
    SDL_Delay(10);
  }
  printf("historico de %s: estado %d\n", imdb, est);
}

// Espera o fio da agenda e deixa a tela remontar (agendaui_atualizar remonta
// na borda 1 -> 0 de agenda_atualizando).
static void esperaFio(void) {
  int k;
  for (k = 0; k < 300 && agenda_atualizando(); k++) SDL_Delay(10);
  printf("fio da agenda terminou: %d\n", !agenda_atualizando());
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-agenda");
  char nome[600];
  char cache[20000] = "";
  SDL_Window *w;
  SDL_GLContext gl;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: revisao da Agenda", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(160);
  gfx_icones_dir("deploy/app/art");

  dados_iniciar(".");
  { const char *esperado = getenv("NUVIO_DADOS");
    if (!esperado || !esperado[0] || strcmp(dados_dir(), esperado)) {
      printf("FALHA: dados_dir() e [%s], esperado [%s]. Rode por tests/agenda_shot.sh.\n",
             dados_dir(), esperado ? esperado : "(vazia)");
      return 1;
    } }
  agenda_definir_hoje("2026-09-16");
  // Portugues: a chave da tabela de traducao E o portugues, e e nele que o dono
  // julga a tela. Com a build em ingles as palavras novas (as que ainda nao
  // entraram em idioma_tab.h) apareceriam soltas no meio do ingles e a foto
  // diria mais sobre a tabela que sobre o desenho.
  ajustesDeTeste(0, 0);

  // --- 1. O ESTADO VAZIO ----------------------------------------------------
  agenda_iniciar();
  agenda_montar();
  snprintf(nome, sizeof nome, "%s-vazia.bmp", saida);
  captura(nome, w);

  // --- os dados, pelo disco -------------------------------------------------
  //
  // As sinopses sao INVENTADAS, e so podem ser aqui: o corpo do TMDB nao entra
  // no teste (nao ha rede) e o que a foto precisa provar e que duas linhas de
  // sinopse cabem e cortam. Nenhuma delas chega perto do aparelho.
  poeLinha(cache, sizeof cache, "tt10255564", "Foundation",
           "deploy/app/art/00.jpg", AG_VOLTANDO, 3, 9, "The Last Empress",
           "2026-09-16", "2026-09-12",
           getenv("NUVIO_SHOT_SO")
             ? "Gaal e Salvor chegam a Trantor no dia em que o Império anuncia o fim da linhagem genética."
             : "Gaal and Salvor reach Trantor on the day the Empire announces the "
               "end of the genetic dynasty, and the Foundation has to decide whether "
               "the Plan is still worth anything after three hundred years.",
           "finale", "Apple TV+", 58, 3);
  poeLinha(cache, sizeof cache, "tt1520211", "The Last of Us",
           "deploy/app/art/01.jpg", AG_VOLTANDO, 2, 4, "Day One",
           "2026-09-17", "2026-09-10",
           "Ellie atravessa Seattle sozinha.",
           "standard", "HBO", 52, 2);
  poeLinha(cache, sizeof cache, "tt2661044", "Severance",
           "deploy/app/art/02.jpg", AG_VOLTANDO, 3, 1, "The Other Half",
           "2026-09-19", "2025-03-21",
           "Mark acorda do outro lado do andar severado e encontra uma porta que "
           "não existia no mapa do departamento.",
           "premiere", "Apple TV+", 47, 3);
  poeLinha(cache, sizeof cache, "tt7366338", "Andor",
           "deploy/app/art/03.jpg", AG_VOLTANDO, 3, 1, "", "2026-11-04", "",
           "", "premiere", "Disney+", 44, 2);
  poeLinha(cache, sizeof cache, "tt0944947", "Succession",
           "deploy/app/art/04.jpg", AG_VOLTANDO, 0, 0, "", "", "2026-02-11",
           "", "", "HBO", 62, 4);
  poeLinha(cache, sizeof cache, "tt0903747", "Breaking Bad",
           "deploy/app/art/05.jpg", AG_ENCERRADA, 0, 0, "", "", "2013-09-29",
           "", "", "AMC", 47, 5);
  poeLinha(cache, sizeof cache, "tt9999991", "Uma Série Cancelada",
           "deploy/app/art/06.jpg", AG_CANCELADA, 0, 0, "", "", "2023-05-26",
           "", "", "", 0, 1);
  dados_gravar("agenda-p1.txt", cache);
  // Dois lembretes ligados, um deles VENCIDO (estreia hoje) — e o que o cartao
  // de abertura vai mostrar.
  dados_gravar("lembretes-p1.txt",
               "tt10255564\t2026-09-16\t0\n"
               "tt2661044\t2026-09-19\t0\n");
  // RELER O DISCO. agenda_iniciar() so recarrega quando o PERFIL muda — e o
  // que evita uma leitura de arquivo por quadro no aparelho. Aqui os arquivos
  // nasceram depois da primeira leitura, entao a ida e volta ao perfil 2 e o
  // caminho publico para forcar a releitura. A primeira versao desta captura
  // saiu com o cartao fechado exatamente por causa disso.
  perfis_definir_ativo(2); agenda_iniciar();
  perfis_definir_ativo(1); agenda_iniciar();

  // O catalogo entra tambem: a lista de "series que eu sigo" sai de
  // CatItem.naLista, e sem ele so os dois com lembrete apareceriam.
  { CatItem ci[7];
    static const char *ID[7] = { "tt10255564", "tt1520211", "tt2661044",
                                 "tt7366338", "tt0944947", "tt0903747",
                                 "tt9999991" };
    static const char *TIT[7] = { "Foundation", "The Last of Us", "Severance",
                                  "Andor", "Succession", "Breaking Bad",
                                  "Uma Série Cancelada" };
    static const char *GEN[7] = {
      "Programa de TV \xc2\xb7 Drama \xc2\xb7 Ficcao cientifica",
      "Programa de TV \xc2\xb7 Drama \xc2\xb7 Aventura",
      "Programa de TV \xc2\xb7 Drama \xc2\xb7 Misterio",
      "Programa de TV \xc2\xb7 Drama \xc2\xb7 Ficcao cientifica",
      "Programa de TV \xc2\xb7 Drama",
      "Programa de TV \xc2\xb7 Crime \xc2\xb7 Drama",
      "Programa de TV \xc2\xb7 Drama" };
    static const int NOTA[7] = { 84, 81, 0, 79, 0, 96, 0 };
    int i;
    memset(ci, 0, sizeof ci);
    for (i = 0; i < 7; i++) {
      snprintf(ci[i].imdb, sizeof ci[i].imdb, "%s", ID[i]);
      snprintf(ci[i].tipo, sizeof ci[i].tipo, "%s", "series");
      snprintf(ci[i].titulo, sizeof ci[i].titulo, "%s", TIT[i]);
      snprintf(ci[i].poster, sizeof ci[i].poster, "deploy/app/art/0%d.jpg", i);
      // A arte de paisagem da coluna da esquerda (C1) sai do backdrop do
      // catalogo quando o registro nao tem still/backdrop proprio.
      snprintf(ci[i].backdrop, sizeof ci[i].backdrop, "deploy/app/art/0%d.jpg", i);
      snprintf(ci[i].genero, sizeof ci[i].genero, "%s", GEN[i]);
      snprintf(ci[i].meta, sizeof ci[i].meta, "%s", "2026 · 3 temporadas");
      ci[i].nota = NOTA[i];
      ci[i].naLista = 1;
      ci[i].nTemporadas = 3;
    }
    cat_definir_tudo(ci, 7, NULL, 0); }

  // --- 2. O TOPO DO EIXO: a origem "HOJE" e a estreia de hoje em foco -------
  agendaui_iniciar();
  snprintf(nome, sizeof nome, "%s-hoje.bmp", saida);
  captura(nome, w);
  // So a captura comparada ao mockup (iteracao rapida): NUVIO_SHOT_SO=hoje.
  if (getenv("NUVIO_SHOT_SO") && !strcmp(getenv("NUVIO_SHOT_SO"), "hoje")) {
    printf("PASS: captura -hoje gravada.\n");
    return 0;
  }
  if (getenv("NUVIO_SHOT_SO") && !strcmp(getenv("NUVIO_SHOT_SO"), "calendario")) {
    // O FOCO NO SEGMENTADO da ilha (C1): "Mes" focado, "Lista" ainda ativa.
    tecla(SDLK_UP); tecla(SDLK_RIGHT);
    snprintf(nome, sizeof nome, "%s-cabecalho.bmp", saida);
    captura(nome, w);
    tecla(SDLK_RETURN);
    snprintf(nome, sizeof nome, "%s-calendario.bmp", saida);
    captura(nome, w);
    tecla(SDLK_RETURN); tecla(SDLK_RETURN);
    assert(agendaui_menu_aberto());
    tecla(SDLK_ESCAPE);
    assert(!agendaui_menu_aberto());
    tecla(SDLK_LEFT);
    tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_UP);
    // O dia selecionado deve ser preservado ao avançar o mês.
    tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
    snprintf(nome, sizeof nome, "%s-calendario-proximo-mes.bmp", saida);
    captura(nome, w);
    tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_UP);
    tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
    snprintf(nome, sizeof nome, "%s-calendario-lista.bmp", saida);
    captura(nome, w);
    printf("PASS: captura -calendario gravada.\n");
    return 0;
  }

  // --- 2b. A ULTIMA NOTICIA como citacao, o menu de contexto e o painel ------
  // Rede de verdade (Google News): espera ate 8 s pela resposta da primeira
  // linha. Sem rede a foto sai com a sinopse, que e o comportamento certo.
  { int i; for (i = 0; i < 80 && !noticias_respondeu("tt10255564"); i++) SDL_Delay(100); }
  printf("noticias Foundation: %d manchete(s)\n", noticias_n("tt10255564"));
  snprintf(nome, sizeof nome, "%s-noticia.bmp", saida);
  captura(nome, w);
  // SEGURAR continua abrindo o MESMO modal que o toque curto abre agora.
  segurarOk();
  snprintf(nome, sizeof nome, "%s-ctx.bmp", saida);
  captura(nome, w);
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-ctx-noticias.bmp", saida);
  captura(nome, w);
  tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE);

  // --- 3. UMA LINHA DE SINOPSE: The Last of Us --------------------------------
  // O caso que o pedido do dono nomeia. Com a sinopse embaixo, uma frase curta
  // abria os mesmos 86px e sobrava um terco de laje em branco; na coluna da
  // direita ela simplesmente ocupa menos linhas e a altura nao muda.
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-curta.bmp", saida);
  captura(nome, w);

  // --- 4. A VIRADA DE MES: Andor estreia em novembro ------------------------
  { int i; for (i = 0; i < 2; i++) tecla(SDLK_DOWN); }
  snprintf(nome, sizeof nome, "%s-mes.bmp", saida);
  captura(nome, w);

  // --- 5. O FIM DA LISTA: o separador, o eixo tracejado e as sem data -------
  { int i; for (i = 0; i < 3; i++) tecla(SDLK_DOWN); }
  snprintf(nome, sizeof nome, "%s-semdata.bmp", saida);
  captura(nome, w);

  // --- 6. O CABECALHO DE MES SEM LINHAS EMBAIXO -----------------------------
  //
  // Com "hoje" em 30 de agosto e a primeira estreia em 16 de setembro, a faixa
  // de origem escrevia AGOSTO 2026 e o cabecalho de virada escrevia SETEMBRO
  // 2026 logo abaixo, sem nada entre os dois. Era o que estava na captura do
  // dono (SETEMBRO vazio, OUTUBRO com a primeira serie). A foto tem de mostrar
  // UM cabecalho, o do mes que realmente comeca ali.
  agenda_definir_hoje("2026-08-30");
  agenda_montar();
  agendaui_iniciar();
  snprintf(nome, sizeof nome, "%s-mesvazio.bmp", saida);
  captura(nome, w);
  agenda_definir_hoje("2026-09-16");
  agenda_montar();

  // --- 7. REDUZIR ANIMACOES -------------------------------------------------
  // O despertador nao treme e as ondas ficam PARADAS e opacas. A foto existe
  // porque o estado ligado nao pode depender de movimento: quem liga este
  // ajuste continua precisando de saber quais linhas estao marcadas.
  ajustesDeTeste(0, 1);
  agendaui_iniciar();
  snprintf(nome, sizeof nome, "%s-reduzido.bmp", saida);
  captura(nome, w);
  ajustesDeTeste(0, 0);

  // --- 8. O CARTAO DO LEMBRETE VENCIDO -------------------------------------
  oQue = DES_AVISO;
  agendaviso_mostrar_se_houver();
  printf("cartao de lembrete aberto: %d\n", agendaviso_aberto());
  snprintf(nome, sizeof nome, "%s-aviso.bmp", saida);
  captura(nome, w);

  // --- 9. A BARRA LATERAL com o item Agenda em foco ------------------------
  oQue = DES_MENU;
  menu_iniciar();
  menu_abrir();
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-menu.bmp", saida);
  captura(nome, w);

  // --- 10. OS QUATRO ESTADOS DO BOTAO DO LEMBRETE --------------------------
  //
  // O registro de tt10255564 no cache ganha data FUTURA, que e a condicao do
  // botao: com a data de hoje ele apareceria tambem, mas a foto ficaria sem o
  // caso mais comum ("em 3 dias").
  agenda_registrar("tt10255564", "Foundation", "", "Returning Series", 3, 9,
                   "The Last Empress", "2026-09-19", "2026-09-12");
  oQue = DES_DETALHE;
  { HomeItem it;
    memset(&it, 0, sizeof it);
    it.indice = 0;
    it.rect = (GfxRect){ 760.0f, 340.0f, 248.0f, 372.0f };
    it.titulo = "Foundation";
    it.arte = "deploy/app/art/00.jpg";
    detail_abrir(&it); }

  // LIGADO em repouso: o circulo esmeralda. (tt10255564 ja tem lembrete no
  // arquivo de lembretes gravado acima.)
  printf("lembrete de tt10255564: %d\n", agenda_lembrete("tt10255564"));
  snprintf(nome, sizeof nome, "%s-lembrete-ligado.bmp", saida);
  captura(nome, w);

  // LIGADO em foco: superficie clara, relogio escuro-esmeralda, e a linha acima
  // vira a legenda do botao.
  teclaDet(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-lembrete-ligado-foco.bmp", saida);
  captura(nome, w);

  // DESLIGADO em foco: o OK sobre o botao focado alterna e grava.
  teclaDet(SDLK_RETURN);
  printf("lembrete depois do OK: %d\n", agenda_lembrete("tt10255564"));
  snprintf(nome, sizeof nome, "%s-lembrete-desligado-foco.bmp", saida);
  captura(nome, w);

  // DESLIGADO em repouso: o foco volta para o botao primario.
  teclaDet(SDLK_LEFT);
  snprintf(nome, sizeof nome, "%s-lembrete-desligado.bmp", saida);
  captura(nome, w);

  // --- 11. SEM CHAVE DO TMDB --------------------------------------------------
  //
  // Perfil 3, cache VAZIO, catalogo sem nenhuma serie na lista: as cinco linhas
  // saem so dos lembretes (terceira fonte de agenda_montar), que e o caso das
  // series que o dono segue e o catalogo da TV nao publicou. O fio roda de
  // verdade — so o GET e falso.
  agenda_rede_teste(cinemetaFalso);
  { CatItem ci;
    memset(&ci, 0, sizeof ci);
    snprintf(ci.imdb, sizeof ci.imdb, "%s", "tt0000001");
    snprintf(ci.tipo, sizeof ci.tipo, "%s", "movie");
    snprintf(ci.titulo, sizeof ci.titulo, "%s", "Fora da lista");
    cat_definir_tudo(&ci, 1, NULL, 0); }
  perfis_definir_ativo(3);
  dados_gravar("lembretes-p3.txt",
               "tt5550001\t\t0\n" "tt5550002\t\t0\n" "tt5550003\t\t0\n"
               "tt5550004\t\t0\n" "tt5550005\t\t0\n");
  agenda_iniciar();
  oQue = DES_AGENDA;
  agendaui_iniciar();
  esperaFio();
  snprintf(nome, sizeof nome, "%s-semchave.bmp", saida);
  captura(nome, w);
  // O fim da lista: a serie que nenhuma fonte conhece e a sem data.
  { int i; for (i = 0; i < 4; i++) tecla(SDLK_DOWN); }
  snprintf(nome, sizeof nome, "%s-semchave-fim.bmp", saida);
  captura(nome, w);

  // --- 12. CHAVE NO PACOTE, AJUSTE TMDB DESLIGADO --------------------------
  // tmdb_enabled 1 = "Desligado" (V_LIGA). A chave e falsa e nunca e usada:
  // com o ajuste desligado desc_chave_tmdb() devolve "".
  desc_tmdb_definir("0123456789abcdef0123456789abcdef");
  { char caminho[600];
    FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
    f = fopen(caminho, "w");
    if (f) { fprintf(f, "idioma %d\nanimacoes 0\ntmdb_enabled 1\n",
                     getenv("NUVIO_SHOT_EN") && *getenv("NUVIO_SHOT_EN") != '0');
             fonteDeTeste(f); fclose(f); ajustes_dir(dados_dir()); } }
  printf("chave TMDB para o fio: [%s]\n", desc_chave_tmdb()[0] ? "sim" : "nao");
  agendaui_iniciar();
  snprintf(nome, sizeof nome, "%s-tmdbdesligado.bmp", saida);
  captura(nome, w);
  agenda_rede_teste(NULL);

  // --- 13. O MODAL, AS MANCHETES E A NOTICIA, SEM REDE ------------------------
  //
  // Perfil 4, cache e lembretes pelo disco. As tres series tem IMDb proprio
  // (tt777000*) para as manchetes virem do arquivo de fixture e nao da busca
  // de verdade que a captura -noticia ja fez para tt10255564.
  agenda_rede_teste(cinemetaModal);
  noticia_rede_teste(noticiaFalsa);
  perfis_definir_ativo(4);
  idiomaDeTeste(0);
  cache[0] = 0;
  poeLinha(cache, sizeof cache, "tt7770001", "Foundation", "deploy/app/art/00.jpg",
           AG_VOLTANDO, 3, 9, "The Last Empress", "2026-09-16", "2026-09-12",
           "Gaal and Salvor reach Trantor on the day the Empire announces the end of the genetic dynasty.",
           "finale", "Apple TV+", 58, 3);
  poeLinha(cache, sizeof cache, "tt7770002", "Die Schule der magischen Tiere: Weltraumabenteuer",
           "deploy/app/art/03.jpg", AG_VOLTANDO, 2, 7,
           "Donaudampfschifffahrtsgesellschaftskapitänswitwe", "2026-09-18", "2026-09-11",
           "Eine Donaudampfschifffahrtsgesellschaftskapitänswitwe erbt überraschend eine Raumstation.",
           "mid_season", "ZDFneo Fernsehproduktionsgesellschaft", 52, 2);
  poeLinha(cache, sizeof cache, "tt7770003", "Достопримечательности Санкт-Петербурга",
           "deploy/app/art/05.jpg", AG_VOLTANDO, 1, 4, "Высокопревосходительство",
           "2026-09-21", "2026-09-14", "", "premiere", "Кинопоиск", 47, 1);
  dados_gravar("agenda-p4.txt", cache);
  dados_gravar("lembretes-p4.txt", "tt7770001\t2026-09-16\t0\t2026-08-20\n"
                                   "tt7770002\t2026-09-18\t0\t2026-09-01\n");
  { static const char *const PT[] = {
      "Foundation: 2ª temporada da série do Apple TV+ ganha primeiras imagens e novos nomes no elenco",
      "Asimov's daughter on what her father would have thought of Apple's adaptation",
      "Fundação: o que esperar do episódio final da terceira temporada",
      "Apple TV renova Foundation para a quarta temporada",
      "Os bastidores das filmagens em Praga" };
    static const char *const DE[] = {
      "Foundation: Donaudampfschifffahrtsgesellschaftskapitänswitwe erklärt die Staffelfinalvorbereitungen",
      "Asimovs Tochter über die Serienadaption",
      "Foundation: Was das Staffelfinale bringt",
      "Apple verlängert Foundation um eine vierte Staffel",
      "Hinter den Kulissen in Prag" };
    static const char *const RU[] = {
      "«Основание»: высокопревосходительство и достопримечательности финального сезона раскрыты",
      "Дочь Азимова о сериале",
      "«Основание»: чего ждать от финала",
      "Apple продлила «Основание» на четвёртый сезон",
      "Съёмки в Праге" };
    noticiasFixture("tt7770001", "pt", PT, 5);
    noticiasFixture("tt7770001", "de", DE, 5);
    noticiasFixture("tt7770001", "ru", RU, 5);
    noticiasFixture("tt7770002", "de", DE, 5);
    noticiasFixture("tt7770003", "ru", RU, 5); }
  // O MAPA DE VISTOS de tt7770001: T3E5 e T3E6 assistidos, T3E7 nao, e o resto
  // o mapa nao sabe — os tres estados que o modal escreve.
  vistoep_esquecer();
  vistoep_definir("tt7770001", 3, 5, 1);
  vistoep_definir("tt7770001", 3, 6, 1);
  vistoep_definir("tt7770001", 3, 7, 0);
  // A alema: os tres estados com os rotulos alemaes ("Nicht gesehen" e o
  // mais longo da coluna de estado).
  vistoep_definir("tt7770002", 2, 5, 1);
  vistoep_definir("tt7770002", 2, 6, 0);
  { CatItem ci[3];
    static const char *ID[3] = { "tt7770001", "tt7770002", "tt7770003" };
    int i;
    memset(ci, 0, sizeof ci);
    for (i = 0; i < 3; i++) {
      snprintf(ci[i].imdb, sizeof ci[i].imdb, "%s", ID[i]);
      snprintf(ci[i].tipo, sizeof ci[i].tipo, "%s", "series");
      snprintf(ci[i].genero, sizeof ci[i].genero, "%s",
               "Programa de TV \xc2\xb7 Science-Fiction-Abenteuer");
      snprintf(ci[i].meta, sizeof ci[i].meta, "%s", "2026 · 3 temporadas");
      ci[i].nota = 84 - i * 3;
      ci[i].naLista = 1;
      ci[i].nTemporadas = 3;
    }
    snprintf(ci[0].titulo, sizeof ci[0].titulo, "%s", "Foundation");
    snprintf(ci[1].titulo, sizeof ci[1].titulo, "%s", "Die Schule der magischen Tiere: Weltraumabenteuer");
    snprintf(ci[2].titulo, sizeof ci[2].titulo, "%s", "Достопримечательности Санкт-Петербурга");
    snprintf(ci[0].poster, sizeof ci[0].poster, "%s", "deploy/app/art/00.jpg");
    snprintf(ci[1].poster, sizeof ci[1].poster, "%s", "deploy/app/art/03.jpg");
    snprintf(ci[2].poster, sizeof ci[2].poster, "%s", "deploy/app/art/05.jpg");
    cat_definir_tudo(ci, 3, NULL, 0); }
  agenda_iniciar();
  oQue = DES_AGENDA;
  agendaui_iniciar();
  snprintf(nome, sizeof nome, "%s-fx-foco.bmp", saida);
  captura(nome, w);
  // O OK (toque curto) ABRE O MODAL.
  tecla(SDLK_RETURN);
  esperaHistorico("tt7770001");
  snprintf(nome, sizeof nome, "%s-fx-modal.bmp", saida);
  captura(nome, w);
  // Acoes: Assistir T3E7, Abrir o titulo, Ultimas noticias, Marcar 3, Desligar.
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-fx-modal-foco.bmp", saida);
  captura(nome, w);
  tecla(SDLK_RETURN);
  // A manchete em foco pede o trecho depois de 350 ms parada.
  SDL_Delay(420);
  snprintf(nome, sizeof nome, "%s-aquece.bmp", saida);
  captura(nome, w);
  SDL_Delay(100);
  snprintf(nome, sizeof nome, "%s-fx-manchetes.bmp", saida);
  captura(nome, w);
  tecla(SDLK_RETURN);
  SDL_Delay(150);
  snprintf(nome, sizeof nome, "%s-fx-noticia.bmp", saida);
  captura(nome, w);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-fx-noticia-rolada.bmp", saida);
  captura(nome, w);
  tecla(SDLK_ESCAPE);
  tecla(SDLK_DOWN);
  tecla(SDLK_RETURN);
  SDL_Delay(150);
  snprintf(nome, sizeof nome, "%s-fx-noticia-qr.bmp", saida);
  captura(nome, w);
  tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE);
  // Marcar os lancados como assistidos: o historico muda no mesmo quadro.
  tecla(SDLK_DOWN);
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-fx-modal-vistos.bmp", saida);
  captura(nome, w);
  tecla(SDLK_ESCAPE);
  // VIDRO: o mesmo modal na interface de vidro.
  ajustes_definir_vidro(1);
  vistoep_definir("tt7770001", 3, 7, 0);
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-fx-modal-vidro.bmp", saida);
  captura(nome, w);
  tecla(SDLK_ESCAPE);
  ajustes_definir_vidro(0);

  // ALEMAO E RUSSO: o titulo, o episodio, a rede e a citacao longos, e a
  // legenda do sino ("Erinnerung aktiv", "Напоминание включено") na coluna
  // de 120 px. Foco na segunda linha (a alema) e na terceira (a russa).
  idiomaDeTeste(6);
  agendaui_iniciar();
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-fx-de-foco.bmp", saida);
  captura(nome, w);
  tecla(SDLK_RETURN);
  esperaHistorico("tt7770002");
  snprintf(nome, sizeof nome, "%s-fx-de-modal.bmp", saida);
  captura(nome, w);
  tecla(SDLK_ESCAPE);
  idiomaDeTeste(4);
  agendaui_iniciar();
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-fx-ru-foco.bmp", saida);
  captura(nome, w);
  tecla(SDLK_UP); tecla(SDLK_UP);
  snprintf(nome, sizeof nome, "%s-fx-ru-foco1.bmp", saida);
  captura(nome, w);
  tecla(SDLK_RETURN);
  esperaHistorico("tt7770001");
  snprintf(nome, sizeof nome, "%s-fx-ru-modal.bmp", saida);
  captura(nome, w);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  SDL_Delay(420);
  snprintf(nome, sizeof nome, "%s-aquece.bmp", saida);
  captura(nome, w);
  snprintf(nome, sizeof nome, "%s-fx-ru-manchetes.bmp", saida);
  captura(nome, w);
  idiomaDeTeste(0);
  agenda_rede_teste(NULL);
  noticia_rede_teste(NULL);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas da Agenda gravadas.");
  return 0;
}
