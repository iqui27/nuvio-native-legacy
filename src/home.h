#ifndef NV_HOME_H
#define NV_HOME_H
#include <SDL2/SDL.h>
#include "gfx.h"
#include "catalogo.h"

// Tipos de fileira presentes na home moderna do Nuvio 1.0.1 legacy.
typedef enum {
  FILEIRA_CONTINUE,   // card 16:9 com barra de progresso
  FILEIRA_NORMAL,      // poster retrato 2:3
  FILEIRA_DESTAQUE,    // seleção editorial: arte landscape panorâmica 16:9
  FILEIRA_DESTAQUE_QUADRADO, // seleção editorial: arte maior em 4:3
  FILEIRA_TOP10,       // ranking usa poster retrato no legacy
  FILEIRA_COLECAO,     // premiações / coleções: landscape intermediário
  FILEIRA_SERVICO,     // catálogo por serviço: landscape compacto
  FILEIRA_SOCIAL,      // atividade dos amigos: editorial largo com autoria
  FILEIRA_RETORNO,     // sessão recém-interrompida: faixa compacta de retomar
  FILEIRA_CATALOGOS,   // atalhos para catálogos existentes, não títulos
  // SO NO LAYOUT DINAMICA (ajustes_home_layout): formas que a Apple TV usa e a
  // home Moderna nao tem. Nascem em dinAtribuirTipos, nunca do catalogo.
  FILEIRA_TOP10_NUM,   // ranking: numeral grande ao lado de cada cartaz
  FILEIRA_LARGA,       // cartao deitado 16:9 maior que o da Moderna
  // Escolhiveis em qualquer layout (06/10):
  FILEIRA_LINHA_TEMPO, // cartazes por ano sobre a linha animada dos anos
  FILEIRA_FICHA,       // cartaz com a ficha do titulo ao lado
  FILEIRA_PALCO,       // o foco grande no centro, vizinhos diminuindo
  FILEIRA_REDONDA,     // avatar circular com o nome embaixo (pessoas)
  // "Herói" (#205): UM cartão do tamanho da fileira, com fundo, selo,
  // título e duas linhas — o banner entre fileiras do Apple TV.
  FILEIRA_HEROI,
  // 14/10: o pôster 3:4 grande da Apple TV, com logo, gênero, classificação
  // e sinopse na base, sobre o véu da COR PREDOMINANTE do cartaz.
  FILEIRA_POSTER_GRANDE,
  // 16/10: o painel deitado do foco a esquerda (arte, logo e ficha) e a fila
  // de cartazes a direita; o painel segue o foco da fileira.
  FILEIRA_HEROI_DIVIDIDO,
  FILEIRA_POSTER       // Explicit portrait row style, independent of global landscape.
} TipoFileira;

// O item sob o foco, com o retangulo que ele ocupa na tela NESTE quadro. A
// transicao para o detalhe parte dai: o card nao "abre uma tela nova", ele voa
// ate virar o hero — e sem o rect de origem real a animacao teria que chutar
// de onde veio, que e exatamente o que faz uma transicao parecer barata.
typedef struct {
  // Indice NO CATALOGO do card focado. Faltava, e por isso o detalhe abria
  // sempre o item 0: a tela de destino nao tinha como saber o que fora aberto.
  int indice;
  GfxRect rect;
  const char *arte;
  const char *titulo;
  const char *genero;
  const char *meta;
} HomeItem;

int  home_iniciar(const char *dirArte);
int  home_cartao_foco_por_cima(int indice); // repinta o cartao focado (menu do cartaz)
int  home_item_focado(HomeItem *out);      // 0 se o foco ainda nao foi desenhado
// Retangulo (tela real) do cartao focado de qualquer fileira, pilha e pasta
// inclusas. 0 sem cartao focado neste quadro (destaque, fileira vazia).
int  home_ancora_foco(GfxRect *out);
int  home_n_artes(void);                   // acervo de backdrops, usado pelo detalhe
// Ha fileira montada? Serve para o app saber que o catalogo CHEGOU depois do
// arranque, e nao so que existia na hora de abrir.
int  home_tem_fileiras(void);
const char *home_arte(int i);
const char *home_backdrop(int i);   // arte do titulo i do catalogo
void home_evento(const SDL_Event *e);
void home_atualizar(float dt, Uint32 agora);
// Palco: o veu da tela inteira com a luz no titulo em foco; por cima de tudo
// da home (barra inclusive), antes dos menus e da pagina do titulo.
void home_holofote_desenhar(Uint32 agora);
// Poe a home no topo (destaque, fileira 0, sem rolagem) e esquece a posicao
// lembrada da sessao. Usado ao entrar num perfil novo.
void home_ir_topo(void);
const char *home_perf_tag(void);
void home_perf_style_menu(void);
// Nova sessao do perfil confirmado: esquece somente estado transitorio da UI.
// Catalogo, contas e preferencias sao carregados pelos seus proprios donos.
void home_abrir_sessao(void);
// Trailer no destaque: `topo` = 1 quando a home e o que esta na frente (sem
// detalhe, player, painel, menu ou cartao por cima). app.c chama por quadro.
void home_trailer_passo(int topo, float dt, Uint32 agora);
#ifdef NV_SHOT_HOOKS
// Capturas: finge um trailer ja tocando onde o passo real o abriria (destaque,
// Tela cheia ou No card), sem plano de video. tests/homelayouts_shot.c.
void home_shot_trailer(int sim);
#endif
// Why `topo` is 0 ("profile-choice", "detail"...), or NULL. Only for the
// "[home-trailer] autoplay gate=top-overlay (...)" line; call before the step.
void home_trailer_topo_motivo(const char *motivo);
void home_desenhar(Uint32 agora);
// Fileiras ainda visiveis atras da copia parada do painel (salvospainel.c).
void home_fileiras_vistas(Uint32 agora);

