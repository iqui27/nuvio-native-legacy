// A MODAL DE TECLADO: o modelo de foco, as camadas, o celular hospedado, e as
// capturas.
//
// Nasceu com a #88 (o alfabeto de usuario/senha Xtream tem 73 simbolos, a
// grade de 6 colunas cortava depois do "P" e os digitos ficavam inalcancaveis):
// "aparece na tela" e "da para apertar" sao duas afirmacoes, entao o teste
// VARRE a grade com o D-pad e confere cada simbolo. Com as camadas (05/10) a
// varredura passou a valer nos dois sentidos — tudo o que o alfabeto tem sai, e
// NADA do que ele nao tem sai — em cada camada e em cada caixa.
//
// Os alfabetos estao COPIADOS de ajustes.c e login.c (la sao static). Se um dia
// mudarem la, este teste continua valido para o formato; o que ele mede e a
// grade.
#include "teclado.h"
#include "nvtmp.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "sistexto.h"
#include "celular.h"
#include "celbotao.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *XT =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-@!#$%&*+=";
static const char *PORTAL = "abcdefghijklmnopqrstuvwxyz0123456789.:-";
static const char *MAC = "0123456789abcdef:";
static const char *DEB = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.";
static const char *SK = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
static const char *EMAIL = "abcdefghijklmnopqrstuvwxyz0123456789@._-+";
static const char *SENHA =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
  " .,_-@!#$%&*+=?/\\:;'\"()[]{}<>^~`|";

int teclado_teste_camada(int cam, int cx);   // teclado.c, so com AJUSTES_TESTE

static int falhas;
#define CONF(c, o) do { if (c) printf("ok: %s\n", o); else { printf("FALHA: %s\n", o); falhas++; } } while (0)

static SDL_Window *janela;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  // Como app.c: o cartao do celular aberto come a tecla antes da modal.
  if (!celb_evento(&e)) teclado_evento(&e);
}
static void teclas(SDL_Keycode k, int n) { while (n-- > 0) tecla(k); }

static void quadro(void) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(12);
  gfx_novo_quadro();
  teclado_atualizar(1.0f / 60.0f, SDL_GetTicks());
  celb_atualizar(1.0f / 60.0f);
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  // NUVIO_SHOT_FUNDO=1: texto claro de Ajustes por tras, para ver se a modal
  // deixa vazar o que esta atras dela.
  { static int fundo = -1;
    if (fundo < 0) { const char *e = getenv("NUVIO_SHOT_FUNDO"); fundo = e && *e == '1'; }
    if (fundo) {
      int i;
      for (i = 0; i < 14; i++) {
        TxtLinha t = txt_linha(TXT_AJ_TIT28, "When leaving the player  Seekr key  Thumbnail strip", 243, 242, 239, 255);
        txt_desenhar_alpha(t, 220 + (i % 3) * 330, 120 + i * 70, 1.0f);
      }
    } }
  teclado_desenhar(SDL_GetTicks());
  celb_desenhar();
  SDL_GL_SwapWindow(janela);
}

static void captura(const char *saida, const char *sufixo) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  char nome[600];
  int i, y;
  assert(pix);
  for (i = 0; i < 45; i++) quadro();
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4,
           1920 * 4);
  snprintf(nome, sizeof nome, "%s-%s.bmp", saida, sufixo);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

