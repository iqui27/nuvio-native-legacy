// O AVISO DO LEMBRETE DE PROGRAMA, em qualquer tela (home, detalhe, player,
// guia). A lista mora em lembrete.h; aqui ficam o relogio, o cartao e as
// teclas dele.
//
// CARTAO: "<Programa> comeca agora no <Canal>" com Assistir e Dispensar, no
// canto de cima, por cima de tudo. Some sozinho em GLEM_CARTAO_MS. Enquanto
// esta de pe, ESQUERDA/DIREITA/OK/Voltar sao dele — o resto passa.
// JA VENDO O CANAL (tela cheia, faixa ou preview do guia): so um aviso curto,
// sem botoes e sem pegar tecla nenhuma.
//
// RELOGIO: glem_passo confere a lista no maximo uma vez por segundo, sem
// alocar nada.
#ifndef NV_GUIALEMBRETE_H
#define NV_GUIALEMBRETE_H
#include <SDL2/SDL.h>

#define GLEM_CARTAO_MS 15000u
#define GLEM_CURTO_MS   4500u

void glem_passo(float dt, Uint32 agora);
int  glem_evento(const SDL_Event *e);   // 1 = consumiu
void glem_desenhar(Uint32 agora);
int  glem_cartao_aberto(void);
// Aviso curto de uma linha (marcar/cancelar lembrete no guia usa este).
void glem_aviso_curto(const char *texto);
// "Assistir" no cartao: o app consome e toca o canal (id em `id`).
int  glem_pediu_assistir(char *id, size_t tam, char *nome, size_t tamNome,
                         char *base, size_t tamBase);
// Para a captura (tests/guia_shot): monta o cartao como se o lembrete venceu.
void glem_teste_cartao(const char *titulo, const char *canal, int curto);

#endif
