// TRAILER DENTRO DO APP. Pedido do dono, 20/09/2026, e o #82 do rawldon:
// "when I open a trailer it takes me into a browser view and I can't get
// back". Dois motores, uma interface:
//
//   Samsung  o app e uma pagina, e o player do YouTube existe como <iframe>
//            embed com a IFrame API — controlavel por postMessage e
//            posicionavel ATRAS do canvas. `fonte` e o id do YouTube.
//   LG       app nativo, sem web view. O trailer e o MP4 do IMDb
//            (trailerimdb.h) no plano de video da TV, o mesmo do player.
//            `fonte` e a URL do MP4.
//
// Nos dois o canvas abre um furo (gfx_furo, o mesmo do player) onde o video
// deve aparecer e o texto do app continua por cima.
//
// DOIS USOS:
//   autoplay   mudo, na area do fundo da pagina de titulo, depois de a
//              pagina assentar (ajuste "Trailer automático");
//   tela cheia pelo botao de trailer — OK pausa/continua, Voltar fecha.
//              Com som so na LG; na Samsung sempre mudo (trailerfonte.h,
//              trailerfonte_com_som). Antes o botao abria o navegador da TV.
//
// DE QUAL FONTE (Apple, IMDb, YouTube) decide trailerfonte.h, pelo ajuste
// "Fonte do trailer"; aqui so se toca o que vier.
//
// No Mac nao ha nem pagina nem pipeline: trailer_suportado() e 0 e o botao
// continua abrindo o navegador (extras_trailer_abrir).
#ifndef NV_TRAILER_H
#define NV_TRAILER_H
#include <SDL2/SDL.h>
#include "gfx.h"

int  trailer_suportado(void);
// Abre (ou reposiciona) o trailer `fonte` no retangulo `r` (coordenadas da
// tela 1920x1080). `som` 0 = mudo (autoplay; na Samsung e forcado a 0 sempre).
// `cheia` marca o modo de tela
// cheia com teclado proprio.
void trailer_abrir(const char *fonte, GfxRect r, int som, int cheia);
void trailer_rect(GfxRect r);
// O proximo trailer_abrir (e os trailer_rect dele) toca DENTRO DE UM CARTAZ da
// home (modo "No card"). So muda algo no .tpk: la o recorte de fonte vira um
// ROI maior que o destino, e no cartaz o ROI tem de ser o proprio retangulo
// (quem chama encaixa o quadro, LetterBox). trailer_fechar zera.
void trailer_no_cartao(int sim);
// O recorte da FONTE (sx,sy,sw,sh) que o plano de video recebe para um
// quadro vw x vh com o zoom `z` do ajuste, num destino dw x dh: o zoom e a
// tarja de sempre e depois o "cover" na proporcao do destino, pelo centro.
// Sem ele o plano esticava a fonte no destino (o banner 1920x528 do Padrao
// achatava o trailer pela metade). Pura conta: os testes chamam direto.
void trailer_recorte(int vw, int vh, float z, float dw, float dh,
                     int *sx, int *sy, int *sw, int *sh);
void trailer_fechar(void);
int  trailer_aberto(void);
// Tests (desktop): fake native trailer. -1 off, 0 open but no image (stall), 1 playing.
void trailer_teste_falso(int tocando);
// Muda a cada fonte de trailer aberta: uma sessao principal anterior perdeu
// o pipeline, mesmo se o trailer ja terminou ou usou a mesma URL.
unsigned trailer_geracao(void);
int  trailer_cheia(void);
// 1 quando ha video de fato tocando: so ai a pagina abre o furo — antes
// disso mostrar um buraco preto seria pior que a arte.
int  trailer_tocando(void);
// #338: 1 enquanto um trailer toca DE VERDADE — e 0 na pausa da tela cheia
// nativa, onde trailer_tocando continua preso em 1 (o medidor de desempenho
// volta; ver trailer.c). O main alimenta desempenho_trailer com isto.
int  trailer_medidor_esconde(void);
// 1 quando o PLANO pode aparecer: quem desenha o furo (fundo, destaque, tela
// cheia) pergunta isto alem de trailer_tocando. No .tpk (#178) o recorte do
// zoom so e pedido ~800 ms depois de tocar; ate ele assentar o quadro inteiro
// com tarja estaria na tela, e a pessoa via "tarja 1 s, depois zoom". Ate la
// fica a arte (ou preto na tela cheia). Seguranca: 2 s depois de tocar mostra
// de qualquer jeito, nunca prende o trailer. Na LG e no .wgt e sempre 1.
int  trailer_mostra_video(void);
// 1 quando o ultimo elemento fechado pelo atualizador terminou por erro.
// Permite ao hero tentar a proxima fonte (Apple -> YouTube) uma unica vez,
// sem confundir fechamento voluntario com falha de rede.
int  trailer_falhou(void);
// Estado cru do elemento, so para LOG: -2 sem elemento, -1 criado, 1 tocando,
// 3 buffering, 0 fim, -3 erro (Samsung). Na LG devolve -9 (nao ha elemento).
int  trailer_estado(void);
// Retangulo atual, para quem desenha o furo.
GfxRect trailer_retangulo(void);
// QUEM ABRIU o trailer aberto, e de qual titulo (imdb). O destaque da home
// marca TRAILER_DONO_HOME logo depois de abrir; a pagina do titulo, quando
// ADOTA esse trailer (trailerfonte.h, NV_TRAILER_CONTINUA_DETALHE), passa
// para TRAILER_DONO_DETALHE — e a home deixa de fechar o que nao e mais dela.
// trailer_fechar zera. Sem marca (qualquer outro chamador) e NENHUM, como
// antes.
enum { TRAILER_DONO_NENHUM = 0, TRAILER_DONO_HOME = 1, TRAILER_DONO_DETALHE = 2 };
void        trailer_marcar_dono(int dono, const char *imdb);
int         trailer_dono(void);
const char *trailer_dono_imdb(void);
// Leva o trailer aberto (fora da tela cheia) para `r` com `som`, SEM trocar
// de fonte nem reabrir o player. 0 quando nao ha o que levar.
int  trailer_continuar(GfxRect r, int som);
// 1 com o trailer aberto e com som (o `som` efetivo do ultimo trailer_abrir).
int  trailer_com_som(void);
// Teclado do modo de tela cheia. 1 quando consumiu.
int  trailer_evento(const SDL_Event *e);
void trailer_atualizar(Uint32 agora);

// O trailer em tela cheia esta pausado (o OSD so aparece entao).
int  trailer_pausado(void);
// O OSD minimo do trailer em tela cheia, por cima do furo: pilula "Trailer ·
// titulo", barra fina, tempo e Continuar. So desenha com o trailer pausado.
void trailer_osd_desenhar(const char *titulo, float a);
#ifdef NV_SHOT_HOOKS
void trailer_shot_pausado(int sim);
#endif

#endif
