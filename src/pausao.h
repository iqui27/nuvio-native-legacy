// PAINEL DE PAUSA — o overlay que o app web sobe quando o filme fica parado.
//
// ONDE ELE MORA NO WEB: dentro de playerScreen.js, e nao no arquivo que tem o
// nome dele. Existe um js/ui/screens/player/pauseOverlay.js de 16 linhas que
// cria uma <div> escrita "Paused" e que NINGUEM importa — casca morta de uma
// versao anterior. O recurso de verdade sao os metodos schedulePauseOverlay,
// canShowPauseOverlay, buildPauseOverlayMeta e renderPauseOverlay, e foi deles
// que este modulo saiu.
//
// AS REGRAS, uma a uma, com a linha de origem:
//
//   playerScreen.js:567   PAUSE_OVERLAY_DELAY_MS = 5000. O painel NAO sobe no
//                         instante da pausa: sobe cinco segundos depois. Pausa
//                         curta (levantar, pegar agua) nao merece uma ficha na
//                         cara; pausa longa e alguem que parou para ler.
//   playerScreen.js:568   MAX_PAUSE_OVERLAY_CAST = 8 nomes de elenco.
//   playerScreen.js:7299  canShowPauseOverlay: so com o ajuste ligado, so
//                         pausado, e com NENHUMA outra folha aberta. O painel e
//                         o ultimo da fila — qualquer coisa que o usuario tenha
//                         aberto de proposito vence.
//   playerScreen.js:7381  o relogio de 5s recomeca do zero a cada mudanca de
//                         estado; se ao estourar a condicao ja nao vale, nao
//                         sobe.
//   playerScreen.js:7397  com o painel de pe, qualquer condicao que caia o
//                         derruba na hora.
//   playerScreen.js:7459  buildPauseOverlayMeta() SEM dados: o painel monta com
//                         o que ja se sabe do titulo e sobe assim mesmo. Nunca
//                         existe "carregando" aqui — metade da ficha e melhor
//                         que uma ficha vazia esperando rede.
//   playerScreen.js:22212 OK/Play com o painel de pe: derruba o painel E
//                         retoma. Qualquer outra tecla: derruba o painel,
//                         traz os controles e REARMA os 5s (:22218).
//
// O QUE FICOU DE FORA, e por que:
//
//   - HIDRATACAO POR REDE (hydratePauseOverlayMeta, :7264). O web pede o meta a
//     todos os addons quando o painel vai subir. Aqui o titulo ja veio inteiro
//     no CatItem quando a pagina de detalhe abriu — titulo, genero, meta,
//     sinopse e ate seis nomes de elenco — e extras.c ja falou com o Trakt.
//     Fazer uma consulta nova para reescrever o que esta na memoria seria rede
//     em cima de um app pausado, que e exatamente quando ela nao e precisa.
//     Consequencia aceita: o elenco para em SEIS nomes (CatItem.elenco[6]) e
//     nao em oito. Nao ha oitavo nome guardado em lugar nenhum deste port.
//   - LOGO DO TITULO no lugar do nome (:7466). O CatItem tem `logo`, mas ele e
//     buscado em textura e o painel sobe justamente quando o app esta ocioso —
//     nao vale carregar arte nova para trocar um texto que ja esta certo.
//   - "Are you still watching?" (:7154). E outro recurso que divide a mesma
//     <div> no web (stillWatchingPromptVisible), com contagem regressiva e dois
//     botoes. Nao e o painel de pausa e nao esta portado aqui.
#ifndef NV_PAUSAO_H
#define NV_PAUSAO_H
#include <SDL2/SDL.h>

// O que `pausao_evento` devolve. O modulo nao mexe na reproducao: quem pausa e
// retoma continua sendo o player, senao passariam a existir dois donos do
// estado `tocando`.
enum { PAUSAO_LIVRE = 0,     // nao consumiu: trate a tecla normalmente
       PAUSAO_CONSUMIU = 1,  // o painel caiu e a tecla morre aqui
       PAUSAO_RETOMAR = 2 }; // o painel caiu e o player deve voltar a tocar

// Uma vez por quadro, ANTES do desenho. `podeSubir` e a traducao de
// canShowPauseOverlay (:7299) e quem a monta e o player, que e o unico que sabe
// quais folhas estao abertas. `idx` e o item do catalogo em reproducao, `imdb`
// o id DELE (#190: o desenho confere que o indice ainda e aquele titulo, porque
// o catalogo pode ser trocado entre este quadro e o desenho) e `linhaEp` a
// linha "T1 · E4 · Nome" que o player ja monta (vazia num filme).
void pausao_atualizar(float dt, Uint32 agora, int podeSubir, int idx,
                      const char *imdb, const char *linhaEp);

// 1 do quadro em que o painel aparece ate o quadro em que some por completo.
int  pausao_visivel(void);

// SOMEONE TOUCHED THE CONTROL. Restarts the 5 s clock that makes the panel come
// up, and that is what stops it returning over someone who is still using the
// player. Without it: the panel rises, a key dismisses it, its clock restarts
// from zero, and the panel is back 5 s later - taking away the controls the
// person just brought up.
//
// Call on EVERY key, not only the ones the panel consumes.
void pausao_activity(void);

// So chame com o painel de pe. Ver o enum acima.
int  pausao_evento(const SDL_Event *e);

// O que o player sabe e o painel nao: onde o filme parou e a cor de destaque
// do player (ja com o contraste tratado por corFocoPlayer). `dur` <= 0 tira a
// barra e o "termina as".
typedef struct { float pos, dur; float fr, fg, fb; } PausaoCena;

// O painel e uma CAMADA DE TELA CHEIA (1920x1080 em unidades de layout, seja
// qual for o drawable): veu de ponta a ponta, selo "Pausado" e relogio no alto,
// a ficha ancorada na margem inferior e, embaixo dela, a barra de onde o filme
// parou (trilho de 4 px na margem do conteudo, o tempo na ponta direita). O quadro continua visivel por tras — so escurecido.
//
// HISTORICO: ate a 1.5.2 ele era uma faixa ancorada por `baseY` (o topo do que
// o player ja desenhava), com veu so do topo do texto para baixo e texto em
// 1160 de largura. O dono relatou: "quando o player ta parado, as infos que
// mostra com o overlay nao pegam a tela inteira". Como os controles saem de
// cena enquanto o painel esta de pe, ele nao precisa mais se esquivar deles.
void pausao_desenhar(Uint32 agora, const PausaoCena *cena);

// O selo "Pausado" do alto do painel, para quem mais mostra pausa (o OSD do
// canal ao vivo). `direita` = 1 ancora pela borda direita em `x`. Devolve a
// largura.
#define PAUSAO_SELO_H 56.0f
float pausao_selo(float x, float y, int direita, float a);

// Fim da reproducao: zera o relogio e o painel. Sem isto o proximo filme
// abriria com o cronometro do anterior ja meio andado.
void pausao_fechar(void);
// O item que o painel desenharia agora, ja conferido pelo titulo (-1 = nenhum).
// Para teste (#190).
int  pausao_indice(void);

#endif
