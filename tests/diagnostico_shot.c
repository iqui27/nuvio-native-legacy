// CAPTURAS DA TELA "Diagnostico e otimizacao", sem rede e sem interacao.
//
// Os estados sao montados a mao em `d` (o .c e incluido) com os NUMEROS DO
// RELATORIO DA C9 de 22/09 (manifestos 2513 ms, catalogos 1402, artes 1886,
// fontes 5446, 35 de 300 MB), para a tela ser julgada com valores do tamanho
// real — os defeitos de sobreposicao so apareciam com eles. Idioma ingles por
// padrao, porque e como o app do dono esta; NUVIO_SHOT_PT=1 troca.
//
// NUVIO_DADOS e uma pasta temporaria (ver o .sh): ajustes.txt e o perfil
// aprovado sao escritos la, nunca nos dados de quem roda.
#include "../src/diagnostico.c"
#include "rail_shot.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

// Janela escondida e desenho num FBO (padrao de explorar_shot): nada aparece
// na tela de quem roda.
static GLuint fbo, fboTex;

static void captura(const char *nome, SDL_Window *win, int tela) {
  int i;
  rail_shot_aplicar();
  for (i = 0; i < 40; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (tela) ajustes_desenhar(SDL_GetTicks());
    else diagnostico_desenhar(SDL_GetTicks());
    rail_shot_desenhar(MENU_AJUSTES);
    if (i == 39) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glFinish();
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    (void)win;
  }
  printf("captura: %s\n", nome);
}

