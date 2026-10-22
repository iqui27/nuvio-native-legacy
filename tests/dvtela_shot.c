// A TELA DO DOLBY VISION EM MKV dentro do player de verdade (dvtela.h), com o
// pipeline simulado (video_simular). Precisa de janela GL; fora da suite como
// todo *_shot.
//
//   bash tests/dvtela_shot.sh /tmp/nv-dvtela/dv
//
// Prova, com o player inteiro (player.c, plrilha.c, pausao.c):
//   1. a tela entra no Play de uma fonte candidata e o player da TV fica MUDO;
//   2. NAO ha o cartao "Abrindo fonte" por baixo (a segunda ilha do relato);
//   3. o preroll do caminho (loadCompleted sem playing) NAO sobe o painel de
//      pausa — com a tela e SEM ela (o defeito do "comeca pausado");
//   4. a tela so sai com DV confirmado + tocando, e o mudo e devolvido;
//   5. a recusa sai com a nota de uma linha na ilha;
//   6. "Assistir agora em HDR10" chama video_dv_recusar; Voltar sai do player.
// E salva uma captura 1920x1080 de cada passo.
#include "catalogo.h"
#include "player.h"
#include "pausao.h"
#include "plrilha.h"
#include "dvtela.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include "video.h"
#include "idioma.h"
#include "artehero.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static const int LW = 1920, LH = 1080;
static const char *saida;
static Uint32 relogio = 100000;
static VideoSimulacao V;

static void salvar(const char *passo) {
  char nome[700];
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  int y; unsigned char *p, *t;
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  p = s->pixels; t = malloc((size_t)s->pitch);
  for (y = 0; y < LH / 2; y++) {
    memcpy(t, p + y * s->pitch, (size_t)s->pitch);
    memcpy(p + y * s->pitch, p + (LH - 1 - y) * s->pitch, (size_t)s->pitch);
    memcpy(p + (LH - 1 - y) * s->pitch, t, (size_t)s->pitch);
  }
  free(t);
  snprintf(nome, sizeof nome, "%s-%s.bmp", saida, passo);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  printf("captura: %s\n", nome);
}

// FAIXAS NO FUNDO (dono, C9 OLED, 08/10: "o gradiente do background ta
// daquele jeito"). Mede a faixa ACIMA do cartao (y 24..156, a arte desfocada
// com o veu, sem texto nenhum): o maior trecho horizontal com o MESMO pixel e
// quantos niveis de luminancia distintos ha. Degrade escuro quantizado em 8
// bits sem ruido vira patamares largos (o que o OLED mostra como contorno);
// com o meio degrau de ruido do nv_dither os patamares somem.
static void medirFaixas(int *maiorPatamar, int *niveis) {
  static unsigned char px[1920 * 3];
  static char visto[256];
  int y, x, maior = 0, n = 0;
  memset(visto, 0, sizeof visto);
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  for (y = 24; y < 156; y += 4) {
    int run = 1;
    glReadPixels(0, LH - 1 - y, LW, 1, GL_RGB, GL_UNSIGNED_BYTE, px);
    for (x = 0; x < LW; x++) {
      unsigned char *c = px + x * 3;
      int l = (c[0] * 54 + c[1] * 183 + c[2] * 19) >> 8;
      if (!visto[l]) { visto[l] = 1; n++; }
      if (x && !memcmp(c, c - 3, 3)) { if (++run > maior) maior = run; } else run = 1;
    }
  }
  *maiorPatamar = maior; *niveis = n;
}

// Um quadro do app na ordem de app.c: player, ilha do player e a tela do DV
// por cima de tudo. O fundo e claro de proposito: o que a tela nao cobrir
// aparece.
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    video_simular(&V);
    player_atualizar(1.f / 60, relogio);
    episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, relogio); faixas_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(.62f, .66f, .74f, 1); glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(relogio);
    plrilha_desenhar(relogio);
    dvtela_desenhar(relogio);
  }
}
// O FUNDO PRONTO ANTES DE MEDIR. dvtelaui.c (fundo) so desenha a arte quando
// tex_obter_larg ja decodificou o JPEG (fila assincrona, tex_bombear) E o
// gfx_desfocado ja gerou a copia 96x54 (no maximo NV_DESF_POR_QUADRO por
// quadro); antes disso o fundo e o chapado gfx_cor, uma linha inteira do
// mesmo pixel — o "maior patamar 1920 px" que aparecia de vez em quando. Aqui
// espera os dois (a mesma chave: a copia fica em cache para o desenho) e mais
// alguns quadros para a mola assentar.
static int esperarFundo(void) {
  const char *u = artehero_url(cat_item(0));
  int i;
  for (i = 0; i < 1200; i++) {
    GLuint t = u && u[0] ? tex_obter_larg(u, 480) : 0;
    if (t && gfx_desfocado(t, u)) { quadros(8); return i; }
    quadros(1);
  }
  return -1;
}

