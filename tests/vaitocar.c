// 2.1, tela 4 "Titulo e Fontes": o TEXTO da faixa "Vai tocar", o nome amigavel,
// a origem das marcas, o cartao da escolha automatica e o agrupamento de copias
// do mesmo arquivo. Sem GL e sem rede: so a lista em memoria e o cache de marcas.
#include "streams.h"
#include "vaitocar.h"
#include "ajustes.h"
#include "badges.h"
#include "intro.h"
#include "fonteregra.h"
#include "fontepref.h"
#include "dados.h"
#include "catalogo.h"
#include "player.h"
#include "video.h"
#include "linguas.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>

static void fonte(Stream *s, const char *prov, const char *rot, const char *arq,
                  const char *desc, int altura, int mp4, unsigned long long mb) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rot);
  snprintf(s->arquivo, sizeof s->arquivo, "%s", arq);
  snprintf(s->descricao, sizeof s->descricao, "%s", desc);
  snprintf(s->url, sizeof s->url, "https://exemplo.invalido/%s-%s", prov, arq);
  s->altura = altura; s->mp4 = mp4; s->fileIdx = -1;
  s->tamanhoMB = (long)mb; s->tamanhoBytes = mb << 20;
  { char t[3200]; snprintf(t, sizeof t, "%s %s %s", rot, arq, desc); s->badges = badges_detectar(t); }
}

static int tem(const char *s, const char *agulha) { return strstr(s, agulha) != NULL; }

static atomic_int redeStatus = 500, redePedidos;
static int semMarcadores(const char *url, char **corpo, int *status) {
  (void)url; *corpo = NULL; *status = atomic_load(&redeStatus);
  if (*status == 200) *corpo = strdup(
    "{\"intro\":[{\"start_ms\":10000,\"end_ms\":80000}],"
    "\"credits\":[{\"start_ms\":3000000,\"end_ms\":3200000}]}");
  atomic_fetch_add(&redePedidos, 1);
  return *status;
}

