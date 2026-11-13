// O BOTAO "DIGITAR PELO CELULAR", o mesmo em todo campo de texto do app.
//
// POR QUE EXISTE (dono, 02/10/2026, depois de usar o painel do QR na chave do
// Seekr): "um botaozinho em todos os lugares de texto, bem bonitinho, do
// celular". O painel fixo ao lado da modal de teclado (agente/celular) so
// existia ali; o Spotlight e a Busca, onde mais se digita, nao tinham nada.
//
// O QUE E:
//   - um disco pequeno com o icone de celular (Lucide "smartphone"), na mesma
//     familia do botao Falar que mora ao lado dele: repouso em vidro escuro com
//     fio claro, FOCO cheio na cor do realce (branco no tema padrao) com o
//     icone escuro. Fica sempre A DIREITA do campo: DIREITA a partir do campo
//     chega nele (passando pelo Falar, onde ha voz);
//   - OK nele abre um CARTAO pequeno por cima, ancorado no botao, com o QR, o
//     endereco curto (sem http://) e "Aponte a camera..." — nada da tela anda;
//   - o texto que o celular envia entra no campo DE ONDE o cartao foi aberto
//     (o dono, CELB_*), e o cartao fecha sozinho.
//
// O SERVIDOR (celular.h) so existe enquanto o cartao esta aberto: abrir o
// campo nao abre porta nenhuma na rede, so o OK no botao. Fechar o cartao
// (Voltar, clique fora, a tela do dono fechando, texto recebido) derruba o
// servidor e invalida o token. As regras de seguranca sao as de celular.h.
//
// SEM SERVIDOR (.wgt, Emscripten): celb_disponivel() = 0 e o botao nao e
// desenhado nem entra na navegacao.
//
// Camada: celb_evento() vem cedo no laco de eventos do app (cartao aberto come
// o teclado todo), celb_desenhar() por ultimo no quadro (app.c).
#ifndef NV_CELBOTAO_H
#define NV_CELBOTAO_H
#include <stddef.h>
#include <SDL2/SDL.h>
#include "gfx.h"
#include "ponteiro.h"

enum { CELB_NENHUM = 0, CELB_SPOT, CELB_BUSCA, CELB_TECLADO, CELB_AMIGO, CELB_N };

int  celb_disponivel(void);
// Desenha o botao em `r` (quadrado) e registra o alvo do ponteiro: passar por
// cima chama focar(a, b) do dono; o clique entrega OK, que o dono trata como
// OK no botao (celb_abrir). `focado` = o foco do dono esta no botao agora.
void celb_botao(int dono, GfxRect r, int focado, PonteiroFn focar, int a, int b, float alpha);
// OK no botao: sobe o servidor e mostra o cartao. `titulo` vai para a pagina
// do celular (passa por i18n). 1 = cartao aberto.
int  celb_abrir(int dono, const char *titulo);
void celb_fechar(void);
// Fecha so se o cartao for desse dono (a tela dele fechou).
void celb_fechar_dono(int dono);
int  celb_aberto(void);
int  celb_dono(void);
// 1 UMA VEZ, quando o texto chegou para esse dono; o cartao ja fechou.
int  celb_pegar(int dono, char *dst, size_t n);
// Com o cartao aberto, todo evento de teclado e dele (Voltar fecha; OK com o
// endereco vencido gera outro). 1 = consumido.
int  celb_evento(const SDL_Event *e);
// Uma vez por quadro (app.c): as molas do botao e do cartao.
void celb_atualizar(float dt);
// O cartao, por cima de tudo. Sem cartao (ou com o QR hospedado), nao faz nada.
void celb_desenhar(void);

// --- HOSPEDADO: o QR dentro da superficie do dono ---
// POR QUE EXISTE (dono, 05/10/2026, modal de teclado): o cartao ancora num
// botao, e a modal de teclado nao tem botao — tem um segmento "Digitar pelo
// celular" numa coluna. O cartao abria num canto da tela, sem ligacao com o
// gesto. Quem ja e uma superficie propria (uma modal) pede o QR hospedado e o
// desenha onde o gesto aconteceu; o Spotlight e a Busca continuam no cartao.
//
// O servidor, o token, o dono e celb_pegar sao os MESMOS: so mudam o desenho e
// o teclado — hospedado, celb_evento so consome o Voltar (fecha o QR, nao a
// tela do dono) e o resto segue para o dono, que continua navegavel.
int   celb_abrir_embutido(int dono, const char *titulo);
// 1 = aberto E hospedado por esse dono.
int   celb_embutido(int dono);
// Altura que o bloco pede numa coluna de largura `w`, com no maximo `hMax`
// disponivel: escolhe o arranjo que cabe (QR + frase ao lado, empilhado, ou so
// QR e endereco). O QR nunca encolhe. 0 se nao esta hospedado por esse dono.
float celb_embutido_altura(int dono, float w, float hMax);
// Desenha o bloco com o canto em (r.x, r.y), largura r.w e ate r.h de altura,
// no sistema de coordenadas de quem chama. Sem fundo nem camada: e conteudo
// do dono.
void  celb_desenhar_em(int dono, GfxRect r, float alpha);
// Compact QR for the friends invitation; uses the same hosted token.
void  celb_desenhar_qr_em(int dono, GfxRect r, float alpha);

// --- testes ---
// Onde o cartao (ou o bloco hospedado) ficou no ultimo desenho (0 se fechado).
GfxRect celb_cartao_rect(void);
#endif