// Fecha sem animacao residual: a proxima abertura parte do zero.
static void fechar(void) {
  int i;
  tecla(SDLK_ESCAPE);
  for (i = 0; i < 60; i++) quadro();
  (void)teclado_resultado();
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-teclado");
  const char *dir = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  int falhou = 0;
  if (!dir || !*dir) return 2;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: revisao do teclado", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, 1920, 1080,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(96);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(dir);
  { char caminho[700];
    const char *li = getenv("NUVIO_SHOT_IDIOMA");   // 0 pt, 1 en, 4 ru, 6 de
    const char *v = getenv("NUVIO_SHOT_VIDRO");
    FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
    f = fopen(caminho, "w");
    assert(f);
    fprintf(f, "idioma %d\nvidroLocal %d\n", li && *li ? atoi(li) : 0, v && *v == '0' ? 1 : 0);
    fclose(f);
    ajustes_dir(dir); }
  ajustes_iniciar();


  // ===========================================================================
  // 1. O MODELO DE FOCO. A coluna do campo fica a ESQUERDA da grade: ESQUERDA
  // na primeira coluna de qualquer fileira entra nela, DIREITA volta para a
  // tecla de onde saiu, e CIMA da primeira fileira nao faz nada. La dentro o
  // campo fica em cima e os segmentos embaixo. teclado_teste_modos finge o que
  // o aparelho tem (teclado da TV, voz, celular); foco: 1 campo, 2 Falar,
  // 3 celular, 4 segmento "Teclado da TV".
  // ===========================================================================
  // LG/Samsung (so o celular), alfabeto padrao.
  teclado_teste_modos(0, 0, 1);
  teclado_abrir_com("Busca", "", 24, NULL, NULL);
  quadro();
  tecla(SDLK_UP);
  CONF(teclado_foco_campo() == 0, "CIMA da primeira fileira nao sai da grade");
  tecla(SDLK_RETURN);
  CONF(!strcmp(teclado_texto(), "a"), "e o foco continua no 'a'");
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 3, "ESQUERDA da coluna 0: o segmento do celular (nao ha campo focavel)");
  tecla(SDLK_LEFT); tecla(SDLK_UP); tecla(SDLK_DOWN);
  CONF(teclado_foco_campo() == 3, "sem vizinho, ESQUERDA/CIMA/BAIXO nao andam");
  tecla(SDLK_RIGHT);
  CONF(teclado_foco_campo() == 0, "DIREITA volta a grade");
  teclas(SDLK_DOWN, 3); tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 3, "ESQUERDA da fileira 3 tambem entra");
  tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  CONF(!strcmp(teclado_texto(), "as"), "e DIREITA volta a MESMA fileira ('s')");
  teclas(SDLK_DOWN, 9);
  CONF(teclado_foco_tipo() == TECLADO_FOCO_ACOES, "BAIXO para na fileira apagar/limpar/concluir");
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 3, "ESQUERDA do 'apagar' entra");
  tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  CONF(teclado_foco_tipo() == TECLADO_FOCO_ACOES && !strcmp(teclado_texto(), "a"), "e volta ao 'apagar'");
  fechar();
  // Nada a esquerda: ESQUERDA nao faz nada.
  teclado_teste_modos(0, 0, 0);
  teclado_abrir_com("Busca", "", 24, NULL, NULL);
  quadro();
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 0, "sem modo nenhum, ESQUERDA nao sai da grade");
  fechar();
  // So a voz.
  teclado_teste_modos(0, 1, 0);
  teclado_abrir_com("Busca", "", 24, NULL, NULL);
  quadro();
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 2, "so voz: ESQUERDA cai no Falar");
  fechar();
  // Android TV com tudo (campo de texto, 6 colunas).
  teclado_teste_modos(1, 1, 1);
  teclado_abrir_com("Portal Stalker (MAC)", "Endereço e porta, sem http://", 48, PORTAL, NULL);
  quadro();
  tecla(SDLK_DOWN); tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 1, "tudo ligado: ESQUERDA cai no campo");
  tecla(SDLK_LEFT); tecla(SDLK_UP);
  CONF(teclado_foco_campo() == 1, "no campo, ESQUERDA e CIMA nao andam");
  teclado_teste_quebra(0, 0, 0);
  tecla(SDLK_DOWN);
  CONF(teclado_foco_campo() == 4, "BAIXO do campo: o primeiro segmento (Teclado da TV)");
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 4, "ESQUERDA no primeiro segmento nao anda");
  tecla(SDLK_RIGHT);
  CONF(teclado_foco_campo() == 2, "DIREITA: Falar");
  tecla(SDLK_RIGHT);
  CONF(teclado_foco_campo() == 3, "DIREITA: celular");
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 2, "ESQUERDA volta ao Falar");
  tecla(SDLK_UP);
  CONF(teclado_foco_campo() == 1, "CIMA do segmentado: o campo");
  tecla(SDLK_DOWN); teclas(SDLK_RIGHT, 2);
  CONF(teclado_foco_campo() == 3, "de volta ao celular");
  tecla(SDLK_RIGHT);
  CONF(teclado_foco_campo() == 0, "DIREITA do ultimo segmento: a grade");
  tecla(SDLK_RETURN);
  CONF(!strcmp(teclado_texto(), "g"), "na fileira 1 de onde saiu ('g')");
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 3, "ESQUERDA de novo: o alvo em que a pessoa estava (celular)");
  // O segmentado quebrado em duas fileiras: [TV, Falar] / [celular].
  teclado_teste_quebra(0, 0, 1);
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 3, "quebrado: o celular e o primeiro da fileira dele");
  tecla(SDLK_UP);
  CONF(teclado_foco_campo() == 4, "CIMA: a fileira de cima do segmentado");
  tecla(SDLK_RIGHT); tecla(SDLK_DOWN);
  CONF(teclado_foco_campo() == 3, "BAIXO do Falar: o celular (a fileira de baixo so tem ele)");
  tecla(SDLK_DOWN);
  CONF(teclado_foco_campo() == 3, "BAIXO da ultima fileira nao anda");
  tecla(SDLK_RIGHT);
  CONF(teclado_foco_campo() == 0, "DIREITA do fim da fileira: a grade");
  // Uma fileira por segmento.
  tecla(SDLK_LEFT);
  teclado_teste_quebra(0, 1, 2);
  tecla(SDLK_UP); 
  CONF(teclado_foco_campo() == 2, "tres fileiras: CIMA do celular e o Falar");
  tecla(SDLK_UP); tecla(SDLK_UP);
  CONF(teclado_foco_campo() == 1, "e depois o Teclado da TV e o campo");
  fechar();
  // Codigo curto (caixas em vez de campo de texto): o campo nao e alvo.
  teclado_abrir("Código do amigo", "Peça o código que aparece na tela dele", 6);
  quadro();
  teclado_teste_quebra(0, 0, 0);
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 4, "caixas: ESQUERDA cai no segmentado, nao num campo sem foco desenhado");
  tecla(SDLK_UP);
  CONF(teclado_foco_campo() == 4, "e CIMA nao sobe para as caixas");
  fechar();
  // So o teclado da TV.
  teclado_teste_modos(1, 0, 0);
  teclado_abrir_com("Portal Stalker (MAC)", "", 48, PORTAL, NULL);
  quadro();
  tecla(SDLK_LEFT); tecla(SDLK_DOWN);
  CONF(teclado_foco_campo() == 4, "so teclado da TV: campo e um segmento");
  tecla(SDLK_RIGHT);
  CONF(teclado_foco_campo() == 0, "DIREITA do unico segmento: a grade");
  fechar();
  // E-MAIL: 13 colunas, fileira de atalhos, sem celular mesmo com ele ligado.
  teclado_teste_modos(1, 1, 1);
  teclado_abrir_com("E-mail", "", 120, EMAIL, NULL);
  teclado_tipo(TECLADO_TIPO_EMAIL);
  quadro();
  teclas(SDLK_RIGHT, 12); tecla(SDLK_RETURN);
  CONF(!strcmp(teclado_texto(), "m"), "e-mail: 13 colunas como antes");
  teclas(SDLK_DOWN, 4);
  CONF(teclado_foco_tipo() == TECLADO_FOCO_MEIO, "e-mail: a fileira de atalhos");
  tecla(SDLK_UP); tecla(SDLK_RETURN);
  CONF(!strcmp(teclado_texto(), "m+"), "e-mail: CIMA dos atalhos prende na ultima tecla da fileira curta");
  tecla(SDLK_DOWN); teclas(SDLK_LEFT, 4);
  CONF(teclado_foco_campo() == 1, "e-mail: ESQUERDA dos atalhos entra no campo");
  // With larger text (escala de texto) TV and Falar wrap into two rows, and
  // RIGHT leaves the bar from TV already: walk right until it does.
  tecla(SDLK_DOWN);
  for (int k = 0; k < 2 && teclado_foco_campo() != 0; k++) tecla(SDLK_RIGHT);
  CONF(teclado_foco_campo() == 0 && teclado_foco_tipo() == TECLADO_FOCO_MEIO, "e-mail: so TV e Falar; DIREITA volta aos atalhos");
  tecla(SDLK_RETURN);
  CONF(!strcmp(teclado_texto(), "m+.com"), "e o atalho .com digita o pedaco inteiro");
  fechar();
  // SENHA: mostrar/ocultar continua na fileira de acoes; sem celular.
  teclado_teste_modos(0, 0, 1);
  teclado_abrir_com("Senha", "", 128, SENHA, "abc");
  teclado_tipo(TECLADO_TIPO_SENHA);
  quadro();
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 0, "senha na LG: sem celular nao ha coluna para entrar");
  teclas(SDLK_DOWN, 9); teclas(SDLK_RIGHT, 2);
  CONF(teclado_mascarado(), "senha nasce mascarada");
  tecla(SDLK_RETURN);
  CONF(!teclado_mascarado(), "mostrar/ocultar na terceira tecla das acoes");
  fechar();
  teclado_teste_modos(-1, -1, -1);
  puts("ok: modelo de foco");

  // ===========================================================================
  // 2. CAMADAS. Alfabeto com as duas caixas: letras (0-9 / a-z + os primeiros
  // sinais) e, se os sinais nao couberam, a camada de sinais. O que se digita
  // e SEMPRE o alfabeto do chamador, nem um caractere a mais.
  // ===========================================================================
  teclado_teste_modos(0, 0, 0);   // ESQUERDA a vontade na varredura
  // Maiusculas: um toque vale UMA letra; dois travam; tres desligam.
  teclado_abrir_com("Chave do Real-Debrid", "", 96, DEB, NULL);
  tecla(SDLK_RETURN);                       // fileira 0 = digitos
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);     // fileira 1 = a-j
  CONF(!strcmp(teclado_texto(), "0a"), "camadas: digitos em cima, letras embaixo");
  teclas(SDLK_DOWN, 2); teclas(SDLK_RIGHT, 2);          // fileira 3, 'w'
  tecla(SDLK_DOWN);
  CONF(teclado_foco_tipo() == TECLADO_FOCO_MEIO, "a fileira das teclas de camada fica embaixo das letras");
  tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  CONF(teclado_caixa() == 1 && teclado_camada() == 0, "sem camada de sinais (3 sinais cabem ao lado do z): so a tecla de maiusculas");
  tecla(SDLK_UP); tecla(SDLK_RETURN); tecla(SDLK_RETURN);
  CONF(!strcmp(teclado_texto(), "0aWw") && teclado_caixa() == 0, "um toque: so a proxima letra, e CIMA volta a mesma tecla");
  tecla(SDLK_DOWN); tecla(SDLK_RETURN); tecla(SDLK_RETURN);
  CONF(teclado_caixa() == 2, "dois toques: travada");
  tecla(SDLK_UP); tecla(SDLK_RETURN); tecla(SDLK_RETURN);
  teclas(SDLK_RIGHT, 6); tecla(SDLK_RETURN);            // '.', sinal rapido ao lado do z
  CONF(!strcmp(teclado_texto(), "0aWwWW.") && teclado_caixa() == 2, "travada continua, e sinal nao tem caixa");
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  CONF(teclado_caixa() == 0, "terceiro toque desliga");
  fechar();
  // Sinais: a tecla troca a camada; os digitos ficam nas duas.
  teclado_abrir_com("Usuário Xtream", "", 64, XT, NULL);
  teclas(SDLK_DOWN, 4); tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  CONF(teclado_camada() == 1, "Xtream (11 sinais): a segunda tecla abre a camada de sinais");
  teclas(SDLK_UP, 9); tecla(SDLK_RETURN);
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  CONF(!strcmp(teclado_texto(), "0."), "sinais: 0-9 em cima, depois os sinais na ordem do chamador");
  teclas(SDLK_DOWN, 9); 
  CONF(teclado_foco_tipo() == TECLADO_FOCO_ACOES, "fileira vazia da camada de sinais e pulada ate as acoes");
  tecla(SDLK_UP); 
  CONF(teclado_foco_tipo() == TECLADO_FOCO_MEIO, "e CIMA volta pelas teclas de camada");
  teclas(SDLK_LEFT, 3); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  CONF(teclado_camada() == 0, "a mesma tecla volta as letras");
  fechar();
  // O texto do sistema continua passando pelo alfabeto.
  st_teste_ligar(1);
  teclado_abrir_com("Chave do Seekr", "", 90, SK, NULL);
  teclado_teste_modos(1, 0, 0);
  tecla(SDLK_LEFT); tecla(SDLK_RETURN);
  st_teste_evento("TAb_c d.é-9");
  quadro();
  CONF(!strcmp(teclado_texto(), "Ab_cd-9"), "teclado da TV: filtrado pelo alfabeto (sem espaco, ponto ou acento)");
  fechar();
  st_teste_ligar(0);
  teclado_teste_modos(0, 0, 0);
  // Varredura: em cada camada e caixa, OK em toda tecla de caractere. Tudo o
  // que saiu esta no alfabeto, e tudo o que esta no alfabeto saiu.
  { static const struct { const char *nome, *alfa; int camadas; } V[] = {
      { "Real-Debrid", NULL, 1 }, { "Seekr/RPDB", NULL, 1 }, { "Xtream", NULL, 1 }, { "senha Nuvio", NULL, 1 },
      { "senha Jellyfin", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-@!#$%&*+=?/ ", 1 },
      { "URL de selos", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/.-_?=&%#+~@!,;", 1 },
      { "token SpatialPosters", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.~=-:/", 1 },
      { "caixas desencontradas", "abcABXYZ1", 1 },
      { "modelo de URL (47, uma caixa)", "abcdefghijklmnopqrstuvwxyz0123456789:/.-_?=&{}%", 0 },
      { "portal (41, uma caixa)", "abcdefghijklmnopqrstuvwxyz0123456789.:-/_", 0 },
      { "guia (com espaco, uma caixa)", "abcdefghijklmnopqrstuvwxyz0123456789 ", 0 },
      { "padrao", NULL, 0 }, { "MAC", NULL, 0 } };
    int v;
    for (v = 0; v < (int)(sizeof V / sizeof V[0]); v++) {
      const char *al = V[v].alfa ? V[v].alfa : v == 0 ? DEB : v == 1 ? SK : v == 2 ? XT : v == 3 ? SENHA
                     : !strcmp(V[v].nome, "MAC") ? MAC : teclado_alfabeto();
      char visto[256] = { 0 };
      const char *p;
      int cam, cx, f, c, ok = 1, temSinais = 0, temCaps = 0;
      for (cam = 0; cam < 2; cam++)
        for (cx = 0; cx <= 2; cx += 2)
          for (f = 0; f < 8; f++) {
            teclado_abrir_com(V[v].nome, "", TECLADO_LONGO, al, NULL);
            // As teclas de camada ja foram exercitadas acima; aqui o estado e
            // posto direto, e a varredura mede a GRADE de cada um.
            if (!teclado_teste_camada(cam, cx)) { fechar(); break; }
            if (cx) temCaps = 1;
            if (cam) temSinais = 1;
            teclas(SDLK_UP, 9);
            teclas(SDLK_DOWN, f);
            if (teclado_foco_tipo() != TECLADO_FOCO_CARACTERE) { fechar(); break; }
            teclas(SDLK_LEFT, 22);
            for (c = 0; c < 22; c++) {
              int antes = (int)strlen(teclado_texto()), k;
              tecla(SDLK_RETURN);
              k = (int)strlen(teclado_texto());
              if (k > antes) visto[(unsigned char)teclado_texto()[k - 1]] = 1;
              tecla(SDLK_RIGHT);
            }
            fechar();
          }
      // O espaco, onde ha camadas, e a ultima tecla da fileira delas.
      if (temCaps && strchr(al, ' ')) {
        teclado_abrir_com(V[v].nome, "", TECLADO_LONGO, al, NULL);
        teclas(SDLK_DOWN, 9); tecla(SDLK_UP); teclas(SDLK_RIGHT, 3); tecla(SDLK_RETURN);
        if (!strcmp(teclado_texto(), " ")) visto[' '] = 1;
        fechar();
      }
      for (p = al; *p; p++)
        if (!visto[(unsigned char)*p]) { printf("FALHA: '%c' inalcancavel em %s\n", *p, V[v].nome); ok = 0; }
      for (c = 1; c < 256; c++)
        if (visto[c] && !strchr(al, c)) { printf("FALHA: '%c' digitado FORA do alfabeto de %s\n", c, V[v].nome); ok = 0; }
      if (temCaps != V[v].camadas) { printf("FALHA: %s %s ter tecla de maiusculas\n", V[v].nome, V[v].camadas ? "devia" : "NAO devia"); ok = 0; }
      if (!ok) falhou = 1;
      else printf("ok: %s — %d simbolos, todos alcancaveis, nenhum fora%s%s\n", V[v].nome, (int)strlen(al),
                  temCaps ? ", maiusculas" : "", temSinais ? ", camada de sinais" : "");
    } }
  teclado_teste_modos(-1, -1, -1);

  // ===========================================================================
  // 3. DIGITAR PELO CELULAR, DENTRO DA MODAL. OK no segmento sobe o servidor e
  // o QR aparece embaixo do segmentado (celbotao hospedado), nao num cartao
  // solto. A grade continua navegavel; Voltar fecha o QR e nao a modal; um
  // POST de verdade (curl) entrega um TEXTO DE TESTE, o foco vai para
  // "concluir" e um OK confirma. Fechar a modal derruba o servidor.
  // ===========================================================================
  { static const char *TESTE = "TESTE_celular-0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN";
    char cmd[512], url[128];
    GfxRect q;
    teclado_abrir_com("Chave do Seekr", "Chave pessoal: seekr.tv. Vazio apaga.", 90, SK, NULL);
    CONF(celular_estado() == CEL_PARADO, "abrir o teclado nao abre porta");
    tecla(SDLK_LEFT);
    CONF(teclado_foco_campo() == 3, "Mac = LG: ESQUERDA cai no segmento do celular");
    tecla(SDLK_RETURN);
    CONF(celb_aberto() && celb_dono() == CELB_TECLADO && celb_embutido(CELB_TECLADO) &&
         celular_estado() == CEL_ESPERANDO, "OK: servidor no ar, QR hospedado na modal");
    snprintf(url, sizeof url, "%s", celular_url());
    { int i; for (i = 0; i < 45; i++) quadro(); }
    q = celb_cartao_rect();
    CONF(q.w > 0 && q.h >= 248.0f + 24.0f, "o QR foi desenhado no tamanho de sempre (248 + moldura)");
    tecla(SDLK_RETURN);
    CONF(!strcmp(url, celular_url()), "OK com o codigo na tela nao troca o token");
    tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
    CONF(teclado_foco_campo() == 0 && !strcmp(teclado_texto(), "0") && celb_aberto(),
         "com o QR aberto a grade continua navegavel");
    tecla(SDLK_LEFT);
    // Voltar fecha o QR e derruba o servidor, sem fechar a modal.
    tecla(SDLK_ESCAPE);
    CONF(!celb_aberto() && celular_estado() == CEL_PARADO && teclado_aberto(), "Voltar fecha o QR, nao a modal");
    tecla(SDLK_RETURN);   // o foco continua no segmento: abre de novo, token novo
    CONF(celb_embutido(CELB_TECLADO) && strcmp(url, celular_url()), "OK de novo: token novo");
    snprintf(url, sizeof url, "%s", celular_url());
    snprintf(cmd, sizeof cmd, "curl -s -m 5 -o /dev/null -w '%%{http_code}\\n' --data-urlencode 't= %s \n' '%s'", TESTE, url);
    assert(system(cmd) == 0);
    { int i; for (i = 0; i < 3; i++) quadro(); }
    CONF(!celb_aberto() && celular_estado() == CEL_PARADO, "texto recebido: servidor fora");
    CONF(!strcmp(teclado_texto(), TESTE) && teclado_aberto(), "o texto chegou inteiro e a modal espera");
    tecla(SDLK_RETURN);   // o foco ja esta em "concluir"
    CONF(!teclado_aberto() && teclado_resultado() == TECLADO_PRONTO, "um OK confirma");
    { int i; for (i = 0; i < 60; i++) quadro(); }
    // Fechar a modal com o QR aberto derruba o servidor (celb_fechar_dono).
    teclado_abrir("Código do amigo", "Peça o código que aparece na tela dele", 6);
    tecla(SDLK_LEFT); tecla(SDLK_RETURN);
    CONF(celb_embutido(CELB_TECLADO), "QR aberto no teclado padrao");
    { SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
      teclado_evento(&e); }   // direto na modal, sem passar pelo celb
    CONF(!teclado_aberto() && !celb_aberto() && celular_estado() == CEL_PARADO, "fechar a modal derruba o servidor");
    { int i; for (i = 0; i < 60; i++) quadro(); }
    (void)teclado_resultado();
    puts("ok: celular hospedado na modal"); }

  // ANDROID (sistexto em modo de teste): OK no campo chama o teclado da TV; o
  // texto do sistema passa pelo alfabeto da modal; Concluir e o "pronto".
  st_teste_ligar(1);
  teclado_abrir_com("Portal Stalker (MAC)", "Endereço e porta, sem http://", 48, PORTAL, NULL);
  quadro();
  tecla(SDLK_LEFT);
  CONF(teclado_foco_campo() == 1, "Android: ESQUERDA cai no campo");
  tecla(SDLK_RETURN);
  CONF(st_dono() == ST_TECLADO && st_estado() == ST_DIGITANDO, "OK no campo: teclado da TV");
  st_teste_evento("TMeu-Portal.tv:8080 ção");
  quadro();
  CONF(!strcmp(teclado_texto(), "meu-portal.tv:8080o"), "texto do sistema filtrado");
  st_teste_evento("Dmeu.tv:80");
  quadro();
  CONF(!teclado_aberto() && teclado_resultado() == TECLADO_PRONTO, "Concluir do sistema e o pronto");
  CONF(!strcmp(teclado_texto(), "meu.tv:80") && st_dono() == ST_DONO_NENHUM, "com o valor final");
  { int i; for (i = 0; i < 60; i++) quadro(); }
  st_teste_ligar(0);

  // ===========================================================================
  // 4. CAPTURAS. "tv-" = Android TV com tudo (teclado da TV, falar, celular),
  // que e onde o dono olhou; "lg-" = so o celular.
  // ===========================================================================
  st_teste_ligar(1);
  teclado_contexto("Fontes e addons · Debrid");
  teclado_abrir_com("Chave do Real-Debrid", "Sua chave em real-debrid.com/apitoken. Vazio apaga.", 96, DEB, "AbC123xyz");
  captura(saida, "chave-letras");
  teclas(SDLK_DOWN, 4); tecla(SDLK_RETURN);
  captura(saida, "chave-caps-um-toque");
  tecla(SDLK_RETURN); tecla(SDLK_UP); teclas(SDLK_RIGHT, 3);
  captura(saida, "chave-caps-travada");
  teclas(SDLK_LEFT, 3); tecla(SDLK_LEFT);
  captura(saida, "chave-foco-campo");
  tecla(SDLK_DOWN); captura(saida, "chave-foco-tv");
  tecla(SDLK_RIGHT); captura(saida, "chave-foco-falar");
  tecla(SDLK_RIGHT); captura(saida, "chave-foco-celular");
  tecla(SDLK_RETURN); captura(saida, "chave-qr");
  tecla(SDLK_RIGHT); captura(saida, "chave-qr-foco-na-grade");
  tecla(SDLK_ESCAPE);
  fechar();
  teclado_abrir_com("Usuário Xtream", "Como o provedor mandou", 48, XT, "joao2024");
  captura(saida, "xtream-letras");
  teclas(SDLK_DOWN, 4); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  captura(saida, "xtream-sinais");
  fechar();
  teclado_abrir("Código do amigo", "Peça o código que aparece na tela dele", 6);
  captura(saida, "padrao");
  tecla(SDLK_LEFT); teclas(SDLK_RIGHT, 2); tecla(SDLK_RETURN);
  captura(saida, "padrao-qr");
  tecla(SDLK_ESCAPE);
  fechar();
  teclado_abrir_com("Portal Stalker (MAC)", "Endereço e porta, sem http://", 48, PORTAL, "meu-portal.tv:8080");
  tecla(SDLK_LEFT);
  captura(saida, "portal-foco-campo");
  fechar();
  st_teste_ligar(0);
  teclado_abrir("Código do amigo", "Peça o código que aparece na tela dele", 6);
  captura(saida, "lg-padrao");
  fechar();
  teclado_abrir_com("MAC do portal", "Formato 00:1a:79:xx:xx:xx", 17, MAC, NULL);
  captura(saida, "lg-mac");
  fechar();
  teclado_contexto("Fontes e addons · Debrid");
  teclado_abrir_com("Chave do Real-Debrid", "Sua chave em real-debrid.com/apitoken. Vazio apaga.", 96, DEB, "AbC123xyz");
  tecla(SDLK_LEFT); tecla(SDLK_RETURN);
  captura(saida, "lg-chave-qr");
  tecla(SDLK_ESCAPE);
  fechar();
  teclado_abrir_com("E-mail", "O e-mail da sua conta Nuvio.", 120, EMAIL, "joao");
  teclado_tipo(TECLADO_TIPO_EMAIL);
  captura(saida, "lg-email");
  fechar();
  teclado_abrir_com("Senha", "A senha da sua conta Nuvio. Ela não aparece na tela.", 128, SENHA, "segredo");
  teclado_tipo(TECLADO_TIPO_SENHA);
  captura(saida, "lg-senha");
  teclas(SDLK_DOWN, 4); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  captura(saida, "lg-senha-sinais");
  fechar();

  if (falhas) { printf("FAIL: %d conferencia(s) falharam.\n", falhas); return 1; }
  if (falhou) { puts("FAIL: capturas gravadas, mas ha simbolo inalcancavel ou fora do alfabeto."); return 1; }
  puts("PASS: capturas do teclado gravadas.");
  return 0;
}
