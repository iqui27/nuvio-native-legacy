// CAPTURA DOS ESTADOS NOVOS DA ILHA (mockup aprovado em 02/10, design/ilha):
// avisos com rosto, capa e "duo", frase com destaque, INFO neutro e ACENTO no
// acento, "+N" e o resumo da central, atividade do sync, o cartao do amigo e
// os modais genericos (recomendacao, pedido de amizade, versao nova, Trakt).
//
// NAO ENTRA NA SUITE (*_shot.sh): precisa de janela GL e de olho humano. Cada
// captura e um PNG so da faixa de cima (a altura do quadro do mockup), para
// comparar lado a lado com o mockup renderizado na mesma escala.
//
// FUNDO: NUVIO_SHOT_FUNDOS=<pasta> com bgNN.png (o fundo do quadro NN do
// mockup, 1920x1080, sem a ilha) — assim o vidro fica sobre a MESMA arte. Sem
// a pasta, a arte embarcada 00.jpg. NUVIO_SHOT_VIDRO=0 = solido.
#include "ilha.h"
#include "nvtmp.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *win;
static const char *base;
static GLuint fundoTex;
static int fundoN = -1, direita;

static void fundo(int n) {
  const char *dir = getenv("NUVIO_SHOT_FUNDOS");
  char cam[600];
  SDL_Surface *s, *t;
  if (n == fundoN) return;
  fundoN = n;
  if (fundoTex) { glDeleteTextures(1, &fundoTex); fundoTex = 0; }
  if (dir && *dir) snprintf(cam, sizeof cam, "%s/bg%02d.png", dir, n);
  else snprintf(cam, sizeof cam, "deploy/app/art/00.jpg");
  s = IMG_Load(cam);
  if (!s) return;
  t = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  if (!t) return;
  glGenTextures(1, &fundoTex);
  glBindTexture(GL_TEXTURE_2D, fundoTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL_FreeSurface(t);
}

// Grava a faixa [0, h) da tela em PNG (sem BMP intermediario: disco cheio).
static void captura(const char *nome, int h) {
  unsigned char *pix = malloc(1920 * (size_t)h * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  assert(pix);
  glReadPixels(0, 1080 - h, 1920, h, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, h, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < h; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(h - 1 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.png", base, nome);
  assert(IMG_SavePNG(s, cam) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

static int relogio = 1;
static const char *atividade;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    // O orcamento de rasterizacao de texto e POR QUADRO: sem isto, as linhas
    // novas depois das primeiras centenas voltam vazias (o rotulo do botao).
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(3);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (fundoTex) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundoTex, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    if (atividade) ilha_atividade(atividade, -1.0f);
    ilha_relogio_visivel(relogio);
    // A margem do mockup: pilula em x 96 (esquerda) ou a 64 da borda direita.
    if (direita) ilha_ancorar(1920 - 64, 36, 1); else ilha_ancorar(96, 36, 0);
    ilha_desenhar(agora);
    SDL_GL_SwapWindow(win);
    SDL_Delay(16);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  ilha_evento(&e);
}

static void limpar(void) {
  int q;
  while (ilha_aviso_vez()[0]) ilha_retirar(ilha_aviso_vez());
  for (q = 0; q < ILHA_N_CARTOES; q++) ilha_cartao(q, NULL);
  ilha_modal_fechar(1);
  atividade = NULL;
  direita = 0;
  quadros(45);
}

static char f1[200], f2[200], txt[300];
static IlhaAvisoEx ex(const char *chave, int tipo, const char *icone, unsigned ms) {
  IlhaAvisoEx e;
  memset(&e, 0, sizeof e);
  e.chave = chave; e.tipo = tipo; e.icone = icone; e.texto = txt; e.ms = ms ? ms : 60000;
  return e;
}
// Um aviso, `n` = o numero do quadro do mockup (fundo e nome do arquivo).
static void pilula(int n, const char *nome, IlhaAvisoEx e) {
  char b[80];
  fundo(n);
  limpar();
  ilha_avisar_ex(&e);
  quadros(70);
  snprintf(b, sizeof b, "%02d-%s", n, nome);
  captura(b, n >= 33 ? 250 : 140);
}
static void modal(int n, const char *nome, int h, IlhaAvisoEx e) {
  char b[80];
  fundo(n);
  limpar();
  ilha_avisar_ex(&e);
  quadros(50);
  tecla(SDLK_s);
  quadros(80);
  snprintf(b, sizeof b, "%02d-%s", n, nome);
  captura(b, h);
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  char ajustes[700];
  SDL_GLContext gl;
  FILE *f;
  const char *vidro = getenv("NUVIO_SHOT_VIDRO");
  base = argc > 1 ? argv[1] : nvt("/nuvio-ilha3");
  assert(dir && *dir);
  snprintf(ajustes, sizeof ajustes, "%s/ajustes.txt", dir);
  f = fopen(ajustes, "w"); assert(f);
  // Tema 2 = o azul do mockup (#5aa2ff, o --ac do mockup).
  fprintf(f, "idioma 0\nselected_theme 2\n");
  fclose(f);
  ajustes_dir(dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: ilha3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(!(vidro && vidro[0] == '0'));

  // 01 relogio em repouso (o minuto troca seco: nao ha o que capturar no 02).
  fundo(1); quadros(60); captura("01-relogio", 130);

  // --- 2. avisos curtos -------------------------------------------------------
  { IlhaAvisoEx e = ex("amigo:c", ILHA_OK, NULL, 6000);
    snprintf(txt, sizeof txt, "Você e %s agora são amigos", ilha_forte(f1, sizeof f1, "Carla"));
    e.rostoNome = "Carla";
    pilula(4, "amizade-nova", e); }
  { IlhaAvisoEx e = ex("pedido:b", ILHA_ACENTO, NULL, 10000);
    snprintf(txt, sizeof txt, "%s quer ser seu amigo", ilha_forte(f1, sizeof f1, "Bruno"));
    e.rostoNome = "Bruno"; e.tecla = 1;
    pilula(5, "pedido-amizade", e); }
  { IlhaAvisoEx e = ex("av:rec", ILHA_ACENTO, NULL, 8000);
    snprintf(txt, sizeof txt, "%s recomendou %s", ilha_forte(f1, sizeof f1, "Ana"), ilha_forte(f2, sizeof f2, "Fallout"));
    e.rostoNome = "Ana"; e.capa = "deploy/app/art/poster/00.jpg"; e.tecla = 1; e.grupo = 1;
    pilula(6, "recomendacao", e); }
  { IlhaAvisoEx e = ex("avisos", ILHA_ACENTO, "sino", 20000);
    snprintf(txt, sizeof txt, "3 avisos novos"); e.tecla = 1; e.grupo = 1;
    pilula(7, "central", e); }
  { IlhaAvisoEx e = ex("av:agenda", ILHA_ACENTO, "aj_tv-minimal-play", 8000);
    snprintf(txt, sizeof txt, "Saiu T2E5 de %s", ilha_forte(f1, sizeof f1, "The Bear"));
    e.tecla = 1; e.grupo = 1;
    pilula(8, "episodio-novo", e); }
  { IlhaAvisoEx e = ex("av:agenda2", ILHA_ACENTO, "aj_calendar", 6000);
    snprintf(txt, sizeof txt, "%s estreia hoje", ilha_forte(f1, sizeof f1, "Andor"));
    pilula(9, "estreia-hoje", e); }
  { IlhaAvisoEx e = ex("lembrete-curto", ILHA_ACENTO, "sino", 4500);
    snprintf(txt, sizeof txt, "Lembrete marcado: Jornal Nacional");
    fundo(10); limpar(); direita = 1; ilha_avisar_ex(&e); quadros(70); captura("10-lembrete-guia-direita", 140); direita = 0; }
  { IlhaAvisoEx e = ex("av:update", ILHA_ACENTO, "aj_download", 10000);
    snprintf(txt, sizeof txt, "Versão 1.7.2 chegou"); e.tecla = 1;
    pilula(11, "versao-nova", e); }
  { IlhaAvisoEx e = ex("atualizacao", ILHA_OK, NULL, 7000);
    snprintf(txt, sizeof txt, "Atualizado. Feche e abra o app para usar.");
    pilula(12, "atualizado", e); }
  { IlhaAvisoEx e = ex("rede", ILHA_ERRO, "aj_wifi-off", 0);
    snprintf(txt, sizeof txt, "Sem internet. Mostrando o que já estava salvo.");
    pilula(13, "sem-internet", e); }
  { IlhaAvisoEx e = ex("rede", ILHA_OK, "aj_wifi", 3000);
    snprintf(txt, sizeof txt, "Internet de volta");
    pilula(14, "internet-de-volta", e); }
  { IlhaAvisoEx e = ex("conta-fora", ILHA_INFO, NULL, 9000);
    snprintf(txt, sizeof txt, "Servidor da conta Nuvio fora do ar — usando seus addons salvos");
    pilula(15, "conta-fora", e); }
  { IlhaAvisoEx e = ex("conta-fora", ILHA_OK, "aj_cloud", 4000);
    snprintf(txt, sizeof txt, "A conta voltou. Addons atualizados.");
    pilula(16, "conta-voltou", e); }
  { IlhaAvisoEx e = ex("addon:t", ILHA_ERRO, "aj_puzzle", 6000);
    snprintf(txt, sizeof txt, "O addon %s não respondeu", ilha_forte(f1, sizeof f1, "Torrentio"));
    pilula(17, "addon-fora", e); }
  { IlhaAvisoEx e = ex("debrid-plano", ILHA_ERRO, "aj_triangle-alert", 8000);
    snprintf(txt, sizeof txt, "Seu %s está sem plano. As fontes dele ficam de fora.", ilha_forte(f1, sizeof f1, "TorBox"));
    pilula(18, "debrid-sem-plano", e); }
  { IlhaAvisoEx e = ex("trakt", ILHA_ERRO, "aj_link-2-off", 9000);
    snprintf(txt, sizeof txt, "O Trakt desconectou"); e.tecla = 1;
    pilula(19, "trakt", e); }
  { IlhaAvisoEx e = ex("debrid-baixa", ILHA_INFO, "aj_download", 9000);
    snprintf(txt, sizeof txt, "O TorBox está baixando %s. Volte em alguns minutos.", ilha_forte(f1, sizeof f1, "Duna"));
    pilula(20, "debrid-baixando", e); }
  { IlhaAvisoEx e = ex("sync", ILHA_ERRO, "aj_rotate-cw", 6000);
    snprintf(txt, sizeof txt, "Não deu para sincronizar a conta. Tento de novo sozinho.");
    pilula(21, "sync-falhou", e); }
  { IlhaAvisoEx e = ex("perfil", ILHA_INFO, NULL, 3000);
    snprintf(txt, sizeof txt, "Agora no perfil %s", ilha_forte(f1, sizeof f1, "Lia"));
    e.rostoNome = "Lia";
    pilula(22, "perfil", e); }
  { IlhaAvisoEx e = ex("av:crash", ILHA_ERRO, "aj_triangle-alert", 9000);
    snprintf(txt, sizeof txt, "O app fechou sozinho da última vez"); e.tecla = 1; e.prior = ILHA_P2;
    pilula(23, "queda", e); }
  { IlhaAvisoEx e = ex("av:canal", ILHA_INFO, "aj_megaphone", 10000);
    snprintf(txt, sizeof txt, "Guia de TV lento hoje"); e.tecla = 1;
    pilula(24, "aviso-do-dono", e); }

  // --- 3. atividade -------------------------------------------------------------
  { IlhaAvisoEx e = ex("amigo-vendo", ILHA_INFO, NULL, 6000);
    snprintf(txt, sizeof txt, "%s está vendo %s", ilha_forte(f1, sizeof f1, "Ana"), ilha_forte(f2, sizeof f2, "Severance"));
    e.rostoNome = "Ana"; e.capa = "deploy/app/art/poster/02.jpg"; e.meta = "T2E4"; e.vivo = 1;
    pilula(25, "amigo-assistindo", e); }
  fundo(28); limpar(); atividade = "Sincronizando a conta…"; quadros(70); captura("28-sincronizando", 140); atividade = NULL;

  // --- 4. o cartao do amigo -------------------------------------------------------
  { IlhaCartao c;
    memset(&c, 0, sizeof c);
    snprintf(c.chave, sizeof c.chave, "amigo:ana:tt1:1");
    snprintf(c.titulo, sizeof c.titulo, "Severance");
    snprintf(c.pessoa, sizeof c.pessoa, "Ana");
    snprintf(c.poster, sizeof c.poster, "deploy/app/art/poster/02.jpg");
    c.progresso = -1.0f;
    fundo(32); limpar(); ilha_cartao(ILHA_AMIGO, &c); quadros(80); captura("32-cartao-amigo", 140); }

  // --- 5. modais -------------------------------------------------------------------
  { static IlhaModal m;
    IlhaAvisoEx e = ex("av:rec", ILHA_ACENTO, NULL, 60000);
    memset(&m, 0, sizeof m);
    snprintf(txt, sizeof txt, "%s recomendou %s", ilha_forte(f1, sizeof f1, "Ana"), ilha_forte(f2, sizeof f2, "Severance"));
    e.rostoNome = "Ana"; e.capa = "deploy/app/art/poster/02.jpg";
    snprintf(m.kicker, sizeof m.kicker, "Recomendação de Ana");
    snprintf(m.titulo, sizeof m.titulo, "Severance");
    snprintf(m.linha, sizeof m.linha, "Série · 2022 · IMDb 8,7");
    snprintf(m.fala, sizeof m.fala, "\xe2\x80\x9cVocê vai gostar, confia.\xe2\x80\x9d");
    snprintf(m.estado, sizeof m.estado, "há 12 min");
    snprintf(m.arte, sizeof m.arte, "deploy/app/art/02.jpg");
    snprintf(m.rostoNome, sizeof m.rostoNome, "Ana");
    m.salvos = 1; m.nBotoes = 3;
    snprintf(m.botao[0], 40, "Ver"); snprintf(m.botaoIcone[0], 32, "play");
    snprintf(m.botao[1], 40, "Salvar"); snprintf(m.botaoIcone[1], 32, "aj_bookmark");
    snprintf(m.botao[2], 40, "Dispensar");
    e.modal = &m;
    modal(36, "modal-recomendacao", 500, e); }
  { static IlhaModal m;
    IlhaAvisoEx e = ex("pedido:b", ILHA_ACENTO, NULL, 60000);
    memset(&m, 0, sizeof m);
    snprintf(txt, sizeof txt, "%s quer ser seu amigo", ilha_forte(f1, sizeof f1, "Bruno"));
    e.rostoNome = "Bruno";
    snprintf(m.kicker, sizeof m.kicker, "Pedido de amizade");
    snprintf(m.titulo, sizeof m.titulo, "Bruno");
    snprintf(m.texto, sizeof m.texto, "Terror dos anos 80 e qualquer coisa com o Bill Hader.");
    snprintf(m.chips[0], 32, "Terror"); snprintf(m.chips[1], 32, "Comédia"); snprintf(m.chips[2], 32, "Ficção científica");
    snprintf(m.rostoNome, sizeof m.rostoNome, "Bruno");
    snprintf(m.rodape, sizeof m.rodape, "Recusar não avisa a pessoa.");
    m.nBotoes = 2;
    snprintf(m.botao[0], 40, "Aceitar"); snprintf(m.botaoIcone[0], 32, "check");
    snprintf(m.botao[1], 40, "Recusar"); snprintf(m.botaoIcone[1], 32, "aj_x");
    e.modal = &m;
    modal(37, "modal-pedido", 390, e); }
  { static IlhaModal m;
    IlhaAvisoEx e = ex("av:update", ILHA_ACENTO, "aj_download", 60000);
    memset(&m, 0, sizeof m);
    snprintf(txt, sizeof txt, "Versão 1.7.2 chegou");
    snprintf(m.kicker, sizeof m.kicker, "Versão nova");
    snprintf(m.titulo, sizeof m.titulo, "Nuvio 1.7.2");
    snprintf(m.nota, sizeof m.nota, "Você está na 1.7.1");
    snprintf(m.lista[0], 96, "Legendas ASS mais rápidas na C9");
    snprintf(m.lista[1], 96, "Folha de Fontes com filtro Dublado");
    snprintf(m.lista[2], 96, "Correções no Guia de TV");
    snprintf(m.icone, sizeof m.icone, "aj_download");
    m.nBotoes = 2;
    snprintf(m.botao[0], 40, "Atualizar"); snprintf(m.botaoIcone[0], 32, "aj_download");
    snprintf(m.botao[1], 40, "Depois");
    e.modal = &m;
    modal(38, "modal-versao", 420, e); }
  { static IlhaModal m;
    IlhaAvisoEx e = ex("trakt", ILHA_ERRO, "aj_link-2-off", 60000);
    memset(&m, 0, sizeof m);
    snprintf(txt, sizeof txt, "O Trakt desconectou");
    snprintf(m.kicker, sizeof m.kicker, "Trakt");
    snprintf(m.titulo, sizeof m.titulo, "A sessão do Trakt expirou");
    snprintf(m.texto, sizeof m.texto, "Enquanto isso, o que você assistir não vai para o Trakt. Leva um minuto: um código na TV, o celular confirma.");
    snprintf(m.icone, sizeof m.icone, "aj_link-2-off");
    m.tipo = ILHA_ERRO; m.nBotoes = 2;
    snprintf(m.botao[0], 40, "Reconectar"); snprintf(m.botaoIcone[0], 32, "aj_rotate-cw");
    snprintf(m.botao[1], 40, "Depois");
    e.modal = &m;
    modal(39, "modal-trakt", 360, e); }

  // --- 6. fila: prioridade, "+N" e o resumo da central ---------------------------
  fundo(41); limpar();
  { IlhaAvisoEx e = ex("av:rec", ILHA_ACENTO, NULL, 4000);
    snprintf(txt, sizeof txt, "%s recomendou %s", ilha_forte(f1, sizeof f1, "Ana"), ilha_forte(f2, sizeof f2, "Fallout"));
    e.rostoNome = "Ana"; e.capa = "deploy/app/art/poster/00.jpg"; e.grupo = 1; e.tecla = 1;
    ilha_avisar_ex(&e); }
  { IlhaAvisoEx e = ex("av:update", ILHA_ACENTO, "aj_download", 10000);
    snprintf(txt, sizeof txt, "Versão 1.7.2 chegou"); e.grupo = 1; e.tecla = 1; ilha_avisar_ex(&e); }
  { IlhaAvisoEx e = ex("av:canal", ILHA_INFO, "aj_megaphone", 10000);
    snprintf(txt, sizeof txt, "Guia de TV lento hoje"); e.grupo = 1; e.tecla = 1; ilha_avisar_ex(&e); }
  quadros(70); captura("41a-fila-mais2", 140);
  { IlhaAvisoEx e = ex("trakt", ILHA_ERRO, "aj_link-2-off", 3000);
    snprintf(txt, sizeof txt, "O Trakt desconectou"); e.tecla = 1; ilha_avisar_ex(&e); }
  quadros(70); captura("41b-fila-erro-fura", 140);
  ilha_retirar("trakt");
  quadros(70); captura("41c-fila-central", 140);

  // Solido (Glass UI desligado), o aviso de debrid como no quadro 45.
  ajustes_definir_vidro(0);
  { IlhaAvisoEx e = ex("debrid-plano", ILHA_ERRO, "aj_triangle-alert", 8000);
    snprintf(txt, sizeof txt, "Seu %s está sem plano. As fontes dele ficam de fora.", ilha_forte(f1, sizeof f1, "TorBox"));
    pilula(45, "solido-debrid", e); }
  ajustes_definir_vidro(1);
  // Direita, com o aviso de recomendacao (quadro 43).
  { IlhaAvisoEx e = ex("av:rec", ILHA_ACENTO, NULL, 8000);
    snprintf(txt, sizeof txt, "%s recomendou %s", ilha_forte(f1, sizeof f1, "Ana"), ilha_forte(f2, sizeof f2, "Fallout"));
    e.rostoNome = "Ana"; e.capa = "deploy/app/art/poster/00.jpg"; e.tecla = 1;
    fundo(43); limpar(); direita = 1; ilha_avisar_ex(&e); quadros(70); captura("43-direita-recomendacao", 140); direita = 0; }

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas dos estados novos da ilha gravadas.");
  return 0;
}
