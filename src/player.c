// Tela de reproducao, no formato do NOSSO APP WEB.
//
// A referencia mudou: esta variante legacy segue o player do app web (o bloco
// #playerUiRoot em css/components.css), nao o do app da Apple TV que o
// prototipo nuvio-native desenha. O que veio de la e a MECANICA — mola de
// foco, auto-esconder, furo do pipeline — porque essa parte nao e questao de
// estilo. O arranjo e as medidas sao do web, anotadas uma a uma abaixo.
//
// Diferencas concretas em relacao ao que estava aqui: os botoes ficam a
// ESQUERDA e nao centralizados; o tempo e UM rotulo "decorrido / total" na
// ponta direita e nao dois com restante negativo; o subtitulo fica ABAIXO do
// titulo; a barra tem 6px e nao 8, sem marcador na cabeca; as tres pilulas
// informativas ("Informacoes", "Em Foco", "Continue Assistindo") sairam, que
// sao mobiliario do app da Apple e nao existem no nosso.
//
// Sao tres comportamentos observados no aparelho, e cada um deles muda o
// desenho inteiro:
//
//   1. Enquanto toca, a tela e SO o quadro. Zero interface. Nenhuma barra
//      residual, nenhum relogio de canto — o que aparece por cima da imagem
//      quando ninguem pediu e ruido.
//   2. Qualquer direcao no D-pad SOBE os controles pela base. Eles nao piscam
//      para dentro: entram com mola, deslizando de baixo, junto com o veu.
//   3. Parado alguns segundos, eles somem sozinhos — mas nao enquanto o video
//      esta pausado. Pausado sem controles o usuario fica olhando um quadro
//      congelado sem saber o que houve.
#include "mediaurl.h"
#include "horafmt.h"
#include "player.h"
#include "legfontes.h"
#include "memlog.h"
#include "dvtela.h"
#include "abrecine.h"
#include "ilhacart.h"
#include "idbase.h"
#include "dados.h"
#include "trailer.h"
#include "linguas.h"
#include "audioescolha.h"
#include "idioma.h"
#include "fontesonda.h"
#include "posplay.h"
#include "extras.h"
#include "video.h"
#include "seekprobe.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "telefoneui.h"
#include "catalogo.h"
#include "artehero.h"
#include "detail.h"
#include "logotitulo.h"
#include "extplayer.h"
#include "corviva.h"
#include "assperf.h"

// A CASCA DO TIZEN PRECISA SABER SE O PLAYER ESTA NA TELA. tools/tizen-shell.html
// traduz as teclas de midia do controle Samsung (play/pause, stop, avancar,
// voltar) para as teclas que este arquivo ja entende — e so pode fazer isso
// enquanto o player existe. Fora dele, play/pause viraria "OK" em cima do que
// estivesse em foco, inclusive um item de menu que marca coisa como vista. Um
// OK perdido ja fez exatamente isso uma vez na conta do dono (#36). O aviso e
// dado nas TRES transicoes de `aberto`, e so nelas: nada por quadro.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// #415: o piso de reproducao do cache de arte vale do abrir ao fechar. No
// Android a mesma chamada ja sai de video_android.c ao abrir a sessao; repetir
// aqui e idempotente (tex_pressao_reproducao sai cedo se o pct nao mudou).
// No webOS/.tpk o piso efetivo vem da faixa armada pelo MemAvailable
// (pressaoPlayMin em tex_memoria_livre): TV com memoria de sobra nao perde
// cache. A miniatura (PiP) tambem liga: ela significa video no ar.
static void avisarCascaAberto(int v) { EM_ASM({ window.nvPlayerAberto = $0; }, v); tex_pressao_reproducao(v); }
#elif defined(NV_ANDROID) && defined(NV_TOUCH_UI)
#include "android.h"
static void avisarCascaAberto(int v) { android_player_tela_cheia(v); tex_pressao_reproducao(v); }
#else
static void avisarCascaAberto(int v) { (void)v; tex_pressao_reproducao(v); }
#endif
#include "trakt.h"
#include "simkl.h"
#include "traktscrobble.h"
#include "perfisessao.h"
extern int cat_episodios_finais(int, const char *) NV_PERFIL_OPCIONAL;
extern int cat_episodio_tvdb(int, const char *, int, int) NV_PERFIL_OPCIONAL;
extern int cat_episodio_tvdb_qualquer(int, const char *, int, int) NV_PERFIL_OPCIONAL;
extern int cat_episodio_existe_final(int, const char *, int, int) NV_PERFIL_OPCIONAL;
extern int trakt_scrobble_alvo(int, const TraktScrobbleAlvo *, double, double,
                              double, int) NV_PERFIL_OPCIONAL;
#include "watchstate_envio.h"
#include "perfisessao.h"
#include "sync.h"
#include "parental.h"
#include "plrguia.h"
#include "episodios.h"
#include "streams.h"
#include "addons.h"
#include "duracaook.h"
#include "partes.h"
#include "badges.h"
#include "legenda.h"
#include "imgsub.h"
#include "assrender.h"
#include "mkvass.h"
#include "relogio.h"
#include "legimagem.h"
#include "intro.h"
#include "proximo.h"
#include "creditosauto.h"
#include "credfonte.h"
#include "credaprende.h"
#include "seekr.h"
#include "visto.h"     /* fim de episodio/filme para Simkl e conta */
#include "vistoep.h"   /* o check de "assistido" na lista de episodios (issue #100) */
#include "pausao.h"
#include "relogiofim.h"
#include "aovivo.h"
#include "ltv.h"
#include "botoes.h"
#include "velocidade.h"
#include "recomenda.h"
#include "home.h"
#include "descoberta.h"
#include "guia.h"
#include "livetv_regras.h"
#include "epg.h"
#include "xtream.h"
#include "xtepg.h"   /* grade curta do Xtream quando a XMLTV nao casa (#158) */
#include "ajustes.h"
#include "esmaecer.h"
#include "proxyts.h"
#include "xtream.h"
#include "perfis.h"
#include "sessao.h"
#include "legref.h"    // cabecalhos da fonte na leitura da referencia
#include "legsync.h"   // F05 AutoSync: lifecycle + offset (hunks marcados F05)
#include "progresso.h"
#include "fontevolta.h"
#include "retomamorna.h"
#include "marco.h"
#include "ilha_voo.h"   /* NV_ILHA_VOO_MS: quanto dura o voo da saida */
#include "plrui.h"
#include "escala.h"
#include "plrilha.h"
#include "neste.h"
#include "cacheboost.h"
#include "audioinfo.h"
#include "legendasui.h"   /* F04: second subtitle band */
#include <time.h>
#include "servidores.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>   // strcasecmp, para comparar o hdrType do pipeline
#include <math.h>
#ifdef NV_ANDROID
#include <pthread.h>
#include <stdatomic.h>
#endif
#include "ponteiro.h"
#include "atividade.h"
#include "reacao.h"
#include "bidi.h"

// Quanto tempo os controles ficam de pe sem receber tecla. Medido a olho no
// aparelho: perto de 4s. Menos que isso e o usuario perde a barra no meio de
// uma leitura; muito mais e a interface some tarde demais e atrapalha a cena.
#define PLR_ESCONDE_MS   4000u
// Salto de 10s (SALTO_SEG, salto.h) do avanca/retrocede. E o passo do controle da Apple, e ele so
// vale com os controles em pe: cegamente, seta seria um pulo invisivel.
// AVANCO SEGURADO: o passo cresce com o TEMPO de tecla segurada (nao com a
// contagem de repeticoes, que depende do controle) e anda no maximo ~3 vezes
// por segundo. Atravessar meia hora de filme a 10 s por toque eram 180 toques
// — foi a queixa — mas a primeira versao disparava a ~20 min/s e "ja voava pro
// final". A regra e os degraus estao em salto.h.
#include "salto.h"
#include "seekgesto.h"    // .tpk: seek gesture (owner-tuned values, SEEKG_PADRAO)
#include "video_seekretry.h"
#include "queda.h"
// Sem tecla por este tempo, o avanco termina: manda a posicao ao pipeline e
// retoma. 420 ms e maior que o intervalo de repeticao do controle (que na C9
// fica perto de 100 ms) e menor que o tempo de reacao de quem soltou de
// proposito.
#define PLR_SCRUB_FIM_MS  SALTO_FIM_MS   // sem Seekr; com ele salto_fim_ms()
// Duracao de reserva, em segundos, para quando o `meta` do catalogo nao traz
// tempo de filme (as series trazem "3 temporadas", que nao e duracao de nada).
// 1h54 e so um numero plausivel para o layout ter o que mostrar — assim que o
// video real entrar, a duracao vem do decodificador e esta constante morre.
#define PLR_DUR_PADRAO   (114.0f * 60.0f)
// GEOMETRIA DO OSD — GLASS UI (mockup aprovado em 03/10, player-mockup.html).
// Uma margem so, 96, para titulo, barra, botoes, tempo, selos e a ilha da hora
// (era 64 em parte disso e a barra ia de ponta a ponta). A barra tem cantos
// redondos (plrui_barra, a mesma da pausa e do ao vivo), os botoes sao discos
// de 68 no material da ilha e o focado vira a pilula no acento com o nome.
#define PLR_PAD_X         PLR_MARGEM
#define PLR_PAD_Y         48.0f
#define PLR_MARGEM        NV_ILHA_MARGEM_D   // a margem da ilha do relogio (Home e player)
#define PLR_BTN_D         68.0f
#define PLR_BTN_GAP       12.0f
#define PLR_GAP_ROW       38.0f   // topo da barra -> topo dos botoes
#define PLR_TOQUE_CELULA  144.0f  // antes da escala UI; 48 dp numa tela de 360 dp
// Onde a barra fica (topo do trilho): com o OSD inteiro, e na busca so com a
// barra (#128) — mais baixa, sem nada embaixo dela alem do tempo. Com o Seekr
// ligado a busca deixa 30 px a mais para a ilha da miniatura.
// Ancoradas na BASE da tela do OSD: o OSD e camada ampliada (escala.h) e mede
// pela tela virtual. Em 1080 sao 902, 960 e 930, como no mockup.
#define PLR_BARRA_Y      (NV_VTELA_H - 178.0f)
#define PLR_BARRA_Y_SO   (NV_VTELA_H - 120.0f)
#define PLR_BARRA_Y_SEEKR (NV_VTELA_H - 150.0f)
// De quanto o bloco desliza para baixo quando escondido. Pequeno de proposito:
// o que faz o movimento ser lido nao e a distancia, e a mola somada ao fade.
#define PLR_DESLIZE       46.0f
// Quanto tempo a guia parental fica na tela, contando do primeiro quadro com
// imagem, e quanto dura o esmaecimento final. Depois disso ela nao volta nesta
// reproducao.
//
// ERAM SETE SEGUNDOS, e a conta que os justificava media LEITURA, nao quadros.
// Sete segundos sao o bastante para ler quatro linhas curtas a 60 fps. No
// Samsung do relator do #31 o app estava a 1,3-2,6 fps nos trechos ruins (ver
// #33): a entrada escalonada sozinha leva ~1 s, e os sete segundos inteiros
// cabem em cerca de DEZ quadros desenhados. Para quem esta na frente da TV isso
// e exatamente o relato — "aparece por uma fracao de segundo e some".
//
// Doze segundos nao consertam o fps, e nao e para isso que estao aqui: eles
// fazem o aviso sobreviver a um aparelho lento, que e a unica coisa que este
// numero pode fazer sozinho.
// 12 s -> 6 s (dono, 05/10: metade do tempo em tela; a pilula da hora segura
// 1600 -> 800 ms em plrilha.c, a animacao em si nao mudou).
#define PG_SEG_TOTAL      6.0f
// Depois disto o aviso nao entra mais: e um aviso do comeco do filme, e a
// resposta pode chegar tarde. Ver a nota no desenho.
#define PG_LIMITE_SEG     45.0f

// Transporte compacto. Os saltos continuam acessiveis pelas setas na barra.
// IDs estaveis: a ordem visual e de navegacao vem de transporteAcoes.
// Na serie, Anterior/Proximo ficam ao lado do Play; o restante conserva a ordem.
enum { PLR_PLAY, PLR_CC, PLR_AUDIO, PLR_ASPECTO,
       PLR_FONTES, PLR_EPISODIOS, PLR_PROXIMO, PLR_REINICIAR, PLR_NESTE, PLR_NBTNS };

// Avanco em curso: enquanto vale, posSeg e do DONO e nao do pipeline.
static int    scrubbing, scrubPassos, scrubTocava, scrubDir, scrubToque;
static double scrubInicio;
static Uint32 scrubUltimo;
static SaltoEst scrubSalto;   // rajada do teclado (salto.h)
#ifdef NV_TPK
// .tpk SEEK GESTURE (seekgesto.h, owner 20/10): taps and holds share one
// remembered step; the memory lives until the seek commits (terminarSalto).
static SeekGesto gesto;
static const SeekParams seekPar = SEEKG_PADRAO;
// SEEK IN FLIGHT. The host only updates its position cache once
// SetPlayPositionAsync finishes, so for a while after a seek video_pos() is
// the OLD position. Copying it into posSeg made the bar slide back to where
// the video was, and a press in that window scrubbed from the old spot. While
// `vooAtivo` the bar, the clock and a new scrub use the target instead.
static int    vooAtivo, vooTentativas;
static double vooAlvo;
static Uint32 vooDesde, vooLivreEm, vooRetryEm;
static unsigned vooFalhas, vooGer;
#define VOO_PRAZO_MS   20000u   // the host gives up at 15 s and reconnects
#define VOO_PERTO_S    3.0      // landed: position within this of the target
#define VOO_LIVRE_MS   1500u    // or this long after the TV stopped seeking
// Elapsed ms, never negative. `agora` is the frame clock (taken once, before
// player_atualizar) while a commit from the inactivity timer stamps
// SDL_GetTicks() LATER in the same frame: unsigned `agora - vooDesde` wrapped
// to ~4e9 and the 20 s timeout fired on the very next line (the log showed
// "gave up waiting" 16 ms after "seek sent"), dropping the target and bringing
// the snap-back back.
static Uint32 vooDecorrido(Uint32 agora, Uint32 desde) {
  Sint32 d = (Sint32)(agora - desde);
  return d > 0 ? (Uint32)d : 0u;
}
#endif
// BUSCA SUAVE: o que a BARRA mostra durante o avanco. posSeg anda em degraus
// (10 s, 30 s, 60 s, 120 s por passo, ~3 passos por segundo) e desenhar direto dele fazia
// o preenchimento saltar aos trancos. posVis persegue posSeg por mola de
// segunda ordem; so a barra le posVis. O TEMPO escrito e o seek continuam em
// posSeg, o alvo exato. Fora do avanco (e depois de assentar) posVis = posSeg.
static float  posVis, posVisV;
static int    posVisSolto;   // 1 = ainda deslizando ate o alvo
#define PLR_BUSCA_MOLA 18.0f // rad/s: assenta em ~300 ms depois da ultima tecla

static int   aberto = 0, saindo = 0, pediuSair = 0;
static int   idx = 0;
// UI-thread ownership of this exact player source/session.
static unsigned fonteAcaoGeracao;
static void fonteAcaoInvalidar(void) { if (!++fonteAcaoGeracao) ++fonteAcaoGeracao; }
#define PLR_SCR_TOCOU_S 5.0f   // #179: reproducao continua antes do /scrobble/start
// Reproducao real minima para a SAIDA gravar o ponto (ver `prova`). Nao e o
// limiar do start: um seek logo depois de abrir nao pode custar o ponto.
#define PLR_PROVA_MIN_S 1.0
static int   tocando = 1;
static int   pausaPessoa;   // pausa pedida pela pessoa (player_pausa_pessoa, #302)
static int retomandoSalto; // seek requested playback; buffering is not user pause
static CreditosAuto creditosAuto;
// Uma unica sessao VOD pausada, por no maximo dois minutos. Nao abre conexao
// especulativa: e o pipeline que ja estava exibindo este titulo.
//
// SO SE A PESSOA PEDIR (dono, 05/10): "segurar o filme esta estragando a
// experiencia". O padrao e NAO reter — sair libera o pipeline na hora e o
// Retomar abre pela fonte guardada, sem busca nos addons. Reter e o ajuste
// avancado "Manter o video pronto ao sair" (ajustes_manter_video, podeReter).
//
// O PRECO DE RETER MAIS: o pipeline e um so. Enquanto a sessao esta retida o
// trailer do destaque da home nao toca (home_trailer_passo exige
// !player_retido(), app.c) — pelo prazo escolhido, ou sem limite,
// a home fica sem trailer. Abrir uma pagina de titulo, outro video, trocar
// de perfil/conta ou dispensar a ilha soltam na hora, como antes. Passado o
// prazo, o Retomar ainda evita a busca nos addons pela fonte guardada
// (fontevolta.h).
// RETIDO SO PARA O VOO (Android, 05/10/2026). Sem "Manter o video pronto ao
// sair" a saida fechava a sessao ANTES do voo (fecharSessao(0) -> video_parar,
// "[player] saida: video parar 3 ms"): o SurfaceView ficava vazio, o
// ilha_minimizar_dissolver recebia player_retido()=0 e o voo nascia com a ARTE
// em tela cheia por cima de um video que ja nao existia — e ainda depois dos
// ~430 ms do fade do OSD, porque sem preparar a retencao nao ha a saida rapida
// (PLR_SAIDA_ILHA_TETO_MS). Dono: "pisca, mostra a arte e depois encolhe".
// Agora a mesma pausa da retencao vale tambem sem o ajuste, mas so ate o voo
// pousar: o quadro parado fica no plano de baixo enquanto a home + arte se
// dissolvem por cima (VOO_DISSOLVE_MS em ilha.c) e o pipeline e solto logo
// depois do pouso, atras da home ja opaca. Memoria e decoder voltam em < 1 s,
// nao em 2 min. LG/Samsung nao dissolvem (o plano nao aparece por baixo do GL
// do mesmo jeito), entao la nada muda.
#define PLR_RETIDO_VOO_MS (NV_ILHA_VOO_MS + 240u)
static int retidoVoo;
static int retido, prepararRetencao, retidoPerfil, retomarMkv;
static Uint32 retidoDesde;
static int quadroCapturado;
// SAIDA PARA A ILHA SEM O FADE DO PLAYER (Android): o voo comeca assim que a
// pausa foi confirmada (evento 3, ~3 ms na TCL), em vez de ~430 ms de OSD
// apagando sobre o video parado antes de a home aparecer. Sem confirmacao no
// teto, segue sem reter (fechamento normal).
#define PLR_SAIDA_ILHA_TETO_MS 220u
static Uint32 saidaIlhaDesde;
static char retidoConta[96], retidoUrl[NV_MEDIA_URL_MAX];
// Botao em foco na fileira de transporte. Comeca no PLAY porque e a resposta
// que nove de cada dez aberturas quer: o dedo para no centro e o OK decide.
static int   botao = PLR_PLAY;
// A barra de progresso e um alvo de foco, como no web: `.player-progress-shell`
// engorda de 6 para 10px e clareia o trilho quando focada. Fica FORA do enum
// dos botoes porque nao e um botao — o OK nela nao 'aperta' nada, e o
// ESQUERDA/DIREITA muda de significado (procura, em vez de trocar de foco).
static int   barraFoco = 0;
static int   visivel = 0;          // alvo dos controles (1 = em pe)
static float anim = 0.0f;          // 0..1 seguindo `visivel`, por mola
// SO A BARRA (#128). A busca que comeca com os controles escondidos (#121)
// sobe so o trilho e o tempo: titulo, botoes, relogio e selos continuam fora,
// porque quem procura um ponto no filme esta olhando o video, e o resto da
// interface na frente dele "distrai". Qualquer outra tecla (BAIXO, OK, CIMA)
// ou o ponteiro devolve os controles inteiros, como antes. `cheio` e a mola
// desse resto: 0 com so a barra, 1 com tudo.
static int   soBarra = 0;
static float cheio = 1.0f;
static float focoB[PLR_NBTNS];     // mola de foco de cada botao
// A FILEIRA DE BOTOES FECHA COM O FOCO NA BARRA (pedido do dono, 03/10): quem
// subiu para a barra esta procurando no filme, e os botoes embaixo dela so
// disputam o olhar. Ela desce e apaga pela mola dos controles e volta quando o
// foco desce de novo. Com o ponteiro ela fica: passar a mao na barra a
// fecharia, e a mao nao teria onde achar os botoes de volta.
static float fileira = 1.0f;
static int ponteiroNoPlayer(void);
static float entrada = 0.0f;       // 0..1 fade de abertura/fechamento da tela
static Uint32 ultimoInput = 0;
// Foco no botao "Pular abertura/resumo". Ele e um alvo de foco de verdade no
// web (.player-skip-intro-btn.focused); aqui ele vive ACIMA da barra: CIMA da
// barra vai para ele, BAIXO volta, OK pula. Quando os controles acordam com um
// trecho no ar o foco ja nasce nele — e o que a mao faz: "pra cima" e OK.
static int skipFoco = 0;
static int trechoPulavel(double *fim);

// UMA COR DE ACENTO SO NO PLAYER (Glass UI): ajustes_acento(), a mesma das
// ilhas. Havia duas contas (botao_cor_foco aqui, a mistura de 74% no
// pos-reproducao e nas faixas) e o mesmo tema saia em dois tons na mesma tela.
static void corFocoPlayer(float *r, float *g, float *b) { ajustes_acento(r, g, b); }

// Instante em que a IMAGEM comecou (nao a abertura da tela: entre uma coisa e
// outra ha a busca de fonte, que pode levar segundos). Zero enquanto nao houve.
// A guia parental se apoia nisto para aparecer UMA vez, no comeco, e sumir.
static Uint32 inicioImagem = 0;
static int memQuadroPendente; // Um primeiro quadro por load, nao por volta da ilha.
// MEDIDA DO RETOMAR (05/10): desde player_abrir ate o pronto e ate o primeiro
// "tocando" depois de aplicada a retomada. E o numero para comparar, na TV, o
// Retomar sem reter (fonte guardada + posicao) com o retido ("retomada pronta").
// So log; nao decide nada.
static Uint32 abertoEm;
static int    medirAbertura, tocouMedido;
static Uint32 loadEm;          // quando a URL foi entregue ao pipeline (0 = ainda nao)
// Quando os selos do guia parental entraram na tela. Ver a nota no desenho.
static Uint32 pgDesde;
// AS DUAS VARIAVEIS DE MIDIA. Todo o resto do arquivo le so daqui — quando o
// video real entrar, sao elas que passam a ser preenchidas pelo decodificador.
static int   comVideo = 0;
#ifdef NV_SHOT_HOOKS
static int   shotSemFuro, shotBusca;
static int   shotEpgOk, shotNumero, shotAtras, shotFav;   // shotFav: 1 = botao, 2 = nos favoritos
static AoVivoEpg shotEpg;
#endif
static int   pedFaixas = 0;
static int   esperandoFonte = 0;   // aberto sem URL, esperando o addon responder
#ifdef NV_ANDROID
static struct {
  unsigned token;
  int legenda, nativo, adotado, buffer, cacheMb, proximo, kbps;
  MkvassPreparo *mkv;
  Uint32 desde;
  double segundos, pct;
  char url[NV_MEDIA_URL_MAX], cab[2048], idioma[16], audio[16];
} preTitulo;
static _Atomic unsigned preTituloTokens;
#endif
static float posSeg = 0.0f;
// Relogio da LEGENDA (#92): posSeg e o ultimo currentTime do pipeline, que na
// C9 chega a cada ~200 ms. A legenda desenhada com ele andava aos degraus e em
// media 100 ms atras; relogio.c interpola entre as amostras.
static Relogio relLeg;
// #202: a velocidade que o PIPELINE esta tocando, medida (velocidade.h). E
// ela, e nao o pedido, que move a legenda, o "termina as" e o proximo episodio.
static VelMedidor velMed = { 100, 100, 0, 0, 0, 0, 0, 0 };
int player_velocidade_efetiva(void) { return velMed.efetiva; }
static double monoSeg(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}
// Prova de reproducao da fonte principal, independente de posicao/seek.
static char fonteSessao[NV_MEDIA_URL_MAX];
static double assistidoSeg, assistidoMono, assistidoPos;
static double eofDesde = -1.0;
static int assistidoAmostra;
static unsigned trailerSessao;
// Ultima posicao gravada DURANTE a sessao (gravarProgressoParcial); -1 = nenhuma.
static double parcialPos = -1.0;
static int parcialPausa;
static float parcialRelogio, parcialConta;
static void identidadeFonteAberta(void);
static void zerarAssistido(void) {
  eofDesde = -1.0;
  assistidoSeg = 0; assistidoAmostra = 0; trailerSessao = trailer_geracao();
  parcialPos = -1.0; parcialPausa = 0;
  parcialRelogio = parcialConta = 0;
  snprintf(fonteSessao, sizeof fonteSessao, "%s", comVideo ? video_url_atual() : "");
  identidadeFonteAberta();
}
static int fontePrincipal(void) {
  return comVideo && fonteSessao[0] && !trailer_aberto() &&
         trailerSessao == trailer_geracao() && !strcmp(fonteSessao, video_url_atual());
}
static void medirAssistido(void);
static double posLegenda(void) {
  double v;
  if (!relLeg.temAmostra || scrubbing) return posSeg;
  v = relogio_ler(&relLeg, monoSeg());
#ifdef NV_TPK
  /* CORRECAO DA LEGENDA (legimagem.h, dono 25/10): a imagem da TV anda um
   * pouco atras da nossa camada enquanto TOCA; pausada, nao. So tocando, com
   * leituras carimbadas, e nunca logo depois de um seek ainda sem leitura. */
  if (relLeg.carimbo && relLeg.tocando && !video_pos_incerta()) v -= legimagem_ms() / 1000.0;
#endif
  return v;
}
// Creditos tambem sao pulaveis — e o "Skip Outro" do web.
static int skipDisponivel(void), skipLiberado;
static int trechoPulavel(double *fim) { int tipo; return skipDisponivel() && intro_botao_visivel(fim, &tipo); }
static float duracaoSeg = PLR_DUR_PADRAO;
// O fim ZERO do TheIntroDB quer dizer "ate o fim da midia" (intro.c): mirar
// fim+0.25 ai seria um salto para o COMECO do arquivo — o alvo resolve o nulo.
static double puloDestino(double fim) { return fim > 0.0 ? fim + .25 : duracaoSeg; }
// O PULO OFICIAL do app (botao e automatico) conta como tocado: pular a
// abertura de um episodio curto nao pode custar o visto (#205). Seek manual nao.
// So credita e move posSeg; quem chama faz a busca (usuario ou automatica).
static void pularTrecho(double alvo) {
  if (alvo > posSeg) assistidoSeg += alvo - posSeg;
  posSeg = (float)alvo;
}

static char linhaEp[220];          // "T1, E1 · <sinopse curta>", montada na abertura

// CANAL DE TV (tipo "channel"/"tv"): sem progresso a gravar, sem episodio a
// seguir, sem fim — e com o GUIA como menu de contexto (BAIXO/azul abrem o
// overlay, CH+/- trocam de canal).
//
// O item fica CONGELADO na abertura, nao relido do catalogo a cada chamada: a
// descoberta republica o vetor de itens inteiro por fileira (cat_definir_tudo)
// DURANTE a reproducao, e a troca faz `idx` apontar para outro titulo — o
// canal no ar virava "nao-canal", o Baixo voltava a so acordar a interface e
// o overlay nunca abria. CatItem e so arrays, a copia e segura.
static int     canalSessao;
static CatItem itemCanal;
// MINI-PLAYER (PiP): o canal saiu da tela cheia mas segue no ar num canto.
// `querMini` e o pedido do CH+/- feito dentro do PiP: so ele mantem a
// miniatura na troca de canal — OK num canal (guia, home) volta a tela cheia.
static int     mini, querMini;
// MINI NO GUIA (25/09/2026): a mesma sessao "mini", mas o destino e o
// PREVIEW 800x450 do guia e nao o canto da tela, e sem moldura, etiqueta nem
// dica — quem fura a superficie e desenha em volta e o guia. E o que faz o
// canal no ar ir da tela cheia para o preview (e voltar) SEM recarregar: o
// pipeline e o mesmo, so o retangulo muda. O PiP de canto continua existindo
// fora do guia (canal aberto pela home).
static int     miniGuia;
// O TITULO ABERTO E UMA COPIA, NAO UM INDICE.
//
// `idx` e uma posicao no catalogo, e o catalogo e REPUBLICADO durante a sessao
// — sync da conta, remontagem da descoberta, colecoes chegando. Depois de uma
// republicacao a mesma posicao aponta para OUTRO titulo, e tudo que lia
// cat_item(idx) passava a falar dele: a arte e o nome na tela de carregamento,
// o alvo da busca de fontes, e ate a GRAVACAO DE PROGRESSO (que ia para o
// titulo errado). O relato do @rawldon foi o sintoma visivel: ao trocar de
// fonte, a tela de carregamento piscava a arte de um titulo visto antes —
// porque player_abrir reabria pelo indice velho.
//
// O canal ja tinha exatamente esta protecao (itemCanal / player_marcar_canal),
// pelo mesmo motivo. Agora TODO titulo e copiado na abertura e e a copia que
// vale; o indice e re-resolvido pelo IMDb quando alguem precisa dele (ver
// idxAtual).
static CatItem itemFixo; static int temFixo;
// A mesma URL que a pagina mostrava ao pedir Play. So a URL e copiada:
// texturas continuam no cache assincrono e nunca seguram a entrada do video.
static char cinemaArte[NV_TEX_URL_MAX];
static int cinemaPoster;
static void cinemaArteIniciar(const CatItem *c) {
  cinemaArte[0] = 0; cinemaPoster = 0;
  if (c) detail_arte_titulo(c, cinemaArte, sizeof cinemaArte, &cinemaPoster);
}
static const char *cinemaArteObter(const CatItem *c) {
  const char *u;
  if (cinemaArte[0] && !tex_falhou(cinemaArte)) {
    // O cartaz da semente e temporario (#503). Se a ficha do MESMO titulo
    // ganhou um fundo enquanto a fonte abria, acompanha a pagina uma vez.
    // Fundo de verdade fica congelado; o getter so copia estado ja local.
    if (cinemaPoster) {
      char atual[sizeof cinemaArte]; int poster;
      if (detail_arte_titulo(c, atual, sizeof atual, &poster) && !poster) {
        snprintf(cinemaArte, sizeof cinemaArte, "%s", atual);
        cinemaPoster = 0;
      }
    }
    return cinemaArte;
  }
  cinemaArte[0] = 0; cinemaPoster = 0;
  // Next e Play direto da Home nao precisam passar pela pagina. Usam a mesma
  // politica dela, inclusive arte alternativa, escolha manual e fundo por id.
  u = artehero_url_destaque(c, ajustes_hero_fonte(), ajustes_hero_arte_diferente());
  if (u && *u && strlen(u) < sizeof cinemaArte) {
    snprintf(cinemaArte, sizeof cinemaArte, "%s", u);
    cinemaPoster = c && c->poster[0] && !strcmp(u, c->poster);
    return cinemaArte;
  }
  return NULL;
}
static NvPerfilSessao itemSessao;
static unsigned itemTipoRevisao;
static int itemTipoConsultado, epPedidoT, epPedidoE, fonteSemEpisodio;
static char itemTipoConfirmado[8];
static const CatItem *item(void) {
  if (canalSessao) return &itemCanal;
  if (temFixo) return &itemFixo;
  return cat_item(idx);
}
// O indice CORRENTE do titulo aberto: re-resolvido pelo IMDb, porque o que foi
// guardado em `idx` pode ter sido deslocado por uma republicacao. Cai em `idx`
// so sem IMDb (ou com o player fechado). #151: fica na copia de sempre
// enquanto ela for o mesmo titulo — a primeira copia pelo IMDb costuma ser o
// card do CW, que nao tem a lista de episodios.
//
// #190 (Owlphibia29, "Attack on Titan trocado por Knights of Guinevere na tela
// de pausa e no menu de episodios"): o fio de "Continuar assistindo"
// (cat_trocar_continuar) refaz a fileira COM O PLAYER ABERTO — o log dela
// mostra "continuar assistindo refeita" logo depois do loadCompleted — e isso
// reordena os primeiros itens e desliza o resto. A tela de pausa, a folha de
// episodios e o cartao A seguir recebiam o `idx` cru e liam o vizinho.
//
// `idxVivo` guarda a ultima resolucao, para a busca partir dela e nao do
// indice da abertura. E quando o titulo SAIU do catalogo (a fileira de onde
// ele veio nao voltou), a copia da abertura entra de novo por cat_acrescentar,
// a mesma saida de detail.c (revalidarIdx). Devolver `idx` nesse caso era
// devolver outro titulo.
static int idxVivo = -1;
static int idxAtual(void) {
  if (temFixo && itemFixo.imdb[0]) {
    int i = cat_indice_titulo(itemFixo.imdb, idxVivo >= 0 ? idxVivo : idx);
    if (i < 0 && (aberto || mini) && !canalSessao && cat_n() > 0) {
      i = cat_acrescentar(&itemFixo);
      if (i >= 0) {
        printf("[player] %s saiu do catalogo com o player aberto: copia da abertura de volta em %d\n",
               itemFixo.imdb, i);
        fflush(stdout);
      }
    }
    if (i >= 0) {
      if (idxVivo >= 0 && i != idxVivo) {
        printf("[player] catalogo remontou: %s saiu de %d para %d\n", itemFixo.imdb, idxVivo, i);
        fflush(stdout);
      }
      idxVivo = i;
      return i;
    }
  }
  return idx;
}
static int ehCanal(void) { return canalSessao; }
// A MARCA DO DTS da faixa que esta tocando (DTS, DTS-HD, DTS:X), ou -1. Aparece
// em todo estado do DTS — a TV tocando sozinha, o app convertendo, a conversao
// desligada ou falha (dono, 06/10: "a badge do DTS ... quando for ativado ou
// quando nao for"). O DTS e da FONTE; o estado so diz como ele chega, e isso
// vai na ilha e nas Informacoes.
static int marcaDtsAtual(void) {
  int e = video_dts_estado();
  if (e == VIDEO_DTS_NENHUM) return -1;
  switch (video_faixa_dts(video_audio(video_audio_atual()))) {
    case 3: return FMT_DTSX;
    case 2: return FMT_DTSHD;
    default: return FMT_DTS;   // convertendo, a faixa vem do FFmpeg como "dts"
  }
}
// CODEC E CANAIS DA FAIXA TOCANDO ("E-AC-3 5.1 Atmos", "DTS-HD MA 5.1", #293),
// ou "" quando o codec e desconhecido (some em vez de chutar). Calculado na
// TROCA de faixa (indice, codec, canais, estado do receptor), nunca por quadro.
// Com o audio indo em bitstream para o receptor (Android), acrescenta o
// "receptor", discreto.
static const char *textoAudioAtual(void) {
  static char txt[56], chave[96];
  const VideoFaixa *f = video_audio(video_audio_atual());
  int pt = cacheboost_ganho_estado() == CB_GANHO_PASSTHROUGH;
  char k[sizeof chave];
  snprintf(k, sizeof k, "%d|%s|%d|%d|%s", video_audio_atual(), f ? f->codec : "", f ? f->canais : 0, pt,
           f ? f->rotulo : "");
  if (strcmp(k, chave)) {
    memcpy(chave, k, sizeof chave);
    txt[0] = 0;
    if (f && audioinfo_texto(f->codec, f->canais, strstr(f->rotulo, "Atmos") != NULL, txt, sizeof txt) && pt) {
      size_t n = strlen(txt);
      snprintf(txt + n, sizeof txt - n, " \xc2\xb7 %s", i18n("receptor"));
    }
  }
  return txt;
}
// IDIOMA E CANAIS DA FAIXA TOCANDO, o texto da fileira de formato ("Inglês
// 5.1"), com o CODEC no meio quando o dono quer (Ajustes > Mostrar o codec do
// audio, ligado de fabrica): "Inglês · E-AC-3 5.1 Atmos". O idioma vem da
// etiqueta do arquivo ("und" e vazio ficam de fora); "" sem nenhum dos dois.
static const char *rotuloAudioOsd(void) {
  static char txt[112], chave[160];
  const VideoFaixa *f = video_audio(video_audio_atual());
  int pt = cacheboost_ganho_estado() == CB_GANHO_PASSTHROUGH;
  const char *cod = ajustes_plr_codec() ? textoAudioAtual() : "";   // ja traz canais, Atmos e "receptor"
  char k[sizeof chave];
  snprintf(k, sizeof k, "%d|%s|%d|%d|%d|%s", video_audio_atual(), f ? f->idioma : "", f ? f->canais : 0, pt,
           ajustes_idioma(), cod);
  if (strcmp(k, chave)) {
    const char *nome = (f && f->idioma[0] && strcasecmp(f->idioma, "und")) ? i18n(ling_nome(f->idioma)) : "";
    const char *ch = f ? audioinfo_canais(f->canais) : "";
    memcpy(chave, k, sizeof chave);
    if (cod[0]) snprintf(txt, sizeof txt, "%s%s%s", nome, nome[0] ? " \xc2\xb7 " : "", cod);
    else {
      snprintf(txt, sizeof txt, "%s%s%s", nome, nome[0] && ch[0] ? " " : "", ch);
      if (txt[0] && pt) {
        size_t n = strlen(txt);
        snprintf(txt + n, sizeof txt - n, " \xc2\xb7 %s", i18n("receptor"));
      }
    }
  }
  return txt;
}
const char *player_id_canal(void) { return canalSessao ? itemCanal.imdb : ""; }
const char *player_tipo_canal(void) { return canalSessao ? itemCanal.tipo : ""; }

// Programa do canal no ar: a XMLTV (epgIdx) ou, num canal Xtream que nao
// casou nela, a grade curta do painel (#158). xtepg_passo aqui porque o guia,
// que o chama por quadro, pode nem estar aberto com o canal em tela cheia.
static int pAgora(int epgIdx, time_t t, EpgProg *p) {
  const char *id = player_id_canal();
  // O guia primeiro: cascata de casamento + fuso da fonte (M6).
  if (guia_programa_agora(id, t, p)) return 1;
  if (epgIdx >= 0) return epg_agora(epgIdx, t, p);
  if (!xtream_e_id(id)) return 0;
  xtepg_querer(id);
  xtepg_passo();
  return xtepg_agora(id, t, p);
}
static int pProximo(int epgIdx, time_t t, int k, EpgProg *p) {
  const char *id = player_id_canal();
  if (guia_programa_proximo(id, t, k, p)) return 1;
  if (epgIdx >= 0) return epg_proximo(epgIdx, t, k, p);
  return xtream_e_id(id) && xtepg_proximo(id, t, k, p);
}
// Marcacao deterministica para quem JA SABE que e canal (o guia): cobre a
// janela em que uma republicacao cai entre o cat_acrescentar e o player_abrir
// — o item no indice ja pode ser outro quando player_abrir le.
void player_marcar_canal(const CatItem *it) {
  if (!it) return;
  canalSessao = 1;
  itemCanal = *it;
}
// "38 min" / "1 h 5 min" do catalogo -> segundos (0 = nao deu para ler). So
// serve de ESCALA para o marcador de um episodio vizinho (credfonte.h).
static double duracaoTexto(const char *s) {
  int h = 0, m = 0; const char *p = s ? s : "";
  while (*p) {
    if (*p >= '0' && *p <= '9') {
      int v = 0; while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
      while (*p == ' ') p++;
      if (*p == 'h' || *p == 'H') h = v; else m = v;
    } else p++;
  }
  return (double)(h * 60 + m) * 60.0;
}
static int credFonteAtual;
static int retomadaAplicada, retomarPct;
static double retomarFonteSeg = -1.0;
// BASE DO VISTO (#205): o registro do ponto retomado, lido UMA vez por episodio
// (baseT/baseE) e nunca no fechamento — reler contaria de novo o que a
// suspensao (player_suspender) ja gravou. baseDur 0 = retomada so pelo
// percentual, sem registro local (Trakt): vale basePct, como antes da #205.
static double basePos, baseDur;
static int basePct, baseTocou, baseT = -1, baseE = -1;
static int epT, epE, pedFontes, erroFonte, pedProxT, pedProxE;
// Sinopse do episodio em exibicao (do catalogo); vazia se ele nao a trouxe,
// e ai a pausa usa a da serie.
static const char *sinopseEpisodioAtual(void) {
  int ix = idxAtual();
  if (epE <= 0) return "";
  for (int i = 0; i < cat_n_episodios(ix); i++) {
    const CatEp *ep = cat_episodio(ix, i);
    if (ep && ep->temporada == epT && ep->episodio == epE) return ep->sinopse;
  }
  return "";
}

static int itemTipoCerto(void) {
  return temFixo && (!strcmp(itemFixo.tipo, "movie") || !strcmp(itemFixo.tipo, "series"));
}

static void identidadeFonteAberta(void) {
  char base[64];
  idbase_copiar(temFixo ? itemFixo.imdb : "", base, sizeof base);
  // Uma fonte ja aberta para o titulo sem episodio nao vira outro video
  // quando o /meta descobre que e serie. So uma nova fonte limpa esta prova.
  fonteSemEpisodio = comVideo && temFixo && !itemTipoCerto() &&
                     base[0] && stream_lista_do_alvo(base);
}

static int atualizarTipoItem(void) {
  unsigned revisao;
  int mudou = 0;
  if (!temFixo || canalSessao || !itemFixo.imdb[0] || itemTipoCerto() ||
      !nv_perfil_sessao_travar(itemSessao)) return 0;
  revisao = cat_revisao_itens();
  if (!itemTipoConsultado || revisao != itemTipoRevisao) {
    itemTipoConsultado = 1; itemTipoRevisao = revisao;
    itemTipoConfirmado[0] = 0;
    cat_tipo_confirmado_por_id(itemFixo.imdb, itemTipoConfirmado, sizeof itemTipoConfirmado);
  }
  if (itemTipoConfirmado[0] &&
      (strcmp(itemTipoConfirmado, "series") ||
       (epPedidoT >= 0 && epPedidoE > 0 && !fonteSemEpisodio))) {
    // Pedido de episodio nao vira filme. Uma copia movie do mesmo id e a
    // colisao do Cinemeta (#524), e adotá-la apagava T/E e gravava o filme.
    if (!strcmp(itemTipoConfirmado, "movie") &&
        epPedidoT >= 0 && epPedidoE > 0 && !fonteSemEpisodio) {
      nv_perfil_sessao_destravar();
      return 0;
    }
    // O retrato continua sendo o da abertura. Nunca copiar nome, arte,
    // progresso ou URL de uma republicacao da home para a sessao tocando.
    snprintf(itemFixo.tipo, sizeof itemFixo.tipo, "%s", itemTipoConfirmado);
    mudou = 1;
  }
  nv_perfil_sessao_destravar();
  return mudou;
}

static void atualizarIdentidade(void) {
  if (atualizarTipoItem()) player_definir_episodio(epPedidoT, epPedidoE);
}

static int identidadeValida(void) {
  return temFixo && nv_perfil_sessao_atual(itemSessao) &&
         (!strcmp(itemFixo.tipo, "movie") ||
          (!strcmp(itemFixo.tipo, "series") && epPedidoT >= 0 && epPedidoE > 0 && epT >= 0 && epE > 0));
}
// A faixa de audio escolhida a mao vale para ESTE titulo (o filme, ou a serie
// inteira: os outros episodios tambem) (audioescolha.h): troca de fonte,
// reabertura e proximo episodio mantem, outro titulo esquece.
static void audioTitulo(void) {
  const CatItem *c = item();
  if (!c) { audioesc_titulo(""); return; }
  audioesc_titulo(c->imdb[0] ? c->imdb : c->titulo);
}
static int trocaEpisodio, episodioPedidoMarcado, confirmacaoTransporteRetida;
static Uint32 episodioPedidoEm, recarregarPedidoEm;
static int recarregarPedidoMarcado;
static void medirAssistido(void) {
  double m = monoSeg(), p = video_pos();
  int ok = fontePrincipal() && video_ativo() && video_pronto() && video_tocando() &&
           !video_bufferando_ms() && !video_falhou() && !scrubbing &&
           !esperandoFonte && !erroFonte && retomadaAplicada;
  double dm = m - assistidoMono, dp = p - assistidoPos;
  // Exige avanco do pipeline e limita pelo monotono: seek nao acrescenta
  // horas, relogio parado nao conta e uma pausa longa nao vira reproducao.
  if (ok && assistidoAmostra && dm > 0 && dm <= 2.0) {
    // currentTime da LG chega em degraus (~200 ms). Mantem a amostra ate
    // andar: somar so o ultimo quadro perderia quase todo o tempo real.
    if (dp == 0) return;
    // Segundos de MIDIA: a 1,5x o titulo inteiro passa em 2/3 do relogio, e
    // quem viu assim viu tudo. O teto e a maior velocidade que o app oferece.
    if (dp > 0 && dp <= dm * 4.0 + 0.5)
      assistidoSeg += fmin(dm * VEL_LISTA[VEL_N - 1] / 100.0, dp);
  }
  assistidoMono = m; assistidoPos = p; assistidoAmostra = ok;
}
// Dentro de player_abrir: o episodio ainda e o do progresso, nao o que vai
// tocar. Ver a nota la — segura a invalidacao da lista de fontes (#101).
static int abrindoSessao;
// Identidade Trakt escolhida uma vez por abertura. O par exibido continua a
// servir ao addon/Simkl/estado local; TVDB serve apenas ao Trakt quando a lista
// final de episodios provou a correspondencia.
static TraktScrobbleAlvo scrobbleAlvo;
static int scrobbleAlvoPronto;
static int scrobbleTvdbCapturado;
static int pedGuia, pedZap, pedGuiaCheio;   // pedidos de canal: overlay do guia / CH+/-
// OSD PROPRIO DO CANAL AO VIVO (aovivo.h). O zapping acumula toques e so pede a
// troca depois do debounce; `pedZap` passa a ser um DESLOCAMENTO (pode ser 3 ou
// -2), 0 = nenhum. `pedRecarregar` refaz a busca de fonte do mesmo canal.
static AoVivoZap zapEst;
static float bannerAV;         // 0..1, a entrada do banner do zapping
static int botaoAV, infoAV, pedRecarregar, pedUltimo;
static LtvDigitos digitosCanal;   // numero digitado na Live TV (M7)
static int pedNumero;
static int avErroFoco;   // o botao em foco no modal do erro do canal
// ATRAS DO AO VIVO (29/09/2026). Canal com janela de tempo (DVR do provedor)
// pausa; quem pausa fica atras da transmissao, e o OSD diz quanto e oferece
// "Voltar ao vivo". A conta NAO e `duracao - posicao` crua: na borda do ao vivo
// o pipeline ja fica uns segundos atras (a latencia do HLS), e isso nao e
// atraso de quem assiste. `avLat0` e essa folga, medida na primeira leitura
// tocando; atraso = (duracao - posicao) - avLat0. Pausado, o pipeline para de
// andar (a duracao pode nao crescer), entao o atraso e o da hora da pausa mais
// o relogio desde ela (`avPausaDesde`).
static double avLat0 = -1.0, avAtraso;
static Uint32 avPausaDesde;
// Indice EPG do canal no ar, resolvido uma vez por abertura (-2 = sem grade).
static int epgIdx = -1;
static unsigned epgIdxRev;
static void pEpgAtualizar(void) {
  if(epgIdxRev!=epg_revisao()){epgIdxRev=epg_revisao();epgIdx=-1;}
  if(epgIdx==-1) {
    epgIdx=epg_match_canal(player_id_canal(),NULL,itemCanal.titulo);
    if(epgIdx<0) {
      int estadoFonte=epg_canal_estado(player_id_canal());
      if(estadoFonte==EPG_PRONTO || estadoFonte==EPG_FALHOU)epgIdx=-2;
    }
  }
}
// Diagnostico da janela do cartao de proximo episodio. Zerados a cada episodio
// por player_definir_episodio: um `static int` dentro da funcao registraria a
// PRIMEIRA reproducao da sessao e ficaria mudo em todas as outras — que e
// justamente quando o relato acontece.
static int credAvisado, credFimAvisado, semProxAvisado;
static double credAvisadoEm, credAvisadoDur;
// 2.0.1, "o Proximo apareceu no meio do episodio". Zerados a cada ABERTURA, e
// nao a cada episodio: a mesma copia reaberta por outra fonte e outra duracao.
//   duracaoReal    o pipeline ja disse a duracao DESTE video. Ate la duracaoSeg
//                  e a minutagem do catalogo ou os 114 min de reserva, e decidir
//                  com ela e decidir com numero inventado.
//   durCurtaAvisado/durPassouAvisado   uma linha de log cada, por reproducao.
static int duracaoReal, durCurtaAvisado, durPassouAvisado;
static int concluiuAgora(double cred);
static double duracaoCatalogo(void);
static int introIdx=-1, introT=-1, introE=-1;
// Com que id o TheIntroDB foi perguntado (0 = imdb). Muda quando o TMDB do
// episodio chega depois da abertura (2.0.3): pede de novo por tmdb_id.
static long introTmdb;

// O EPISODIO TOCANDO NO TMDB, quando o fio de episodios o confirmou
// (CatEp.tmdbSerie/tmdbT/tmdbE). Filme: o id do item. 0 = nao se sabe.
static long tmdbDoTocando(int *tt, int *te) {
  const CatItem *c = item();
  *tt = *te = 0;
  if (!c) return 0;
  if (epE <= 0) return c->tmdb > 0 ? c->tmdb : 0;
  for (int ix = idxAtual(), i = 0; i < cat_n_episodios(ix); i++) {
    const CatEp *ep = cat_episodio(ix, i);
    if (ep && ep->temporada == epT && ep->episodio == epE) {
      if (ep->tmdbSerie > 0 && ep->tmdbT >= 0 && ep->tmdbE > 0) {
        *tt = ep->tmdbT; *te = ep->tmdbE; return ep->tmdbSerie;
      }
      return 0;
    }
  }
  return 0;
}
#ifdef NV_ANDROID
static double retomarSeg;
static int retomadaNaPreparacao;
#endif
// "Assistir do comeco" (issue #46): trava da sessao, armada por
// player_do_inicio depois de player_abrir. Tem de sobreviver as CHAMADAS
// REPETIDAS de player_definir_episodio — uma delas dispara quando o nome do
// episodio chega tarde (player_atualizar) e reatribuiria retomarPct.
static int semRetomada;
int player_indice(void) { return idxAtual(); }
const char *player_linha_episodio(void) { return linhaEp; }
int  player_pediu_guia(void) { int v = pedGuia; pedGuia = 0; return v; }
int  player_pediu_guia_cheio(void) { int v = pedGuiaCheio; pedGuiaCheio = 0; return v; }
int  player_pediu_zap(void)  { int v = pedZap;  pedZap  = 0; return v; }
int  player_pediu_recarregar(void) { int v = pedRecarregar; pedRecarregar = 0; return v; }
int  player_pediu_ultimo(void) { int v = pedUltimo; pedUltimo = 0; return v; }
int  player_pediu_numero(void) { int v = pedNumero; pedNumero = 0; return v; }
const char *player_titulo_canal(void) { return itemCanal.titulo; }
void player_episodio_atual(int *t, int *e) { *t = epT; *e = epE; }
// O dono avancou na mao (Comecar agora): lembra quanto faltava (credaprende.h).
void player_aprender_creditos(void) {
  const CatItem *c = item();
  if (c && c->imdb[0] && epT >= 0 && epE > 0 && comVideo && video_pronto())
    cred_aprender(c->imdb, duracaoSeg, posSeg, credFonteAtual);
}
// A EXPLICACAO DA ESPERA NA ILHA (#202, inicio.h). app.c diz por quadro, ja
// traduzida, o que a abertura esta esperando; vazio = nada a dizer. Com texto,
// o cartao "Abrindo fonte" ganha uma linha e o OK abre Ajustes > Fontes e
// addons > Escolha da fonte (player_pediu_ajustes_fonte). Transitoria: some
// sozinha quando a abertura acaba, e nunca segura nada.
static char motivoInicio[112];
static int pedAjFonte;
void player_definir_motivo_inicio(const char *texto) {
  snprintf(motivoInicio, sizeof motivoInicio, "%s", texto ? texto : "");
}
int player_pediu_ajustes_fonte(void) { int v = pedAjFonte; pedAjFonte = 0; return v; }
Uint32 player_aberto_ha_ms(void) { return aberto && abertoEm ? SDL_GetTicks() - abertoEm : 0; }
int player_pediu_fontes(void) { int p = pedFontes; pedFontes = 0; return p; }
int player_pediu_proximo(int *t,int *e) {
  if(pedProxT<0||pedProxE<=0)return 0;
  if(t)*t=pedProxT;if(e)*e=pedProxE;pedProxT=pedProxE=0;return 1;
}
int player_transporte_pedido_pendente(void) {
  return (pedProxT >= 0 && pedProxE > 0) || (!ehCanal() && pedRecarregar);
}
void player_episodio_pedido_liberar(void) {
  if (!pedProxT && !pedProxE) trocaEpisodio = 0;
}
// A regra mora em prox_indice_seguinte (proximo.h), a mesma do cartao.
const CatEp *player_proximo_episodio(void) {
  int ix=idxAtual(),n=cat_n_episodios(ix),i;
  const CatEp *base=n>0?cat_episodio(ix,0):NULL;
  if(!base)return NULL;
  i=prox_indice_seguinte(base,n,epT,epE);
  return i>=0?cat_episodio(ix,i):NULL;
}
// O MOTIVO DO CARTAO DE ERRO (issue #112). O cartao dizia sempre "Nao foi
// possivel abrir a fonte / Abra Fontes para escolher outra opcao" — e para um
// canal sem fonte nenhuma as duas frases sao falsas: nada foi aberto, e a
// folha de Fontes estaria vazia. No log do #112 (Samsung, id 1647) foram oito
// canais seguidos em "nenhuma fonte serve", e o relato e "tela preta". Com o
// motivo, o cartao diz QUEM respondeu o que ("FrostView TV nao tem fonte para
// este canal agora"). Vazio = as frases genericas. Ja traduzido por quem
// chama; txt_linha tenta traduzir de novo, nao acha chave e deixa como esta.
static char erroTitulo[160], erroDica[160];
#ifdef NV_TOUCH_UI
static int (*modalToqueGuarda)(void);
static int avToqueQuadro, avToqueErro, avToqueElegivel;
static char avToqueCanal[sizeof itemCanal.imdb];
void player_toque_modal_guarda(int (*pode)(void)) { modalToqueGuarda = pode; }
static int modalToquePode(void) { return !modalToqueGuarda || modalToqueGuarda(); }
#endif
// "Fonte 2 de 3": a tentativa do automatico VOD (app.c, tentarProximaFonteVOD).
static int tentativaN, tentativaM;
static int durFonteAvisada, durFontePulou, pedProximaFonte, durFonteForaCache;
// O arquivo e bem MAIS CURTO que o titulo (metade ou menos): pode ser uma
// parte de um filme em partes (partes.h). app.c procura as outras.
static int durFonteCurta;
// Cache da duracao esperada da fonte corrente (achado 4 do review #205):
// duracaoFonteEsperada varre os episodios do titulo, e durFonteAtualizar roda
// por quadro. Recalcula so quando o conteudo dos episodios muda (a primeira
// lista — e o metadado que preenche a duracao — chega DEPOIS da fonte abrir,
// sem zerar nada: cat_versao_episodios sobe) ou o catalogo inteiro troca.
static int durFonteEsperadaOk;
static double durFonteEsperadaCache;
static unsigned durFonteEsperadaVer, durFonteEsperadaRev;
static Uint32 durFonteAcaoAte;
// O PULO DO AUTOMATICO ESPERA (duracaook.h): o aviso mostra "Manter esta fonte
// (OK)" e a contagem; vencida, pula. durFonteManter = o OK do aviso mantem
// (em vez de "Tentar proxima fonte").
#define DUR_CONTAGEM_MS 8000u
static Uint32 durFonteSkipEm;
static int durFonteManter;
// ABRINDO A FONTE, COMPACTO POR PADRAO (dono, 05/10): o cartao nasce enxuto —
// o ponto, o estado e as marcas da fonte — e o BAIXO expande os detalhes.
// abrindoExp e o pedido (BAIXO liga, CIMA desliga); abrindoT (0..1) e a mola
// que faz o conteudo cruzar junto com o crescimento do cartao.
static int abrindoExp;
static float abrindoT;
static Uint32 abrindoDesde, abrindoAgora, abrindoUlt;
static int abrindoViva;   // o quadro anterior estava abrindo
static float abrindoFundo;   // o preto da abertura, que sai junto com o cartao
static Uint32 abrindoFundoUlt;
// A TELA CINEMA da abertura (#205, abrecine.h): no lugar do cartao, por padrao.
static AbreCine cine;
// O botao em foco no modal do erro: 0 = Abrir Fontes, 1 = Voltar.
static int erroBotao;
static void erroEscolher(int i) {
#ifdef NV_ANDROID
  (void)i;
  player_voltar_a_esperar(); pedFontes = 1; // Abrir Fontes e Voltar dispensam o erro (#409)
#else
  if (i == 0) pedFontes = 1;
  else { saindo = 1; pediuSair = 1; }
#endif
}
void player_definir_tentativa(int n, int max) { tentativaN = n; tentativaM = max; }
#ifdef NV_TPK
int video_tpk_convertendo(void);   // video_tpk.c
int video_tpk_aviso_busca(void);   // video_tpk.c (mkvfix.h)
#endif
// DE QUEM E O PROBLEMA (#205). Sem motivo proprio, o cartao diz o que o erro do
// player ja deixa saber: servidor da fonte recusou (add-on/debrid), a conexao
// nao chegou, ou formato/decoder (esse sim pode ser app ou TV). Nunca a URL.
static void erroCausaDoPlayer(void) {
  int http = 0, causa = fontesonda_causa(video_erro_texto(), &http);
  const char *err = video_erro_texto();
  if (xtream_url_limitada(video_url_atual()) && err &&
      (strstr(err, "403") || strstr(err, "458") || strstr(err, "509"))) {
    snprintf(erroTitulo, sizeof erroTitulo, "%s", i18n("Esta linha está em uso em outra tela"));
    snprintf(erroDica, sizeof erroDica, "%s", i18n("Feche o Xtream em outro aparelho e tente de novo."));
    return;
  }
  if (causa == FS_CAUSA_NENHUMA) return;
  printf("[fonte] causa do erro: %d http=%d\n", causa, http);
  fflush(stdout);
  if (causa == FS_CAUSA_HTTP) {
    if (http) snprintf(erroTitulo, sizeof erroTitulo, i18n("O servidor da fonte recusou o link (HTTP %d)."), http);
    else snprintf(erroTitulo, sizeof erroTitulo, "%s", i18n("O servidor da fonte recusou o link."));
    snprintf(erroDica, sizeof erroDica, "%s", i18n("O problema é do add-on ou do debrid, não do app. Abra Fontes e tente outra fonte."));
  } else if (causa == FS_CAUSA_REDE) {
    snprintf(erroTitulo, sizeof erroTitulo, "%s", i18n("Sua conexão não chegou até a fonte."));
    snprintf(erroDica, sizeof erroDica, "%s", i18n("Confira a internet da TV e tente de novo, ou escolha outra fonte."));
  } else {
    snprintf(erroTitulo, sizeof erroTitulo, "%s", i18n("A TV não conseguiu tocar o formato desta fonte."));
    snprintf(erroDica, sizeof erroDica, "%s", i18n("Pode ser a fonte, o app ou a TV. Tente outra fonte; se repetir, envie o registro em Ajustes."));
  }
}
void player_erro_fonte(void) {
#ifdef NV_TPK
  // O fluxo DTS/TrueHD convertido nao fica tocando atras do cartao de erro (e da
  // tela inicial depois dele). So convertendo: o resto segue como antes.
  if (comVideo && video_tpk_convertendo()) {
    printf("[dts-tpk] erro de fonte na tela: o fluxo convertido para\n");
    fflush(stdout);
    video_parar();
  }
#endif
  esperandoFonte = 0; erroFonte = 1; visivel = 0; tocando = 0; soBarra = 0;
  erroBotao = 0;
  erroTitulo[0] = erroDica[0] = 0;   // erro sem motivo nao herda o do anterior
  erroCausaDoPlayer();
}
void player_erro_fonte_motivo(const char *titulo, const char *dica) {
  player_erro_fonte();
  snprintf(erroTitulo, sizeof erroTitulo, "%s", titulo ? titulo : "");
  snprintf(erroDica, sizeof erroDica, "%s", dica ? dica : "");
}
// O VIDEO NAO ABRIU. Quase sempre e a fonte; mas quando o hub LS2 negou o
// registro (registros 1720-1774: PERMISSION em toda sessao) nenhuma fonte
// abriria, e "nao foi possivel abrir a fonte" mandava a pessoa trocar de fonte
// a toa. A dica diz "pode": que reiniciar/reinstalar resolve nao foi provado.
static void erroSemVideo(void) {
  if (video_registro_negado())
    player_erro_fonte_motivo(i18n("A TV não deixou o app usar o player de vídeo"),
        i18n("Reiniciar a TV ou reinstalar o app pode resolver."));
  else player_erro_fonte();
}
// Desfaz o de cima quando a fonte que parecia morta volta a entregar. Ver a
// nota no watchdog de canal em app.c.
void player_limpar_erro_fonte(void) { if (erroFonte) { erroFonte = 0; tocando = 1; } }
// Leitura do estado para o watchdog de canal do app.c: uma fonte ao vivo que
// falhou (ou nao abre no prazo) deve trocar para a proxima da lista sozinha.
int  player_fonte_falhou(void) { return erroFonte; }
// O pipeline de video e COMPARTILHADO: o trailer do detalhe toca por ele
// tambem. "O video esta entregando" so desmente o cartao de erro se o video
// que entrega foi aberto POR ESTA SESSAO do player — ver app.c.
int  player_tem_video(void) { return comVideo && !retido; }
// Arma DEPOIS de player_abrir + player_definir_episodio: daqui em diante a
// sessao ignora o ponto salvo, inclusive nas re-chamadas tardias de
// player_definir_episodio. O progresso gravado NAO e apagado — comecar do
// zero nao desmarca nada (mesma regra do web: startOver so pula o seek).
double player_regra_retomada_inicial(double posSalva, double durSalva,
                                     int percentual, int concluido) {
  double pct;
  if (percentual <= 0 || percentual >= concluido ||
      !isfinite(posSalva) || !isfinite(durSalva) || durSalva <= 1.0 ||
      posSalva <= 0.0 || posSalva >= durSalva || posSalva > 2147483.647)
    return 0.0;
  pct = posSalva * 100.0 / durSalva;
  // Catalogo guarda percentual inteiro. Uma discrepancia maior que o
  // arredondamento e dado velho/novo: espera a duracao do pipeline.
  return fabs(pct - percentual) < 1.0 ? posSalva : 0.0;
}
void player_do_inicio(void) {
  retomarFonteSeg = -1.0;
  semRetomada = 1; retomarPct = 0; baseTocou = 0;
#ifdef NV_ANDROID
  retomarSeg = 0.0;
#endif
}
int player_sem_retomada(void) { return semRetomada; }
void player_definir_episodio(int t, int e) {
  if (t >= 0 && e > 0) { epPedidoT = t; epPedidoE = e; }
  atualizarTipoItem();
  const CatItem *c = item();
  if (t != epT || e != epE) {
    retomarFonteSeg = -1.0;
    memset(&scrobbleAlvo, 0, sizeof scrobbleAlvo); scrobbleAlvoPronto = 0;
    scrobbleTvdbCapturado = 0;
  }
  epT = t; epE = e; linhaEp[0] = 0;
  // Capture exact identity while the pair still belongs to the provisional
  // list. A later final list may renumber this same episode and remove the old
  // pair; TVDB is the bridge that remains valid across that replacement.
  if (!scrobbleTvdbCapturado && c && !strcmp(c->tipo, "series") && t >= 0 && e > 0 &&
      cat_episodio_tvdb_qualquer)
    scrobbleTvdbCapturado = cat_episodio_tvdb_qualquer(idxAtual(), c->imdb, t, e);
  audioTitulo();
  retomarPct = 0;
  // !canalSessao: canal nao tem retomada, e se o indice ja foi remapeado o
  // "progresso" lido ali seria de outro titulo qualquer.
  if (c && !canalSessao && !semRetomada && c->progresso > 0 && c->progresso < ajustes_cw_concluido() &&
      (strcmp(c->tipo,"series") || (t==c->temporada && e==c->episodio) ||
       (c->tvdbEp > 0 && cat_episodio_tvdb &&
        cat_episodio_tvdb(idxAtual(), c->imdb, t, e) == c->tvdbEp))) retomarPct=c->progresso;
#ifdef NV_ANDROID
  retomarSeg = 0.0;
  if (c && retomarPct > 0) {
    char chave[48]; ProgRegistro r;
    int serie = !strcmp(c->tipo, "series");
    prog_chave(chave, sizeof chave, c->imdb, serie ? t : 0, serie ? e : 0);
    if (prog_por_chave(chave, &r))
      retomarSeg = player_regra_retomada_inicial(r.posSeg, r.durSeg,
                                                retomarPct, ajustes_cw_concluido());
  }
#endif
  if (t != baseT || e != baseE) {
    ProgRegistro r;
    char chave[48];
    baseT = t; baseE = e; basePct = retomarPct; baseTocou = 0;
    if (c && retomarPct > 0) {
      int serie = !strcmp(c->tipo, "series");
      prog_chave(chave, sizeof chave, c->imdb, serie ? t : 0, serie ? e : 0);
      if (prog_por_chave(chave, &r)) { basePos = r.posSeg; baseDur = r.durSeg; baseTocou = r.tocou; }
      else { basePos = baseDur = 0.0; baseTocou = -1; }
    }
  }
  // FILME TAMBEM PEDE MARCADOR, e ate agora nao pedia: esta linha desligava o
  // modulo e voltava. Fazia sentido enquanto a fonte era o api.introdb.app, que
  // e indexado por episodio; o TheIntroDB responde por imdb sozinho e devolve os
  // creditos do filme (ver intro.h). Sem isto, filme so tinha o capitulo do
  // Matroska — e num MP4, nada.
  if (!c) { epT = epE = 0; intro_desligar(); return; }
  if (strcmp(c->tipo, "series")) {
    // Tipo incerto ("anime") guarda o episodio pedido. Zerar aqui fazia o
    // scrobble e o Continuar assistindo sairem como filme (#524). Filme
    // confirmado, e fonte aberta sem episodio, continuam sem T/E.
    if (t >= 0 && e > 0 && strcmp(c->tipo, "movie") && !fonteSemEpisodio) {
      epPedidoT = t; epPedidoE = e;
      return;
    }
    epT = epE = 0;
    if (idx != introIdx || introT || introE) {
      int tt, te;
      introIdx = idx; introT = introE = 0;
      introTmdb = tmdbDoTocando(&tt, &te);
      intro_pedir_ids(c->imdb, introTmdb, 0, 0, 0.0, 0.0);
      credAvisado = credFimAvisado = semProxAvisado = 0; credAvisadoEm = credAvisadoDur = 0;
    }
    return;
  }
  // An unnumbered file must not inherit the catalog episode.
  if (t < 0 && e < 0) {
    epPedidoT = epPedidoE = -1;
    intro_desligar();
    return;
  }
  // 0/0 e sem episodio; 0/E e um especial escolhido, nunca T1E.
  if (epT < 0 || (epT == 0 && epE < 1))
    epT = c->temporada > 0 || (c->temporada == 0 && c->episodio > 0) ? c->temporada : 1;
  if (epE < 1) epE = c->episodio > 0 ? c->episodio : 1;
  // O default antigo de uma serie JA conhecida continua valido. Tipo
  // incerto so chega aqui depois de um pedido positivo de T/E confirmado.
  epPedidoT = epT; epPedidoE = epE;
  // AS FONTES SAO DESTE EPISODIO, E SO DELE — issue #101.
  //
  // A lista de streams.c e uma so, global, e nada a invalidava ao trocar de
  // episodio: descer do E5 para o E6 no carrossel reproduzia a fonte do E5, e
  // a folha de "Fontes" dentro do player listava as do E5. O unico caminho que
  // acertava era o hold-OK no card, que refaz a busca antes de abrir a folha —
  // e por isso o relator (#101) descreveu esse como o jeito de "confirmar".
  //
  // ESTA FUNCAO E O FUNIL: todo caminho que troca de episodio passa por ela
  // (folha de episodios, proximo automatico, card do detalhe, CW). Descartar
  // aqui vale para os quatro de uma vez, em vez de uma guarda por chamador.
  //
  // SO QUANDO A LISTA E DE OUTRO ALVO. A busca que o detalhe ja fez para ESTE
  // episodio continua valendo; jogar fora sempre custaria uma busca a mais (e
  // segundos de tela de carregamento) em toda reproducao que ja estava certa.
  // E esta funcao e re-chamada por quadro enquanto o nome do episodio nao
  // chega (player_atualizar), entao "sempre" seria a cada quadro.
  if (c->imdb[0]) {
    char alvo[64];
    cat_id_stream(idxAtual(), epT, epE, alvo, sizeof alvo);
    if (!abrindoSessao && !aberto && stream_n() > 0 && !stream_lista_do_alvo(alvo))
      stream_invalidar("episode changed");
  }
  // T/E vira S/E em ingles, e a frase montada nao casa com chave nenhuma:
  // i18n vai no FORMATO. Ver idioma.h e o issue #12.
  snprintf(linhaEp, sizeof linhaEp, i18n("T%dE%d"), epT, epE);
  if (epT == c->temporada && epE == c->episodio && c->nomeEpisodio[0])
    snprintf(linhaEp, sizeof linhaEp, i18n("T%dE%d · %s"), epT, epE, c->nomeEpisodio);
  for (int ix = idxAtual(), i = 0; i < cat_n_episodios(ix); i++) {
    const CatEp *ep = cat_episodio(ix, i);
    if (ep && ep->temporada == epT && ep->episodio == epE) {
      snprintf(linhaEp, sizeof linhaEp, i18n("T%dE%d · %s"), epT, epE, ep->nome);
      break;
    }
  }
  if(idx!=introIdx||epT!=introT||epE!=introE){
    introIdx=idx;introT=epT;introE=epE;
    { double dA=0,dP=0;
      for(int ix=idxAtual(),i=0;i<cat_n_episodios(ix);i++){
        const CatEp *ep=cat_episodio(ix,i);
        if(!ep||ep->temporada!=epT)continue;
        if(ep->episodio==epE-1)dA=duracaoTexto(ep->duracao);
        else if(ep->episodio==epE+1)dP=duracaoTexto(ep->duracao);
      }
      { int tt, te; introTmdb = tmdbDoTocando(&tt, &te);
        if (introTmdb > 0) intro_pedir_ids_mapeados(c->imdb, introTmdb, tt, te, epT, epE, dA, dP);
        else intro_pedir_ids(c->imdb, 0, epT, epE, dA, dP); } }
    credAvisado=credFimAvisado=semProxAvisado=0;credAvisadoEm=credAvisadoDur=0;
  }
}

// --- duracao a partir do texto livre do catalogo -----------------------------
// O campo `meta` e prosa, nao dado: "2023 · 3 h 28 min" num filme e
// "2022 · 3 temporadas" numa serie. Em vez de um parser posicional (que quebra
// no primeiro titulo com formato diferente), procuro apenas os dois pares
// numero+unidade em qualquer lugar da string. Nao achando NENHUM dos dois,
// devolvo 0 e quem chama cai no padrao — que e o caso correto para series.
static float duracaoDeMeta(const char *meta) {
  if (!meta) return 0.0f;
  float h = 0.0f, m = 0.0f;
  int achou = 0;
  for (const char *p = meta; *p; p++) {
    if (*p < '0' || *p > '9') continue;
    float v = 0.0f;
    while (*p >= '0' && *p <= '9') { v = v * 10.0f + (*p - '0'); p++; }
    while (*p == ' ') p++;
    // "min" tem que ser testado ANTES de "m": senao todo "min" vira minuto por
    // acidente do prefixo — o que ate daria certo aqui, mas escondia o bug do
    // dia em que aparecer uma unidade nova comecando com m.
    if (!strncmp(p, "min", 3))    { m = v; achou = 1; p += 2; }
    else if (*p == 'h')           { h = v; achou = 1; }
    if (!*p) break;
  }
  return achou ? (h * 3600.0f + m * 60.0f) : 0.0f;
}

// Corta a sinopse na primeira frase, sem passar de `maxBytes`. O corte respeita
// UTF-8: os titulos do catalogo sao em portugues e cortar no meio de um "ç" ou
// "ã" produz um retangulo vazio na fonte, nao um acento faltando.
static void frasePrimeira(char *dst, size_t n, const char *src, size_t maxBytes) {
  if (!src || !*src) { dst[0] = 0; return; }
  if (maxBytes > n - 4) maxBytes = n - 4;
  size_t i = 0, corte = 0;
  for (; src[i] && i < maxBytes; i++)
    if (src[i] == '.') { corte = i; break; }
  if (!corte) {
    corte = i;
    // volta ate o inicio de um caractere (bytes de continuacao sao 10xxxxxx)
    while (corte > 0 && ((unsigned char)src[corte] & 0xC0) == 0x80) corte--;
    while (corte > 0 && src[corte - 1] == ' ') corte--;
  }
  memcpy(dst, src, corte);
  dst[corte] = 0;
  if (src[i] && src[i] != '.') strncat(dst, "\xe2\x80\xa6", n - strlen(dst) - 1);
}

// --- MODOS DE PROPORCAO ------------------------------------------------------
// A porta do web para o nativo. No web o modo mexe em duas coisas do elemento
// <video>: o `object-fit` e um `transform: scale()`. Aqui nao ha elemento — ha
// um plano de hardware posicionado por video_janela() — entao os dois viram UMA
// coisa so: o retangulo do plano.
//
// A traducao e literal e nesta ordem, igual ao resolveAspectRender do web:
//   1. o retangulo que o object-fit do modo produziria (contain/cover/fill);
//   2. multiplicado pela escala do modo (resolveAspectScale), em torno do
//      CENTRO da tela — que e o `transform-origin: center center` de la.
// O retangulo aqui e VIRTUAL: ele pode sair da tela, e sair da tela e o que
// significa "recortar". Mas ele NAO e o que se manda ao plano — ver
// aplicarAspecto, que o converte em fonte + destino.
//
// ERRO MEDIDO, e vale ficar escrito porque a leitura do web induz a ele: eu
// mandava este retangulo direto ao ACB, com x/y negativos e tamanho maior que a
// tela. O ACB aceitou as quatro chamadas sem reclamar e o log ficou bonito —
//   [video] janela -144,-81  2208x1242 cheia=0   <- Zoom leve   (1.15)
//   [video] janela -326,-184 2573x1447 cheia=0   <- Zoom cinema (1.34)
//   [video] janela -528,-297 2976x1674 cheia=0   <- Zoom ultra  (1.55)
// — batendo ate o pixel com o resolveAspectRender do web. E a TELA FICOU PRETA
// em todos os tres. Aceitar a chamada nao e exibir: um plano de hardware nao
// descarta o excedente como o compositor do navegador faz com transform:
// scale(), entao retangulo fora do painel nao vira recorte, vira retangulo
// invalido e o plano apaga. So o ORIGINAL mostrava imagem, por ser o unico com
// escala 1. A licao: `resolveAspectScale` era justamente a parte do web que NAO
// se traduz, porque a metade que fazia o recorte no web nem esta no arquivo.
//
// NAO da para conferir isto por captura de tela: durante a reproducao o
// /tmp/nuvio-shot.bmp sai PRETO onde esta o video, porque o plano fica atras da
// superficie GL e o glReadPixels nao o enxerga. E foi essa cegueira que deixou
// o erro passar — o log dizia sucesso, a captura era preta de qualquer jeito, e
// so quem olhou a TV viu. Conferir zoom exige olhar o aparelho.
static int    aspecto = PLR_ASP_ORIGINAL;
static Uint32 toastAte = 0;      // ate quando o aviso de modo fica de pe
// Texto do aviso quando NAO e o modo de proporcao (vazio = rotulo do modo).
// Hoje so o audio nao suportado usa: um aviso por fonte, ver player_atualizar.
static char   toastTexto[320];
// O icone do aviso (art/icones) e a cor dele: 0 branco, 1 ambar (aviso).
static char   toastIcone[32];
static int    toastCor;
static int    avisouAudio;
static char   dirPrefs[512];
static int    prefsLidas;

// Rotulos em portugues. Os do web sao "Fit (Original)", "Crop", "Stretch",
// "Slight/Cinema/Ultra Zoom", "Fit Height", "Fit Width" — o resto do app fala
// portugues, entao traduzir aqui e o que mantem a tela coerente.
static const char *ASP_ROTULO[PLR_ASP_N] = {
  "Original", "Recortar", "Esticar", "Zoom leve",
  "Zoom cinema", "Zoom ultra", "Ajustar altura", "Ajustar largura"
};

const char *player_aspecto_rotulo(int modo) {
  if (modo < 0 || modo >= PLR_ASP_N) modo = PLR_ASP_ORIGINAL;
  return ASP_ROTULO[modo];
}
int player_aspecto(void) { return aspecto; }

// Onde o modo escolhido fica gravado. ERA SDL_GetBasePath()+"art" direto — a
// pasta de ARTE (cache de poster/backdrop), nao a de DADOS. No Mac as duas
// convivem no mesmo lugar e ninguem notava; no aparelho de verdade sao
// diretorios diferentes (ver dados_iniciar/dados_dir), e so o de DADOS e o que
// main.c trata como persistente — e o que ajustes_dir tambem recebe. Gravar
// legenda/aspecto na pasta errada e o tipo de bug que some ao reabrir o app: a
// TV mata o processo (nunca ha saida limpa), e o que sobrevive e so o que foi
// escrito no diretorio de dados de verdade. ISSUE #42: "Settings are not
// getting saved (player caption settings)".
//
// player_dir() e chamada por main.c com dados_dir() (o mesmo valor que
// ajustes_dir recebe); SDL_GetBasePath()+"art" continua como ultimo recurso
// para quem chama player_abrir sem ter passado por main.c (testes).
void player_dir(const char *dir) {
  if (!dir || !*dir) return;
  snprintf(dirPrefs, sizeof dirPrefs, "%s", dir);
  prefsLidas = 0;
  legfontes_dir(dir);
}
static const char *prefsArquivo(void) {
  static char caminho[600];
  if (!dirPrefs[0]) {
    char *base = SDL_GetBasePath();
    if (base) { snprintf(dirPrefs, sizeof dirPrefs, "%sart", base); SDL_free(base); }
    else      snprintf(dirPrefs, sizeof dirPrefs, "/tmp/art");
  }
  snprintf(caminho, sizeof caminho, "%s/player.txt", dirPrefs);
  return caminho;
}

// ESTILO DA LEGENDA: preferencia DO APARELHO, como o aspecto — nao vai em
// ajustes.txt, que espelha as chaves de layout do app web. Padrao: tamanho 2
// (o do aparelho), branco, sem fundo, posicao central, contorno.
// -1 preserves the legacy outline preference until a border colour is chosen.
static VideoLegendaEstilo legEstilo = { 120, 0, 0, 3, 1, 0, 0, TXT_FAMILIA_INTER, 0, -1 };
// Ver player_leg_estilo_tocou, em player.h: o que a pessoa mexeu vence o que o
// arquivo ASS pede. Persistido junto com o resto — a preferencia nao pode
// valer so ate desligar a TV.
static int legTocado = PLR_LEG_NADA;

static int prefsGravar(void);
static void prefsLer(void) {
  prefsLidas = 1;
  legEstilo.corBorda = -1;
  FILE *f = fopen(prefsArquivo(), "r");
  char chave[64]; int v, sinalNovo = 0, temAtraso = 0, bordaExplicita = 0;
  if (!f) return;
  while (fscanf(f, "%63s %d", chave, &v) == 2) {
    // Valor de outra versao (ou arquivo editado a mao) cai no padrao em vez de
    // indexar fora do vetor de rotulos.
    if (!strcmp(chave, "aspecto") && v >= 0 && v < PLR_ASP_N) aspecto = v;
    else if (!strcmp(chave, "leg_tamanho")) {
      /* Migra o arquivo antigo 0..4 sem perder a preferencia do aparelho. */
      static const int antigo[5]={60,80,120,160,200};
      if(v>=0&&v<=4)legEstilo.tamanho=antigo[v];
      else if(v>=TXT_LEG_PCT_MIN&&v<=TXT_LEG_PCT_MAX)legEstilo.tamanho=(v/10)*10;
    }
    else if (!strcmp(chave, "leg_cor")     && v >= 0 && v < VIDEO_LEG_NCORES) legEstilo.cor = v;
    else if (!strcmp(chave, "leg_fundo")   && v >= 0 && v <= 4)  legEstilo.fundo = v;
    else if (!strcmp(chave, "leg_pos")     && v >= 0 && v <= 7)  legEstilo.posicao = v;
    else if (!strcmp(chave, "leg_borda")   && v >= 0 && v <= 2)  legEstilo.borda = v;
    else if (!strcmp(chave, "leg_atraso")  && v >= -LEG_ATRASO_MANUAL_MAX && v <= LEG_ATRASO_MANUAL_MAX) { legEstilo.atrasoMs = v; temAtraso = 1; }
    else if (!strcmp(chave, "leg_atraso_sinal") && v == 1) sinalNovo = 1;
    else if (!strcmp(chave, "leg_cor_borda") && v >= -1 && v < VIDEO_LEG_NCORES) legEstilo.corBorda = v;
    else if (!strcmp(chave, "leg_cor_borda_explicita") && v == 1) bordaExplicita = 1;
    else if (!strcmp(chave, "leg_opacidade") && v >= 0 && v <= 3) legEstilo.opacidade = v;
    else if (!strcmp(chave, "leg_familia") && v >= 0 && v < TXT_FAMILIA_N) legEstilo.familia = v;
    else if (!strcmp(chave, "leg_negrito") && (v == 0 || v == 1)) legEstilo.negrito = v;
    else if (!strcmp(chave, "leg_tocado")  && v >= 0) legTocado = v;
  }
  fclose(f);
  // Older players wrote default Black even when no border colour was chosen.
  if (!bordaExplicita && legEstilo.corBorda == 0 && !(legTocado & PLR_LEG_COR_BORDA))
    legEstilo.corBorda = -1;
  // One-time migration: leg_atraso saved before the sign fix (positive =
  // later) was tuned under the inverted sign. Negate once; the marker written
  // by prefsGravar keeps this from running again.
  if (!sinalNovo && temAtraso) { legEstilo.atrasoMs = -legEstilo.atrasoMs; prefsGravar(); }
}

static int prefsGravar(void) {
  char destino[600], temporario[608];
  snprintf(destino, sizeof destino, "%s", prefsArquivo());
  snprintf(temporario, sizeof temporario, "%s.tmp", destino);
  FILE *f = fopen(temporario, "w");
  if (!f) return 0;
  fprintf(f, "aspecto %d\n", aspecto);
  fprintf(f, "leg_tamanho %d\n", legEstilo.tamanho);
  fprintf(f, "leg_cor %d\n",     legEstilo.cor);
  fprintf(f, "leg_fundo %d\n",   legEstilo.fundo);
  fprintf(f, "leg_pos %d\n",     legEstilo.posicao);
  fprintf(f, "leg_borda %d\n",   legEstilo.borda);
  fprintf(f, "leg_atraso %d\n",  legEstilo.atrasoMs);
  fprintf(f, "leg_atraso_sinal 1\n");
  fprintf(f, "leg_opacidade %d\n", legEstilo.opacidade);
  fprintf(f, "leg_familia %d\n", legEstilo.familia);
  fprintf(f, "leg_negrito %d\n", legEstilo.negrito);
  fprintf(f, "leg_tocado %d\n", legTocado);
  fprintf(f, "leg_cor_borda %d\n", legEstilo.corBorda);
  fprintf(f, "leg_cor_borda_explicita 1\n");
  int erro = ferror(f);
  if (fclose(f)) erro = 1;
  if (erro || rename(temporario, destino)) { remove(temporario); return 0; }
  dados_marcar_sujo(0);   // IDBFS (Samsung): ver o gravador de ajustes.c
  return 1;
}

// Lidos pela folha de faixas, que e quem desenha os controles.
VideoLegendaEstilo *player_leg_estilo(void) { return &legEstilo; }
void player_leg_estilo_carregar(void) { if (!prefsLidas) prefsLer(); }
void player_leg_estilo_mudou(void) {
  video_legenda_estilo(&legEstilo);
  if (prefsGravar()) sync_proteger_ajustes_locais();
}
const VideoLegendaEstilo *player_leg_estilo_padrao(void) {
  static const VideoLegendaEstilo padrao = {120, 0, 0, 3, 1, 0, 0, TXT_FAMILIA_INTER, 0, 0};
  return &padrao;
}
static int estiloPreparar(const VideoLegendaEstilo *estilo, int tocou) {
  if (!estilo || estilo->tamanho < TXT_LEG_PCT_MIN || estilo->tamanho > TXT_LEG_PCT_MAX ||
      estilo->tamanho % 10 || estilo->cor < 0 || estilo->cor >= VIDEO_LEG_NCORES ||
      estilo->corBorda < -1 || estilo->corBorda >= VIDEO_LEG_NCORES ||
      estilo->familia < 0 || estilo->familia >= TXT_FAMILIA_N ||
      estilo->fundo < 0 || estilo->fundo > 4 || estilo->posicao < 0 || estilo->posicao > 7 ||
      estilo->borda < 0 || estilo->borda > 2 || estilo->opacidade < 0 || estilo->opacidade > 3 ||
      estilo->negrito < 0 || estilo->negrito > 1 ||
      estilo->atrasoMs < -LEG_ATRASO_MANUAL_MAX || estilo->atrasoMs > LEG_ATRASO_MANUAL_MAX) return 0;
  player_leg_estilo_carregar();
  VideoLegendaEstilo antes = legEstilo;
  legEstilo = *estilo;
  if (tocou != PLR_LEG_NADA) {
    if (antes.cor != estilo->cor) legTocado |= PLR_LEG_COR;
    if (antes.familia != estilo->familia) legTocado |= PLR_LEG_FONTE;
    if (antes.corBorda != estilo->corBorda) legTocado |= PLR_LEG_COR_BORDA;
    if (antes.borda != estilo->borda) legTocado |= PLR_LEG_BORDA;
    if (antes.fundo != estilo->fundo) legTocado |= PLR_LEG_FUNDO;
    if (antes.posicao != estilo->posicao) legTocado |= PLR_LEG_POSICAO;
    if (antes.negrito != estilo->negrito) legTocado |= PLR_LEG_NEGRITO;
  }
  if (tocou >= 0) player_leg_estilo_tocou(tocou);
  return 1;
}
typedef struct { const VideoLegendaEstilo *novo; int tocou; } EstiloCommit;
static int estiloConfirmar(void *u) {
  EstiloCommit *c = u;
  VideoLegendaEstilo antes = legEstilo;
  int tocadoAntes = legTocado;
  if (!estiloPreparar(c->novo, c->tocou)) return 0;
  if (!prefsGravar()) { legEstilo = antes; legTocado = tocadoAntes; return 0; }
  return 1;
}
int player_leg_estilo_definir(const VideoLegendaEstilo *estilo, int tocou) {
  player_leg_estilo_carregar();
  EstiloCommit c = { estilo, tocou };
  if (!estiloConfirmar(&c)) return 0;
  video_legenda_estilo(&legEstilo);
  return 1;
}
// A bundled-font selection and Restore also clear the device-local override.
// Keep both persisted sources unchanged if either write fails.
static int estiloSemFonteLocal(const VideoLegendaEstilo *novo, int tocou) {
  player_leg_estilo_carregar();
  EstiloCommit c = { novo, tocou };
  if (!legfontes_escolher_transacao(0, -1, estiloConfirmar, &c)) return 0;
  video_legenda_estilo(&legEstilo);
  return 1;
}
int player_leg_fonte_definir(int familia) {
  player_leg_estilo_carregar();
  VideoLegendaEstilo novo = legEstilo;
  novo.familia = familia;
  return estiloSemFonteLocal(&novo, PLR_LEG_FONTE);
}
int player_leg_fonte_local_definir(int indice) {
  player_leg_estilo_carregar();
  VideoLegendaEstilo novo = legEstilo;
  EstiloCommit c = { &novo, PLR_LEG_FONTE };
  if (!legfontes_escolher_transacao(0, indice, estiloConfirmar, &c)) return 0;
  video_legenda_estilo(&legEstilo);
  return 1;
}
int player_leg_estilo_restaurar(void) {
  return estiloSemFonteLocal(player_leg_estilo_padrao(), PLR_LEG_NADA);
}
void player_leg_estilo_tocou(int campos) {
  if (campos == PLR_LEG_NADA) legTocado = PLR_LEG_NADA;
  else legTocado |= campos;
}
int player_leg_estilo_tocado(int campo) { return (legTocado & campo) != 0; }

// Proporcao do QUADRO decodificado. Sem videoInfo ainda, 16:9 — que e a
// proporcao de quase todo arquivo entregue, e a suposicao que faz "Original"
// abrir em tela cheia em vez de piscar uma faixa errada por um segundo.
static float aspectoQuadro(void) {
  int w = video_largura(), h = video_altura();
  if (w > 0 && h > 0) return (float)w / (float)h;
  return NV_TELA_W / NV_TELA_H;
}

typedef struct { float x, y, w, h; } PlrRect;
static PlrRect miniDestino(void);
// ANIMACAO DA JANELA DE VIDEO entre dois retangulos (tela cheia <-> preview
// do guia). Em degraus espacados, como o recuo do painel de creditos: cada
// degrau e uma mensagem ao pipeline, entao ha PLR_ENC_PASSOS delas em
// ~450 ms, e nao uma por quadro. Enquanto anima, aplicarAspecto nao mexe no
// plano — o fim da animacao e quem entrega o destino definitivo.
static int     janAtiva;
static float   janT;
static Uint32  janEm;
static PlrRect janDe, janPara, janAgora;

static PlrRect aspectoRect(int modo) {
  const float tela = NV_TELA_W / NV_TELA_H;
  float q = aspectoQuadro();
  float bw, bh, sx = 1.0f, sy = 1.0f;
  PlrRect r;
  if (q <= 0.0f) q = tela;

  // 1) o object-fit do modo. Os tres casos sao os do ASPECT_MODE_DEFINITIONS.
  switch (modo) {
    case PLR_ASP_ESTICAR:                       // fill
      bw = NV_TELA_W; bh = NV_TELA_H;
      break;
    case PLR_ASP_CROP:                          // cover
    case PLR_ASP_ZOOM_LEVE:
    case PLR_ASP_ZOOM_CINEMA:
    case PLR_ASP_FIT_ALTURA:
      if (q > tela) { bh = NV_TELA_H; bw = bh * q; }
      else          { bw = NV_TELA_W; bh = bw / q; }
      break;
    default:                                    // contain
      if (q > tela) { bw = NV_TELA_W; bh = bw / q; }
      else          { bh = NV_TELA_H; bw = bh * q; }
      break;
  }

  // 2) a escala do modo, copiada linha a linha do resolveAspectScale.
  switch (modo) {
    case PLR_ASP_CROP:        sx = sy = (q > tela) ? q / tela : tela / q; break;
    case PLR_ASP_ESTICAR:     if (q > tela) sy = q / tela; else sx = tela / q; break;
    case PLR_ASP_ZOOM_LEVE:   sx = sy = PLR_ZOOM_LEVE;   break;
    case PLR_ASP_ZOOM_CINEMA: sx = sy = PLR_ZOOM_CINEMA; break;
    case PLR_ASP_ZOOM_ULTRA:  sx = sy = PLR_ZOOM_ULTRA;  break;
    case PLR_ASP_FIT_ALTURA:  if (q > tela) sx = sy = q / tela; break;
    case PLR_ASP_FIT_LARGURA: if (q < tela) sx = sy = tela / q; break;
    default: break;   // ORIGINAL: contain e nada mais
  }

  r.w = bw * sx;
  r.h = bh * sy;
  r.x = (NV_TELA_W - r.w) * 0.5f;
  r.y = (NV_TELA_H - r.h) * 0.5f;
  return r;
}

// O retangulo VISIVEL do modo: o retangulo virtual cortado pela tela. E ele que
// o furo do GL segue e que vira o destino do plano.
static PlrRect aspectoVisivel(int modo) {
  PlrRect r = aspectoRect(modo), d;
  d.x = r.x < 0.0f ? 0.0f : r.x;
  d.y = r.y < 0.0f ? 0.0f : r.y;
  d.w = (r.x + r.w > NV_TELA_W ? NV_TELA_W : r.x + r.w) - d.x;
  d.h = (r.y + r.h > NV_TELA_H ? NV_TELA_H : r.y + r.h) - d.y;
  if (d.w < 0.0f) d.w = 0.0f;
  if (d.h < 0.0f) d.h = 0.0f;
  return d;
}

// ENCOLHER O VIDEO PARA O PAINEL DE CREDITOS.
//
// Pedido do dono, "que nem o Netflix": quando os creditos comecam, o filme
// recua para um canto e os cartoes ficam FORA da imagem, em vez de por cima
// dela. Aqui isso nao e um transform de CSS — o video e um plano de hardware
// atras da superficie GL, e recuar significa mandar ao ACB um retangulo de
// destino menor (ver a nota extensa em video_janela).
//
// NAO ANIMADO POR QUADRO, e essa e a decisao que importa: cada mudanca e uma
// chamada luna ao pipeline, e este arquivo ja registra que mandar quatro
// posicoes seguidas de seek precedeu a morte do pipeline. Sao poucos degraus,
// espacados, e o resultado le como movimento sem transformar um efeito visual
// numa enxurrada de comandos no aparelho.
// 0.52 e 48 nao sao gosto, sao a conta de caber. O painel de relacionados mede
// cabecalho (~40) + 18 + cartaz (318) + rotulo (44) = ~420. Com o video a 52%
// de 1080 e 48 de respiro no topo, ele termina em 609 e sobram 471 ate a
// margem inferior — o painel entra inteiro ABAIXO da imagem, que era o pedido.
// Subir para 0.62 devolve 346 de espaco e os cartoes voltam a cobrir o filme.
#define PLR_ENC_ALVO_100  0.52f   // fracao da tela que o video ocupa recuado
// "Tamanho da interface" (escala.h): a ilha de baixo do pos-play cresce por s e
// continua precisando dos ~470 px de 1080 em unidades dela; o video recua o que
// ela ganha. Em 100% e 0,52 exato.
#define PLR_ENC_ALVO (PLR_ENC_ALVO_100 - (gfx_escala_ui() - 1.0f) * 470.0f / 1080.0f)
#define PLR_ENC_TOPO      48.0f   // respiro acima do video quando recuado
// Mais degraus e mais curtos que a primeira versao (eram 6 x 70 ms), e com
// CURVA em vez de passo constante: o dono viu e pediu mais fluidez. O custo
// continua sendo uma chamada ao pipeline por degrau, entao a fluidez vem
// principalmente da curva — comecar e terminar devagar esconde a natureza
// discreta do movimento muito melhor que dobrar o numero de chamadas.
#define PLR_ENC_PASSOS      12
#define PLR_ENC_MS         38u    // entre um degrau e o seguinte

static float  encolhe = 1.0f;     // 1 = tela cheia
static float  encolheAlvo = 1.0f;
static float  encolheT;           // 0 = tela cheia, 1 = recuado
static Uint32 encolheEm;

// Aceleracao e desaceleracao simetricas (smoothstep). Sem ela os degraus sao
// todos do mesmo tamanho e o olho le cada um deles.
static float suaveEnc(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

// O destino do plano, ja com o recuo aplicado. Ancorado no ALTO: o painel vive
// no rodape, entao o espaco que se abre tem de ser embaixo.
static PlrRect destinoComRecuo(PlrRect d) {
  float k = encolhe;
  PlrRect o;
  if (k > 0.999f) return d;
  o.w = d.w * k;
  o.h = d.h * k;
  o.x = d.x + (d.w - o.w) * 0.5f;
  o.y = PLR_ENC_TOPO;
  if (o.y + o.h > NV_TELA_H) o.y = NV_TELA_H - o.h;
  if (o.y < 0.0f) o.y = 0.0f;
  return o;
}

// Manda o modo ao plano de hardware. Chamado na abertura, na troca de modo e
// quando o videoInfo chega — antes dele a proporcao do quadro e chute, e o modo
// calculado com o chute estaria errado justamente nos filmes widescreen, que
// sao o motivo de tudo isto existir.
//
// AQUI ESTAVA O ERRO que deixava a tela preta em todo modo com zoom. Eu mandava
// o retangulo VIRTUAL direto ao plano — com x/y negativos e tamanho maior que a
// tela — na suposicao de que o excedente sairia pela borda, como sai no web. No
// web quem descarta o excedente e o compositor do navegador; um plano de
// hardware nao tem esse passo, e retangulo fora do painel nao e recorte, e
// retangulo invalido: o plano apaga. So o ORIGINAL sobrevivia, por ser o unico
// com escala 1.
//
// A conta certa e a INVERSA: o destino nunca sai da tela, e o zoom vira um
// pedaco MENOR da FONTE. O retangulo virtual continua sendo o mesmo do web —
// ele so deixa de ser o que se manda e passa a ser o que se USA PARA CALCULAR
// que fatia do quadro cai dentro da tela.
// Definida abaixo, junto do ciclo de modos: quem aplica precisa dela antes.
static int modoPrecisaRecorte(int modo);

#ifdef NV_ANDROID
// DOLBY VISION ESCURO (2.0.3). A fonte abre com o layout NEUTRO da SurfaceView
// (quadro inteiro encaixado, nada pedido) e o modo salvo so entra DEPOIS do
// primeiro quadro + ASP_ASSENTA_MS; aplicar o recorte/zoom antes disso fazia o
// decoder/painel escolher o caminho errado (escuro) no DV. Mudanca manual
// durante a reproducao continua imediata (player_aspecto_definir libera).
#define ASP_ASSENTA_MS 300u
static int    aspPendente = 0;
#define ASP_SEGURANCA_MS 3000u
static Uint32 aspLiberarEm = 0, aspProntoDesde = 0;
static void aspArmar(void) { aspPendente = comVideo ? 1 : 0; aspLiberarEm = aspProntoDesde = 0; }
#else
#define aspPendente 0
static void aspArmar(void) {}
#endif

static void aplicarAspecto(void) {
  PlrRect r, d;
  float qw, qh;
  int sx, sy, sw, sh;
  if (!comVideo) return;
  if (janAtiva) return;

  // No PiP todo recalculo cai na miniatura — o videoInfo da fonte nova num
  // zap, por exemplo, chega DEPOIS do video_janela do canto e sem esta
  // guarda reexpandiria o plano para a tela cheia com a moldura la embaixo.
  if (mini) { PlrRect o = miniDestino();
              video_janela((int)(o.x + 0.5f), (int)(o.y + 0.5f),
                           (int)(o.w + 0.5f), (int)(o.h + 0.5f));
              return; }

#ifdef NV_TPK
  // Source lookup may still expose the previous live channel's dimensions.
  if (ehCanal() && esperandoFonte) {
    video_janela(0, 0, (int)NV_TELA_W, (int)NV_TELA_H);
    return;
  }
#endif

  // Layout neutro ate o modo salvo ser liberado (ver aspArmar).
  if (aspPendente) return;

  // VALOR HERDADO. `aspecto` e gravado por aparelho, mas um arquivo copiado —
  // ou um modo que existia numa versao anterior — pode trazer um modo que esta
  // TV nao honra. Cair no ORIGINAL e melhor que esticar a imagem: esticar muda
  // a proporcao das pessoas na tela e ninguem pediu isso.
  if (!video_recorte_fonte() && modoPrecisaRecorte(aspecto))
    aspecto = PLR_ASP_ORIGINAL;

  r = aspectoRect(aspecto);
  d = aspectoVisivel(aspecto);
  if (d.w < 1.0f || d.h < 1.0f || r.w < 1.0f || r.h < 1.0f) return;

  qw = (float)video_largura();
  qh = (float)video_altura();
  // Sem as dimensoes do quadro nao da para falar em coordenadas de fonte. Cai
  // no caminho antigo, que serve ao caso sem recorte — e o unico em que ele
  // funciona. Assim que o videoInfo chegar, aplicarAspecto roda de novo.
  if (qw < 2.0f || qh < 2.0f) {
    PlrRect o = destinoComRecuo(d);
    video_janela((int)(o.x + 0.5f), (int)(o.y + 0.5f),
                 (int)(o.w + 0.5f), (int)(o.h + 0.5f));
    return;
  }

  // Que fatia do quadro cai dentro do destino: o quadro inteiro mapeia no
  // retangulo virtual `r`, entao a fatia e a regra de tres de `d` dentro de `r`.
  sx = (int)((d.x - r.x) / r.w * qw + 0.5f);
  sy = (int)((d.y - r.y) / r.h * qh + 0.5f);
  sw = (int)(d.w / r.w * qw + 0.5f);
  sh = (int)(d.h / r.h * qh + 0.5f);
  // Par: o escalonador trabalha em 4:2:0 e origem ou tamanho impar em croma da
  // meio pixel de deslocamento de cor na borda do recorte.
  sx &= ~1; sy &= ~1; sw &= ~1; sh &= ~1;
  if (sx < 0) sx = 0;
  if (sy < 0) sy = 0;
  if (sx + sw > (int)qw) sw = (int)qw - sx;
  if (sy + sh > (int)qh) sh = (int)qh - sy;

  // A FONTE sai do `d` inteiro e o DESTINO e o recuado: e o mesmo conteudo,
  // menor. Calcular a fonte a partir do retangulo ja recuado espremeria a
  // imagem, porque o recuo mudaria a regra de tres sem mudar o quadro.
  // O LOG UMA LINHA por aplicacao e o que o aparelho devolve na duvida:
  // sem URL nem ficha, so modo + destino + recorte — a troca que nao chega
  // ao plano fica visivel no registro da TV.
  { PlrRect o = destinoComRecuo(d);
    printf("[aspecto] modo=%d rect=%d,%d %dx%d crop=%d,%d %dx%d\n",
           aspecto, (int)(o.x + 0.5f), (int)(o.y + 0.5f),
           (int)(o.w + 0.5f), (int)(o.h + 0.5f), sx, sy, sw, sh);
    fflush(stdout);
    video_janela_fonte(sx, sy, sw, sh,
                       (int)(o.x + 0.5f), (int)(o.y + 0.5f),
                       (int)(o.w + 0.5f), (int)(o.h + 0.5f)); }
}

#ifdef NV_TOUCH_UI
static void playerTelaAtualizar(void) {
  static float largura, altura;
  if (!comVideo || retido) return;
  if (largura != NV_LAYOUT_REAL_W || altura != NV_LAYOUT_REAL_H) {
    largura = NV_LAYOUT_REAL_W; altura = NV_LAYOUT_REAL_H;
    aplicarAspecto();
  }
}
#endif

void player_aspecto_definir(int modo) {
  if (modo < 0 || modo >= PLR_ASP_N) modo = PLR_ASP_ORIGINAL;
  aspecto = modo;
  prefsGravar();
#ifdef NV_ANDROID
  aspPendente = 0;   // pedido manual: vale na hora
#endif
  aplicarAspecto();
}

// O MODO PRECISA RECORTAR? Depende do modo E do quadro.
//
// "Fit Width" num filme 2.39:1 cabe na tela inteira e nao recorta nada; no
// mesmo modo com um 4:3 a altura estoura e vira recorte. Por isso a pergunta e
// feita com o retangulo JA calculado, e nao por uma lista de modos.
static int modoPrecisaRecorte(int modo) {
  PlrRect r = aspectoRect(modo);
  return r.x < -0.5f || r.y < -0.5f ||
         r.x + r.w > NV_TELA_W + 0.5f || r.y + r.h > NV_TELA_H + 0.5f;
}

// ONDE O ALVO NAO RECORTA, O MODO NAO ENTRA NO CICLO.
//
// O Tizen so tem encaixar e esticar (ver a nota em video.h: as tres APIs foram
// testadas na TV). Sem esta guarda o botao oferecia os oito modos, escrevia o
// nome do modo na tela e a imagem ou nao mudava ou esticava — o dono viu isso
// como "escala funcionou mas nao parece estar recortando, parece que ta
// esticando". Oferecer dois modos que fazem o que dizem e melhor que oito em
// que seis mentem.
static int modoDisponivel(int modo) {
  if (video_recorte_fonte()) return 1;
  return !modoPrecisaRecorte(modo);
}

void player_toast_ex(const char *texto, unsigned ms, const char *icone, int ambar) {
  if (!texto || !*texto) return;
  durFonteAcaoAte = 0;
  snprintf(toastTexto, sizeof toastTexto, "%s", texto);
  snprintf(toastIcone, sizeof toastIcone, "%s", icone && *icone ? icone : "aj_info");
  toastCor = ambar ? 1 : 0;
  toastAte = SDL_GetTicks() + ms;
}
void player_toast(const char *texto, unsigned ms) { player_toast_ex(texto, ms, NULL, 0); }

void player_aspecto_ciclar(void) {
  durFonteAcaoAte = 0;
  int m = aspecto, i;
  // No maximo uma volta: se nada mais estiver disponivel, fica onde esta em vez
  // de girar para sempre.
  for (i = 0; i < PLR_ASP_N; i++) {
    m = (m + 1) % PLR_ASP_N;
    if (modoDisponivel(m)) break;
  }
  player_aspecto_definir(m);
  toastTexto[0] = 0;
  toastAte = SDL_GetTicks() + PLR_TOAST_MS;
}

static void alvoStream(char *dst, unsigned tam);

// Warm resume (#492): the id of this title/episode goes with the cache arming,
// so the Android shell keeps the downloaded part of the LAST title when the
// player closes and reuses it when the same title opens again. Channels and
// trailers (never armed) send nothing.
static void armarTituloGuardado(void) {
  char alvo[64];
  alvo[0] = 0;
  if (!ehCanal()) alvoStream(alvo, sizeof alvo);
  cacheboost_backend_morno(ajustes_retomada_morna());
  cacheboost_backend_titulo(alvo);
}

void player_abrir(int indiceCatalogo, const char *url) {
#ifdef NV_TOUCH_UI
  avToqueCanal[0] = 0;
#endif
  retomamorna_cancelar();
  creditosauto_zerar(&creditosAuto);
  memset(&scrobbleAlvo, 0, sizeof scrobbleAlvo); scrobbleAlvoPronto = 0;
  scrobbleTvdbCapturado = 0;
  quadroCapturado = 0;
  player_descartar_retido();
  // O trailer usa o mesmo plano de video (LG) — solta antes de o player
  // carregar a fonte, senao o load novo pisa no mediaId do trailer.
  trailer_fechar();
  // QUAL E O IDIOMA ORIGINAL DESTE TITULO. A escolha "Original" em Ajustes
  // resolve para isto; sem aviso ela se comporta como "nao trocar de faixa".
  //
  // A ficha e do titulo que a tela de detalhe abriu, que e por onde a
  // reproducao passa. Quem manda tocar direto da home sem abrir o detalhe
  // chega aqui com a ficha vazia — e ai o certo e mesmo nao trocar nada.
  ling_definir_original(extras_idioma_original());

  int ficaMini = querMini; querMini = 0;
  int n = cat_n(); if (n < 1) n = 1;
  idx = ((indiceCatalogo % n) + n) % n;
  idxVivo = idx;
  // Serie aberta pela ilha ou pelo player sem passar pelo detalhe: sem a lista
  // nao ha cards de episodios nem "Up next". Idempotente com o roteador de app.c.
  desc_episodios_garantir(idx);
  fonteAcaoInvalidar();
  aberto = 1; saindo = 0; pediuSair = 0; barraFoco = 0;
  // Titulo novo: um avanco em curso do anterior mandaria a posicao velha ao
  // pipeline novo assim que o silencio vencesse.
  scrubbing = 0; scrubPassos = 0; scrubTocava = 0; scrubDir = 0; scrubToque = 0; scrubInicio = 0.0;
  posVis = 0.0f; posVisV = 0.0f; posVisSolto = 0;
#ifdef NV_TPK
  seekgesto_zerar(&gesto);
  vooAtivo = 0; vooRetryEm = 0;
#endif
  encolhe = 1.0f; encolheAlvo = 0.0f; encolheT = 0.0f; encolheEm = 0;
  posplay_fechar();   // titulo novo, painel do anterior nao vale mais
  reacao_fechar();
  pgDesde = 0;
  epgIdx = -1;
  // Canal ao vivo nao tem classificacao por titulo: o id "cs:channel:..." nao
  // existe na base parental e o pedido so gastaria uma requisicao.
  { const CatItem *ci = cat_item(idx);
    // A COPIA e feita AQUI, no unico instante em que o indice e sabidamente o
    // do titulo pedido. Ver item().
    if (ci) { itemFixo = *ci; temFixo = 1; } else temFixo = 0;
    cinemaArteIniciar(temFixo ? &itemFixo : NULL);
    itemSessao = nv_perfil_sessao_ler();
    itemTipoConsultado = 0; itemTipoConfirmado[0] = 0;
    epPedidoT = epPedidoE = fonteSemEpisodio = 0;
    audioTitulo();
    artehero_logo_sessao_iniciar(temFixo ? &itemFixo : NULL);
    canalSessao = ci && nv_tipo_canal(ci->tipo);
    stream_folha_canal(canalSessao);
    if (canalSessao) itemCanal = *ci;
    // So o CH+/- feito dentro do PiP (player_manter_mini) mantem a miniatura
    // na troca — o stream velho segue no ar ate a fonte nova chegar pelo
    // fluxo de sempre. Qualquer outra abertura (OK no guia, filme, serie)
    // volta a tela cheia e para o video do canto.
    // `miniGuia` armado pelo app (player_mini_no_guia) ANTES da busca tambem
    // manda a sessao nova direto para o preview — inclusive na conta
    // limitada, onde querMini e 0 (a conexao velha ja foi solta).
    if ((ficaMini || miniGuia) && canalSessao) { mini = 1; aberto = 0; }
    else if (mini || ficaMini) { mini = 0; miniGuia = 0; video_parar(); }
    else miniGuia = 0;
    janAtiva = 0;
    // O piso de arte conta o PiP: video no ar (mini/comVideo) com a UI toda
    // interativa e o pior caso do #415 — o comentario antigo prometia e o
    // `aberto=0` da miniatura desligava (rev #415).
    avisarCascaAberto(aberto || mini || comVideo);
    if (ci && ci->imdb[0] && !canalSessao && ajustes_classif_player()) parental_pedir(ci->imdb);
    // A grade EPG comeca a baixar ja: o banner "agora/a seguir" do OSD e o
    // overlay do guia dependem dela. Idempotente.
    if (canalSessao) { guia_carregar(); epg_iniciar(); } }
  tocando = 1; pausaPessoa = 0; retomandoSalto = 0; visivel = 1; anim = 0.0f; entrada = 0.0f; soBarra = 0; cheio = 1.0f; fileira = 1.0f;
  pedFontes = erroFonte = pedFaixas = pedProxT = pedProxE = trocaEpisodio = 0; inicioImagem = 0;
  durFonteAvisada = durFontePulou = pedProximaFonte = durFonteCurta = durFonteForaCache = 0;
  durFonteAcaoAte = 0; durFonteSkipEm = 0; durFonteManter = 0;
  abrindoExp = 0; abrindoT = 0.0f; abrindoDesde = 0;
  memset(&cine, 0, sizeof cine);
  cine.calado = 1;
  erroTitulo[0] = erroDica[0] = 0;
  pedGuia = pedZap = pedGuiaCheio = 0;
  memset(&zapEst, 0, sizeof zapEst); bannerAV = 0.0f; botaoAV = 0; infoAV = 0; pedRecarregar = 0; pedUltimo = 0; pedNumero = 0; ltv_dig_limpar(&digitosCanal);
  avLat0 = -1.0; avAtraso = 0.0; avPausaDesde = 0;
  retomadaAplicada=0; semRetomada=0; retomarFonteSeg = -1.0; baseT = baseE = -1;
  abertoEm = SDL_GetTicks(); medirAbertura = 1; tocouMedido = 0; loadEm = 0;
#ifdef NV_ANDROID
  retomarSeg = 0.0; retomadaNaPreparacao = 0;
#endif
  botao = PLR_PLAY;
  memset(focoB, 0, sizeof focoB);
  posSeg = 0.0f; relogio_zerar(&relLeg); velmed_zerar(&velMed);
  ultimoInput = SDL_GetTicks();
  esperandoFonte = (url == NULL);
  // Legenda externa e da sessao que acabou, nao desta.
#ifdef NV_ANDROID
  if (preTitulo.adotado && preTitulo.legenda && !preTitulo.mkv) faixas_reiniciar_preparada();
  else
#endif
    faixas_reiniciar();
  // F07: volume back to 100% and the cache report of the last title forgotten.
  cacheboost_sessao();
  // O modo de proporcao e do APARELHO, nao da sessao: reler aqui e o que faz
  // "Zoom cinema" continuar valendo no filme seguinte, como no web.
  prefsLer();
  // #202: Proporcao padrao (Ajustes > Reproducao). So em memoria: o arquivo
  // guarda o ultimo modo escolhido na mao, que continua valendo para "Ultimo usado".
  { int pad = ajustes_proporcao_padrao(); if (pad >= 0 && pad < PLR_ASP_N) aspecto = pad; }
  toastAte = 0; toastTexto[0] = 0; avisouAudio = 0;
  // Reconexao so no filme/episodio: canal ao vivo tem o watchdog de app.c.
  video_definir_reconexao(!ehCanal());
  video_definir_modo_live(ehCanal() ? guia_modo_do_canal(player_id_canal(), ajustes_livetv_modo()) : 0);
  // F07: seek cache for this open (live channels never; the backend also
  // refuses HLS/DASH). Arms only the next video_tocar: trailers stay uncached.
  cacheboost_backend_cache(ehCanal() ? 0 : ajustes_cache_seek_mb());
  epT = itemFixo.temporada; epE = itemFixo.episodio;
  armarTituloGuardado();
  cacheboost_backend_buffer(cacheboost_buffer_mb(ajustes_buffer_preset(), memlog_livre_mb()));
  loadEm = SDL_GetTicks();
  { char px[96];
    if (url && *url) { memlog_evento("load"); memQuadroPendente = 1; }
    comVideo = (url && *url && video_tocar(proxyts_resolver(url, px, sizeof px))); }
  zerarAssistido();
  mkvass_video_aberto(comVideo);
  legsync_auto_habilitar(ajustes_legenda_sync_auto());
  legref_fonte_local(mkvass_referencia);
  legref_fonte_cabecalhos(video_cabecalhos);   // a sincronia le o video com os cabecalhos do addon
  legsync_iniciar(comVideo && !xtream_url_limitada(video_url_atual()) ? video_url_atual() : "");   // F05: geracao nova, sem rede nem espera
  legsync_ui_ligar();   // F05: a linha de AutoSync do seletor de legendas (legendasui.c)
  aspArmar();
  aplicarAspecto();

  const CatItem *c = item();
  float d = c ? duracaoDeMeta(c->meta) : 0.0f;
  duracaoSeg = d > 1.0f ? d : PLR_DUR_PADRAO;
  duracaoReal = durCurtaAvisado = durPassouAvisado = 0;

  // Identidade do episodio e independente do foco no painel de navegacao.
  //
  // AQUI O EPISODIO AINDA NAO E DEFINITIVO, e por isso esta chamada nao pode
  // descartar a lista de fontes (issue #101): o que entra e o progresso do
  // item ("Continuar assistindo"), e quem abriu diz o episodio de verdade na
  // linha seguinte (episodioDoDetalhe / cwTocar / player_definir_episodio em
  // app.c). Sem esta trava, tocar o E6 com o progresso no E5 jogaria fora a
  // busca que o detalhe ja tinha feito para o E6 — correto no resultado, e
  // alguns segundos de "abrindo fonte" cobrados por nada.
  //
  // Nada fica descoberto: app.c confere o dono da lista (stream_lista_do_alvo)
  // antes de reproduzir e refaz a busca se ela for de outro alvo.
  abrindoSessao = 1;
  player_definir_episodio(c ? c->temporada : 0, c ? c->episodio : 0);
  abrindoSessao = 0;
  if (url && *url && !comVideo) erroSemVideo();
}

int player_aberto(void)    { return aberto; }
int player_quer_sair(void) { return pediuSair; }

// --- PRE-BUSCA LATERAL DA LEGENDA ------------------------------------------
// O pipeline abre imediatamente; mkvass colhe pelo playhead em fio proprio.
// Recusas do CDN seguem o recuo/fallback sem segurar o video.
#ifndef __EMSCRIPTEN__
static int temMkv(const char *t) {
  const char *p;
  for (p = t ? t : ""; (p = strchr(p, '.')) != NULL; p++)
    if (!strncasecmp(p, ".mkv", 4)) return 1;
  return 0;
}

// O ordinal da legenda que a legenda AUTOMATICA vai ligar, pela mesma regra
// (ling_legenda_auto, embutida primeiro). Os idiomas vem do cabecalho do
// arquivo, na ordem das TrackEntry — a mesma ordem da lista da TV.
static int escolherLegendaPrebusca(const char *const *idiomas, int n) {
  int r = ling_legenda_auto(ling_legenda(), idiomas, n, 1, NULL, 0, 1);
  return r >= 0 && r < n ? r : -1;
}

static int prebuscaCabe(const char *url) {
  const char *pref = ling_legenda();
  const Stream *s = stream_item(stream_atual());
  if (mini || ehCanal() || !url || !*url || xtream_url_limitada(url)) return 0;
  if (!pref || !*pref || !strcasecmp(pref, "none")) return 0;
  if (s && strcmp(s->url, url)) s = NULL;     // torrent resolvido: a url e outra
  if (s && s->mp4) return 0;
  return temMkv(url) || (s && (temMkv(s->arquivo) || temMkv(s->descricao) || temMkv(s->rotulo)));
}
#endif

#ifdef NV_ANDROID
static int escolherLegendaPreTitulo(const char *const *idiomas, int n, const void *contexto) {
  const char *pref = contexto;
  return ling_legenda_auto(pref ? pref : "", idiomas, n, 1, NULL, 0, 1);
}
void player_precarregar_cancelar(unsigned token) {
  if (token && preTitulo.token != token) return;
  if (preTitulo.mkv) mkvass_preparo_cancelar(&preTitulo.mkv);
  else if (preTitulo.legenda && (!preTitulo.adotado || !comVideo)) mkvass_parar();
  video_precarregar_cancelar(token);
  memset(&preTitulo, 0, sizeof preTitulo);
}
static int preTituloComecarVideo(void) {
  preTitulo.nativo = (preTitulo.proximo ? video_precarregar_proximo : video_precarregar)(
      preTitulo.url, preTitulo.cab, preTitulo.segundos, preTitulo.pct,
      preTitulo.buffer, preTitulo.cacheMb, preTitulo.token, preTitulo.audio, preTitulo.kbps);
  return preTitulo.nativo;
}
unsigned player_precarregar_novo_token(void) {
  unsigned token;
  do { token = (atomic_fetch_add_explicit(&preTituloTokens, 1, memory_order_relaxed) + 1) & 0x7fffffffu; }
  while (!token);
  return token;
}
static int preTituloCriar(int indiceCatalogo, int t, int e, const Stream *s, unsigned token, int proximo) {
  const CatItem *c = cat_item(indiceCatalogo);
  const char *pref;
  int serie;
  if (!ajustes_fonte_precarregar() || !token || !c || !s || !s->url[0] || mini || retido ||
      xtream_url_limitada(s->url)) return 0;
  if (proximo ? (!aberto || !comVideo) : (aberto || video_ativo())) return 0;
  // Na ficha sem video, Original pertence ao titulo que sera aberto. O Next
  // apenas copia a politica atual, sem alterar o idioma da sessao em curso.
  if (!proximo) ling_definir_original(extras_idioma_original());
  pref = ling_legenda();
  player_precarregar_cancelar(0);
  preTitulo.token = token; preTitulo.proximo = proximo; preTitulo.kbps = stream_kbps(s);
  snprintf(preTitulo.url, sizeof preTitulo.url, "%s", s->url);
  snprintf(preTitulo.cab, sizeof preTitulo.cab, "%s", s->cabecalhos);
  snprintf(preTitulo.idioma, sizeof preTitulo.idioma, "%s", pref ? pref : "");
  snprintf(preTitulo.audio, sizeof preTitulo.audio, "%s", ling_audio());
  preTitulo.buffer = cacheboost_buffer_mb(ajustes_buffer_preset(), memlog_livre_mb()); preTitulo.cacheMb = ajustes_cache_seek_mb();
  serie = !strcmp(c->tipo, "series");
  if (c->progresso > 0 && c->progresso < ajustes_cw_concluido() &&
      (!serie || (t == c->temporada && e == c->episodio))) {
    char chave[48]; ProgRegistro r;
    preTitulo.pct = c->progresso;
    prog_chave(chave, sizeof chave, c->imdb, serie ? t : 0, serie ? e : 0);
    if (prog_por_chave(chave, &r))
      preTitulo.segundos = player_regra_retomada_inicial(r.posSeg, r.durSeg,
          c->progresso, ajustes_cw_concluido());
  }
  // O contexto independente colhe a legenda enquanto o video aquece. O
  // proximo episodio nao troca o worker nem a legenda da reproducao atual.
  if (pref && *pref && strcasecmp(pref, "none") && !s->mp4 &&
      (temMkv(s->url) || temMkv(s->arquivo) || temMkv(s->descricao) || temMkv(s->rotulo))) {
    preTitulo.mkv = mkvass_preparo_criar(s->url, escolherLegendaPreTitulo,
        preTitulo.idioma, sizeof preTitulo.idioma, preTitulo.pct / 100.0);
    preTitulo.legenda = preTitulo.mkv != NULL;
    preTitulo.desde = SDL_GetTicks();
  }
  if (preTituloComecarVideo()) return 1;
  player_precarregar_cancelar(token);
  return 0;
}
int player_precarregar_fonte(int indiceCatalogo, int t, int e, int indiceFonte, unsigned token) {
  return preTituloCriar(indiceCatalogo, t, e, stream_item(indiceFonte), token, 0);
}
int player_precarregar_proxima_fonte(int indiceCatalogo, int t, int e, const Stream *s, unsigned token) {
  // A legenda atual e o preparo futuro possuem contextos independentes.
  return preTituloCriar(indiceCatalogo, t, e, s, token, 1);
}
int player_precarregar_estado(unsigned token) {
  int estado;
  if (!token || preTitulo.token != token) return 0;
  if (!preTitulo.nativo) {
    if (!preTituloComecarVideo()) return 0;
  }
  estado = video_precarregar_estado(token);
  if (estado == 2 && preTitulo.mkv && mkvass_preparo_fase(preTitulo.mkv) == 1 &&
      SDL_GetTicks() - preTitulo.desde < MKVASS_PREBUSCA_MS) return 1;
  return estado;
}
void player_precarregar_adotar(unsigned token) {
  if (!token || preTitulo.token != token) return;
  preTitulo.adotado = 1;
  video_precarregar_adotar(token);
}
// A folha reinicia seus controles sem cancelar a legenda que este Play adota.
// Troca de posicao/idioma usa a pre-busca normal; nunca aplica a janela antiga.
static int preTituloLegendaUsar(const char *url) {
  const char *pref = ling_legenda();
  double segundos = retomarFonteSeg >= 0.0 ? retomarFonteSeg : retomarSeg;
  double pct = !semRetomada && segundos <= 0.0 ? retomarPct : 0.0;
  int ok = preTitulo.adotado && preTitulo.legenda &&
      !strcmp(preTitulo.url, url) && !strcmp(preTitulo.idioma, pref ? pref : "") &&
      fabs(preTitulo.segundos - segundos) < 0.001 &&
      (segundos > 0.0 || fabs(preTitulo.pct - pct) < 0.001);
  if (!ok) {
    if (preTitulo.adotado && preTitulo.legenda) player_precarregar_cancelar(preTitulo.token);
    return 0;
  }
  // O player anterior ja fechou. Transfere o mesmo worker, cabecalho e
  // blocos; nao repete Ranges nem substitui a legenda enquanto ele toca.
  if (preTitulo.mkv && !mkvass_preparo_adotar(&preTitulo.mkv, url, preTitulo.pct / 100.0)) {
    player_precarregar_cancelar(preTitulo.token);
    return 0;
  }
  // O video nao espera a legenda: o worker adotado segue em paralelo.
  memset(&preTitulo, 0, sizeof preTitulo);
  return 1;
}
#endif

static void tocarFonteInterno(const char *url);
static void tocarFonteInterno(const char *url) {
  marco("abrir: url ao pipeline");
  skipLiberado = 0;
  loadEm = SDL_GetTicks();   // #202: do pipeline ao primeiro quadro ("[player] load->1o quadro")
  video_definir_reconexao(!ehCanal());
  video_definir_modo_live(ehCanal() ? guia_modo_do_canal(player_id_canal(), ajustes_livetv_modo()) : 0);
  cacheboost_backend_cache(ehCanal() ? 0 : ajustes_cache_seek_mb());   // F07, ver player_abrir
  armarTituloGuardado();
  memlog_evento("load"); memQuadroPendente = 1;
  cacheboost_backend_buffer(cacheboost_buffer_mb(ajustes_buffer_preset(), memlog_livre_mb()));
  { char px[96];
#ifdef NV_ANDROID
    // app.c define episodio e "do inicio" antes de entregar a fonte. O
    // instante viaja junto da URL, nunca numa variavel pendente do Kotlin.
    // So o percentual (retomada da conta, sem registro local com segundos):
    // vai junto e o Media3 aplica com a duracao do container antes do
    // primeiro quadro. Antes abria em 0, tocava e buscava depois do "pronto"
    // (TCL, 2.0.3: +6,4 s e um primeiro quadro jogado fora).
    double ponto = retomarFonteSeg >= 0.0 ? retomarFonteSeg : retomarSeg;
    retomadaNaPreparacao = !ehCanal() && (retomarFonteSeg >= 0.0 ||
                            (!semRetomada && (retomarSeg > 0.0 || retomarPct > 0)));
    if (retomadaNaPreparacao) {
      if (ponto > 0.0) marco("abrir: ponto salvo vai na abertura (segundos)");
      else marco("abrir: ponto salvo vai na abertura (percentual)");
    }
    comVideo = video_tocar_retomada(proxyts_resolver(url, px, sizeof px),
                                    retomadaNaPreparacao ? ponto : 0.0,
                                    retomadaNaPreparacao && retomarFonteSeg < 0.0 && ponto <= 0.0 ? retomarPct : 0.0);
    if (!comVideo) retomadaNaPreparacao = 0;
#else
    comVideo = video_tocar(proxyts_resolver(url, px, sizeof px));
#endif
  }
  zerarAssistido();
  mkvass_video_aberto(comVideo);
  aspArmar();
  if (!comVideo) {
    fontesonda_recusa(url, video_erro_texto(), 1);
    erroSemVideo();
  }
  // No PiP a fonte nova retoca o mesmo canto — o destino de tela cheia do
  // aplicarAspecto so vale com a tela aberta.
  if (mini) { PlrRect r = miniDestino();
              video_janela((int)(r.x + 0.5f), (int)(r.y + 0.5f),
                           (int)(r.w + 0.5f), (int)(r.h + 0.5f)); }
  else aplicarAspecto();
}
#ifdef NV_ANDROID
static void sairParaExterno(void) { saindo = 1; pediuSair = 1; }
#endif
static void tocarFonte(const char *url) {
#ifdef NV_ANDROID
  const CatItem *c = item();   // #501: Interno / Externo / Perguntar sempre
  if (extplayer_tentar(url, c ? c->titulo : "", c && artehero_eh_anime(c), tocarFonteInterno, sairParaExterno)) return;
#endif
  tocarFonteInterno(url);
}
// A TELA DO DOLBY VISION EM MKV (dvtela.h). O relogio dela e o do quadro do
// player (dvAgora), o mesmo que as teclas usam: a espera do OK que sobra do
// Play e medida na mesma regua da entrada.
static Uint32 dvAgora;
static Uint32 dvRelogio(void) { return dvAgora ? dvAgora : SDL_GetTicks(); }
// A fonte nova vai tentar Dolby Vision em MKV? Entra a tela desde ja, no lugar
// do "Abrindo fonte"; uma fonte que nao vai sai dela (a troca automatica de
// fonte pode cair numa sem DV).
static void dvTelaFonte(const char *url) {
  const CatItem *c = item();
  if (!mini && !ehCanal() && video_dv_candidato(url)) {
    dvtela_definir_arte(c && c->backdrop[0] ? artehero_url(c) : "");
    dvtela_entrar(dvRelogio());
    video_dv_tela(1);
  } else if (dvtela_ativa()) {
    dvtela_sair(DVT_SAIDA_FONTE, dvRelogio());
    video_dv_tela(0);
  }
}
// O arquivo fica em HDR10: a nota de uma linha na ilha, com o motivo. A
// conexao lenta e o caminho que nao abriu ja tem a nota do app.c
// (video_dv_recuo_consumir); as outras saem daqui.
static void dvTelaNota(int recusa, int perfil) {
  char t[160];
  t[0] = 0;
  if (recusa == VIDEO_DV_NAO_SEM_DV)
    snprintf(t, sizeof t, "%s", i18n("Este arquivo não tem Dolby Vision. Tocando em HDR10."));
  else if (recusa == VIDEO_DV_NAO_PERFIL)
    snprintf(t, sizeof t, i18n("Dolby Vision perfil %d não toca nesta TV. Tocando em HDR10."), perfil);
  else if (recusa == VIDEO_DV_NAO_AUDIO)
    snprintf(t, sizeof t, "%s", i18n("O áudio deste arquivo não passa pelo Dolby Vision. Tocando em HDR10."));
  else if (recusa == VIDEO_DV_NAO_SONDA)
    snprintf(t, sizeof t, "%s", i18n("Não deu para ler o arquivo. Tocando em HDR10."));
  // VIDEO_DV_NAO_CONTENTOR (o arquivo nao e Matroska): sem nota. O player da TV
  // segue tocando o MP4/TS como tocaria sem a tela — no MP4 o DV e o da TV.
  if (!t[0]) return;
  // No relogio do quadro (como o aviso do audio que a TV nao toca): a nota
  // nasce no mesmo quadro em que a tela comeca a esvair.
  snprintf(toastTexto, sizeof toastTexto, "%s", t);
  snprintf(toastIcone, sizeof toastIcone, "aj_info"); toastCor = 0;
  toastAte = dvRelogio() + 7000u;
}
// Por quadro: os sinais reais do backend para a maquina, e o que cada saida
// pede (o som do HDR10 de volta, a nota, a escolha da pessoa).
static void dvTelaAtualizar(float dt, Uint32 agora) {
  VideoDvFase f;
  DvtelaSinais s;
  int saiu;
  if (!dvtela_visivel()) return;
  // The key that turned the normal opening into the card (abrecine_tecla) holds for the DV screen.
  if (dvtela_ativa())
    dvtela_apresentacao(ajustes_abertura_detalhada() || cine.detalhada);
  memset(&s, 0, sizeof s);
  if (dvtela_ativa()) {
    video_dv_fase(&f);
    s.sessao = f.sessao; s.sondado = f.sondado; s.perfil = f.perfil;
    s.audioTrocado = f.audioTrocado;
    snprintf(s.audioDe, sizeof s.audioDe, "%s", f.audioDe);
    snprintf(s.audioPara, sizeof s.audioPara, "%s", f.audioPara);
    s.caminho = f.caminho; s.fonteAberta = f.fonteAberta; s.carregado = f.carregado;
    s.dvConfirmado = f.dvConfirmado; s.tocando = f.tocando; s.recusa = f.recusa;
    // Erro de fonte do player (prazo, recusa da TV) tambem encerra: o cartao
    // de erro de sempre e quem oferece Abrir fontes / Voltar.
    s.falhou = f.falhou || erroFonte;
  }
  saiu = dvtela_atualizar(&s, dt, agora);
  if (!saiu) return;
  video_dv_tela(0);
  if (saiu == DVT_SAIDA_RECUSA) dvTelaNota(dvtela_estado()->recusa, dvtela_estado()->perfil);
}

// So depois do loadCompleted. Antes disso o pipeline ainda nao pos nada no
// plano de hardware, e furar a superficie cedo trocava a arte por um retangulo
// PRETO enquanto o fluxo abria — que era o "clica em reproduzir e fica preto".
void player_definir_retomada_fonte(double segundos) {
  if ((aberto || mini) && !ehCanal() && isfinite(segundos) && segundos >= 0.0)
    retomarFonteSeg = segundos;
}
void player_definir_fonte(const char *url) {
  if ((!aberto && !mini) || !url || !*url) return;
  fonteAcaoInvalidar();
  // A primeira fonte ainda pode aproveitar a legenda antecipada na espera.
  if (comVideo) legenda_pre_descartar(i18n("troca de fonte"));
  durFonteAvisada = pedProximaFonte = durFonteCurta = durFonteForaCache = 0;
  durFonteSkipEm = 0; durFonteManter = 0;
  // Fonte nova: a duracao esperada conhecida nao vale mais (mesmo titulo,
  // episodio outro, catalogo pode ter andado).
  durFonteEsperadaOk = 0;
  // So apaga o toast que era o aviso de duracao (os dois prazos caminham
  // juntos); qualquer OUTRO toast na tela (sem sinal, velocidade, DV) e do
  // video anterior e continua valendo ate o prazo dele.
  if (durFonteAcaoAte == toastAte) toastAte = 0;
  durFonteAcaoAte = 0;
  duracaoReal = durCurtaAvisado = durPassouAvisado = 0;
  esperandoFonte = 0;
  erroFonte = 0;
  dvTelaFonte(url);
#ifdef NV_ANDROID
  if (preTituloLegendaUsar(url)) { tocarFonte(url); return; }
#endif
  mkvass_parar();
  tocarFonte(url);
#ifndef __EMSCRIPTEN__
  if (comVideo && prebuscaCabe(url))
    mkvass_prebuscar(url, escolherLegendaPrebusca, retomarPct / 100.0);
#endif
}

void player_voltar_a_esperar(void) {
  if (!aberto) return;
  fonteAcaoInvalidar();
  legenda_pre_descartar(i18n("fonte descartada"));
  // A fonte foi descartada: a tela do DV sai e o cartao de sempre conta a
  // troca ("Fonte falhou, tentando a proxima"); a proxima, se for DV, a traz.
  if (dvtela_ativa()) { dvtela_sair(DVT_SAIDA_FONTE, dvRelogio()); video_dv_tela(0); }
  if (comVideo) { video_parar(); comVideo = 0; }
  mkvass_parar();
  mkvass_video_aberto(0);
  esperandoFonte = 1; erroFonte = 0; tocando = 1; pausaPessoa = 0;
  erroTitulo[0] = erroDica[0] = 0;
  retomadaAplicada = 0; inicioImagem = 0;
}

// Consome o pedido de abrir a folha de faixas: quem le, zera.
int  player_pediu_faixas(void) { int v = pedFaixas; pedFaixas = 0; return v; }

int  player_com_video(void) { return comVideo && !retido && video_pronto(); }
// Para o Discord (discord.c), que so le: pausado, duracao e se e canal ao vivo.
int   player_pausado(void) { return !tocando; }
// Pausa DA PESSOA (tecla/botao), nunca o `tocando` espelhado do backend: no
// quadro, `tocando = video_tocando()` vira 0 quando o provedor fecha o fluxo
// (PlaybackCompleted) e o vigia do canal (livestall.h) lia isso como pausa (#302).
int   player_pausa_pessoa(void) { return pausaPessoa; }
float player_duracao_seg(void) { return duracaoSeg; }
int   player_eh_canal(void) { return ehCanal(); }
// StreamFit (F03): the backend's REAL duration of this session's own source,
// never duracaoSeg (which starts as PLR_DUR_PADRAO or a meta guess). Not a
// channel, not while waiting for a source, not before the backend is ready;
// the shared pipeline counts only while comVideo says it is the player's.
int player_duracao_midia(double *seg) {
  double d;
  if (!aberto || !comVideo || esperandoFonte || ehCanal() || !video_ativo() || !video_pronto()) return 0;
  d = video_duracao();
  if (!(d >= 120.0 && d <= 86400.0)) return 0;   // provider error clips are short
  if (seg) *seg = d;
  return 1;
}

// Esta abrindo o fluxo: ha video pedido, mas ainda nao ha imagem.
int  player_carregando(void) { return esperandoFonte || (comVideo && !video_pronto()); }
int  player_controles_visiveis(void) { return visivel; }
int   player_foco_na_barra(void) { return barraFoco; }
// UP belongs to the player while controls are up (button row -> seek bar, bar ->
// Skip Intro), the loading card expands, or the pause panel is shown.
int   player_cima_dele(void) { return visivel || skipFoco || player_carregando() || pausao_visivel(); }
float player_fileira(void) { return fileira; }
int   player_so_barra(void) { return soBarra && visivel; }
float player_posicao_seg(void) { return posSeg; }

// Id do titulo em cena no formato do Trakt ("tt1", "tt1:T:E", "tmdb:m1").
// Serie com episodio conhecido leva ":T:E"; o "tmdb:t123" guarda o ':' do
// prefixo, entao o corte e no SEGUNDO ':' nesse caso.
static void idTrakt(const CatItem *ci, char *dst, size_t n) {
  const char *base = ci->imdb;
  size_t l = idbase_len(base);   // "tmdb:t123", "kitsu:41370" guardam o ':' do prefixo
  if (epT >= 0 && epE > 0) snprintf(dst, n, "%.*s:%d:%d", (int)l, base, epT, epE);
  else snprintf(dst, n, "%s", base);
}

static const TraktScrobbleAlvo *alvoTrakt(const CatItem *ci) {
  char id[64];
  int tvdb = scrobbleTvdbCapturado, final, parFinal = 0;
  if (!ci || !ci->imdb[0] || (strcmp(ci->tipo, "movie") && strcmp(ci->tipo, "series")) ||
      (!strcmp(ci->tipo, "series") && (epT < 0 || epE <= 0))) return NULL;
  idTrakt(ci, id, sizeof id);
  // `ci` is the immutable snapshot captured when the player opened. Consult
  // the live catalog so a playback opened from a provisional episode list can
  // begin scrobbling once the canonical list replaces it.
  final = strcmp(ci->tipo, "series") ||
          (cat_episodios_finais ? cat_episodios_finais(idxAtual(), ci->imdb)
                                : !ci->epsParciais);
  if (!strcmp(ci->tipo, "series")) {
    if (!tvdb && final && cat_episodio_tvdb)
      tvdb = cat_episodio_tvdb(idxAtual(), ci->imdb, epT, epE);
    if (final && cat_episodio_existe_final)
      parFinal = cat_episodio_existe_final(idxAtual(), ci->imdb, epT, epE);
    // Without the helper, retain the old behavior for optional-link builds.
    else if (final && !cat_episodio_existe_final) parFinal = 1;
  } else parFinal = final;
  // An exact id captured before topology replacement is safe immediately. A
  // numeric pair is safe only while that pair still exists in the final list.
  if (!scrobble_alvo_congelar(&scrobbleAlvo, &scrobbleAlvoPronto, id,
                              final, parFinal, tvdb))
    return NULL;
  return &scrobbleAlvo;
}

static int scrobbleTrakt(int evento, const CatItem *ci, double pos, double dur,
                         double assistido, int fim) {
  const TraktScrobbleAlvo *alvo = alvoTrakt(ci);
  if (!alvo) return SCR_NADA;
  if (trakt_scrobble_alvo)
    return trakt_scrobble_alvo(evento, alvo, pos, dur, assistido, fim);
  if (evento == SCR_EV_SAIU) {
    trakt_marcar(alvo->id, pos, dur, assistido, fim);
    return SCR_NADA;
  }
  return trakt_scrobble(evento, alvo->id, pos, dur, assistido, fim);
}

// WATCH_STATE v2 (#466): o player avisa os addons que declaram o resource
// (AIOStreams) de start, pause e stop. So enfileira: quem entrega e o fio de
// watchstate_envio.c, entao nada aqui espera por rede. Mesmas guardas do
// Trakt (sem canal, sem servidor pessoal, sem clipe/trailer de duracao
// errada) e identidade sem ambiguidade: filme com IMDb puro, ou episodio com
// temporada e episodio conhecidos. estado: -1 recusado nesta sessao, 0 fora,
// 1 tocando (start enviado), 2 pausado.
static struct { int estado; WsEvento ev; } ws;

static int wsIdentidade(WsEvento *e) {
  const CatItem *c = item();
  size_t n;
  memset(e, 0, sizeof *e);
  if (!c || !c->imdb[0] || jfid_e(c->imdb) || ehCanal() || !idbase_e_imdb(c->imdb) ||
      !isdigit((unsigned char)c->imdb[2])) return 0;
  n = strlen(c->imdb);
  if (n >= sizeof e->imdb || strcspn(c->imdb, ":") != n) return 0;
  if (!strcmp(c->tipo, "series") && epT >= 0 && epE > 0) {
    snprintf(e->tipo, sizeof e->tipo, "series");
    snprintf(e->videoId, sizeof e->videoId, "%s:%d:%d", c->imdb, epT, epE);
    e->season = epT; e->episode = epE;
  } else if (!strcmp(c->tipo, "movie") && epE <= 0) {
    snprintf(e->tipo, sizeof e->tipo, "movie");
    snprintf(e->videoId, sizeof e->videoId, "%s", c->imdb);
    e->season = e->episode = -1;
  } else return 0;
  snprintf(e->metaId, sizeof e->metaId, "%s", c->imdb);
  snprintf(e->imdb, sizeof e->imdb, "%s", c->imdb);
  if (c->tmdb > 0) snprintf(e->tmdb, sizeof e->tmdb, "%ld", c->tmdb);
  return 1;
}

// Posicao REAL (nao a pos=dur que o disco guarda ao concluir) e duracao so
// quando o pipeline descreve este titulo; durMs 0 faz a chave sair do corpo.
static void wsEmitir(int ev, int played, int duracaoBoa) {
  WsEvento e = ws.ev;
  e.ev = ev; e.played = played;
  e.atMs = (long long)time(NULL) * 1000LL;
  e.posMs = isfinite(posSeg) && posSeg > 0.0f ? (long long)((double)posSeg * 1000.0) : 0;
  e.durMs = duracaoBoa && isfinite(duracaoSeg) && duracaoSeg >= 120.0f ?
            (long long)((double)duracaoSeg * 1000.0) : 0;
  wsenvio_emitir(&e);
}

// A sessao ainda e a do snapshot? (troca de episodio sem passar por fecharSessao)
static int wsMesma(void) {
  WsEvento a;
  return wsIdentidade(&a) && !strcmp(a.videoId, ws.ev.videoId);
}

static void wsTocando(void) {
  if (ws.estado < 0 || ws.estado == 1) return;
  if (ws.estado == 0) {
    if (!wsIdentidade(&ws.ev) ||
        player_duracao_suspeita((double)duracaoSeg, duracaoCatalogo())) { ws.estado = -1; return; }
    ws.ev.iniMs = (long long)time(NULL) * 1000LL;
  } else if (!wsMesma()) { ws.estado = 0; return; }
  ws.estado = 1;
  wsEmitir(WS_START, 0, 1);
}

static void wsPausou(void) {
  if (ws.estado != 1) return;
  if (!wsMesma()) { ws.estado = 0; return; }
  ws.estado = 2;
  wsEmitir(WS_PAUSE, 0, 1);
}

// Encerramento: so com start enviado. `played` vem da regra do player (e da
// prova de reproducao), nao da supressao 80-90% do Trakt.
static void wsParou(int played, int duracaoBoa) {
  if (ws.estado > 0 && wsMesma()) wsEmitir(WS_STOP, played, duracaoBoa);
  ws.estado = 0;
}

// O ALVO DO STREAM desta sessao, no formato que app.c usa para pedir fontes
// (alvoPlayer): "tt1" no filme, o id do episodio na serie.
static void alvoStream(char *dst, unsigned tam) {
  const CatItem *c = item();
  dst[0] = 0;
  if (!c || !c->imdb[0]) return;
  if (epT >= 0 && epE > 0) cat_id_stream(idxAtual(), epT, epE, dst, tam);
  else snprintf(dst, tam, "%s", c->imdb);
}

// O FILME EM PARTES (partes.h): com a sessao de partes deste alvo, o que vai
// ao Continuar assistindo, ao Trakt e a conta e a posicao do FILME INTEIRO,
// estimada pelo tamanho das partes — o arquivo tocando e so um pedaco dele.
static int partesVirt(double *vp, double *vd, int *ultima) {
  char alvo[64];
  if (ehCanal() || !comVideo) return 0;
  alvoStream(alvo, sizeof alvo);
  return partes_virtual(alvo, posSeg, duracaoSeg, duracaoCatalogo(), vp, vd, ultima);
}
static int partesAtiva(void) {
  char alvo[64];
  if (ehCanal()) return 0;
  alvoStream(alvo, sizeof alvo);
  return partes_sessao_de(alvo);
}
// Uma parte que NAO e a ultima: o fim dela nao e o fim do filme.
static int partesMeio(void) { double a, b; int u = 1; return partesVirt(&a, &b, &u) && !u; }

// ONDE OS CREDITOS COMECAM, pelas fontes de credfonte.h (capitulo, TheIntroDB,
// episodio vizinho, o que o dono ensinou). 0 = nenhuma: vale a estimativa.
// `credFonteAtual` e a que mandou da ultima vez, para o log (uma linha por
// mudanca) e para o cred_aprender.
static double credEfetivo(void) {
  const CatItem *c = item();
  static int ultimaFonte = -1; static long ultimoInicio = -1;
  CredEntrada e; int f; double s;
  memset(&e, 0, sizeof e);
  e.dur = duracaoSeg;
  e.capitulo = video_creditos();
  { double ic = intro_creditos_seg();
    if (intro_creditos_aniskip()) e.aniskip = ic; else e.introdb = ic; }
  if (c && c->imdb[0] && epT >= 0 && epE > 0) {
    cred_viz_ler(c->imdb, epT, &e.vizInicio, &e.vizDur);
    e.aprendidoResto = cred_aprendido_resto(c->imdb);
  }
  s = cred_escolher(&e, &f);
  credFonteAtual = f;
  if (f != CRED_FONTE_NENHUMA && video_pronto() && duracaoSeg >= 120.0f &&
      (f != ultimaFonte || (long)s != ultimoInicio)) {
    ultimaFonte = f; ultimoInicio = (long)s;
    printf("[credits] source=%s start=%.0fs of %.0fs\n", cred_fonte_nome(f), s, (double)duracaoSeg);
    fflush(stdout);
  }
  return s;
}

// A FONTE PARA O PROXIMO RETOMAR (fontevolta.h). Roda uma vez por sessao, no
// fechamento de verdade ou na suspensao — nunca no descarte da retida, que ja
// passou por aqui. So a sessao que TOCOU guarda: pronto, sem erro, duracao de
// titulo (o clipe de aviso de 30 s do debrid nao conta) e sem ter terminado.
// Falhou ou terminou: apaga, para o Retomar nao reabrir o que nao serve.
static void lembrarFonte(void) {
  const Stream *s = stream_item(stream_atual());
  const char *url = video_url_atual();
  double cred;
  char alvo[64];
  if (ehCanal() || !comVideo) return;
  alvoStream(alvo, sizeof alvo);
  if (erroFonte || video_falhou()) {
    fontevolta_esquecer_alvo(alvo, sessao_usuario(), perfis_ativo(), "sessao falhou"); return;
  }
  if (!video_pronto() || duracaoSeg < 120.0f) return;
  cred = credEfetivo();
  if (concluiuAgora(cred)) {
    fontevolta_esquecer_alvo(alvo, sessao_usuario(), perfis_ativo(), "titulo concluido"); return;
  }
  // A lista pode ter sido trocada por baixo (a busca de fundo do Retomar que
  // abriu pela fonte guardada): quem toca e a entrada que ja existe.
  if (!s || strcmp(s->url, url)) return;
  fontevolta_guardar(alvo, sessao_usuario(), perfis_ativo(), s, stream_idade_ms(), SDL_GetTicks());
}

static void fecharSessao(int manter, int trocaFonte) {
#ifdef NV_TPK
  // The crash after Back (27-28/10) came with no report: before the teardown,
  // make sure our SIGSEGV handler is the one on top (the main loop also checks
  // every frame; queda.h).
  queda_conferir("Back");
#endif
  imgsub_gl_limpar();
  atualizarIdentidade();
  int jaRetido = retido;
  if (!jaRetido && !quadroCapturado && ajustes_retomada_morna() && comVideo && !ehCanal()) {
    quadroCapturado = 1; cacheboost_backend_quadro(1);
  }
  // A tela do DV nao sobrevive a sessao (Voltar ja a tirou; aqui e o resto:
  // troca de titulo, encerramento pelo app).
  if (dvtela_ativa()) { dvtela_sair(DVT_SAIDA_VOLTAR, dvRelogio()); video_dv_tela(0); }
  // #202: a velocidade e desta reproducao; o trailer da home e o proximo
  // titulo comecam em 1x.
  video_velocidade(VEL_NORMAL);
  legsync_encerrar();   // F05: cancela leitura/analise da sessao; join so no fim do app
  if (!jaRetido && identidadeValida()) lembrarFonte();
  // PERSONAL-SERVER END OF SESSION (Jellyfin/Emby/Plex): stop check-in (and
  // transcode release) for the URL this session played; addon URLs never match.
  // Before video_parar so the last position is still the pipeline's.
  if (!jaRetido && comVideo) servidores_reproducao_fim(video_url_atual(), posSeg, duracaoSeg);
  // Salvar ANTES de parar: video_parar descarrega o pipeline e a posicao some
  // junto. Titulo quase no fim conta como visto por inteiro — voltar a um card
  // marcando "2 min restantes" que na verdade acabou e pior que arredondar.
  // CLIPE DE ERRO NAO E PROGRESSO. Em 17/09 o scraper do Debridio caiu e passou
  // a responder 302 para https://static.debridio.com/scraperV2/500.mp4 — um mp4
  // de 26 KB, 1280x720, 30 s e ZERO faixas de audio. Eles tem um clipe por
  // codigo de erro (400/401/403/404/429/500), entao isto nao e um acidente de
  // um provedor so: e como a familia toda avisa que falhou.
  //
  // O webOS carrega esse arquivo sem reclamar — loadCompleted, playing, tudo
  // "No Error" — e o clipe termina em 30 s. O app leu isso como "acabou o
  // episodio" e mandou /scrobble/stop com 100%: TRES episodios do dono foram
  // marcados como assistidos no Trakt de verdade sem ninguem ter visto nada.
  //
  // O limiar e de duracao porque e o unico sinal que chega aqui sem depender do
  // provedor. Dois minutos nao descartam nada que este catalogo toque: filme e
  // episodio de serie. Canal ao vivo ja sai pelo ehCanal() logo abaixo, e a
  // duracao dele nem e comparavel.
  if (!jaRetido && comVideo && video_pronto() && duracaoSeg > 1.0f && duracaoSeg < 120.0f &&
      !ehCanal()) {
    printf("[player] fluxo de %.0f s: curto demais para ser o titulo, "
           "progresso NAO gravado (clipe de erro do provedor?)\n", duracaoSeg);
    fflush(stdout);
  }
  // TRAILER NAO E PROGRESSO, nem visto (#205). O detalhe toca o trailer do
  // IMDb pela MESMA pipeline de video do player (trailer.c: video_tocar), e o
  // prepare do trailer demora: o evento PRONTO dele (129 s, "tpk evento 1
  // (129036)") chega DEPOIS de o dono pedir Reproduzir, sem selo de sessao —
  // cai na sessao nova e vira a duracao do TITULO. O filme real toca, mas com
  // duracaoSeg = 129 a regra da conclusao (pos >= dur-60 e pos >= dur/2) da o
  // filme por VISTO a 69 s de reproducao real: pos := duracaoSeg grava 100%
  // no catalogo, o trakt.c manda /scrobble/stop 100%, visto_titulo marca o
  // filme no Simkl e na conta — "quase todos os filmes da fileira Trending"
  // marcados sem ninguem ter visto nada. O limiar e o mesmo do episodio
  // (player_duracao_suspeita): pipeline que diz MENOS DE UM TERCO da minutagem
  // do titulo nao descreve este titulo — nem o fim dele, nem onde a pessoa
  // parou. Nao se grava nada, em destino nenhum. (O aviso "Esta fonte parece
  // ser outra obra" ja disparava para este caso — a partir daqui ele tambem
  // segura a gravacao.)
  double catDur = identidadeValida() ? duracaoCatalogo() : 0.0;
  int duracaoErrada = !jaRetido && comVideo && video_pronto() && duracaoSeg >= 120.0f &&
                      !ehCanal() && !partesAtiva() && player_duracao_suspeita((double)duracaoSeg, catDur);
  if (duracaoErrada) {
    printf("[player] duracao %.0fs contra %.0fs no catalogo: trailer ou fonte "
           "que nao e este titulo — progresso e visto NAO gravados\n",
           (double)duracaoSeg, catDur);
    fflush(stdout);
  }
  // PARIDADE COM O NUVIOTV OFICIAL (saveWatchProgress / saveWatchProgressInternal):
  // grava ao sair se o video da fonte principal chegou a aparecer, a duracao e
  // a real e a posicao passa de 1 s — quem pula para 94% e sai encontra 94% no
  // Continuar assistindo, como no oficial. Ate aqui eram 5 s TOCADOS: com um
  // seek logo depois de abrir (4,5 s tocados), a saida nao gravava nada.
  // O que fica de Legacy e PLR_PROVA_MIN_S de reproducao real: aqui o trailer
  // usa a MESMA pipeline (#205) e, sem nada tocado, a duracao pode ser a dele.
  int prova = fontePrincipal() && duracaoReal && !esperandoFonte && !erroFonte && !video_falhou() &&
              isfinite(posSeg) && isfinite(duracaoSeg) && posSeg >= 1.0f &&
              assistidoSeg >= PLR_PROVA_MIN_S;
  // Prova do VISTO (#205): o que esta sessao tocou desde a base — o ponto
  // retomado, e so quando o registro diz que ele foi alcancado tocando e a
  // duracao e a mesma (a de um trailer de 129 s nao empresta a base do filme).
  double base = baseDur > 0.0 ? basePos : duracaoSeg * basePct / 100.0;
  if (!baseTocou || (baseDur > 0.0 && fabs(baseDur - duracaoSeg) > fmax(5.0, duracaoSeg * 0.02))) base = 0.0;
  int chegou;
  // `chegou` (alcancado tocando) continua sendo a regra #205: ele so diz COMO a
  // posicao foi alcancada, para a base da proxima sessao. O VISTO segue o
  // oficial: posicao >= 90% (ou o fim de Legacy: creditos / cartao A seguir).
  // VISTO = a regra de Legacy OU a do oficial. A de Legacy (#205/#100) conclui
  // antes dos 90% — no marcador de creditos ou no cartao A seguir — mas so com
  // o trecho realmente tocado. A do oficial (WatchProgress.isCompleted) e so a
  // POSICAO >= 90%, inclusive depois de um seek.
  double cred = credEfetivo();
  int duracaoContradita = cred > 1.0 && cred - duracaoSeg > intro_creditos_janela(duracaoSeg);
  int fimLegacy = player_regra_visto(base, assistidoSeg, posSeg, concluiuAgora(cred), &chegou);
  // A known later ending disproves completion by the reported percentage.
  int assistiu = !duracaoContradita && scrobble_concluiu(posSeg, duracaoSeg, fimLegacy);
  // FILME EM PARTES: so a ULTIMA parte conclui, pela regra do proprio
  // arquivo (a minutagem do catalogo e a do filme inteiro). O ponto da parte
  // fica guardado para o Retomar; concluido, sai.
  double durG = duracaoSeg, posG = posSeg;
  { double vp, vd; int ult = 0;
    if (!jaRetido && partesVirt(&vp, &vd, &ult)) {
      const PartesLista *sl = partes_sessao_lista();
      int k = partes_sessao_indice();
      // IntroDB describes the whole movie; embedded chapters describe this file.
      if (video_creditos() <= 1.0)
        duracaoContradita = cred > 1.0 && cred - vd > intro_creditos_janela(vd);
      assistiu = ult && !duracaoContradita &&
                 (player_regra_concluiu(posSeg, duracaoSeg, cred) || scrobble_concluiu(posSeg, duracaoSeg, 0));
      durG = vd; posG = vp;
      chegou = chegou && assistiu;
      printf("[partes] closing on part %d of %d at %.0f s: movie position %.0f of %.0f s%s\n",
             partes_numero(sl, k), partes_principais(sl), (double)posSeg, vp, vd, assistiu ? " (finished)" : "");
      fflush(stdout);
    } }
  // Replacing a source preserves resume position, never completes the title.
  if (trocaFonte) assistiu = chegou = 0;
  // O ARQUIVO DO TORBOX ABERTO PELA LISTA (partes.h), filme ou episodio: o
  // ponto dele para o Retomar voltar no mesmo arquivo; concluido, sai.
  { PartesPonto pt; char alvoA[64];
    alvoStream(alvoA, sizeof alvoA);
    if (!jaRetido && comVideo && !ehCanal() && partes_arquivo_de(alvoA, &pt)) {
      if (assistiu) partes_ponto_esquecer(alvoA, sessao_usuario(), perfis_ativo());
      else if (video_pronto() && posSeg > 1.0f) {
        pt.pos = posSeg;
        partes_ponto_guardar(alvoA, sessao_usuario(), perfis_ativo(), &pt);
      }
    } }
  // Concluido e concluido: o registro leva o "alcancado tocando" que o selo
  // de visto do catalogo (cat_visto) le, como o markAsCompleted do oficial.
  if (assistiu) chegou = 1;
  if (duracaoContradita) chegou = 0;
  // Sem ela a posicao REAL continua gravada (barra, Continuar assistindo,
  // conta): so o arredondamento para 100% e o visto ficam de fora.
  if (identidadeValida() && !duracaoErrada && !jaRetido && prova && video_pronto() && duracaoSeg >= 120.0f &&
      !ehCanal()) {
    // CANAL nao grava progresso: uma transmissao ao vivo nao tem "onde parou" —
    // guardar posSeg contra a duracao reserva colocaria "Globo 68%" em
    // Continuar assistindo, que e justamente o que nao pode acontecer.
    // O FIM E O MESMO PARA OS DOIS LADOS DA CASA — issue #100.
    //
    // Era `posSeg >= duracaoSeg - 60`: so os ultimos 60 s arredondavam para o
    // fim, e so com o arredondamento o trakt.c manda /scrobble/stop (ele exige
    // >= 90%; abaixo disso vira /scrobble/pause, que nao marca nada). Mas o
    // cartao de "proximo episodio" declara o episodio terminado 120 s antes do
    // fim, ou no marcador de creditos — e num episodio de 18 min isso da 88,9%.
    // Quem aceitava o proximo episodio que o PROPRIO APP ofereceu saia abaixo
    // dos 90% e nada marcava: o relato #100, "tenho de marcar a mao".
    //
    // Agora ha um numero so, e e o do cartao (player_regra_concluiu). As duas
    // fontes de marcador sao lidas na mesma ordem de ofertaProximo: o capitulo
    // do Matroska descreve ESTA copia, o TheIntroDB descreve o lancamento.
    float pos = assistiu ? durG : posG;
    // Pelo indice CORRENTE do titulo, nao pelo guardado: depois de uma
    // republicacao o guardado grava o progresso no titulo errado.
    const CatItem *ci = item();
    int ia = ci ? cat_indice_por_imdb_tipo(ci->imdb, ci->tipo) : -1;
    if (ia >= 0) {
      home_registrar_retorno(ia, pos, durG);
      ilhacart_player_saiu(ia, pos, durG, epT, epE);
    } else {
      printf("[player] progresso local ignorado: sem copia do tipo %s para %s\n",
             ci ? ci->tipo : "", ci ? ci->imdb : "");
      fflush(stdout);
    }
    if (assistiu) {
      printf("[visto] marcado por player pos=%.3f dur=%.3f assistido=%.3fs id=%s T%dE%d\n",
             (double)pos, durG, assistidoSeg, ci ? ci->imdb : "", epT, epE);
      fflush(stdout);
    }
    // A posicao gravada e sempre a REAL (ponto de retomada), com o como ela
    // foi alcancada: cat_visto e a proxima base leem `tocou`.
    if (ia >= 0) cat_salvar_progresso_ep(ia, pos, durG, epT, epE, chegou);
    // E tambem para o Trakt, que e de onde o "continue assistindo" vem: gravar
    // so aqui deixaria este app discordando dos outros aparelhos do dono.
    // Personal-server items stay private: no Trakt, friends feed or social
    // activity (jfid.h). The server got its own stop check-in above.
    if (ci && ci->imdb[0] && !jfid_e(ci->imdb)) {
      char id[64];
      int simklScr;
      idTrakt(ci, id, sizeof id);
      // Trakt also completes by percentage, so an invalid duration cannot be sent.
      if (!trocaFonte && !duracaoContradita) {
        scrobbleTrakt(SCR_EV_SAIU, ci, pos, durG, assistidoSeg, assistiu);
      }
      // Simkl (#459): mesmo scrobble, independente de o Trakt estar ligado.
      // Sem vinculo, id fora do IMDb ou guarda recusando: SCR_NADA.
      simklScr = trocaFonte ? SCR_NADA : simkl_scrobble(SCR_EV_SAIU, id, pos, durG, assistidoSeg, assistiu);
      // "ASSISTIU" para os amigos, so quando CONCLUIU e so se ela ligou a
      // atividade (ou os "vistos recentemente" do perfil). Largar aos 8% nao
      // assistiu nada — e nao chega a lugar nenhum.
      recomenda_atividade_fim(ci, assistiu);
      // A PARADA para o Social: fim, abandono (< 20%) ou progresso.
      atividade_player_saiu(pos, durG, assistiu);
      // O CHECK NA LISTA, LOCALMENTE E AGORA — a outra metade do #100.
      //
      // O relato e preciso: "mostra a barra de progresso mas nao fica com o
      // check". Sao dois dados diferentes e o player so escrevia UM. A barra
      // sai de cat_salvar_progresso_ep, logo acima; o check sai de vistoep, e
      // ate aqui NADA no player tocava nesse mapa — ele so era preenchido pela
      // leitura de /shows/<id>/progress/watched, que acontece ao abrir a pagina
      // de detalhe. Ou seja: mesmo com o Trakt aceitando o scrobble, o check so
      // aparecia na visita seguinte.
      //
      // Otimista de proposito, como o gesto manual da folha ja e ("o efeito
      // LOCAL ja aconteceu antes de o fio nascer", episodios.c): a proxima
      // leitura do Trakt corrige se o servidor tiver recusado.
      // CONCLUIU TAMBEM E "VISTO" NO SIMKL E NA CONTA, e so quando assistiu:
      // um episodio largado aos 0,8% gera `[trakt] pause ... 0.8%` (scrobble,
      // "parei aqui") e nao pode virar historico em lugar nenhum. O Trakt fica
      // de fora da mascara porque o scrobble acima ja fecha o visto la. Um
      // pedido por conclusao, e so quando o check local ainda nao existia —
      // rever um episodio ja visto nao reescreve historico.
      if (assistiu) {
        int dest = visto_destinos() & ~VISTO_TRAKT;
        // O stop do scrobble ja fecha o visto no Simkl (e cai no historico
        // sozinho se falhar): mandar o historico de novo seria pedido em dobro.
        if (simklScr == SCR_STOP) dest &= ~VISTO_SIMKL;
        if (epT >= 0 && epE > 0) {
          if (vistoep_estado(ci->imdb, epT, epE) != 1) {
            VistoPar par = { (short)epT, (short)epE };
            visto_episodios(ci->imdb, "series", &par, 1, 1, dest);
          }
        } else if (strcmp(ci->tipo, "series")) {
          visto_titulo(ci->imdb, ci->tipo, NULL, 0, 1, dest);
        }
      }
      if (assistiu && epT >= 0 && epE > 0) vistoep_definir(ci->imdb, epT, epE, 1);
      // E para a CONTA. Trakt e conta sao dois destinos diferentes: nem todo
      // usuario liga o Trakt, e o progresso do app oficial vem da conta.
      sync_sujar_progresso();
    }
    // A fileira "Continuar assistindo" so era refeita no ciclo completo da
    // descoberta (issue #38): sem isto o titulo terminado ficava nela e o que
    // comecou agora nao entrava ate a proxima volta.
    desc_refazer_continuar();
  }
  if (!jaRetido) wsParou(!duracaoErrada && prova && assistiu, !duracaoErrada);
  if (!jaRetido && comVideo && !ehCanal() && (!prova || !assistiu)) {
    printf("[visto] encerramento player pos=%.3f dur=%.3f assistido=%.3fs fonte=%d prova=%d id=%s T%dE%d\n",
           (double)posSeg, (double)duracaoSeg, assistidoSeg, fontePrincipal(), prova,
           temFixo ? itemFixo.imdb : "", epT, epE);
    fflush(stdout);
  }
  // QUANTO CUSTA CADA PASSO DA SAIDA, e por que isto e uma medida e nao um
  // conserto.
  //
  // O relato e "saio do filme e trava", no Tizen. Duas hipoteses cairam antes
  // de virarem codigo: trakt_marcar ja roda em fio proprio e detached (nao
  // bloqueia nada), e o progresso local so mexe em memoria e num arquivo curto.
  // O que sobra no fio do desenho e o desmonte do AVPlay — p.stop() seguido de
  // p.close(), sincronos, e `webapis.avplay` so existe no fio principal, entao
  // nao ha para onde mover.
  //
  // Se esses milissegundos forem milhares, a resposta nao e "tirar do fio
  // principal" (impossivel): e a tela DIZER que esta saindo em vez de parecer
  // travada. Medir antes de escolher.
  { Uint32 t0 = SDL_GetTicks(), tv;
#ifdef NV_ANDROID
    // Depois da adocao, o buffer pertence a este player, mesmo se a fonte
    // ainda aguarda legendas. Voltar/Stop nao deixa o token armado para
    // outra abertura. O Next ja armado sobrevive ao fechamento do video
    // anterior, que ainda tem comVideo e usa pararMantendoPreparo.
    if (!manter && !comVideo && preTitulo.adotado)
      player_precarregar_cancelar(0);
#endif
    if (comVideo && !manter) video_parar();
    tv = SDL_GetTicks();
    pausao_fechar();
    reacao_fechar();
    episodios_fechar();
    if (!manter) { intro_desligar(); introIdx=introT=introE=-1; }
    seekr_desligar();
    // Antes do legenda_desligar: o fio do mkvass ainda entregaria um lote ao
    // overlay depois do desligamento, e o proximo titulo abriria com a legenda
    // do anterior. mkvass_parar grava o sidecar parcial com o que ja veio.
    mkvass_parar();
    mkvass_video_aberto(0);
    if (!manter) { legenda_desligar(); memlog_devolver(); }
    printf("[player] saida: video %s %u ms, resto %u ms\n", manter ? "retido" : "parar",
           (unsigned)(tv - t0), (unsigned)(SDL_GetTicks() - tv));
    fflush(stdout); }
  if (!manter) comVideo = 0;
  retido = manter; if (!manter) retidoVoo = 0;
  prepararRetencao = 0; saidaIlhaDesde = 0;
  esperandoFonte = 0; aberto = 0; saindo = 0; pediuSair = 0;
  cinemaArteIniciar(NULL);
  pedProxT = pedProxE = trocaEpisodio = pedRecarregar = 0;
  scrubbing = scrubToque = 0; scrubInicio = 0.0;
  mini = 0; querMini = 0; miniGuia = 0; janAtiva = 0;
  avisarCascaAberto(0);
  // Os DOIS relogios, e nao so o do primeiro quadro. `pgDesde` sobrevivendo ao
  // fechamento faria a proxima reproducao achar que a janela do aviso ja tinha
  // corrido — o aviso simplesmente nao entraria, sem nada no log dizendo por
  // que. Ver a nota no desenho da guia parental.
  inicioImagem = 0; pgDesde = 0;
}

void player_encerrar(void) {
  if (retido) player_descartar_retido(); else fecharSessao(0, 0);
}

void player_encerrar_fonte(void) {
  if (retido) player_descartar_retido(); else fecharSessao(0, 1);
}

// So o voo (RETIDO SO PARA O VOO, acima): Android, saida para a ilha, com
// animacao. Sem o ajuste de reter, a sessao vive so ate o pouso.
static int reterSoVoo(void) {
#ifdef NV_ANDROID
  return !ajustes_manter_video() && ajustes_saida_player_home() &&
         !ajustes_animacoes_reduzidas();
#else
  return 0;
#endif
}

static int podeReter(void) {
  const CatItem *c = item();
  // OPT-IN (05/10): so com "Manter o video pronto ao sair" (Avancado, padrao
  // Desligado; nunca no perfil seguro). Sem ele a saida fecha a sessao na hora
  // e o Retomar reabre pela fonte guardada (fontevolta.h) — exceto o voo da
  // saida no Android, que segura o quadro parado ate pousar (reterSoVoo).
  return !xtream_url_limitada(video_url_atual()) && (ajustes_manter_video() || reterSoVoo()) &&
         comVideo && !ehCanal() && c && c->imdb[0] && !erroFonte &&
         video_pronto() && video_ativo() && !video_falhou() && !video_terminou() &&
         !video_conflito_recurso() && !video_reconectando() && video_url_atual()[0] &&
         duracaoSeg >= 120.0f && home_retorno_vale(idxAtual(), posSeg, duracaoSeg) &&
         !concluiuAgora(credEfetivo());
}

void player_preparar_retencao(void) {
  // Antes do fade terminar, para o ack chegar enquanto a tela ainda e do
  // player. Sem confirmacao na saida, o app usa o fechamento normal.
  if (!quadroCapturado && ajustes_retomada_morna() && comVideo && !ehCanal()) {
    quadroCapturado = 1; cacheboost_backend_quadro(1);
  }
  if (prepararRetencao || retido || !podeReter()) return;
  prepararRetencao = 1;
#ifdef NV_ANDROID
  saidaIlhaDesde = SDL_GetTicks() | 1u;
#endif
  video_pausar(1);
}

// Durable local checkpoint every 30 s; account updates every five minutes.
// Keep source ownership, real-duration and watched-proof guards.
#define PLR_PARCIAL_INTERVALO_S 30.0f
#define PLR_PARCIAL_SEEK_S 0.7f
#define PLR_PARCIAL_LIMIAR_S 5.0
static void gravarProgressoParcial(int conta, const char *porque) {
  atualizarIdentidade();
  const CatItem *ci = item();
  double base;
  int chegou, ia;
  if (!identidadeValida() || !ci || !ci->imdb[0] || jfid_e(ci->imdb) || !comVideo || !fontePrincipal() || !duracaoReal ||
      !video_pronto() || esperandoFonte || erroFonte || video_falhou() || ehCanal() || scrubbing ||
      !isfinite(posSeg) || !isfinite(duracaoSeg) || duracaoSeg < 120.0f || posSeg < 1.0f ||
      assistidoSeg < PLR_PROVA_MIN_S ||
      (!partesAtiva() && player_duracao_suspeita((double)duracaoSeg, duracaoCatalogo())))
    return;
  if (parcialPos >= 0.0 && fabs(posSeg - parcialPos) < (conta ? 0.5 : PLR_PARCIAL_LIMIAR_S)) return;
  ia = cat_indice_por_imdb_tipo(ci->imdb, ci->tipo);
  if (ia < 0) {
    printf("[player] progresso parcial ignorado: sem copia do tipo %s para %s\n", ci->tipo, ci->imdb);
    fflush(stdout); return;
  }
  base = baseDur > 0.0 ? basePos : duracaoSeg * basePct / 100.0;
  if (!baseTocou || (baseDur > 0.0 && fabs(baseDur - duracaoSeg) > fmax(5.0, duracaoSeg * 0.02))) base = 0.0;
  (void)player_regra_visto(base, assistidoSeg, posSeg, 0, &chegou);
  // Contradictory credits also invalidate the catalogue's watched proof.
  double cred = credEfetivo();
  if (cred > 1.0 && cred - duracaoSeg > intro_creditos_janela(duracaoSeg)) chegou = 0;
  { double vp = posSeg, vd = duracaoSeg;
    if (partesVirt(&vp, &vd, NULL)) chegou = 0;   // o visto e so da saida da ultima parte
    if (!cat_salvar_progresso_ep(ia, vp, vd, epT, epE, chegou)) return; }
  dados_sincronizar_logo();
  if (conta) {
    sync_sujar_progresso();
    char id[64];
    if (epT >= 0 && epE > 0) snprintf(id, sizeof id, "%.*s:%d:%d", (int)strcspn(ci->imdb, ":"), ci->imdb, epT, epE);
    else snprintf(id, sizeof id, "%s", ci->imdb);
    double sp = posSeg, sd = duracaoSeg;
    partesVirt(&sp, &sd, NULL);
    scrobbleTrakt(SCR_EV_PONTO, ci, sp, sd, assistidoSeg, 0);
    simkl_scrobble(SCR_EV_PONTO, id, sp, sd, assistidoSeg, 0);
    parcialConta = 0;
  }
  parcialPos = posSeg;
  printf("[visto] ponto gravado (%s) pos=%.1f dur=%.1f (%.0f%%)\n", porque, (double)posSeg,
         (double)duracaoSeg, 100.0 * posSeg / duracaoSeg);
  fflush(stdout);
}

int player_suspender(void) {
  if (!prepararRetencao || !podeReter() || !video_pausa_confirmada()) return 0;
  retidoPerfil = perfis_ativo();
  snprintf(retidoConta, sizeof retidoConta, "%s", sessao_usuario());
  snprintf(retidoUrl, sizeof retidoUrl, "%s", video_url_atual());
  retidoDesde = SDL_GetTicks();
  retomarMkv = mkvass_ocupado();
  retidoVoo = reterSoVoo();
  fecharSessao(1, 0);
  if (retidoVoo)
    printf("[player] sessao pausada so ate o voo pousar (teto %u ms), depois solta\n", PLR_RETIDO_VOO_MS);
  else
    printf("[player] sessao pausada pronta para retomar (teto %u ms; 0 = sem limite)\n", ajustes_manter_video_ms());
  return 1;
}

int player_retido(void) { return retido; }
int player_retido_so_voo(void) { return retido && retidoVoo; }

void player_descartar_retido(void) {
  if (!retido) return;
  // Um backend ja substituido nao pertence mais a esta sessao.
  if (strcmp(retidoUrl, video_url_atual())) comVideo = 0;
  fecharSessao(0, 0); // progresso ja gravado na suspensao, nao faz outro scrobble
  retidoUrl[0] = retidoConta[0] = 0;
  retomarMkv = 0; retidoVoo = 0;
}

void player_ciclo_evento(Uint32 tipo) {
  if (tipo == SDL_APP_WILLENTERBACKGROUND || tipo == SDL_APP_DIDENTERBACKGROUND ||
      tipo == SDL_APP_LOWMEMORY || tipo == SDL_APP_TERMINATING) {
    if (aberto && fontePrincipal() && !scrubbing) {
      double p = video_pos();
      if (isfinite(p) && p >= 0) posSeg = (float)p;
      gravarProgressoParcial(1, "background");
    }
    player_descartar_retido(); retomamorna_cancelar();
  }
}

void player_validar_retido(Uint32 agora) {
  const char *por = NULL;
  if (!retido) return;
  // (Sint32): `agora` e o do inicio do quadro e retidoDesde pode ser mais novo.
  unsigned prazo = ajustes_manter_video_ms();
  if (!retidoVoo && prazo && (Sint32)(agora - retidoDesde) >= (Sint32)prazo) por = "prazo";
  else if (retidoVoo && (Sint32)(agora - retidoDesde) >= (Sint32)PLR_RETIDO_VOO_MS) por = "voo pousou";
  else if (perfis_ativo() != retidoPerfil || strcmp(retidoConta, sessao_usuario())) por = "perfil";
  else if (strcmp(retidoUrl, video_url_atual())) por = "outro video";
  else if (!video_pausa_confirmada()) por = "pausa perdida";
  else if (video_falhou() || video_terminou() || video_conflito_recurso() || video_reconectando()) por = "backend";
  if (!por) return;
  printf("[player] sessao retida solta: %s (%u ms)\n", por, (unsigned)(SDL_GetTicks() - retidoDesde));
  fflush(stdout);
  player_descartar_retido();
}

int player_retomar_retido(const char *imdb, int t, int e) {
  Uint32 agora = SDL_GetTicks();
  player_validar_retido(agora);
  if (!retido) return 0;
  if (!imdb || strcmp(imdb, itemFixo.imdb) || t != epT || e != epE) {
    player_descartar_retido(); return 0;
  }
  retido = 0; retidoVoo = 0; quadroCapturado = 0; aberto = 1; saindo = pediuSair = 0;
  desc_episodios_garantir(idxAtual());
  entrada = 1.0f; visivel = 0; anim = 0.0f; soBarra = 0;
  scrubbing = barraFoco = scrubToque = 0; scrubInicio = 0.0; encolhe = 1.0f; encolheT = encolheAlvo = 0.0f;
  ultimoInput = agora; retomadaAplicada = 1; tocando = 1;
  medirAbertura = 0;   // a medida desta volta e a "retomada pronta" abaixo
  relogio_zerar(&relLeg);
  assistidoAmostra = 0;
  avisarCascaAberto(1); aplicarAspecto();
  mkvass_video_aberto(1);
  if (retomarMkv) mkvass_retomar();
  retomarMkv = 0;
  video_pausar(0);
  printf("[player] retomada pronta: %u ms, sem load nem seek\n", (unsigned)(SDL_GetTicks() - agora));
  return 1;
}

// MINI-PLAYER (PiP) DE CANAL AO VIVO.
//
// Sair de um canal para a home nao precisa matar a transmissao: o plano de
// video aceita qualquer retangulo de destino (video_janela), entao "sair"
// encolhe o destino para um canto e o desenho fura a superficie ali — o
// decode, que era o custo de verdade, continua exatamente o mesmo, e voltar a
// tela cheia e instantaneo porque o fluxo nunca parou. So canal entra: um
// filme no canto tem progresso, episodio e fim para cuidar; um canal ao vivo
// nao perde nada.
// O CANAL NO CANTO (Glass UI): um cartaz-ilha de 500 a 40 das bordas — o
// video 480x270 com raio 22 dentro do miolo, e embaixo a faixa com AO VIVO,
// o canal e o programa, e as dicas. PLR_PIP_* e o retangulo do VIDEO.
#define PLR_PIP_ILHA_H 368.0f
#define PLR_PIP_W  480.0f
#define PLR_PIP_H  270.0f
#define PLR_PIP_X  (NV_TELA_W - 40.0f - 500.0f + 10.0f)
#define PLR_PIP_Y  (NV_TELA_H - 40.0f - PLR_PIP_ILHA_H + 10.0f)

// O destino em miniatura: a caixa e 16:9 e a proporcao do quadro e respeitada
// DENTRO dela — um canal 4:3 letterboxa na caixa em vez de esticar.
static PlrRect miniGuiaCaixa = { 1040.0f, 108.0f, 800.0f, 450.0f };
static PlrRect miniDestino(void) {
  PlrRect o = { PLR_PIP_X, PLR_PIP_Y, PLR_PIP_W, PLR_PIP_H };
  if (miniGuia) o = miniGuiaCaixa;
  float vw = (float)video_largura(), vh = (float)video_altura();
  if (vw > 1.0f && vh > 1.0f) {
    float ca = vw / vh, ba = o.w / o.h;
    if (ca > ba) { float h = o.w / ca; o.y += (o.h - h) * 0.5f; o.h = h; }
    else         { float w = o.h * ca; o.x += (o.w - w) * 0.5f; o.w = w; }
  }
  return o;
}

int  player_minimizavel(void) {
  return canalSessao && comVideo && !(xtream_e_id(player_id_canal()) && xtream_conexoes_limitadas());
}
int  player_mini_ativo(void)  { return mini; }
void player_manter_mini(void) {
  querMini = !(xtream_e_id(player_id_canal()) && xtream_conexoes_limitadas());
}

void player_minimizar(void) {
  PlrRect r;
  if (!player_minimizavel()) { player_encerrar(); return; }
  mini = 1; pediuSair = 0; visivel = 0;
#if defined(NV_ANDROID) && defined(NV_TOUCH_UI)
  android_player_tela_cheia(0);
#endif
  pausao_fechar(); episodios_fechar(); posplay_fechar();
  r = miniDestino();
  video_janela((int)(r.x + 0.5f), (int)(r.y + 0.5f),
               (int)(r.w + 0.5f), (int)(r.h + 0.5f));
}

// Tela cheia "contain" pela proporcao do quadro: o fim da animacao de
// crescer. O modo de proporcao de verdade (zoom, recorte) entra no ultimo
// degrau, por aplicarAspecto.
static PlrRect telaCheia(void) {
  PlrRect o = { 0.0f, 0.0f, NV_TELA_W, NV_TELA_H };
  float q = aspectoQuadro(), t = NV_TELA_W / NV_TELA_H;
  if (q > t + 0.01f) { o.h = NV_TELA_W / q; o.y = (NV_TELA_H - o.h) * 0.5f; }
  else if (q < t - 0.01f) { o.w = NV_TELA_H * q; o.x = (NV_TELA_W - o.w) * 0.5f; }
  return o;
}
static void janelaMandar(PlrRect r) {
  video_janela((int)(r.x + 0.5f), (int)(r.y + 0.5f), (int)(r.w + 0.5f), (int)(r.h + 0.5f));
}
static void janelaAnimar(PlrRect de, PlrRect para) {
#ifdef NV_TPK
  // Native display writes block on Samsung; send only the final rectangle.
  (void)de; (void)para;
  janAtiva = 0;
  aplicarAspecto();
#else
  janDe = de; janPara = para; janAgora = de; janT = 0.0f; janEm = 0; janAtiva = 1;
#endif
}
// Um degrau por PLR_ENC_MS. Curva ease-out (1-(1-t)^3): parte rapido e
// assenta devagar, que e o que se le como "o video veio para a frente".
static void janelaPasso(Uint32 agora) {
  float k, u;
  if (!janAtiva || agora < janEm) return;
  janT += 1.0f / (float)PLR_ENC_PASSOS;
  if (janT > 1.0f) janT = 1.0f;
  u = 1.0f - janT; k = 1.0f - u * u * u;
  janAgora.x = janDe.x + (janPara.x - janDe.x) * k;
  janAgora.y = janDe.y + (janPara.y - janDe.y) * k;
  janAgora.w = janDe.w + (janPara.w - janDe.w) * k;
  janAgora.h = janDe.h + (janPara.h - janDe.h) * k;
  janEm = agora + PLR_ENC_MS;
  if (janT >= 1.0f) {
    janAtiva = 0;
    if (mini) janelaMandar(miniDestino());
    else aplicarAspecto();
  } else janelaMandar(janAgora);
}

int player_janela_animando(float *x, float *y, float *w, float *h) {
  if (!janAtiva) return 0;
  if (x) *x = janAgora.x;
  if (y) *y = janAgora.y;
  if (w) *w = janAgora.w;
  if (h) *h = janAgora.h;
  return 1;
}

void player_mini_no_guia(float x, float y, float w, float h) {
  PlrRect de = miniDestino();
  int eraCanto = mini && !miniGuia;
  miniGuiaCaixa = (PlrRect){ x, y, w, h };
  miniGuia = 1;
  // Um PiP de canto que vira preview do guia desliza do canto ate o preview.
  if (eraCanto && comVideo) janelaAnimar(de, miniDestino());
}
int  player_mini_no_guia_ativo(void) { return mini && miniGuia; }

void player_minimizar_para_guia(float x, float y, float w, float h) {
  PlrRect de;
  // The guide reuses this pipeline; connection limits only guard new streams.
  if (!canalSessao || !comVideo) { player_encerrar(); return; }
  de = telaCheia();
  miniGuiaCaixa = (PlrRect){ x, y, w, h };
  miniGuia = 1;
  mini = 1; pediuSair = 0; visivel = 0; aberto = 0; saindo = 0;
  avisarCascaAberto(0);
  pausao_fechar(); episodios_fechar(); posplay_fechar();
  janelaAnimar(de, miniDestino());
}

void player_restaurar(void) {
  if (!mini) return;
  if (miniGuia && comVideo) {
    // Do preview do guia para a tela cheia, crescendo, com o MESMO fluxo: nada
    // de video_tocar, nada de busca de fonte.
    PlrRect de = miniDestino();
    miniGuia = 0;
    mini = 0; aberto = 1; saindo = 0; entrada = 0.0f;
    visivel = 1; soBarra = 0; ultimoInput = SDL_GetTicks();
    avisarCascaAberto(1);
    janelaAnimar(de, telaCheia());
    return;
  }
  miniGuia = 0;
  mini = 0; aberto = 1; saindo = 0; entrada = 0.0f;
  visivel = 1; soBarra = 0; ultimoInput = SDL_GetTicks();
  avisarCascaAberto(1);
  aplicarAspecto();   // devolve o destino de tela cheia ao plano
}

void player_fechar_mini(void) {
  if (!mini) return;
  mini = 0; miniGuia = 0; janAtiva = 0;
  // comVideo pode ja ser 0 (zap em transito: a fonte nova nao chegou) e o
  // stream velho continuaria no ar — parar e seguro mesmo sem pipeline ativo.
  video_parar();
  player_encerrar();
}

// O PiP por cima de qualquer tela: o furo abre a superficie para o plano de
// video, o anel vira moldura e a etiqueta do canal desenha DENTRO do furo —
// o alpha do blend se soma ali, entao a faixa escurece sobre a imagem sem a
// tampar (mesma propriedade que o veu dos controles usa). O destino e
// reenviado so quando muda: cada chamada ao plano e uma mensagem ao ACB, e a
// proporcao real do quadro pode chegar segundos depois da miniatura abrir.
#ifdef NV_TOUCH_UI
static int (*miniToqueGuarda)(void);
void player_mini_toque_guarda(int (*pode)(void)) { miniToqueGuarda = pode; }
static int miniToquePode(void) {
  return mini && !miniGuia && telefoneui_ativo() && miniToqueGuarda && miniToqueGuarda();
}
static void miniToqueRestaurar(int a, int b) {
  (void)a; (void)b;
  if (miniToquePode()) player_restaurar();
}
static void miniToqueFechar(int a, int b) {
  (void)a; (void)b;
  if (miniToquePode()) player_fechar_mini();
}
#endif
void player_mini_desenhar(Uint32 agora) {
  static float lx = -1.0f, ly, lw, lh;
  PlrRect r; GfxRect f;
  float fr, fg, fb;
  (void)agora;
  if (!mini) { lx = -1.0f; return; }
  r = miniDestino();
  if (janAtiva) { lx = r.x; ly = r.y; lw = r.w; lh = r.h; }
  if (r.x != lx || r.y != ly || r.w != lw || r.h != lh) {
    video_janela((int)(r.x + 0.5f), (int)(r.y + 0.5f),
                 (int)(r.w + 0.5f), (int)(r.h + 0.5f));
    lx = r.x; ly = r.y; lw = r.w; lh = r.h;
  }
  // No guia quem desenha e o guia (furo no preview, selo, bordas).
  if (miniGuia) return;
#ifdef NV_TOUCH_UI
  // A foreground sheet owns its pixels as well as its touch targets. Keep
  // the native window current across rotation, without painting over it.
  if (telefoneui_ativo() && !miniToquePode()) return;
#endif
  f = (GfxRect){ r.x, r.y, r.w, r.h };
  (void)fr; (void)fg; (void)fb;
  // A ILHA em volta (sem o anel de acento de 4 px), o furo com raio 22.
  { GfxRect il = { telefoneui_ativo() ? PLR_PIP_X - 10 : f.x - 10.0f,
                   telefoneui_ativo() ? PLR_PIP_Y - 10 : f.y - 10.0f, 500.0f, PLR_PIP_ILHA_H };
    char rot[160];
    EpgProg ag;
    float x = il.x + 20.0f, yc = f.y + f.h + 14.0f + 14.0f;
    plrui_material(il, 30.0f, 0, 1.0f);
    gfx_furo_raio(f, 22.0f / f.h);
    // AO VIVO em vermelho (estado da transmissao) e "Canal · programa".
    { const char *av = i18n("AO VIVO");
      TxtLinha l = txt_linha(TXT_MINI, av, 255, 255, 255, 255);
      float tw = txt_tracking(TXT_MINI, av, 255, 255, 255, -1.0f, 0.0f, 1.0f, 1.3f);   // medido COM o tracking
      float w = 12.0f + 8.0f + 8.0f + tw + 12.0f;
      gfx_cor((GfxRect){ x, yc - 14.0f, w, 28.0f }, 0.5f, 0.898f, 0.282f, 0.302f, 1.0f);
      gfx_cor((GfxRect){ x + 12.0f, yc - 4.0f, 8.0f, 8.0f }, 0.5f, 1, 1, 1, 1.0f);
      txt_tracking(TXT_MINI, av, 255, 255, 255, x + 28.0f, yc - (float)l.h * 0.5f, 1.0f, 1.3f);
      x += w + 12.0f; }
    snprintf(rot, sizeof rot, "%s", itemCanal.titulo[0] ? itemCanal.titulo : i18n("Canal"));
    if (pAgora(epgIdx, time(NULL), &ag)) {
      size_t u = strlen(rot);
      snprintf(rot + u, sizeof rot - u, " \xc2\xb7 %s", ag.titulo);
    }
    plrui_limpar_sep(rot);
    { TxtLinha nm = txt_linha_corta(TXT_G18M, rot, 243, 242, 239, 255, il.x + il.w - 20.0f - x);
      txt_desenhar_alpha(nm, x, yc - (float)nm.h * 0.5f, 1.0f); }
#ifdef NV_TOUCH_UI
    if (telefoneui_ativo()) {
      if (miniToquePode()) {
        GfxRect fechar = { il.x + il.w - 72, il.y + 16, 56, 56 };
        ponteiro_alvo(il.x, il.y, il.w, il.h, NULL, miniToqueRestaurar, 0, 0);
        plrui_botao_repouso_video(fechar, 1);
        gfx_icone((GfxRect){fechar.x + 16, fechar.y + 16, 24, 24}, "aj_x", 1, 1, 1, 1);
        ponteiro_alvo(fechar.x, fechar.y, fechar.w, fechar.h, NULL, miniToqueFechar, 0, 0);
        TxtLinha l = txt_linha_corta(TXT_G18M, i18n("Tela cheia"), 243, 242, 239, 255, il.w - 40);
        txt_desenhar_alpha(l, il.x + 20, il.y + il.h - 24 - l.h, .65f);
      }
    } else
#endif
    { const char *k[2] = {
#ifdef NV_ANDROID
        "CH+",
#else
        "Azul",
#endif
        "Voltar" }, *rt[2] = { "Tela cheia", "Fechar" };
      plrui_dicas_video(k, rt, 2, il.x + 20.0f, yc + 14.0f + 8.0f + 15.0f, 0, 1.0f); } }
}

// O ultimo botao da fileira: "Episodios" numa serie, "Relacionados" num filme
// que tenha o que mostrar. Num filme sem relacionado nenhum ele some, em vez de
// ficar la sem fazer nada.
static int temUltimoBotao(void) {
  return epE > 0 || extras_n_relacionados() > 0;
}

// Sources captures this token, never a mutable catalogue/list index.
unsigned player_recarregar_contexto(void) {
  return aberto && !saindo && !mini && !retido && !ehCanal() &&
      !trocaEpisodio && !pedRecarregar && !player_carregando() && !erroFonte &&
      fontePrincipal() && !video_falhou() && !video_reconectando()
      ? fonteAcaoGeracao : 0;
}
int player_recarregar_fonte(unsigned contexto) {
  Uint32 agora = SDL_GetTicks();
  if (!contexto || contexto != player_recarregar_contexto() ||
      (recarregarPedidoMarcado && agora - recarregarPedidoEm < 400u)) return 0;
  pedRecarregar = recarregarPedidoMarcado = 1;
  recarregarPedidoEm = agora;
  posplay_troca_manual();
  return 1;
}

// NESTE EPISODIO: painel da ilha com o elenco convidado (TMDB), sem sincronia.
static int nesteAberto, nesteT, nesteE;
static char nesteImdb[16];
static void corpoNeste(GfxRect r, float a, void *u) {
  NesteElenco el;
  float x = r.x + 36.0f, w = r.w - 72.0f, y = r.y + 18.0f;
  NesteEstado st = neste_estado(nesteImdb, nesteT, nesteE, &el);
  (void)u;
  if (st != NESTE_PRONTO) {
    txt_bloco_corta(TXT_ILHA_TEXTO, i18n(st == NESTE_CARREGANDO || st == NESTE_NADA ? "Carregando…" : "Sem elenco convidado listado."),
                    158, 157, 155, x, y + 8.0f, w, 30.0f, a, 2);
    return;
  }
  for (int i = 0; i < el.n; i++) {
    y += txt_bloco_corta(TXT_ILHA_PERGUNTA, el.p[i].nome, 243, 242, 239, x, y, w * 0.5f - 12.0f, 30.0f, a, 1);
    txt_bloco_corta(TXT_ILHA_TEXTO, el.p[i].personagem, 158, 157, 155, x + w * 0.5f, y - 30.0f + 3.0f, w * 0.5f, 30.0f, a, 1);
    y += 4.0f;
  }
}

typedef struct { int id, habilitada; } PlrAcao;
static int transporteSerie(void) { return !ehCanal() && epT >= 0 && epE > 0; }
static int transporteAcoes(PlrAcao a[PLR_NBTNS]) {
  int n = 0, serie = transporteSerie();
  int pronto = serie && !trocaEpisodio && !pedRecarregar && !player_carregando() && !erroFonte && !saindo;
  a[n++] = (PlrAcao){PLR_PLAY, 1};
  if (serie) a[n++] = (PlrAcao){PLR_PROXIMO, pronto && player_proximo_episodio() != NULL};
  if (!ehCanal()) a[n++] = (PlrAcao){PLR_REINICIAR,
      player_recarregar_contexto() != 0 && video_duracao() > 0.5};
  a[n++] = (PlrAcao){PLR_CC, 1};
  a[n++] = (PlrAcao){PLR_AUDIO, 1};
  a[n++] = (PlrAcao){PLR_ASPECTO, 1};
  a[n++] = (PlrAcao){PLR_FONTES, 1};
  if (temUltimoBotao()) a[n++] = (PlrAcao){PLR_EPISODIOS, 1};
  if (serie) a[n++] = (PlrAcao){PLR_NESTE, 1};
  return n;
}
static int transporteHabilitado(int id) {
  PlrAcao a[PLR_NBTNS]; int n = transporteAcoes(a);
  for (int i = 0; i < n; i++) if (a[i].id == id) return a[i].habilitada;
  return 0;
}
static void transporteMover(int direcao) {
  if (direcao != -1 && direcao != 1) return;
  PlrAcao a[PLR_NBTNS]; int n = transporteAcoes(a), atual = -1;
  for (int i = 0; i < n; i++) if (a[i].id == botao) atual = i;
  if (atual < 0) { botao = PLR_PLAY; return; }
  for (int i = atual + direcao; i >= 0 && i < n; i += direcao)
    if (a[i].habilitada) { botao = a[i].id; return; }
}
static int transporteToque(void) { return telefoneui_ativo() && !ehCanal(); }
static int transporteDuasLinhas(void) {
  if (!transporteToque()) return 0;
  PlrAcao a[PLR_NBTNS]; int n = transporteAcoes(a);
  return NV_LAYOUT_REAL_H > NV_LAYOUT_REAL_W ||
      (NV_VTELA_W - PLR_MARGEM * 2.0f) * gfx_escala_ui() < n * PLR_TOQUE_CELULA;
}
static int transportePrimeiraLinha(void) { return transporteSerie() ? 4 : 2; }
static float transporteCelulaH(void) { return PLR_TOQUE_CELULA / gfx_escala_ui(); }
static float transporteBarraY(void) {
  if (!transporteToque()) return PLR_BARRA_Y;
  int linhas = transporteDuasLinhas() ? 2 : 1;
  return NV_VTELA_H - (64.0f + PLR_TOQUE_CELULA * linhas + 12.0f * (linhas - 1)) / gfx_escala_ui() - PLR_GAP_ROW;
}
static int transporteCompacto(void) {
  return transporteToque() || (!ehCanal() && NV_VTELA_W - PLR_MARGEM * 2.0f < 1280.0f);
}
static int transporteMoverLinha(int direcao) {
  if (!transporteDuasLinhas()) return 0;
  PlrAcao a[PLR_NBTNS]; int n = transporteAcoes(a), p = transportePrimeiraLinha(), atual = -1;
  for (int i = 0; i < n; i++) if (a[i].id == botao) atual = i;
  if (atual < 0 || (direcao < 0 && atual < p) || (direcao > 0 && atual >= p)) return 0;
  int inicio = direcao > 0 ? p : 0, quantidade = direcao > 0 ? n - p : p;
  float coluna = atual < p ? (atual + 0.5f) / p : (atual - p + 0.5f) / (n - p);
  int melhor = -1; float distancia = 2.0f;
  for (int i = 0; i < quantidade; i++) if (a[inicio + i].habilitada) {
    float d = fabsf((i + 0.5f) / quantidade - coluna);
    if (d < distancia) { distancia = d; melhor = inicio + i; }
  }
  if (melhor < 0) return 0;
  botao = a[melhor].id; return 1;
}

// A JANELA DO CARTAO DE PROXIMO EPISODIO.
//
// O marcador de creditos vem de fora (intro.c, dados de terceiro) e as vezes
// esta simplesmente errado: o relato foi "teve alguns que apareceram bem
// antes". Um marcador que dispara com meia hora de episodio pela frente nao e
// credito, e obedece-lo cegamente tira o dono do episodio que ele esta vendo.
//
// SANIDADE PROPORCIONAL COM TETO, e nao so uma fracao.
//
// A primeira versao desta guarda aceitava o marcador com ate 20% do episodio
// pela frente, e 20% de 50 min sao DEZ MINUTOS — o relato depois dela continuou
// sendo "ainda aparece antes do final", e com razao: dez minutos antes do fim
// nao e credito por nenhuma medida. A fracao sozinha erra no episodio longo.
//
// R8: a regra anterior (10%, teto 300 s, piso 120 s) recusou num log de webOS
// um marcador valido: "comecam em 1346s de 1500s, sobram 154s, janela 150s" —
// 10% de um episodio de 25 min sao 150 s, e creditos de 2m34 sao normais. O
// erro que a guarda quer barrar (#34) e marcador MINUTOS fora do lugar, entao
// agora aceita-se o que sobra ate 8% da duracao ou 5 min, o que for MAIOR,
// com teto de 10 min:
//     25 min -> 300 s     50 min -> 300 s     3 h -> 600 s
//
// 2.0.3 (dono: "o cartao apareceu no meio do episodio"): a janela passou a
// ser a mesma do botao de pular creditos, intro_creditos_janela — 15% com piso
// de 2 min e teto de 5 min. 25 min -> 225 s (o caso do R8, 154 s, continua
// aceito), 22 min -> 198 s (antes 300 s = 23% do episodio), 45 min+ -> 300 s.
// O teto de 10 min para episodio longo saiu: dez minutos antes do fim e o #34.
static double credJanelaDe(double durSeg) { return intro_creditos_janela(durSeg); }
static double credJanela(void) { return credJanelaDe(duracaoSeg); }

// A REGRA SOZINHA, sem o estado do player e sem log: e o que o teste consegue
// chamar. player_janela_proximo(0) abaixo e ela mais a leitura das duas fontes de
// marcador e as duas linhas de diagnostico.
int player_regra_proximo(double posSeg, double durSeg, double cred) {
  if (durSeg <= 1.0) return 0;
  if (cred > 1.0 && durSeg - cred <= credJanelaDe(durSeg))
    return posSeg >= cred;   // marcador aceito: ele manda, e so ele
  // SEM MARCADOR: TEMPO FIXO antes do fim, nunca porcentagem (2.0.3, dono).
  // Era "os 2 min finais"; agora sao os creditos tipicos (intro_fim_estimado:
  // 50 s, 15 s em episodio < 10 min). Log da TCL na 2.0.0: "2 min finais: pos
  // 0s de 30s" — abaixo de 2 min de duracao nao ha "fim" para estimar.
  { double fimEst = intro_fim_estimado(durSeg);
    if (fimEst <= 0.0) return 0;
    return durSeg - posSeg <= fimEst; }
}

// A DURACAO INFORMADA NAO E A DO EPISODIO. Log de webOS na 2.0.0: uma fonte MP4
// informou "duration":536033 num episodio cujo marcador de creditos fica em
// 1335 s. Com 536 s a estimativa sobe o cartao aos 416 s — o meio do episodio.
// O catalogo traz a minutagem do episodio ("38 min"); quando o pipeline diz
// MENOS DE UM TERCO dela, o numero dele nao descreve este episodio. Um terco e
// largo de proposito: minutagem de catalogo e arredondada, as vezes a media da
// serie, e episodio duplo existe.
#define PLR_CAT_MIN_S 900.0
int player_duracao_suspeita(double durSeg, double catSeg) {
  return catSeg >= PLR_CAT_MIN_S && durSeg > 1.0 && durSeg * 3.0 < catSeg;
}

// 10% de folga (+3 s de amostragem) para buffer e o pulo manual de um recap.
int player_regra_visto(double base, double tocado, double pos, int fim, int *chegou) {
  *chegou = tocado + 3.0 >= 0.9 * (pos - base);
  return fim && *chegou && base + tocado >= 120.0;
}

// Ver a nota longa em player.h. Aqui so a conta: a mesma regra do cartao, mais
// a folga de 60 s que player_encerrar ja aplicava — ela cobre quem sai por
// cima do fim num titulo sem marcador e com duracao curta demais para os 120 s
// valerem alguma coisa.
int player_regra_concluiu(double posSeg, double durSeg, double cred) {
  if (durSeg <= 1.0) return 0;
  // MARCADOR MUITO DEPOIS DO "FIM": a duracao e que esta errada (536 s contra
  // creditos em 1335 s). Sem isto os 60 s de folga abaixo davam o episodio por
  // visto aos 476 s.
  if (cred > 1.0 && cred - durSeg > credJanelaDe(durSeg)) return posSeg >= cred;
  // A folga nunca vale antes da metade: com 30 s informados ela concluia o
  // titulo no segundo zero.
  if (posSeg >= durSeg - 60.0 && posSeg >= durSeg * 0.5) return 1;
  return player_regra_proximo(posSeg, durSeg, cred);
}

// Minutagem do TITULO corrente no catalogo, em segundos (0 = nao ha). Lida na
// hora: a lista de episodios pode chegar depois da abertura (#151).
//
// FILME NAO TEM LISTA: a minutagem vem do meta do proprio titulo ("2023 ·
// 1 h 46 min"), o mesmo texto que duracaoFonteEsperada le. Ate a 2.0.4 este
// zero fazia a duracao suspeita — a guarda que impede uma duracao falsa de
// dar o titulo por assistido — valer SO para episodio com minutagem na lista;
// filme NENHUM tinha guarda. Foi por ai que o PRONTO atrasado do trailer do
// IMDb virou a duracao do filme no #205.
static double duracaoCatalogo(void) {
  if (epE <= 0) {
    const CatItem *c = cat_item(idxAtual());
    return c && !strcmp(c->tipo, "movie") ? (double)duracaoDeMeta(c->meta) : 0.0;
  }
  for (int ix = idxAtual(), i = 0; i < cat_n_episodios(ix); i++) {
    const CatEp *ep = cat_episodio(ix, i);
    if (ep && ep->temporada == epT && ep->episodio == epE) return duracaoTexto(ep->duracao);
  }
  return 0.0;
}

// TMDB ja enriquece CatEp.duracao. Sem o episodio, so dois vizinhos da mesma
// temporada que concordem (20%) dao escala confiavel; nunca a reserva do player.
static double duracaoFonteEsperada(void) {
  const CatItem *c = cat_item(idxAtual());
  double antes = 0, depois = 0;
  if (!c || ehCanal()) return 0;
  if (!strcmp(c->tipo, "movie")) return duracaoDeMeta(c->meta);
  if (epT < 0 || epE <= 0) return 0;
  for (int ix = idxAtual(), i = 0; i < cat_n_episodios(ix); i++) {
    const CatEp *ep = cat_episodio(ix, i);
    double d;
    if (!ep || ep->temporada != epT) continue;
    d = duracaoDeMeta(ep->duracao);
    if (ep->episodio == epE && d > 0) return d;
    if (ep->episodio == epE - 1) antes = d;
    if (ep->episodio == epE + 1) depois = d;
  }
  return antes > 0 && depois > 0 && fmax(antes, depois) <= fmin(antes, depois) * 1.2
           ? (antes + depois) / 2 : 0;
}

void player_confirmar_partes(void) {
  // Confirmed parts are intentionally short; discard pending or queued rejection.
  durFonteSkipEm = 0; durFonteManter = 0; pedProximaFonte = 0;
  if (durFonteAcaoAte && durFonteAcaoAte == toastAte) toastAte = 0;
  durFonteAcaoAte = 0;
}
int player_parece_parte(void) { return durFonteCurta && !ehCanal(); }
int player_fonte_fora_cache(void) { return durFonteForaCache; }
int player_aviso_duracao_ativo(void) {
  return durFonteAcaoAte && (int)(SDL_GetTicks() - durFonteAcaoAte) < 0;
}
int player_pediu_proxima_fonte(void) {
  int p = pedProximaFonte; pedProximaFonte = 0; return p;
}
// O titulo do aviso de duracao: o filme, ou a serie inteira (todas as partes
// de um filme cortado em episodios, e os episodios seguintes).
static const char *durFonteTitulo(void) {
  const CatItem *c = item();
  return c ? (c->imdb[0] ? c->imdb : c->titulo) : "";
}
static void durFonteTentar(int a, int b) {
  (void)a; (void)b;
  if (!aberto || saindo || !durFonteAcaoAte || durFonteAcaoAte != toastAte ||
      SDL_GetTicks() >= durFonteAcaoAte) return;
  if (durFonteManter) {
    // MANTER: o pulo do automatico nao acontece, e este titulo nao pula mais.
    durFonteSkipEm = 0; durFonteManter = 0; durFonteAcaoAte = 0;
    duracaook_guardar(sessao_usuario(), perfis_ativo(), durFonteTitulo());
    printf("[fonte] duration: kept this source (OK); this title no longer skips sources of another length\n");
    fflush(stdout);
    player_toast_ex(i18n("Fonte mantida neste título"), 3000, "aj_check", 0);
    return;
  }
  pedProximaFonte = 1; durFonteAcaoAte = toastAte = 0;
}
// Por quadro: a contagem do pulo do automatico venceu.
static void durFonteContagem(Uint32 agora) {
  if (!durFonteSkipEm || (int)(agora - durFonteSkipEm) < 0) return;
  durFonteSkipEm = 0; durFonteManter = 0;
  if (durFonteAcaoAte == toastAte) toastAte = 0;
  durFonteAcaoAte = 0;
  if (!aberto || saindo) return;
  pedProximaFonte = 2;
  printf("[fonte] duracao: pulando para a proxima uma vez no inicio\n");
  fflush(stdout);
}
static void durFonteAtualizar(double real, Uint32 agora) {
  double esperada;
  char r[32], e[32], aviso[320];
  int rm, em;
  if (durFonteAvisada || ehCanal() || !isfinite(real) || real <= 1) return;
  // Uma parte de um filme em partes e curta de proposito, e um arquivo aberto
  // pela lista "Arquivos desta fonte" (um especial, um OVA) foi escolha da
  // pessoa: nem aviso nem pulo (partes.h).
  if (partesAtiva()) return;
  { char alvoA[64]; alvoStream(alvoA, sizeof alvoA);
    if (partes_arquivo_de(alvoA, NULL)) return; }
  { unsigned ver = cat_versao_episodios(), rev = cat_revisao();
    if (!durFonteEsperadaOk || durFonteEsperadaVer != ver || durFonteEsperadaRev != rev) {
      durFonteEsperadaOk = 1; durFonteEsperadaVer = ver; durFonteEsperadaRev = rev;
      durFonteEsperadaCache = duracaoFonteEsperada(); } }
  esperada = durFonteEsperadaCache;
  if (esperada <= 0 || fabs(real - esperada) <= 600 ||
      !(real > esperada * 2 || real < esperada * .5)) return;
  durFonteAvisada = 1;
  durFonteCurta = real < esperada * .5;
  if (duracaook_tem(sessao_usuario(), perfis_ativo(), durFonteTitulo())) {
    printf("[fonte] duracao incompatível: real=%.0fs esperada=%.0fs; kept: this title keeps sources of another length (OK earlier)\n",
           real, esperada);
    fflush(stdout);
    return;
  }
  // Only auto-play rejects short clips; manual choices retain the duration warning.
  if (tentativaN > 0 && real <= 120 && real < esperada * .5) {
    durFonteForaCache = 1;
    durFonteCurta = 0;
    durFonteSkipEm = durFonteAcaoAte = toastAte = 0;
    durFonteManter = 0;
    video_parar(); comVideo = 0;
    player_erro_fonte_motivo(i18n("Fora do cache"),
        i18n("O link levou a um vídeo de aviso: expirado ou fora de cache."));
    printf("[fonte] warning clip: real=%.0fs expected=%.0fs; stopped, not cached\n", real, esperada);
    fflush(stdout);
    return;
  }
  rm = (int)(real / 60); em = (int)(esperada / 60);
  if (rm < 60) snprintf(r, sizeof r, i18n("%d min"), rm);
  else snprintf(r, sizeof r, i18n("%dh %dmin"), rm / 60, rm % 60);
  if (em < 60) snprintf(e, sizeof e, i18n("%d min"), em);
  else snprintf(e, sizeof e, i18n("%dh %dmin"), em / 60, em % 60);
  snprintf(aviso, sizeof aviso, i18n("Esta fonte parece ser outra obra (%s vs %s)"), r, e);
  player_toast_ex(aviso, 12000, "aj_triangle-alert", 1);
  durFonteAcaoAte = toastAte = agora + 12000;
  printf("[fonte] duracao incompatível: real=%.0fs esperada=%.0fs automatico=%d\n",
         real, esperada, tentativaN > 0);
  if (tentativaN > 0 && !durFontePulou && agora - loadEm <= 10000u) {
    // O automatico ainda pula, mas so depois da contagem: o aviso mostra
    // "Manter esta fonte (OK)" ate la.
    durFontePulou = 1; durFonteManter = 1;
    durFonteSkipEm = agora + DUR_CONTAGEM_MS;
    durFonteAcaoAte = toastAte = agora + DUR_CONTAGEM_MS;
    printf("[fonte] duration: auto-play tries another source in %u s unless OK keeps this one\n", DUR_CONTAGEM_MS / 1000u);
  }
}

// player_regra_concluiu com o estado do player: e o que as saidas chamam.
// Duracao suspeita e sem marcador, quem mede o fim e a minutagem do catalogo.
static int concluiuAgora(double cred) {
  double cat = duracaoCatalogo();
  if (!(cred > 1.0) && player_duracao_suspeita(duracaoSeg, cat))
    return posSeg >= cat * 0.9;
  return player_regra_concluiu(posSeg, duracaoSeg, cred);
}

int player_janela_proximo(double antecedencia) {
  double posAntecipada = posSeg + antecedencia * player_velocidade_efetiva() / 100.0;
  // SEM A DURACAO DO VIDEO NAO SE DECIDE NADA. Log da 2.0.0: "creditos
  // RECUSADO: comecam em 1335s de 6840s" impresso ANTES de "abrir: url ao
  // pipeline" — a conta rodou com os 114 min de reserva e ainda gastou a linha
  // unica de diagnostico daquele marcador.
  if (comVideo && !duracaoReal) return 0;
  if (!durPassouAvisado && duracaoReal && posSeg > duracaoSeg + 5.0f) {
    durPassouAvisado = 1;
    printf("[posplay] duracao informada errada: posicao %.0fs ja passou dos %.0fs\n",
           (double)posSeg, (double)duracaoSeg);
    fflush(stdout);
  }
  const CatEp *p=player_proximo_episodio();
  // #151: "o Proximo as vezes nao aparece". Sem proximo episodio na lista o
  // cartao nao existe, e isso nao deixava rastro: uma linha por episodio, nos
  // 2 minutos finais, com o tamanho da lista e se ela ainda estava chegando.
  // Lista vazia = o player abriu sem os episodios do titulo; lista cheia sem
  // proximo = fim da serie, ou a temporada seguinte ainda nao esta no addon.
  if(!p&&epT>0&&duracaoSeg>1&&duracaoSeg-posSeg<=INTRO_CRED_MIN_S&&!semProxAvisado){
    semProxAvisado=1;
    printf("[posplay] sem proximo episodio depois de T%dE%d: lista com %d episodios%s\n",
           epT,epE,cat_n_episodios(idxAtual()),desc_episodios_carregando(idxAtual())?" (ainda carregando)":"");
    fflush(stdout);
  }
  if(!p||duracaoSeg<=1)return 0;
  // DUAS FONTES, NESTA ORDEM, e e a mesma ordem do posplay.c: o capitulo do
  // Matroska descreve ESTA copia, o TheIntroDB descreve o lancamento. Esta
  // funcao so olhava o TheIntroDB (por intro_ativo), entao um episodio em MKV
  // com capitulo de creditos tinha o capitulo ignorado aqui e obedecido no
  // painel de filme — duas leituras diferentes do mesmo arquivo.
  double cred = credEfetivo();
  if (cred > 1.0) {
    // SANIDADE: ver a nota de credJanela acima. Marcador que sobra mais que a
    // janela nao e credito, e dado errado — cai na regra de baixo.
    double resta = duracaoSeg - cred, janela = credJanela();
    int aceito = resta <= janela;
    // UMA LINHA POR MARCADOR, e nao por quadro. Os dois lados sao registrados
    // de proposito: so o recusado aparecia no log, e por isso "ainda aparece
    // antes do final" nao tinha como ser medido — nao dava para saber se quem
    // abriu o cartao foi o marcador aceito ou a regra dos 2 minutos com uma
    // duracao errada.
    if (!credAvisado || credAvisadoEm != cred || credAvisadoDur != (double)duracaoSeg) {
      credAvisado = 1; credAvisadoEm = cred; credAvisadoDur = (double)duracaoSeg;
      printf("[posplay] creditos %s: comecam em %.0fs de %.0fs (sobram %.0fs, janela %.0fs)\n",
             aceito ? "aceito" : "RECUSADO", cred, (double)duracaoSeg,
             resta, janela);
      fflush(stdout);
    }
    // HA MARCADOR ACEITO: ELE MANDA, E SO ELE.
    //
    // Este `return` e o conserto do #34. Antes, um marcador aceito nao impedia
    // a regra dos 2 minutos logo abaixo de rodar primeiro: em todo episodio
    // cujos creditos comecam a MENOS de 120 s do fim — que e a maioria — o
    // cartao subia antes do marcador que existia justamente para segura-lo.
    // O marcador era lido, era aprovado, e nao servia para nada.
    //
    // E a mesma guarda que o filme ganhou na 1.0.35 (posplay.c: "ha marcador e
    // ele ainda nao chegou: NAO cair no plano B"). A serie tinha ficado de
    // fora.
    if (aceito) return player_regra_proximo(posAntecipada, duracaoSeg, cred);
  }
  // FALLBACK sem dado de ninguem. Se a duracao estiver errada, e ELE quem abre
  // o cartao cedo — por isso a linha abaixo diz de onde veio.
  { double cat = duracaoCatalogo();
    if (player_duracao_suspeita(duracaoSeg, cat)) {
      if (!durCurtaAvisado) {
        durCurtaAvisado = 1;
        printf("[posplay] duracao suspeita: pipeline diz %.0fs, catalogo diz %.0fs; sem marcador o cartao nao sobe\n",
               (double)duracaoSeg, cat);
        fflush(stdout);
      }
      return 0;
    } }
  if (player_regra_proximo(posAntecipada, duracaoSeg, 0.0)) {
    if (!credFimAvisado && antecedencia <= 0.0) {
      credFimAvisado = 1;
      printf("[posplay] estimativa sem marcador (%.0f s antes do fim): pos %.0fs de %.0fs\n",
             intro_fim_estimado(duracaoSeg), (double)posSeg, (double)duracaoSeg);
      fflush(stdout);
    }
    return 1;
  }
  return 0;
}

// Toda tecla acorda os controles, inclusive a que ja executou alguma acao: no
// aparelho nao existe comando que aconteca com a barra escondida sem trazer a
// barra junto — o usuario precisa ver o efeito do que apertou.
static void acordar(void) { abrecine_acordou(&cine, player_carregando()); visivel = 1; ultimoInput = SDL_GetTicks(); }
// Tira os controles da frente (BAIXO na fileira, CIMA no topo, #305). Nao
// chama acordar(): isso os recolocaria no mesmo evento e o comando pareceria
// quebrado. O proximo toque direcional os revela de novo.
static void esconderControles(void) { barraFoco = 0; visivel = 0; ultimoInput = SDL_GetTicks(); }

// AS DUAS LINHAS DO CANAL no OSD: "AGORA hh:mm–hh:mm · titulo" e "A SEGUIR
// hh:mm · titulo", da grade EPG. Sem grade real o canal se mostra como
// "AO VIVO" — a verdade, em vez de um programa inventado.
static void linhasCanal(char *l1, size_t n1, char *l2, size_t n2) {
  time_t agoraT = time(NULL);
  EpgProg ag, px;
  struct tm lt;
  if (n1) l1[0] = 0;
  if (n2) l2[0] = 0;
  // item() ja devolve o canal congelado da sessao — o indice no catalogo pode
  // ter sido remapeado por uma republicacao da descoberta.
  if (!itemCanal.titulo[0]) { if (n1) snprintf(l1, n1, "%s", i18n("AO VIVO")); return; }
  pEpgAtualizar();
  if (pAgora(epgIdx, agoraT, &ag)) {
    char h1[12], h2[12];
    localtime_r(&ag.ini, &lt); hora_tela(h1, sizeof h1, &lt);
    localtime_r(&ag.fim, &lt); hora_tela(h2, sizeof h2, &lt);
    snprintf(l1, n1, "%s  %s\xe2\x80\x93%s  \xc2\xb7  %s",
             i18n("AGORA"), h1, h2, ag.titulo);
    if (pProximo(epgIdx, agoraT, 0, &px)) {
      char h3[12];
      localtime_r(&px.ini, &lt); hora_tela(h3, sizeof h3, &lt);
      snprintf(l2, n2, "%s %s  \xc2\xb7  %s", i18n("A seguir"), h3, px.titulo);
    }
  } else {
    snprintf(l1, n1, "%s", i18n("AO VIVO"));
  }
}

// A barra do CANAL mostra o progresso do PROGRAMA no ar, nao do fluxo — um
// ao vivo nao tem fim, e a posSeg contra a duracao reserva diria "1h12 de
// 1h54" sobre uma transmissao que nao termina.
static float fracCanal(void) {
  time_t agoraT = time(NULL);
  EpgProg ag;
  if (pAgora(epgIdx, agoraT, &ag) && ag.fim > ag.ini)
    return anim_clamp((float)(agoraT - ag.ini) / (float)(ag.fim - ag.ini),
                      0.0f, 1.0f);
  return 0.0f;
}

static int avPodePausar(void);
static void alternarTocando(void) {
  // Canal sem janela de tempo nao pausa (ver avPodePausar): a tecla fisica de
  // Pause tambem cai aqui, e congelar um ao vivo so desincroniza o som.
  if (ehCanal() && !avPodePausar()) return;
  retomandoSalto = 0;
  tocando = !tocando;
  pausaPessoa = !tocando;
  if (comVideo) video_pausar(!tocando);
}

// --- CANAL AO VIVO: fileira de botoes e dados do OSD proprio ----------------------
// PAUSAR: so quando o fluxo tem janela de tempo (duracao > 0, o DVR do provedor).
// Um ao vivo puro nao pausa de verdade: o botao apertado deixaria a imagem
// congelada e a transmissao seguindo sem ele. Sem janela, o OK so acorda o OSD.
static int avPodePausar(void) { return comVideo && video_duracao() > 0.5; }

// Segundos atras do ao vivo agora (0 no ao vivo ou sem janela).
static int avAtrasoS(Uint32 agora) {
  double t = avAtraso;
  if (!avPodePausar()) return 0;
  if (!tocando && avPausaDesde) t += (double)(agora - avPausaDesde) / 1000.0;
  if (t > video_duracao()) t = video_duracao();   // nao volta alem da janela
  return t > 0.0 ? (int)(t + 0.5) : 0;
}
// Por quadro (player_atualizar), com ou sem OSD na tela: a pausa conta mesmo
// com os controles recolhidos.
static void avRelogioAoVivo(Uint32 agora) {
  double d;
  if (!ehCanal() || !avPodePausar()) { avLat0 = -1.0; avAtraso = 0.0; avPausaDesde = 0; return; }
  d = video_duracao() - video_pos();
  if (d < 0.0) d = 0.0;
  if (tocando) {
    avPausaDesde = 0;
    if (avLat0 < 0.0 || d < avLat0) avLat0 = d;   // mais perto da borda: recalibra
    avAtraso = d - avLat0;
  } else if (!avPausaDesde) {
    avPausaDesde = agora ? agora : 1;
  }
}
// "Voltar ao vivo": a borda da janela menos a folga medida, e tocando.
static void avVoltarAoVivo(void) {
  double alvo = video_duracao() - (avLat0 > 0.0 ? avLat0 : 0.0);
  if (!avPodePausar()) return;
  video_buscar(alvo > 0.0 ? alvo : 0.0);
  if (!tocando) { tocando = 1; video_pausar(0); }
  pausaPessoa = 0;
  avAtraso = 0.0; avPausaDesde = 0;
  botaoAV = 0;   // o botao some da fileira; o foco volta ao primeiro
}

// Mostrar "Voltar ao vivo" a partir de 10 s atras: abaixo disso o atraso e
// ruido de rede, e o botao piscaria.
#define AV_ATRASO_BOTAO_S 10
static int avBotoes(int *ids) {
  int n = 0;
  if (avPodePausar()) ids[n++] = AV_B_PAUSA;
  if (avAtrasoS(SDL_GetTicks()) >= AV_ATRASO_BOTAO_S) ids[n++] = AV_B_AOVIVO;
  ids[n++] = AV_B_GUIA;
  ids[n++] = AV_B_ANT;
  ids[n++] = AV_B_PROX;
  if (guia_info_canal(player_id_canal(), NULL, NULL, NULL, 0)) ids[n++] = AV_B_FAV;
#ifdef NV_SHOT_HOOKS
  else if (shotFav) ids[n++] = AV_B_FAV;
#endif
  ids[n++] = AV_B_AUDIO;
  ids[n++] = AV_B_LEGENDA;
  ids[n++] = AV_B_INFO;
  ids[n++] = AV_B_RECARREGAR;
  ids[n++] = AV_B_FONTE;
  ids[n++] = AV_B_ULTIMO;
  ids[n++] = AV_B_ASPECTO;   // no fim, como no mockup do Glass UI
  return n;
}

// CH+/CH-, PgUp/PgDn e os botoes "Canal -/+": nenhum troca na hora, todos
// somam ao debounce (aovivo.h).
static void avZap(int dir) { aovivo_zap_apertar(&zapEst, dir, SDL_GetTicks()); }

static void avAtivar(int b) {
  switch (b) {
    case AV_B_PAUSA: alternarTocando(); break;
    case AV_B_AOVIVO: avVoltarAoVivo(); break;
    // O GUIA COMPLETO com o canal no preview (dono, 01/10: "nao ta subindo o
    // guia da TV"): o botao abria a FAIXA de zapping, a mesma do BAIXO; quem
    // aperta "Guia" espera a grade inteira por cima, com o canal tocando no
    // canto — o caminho do Voltar, que o registro mostra funcionando.
    case AV_B_GUIA: pedGuiaCheio = 1; break;
    case AV_B_ASPECTO: player_aspecto_ciclar(); break;
    case AV_B_ANT: avZap(-1); break;
    case AV_B_PROX: avZap(1); break;
    case AV_B_FAV: guia_alternar_favorito(player_id_canal()); break;
    case AV_B_AUDIO: pedFaixas = 1; break;
    case AV_B_LEGENDA: pedFaixas = 2; break;
    case AV_B_INFO: infoAV = !infoAV; break;
    case AV_B_RECARREGAR: pedRecarregar = 1; break;
    case AV_B_ULTIMO: pedUltimo = 1; break;
    case AV_B_FONTE: pedFontes = 1; break;
  }
}

// Reune o que o OSD do canal precisa: identidade (marca, numero, categoria),
// programacao, botoes e o estado do fluxo.
static void avMontarOsd(AoVivoOsd *o) {
  static char cat[64];
  int ids[AV_B_N], i, n = avBotoes(ids), w = video_largura(), h = video_altura();
  const char *id = player_id_canal();
  memset(o, 0, sizeof *o);
  o->nome = itemCanal.titulo;
  o->logo = itemCanal.poster;
  o->desc = itemCanal.sinopse;
  cat[0] = 0;
  guia_info_canal(id, &o->numero, &o->total, cat, sizeof cat);
  if (!cat[0]) {
    // Sem lista de guia: a categoria vem no genero do item ("Canal · Esportes").
    const char *sep = strstr(itemCanal.genero, " \xc2\xb7 ");
    if (sep) snprintf(cat, sizeof cat, "%s", sep + 3);
  }
  o->categoria = cat;
  pEpgAtualizar();
  if (id[0] && xtream_e_id(id)) { xtepg_querer(id); xtepg_passo(); }
  { EpgProg fx[40]; time_t agT = time(NULL);
    int nf = guia_programa_faixa(id, agT - 4 * 3600, agT + 12 * 3600, fx, 40);
    if (nf < 1 || !aovivo_epg_de_faixa(fx, nf, agT, &o->epg))
      aovivo_epg_montar(epgIdx, id, agT, &o->epg); }
#ifdef NV_SHOT_HOOKS
  if (shotEpgOk) { o->epg = shotEpg; o->numero = shotNumero; }
#endif
  for (i = 0; i < n; i++) o->botoes[i] = ids[i];
  o->nBotoes = n;
  o->foco = (botaoAV < n) ? botaoAV : n - 1;
  o->favorito = guia_e_favorito(id);
#ifdef NV_SHOT_HOOKS
  if (shotFav) o->favorito = shotFav == 2;
#endif
  o->pausado = !tocando && avPodePausar();
  o->atrasoS = avAtrasoS(SDL_GetTicks());
#ifdef NV_SHOT_HOOKS
  if (shotEpgOk && shotAtras) { o->atrasoS = shotAtras; o->pausado = 0; }
#endif
  o->pausaS = (o->pausado && avPausaDesde) ? (int)((SDL_GetTicks() - avPausaDesde) / 1000u) : 0;
  o->janelaS = avPodePausar() ? (int)video_duracao() : 0;
  o->bufferando = comVideo && video_bufferando_ms() > 1500u;
  // RESOLUCAO pela ALTURA medida, e so onde a marca nao afirma mais do que se
  // mediu (a mesma regra dos selos do filme): 1440p nao e 1080p nem 4K, e fica
  // sem marca.
  o->aspecto = player_aspecto_rotulo(aspecto);
  if (h >= 2160) snprintf(o->res, sizeof o->res, "4K");
  else if (h >= 1440) o->res[0] = 0;
  else if (h >= 1080) snprintf(o->res, sizeof o->res, "1080p");
  else if (h >= 720) snprintf(o->res, sizeof o->res, "720p");
  else if (h > 0) snprintf(o->res, sizeof o->res, "SD");
  o->infoAberta = infoAV;
  if (infoAV) {
    int k = 0;
    if (w > 0 && h > 0) snprintf(o->info[k++], sizeof o->info[0], i18n("Resolução: %dx%d"), w, h);
    else snprintf(o->info[k++], sizeof o->info[0], "%s", i18n("Resolução: ainda não informada"));
    if (strcasecmp(video_hdr(), "none") && video_hdr()[0])
      snprintf(o->info[k++], sizeof o->info[0], i18n("Imagem: %s"), video_tem_dolby_vision() ? "Dolby Vision" : video_hdr());
    { const char *d = NULL;
      switch (video_dts_estado()) {
        case VIDEO_DTS_NATIVO:     d = "Áudio: DTS, decodificado pela TV"; break;
        case VIDEO_DTS_PREPARANDO: d = "Áudio: DTS, preparando a conversão"; break;
#ifdef NV_TPK
        case VIDEO_DTS_CONVERTIDO: d = ajustes_dts_ac3() ? "Áudio: DTS convertido para Dolby Digital nesta TV"
                                                          : "Áudio: DTS convertido para AAC estéreo nesta TV"; break;
#else
        case VIDEO_DTS_CONVERTIDO: d = "Áudio: DTS convertido para AAC estéreo nesta TV"; break;
#endif
        case VIDEO_DTS_FALHOU:     d = "Áudio: DTS, a conversão falhou (sem som)"; break;
        case VIDEO_DTS_SEM_SOM:    d = "Áudio: DTS, esta TV não toca (sem som)"; break;
      }
      if (d) snprintf(o->info[k++], sizeof o->info[0], "%s", i18n(d)); }
    if (textoAudioAtual()[0]) snprintf(o->info[k++], sizeof o->info[0], i18n("Áudio: %s"), textoAudioAtual());
    else if (video_tem_atmos()) snprintf(o->info[k++], sizeof o->info[0], "%s", i18n("Áudio: Dolby Atmos"));
    if (comVideo && video_bufferando_ms() > 0)
      snprintf(o->info[k++], sizeof o->info[0], i18n("Buffer: carregando há %u s"), video_bufferando_ms() / 1000u);
    else snprintf(o->info[k++], sizeof o->info[0], "%s", i18n("Buffer: estável"));
    snprintf(o->info[k++], sizeof o->info[0], "%s", i18n("Codec, quadros e taxa: a TV não informa"));
    o->nInfo = k;
    // As marcas do OSD de filme no topo do painel (pela mesma regra de
    // medida: o que o pipeline confirma).
    { int fm = marca_resolucao(o->res);
      if (fm >= 0) o->marcas[o->nMarcas++] = fm;
      if (video_tem_dolby_vision()) o->marcas[o->nMarcas++] = FMT_DV;
      else if (!strcasecmp(video_hdr(), "HDR10")) o->marcas[o->nMarcas++] = FMT_HDR10;
      if (video_tem_atmos()) o->marcas[o->nMarcas++] = FMT_ATMOS;
      if (marcaDtsAtual() >= 0 && o->nMarcas < (int)(sizeof o->marcas / sizeof o->marcas[0]))
        o->marcas[o->nMarcas++] = marcaDtsAtual(); }
  }
}

// Teclas do OSD do canal (CH+/-, azul e BAIXO ja foram tratados antes).
static void avTecla(SDL_Keycode k) {
  int ids[AV_B_N], n = avBotoes(ids);
  int ok = (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE);
  // CANAL QUE NAO ABRIU: a tecla e do modal (Recarregar, Fonte, Guia).
  if (erroFonte) {
    if (k == SDLK_LEFT && avErroFoco > 0) avErroFoco--;
    else if (k == SDLK_RIGHT && avErroFoco < 2) avErroFoco++;
    else if (ok) avAtivar(avErroFoco == 0 ? AV_B_RECARREGAR : avErroFoco == 1 ? AV_B_FONTE : AV_B_GUIA);
    aovivo_erro_foco(avErroFoco);
    return;
  }
  if (botaoAV >= n) botaoAV = n - 1;
  if (botaoAV < 0) botaoAV = 0;
  if (!visivel) {
    soBarra = 0;
    // Escondido, o OK e o gesto do aparelho: pausa se pausa, senao mostra o OSD.
    if (ok && avPodePausar()) { botaoAV = 0; alternarTocando(); }
    acordar();
    return;
  }
  if (ok) { avAtivar(ids[botaoAV]); acordar(); return; }
  if (k == SDLK_LEFT && botaoAV > 0) botaoAV--;
  else if (k == SDLK_RIGHT && botaoAV < n - 1) botaoAV++;
  acordar();
}


// AVANCO. So vale com os controles em pe: cegamente, seta seria um pulo
// invisivel — com os botoes, quem aperta esta olhando para a barra.
//
// TRES COISAS QUE ESTAVAM ERRADAS AQUI, todas relatadas depois de usar:
//
// 1. Passo fixo de 10 s. Segurando a tecla, chegar ao fim de um filme levava
//    centenas de repeticoes. Agora o passo cresce com o tempo segurado.
//
// 2. A barra VOLTAVA sozinha para onde estava. A causa nao era o salto: e que
//    player_atualizar reescreve posSeg com video_pos() a cada quadro, e o
//    comando ao pipeline e adiado 350 ms de proposito (SEEK_REPOUSO_MS, para
//    nao mandar quatro posicoes quando o dono quis uma). No vao entre uma coisa
//    e outra, a posicao real ainda era a antiga e apagava a que o dono acabou
//    de escolher. Enquanto se avanca, quem manda em posSeg e o avanco.
//
// 3. Avancar TOCANDO fazia o icone de carga e o painel piscarem: cada salto
//    mexia no pipeline com o video correndo. Agora o video PAUSA ao comecar o
//    avanco e volta a tocar sozinho ao terminar, se estava tocando — e o
//    pipeline recebe UMA posicao, no fim, em vez de uma por toque.
static void recuarAutomatico(double de, double para) {
  if (!isfinite(de) || !isfinite(para) || para >= de) return;
  intro_auto_recuar(de, para);
  creditosauto_recuar(&creditosAuto, de, para);
  posplay_recuar();
}

// Busca do dono: se substitui uma busca de creditosAuto ainda armada, desarma.
static void buscarUsuario(double alvo) {
  if (comVideo && !ehCanal() && !strstr(video_url_atual(), "://127.0.0.1") && !strstr(video_url_atual(), "://localhost"))
    if (!xtream_url_limitada(video_url_atual())) seekprobe_iniciar(video_url_atual(), video_cabecalhos());   // #557: own proxies handle Range
  creditosauto_destino(&creditosAuto, alvo);
  video_buscar(alvo);
#ifdef NV_TPK
  vooAtivo = 1; vooAlvo = alvo; vooDesde = SDL_GetTicks(); vooLivreEm = vooRetryEm = 0;
  vooTentativas = 0; vooFalhas = video_busca_falhas(); vooGer = fonteAcaoGeracao;
#endif
}

#ifdef NV_TPK
// Once per frame, before the position is read: follow the seek in flight
// until the TV confirms it, retry a refused one (0.5 / 1.5 / 3 s, like the
// webOS seekretry) and only then tell the person it failed.
static void vooPasso(Uint32 agora) {
  unsigned f;
  double p;
  if (!vooAtivo) return;
  if (vooGer != fonteAcaoGeracao || !comVideo) {
    printf("[seek] in-flight seek dropped (source changed)\n"); fflush(stdout);
    vooAtivo = 0; vooRetryEm = 0;
    return;
  }
  f = video_busca_falhas();
  if (f != vooFalhas) {
    vooFalhas = f;
    if (vooTentativas < NV_SEEK_MAX) {
      vooTentativas++;
      vooRetryEm = (agora + nv_seek_espera_ms(vooTentativas)) | 1u;
      printf("[seek] retry %d/%d in %u ms (TV refused the seek to %.1f)\n", vooTentativas, NV_SEEK_MAX,
             nv_seek_espera_ms(vooTentativas), vooAlvo);
      fflush(stdout);
    } else {
      printf("[seek] failed: TV refused the seek to %.1f %d times, staying at %.1f\n", vooAlvo,
             NV_SEEK_MAX + 1, video_pos());
      fflush(stdout);
      player_toast_ex("Seek failed", 3500u, "pl_triangle-alert", 1);
      vooAtivo = 0; vooRetryEm = 0;
      return;
    }
  }
  if (vooRetryEm) {
    if ((Sint32)(agora - vooRetryEm) < 0) return;   // signed: see vooDecorrido
    vooRetryEm = 0; vooLivreEm = 0;
    printf("[seek] retry sent target=%.1f\n", vooAlvo); fflush(stdout);
    video_buscar(vooAlvo);
    return;
  }
  if (video_busca_em_voo()) {
    vooLivreEm = 0;
    if (vooDecorrido(agora, vooDesde) > VOO_PRAZO_MS) {
      printf("[seek] gave up waiting: no confirmation in %u ms, staying at %.1f\n", VOO_PRAZO_MS, video_pos());
      fflush(stdout);
      player_toast_ex("Seek failed", 3500u, "pl_triangle-alert", 1);
      vooAtivo = 0;
    }
    return;
  }
  if (!vooLivreEm) vooLivreEm = agora | 1u;
  p = video_pos();
  if (fabs(p - vooAlvo) < VOO_PERTO_S || vooDecorrido(agora, vooLivreEm) >= VOO_LIVRE_MS) {
    vooAtivo = 0;
  }
}
#endif

static void iniciarSalto(int toque) {
  scrubInicio = comVideo ? video_pos() : posSeg;
#ifdef NV_TPK
  // A seek still in flight: start from where the person sent it, not from the
  // stale position the TV is still reporting (the second half of the bug).
  if (vooAtivo) { scrubInicio = vooAlvo; posSeg = (float)scrubInicio; }
  seekgesto_zerar(&gesto);
#endif
  scrubToque = toque;
  scrubbing = 1;
  scrubPassos = 0;
  scrubTocava = tocando || retomandoSalto;
  pausao_fechar();
  if (tocando && comVideo) { video_pausar(1); tocando = 0; }
  posplay_busca_usuario();
}

static void saltar(int dir, int repeticao) {
  int novo = 0;
  if (!scrubbing) {
    iniciarSalto(0);
    novo = 1;
  }
  // Trocar de direcao no meio da rajada recomeca a rampa: quem passou do ponto
  // e volta quer o passo pequeno de novo, nao o de 2 minutos.
  if (dir != scrubDir) novo = 1;
  scrubToque = 0;
  scrubDir = dir;
  Uint32 agora = SDL_GetTicks();
#ifdef NV_TPK
  { const SeekParams *sp = &seekPar;
    float s = seekgesto_tecla(&gesto, sp, dir, agora, duracaoSeg, NULL);
    (void)novo; (void)repeticao;
    scrubUltimo = agora;
    if (s != 0.0f) {
      double de = posSeg;
      scrubPassos++;
      posSeg = anim_clamp(posSeg + s, 0.0f, duracaoSeg);
      recuarAutomatico(de, posSeg);
    }
    return; }
#endif
  // Repeticao de tecla segurada entre dois passos: so mantem o avanco vivo (scrubUltimo), nao anda.
  float passo = salto_tecla(&scrubSalto, novo, repeticao, agora, duracaoSeg);
  scrubUltimo = agora;
  // Repeticao so mantem o avanco vivo: quem anda e salto_quadro, por quadro.
  if (passo <= 0.0f || scrubSalto.rep) return;
  scrubPassos++;
  double de = posSeg;
  posSeg += dir * passo;
  posSeg = anim_clamp(posSeg, 0.0f, duracaoSeg);
  recuarAutomatico(de, posSeg);
}

// Fim do avanco: manda a posicao escolhida e devolve o estado de antes.
static void terminarSalto(void) {
  if (!scrubbing) return;
  scrubbing = 0;
  // No toque, o gesto inteiro decide a direcao. Ajustar um destino adiantado
  // alguns segundos para tras nao e pedir para rever o episodio.
  if (scrubToque) recuarAutomatico(scrubInicio, posSeg);
  retomandoSalto = scrubTocava;
  pausao_fechar();
  seekr_ocioso();   // solta a folha decodificada (~22 MB), fica o JPEG
#ifdef NV_TPK
  seekgesto_zerar(&gesto);   // the memory lives until the commit
#endif
  if (comVideo) {
    if (posSeg >= scrubInicio) creditosauto_buscar(&creditosAuto, posSeg);
    buscarUsuario(posSeg);
    if (scrubTocava) { video_pausar(0); tocando = 1; }
  }
  scrubToque = 0; scrubInicio = 0.0;
}

static int ativarBotao(int i, int repetiu) {
  if (!aberto || saindo || !transporteHabilitado(i)) return 0;
  // Uma abertura rapida pode devolver o foco ao Play antes do OK ser solto.
  // So uma confirmacao nova pode aciona-lo depois de trocar de episodio.
  if (repetiu && confirmacaoTransporteRetida) return 0;
  switch (i) {
    case PLR_PROXIMO: {
      Uint32 agora = SDL_GetTicks();
      const CatEp *ep;
      char alvo[64];
      if (repetiu || trocaEpisodio ||
          (episodioPedidoMarcado && agora - episodioPedidoEm < 400u)) return 0;
      ep = player_proximo_episodio();
      if (!ep || ep->temporada < 0 || ep->episodio <= 0 ||
          !cat_id_stream(idxAtual(), ep->temporada, ep->episodio, alvo, sizeof alvo) || !alvo[0]) return 0;
      // Copiar os numeros agora: a lista do catalogo pode ser republicada.
      // Nao aprender creditos nem marcar o episodio atual como terminado.
      pedProxT = ep->temporada; pedProxE = ep->episodio;
      trocaEpisodio = episodioPedidoMarcado = 1; episodioPedidoEm = agora;
      confirmacaoTransporteRetida = 1;
      posplay_troca_manual();
      return 1;
    }
    case PLR_REINICIAR: {
      // A held OK (including firmware repeat=0) restarts only once. This is
      // a user seek: preserve pause, tracks, opening policy and current URL.
      double de = video_pos();
      int retomar = scrubbing && scrubTocava;
      if (repetiu || confirmacaoTransporteRetida) return 0;
      if (!isfinite(de)) de = posSeg;
      posplay_busca_usuario();
      recuarAutomatico(de, 0.0);
      scrubbing = scrubToque = scrubTocava = 0; scrubInicio = 0.0;
      if (retomar) retomandoSalto = 1;
      pausao_fechar(); seekr_ocioso();
      posSeg = posVis = posVisV = 0.0f; posVisSolto = 0;
      retomarPct = 0; retomarFonteSeg = -1.0; retomadaAplicada = 1;
      relogio_zerar(&relLeg); velmed_zerar(&velMed);
      assistidoAmostra = 0; eofDesde = -1.0;
      buscarUsuario(0.0);
      if (retomar) { video_pausar(0); tocando = 1; }
      confirmacaoTransporteRetida = 1;
      return 1;
    }
    case PLR_PLAY: alternarTocando(); break;
    case PLR_ASPECTO: player_aspecto_ciclar(); break;
    case PLR_CC: pedFaixas = 2; break;
    case PLR_AUDIO: pedFaixas = 1; break;
    case PLR_FONTES: pedFontes = 1; break;
    case PLR_NESTE: nesteAberto = 1; nesteT = epT; nesteE = epE; break;
    case PLR_EPISODIOS:
      if (ehCanal()) pedGuia = 1;
      else if (epE > 0) episodios_abrir(idxAtual(), epT, epE);
      else posplay_abrir_relacionados(idxAtual());
      break;
    default: return 0;
  }
  if (!repetiu) confirmacaoTransporteRetida = 0;
  return 1;
}

void player_evento(const SDL_Event *e) {
  // QUALQUER ENTRADA E ATIVIDADE, antes de qualquer return: tecla, clique ou roda
  // do Magic Remote/mouse, toque e botoes do controle. O painel de pausa so sobe
  // depois de 4 s SEM nenhum comando; mexer em legenda, audio ou na barra
  // recomeca a conta (o movimento do ponteiro entra em player_atualizar).
  switch (e->type) {
    case SDL_KEYDOWN: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEWHEEL:
    case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP:
    case SDL_CONTROLLERBUTTONDOWN: case SDL_JOYBUTTONDOWN: case SDL_JOYHATMOTION:
      pausao_activity(); break;
    default: break;
  }
  if (e->type == SDL_KEYUP && (e->key.keysym.sym == SDLK_RETURN ||
      e->key.keysym.sym == SDLK_KP_ENTER || e->key.keysym.sym == SDLK_SPACE))
    confirmacaoTransporteRetida = 0;
  // O painel de pos-reproducao come a tecla quando esta no ar; ele e a coisa
  // mais recente na tela e o dono esta olhando para ele. O 2 e o BAIXO: ele
  // dispensa o painel E pede a barra de tempo de volta, que e o gesto que o
  // dono descreveu ("se clicar para baixo ele sobe e mostra o player").
  // O CARTAO "O QUE ACHOU?" vem antes do pos-reproducao: e o mais recente na
  // tela e some em 8 s. So toma ESQUERDA/DIREITA/OK/VOLTAR, e so com a barra
  // escondida (reacao.h).
  // A TELA DO DOLBY VISION tem todas as teclas enquanto esta de pe: Voltar
  // sai do player como sempre, OK no botao fica em HDR10; o resto e engolido
  // (o OSD esta coberto e nao ha o que pausar).
  if (aberto && !saindo && dvtela_ativa()) {
    int r = dvtela_evento(e, dvRelogio());
    if (r == DVT_EV_VOLTAR) { video_dv_tela(0); saindo = 1; pediuSair = 1; }
    else if (r == DVT_EV_HDR10) { video_dv_recusar(); video_dv_tela(0); }
    if (r != DVT_EV_NADA) return;
  }
  if (reacao_evento(e, visivel)) return;
  { int r = posplay_evento(e);
    if (r) { if (r == 2) acordar(); return; } }
  if (!aberto || saindo || e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (e->key.repeat && confirmacaoTransporteRetida &&
      (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)) return;

  if (durFonteAcaoAte && durFonteAcaoAte == toastAte && SDL_GetTicks() < durFonteAcaoAte &&
      (k == SDLK_RETURN || k == SDLK_KP_ENTER)) { durFonteTentar(0, 0); return; }

  // EVERY KEY IS ACTIVITY, including the ones a panel consumes below. The pause
  // panel re-arms its 5 s clock from here, so a person pressing keys - or holding
  // Left/Right - never has it reappear under their hands and take the controls
  // away. Called before any handler returns.
  pausao_activity();
  if (nesteAberto) { nesteAberto = 0; return; }

  // DIGITAR O NUMERO DO CANAL (M7): 1-9 (e o 0 com digitacao em curso) montam o
  // numero; 1,5 s sem tecla ou OK confirma; Voltar cancela so o numero.
  if (ehCanal() && !erroFonte) {
    int d = (k >= SDLK_0 && k <= SDLK_9) ? (int)(k - SDLK_0)
          : (k >= SDLK_KP_1 && k <= SDLK_KP_9) ? (int)(k - SDLK_KP_1) + 1
          : (k == SDLK_KP_0) ? 0 : -1;
    Uint32 ag = SDL_GetTicks();
    if (d >= 0 && (d > 0 || ltv_dig_ativo(&digitosCanal))) {
      ltv_dig_apertar(&digitosCanal, d, ag);
      return;
    }
    if (ltv_dig_ativo(&digitosCanal)) {
      int n;
      if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        if (ltv_dig_confirmar(&digitosCanal, &n)) pedNumero = n;
        return;
      }
      if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE) {
        ltv_dig_limpar(&digitosCanal);
        return;
      }
    }
  }
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) {
    // Back dismisses exactly one layer. Loading/buffering keeps its exit path.
    if (pausao_visivel()) { pausao_fechar(); acordar(); return; }
    if (!erroFonte && !player_carregando() && !video_bufferando_ms()) {
      if (skipFoco) { skipFoco = 0; return; }
      if (visivel || anim > 0.004f) {
        if (scrubbing) terminarSalto();
        esconderControles(); anim = 0.0f;
        intro_botao_esconder(monoSeg());
        return;
      }
      if (trechoPulavel(NULL)) { intro_botao_esconder(monoSeg()); return; }
    }
#ifdef NV_ANDROID
    if (erroFonte && !ehCanal()) {
      player_voltar_a_esperar(); pedFontes = 1;
      return;
    }
#endif
    saindo = 1; pediuSair = 1;
    return;
  }

  // A FONTE NAO ABRIU (filme/serie): a tecla e do MODAL da ilha. ESQUERDA e
  // DIREITA escolhem entre Abrir Fontes e Voltar; OK aciona. Voltar fecha o
  // player como o BACK (tratado acima); no Android ambos voltam as fontes (#409).
  if (erroFonte && !ehCanal()) {
    if (k == SDLK_LEFT) erroBotao = 0;
    else if (k == SDLK_RIGHT) erroBotao = 1;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      erroEscolher(erroBotao);
    }
    return;
  }

  // ABRINDO A FONTE: BAIXO expande o cartao e CIMA o recolhe. Antes o BAIXO aqui
  // so movia o foco dos botoes do OSD, que fica apagado enquanto a fonte abre
  // (anim = 0): a tecla nao fazia nada visivel. Voltar continua cancelando (acima)
  // e o OK/Fontes nao mudam. Com a folha de Fontes aberta a tecla e dela
  // (app.c nem chega aqui). Canal sintonizando fica de fora.
  if (player_carregando() && !ehCanal() && !stream_folha_aberta()) {
    // Na CINEMA, Para cima/Para baixo primeiro mostra a detalhada (#205).
    if (abrecine_tecla(&cine, k)) return;
    if (k == SDLK_DOWN) { abrindoExp = 1; return; }
    if (k == SDLK_UP && abrindoExp) { abrindoExp = 0; return; }
    if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !abrecine_cinema(&cine)) {
      if (stream_n() > 0 && addons_busca_parcial()) { pedFontes = 1; return; }
      if (motivoInicio[0]) { pedAjFonte = 1; return; }
    }
  }

  // PAINEL DE PAUSA: com ele de pe, a tecla e DELE. Vem antes de tudo o que
  // sobra (inclusive da tecla de proporcao) porque e o que o web faz — la o
  // ramo `if (this.pauseOverlayVisible)` engole o evento inteiro
  // (playerScreen.js:22191). Um painel que cobre a informacao da tela e nao
  // responde ao primeiro toque le como travamento.
  // TECLA FISICA DE PLAY/PAUSE (#109): alterna e poe o foco no Play, em
  // qualquer estado dos controles. Antes a Samsung traduzia Play/Pause em
  // Enter (tools/tizen-shell.html) — com os controles em pe e o foco em
  // Legendas, Audio ou Proporcao, "Pause" abria a folha daquele botao em vez
  // de pausar. Agora a casca manda a tecla Pause do teclado (keyCode 19 ->
  // SDLK_PAUSE) e o C trata como o que e. SDLK_AUDIOPLAY/STOP cobrem o SDL que
  // mapeia teclas de midia (Mac; LG, se o firmware entregar — nao verificado).
  if (k == SDLK_PAUSE || k == SDLK_AUDIOPLAY) {
    alternarTocando(); botao = PLR_PLAY; barraFoco = 0; skipFoco = 0;
    acordar(); return;
  }
  if (pausao_visivel()) {
    int r = pausao_evento(e);
    if (r == PAUSAO_RETOMAR) { alternarTocando(); acordar(); return; }
    if (r == PAUSAO_CONSUMIU) { acordar(); return; }
  }

  // CONTROLES ESCONDIDOS: qualquer direcao so acorda a interface. O OK direto
  // pausa/retoma sem navegar nada — e o gesto do aparelho: um toque no centro
  // e o video obedece, sem passos no meio.
  // A TECLA DE PROPORCAO vale sempre, com controles em pe ou escondidos. No web
  // o modo so se troca por um botao dentro de "More Actions" — dois passos com
  // um cursor que aqui nao existe. Numa TV o gesto tem que ser um toque, e o
  // aviso que sobe na troca ja diz em que modo se entrou, entao a tecla nem
  // precisa da interface aberta. O 0 e a tecla livre no controle da LG.
  // NA LIVE TV o "0" e o ULTIMO CANAL (decisao 5 do dono, plano do guia M7): e
  // o atalho mais usado de TV paga. A proporcao do canal fica no botao do OSD.
  // No filme continua a proporcao.
  if ((k == SDLK_0 || k == SDLK_KP_0) && ehCanal()) { pedUltimo = 1; return; }
  if (k == SDLK_0 || k == SDLK_KP_0) { player_aspecto_ciclar(); return; }

  // CANAL AO VIVO: OSD proprio (aovivo.h) e teclas proprias. BAIXO e o botao
  // AZUL abrem o overlay do guia em qualquer estado — e la que mora "o que esta
  // passando / trocar de canal". CH+/- (scancodes 480/481 do SDL_webOS.h da LG)
  // e PgUp/PgDn zapeiam na ordem do guia: cada toque soma ao debounce e o canal
  // so troca 600 ms depois do ultimo, com um banner dizendo quem vai tocar. No
  // Tizen o CH+ ja chega aqui como "s" pela casca (tizen-shell.html).
  // CIMA/BAIXO NAO zapeiam: BAIXO ja era o guia (decisao do dono) e CIMA acorda
  // o OSD; trocar isso quebraria o gesto que a mao ja sabe.
  if (ehCanal()) {
    int sc = e->key.keysym.scancode;
    if (sc == NV_SCANCODE_CH_UP   || k == SDLK_PAGEUP)   { avZap(1);  return; }
    if (sc == NV_SCANCODE_CH_DOWN || k == SDLK_PAGEDOWN) { avZap(-1); return; }
    if (k == SDLK_F10 || k == SDLK_MENU) { pedGuiaCheio = 1; return; }
    if (k == SDLK_DOWN || k == SDLK_s || sc == NV_SCANCODE_BLUE) {
      pedGuia = 1; return;
    }
    // Remotes without colour keys open the band with OK; visible panels keep OK.
    if (!visivel && !erroFonte && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) {
      if (!e->key.repeat) pedGuia = 1;
      return;
    }
    avTecla(k);
    return;
  }

  if (!visivel) {
    // Tudo que acorda daqui acorda INTEIRO; so a busca abaixo volta a pedir
    // so a barra. Sem isto um soBarra de uma busca antiga, ja apagada, ficava
    // valendo para o BAIXO seguinte.
    soBarra = 0;
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      // O proximo episodio NAO e mais tratado aqui: posplay_evento roda antes
      // de tudo em player_evento e ja consome o OK enquanto o cartao esta no
      // ar. Manter esta linha faria o OK disparar a troca duas vezes.
      { double fim;if(trechoPulavel(&fim)){
          { int tp=0;if(intro_botao_visivel(NULL,&tp)&&tp==INTRO_CREDITOS)player_aprender_creditos(); }
          pularTrecho(puloDestino(fim));if(comVideo)buscarUsuario(posSeg);return; } }
      // O OK com os controles escondidos e o Play/Pause: os controles sobem
      // com o foco NO PLAY, nao onde ficou da ultima vez (Legendas, Audio,
      // Proporcao). Sem isto o OK seguinte abria aquela folha (#109).
      botao = PLR_PLAY; barraFoco = 0;
      alternarTocando(); acordar(); return;
    }
    // ESQUERDA/DIREITA ESCONDIDOS SAO BUSCA (#121), o gesto do YouTube e da
    // Netflix: os controles sobem com o foco NA BARRA e o primeiro toque ja
    // anda. Antes so acordavam, com o foco nos botoes, e o toque seguinte
    // trocava de botao — "pressiono para avancar e o foco vai para os botoes".
    // A fileira continua a um BAIXO (ou um OK, #109). Canal ao vivo nao tem
    // para onde buscar: la continua so acordando.
    if ((k == SDLK_LEFT || k == SDLK_RIGHT) && !ehCanal()) {
      acordar();
      barraFoco = 1; skipFoco = 0; soBarra = 1;
      if (anim < 0.05f) cheio = 0.0f;   // de tudo apagado: o resto nem comeca a subir
      saltar(k == SDLK_RIGHT ? 1 : -1, e->key.repeat);
      return;
    }
    if (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT) {
      acordar();
      if (trechoPulavel(NULL)) { skipFoco = 1; barraFoco = 1; }
      else if (k == SDLK_DOWN) barraFoco = 0;   // BAIXO revela a fileira
    }
    return;
  }

  // Na busca so com a barra (#128), ESQUERDA/DIREITA seguem buscando sem
  // trazer o resto; qualquer outra tecla traz os controles inteiros e segue
  // valendo pelo caminho de sempre (BAIXO leva a fileira, OK confirma/pausa).
  if (soBarra && !(barraFoco && (k == SDLK_LEFT || k == SDLK_RIGHT))) soBarra = 0;

  // OK no meio de um avanco CONFIRMA o avanco, em vez de alternar play/pausa.
  // Quem aperta o centro com a barra correndo quer parar ali, e alternar o
  // estado deixaria o filme pausado no ponto novo — meio comando executado.
  if (scrubbing && (barraFoco || skipFoco || botao != PLR_REINICIAR) &&
      (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)) {
    terminarSalto();
    acordar();
    return;
  }

  // CONTROLES EM PE: o foco anda pelos botoes e o OK aperta o botao em foco.
  if (skipFoco) {
    double fim;
    if (!trechoPulavel(&fim)) skipFoco = 0;          // o trecho acabou por baixo do foco
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      pularTrecho(puloDestino(fim)); if (comVideo) buscarUsuario(posSeg);
      skipFoco = 0; acordar(); return;
    } else if (k == SDLK_DOWN) { skipFoco = 0; acordar(); return; }
    else if (k == SDLK_UP) { skipFoco = 0; esconderControles(); return; }   // #305
    // CIMA nao abre mais a folha de Audio daqui (pedido do dono, 03/10): acima
    // do botao de pular nao ha nada. Esquerda/direita: nada ao lado.
    else { acordar(); return; }
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    // Na barra o OK pausa/retoma: e o que sobra de util, ja que a barra nao
    // tem acao propria no web.
    if (barraFoco) { alternarTocando(); acordar(); return; }
    ativarBotao(botao, e->key.repeat);
    acordar();
    return;
  }
  // CIMA sobe para a BARRA, que no web e um alvo de foco de verdade
  // (`.player-progress-shell.focused` engorda o trilho de 6 para 10px). Sem
  // isso nao havia como adiantar o filme pela barra — so os saltos de 10s dos
  // botoes, que e o defeito que o dono relatou.
  //
  // DA BARRA PARA CIMA NAO HA NADA (pedido do dono, 03/10): o CIMA na barra
  // abria a folha no Audio, um salto que ninguem pediu no meio de uma busca.
  // Audio e Legendas ficam nos botoes deles. O unico alvo acima da barra e o
  // "Pular abertura", quando existe — e esse esta mesmo em cima dela.
  if (k == SDLK_UP) {
    if (!barraFoco && transporteMoverLinha(-1)) { acordar(); return; }
    if (!barraFoco) barraFoco = 1;
    else if (trechoPulavel(NULL)) skipFoco = 1;
    else { esconderControles(); return; }   // #305: CIMA no topo tira a interface da frente
    acordar();
    return;
  }
  if (barraFoco) {
    // Na barra, ESQUERDA e DIREITA procuram no filme em vez de trocar de botao.
    if (k == SDLK_LEFT)       saltar(-1, e->key.repeat);
    else if (k == SDLK_RIGHT) saltar(1, e->key.repeat);
    else if (k == SDLK_DOWN)  barraFoco = 0;
    acordar();
    return;
  }
  if (k == SDLK_DOWN) {
    if (transporteMoverLinha(1)) { acordar(); return; }
    // BAIXO a partir da fileira significa "tirar os controles da frente".
    // Nao chama acordar(): isso recolocaria a barra no mesmo evento e faria o
    // comando parecer quebrado. O proximo toque direcional a revela de novo.
    esconderControles();
    return;
  }
  // Sem rotacao nas pontas: a fileira e curta e cabe inteira no olhar; dar a
  // volta no fim le como erro, nao como atalho.
  if (k == SDLK_LEFT) transporteMover(-1);
  else if (k == SDLK_RIGHT) transporteMover(1);
  acordar();
}

  // O TMDB DO EPISODIO CHEGOU DEPOIS DO PEDIDO (o fio de episodios enriquece a
  // lista depois de publica-la): pergunta de novo por tmdb_id, uma vez. 2.0.3.
static void introTmdbTardio(Uint32 agora) {
  if (!ehCanal() && introIdx >= 0 && introIdx == idx && introTmdb <= 0) {
    static Uint32 visto;
    if ((Uint32)(agora - visto) > 1000u) {
      int tt, te; long tm;
      const CatItem *cx = item();
      visto = agora;
      tm = tmdbDoTocando(&tt, &te);
      if (tm > 0 && cx) {
        double dA = 0, dP = 0;
        introTmdb = tm;
        if (epE > 0)
          for (int ix = idxAtual(), i = 0; i < cat_n_episodios(ix); i++) {
            const CatEp *ep = cat_episodio(ix, i);
            if (!ep || ep->temporada != epT) continue;
            if (ep->episodio == epE - 1) dA = duracaoTexto(ep->duracao);
            else if (ep->episodio == epE + 1) dP = duracaoTexto(ep->duracao);
          }
        printf("[intro] tmdb %ld chegou depois: pedindo de novo por tmdb_id\n", tm); fflush(stdout);
        intro_pedir_ids_mapeados(cx->imdb, tm, epE > 0 ? tt : 0, epE > 0 ? te : 0, epT, epE, dA, dP);
      }
    }
  }
}

void player_atualizar(float dt, Uint32 agora) {
#ifdef NV_TOUCH_UI
  playerTelaAtualizar();
#endif
  if (retido) { player_validar_retido(agora); return; }
  if (aberto || mini) atualizarIdentidade();
#ifdef NV_TPK
  // A TV recusou um seek e o arquivo nao tem indice nenhum (mkvfix.h): diz,
  // em vez de o video seguir como se a tecla nao tivesse feito nada.
  if (video_tpk_aviso_busca() && comVideo) player_toast(i18n("Este arquivo não deixa avançar ou voltar nesta TV"), 5000u);
#endif
  // DV EM MKV NASCE NO PONTO SALVO: enquanto a retomada nao foi pedida ao
  // player da TV, a troca para o caminho do DV espera (video.c, iniciarDts le o
  // seek pendente). Sem ponto salvo nao ha o que esperar.
  video_dv_segurar(comVideo && !retomadaAplicada && retomarPct > 0 && !ehCanal());
  dvAgora = agora;
  dvTelaAtualizar(dt, agora);
  // AUDIO QUE A TV NAO TOCA (uMS errorCode 200, registro 1545): o video segue
  // mudo, e sem isto a pessoa nao tinha como saber que era a fonte e nao o
  // volume. Um aviso por sessao, 6 s, no lugar do de proporcao.
  if (comVideo && !avisouAudio && video_audio_nao_suportado()) {
    avisouAudio = 1;
    snprintf(toastTexto, sizeof toastTexto, "%s",
             i18n("Esta TV não toca o áudio desta fonte. Troque a fonte ou o áudio."));
    snprintf(toastIcone, sizeof toastIcone, "aj_triangle-alert"); toastCor = 1;
    toastAte = agora + 6000;
  }
  // SEEK RECUSADO 3x pelo pipeline (webOS "seek Failure"): a fonte segue de
  // onde esta; um aviso por sessao em vez de pular o link.
  { static int seekAvisado;
    if (!comVideo) seekAvisado = 0;
    else if (!seekAvisado && (video_seek_desistiu() || seekprobe_resultado(video_url_atual()) == 1)) {
      seekAvisado = 1;
      player_toast_ex(i18n("Não foi possível pular para esse ponto. Seguindo daqui."), 5000, "aj_triangle-alert", 1);
    }
  }
  // F07: the seek cache turned itself off (no free space at open, or the disk
  // filled up mid-session). Playback goes on; one short notice per session.
  if (comVideo) { const char *av = cacheboost_cache_aviso();
    if (av) player_toast_ex(i18n(av), 5000, "aj_database", 1); }
  // REDE CAIU no meio do video (video_reconexao.h): um aviso por tentativa.
  { static int reconVisto;
    int t = comVideo ? video_reconectando() : 0;
    if (t && t != reconVisto) {
      snprintf(toastTexto, sizeof toastTexto, "%s", i18n("Conexão caiu, reconectando…"));
      snprintf(toastIcone, sizeof toastIcone, "aj_wifi-off"); toastCor = 1;
      toastAte = agora + 5000;
    }
    reconVisto = t; }
  janelaPasso(agora);
  if (!aberto) return;

  entrada = anim_mola(entrada, saindo ? 0.0f : 1.0f, dt, NV_MOLA_TELA);
  // ZAPPING: o prazo do debounce vence aqui. O banner segura o OSD longe enquanto
  // ha troca pendente; o deslocamento vira o pedido que o app executa.
  if (ehCanal()) {
    int off = 0;
    bannerAV = anim_mola(bannerAV, zapEst.pend ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
    if (aovivo_zap_pronto(&zapEst, agora, &off)) pedZap = off;
    { int n; if (ltv_dig_pronto(&digitosCanal, agora, &n)) pedNumero = n; }
    avRelogioAoVivo(agora);
  } else {
    bannerAV = 0.0f;
  }
  // Marca o primeiro quadro COM IMAGEM. E daqui que a guia parental conta o
  // tempo dela — contar da abertura da tela faria a guia gastar o prazo
  // enquanto o app ainda procurava fonte, e ela sumiria antes de o filme
  // aparecer.
  // webOS: pronto ja sobe no loadCompleted. A posicao em reproducao e o
  // sinal de quadro usado pelo log do backend, nao apenas a preparacao.
  if (memQuadroPendente && comVideo && video_pronto() && video_tocando() && video_pos() > 0.0) {
    memQuadroPendente = 0;
    memlog_evento("primeiro-quadro");
  }
  if (!inicioImagem && comVideo && video_pronto()) {
    inicioImagem = agora; acordar();
    if (medirAbertura && !ehCanal()) {
      printf("[player] pronto em %u ms desde a abertura (retomada %d%%)\n",
             (unsigned)(SDL_GetTicks() - abertoEm), retomarPct);
      // O TRECHO QUE O LOG NAO SEPARAVA (#202): da URL entregue ao pipeline ate
      // a imagem. O resto do "pronto em" e busca, verificacao e debrid.
      if (loadEm) printf("[player] load->1o quadro %u ms\n", (unsigned)(SDL_GetTicks() - loadEm));
      fflush(stdout);
    }
  }
  if (saindo && saidaIlhaDesde && prepararRetencao &&
      (video_pausa_confirmada() ||
       (Sint32)(SDL_GetTicks() - saidaIlhaDesde) >= (Sint32)PLR_SAIDA_ILHA_TETO_MS)) {
    printf("[player] saida para a ilha em %d ms (pausa %d)\n",
           (int)(Sint32)(SDL_GetTicks() - saidaIlhaDesde), video_pausa_confirmada());
    fflush(stdout);
    entrada = 0.0f;
  }
  if (saindo && entrada < 0.02f) { aberto = 0; saindo = 0; entrada = 0.0f; saidaIlhaDesde = 0; avisarCascaAberto(0); return; }

  // Havendo pipeline, posicao e duracao vem DELE; o dt so serve para as
  // animacoes. O relogio somado continua existindo para quando nao ha video
  // (no Mac, ou se a fonte falhar): sem ele a barra ficaria parada em zero e a
  // tela mentiria dizendo que nada acontece.
  // O retangulo depende da proporcao do QUADRO, e ela so existe quando o
  // videoInfo chega — segundos depois da abertura. Sem esta releitura o modo
  // ficaria calculado com o chute de 16:9 para sempre, e num arquivo 3840x1606
  // (que e o caso real medido nesta TV) o "Original" cortaria a imagem.
#ifdef NV_ANDROID
  if (!comVideo) aspPendente = 0;   // fonte fechada: cancela
  else if (aspPendente && video_pronto()) {
    // video_pronto() = primeiro quadro renderizado (EV_PRIMEIRO_QUADRO).
    // Espera a superficie ESTAVEL (ultima recriacao do HDR/DV, NvPlayer.kt);
    // o prazo de seguranca evita ficar neutro para sempre.
    if (!aspProntoDesde) aspProntoDesde = agora | 1;
    if (!aspLiberarEm) {
      if (video_superficie_estavel()) aspLiberarEm = agora + ASP_ASSENTA_MS;
      else if ((int)(agora - aspProntoDesde) >= (int)ASP_SEGURANCA_MS) aspLiberarEm = agora;
    }
    else if ((int)(agora - aspLiberarEm) >= 0) {
      aspPendente = 0;
      printf("[aspect] aplicado apos primeiro quadro: %d\n", aspecto);
      fflush(stdout);
      aplicarAspecto();
    }
  }
#endif
  if (comVideo) {
    static int ultLarg, ultAlt;
    int lw = video_largura(), lh = video_altura();
    if (lw != ultLarg || lh != ultAlt) { ultLarg = lw; ultAlt = lh; aplicarAspecto(); }
  }

  // RECUO DO VIDEO: alvo pelo painel de creditos, e o caminho ate ele em poucos
  // degraus espacados. `encolheEm` e o proximo instante permitido — sem ele
  // isto viraria uma chamada ao pipeline por quadro.
  encolheAlvo = (posplay_visivel() && !posplay_sobre_video() && comVideo) ? 1.0f : 0.0f;   // agora e o T
  if (encolheT != encolheAlvo && agora >= encolheEm) {
    float passo = 1.0f / (float)PLR_ENC_PASSOS;
    if (encolheT < encolheAlvo) {
      encolheT += passo;
      if (encolheT > encolheAlvo) encolheT = encolheAlvo;
    } else {
      encolheT -= passo;
      if (encolheT < encolheAlvo) encolheT = encolheAlvo;
    }
    encolhe = 1.0f - (1.0f - PLR_ENC_ALVO) * suaveEnc(encolheT);
    encolheEm = agora + PLR_ENC_MS;
    aplicarAspecto();
  }

  // Fim do avanco por inatividade: o controle nao manda KEYUP confiavel, entao
  // quem decide que a pessoa soltou e o silencio.
#ifdef NV_SHOT_HOOKS
  if (shotBusca) {
    if (!scrubbing) scrubInicio = posSeg;
    scrubbing = 1; posVisSolto = 1; scrubUltimo = agora;
  }
#endif
  if (scrubbing && agora - scrubUltimo > salto_fim_ms(ajustes_seekr_ligado())) terminarSalto();

  medirAssistido();
    { static Uint32 vodPosEm; static double vodPos; static int vodLogado;
      unsigned agora = SDL_GetTicks(), buf = video_bufferando_ms();
      double pos = video_pos();
      if (!comVideo || !video_pronto() || ehCanal()) { vodPos = pos; vodPosEm = agora; vodLogado = 0; }
      else if (pos > vodPos + 0.05) { vodPos = pos; vodPosEm = agora; vodLogado = 0; }
#ifdef NV_TPK
      // A seek BACK also moves: the position drops under vodPos and must not
      // read as a stop (log of 23/10).
      else if (pos < vodPos - 0.5) { vodPos = pos; vodPosEm = agora; vodLogado = 0; }
#endif
      // UMA linha por PARADA, nao por quadro: a condicao antiga tinha
      // `|| buf == 0`, e `buf` e o buffering AGORA — que continua 0 enquanto o
      // pipeline nao entrega nada. A propria linha reincidia no frame seguinte
      // (1362 linhas num registro, 1300 em 30 s no harness) e a evidencia do
      // #681 era cortada do log. Agora `vodLogado` e a unica garantia, e ele so
      // volta a zero quando a posicao anda de novo. Pausa DA PESSOA nao e
      // parada: nao conta.
      else if (vodPosEm && (unsigned)(agora - vodPosEm) >= 8000u && !vodLogado && !pausaPessoa) {
        vodLogado = 1;
        { int itens = 0; long bytes = 0; char mem[96] = "";
          tex_estatisticas(&itens, NULL, &bytes, NULL, NULL);
          memlog_amostra(mem, sizeof mem);
          printf("[player] VOD parado pos=%.3f idade=%ums buffering=%u pronto=%d tocando=%d gpu-cache=%d %.1fMB%s quem=pipeline\n",
                 pos, (unsigned)(agora - vodPosEm), buf, video_pronto(), video_tocando(),
                 itens, bytes / 1048576.0, mem); }
        fflush(stdout);
      } }
#ifdef NV_TPK
  vooPasso(agora);
#endif
  if (fontePrincipal() && video_ativo()) {
    double d = video_duracao();
    // AVANCANDO, a posicao e a que o dono escolheu. Ler video_pos() aqui era o
    // que fazia a barra pular de volta a cada repeticao de tecla.
#ifdef NV_TPK
    // Seek in flight: the target, never the stale position (vooAtivo).
    if (!scrubbing && vooAtivo) posSeg = (float)vooAlvo;
    else
#endif
    if (!scrubbing) posSeg = (float)video_pos();
    // Move a janela de colheita da legenda ASS embutida (#92). Barato: so
    // acorda o fio quando a posicao andou meio segundo.
    mkvass_passo(posSeg);
    // Folga do buffer de video para a VARREDURA pausar (webOS informa o fim
    // do buffer; no Tizen video_buffer_fim e 0 = desconhecido, sem pausa).
    { double bf = video_buffer_fim();
      mkvass_folga(bf > 0.5 ? bf - (double)posSeg : -1.0);
      // F05: troca de fonte, seek e buffer curto cancelam/pausam o AutoSync.
      // F06: o ajuste local decide se "Por audio" existe (padrao desligado).
      legsync_auto_habilitar(ajustes_legenda_sync_auto());
      legsync_audio_habilitar(ajustes_legenda_sync_audio());
      legsync_passo(xtream_url_limitada(video_url_atual()) ? "" : video_url_atual(), posSeg, bf > 0.5 ? bf - (double)posSeg : -1.0,
                    scrubbing || video_bufferando_ms() > 0, agora); }
    if (d > 1.0) {
      duracaoSeg = (float)d; duracaoReal = 1;
      durFonteAtualizar(d, agora);
      if (durFonteForaCache) return;
    }
    durFonteContagem(agora);
    // MINIATURAS DO SEEKR: so com a duracao REAL (o servico escolhe a versao
    // da folha por ela) e uma vez por titulo — seekr_pedir ignora o repetido.
    if (d > 60.0 && video_pronto() && !ehCanal() && ajustes_seekr_ligado()) {
      const CatItem *cs = item();
      if (cs && !strncmp(cs->imdb, "tt", 2))
        seekr_pedir(cs->imdb, strcmp(cs->tipo, "series") ? 0 : epT,
                    strcmp(cs->tipo, "series") ? 0 : epE, (long)(d * 1000.0));
    }
    // Retomada so com o pipeline ASSENTADO: pronto + tocando (ou 2,5 s depois
    // de pronto, para quem abre pausado). Seek logo no loadCompleted e o que o
    // uMS recusa com "seek Failure" (#246).
    // O PRELOAD DO CAMINHO PROPRIO (DV/DTS, video_iniciando) nao e "abrir
    // pausado": la o seek recarrega o fluxo inteiro (native-seek-reload) e o
    // primeiro quadro ainda vem sozinho. Espera o `playing`.
    static Uint32 prontoDesde;
    if (retomadaAplicada || !video_pronto()) prontoDesde = 0;
    else if (!prontoDesde) prontoDesde = agora | 1;
    if (!retomadaAplicada && video_pronto() && d>1.0 &&
#ifdef NV_ANDROID
        1
#else
        (video_tocando() || (!video_iniciando() && (int)(agora - prontoDesde) >= 2500))
#endif
        ) {
#ifdef NV_ANDROID
      // O ack chega antes do prepare. Se a ponte/Media3 recusou a posicao,
      // recua ao seek de sempre; pendente nunca vira um segundo seek.
      int estado = retomadaNaPreparacao ? video_retomada_inicial_estado() : -1;
      if (estado != 0) {
        retomadaAplicada = 1;
        if (estado > 0) marco("abrir: ponto salvo na preparacao");
        if (estado < 0 && (retomarFonteSeg >= 0.0 || retomarPct > 0)) {
          if (retomadaNaPreparacao) {
            printf("[player] retomada: a abertura nao aplicou o ponto salvo; seek depois do pronto\n");
            fflush(stdout);
          }
          marco("abrir: seek para o ponto salvo");
          video_buscar(retomarFonteSeg >= 0.0 ? retomarFonteSeg : d * retomarPct / 100.0);
        }
      }
#else
      retomadaAplicada=1;
      if(retomarFonteSeg>=0.0 || retomarPct>0) {
        marco("abrir: seek para o ponto salvo");
        video_buscar(retomarFonteSeg >= 0.0 ? retomarFonteSeg : d*retomarPct/100.0);
      }
#endif
    }
    tocando = video_tocando();
    if (tocando && retomadaAplicada && medirAbertura && !tocouMedido && !ehCanal()) {
      tocouMedido = 1;
      printf("[player] tocando em %u ms desde a abertura (retomada %d%%)\n",
             (unsigned)(SDL_GetTicks() - abertoEm), retomarPct);
      fflush(stdout);
    }
    if (tocando && !scrubbing) retomandoSalto = 0;
    { const CatItem *ci = ehCanal() ? NULL : item();
      // Personal-server check-ins: start once playback really runs, progress
      // every 10 s, pause/resume immediately. Asked for EVERY title because a
      // regular "tt..." title can be playing from a Plex source; each module
      // ignores URLs it did not hand out, so addon playback is untouched.
      if (ci) servidores_reproducao_tick(video_url_atual(), posSeg, duracaoSeg,
                                         video_pronto() && tocando && !scrubbing);
      // A server-native item is not a social/Trakt title: never fed onward.
      if (ci && (jfid_e(ci->imdb) || !identidadeValida())) ci = NULL;
      // "ASSISTINDO AGORA" para os amigos, SO se a pessoa ligou o nivel 2 em
      // Ajustes (recomenda.c decide; com tudo desligado isto nao faz nada).
      // Filme/episodio de verdade, nunca o clipe curto de erro do provedor.
      if (ci && ci->imdb[0] && video_pronto() && duracaoSeg >= 120.0f)
        recomenda_atividade_passo(ci, tocando);
      // ATIVIDADE PARA O SOCIAL (atividade.h): inicio, progresso a cada 5 min
      // tocando e fim (90% ou creditos, pela MESMA regra da saida). Sem envio
      // permitido, atividade.c so acompanha o trecho e nada sai.
      if (ci && ci->imdb[0] && video_pronto() && duracaoSeg >= 120.0f) {
        double cr = credEfetivo();
        atividade_player_passo(ci, epT, epE, posSeg, duracaoSeg, tocando && !scrubbing,
                               concluiuAgora(cr), dt);
      }
    }
    // #202: confere que o pipeline anda na velocidade pedida antes de usa-la.
    // So com o video tocando de verdade: pausa, busca e buffering fecham a
    // janela de medida. "ok" da plataforma com o video a 1x (passthrough na
    // TCL) desfaz o pedido e a folha avisa (faixas_atualizar).
    { int r;
      velmed_pedir(&velMed, video_velocidade_atual());
      r = velmed_passo(&velMed, monoSeg(), video_pos(),
                       comVideo && video_pronto() && tocando && !scrubbing &&
                       !video_bufferando_ms() && !ehCanal());
      if (r == VELMED_CONFIRMOU) vel_log(velMed.efetiva, "confirmada pelo relogio do pipeline");
      else if (r == VELMED_NAO_ANDOU) {
        vel_log(velMed.pedida, "pipeline seguiu a 1x: volta a 1x");
        video_velocidade_recusada();
        velmed_pedir(&velMed, VEL_NORMAL);
      } }
    // A legenda interpola na velocidade MEDIDA (relogio.h).
    relogio_taxa(&relLeg, velMed.efetiva / 100.0);
#ifdef NV_TPK
    /* Leitura carimbada pelo host (video.h, video_pos_amostra): a hora em que
     * a TV leu a posicao, nao a hora em que a resposta chegou aqui. */
    { double ap, ac, ai; unsigned sq; static unsigned sqAnt;
      if (video_pos_amostra(&ap, &ac, &ai, &sq) && sq != sqAnt) {
        sqAnt = sq;
        relogio_amostra_carimbo(&relLeg, ap, ac, ai, tocando && !scrubbing);
      } }
#endif
    relogio_amostra(&relLeg, video_pos(), monoSeg(), tocando && !scrubbing);
    // A cada 10 s: o numero cru do pipeline e o do relogio da legenda, no
    // mesmo instante. A diferenca e o que a interpolacao acrescenta (0..~250).
    { static double ultRel;
      double m = monoSeg();
      if (tocando && m - ultRel >= 10.0) {
        ultRel = m;
        printf("[relogio] pipeline=%.3f legenda=%.3f (%+.0f ms; amostra de %.0f ms atras)\n",
               video_pos(), relogio_ler(&relLeg, m), (relogio_ler(&relLeg, m) - video_pos()) * 1000.0,
               (m - relLeg.ultAgora) * 1000.0);
      } }
  } else if (!comVideo && tocando && !esperandoFonte && !erroFonte) {
    posSeg += dt;
    // Canal ao vivo nao "termina": o relogio reserva estourar em ~1h54 nao pode
    // derrubar o estado para pausado com a transmissao ainda no ar.
    if (posSeg >= duracaoSeg) { posSeg = duracaoSeg; if (!ehCanal()) tocando = 0; }
  }
  // BUSCA SUAVE (ver posVis). Desliza enquanto o dono avanca e ate assentar
  // depois de soltar; no resto do tempo e a posicao de verdade, sem atraso.
  if (scrubbing) {
    posVisSolto = 1;
    double de = posSeg;
#ifdef NV_TPK
    posSeg = anim_clamp(posSeg + seekgesto_quadro(&gesto, &seekPar, agora, dt), 0.0f, duracaoSeg);
#else
    posSeg = anim_clamp(posSeg + salto_quadro(&scrubSalto, scrubDir, agora, dt, duracaoSeg),
                        0.0f, duracaoSeg);
#endif
    if (!scrubToque) recuarAutomatico(de, posSeg);
  }
  if (posVisSolto) {
    posVis = anim_mola2(&posVisV, posVis, posSeg, dt, PLR_BUSCA_MOLA);
    // Assentou, ou o pipeline demorou a confirmar a posicao nova: o teto de
    // 800 ms depois da ultima tecla impede a barra de ficar perseguindo um
    // video_pos() velho.
    if (!scrubbing && (fabsf(posVis - posSeg) < 0.25f || agora - scrubUltimo > 800))
      posVisSolto = 0;
  }
  if (!posVisSolto) { posVis = posSeg; posVisV = 0.0f; }

  // TRAKT "NOW WATCHING" (#179). Ate a 1.5.3 o Trakt so ouvia pause/stop, ao
  // SAIR: nunca havia /scrobble/start, e sem ele o episodio nao aparece em
  // "Now Watching" nem o progresso anda durante a exibicao. Start depois de
  // PLR_SCR_TOCOU_S de reproducao continua (o web espera 15 s: evita mandar a
  // cada troca de fonte); pause ao pausar. Mesma guarda do encerramento: fluxo
  // curto (<120 s, clipe de erro do provedor) e canal ao vivo nao falam.
  { int ok = fontePrincipal() && video_pronto() && video_ativo() && tocando && !scrubbing &&
             !saindo && !erroFonte && !esperandoFonte && !ehCanal() &&
             duracaoSeg >= 120.0f &&
             (partesAtiva() || !player_duracao_suspeita((double)duracaoSeg, duracaoCatalogo()));   // PRONTO do trailer (129 s) nao e deste titulo
    static float tocouS, parcialSeek;
    static int scrubAnterior;
    const CatItem *cs = item();
    if (cs && (jfid_e(cs->imdb) || !identidadeValida())) cs = NULL;
    if (ok && cs && cs->imdb[0]) {
      char id[64];
      if (epT >= 0 && epE > 0) snprintf(id, sizeof id, "%.*s:%d:%d", (int)strcspn(cs->imdb,":"), cs->imdb, epT, epE);
      else snprintf(id, sizeof id, "%s", cs->imdb);
      tocouS += dt;
      if (tocouS >= PLR_SCR_TOCOU_S) {
        { double sp = posSeg, sd = duracaoSeg;
          partesVirt(&sp, &sd, NULL);
          scrobbleTrakt(SCR_EV_TOCANDO, cs, sp, sd, assistidoSeg, 0);
          simkl_scrobble(SCR_EV_TOCANDO, id, sp, sd, assistidoSeg, 0); }
        wsTocando();
      }
      parcialRelogio += dt;
      parcialConta += dt;
      if (parcialRelogio >= PLR_PARCIAL_INTERVALO_S) {
        parcialRelogio = 0.0f;
        int sincronizar = parcialConta >= 300.0f;
        gravarProgressoParcial(sincronizar, "30 s playing");

      }
    } else {
      // Pausou (ou o video parou de andar): so fala se ha um start em pe.
      if (tocouS > 0.0f && cs && cs->imdb[0] && fontePrincipal() && video_pronto() && !ehCanal() &&
          duracaoSeg >= 120.0f && !scrubbing &&
          (partesAtiva() || !player_duracao_suspeita((double)duracaoSeg, duracaoCatalogo()))) {
        char id[64];
        if (epT >= 0 && epE > 0) snprintf(id, sizeof id, "%.*s:%d:%d", (int)strcspn(cs->imdb,":"), cs->imdb, epT, epE);
        else snprintf(id, sizeof id, "%s", cs->imdb);
        { double sp = posSeg, sd = duracaoSeg;
          partesVirt(&sp, &sd, NULL);
          scrobbleTrakt(SCR_EV_PAUSOU, cs, sp, sd, assistidoSeg, 0);
          simkl_scrobble(SCR_EV_PAUSOU, id, sp, sd, assistidoSeg, 0); }
        if (!saindo) wsPausou();
        parcialPausa = 1;
      }
      tocouS = 0.0f;
      parcialRelogio = 0.0f;
    }
    if (parcialPausa && !video_bufferando_ms() && !scrubbing) {
      gravarProgressoParcial(1, "pausa");
      parcialPausa = 0;
    }
    // ~0,7 s depois de soltar um seek, grava o ponto novo (oficial:
    // scheduleProgressSyncAfterSeek). Vale pausado ou tocando.
    if (scrubAnterior && !scrubbing) parcialSeek = PLR_PARCIAL_SEEK_S;
    scrubAnterior = scrubbing;
    if (parcialSeek > 0.0f && (parcialSeek -= dt) <= 0.0f) {
      parcialSeek = 0.0f;
      gravarProgressoParcial(1, "seek");
    } }

  // "A SEGUIR" QUE SOME (#151). Aberto pelo Continuar assistindo, o episodio
  // pode chegar ao player sem a lista de episodios da serie (o card do CW nao
  // a tem, e o pedido do detalhe pode nem ter saido). Sem lista nao ha
  // proximo: nem cartao A seguir, nem botao Proximo, nem autoplay. O player
  // pede a lista ele mesmo, UMA vez por titulo, e so depois que qualquer
  // carregamento em curso terminou vazio. desc_episodios dispara um fio e volta
  // na hora; o quadro nao espera nada.
  //
  // #190: "uma vez por titulo" e por CATALOGO. Toda troca de bloco (a refacao
  // de "Continuar assistindo" roda com o player aberto) zera as faixas de
  // episodio de todos os titulos, e com a marca so por IMDb a lista que o
  // player ja tinha pedido nao voltava mais: folha de episodios vazia e sem
  // A seguir ate sair do player.
  // E por GERACAO DAS FAIXAS (cat_geracao_episodios): a volta do vetor comum de
  // episodios apaga a lista sem subir a revisao, e a marca por IMDb impedia o
  // novo pedido — "[posplay] sem lista" e nenhum cartao A seguir (2.0.3).
  { static char epsPedidoDe[64];
    static unsigned epsPedidoRev, epsPedidoGer;
    const CatItem *ci = item();
    if (epsPedidoRev != cat_revisao() || epsPedidoGer != cat_geracao_episodios()) {
      epsPedidoRev = cat_revisao(); epsPedidoGer = cat_geracao_episodios(); epsPedidoDe[0] = 0; }
    // Tipo incerto guarda T/E e ainda nao e serie. A lista espera o tipo
    // confirmado; other-title pedia a ficha ao tirar o titulo do catalogo.
    if (!ehCanal() && epE > 0 && ci && !strcmp(ci->tipo, "series") &&
        ci->imdb[0] && strcmp(epsPedidoDe, ci->imdb)) {
      int ix = idxAtual();
      if (cat_n_episodios(ix) > 0) {
        snprintf(epsPedidoDe, sizeof epsPedidoDe, "%s", ci->imdb);
      } else if (!desc_episodios_carregando(ix)) {
        snprintf(epsPedidoDe, sizeof epsPedidoDe, "%s", ci->imdb);
        printf("[posplay] sem lista de episodios de %s no player: pedindo de novo\n", ci->imdb);
        fflush(stdout);
        desc_episodios(ix, epT);
      }
    }
    // Um pedido que chegou com outro fio de episodios em voo fica guardado;
    // com o detalhe fechado ninguem mais o soltaria.
    desc_episodios_pendente(); }

  if (comVideo) creditosauto_fonte(&creditosAuto, video_url_atual());
  // PULAR SOZINHO (#329/#331): abertura/resumo/creditos com marcador e ajuste ligado. Uma vez por
  // trecho (voltar de proposito para dentro dele nao pula de novo), so tocando de verdade.
  if (!ehCanal() && comVideo && tocando && video_ativo() && video_pronto() && !partesAtiva() &&
      retomadaAplicada && !scrubbing && !retomandoSalto && duracaoReal &&
      !saindo && !erroFonte && !esperandoFonte && !pedFontes && !pedFaixas &&
      !stream_folha_aberta() && !faixas_aberta() && !episodios_aberto() &&
      !episodios_menu_aberto_qualquer() && !pausao_visivel() &&
      !video_bufferando_ms() && !creditosAuto.buscaPendente) {
    double fimT = 0.0; int tipoT = 0;
    if (intro_auto_ativo(posSeg, &fimT, &tipoT) &&
        intro_auto_pular(tipoT, fimT, duracaoSeg, ajustes_pular_abertura(), ajustes_pular_resumo(),
                         ajustes_pular_creditos() && !creditosAuto.fonteMudou) &&
        !intro_auto_ja(tipoT, fimT)) {
      intro_auto_marcar(tipoT, fimT);
      pularTrecho(tipoT == INTRO_CREDITOS ? fimT : puloDestino(fimT));
      creditosauto_buscar(&creditosAuto, posSeg);
      video_buscar(posSeg);
      player_toast(tipoT == INTRO_RESUMO ? i18n("Resumo pulado") :
                   tipoT == INTRO_CREDITOS ? i18n("Créditos pulados") : i18n("Abertura pulada"), 2200);
      printf("[intro] pulo automatico: tipo=%d ate %.0fs\n", tipoT, fimT); fflush(stdout);
    }
  }

  // PÓS-REPRODUÇÃO: o proximo episodio ou os relacionados, no fim do titulo.
  { const CatItem *ci = item();
    int eSerie = ci && !strcmp(ci->tipo, "series");
    int aguardaBusca = creditosauto_aguardar(&creditosAuto, video_pos(),
      comVideo && video_pronto() && tocando && !scrubbing && !video_bufferando_ms(),
      comVideo && video_terminou(), SDL_GetTicks());
    posplay_aguardar_busca(scrubbing || aguardaBusca);
    // Canal ao vivo nao tem "fim": sem a guarda, o relogio reserva cruzaria os
    // 90% em ~1h42 de exibicao e abriria o painel de relacionados no meio da
    // programacao.
    // O fim de uma parte do meio nao e o fim do filme: sem relacionados nem
    // "o que achou"; a parte seguinte entra sozinha (app.c, vigiarPartes).
    int meio = partesMeio();
    if (meio && posplay_visivel()) posplay_fechar();
    if (!ehCanal() && !meio) {
      posplay_atualizar(dt, agora, posSeg, duracaoSeg, eSerie, idxAtual(),
                        eSerie && player_janela_proximo(0));
    }
    // "O QUE ACHOU?" (reacao.h): no mesmo instante do pos-reproducao do filme,
    // e na serie so no fim da temporada ou no ultimo episodio disponivel.
    //
    // SERIE SO COM A LISTA DE EPISODIOS NA MAO: sem ela, player_proximo_episodio
    // e NULL e todo episodio pareceria "o ultimo disponivel" (o #151 mostra que
    // a lista pode chegar tarde ou nao chegar).
    int listaConhecida = !eSerie ||
      (cat_n_episodios(idxAtual()) > 0 && !desc_episodios_carregando(idxAtual()));
    if (!ehCanal() && !meio && ci && comVideo && video_pronto() && duracaoSeg >= 120.0f &&
        listaConhecida) {
      const CatEp *px = eSerie ? player_proximo_episodio() : NULL;
      double cr = credEfetivo();
      reacao_player_episodio(eSerie ? epT : 0, eSerie ? epE : 0);
      reacao_player_atualizar(dt, agora, ci, eSerie, posSeg, duracaoSeg, cr,
                              px != NULL, px && px->temporada != epT);
    } else reacao_player_atualizar(dt, agora, NULL, 0, 0, 0, 0, 0, 0);
    // webOS conserva terminou depois de buscar: confira o fim e o recuo.
    // Espera a lista so enquanto carrega, por no maximo 8 s no EOF.
    int noFim = !meio && !saindo && fontePrincipal() && video_ativo() && video_terminou() &&
      !ehCanal() && !scrubbing && !retomandoSalto && !aguardaBusca &&
      duracaoSeg > 1.0f && posSeg >= duracaoSeg - 1.0f && video_pos() >= duracaoSeg - 1.0;
    double agoraEof = monoSeg();
    if (!noFim) eofDesde = -1.0;
    else {
      if (eofDesde < 0.0) eofDesde = agoraEof;
      if ((!eSerie || !desc_episodios_carregando(idxAtual()) || agoraEof - eofDesde >= 8.0) &&
          (!player_proximo_episodio() || !posplay_aguarda_escolha())) { saindo = 1; pediuSair = 1; }
    }
  }
  // Com o painel no ar os controles nao somem: eles sao a saida do dono.
  if (posplay_visivel()) ultimoInput = agora;

  // Pausado, os controles ficam. Sumir com eles deixaria o usuario diante de um
  // quadro parado sem nenhuma pista de que foi ele quem pausou.
  // A HAND ON THE REMOTE COUNTS AS ACTIVITY, not only a key. Pointer motion never
  // reaches here as an event (main.c consumes it in the pointer, and inside a
  // single target the hover only fires when the target CHANGES), so the 4 s hide
  // clock kept running under a moving pointer: it only ever read `ultimoInput`,
  // which only a key updates.
  { Uint32 mov = ponteiro_last_motion();
    if (mov && (Sint32)(mov - ultimoInput) > 0) { ultimoInput = mov; pausao_activity(); } }
  if (visivel && tocando && !player_carregando() && !episodios_aberto() &&
      !stream_folha_aberta() && !faixas_aberta() && agora - ultimoInput > PLR_ESCONDE_MS
#ifdef NV_TPK
      && !vooAtivo   // the bar stays while the TV is still seeking
#endif
      ) visivel = 0;
  if (epE > 0 && !strstr(linhaEp, " · ")) player_definir_episodio(epT, epE);
  introTmdbTardio(agora);

  // PAINEL DE PAUSA. A condicao e a traducao de canShowPauseOverlay
  // (playerScreen.js:7299): pausado, com imagem na tela, sem nenhuma folha
  // aberta por cima. `!player_carregando()` cobre o `loadingVisible` do web e
  // `!player_janela_proximo(0)` cobre o cartao de proximo episodio (:8241) — os dois sao
  // convites a uma acao, e um painel informativo nao pode competir com eles.
  //
  // `!posplay_visivel()` nao vem do web: la o pos-reproducao e outra tela. Aqui
  // os dois sao faixa inferior, o pos-reproducao desenha DEPOIS (mais abaixo
  // nesta funcao) e cobriria a ficha. Pausar no fim de um episodio e comum
  // justamente porque a contagem esta correndo — sem esta linha o painel
  // subiria invisivel, atras dela.
  {
    int podeSubir =
                   // `!scrubbing`: durante o avanco o video fica pausado por
                   // conta do player, e nao porque o dono parou para olhar. Sem
                   // esta condicao o painel subia sozinho no meio de um avanco
                   // longo — o "componente que aparece quando ta pausado
                   // piscando" do relato.
                   !ehCanal() && !abrecine_osd_calado(&cine) && !tocando && !scrubbing && !retomandoSalto && !saindo && !erroFonte &&
                   !player_carregando() &&
                   // O preroll do caminho proprio (DV/DTS) nao e pausa: o
                   // fluxo carregou e o primeiro quadro ainda nao veio. Era o
                   // "comeca pausado" do Dolby Vision em MKV (video_iniciando).
                   !video_iniciando() && !dvtela_visivel() &&
                   !episodios_aberto() && !stream_folha_aberta() &&
                   !faixas_aberta() && !player_janela_proximo(0) && !posplay_visivel();
    pausao_atualizar(dt, agora, podeSubir,
                   idxAtual(), item() ? item()->imdb : "", epT, epE, linhaEp,
                   podeSubir && ajustes_pausa_overlay() ? sinopseEpisodioAtual() : "");
  }
  // Com o painel de pe os controles SAEM de cena. Este ponto ja foi das duas
  // formas: com os controles visiveis o dono achou pior e pediu de volta o
  // comportamento original, com o painel ocupando o rodape sozinho, so que
  // ancorado mais abaixo — onde a barra ficaria. Um lugar, um conteudo.
  if (pausao_visivel()) visivel = 0;
  // ABERTURA SEM OSD (dono, 14/10): o filme entra suave, sem controles, ate a
  // primeira tecla — tambem no Dolby Vision que comeca pausado.
  if (abrecine_osd_calado(&cine) && !ehCanal() && !erroFonte) visivel = 0;
  // A tela do DV cobre tudo: os controles nao sobem por baixo dela.
  if (dvtela_visivel()) visivel = 0;
  // O painel de pos-reproducao tambem toma o rodape para si. BAIXO devolve os
  // controles (posplay_evento responde 2), que e a saida documentada.
  if (posplay_visivel()) visivel = 0;

  anim = anim_mola(anim, visivel ? 1.0f : 0.0f, dt,
                   visivel ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  // ABRINDO A FONTE, SO A ILHA (dono, 03/10: "quando clicar para ver o filme
  // so mostrar o componente de carregando sem mostrar o player; agora o player
  // fica sempre visivel"). player_abrir pede os controles (visivel = 1) e o
  // prazo de esconder nao corre com o fluxo abrindo, entao titulo, barra e
  // botoes ficavam de pe em volta da ilha de "Abrindo fonte" ate a imagem
  // chegar. Agora o OSD fica em zero enquanto abre e o prazo e empurrado: na
  // primeira imagem os controles entram pela mola e somem no prazo normal.
  // O erro nao passa aqui (o modal da ilha e a saida), nem o canal, cujo
  // banner de zapping e o OSD proprio contam a troca.
  if (player_carregando() && !erroFonte && !ehCanal()) { anim = 0.0f; ultimoInput = agora; }
  // Com os controles apagando, `cheio` NAO volta a 1: o resto nao pode
  // reaparecer no meio do fade-out da barra. Ele so sobe quando alguem pediu
  // os controles inteiros com eles em pe.
  if (visivel || soBarra)
    cheio = anim_mola(cheio, soBarra ? 0.0f : 1.0f, dt,
                      soBarra ? NV_MOLA_DESFOCO : NV_MOLA_FOCO);
  { float alvoF = (barraFoco && !ponteiroNoPlayer()) ? 0.0f : 1.0f;
    fileira = anim_mola(fileira, alvoF, dt, alvoF > fileira ? NV_MOLA_FOCO : NV_MOLA_DESFOCO); }
  PlrAcao acoes[PLR_NBTNS]; int nAcoes = transporteAcoes(acoes), habilitadas[PLR_NBTNS] = {0};
  for (int i = 0; i < nAcoes; i++) habilitadas[acoes[i].id] = acoes[i].habilitada;
  if (botao < 0 || botao >= PLR_NBTNS || !habilitadas[botao]) botao = PLR_PLAY;
  for (int i = 0; i < PLR_NBTNS; i++) {
    // Foco na barra: o botao fecha (antes ficava aberto sem texto).
    float alvo = (visivel && habilitadas[i] && botao == i && !barraFoco) ? 1.0f : 0.0f;
    focoB[i] = anim_mola(focoB[i], alvo, dt,
                         alvo > focoB[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
}

// MINIATURA DO SEEKR: UMA ILHA acima da barra (Glass UI), so enquanto a
// pessoa procura. 384x216 com raio 17 dentro do miolo (raio 26) e o tempo do
// QUADRO na base da propria ilha, com o que falta ao lado — saiu a chapa
// preta, o anel branco de 2 px e a pilula preta sobre a imagem. Na fita, a
// anterior e a seguinte (256x144, apagadas) ficam centradas na altura da
// atual, cada uma com o seu tempo. Sem quadro pronto a ilha ja nasce no lugar
// com o tempo certo e o anel de 12 pontos: o Seekr nao parece desligado.
// O tempo e o do QUADRO (cue), nao a posicao crua (seekrvtt.h).
// Status is shown only while scrubbing, in the same material as the preview.
// These lines use fixed translated labels and numeric usage/times only;
// neither credentials nor provider responses enter the OSD.
static int seekrStatusLinhas(int est, const SeekrUso *uso, int temChave,
                             char linhas[3][192]) {
  int n = 1;
  char hora[48];
  memset(linhas, 0, 3 * 192);
  if (est == SEEKR_PRONTO || est == SEEKR_BUSCANDO ||
      (est == SEEKR_DESLIGADO && temChave) ||
      est < SEEKR_DESLIGADO || est > SEEKR_RELOGIO_INDISPONIVEL) return 0;
  if (est == SEEKR_DESLIGADO)
    snprintf(linhas[0], 192, "Seekr · %s", i18n("sem chave"));
  else snprintf(linhas[0], 192, "%s", i18n(seekr_estado_rotulo(est)));
  if (est == SEEKR_LIMITE_LOCAL && uso) {
    if (!uso->persistente) {
      snprintf(linhas[0], 192, "%s", i18n(seekr_estado_rotulo(SEEKR_ARMAZENAMENTO_INDISPONIVEL)));
    } else {
      snprintf(linhas[n++], 192, i18n("%d de %d consultas hoje (UTC)"), uso->usadas, uso->limite);
      if (uso->relogioAtrasado)
        snprintf(linhas[n++], 192, "%s", i18n(seekr_estado_rotulo(SEEKR_RELOGIO_INDISPONIVEL)));
      else if (seekr_horario_local(uso->reinicioUtc, hora, sizeof hora))
        snprintf(linhas[n++], 192, i18n("Renova em %s (hora local)"), hora);
    }
  } else if (est == SEEKR_LIMITE_PROVEDOR && uso) {
    if (uso->relogioAtrasado)
      snprintf(linhas[n++], 192, "%s", i18n(seekr_estado_rotulo(SEEKR_RELOGIO_INDISPONIVEL)));
    else if (seekr_horario_local(uso->retryUtc, hora, sizeof hora))
      snprintf(linhas[n++], 192, i18n("Tente após %s (hora local)"), hora);
  }
  return n;
}

static void seekrStatus(float bx, float bw, float frac, float yBarra, float a, int est) {
  char linhas[3][192];
  SeekrUso uso;
  int n, k;
  float w = 360.0f, h, x, y;
  if (!scrubbing) return;
  seekr_uso(&uso);
  n = seekrStatusLinhas(est, &uso, seekr_tem_chave(), linhas);
  if (!n) return;
  for (k = 0; k < n; k++) {
    float medida = (float)txt_largura(k ? TXT_G18M : TXT_G20B, linhas[k]) + 48.0f;
    if (medida > w) w = medida;
  }
  if (w > 760.0f) w = 760.0f;
  h = 32.0f + (float)n * 30.0f;
  x = bx + bw * frac - w * 0.5f;
  if (x < PLR_MARGEM) x = PLR_MARGEM;
  if (x + w > bx + bw) x = bx + bw - w;
  y = yBarra - 24.0f - h;
  plrui_material((GfxRect){x, y, w, h}, 26.0f, 0, a);
  for (k = 0; k < n; k++) {
    int tinta = k ? 180 : 243;
    TxtLinha l = txt_linha_corta(k ? TXT_G18M : TXT_G20B, linhas[k],
                                tinta, tinta, tinta, 255, w - 48.0f);
    txt_desenhar_alpha(l, x + (w - (float)l.w) * 0.5f,
                       y + 16.0f + (float)k * 30.0f + (30.0f - (float)l.h) * 0.5f, a);
  }
}

static void seekrMiniatura(float bx, float bw, float frac, float yBarra, float a) {
  GLuint t[3] = { 0, 0, 0 };
  double cue[3] = { -1, -1, -1 };
  int fita, n, k, est;
  const float w = 384.0f, h = 216.0f, ws = 256.0f, hs = 144.0f, vao = 12.0f, linha = 52.0f;
  float x, y, tot, xs[3];
  Uint32 agora = SDL_GetTicks();
  if (ehCanal() || !(scrubbing || posVisSolto) || a < 0.05f || !ajustes_seekr_habilitado()) return;
  est = seekr_tem_chave() ? seekr_estado() : SEEKR_DESLIGADO;
  if (est != SEEKR_PRONTO && est != SEEKR_BUSCANDO) {
    seekrStatus(bx, bw, frac, yBarra, a, est);
    return;
  }
  seekr_definir_ajuste_ms((long)ajustes_seekr_ajuste_s() * 1000L);
  fita = ajustes_seekr_fita();
  n = fita ? 3 : 1;
  { float pv = posVisSolto ? posVis : posSeg;
  if (est == SEEKR_PRONTO) seekr_quadros(pv, n, t, cue);
  // A sincronia ja entra no tempo do quadro; sem cue ainda (a folha chegando),
  // a posicao com o mesmo ajuste.
  if (cue[n == 1 ? 0 : 1] < 0.0) cue[n == 1 ? 0 : 1] = pv + ajustes_seekr_ajuste_s(); }
  tot = fita ? 20.0f + ws * 2.0f + w + vao * 2.0f : w + 20.0f;
  x = bx + bw * frac - tot * 0.5f;
  if (x < PLR_MARGEM) x = PLR_MARGEM;
  if (x + tot > bx + bw) x = bx + bw - tot;
  y = yBarra - 24.0f - (10.0f + h + linha);
  plrui_material((GfxRect){ x, y, tot, 10.0f + h + linha }, 26.0f, 0, a);
  if (fita) { xs[0] = x + 10.0f; xs[1] = xs[0] + ws + vao; xs[2] = xs[1] + w + vao; }
  else xs[0] = x + 10.0f;
  for (k = 0; k < n; k++) {
    int atual = (n == 1 || k == 1);
    float qw = atual ? w : ws, qh = atual ? h : hs;
    float qx = xs[k], qy = y + 10.0f + (h - qh) * 0.5f, ca = atual ? 1.0f : 0.6f;
    GfxRect q = { qx, qy, qw, qh };
    char rot[32];
    if (cue[k] < 0.0) continue;
    if (t[k]) {
      gfx_tex_aspect_atual = 16.0f / 9.0f;
      gfx_rect(q, t[k], GFX_CARD, 0, 0, 0, 17.0f / qh, 0, 0, 0, a * ca);
      gfx_tex_aspect_atual = 0.0f;
      // A atual ainda chegando (seekr da a ultima usada no lugar): o anel por
      // cima, para nao parecer que o quadro e deste ponto.
      if (atual && seekr_quadro_velho()) {
        gfx_cor(q, 17.0f / qh, 1, 1, 1, 0.05f * a);
        plrui_anel(qx + qw * 0.5f, qy + qh * 0.5f, 44.0f, 1, agora, a);
      }
    } else {
      if (ajustes_vidro()) gfx_cor(q, 17.0f / qh, 1, 1, 1, 0.05f * a);
      else gfx_cor(q, 17.0f / qh, 0.125f, 0.129f, 0.149f, a);
      if (atual) plrui_anel(qx + qw * 0.5f, qy + qh * 0.5f, 44.0f, 1, agora, a);
    }
    plrui_tempo(rot, sizeof rot, cue[k]);
    { float yc = y + 10.0f + h + linha * 0.5f;
      if (atual) {
        TxtLinha lt = txt_linha(TXT_G26B, rot, 243, 242, 239, 255), lr = { 0 };
        float tw = (float)lt.w;
        if (!fita) {
          char r[32], d[24];
          plrui_tempo(d, sizeof d, duracaoSeg - cue[k]);
          snprintf(r, sizeof r, "\xe2\x88\x92%s", d);
          lr = txt_linha(TXT_G18M, r, 243, 242, 239, 128);
          tw += 10.0f + (float)lr.w;
        }
        txt_desenhar_alpha(lt, qx + (qw - tw) * 0.5f, yc - (float)lt.h * 0.5f, a);
        if (lr.w) txt_desenhar_alpha(lr, qx + (qw - tw) * 0.5f + lt.w + 10.0f, yc - (float)lr.h * 0.5f + 2.0f, a);
      } else {
        TxtLinha lt = txt_linha(TXT_G20B, rot, 243, 242, 239, 140);
        txt_desenhar_alpha(lt, qx + (qw - (float)lt.w) * 0.5f, yc - (float)lt.h * 0.5f, a);
      } }
  }
}

// --- icones -----------------------------------------------------------------
// ARQUIVOS DE VERDADE, de art/icones (os .svg do app web rasterizados a 128px).
// Antes cada glifo era montado com as primitivas — o play de um triangulo, a
// pausa de dois retangulos, a legenda de barras, o aspecto de quatro linhas de
// 3px — e cada um era uma aproximacao do original.
//
// A cor continua vindo daqui: gfx_icone desenha com GFX_MARCA, que tira a forma
// do ALPHA do arquivo, entao o mesmo PNG serve escuro sobre o circulo branco do
// foco e claro sobre o circulo translucido.
//
// O icone e o rotulo de cada botao do OSD (os icones do mockup, pl_*). O
// rotulo vai DENTRO da pilula de foco; o do Play diz o que o OK vai fazer.
static const char *iconeBotao(int i) {
  switch (i) {
    case PLR_PLAY:      return tocando ? "pl_pause-f" : "pl_play-f";
    case PLR_CC:        return "pl_captions";
    case PLR_AUDIO:     return "pl_audio-lines";
    case PLR_ASPECTO:   return "pl_ratio";
    case PLR_FONTES:    return "pl_layers";
    case PLR_PROXIMO:   return "pl_chevron-right";
    case PLR_REINICIAR: return "pl_rotate-ccw";
    case PLR_NESTE:     return "pl_users";
    default:            return epE > 0 ? "pl_list-video" : "pl_film";
  }
}
static const char *rotuloBotao(int i) {
  switch (i) {
    case PLR_PLAY:      return tocando ? "Pausar" : "Continuar";
    case PLR_CC:        return "Legendas";
    case PLR_AUDIO:     return "\xc3\x81udio";
    case PLR_ASPECTO:   return "Propor\xc3\xa7\xc3\xa3o";
    case PLR_FONTES:    return "Fontes";
    case PLR_PROXIMO:   return "Próximo episódio";
    case PLR_REINICIAR: return "Assistir do começo";
    case PLR_NESTE:     return "Neste episódio";
    default:            return ehCanal() ? "Guia" : epE > 0 ? "Epis\xc3\xb3""dios" : "Relacionados";
  }
}

static void corLegenda(int i,int *r,int *g,int *b){
  static const unsigned char c[VIDEO_LEG_NCORES][3]={
    {255,255,255},{255,222,48},{64,224,112},{78,156,255},{255,80,80},{18,18,18}};
  if(i<0||i>=VIDEO_LEG_NCORES)i=0;*r=c[i][0];*g=c[i][1];*b=c[i][2];
}

// Um bloco ASS na tela: quem manda em cada coisa.
//   tamanho, fonte, borda, opacidade, fundo -> SEMPRE a pessoa (acessibilidade)
//   cor  -> o arquivo, a nao ser que a pessoa tenha mexido na cor (PLR_LEG_COR)
//   lugar -> \pos / \an do arquivo; sem eles, a posicao da folha
// O \pos vem em PlayResX/PlayResY do cabecalho e vira pixel de tela por regra
// de tres. A ancora ASS e a do libass: 1-3 base, 4-6 meio, 7-9 topo; 1/4/7
// esquerda, 2/5/8 centro, 3/6/9 direita.
#define PLR_LEG_LINHAS 6
#define PLR_LEG_LARG   1660.0f
typedef struct { TxtLinha cor[PLR_LEG_LINHAS], borda[PLR_LEG_LINHAS]; int n; float w, h; } LegBloco;

static void blocoLinha(LegBloco *bl, TxtEstilo est, const char *linha, int r, int g, int b,
                       TxtFamilia fam, int enf) {
  char visual[768];
  if (bl->n >= PLR_LEG_LINHAS) return;
  // Arabic/Hebrew: shape + visual order of the ALREADY WRAPPED line (bidi.c).
  // Latin text comes back byte-identical.
  if (txt_bidi_legenda(fam, est, linha, visual, sizeof visual) > 0) linha = visual;
  bl->cor[bl->n]   = txt_linha_corta_enfase(est, linha, r, g, b, 255, PLR_LEG_LARG, fam, enf);
  { unsigned cor = ajustes_leg_borda_rgb(legEstilo.corBorda);
    bl->borda[bl->n] = legEstilo.borda ? txt_linha_corta_enfase(est, linha, (cor >> 16) & 255, (cor >> 8) & 255, cor & 255, 255, PLR_LEG_LARG, fam, enf) : (TxtLinha){0}; }
  if (bl->cor[bl->n].w > bl->w) bl->w = bl->cor[bl->n].w;
  bl->h += bl->cor[bl->n].h + (bl->n ? 5 : 0);
  bl->n++;
}

// QUEBRA POR PALAVRA (#156). Cada linha do arquivo ia inteira para o corte com
// reticencias: uma fala longa numa linha so (comum em SRT externo, que nao
// quebra) virava "... ele disse que…" e o resto da fala sumia. Agora a linha
// quebra onde passa da largura, como o player do sistema faz com a legenda
// embutida. O \n do arquivo continua sendo quebra dura. Medir cada tentativa
// rasteriza, como em txt_bloco; o cache de linhas do text.c devolve as mesmas
// no quadro seguinte.
static void montarBloco(const LegendaCue *c, TxtEstilo est, int r, int g, int b, LegBloco *bl) {
  char texto[768], *linha, *salva;
  TxtFamilia fam = legfontes_caminho(0) ? TXT_FAMILIA_IMPORTADA1 : (TxtFamilia)legEstilo.familia;
  int enf = (c->negrito || legEstilo.negrito ? TXT_ENF_NEGRITO : 0) | (c->italico ? TXT_ENF_ITALICO : 0);
  bl->n = 0; bl->w = 0; bl->h = 0;
  snprintf(texto, sizeof texto, "%s", c->texto);
  linha = strtok_r(texto, "\n", &salva);
  while (linha && bl->n < PLR_LEG_LINHAS) {
    char atual[768] = "", tent[768];
    char *palavra, *sp;
    for (palavra = strtok_r(linha, " ", &sp); palavra; palavra = strtok_r(NULL, " ", &sp)) {
      snprintf(tent, sizeof tent, "%s%s%s", atual, atual[0] ? " " : "", palavra);
      if (atual[0] &&
          txt_linha_corta_enfase(est, tent, r, g, b, 255, 1e9f, fam, enf).w > PLR_LEG_LARG) {
        blocoLinha(bl, est, atual, r, g, b, fam, enf);
        snprintf(atual, sizeof atual, "%s", palavra);
      } else {
        snprintf(atual, sizeof atual, "%s", tent);
      }
    }
    if (atual[0]) blocoLinha(bl, est, atual, r, g, b, fam, enf);
    linha = strtok_r(NULL, "\n", &salva);
  }
}

static void desenharBloco(const LegBloco *bl, float x0, float y, float alpha, int alinha) {
  int i;
  for (i = 0; i < bl->n; i++) {
    TxtLinha l = bl->cor[i];
    // alinha: 0 esquerda (x0 e a borda esquerda), 1 centro (x0 e o centro),
    // 2 direita (x0 e a borda direita).
    float x = alinha == 0 ? x0 : alinha == 1 ? x0 - l.w * .5f : x0 - l.w;
    if (legEstilo.fundo) { float fa = legEstilo.fundo * .16f * alpha; gfx_cor((GfxRect){x-18,y-6,l.w+36,l.h+12},.16f,0,0,0,fa); }
    txt_desenhar_borda(bl->borda[i], x, y, legEstilo.borda, alpha);
    txt_desenhar_alpha(l, x, y, alpha); y += l.h + 5;
  }
}

// LIMPA o texto que o AVPlay entrega no onsubtitlechange (#122): SRT vem com
// <i>, <b>, <font ...> e <br>; ASS com {\\tags} e \\N. O overlay desenha texto
// puro, linha por linha. Exposto para a regressao.
void player_limpar_legenda_nativa(char *s) {
  char *r = s, *w = s;
  while (*r) {
    if (*r == '<') {
      if (!strncasecmp(r, "<br", 3)) *w++ = '\n';
      while (*r && *r != '>') r++;
      if (*r) r++;
    } else if (*r == '{' && r[1] == '\\') {
      while (*r && *r != '}') r++;
      if (*r) r++;
    } else if (*r == '\\' && (r[1] == 'N' || r[1] == 'n')) { *w++ = '\n'; r += 2; }
    else if (*r == '\\' && r[1] == 'h') { *w++ = ' '; r += 2; }
    else if (*r == '\r') r++;
    else if (!strncmp(r, "&amp;", 5)) { *w++ = '&'; r += 5; }
    else if (!strncmp(r, "&lt;", 4))  { *w++ = '<'; r += 4; }
    else if (!strncmp(r, "&gt;", 4))  { *w++ = '>'; r += 4; }
    else if (!strncmp(r, "&quot;", 6)) { *w++ = '"'; r += 6; }
    else *w++ = *r++;
  }
  *w = 0;
  // Sem linhas vazias nas pontas: um "\n" final viraria um bloco mais alto.
  while (w > s && (w[-1] == '\n' || w[-1] == ' ')) *--w = 0;
  r = s; while (*r == '\n' || *r == ' ') r++;
  if (r != s) memmove(s, r, strlen(r) + 1);
}

int player_texto_legenda_nativa(char *dst, int tam) {
  if (!video_legenda_nativa(dst, tam)) return 0;
  player_limpar_legenda_nativa(dst);
  return dst[0] != 0;
}

/* O uMS da C9 limita fonte e escala. OpenSubtitles passa por este overlay
 * SDL/GLES, exatamente como o overlay HTML do app web. Desde o #92 tambem
 * desenha ASS: varios blocos ao mesmo tempo, cada um no seu lugar. */
/* Onde o video esta NESTE quadro — e ali que o ASS e composto: as placas do
 * arquivo sao posicionadas em cima da imagem, nao da tela. Segue o mesmo
 * caminho do plano de hardware: miniatura, animacao da janela, modo de
 * proporcao (inclusive o retangulo virtual que passa da tela no zoom) e o
 * recuo do painel de creditos. Sem as dimensoes do quadro, a tela inteira. */
static PlrRect areaVideoLegenda(void) {
  PlrRect r = { 0.0f, 0.0f, NV_TELA_W, NV_TELA_H }, d, o;
  if (video_largura() < 2 || video_altura() < 2) return r;
  if (mini) return miniDestino();
  if (janAtiva) return janAgora;
  r = aspectoRect(aspecto);
  if (encolhe > 0.999f) return r;
  d = aspectoVisivel(aspecto);
  o = destinoComRecuo(d);
  r.x = o.x + (r.x - d.x) * encolhe;
  r.y = o.y + (r.y - d.y) * encolhe;
  r.w *= encolhe; r.h *= encolhe;
  return r;
}

/* Tamanho da folha aplicado ao ASS: 120 % e o padrao do app, e no ASS o
 * padrao e o tamanho que o autor escolheu. O resto e proporcional — tudo
 * cresce junto, como o sub-scale do mpv, sem mexer em cor, borda ou lugar. */
static double escalaFonteAss(void) {
  int pct = legEstilo.tamanho;
#ifdef NV_TPK
  /* Styled ASS on the .tpk: its own dialogue size (Subtitles > Style), 100 %
   * = as authored; libass scales only the dialogue (selective font scale). */
  if (ASS_NOVO) return assperf_get(AP_ASSTAM) / 100.0;
#endif
  if (pct < TXT_LEG_PCT_MIN) pct = TXT_LEG_PCT_MIN;
  if (pct > TXT_LEG_PCT_MAX) pct = TXT_LEG_PCT_MAX;
  return pct / 120.0;
}

// Base (y) da pilha de baixo da legenda principal: posicao da folha, barra de
// controles e botao de proximo episodio. Compartilhada com a segunda legenda
// em modo "Junto da principal" (R4).
// Lugar que a segunda legenda ocupa EMBAIXO da principal ("Junto da
// principal"): a principal sobe isso. Zero fora desse modo.
static float leg2Reserva;
float player_base_legenda(float baseNormal, int cartaoProximoNoAr) {
  return cartaoProximoNoAr ? 690.f : baseNormal;
}
static float baseLegendaPrincipal(void) {
  float base = (visivel && !faixas_estilo_topo() ? 760.f : 1000.f) - leg2Reserva;
  if (visivel && transporteToque() && !faixas_estilo_topo())
    base = fminf(base, (transporteBarraY() - 48.0f) * gfx_escala_ui() - leg2Reserva);
  // A LEGENDA SOBE SO ENQUANTO O CARTAO ESTA NA TELA (2.0.3). Subia pela
  // JANELA do cartao (a oferta do proximo), nao pelo cartao: ela continua
  // verdadeira depois de Voltar/Baixo dispensar o cartao (e ate o fim do
  // episodio), e tambem quando o cartao nao chega a subir (sem proximo na
  // lista do posplay). O relato da Samsung: o cartao apareceu no meio do
  // episodio, foi dispensado, e a legenda ficou no meio da tela ate o episodio
  // seguinte. Vale para todo alvo: o overlay de legenda e o mesmo.
  base = player_base_legenda(base, posplay_sobre_video());
  // Abaixo do padrao (3) o passo e de 20 e nao de 48: com 48 as posicoes 1 e 2
  // punham a base em 1144 e 1096, fora da tela de 1080 — a legenda sumia, e a
  // previa da barra de estilo mostrou isso na primeira captura.
  if (legEstilo.posicao >= 3) base -= (legEstilo.posicao - 3) * 48.f;
  else base += (3 - legEstilo.posicao) * 20.f;
  return base;
}

static void desenharLegendaPrincipal(float *topoPilha);
static void desenharLegendaExternaCorpo(void);
/* A legenda e conteudo do filme, nao interface: o brilho do player nao a toca. */
static void desenharLegendaExterna(void) {
  const float osd = gfx_osd_mult;
  gfx_osd_mult = 1.0f;
  desenharLegendaExternaCorpo();
  gfx_osd_mult = osd;
}
static void desenharLegendaExternaCorpo(void) {
  /* F04: SECOND SUBTITLE (legendasui.h). Before every primary early return
   * (libass, no cues) so both show in every branch; its own document and
   * offset. R4: "No topo" draws it first, in the top band; "Junto da
   * principal" draws it AFTER the primary, stacked right above it, so it needs
   * where the primary's bottom stack starts (topo). Geometry is explicit. */
#ifdef NV_TPK
  if (video_pos_incerta()) return;   // the same wait as the primary (seek not landed yet)
#endif
  PlrRect av = areaVideoLegenda();
  int junto = ajustes_leg2_junto();
  LegendasGeo g2 = { av.x, av.y, av.w, av.h, anim * entrada, entrada, posLegenda(), junto, 0.0f };
  float topo = -1.0f;
  // "Junto da principal": A SECUNDARIA FICA EMBAIXO (dono, 05/10/2026; antes
  // ia em cima). A principal sobe a altura dela e ela termina onde a principal
  // terminava. Com ASS (libass) a principal nao se move — ali a secundaria
  // continua em cima, estimada como sempre.
  leg2Reserva = 0.0f;
  g2.baseY = baseLegendaPrincipal();   // safe anchor for an explicit secondary position
  if (junto) legendasui_banda_zerar(); else legendasui_desenhar_secundaria(&g2);
  if (junto && (!assrender_ativo() || assrender_texto_simples())) {
    float h2 = legendasui_altura_secundaria(&g2);
    float baseOrig = baseLegendaPrincipal();
    if (h2 > 0.0f) leg2Reserva = h2 + 10.0f;
    desenharLegendaPrincipal(&topo);
    leg2Reserva = 0.0f;
    g2.baseY = baseOrig + 10.0f;   // o bloco termina na base original
    legendasui_desenhar_secundaria(&g2);
    return;
  }
  desenharLegendaPrincipal(&topo);
  if (junto) { g2.baseY = topo >= 0.0f ? topo : baseLegendaPrincipal(); legendasui_desenhar_secundaria(&g2); }
}

#ifdef NV_TPK
/* Como a externa acima: o brilho do player (gfx_osd_mult, 0,80 no padrao e
 * menos com o degrau automatico) multiplicava a COR do libass — a mascara
 * branca do typeset virava cinza e as bordas claras escureciam (Kaguya,
 * INBOX). A legenda e conteudo do filme: desenha a 1,0. */
static void desenharLegendaPrincipalCorpo(float *topoPilha);
static void desenharLegendaPrincipal(float *topoPilha) {
  const float osd = gfx_osd_mult;
  gfx_osd_mult = 1.0f;
  desenharLegendaPrincipalCorpo(topoPilha);
  gfx_osd_mult = osd;
}
static void desenharLegendaPrincipalCorpo(float *topoPilha){
#else
static void desenharLegendaPrincipal(float *topoPilha){
#endif
#ifdef NV_TPK
  /* Seek feito e a TV ainda nao disse onde caiu (quadro-chave, ate segundos
   * antes do alvo): desenhar agora mostraria o letreiro de outra cena. */
  if (video_pos_incerta()) return;
#endif
  // Gate collector output, preserving native callback rendering below.
  int overlay = faixas_legenda_overlay();
  PlrRect bitmapArea = areaVideoLegenda();
  float bitmapAlpha = (legEstilo.opacidade==3?.25f:legEstilo.opacidade==2?.5f:
                      legEstilo.opacidade==1?.75f:1.f) * entrada;
  if (overlay && video_dts_legenda_desenhar(posLegenda(), legEstilo.atrasoMs,
        bitmapArea.x, bitmapArea.y, bitmapArea.w, bitmapArea.h, bitmapAlpha)) {
    imgsub_gl_limpar();
    return;
  }
  // PGS decodificado pelo app (mkvass): so ha quadros quando a faixa e de imagem.
  if (overlay && imgsub_desenhar(legsync_posicao(posLegenda()) + legsync_offset_ms(legEstilo.atrasoMs) / 1000.0,
        bitmapArea.x, bitmapArea.y, bitmapArea.w, bitmapArea.h, bitmapAlpha)) return;
  /* Authored ASS keeps its styles until the corresponding user setting
   * changes. Size remains proportional; opacity and delay apply globally. */
  int r, g, b;
  // F05: manual + automatico aceito, UMA vez, so para o documento dono do
  // overlay (sem ele, o manual intacto). O estilo guarda o ATRASO da pessoa
  // (positivo = a legenda aparece mais tarde); legsync_offset_ms ja devolve o
  // deslocamento de busca (positivo adianta, como sempre).
  int atraso = legsync_offset_ms(legEstilo.atrasoMs);
  // Escala/trechos aceitos: o instante na legenda, nao no video (o mesmo
  // para libass e para o desenho do app).
  double posLeg = legsync_posicao(posLegenda());
  assrender_aplicar_invalidacao();
  assrender_definir_cor(0, 0, 0, 0);
  if (overlay && assrender_ativo()) {
    PlrRect area = areaVideoLegenda();
    {
      static const char *const families[] = {
        "Inter Display", "LG Display", "Droid Sans",
        "Montserrat", "Roboto", "Atkinson Hyperlegible Next",
        "JetBrains Sans"
      };
      PlainAssStyle style = {0};
      int pct = legEstilo.tamanho, fam = legEstilo.familia;
      float base = baseLegendaPrincipal();
      if (pct < TXT_LEG_PCT_MIN) pct = TXT_LEG_PCT_MIN; if (pct > TXT_LEG_PCT_MAX) pct = TXT_LEG_PCT_MAX;
      style.size = (pct / 10) * 4;
      if (fam < 0 || fam >= (int)(sizeof families / sizeof families[0])) fam = 0;
      snprintf(style.font, sizeof style.font, "%s", families[fam]);
      corLegenda(legEstilo.cor, &r, &g, &b);
      style.rgb = (r << 16) | (g << 8) | b;
      style.background = legEstilo.fundo >= 0 && legEstilo.fundo <= 4 ? legEstilo.fundo : 0;
      style.border = legEstilo.borda; style.bold = legEstilo.negrito;
      style.outlineRgb = (int)ajustes_leg_borda_rgb(legEstilo.corBorda);
      style.overrides = legTocado;
      if (assrender_fonte_local_ativa(legfontes_caminho(0))) {
        snprintf(style.font, sizeof style.font, "%s", legfontes_nome_ass(0));
        style.overrides |= PLAIN_ASS_FONT;
      }
      style.marginV = (int)(1080.f - base);
      assrender_definir_texto_estilo(&style);
      /* Plain captions follow the old screen-space layout; authored ASS
       * still follows the video rectangle and the author's styling. */
      if (assrender_texto_simples()) area = (PlrRect){0, 0, NV_TELA_W, NV_TELA_H};
    }
    float opacidade = legEstilo.opacidade==3?.25f:legEstilo.opacidade==2?.5f:
                      legEstilo.opacidade==1?.75f:1.f;
    float alpha = opacidade * entrada;
#ifdef NV_TPK
    /* Styled ASS: the signs and typesetting (white masks) are part of the
     * picture; the opacity from Subtitles > Style goes to the dialogue only,
     * inside libass (assperf AP_ASSOPAC). Plain text follows the sheet. */
    if (ASS_NOVO && !assrender_texto_simples()) alpha = entrada;
#endif
    assrender_pixels_por_unidade(gfx_px_por_unidade());
    assrender_definir_layout(area.x, area.y, area.w, area.h,
                             assrender_texto_simples() ? 1920 : video_largura(),
                             assrender_texto_simples() ? 1080 : video_altura(),
                             assrender_texto_simples() ? 1.0 : escalaFonteAss());
#ifdef NV_TPK
    assrender_definir_fps(legimagem_fps());   // frame grid (assrender.c)
#endif
    assrender_desenhar(posLeg, atraso, alpha,
                        0, 0, NV_TELA_W, NV_TELA_H);
    // R4: libass nao informa onde pintou. Estima duas linhas no tamanho da
    // folha acima da base, para a segunda legenda empilhada nao cobri-las.
    *topoPilha = baseLegendaPrincipal() - (2.0f * 58.0f + 5.0f) * (float)escalaFonteAss();
    return;
  }
  LegendaCue cues[LEGENDA_SIMULTANEAS];
  int n = overlay ? legenda_cues(posLeg, atraso, cues, LEGENDA_SIMULTANEAS) : 0, i;
  // Sem legenda externa, a EMBUTIDA que o player nativo nao desenha (#122):
  // o mesmo overlay, com a mesma folha de estilo da pessoa.
  if (n <= 0 && comVideo && player_texto_legenda_nativa(cues[0].texto, sizeof cues[0].texto)) {
    cues[0].inicio = cues[0].fim = 0; cues[0].an = 0; cues[0].negrito = cues[0].italico = 0;
    cues[0].cor = -1; cues[0].posX = cues[0].posY = -1.f; cues[0].resX = cues[0].resY = 0.f;
    cues[0].ordem = 0;
    n = 1;
  }
  // PREVIA DA BARRA DE ESTILO (faixas.c): sem fala neste instante, uma linha
  // de exemplo com o estilo da pessoa, para tamanho, cor e posicao terem o que
  // mostrar. So quando e o app que desenha (no ASS o lugar e do arquivo).
  if (n <= 0 && faixas_estilo_topo()) {
    memset(&cues[0], 0, sizeof cues[0]);
    snprintf(cues[0].texto, sizeof cues[0].texto, "%s", i18n("Assim a legenda vai aparecer"));
    cues[0].cor = -1; cues[0].posX = cues[0].posY = -1.f;
    n = 1;
  }
  if (n <= 0) return;
  int pct=legEstilo.tamanho;if(pct<TXT_LEG_PCT_MIN)pct=TXT_LEG_PCT_MIN;if(pct>TXT_LEG_PCT_MAX)pct=TXT_LEG_PCT_MAX;pct=(pct/10)*10;
  TxtEstilo est=(TxtEstilo)(TXT_LEG_50+(pct-50)/10);corLegenda(legEstilo.cor,&r,&g,&b);
  float alpha=(legEstilo.opacidade==3?.25f:legEstilo.opacidade==2?.5f:legEstilo.opacidade==1?.75f:1.f)*entrada;
  // A pilha "normal" (sem \pos e sem \an, ou \an 2): empilha de baixo para
  // cima a partir da base da folha, como o SRT sempre fez.
  // Com a barra de estilo no topo a base e a da reproducao sem controles: a
  // previa mostra onde a legenda vai tocar, nao onde ela fica com a barra.
  float base = baseLegendaPrincipal();
  *topoPilha = base;
  float baseTopo = legendasui_topo_livre(80.f);  // pilha do \an8 (letreiros), abaixo da 2a legenda (F04)
  float baseMeio = NV_TELA_H * .5f;
  for (i = 0; i < n; i++) {
    const LegendaCue *c = &cues[i];
    LegBloco bl;
    int cr = r, cg = g, cb = b;
    int an = c->an > 0 && c->an <= 9 ? c->an : 2;
    int alinha = (an - 1) % 3;        // 0 esq, 1 centro, 2 dir
    int fila   = (an - 1) / 3;        // 0 base, 1 meio, 2 topo
    if (c->cor >= 0 && !player_leg_estilo_tocado(PLR_LEG_COR)) { cr = (c->cor >> 16) & 255; cg = (c->cor >> 8) & 255; cb = c->cor & 255; }
    montarBloco(c, est, cr, cg, cb, &bl);
    if (!bl.n) continue;
    if (c->posX >= 0.f && c->posY >= 0.f && c->resX > 0.f && c->resY > 0.f) {
      // \pos: o ponto e a ANCORA do bloco (libass): x conforme alinha, y a
      // base/meio/topo do bloco conforme a fila.
      float x = c->posX * NV_TELA_W / c->resX, y = c->posY * NV_TELA_H / c->resY;
      float yTopo = fila == 0 ? y - bl.h : fila == 1 ? y - bl.h * .5f : y;
      if (yTopo < 0) yTopo = 0; if (yTopo + bl.h > NV_TELA_H) yTopo = NV_TELA_H - bl.h;
      desenharBloco(&bl, x, yTopo, alpha, alinha);
      continue;
    }
    { float margem = 60.f;
      float x = alinha == 0 ? margem : alinha == 1 ? NV_TELA_W * .5f : NV_TELA_W - margem;
      if (fila == 2)      { desenharBloco(&bl, x, baseTopo, alpha, alinha); baseTopo += bl.h + 10.f; }
      else if (fila == 1) { desenharBloco(&bl, x, baseMeio - bl.h * .5f, alpha, alinha); baseMeio += bl.h + 10.f; }
      else                { base -= bl.h; desenharBloco(&bl, x, base, alpha, alinha); *topoPilha = base; base -= 10.f; } }
  }
}

// PONTEIRO (#99). Os alvos mexem nas MESMAS variaveis das setas (botao,
// barraFoco, skipFoco) e todos acordam os controles: mexer a mao sobre o
// video e o "toque" que faz a barra subir. Clicar no video (fora dos
// controles) e o OK dos controles escondidos: Play/Pause.
static float barraPtrX, barraPtrW = NV_TELA_BASE_W;
static int ponteiroNoPlayer(void) {
  // "O que achou?" com os controles escondidos e dono das teclas
  // (reacao_evento): o alvo de tela inteira do player cobriria as pilulas dele.
  return ponteiro_ativo() && aberto && !saindo &&
         !posplay_visivel() && !pausao_visivel() && !(reacao_visivel() && !visivel);
}
// DEDO (#216): tocar no video com os controles escondidos so os mostra (o
// gesto de todo player de celular); com eles na tela, tocar no video os
// esconde. Play/Pause por dedo e o botao. O Magic Remote segue como antes.
static int visivelAntesDoToque;
static void ponteiroAcordar(int a, int b) {
  (void)a; (void)b;
  if (ponteiro_toque()) visivelAntesDoToque = visivel;
  soBarra = 0; acordar();
}
static void ponteiroPlayPause(int a, int b) {
  (void)a; (void)b;
  if (ponteiro_toque()) { if (visivelAntesDoToque) visivel = 0; return; }
  botao = PLR_PLAY; barraFoco = 0; skipFoco = 0;
  alternarTocando(); acordar();
}
static void ponteiroBotao(int i, int b) {
  (void)b;
  if (i < 0 || i >= PLR_NBTNS || !transporteHabilitado(i)) return;
  botao = i; barraFoco = 0; skipFoco = 0; acordar();
}
static void ponteiroAtivarBotao(int i, int b) {
  (void)b;
  if (!transporteHabilitado(i)) return;
  ponteiroBotao(i, 0);
  // Conservar a prioridade dos paineis, como o Enter sintetizado do ponteiro.
  SDL_Event e; SDL_zero(e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  player_evento(&e);
  e.type = SDL_KEYUP; player_evento(&e);
}
static void ponteiroBotaoInativo(int i, int b) { (void)i; (void)b; }
static void ponteiroBarra(int a, int b) { (void)a; (void)b; barraFoco = 1; skipFoco = 0; acordar(); }
// Clique na barra PROCURA ali — o que o mouse faz em qualquer player. Canal ao
// vivo nao tem para onde ir.
static void ponteiroBuscar(int a, int b) {
  float f;
  (void)a; (void)b;
  acordar();
  if (ehCanal() || duracaoSeg <= 0.0f || barraPtrW <= 0.0f) return;
  // A barra mora na tela virtual do OSD; o ponteiro, na real.
  f = anim_clamp((ponteiro_x() / gfx_escala_ui() - barraPtrX) / barraPtrW, 0.0f, 1.0f);
  if (ponteiro_toque()) {
    // ARRASTAR NA BARRA (#216): o mesmo avanco das setas (saltar) — video
    // pausado, posSeg na mao do dedo, UMA busca no fim (terminarSalto, depois
    // de PLR_SCRUB_FIM_MS sem movimento).
    if (!scrubbing) iniciarSalto(1);
    barraFoco = 1; skipFoco = 0;
    posSeg = f * duracaoSeg;
    scrubUltimo = SDL_GetTicks();
    return;
  }
  double de = comVideo ? video_pos() : posSeg;
  posplay_busca_usuario();
  posSeg = f * duracaoSeg;
  recuarAutomatico(de, posSeg);
  if (comVideo) {
    if (posSeg >= de) creditosauto_buscar(&creditosAuto, posSeg);
    buscarUsuario(posSeg);
  }
}
// Marker timing alone cannot prove that the current source is playing. Once it
// played, pause/scrub keep the button; a new source (tocarFonteInterno) clears it.
static int skipDisponivel(void) {
  if (player_carregando() || erroFonte || saindo || cine.veu > 0.01f || dvtela_ativa()) return 0;
  if (!comVideo) return 1;   // host/screenshot: no pipeline to wait for
  if (!skipLiberado)
    skipLiberado = fontePrincipal() && retomadaAplicada && video_tocando() && !video_iniciando() &&
#ifdef NV_ANDROID
                   video_quadro_visto() &&
#endif
                   !video_bufferando_ms();
  return skipLiberado && fontePrincipal();
}
static void ponteiroSkip(int a, int b) { (void)a; (void)b; if (!skipDisponivel()) return; skipFoco = 1; barraFoco = 1; acordar(); }

static void desenharAcoesEpisodioCorpo(void);
// O botao de pular e OSD: camada ampliada (escala.h), na tela virtual.
static void desenharAcoesEpisodio(void){
  ESCALA_INI();
  desenharAcoesEpisodioCorpo();
  ESCALA_FIM();
}
static void desenharAcoesEpisodioCorpo(void){
  const CatEp *prox=player_proximo_episodio();double fim;int tipo=0;
  if (!skipDisponivel()) { skipFoco = 0; return; }
  intro_definir_duracao(duracaoReal ? (double)duracaoSeg : 0.0, epE <= 0);
  int trecho=intro_botao(posSeg,monoSeg(),visivel,skipFoco&&visivel,&fim,&tipo);
  // A CAIXA DE "Próximo episódio" QUE FICAVA AQUI FOI APAGADA. Era um retangulo
  // de posicao fixa em {420,720} que caia por cima da barra de tempo, sem foco
  // e sem parecer clicavel — o dono fotografou. O proximo episodio agora e um
  // cartao no molde do de episodios.c, desenhado por posplay.c, ancorado acima
  // dos controles e com a mesma janela de aparicao que esta caixa usava
  // (ofertaProximo). Duas interfaces para a mesma coisa na mesma tela era o
  // defeito de fundo; sobrou uma.
  (void)prox;
  if(!trecho){skipFoco=0;return;}
  {
    // PULAR ABERTURA/RESUMO/CREDITOS (Glass UI): pilula de verdade no material
    // da ilha, na margem de 96 (era 64). Com o OSD escondido ela fica sozinha
    // embaixo a esquerda, ja cheia no acento — OK pula direto; com o OSD de pe
    // sobe para a direita, acima da barra, e so e cheia com o foco nela.
    const char *rot=tipo==INTRO_RESUMO?"Pular resumo":
                    tipo==INTRO_CREDITOS?"Pular cr\xc3\xa9" "ditos":"Pular abertura";
    int sel=(skipFoco&&visivel)||!visivel;
    TxtLinha t=txt_linha(TXT_G21B,rot,255,255,255,255);
    const float h=64.0f;
    float w=22.0f+26.0f+12.0f+(float)t.w+28.0f;
    float k=anim, xa=posplay_visivel() ? NV_VTELA_W-PLR_MARGEM-w : PLR_MARGEM, ya=NV_VTELA_H-120.0f;
    float xb=NV_VTELA_W-PLR_MARGEM-w, yb=transporteBarraY()-36.0f-h;
    GfxRect p={xa+(xb-xa)*k,ya+(yb-ya)*k,w,h};
    int tinta=sel?plrui_tinta():243;
    float kt=tinta/255.0f;
    if (ponteiroNoPlayer()) ponteiro_alvo(p.x, p.y, p.w, p.h, ponteiroSkip, NULL, 0, 0);
    if(!visivel) gfx_veu_css((GfxRect){0,NV_VTELA_H-300.0f,NV_VTELA_W,300.0f},0,1.0f,1.0f,0.60f*entrada*(1.0f-anim));
    if(sel) plrui_pilula_foco(p,entrada);
    else plrui_disco_osd(p,entrada);
    gfx_icone((GfxRect){p.x+22.0f,p.y+(h-26.0f)*0.5f,26.0f,26.0f},"pl_skip-forward",kt,kt,kt,entrada);
    t=txt_linha(TXT_G21B,rot,tinta,tinta,tinta,255);
    txt_desenhar_alpha(t,p.x+22.0f+26.0f+12.0f,p.y+(h-(float)t.h)*0.5f,entrada);
  }
}

// --- O CORPO DA ILHA AO ABRIR A FONTE E NO ERRO ------------------------------
// A ARTE DO TITULO NO TOPO DO CARTAO (dono, 04/10: "o opening source podia
// mostrar a arte da logo"): a mesma logo do hero/detalhe (logotitulo.h), numa
// caixa reservada e centrada; sem logo, o nome em texto na MESMA caixa.
#define CAR_LOGO_H 92.0f
#define CAR_LOGO_W 440.0f
static void arteCarregando(float x, float y, float w, float a) {
  const CatItem *ci = (!ehCanal()) ? item() : NULL;
  const char *u = ci ? logotitulo_url(ci, CAR_LOGO_W) : NULL;
  GLuint t = u ? tex_obter_larg_qualquer(u, CAR_LOGO_W) : 0;
  float asp = t ? tex_aspecto(u) : 0.0f;
  if (t && asp > 0.01f) {
    float lw = CAR_LOGO_W, lh = lw / asp;
    if (lh > CAR_LOGO_H) { lh = CAR_LOGO_H; lw = lh * asp; }
    logotitulo_desenhar(ci, "", TXT_G30B, x + (w - lw) * 0.5f, y, CAR_LOGO_W, CAR_LOGO_H, 0.0f, a);
  } else if (ci && ci->titulo[0]) {
    TxtLinha l = txt_linha_corta(TXT_G30B, ci->titulo, 245, 248, 255, 255, w - 24.0f);
    txt_desenhar_alpha(l, x + (w - (float)l.w) * 0.5f, y + (CAR_LOGO_H - (float)l.h) * 0.5f, a);
  }
}
// AS LINHAS DE DETALHE do cartao expandido: so o que o app SABE neste instante.
// Nome da fonte, addon e tamanho vem da propria fonte; "Rede" e a medida real do
// host dela (streamfit), que so existe onde a rede e conhecida (Android) e ja
// houve diagnostico ou reproducao recente. Velocidade de download, latencia e
// bitrate AO VIVO nao existem enquanto a fonte abre (o pipeline so informa
// ativo/pronto, ver o trilho abaixo): nao entram, em vez de um numero inventado.
// NUNCA a URL: ela pode carregar a chave do debrid.
typedef struct { char rot[40]; char val[220]; } AbrLinha;
#define ABR_MAX 6
static int abrFitIdx = -2;
static char abrFit1[160], abrFit2[160];
static int linhasAbrindo(const Stream *st, AbrLinha *L) {
  int n = 0;
  if (!st) return 0;
  if (st->rotulo[0]) { snprintf(L[n].rot, sizeof L[n].rot, "Fonte"); snprintf(L[n].val, sizeof L[n].val, "%s", st->rotulo); n++; }
  if (st->provedor[0]) { snprintf(L[n].rot, sizeof L[n].rot, "Addon"); snprintf(L[n].val, sizeof L[n].val, "%s", st->provedor); n++; }
  if (st->tamanhoMB > 0) {
    snprintf(L[n].rot, sizeof L[n].rot, "Tamanho");
    if (st->tamanhoMB >= 1024) snprintf(L[n].val, sizeof L[n].val, "%.1f GB", st->tamanhoMB / 1024.0);
    else snprintf(L[n].val, sizeof L[n].val, "%ld MB", st->tamanhoMB);
    plrui_decimal(L[n].val);
    n++;
  }
  if (!st->url[0] && st->infoHash[0] && st->temSemeadores && n < ABR_MAX) {
    snprintf(L[n].rot, sizeof L[n].rot, "Seeds");
    snprintf(L[n].val, sizeof L[n].val, "%d", st->semeadores);
    n++;
  }
  if (abrFitIdx != stream_atual()) {   // a medida e relida so quando a fonte muda
    abrFitIdx = stream_atual();
    stream_fit_abrindo(st, abrFit1, sizeof abrFit1, abrFit2, sizeof abrFit2);
  }
  if (abrFit1[0]) {
    snprintf(L[n].rot, sizeof L[n].rot, "Rede"); snprintf(L[n].val, sizeof L[n].val, "%s", abrFit1); n++;
    if (abrFit2[0]) { L[n].rot[0] = 0; snprintf(L[n].val, sizeof L[n].val, "%s", abrFit2); n++; }
  }
  return n;
}
// Alturas (22 de respiro, a logo, o titulo, ...). O compacto reserva sempre a
// fileira das marcas: a troca de fonte nao pode mudar a altura do cartao.
#define ABR_COMPACTO_H (22.0f + CAR_LOGO_H + 16.0f + 32.0f + 10.0f + 24.0f + 22.0f)
static float alturaExpandido(void) {
  AbrLinha L[ABR_MAX];
  const Stream *st = stream_item(stream_atual());
  float h = 22.0f + CAR_LOGO_H + 16.0f + 32.0f + 6.0f;
  if (linhaEp[0]) h += 24.0f + 10.0f;
  if (st && (stream_selos_ha(st) || st->badges)) h += 24.0f + 14.0f;
  h += 1.0f + 10.0f + (float)linhasAbrindo(st, L) * 30.0f + 8.0f;
  h += 24.0f + 14.0f + 4.0f + 22.0f;
  return h;
}
// A linha da explicacao (inicio.h), embaixo do cartao: texto a esquerda e a
// dica "OK Ajustes" na ponta. So existe com motivo, e a altura acompanha.
#define ABR_MOTIVO_H 40.0f
static float alturaCarregando(void) {
  return (abrindoExp ? alturaExpandido() : ABR_COMPACTO_H) + (motivoInicio[0] ? ABR_MOTIVO_H : 0.0f);
}
// A mola que cruza o compacto e o expandido (chamada por quadro, fora do corpo).
static void abrindoAtualizar(Uint32 agora) {
  float dt = abrindoUlt ? (float)(agora - abrindoUlt) / 1000.0f : 1.0f / 60.0f;
  abrindoUlt = agora; abrindoAgora = agora;
  if (dt > 0.1f) dt = 0.1f;
  if (ajustes_animacoes_reduzidas()) abrindoT = abrindoExp ? 1.0f : 0.0f;
  else abrindoT = anim_mola(abrindoT, abrindoExp ? 1.0f : 0.0f, dt, 12.0f);
}
static void motivoDesenhar(GfxRect r, float a) {
  float x = r.x + 18.0f, w = r.w - 36.0f, yc = r.y + r.h - 22.0f - ABR_MOTIVO_H * 0.5f + 6.0f;
  const char *k[2], *rt[2];
  int nk = 0;
  float dw = 0.0f;
  TxtLinha l;
  if (!motivoInicio[0] && !(stream_n() > 0 && addons_busca_parcial())) return;
  if (motivoInicio[0]) {
    k[nk] = "OK"; rt[nk] = "Ajustes"; nk++;
    dw = 150.0f;
  }
  if (stream_n() > 0 && addons_busca_parcial()) {
    k[nk] = "OK"; rt[nk] = "Fontes"; nk++;
    dw = fmaxf(dw, 150.0f);
  }
  gfx_cor((GfxRect){ x, yc - ABR_MOTIVO_H * 0.5f, w, 1.0f }, 0.0f, 1, 1, 1, 0.07f * a);
  if (motivoInicio[0]) {
    l = txt_linha_corta(TXT_G19M, motivoInicio, 255, 196, 90, 235, w - dw - 24.0f);
    txt_desenhar_alpha(l, x + 6.0f, yc + 4.0f - (float)l.h * 0.5f, a);
  }
  plrui_dicas_video(k, rt, nk, x + w - 6.0f, yc + 4.0f, 1, a * 0.85f);
}
static void corpoCarregando(GfxRect r, float a, void *u) {
  float x = r.x + 18.0f, w = r.w - 36.0f, y = r.y + 22.0f;
  Uint32 agora = SDL_GetTicks();
  const Stream *st = stream_item(stream_atual());
  float t = abrindoT, ac = a * (1.0f - t), ae = a * t, yc;
  AbrLinha L[ABR_MAX];
  int nL, i;
  char fd[48];
  (void)u;
  fd[0] = 0;
  if (tentativaN >= 2 && tentativaM >= tentativaN)
    snprintf(fd, sizeof fd, i18n("Fonte %d de %d"), tentativaN, tentativaM);
  arteCarregando(x, y, w, a);
  y += CAR_LOGO_H + 16.0f;
  // O ESSENCIAL, nos dois estados: o ponto que respira e o estado.
  plrui_respira(x + 6.0f + 5.0f, y + 16.0f, 10.0f, agora, a);
  { TxtLinha l = txt_linha(TXT_G26B, "Abrindo fonte", 243, 242, 239, 255);
    txt_desenhar_alpha(l, x + 6.0f + 10.0f + 16.0f, y + 16.0f - (float)l.h * 0.5f, a); }
  if (dvtela_visivel() && dvtela_variante() == DVT_VAR_DETALHADA) {
    dvtela_abertura_marca(1, x + w - 260.0f, r.y + 62.0f, a);
  }
  // A dica de tecla na ponta direita da linha do estado: no compacto leva aos
  // detalhes, no expandido volta. Discreta (o apoio cinza das outras dicas).
  if (ac > 0.01f) { const char *k[1] = { "Baixo" }, *rt[1] = { "Detalhes" };
    plrui_dicas_video(k, rt, 1, x + w - 6.0f, y + 16.0f, 1, ac * 0.85f); }
  if (ae > 0.01f) { const char *k[1] = { "Cima" }, *rt[1] = { "Recolher" };
    plrui_dicas_video(k, rt, 1, x + w - 6.0f, y + 16.0f, 1, ae * 0.85f); }
  yc = y + 32.0f;

  // COMPACTO: as marcas do que esta fonte tem (resolucao, HDR/DV, audio), no
  // estilo cinza do app, e o "Fonte 2 de 3" miudo na ponta direita.
  if (ac > 0.01f) {
    float yy = yc + 10.0f;
    if (st && stream_selos_ha(st)) stream_selos_fileira(st, x + 32.0f, yy, w * 0.62f, 24.0f, .52f, ac);
    else if (st && st->badges) badges_desenhar_tom(st->badges, x + 32.0f, yy, w * 0.62f, 24.0f, .52f, .52f, .52f, ac);
    if (fd[0]) {
      TxtLinha l = txt_linha(TXT_ILHA_APOIO, fd, 243, 242, 239, 140);
      txt_desenhar_alpha(l, x + w - 6.0f - (float)l.w, yy + 12.0f - (float)l.h * 0.5f, ac);
    }
  }

  // EXPANDIDO: o episodio, as marcas e as linhas de detalhe com valor real.
  if (ae > 0.01f) {
    float yy = yc + 6.0f;
    float lx = x + 32.0f;
    if (linhaEp[0]) {
      TxtLinha l = txt_linha_corta(TXT_ILHA_SUB, linhaEp, 243, 242, 239, 153, w - 32.0f);
      txt_desenhar_alpha(l, lx, yy, ae);
      yy += 24.0f + 10.0f;
    }
    if (st && (stream_selos_ha(st) || st->badges)) {
      if (stream_selos_ha(st)) stream_selos_fileira(st, lx, yy, w * 0.62f, 24.0f, .52f, ae);
      else badges_desenhar_tom(st->badges, lx, yy, w * 0.62f, 24.0f, .52f, .52f, .52f, ae);
      yy += 24.0f + 14.0f;
    }
    gfx_cor((GfxRect){ x, yy, w, 1.0f }, 0.0f, 1, 1, 1, 0.07f * ae);
    yy += 1.0f + 10.0f;
    nL = linhasAbrindo(st, L);
    for (i = 0; i < nL; i++) {
      float cy = yy + 15.0f;
      if (L[i].rot[0]) {
        TxtLinha rl = txt_linha(TXT_ILHA_APOIO, L[i].rot, 243, 242, 239, 115);
        txt_desenhar_alpha(rl, x + 6.0f, cy - (float)rl.h * 0.5f, ae);
      }
      { TxtLinha vl = txt_linha_corta(TXT_G19M, L[i].val, 243, 242, 239, 215, w * 0.74f);
        txt_desenhar_alpha(vl, x + w - 6.0f - (float)vl.w, cy - (float)vl.h * 0.5f, ae); }
      yy += 30.0f;
    }
    yy += 8.0f;
    // Ha quanto tempo esta abrindo (relogio do proprio player) e a tentativa.
    { char el[48];
      unsigned sec = abrindoDesde ? (unsigned)((abrindoAgora - abrindoDesde) / 1000u) : 0u;
      TxtLinha l;
      snprintf(el, sizeof el, i18n("Abrindo há %u s"), sec);
      l = txt_linha(TXT_G19M, el, 243, 242, 239, 140);
      txt_desenhar_alpha(l, x + 6.0f, yy + 12.0f - (float)l.h * 0.5f, ae);
      if (fd[0]) {
        l = txt_linha(TXT_G19M, fd, 243, 242, 239, 140);
        txt_desenhar_alpha(l, x + w - 6.0f - (float)l.w, yy + 12.0f - (float)l.h * 0.5f, ae);
      } }
    yy += 24.0f + 14.0f;
    // O TRILHO E INDETERMINADO, DE PROPOSITO. O pipeline nao da porcentagem
    // de abertura: video.h so diz ativo/pronto (loadCompleted), e o buffer_fim
    // antes do pronto e contra uma duracao que ainda nao existe. Uma barra que
    // "enche" com o tempo ou com as duas etapas mentiria sobre quanto falta.
    // Entao um brilho atravessa o trilho da esquerda para a direita e recomeca;
    // com Animacoes reduzidas o trilho fica inteiro no acento apagado, parado.
    { float ar, ag, ab;
      GfxRect tr = { x, yy, w, 4.0f };
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor(tr, 0.5f, 1, 1, 1, 0.16f * ae);
      if (ajustes_animacoes_reduzidas()) gfx_cor(tr, 0.5f, ar, ag, ab, 0.45f * ae);
      else {
        float tt = (float)(agora % 1600u) / 1600.0f, seg = 0.30f;
        float x0 = x + (w * (1.0f + seg)) * tt - w * seg, x1 = x0 + w * seg;
        if (x0 < x) x0 = x;
        if (x1 > x + w) x1 = x + w;
        if (x1 - x0 > 1.0f) {
          // Pontas suaves: o miolo cheio e duas abas a 45% de cada lado.
          float aba = (x1 - x0) * 0.22f;
          gfx_cor((GfxRect){ x0, yy, x1 - x0, 4.0f }, 0.5f, ar, ag, ab, 0.45f * ae);
          if (x1 - x0 > aba * 2.0f + 4.0f)
            gfx_cor((GfxRect){ x0 + aba, yy, x1 - x0 - aba * 2.0f, 4.0f }, 0.5f, ar, ag, ab, ae);
        }
      } }
  }
  motivoDesenhar(r, a);
}

// O MODAL DO ERRO: o motivo (36/700), a dica e os dois botoes. ESQUERDA e
// DIREITA andam, OK aciona (player_evento).
static float erroLargura(void) {
  return telefoneui_largura(880, NV_VTELA_W, 96);
}

#ifdef NV_TOUCH_UI
static int avToquePode(int erro) {
  return telefoneui_ativo() && aberto && !mini && !saindo && ehCanal() &&
         erroFonte == erro && modalToquePode() && !stream_folha_aberta() &&
         !faixas_aberta() && !episodios_aberto() && !zapEst.pend &&
         (erro || anim * entrada > .99f);
}
static void avToqueAcao(int acao, int erro, int quadro) {
  int ids[AV_B_N], n, i;
  if (quadro != avToqueQuadro || erro != avToqueErro ||
      strcmp(avToqueCanal, player_id_canal()) || !avToquePode(erro)) return;
  if (erro) {
    if (acao != AV_B_RECARREGAR && acao != AV_B_FONTE && acao != AV_B_GUIA) return;
  } else {
    n = avBotoes(ids);
    for (i = 0; i < n && ids[i] != acao; i++) {}
    if (i == n) return;
    botaoAV = i;
  }
  avAtivar(acao);
  acordar();
}
static void avToquePreparar(int erro) {
  int elegivel = avToquePode(erro), mudou = avToqueErro != erro || strcmp(avToqueCanal, player_id_canal());
  if (mudou) aovivo_toque_reiniciar();
  if (!avToqueQuadro || mudou || avToqueElegivel != elegivel)
    avToqueQuadro = avToqueQuadro == 0x7fffffff ? 1 : avToqueQuadro + 1;
  avToqueErro = erro;
  avToqueElegivel = elegivel;
  snprintf(avToqueCanal, sizeof avToqueCanal, "%s", player_id_canal());
  aovivo_toque_definir(elegivel ? avToqueAcao : NULL, avToqueQuadro);
}
#endif
#ifdef NV_TOUCH_UI
static int erroToquePode(void) {
  return telefoneui_ativo() && aberto && !mini && !saindo && erroFonte && !ehCanal() &&
         modalToquePode() && !stream_folha_aberta() && !faixas_aberta() && !episodios_aberto();
}
static void erroToqueAcao(int i, int b) {
  (void)b;
  if (!erroToquePode() || i < 0 || i > 1) return;
  erroBotao = i; erroEscolher(i);
}
#endif
static float alturaErro(void) {
  TxtLinha t = txt_linha(TXT_ILHA_PERGUNTA, "Ag", 0, 0, 0, 255);
  float h = 18.0f + 4.0f;
  const char *tit = erroTitulo[0] ? erroTitulo : "Não foi possível abrir a fonte";
  const char *dica = erroDica[0] ? erroDica : "Abra Fontes para escolher outra opção ou recarregar.";
  h += txt_bloco_corta(TXT_ILHA_PERGUNTA, tit, 0, 0, 0, 0, 0, erroLargura() - 72.0f, (float)t.h + 7.0f, 0.0f, 3);
  h += 14.0f + txt_bloco_corta(TXT_ILHA_TEXTO, dica, 0, 0, 0, 0, 0, erroLargura() - 72.0f, 30.0f, 0.0f, 3);
  return h + 28.0f + (telefoneui_ativo() ? 76 : 60) + 34.0f;
}
static void corpoErro(GfxRect r, float a, void *u) {
  float x = r.x + 36.0f, w = r.w - 72.0f, y = r.y + 18.0f + 4.0f;
  TxtLinha t = txt_linha(TXT_ILHA_PERGUNTA, "Ag", 0, 0, 0, 255);
  const char *tit = erroTitulo[0] ? erroTitulo : "Não foi possível abrir a fonte";
  const char *dica = erroDica[0] ? erroDica : "Abra Fontes para escolher outra opção ou recarregar.";
  (void)u;
  y += txt_bloco_corta(TXT_ILHA_PERGUNTA, tit, 243, 242, 239, x, y, w, (float)t.h + 7.0f, a, 3);
  y += 14.0f;
  y += txt_bloco_corta(TXT_ILHA_TEXTO, dica, 158, 157, 155, x, y, w, 30.0f, a, 3);
  y += 28.0f;
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) {
    int pont = a > .99f && erroToquePode();
    if (pont) {
      ponteiro_camada();
      ponteiro_alvo(0, 0, NV_VTELA_W, NV_VTELA_H, NULL, NULL, 0, 0);
    }
    for (int i = 0; i < 2; i++) {
      GfxRect b = {x + i * (w + 12) * .5f, y, (w - 12) * .5f, 76};
      int foco = erroBotao == i, tinta = foco ? plrui_tinta() : 225;
      const char *rot = i18n(i ? "Voltar" : "Abrir Fontes");
      if (foco) plrui_pilula_foco_video(b, a); else plrui_botao_repouso_video(b, a);
      float th = txt_bloco_corta(TXT_G21B, rot, tinta, tinta, tinta, 0, 0, b.w - 32, 28, 0, 2);
      txt_bloco_corta(TXT_G21B, rot, tinta, tinta, tinta, b.x + 16, b.y + (b.h - th) * .5f, b.w - 32, 28, a, 2);
      if (pont) ponteiro_alvo(b.x, b.y, b.w, b.h, NULL, erroToqueAcao, i, 0);
    }
    return;
  }
#endif
  x += plrui_botao_video(x, y, "Abrir Fontes", "pl_layers", erroBotao == 0 ? 1.0f : 0.0f, a) + 12.0f;
  plrui_botao_video(x, y, "Voltar", NULL, erroBotao == 1 ? 1.0f : 0.0f, a);
}

static void corpoDurFonte(GfxRect r, float a, void *u) {
  float x = r.x + 28, y = r.y + 12;
  float w = plrui_botao_video(x, y, durFonteManter ? "Manter esta fonte (OK)" : "Tentar próxima fonte (OK)",
                        durFonteManter ? "aj_check" : "pl_layers", 1, a);
  (void)u;
  if (durFonteManter && durFonteSkipEm) {
    // A contagem do pulo, ao lado do botao.
    char t[64];
    int falta = (int)(durFonteSkipEm - SDL_GetTicks());
    snprintf(t, sizeof t, i18n("Outra fonte em %d s"), falta > 0 ? (falta + 999) / 1000 : 0);
    txt_bloco_corta(TXT_ILHA_TEXTO, t, 158, 157, 155, x + w + 20, y + 16, r.w - (x - r.x) - w - 48, 30.0f, a, 1);
  }
  if (a > .9f && durFonteAcaoAte && durFonteAcaoAte == toastAte)
    ponteiro_alvo(x, y, w, 60, NULL, durFonteTentar, 0, 0);
}

static void desenharOsd(Uint32 agora, float a, float ac, const CatItem *c);
// O QUE FICA FORA DO FURO. Video em tela cheia com barras (Original, 2.39:1,
// 4:3...): PRETO de verdade, (0,0,0,1). O Glass UI (5a058b6c) trocou o chapado
// por #0B0C0E para todo caso, e na TCL (Android TV) as barras viraram cinza.
// So o video RECUADO (A seguir / Mais como este) tem em volta a arte do titulo
// desfocada, escurecida, com sombra sob o cartaz.
void player_fundo_fora_do_furo(GfxRect furo, int recuado, const CatItem *c) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  if (!(furo.w < NV_TELA_W - 0.5f || furo.h < NV_TELA_H - 0.5f)) return;
  if (!recuado) { gfx_cor(tela, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); return; }
  { const char *arte = (c && c->backdrop[0]) ? artehero_url(c) : NULL;
    GLuint tb = arte ? tex_obter_larg(arte, 480) : 0;
    if (tb) tb = gfx_desfocado(tb, arte);   // a copia 96x54 desfocada (#133)
    gfx_cor(tela, 0.0f, 0.043f, 0.047f, 0.055f, 1.0f);
    if (tb) {
      gfx_tex_aspect_atual = tex_aspecto(arte);
      gfx_rect(tela, tb, GFX_CARD, 0, 0, 0, 0.0f, 0, 0, 0, 1.0f);
      gfx_tex_aspect_atual = 0.0f;
      gfx_cor(tela, 0.0f, 0, 0, 0, 0.58f);
    }
    gfx_rect((GfxRect){ furo.x - 40.0f, furo.y - 10.0f, furo.w + 80.0f, furo.h + 90.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, 0, 0, 0, 0.5f); }
}

// A3: o logo e o Play do detalhe no ultimo quadro dele (player_morph_de).
static GfxRect morphLogo, morphPlay;
static Uint32 morphEm;
void player_morph_de(GfxRect logo, GfxRect play) {
  morphLogo = logo; morphPlay = play; morphEm = SDL_GetTicks();
}

// A TELA CINEMA DA ABERTURA (#205, abrecine.h). Tudo multiplicado pelo veu,
// que so cai com o quadro real (o crossfade e o veu saindo por cima dele).
// CUSTO: na Arte (padrao), a textura do detalhe e uma rampa de meia tela, sem
// copia nem blur. A Arte borrada e a copia 96x54 de sempre (gfx_desfocado),
// uma passada; a nitida so enquanto recua (~1 s). O preto so com o furo aberto
// (sem furo o preto opaco de baixo ja esta la). Logo, anel e linha sao
// retangulos pequenos.
static void desenharCinema(Uint32 agora, const CatItem *c, int furou) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  float v = cine.veu, f = cine.foco, t = (float)(agora - cine.desde) / 1000.0f;
  float entra, cx = NV_TELA_W * 0.5f, tm, k, base, y, tx, topo;
  int reduz = ajustes_animacoes_reduzidas();
  // #508: preserve the saved Off / Art / Blurred art choice. Reuse the
  // title page's own artwork without starting a second metadata request.
  int nf = ajustes_abertura_fundo();
  const char *arte = nf ? cinemaArteObter(c) : NULL;
  const char *marca = c ? artehero_logo_sessao(c) : NULL;
  GLuint ts = arte ? tex_obter_hero(arte) : 0, tb = nf == 2 && arte ? tex_obter_larg(arte, 480) : 0, lg;
  // A3 (dono, 16/10): o logo do detalhe vai ao centro e o Play vira um anel
  // pequeno EMBAIXO dele, 900 ms em ease-out-expo, sem quique. Parte do quadro
  // do detalhe so se ele estava na tela quando a abertura comecou.
  static Uint32 morphDe;
  static int morph;
  static GfxRect l0, p0;
  // B3 (Canto): same morph, same art and veil; only the targets move. Logo at the
  // bottom left (x 96, 0.744 of the A3 size), the 44 px ring under it, lines beside the ring.
  int canto = ajustes_abertura_estilo();
  float sc = canto ? 0.744f : 1.0f, lx = 96.0f, gap = canto ? 40.0f : 64.0f;
  GfxRect anel = canto ? (GfxRect){ lx, 940.0f, 44.0f, 44.0f } : (GfxRect){ cx - 28.0f, 756.0f, 56.0f, 56.0f }, ra = anel;
  if (v <= 0.0f) return;
  if (morphDe != cine.desde) {
    morphDe = cine.desde; l0 = morphLogo; p0 = morphPlay;
    morph = !reduz && morphEm && p0.w > 0.0f && (Sint32)(cine.desde - morphEm) < 1000;
  }
  tm = !morph || t >= 0.9f ? 1.0f : t > 0.0f ? t / 0.9f : 0.0f;
  k = tm >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * tm);
  entra = morph || reduz ? 1.0f : (t < 0.4f ? t / 0.4f : 1.0f);
  if (furou || !player_carregando()) gfx_cor(tela, 0.0f, 0, 0, 0, v);
  if (tb) tb = gfx_desfocado(tb, arte);
  gfx_tex_aspect_atual = arte ? tex_aspecto(arte) : 0.0f;
  // ARTE (o padrao): o backdrop NITIDO, sem desfoque nem recuo, e o veu da
  // rampa do hero da Home (smoothstep^2, preto) da base ate a METADE da tela,
  // para o logo e o anel lerem. Duas passadas, nenhuma copia, nenhum blur.
  if (nf == 1) {
    GfxRect alvo = tela;
    // A poster fallback stays in proportion, including on a portrait phone.
    if (cinemaPoster && gfx_tex_aspect_atual > 0.0f) {
      float ar = gfx_tex_aspect_atual;
      if (ar < tela.w / tela.h) { alvo.w = tela.h * ar; alvo.x = (tela.w - alvo.w) * 0.5f; }
      else { alvo.h = tela.w / ar; alvo.y = (tela.h - alvo.h) * 0.5f; }
    }
    if (ts) gfx_rect(alvo, ts, GFX_CARD, 0, 0, 0, 0.0f, 0, 0, 0, v);
    gfx_tex_aspect_atual = 0.0f;
    gfx_rect((GfxRect){ 0, NV_TELA_H * 0.5f, NV_TELA_W, NV_TELA_H * 0.5f }, 0, GFX_VEU_BAIXO,
             0, 0, 0, 0.0f, 0, 0, 0, 0.85f * v * (morph ? k : entra));
  } else {
  // ARTE BORRADA: recua para 45%, a arte vira luz de fundo (a copia 96x54 de
  // sempre, gfx_desfocado). Pelo GFX_SNAP com o nv_dither e o B-spline ligados
  // (uPar.x = 2) e o escurecimento na mesma passada (uFoco): sem o ruido, a
  // arte desfocada e escura saia em faixas de 8 bits no OLED (dono, 11/10).
  if (tb) gfx_rect(tela, tb, GFX_SNAP, 0.55f * f, 2.0f, 0.0f, 0.0f, 0, 0, 0, v);
  { float as = v * (tb ? 1.0f - f : 1.0f - 0.55f * f);
    if (ts && as > 0.01f) gfx_rect(tela, ts, GFX_CARD, 0, 0, 0, 0.0f, 0, 0, 0, as); }
  }
  gfx_tex_aspect_atual = 0.0f;

  // O LOGO: a mesma textura do detalhe, so o quad anda e escala ate o centro.
  lg = marca ? tex_obter_larg_qualquer(marca, 640) : 0;
  if (lg) {
    float ar = tex_aspecto(marca), lw = 560.0f * sc, lh = ar > 0.0f ? lw / ar : 172.0f * sc;
    GfxRect r;
    if (lh > 172.0f * sc) { lh = 172.0f * sc; lw = lh * ar; }
    r = (GfxRect){ canto ? lx : cx - lw * 0.5f, anel.y - gap - lh, lw, lh };
    if (morph && l0.w > 0.0f) {
      r.x = l0.x + (r.x - l0.x) * k; r.y = l0.y + (r.y - l0.y) * k;
      r.w = l0.w + (r.w - l0.w) * k; r.h = l0.h + (r.h - l0.h) * k;
    }
    topo = r.y;
    gfx_rect(r, lg, tex_marca_escura(marca) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0, .96f, .96f, .97f,
             v * entra);
  } else {
    TxtLinha l = txt_linha_corta(TXT_TITULO2, c ? c->titulo : "", 255, 255, 255, 255, 1400.0f);
    topo = anel.y - gap - l.h;
    txt_desenhar_alpha(l, canto ? lx : cx - l.w * 0.5f, topo, v * entra);
  }
  // Dolby Vision slot 1: the mark above the logo (dvtelaui.c); the rest is this same drawing.
  dvtela_abertura_marca(canto, canto ? lx : cx, topo, v * entra);

  // O PLAY VIRA O ANEL: a pilula encolhe ate o circulo (raio = h/2), o branco
  // some nos primeiros 45% e a borda vira o trilho; o arco acende nos ultimos 40%.
  if (morph) {
    float aw = 1.0f - tm / 0.45f;
    ra.x = p0.x + (anel.x - p0.x) * k; ra.y = p0.y + (anel.y - p0.y) * k;
    ra.w = p0.w + (anel.w - p0.w) * k; ra.h = p0.h + (anel.h - p0.h) * k;
    if (aw > 0.0f) gfx_cor(ra, 0.5f, 1, 1, 1, aw * v);
  }
  gfx_anel(ra, 0.5f, 3.0f, 1, 1, 1, 0.16f * v * entra);
  // Como na carga da Home: o arco so enche com progresso medido.
  // Samsung informa porcentagem; segundos de buffer dos demais nao sao %.
  { VideoStats stats;
    int pct;
    float r, g, b, aa = v * entra * (morph ? (tm - 0.6f) / 0.4f : 1.0f);
    video_estatisticas(&stats);
    pct = !esperandoFonte && stats.bufferPct >= 0 && stats.bufferPct <= 100 ? stats.bufferPct : -1;
    // O arco leva a cor do logo, como a Imersiva (dono, 14/10); sem paleta, o realce.
    if (!corviva_logo_acento(&r, &g, &b)) corFocoPlayer(&r, &g, &b);
    if (aa > 0.0f) {
      if (pct > 0) gfx_arco(ra, 3.0f, 0.0f, pct / 100.0f, r, g, b, 0.9f * aa);
      else if (pct < 0 && !reduz) gfx_arco(ra, 3.0f, fmodf(t / 1.2f, 1.0f), 0.25f, r, g, b, 0.9f * aa);
    } }

  // UMA LINHA QUIETA sob o anel: o episodio (so serie). O add-on so com o
  // ajuste ligado; a de demora so depois de 8 s.
  base = canto ? ra.y + 4.0f : ra.y + ra.h + 30.0f; y = base;
  tx = ra.x + ra.w + 20.0f;
  if (linhaEp[0]) {
    TxtLinha l = txt_linha_corta(TXT_ILHA_SUB, linhaEp, 243, 242, 239, 170, 1100.0f);
    txt_desenhar_alpha(l, canto ? tx : cx - l.w * 0.5f, y, v * entra);
    y += 34.0f;
  }
  if (ajustes_abertura_addon()) {
    const Stream *st = stream_item(stream_atual());
    char det[260];
    snprintf(det, sizeof det, "%s", st ? st->provedor : "");
    // O nome e uma linha, como os rotulos da ilha.
    for (char *p = det; *p; p++) if ((unsigned char)*p < 32) *p = ' ';
    if (tentativaN >= 2 && tentativaM >= tentativaN) {
      size_t n = strlen(det);
      snprintf(det + n, sizeof det - n, "%s", n ? " \xc2\xb7 " : "");
      n = strlen(det);
      snprintf(det + n, sizeof det - n, i18n("Fonte %d de %d"), tentativaN, tentativaM);
    }
    if (det[0]) {
      TxtLinha d = txt_linha_corta(TXT_G19M, det, 243, 242, 239, 140, 1100.0f);
      txt_desenhar_alpha(d, canto ? tx : cx - d.w * 0.5f, y, v * entra);
      y += 34.0f;
    } }
  // DEMOROU: uma linha so, curta e calma, que entra devagar.
  if (abrecine_demorando(&cine, agora)) {
    float d = (float)(agora - cine.desde - ABRECINE_DEMORA_MS) / 600.0f;
    TxtLinha l = txt_linha(TXT_G19M, i18n("Só mais um instante…"), 243, 242, 239, 255);
    txt_desenhar_alpha(l, canto ? tx : cx - l.w * 0.5f, y, 0.62f * v * (reduz || d > 1.0f ? 1.0f : d));
    y += 34.0f;
  }
  // Append DV information without moving existing loading content.
  dvtela_abertura_linha(canto, canto ? tx : cx, y, v * entra);
#ifdef NV_SHOT_HOOKS
  dvtela_abertura_zona_linha(y, NV_TELA_W);
#endif
}

#ifdef NV_TPK
int video_tpk_capa(void);   // video_tpk.c
// CAPA DA REABERTURA (DTS/TrueHD convertido). No TS continuo cada seek, a
// reconexao e a troca para o convertido abrem OUTRO player, e o plano de video
// fica preto ate o primeiro quadro novo. No lugar do preto: a miniatura do
// ponto de destino (a mesma do scrub, seekr) ou, sem ela, a arte do titulo,
// escurecida, com o anel de buffer por cima (video_bufferando_ms). Some em
// 250 ms quando o quadro chega. So o convertido chama isto (o resto: 0).
static void capaReabertura(GfxRect furo, const CatItem *c, Uint32 agora) {
  static float a; static Uint32 ult; static int usouSeekr;
  float dt = ult && agora - ult < 100u ? (float)(agora - ult) / 1000.0f : 1.0f / 60.0f;
  GLuint tex = 0;
  const char *arte = NULL;
  int on = video_tpk_capa() && !ehCanal();
  ult = agora;
  a += (on ? dt : -dt) / 0.25f;
  if (a > 1.0f) a = 1.0f;
  if (a <= 0.0f) { a = 0.0f; if (usouSeekr) { seekr_ocioso(); usouSeekr = 0; } return; }
  if (ajustes_seekr_habilitado() && seekr_tem_chave() && seekr_estado() == SEEKR_PRONTO) {
    double cue = -1;
    seekr_definir_ajuste_ms((long)ajustes_seekr_ajuste_s() * 1000L);
    tex = seekr_quadro(video_pos(), &cue);
    if (tex && seekr_quadro_velho()) tex = 0;   // a de outro ponto confundiria
    if (tex) usouSeekr = 1;
  }
  if (!tex && c && c->backdrop[0]) { arte = artehero_url(c); tex = tex_obter_hero(arte); }
  if (!tex) return;   // fica o preto, com o anel
  gfx_tex_aspect_atual = arte ? tex_aspecto(arte) : 16.0f / 9.0f;
  gfx_rect(furo, tex, GFX_CARD, 0, 0, 0, 0.0f, 0, 0, 0, a);
  gfx_tex_aspect_atual = 0.0f;
  gfx_cor(furo, 0.0f, 0.0f, 0.0f, 0.0f, 0.45f * a);
}
#endif

void player_desenhar(Uint32 agora) {
  if (!aberto) return;
  const CatItem *c = item();
  // COR VIVA: o titulo que TOCA manda na cor (abrindo, OSD, pausa, pos-play),
  // acima de tudo. A chave e a arte da tela de abertura logo abaixo; se ela
  // ainda nao tem paleta, fica a do detalhe, que e o mesmo titulo. Canal ao
  // vivo nao pede: o logo do canal nao e cor de titulo, e o zap trocaria a
  // cor da tela a cada canal.
  if (c && c->backdrop[0] && !player_id_canal()[0]) {
    corviva_definir(artehero_url(c), CORVIVA_PLAYER);
    if (c->logo[0]) corviva_definir_logo(artehero_logo_sessao(c), CORVIVA_PLAYER);
  }

  // --- o quadro de video ---
  // Com pipeline nao ha o que desenhar: o video esta num plano de hardware ATRAS
  // desta superficie, e o que se faz aqui e abrir o buraco por onde ele aparece.
  // O furo tem de sair DAQUI e nao no fim do quadro: feito por ultimo ele
  // apagaria os proprios controles. Tudo o que vem depois (veu, barra, textos)
  // desenha por cima do buraco e continua visivel, porque o alpha do blend e
  // somado — um veu a 60% sobre o furo devolve 0.6 de opacidade, que e
  // exatamente o escurecimento que se quer sobre o video.
  //
  // Sem pipeline, o lugar do quadro fica com a arte-chave parada. GFX_CARD com
  // raio 0 e o quad de tela inteira: o recorte (cover) do shader e o que impede
  // a arte 16:9 de esticar quando a tela nao for exatamente 16:9.
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  int quadroMorno = player_carregando() && !erroFonte && cacheboost_backend_quadro(3);
  int furar = (player_com_video() && !esperandoFonte) || quadroMorno;   // abrindo a fonte: sem furo, so preto
#ifdef NV_SHOT_HOOKS
  if (shotSemFuro && encolhe > 0.999f) furar = 0;   // capturas: a arte faz de video
#endif
#ifdef NV_TPK
  // O FURO SO COM IMAGEM (#188). O host manda `pronto` ANTES do Start (Video.cs,
  // Abrir), e o primeiro quadro so vem depois do buffer: furo aberto nesse vao
  // mostra o que esta atras do app, que no Tizen 9 e a tela inicial da
  // Samsung (#185 viu o mesmo numa linha de 1 px). Espera a posicao andar; se
  // ela nao andar (ao vivo sem posicao, host que nao le), abre em
  // NV_PLR_TPK_FURO_PRAZO_MS de `tocando`, como antes. A janela que cresce do
  // guia ja vem com video tocando e fica de fora.
#define NV_PLR_TPK_FURO_PRAZO_MS 3000u
  { static int vista; static Uint32 tocandoEm;
    if (!furar) { vista = 0; tocandoEm = 0; }
    else if (!vista && !janAtiva) {
      if (video_tocando() && !tocandoEm) tocandoEm = SDL_GetTicks() | 1;
      if (video_pos() >= 0.25 ||
          (tocandoEm && SDL_GetTicks() - tocandoEm >= NV_PLR_TPK_FURO_PRAZO_MS)) {
        vista = 1;
        printf("[player] tpk: furo aberto (posicao %.2fs)\n", video_pos());
        fflush(stdout);
      } else furar = 0;
    } }
#endif
  // O QUADRO REAL da tela cinema (#205): furo aberto E video andando. No LG o
  // `pronto` sobe no loadCompleted e o quadro pode vir segundos depois; no
  // .tpk o furo acima ja espera a posicao. abrecine.h tem o teto.
  { static Uint32 cineUlt;
    int imagem = furar && (video_tocando() || video_pos() >= 0.25);
    float dtc = cineUlt && agora - cineUlt < 100u ? (float)(agora - cineUlt) / 1000.0f : 1.0f / 60.0f;
#ifdef NV_SHOT_HOOKS
    if (shotSemFuro) imagem = player_com_video() && !esperandoFonte && (video_tocando() || video_pos() >= 0.25);
#endif
    if (!player_com_video()) imagem = 1;
    cineUlt = agora;
    // The DV screen keeps the opening alive: it only falls with the real frame, DV or not.
    abrecine_passo(&cine, (player_carregando() || dvtela_ativa()) && !ehCanal(), imagem, ajustes_abertura_detalhada(),
                   agora, dtc, ajustes_animacoes_reduzidas());
    if (erroFonte || saindo || ehCanal()) cine.veu = 0.0f; }
  // Live video below (hardware plane): the glass of the frame is translucent
  // flat glass, no capture, no blur; the video shows through the hole.
  if (furar) gfx_vidro_fosco_bloquear();
  if (furar) {
    // O furo acompanha o MESMO retangulo que foi ao plano de hardware, cortado
    // na tela. Furar sempre a tela inteira, como antes, deixava faixa preta nos
    // modos que nao ocupam tudo ("Original" num 2.39:1 entregue como 2.39:1):
    // o furo mostrava o nada atras do plano em vez de mostrar o plano.
    PlrRect r = destinoComRecuo(aspectoVisivel(aspecto));
    GfxRect furo;
    // OS MESMOS PIXELS INTEIROS QUE FORAM AO PLANO (video_janela arredonda com
    // +0,5). Furo fracionario e plano inteiro discordam em ate 1 px, e esse px
    // fica transparente SEM video por baixo: aparece o que esta atras do app —
    // a tela da Samsung numa linha fina colorida na faixa preta (#185, S90D,
    // modo Original em todo conteudo com barra). Fora da tela cheia o furo
    // ainda encolhe 1 px por lado: o preto cobre a borda do video em vez de
    // deixar vao.
    { float x0 = (float)(int)(r.x + 0.5f), y0 = (float)(int)(r.y + 0.5f);
      float w0 = (float)(int)(r.w + 0.5f), h0 = (float)(int)(r.h + 0.5f);
      furo.x = x0; furo.y = y0; furo.w = w0; furo.h = h0;
      if (w0 < NV_TELA_W - 0.5f || h0 < NV_TELA_H - 0.5f) {
        furo.x += 1.0f; furo.y += 1.0f; furo.w -= 2.0f; furo.h -= 2.0f;
      } }
    // Fora do furo fica PRETO, e nao a arte-chave: e o que a TV mostra ao lado
    // do plano de video, e pintar outra coisa ali criaria uma borda que nao
    // existe no aparelho.
    // CRESCENDO DO PREVIEW DO GUIA: o furo segue o retangulo do degrau, com
    // canto arredondado, e fora dele NAO ha preto — o guia continua a vista
    // em volta ate o video tomar a tela.
    if (janAtiva) {
      GfxRect fa = { janAgora.x, janAgora.y, janAgora.w, janAgora.h };
      gfx_furo_raio(fa, (16.0f * (1.0f - janT)) / (fa.h > 1.0f ? fa.h : 1.0f));
    } else {
      // VIDEO RECUADO PARA O "A SEGUIR" / "MAIS COMO ESTE" (Glass UI): em
      // volta dele, a arte do titulo tao desfocada que so a cor fica (a
      // textura pequena esticada) e escurecida, em vez do preto; o video ganha
      // raio de cartaz e sombra.
      int recuado = encolhe < 0.999f;
      player_fundo_fora_do_furo(furo, recuado, c);
#ifdef NV_SHOT_HOOKS
      if (shotSemFuro) {
        const char *arte = c && c->backdrop[0] ? artehero_url(c) : NULL;
        GLuint tv = arte ? tex_obter_hero(arte) : 0;
        if (tv) { gfx_tex_aspect_atual = tex_aspecto(arte);
                  gfx_rect(furo, tv, GFX_CARD, 0, 0, 0, recuado ? 22.0f / furo.h : 0.0f, 0, 0, 0, 1.0f);
                  gfx_tex_aspect_atual = 0.0f; }
      } else
#endif
      if (furo.w > 0.0f && furo.h > 0.0f) {
        if (recuado) gfx_furo_raio(furo, 22.0f / furo.h);
        else gfx_furo(furo);
#ifdef NV_TPK
        if (!recuado) capaReabertura(furo, c, agora);
#endif
      }
    }
  } else {
    // CANAL NAO TEM BACKDROP, TEM LOGO. O addon de canais manda a MESMA imagem
    // em poster/background, e ela e a marca do canal — um PNG pequeno, com
    // fundo proprio. Esticada para 1920x1080 ela vira um borrao gigante atras da
    // interface, que foi o que o dono viu e pediu para tirar: "deixe o fundo
    // preto ao inves do logo no background". A marca passa a ser desenhada no
    // tamanho dela, no centro, onde antes ficava o nome em texto.
    // A MESMA POLITICA DE TELA CHEIA do destaque e do detalhe: o fundo guardado
    // no catalogo e o do card, e aqui ele ocupa 1920.
    const char *arte = (c && c->backdrop[0] && !player_id_canal()[0])
                     ? artehero_url(c) : NULL;
#ifdef NV_SHOT_HOOKS
    if (shotSemFuro && c && c->backdrop[0]) arte = c->backdrop;   // capturas: o "video" do canal
#endif
    GLuint tex = arte ? tex_obter_hero(arte) : 0;   // ocupa a tela inteira
    // ABRINDO A FONTE A TELA E PRETA (pedido do dono, 03/10): sem a arte e sem
    // o logo; quem conta o que acontece e so a ilha do canto.
    if (player_carregando()) tex = 0;
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(arte);
      gfx_rect(tela, tex, GFX_CARD, 0, 0, 0, 0.0f, 0, 0, 0, entrada);
      gfx_tex_aspect_atual = 0.0f;
    } else if (player_id_canal()[0] || player_carregando()) {
      // PRETO de verdade no canal, e nao o quase-preto da interface: com o
      // video entrando por tras da pagina, qualquer tinta aqui e uma camada a
      // mais sobre o plano de hardware.
      // ABRINDO: opaco DESDE O PRIMEIRO QUADRO. Com `entrada` (mola de 0 a 1) o
      // preto nascia transparente e a arte do detalhe/cartao, que continua
      // desenhada por baixo do player, aparecia atras da ilha enquanto ele
      // abria — e a cada troca de fonte/tentativa.
      gfx_cor(tela, 0.0f, 0.0f, 0.0f, 0.0f,
              (player_carregando() && !player_id_canal()[0] && !saindo) ? 1.0f : entrada);
    } else {
      gfx_cor(tela, 0.0f, 0.04f, 0.04f, 0.05f, entrada);
    }
  }

  // ABRINDO A FONTE (Glass UI, revisto pelo dono em 03/10): fundo PRETO, sem
  // a arte, sem o logo e sem anel no meio da tela. O estado mora so na ILHA
  // do canto, crescida como a atividade da ilha do relogio — o ponto que
  // respira e "Abrindo fonte", com as marcas da fonte logo abaixo (cartao
  // COMPACTO, o padrao). O BAIXO expande: linhaEp, nome da fonte, addon,
  // tamanho, rede, ha quanto tempo abre e o trilho; o CIMA recolhe. "Fonte 2
  // de 3" quando o automatico ja esta na segunda.
  // O PRETO DA ABERTURA SAI COM O CARTAO. Ao chegar a imagem o preto opaco sumia
  // no mesmo quadro e o cartao encolhia sobre um corte seco; agora o preto
  // esvai com a mesma taxa (12) com que a ilha do relogio apaga a atividade, por
  // cima da imagem ja entrando. Animacoes reduzidas: corta, como as outras.
  { float dtf = abrindoFundoUlt ? (float)(agora - abrindoFundoUlt) / 1000.0f : 1.0f / 60.0f;
    abrindoFundoUlt = agora;
    if (dtf > 0.1f) dtf = 0.1f;
    // A CINEMA tem o proprio crossfade (o veu dela), que espera o quadro real.
    if (quadroMorno || abrecine_cinema(&cine)) abrindoFundo = 0.0f;
    else if (player_carregando() && !ehCanal()) abrindoFundo = 1.0f;
    else if (abrindoFundo > 0.0f) {
      abrindoFundo = ajustes_animacoes_reduzidas() ? 0.0f : anim_mola(abrindoFundo, 0.0f, dtf, 12.0f);
      if (abrindoFundo < 0.01f) abrindoFundo = 0.0f;
      else if (player_com_video() && !erroFonte && !saindo && !ehCanal())
        gfx_cor(tela, 0.0f, 0.0f, 0.0f, 0.0f, abrindoFundo);
    }
    if (!quadroMorno && abrecine_cinema(&cine) && !(dvtela_visivel() && dvtela_variante() == DVT_VAR_DETALHADA)) desenharCinema(agora, c, furar); }
  // Uma abertura NOVA (a anterior acabou, ou o player reabriu): volta ao compacto
  // e zera o relogio. Ao acabar, o estado fica como esta para o cartao sair com o
  // mesmo conteudo (a saida e a da ilha, plrilha.c).
  if (player_carregando() && !ehCanal()) {
    if (!abrindoViva) { abrindoDesde = agora; abrindoExp = 0; abrindoT = 0.0f; abrFitIdx = -2; }
    abrindoViva = 1;
  } else abrindoViva = 0;
  // O DTS NA ILHA: como o audio DTS desta faixa chega — direto da TV,
  // convertido pelo app (PR #259), ou sem som. Uma vez por estado, depois de
  // ele ficar 1,5 s parado (o erro 200 da TV chega logo depois do load e
  // trocaria "nativo" por "sem som" num piscar). "Preparando" nao entra: o
  // cartao de abertura ja esta na tela.
  { static int dtsVisto;
    static Uint32 dtsDesde;
    int e = player_carregando() || ehCanal() ? VIDEO_DTS_NENHUM : video_dts_estado();
    if (e != dtsVisto) { dtsVisto = e; dtsDesde = agora; }
    // A guia parental (tambem na ilha, de prioridade baixa) vem primeiro: o
    // aviso do DTS espera ela fechar, em vez de derruba-la.
    if (pgDesde && (float)(agora - pgDesde) / 1000.0f < PG_SEG_TOTAL) dtsDesde = agora;
    if (e != VIDEO_DTS_NENHUM && e != VIDEO_DTS_PREPARANDO && agora - dtsDesde >= 1500u) {
      Uint32 dur = e == VIDEO_DTS_FALHOU || e == VIDEO_DTS_SEM_SOM ? 10000u : 5000u;
      if (agora - dtsDesde < 1500u + dur) {
        PlrIlhaPedido pd;
        memset(&pd, 0, sizeof pd);
        pd.icone = "dts";
        pd.corIcone = e == VIDEO_DTS_FALHOU || e == VIDEO_DTS_SEM_SOM ? 1 : 0;
        pd.texto = e == VIDEO_DTS_NATIVO ? i18n("Áudio DTS nativo")
                 : e == VIDEO_DTS_CONVERTIDO ? i18n("Áudio DTS convertido nesta TV")
                 : e == VIDEO_DTS_FALHOU ? i18n("Não deu para converter o áudio DTS")
                 : i18n("Esta TV não toca áudio DTS");
        pd.semFim = 1; pd.aberta = 1;
        plrilha_pedir(&pd);
      }
    } }
  if (player_carregando() && ehCanal()) {
    // CANAL SINTONIZANDO: a versao COMPACTA, uma linha na propria ilha do
    // relogio (dono, 03/10: "no player do live tv vamos usar a versao compacta
    // do opening source, sendo na mesma linha do relogio, uma linha"). O ponto
    // que respira, o canal e "Sintonizando…", e a hora — sem cartao no meio,
    // sem corpo: o zap e frequente e a troca nao pode tapar a tela.
    static char lin[200];
    PlrIlhaPedido pd;
    memset(&pd, 0, sizeof pd);
    if (itemCanal.titulo[0]) snprintf(lin, sizeof lin, "%s \xc2\xb7 %s", itemCanal.titulo, i18n("Sintonizando…"));
    else snprintf(lin, sizeof lin, "%s", i18n("Sintonizando…"));
    pd.texto = lin; pd.respira = 1; pd.semFim = 1; pd.aberta = 1;
    plrilha_pedir(&pd);
  } else if (player_carregando() && !stream_folha_aberta() && stream_folha_anim() < 0.05f &&
             // A TELA DO DOLBY VISION substitui o cartao: sem ela, o caminho do
             // DV mostrava "Abrindo fonte" DUAS vezes (a TV e depois o demux).
             (!dvtela_visivel() || dvtela_variante() == DVT_VAR_DETALHADA) &&
             // A CINEMA (#205) esta no lugar do cartao.
             !abrecine_cinema(&cine)) {
    // COM A FOLHA DE FONTES NA TELA o cartao fica de fora: ela e a escolha que
    // o player espera, e o cartao no meio tapava a esquerda das primeiras
    // linhas (foto do dono, 04/10). Escolhida a fonte, a folha sai e o cartao
    // entra no lugar dela.
    { PlrIlhaPedido pd;
      abrindoAtualizar(agora);
      memset(&pd, 0, sizeof pd);
      pd.semFim = 1;
      pd.centro = 1;   // cartao no meio da tela, sem relogio (dono, 03/10)
      pd.ancoraTopo = 1;   // o conteudo acompanha o topo ao crescer/encolher
      pd.w = 680.0f; pd.h = alturaCarregando();
      pd.corpo = corpoCarregando;
      plrilha_pedir(&pd); }
  }
  // REBUFFER (#182 pediu um indicador): com o video ja rodando, o buffer que
  // esvazia congelava a imagem sem nenhum sinal. So o anel, e so depois de 600 ms parado — o
  // vai-e-volta curto de um seek nao acende.
  // SEM PLACA (dono, 05/10): era um disco de material opaco por tras do anel.
  // Agora so os pontos, cada um com a sombra suave propria (plrui_anel_solto).
  // Com o veu da Cinema ainda na tela o trilho dela ja e o sinal (dono, 11/10:
  // o anel por cima do logo ficava feio).
  else if (comVideo && !erroFonte && !saindo && video_bufferando_ms() >= 600 && cine.veu <= 0.01f) {
    ESCALA_INI();
    plrui_anel_solto(NV_VTELA_W * 0.5f, NV_VTELA_H * 0.5f, 56.0f, agora, entrada);
    ESCALA_FIM();
  }
  if (erroFonte && ehCanal()) {
    // Cartao no estilo do ao vivo: a marca do canal e a causa (provedor, conta)
    // que o app ja calculou em erroTitulo/erroDica.
#ifdef NV_TOUCH_UI
    avToquePreparar(1);
#endif
    aovivo_erro_desenhar(itemCanal.titulo, itemCanal.poster, erroTitulo, erroDica, entrada);
  } else if (erroFonte) {
    // A FONTE NAO ABRIU: a ilha do canto vira o MODAL (o mesmo das
    // confirmacoes), com o motivo real e as duas saidas que o proprio texto
    // manda fazer — Abrir Fontes e Voltar. Era um veu .65 com duas linhas e
    // nenhum botao, e o Ao vivo ja tinha cartao: dois tratamentos.
    PlrIlhaPedido pd;
    // O video apagado (brightness .6 no mockup) com o veu da ilha por cima.
    gfx_cor(tela, 0, 0, 0, 0, (ajustes_vidro() ? 0.58f : 0.65f) * entrada);
    memset(&pd, 0, sizeof pd);
    pd.icone = "aj_triangle-alert"; pd.corIcone = 1;
    pd.texto = i18n("Fonte");
    pd.w = erroLargura(); pd.h = alturaErro();
    pd.corpo = corpoErro; pd.modal = 1; pd.video = 1;
    plrilha_pedir(&pd);
  }

  if (nesteAberto && (epT != nesteT || epE != nesteE || !aberto || saindo)) nesteAberto = 0;
  if (nesteAberto) {
    PlrIlhaPedido pd;
    const CatItem *c = item();
    snprintf(nesteImdb, sizeof nesteImdb, "%s", c ? c->imdb : "");
    neste_pedir(nesteImdb, nesteT, nesteE);
    memset(&pd, 0, sizeof pd);
    pd.icone = "pl_users"; pd.texto = i18n("Neste episódio");
    pd.semFim = 1; pd.aberta = 1; pd.video = 1;
    pd.w = 640.0f; pd.h = 36.0f + 34.0f * NESTE_MAX; pd.corpo = corpoNeste;
    plrilha_pedir(&pd);
  }

  // --- avisos do player: a PILULA DA ILHA que abre (plrilha.h) --------------
  // Era uma pilula de 128 px cheia de acento no meio da tela (o acento como
  // decoracao). Agora o aviso abre a pilula do canto para 64, com o icone do
  // tipo e a frase curta antes da hora — o mesmo lugar e material da ilha do
  // relogio fora do player. A proporcao leva tambem os pontos do ciclo: a
  // posicao do modo entre os disponiveis, para saber quantas vezes apertar.
  // Desenhado ANTES do corte por `a`: a tecla de proporcao funciona com os
  // controles escondidos, e o aviso e a unica confirmacao da troca.
  if (toastAte > agora) {
    PlrIlhaPedido pd;
    memset(&pd, 0, sizeof pd);
    pd.aberta = 1; pd.semFim = 1;
    if (toastTexto[0]) {
      pd.texto = toastTexto;
      pd.icone = toastIcone[0] ? toastIcone : "aj_info";
      pd.corIcone = toastCor;
      if (durFonteAcaoAte == toastAte) {
        pd.w = 1000; pd.h = 88; pd.corpo = corpoDurFonte; pd.video = 1;
      }
    } else {
      int m, n = 0, k = 0;
      for (m = 0; m < PLR_ASP_N; m++) if (modoDisponivel(m)) { if (m == aspecto) k = n; n++; }
      pd.texto = i18n(player_aspecto_rotulo(aspecto));
      pd.icone = "pl_ratio";
      pd.pontos = n > 1 ? n : 0; pd.ponto = k;
    }
    plrilha_pedir(&pd);
    // Sem o OSD, o veu de cima do mockup segura a pilula em cena clara.
    gfx_veu_css((GfxRect){ 0, 0, NV_TELA_W, 260.0f }, 1, 1.38f, 1.0f, 0.52f * entrada * (1.0f - anim));
  }

  // ANTES do corte por `a`: desenha-lo depois do `return` de "tocando limpo"
  // faria dele um painel que so aparece quando ja ha barra na tela.
  //
  // CAMADA DE TELA CHEIA: o painel de pausa cobre o quadro inteiro (veu de ponta
  // a ponta, selo no alto, ficha na margem inferior, barra na borda) e por isso
  // nao depende mais de onde o player desenha os controles — que saem de cena
  // enquanto ele esta de pe. Ver pausao.h.
  { PausaoCena cena;
    cena.pos = posSeg; cena.dur = ehCanal() ? 0.0f : duracaoSeg;
    corFocoPlayer(&cena.fr, &cena.fg, &cena.fb);
    pausao_desenhar(agora, &cena); }

  // O PAINEL DE POS-REPRODUCAO DESENHA AQUI, e o lugar importa.
  //
  // Ele estava no FIM desta funcao, depois do `return` de "tocando limpo" logo
  // abaixo. Enquanto ele nao mexia nos controles, tudo bem — havia barra na
  // tela, `a` era alto e o return nao disparava. Quando ele passou a recolher
  // os controles para ocupar o rodape sozinho, passou a se apagar: `a` caia a
  // zero, a funcao voltava antes da ultima linha e o painel nunca era
  // desenhado. O video encolhia (isso mora em player_atualizar) e o que sobrava
  // era tela preta — os dois sintomas relatados, o "pisca e nao aparece" ao
  // abrir pelo botao e o "diminuiu mas ficou tudo preto" nos creditos, eram
  // este mesmo return.
  //
  // BASE NA MARGEM INFERIOR: com o video recuado, o painel ocupa o espaco que
  // se abriu. Ancorar acima da barra desperdicaria a faixa que o recuo existe
  // para criar.
  posplay_desenhar(agora, NV_VTELA_H - PLR_PAD_Y);   // camada ampliada: tela virtual
  // Acima do pos-reproducao quando ele esta no ar (posplay_topo), senao na
  // mesma margem inferior.
  // "O que achou?" na margem de 96 (mockup) ou acima do pos-reproducao.
  { float base = posplay_visivel() ? posplay_topo(NV_VTELA_H) : NV_VTELA_H - 96.0f;
    if (reacao_visivel()) {
      // O video um degrau mais escuro e o veu leve de baixo, sob o cartao.
      gfx_cor(tela, 0, 0, 0, 0, 0.25f * entrada);
      gfx_veu_css((GfxRect){ 0, NV_TELA_H - 300.0f, NV_TELA_W, 300.0f }, 0, 1.0f, 1.0f, 0.60f * entrada);
    }
    reacao_desenhar(agora, base); }

  // GUIA PARENTAL, canto superior esquerdo (.player-parental-guide).
  //
  // MOVIDO PARA ANTES DO RETORNO DE "TOCANDO LIMPO" (issue #31, "it needs me
  // to toggle the player menu to see it"). Este bloco inteiro morava depois
  // do `if (a <= 0.005f) return;` abaixo — o MESMO retorno que ja tinha
  // prendido o painel de pos-reproducao (ver o comentario dele, logo acima).
  // `a` e o alpha da OSD (anim * entrada) e cai a zero assim que os controles
  // terminam de sumir, poucos segundos depois de abrir o titulo; a guia usa
  // seu PROPRIO relogio (pgDesde/entrada/saida, nada de `anim`) mas dependia
  // do mesmo `return` antecipado so por estar depois dele no arquivo. Sem
  // mexer no menu a OSD nunca fica de pe por tempo nenhum e a funcao nunca
  // chegava a esta linha; abrir e fechar o menu uma vez bastava para o `a`
  // ficar alto o suficiente numa passada e o painel finalmente desenhar. Os
  // diagnosticos `[pg] abriu`/`[pg] derrubado` (abaixo) continuavam mudos
  // porque pgDesde so e escrito aqui dentro — a funcao nunca alcancava esta
  // linha para escrever nada.
  //
  // Aqui havia um selo de classificacao com o GENERO do titulo ao lado, que
  // nao existe no app web — genero nao e advertencia de conteudo, e "Drama"
  // dentro de um selo laranja se le como aviso. O web mostra ate cinco linhas
  // "Categoria · Gravidade" vindas do guia parental do IMDb, com uma barra
  // vertical de 6px na cor de destaque encostada a esquerda.
  //
  //   .player-parental-guide  left 64, top 48
  //   .player-parental-line   6 de largura, raio 3, altura = a da lista
  //   .player-parental-list   padding-left 20, gap 4
  //   .player-parental-item   36 de altura
  //   rotulo 22/600 branco 85% · separador 22/400 branco 40% ·
  //   gravidade 22/400 branco 50%
  //
  // TEMPO PROPRIO, e nao o alpha do OSD. Esta guia e um AVISO DE ABERTURA: diz
  // o que o filme contem antes de a cena comecar a valer. Presa ao OSD ela
  // reaparecia toda vez que o dono mexia no controle, no meio do filme, quando
  // a informacao ja nao serve para nada — "ele deveria so aparecer animado no
  // inicio do filme e depois nao deveria aparecer mais".
  //
  // Conta de inicioImagem (o primeiro quadro com imagem, nao a abertura da
  // tela): entra escalonada linha a linha, fica PG_SEG_VISIVEL e sai. Depois
  // disso nao volta nesta reproducao.
  {
    // Ajustes > Reproducao > Classificacao no player: desligada, nada da guia sobe.
    int np = ajustes_classif_player() ? parental_n() : 0;
    // A JANELA COMECA QUANDO A RESPOSTA CHEGA, e nao no primeiro quadro.
    //
    // Era `tg < PG_SEG_TOTAL`, com tg contado do primeiro quadro: sete
    // segundos. A consulta a api.tiffara.com tem timeout de OITO. Numa rede
    // comum ela chega depois de a janela ter fechado, e os selos nunca
    // aparecem — sem erro, sem nada na tela. E a metade lenta do relato do
    // rawldon (#31): "sometimes the badges appear correctly, but most of the
    // time they are missing". A outra metade era o pedido descartado quando
    // outro estava em voo (ver parental.c).
    //
    // O teto de PG_LIMITE_SEG segura a intencao original: isto e um aviso do
    // COMECO do filme. Se a resposta demorar mais do que isso, quem esta
    // assistindo ja passou do ponto em que um aviso faz sentido, e ele nao
    // entra mais.
    float tg = inicioImagem ? (float)(agora - inicioImagem) / 1000.0f : -1.0f;
    // POR QUE ELE ABRIU, E POR QUE FECHOU. Issue #31 na segunda rodada: "I can
    // sometimes see the advisory line for only a fraction of a second, and then
    // it disappears". Com so o "[parental] -> N linhas" nao da para separar as
    // tres causas — a guia nunca chegou, chegou e a janela ja tinha fechado, ou
    // chegou, abriu e algo a derrubou antes dos sete segundos. Uma linha por
    // abertura e uma por fechamento, nunca por quadro.
    if (np > 0 && tg >= 0.0f && !pgDesde && tg < PG_LIMITE_SEG) {
      pgDesde = agora;
      printf("[pg] abriu: %d selos, %.1fs depois da imagem\n", np, tg);
      fflush(stdout);
    }
    if (np < 1 && pgDesde) {
      // Caiu a zero COM O PAINEL NO AR. So parental_pedir zera nLinhas, e ele
      // so zera quando o imdb pedido muda — ou seja, alguem trocou de titulo
      // por baixo desta reproducao.
      printf("[pg] derrubado: a guia zerou com o painel no ar (%.1fs de %.1f)\n",
             (float)(agora - pgDesde) / 1000.0f, PG_SEG_TOTAL);
      fflush(stdout);
    }
    if (np < 1) pgDesde = 0;
    if (np > 0 && pgDesde &&
        (float)(agora - pgDesde) / 1000.0f < PG_SEG_TOTAL) {
      tg = (float)(agora - pgDesde) / 1000.0f;
      // NASCE DA ILHA DO RELOGIO (plrguia.c): a pilula cresce ate o corpo com
      // as linhas e volta a hora quando a janela fecha. Cede aos outros pedidos.
      { const CatItem *ci = item();
        plrguia_pedir(tg, anim, ci ? ci->classificacao : ""); }
    }
  }

  // O "termina as" da pilula vale tambem sem o OSD (a ilha crescida das
  // faixas, os avisos): o tempo que falta e dado a ilha a cada quadro.
  // #202: "termina as" em tempo de RELOGIO: a 1,5x o resto do arquivo passa
  // em 2/3 do tempo. A ilha tambem mostra a velocidade quando nao e 1x.
  plrilha_relogio(0.0f, ehCanal() ? -1.0 : vel_tempo_real((double)(duracaoSeg - posSeg), velMed.efetiva));
  plrilha_velocidade(ehCanal() ? VEL_NORMAL : velMed.efetiva);
  { const CatItem *ct = item(); plrilha_titulo(ct ? ct->titulo : NULL); }
  float a = anim * entrada;
  // FOLHA ABERTA, OSD APAGADO. A folha de Fontes e a de Legendas sao vidro
  // translucido: o relogio, os selos 4K/HDR e o tempo do player apareciam
  // atraves dela, encavalados nos botoes da folha (foto do dono, 30/09).
  { float fa = stream_folha_anim(), fx = faixas_anim(), fe = episodios_anim();
    float cob = fa > fx ? fa : fx;
    if (fe > cob) cob = fe;
    // Os episodios nascem da ilha (episodios.c): o OSD sai, a ilha cresce.
    a *= 1.0f - anim_clamp(cob, 0.0f, 1.0f); }
  // O que NAO e barra nem tempo (titulo, meta, botoes, relogio, selos, veu de
  // cima) segue `ac`: some na busca so com a barra (#128).
  float ac = a * cheio;
  if (ponteiroNoPlayer())
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, ponteiroAcordar, ponteiroPlayPause, 0, 0);
  // O botao de pular fica POR CIMA dos degrades e dos controles: desenhado
  // antes deles, o veu de 400px do rodape o afogava assim que a barra subia —
  // era o "aparece e some" do relato. Sem controles ele e a unica coisa na tela.
  // CANAL AO VIVO: banner do zapping (sobrepoe tudo enquanto a troca espera o
  // debounce) e o OSD proprio no lugar dos controles de filme.
  if (ehCanal()) {
    if (ltv_dig_ativo(&digitosCanal)) {
      CatItem alvo;
      char fonte[64], sp[2 * LTV_DIG_MAX + 2];
      const char *d = digitosCanal.d;
      size_t i, u = 0;
      for (i = 0; d[i] && u + 2 < sizeof sp; i++) { sp[u++] = d[i]; if (d[i + 1]) sp[u++] = ' '; }
      sp[u] = 0;
      if (guia_numero_canal(atoi(d), &alvo, fonte, sizeof fonte, 0))
        aovivo_numero_desenhar(sp, alvo.titulo, fonte, entrada);
      else
        aovivo_numero_desenhar(sp, NULL, "", entrada);
    }
    if (zapEst.pend && bannerAV > 0.004f) {
      CatItem alvo;
      AoVivoBanner bn;
      EpgProg ep;
      char ag[160] = "";
      int num = 0;
      if (guia_zap_ver(player_id_canal(), zapEst.pend, &alvo)) {
        if (guia_programa_agora(alvo.imdb, time(NULL), &ep) && ep.titulo)
          snprintf(ag, sizeof ag, "%s", ep.titulo);
        guia_info_canal(alvo.imdb, &num, NULL, NULL, 0);
        bn.nome = alvo.titulo; bn.logo = alvo.poster; bn.agoraTit = ag;
        bn.numero = num; bn.salto = zapEst.pend;
        aovivo_banner_desenhar(&bn, entrada * bannerAV);
      }
    }
    if (a > 0.005f && !erroFonte) {
      // static: a ilha chama o corpo das Informacoes em plrilha_desenhar,
      // depois desta funcao, e enquanto encolhe (aovivo.h).
      static AoVivoOsd o;
      desenharLegendaExterna();
      if (!zapEst.pend) {
        avMontarOsd(&o);
#ifdef NV_TOUCH_UI
        avToquePreparar(0);
#endif
        aovivo_osd_desenhar(&o, a);
        // A HORA E A ILHA DO PLAYER, como no filme: Fontes, Audio, Legendas
        // e Informacoes crescem dela (o OSD do canal tinha pilula propria).
        plrilha_relogio(a, -1.0);
        if (o.infoAberta && o.nInfo > 0) {
          PlrIlhaPedido pd;
          memset(&pd, 0, sizeof pd);
          pd.icone = "pl_info"; pd.texto = i18n("Informações"); pd.semFim = 1;
          pd.w = 600.0f; pd.h = aovivo_info_altura(&o);
          pd.corpo = aovivo_info_corpo; pd.u = &o;
          plrilha_pedir(&pd);
        }
      }
    } else {
      desenharLegendaExterna();
    }
    return;
  }
  if (a <= 0.005f) {
    // O chrome pode estar completamente recolhido enquanto a legenda ainda
    // e conteudo do filme. Desenha-la antes do retorno preserva ASS/SRT/VTT
    // durante a maior parte da reproducao, quando a barra nao esta na tela.
    desenharLegendaExterna();
    desenharAcoesEpisodio(); return;
  }   // tocando limpo

  // OS VEUS DO GLASS UI (mockup aprovado em 03/10): so dois degrades, o de
  // cima (260, preto .52 -> 0) e o de baixo (520, .80 -> 0), pelo shader com
  // dither (GFX_VEU_CSS): no painel de 8 bits da OLED um degrade escuro longo
  // em faixas de gfx_cor sai em degraus. Na busca so com a barra o de baixo
  // fica no leve (300, .60), o .veu-b.leve do mockup.
  // OSD = CAMADA AMPLIADA (escala.h): veus, barra, titulo, botoes, tempo e
  // selos na tela virtual. A legenda logo abaixo e conteudo do filme: ela
  // sai da escala e fica no tamanho escolhido para ela.
  { ESCALA_INI();
    gfx_veu_css((GfxRect){ 0, NV_VTELA_H - 520.0f, NV_VTELA_W, 520.0f }, 0, 1.25f, 1.0f, 0.80f * a * cheio);
    gfx_veu_css((GfxRect){ 0, NV_VTELA_H - 300.0f, NV_VTELA_W, 300.0f }, 0, 1.0f, 1.0f, 0.60f * a * (1.0f - cheio));
    gfx_veu_css((GfxRect){ 0, 0, NV_VTELA_W, 260.0f }, 1, 1.38f, 1.0f, 0.52f * ac);
    ESCALA_FIM(); }

  /*
   * Legenda e conteudo, enquanto o degrade e chrome do player. Ela precisa
   * ficar acima dele: caso contrario um ASS amarelo ou vermelho recebe a
   * opacidade preta dos controles e parece oliva/marrom nas capturas com a
   * barra aberta. Mantemos esta chamada antes dos textos dos controles para
   * que a legenda continue abaixo dos botoes quando o usuario os revela;
   * quando o chrome some, o retorno acima ja desenhou a legenda sozinha.
   */
  desenharLegendaExterna();
  desenharOsd(agora, a, ac, c);
}

// O CORPO DO OSD (barra, titulo, Seekr, botoes, tempo, selos e o botao de
// pular), na camada ampliada: tudo aqui mede pela tela virtual.
static void desenharOsdCorpo(Uint32 agora, float a, float ac, const CatItem *c);
static void desenharOsd(Uint32 agora, float a, float ac, const CatItem *c) {
  ESCALA_INI();
  desenharOsdCorpo(agora, a, ac, c);
  ESCALA_FIM();
}
// FILEIRA DE FORMATO ("4K · Dolby Vision · Atmos · Inglês 5.1", como no pedido
// #4141 do NuvioTV). Vem do FLUXO, nao de constante: selo que mente e pior que
// selo ausente, porque e nele que o dono confia para saber se pegou a versao
// boa. Dois ajustes do dono (Reproducao > Durante e depois): o ESTILO (Formato
// no player) e a POSICAO (embaixo do titulo, ao lado dele ou no canto de cima,
// do lado oposto ao da ilha da hora).
//   TEXTO  em negrito, solto; PILULA  pilulas da ilha do relogio (a mesma
//   medida, material e letra da hora e do "termina as"); MARCAS  todas as marcas
//   na arte branca de badges.h, o que o OSD sempre mostrou (o padrao, no canto); SELOS  os do pacote
//   configurado, como na folha de Fontes; DESLIGADO  sem a fileira.
// Depois das marcas, o idioma e os canais da faixa tocando. Devolve a altura (0
// = nada a mostrar) e, em `larg`, a largura; `medir` so faz a conta.
#define FORMATO_FIL_H 42.0f
#define FORMATO_PIL_H 56.0f   // PIL_H da ilha do relogio (plrilha.c)
typedef struct { int tipo; int g; const char *ic; FormatoMarca f; TxtLinha l; float w; } FmtPeca;   // tipo: 0 texto, 1 arte, 2 selos do pacote; g: grupo da pilula (0 resolucao, 1 video/som, 2 idioma); ic: simbolo pequeno antes do texto (pilula)
#define FMT_IC 24.0f       // o icone da ilha: 24 com 12 de folga
#define FMT_IC_GAP 12.0f
#define FMT_PIL_PAD 24.0f  // a margem de dentro da ilha
#define FMT_PIL_SEP 25.0f  // 12 + filete de 1 + 12, como entre a hora e o "termina as"
static void fmtPecaDesenhar(const FmtPeca *p, float x, float yy, float H, float mh, float a,
                            const char *txtPac, float wPac, float hPac) {
  if (p->ic) {
    gfx_icone((GfxRect){ x, yy + (H - FMT_IC) * 0.5f, FMT_IC, FMT_IC }, p->ic, 0.953f, 0.949f, 0.937f, a);
    x += FMT_IC + FMT_IC_GAP;
  }
  if (p->tipo == 0) txt_desenhar_alpha(p->l, x, yy + (H - (float)p->l.h) * 0.5f, a);
  else if (p->tipo == 2) stream_selos_medidos(txtPac, x, yy + (H - hPac) * 0.5f, wPac + 1.0f, hPac, 0.953f, a, 0);
  else marca_formato(p->f, x, yy + (H - mh) * 0.5f, mh, 0.953f, 0.949f, 0.937f, a);
}
static float fileiraFormato(Uint32 agora, float x, float y, float ac, int medir, float *larg) {
  const float mh = 34.0f;
  const int estilo = ajustes_plr_formato(), pil = estilo == PLR_FMT_PILULA;
  const float gap = estilo == PLR_FMT_MARCAS ? 22.0f : 18.0f, H = pil ? FORMATO_PIL_H : FORMATO_FIL_H;
  FormatoMarca selos[5];
  FmtPeca p[8];
  int nSelos = 0, n = 0, i;
  float wPac = 0.0f, total = 0.0f;
  const float hPac = 30.0f;
  char txtPac[160] = "";
  if (larg) *larg = 0.0f;
  if (estilo == PLR_FMT_OFF) return 0.0f;
  // A classe sai da LARGURA primeiro: filme 2.39:1 em 1080p chega como
  // 1920x800. A faixa do 1440p (2560) fica SEM selo: ausente e mais honesto
  // que errado.
  { int w = video_largura(), h = video_altura();
    if (w >= 3200 || h >= 1800)       selos[nSelos++] = FMT_4K;
    else if (w >= 2400)               { /* 1440p: sem selo */ }
    else if (w >= 1800 || h >= 1000)  selos[nSelos++] = FMT_1080;
    else if (w >= 1200 || h >= 700)   selos[nSelos++] = FMT_720;
    else if (w > 0)                   selos[nSelos++] = FMT_SD; }
  // "Dolby Vision" so quando o PIPELINE devolveu DolbyVision no videoInfo
  // (MEDIDO: um MKV anunciado como DV volta HDR10 nesta TV).
  if (video_tem_dolby_vision())                  selos[nSelos++] = FMT_DV;
  else if (!strcasecmp(video_hdr(), "HDR10"))    selos[nSelos++] = FMT_HDR10;
#if defined(__EMSCRIPTEN__) || defined(NV_TPK)
  else {
    // AVPlay nao confirma HDR ativo: o selo e o da FONTE, nunca Dolby Vision
    // (TV Samsung nao tem DV; badges_fonte_hdr so conhece HDR10+/HDR10/HDR).
    const Stream *fonte = stream_item(stream_atual());
    int hdr = fonte ? badges_fonte_hdr_marca(fonte->badges) : -1;
    if (hdr >= 0) selos[nSelos++] = (FormatoMarca)hdr;
  }
#endif
  if (video_tem_atmos())        selos[nSelos++] = FMT_ATMOS;
  if (marcaDtsAtual() >= 0)     selos[nSelos++] = (FormatoMarca)marcaDtsAtual();
  const char *rotAudio = ehCanal() ? "" : rotuloAudioOsd();
  if (!nSelos && !rotAudio[0]) return 0.0f;
  // SELOS: os do pacote que o dono configurou (Ajustes > Selos), a mesma arte
  // da folha de Fontes, casados com o que foi MEDIDO (o codec da faixa entra
  // no casamento). Sem pacote, ou sem casar nada, cai no texto.
  if (estilo == PLR_FMT_SELOS && nSelos) {
    const VideoFaixa *fa = ehCanal() ? NULL : video_audio(video_audio_atual());
    const char *cod = fa ? audioinfo_codec(fa->codec) : "";
    for (i = 0; i < nSelos; i++) {
      size_t k = strlen(txtPac);
      snprintf(txtPac + k, sizeof txtPac - k, "%s%s", i ? " " : "", marca_formato_nome(selos[i]));
    }
    if (cod[0]) {
      size_t k = strlen(txtPac);
      snprintf(txtPac + k, sizeof txtPac - k, " %s%s", cod, fa && strstr(fa->rotulo, "Atmos") ? " Atmos" : "");
    }
    wPac = stream_selos_medidos(txtPac, 0.0f, 0.0f, 1200.0f, hPac, 0.953f, 1.0f, 1);
  }
  if (wPac > 0.0f) { p[n].tipo = 2; p[n].g = 1; p[n].ic = NULL; p[n].w = wPac; n++; }
  else for (i = 0; i < nSelos; i++, n++) {
    // MARCAS: todas as marcas na arte branca de badges.h, como o OSD sempre
    // mostrou (4K, HDR, Dolby Vision, Atmos, DTS).
    int arte = estilo == PLR_FMT_MARCAS;
    p[n].f = selos[i];
    p[n].tipo = arte;
    p[n].ic = NULL;
    p[n].g = (n == 0 && marca_resolucao(marca_formato_nome(selos[i])) >= 0) ? 0 : 1;
    if (arte) p[n].w = marca_formato_largura(selos[i], mh);
    else {
      const char *nome = marca_formato_nome(selos[i]);
      // Na pilula o simbolo da Dolby abre "Dolby Vision" e "Atmos" (o "Dolby" do
      // Atmos vira o proprio simbolo), como no pedido #4141.
      if (pil && (selos[i] == FMT_DV || selos[i] == FMT_ATMOS)) {
        p[n].ic = "marca_dolby";
        if (selos[i] == FMT_ATMOS) nome = "Atmos";
      }
      p[n].l = txt_linha(pil ? TXT_ILHA_NOME : TXT_G23B, nome, 243, 242, 239, 255);
      p[n].w = (float)p[n].l.w;
    }
    if (p[n].ic) p[n].w += FMT_IC + FMT_IC_GAP;
  }
  if (rotAudio[0]) {
    // Na pilula, o idioma tem o peso do "termina as": o segundo dado da ilha.
    p[n].tipo = 0; p[n].g = 2; p[n].ic = pil ? "aj_audio-lines" : NULL;
    p[n].l = pil ? txt_linha(TXT_G19M, rotAudio, 243, 242, 239, 153) : txt_linha(TXT_G22M, rotAudio, 243, 242, 239, 255);
    p[n].w = (float)p[n].l.w + (p[n].ic ? FMT_IC + FMT_IC_GAP : 0.0f); n++;
  }
  // LARGURA TOTAL (a conta que decide se cabe ao lado do titulo).
  if (pil) {
    int k = 0;
    while (k < n) {
      int g = p[k].g, m = k;
      float W = FMT_PIL_PAD * 2.0f;
      while (m < n && p[m].g == g) { W += p[m].w + (m > k ? FMT_PIL_SEP : 0.0f); m++; }
      total += (k > 0 ? (g == 2 ? 28.0f : 10.0f) : 0.0f) + W;
      k = m;
    }
  } else for (i = 0; i < n; i++) total += p[i].w + (i ? gap : 0.0f);
  if (larg) *larg = total;
  if (medir) return H;
  // ENTRADA ESCALONADA: o rasterizador faz poucas linhas por quadro e os
  // selos ja chegariam um a um; a curva assume a cadencia (90 ms, 10 px).
  // Conta de quando o OSD APARECEU, nao da ultima tecla: navegar nos
  // botoes ou no seek fazia os selos sumirem e voltarem a cada toque
  // (dono, 11/10). Quadro sem selos ha mais de 300 ms = OSD fechou.
  static Uint32 selosDesde, selosVisto;
  if (!selosVisto || agora - selosVisto > 300 || ac < 0.01f) selosDesde = agora;
  selosVisto = agora;
  float t0 = (float)(agora - selosDesde) / 1000.0f;
#define ENTRADA(k) (1.0f - (1.0f - anim_clamp((t0 - (k) * 0.09f) / 0.26f, 0.0f, 1.0f)) * \
                           (1.0f - anim_clamp((t0 - (k) * 0.09f) / 0.26f, 0.0f, 1.0f)))
  if (pil) {
    // PILULAS SEPARADAS, como no pedido #4141: [4K]  [Dolby Vision | Atmos]  |  [Inglês 5.1].
    // Cada grupo e uma pilula da ilha do relogio (mesmo material, 56 de altura,
    // margem de 24); dentro do grupo, o filete da ilha entre um dado e o outro.
    int k = 0;
    while (k < n) {
      int g = p[k].g, m = k;
      float W = FMT_PIL_PAD * 2.0f, eg = ENTRADA(k);
      while (m < n && p[m].g == g) { W += p[m].w + (m > k ? FMT_PIL_SEP : 0.0f); m++; }
      if (k > 0) {
        if (g == 2) {
          if (eg > 0.004f) plrui_sep(x + 13.0f, y + H * 0.5f + (1.0f - eg) * 10.0f, ac * eg);
          x += 28.0f;
        } else x += 10.0f;
      }
      if (eg > 0.004f)
        plrui_material_video((GfxRect){ x, y + (1.0f - eg) * 10.0f, W, H }, H * 0.5f, ac * eg);
      { float xi = x + FMT_PIL_PAD;
        for (i = k; i < m; i++) {
          float e = ENTRADA(i), yy = y + (1.0f - e) * 10.0f;
          if (i > k && e > 0.004f) plrui_sep(xi - 13.0f, yy + H * 0.5f, ac * e);
          if (e > 0.004f) fmtPecaDesenhar(&p[i], xi, yy, H, mh, ac * e, txtPac, wPac, hPac);
          xi += p[i].w + FMT_PIL_SEP;
        } }
      x += W; k = m;
    }
  } else {
    for (i = 0; i < n; i++) {
      float e = ENTRADA(i), yy = y + (1.0f - e) * 10.0f;
      if (e > 0.004f) fmtPecaDesenhar(&p[i], x, yy, H, mh, ac * e * 0.9f, txtPac, wPac, hPac);
      x += p[i].w + gap;
    }
  }
#undef ENTRADA
  return H;
}
static void desenharOsdCorpo(Uint32 agora, float a, float ac, const CatItem *c) {
  // O bloco inteiro desliza junto: titulo, barra e botoes sao UM objeto que
  // sobe, acompanhando o OSD (`anim`) e a abertura da tela (`entrada`, numa
  // desaceleracao: a mola crua repica e num bloco alto isso le como tremida).
  float eEnt  = 1.0f - (1.0f - entrada) * (1.0f - entrada) * (1.0f - entrada);
  float desce = (1.0f - anim) * PLR_DESLIZE
              + (1.0f - eEnt) * PLR_DESLIZE * 1.8f;

  // GEOMETRIA DO MOCKUP: a barra no y 902 (960 na busca so com a barra), os
  // botoes 38 abaixo dela, o tempo na linha dos botoes, o titulo 36 acima.
  // Tudo entre as margens da ilha (PLR_MARGEM): a barra nao vai de ponta a ponta.
  float ySo = ajustes_seekr_ligado() ? PLR_BARRA_Y_SEEKR : PLR_BARRA_Y_SO;
  float yCheio = transporteBarraY();
  float yBarra   = (yCheio + (ySo - yCheio) * (1.0f - cheio)) + desce;
  float yRowTopo = yBarra + PLR_GAP_ROW;
  float cyBotoes = yRowTopo + PLR_BTN_D * 0.5f;
  float bx = PLR_MARGEM, bw = NV_VTELA_W - PLR_MARGEM * 2.0f;
  float cx = PLR_MARGEM;
  float cw = bw;
  int transporteCurto = transporteCompacto();
  float cyTempo = transporteCurto ? yRowTopo - 16.0f : cyBotoes;
  float frac = ehCanal()
             ? fracCanal()
             : (duracaoSeg > 0.0f ? anim_clamp(posVis / duracaoSeg, 0.0f, 1.0f) : 0.0f);

  // --- barra de progresso: a MESMA de todo o player (plrui_barra) -----------
  barraPtrX = bx; barraPtrW = bw;
  if (ponteiroNoPlayer() && a > 0.3f) {
    float folga = ponteiro_tem_toque() ? 44.0f : 14.0f;
    ponteiro_alvo(bx, yBarra - folga, bw, 6.0f + folga * 2.0f, ponteiroBarra, ponteiroBuscar, 0, 0);
    ponteiro_alvo_arrastavel();
  }
  { float bufFrac = (!ehCanal() && duracaoSeg > 0.0f) ? anim_clamp(video_buffer_fim() / duracaoSeg, 0.0f, 1.0f) : 0.0f;
    // OS CORTES DO TheIntroDB (abertura, resumo, creditos) que o player ja
    // tem: onde cada trecho comeca e acaba, discretos sobre o trilho.
    float caps[16];
    int nCaps = 0;
    if (!ehCanal() && duracaoSeg > 0.0f) {
      IntroTrecho tr[8];
      int nt = intro_trechos(tr, 8), i;
      for (i = 0; i < nt && nCaps < 15; i++) {
        caps[nCaps++] = (float)(tr[i].inicio / duracaoSeg);
        if (tr[i].fim > 0.0) caps[nCaps++] = (float)(tr[i].fim / duracaoSeg);
      }
      for (i = 0; i < nCaps; i++) if (caps[i] <= 0.0f || caps[i] >= 1.0f) caps[i--] = caps[--nCaps];
    }
    plrui_barra(bx, yBarra, bw, frac, bufFrac, barraFoco, caps, nCaps, a); }

  // --- O TITULO: o LOGO, e o texto so na falta dele -------------------------
  // (O dono tinha pedido texto em 2026-09; o mockup aprovado em 03/10 volta ao
  // logo, que e o que o resto do app mostra. Logo escuro sai tingido de
  // claro, GFX_MARCA, como no detalhe.) Embaixo do logo, a linha do episodio
  // ("T1E3 · The Head", o codigo em negrito e o nome apagado) ou, num filme,
  // a meta do catalogo com pontos entre os campos.
  { float yBase = yBarra - 36.0f;
    const char *marca = (!canalSessao && c) ? artehero_logo_sessao(c) : NULL;
    GLuint logo = marca ? tex_obter_larg_qualquer(marca, 480) : 0;
    if (!canalSessao && linhaEp[0]) {
      const char *sep = strstr(linhaEp, " \xc2\xb7 ");
      char cod[64];
      size_t nc = sep ? (size_t)(sep - linhaEp) : strlen(linhaEp);
      if (nc >= sizeof cod) nc = sizeof cod - 1;
      memcpy(cod, linhaEp, nc); cod[nc] = 0;
      { TxtLinha l1 = txt_linha(TXT_G30B, cod, 243, 242, 239, 255);
        TxtLinha l2 = sep ? txt_linha_corta(TXT_G30M, sep + 1, 243, 242, 239, 153, cw * 0.6f - l1.w)
                          : (TxtLinha){ 0 };
        yBase -= (float)l1.h;
        txt_desenhar_alpha(l1, cx, yBase, ac);
        if (sep) txt_desenhar_alpha(l2, cx + l1.w + 8.0f, yBase, ac);
        yBase -= 16.0f; }
    } else if (!canalSessao && c && c->meta[0]) {
      // "2026 · 2h 37min · Aventura": os campos do catalogo separados por um
      // ponto de 4 px a 40%, 21/400 a 66%.
      char m[sizeof c->meta];
      char *q = m, *fimp;
      float x = cx, h = (float)txt_linha(TXT_CAPTION2, "Ag", 243, 242, 239, 168).h;
      int prim = 1;
      snprintf(m, sizeof m, "%s", c->meta);
      yBase -= h;
      while (q && *q) {
        fimp = strstr(q, " \xc2\xb7 ");
        if (fimp) *fimp = 0;
        if (!*q) { q = fimp ? fimp + 4 : NULL; continue; }   // campo vazio: sem ponto solto
        if (!prim) {
          gfx_cor((GfxRect){ x + 10.0f, yBase + h * 0.5f - 2.0f, 4.0f, 4.0f }, 0.5f, 0.953f, 0.949f, 0.937f, 0.40f * ac);
          x += 24.0f;
        }
        { TxtLinha l = txt_linha(TXT_CAPTION2, q, 243, 242, 239, 168);
          if (x + l.w > cx + cw * 0.6f) break;
          txt_desenhar_alpha(l, x, yBase, ac); x += l.w; }
        prim = 0;
        q = fimp ? fimp + 4 : NULL;
      }
      yBase -= 12.0f;
    }
    // A fileira de formato (Ajustes > Posicao do formato no player): EMBAIXO,
    // entre a meta e o titulo (o logo sobe junto). A posicao "Original", o canto
    // de cima, e desenhada la embaixo, junto da ilha.
    { float hfF = ajustes_plr_formato_pos() == PLR_POS_ABAIXO ? fileiraFormato(agora, cx, 0.0f, ac, 1, NULL) : 0.0f;
      if (hfF > 0.0f) { yBase -= hfF; fileiraFormato(agora, cx, yBase, ac, 0, NULL); yBase -= 14.0f; } }
    if (logo) {
      float ar = tex_aspecto(marca), w = 480.0f, h = ar > 0.0f ? w / ar : 150.0f;
      if (h > 150.0f) { h = 150.0f; w = h * ar; }
      gfx_rect((GfxRect){ cx, yBase - h, w, h }, logo,
               tex_marca_escura(marca) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0, .95f, .95f, .97f, ac);
    } else {
      const char *nome = canalSessao ? itemCanal.titulo : (c ? c->titulo : "");
      if (nome && nome[0]) {
        TxtLinha lt = txt_linha_corta(TXT_PLR_TITULO, nome, 255, 255, 255, 255, cw * 0.62f);
        txt_desenhar_alpha(lt, cx, yBase - (float)lt.h, ac);
      }
    } }
  // Depois do titulo: a miniatura fica POR CIMA dele enquanto a pessoa procura.
  seekrMiniatura(bx, bw, frac, yBarra, a);

  // --- fileira de BOTOES ---------------------------------------------------
  // Discos de 68 no material da ilha; o focado vira PILULA cheia no acento com
  // o NOME DENTRO (o rotulo solto embaixo caia em y~1042, zona de overscan). A
  // largura acompanha a mola do foco, entao os vizinhos andam junto.
  // Com o foco na barra a fileira desce PLR_DESLIZE/2 e apaga (`fileira`).
  if (fileira > 0.01f) {
    float x = cx, af = ac * fileira, dy = (1.0f - fileira) * PLR_DESLIZE * 0.5f;
    PlrAcao acoes[PLR_NBTNS]; int nAcoes = transporteAcoes(acoes);
    int toque = transporteToque(), duas = transporteDuasLinhas(), primeira = transportePrimeiraLinha();
    float celulaH = transporteCelulaH(), espaco = 12.0f / gfx_escala_ui();
    float diametro = toque ? (PLR_TOQUE_CELULA - 16.0f) / gfx_escala_ui() : transporteCurto ?
        fminf(PLR_BTN_D, (cw - (nAcoes - 1) * PLR_BTN_GAP) / nAcoes) : PLR_BTN_D;
    float yRow = yRowTopo + dy;
    for (int j = 0; j < nAcoes; j++) {
      int i = acoes[j].id, habilitada = acoes[j].habilitada;
      float f = habilitada ? focoB[i] : 0.0f, alpha = habilitada ? af : af * 0.35f;
      const char *ic = iconeBotao(i), *rot = rotuloBotao(i);
      TxtLinha lr = txt_linha(TXT_G21B, rot, 0, 0, 0, 255);
      float wCheio = 22.0f + 30.0f + 12.0f + (float)lr.w + 28.0f;
      float w = transporteCurto ? diametro : PLR_BTN_D + (wCheio - PLR_BTN_D) * f;
      GfxRect r = { x, yRow, w, diametro }, alvo = r;
      if (toque) {
        int linha = duas && j >= primeira, inicio = linha ? primeira : 0;
        int colunas = duas ? (linha ? nAcoes - primeira : primeira) : nAcoes;
        float celulaW = cw / colunas;
        alvo = (GfxRect){cx + (j - inicio) * celulaW, yRow + linha * (celulaH + espaco), celulaW, celulaH};
        r = (GfxRect){alvo.x + (celulaW - diametro) * 0.5f,
                      alvo.y + (celulaH - diametro) * 0.5f, diametro, diametro};
      }
      float cyB = r.y + r.h * 0.5f;
      int tinta = (int)(242.0f + (plrui_tinta() - 242.0f) * f);
      float k = tinta / 255.0f;
      if (f > 0.02f) plrui_pilula_foco(r, alpha * f);
      if (f < 0.98f) plrui_disco_osd(r, alpha * (1.0f - f));
      if (ponteiroNoPlayer() && af > 0.3f)
        ponteiro_alvo(alvo.x, alvo.y, alvo.w, alvo.h, habilitada ? ponteiroBotao : NULL,
                      habilitada ? ponteiroAtivarBotao : ponteiroBotaoInativo, i, 0);
      { float tamanho = toque ? 64.0f / gfx_escala_ui() : 30.0f;
        float ix = transporteCurto ? r.x + (diametro - tamanho) * 0.5f :
          x + (PLR_BTN_D - 30.0f) * 0.5f + (22.0f - (PLR_BTN_D - 30.0f) * 0.5f) * f;
        gfx_icone((GfxRect){ ix, cyB - tamanho * 0.5f, tamanho, tamanho }, ic, k, k, k, alpha * 0.96f);
        if (!transporteCurto && f > 0.05f) {
          TxtLinha l = txt_linha(TXT_G21B, rot, tinta, tinta, tinta, 255);
          gfx_recorte(r.x, r.y, r.w, r.h);
          txt_desenhar_alpha(l, ix + 30.0f + 12.0f, cyB - (float)l.h * 0.5f, af * f);
          gfx_sem_recorte();
        } }
      if (!toque) x += w + PLR_BTN_GAP;
    }
    if (transporteCurto && !barraFoco && transporteHabilitado(botao)) {
      char atual[24], dur[24], total[32];
      plrui_tempo(atual, sizeof atual, posVisSolto ? posVis : posSeg);
      plrui_tempo(dur, sizeof dur, duracaoSeg); snprintf(total, sizeof total, "/ %s", dur);
      float reserva = txt_largura(TXT_G22M, atual) + txt_largura(TXT_G22M, total) + 32.0f;
      if (cw > reserva) {
        TxtLinha l = txt_linha_corta(TXT_G21B, rotuloBotao(botao), 243, 242, 239, 235, cw - reserva);
        txt_desenhar_alpha(l, cx, cyTempo - (float)l.h * 0.5f, af);
      }
    }
  }

  // --- tempo -----------------------------------------------------------------
  // Com o OSD inteiro: "1:12:40 / 2:37:00" na linha dos botoes, o decorrido
  // claro (24/600 a 92%) e "/ total" apagado. Na busca so com a barra: o
  // decorrido a esquerda e, a direita, o que falta ("-1:16:40") — ou o total,
  // com o Seekr, cuja ilha ja diz o que falta.
  {
    char t1[24], t2[32], d[24];
    if (ehCanal()) {
      TxtLinha l = txt_linha(TXT_ILHA_NOME, i18n("AO VIVO"), 243, 242, 239, 235);
      txt_desenhar_alpha(l, cx + cw - l.w, cyTempo - (float)l.h * 0.5f, a);
    } else {
      plrui_tempo(t1, sizeof t1, posVisSolto ? posVis : posSeg);
      plrui_tempo(d, sizeof d, duracaoSeg);
      snprintf(t2, sizeof t2, "/ %s", d);
      if (cheio > 0.01f) {
        TxtEstilo estilo = transporteCurto ? TXT_G22M : TXT_ILHA_NOME;
        TxtLinha l2 = txt_linha(estilo, t2, 243, 242, 239, 128);
        TxtLinha l1 = txt_linha(estilo, t1, 243, 242, 239, 235);
        float xr = cx + cw - l2.w, y = cyTempo - (float)l1.h * 0.5f;
        txt_desenhar_alpha(l2, xr, y, a * cheio);
        txt_desenhar_alpha(l1, xr - 8.0f - l1.w, y, a * cheio);
#ifdef NV_TPK
        if (vooAtivo && !scrubbing) plrui_anel(xr - 8.0f - l1.w - 24.0f, cyTempo, 24.0f, 0, agora, a * cheio);
#endif
      }
      if (cheio < 0.99f) {
        char r[32];
        TxtLinha l1 = txt_linha(TXT_ILHA_NOME, t1, 243, 242, 239, 235), l2;
        if (ajustes_seekr_ligado()) snprintf(r, sizeof r, "%s", d);
        else { char f[24]; plrui_tempo(f, sizeof f, duracaoSeg - posSeg); snprintf(r, sizeof r, "\xe2\x88\x92%s", f); }
        l2 = txt_linha(TXT_ILHA_NOME, r, 243, 242, 239, 128);
        txt_desenhar_alpha(l1, cx, yBarra + 32.0f, a * (1.0f - cheio));
#ifdef NV_TPK
        if (vooAtivo && !scrubbing)
          plrui_anel(cx + l1.w + 26.0f, yBarra + 32.0f + l1.h * 0.5f, 24.0f, 0, agora, a * (1.0f - cheio));
#endif
        txt_desenhar_alpha(l2, cx + cw - l2.w, yBarra + 32.0f, a * (1.0f - cheio));
      }
    }
  }

  // A PILULA DA ILHA (plrilha.h): a hora e "termina as" moram nela agora, no
  // canto da Posicao do relogio. O canal nao tem fim.
  plrilha_relogio(ac, ehCanal() ? -1.0 : vel_tempo_real((double)(duracaoSeg - posSeg), velMed.efetiva));

  // POSICAO "canto" da fileira de formato (Ajustes > Posicao do formato no
  // player): no alto, no canto OPOSTO ao da ilha, centrada na altura dela, no
  // estilo escolhido. As outras posicoes saem do bloco do titulo, acima.
  if (ajustes_plr_formato_pos() == PLR_POS_CANTO) {
    float lw = 0.0f, hf = fileiraFormato(agora, 0.0f, 0.0f, ac, 1, &lw);
    if (hf > 0.0f) {
      float xx = plrilha_direita() ? NV_ILHA_MARGEM_D : NV_VTELA_W - NV_ILHA_MARGEM_D - lw;
      fileiraFormato(agora, xx, NV_ILHA_Y + (56.0f - hf) * 0.5f + desce, ac, 0, NULL);
    }
  }

  // POR CIMA DE TUDO: o painel de pos-reproducao e o mais recente na tela.
  // Ancorado pela MESMA base do painel de pausa, para nao cair sobre a barra.

  // O BOTAO DE PULAR TAMBEM COM OS CONTROLES EM PE. Ate aqui ele so era
  // desenhado no ramo "tocando limpo" (a <= 0.005, acima) — com os controles na
  // tela ele sumia, mas CONTINUAVA COM O FOCO: qualquer seta dentro da abertura
  // sobe os controles com skipFoco = 1, e o OK seguinte pulava a abertura sem
  // botao nenhum a vista. Visto na C9 em 22/09 (S1E2 de Imperfect Women: o OK
  // que devia pausar deu "seek para 149s", o fim da abertura). A posicao ja
  // acompanha `anim` (sobe acima dos controles), era so a chamada que faltava.
  desenharAcoesEpisodio();
}

#ifdef NV_SHOT_HOOKS
// CAPTURAS (tests/player_glass_shot.c): poe o player num estado de tela sem
// pipeline. `agora` e o relogio do harness (o mesmo passado a desenhar).
void player_shot_video(int sim) { comVideo = sim; shotSemFuro = sim; }
// O furo de verdade (alfa 0, como na TV) em vez da arte fazendo de video.
void player_shot_furo_real(void) { shotSemFuro = 0; }
void player_shot_canal(const AoVivoEpg *e, int numero, int atrasS, int botaoFoco, int info) {
  shotEpgOk = e != NULL; if (e) shotEpg = *e; shotNumero = numero; shotAtras = atrasS;
  botaoAV = botaoFoco; infoAV = info;
}
void player_shot_favorito(int f) { shotFav = f; }
void player_shot_estado(Uint32 agora, float pos, float dur, int toca, int bt,
                        int barra, int so) {
  posSeg = posVis = pos; posVisSolto = 0;
  if (dur > 0.0f) { duracaoSeg = dur; duracaoReal = 1; }
  tocando = toca; botao = bt; barraFoco = barra; soBarra = so;
  cheio = so ? 0.0f : 1.0f;
  fileira = barra ? 0.0f : 1.0f;
  visivel = 1; ultimoInput = agora; anim = 1.0f; entrada = 1.0f; cine.calado = 0;
  esperandoFonte = 0; erroFonte = 0;
}
// Move so o foco (sem zerar molas): o que o controle remoto faz de verdade.
void player_shot_foco(int bt, int barra) { botao = bt; barraFoco = barra; }
void player_shot_toast(Uint32 agora, const char *texto, const char *icone, int ambar, int modo) {
  if (texto) {
    snprintf(toastTexto, sizeof toastTexto, "%s", texto);
    snprintf(toastIcone, sizeof toastIcone, "%s", icone ? icone : "aj_info");
    toastCor = ambar;
  } else { toastTexto[0] = 0; aspecto = modo; }
  toastAte = agora + 60000u;
  visivel = 0; anim = 0.0f; ultimoInput = agora;
}
void player_shot_esconder(void) { visivel = 0; anim = 0.0f; }
// O aviso da ilha que esta no ar ("" sem aviso): a nota do HDR10 (dvtela.h).
const char *player_shot_toast_texto(Uint32 agora) { return toastAte > agora ? toastTexto : ""; }
void player_shot_carregando(int sim) { esperandoFonte = sim; erroFonte = 0; }
void player_shot_buscando(int sim) {
  if (sim && !scrubbing) scrubInicio = posSeg;
  if (!sim) scrubInicio = 0.0;
  scrubToque = 0;
  shotBusca = sim; scrubbing = sim; posVisSolto = sim;
}
#endif

// BRILHO DA INTERFACE DO PLAYER (esmaecer.h): o fator de cor que app.c aplica
// (gfx_osd_mult) a tudo o que o player desenha por cima do video. O degrau
// automatico so vale com o filme tocando e a barra parada.
// UI EM HDR (esmaecer.h): o fator de app.c (gfx_hdr_ui) enquanto o video esta
// em HDR/DV. Android e webOS dizem o hdrType real; o .tpk responde "none" e o
// .wgt "desconhecido", e la fica desligado.
float player_ui_hdr(void) {
  return ajustes_hdr_ui() ? esmaecer_brilho_hdr(video_hdr(), video_tem_dolby_vision()) : 1.0f;
}
float player_osd_brilho(void) {
  Uint32 agora = SDL_GetTicks();
  return esmaecer_brilho_osd(ajustes_brilho_player(),
                             agora > ultimoInput ? agora - ultimoInput : 0u,
                             tocando);
}