// Segundos de relogio sem desenhar cada quadro (o painel de pausa espera 5 s).
static void segundos(int s) { int i; for (i = 0; i < s * 4; i++) { relogio += 234; quadros(1); } }

static void tecla(SDL_Keycode k) {
  SDL_Event ev;
  memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k;
  player_evento(&ev);
}

static void abrirFonte(void) {
  if (player_aberto()) player_encerrar();
  memset(&V, 0, sizeof V);
  V.aceita = 1; V.dvCandidato = 1; V.duracao = 7980.0;
  video_simular(&V);
  player_abrir(0, NULL);
  quadros(4);
  // Sem ".mkv" no nome: a pre-busca do mkvass iria a rede.
  player_definir_fonte("https://fonte.invalid/arquivo?id=1");
}

int main(int argc, char **argv) {
  saida = argc > 1 ? argv[1] : "/tmp/nv-dvtela/dv";
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: dvtela", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  { char caminho[700]; FILE *f;
    const char *en = getenv("NUVIO_SHOT_EN");
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma %d\nselected_theme 2\n", en && *en == '1');
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  if (getenv("NUVIO_SHOT_VIDRO")) ajustes_definir_vidro(1);
  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
    snprintf(c.meta, sizeof c.meta, "2024 · 2h 13min · Suspense");
    snprintf(c.sinopse, sizeof c.sinopse, "Uma restauradora herda um casarao cheio de espelhos.");
    cat_definir(&c, 1); }

  // NUVIO_DVTELA_SO_PAUSA=1: so o passo 10 (o defeito do "comeca pausado",
  // independente da tela), para provar o antes e o depois dele sozinho.
  if (getenv("NUVIO_DVTELA_SO_PAUSA")) goto soPausa;

  // ---- 1. Play numa fonte candidata: a tela desde o primeiro quadro --------
  abrirFonte();
  quadros(40);
  assert(dvtela_ativa());
  assert(video_simulado_dv_tela() == 1);            // o HDR10 do player da TV fica mudo
  assert(plrilha_corpo_alfa() < 0.01f);             // sem "Abrindo fonte" por baixo
  { int q = esperarFundo();
    printf("fundo pronto depois de %d quadros a mais\n", q);
    assert(q >= 0); }
  salvar("1-lendo");
  { int patamar, niveis;
    medirFaixas(&patamar, &niveis);
    printf("faixas do fundo: maior patamar %d px, %d niveis de luminancia\n", patamar, niveis);
    // Sem ruido o patamar passa de 100 px; com o dither fica em poucos px.
    assert(patamar <= 24); }

  // ---- 2. o cabecalho chegou: perfil 8 e a troca do TrueHD -----------------
  V.pronto = 1; V.tocando = 1;                      // o HDR10 tocando, coberto e mudo
  V.dvFase.sondado = 1; V.dvFase.perfil = 8;
  V.dvFase.audioTrocado = 1;
  snprintf(V.dvFase.audioDe, sizeof V.dvFase.audioDe, "A_TRUEHD");
  snprintf(V.dvFase.audioPara, sizeof V.dvFase.audioPara, "A_EAC3");
  quadros(50);
  assert(dvtela_ativa() && dvtela_estado()->passo == DVT_PASSO_AUDIO);
  assert(!pausao_visivel() && plrilha_corpo_alfa() < 0.01f);
  salvar("2-achou");

  // ---- 3. o nosso demux abrindo (a parte lenta) ----------------------------
  V.pronto = 0; V.tocando = 0;                      // o player da TV saiu
  V.dvFase.caminho = 1;
  quadros(50);
  assert(dvtela_ativa() && dvtela_estado()->passo == DVT_PASSO_ABRIR);
  assert(plrilha_corpo_alfa() < 0.01f);             // a SEGUNDA ilha do relato
  salvar("3-abrindo");

  // ---- 4. carregado, sem playing: preroll, nao pausa -----------------------
  V.dvFase.fonteAberta = 1; V.dvFase.carregado = 1;
  V.pronto = 1; V.iniciando = 1;
  segundos(8);
  assert(dvtela_ativa() && dvtela_estado()->passo == DVT_PASSO_IMAGEM);
  assert(!pausao_visivel());
  salvar("4-imagem");

  // ---- 5. 60 s sem nada mudar: a dica calma --------------------------------
  segundos(61);
  assert(dvtela_ativa() && dvtela_estado()->dica);
  quadros(40);
  salvar("5-dica");

  // ---- 6. DV confirmado e tocando: esvai sobre o filme ---------------------
  V.dvFase.dvConfirmado = 1; V.dvFase.tocando = 1;
  V.tocando = 1; V.iniciando = 0; V.dv = 1;
  snprintf(V.hdr, sizeof V.hdr, "DolbyVision");
  quadros(6);
  assert(!dvtela_ativa() && dvtela_visivel());
  salvar("6-saindo");
  quadros(90);
  assert(!dvtela_visivel());
  assert(video_simulado_dv_tela() == 0);
  assert(!pausao_visivel());

  // ---- 7. recusa: perfil 7 com camada de realce -> HDR10 com a nota --------
  abrirFonte();
  quadros(20);
  assert(dvtela_ativa());
  V.pronto = 1; V.tocando = 1;
  V.dvFase.sondado = 1; V.dvFase.perfil = 7; V.dvFase.recusa = VIDEO_DV_NAO_PERFIL;
  quadros(12);
  assert(!dvtela_ativa());
  assert(video_simulado_dv_tela() == 0);            // o som do HDR10 volta
  printf("nota: %s\n", player_shot_toast_texto(relogio));
  assert(player_shot_toast_texto(relogio)[0]);
  quadros(60);
  salvar("7-nota-hdr10");

  // ---- 8. "Assistir agora em HDR10" -----------------------------------------
  abrirFonte();
  quadros(20);
  assert(dvtela_ativa());
  { int antes = video_simulado_dv_recusas();
    tecla(SDLK_RETURN);                               // o OK que sobra do Play e ignorado
    quadros(2);
    assert(dvtela_ativa() && video_simulado_dv_recusas() == antes);
    quadros(60);
    tecla(SDLK_RETURN);
    quadros(2);
    assert(video_simulado_dv_recusas() == antes + 1);
    assert(!dvtela_ativa() && dvtela_estado()->saida == DVT_SAIDA_HDR10); }
  quadros(60);

  // ---- 9. Voltar cancela para a pagina do titulo, como hoje -----------------
  abrirFonte();
  quadros(20);
  assert(dvtela_ativa());
  tecla(SDLK_ESCAPE);
  quadros(2);
  assert(player_quer_sair());
  assert(!dvtela_ativa() && dvtela_estado()->saida == DVT_SAIDA_VOLTAR);

  // ---- 10. SEM a tela: o preroll do caminho nao sobe o painel de pausa ------
  // (uma busca no caminho do DV recarrega o fluxo: loadCompleted, alguns
  // segundos sem playing, e o painel subia como se a pessoa tivesse pausado.)
soPausa:
  if (player_aberto()) player_encerrar();
  memset(&V, 0, sizeof V);
  V.aceita = 1; V.duracao = 7980.0;
  video_simular(&V);
  player_abrir(0, NULL);
  player_definir_fonte("https://fonte.invalid/arquivo?id=2");
  V.pronto = 1; V.tocando = 1; quadros(30);
  assert(!dvtela_ativa());
  V.tocando = 0; V.iniciando = 1;
  segundos(8);
  assert(!pausao_visivel());
  // E a pausa de verdade continua subindo o painel.
  V.iniciando = 0;
  segundos(7);
  assert(pausao_visivel());

  puts("dvtela_shot: ok");
  return 0;
}
