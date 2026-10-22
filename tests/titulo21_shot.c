// FOLHA DE FONTES 2.1 (tela 4 "Titulo e Fontes"): o cartao da escolha
// automatica no topo da lista, as copias do mesmo arquivo de add-ons
// diferentes dobradas numa linha ("N add-ons · mesmo arquivo", #457/#458) e a
// navegacao do D-pad que nunca cai numa linha dobrada.
//
// Confere em 1920x1080: cartao presente na aba "Todos" e ausente na aba de um
// add-on; lista + dobradas = fontes; foco nunca numa copia dobrada; alguma
// copia realmente dobrou. Grava PNGs em <prefixo>-*.png (a suite usa
// tests/out/titulo21). NAO ENTRA NA SUITE de unitarios: janela GL.
//   bash tests/titulo21_shot.sh tests/out/titulo21
#include "catalogo.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include "badges.h"
#include "idioma.h"
#include "vaitocar.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
enum { LW = 1920, LH = 1080 };
static const char *saida;
static unsigned char quadro[LW * LH * 3];
static Uint32 relogio = 100000;

static void salvar(const char *id) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  char nome[700];
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, quadro);
  for (int y = 0; y < LH; y++)
    memcpy((unsigned char *)s->pixels + y * s->pitch, quadro + (size_t)(LH - 1 - y) * LW * 3, (size_t)LW * 3);
  snprintf(nome, sizeof nome, "%s-%s.png", saida, id);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  printf("captura: %s\n", nome);
}

static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
    stream_folha_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(0.025f, 0.025f, 0.03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    gfx_novo_quadro();
    gfx_cor((GfxRect){ 0, 0, LW, LH }, 0, .26f, .17f, .12f, 1);
    gfx_cor((GfxRect){ 0, 0, LW, 360 }, 0, .55f, .36f, .22f, 1);
    stream_folha_desenhar(relogio);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  stream_folha_evento(&e);
  quadros(2);
}

static void fonte(Stream *s, const char *prov, const char *rot, const char *arq, const char *desc, int altura, int mp4, int dv) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rot);
  snprintf(s->descricao, sizeof s->descricao, "%s", desc);
  snprintf(s->arquivo, sizeof s->arquivo, "%s", arq);
  snprintf(s->url, sizeof s->url, "https://exemplo.invalido/%s-%d.%s", prov, altura, mp4 ? "mp4" : "mkv");
  s->altura = altura; s->mp4 = mp4; s->dolbyVision = dv; s->fileIdx = -1;
  s->tamanhoMB = altura >= 2160 ? 11264 : 2048;
  { char t[3200]; snprintf(t, sizeof t, "%s %s", rot, desc); s->badges = badges_detectar(t); }
}

// Aperta a tecla e, a cada uma, confere o foco: nunca numa copia dobrada.
static void andar(SDL_Keycode k, int vezes, int *passos) {
  for (int i = 0; i < vezes; i++) {
    int idx, dob;
    tecla(k);
    stream_shot_foco(&idx, &dob);
    assert(!dob);
    if (idx >= 0) {
      assert(idx < 6);
      if (passos) passos[idx]++;
    }
  }
}

