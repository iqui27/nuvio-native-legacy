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
void player_do_inicio(void);
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

// O EPISODIO CONTA COMO ASSISTIDO AO SAIR? — issue #100.
//
// Havia DOIS numeros para o mesmo acontecimento, e eles discordavam:
//   - o cartao de "proximo episodio" (player_regra_proximo) declara o episodio
//     terminado no marcador de creditos ou a 120 s do fim;
//   - o progresso enviado ao Trakt so virava /scrobble/stop ("assisti") com
//     >= 90% do tempo, porque player_encerrar so arredondava a posicao para o
//     fim dentro dos ULTIMOS 60 s.
// Em todo episodio com menos de 20 min os 120 s caem ABAIXO dos 90% — num de
// 18 min o cartao sobe aos 88,9% —, entao aceitar o proximo episodio que o
// proprio app ofereceu mandava /scrobble/pause e nada marcava. Era preciso
// marcar a mao, que e o relato do #100.
//
// Um numero so: o mesmo que abre o cartao. Mais os 60 s de folga de sempre,
// para quem sai por cima do fim sem passar pelo cartao.
int player_regra_concluiu(double posSeg, double durSeg, double cred);
void player_erro_fonte(void);
void player_limpar_erro_fonte(void);   // fonte "morta" que voltou a entregar
// 1 quando a fonte atual falhou. O app usa no watchdog de canal: stream de TV
// ao vivo que nao abre troca sozinho para o proximo da lista.
int  player_fonte_falhou(void);

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
// Pedidos que so existem com um CANAL no ar (tipo "channel"/"tv"):
// `player_pediu_guia` — BAIXO ou a tecla azul pediram o overlay do guia.
// `player_pediu_zap` — CH+/CH- do controle (NV_SCANCODE_CH_UP/DOWN): +1/-1.
int  player_pediu_guia(void);
int  player_pediu_zap(void);
// Identidade do canal congelada na abertura: o indice do catalogo pode ser
// remapeado por uma republicacao da descoberta em plena reproducao, e zap/foco
// do guia nao podem depender dele. "" quando a sessao nao e de canal.
const char *player_id_canal(void);
// Marca a sessao como CANAL com o item inteiro de quem abriu — usada pelo
// guia, que e quem sabe o que pediu para tocar.
void player_marcar_canal(const CatItem *item);
void player_evento(const SDL_Event *e);
void player_atualizar(float dt, Uint32 agora);
void player_desenhar(Uint32 agora);
int  player_quer_sair(void);  // 1 assim que o Back foi apertado
void player_encerrar(void);

// --- MINI-PLAYER (PiP) DE CANAL ---------------------------------------------
// Sair de um canal para a home nao mata a transmissao: o destino do plano de
// video encolhe para um canto e o app fura a superficie ali. O decode nao
// muda — so muda para onde o quadro vai.
//
// `player_minimizavel`: 1 quando a sessao e de canal com pipeline no ar — o
//   app chama player_minimizar() no lugar de player_encerrar() na saida.
// `player_restaurar`:  volta a tela cheia instantaneamente (o fluxo nunca
//   parou). `player_fechar_mini`: fecha de vez. `player_manter_mini`: marca
//   a proxima abertura como zap dentro do PiP (o CH+/- chama antes de
//   tocarCanal) — sem ela, abrir um canal volta a tela cheia.
int  player_minimizavel(void);
void player_minimizar(void);
int  player_mini_ativo(void);
void player_manter_mini(void);
void player_restaurar(void);
void player_fechar_mini(void);
void player_mini_desenhar(Uint32 agora);

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

// QUAIS CAMPOS A PESSOA MEXEU DE FATO.
//
// Existe por causa da legenda ASS: o arquivo traz cor propria (o letreiro
// amarelo, o narrador em azul) e a folha de faixas tambem oferece cor. Quando
// os dois falam, GANHA A PESSOA — um ajuste que ela mexeu e intencao
// explicita, e sobrepo-la com o que o arquivo acha seria o app discutindo com
// quem usa. Enquanto ela nao mexeu, o arquivo fala.
//
// "Restaurar padrao" LIMPA a marca (PLR_LEG_NADA): voltar ao padrao e dizer
// "quero o comportamento normal do app", e o normal do app e respeitar o
// arquivo. Tamanho, fonte e borda nao entram nesta conta — eles vencem o ASS
// SEMPRE, porque tamanho de legenda numa TV e acessibilidade, nao estilo.
#define PLR_LEG_NADA  0
#define PLR_LEG_COR   1
void player_leg_estilo_tocou(int campos);  // PLR_LEG_NADA zera
int  player_leg_estilo_tocado(int campo);
// Diretorio de DADOS (nao de arte) onde art/player.txt e gravado. Chamada por
// main.c com o mesmo valor de ajustes_dir — sem isto o arquivo ia parar na
// pasta de arte, que a TV nao trata como persistente. Ver a nota em
// prefsArquivo (player.c).
void player_dir(const char *dir);

#endif