static void resultadoC9(void) {
  atomic_store(&d.estado, 2);
  d.intro = 0;
  d.modo = DIAG_QUALIDADE;
  atomic_store(&d.total, 12 + 15);
  atomic_store(&d.feitos, 12 + 15);
  d.nAddon = 12;
  d.manifestMs = 2513; d.catalogMs = 1402; d.assetsMs = 1886; d.streamMs = 5446;
  d.imagensOk = 4; d.streamOk = 2;
  d.fonte[PTV_FONTE_CATALOGO] = (PtvFonte){ 3, 0, 0, 1260, 2550000, 1920, 1080 };
  d.fonte[PTV_FONTE_METAHUB]  = (PtvFonte){ 3, 0, 0, 1320, 2550000, 1920, 1080 };
  d.fonte[PTV_FONTE_TMDB]     = (PtvFonte){ 3, 0, 2610, 3180, 910000, 1280, 720 };
  d.fonte[PTV_FONTE_TRAKT]    = (PtvFonte){ 2, 1, 2890, 2200, 640000, 1280, 720 };
  d.fonte[PTV_FONTE_LOGO]     = (PtvFonte){ 3, 0, 0, 540, 96000, 800, 310 };
  d.fontesMs = 1260 + 1320 + 2610 + 3180 + 2890 + 2200 + 540;
  d.perfAntes = (PtvPerfil){ 128, 4, 1920 };
  d.perfCand = (PtvPerfil){ 300, 4, 1920 };
  d.medFrio = (PtvMedida){ 3120, 10, 0, 41, 0 };
  d.medAntes = (PtvMedida){ 1886, 10, 0, 33, 0 };
  d.medDepois = (PtvMedida){ 1650, 10, 0, 30, 0 };
  d.aplicacao = DA_MANTIDO;
  d.enviado = 1;
  d.sug = (PtvSugestao){ 1, PTV_FONTE_METAHUB, 1, PTV_FONTE_TMDB, PTV_FONTE_CATALOGO, 1930, 420 };
  d.sugEstado = DS_PROPOSTA;
  d.relatorio[0] = 'x';
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-diag";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  int pt = getenv("NUVIO_SHOT_PT") && *getenv("NUVIO_SHOT_PT") == '1';

  assert(dir && *dir);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  { char caminho[600];
    FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
    f = fopen(caminho, "w");
    assert(f);
    fprintf(f, "idioma %d\nanimacoes 0\n", pt ? 0 : 1);
    fclose(f); }
  ajustes_dir(dir);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: captura do diagnostico", 0, 0, 64, 64,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  // A C9: 2245 MB de RAM nao existem no Mac (sem /proc); o que a tela mostra
  // de perfil sai do tex_cache real desta maquina, os tempos sao os da TV.

  diagnostico_iniciar();
  d.intro = 0;
  snprintf(nome, sizeof nome, "%s-1-inicio.bmp", saida);
  captura(nome, w, 0);

  focoModo = 1;
  snprintf(nome, sizeof nome, "%s-2-inicio-desempenho.bmp", saida);
  captura(nome, w, 0);
  focoModo = 0;

  atomic_store(&d.estado, 1);
  atomic_store(&d.fase, 4);
  atomic_store(&d.total, 27);
  atomic_store(&d.feitos, 27);
  snprintf(nome, sizeof nome, "%s-3-rodando.bmp", saida);
  captura(nome, w, 0);

  resultadoC9();
  snprintf(nome, sizeof nome, "%s-4-resultado.bmp", saida);
  captura(nome, w, 0);

  // O outro desfecho: reteste pior, restaurado sozinho; sugestao desfeita;
  // envio sem recibo, com o botao de reenviar; foco no segundo botao.
  resultadoC9();
  d.medDepois = (PtvMedida){ 2710, 10, 0, 64, 0 };
  d.aplicacao = DA_RESTAURADO_AUTO;
  d.motivo = "Artes ficaram mais lentas depois da mudança";
  d.sugEstado = DS_DESFEITA;
  d.sugMotivo = "Artes ficaram mais lentas depois da mudança";
  d.enviado = 0; d.envioFalhou = 1;
  d.botao = 1;
  snprintf(nome, sizeof nome, "%s-5-restaurado.bmp", saida);
  captura(nome, w, 0);

  // TESTE DE VELOCIDADE. Addons de mentira so para o nome na linha (nenhum
  // pedido sai: o fio nao e criado, o estado e montado a mao).
  d.botao = 0;
  atomic_store(&d.estado, 0);
  focoLinha = 1;
  snprintf(nome, sizeof nome, "%s-7-inicio-velocidade.bmp", saida);
  captura(nome, w, 0);
  focoLinha = 0;
  addons_adicionar("Torrentio", "https://exemplo.invalid/a/manifest.json");
  addons_adicionar("AIOStreams", "https://exemplo.invalid/b/manifest.json");
  addons_adicionar("Comet", "https://exemplo.invalid/c/manifest.json");
  addons_adicionar("MediaFusion", "https://exemplo.invalid/d/manifest.json");
  addons_adicionar("OpenSubtitles v3", "https://exemplo.invalid/e/manifest.json");
  memset(&vz, 0, sizeof vz);
  vz.aberto = 1;
  vz.nAddon = 5;
  vz.addon[0] = (VazAddon){ 1, 812, 200, 1, 34, 3 };
  vz.addon[1] = (VazAddon){ 1, 2410, 200, 1, 112, 3 };
  vz.addon[2] = (VazAddon){ 1, 1333, 200, 1, 18, 2 };
  vz.addon[3] = (VazAddon){ 1, 6004, 0, 0, 0, 0 };
  vz.addon[4] = (VazAddon){ 1, 390, 404, 0, 0, 0 };
  { static const int A[8] = { 41000, 44000, 39000, 22000, 45000, 43000, 40000, 38000 };
    static const int B[8] = { 30000, 29000, 12000, 31000, 33000, 28000, 30000, 27000 };
    memcpy(vz.fonte[0].kbps, A, sizeof A); vz.fonte[0].n = 8;
    memcpy(vz.fonte[1].kbps, B, sizeof B); vz.fonte[1].n = 8;
    vazao_resumir(A, 8, &vz.fonte[0].r);
    vazao_resumir(B, 8, &vz.fonte[1].r);
    memcpy(vz.amostra, A, sizeof A); memcpy(vz.amostra + 8, B, sizeof B);
    vz.nAmostra = 16; }
  atomic_store(&vz.estado, 1);
  atomic_store(&vz.fase, 2);
  atomic_store(&vz.total, 8);
  atomic_store(&vz.feitos, 6);
  atomic_store(&vz.nFonte, 1);
  vz.fonteIniMs = SDL_GetTicks();
  snprintf(nome, sizeof nome, "%s-8-velocidade-rodando.bmp", saida);
  captura(nome, w, 0);

  atomic_store(&vz.nFonte, 2);
  vz.tentadas = 3;
  vazao_resumir(vz.amostra, vz.nAmostra, &vz.resumo);
  vz.resultado = VR_OK;
  atomic_store(&vz.estado, 2);
  snprintf(nome, sizeof nome, "%s-9-velocidade-resultado.bmp", saida);
  captura(nome, w, 0);

  // Samsung com CDN sem CORS: diz que nao mediu, sem numero inventado.
  atomic_store(&vz.nFonte, 0);
  vz.nAmostra = 0;
  memset(&vz.resumo, 0, sizeof vz.resumo);
  vz.resultado = VR_NAVEGADOR;
  snprintf(nome, sizeof nome, "%s-10-velocidade-navegador.bmp", saida);
  captura(nome, w, 0);
  memset(&vz, 0, sizeof vz);

  // Cabecalho de Ajustes com a contagem de categorias.
  ajustes_iniciar();
  snprintf(nome, sizeof nome, "%s-6-ajustes.bmp", saida);
  captura(nome, w, 1);

  diagnostico_encerrar();
  tex_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
