// Interface de reproducao nativa, seguindo o Nuvio oficial.
// O video e fornecido por video.c na LG; no Mac ha somente a interface.
#ifndef NV_PLAYER_H
#define NV_PLAYER_H

// VideoLegendaEstilo vem daqui: o estilo da legenda e guardado nas
// preferencias do player, mas quem o define e o modulo de video.
#include "video.h"
#include "catalogo.h"
#include <SDL2/SDL.h>

// Abre a reproducao do titulo `indiceCatalogo` (indice circular, igual ao do
// catalogo). Titulo, logo, sinopse e arte saem dali.
//
// Com NULL, aguarda a consulta de fontes; nao simula uma reproducao.
void player_abrir(int indiceCatalogo, const char *url);
void player_definir_episodio(int temporada, int episodio);
void player_episodio_atual(int *temporada, int *episodio);
int player_indice(void);
const char *player_linha_episodio(void);
int player_pediu_fontes(void);
int player_pediu_proximo(int *temporada, int *episodio);
const CatEp *player_proximo_episodio(void);
// A REGRA DO CARTAO DE PROXIMO EPISODIO, isolada do estado do player para
// poder ser exercitada por teste. `cred` e o segundo em que os creditos
// comecam (0 quando nenhuma das duas fontes tem marcador). Devolve 1 quando o
// cartao deve estar no ar.
int player_regra_proximo(double posSeg, double durSeg, double cred);
void player_erro_fonte(void);

// 1 quando ha video de verdade por tras desta sessao. O desenho usa isto para
// nao pintar a arte-chave por cima do plano de video.
int  player_com_video(void);
int  player_pediu_faixas(void);   // CIMA no player abre audio/legendas

// 1 enquanto a fonte abre. A tela mostra a arte-chave e um indicador; sem isso
// o usuario aperta Reproduzir e encara uma tela parada sem saber se funcionou.
int  player_carregando(void);
// Exposto para regressao de D-pad: BAIXO na fileira deve fechar a barra.
int  player_controles_visiveis(void);

// Liga a fonte numa sessao ja aberta. Existe porque o link so pode ser pedido
// no ultimo instante (ver stream_idade_ms), entao a tela abre antes de haver
// URL e o video entra quando chega.
void player_definir_fonte(const char *url);

int  player_aberto(void);   // 1 enquanto a tela existe, inclusive durante o fade de saida
void player_evento(const SDL_Event *e);
void player_atualizar(float dt, Uint32 agora);
void player_desenhar(Uint32 agora);
int  player_quer_sair(void);  // 1 assim que o Back foi apertado
void player_encerrar(void);

// --- MODOS DE PROPORCAO -----------------------------------------------------
// Os OITO modos do app web, na mesma ordem e com os mesmos fatores
// (js/core/player/playerAspect.js). A ordem importa: e ela que o ciclo percorre,
// e trocar a ordem aqui muda o que o dono encontra ao apertar a tecla.
//
// POR QUE ZOOM, e nao object-fit: a barra preta de um filme widescreen esta
// EMBUTIDA no quadro. Um 2.39:1 entregue como 3840x2160 tem proporcao de quadro
// 1.778 — a mesma da tela — entao "encaixar" e "preencher" dao exatamente a
// mesma imagem e nenhum dos dois corta coisa alguma. Cortar exige AMPLIAR e
// deixar o excesso sair da tela.
//
// Os fatores sao 16/9 dividido pela proporcao do filme, nao numeros escolhidos
// a gosto:  2.35:1 -> 1.32,  2.39:1 -> 1.34,  2.76:1 -> 1.55. O ULTRA existe
// porque o CINEMA (1.34) ainda deixa barra visivel num 2.76:1 — observado na
// TV do dono, nao deduzido.
//
// No nativo o video NAO e um elemento HTML: e um plano de hardware atras da
// superficie GL, posicionado por video_janela(). Entao cada modo vira um
// RETANGULO, e o "excesso que sai da viewport" do web vira um retangulo com
// coordenadas negativas e tamanho maior que a tela.
typedef enum {
  PLR_ASP_ORIGINAL = 0,   // "Fit (Original)"  contain, sem zoom  — PADRAO
  PLR_ASP_CROP,           // "Crop"            cover
  PLR_ASP_ESTICAR,        // "Stretch"         fill
  PLR_ASP_ZOOM_LEVE,      // "Slight Zoom"     cover x 1.15
  PLR_ASP_ZOOM_CINEMA,    // "Cinema Zoom"     cover x 1.34
  PLR_ASP_ZOOM_ULTRA,     // "Ultra Zoom"      contain x 1.55
  PLR_ASP_FIT_ALTURA,     // "Fit Height"      cover
  PLR_ASP_FIT_LARGURA,    // "Fit Width"       contain
  PLR_ASP_N
} PlrAspecto;

// Fatores de zoom, iguais aos do resolveAspectScale do web.
#define PLR_ZOOM_LEVE    1.15f
#define PLR_ZOOM_CINEMA  1.34f
#define PLR_ZOOM_ULTRA   1.55f
// Quanto tempo o aviso de troca de modo fica na tela. 1400ms e o setTimeout do
// showAspectToast do web.
#define PLR_TOAST_MS     1400u

int         player_aspecto(void);              // modo atual (PlrAspecto)
const char *player_aspecto_rotulo(int modo);   // "Cinema Zoom", "Encaixar"...
void        player_aspecto_definir(int modo);  // aplica e grava
void        player_aspecto_ciclar(void);       // proximo modo + aviso na tela

// ESTILO DA LEGENDA, guardado em art/player.txt junto com o aspecto: e
// preferencia do APARELHO e nao do titulo. A folha de faixas edita a struct e
// chama player_leg_estilo_mudou(), que aplica no pipeline e grava.
VideoLegendaEstilo *player_leg_estilo(void);
void player_leg_estilo_mudou(void);

#endif
