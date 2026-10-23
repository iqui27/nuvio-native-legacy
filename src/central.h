// CENTRAL DE CONTROLE (dono, 06/10: "control centre tipo da Apple TV quando
// segura o CH+"). Ela NASCE DA ILHA DO RELOGIO (dono: "tem que sair da ilha do
// relogio"): a pilula estica ate o painel (ilha_corpo; no player, a plrilha
// cresce da pilula da hora) e volta para ela ao fechar. O painel tem:
//  - o que o app ja sabe e e util ver sem procurar: hora, perfil, internet,
//    memoria de imagens, versao e plataforma, e o video que esta tocando
//    (resolucao, HDR, Atmos — so o que o pipeline disse, nunca palpite);
//  - botoes de atalho para opcoes de Ajustes: OK passa ao proximo valor ali
//    mesmo, gravado como na tela de Ajustes (ajustes_rapido_*);
//  - "Editar atalhos": a lista do catalogo (centrallista.h), OK poe ou tira.
//    Guardado por perfil nesta TV (central-p<N>.txt).
//
// A TECLA. Segurar CH+ abre; o toque curto continua com o dono de antes
// (Salvos, ilha, zap), so que entregue quando a tecla sobe (chsegura.h). Com
// canal na tela (zap, guia) o CH+ nao passa por aqui. Com a central aberta,
// Voltar ou CH+ fecham.
#ifndef NV_CENTRAL_H
#define NV_CENTRAL_H
#include "gfx.h"
#include <SDL2/SDL.h>

void central_abrir(void);
void central_fechar(void);
int  central_aberta(void);
// Aberta: come o teclado todo.
void central_evento(const SDL_Event *e);
// Por quadro, antes do desenho: pede a ilha esticada (fora do player).
void central_atualizar(float dt, Uint32 agora);
// Com o player aberto: o pedido a plrilha, logo antes de plrilha_desenhar.
void central_desenhar(Uint32 agora);

// A TECLA (main.c). `ehCh` = este evento e o CH+ cru (antes do remapeamento
// para Salvos). Devolve 1 quando o evento foi engolido: o toque curto volta
// depois, por central_tecla_quadro, que chama `entregar` com o KEYDOWN e o
// KEYUP originais. `pode` = 0 quando a central nao pode abrir agora (login,
// zap): o CH+ segue direto, sem atraso.
int  central_tecla(const SDL_Event *e, int pode);
void central_tecla_quadro(Uint32 agora, void (*entregar)(SDL_Event *e));
// 1 enquanto um gesto de CH+ esta em curso (tecla embaixo ou esperando).
int  central_tecla_ocupada(void);

// HOLD FEEDBACK (owner: "tem que ter animacao de quando segura o botao para
// abrir o control center"). 0..1 exactly along CHS_SEGURAR_MS; a valid early
// release (or a silence-ended short press) retracts through anim_suave over
// CHS_EMENDA_MS. The clock pill calls the renderer, and the existing island
// growth opens from it at 1. No timer, threshold, or short-press path of its
// own.
float central_segurar_progresso(Uint32 agora);
void central_segurar_desenhar(GfxRect r, float raio, float a, Uint32 agora);

// A previa do cartao de novidades: o painel aberto com os atalhos de fabrica,
// em (x, y), largura CENTRAL_PREVIA_W. `foco` = botao em foco (-1 nenhum);
// `inverte` = botao cujo interruptor aparece trocado (o OK da animacao), -1
// nenhum. A hora, a direita da faixa de cima, e de quem chama.
#define CENTRAL_PREVIA_W 492.0f
float central_previa_altura(void);
void  central_previa_desenhar(float x, float y, float a, int foco, int inverte);

// Largura util do nome num botao (tests/central_rotulos.sh).
float central_rotulo_w(void);

#ifdef CENTRAL_TESTE
// Capturas: foco num botao (-1 = "Editar atalhos") e modo edicao.
void central_teste_foco(int foco, int editar);
// Um "Tocando agora" de mentira (NULL tira) e a lista de atalhos pelo texto do arquivo.
void central_teste_tocando(const char *tit, const char *meta);
void central_teste_lista(const char *texto);
void central_teste_cor(int foco);           // o seletor da cor de destaque aberto
void central_teste_estatisticas(int sim);   // o botao das estatisticas, sem player
#endif

#endif
