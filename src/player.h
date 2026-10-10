// Interface de reproducao nativa, seguindo o Nuvio oficial.
// O video e fornecido por video.c na LG; no Mac ha somente a interface.
#ifndef NV_PLAYER_H
#define NV_PLAYER_H

// VideoLegendaEstilo vem daqui: o estilo da legenda e guardado nas
// preferencias do player, mas quem o define e o modulo de video.
#include "video.h"
#include "catalogo.h"
#include "aovivo.h"
#include <SDL2/SDL.h>

// Abre a reproducao do titulo `indiceCatalogo` (indice circular, igual ao do
// catalogo). Titulo, logo, sinopse e arte saem dali.
//
// Com NULL, aguarda a consulta de fontes; nao simula uma reproducao.
void player_abrir(int indiceCatalogo, const char *url);
void player_definir_episodio(int temporada, int episodio);
void player_do_inicio(void);
// Posicao confiavel para preparar o Android: registro com duracao conhecida,
// coerente com o percentual escolhido. Zero mantem a retomada normal depois
// da duracao real; metadados/reservas nao servem para converter percentual.
double player_regra_retomada_inicial(double posSalva, double durSalva,
                                     int percentual, int concluido);
void player_episodio_atual(int *temporada, int *episodio);
void player_aprender_creditos(void);
int player_indice(void);
const char *player_linha_episodio(void);
int player_pediu_fontes(void);
// A explicacao da espera na ilha do player (inicio.h, #202): texto ja traduzido
// por quadro, "" = nada. O OK no cartao de abertura com texto pede Ajustes >
// Fontes e addons > Escolha da fonte; o app.c consome com este.
void player_definir_motivo_inicio(const char *texto);
int  player_pediu_ajustes_fonte(void);
// Ha quantos ms o player abriu (o Play); 0 fechado. Para o log do inicio (#202).
Uint32 player_aberto_ha_ms(void);
int player_pediu_proximo(int *temporada, int *episodio);
const CatEp *player_proximo_episodio(void);
// A REGRA DO CARTAO DE PROXIMO EPISODIO, isolada do estado do player para
// poder ser exercitada por teste. `cred` e o segundo em que os creditos
// comecam (0 quando nenhuma das duas fontes tem marcador). Devolve 1 quando o
// cartao deve estar no ar.
int player_regra_proximo(double posSeg, double durSeg, double cred);
// Mesma janela do cartao, antecipada em segundos reais (0 = agora).
int player_janela_proximo(double antecedencia);
// Base (y, tela de 1080) da legenda principal: sobe para 690 so com o cartao
// do proximo episodio NA TELA (posplay_sobre_video), e volta no quadro em que
// ele some — dispensado, aceito ou contagem encerrada (2.0.3).
float player_base_legenda(float baseNormal, int cartaoProximoNoAr);
// A duracao do pipeline e menos de um terco da minutagem do catalogo (>= 15
// min): ela nao descreve este episodio e a estimativa de fim nao pode usa-la.
int player_duracao_suspeita(double durSeg, double catSeg);

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
// O mesmo cartao, com a causa no lugar das frases genericas (issue #112):
// `titulo` substitui "Nao foi possivel abrir a fonte" e `dica` a linha de
// baixo. Os dois ja traduzidos; "" ou NULL mantem a frase generica.
void player_erro_fonte_motivo(const char *titulo, const char *dica);
// Aviso curto no alto da tela do player (a mesma pilula do modo de aspecto),
// por `ms`. Some sozinho; a sessao nova do player apaga o que estiver de pe.
void player_toast(const char *texto, unsigned ms);
// O mesmo aviso com o icone (art/icones) e, `ambar` = 1, a cor de aviso: a
// pilula da ilha abre com ele (plrilha.h). player_toast e o informativo.
void player_toast_ex(const char *texto, unsigned ms, const char *icone, int ambar);
void player_limpar_erro_fonte(void);   // fonte "morta" que voltou a entregar
// A tentativa do automatico de fontes ("Fonte 2 de 3" na ilha ao abrir):
// `n` = qual (1 = a primeira), `max` = o teto. 0, 0 = nenhuma.
void player_definir_tentativa(int n, int max);
// 1 quando a fonte atual falhou. O app usa no watchdog de canal: stream de TV
// ao vivo que nao abre troca sozinho para o proximo da lista.
int  player_fonte_falhou(void);
int  player_tem_video(void);   // esta sessao abriu um video (nao o trailer)

// 1 quando ha video de verdade por tras desta sessao. O desenho usa isto para
// nao pintar a arte-chave por cima do plano de video.
int  player_com_video(void);
// Pinta o que fica fora do furo do video (ver player.c). So o recuado leva arte.
void player_fundo_fora_do_furo(GfxRect furo, int recuado, const CatItem *c);
int  player_pediu_faixas(void);   // CIMA no player abre audio/legendas

// 1 enquanto a fonte abre. A tela mostra a arte-chave e um indicador; sem isso
// o usuario aperta Reproduzir e encara uma tela parada sem saber se funcionou.
int  player_carregando(void);
// Exposto para regressao de D-pad: BAIXO na fileira deve fechar a barra.
int  player_controles_visiveis(void);
// Idem, para #121: foco na barra de tempo e a posicao que ela mostra.
int   player_foco_na_barra(void);
// #128: 1 na busca que comecou com os controles escondidos, quando so a barra
// e o tempo estao na tela.
int   player_so_barra(void);
// A mola da fileira de botoes: fecha (0) com o foco na barra, volta (1) com
// ele embaixo. Para teste.
float player_fileira(void);
// #122: texto da legenda embutida entregue pelo player nativo, ja limpo.
void  player_limpar_legenda_nativa(char *s);
int   player_texto_legenda_nativa(char *dst, int tam);
float player_posicao_seg(void);
int   player_pausado(void);
int   player_pausa_pessoa(void);   // so a pausa pedida pela pessoa (#302)
// Fator de cor do OSD do player (Ajustes > Brilho da interface no player + degrau
// automatico com a barra parada). 1 = sem efeito. Ver esmaecer.h.
float player_osd_brilho(void);
float player_duracao_seg(void);
int   player_eh_canal(void);
// StreamFit (F03): 1 + real backend duration of the player's own source.
int   player_duracao_midia(double *seg);