// Onde a ARTE do hero foi desenhada no ultimo quadro. A tela de detalhe usa
// isto para nascer com o backdrop no mesmo lugar — o fundo do detalhe E a arte
// do titulo em foco, e ela nao deve reaparecer, deve continuar.
void home_hero_rect(float *x, float *y, float *w, float *h);
// A LINHA DE META do destaque do topo: episodio em andamento, genero, ano e
// duracao — cada um no seu ajuste, sem buraco (o que some fecha o espaco).
// Separada do desenho para o teste da combinacao dos ajustes (tests/herometa.c).
void hero_meta_linha(const CatItem *ci, char *dst, size_t n);
// 1 com a pagina no topo (layout Dinamica), caindo a 0 ao rolar; 1 nos outros.
float home_topo_fracao(void);
// Layout Dinamica: as pastas (indices de col_folder) da fileira de colecao
// "Streaming", que sai da home e vai para a barra aberta. 0 sem a fileira ou
// nos outros layouts.
int home_streaming_barra(const int **pastas);
// A home continua se montando mas NAO esta na tela (escolha de perfil): o
// destaque nao gira nem pre-busca enquanto estiver escondida.
void home_oculta(int oculta);
void home_painel_aberto(int aberto);   // painel de Salvos/Social na frente: o carrossel nao gira
void home_encerrar(void);
// Registra o titulo interrompido para a faixa contextual "Retomar agora".
// A faixa so existe enquanto o progresso fizer sentido (nem inicio nem fim).
void home_registrar_retorno(int indice, double posSeg, double durSeg);
// O criterio da faixa, sozinho: entre 1% e o Percentual assistido. A ilha
// (ilhacart.c) usa o mesmo, para a atividade ao vivo durar o mesmo que ela.
int  home_proximo_desfocar(const CatItem *ci, const char *arte);   // #232
int  home_retorno_vale(int indice, double posSeg, double durSeg);
// O titulo da faixa "Retomar agora" (relogio desligado); "" sem sessao. Ver cwretido.h.
const char *home_retomar_imdb(void);
// Dispensar o cartao "Retomar agora" (menu do cartao) e esquece-lo na troca de conta/perfil.
void home_retomar_dispensar(void);
void home_retomar_esquecer(void);
int  home_quer_sair(void);
int  home_pediu_abrir(void);   // OK pressionado: consome o pedido
void home_pedir_abrir(void);   // pede a pagina do titulo focado (menu do cartao)
int  home_foco_retomada(void); // card focado e de retomada (OK nele toca)
int  home_pediu_tocar(void);   // OK num card de retomada com "OK no card" = Retomar
int  home_pediu_menu(void);    // ESQUERDA na primeira coluna: chama o menu
int  home_pediu_relogio(void); // CIMA alem do topo; consumido pelo roteador
int  home_pediu_ilha(void);   // CIMA no alto da home: foco na ilha (#495)
int home_pediu_social(void);
int home_pediu_pessoa_social(CatItem *saida);
// OK numa fileira de canal ("Ver tudo" dela, ou num cartao de canal — que ai
// recebe o id para o guia ja abrir focado nele). `id` pode ser NULL.
int  home_pediu_guia(char *id, int tam);

// OK no "Ver tudo" da fileira da Biblioteca (#672): o roteador troca para
// TELA_BIBLIOTECA.
int  home_pediu_biblioteca(void);

// VITRINE DAS COLECOES (vertudo.c): destaque e cartaz da Home para um CatItem
// que nao esta no catalogo global. Ver home.c.
void home_vitrine_medida(float *w, float *h, float *passo, float *rotH, int redonda);
int  home_vitrine_arte_pronta(const CatItem *ci);
int  home_vitrine_arte(const CatItem *ci, float alfa);
int  home_vitrine_layout(void);
float home_vitrine_topo(float filH);
float home_vitrine_texto(const CatItem *ci, float topo, float teto, float alfa);
void home_vitrine_cartao(const CatItem *ci, GfxRect base, GfxRect card, float alfa, int redonda, float foco);
#endif
const char *home_rastro_foco(void);
// CARROSSEL DA DINAMICA: titulos (indices do catalogo) da fileira em foco e a
// posicao do focado; 0 fora da Dinamica ou numa fileira que nao e de titulos.
int  home_fileira_titulos(int *out, int max, int *pos);
// Poe o foco da fileira atual no titulo `indice` (a volta do carrossel).
void home_focar_titulo(int indice);
