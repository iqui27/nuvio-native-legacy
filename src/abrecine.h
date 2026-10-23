// ABERTURA CINEMA (#205, pedido do dono 11/10): o titulo continua na tela do
// Play ate o primeiro quadro, com episodio, etapa e provedor em linhas curtas.
// A arte recua (desfoca e escurece), o logo fica no meio, e o video entra por
// crossfade. A tela Detalhada (o cartao da ilha) segue existindo: e o ajuste
// "Tela de abertura do video" e, na Cinema, Para cima/Para baixo a mostra.
// So a regra mora aqui (sem GL): o desenho e de player.c.
#ifndef NV_ABRECINE_H
#define NV_ABRECINE_H
#include <SDL2/SDL.h>

// Depois disto, UMA linha curta e tranquila aparece sob o logo.
#define ABRECINE_DEMORA_MS 8000u
// Teto da espera pelo quadro real depois de o player dar a fonte por aberta:
// fonte so de audio, ou host que nao anda a posicao, nao ficam na arte.
#define ABRECINE_SEGURA_MS 4000u

typedef struct {
  int viva;        // o quadro anterior estava abrindo
  int detalhada;   // esta abertura mostra o cartao detalhado
  Uint32 desde;    // inicio desta abertura
  Uint32 fim;      // quando o player deu a fonte por aberta
  float veu;       // 1 = a tela cinema cobre; cai a 0 (crossfade) so com imagem real
  float foco;      // 0 = a arte nitida do detalhe, 1 = recuada (desfocada e escura)
  int calado;      // 1 = sem OSD desde a abertura ate a primeira tecla (inclusive DV pausado)
} AbreCine;

// Por quadro. abrindo = o player espera a fonte/o primeiro quadro; imagem = o
// primeiro quadro real ja aparece (no .tpk, o furo so abre com a posicao
// andando); detalhadaPadrao = o ajuste. Uma abertura nova recomeca do zero.
void abrecine_passo(AbreCine *a, int abrindo, int imagem, int detalhadaPadrao,
                    Uint32 agora, float dt, int reduzidas);
// 1 = desenhar a tela cinema neste quadro (e esconder o cartao da ilha).
int abrecine_cinema(const AbreCine *a);
int abrecine_demorando(const AbreCine *a, Uint32 agora);
// O OSD do player fica fora desde a abertura (o filme entra suave, sem controles)
// ate a PESSOA apertar uma tecla com a fonte ja aberta; tecla durante a abertura
// nao conta. 1 = manter os controles escondidos neste quadro.
int abrecine_osd_calado(const AbreCine *a);
void abrecine_acordou(AbreCine *a, int carregando);
// Tecla durante a abertura: Para cima/Para baixo na cinema vira detalhada.
// 1 = consumiu a tecla.
int abrecine_tecla(AbreCine *a, SDL_Keycode k);
// A etapa mostrada sob o trilho: a URL ainda vem do addon, ou ja chegou e o
// video enche o buffer. O desenho usa VideoStats.bufferPct quando informado.
typedef enum { ABRECINE_RESOLVENDO, ABRECINE_CARREGANDO } AbreCineEtapa;
AbreCineEtapa abrecine_etapa(int esperandoFonte);
#endif