// Liga a fonte numa sessao ja aberta. Existe porque o link so pode ser pedido
// no ultimo instante (ver stream_idade_ms), entao a tela abre antes de haver
// URL e o video entra quando chega.
void player_definir_fonte(const char *url);
// Desfaz a fonte em curso SEM fechar a tela: o video para e a sessao volta a
// "abrindo fonte", como logo depois de player_abrir. E o recuo da fonte
// guardada (fontevolta.h) para a busca normal, sem a pessoa ver o player
// fechar e abrir.
void player_voltar_a_esperar(void);

int  player_aberto(void);   // 1 enquanto a tela existe, inclusive durante o fade de saida
// Pedidos que so existem com um CANAL no ar (tipo "channel"/"tv"):
// `player_pediu_guia` — BAIXO ou a tecla azul pediram o overlay do guia.
// `player_pediu_zap` — CH+/CH- (NV_SCANCODE_CH_UP/DOWN), PgUp/PgDn e os botoes
//   do OSD, somados por um debounce de 600 ms (aovivo.h): o deslocamento total.
int  player_pediu_guia(void);
// O botao "Guia" do OSD do canal: o guia COMPLETO com o canal no preview.
int  player_pediu_guia_cheio(void);
int  player_pediu_zap(void);        // deslocamento em canais (+3, -1...), 0 = nenhum
int  player_pediu_recarregar(void);  // "Recarregar" do OSD: refaz a fonte do mesmo canal
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
// VOD na ilha: pausa antecipada durante a saida, conserva somente com ack,
// depois retoma o mesmo IMDb/episodio sem reabrir fonte ou buscar posicao.
void player_preparar_retencao(void);
int  player_suspender(void);
int  player_retido(void);
// 1 = retida so ate o voo da saida pousar (Android, sem "Manter o video
// pronto ao sair"); player_validar_retido solta em PLR_RETIDO_VOO_MS.
int  player_retido_so_voo(void);
int  player_retomar_retido(const char *imdb, int temporada, int episodio);
void player_validar_retido(Uint32 agora);
void player_descartar_retido(void);

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
// MINI NO GUIA: o "mini" cujo destino e o preview do guia (sem moldura: o
// guia fura e desenha em volta). `player_mini_no_guia` define a caixa e liga
// o modo — antes de player_manter_mini()+abrir, a sessao nova ja nasce no
// preview; com um PiP de canto no ar, ele desliza ate a caixa.
// `player_minimizar_para_guia`: da tela cheia ao preview, encolhendo, com o
// mesmo fluxo. `player_restaurar` com o mini no guia faz o caminho inverso,
// crescendo. `player_janela_animando` devolve 1 e o retangulo do degrau
// enquanto a janela anda — o guia fura ali durante o encolher.
void player_mini_no_guia(float x, float y, float w, float h);
int  player_mini_no_guia_ativo(void);
void player_minimizar_para_guia(float x, float y, float w, float h);
int  player_janela_animando(float *x, float *y, float *w, float *h);

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
//
// Tudo isto vale para o parser REDUZIDO (legenda.c). Com o libass desenhando,
// o arquivo manda em cor, fonte, fundo, posicao e borda (decisao do dono em
// 25/09/2026, alinhada ao app web 1.2.0): trocar a cor apagava karaoke e
// placas. O tamanho continua da pessoa, como escala proporcional do libass.
#define PLR_LEG_NADA  0
#define PLR_LEG_COR   1
void player_leg_estilo_tocou(int campos);  // PLR_LEG_NADA zera
int  player_leg_estilo_tocado(int campo);
// Diretorio de DADOS (nao de arte) onde art/player.txt e gravado. Chamada por
// main.c com o mesmo valor de ajustes_dir — sem isto o arquivo ia parar na
// pasta de arte, que a TV nao trata como persistente. Ver a nota em
// prefsArquivo (player.c).
void player_dir(const char *dir);

#ifdef NV_SHOT_HOOKS
// Capturas (tests/player_glass_shot.c): estado de tela sem pipeline.
void player_shot_estado(Uint32 agora, float pos, float dur, int tocando, int botao,
                        int barraFoco, int soBarra);
void player_shot_foco(int botao, int barra);
void player_shot_toast(Uint32 agora, const char *texto, const char *icone, int ambar, int modo);
void player_shot_esconder(void);
const char *player_shot_toast_texto(Uint32 agora);
void player_shot_carregando(int sim);
void player_shot_buscando(int sim);
void player_shot_video(int sim);
// Canal: a grade, o numero, quanto atras do ao vivo, o botao em foco e o
// painel de Informacoes.
void player_shot_favorito(int f);   // 1 = botao Favorito na fileira, 2 = e o canal nos favoritos
void player_shot_canal(const AoVivoEpg *e, int numero, int atrasS, int botaoFoco, int info);   // comVideo sem furo: a arte faz de video
#endif

// #202: a velocidade (centesimos) que o pipeline esta MEDIDO tocando; 100
// ate a pedida se confirmar pela posicao contra o relogio (velocidade.h).
int player_velocidade_efetiva(void);

#endif
