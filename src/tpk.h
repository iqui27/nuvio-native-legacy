#ifndef NV_TPK_H
#define NV_TPK_H
// ALVO .tpk DA SAMSUNG (Tizen 6+). Entra em TODA unidade por `-include src/tpk.h`
// (tools/tpk.sh); fora do NV_TPK e vazio.
//
// Numa TV Samsung um app de terceiros nao e processo C: e um app .NET, e so ele
// ganha janela. O host (tizen-tpk/NuvioTpk) abre um GLWindow e, a cada quadro,
// chama nv_tpk_quadro() no fio de desenho do NUI. O C do Nuvio roda o main() de
// sempre num fio proprio, e o contexto EGL do GLWindow passa de um fio para o
// outro a cada quadro (tpk.c). Por isso o SDL daqui e o de video "dummy" (fila
// de eventos, tempo, fios, superficies) e as chamadas de janela/GL do SDL viram
// estas, sem mexer no laco do main.c.
#ifdef NV_TPK
#include <SDL2/SDL.h>

void *tpk_gl_criar(void);
void *tpk_gl_corrente(void);
void  tpk_gl_trocar(void);
int   tpk_gl_intervalo(int intervalo);
void  tpk_tamanho(int *w, int *h);
void  tpk_pedir_saida(int w, int h);
int   tpk_gl_atributo(SDL_GLattr a, int *v);

// A janela continua sendo do SDL (dummy): SDL_GetWindowSize e a fila de
// eventos seguem valendo. Sem SDL_WINDOW_OPENGL, que o dummy recusa.
#define SDL_CreateWindow(t, x, y, w, h, f) SDL_CreateWindow((t), (x), (y), (w), (h), 0)
#define SDL_GL_CreateContext(w)            ((void)(w), (SDL_GLContext)tpk_gl_criar())
#define SDL_GL_DeleteContext(c)            ((void)(c))
#define SDL_GL_GetCurrentContext()         ((SDL_GLContext)tpk_gl_corrente())
#define SDL_GL_SwapWindow(w)               ((void)(w), tpk_gl_trocar())
#define SDL_GL_SetSwapInterval(i)          tpk_gl_intervalo(i)
#define SDL_GL_SetAttribute(a, v)          ((void)(a), (void)(v), 0)
#define SDL_GL_GetAttribute(a, v)          tpk_gl_atributo((a), (v))
#define SDL_GL_GetDrawableSize(w, a, b)    ((void)(w), tpk_tamanho((a), (b)))
#define SDL_GL_GetCurrentWindow()          ((SDL_Window *)NULL)

// NV_TPK40: SO a libnuvio.so do pacote Tizen 4/5 (NuvioTpk40), que tools/tpk.sh
// compila a parte, sem _Thread_local (rede.c) e com DT_HASH, porque o host
// dessas TVs pode carrega-la por um carregador de ELF proprio (Program40.cs).
// A libnuvio.so dos hosts Tizen 6+ NAO tem esta macro e nao muda.
#ifdef NV_TPK40
// Rastro de etapas em data/tpk-etapas.txt (#180): uma linha "begin X" /
// "ok X ..." / "fail X ..." / "note ...", com open/write/close e O_APPEND, sem
// buffer e sem handler de sinal. O host .NET escreve no MESMO arquivo, e no
// arranque seguinte mostra na tela a etapa que ficou sem "ok". Antes de
// nv_tpk_iniciar nao ha caminho: a chamada e ignorada.
void nv_tpk40_etapa(const char *linha);
#endif
#endif

#endif