int main(int argc, char **argv) {
  Stream v[6];
  float cartao;
  int linhas, dobradas, idx, dob, vistos[6] = { 0 }, total = 0;
  const char *saida_ = argc > 1 ? argv[1] : "/tmp/nv-titulo21";
  saida = saida_;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: folha de fontes 2.1", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\nvidroLocal 0\nfonteInterface %d\n", TXT_FAMILIA_MONTSERRAT);
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();

  // Tres arquivos, cada um de dois ou tres add-ons: o do 4K em 2 add-ons e o
  // 1080p x265 em 3. O MediaFusion e unico (nome e tamanho proprios).
  fonte(&v[0], "Torrentio", "Torrentio 4k", "Silo.S02E05.2160p.WEB-DL.DV.HDR.Atmos.mkv",
        "Silo.S02E05.2160p.WEB-DL.DV.HDR.Atmos.mkv\n11.0 GB", 2160, 0, 1);
  fonte(&v[1], "AIOStreams | ElfHosted", "Silo S02 E05", "Silo.S02E05.2160p.WEB-DL.DV.HDR.Atmos.mkv",
        "Silo.S02E05.2160p.WEB-DL.DV.HDR.Atmos.mkv\n11.0 GB", 2160, 0, 1);
  fonte(&v[2], "MediaFusion", "1080p WEB-DL", "Silo.2024.S02E05.WEB-DL.1080p.mkv",
        "Silo.2024.S02E05.WEB-DL.1080p.mkv\n4.1 GB", 1080, 0, 0);
  fonte(&v[3], "Torrentio", "Torrentio 1080p", "Silo.S02E05.1080p.x265-ELiTE.mkv",
        "Silo.S02E05.1080p.x265-ELiTE.mkv\n2.0 GB", 1080, 0, 0);
  fonte(&v[4], "Comet", "Comet 1080p", "Silo.S02E05.1080p.x265-ELiTE.mkv",
        "Silo.S02E05.1080p.x265-ELiTE.mkv\n2.0 GB", 1080, 0, 0);
  fonte(&v[5], "AIOStreams | ElfHosted", "Silo 1080p", "Silo.S02E05.1080p.x265-ELiTE.mkv",
        "Silo.S02E05.1080p.x265-ELiTE.mkv\n2.0 GB", 1080, 0, 0);
  for (int i = 0; i < 6; i++) { v[i].tamanhoMB = v[i].altura >= 2160 ? 11264 : (i == 2 ? 4198 : 2048); v[i].tamanhoBytes = 0; }

  stream_definir_alvo("tt14688458:2:5");
  stream_definir_lista(v, 6);
  stream_folha_nome("Silo");
  stream_folha_contexto("T2:E5 · Silo");
  stream_folha_abrir();
  quadros(60);

  // 1. aba "Todos": cartao da escolha + copias dobradas.
  stream_shot_folha(&cartao, &linhas, &dobradas);
  printf("todos: cartao %.0f px, %d linhas, %d dobradas, folha_n %d\n", cartao, linhas, dobradas, stream_folha_n());
  assert(cartao > 40.0f);
  assert(dobradas >= 2);                 // 4K (1 copia) + 1080p x265 (2 copias), menos as pinadas
  assert(linhas + dobradas == stream_folha_n());
  assert(linhas >= 3 && linhas <= 4);
  stream_shot_foco(&idx, &dob);
  assert(!dob);
  salvar("1-todos");

  // 2. D-pad pela lista inteira: nenhum foco numa copia dobrada, e cada linha
  // desenhada (linhas) recebe foco ao menos uma vez.
  stream_shot_foco(&idx, &dob);
  if (idx >= 0) vistos[idx]++;   // a linha em que a folha ja abre
  andar(SDLK_DOWN, 12, vistos);
  for (int i = 0; i < 6; i++) total += vistos[i] > 0;
  printf("foco visitou %d fontes distintas de %d linhas\n", total, linhas);
  assert(total == linhas);
  salvar("2-foco-fundo");
  andar(SDLK_UP, 12, NULL);
  quadros(20);
  salvar("3-foco-topo");

  // 3. aba de add-on: o cartao some (so existe em "Todos").
  for (int i = 0; i < 14; i++) tecla(SDLK_UP);
  tecla(SDLK_DOWN);    // fileira de abas
  tecla(SDLK_RIGHT);   // primeira aba de add-on
  quadros(40);
  stream_shot_folha(&cartao, &linhas, &dobradas);
  printf("aba de add-on: cartao %.0f px, %d linhas, %d dobradas\n", cartao, linhas, dobradas);
  assert(cartao == 0.0f);
  salvar("4-aba-addon");
  // Cartao aberto nao conserva o ranking 4K depois de lembrar o indice 2.
  stream_preferir(2);
  stream_folha_abrir();
  quadros(4);
  { VaiTocar vt; VtEscolha e;
    assert(vt_montar(&vt, "tt14688458", 2, 5) && !strcmp(vt.res, "1080p"));
    assert(vt_escolha(&e) && e.indice == 2 && strstr(e.nome, "1080p")); }

  // Foco na 1080p: copia tardia de addon anterior, sem resolucao conhecida.
  stream_preferir(-1);
  stream_definir_lista(&v[0], 1);
  stream_lista_acrescentar(&v[2], 1, 1);
  fonte(&v[3], "Outro", "720p", "Outro.Arquivo.720p.mkv", "720p", 720, 0, 0);
  stream_lista_acrescentar(&v[3], 1, 2);
  stream_folha_abrir();
  quadros(4);
  tecla(SDLK_DOWN);
  stream_shot_foco(&idx, &dob);
  assert(idx == 1 && !dob);
  v[4] = v[2];
  snprintf(v[4].provedor, sizeof v[4].provedor, "Anterior");
  v[4].altura = 0; v[4].badges = 0;
  stream_lista_acrescentar(&v[4], 1, 0);
  assert(vt_copia_rep(1) == 3);
  quadros(8);
  stream_shot_foco(&idx, &dob);
  assert(idx == 3 && !dob);   // foco transferido explicitamente a copia representante
  tecla(SDLK_RETURN);
  assert(stream_folha_escolheu(&idx) && idx == 3);
  return 0;
}