int main(void) {
  char dir[] = "/tmp/nv-vaitocar-XXXXXX", caminho[600];
  Stream v[6];
  VaiTocar vt;
  VtEscolha e;
  char nome[256];
  FILE *f;
  IntroTrecho tr[2];
  int origem, k, fora = 0, copias = 0;

  assert(mkdtemp(dir));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w"); assert(f);
  fprintf(f, "idioma 0\npularAberturaLocal 0\nfontePrepararLocal 1\nclassifPlayerLocal 0\n");
  fclose(f);
  ajustes_dir(dir);
  ajustes_iniciar();
  badges_carregar("deploy/app/art");

  const char *ARQ = "Silo.S02E05.2160p.WEB-DL.DV.HDR.Atmos.mkv";
  fonte(&v[0], "Torrentio", "Torrentio 4k", ARQ, "Silo 2160p WEB-DL DV HDR Atmos 5.1", 2160, 0, 11264);
  fonte(&v[1], "AIOStreams", "Silo S02 E05", ARQ, "mesmo arquivo, outro add-on", 2160, 0, 11264);   // copia de 0
  fonte(&v[2], "MediaFusion", "1080p WEB-DL", "Silo.2024.S02E05.WEB-DL.1080p.mkv", "WEB-DL 1080p", 1080, 0, 4096);
  fonte(&v[3], "Comet", "Comet 2160p", ARQ, "mesmo nome, outro tamanho", 2160, 0, 9000);          // NAO e copia
  fonte(&v[4], "Comet", "Comet curto", "S02E05.mkv", "nome curto demais", 1080, 0, 2048);
  fonte(&v[5], "Torrentio", "Torrentio curto", "S02E05.mkv", "nome curto demais", 1080, 0, 2048); // NAO: < 16 letras

  stream_definir_alvo("tt14688458:2:5");
  stream_definir_lista(v, 6);

  // --- copias do mesmo arquivo (#457/#458) ---
  assert(vt_copias_mesmas(0, 1) == 1);
  assert(vt_copias_mesmas(0, 3) == 0);   // tamanho diferente
  assert(vt_copias_mesmas(0, 2) == 0);   // outro arquivo
  assert(vt_copias_mesmas(4, 5) == 0);   // nome curto nunca agrupa
  assert(vt_copia_rep(1) == 0 && vt_copia_rep(0) == 0 && vt_copia_rep(3) == 3);
  assert(vt_copias_n(0) == 1 && vt_copias_n(1) == 0);

  // --- descricao da fonte ---
  memset(&vt, 0, sizeof vt);
  vt_descrever(&v[0], &vt);
  assert(vt.tem);
  assert(!strcmp(vt.res, "4K"));
  assert(!strcmp(vt.tipo, "WEB-DL"));
  assert(vt.hdr[0]);
  assert(!strcmp(vt.tamanho, "11,0 GB") || !strcmp(vt.tamanho, "11.0 GB"));
  assert(!strcmp(vt.provedor, "Torrentio"));
  vt_nome_amigavel(&vt, nome, sizeof nome);
  assert(tem(nome, "WEB-DL") && tem(nome, "4K"));
  assert(!tem(nome, ".mkv") && !tem(nome, "Silo."));   // frase amigavel, nao o nome cru

  // Badges combinados preservam video/audio; Atmos nao inventa canais.
  { const char *desc[] = { "DV Atmos 7.1", "DV TrueHD 5.1", "DV EAC3 5.1", "Atmos" };
    const char *audio[] = { "Atmos 7.1", "TrueHD 5.1", "EAC3 5.1", "Atmos" };
    for (int j = 0; j < 4; j++) {
      Stream s;
      fonte(&s, "Teste", "", "", desc[j], 1080, 0, 100);
      s.dolbyAtmos = j == 0 || j == 3;
      memset(&vt, 0, sizeof vt);
      vt_descrever(&s, &vt);
      assert(!strcmp(vt.audio, audio[j]));
      if (j < 3) assert(!strcmp(vt.hdr, "Dolby Vision"));
    }
  }

  // --- origem das marcas ---
  assert(!strcmp(vt_origem_rotulo(VT_O_NENHUMA), ""));
  assert(tem(vt_origem_rotulo(VT_O_THEINTRODB), "TheIntroDB"));
  assert(tem(vt_origem_rotulo(VT_O_ANISKIP), "AniSkip"));
  assert(tem(vt_origem_rotulo(VT_O_CAPITULOS | VT_O_THEINTRODB), "TheIntroDB"));

  // --- marcas lembradas: so o que a rede respondeu; 0 = sem dado ---
  assert(vt_pular_ligado());
  assert(vt_marcas_episodio("tt14688458", 2, 5) == 0);
  memset(tr, 0, sizeof tr);
  tr[0].tipo = INTRO_ABERTURA; tr[0].inicio = 0; tr[0].fim = 90;
  tr[1].tipo = INTRO_CREDITOS; tr[1].inicio = 3000; tr[1].fim = 3300;
  intro_shot_marcas("tt14688458", 2, 5, tr, 2, 0);
  assert(vt_marcas_episodio("tt14688458", 2, 5) == (VT_T_A | VT_T_C));
  assert(vt_marcas_episodio("tt14688458", 2, 6) == 0);   // outro episodio: sem dado
  assert(intro_marcas("tt14688458", 2, 5, &origem) && origem == INTRO_O_THEINTRODB);
  tr[0].ani = 1;
  intro_shot_marcas("tt14688458", 3, 1, tr, 1, 0);
  assert(intro_marcas("tt14688458", 3, 1, &origem) == VT_T_A && origem == VT_O_ANISKIP);

  intro_shot_marcas("tt14688458", 3, 2, tr, 2, 0);
  assert(intro_marcas("tt14688458", 3, 2, &origem) == (VT_T_A | VT_T_C));
  assert(tem(vt_origem_rotulo(origem), "AniSkip") && tem(vt_origem_rotulo(origem), "TheIntroDB"));
  assert(!tem(vt_origem_rotulo(origem), "arquivo"));
  intro_shot_marcas("tt14688458", 3, 2, tr, 1, VT_O_CAPITULOS);
  assert(intro_marcas("tt14688458", 3, 2, &origem));
  assert(!tem(vt_origem_rotulo(origem), "AniSkip"));
  assert(tem(vt_origem_rotulo(origem), "arquivo") && tem(vt_origem_rotulo(origem), "TheIntroDB"));

  // --- faixa montada do que o automatico escolhe ---
  assert(vt_montar(&vt, "tt14688458", 2, 5) == 1);
  assert(vt.tem && vt.pularOn && vt.marcas == (VT_T_A | VT_T_C) && vt.origem == VT_O_THEINTRODB);
  assert(vt_montar(&vt, "tt14688458", 2, 6) == 0);   // lista e do 2:5, nao do 2:6
  assert(vt_montar(&vt, "tt999", 1, 1) == 0 && !vt.tem);                // lista de outro alvo

  // --- cartao da escolha automatica ---
  assert(vt_escolha(&e) == 1 && e.tem);
  assert(e.indice >= 0 && e.indice < 6 && e.nome[0] && e.prov[0]);
  assert(e.nLinha >= 1 && e.nLinha <= VT_CRITERIA_MAX);
  for (k = 0; k < e.nLinha; k++) {
    assert(e.linha[k][0]);
    if (e.fora[k]) fora++;
    if (tem(e.linha[k], "Cópias do mesmo arquivo agrupadas: 1")) copias++;
  }
  assert(fora >= 1 && copias == 1);   // o que ficou de fora vem DEPOIS dos criterios cumpridos
  for (k = 1; k < e.nLinha; k++) assert(!(e.fora[k - 1] && !e.fora[k]));

  // A lembrada no indice 2 vence a 4K tanto na faixa quanto no cartao.
  stream_preferir(2);
  assert(vt_montar(&vt, "tt14688458", 2, 5));
  assert(!strcmp(vt.provedor, "MediaFusion") && !strcmp(vt.res, "1080p"));
  assert(vt_escolha(&e) && e.indice == 2 && tem(e.nome, "1080p"));
  stream_preferir(-1);

  // Reabrir com preferencia persistida e preparo desligado: faixa e Play iguais.
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir) && !ajustes_fonte_preparar());
  assert(fontepref_guardar("tt14688458", &v[2]));
  fontepref_definir_perfil(1);
  fontepref_definir_perfil(0);   // descarrega a tabela: a escolha vem do disco
  stream_definir_lista(v, 6);   // reabrir zera preferida
  assert(stream_preferida() == -1 && stream_automatico() != 2);
  assert(vt_montar(&vt, "tt14688458", 2, 5));
  assert(!strcmp(vt.provedor, "MediaFusion") && !strcmp(vt.res, "1080p"));
  assert(vt_escolha(&e) && e.indice == 2);
  assert(stream_preferida() == -1);   // desenhar nao prepara nem muda a lista
  stream_preferir(fontepref_escolher("tt14688458"));   // caminho do Play
  assert(stream_play_candidata() == e.indice);
  fontepref_esquecer();
  stream_preferir(-1);

  // Primeira da lista e ordem estrita dos add-ons tambem chegam ao texto.
  f = fopen(caminho, "w"); assert(f);
  fprintf(f, "idioma 0\nfonteAutoLocal 1\n"); fclose(f); ajustes_dir(dir);
  stream_definir_lista(&v[2], 1);
  stream_lista_acrescentar(&v[0], 1, 1);
  assert(vt_montar(&vt, "tt14688458", 2, 5) && !strcmp(vt.res, "1080p"));
  assert(vt_escolha(&e) && e.indice == 0);
  f = fopen(caminho, "w"); assert(f);
  fprintf(f, "idioma 0\nfonteAutoLocal 0\nfonteOrdemUsoLocal 2\n"); fclose(f); ajustes_dir(dir);
  { const char *nomes[] = { "MediaFusion", "Torrentio" }; fonteregra_ordem_definir(nomes, 2); }
  assert(vt_montar(&vt, "tt14688458", 2, 5) && !strcmp(vt.res, "1080p"));
  assert(vt_escolha(&e) && e.indice == 0);

  // Fallback na mesma sessao: MKV falha e o player real recebe um MP4.
  // A rede indisponivel nao pode apagar a regressao com uma resposta tardia.
  {
    CatItem c = {0}; CatEp ep = {.temporada = 2, .episodio = 5};
    VideoSimulacao sim = {0};
    IntroTrecho cap = {30, 90, INTRO_ABERTURA, 0}, trechos[8];
    assert(SDL_Init(SDL_INIT_TIMER) == 0);
    ling_local_legenda("none");
    intro_definir_buscador(semMarcadores);
    strcpy(c.imdb, "tt14688458"); strcpy(c.tipo, "series");
    c.temporada = 2; c.episodio = 5;
    cat_definir(&c, 1); cat_definir_episodios(0, &ep, 1);
    video_simular(&sim);   // pipeline recusa o MKV
    player_abrir(0, NULL);
    player_definir_fonte("https://exemplo.invalido/fallback.mkv");
    intro_definir_capitulos(&cap, 1);
    assert(intro_trechos(trechos, 8) == 1 && trechos[0].inicio == 30);
    assert(intro_marcas(c.imdb, 2, 5, &origem) == (VT_T_A | VT_T_C));
    assert(origem == (VT_O_CAPITULOS | VT_O_THEINTRODB));
    player_voltar_a_esperar();
    sim.aceita = 1; video_simular(&sim);
    player_definir_fonte("https://exemplo.invalido/fallback.mp4");
    assert(intro_trechos(trechos, 8) == 0);
    assert(vt_montar(&vt, c.imdb, 2, 5));
    assert(vt.marcas == VT_T_C && vt.origem == VT_O_THEINTRODB);
    intro_desligar();
    puts("ok  fallback MKV->MP4 descarta capitulos e preserva marcas da rede");

    // Resposta valida na sessao: trocar a fonte com 500 mantem o pulo real.
    atomic_store(&redeStatus, 200);
    intro_pedir(c.imdb, 2, 5);
    for (int i = 0; i < 300 && intro_trechos(trechos, 8) < 2; i++) usleep(10000);
    assert(intro_trechos(trechos, 8) == 2);
    intro_definir_duracao(3300, 0);
    usleep(200000);   // consulta com duracao termina antes da troca
    atomic_store(&redeStatus, 500);
    for (int comCap = 0; comCap < 2; comCap++) {
      double fim; int tipo;
      if (comCap) {
        IntroTrecho caps[] = {cap, {100, 120, INTRO_RESUMO, 0}};
        intro_definir_capitulos(caps, 2);
        assert(intro_trechos(trechos, 8) == 3);
        assert(intro_ativo(35, &fim, &tipo) && fim == cap.fim);
      }
      int antes = atomic_load(&redePedidos);
      player_voltar_a_esperar();
      player_definir_fonte("https://exemplo.invalido/rede.mp4");
      for (int i = 0; i < 300 && atomic_load(&redePedidos) == antes; i++) usleep(10000);
      assert(atomic_load(&redePedidos) > antes);
      usleep(200000);   // deixa a resposta 500 chegar ao estado do intro
      assert(intro_trechos(trechos, 8) == 2);
      assert(trechos[0].inicio == 10 && trechos[0].fim == 80);
      assert(intro_botao(35, 1000 + comCap, 1, 0, &fim, &tipo));
      assert(tipo == INTRO_ABERTURA && fim == 80);
      assert(intro_creditos_limitados(trechos, 8) == 1);
      assert(trechos[0].inicio == 3000 && trechos[0].fim == 3200);
      assert(intro_auto_ativo(3100, &fim, &tipo) && tipo == INTRO_CREDITOS && fim == 3200);
      assert(vt_montar(&vt, c.imdb, 2, 5));
      assert(vt.marcas == (VT_T_A | VT_T_C) && vt.origem == VT_O_THEINTRODB);
    }
    intro_desligar();
    puts("ok  troca com rede 500 preserva abertura e creditos do TheIntroDB");
  }

  printf("ok vaitocar (escolha: %s, %d linhas, %d fora)\n", e.nome, e.nLinha, fora);
  return 0;
}
