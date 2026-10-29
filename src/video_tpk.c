// Player do .tpk da Samsung. Quem toca e o host .NET (Tizen.Multimedia.Player,
// tizen-tpk/Program.cs), no plano de video da TV, por baixo do GLWindow; aqui
// fica so o estado que o resto do app le (video.h) e as chamadas ao host.
//
// O host registra as funcoes dele uma vez (nv_tpk_video_registrar) e avisa o
// que acontece pelo nv_tpk_video_evento, de qualquer fio. O app le o estado no
// fio dele; por isso os campos sao volatile e nada aqui bloqueia.
//
// Faixas: o host manda a lista depois do prepare (nv_tpk_video_faixa) e o app
// escolhe por hEscolher. Legenda EMBUTIDA: o player entrega o texto por evento
// (SubtitleUpdated, com duracao) e o app desenha, igual ao Tizen web. Legenda
// EXTERNA (OpenSubtitles/addon) nem passa por aqui: legenda.c baixa e desenha.
//
// #269: legenda embutida de TEXTO num MKV (ASS, SRT, WebVTT) nao depende mais
// do SubtitleUpdated: faixas.c entrega a faixa ao overlay do app, que a le do
// MKV por Range (mkvass.c, ligado aqui por mkvass_aceitar_texto), e o player
// fica com a legenda "desligada" (legAtual = -1: o texto dele, se vier, nao e
// desenhado — sem desenho duplo). O caminho nativo continua para fonte que nao
// e MKV, faixa sem par no cabecalho e no-go do mkvass, e agora deixa rastro:
// "[video] tpk: first subtitle cue ..." ou "... no subtitle cue in 60 s ...".
#ifdef NV_TPK
#include "video.h"
#include "mediaurl.h"
#include "video_reconexao.h"
#include "idioma.h"
#include "linguas.h"
#include "audioescolha.h"
#include "mkv.h"
#include "capmkv.h"
#include "mkvass.h"
#include "rede.h"
#include "faixasmkv.h"
#include "velocidade.h"
#include "vazao.h"
#include "ajustes.h"
#include "proxyts.h"
#include "dts/dts_hls_tpk.h"
#include "dts/dts_remux.h"
#include "cdnlink.h"
#include "debrid.h"
#include "mkvfix.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef void (*FnAbrir)(const char *url, const char *cabecalhos);
typedef void (*FnSemArg)(void);
typedef void (*FnInt)(int);
typedef void (*FnRet)(int x, int y, int w, int h);
typedef int  (*FnPos)(void);

static FnAbrir  hAbrir;
static FnSemArg hParar;
static FnInt    hPausar, hBuscar, hVolume;
static FnRet    hJanela;
typedef void (*FnAspecto)(int, int, int, int, int, int, int, int);
static FnAspecto hAspecto;
static int ultRoiX, ultRoiY, ultRoiW, ultRoiH, temRoi;
static int planoX, planoY, planoW, planoH;
static int ultLegX, ultLegY, ultLegW, ultLegH;
static FnPos    hPos;
typedef void (*FnEscolher)(int tipo, int idx);
static FnEscolher hEscolher;

#define MAX_FAIXAS 32
static VideoFaixa faixaAudio[MAX_FAIXAS], faixaLeg[MAX_FAIXAS];
static volatile int nAudio, nLeg, audioAtual, legAtual = -1;
static SDL_mutex *travaLeg;
static char legTexto[1024];
static Uint32 legAte;
// Host cue callbacks may run on another thread; only access under travaLeg.
static int legCuesBloqueados = 1;
// #269 DIAGNOSTICO DO SubtitleUpdated. Nenhum log de TV jamais mostrou um cue
// nativo chegando, entao nao se sabia se o evento dispara no Tizen 9. Sob
// travaLeg: `legEnvAtivo`/`legEnvEm` = ha escrita de faixa no player, e
// quando; `legEnvFaixa` = qual; `legCueSessao` = ja veio
// algum cue nesta sessao, `legCueEnv` = ja veio cue depois da escrita.
// `legSemCueMs` (fio do app) = tempo TOCANDO desde a escrita, sem cue.
static Uint32 legEnvEm, legAbriuEm;
static int legEnvAtivo;
static int legEnvFaixa = -1, legCueSessao, legCueEnv, legSemCueLogado;
static Uint32 legSemCueMs, legBombeouEm;
#ifndef LEG_SEM_CUE_MS
#define LEG_SEM_CUE_MS 60000u   // o teste encurta
#endif

static char urlAtual[NV_MEDIA_URL_MAX];
static char cabecalhos[2048];
static volatile int ativo, pronto, falhou, terminou, tocando, largura, altura;
// video_parar ate o proximo video_tocar: nada abre o conversor (convIniciar).
static int parado;
static volatile int conflito;   // ver video_tpk_log_host
static volatile int durMs, bufferando;
// VELOCIDADE (#202, video.h). O host aplica por escolher(3, centesimos) —
// Player.SetPlaybackRate — e responde no evento EV_VELOCIDADE (a = centesimos,
// b = 1 aceitou / 0 recusou). A documentacao do Tizen.Multimedia diz que o
// SetPlaybackRate lanca InvalidOperationException em "Streaming playback", e
// toda fonte do app e streaming: a linha aparece, o primeiro pedido e a prova,
// e a recusa a esconde pelo resto da execucao.
static volatile int velPedida = 100, velEnviada = 100, velRecusada;
static volatile Uint32 bufferDesde;
static volatile int bufPct;   // o ultimo evento 7 do host (0..100)
static unsigned sessao;
// The host reports numeric errors from another thread. Preserve that evidence
// for the source failure screen without guessing a codec or network cause.
static atomic_uint erroDetalhe;
static atomic_int temErroDetalhe;
// DEFERRED SELECTION (see escolhasPendentes). The track choice is born too
// soon after open and the write is swallowed: measured on the TV, the subtitle
// write at 0.00 s of the first tick was accepted by the player's bookkeeping
// and NEVER applied to the demuxer. Audio only needed the deferral (measured:
// the Korean audio came up right); the subtitle waits for the player to settle.
// Counted from the first FRAME, not from EV_TOCANDO - see escolhasPendentes.
static volatile int comecou;                 // the FRAME arrived (not EV_TOCANDO)
static Uint32 comecouEm;                     // when the frame arrived (SDL clock)
static volatile int audioComecou;            // audio only needs the first tick
static int audioPend = -1, legPend = -1;     // choice waiting to be written
// A seek landing right after a track write resets demuxer selection to track 0.
// Choices wait while a seek is in flight and re-send if the seek collided.
static int legReescrever = -1;
static int legEscrito = -1;
static int seekNearWrite;
static atomic_int legSeekChegou;
#define LEG_ESPERA_SEEK_MS 10000u
static Uint32 legEsperaSeekEm;

// RECONEXAO (video_reconexao.h). O evento 5 chega de qualquer fio e so ANOTA;
// a decisao e o recarregar sao do video_bombear. A classe do erro sai do
// codigo do evento ou da linha "erro ConnectionFailed" que o host loga logo
// antes dele (Video.cs, ErrorOccurred).
static NvReconexao recon;
static int reconProxima, reconPermitida, reconIniciou;
static volatile int reconErroPend, reconErroCod, reconLinhaRede;
// Escolhas no instante da queda, e o que falta devolver ao recarregar que abriu.
static int reconAudio = -1, reconLeg = -1;
static volatile int reconFaixasPend;
static int reconBuscarMs = -1;
// #412: relogio congelado com estado Playing, fora de pausa/buffer/seek.
#define TPK_PRESO_MS 15000u
static Uint32 progressoEm;
static double progressoPos;
static int vigiaAtiva, pausaPedida;
static atomic_int vigiaReiniciar;
static atomic_int playerPreso = -2;   // -2 = sem aviso (0 e um relogio valido)
// Ultima consulta de >= 5 s (0 = nenhuma). So vale recente: uma consulta lenta
// isolada nao pode armar o watchdog numa pausa legitima.
#define TPK_LENTA_VALE_MS 11000u   // ~2 consultas de 5 s seguidas
static atomic_uint consultaLentaEm;
static int reconPausa = -1;
// Seek enviado sem confirmacao do host: a recuperacao retoma nele.
static atomic_int buscaAlvoMs = -1;
// ---- RELOGIO DA LEGENDA (dono 25/10) ------------------------------------------
// Leituras reais da posicao, carimbadas pelo host no MEIO da consulta
// (nv_tpk_video_posicao). Antes: depois de um seek o host guardava o ALVO como
// posicao (a TV pula para o quadro-chave, 6,3 s antes no log de 25/10) e na
// pausa nao relia nada; a legenda corria segundos a frente (letreiros do
// celular no bone da menina, a caixa do INBOX fora do celular).
static pthread_mutex_t amTrava = PTHREAD_MUTEX_INITIALIZER;
static struct { unsigned seq; int valido, ms; double carimbo, incerteza; } am;
static double amBusca;                 // monoAgora do ultimo EV_BUSCOU (sob amTrava)
static atomic_int posIncerta;          // 1 = seek feito, ainda sem leitura real
static atomic_uint incertaTick;        // SDL_GetTicks do seek (1,5 s no maximo)
static double monoAgora(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}
static int reconAlvoMs = -1;

// CABECALHO DO MKV (#206). O player do host so da o idioma de cada faixa; o
// Name ("Forced", "DD 5.1"), a FlagForced e os canais estao nas TrackEntry, e
// o .wgt ja os mostra por isso. Fonte, nesta ordem: o trecho que a pre-busca
// do mkvass ja leu (sem rede, e o caso comum: no log da 1.6.5 da QE65Q80A o
// cabecalho chegou ANTES das faixas), senao um Range proprio num fio, so com o
// video andando ha 5 s (mesmo gatilho do video_tizen.c). A RECONEXAO reabre o
// MESMO arquivo: o que foi lido continua valendo.
// mkvEstado: 0 nada a fazer, 1 procurando, 2 fio na rede, 3 lido (ou desistiu).
static MkvFaixa mkvFx[MKV_MAX_FAIXAS];
static volatile int mkvN, mkvEstado, faixasNovas;
// #269: a folha pediu a sonda ja (video_sondar_mkv_agora); `mkvNaoMkv`: a
// sonda pela rede voltou sem TrackEntry nenhuma (nao e MKV).
static volatile int mkvSondarJa, mkvNaoMkv;
static unsigned mkvGeracao;
// AUDIO QUE A TV RECUSA (#313). A Samsung nao toca DTS (2018+) nem TrueHD: o
// player so lista as faixas que decodifica, entao um MKV so com DTS chega com a
// lista de audio vazia e o filme roda mudo, sem erro nenhum. O cabecalho do MKV
// (mkvFx) diz o que o arquivo tem; a lista do player (nAudio) diz o que a TV
// aceitou. audioNaoSup = 1 so quando TODA faixa de audio do arquivo e de um
// codec que a TV nao toca E a TV nao listou nenhuma (lista com ao menos uma
// faixa quer dizer que ha audio saindo: nao se avisa). NAO PROVADO em TV: o que
// o Player devolve exatamente nesse caso; a linha [audio] do log e a prova.
static volatile int audioNaoSup, faixasLidas;
static int audioLogado;
static int fonteMp4;
static Uint32 mkvOlhou;

// CONVERSAO DTS/TrueHD -> DOLBY DIGITAL (docs/features/dts/README.md).
// O player da TV abre o fluxo convertido do servidor local (dts/dts_hls_tpk.c);
// urlAtual SEGUE sendo a fonte original, porque legenda (mkvass), capitulos e a
// sonda do MKV leem dela. Gatilho, sem mudar nada no que ja toca:
//  - TODA faixa de audio do MKV e DTS/TrueHD/MLP e a TV nao listou nenhuma (ou
//    acusou NotSupportedAudioCodec): converte, na posicao em que estava;
//  - o cabecalho ja estava lido antes de abrir (pre-busca do mkvass): ja abre
//    convertido, sem passar pelo original mudo;
//  - a pessoa escolhe, no menu de audio, uma faixa DTS/TrueHD (que a TV nao
//    lista): converte essa faixa.
// Arquivo com faixa que a TV toca segue tocando ela; as DTS/TrueHD aparecem no
// menu com "-> Dolby Digital 5.1". Ajuste de teste "Desligado" = como o master.
// Qualquer falha antes do primeiro quadro volta ao original (mudo) e avisa.
static struct {
  int ativo;            // o host toca o fluxo convertido
  int track;            // TrackNumber do MKV convertido
  int modo, prog;       // DTSTPK_*; prog = progressivo (posicao com deslocamento)
  char url[256];        // o que o host abriu
  double desloc;        // progressivo: posicao = player + desloc
  int deslocOk;
  int tocou;            // o convertido ja mostrou quadro (desde a ultima abertura)
  int jaTocou;          // ... alguma vez nesta fonte: dai em diante erro e reconexao, nao desistencia
  int desistiu;         // falhou nesta fonte: nao tenta de novo
  int buscarMs;         // seek depois de abrir (troca no meio do filme), -1 nada
  Uint32 abriuEm;
  int seekPend; double seekAlvo; Uint32 seekEm;
  int voltar;           // voltando ao original: escolher a faixa `rotuloVolta`
  char rotuloVolta[48];
  int recusadas;        // entradas DTS/TrueHD acrescentadas ao menu (original)
  int offLogado;
  double inicio;        // posicao da fonte em que o convertido abriu
  int refezProg;        // HLS sem indice (MKV sem Cues) ja refeito no TS continuo
  // MP4 / fonte que nao e MKV: a sonda do conversor roda ao lado do original
  // (que segue tocando) e so troca quando acha DTS/TrueHD (convBombear).
  int sondando, naoSup; // naoSup: a TV acusou NotSupportedAudioCodec
  // REABRINDO: o convertido abre outro player (seek do progressivo, reconexao,
  // troca no meio do filme) numa fonte que ja mostrou quadro. Para o app isso e
  // um seek, nao uma fonte abrindo: video_pronto() segue 1 e video_pos() fica
  // no alvo, senao o cartao "Abrindo fonte" e os prazos de abertura do app.c
  // (aberturaVencida, escolha manual) tratam a fonte como morta.
  int reabrindo; double posMantida; Uint32 reabriuEm, reabTocandoEm;
  int fonteTocou;       // esta fonte (original ou convertida) ja andou > 0,5 s
  double posConhecida;  // ultima posicao real do convertido (nunca 0 por falta de dado)
} cv = { .buscarMs = -1 };
static Uint32 faixasEm;           // quando a ultima lista de faixas do player chegou
static volatile int naoSupPend;   // NotSupportedAudioCodec veio do host (outro fio)
// RESGATE PELO CONVERSOR (dono, 16/10/2026). O player acusa
// AudioCodecNotSupported (0xfe6c002e) num arquivo so com DTS/TrueHD; o erro
// virava `falhou` na hora, e o app punha o cartao de erro e marcava a fonte
// morta 2 s antes de o conversor assumir (TTGA so DTS: o convertido chegou a
// tocar 21 s atras do cartao). Agora, com a conversao possivel, esse erro abre
// uma janela: o conversor tem ate RESGATE_MS para assumir; erros do original
// nessa janela (o Prepare que falha depois) nao contam. Sem conversor ate o
// fim da janela: `falhou`, como antes.
#define TPK_ERRO_AUDIO 0xFE6C002Eu
#define RESGATE_MS 20000u
static Uint32 resgateAte;
int video_faixa_dts(const VideoFaixa *f);
static int convIniciar(int track, double pos, const char *motivo);
static void convDesistir(const char *motivo);
static void convVoltar(void);
static int convPossivel(void);
static double posReal(void);

__attribute__((visibility("default")))
void nv_tpk_video_registrar(FnAbrir abrir, FnSemArg parar, FnInt pausar, FnInt buscar,
                            FnInt volume, FnRet janela, FnPos pos) {
  hAbrir = abrir; hParar = parar; hPausar = pausar; hBuscar = buscar;
  hVolume = volume; hJanela = janela; hPos = pos;
}

__attribute__((visibility("default")))
void nv_tpk_video_registrar_faixas(FnEscolher escolher) { hEscolher = escolher; }

__attribute__((visibility("default")))
void nv_tpk_video_registrar_aspecto(FnAspecto janela) { hAspecto = janela; }

/* Host (qualquer fio): uma leitura real da posicao. motivo 0 = a consulta de
 * sempre (tocando), 1 = logo depois de um seek, 2 = logo depois da pausa,
 * 3 = a pausa assentou (~300 ms depois). carimboNs = o meio da consulta no
 * Stopwatch do .NET, que no Linux e o CLOCK_MONOTONIC em ns. */
__attribute__((visibility("default")))
void nv_tpk_video_posicao(int ms, int motivo, int consultaMs, long long carimboNs) {
  double agora = monoAgora(), c = (double)carimboNs / 1e9, inc = consultaMs > 0 ? consultaMs / 2000.0 : 0.0;
  if (!ativo || ms < 0) return;
  if (c > agora + 0.05 || c < agora - 10.0) c = agora - inc;   // relogio diferente: o fim menos meia consulta
  pthread_mutex_lock(&amTrava);
  /* Consulta que COMECOU antes do ultimo seek (chega depois do EV_BUSCOU, ate
   * 600 ms mais lenta): a posicao e a de antes do salto. Fora — ela tirava o
   * posIncerta com a posicao velha e a legenda da outra cena aparecia. */
  if (c - inc < amBusca) { pthread_mutex_unlock(&amTrava); return; }
  am.ms = ms; am.carimbo = c; am.incerteza = inc; am.seq++; am.valido = 1;
  pthread_mutex_unlock(&amTrava);
  if (motivo <= 1) atomic_store(&posIncerta, 0);   // a TV ja disse onde caiu depois do seek
}

void video_escolher_audio(int i);

static int convPossivel(void);
static int faixasDaTv(void);
// A ORDEM DO AUDIO (audioescolha.h) sobre a lista do menu: as faixas que a TV
// toca e as DTS/TrueHD do arquivo que o menu acrescenta (acrescentarRecusadas;
// escolhida, converte). Sem escolha da regra, fica a do arquivo.
static void escolherAudioPreferido(void) {
  AudioCand c[MAX_FAIXAS];
  int i, o, k;
  // Convertendo, a lista e a do arquivo e a faixa ja foi escolhida.
  if (cv.ativo || nAudio < 1) return;
  for (i = 0; i < nAudio; i++) {
    const VideoFaixa *f = &faixaAudio[i];
    c[i].idioma = f->idioma; c[i].codec = f->codec; c[i].rotulo = f->rotulo; c[i].canais = f->canais;
    c[i].dts = f->numero < 0 || audioesc_dts_truehd(f->codec, f->rotulo);
    c[i].pode = f->numero >= 0 || convPossivel();
    c[i].numero = video_audio_numero_memoria(f);
  }
  k = audioesc_escolher(c, nAudio, &o);
  if (k >= 0 && k == audioAtual) {
    // A prova de que a ordem rodou mesmo quando nada muda (BD T3: a TV esconde
    // o DTS japones e a faixa 0 dela ja e o FLAC ingles).
    printf("[video] audio pela ordem (%s): fica %s (faixa %d de %d; a TV listou %d)\n", audioesc_origem_nome(o),
           faixaAudio[k].rotulo, k + 1, nAudio, faixasDaTv());
    fflush(stdout);
  }
  if (k < 0 || k == audioAtual) return;
  printf("[video] audio pela ordem (%s): %s (faixa %d de %d)%s\n", audioesc_origem_nome(o), faixaAudio[k].rotulo,
         k + 1, nAudio, faixaAudio[k].numero < 0 ? ", DTS/TrueHD: converte" : "");
  fflush(stdout);
  video_escolher_audio(k);
}

// Host, fio principal, depois do prepare: uma chamada por faixa (tipo 0 =
// audio, 1 = legenda) e no fim nv_tpk_video_faixas_fim. O contador so sobe no
// fim, com a lista inteira escrita: o app nunca le faixa pela metade.
static VideoFaixa novasA[MAX_FAIXAS], novasL[MAX_FAIXAS];
static int nNovasA, nNovasL;
__attribute__((visibility("default")))
void nv_tpk_video_faixa(int tipo, int idx, const char *lingua) {
  VideoFaixa *f;
  const char *l = lingua ? lingua : "";
  if (!ativo) return;   // lista tardia depois de sair (como nv_tpk_video_evento)
  if (tipo == 0) { if (nNovasA >= MAX_FAIXAS) return; f = &novasA[nNovasA++]; }
  else           { if (nNovasL >= MAX_FAIXAS) return; f = &novasL[nNovasL++]; }
  memset(f, 0, sizeof *f);
  printf("[video] tpk faixa %s%d idioma=%s\n", tipo ? "L" : "A", idx, l[0] ? l : "-");
  f->numero = idx;
  f->ordinalMkv = tipo ? idx : -1;
  if (strcmp(l, "und") && strcmp(l, "unknown")) snprintf(f->idioma, sizeof f->idioma, "%s", l);
  if (f->idioma[0]) snprintf(f->rotulo, sizeof f->rotulo, "%s", i18n(ling_nome(f->idioma)));
  else snprintf(f->rotulo, sizeof f->rotulo, "%s %d", i18n(tipo ? "Legenda" : "Áudio"),
                tipo ? nNovasL : nNovasA);
}
__attribute__((visibility("default")))
void nv_tpk_video_faixas_fim(int selAudio, int selLeg) {
  // Segunda leitura (o host rele o audio com o video ja tocando, #165): so o
  // audio muda; a legenda que o app ja escolheu fica.
  int releitura = (nAudio || nLeg) && !nNovasL && nLeg;
  // Depois de sair: a lista e do player que ja fechou; nao repovoa o menu nem
  // dispara a ordem do audio (video_parar ja zerou tudo).
  if (!ativo) { nNovasA = nNovasL = 0; return; }
  memcpy(faixaAudio, novasA, sizeof novasA);
  audioAtual = selAudio >= 0 ? selAudio : 0;
  if (!releitura) {
    memcpy(faixaLeg, novasL, sizeof novasL);
    legAtual = -1;   // a TV ate pode ter uma escolhida; quem liga e o app (faixas.c)
    nLeg = nNovasL;
  }
  (void)selLeg;
  nAudio = nNovasA;
  nNovasA = nNovasL = 0;
  faixasNovas = 1;   // video_bombear reaplica o cabecalho do MKV, se ja lido
  faixasLidas = 1;
  faixasEm = SDL_GetTicks();
  audioLogado = 0; audioNaoSup = 0;   // o host rele o audio em 2 s (#165): decide de novo
  cv.recusadas = 0;                   // lista nova: as DTS/TrueHD do menu voltam no proximo bombear
  printf("[video] faixas: %d audio, %d legenda%s\n", nAudio, nLeg, cv.ativo ? " (fluxo convertido)" : "");
  fflush(stdout);
  // Convertido: a lista que vale e a do arquivo (sondaMkv a monta); a do player
  // e so a faixa AC3 que ele recebe.
  if (cv.ativo) { reconFaixasPend = 0; return; }
  // Recarregar de reconexao: as faixas que a pessoa tinha, e nao a preferencia.
  // Sem audio ainda (o host rele em 2 s, #165), o audio fica para a releitura.
  if (reconFaixasPend) {
    if (!releitura && reconLeg >= 0 && reconLeg < nLeg) video_escolher_legenda(reconLeg);
    if (nAudio > 0) {
      reconFaixasPend = 0;
      if (reconAudio > 0 && reconAudio < nAudio) video_escolher_audio(reconAudio);
    }
    printf("[video] reconexao: faixas devolvidas (audio %d, legenda %d)\n", reconAudio, reconLeg);
    fflush(stdout);
    return;
  }
  // Com o cabecalho do MKV lido, a ordem roda em sondaMkv, com os rotulos dele e
  // as DTS/TrueHD do arquivo no menu; sem ele, ja, so com o que a TV listou.
  if (mkvN == 0) escolherAudioPreferido();
}

// Host: texto da legenda embutida escolhida, valido por `durMs`.
__attribute__((visibility("default")))
void nv_tpk_video_legenda(const char *texto, int durMs) {
  int primeiroSessao = 0, primeiroEnv = 0, faixa = -1, chars = texto ? (int)strlen(texto) : 0;
  Uint32 agora = SDL_GetTicks(), desdeEnv = 0, desdeAbriu = 0;
  // The player emits an empty cue right after the write; it must not consume
  // the one-shot evidence line, or the log says "0 chars" and nothing more.
  int temTexto = chars > 0;
  if (!travaLeg) return;
  SDL_LockMutex(travaLeg);
    // O cue que chega DURANTE a escrita (bloqueado) e da faixa velha: nao prova
  // a nova.
  if (temTexto && !legCueSessao) { legCueSessao = 1; primeiroSessao = 1; desdeAbriu = agora - legAbriuEm; }
  if (temTexto && legEnvAtivo && !legCueEnv && !legCuesBloqueados) {
    legCueEnv = 1; primeiroEnv = 1; desdeEnv = agora - legEnvEm; faixa = legEnvFaixa;
  }
  SDL_UnlockMutex(travaLeg);
  if (primeiroEnv)
    printf("[video] tpk: first subtitle cue after %u ms, %d chars (track=%d, dur %d ms)\n",
           (unsigned)desdeEnv, chars, faixa, durMs);
  else if (primeiroSessao)
    printf("[video] tpk: first subtitle cue of the session %u ms after open, %d chars (no track written)\n",
           (unsigned)desdeAbriu, chars);
  if (primeiroEnv || primeiroSessao) fflush(stdout);
  SDL_LockMutex(travaLeg);
  if (legCuesBloqueados) { SDL_UnlockMutex(travaLeg); return; }
  snprintf(legTexto, sizeof legTexto, "%s", texto ? texto : "");
  legAte = SDL_GetTicks() + (Uint32)(durMs > 0 ? durMs : 3000);
  SDL_UnlockMutex(travaLeg);
}

enum { EV_PRONTO = 1, EV_TOCANDO = 2, EV_PAUSADO = 3, EV_FIM = 4, EV_ERRO = 5,
       EV_TAMANHO = 6, EV_BUFFER = 7, EV_VELOCIDADE = 8,
       EV_PRESO = 9, EV_CONSULTA_LENTA = 10, EV_BUSCOU = 11, EV_BUSCA_FALHOU = 12 };

// SEEK QUE A TV RECUSA NUM MKV DE INDICE ESCONDIDO (mkvfix.h). `hostUrl` e o
// que o player recebeu de verdade (link do CDN, do addon ou do TorBox): e ele
// que a sonda le, sem ir ao addon de novo.
static char hostUrl[sizeof urlAtual], hostCab[sizeof cabecalhos];
static struct {
  int sondou;            // a sonda deste arquivo ja saiu
  int ativo;             // o player le pelo servidor local
  int pend; double pendAlvo; Uint32 pendEm;   // seek pedido com a sonda em curso
  int avisou;            // "nao da para buscar" ja foi dito nesta fonte
  // Legendas de texto escondidas da TV (mkvfix.h): a abertura espera a sonda
  // quando o cabecalho ja mostrou ASS/SSA; senao, uma reabertura cedo.
  int esperando; Uint32 esperaDesde;
  int reabriuLeg;
} mf;
static char mfEspUrl[sizeof urlAtual], mfEspCab[sizeof cabecalhos];
static int mfEsperarAntes(const char *url, const char *cab);
static atomic_int mfRecusas;   // EV_BUSCA_FALHOU (fio do host)
static atomic_int mfAviso;     // video_tpk_aviso_busca consome
static atomic_uint buscaFalhas;  // EV_BUSCA_FALHOU, so cresce (player.c: seek em voo)
static void abrirHost(const char *url, const char *cab) {
  char seguros[sizeof cabecalhos];
  const char *orig = strstr(urlAtual, "://"), *dest = url ? strstr(url, "://") : NULL;
  size_t no = orig ? (size_t)(orig + 3 - urlAtual) + strcspn(orig + 3, "/?#") : 0;
  size_t nd = dest ? (size_t)(dest + 3 - url) + strcspn(dest + 3, "/?#") : 0;
  // Across origins, forward only transport preferences, never source secrets.
  if (cab && *cab && (!no || no != nd || strncasecmp(urlAtual, url, no))) {
    size_t used = 0;
    seguros[0] = 0;
    for (const char *p = cab; *p;) {
      size_t n = strcspn(p, "\r\n");
      if (!strncasecmp(p, "User-Agent:", 11) || !strncasecmp(p, "Accept:", 7) ||
          !strncasecmp(p, "Accept-Language:", 16)) {
        if (n + 1 < sizeof seguros - used) {
          memcpy(seguros + used, p, n); used += n;
          seguros[used++] = '\n'; seguros[used] = 0;
        }
      }
      p += n; while (*p == '\r' || *p == '\n') p++;
    }
    cab = seguros;
  }
  if (!mkvfix_e_url(url)) {
    snprintf(hostUrl, sizeof hostUrl, "%s", url ? url : "");
    snprintf(hostCab, sizeof hostCab, "%s", cab ? cab : "");
  }
  if (mfEsperarAntes(url, cab)) return;   // mfBombear abre quando a sonda responder
  hAbrir(nv_media_url(url), cab);
}

__attribute__((visibility("default")))
void nv_tpk_video_evento(int tipo, int a, int b) {
  if (!ativo) return; // evento tardio depois de sair
  if ((tipo == EV_TOCANDO && !tocando) || tipo == EV_PAUSADO ||
      (tipo == EV_BUFFER && bufferando != (a < 100)))
    atomic_store(&vigiaReiniciar, 1);
  switch (tipo) {
    case EV_PRESO: { // -1 = ultimo relogio; GetState nao apaga alvo de seek.
      int vazio = -2;
      if (a >= 0) atomic_store(&playerPreso, a);
      else atomic_compare_exchange_strong(&playerPreso, &vazio, -1);
      break;
    }
    case EV_BUSCOU:
      { int alvo = a;   // the CAS writes the current value back into alvo on failure
        if (atomic_compare_exchange_strong(&buscaAlvoMs, &alvo, -1))
          atomic_store(&legSeekChegou, 1); }
      // O host so sabe o ALVO aqui; onde a TV caiu chega pela proxima leitura.
      pthread_mutex_lock(&amTrava); amBusca = monoAgora(); pthread_mutex_unlock(&amTrava);
      atomic_store(&incertaTick, SDL_GetTicks() | 1u);
      atomic_store(&posIncerta, 1);
      break;
    case EV_CONSULTA_LENTA: atomic_store(&consultaLentaEm, SDL_GetTicks() | 1u); break;
    case EV_PRONTO:  durMs = a; pronto = 1; break;
    case EV_TOCANDO: tocando = 1; bufferando = 0; break;
    case EV_PAUSADO: tocando = 0; break;
    case EV_FIM:     terminou = 1; tocando = 0; break;
    // Sem `falhou` aqui: o video_bombear decide entre reconectar e desistir.
    case EV_ERRO:    atomic_store(&erroDetalhe, (unsigned)a);
                     atomic_store(&temErroDetalhe, 1);
                     reconErroCod = a; reconErroPend = 1; tocando = 0;
                     printf("[video] tpk: player error 0x%08x (%d)\n", (unsigned)a, b); break;
    case EV_TAMANHO: largura = a; altura = b; break;
    case EV_BUFFER:
      bufPct = a < 0 ? 0 : a > 100 ? 100 : a;
      if (a < 100 && !bufferando) { bufferando = 1; bufferDesde = SDL_GetTicks(); }
      else if (a >= 100) bufferando = 0;
      break;
    case EV_BUSCA_FALHOU: atomic_fetch_add(&mfRecusas, 1); atomic_fetch_add(&buscaFalhas, 1u); break;
    case EV_VELOCIDADE:
      vel_log(a, b ? "plataforma ok" : "plataforma recusou");
      if (!b) { velRecusada = 1; velPedida = 100; }
      break;
    default: break;
  }
  if (tipo != EV_BUFFER) { printf("[video] tpk evento %d (%d, %d)\n", tipo, a, b); fflush(stdout); }
}

static void legendaLimpar(int bloquear) {
  if (!travaLeg) return;
  SDL_LockMutex(travaLeg);
  legTexto[0] = 0; legAte = 0; legCuesBloqueados = bloquear;
  SDL_UnlockMutex(travaLeg);
}
// #269: marca a escrita da faixa `i` no player (ou -1 = nenhuma), para o
// diagnostico do primeiro cue / do silencio de 60 s.
static void legendaMarcarEnvio(int i) {
  if (!travaLeg) return;
  SDL_LockMutex(travaLeg);
  legEnvAtivo = i >= 0; legEnvEm = SDL_GetTicks();
  legEnvFaixa = i; legCueEnv = 0;
  SDL_UnlockMutex(travaLeg);
  legSemCueMs = 0; legSemCueLogado = 0;
}
static void legendaEnviar(int i) {
  // Discard the cached old cue and callbacks during the host write. The host
  // callback has no track/session identity; late cues after dispatch cannot
  // be identified here and still require the host's track switch to work.
  legendaLimpar(1);
  legendaMarcarEnvio(i);
  if (hEscolher) hEscolher(1, faixaLeg[i].numero);
  legEscrito = i;
  legendaLimpar(0);
}

// #269: escrita feita, video TOCANDO ha 60 s e nenhum cue do player: o
// SubtitleUpdated nao entrega esta faixa nesta TV. Uma linha por escrita, com
// o codec do cabecalho do MKV (o player nao expoe codec de legenda: a
// SubtitleTrackInfo do Tizen.Multimedia so tem GetCount/GetLanguageCode/
// Selected). Conta tempo de reproducao, nao de relogio: pausa e buffer nao
// valem como silencio.
static void legendaVigiarSilencio(Uint32 agora) {
  int env, cue, faixa, l = legAtual;
  Uint32 dt = legBombeouEm ? agora - legBombeouEm : 0;
  legBombeouEm = agora;
  if (!travaLeg || legSemCueLogado || l < 0) return;
  SDL_LockMutex(travaLeg);
  env = legEnvAtivo; cue = legCueEnv; faixa = legEnvFaixa;
  SDL_UnlockMutex(travaLeg);
  if (!env || cue || faixa != l) return;
  if (tocando && !bufferando && dt < 1000u) legSemCueMs += dt;
  if (legSemCueMs < LEG_SEM_CUE_MS) return;
  legSemCueLogado = 1;
  printf("[video] tpk: no subtitle cue in %u s (track=%d, lang=%s, codec=%s, mkv probe=%d)\n",
         LEG_SEM_CUE_MS / 1000u, faixa, faixaLeg[l].idioma[0] ? faixaLeg[l].idioma : "-",
         faixaLeg[l].codec[0] ? faixaLeg[l].codec : "unknown", mkvEstado);
  fflush(stdout);
}

int  video_iniciar(void) {
  if (!travaLeg) travaLeg = SDL_CreateMutex();
  // #269: legenda de texto simples do MKV pelo overlay (ver faixas.c).
  mkvass_aceitar_texto(1);
  mkvass_aceitar_imagem(1);
  return hAbrir != NULL;
}
int  video_iniciar_auto(void) { return hAbrir != NULL; }
int  video_registro_negado(void) { return 0; }

// LINK DO CDN (cdnlink.h). O player recebe o endereco final no CDN em vez do
// link do addon (o MediaFusion leva 2-4 s no primeiro pedido e o player desiste
// em ~3 s). urlAtual segue sendo o link do addon: conversor, legendas, sonda do
// MKV e o resto leem dele. Sem ganchos (testes) nada disto roda.
static CdnlinkGanchos cdnG;
static struct {
  int pend;          // resolvendo num fio: o hAbrir espera (cdnBombear)
  unsigned ger;      // pedido em curso
  Uint32 pediuEm;
  int usado;         // o player desta sessao recebeu o link do CDN
  Uint32 em;         // quando esse link foi resolvido (relogio SDL)
  int avisouVelho;
  int arquivo, usenet, item, file;
} cdn;
static atomic_uint cdnGer, cdnFeitoGer;
static pthread_mutex_t cdnTrava = PTHREAD_MUTEX_INITIALIZER;
static char cdnFinal[NV_MEDIA_URL_MAX];
static int cdnOk;
typedef struct { char orig[NV_MEDIA_URL_MAX]; char cab[2048]; unsigned ger; int arquivo, usenet, item, file; } CdnPedido;

void video_tpk_cdn_arquivo(const char *url, int usenet, int item, int file) {
  if (!ativo || !url || strcmp(url, urlAtual) || item <= 0 || file < 0) return;
  cdn.arquivo = 1; cdn.usenet = usenet; cdn.item = item; cdn.file = file;
  cdn.usado = 1; cdn.em = SDL_GetTicks();
}

__attribute__((visibility("default")))
void video_tpk_cdn_ganchos(const CdnlinkGanchos *g) {
  if (g) cdnG = *g; else memset(&cdnG, 0, sizeof cdnG);
}

static void *fioCdn(void *arg) {
  CdnPedido *p = arg;
  char fim[sizeof cdnFinal];
  int ok = p->arquivo ? debrid_tb_link(p->usenet, p->item, p->file, fim, sizeof fim) :
                       cdnG.resolver(p->orig, p->cab, CDNLINK_PRAZO_S, fim, sizeof fim, NULL);
  pthread_mutex_lock(&cdnTrava);
  if (atomic_load(&cdnGer) == p->ger) {
    cdnOk = ok;
    if (ok) memcpy(cdnFinal, fim, strlen(fim) + 1); else cdnFinal[0] = 0;
    atomic_store(&cdnFeitoGer, p->ger);
  }
  pthread_mutex_unlock(&cdnTrava);
  free(p);
  return NULL;
}

// Idade do link do CDN em uso, em ms (0 sem link do CDN).
static Uint32 cdnIdade(void) { return cdn.usado ? SDL_GetTicks() - cdn.em : 0; }
// O link do CDN do player ja passou da validade (cdnlink_validade_ms): toda
// conexao nova dele (seek, retomar) arrisca o link vencido.
static int cdnVelho(void) {
  return cdn.usado && !cv.ativo && !mf.ativo && cdnG.validade_ms && cdnIdade() > cdnG.validade_ms();
}

// Abre urlAtual pelo CDN. 1 = cuidou da abertura (ja abriu com o link da
// verificacao, ou o fio esta resolvendo e cdnBombear abre); 0 = abrir o link
// do addon como sempre.
static int cdnAbrir(void) {
  char fim[sizeof cdnFinal];
  unsigned idade = 0;
  CdnPedido *p;
  pthread_t t;
  cdn.pend = 0; cdn.usado = 0; cdn.avisouVelho = 0;
  atomic_fetch_add(&cdnGer, 1);
  // So filme/episodio (reconPermitida = !canal) e so link http(s) de fora.
  if (!reconPermitida || (!cdn.arquivo &&
      (!cdnG.resolver || !cdnG.origem_ok || !cdnG.origem_ok(urlAtual)))) return 0;
  if (!cdn.arquivo && cdnG.recente && cdnG.recente(urlAtual, cabecalhos, CDNLINK_REUSO_MS, fim, sizeof fim, &idade)) {
    cdn.usado = 1; cdn.em = SDL_GetTicks() - idade;
    printf("[fonte-cdn] the player gets the CDN link the check resolved %u s ago\n", idade / 1000u);
    fflush(stdout);
    abrirHost(fim, cabecalhos);
    return 1;
  }
  p = calloc(1, sizeof *p);
  if (!p) return 0;
  snprintf(p->orig, sizeof p->orig, "%s", urlAtual);
  snprintf(p->cab, sizeof p->cab, "%s", cabecalhos);
  p->ger = atomic_load(&cdnGer);
  p->arquivo = cdn.arquivo; p->usenet = cdn.usenet; p->item = cdn.item; p->file = cdn.file;
  cdn.ger = p->ger; cdn.pend = 1; cdn.pediuEm = SDL_GetTicks();
  if (pthread_create(&t, NULL, fioCdn, p) != 0) { free(p); cdn.pend = 0; return 0; }
  pthread_detach(t);
  // O player anterior (seek com link velho, reconexao, outra fonte) nao segue
  // tocando enquanto o link novo e resolvido.
  if (hParar) hParar();
  return 1;
}

// Por quadro: a resolucao terminou (ou passou do prazo) -> abre.
static void cdnBombear(void) {
  char fim[sizeof cdnFinal];
  int ok;
  if (!cdn.pend) return;
  if (atomic_load(&cdnFeitoGer) == cdn.ger) {
    pthread_mutex_lock(&cdnTrava);
    ok = cdnOk;
    memcpy(fim, cdnFinal, sizeof fim);
    pthread_mutex_unlock(&cdnTrava);
    cdn.pend = 0;
    if (!ativo || parado || !hAbrir) return;
    if (ok) {
      cdn.usado = 1; cdn.em = SDL_GetTicks();
      abrirHost(fim, cabecalhos);
    } else abrirHost(urlAtual, cabecalhos);
    return;
  }
  if (SDL_GetTicks() - cdn.pediuEm > (CDNLINK_PRAZO_S + 3) * 1000u) {
    printf("[fonte-cdn] no answer in %d s: the player gets the add-on link\n", CDNLINK_PRAZO_S + 3);
    fflush(stdout);
    cdn.pend = 0;
    atomic_fetch_add(&cdnGer, 1);   // o fio atrasado descarta o que achar
    if (ativo && !parado && hAbrir) abrirHost(urlAtual, cabecalhos);
  }
}

static void abrirSessao(void);
// Reabre a fonte original em `alvo` com um link novo do CDN (o velho pode ter
// vencido). Para o app e um seek: video_pronto segue 1, a posicao fica no alvo
// e as faixas escolhidas voltam (o mesmo caminho do recarregar da reconexao).
static void cdnRenovar(double alvo, const char *porque) {
  printf("[fonte-cdn] CDN link is %u min old (limit %u min): %s at %.1f s reopens with a fresh link\n",
         (unsigned)(cdnIdade() / 60000u), cdnG.validade_ms ? cdnG.validade_ms() / 60000u : 0u, porque, alvo);
  fflush(stdout);
  reconAudio = audioAtual; reconLeg = legAtual; reconFaixasPend = 1;
  reconBuscarMs = alvo > 1.0 ? (int)(alvo * 1000.0) : -1;
  cv.reabrindo = 1; cv.posMantida = alvo; cv.reabriuEm = SDL_GetTicks(); cv.reabTocandoEm = 0;
  abrirSessao();
}

static int emTrailer = 0;   // ver video_tpk_trailer_marcar
static int emCartao = 0;    // ver video_tpk_cartao_marcar
// #471: o cover do cartao caiu no encaixe nesta sessao (plano maior que o
// painel, ou o host recusou o ROI). O furo da home segue isto.
static volatile int cartaoSemCover = 0;
// Reabre pelo servidor local em `alvo` (mkvfix.h): para o app e um seek, como
// a troca do link do CDN — posicao mantida, faixas devolvidas, capa por cima.
static void mfLigar(double alvo, const char *porque) {
  printf("[mkvfix] %s at %.1f s: reopening through the local fix\n", porque, alvo);
  fflush(stdout);
  mf.ativo = 1; mf.pend = 0;
  reconAudio = audioAtual; reconLeg = legAtual; reconFaixasPend = 1;
  reconBuscarMs = alvo > 1.0 ? (int)(alvo * 1000.0) : -1;
  cv.reabrindo = 1; cv.posMantida = alvo; cv.reabriuEm = SDL_GetTicks(); cv.reabTocandoEm = 0;
  abrirSessao();
}
// A sonda: so filme/episodio, so o arquivo original (nao o conversor) e so
// link http(s) de fora. Uma vez por fonte.
static int mfPode(void) {
  return reconPermitida && !cv.ativo && !emTrailer && !mf.sondou && hostUrl[0] &&
         (!strncmp(hostUrl, "http://", 7) || !strncmp(hostUrl, "https://", 8)) &&
         !strstr(hostUrl, "://127.0.0.1") && !strstr(hostUrl, "://localhost");
}
static void mfSondar(void) {
  if (!mfPode()) return;
  mf.sondou = 1;
  if (cdn.arquivo) mkvfix_sondar_arquivo(hostUrl, hostCab, cdn.usenet, cdn.item, cdn.file);
  else mkvfix_sondar(hostUrl, hostCab, strcmp(urlAtual, hostUrl) ? urlAtual : NULL);
}
// Arquivo com ASS/SSA no cabecalho ja lido (a pre-busca da legenda, sem rede):
// a TV nao pode receber o arquivo cru. A abertura espera a sonda (algumas
// centenas de ms) e o player ja nasce no servidor local; sem resposta em
// MF_ESPERA_MS, abre o link como sempre e a reabertura cedo cobre depois.
#define MF_ESPERA_MS 4000u
static int mkvTemAss(void) {
  for (int i = 0; i < mkvN; i++)
    if (mkvFx[i].tipo == 17 && (!strcmp(mkvFx[i].codec, "S_TEXT/ASS") || !strcmp(mkvFx[i].codec, "S_TEXT/SSA") ||
                                !strcmp(mkvFx[i].codec, "S_ASS") || !strcmp(mkvFx[i].codec, "S_SSA"))) return 1;
  return 0;
}
static int mfEsperarAntes(const char *url, const char *cab) {
  if (!mfPode() || mf.ativo || mkvfix_e_url(url)) return 0;
  // #648: o Tizen.Multimedia.Player so entrega User-Agent e Cookie na rede
  // (host: "cabecalho ignorado pelo player"). Uma fonte cujo addon exige
  // outro cabecalho (Referer, Origin, Authorization — medido: CDN responde
  // 403/timeout sem ele) nunca abre direto: repassa pelo servidor local do
  // mkvfix, cujo curl manda os cabecalhos em todo Range.
  int relay = cab && mkvfix_cab_fora_da_tv(cab);
  if (!relay && (mkvEstado != 3 || !mkvTemAss())) return 0;
  mf.sondou = 1; mf.esperando = 1; mf.esperaDesde = SDL_GetTicks();
  snprintf(mfEspUrl, sizeof mfEspUrl, "%s", url ? url : "");
  snprintf(mfEspCab, sizeof mfEspCab, "%s", cab ? cab : "");
  if (cdn.arquivo) mkvfix_sondar_arquivo(hostUrl, hostCab, cdn.usenet, cdn.item, cdn.file);
  else if (relay) mkvfix_sondar_relay(hostUrl, hostCab, strcmp(urlAtual, hostUrl) ? urlAtual : NULL);
  else mkvfix_sondar(hostUrl, hostCab, strcmp(urlAtual, hostUrl) ? urlAtual : NULL);
  printf(relay ? "[mkvfix] a fonte exige cabecalho que o player da TV nao envia: abrindo pelo servidor local\n"
              : "[mkvfix] the file has ASS/SSA subtitles: checking it before the TV opens it\n");
  fflush(stdout);
  // O player anterior nao segue tocando durante a sonda (como no cdnAbrir).
  if (hParar) hParar();
  return 1;
}
// A sessao atual toca pelo servidor com as legendas de texto escondidas da TV.
static int legOcultas(void) { return mf.ativo && mkvfix_ocultas() > 0; }

static void mfBombear(void) {
  int e;
  if (mf.esperando) {
    char u[256];
    e = mkvfix_estado();
    if (e == MKVFIX_SONDANDO && SDL_GetTicks() - mf.esperaDesde < MF_ESPERA_MS) return;
    mf.esperando = 0;
    if (!ativo || parado || !hAbrir) return;
    if ((mkvfix_ocultas() > 0 || mkvfix_relay()) && mkvfix_url(u, sizeof u)) {
      mf.ativo = 1;
      printf("[mkvfix] the TV opens the file through the local server (%s), %u ms\n",
             mkvfix_relay() ? "addon headers the TV cannot send" : "subtitle tracks hidden",
             (unsigned)(SDL_GetTicks() - mf.esperaDesde));
      fflush(stdout);
      abrirHost(u, "");
    } else {
      printf("[mkvfix] %s: opening the file directly\n",
             e == MKVFIX_SONDANDO ? "no answer from the check in time" : "nothing hidden");
      fflush(stdout);
      abrirHost(mfEspUrl, mfEspCab);
    }
    return;
  }
  if (pronto && !mf.sondou) mfSondar();
  if (!mf.sondou) return;
  e = mkvfix_estado();
  if (e == MKVFIX_SONDANDO) return;
  if (mf.pend) {
    mf.pend = 0;
    if (e == MKVFIX_PRECISA && !mf.ativo && !cv.ativo && ativo && !parado && SDL_GetTicks() - mf.pendEm < 20000u)
      mfLigar(mf.pendAlvo, "seek asked during the check (the TV can't seek this file as served)");
  }
  // Cabecalho sem ASS conhecido antes de abrir: a sonda de depois do pronto
  // achou legenda a esconder. Reabre cedo, uma vez, no ponto atual.
  if (mkvfix_ocultas() > 0 && !mf.ativo && !mf.reabriuLeg && !cv.ativo && ativo && !parado && pronto) {
    mf.reabriuLeg = 1;
    mfLigar(posReal(), "subtitle tracks hidden from the TV player (it hangs on some ASS drawings)");
    return;
  }
  // A TV recusou seek e nao ha o que consertar: diz uma vez, em vez do nada.
  if (atomic_load(&mfRecusas) > 0 && !mf.ativo && !mf.avisou && (e == MKVFIX_SEM_INDICE || e == MKVFIX_NORMAL)) {
    mf.avisou = 1;
    printf("[mkvfix] the TV refused a seek and the header has %s\n",
           e == MKVFIX_SEM_INDICE ? "no index to point to (nothing to fix)" : "its index listed (not the hidden-index case)");
    fflush(stdout);
    if (e == MKVFIX_SEM_INDICE) atomic_store(&mfAviso, 1);
  }
}
__attribute__((visibility("default")))
int video_tpk_aviso_busca(void) { return atomic_exchange(&mfAviso, 0); }

// Abre urlAtual no host. Serve a fonte nova e ao recarregar da reconexao.
static void abrirSessao(void) {
  // video_iniciar() NAO e chamada no .tpk (so video.c, o ramo da LG, a chama;
  // aqui o trailer usa video_iniciar_auto, que nao a chama), entao a flag do
  // texto simples ficava em 0 e toda faixa S_TEXT/UTF8 saia do mkvass como
  // "no-go: faixa nao e ASS" e voltava para a TV (2.0.1: 13 pessoas, 125 logs,
  // e nenhuma linha "e texto simples"). Ligada a cada sessao, antes de o app
  // escolher qualquer faixa.
  mkvass_aceitar_texto(1);
  mkvass_aceitar_imagem(1);   // PGS: o app decodifica e desenha (imgsub)
  atomic_store(&temErroDetalhe, 0);
  vigiaAtiva = 0; buscaAlvoMs = -1;
  pthread_mutex_lock(&amTrava); am.valido = 0; amBusca = 0; pthread_mutex_unlock(&amTrava);   // seq continua: o player compara
  atomic_store(&posIncerta, 0);
  atomic_store(&vigiaReiniciar, 1);
  ativo = 1; pronto = falhou = terminou = tocando = 0;
  largura = altura = durMs = 0; bufferando = 1; bufferDesde = SDL_GetTicks(); bufPct = 0;
  // Convertendo um MKV, as listas sao as do ARQUIVO (montarListaConvertida) e
  // nao mudam com o player: troca, seek do progressivo e reconexao abrem outro
  // player, mas a legenda escolhida (overlay do app) e o menu ficam. As
  // faixas_fim do player novo so trocam o audio (0 legendas = releitura).
  if (!(cv.ativo && mkvN > 0)) {
    nAudio = 0; audioAtual = 0;
    // Pelo servidor com as legendas escondidas: a lista e a do arquivo e fica.
    if (!(legOcultas() && mkvN > 0)) { nLeg = 0; legAtual = -1; }
  }
  audioNaoSup = faixasLidas = audioLogado = 0;
  comecou = 0; comecouEm = 0; audioComecou = 0; audioPend = legPend = -1; legReescrever = -1;
  legEscrito = -1; seekNearWrite = 0; legEsperaSeekEm = 0;
  atomic_store(&legSeekChegou, 0);
  if (!travaLeg) travaLeg = SDL_CreateMutex();
  legendaLimpar(1);
  legendaMarcarEnvio(-1);
  if (travaLeg) { SDL_LockMutex(travaLeg); legCueSessao = 0; legAbriuEm = SDL_GetTicks(); SDL_UnlockMutex(travaLeg); }
  emTrailer = emCartao = 0;
  cartaoSemCover = 0;
  planoW = planoH = 0;
  temRoi = 0;
  sessao++;
  velEnviada = 100;   // Player novo no host: nasce em 1x
  if (!hAbrir) { falhou = 1; printf("[video] tpk: host sem player\n"); return; }
  if (cv.ativo) {
    // O player le do servidor local; os cabecalhos do addon sao da fonte (o
    // servidor ja os usa), nao do 127.0.0.1.
    cv.abriuEm = SDL_GetTicks(); cv.tocou = 0;
    // The prebuffer goes as a pseudo header that the host turns into
    // player_set_streaming_buffering_time (never sent).
    { char pre[48] = ""; int ms = dts_tpk_prebuffer_ms();
      if (ms > 0) snprintf(pre, sizeof pre, "X-Nuvio-Prebuffer: %d", ms);
      hAbrir(cv.url, pre); }
  } else {
    char u[256];
    if (mf.ativo && mkvfix_url(u, sizeof u)) abrirHost(u, "");
    else {
      if (mf.ativo) {
        mf.ativo = 0;
        printf("[mkvfix] the local fix is gone: opening the file directly\n");
        fflush(stdout);
      }
      if (!cdnAbrir()) abrirHost(urlAtual, cabecalhos);
    }
  }
  // Depois do hAbrir: o host troca de sessao nele, e um aviso da sessao velha
  // emitido antes disso nao pode sobrar para matar a nova.
  atomic_store(&playerPreso, -2); atomic_store(&consultaLentaEm, 0);
  return;
}

static void guardarMkv(const MkvFaixa *fx, int n);
static int trilhaParaConverter(int tvLista);
static int trilhaPelaOrdem(void);
static int videoConvertivel(void);

int video_tocar(const char *u) {
  cdn.arquivo = 0;
  parado = 0; resgateAte = 0;
  if (cv.ativo || cv.sondando) dts_tpk_fechar();
  memset(&cv, 0, sizeof cv); cv.buscarMs = -1; naoSupPend = 0;
  // As listas do titulo anterior nao passam para o convertido (que as mantem
  // em abrirSessao); no caminho de sempre abrirSessao ja as zerava.
  nAudio = nLeg = 0; audioAtual = 0; legAtual = -1;
  snprintf(urlAtual, sizeof urlAtual, "%s", u ? u : "");
  memset(&mf, 0, sizeof mf); atomic_store(&mfRecusas, 0); atomic_store(&mfAviso, 0);
  hostUrl[0] = hostCab[0] = 0;
  mkvfix_parar();
  // player.c supplies !ehCanal(): live must not open side connections to
  // IPTV providers, which can evict the playing session at the next HLS window.
  reconPermitida = reconProxima; reconProxima = 0;
  if (reconPermitida) capmkv_iniciar(urlAtual);
  else capmkv_zerar();
  mkvGeracao++; mkvN = 0; faixasNovas = 0; mkvOlhou = 0; mkvSondarJa = 0; mkvNaoMkv = !reconPermitida;
  // Um fio da fonte anterior ainda na rede ve a geracao mudada e descarta.
  mkvEstado = urlAtual[0] && reconPermitida ? 1 : 0;
  pausaPedida = 0; reconPausa = -1;
  nv_recon_zerar(&recon);
  reconIniciou = 0; reconErroPend = 0; reconLinhaRede = 0;
  reconAudio = reconLeg = -1; reconFaixasPend = 0; reconBuscarMs = -1;
  // CABECALHO JA LIDO (pre-busca do mkvass, sem rede): um MKV so com DTS/TrueHD
  // ja abre convertido, sem o original mudo antes.
  // Lido sempre que a pre-busca trouxe o cabecalho: as legendas ASS decidem
  // a abertura pelo servidor local (mfEsperarAntes); o conversor, o resto.
  if (urlAtual[0] && reconPermitida && mkvass_cabecalho(urlAtual, NULL, NULL)) {
    unsigned char *cab = NULL; long cabN = 0;
    if (mkvass_cabecalho(urlAtual, &cab, &cabN)) {
      MkvFaixa *fx = calloc(MKV_MAX_FAIXAS, sizeof *fx);
      int n = fx ? mkv_faixas_do_trecho(cab, cabN, fx, MKV_MAX_FAIXAS, NULL, 0, NULL) : 0;
      if (n > 0) {
        printf("[mkv] sonda pelo trecho da pre-busca (%ld bytes, sem rede, antes de abrir): %d faixa(s)\n", cabN, n);
        guardarMkv(fx, n); mkvEstado = 3;
      }
      free(fx); free(cab);
      if (convPossivel()) { int t = mkvEstado == 3 ? trilhaParaConverter(-1) : 0;
        if (t > 0 && convIniciar(t, 0.0, i18n("cabecalho lido antes de abrir: so DTS/TrueHD"))) return 1;
        t = mkvEstado == 3 ? trilhaPelaOrdem() : 0;
        if (t > 0 && convIniciar(t, 0.0, "cabecalho lido antes de abrir: a ordem do audio escolhe uma faixa DTS/TrueHD")) return 1; }
    }
  }
  abrirSessao();
  return 1;
}

void video_definir_reconexao(int sim) { reconProxima = sim ? 1 : 0; }
int  video_recarregar(void) { return 0; }
int  video_reconectando(void) {
  return nv_recon_ativa(&recon) && (recon.pendente || !pronto) ? recon.tentativa : 0;
}

typedef struct { char url[sizeof urlAtual]; unsigned geracao; } PedidoMkv;

static void guardarMkv(const MkvFaixa *fx, int n) {
  int i;
  memcpy(mkvFx, fx, sizeof mkvFx);
  for (i = 0; i < n; i++)
    printf("[mkv] faixa num=%d tipo=%d codec=%s idioma=%s nome=%s forcada=%d canais=%d\n",
           fx[i].numero, fx[i].tipo, fx[i].codec, fx[i].idioma[0] ? fx[i].idioma : "-",
           fx[i].nome[0] ? fx[i].nome : "-", fx[i].forcado, fx[i].canais);
  fflush(stdout);
  mkvN = n;
  faixasNovas = 1;
}

static void *fioMkv(void *arg) {
  PedidoMkv *p = arg;
  MkvFaixa *fx = calloc(MKV_MAX_FAIXAS, sizeof *fx);
  int n;
  // #385: a sonda e leitura LATERAL ao video. Host que acabou de recusar
  // conexao (pre-busca da legenda, capitulos) nao recebe outra daqui.
  rede_lateral(1);
  n = fx ? mkv_faixas(p->url, fx, MKV_MAX_FAIXAS) : 0;
  if (p->geracao == mkvGeracao) {
    printf("[mkv] sonda pela rede: %d faixa(s)\n", n);
    if (n > 0) guardarMkv(fx, n);
    else mkvNaoMkv = 1;
    mkvEstado = 3;
  }
  free(fx); free(p);
  return NULL;
}

// Codec de audio do MKV que a Samsung nao toca (DTS em todas as variantes,
// TrueHD, MLP). Puro: o teste do Mac chama por video_tpk_codec_recusado.
int video_tpk_codec_recusado(const char *codecId) {
  return codecId && (!strncmp(codecId, "A_DTS", 5) || !strncmp(codecId, "A_TRUEHD", 8) ||
                     !strncmp(codecId, "A_MLP", 5));
}

// --- conversao DTS/TrueHD -------------------------------------------------------
// A conversao pode acontecer nesta fonte? So no Tizen 6+ (pacotes NuvioTpk60/65
// e NuvioTpk) com o FFmpeg na .so (dts_tpk_disponivel), ajuste de teste ligado, fonte
// por http(s) e sem desistencia nesta fonte. MKV decide pelo cabecalho; MP4 (e o
// que nao e MKV) pela sonda do conversor, ao lado do original (convSondar).
static int convPossivel(void) {
  if (!reconPermitida || cv.desistiu || emTrailer || !urlAtual[0]) return 0;
  if (strncmp(urlAtual, "http://", 7) && strncmp(urlAtual, "https://", 8)) return 0;
  if (proxyts_e_url(urlAtual) || dts_tpk_e_url(urlAtual)) return 0;
  if (ajustes_dts_tpk_modo() == DTSTPK_DESLIGADO || !videoConvertivel()) return 0;
  return dts_tpk_disponivel();
}

static int faixaRecusada(const MkvFaixa *m) { return m->tipo == 2 && video_tpk_codec_recusado(m->codec); }

// VIDEO QUE O CONVERSOR ACEITA: so HEVC e H.264 (dts_remux). VC-1 e MPEG-2 nao
// tocam pelo TS/HLS convertido na TV: esses arquivos ficam no original, como no
// master, sem a troca de player que so voltaria. Sem faixa de video lida
// (MP4, cabecalho ainda nao lido) quem decide e o conversor.
static int videoConvertivel(void) {
  for (int i = 0; i < mkvN; i++)
    if (mkvFx[i].tipo == 1)
      return !strcmp(mkvFx[i].codec, "V_MPEGH/ISO/HEVC") || !strcmp(mkvFx[i].codec, "V_MPEG4/ISO/AVC");
  return 1;
}

// A faixa (TrackNumber) a converter sem a pessoa pedir, ou 0. tvLista: quantas
// faixas de audio a TV listou (-1 = ainda nao se sabe). So quando NENHUMA faixa
// do arquivo toca na TV: arquivo misto segue no que ja toca (o menu oferece as
// outras). Entre as recusadas, a da ordem do audio, senao a primeira.
static int trilhaParaConverter(int tvLista) {
  int i, nArq = 0, nRec = 0, prim = 0, pre;
  if (tvLista > 0 || !videoConvertivel()) return 0;
  for (i = 0; i < mkvN; i++) {
    if (mkvFx[i].tipo != 2) continue;
    nArq++;
    if (!faixaRecusada(&mkvFx[i])) continue;
    nRec++;
    if (!prim) prim = mkvFx[i].numero;
  }
  if (!nRec || nRec != nArq) return 0;
  pre = trilhaPelaOrdem();   // a da ordem do audio (audioescolha.h), senao a primeira
  return pre ? pre : prim;
}

// A ORDEM DO AUDIO PELO CABECALHO, antes de a TV listar (abrir ja convertido,
// sem o original mudo antes): a faixa DTS/TrueHD que a ordem escolhe, ou 0
// (a escolhida a TV toca, ou nenhuma regra decide). A TV toca toda faixa de
// codec nao recusado.
static int trilhaPelaOrdem(void) {
  AudioCand c[MKV_MAX_FAIXAS];
  int num[MKV_MAX_FAIXAS], rec[MKV_MAX_FAIXAS];
  int i, n = 0, k, pode = convPossivel();
  if (!videoConvertivel()) return 0;
  for (i = 0; i < mkvN && n < MKV_MAX_FAIXAS; i++) {
    if (mkvFx[i].tipo != 2) continue;
    rec[n] = faixaRecusada(&mkvFx[i]); num[n] = mkvFx[i].numero;
    c[n].idioma = mkvFx[i].idioma; c[n].codec = mkvFx[i].codec; c[n].rotulo = mkvFx[i].nome;
    c[n].canais = mkvFx[i].canais;
    c[n].dts = rec[n] || audioesc_dts_truehd(mkvFx[i].codec, "");
    c[n].pode = !rec[n] || pode;
    c[n].numero = 0x40000000 | mkvFx[i].numero;   // same MKV identity as the converted menu
    n++;
  }
  k = audioesc_escolher(c, n, NULL);
  return k >= 0 && rec[k] ? num[k] : 0;
}

static const MkvFaixa *mkvTrack(int numero) {
  for (int i = 0; i < mkvN; i++) if (mkvFx[i].numero == numero) return &mkvFx[i];
  return NULL;
}

// "Dolby Digital 5.1" / "2.0" / "AAC 2.0": o que sai para a TV.
static const char *saidaRotulo(int canais) {
  if (!ajustes_dts_ac3()) return "AAC 2.0";
  return canais == 1 ? "Dolby Digital 1.0" : canais == 2 ? "Dolby Digital 2.0" : "Dolby Digital 5.1";
}

// rotulo + " → " + saida, cortando o rotulo (sem partir UTF-8) para caber.
static void rotuloConvertido(char *rot, size_t n, int canais) {
  char suf[40];
  size_t k, max;
  if (strstr(rot, "\xe2\x86\x92")) return;
  snprintf(suf, sizeof suf, " \xe2\x86\x92 %s", saidaRotulo(canais));
  max = n - 1 - strlen(suf);
  k = strlen(rot);
  if (k > max) {
    k = max;
    while (k > 0 && ((unsigned char)rot[k] & 0xC0) == 0x80) k--;
    rot[k] = 0;
  }
  strncat(rot, suf, n - strlen(rot) - 1);
}

// Legenda de imagem (PGS, VobSub, DVB): o player da TV nao lista. PGS o app
// decodifica e desenha; VobSub e DVB ficam fora da lista convertida, como no
// original.
static int legendaImagemMkv(const MkvFaixa *m) {
  return !strncmp(m->codec, "S_HDMV/", 7) || !strcmp(m->codec, "S_VOBSUB") || !strcmp(m->codec, "S_DVBSUB");
}

// As faixas de audio e as legendas de TEXTO do arquivo, na ordem das TrackEntry,
// com os rotulos (e o codec e o ordinal do mkvass, nas legendas) de faixasmkv.
// Audio: numero = -1, nada disso e faixa do player. Legenda: numero = posicao
// entre as de texto, como o player numera o mesmo arquivo no original: a
// legenda lembrada deste titulo (legmem) e o id da folha (legendasui) casam
// nos dois modos, e cada entrada tem id proprio.
static int listaDoArquivo(VideoFaixa *a, int *na, VideoFaixa *l, int *nl) {
  int i;
  *na = *nl = 0;
  for (i = 0; i < mkvN; i++) {
    VideoFaixa *f = NULL;
    if (mkvFx[i].tipo == 2 && *na < MAX_FAIXAS) f = &a[(*na)++];
    else if (mkvFx[i].tipo == 17 && (!legendaImagemMkv(&mkvFx[i]) || faixasmkv_imagem_overlay(mkvFx[i].codec)) &&
             *nl < MAX_FAIXAS) f = &l[(*nl)++];
    if (!f) continue;
    memset(f, 0, sizeof *f);
    f->numero = mkvFx[i].tipo == 17 ? *nl - 1 : -1;
    f->ordinalMkv = -1; f->stream_index = -1; f->stream_id = mkvFx[i].numero;
    snprintf(f->idioma, sizeof f->idioma, "%s", mkvFx[i].idioma);
  }
  faixasmkv_aplicar(a, *na, l, *nl, mkvFx, mkvN);
  for (i = 0; i < *na; i++) {
    const MkvFaixa *m = mkvTrack(a[i].stream_id);
    if (m && faixaRecusada(m)) rotuloConvertido(a[i].rotulo, sizeof a[i].rotulo, m->canais);
  }
  return *na + *nl;
}

// Convertendo: o menu mostra o arquivo inteiro (a do player e so a AC3). A
// legenda escolhida segue a mesma faixa do arquivo (pelo ordinal do mkvass),
// venha a lista anterior do player ou desta mesma funcao.
static void montarListaConvertida(void) {
  static VideoFaixa a[MAX_FAIXAS], l[MAX_FAIXAS];
  int na, nl, i, sel = 0, ordLeg = legAtual >= 0 && legAtual < nLeg ? faixaLeg[legAtual].ordinalMkv : -1;
  listaDoArquivo(a, &na, l, &nl);
  for (i = 0; i < na; i++) if (a[i].stream_id == cv.track) sel = i;
  memcpy(faixaAudio, a, sizeof faixaAudio); memcpy(faixaLeg, l, sizeof faixaLeg);
  audioAtual = sel; nAudio = na; nLeg = nl;
  if (legAtual >= nLeg) legAtual = -1;
  if (ordLeg >= 0) for (i = 0; i < nl; i++) if (l[i].ordinalMkv == ordLeg) legAtual = i;
  printf("[dts-tpk] menu: %d audio e %d legenda(s) de texto do arquivo; tocando faixa %d (%s)%s\n", na, nl, cv.track,
         na ? faixaAudio[sel].rotulo : "-", legAtual >= 0 ? i18n(", legenda escolhida mantida") : "");
  fflush(stdout);
}

// Legendas escondidas da TV (mkvfix.h): a TV nao lista as de texto, entao o
// menu de legendas e o do ARQUIVO, como no convertido. Os numeros seguem os do
// original (legmem e a folha casam); nada disso e escrito no player
// (video_escolher_legenda). A escolhida fica, pelo ordinal do mkvass.
static void legendasDoArquivo(void) {
  static VideoFaixa a[MAX_FAIXAS], l[MAX_FAIXAS];
  int na, nl, i, ordLeg = legAtual >= 0 && legAtual < nLeg ? faixaLeg[legAtual].ordinalMkv : -1;
  listaDoArquivo(a, &na, l, &nl);
  memcpy(faixaLeg, l, sizeof faixaLeg);
  nLeg = nl;
  legAtual = -1;
  if (ordLeg >= 0) for (i = 0; i < nl; i++) if (l[i].ordinalMkv == ordLeg) legAtual = i;
  printf("[mkvfix] subtitle menu: %d subtitle(s) from the file (hidden from the TV, drawn by the app)%s\n",
         nl, legAtual >= 0 ? i18n(", legenda escolhida mantida") : "");
  fflush(stdout);
}

// Original tocando: as faixas DTS/TrueHD do arquivo entram no FIM da lista do
// player, para a pessoa poder escolher uma (converte na hora).
static void acrescentarRecusadas(void) {
  static VideoFaixa a[MAX_FAIXAS], l[MAX_FAIXAS];
  int na, nl, i, n = 0;
  if (cv.recusadas) return;
  cv.recusadas = 1;            // uma vez por lista do player (e nao a cada quadro)
  if (!convPossivel()) return;
  listaDoArquivo(a, &na, l, &nl);
  for (i = 0; i < na && nAudio < MAX_FAIXAS; i++) {
    const MkvFaixa *m = mkvTrack(a[i].stream_id);
    if (!m || !faixaRecusada(m)) continue;
    faixaAudio[nAudio++] = a[i];
    n++;
  }
  if (n) {
    printf("[dts-tpk] menu: %d faixa(s) DTS/TrueHD do arquivo acrescentadas ao audio da TV (convertem se escolhidas)\n", n);
    fflush(stdout);
  }
}

static const char *nomeModo(int m) { return dts_tpk_modo_nome(m); }

// Posicao da faixa entre as de AUDIO do arquivo (0, 1, ...): e assim que o
// conversor a acha (o FFmpeg nao expoe o TrackNumber do Matroska).
static int ordinalAudio(int numero) {
  int i, k = 0;
  for (i = 0; i < mkvN; i++) {
    if (mkvFx[i].tipo != 2) continue;
    if (mkvFx[i].numero == numero) return k;
    k++;
  }
  return -1;
}

// Abre a sessao do conversor (a sonda roda num fio). 0: servidor/FFmpeg indisponivel.
static int convAbrirSessao(int ordinal, int modo, double pos, char *url, size_t n) {
  dts_remux_set_audio_offset_ms(ajustes_audio_offset_ms());
  if (dts_tpk_abrir(urlAtual, cabecalhos, ordinal, !ajustes_dts_ac3(), modo, pos, url, n))
    return 1;
  printf("[dts-tpk] conversao nao comecou (servidor/FFmpeg indisponivel); fica o original\n");
  fflush(stdout);
  cv.desistiu = 1;
  return 0;
}

static int modoProg(int modo) { return modo == DTSTPK_PROG_TS; }

// Outro player no meio de uma fonte que ja tocou: o app ve um seek ate o
// convertido mostrar quadro de novo (convBombear solta).
static void convReabrir(double alvo, const char *porque) {
  if (!cv.fonteTocou) return;
  cv.reabrindo = 1; cv.posMantida = alvo; cv.reabriuEm = SDL_GetTicks();
  printf("[dts-tpk] reabrindo o convertido em %.1f s (%s): para o app e um seek\n", alvo, porque);
  fflush(stdout);
}

// O player passa a ler o fluxo convertido `url`, que comeca em `pos` da fonte.
static void convTrocar(int track, int modo, double pos, const char *url) {
  // Erro ainda nao tratado neste instante e do player que sai (o original que
  // recusou o audio, o Prepare dele que falhou): nao derruba o convertido.
  if (reconErroPend) {
    printf("[dts-tpk] player error 0x%x of the original, pending at the switch: dropped\n", (unsigned)reconErroCod);
    fflush(stdout);
    reconErroPend = 0; reconLinhaRede = 0;
  }
  cv.ativo = 1; cv.sondando = 0; cv.track = track; cv.modo = modo; cv.inicio = pos;
  cv.prog = modoProg(modo);
  snprintf(cv.url, sizeof cv.url, "%s", url);
  cv.desloc = 0; cv.deslocOk = !cv.prog; cv.tocou = cv.jaTocou = 0;
  cv.buscarMs = !cv.prog && pos > 1.0 ? (int)(pos * 1000.0) : -1;
  cv.seekPend = 0; cv.voltar = 0;
  cv.posConhecida = pos;
  convReabrir(pos, i18n("troca para o convertido"));
  // A lista do arquivo ja, antes do player novo responder: a legenda escolhida
  // no original (texto, desenhado pelo overlay do app lendo a fonte) segue na
  // mesma faixa, e abrirSessao nao a zera (ver la).
  if (mkvN > 0) montarListaConvertida();
  faixasNovas = 1;
  abrirSessao();
}

static int convIniciar(int track, double pos, const char *motivo) {
  char url[256];
  int modo = ajustes_dts_tpk_modo();
  const MkvFaixa *m = mkvTrack(track);
  // Parado (Voltar, cartao de erro): a lista de faixas do arquivo que parou
  // nao reabre nada; so video_tocar comeca outra fonte.
  if (parado) return 0;
  if (!convAbrirSessao(ordinalAudio(track), modo, pos, url, sizeof url)) return 0;
  printf("[dts-tpk] convertendo a faixa %d do MKV (audio %d; %s %d canais%s%s) a partir de %.1f s, modo %s, saida %s; motivo: %s\n",
         track, ordinalAudio(track), m ? m->codec : "?", m ? m->canais : 0, m && m->idioma[0] ? ", " : "", m ? m->idioma : "", pos,
         nomeModo(modo), saidaRotulo(m ? m->canais : 6), motivo);
  fflush(stdout);
  convTrocar(track, modo, pos, url);
  return 1;
}

// HLS precisa do indice de quadros-chave inteiro (a playlist sai dele). MKV sem
// Cues nao tem: o mesmo audio sai pelo TS continuo, que busca por tempo. Uma vez
// por fonte; o 404/503 da playlist que o player ja pediu nao conta contra ele.
static void convRefazerProg(void) {
  char url[256];
  double pos = cv.inicio;
  int track = cv.track;
  printf("[dts-tpk] HLS nao serve para esta fonte (%s): refazendo no modo %s a partir de %.1f s\n",
         dts_tpk_erro(), nomeModo(DTSTPK_PROG_TS), pos);
  fflush(stdout);
  dts_tpk_fechar();
  cv.refezProg = 1;
  if (!convAbrirSessao(track > 0 ? ordinalAudio(track) : -1, DTSTPK_PROG_TS, pos, url, sizeof url)) {
    cv.ativo = 0; cv.prog = 0; cv.deslocOk = 1;
    cv.buscarMs = pos > 1.0 ? (int)(pos * 1000.0) : -1;
    abrirSessao();
    return;
  }
  reconErroPend = 0;
  convTrocar(track, DTSTPK_PROG_TS, pos, url);
}

// SEM CABECALHO DE FAIXAS (MP4): quem sabe se ha
// DTS/TrueHD e o proprio conversor. A sonda dele roda AO LADO do original, que
// segue tocando; a troca so acontece quando ela acha a faixa (convBombear). Nao
// achou, ou a TV listou audio: nada muda e nada recarrega.
// So MP4 anunciado e VOD (duracao conhecida): uma sonda lateral em canal ao vivo
// abriria outra conexao na fonte, e ha provedor que derruba a primeira.
static int semCabecalho(void) { return fonteMp4 && durMs > 0; }
static int faixasDaTv(void) {
  int i, n = 0;
  for (i = 0; i < nAudio; i++) if (faixaAudio[i].numero >= 0) n++;
  return n;
}
static void convSondar(int modo, const char *motivo) {
  if (parado || cv.ativo || cv.sondando || !convPossivel() || strstr(urlAtual, ".m3u8")) return;
  if (!convAbrirSessao(-1, modo, video_pos(), cv.url, sizeof cv.url)) return;
  cv.sondando = 1; cv.modo = modo;
  printf("[dts-tpk] %s, %s: o conversor procura DTS/TrueHD na fonte (modo %s); o original segue tocando\n",
         semCabecalho() ? "MP4" : "MKV", motivo, nomeModo(modo));
  fflush(stdout);
}

// Falha do convertido antes de tocar: volta ao original, na mesma posicao, e o
// aviso de sempre ("esta TV nao toca o audio") aparece (regra 4 do plano).
static void convDesistir(const char *motivo) {
  // Antes do primeiro quadro o player ainda nao tem posicao: vale onde o
  // convertido abriu (ou o seek pedido no meio).
  double pos = cv.jaTocou ? video_pos() : cv.seekPend ? cv.seekAlvo : cv.inicio;
  printf("[dts-tpk] desisti da conversao: %s; voltando a fonte original (sem som) em %.1f s\n", motivo, pos);
  fflush(stdout);
  dts_tpk_fechar();
  cv.ativo = 0; cv.desistiu = 1; cv.prog = 0; cv.deslocOk = 1;
  cv.buscarMs = pos > 1.0 ? (int)(pos * 1000.0) : -1;
  convReabrir(pos, i18n("volta ao original"));
  abrirSessao();
}

// A pessoa escolheu uma faixa que a TV toca: volta ao original na posicao e,
// quando as faixas chegarem, escolhe a de mesmo rotulo (cv.rotuloVolta).
static void convVoltar(void) {
  double pos = video_pos();
  printf("[dts-tpk] voltando ao audio da TV (%s) em %.1f s\n", cv.rotuloVolta, pos);
  fflush(stdout);
  dts_tpk_fechar();
  cv.ativo = 0; cv.prog = 0; cv.deslocOk = 1; cv.voltar = 1;
  cv.buscarMs = pos > 1.0 ? (int)(pos * 1000.0) : -1;
  convReabrir(pos, i18n("volta ao audio da TV"));
  abrirSessao();
}

// Cruza o arquivo com o que a TV listou. Roda a cada tick ate decidir, e loga
// uma vez por sessao quando ha codec recusado no arquivo.
static void avaliarAudioTv(void) {
  int i, nArq = 0, nRec = 0, nTv = 0;
  char lista[128] = "", recusados[48] = "";
  if (audioLogado || !faixasLidas || mkvN <= 0 || cv.ativo) return;
  // So as faixas do PLAYER contam (as DTS/TrueHD que o menu acrescenta, nao).
  for (i = 0; i < nAudio; i++) if (faixaAudio[i].numero >= 0) nTv++;
  for (i = 0; i < mkvN; i++) {
    const char *nome;
    if (mkvFx[i].tipo != 2) continue;
    nArq++;
    nome = faixasmkv_codec(mkvFx[i].codec);
    if (!nome[0]) nome = mkvFx[i].codec;
    if (strlen(lista) + strlen(nome) + 2 < sizeof lista) {
      if (lista[0]) strcat(lista, ",");
      strcat(lista, nome);
    }
    if (video_tpk_codec_recusado(mkvFx[i].codec)) {
      nRec++;
      if (!strstr(recusados, nome) && strlen(recusados) + strlen(nome) + 2 < sizeof recusados) {
        if (recusados[0]) strcat(recusados, "/");
        strcat(recusados, nome);
      }
    }
  }
  audioLogado = 1;
  if (!nRec) return;
  printf("[audio] TV recusou %s: faixas da TV=%d, no arquivo=%d [%s]%s\n", recusados, nTv, nArq, lista,
         nRec == nArq && nTv == 0 ? " -> sem audio tocavel" : "");
  fflush(stdout);
  if (nRec == nArq && nTv == 0) {
    int t = convPossivel() ? trilhaParaConverter(0) : 0;
    if (t > 0 && convIniciar(t, video_pos(), i18n("a TV nao listou audio e o arquivo so tem DTS/TrueHD"))) return;
    if (!cv.offLogado && ajustes_dts_tpk_modo() == DTSTPK_DESLIGADO) {
      cv.offLogado = 1;
      printf("[dts-tpk] teste Desligado: tocando o original (sem som), como o master\n");
      fflush(stdout);
    }
    audioNaoSup = 1;
  }
}

// Fio do app. Procura o cabecalho e, quando ha faixas novas, reescreve os
// rotulos. Sem mutex, como o resto deste arquivo: o rotulo sai de uma vez so.
static void sondaMkv(double pos) {
  Uint32 t = SDL_GetTicks();
  // Player fechado: nada a casar nem a escolher (video_bombear roda em todo
  // quadro, tambem depois do Voltar). O proximo titulo abre com video_tocar.
  if (!ativo) return;
  if (mkvEstado == 1 && urlAtual[0] && t - mkvOlhou >= 500) {
    unsigned char *cab = NULL; long cabN = 0;
    mkvOlhou = t;
    if (mkvass_cabecalho(urlAtual, NULL, NULL) && mkvass_cabecalho(urlAtual, &cab, &cabN)) {
      MkvFaixa *fx = calloc(MKV_MAX_FAIXAS, sizeof *fx);
      int n = fx ? mkv_faixas_do_trecho(cab, cabN, fx, MKV_MAX_FAIXAS, NULL, 0, NULL) : 0;
      printf("[mkv] sonda pelo trecho da pre-busca (%ld bytes, sem rede): %d faixa(s)\n", cabN, n);
      fflush(stdout);
      if (n > 0) { guardarMkv(fx, n); mkvEstado = 3; }
      free(fx); free(cab);
    }
    // A TV nao listou audio (ou acusou o codec) e a conversao pode entrar: o
    // cabecalho e preciso ja, nao daqui a 5 s de video mudo.
    // O resgate (NotSupportedAudioCodec) nao espera o original preparar: ele
    // pode nem preparar.
    if (mkvEstado == 1 && (pronto || naoSupPend) && !mkvSondarJa && convPossivel() &&
        ((faixasLidas && nAudio == 0) || naoSupPend)) mkvSondarJa = 1;
    // Sem pre-busca (ou Tracks fora do trecho): Range proprio, com o video
    // andando. MP4 nao tem TrackEntry: nao vale a descida.
    if (mkvEstado == 1 && (pronto || (naoSupPend && mkvSondarJa)) && (pos >= 5.0 || mkvSondarJa)) {
      PedidoMkv *p = fonteMp4 ? NULL : malloc(sizeof *p);
      pthread_t fio;
      mkvEstado = 3;
      if (p) {
        snprintf(p->url, sizeof p->url, "%s", urlAtual);
        p->geracao = mkvGeracao;
        mkvEstado = 2;
        if (pthread_create(&fio, NULL, fioMkv, p) == 0) pthread_detach(fio);
        else { free(p); mkvEstado = 3; }
      }
    }
  }
  if (cv.ativo) {
    // Convertido: a lista e a do arquivo, montada quando o player responde.
    if (faixasNovas && mkvN > 0 && faixasLidas) { faixasNovas = 0; montarListaConvertida(); }
    return;
  }
  if (faixasNovas && mkvN > 0 && (nAudio || nLeg || faixasmkv_tem_imagem(mkvFx, mkvN))) {
    int m;
    faixasNovas = 0;
    m = faixasmkv_aplicar(faixaAudio, nAudio, faixaLeg, nLeg, mkvFx, mkvN);
    // PGS: o player nao lista; entra no fim da lista, com o ordinal do arquivo.
    nLeg = faixasmkv_acrescentar_imagem(faixaLeg, nLeg, MAX_FAIXAS, mkvFx, mkvN);
    if (legOcultas()) legendasDoArquivo();
    printf("[mkv] %d rotulo(s) de faixa vindos do cabecalho\n", m);
    fflush(stdout);
    // Voltando do convertido: a faixa que a pessoa escolheu, pelo rotulo.
    if (cv.voltar) {
      int i;
      cv.voltar = 0;
      for (i = 0; i < nAudio; i++)
        if (!strcmp(faixaAudio[i].rotulo, cv.rotuloVolta)) { video_escolher_audio(i); break; }
      acrescentarRecusadas();
    } else {
      acrescentarRecusadas();
      // Rotulos e idiomas do cabecalho e as DTS/TrueHD no menu: a ordem do audio.
      if (!reconFaixasPend) escolherAudioPreferido();
    }
  } else if (mkvN > 0 && faixasLidas && !cv.recusadas && nAudio > 0) {
    acrescentarRecusadas();
    if (!reconFaixasPend) escolherAudioPreferido();
  }
  avaliarAudioTv();
}

// A deferred choice leaves once playback has really started. AUDIO leaves on the
// first tick, measured to be enough. SUBTITLE waits longer: at 0.00 s the write
// was swallowed even while Playing (the player's bookkeeping said "already on 2"
// while the demuxer kept track 0).
//
// The subtitle window used to count from EV_TOCANDO, which the host emits right
// after Start() with the buffer still filling - `tocando` does not prove a
// decoded frame exists. The 2500 ms expired during buffering and the write landed
// on nothing. Measured over five sessions on the TV, the write took effect only
// when it fell after the first frame (see the commit message for the table); the
// one session where it worked was the one whose buffer was slow.
#define LEG_ACOMODAR_MS 2500u
// The frame is proven by the position passing 0.25 s - the same signal the hole
// of #188 uses (player.c). It is the only "there is a picture" evidence Tizen
// exposes through .NET. Deliberately no ceiling: the one slow-buffer session that
// worked took 4519 ms, so a timeout would fire mid-buffer and restore the bug.
#define LEG_QUADRO_S   0.25
// Progressivo: o relogio cru do player (video_pos ainda soma o deslocamento).
static int temQuadro(void) {
  double p = cv.ativo && cv.prog ? (hPos && pronto ? hPos() / 1000.0 : 0) : posReal();
  return pronto && p >= LEG_QUADRO_S;
}
static void escolhasPendentes(void) {
  Uint32 agora = SDL_GetTicks();
  // Audio keeps its own state: it leaves on the first tick and never had this
  // defect, so it must not be made to wait for the frame.
  if (!audioComecou && (tocando || temQuadro())) audioComecou = 1;
  if (audioComecou && audioPend >= 0) {
    int i = audioPend; audioPend = -1;
    printf("[video] tpk: deferred audio dispatched track=%d\n", i);
    fflush(stdout);
    if (hEscolher) hEscolher(0, faixaAudio[i].numero);
  }
  // The subtitle window counts from this, the first frame.
  if (!comecou && temQuadro()) {
    comecou = 1; comecouEm = agora;
    // The position is the frame evidence and this logs it: on a RESUMED episode
    // the seek target can read >= LEG_QUADRO_S before any frame is decoded, and
    // the log would show "first frame" at 2730.12s one tick into the session.
    printf("[video] tpk: first frame at %.2fs\n", video_pos());
    if (cv.ativo && !cv.tocou)
      printf("[dts-tpk] primeiro quadro do convertido %u ms depois de abrir (modo %s, %s)\n",
             (unsigned)(agora - cv.abriuEm), nomeModo(cv.modo),
             !strncmp(cv.url, "http://127.0.0.1:", 17) ? "127.0.0.1" : "IP da rede");
    if (cv.ativo) cv.tocou = cv.jaTocou = 1;
    fflush(stdout);
  }
  if (!comecou) return;
  // Hold writes while a seek is in flight; timeout if the host never confirms.
  if (atomic_load(&buscaAlvoMs) >= 0) {
    if (!legEsperaSeekEm) legEsperaSeekEm = agora ? agora : 1u;
    if (agora - legEsperaSeekEm < LEG_ESPERA_SEEK_MS) return;
    atomic_store(&buscaAlvoMs, -1);
    printf("[video] tpk: seek did not confirm in %u ms; choosing the subtitle anyway\n",
           LEG_ESPERA_SEEK_MS);
    fflush(stdout);
  }
  legEsperaSeekEm = 0;
  // Seek landed: restart the settle clock and repeat an earlier write if it collided.
  if (atomic_exchange(&legSeekChegou, 0)) {
    comecouEm = agora;
    if (seekNearWrite && legAtual >= 0 && legEscrito == legAtual && legPend < 0)
      legReescrever = legAtual;
    seekNearWrite = 0;
  }
  if (legPend >= 0 && agora - comecouEm >= LEG_ACOMODAR_MS) {
    int i = legPend; legPend = -1; legReescrever = -1;
    printf("[video] tpk: settled subtitle dispatched track=%d (frame at %u ms)\n",
           i, (unsigned)(agora - comecouEm));
    fflush(stdout);
    legendaEnviar(i);
    return;
  }
  // Once, and only for the track that is still the current one.
  if (legReescrever >= 0 && agora - comecouEm >= LEG_ACOMODAR_MS) {
    int i = legReescrever; legReescrever = -1;
    if (legAtual >= 0 && i == legAtual) {
      printf("[video] tpk: subtitle track=%d written again after the seek\n", i);
      fflush(stdout);
      legendaEnviar(i);
    }
  }
}

static void recuperarPlayer(int alvoMs) {
  if (reconErroPend) return; // ja ha recuperacao/erro neste quadro: um hParar so
  reconAlvoMs = alvoMs >= 0 ? alvoMs : buscaAlvoMs;
  reconPausa = pausaPedida;
  reconErroCod = -2; reconErroPend = 1; reconLinhaRede = 1; reconIniciou = 1;
  tocando = 0; vigiaAtiva = 0;
  if (hParar) hParar(); // host confirma/libera antes de aceitar outra abertura
}

static void vigiarPlayer(double pos) {
  Uint32 agora = SDL_GetTicks(), lentaEm = atomic_load(&consultaLentaEm);
  int lenta = lentaEm && agora - lentaEm < TPK_LENTA_VALE_MS;
  if (atomic_exchange(&vigiaReiniciar, 0)) vigiaAtiva = 0;
  if (!ativo || !reconPermitida || !pronto || ((!tocando || pausaPedida || bufferando) && !lenta) || terminou ||
      falhou || recon.pendente || reconBuscarMs >= 0) { vigiaAtiva = 0; return; }
  if (!vigiaAtiva || pos != progressoPos) {
    if (vigiaAtiva && pos != progressoPos) atomic_store(&consultaLentaEm, 0);
    vigiaAtiva = 1; progressoPos = pos; progressoEm = agora; return;
  }
  if (agora - progressoEm < TPK_PRESO_MS) return;
  printf("[video] tpk preso: relogio %.3fs sem andar ha %u ms; parar e recuperar\n",
         pos, (unsigned)(agora - progressoEm));
  fflush(stdout);
  // Mesma maquina limitada da reconexao: nao cria um loop paralelo infinito.
  recuperarPlayer(-1);
}

// O convertido em andamento: sonda que falhou, prepare que nao vem, erro antes
// do primeiro quadro, seek depois de abrir, deslocamento do progressivo e o
// tempo de cada seek. Fio do app.
static void convBombear(void) {
  Uint32 agora = SDL_GetTicks();
  if (naoSupPend && cv.sondando) { naoSupPend = 0; cv.naoSup = 1; }   // a sonda em curso ja decide
  if (naoSupPend && !cv.ativo && (semCabecalho() || mkvEstado == 3 || mkvEstado == 0)) {
    // NotSupportedAudioCodec (log do host, outro fio): com o cabecalho lido,
    // converte ja; sem ele, a sonda foi antecipada em sondaMkv. Sem cabecalho
    // de faixas (MP4), a sonda do conversor decide.
    naoSupPend = 0;
    if (semCabecalho()) {
      cv.naoSup = 1;
      convSondar(ajustes_dts_tpk_modo(), "a TV acusou NotSupportedAudioCodec");
      if (!cv.sondando) audioNaoSup = 1;
    } else {
      int t = convPossivel() ? trilhaParaConverter(0) : 0;
      if (t > 0 && convIniciar(t, video_pos(), "a TV acusou NotSupportedAudioCodec")) return;
      if (t <= 0 && mkvN == 0 && convPossivel()) {
        // A sonda do cabecalho nao trouxe faixa nenhuma (host em pausa para
        // leitura lateral, rede): o conversor le o arquivo ele mesmo e decide.
        cv.naoSup = 1;
        convSondar(ajustes_dts_tpk_modo(), "cabecalho do MKV nao lido; a TV acusou NotSupportedAudioCodec");
        if (cv.sondando) return;
      }
      printf("[dts-tpk] rescue: nothing converted (track to convert %d, header tracks %d, converter %s)\n",
             t, mkvN, t > 0 ? "did not open" : convPossivel() ? "available" : "not possible");
      fflush(stdout);
      audioNaoSup = 1;
    }
  }
  if (!cv.ativo && !cv.sondando && semCabecalho() && pronto && faixasLidas && !faixasDaTv() && convPossivel())
    convSondar(ajustes_dts_tpk_modo(), i18n("a TV nao listou audio"));
  if (cv.sondando) {
    int e = dts_tpk_estado(), nTv = faixasDaTv();
    if (nTv > 0) {
      printf("[dts-tpk] a TV listou %d faixa(s) de audio: sem conversao\n", nTv);
      fflush(stdout);
      dts_tpk_fechar(); cv.sondando = 0; cv.desistiu = 1;
    } else if (e == DTSTPK_FALHOU && dts_tpk_sem_indice() && !cv.refezProg) {
      // MP4 fragmentado sem indice inteiro: a sonda de novo, no TS continuo.
      int modo = DTSTPK_PROG_TS;
      dts_tpk_fechar(); cv.sondando = 0; cv.refezProg = 1;
      convSondar(modo, i18n("sem indice de quadros-chave para o HLS"));
    } else if (e == DTSTPK_FALHOU) {
      printf("[dts-tpk] o conversor nao converte esta fonte (%s): fica o original\n", dts_tpk_erro());
      fflush(stdout);
      dts_tpk_fechar(); cv.sondando = 0; cv.desistiu = 1;
      if (cv.naoSup) audioNaoSup = 1;
    } else if (e == DTSTPK_PRONTO && ((pronto && faixasLidas && (int)(agora - faixasEm) >= 2500) || (cv.naoSup && resgateAte))) {
      // 2,5 s depois da lista: a releitura do audio do host (#165) ja veio.
      char url[256];
      double pos = video_pos();
      snprintf(url, sizeof url, "%s", cv.url);
      if (modoProg(cv.modo)) dts_tpk_url_em(pos, url, sizeof url);
      printf("[dts-tpk] convertendo o audio da fonte sem cabecalho MKV (%s) a partir de %.1f s, modo %s, saida %s; motivo: %s\n",
             dts_tpk_caminho(), pos, nomeModo(cv.modo), saidaRotulo(6),
             cv.naoSup ? "a TV acusou NotSupportedAudioCodec" : i18n("a TV nao listou audio"));
      fflush(stdout);
      convTrocar(0, cv.modo, pos, url);
    }
    return;
  }
  if (!cv.ativo) {
    // Voltando ao original (falha ou escolha): o seek para onde estava.
    if (cv.buscarMs >= 0 && pronto && tocando) {
      if (hBuscar) hBuscar(cv.buscarMs);
      cv.buscarMs = -1;
    }
    return;
  }
  if (!cv.jaTocou) {
    if (dts_tpk_estado() == DTSTPK_FALHOU && dts_tpk_sem_indice() && !cv.prog && !cv.refezProg) { convRefazerProg(); return; }
    if (dts_tpk_estado() == DTSTPK_FALHOU) { char m[200]; snprintf(m, sizeof m, i18n("a fonte nao abriu no conversor (%s)"), dts_tpk_erro()); convDesistir(m); return; }
    if (reconErroPend) {
      char m[64];
      snprintf(m, sizeof m, i18n("o player recusou o fluxo convertido (erro 0x%08x)"), (unsigned)reconErroCod);
      reconErroPend = 0;
      convDesistir(m);
      return;
    }
    // Com sinal: `agora` e lido no topo, e um convertido aberto neste mesmo
    // quadro (sonda, NotSupportedAudioCodec) tem abriuEm DEPOIS dele — sem
    // sinal a conta dava 4 bilhoes e desistia na hora (S95C, 16/10: "nao
    // preparou em 45 s" com 0 pedidos ao servidor).
    if (!pronto && (int)(agora - cv.abriuEm) > 45000) { convDesistir(i18n("o player nao preparou em 45 s")); return; }
  }
  // Troca no meio do filme (HLS): o player abriu do zero, e o seek vai depois
  // do Start, como no recarregar da reconexao.
  if (cv.buscarMs >= 0 && pronto && tocando) {
    if (hBuscar) hBuscar(cv.buscarMs);
    printf("[dts-tpk] seek para %.1f s depois de abrir o convertido\n", cv.buscarMs / 1000.0);
    fflush(stdout);
    cv.seekPend = 1; cv.seekAlvo = cv.buscarMs / 1000.0; cv.seekEm = agora;
    cv.buscarMs = -1;
  }
  // Progressivo: o player conta do comeco do fluxo ou da fonte? Decide uma vez
  // por abertura, com o quadro-chave onde o fluxo comeca (o servidor sabe).
  if (cv.prog && !cv.deslocOk && pronto && hPos) {
    double raw = hPos() / 1000.0, base = dts_tpk_inicio_stream();
    if (raw >= 0.3 && base >= 0) {
      const char *como;
      if (fabs(raw - base) < 3.0) { cv.desloc = 0; como = i18n("absoluta (tempo da fonte)"); }
      else if (fabs(raw - (base + 10.0)) < 3.0) { cv.desloc = -10.0; como = i18n("absoluta com os +10 s do fluxo"); }
      else if (raw < 3.0) { cv.desloc = base; como = i18n("relativa ao comeco do fluxo"); }
      else { cv.desloc = base - raw; como = "desconhecida (ajustada pelo quadro-chave)"; }
      cv.deslocOk = 1;
      printf("[dts-tpk] progressivo: o player informou %.2f s com o fluxo comecando em %.3f s: posicao %s, desloc %+.2f s\n",
             raw, base, como, cv.desloc);
      fflush(stdout);
    }
  }
  if (cv.seekPend && pronto && tocando && cv.deslocOk) {
    // O player devolve o alvo logo depois do seek, antes de ter quadro: conta
    // so quando a posicao ANDA a partir do primeiro valor perto do alvo.
    static Uint32 baseEm; static double base = -1;
    double p = posReal();
    if (baseEm != cv.seekEm) { baseEm = cv.seekEm; base = -1; }
    if (base < 0 && fabs(p - cv.seekAlvo) < 2.5 && p >= cv.seekAlvo - 0.5) base = p;
    if (base >= 0 && p >= base + 0.3) {
      printf("[dts-tpk] seek para %.1f s: tocando de novo em %u ms (posicao %.2f s, modo %s)\n", cv.seekAlvo,
             (unsigned)(agora - cv.seekEm), p, nomeModo(cv.modo));
      fflush(stdout);
      cv.seekPend = 0;
    } else if ((int)(agora - cv.seekEm) > 30000) {
      printf("[dts-tpk] seek para %.1f s: posicao %.2f s depois de 30 s\n", cv.seekAlvo, p);
      fflush(stdout);
      cv.seekPend = 0;
    }
  }
}

// Solta o "reabrindo" quando o player novo mostra quadro perto do alvo (ou anda
// 10 s em outro ponto: quadro-chave longe, seek que caiu ao lado). Falha, saida
// ou 60 s sem player: o app volta a ver o estado real.
static void reabrindoVigiar(void) {
  Uint32 agora = SDL_GetTicks();
  double p;
  if (!cv.reabrindo) return;
  if (!ativo || falhou || agora - cv.reabriuEm > 60000u) {
    if (ativo && !falhou) { printf("[dts-tpk] reabrindo: sem quadro em 60 s, o app ve o estado real\n"); fflush(stdout); }
    cv.reabrindo = 0; return;
  }
  if (!pronto || !tocando || cv.buscarMs >= 0 || reconBuscarMs >= 0 || recon.pendente ||
      (cv.ativo && cv.prog && !cv.deslocOk)) { cv.reabTocandoEm = 0; return; }
  if (!cv.reabTocandoEm) cv.reabTocandoEm = agora ? agora : 1u;   // "| 1" passava agora (par) e virava 4 G ms
  p = posReal();
  // Com sinal: no quadro em que foi marcado, `agora | 1` pode passar de agora
  // por 1 ms, e a conta sem sinal dava 4 bilhoes (soltava em posicao 0).
  if (p < 0.3 && (int)(agora - cv.reabTocandoEm) < 10000) return;
  if (fabs(p - cv.posMantida) > 20.0 && (int)(agora - cv.reabTocandoEm) < 10000) return;
  printf("%s reaberto em %u ms: posicao %.2f s (alvo %.1f s)\n", cv.ativo ? "[dts-tpk]" : "[fonte-cdn]",
         (unsigned)(agora - cv.reabriuEm), p, cv.posMantida);
  fflush(stdout);
  cv.reabrindo = 0;
}

void video_bombear(void) {
  // EOS wins over late polling errors and pending network retries.
  if (terminou) {
    nv_recon_zerar(&recon);
    reconErroPend = 0; reconBuscarMs = reconPausa = -1;
    atomic_store(&playerPreso, -2);
    return;
  }
  cdnBombear();
  mfBombear();
  convBombear();
  reabrindoVigiar();
  escolhasPendentes();
  legendaVigiarSilencio(SDL_GetTicks());
  // O SetPlaybackRate exige Ready/Playing/Paused: so com o prepare feito.
  if (!velRecusada && pronto && hEscolher && velEnviada != velPedida) {
    velEnviada = velPedida;
    hEscolher(3, velEnviada);
  }
  double pos = video_pos();
  int preso = atomic_exchange(&playerPreso, -2);
  // Ao vivo (sem reconexao) a tecla recusada so e ignorada, como antes do #412.
  if (preso >= -1 && ativo && reconPermitida && !recon.pendente && !falhou) recuperarPlayer(preso);
  vigiarPlayer(pos);
  sondaMkv(pos);
  if (pronto && pos > 0.5) reconIniciou = 1;
  if (pronto && !cv.reabrindo && posReal() > 0.5) cv.fonteTocou = 1;
  if (cv.ativo && pronto && !cv.reabrindo && (cv.deslocOk || !cv.prog) && posReal() > 0.5) cv.posConhecida = posReal();
  if (pronto && reconBuscarMs < 0) nv_recon_progresso(&recon, pos);
  // O seek do recarregar sai com o player ja tocando: o host da Start logo
  // depois do prepare, e um seek no meio disso concorre com ele.
  if (reconBuscarMs >= 0 && pronto && tocando) {
    int ms = reconBuscarMs;
    reconBuscarMs = -1;
    if (cv.ativo && cv.prog) {
      // Progressivo: o seek pendente reabre o fluxo no alvo (?t=); o TS nao tem
      // seek cru. Sem URL do servidor, fica no ponto atual.
      double alvo = ms / 1000.0;
      if (dts_tpk_url_em(alvo, cv.url, sizeof cv.url)) {
        cv.seekPend = 1; cv.seekAlvo = alvo; cv.seekEm = SDL_GetTicks(); cv.deslocOk = 0;
        convReabrir(alvo, "seek pendente");
        abrirSessao();
      }
    } else {
      if (hBuscar) hBuscar(ms);
      if (cv.ativo) { cv.seekPend = 1; cv.seekAlvo = ms / 1000.0; cv.seekEm = SDL_GetTicks(); }
    }
    vigiaAtiva = 0;
    printf("[video] reconexao: retomado em %ds\n", ms / 1000);
    fflush(stdout);
  }
  if (reconPausa >= 0 && pronto && tocando && reconBuscarMs < 0 && !reconErroPend && !recon.pendente) {
    if (hPausar) hPausar(reconPausa);
    reconPausa = -1;
  }
  if (resgateAte) {
    if (cv.ativo || parado || !ativo) resgateAte = 0;   // o conversor assumiu (ou saiu)
    else if (!cv.sondando && (!naoSupPend || (int)(SDL_GetTicks() - resgateAte) >= 0)) {
      // convBombear ja decidiu e nao converteu (nenhuma faixa para converter,
      // conversor recusou), ou o cabecalho nao chegou a tempo: falhou. A sonda
      // do conversor (MP4) em curso decide antes.
      resgateAte = 0;
      printf("[dts-tpk] the converter did not take over: the source failed (TV refused the audio; header %s, %d track(s), %s)\n",
             mkvEstado == 3 ? "read" : mkvEstado == 2 ? "still reading" : "not read", mkvN,
             naoSupPend ? "timed out waiting" : "nothing to convert");
      fflush(stdout);
      falhou = 1;
    }
  }
  if (!falhou && !parado && (!pronto || reconBuscarMs >= 0 || (!pausaPedida && bufferando)) &&
      nv_recon_expirou(&recon, SDL_GetTicks())) {
    reconErroCod = NV_RECON_TPK_CONNECTION_FAILED; reconErroPend = 1;
  }
  if (reconErroPend && !cv.ativo && !parado) {
    Uint32 agora = SDL_GetTicks();
    if (resgateAte) {
      printf("[dts-tpk] player error 0x%x while the converter takes over: ignored\n", (unsigned)reconErroCod);
      fflush(stdout);
      reconErroPend = 0; reconLinhaRede = 0;
    } else if ((unsigned)reconErroCod == TPK_ERRO_AUDIO && !reconIniciou && convPossivel()) {
      printf("[dts-tpk] the TV refused the audio codec: the converter takes over (not a failed source; %u s to start)\n",
             RESGATE_MS / 1000u);
      fflush(stdout);
      resgateAte = (agora + RESGATE_MS) | 1u;
      naoSupPend = 1;
      reconErroPend = 0; reconLinhaRede = 0;
    }
  }
  if (reconErroPend) {
    int antes = recon.tentativa;
    int rede = nv_recon_rede_tpk(reconErroCod, NULL) || reconLinhaRede;
    // Link do CDN e a fonte ja tocou: qualquer erro pode ser o link vencido. A
    // reconexao abre de novo pelo link do addon, que da um link novo do CDN.
    if (!rede && cdn.usado && !cv.ativo && reconIniciou && !recon.tentativa) {
      rede = 1;
      printf("[fonte-cdn] player error 0x%x after playing with a CDN link (%u min old): reconnecting with a fresh link\n",
             (unsigned)reconErroCod, (unsigned)(cdnIdade() / 60000u));
      fflush(stdout);
    }
    reconErroPend = 0; reconLinhaRede = 0;
    int alvoMs = reconAlvoMs;
    reconAlvoMs = -1;
    if (reconPermitida && urlAtual[0] && (reconIniciou || recon.tentativa) &&
        nv_recon_erro(&recon, rede, SDL_GetTicks(), alvoMs >= 0 ? alvoMs / 1000.0 : pos)) {
      if (alvoMs >= 0) recon.alvo = alvoMs / 1000.0;
      if (!antes) { reconAudio = audioAtual; reconLeg = legAtual; }
      if (recon.tentativa != antes) {
        printf("[video] conexao caiu (0x%x): tentativa %d/%d, espera %us\n",
               (unsigned)reconErroCod, recon.tentativa, NV_RECON_MAX,
               nv_recon_espera_ms(recon.tentativa) / 1000u);
        fflush(stdout);
      }
      bufferando = 0;
    } else {
      if (recon.esgotou) { printf("[video] reconexao: desistiu depois de %d tentativas\n", NV_RECON_MAX); fflush(stdout); }
      falhou = 1;
    }
  }
  if (nv_recon_vencida(&recon, SDL_GetTicks())) {
    printf("[video] reconectando: alvo %.0fs, tentativa %d\n", recon.alvo, recon.tentativa);
    fflush(stdout);
    reconFaixasPend = 1;
    reconBuscarMs = recon.alvo > 1.0 ? (int)(recon.alvo * 1000.0) : -1;
    if (cv.ativo) {
      double alvo = recon.alvo > 1.0 ? recon.alvo : cv.posConhecida;
      convReabrir(alvo, "reconexao");
      // Progressivo: o fluxo recomeca no alvo (?t=); nunca seek cru nele.
      if (cv.prog) {
        reconBuscarMs = -1; cv.deslocOk = 0;
        if (dts_tpk_url_em(alvo, cv.url, sizeof cv.url)) {
          cv.seekPend = 1; cv.seekAlvo = alvo; cv.seekEm = SDL_GetTicks();
        } else {
          printf("[dts-tpk] reconexao: o servidor nao deu o fluxo em %.1f s; reabrindo o fluxo atual\n", alvo);
          fflush(stdout);
        }
      }
    }
    abrirSessao();
  }
}
void video_parar(void) {
  largura = altura = 0; // Stopped stream metadata must not shape the next live open.
  parado = 1; resgateAte = 0;
  cdn.pend = 0; cdn.usado = 0; cdn.arquivo = 0;
  atomic_fetch_add(&cdnGer, 1);
  if (cv.ativo || cv.sondando) dts_tpk_fechar();
  memset(&cv, 0, sizeof cv); cv.buscarMs = -1; naoSupPend = 0;
  emTrailer = emCartao = 0;
  cartaoSemCover = 0;
  planoW = planoH = 0;
  temRoi = 0;
  memset(&mf, 0, sizeof mf);
  mkvfix_parar();
  capmkv_zerar();   // fio de capitulos em voo nao alimenta o proximo titulo
  audioPend = legPend = -1; comecou = 0; comecouEm = 0; audioComecou = 0; legReescrever = -1;
  legEscrito = -1; seekNearWrite = 0; legEsperaSeekEm = 0;
  atomic_store(&legSeekChegou, 0);
  legendaLimpar(1);
  nv_recon_zerar(&recon);
  reconErroPend = 0; reconFaixasPend = 0; reconBuscarMs = -1; reconPausa = -1;
  reconAlvoMs = buscaAlvoMs = -1;
  atomic_store(&playerPreso, -2); // aviso de antes da saida nao reabre o video
  if (ativo && hParar) hParar();
  ativo = pronto = tocando = 0;
  // AS FAIXAS SAEM COM O VIDEO (27/10). O memset do `cv` acima zerava
  // cv.recusadas e deixava mkvN/faixasLidas/nAudio do episodio fechado: o
  // proximo video_bombear refazia o casamento e a ordem do audio sobre a lista
  // velha ("[mkv] contagem" e "audio pela ordem ... fica" depois do Voltar).
  // A geracao nova faz um fioMkv ainda na rede descartar o que trouxer;
  // video_tocar zera o mesmo ao abrir o proximo.
  mkvGeracao++; mkvN = 0; mkvEstado = 0; faixasNovas = 0; faixasLidas = 0;
  nAudio = nLeg = 0; audioAtual = 0; legAtual = -1; nNovasA = nNovasL = 0;
}
void video_pausar(int p) {
  int estavaPausado = pausaPedida;
  pausaPedida = p != 0; vigiaAtiva = 0;
  // Retomar com o link do CDN vencido: abre de novo na posicao com link novo
  // (o player abre tocando). A pausa longa e o caso classico do link que morre.
  if (!p && estavaPausado && pronto && cdnVelho()) { cdnRenovar(posReal(), "resume"); return; }
  if (nv_recon_ativa(&recon) && (recon.pendente || reconErroPend || !pronto || reconBuscarMs >= 0)) reconPausa = pausaPedida; else if (hPausar) hPausar(p); }
int video_pausa_confirmada(void) { return 0; } // host nao fornece ack por sessao
void video_volume(int pct) { if (hVolume) hVolume(pct); }
void video_buscar(double s) {
  if (nv_recon_ativa(&recon)) {
    recon.alvo = s;
    if (recon.pendente || reconErroPend || !pronto || reconBuscarMs >= 0) { reconBuscarMs = (int)(s * 1000.0); return; }
  }
  progressoEm = SDL_GetTicks(); progressoPos = video_pos(); vigiaAtiva = 1;
  buscaAlvoMs = (int)(s * 1000.0);
  legEsperaSeekEm = 0;
  if (legEscrito >= 0 && travaLeg) {
    Uint32 envEm;
    SDL_LockMutex(travaLeg);
    envEm = legEnvEm;
    SDL_UnlockMutex(travaLeg);
    if (SDL_GetTicks() - envEm < LEG_ACOMODAR_MS * 2) seekNearWrite = 1;
  }
  if (cv.ativo) { cv.seekPend = 1; cv.seekAlvo = s; cv.seekEm = SDL_GetTicks(); }
  if (cv.ativo && cv.prog) {
    // Progressivo: o fluxo recomeca no quadro-chave antes de `s` (?t=).
    if (dts_tpk_url_em(s, cv.url, sizeof cv.url)) {
      printf("[dts-tpk] seek progressivo para %.1f s: fluxo novo\n", s);
      fflush(stdout);
      cv.deslocOk = 0;
      convReabrir(s, "seek");
      abrirSessao();
    } else {
      // Nunca um seek cru no fluxo progressivo (o player nao sabe buscar nele).
      printf("[dts-tpk] seek progressivo para %.1f s: o servidor nao deu o fluxo novo; fica onde esta\n", s);
      fflush(stdout);
      cv.seekPend = 0;
    }
    terminou = 0;
    return;
  }
  // Seek com o link do CDN vencido: o seek abriria uma conexao nova com ele.
  if (pronto && cdnVelho()) { cdnRenovar(s, "seek"); terminou = 0; return; }
  // MKV com o indice so no segundo SeekHead (mkvfix.h): a TV recusaria.
  if (!cv.ativo && !mf.ativo && pronto) {
    if (!mf.sondou) mfSondar();
    if (mf.sondou) {
      int e = mkvfix_estado();
      if (e == MKVFIX_PRECISA) { mfLigar(s, "seek (the TV can't seek this file as served)"); terminou = 0; return; }
      if (e == MKVFIX_SONDANDO) { mf.pend = 1; mf.pendAlvo = s; mf.pendEm = SDL_GetTicks(); }
    }
  }
  if (hBuscar) hBuscar((int)(s * 1000.0));
  terminou = 0;
}
void video_janela(int x, int y, int w, int h) {
  planoX=x; planoY=y; planoW=w; planoH=h;
  temRoi = 0;   // reposicionar invalida o recorte do destino anterior
  if (hJanela) hJanela(x, y, w, h);
}

// RECORTE DE FONTE EMULADO PELO RETANGULO DE DESTINO (ROI).
//
// O webOS recorta pela FONTE: o ACB aceita (sx,sy,sw,sh) do quadro decodificado
// mais um destino, e os modos de aspecto do player saem disso. O
// Tizen.Multimedia.Player (tizen-tpk/Video.cs) NAO tem retangulo de fonte — so
// DisplaySettings.SetRoi, que e o DESTINO na tela. A primeira versao disto
// descartava a fonte e aplicava so o destino, e o resultado era que TODO modo
// de aspecto desenhava o mesmo retangulo: na TV o botao de recorte/zoom nao
// mudava nada, em nenhum modo (#178).
//
// A conta que substitui: desenhar o recorte (sx,sy,sw,sh) dentro de
// (dx,dy,dw,dh) e o MESMO que desenhar o quadro INTEIRO num retangulo maior,
// deslocado para que o pedaco desejado caia sobre o destino.
//
//   escalaX = dw/sw            (quanto a fonte e ampliada na horizontal)
//   escalaY = dh/sh            (idem vertical)
//   W = qw * escalaX           (o quadro inteiro nessa escala)
//   H = qh * escalaY
//   X = dx - sx * escalaX      (recua a origem para o recorte cair em dx)
//   Y = dy - sy * escalaY
//
// O que sobra para fora da tela e o que o recorte descartaria. O ROI resultante
// pode ser MAIOR que a tela e ter origem NEGATIVA — e Video.cs.Janela deixa
// esse retangulo passar cru ao SetRoi (so cai em LetterBox no quadro cheio sem
// zoom).
//
// ROI FORA DA TELA NAO SAI MAIS (#188, #195). Desde este zoom (1.6.x), todo
// trailer do destaque manda um ROI assim (o zoom padrao do trailer e 1,34) e
// as S90C/S90D/QN90D (Tizen 9) mostram a tela inicial da Samsung no lugar do
// video, com o som tocando. A imagem do que esta atras do app aparecendo onde
// o furo nao tem video ja foi vista na S90D (#185). NAO PROVADO que o firmware
// apaga o plano com ROI fora do painel, mas e o que o webOS faz (player.c,
// aplicarAspecto: "retangulo fora do painel nao e recorte, e retangulo
// invalido: o plano apaga") e o zoom nunca foi visto funcionando numa Samsung.
// Por isso o padrao volta a ser o de antes do zoom: video_recorte_fonte() = 0
// (o player fica nos modos sem recorte, o trailer sem zoom) e, se alguem
// chamar isto mesmo assim, um ROI que sai da tela vira o destino cru.
// NV_TPK_ZOOM_ROI=1 liga o zoom de novo, so para canario.
#ifndef NV_TPK_ZOOM_ROI
#define NV_TPK_ZOOM_ROI 0
#endif
// #203: no 4/5 (NV_TPK40) o botao de aspecto do PLAYER ficava sem efeito: sem
// recorte o ciclo so oferece "Original". O host 4/5 aplica o destino movendo a
// janela do video (Video.cs JanelaTizen45), que nao e o ROI que apagava o plano
// no Tizen 9, entao o player libera os modos de recorte la. O trailer segue
// sem zoom (video_recorte_fonte_trailer) a nao ser que o ajuste esteja ligado.
#ifdef NV_TPK40
#define NV_TPK_PLAYER_RECORTE 1
#else
#define NV_TPK_PLAYER_RECORTE 0
#endif
// Flag de EXECUCAO (#241, #290): Ajustes > Trailers > "Zoom no trailer e no
// player (experimental)" deixa cada dono testar na propria TV. Comeca no padrao de compilacao acima.
static int zoomRoi = 0;
// 1 so enquanto o que toca e um TRAILER (trailer.c marca depois do video_tocar);
// zerado a cada abertura e em video_parar. So informativo desde o #290: com o
// ajuste ligado o player tambem ganha ROI fora da tela.
void video_tpk_trailer_marcar(int sim) { emTrailer = sim ? 1 : 0; }
// #471: o destino atual e o furo do cartao da home (trailer.c, trailer_no_cartao).
void video_tpk_cartao_marcar(int sim) { emCartao = sim ? 1 : 0; }
void video_tpk_zoom_roi_definir(int ligado) {
  ligado = ligado ? 1 : 0;
  if (ligado == zoomRoi) return;
  zoomRoi = ligado;
  printf("[trailer] tpk zoom ROI (trailer + player): %s (setting)\n", ligado ? "on" : "off");
  fflush(stdout);
}
int video_tpk_zoom_roi(void) { return zoomRoi; }
#define TPK_TELA_W 1920   // o host (Program.cs) usa 1920x1080 e o layout tambem
#define TPK_TELA_H 1080
static unsigned roiForaLogado;
static int roiNaTela(int x, int y, int w, int h) {
  return w > 0 && h > 0 && x >= 0 && y >= 0 && x + w <= TPK_TELA_W && y + h <= TPK_TELA_H;
}
// Quadro (largura x altura) inteiro, proporcional, centrado em (x,y,w,h).
static void encaixar(int x, int y, int w, int h, int *fx, int *fy, int *fw, int *fh) {
  int ew = w, eh = h;
  if ((double)w * altura > (double)h * largura) ew = (int)((double)h * largura / altura + 0.5) & ~1;
  else eh = (int)((double)w * altura / largura + 0.5) & ~1;
  *fx = x + (w - ew) / 2; *fy = y + (h - eh) / 2; *fw = ew; *fh = eh;
}
// Versao da Tizen (gpunivel.c le platform.version); so o maior importa.
static int tizenMaior;
void video_tpk_tizen_definir(const char *v) { tizenMaior = v ? atoi(v) : 0; }
// COVER NO CARTAO (#471): so na Tizen 6.x (a TV do relato, uma AU7000 6.0) ou com
// o zoom experimental ligado. ROI com origem negativa nunca foi visto no 4/5 (#426)
// e apaga o plano na Tizen 9 (#188), entao as demais familias ficam no quadro
// inteiro encaixado. A prova visual do cover nesta TV ainda e do dono.
static int coverCartao(void) { return tizenMaior == 6 || NV_TPK_ZOOM_ROI || zoomRoi; }
// O modo EFETIVO: home.c abre o furo e trailer.c espera o recorte por aqui.
int video_tpk_cartao_cover(void) { return coverCartao() && !cartaoSemCover; }
void video_tpk_cartao_destino(float x, float y, float w, float h, float *ox, float *oy, float *ow, float *oh) {
  float fx=x, fy=y, fw=w, fh=h;
  if (!video_tpk_cartao_cover()) {
    float asp = (largura > 0 && altura > 0) ? (float)largura / (float)altura : 16.0f / 9.0f;
    if (w / h > asp) { fw = h * asp; fx = x + (w - fw) * 0.5f; }
    else             { fh = w / asp; fy = y + (h - fh) * 0.5f; }
    fx = (float)(int)(fx + 0.5f); fy = (float)(int)(fy + 0.5f);
    fw = (float)((int)(fw + 0.5f) & ~1); fh = (float)((int)(fh + 0.5f) & ~1);
  }
  if (ox) *ox=fx; if (oy) *oy=fy; if (ow) *ow=fw; if (oh) *oh=fh;
}
// O furo e a intersecao do plano final com o cartao: cover nunca apaga fora dele.
void video_tpk_cartao_furo(float x, float y, float w, float h, float *ox, float *oy, float *ow, float *oh) {
  float fx, fy, fw, fh;
  if (emTrailer && emCartao && planoW > 0 && planoH > 0) {
    fx=planoX; fy=planoY; fw=planoW; fh=planoH;
  } else video_tpk_cartao_destino(x,y,w,h,&fx,&fy,&fw,&fh);
  float right = fx + fw < x + w ? fx + fw : x + w;
  float bottom = fy + fh < y + h ? fy + fh : y + h;
  if (fx < x) fx=x; if (fy < y) fy=y;
  fw = right > fx ? right - fx : 0; fh = bottom > fy ? bottom - fy : 0;
  if (ox) *ox=fx; if (oy) *oy=fy; if (ow) *ow=fw; if (oh) *oh=fh;
}
static unsigned cartaoLogado;
static void cartaoModo(const char *modo, const char *porque) {
  if (cartaoLogado == sessao) return;
  cartaoLogado = sessao;
  printf("[trailer] tpk cartao: %s (%s, tizen %d)\n", modo, porque, tizenMaior);
  fflush(stdout);
}
static void cartaoEncaixar(int x, int y, int w, int h, const char *porque) {
  int fx, fy, fw, fh;
  encaixar(x, y, w, h, &fx, &fy, &fw, &fh);
  cartaoModo("letterbox", porque);
  video_janela(fx, fy, fw, fh);
}
void video_janela_fonte(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
  double qw = largura, qh = altura, ex, ey;
  int X, Y, W, H, lx, ly, lw, lh, cartao;

  // Sem as dimensoes do quadro, ou sem recorte de verdade, o destino cru serve.
  if (qw < 2.0 || qh < 2.0 || sw <= 0 || sh <= 0) { temRoi = 0; video_janela(dx, dy, dw, dh); return; }
  // Trailer do CARTAO: quadro inteiro encaixado (ver video_tpk_cartao_marcar).
  // Hero, detalhe e tela cheia seguem o caminho de sempre, abaixo.
  cartao = emTrailer && emCartao;
  if (cartao && !video_tpk_cartao_cover()) {
    // A home ja encaixou o destino pelo mesmo retangulo usado no furo.
    // Encaixar de novo perde pixels por arredondamento e expoe o fundo da TV.
    cartaoModo("letterbox", cartaoSemCover ? "cover recusado nesta sessao" : "cover sem prova nesta familia");
    video_janela(dx, dy, dw, dh);
    return;
  }
  if (cartao && sx <= 0 && sy <= 0 && sw >= (int)qw && sh >= (int)qh) { cartaoEncaixar(dx, dy, dw, dh, "quadro inteiro"); return; }
  // Recorte que cobre o quadro inteiro E o caso sem zoom: mesma coisa.
  if (sx <= 0 && sy <= 0 && sw >= (int)qw && sh >= (int)qh) { temRoi = 0; video_janela(dx, dy, dw, dh); return; }

  ex = (double)dw / (double)sw;
  ey = (double)dh / (double)sh;
  W  = (int)(qw * ex + 0.5);
  H  = (int)(qh * ey + 0.5);
  X  = (int)(dx - sx * ex + 0.5);
  Y  = (int)(dy - sy * ey + 0.5);

  // O plano maior que o painel (retrato no cartao) nao tem cover possivel.
  if (cartao && (W > TPK_TELA_W || H > TPK_TELA_H)) { cartaoSemCover = 1; cartaoEncaixar(dx, dy, dw, dh, "cover maior que o painel"); return; }

  printf("[video] tpk recorte %d,%d %dx%d de %.0fx%.0f -> roi %d,%d %dx%d\n",
         sx, sy, sw, sh, qw, qh, X, Y, W, H);
  fflush(stdout);
  if (cartao) cartaoModo("cover", "recorte de fonte");

  // Preserve the exact 2.0.4 fallback rectangle, including its opt-in gate.
  lx = X; ly = Y; lw = W; lh = H;
  if (cartao) encaixar(dx, dy, dw, dh, &lx, &ly, &lw, &lh);   // host sem ROI: encaixe
  else if (!NV_TPK_ZOOM_ROI && !NV_TPK_PLAYER_RECORTE && !zoomRoi && !roiNaTela(X,Y,W,H)) {
    lx = dx; ly = dy; lw = dw; lh = dh;
  }
  planoX=X; planoY=Y; planoW=W; planoH=H;
  // New hosts accept B without a diagnostic/experimental flag for the player.
  // Trailer zoom remains opt-in. Old staged hosts retain the previous gate.
  if (hAspecto && (!emTrailer || cartao || NV_TPK_ZOOM_ROI || zoomRoi || roiNaTela(X,Y,W,H))) {
    ultRoiX=X; ultRoiY=Y; ultRoiW=W; ultRoiH=H; temRoi=1;
    ultLegX=lx; ultLegY=ly; ultLegW=lw; ultLegH=lh;
    hAspecto(X,Y,W,H,lx,ly,lw,lh);
    return;
  }

  // #290: old host / trailer path.
  if (!cartao && !NV_TPK_ZOOM_ROI && !NV_TPK_PLAYER_RECORTE && !zoomRoi && !roiNaTela(X, Y, W, H)) {
    if (roiForaLogado != sessao) {
      roiForaLogado = sessao;
      printf("[video] tpk: roi fora da tela nao vai ao plano, fica o destino %d,%d %dx%d (sem zoom)\n",
             dx, dy, dw, dh);
      fflush(stdout);
    }
    temRoi = 0;
    video_janela(dx, dy, dw, dh);
    return;
  }

  ultRoiX = X; ultRoiY = Y; ultRoiW = W; ultRoiH = H; temRoi = 1;
  ultLegX=lx; ultLegY=ly; ultLegW=lw; ultLegH=lh;
  if (hJanela) hJanela(X, Y, W, H);
}
// #290 ("aspect ratio still doesn't work"): sem recorte o ciclo do player so
// oferecia modos que cabem na tela, e num 16:9 todos dao o mesmo retangulo. O
// mesmo ajuste experimental do trailer (desligado por padrao) libera os modos
// de recorte/zoom no player.
int  video_recorte_fonte(void) { return hAspecto != NULL || NV_TPK_ZOOM_ROI || NV_TPK_PLAYER_RECORTE || zoomRoi; }
int  video_recorte_fonte_trailer(void) { return NV_TPK_ZOOM_ROI || zoomRoi; }
// O host prende o plano em mais de um ponto depois do prepare; um ROI pedido
// cedo pode ser engolido. trailer.c/player.c repetem o pedido nos primeiros
// segundos por aqui — reenvia o ultimo ROI calculado, sem recalcular.
void video_recorte_reaplicar(void) {
  if (!temRoi) return;
  if (hAspecto) hAspecto(ultRoiX,ultRoiY,ultRoiW,ultRoiH,ultLegX,ultLegY,ultLegW,ultLegH);
  else if (hJanela) hJanela(ultRoiX,ultRoiY,ultRoiW,ultRoiH);
}
const char *video_url_atual(void) { return urlAtual; }
static double posReal(void) {
  if (cv.ativo && cv.prog) {
    // Ate o deslocamento ser conhecido, o comeco do fluxo (ou o alvo do seek, ou
    // a ultima posicao conhecida: 0 aqui virava "reconectando: alvo 0s").
    if (!cv.deslocOk) {
      double b = dts_tpk_inicio_stream();
      return b >= 0 ? b : cv.seekPend ? cv.seekAlvo : cv.posConhecida;
    }
    return (hPos && pronto) ? hPos() / 1000.0 + cv.desloc : cv.posConhecida;
  }
  return (hPos && pronto) ? hPos() / 1000.0 : 0;
}
// SEEK IN FLIGHT (player.c, owner 20/10): the host's position cache (posMs)
// only moves when SetPlayPositionAsync finishes, so until then video_pos() is
// the OLD position. The player keeps its target while this says 1.
int video_busca_em_voo(void) {
  return atomic_load(&buscaAlvoMs) >= 0 || reconBuscarMs >= 0 || cv.reabrindo ||
         (cv.ativo && cv.seekPend) || mf.pend;
}
unsigned video_busca_falhas(void) { return atomic_load(&buscaFalhas); }
int video_pos_amostra(double *pos, double *carimbo, double *incerteza, unsigned *seq) {
  int ms; unsigned sq;
  if (!pronto || cv.reabrindo || (cv.ativo && cv.prog && !cv.deslocOk)) return 0;
  pthread_mutex_lock(&amTrava);
  sq = am.valido ? am.seq : 0; ms = am.ms; *carimbo = am.carimbo; *incerteza = am.incerteza;
  pthread_mutex_unlock(&amTrava);
  if (!sq) return 0;
  *pos = ms / 1000.0 + (cv.ativo && cv.prog ? cv.desloc : 0.0);
  *seq = sq;
  return 1;
}
/* Frame rate and HDR of the playing MKV (picture sync, legimagem.c).
 * 0 = unknown (not an MKV, or the header has no DefaultDuration). */
int video_tpk_quadro(double *fps, int *hdr) {
  int i;
  *fps = 0; *hdr = 0;
  for (i = 0; i < mkvN; i++)
    if (mkvFx[i].tipo == 1) {
      if (mkvFx[i].quadroNs > 0) *fps = 1e9 / (double)mkvFx[i].quadroNs;
      *hdr = mkvFx[i].transferencia == 16 || mkvFx[i].transferencia == 18 || mkvFx[i].dvPerfil > 0;
      break;
    }
  return *fps > 1.0;
}
int video_pos_incerta(void) {
  if (!atomic_load(&posIncerta)) return 0;
  // Sem leitura em 1,5 s (consulta presa): volta ao de antes em vez de esconder.
  if (SDL_GetTicks() - atomic_load(&incertaTick) > 1500u) { atomic_store(&posIncerta, 0); return 0; }
  return 1;
}
double video_pos(void) {
  if (terminou && video_duracao() > 0) return video_duracao();
  return cv.reabrindo ? cv.posMantida : posReal();
}
double video_duracao(void) {
  // Progressivo: o player ve um fluxo sem fim; a duracao e a da fonte.
  if (cv.ativo && cv.prog) return dts_tpk_duracao();
  return durMs / 1000.0;
}
// Capitulos do MKV lidos por um fio lateral (capmkv.c, 203-capitulos).
double video_creditos(void) { return capmkv_creditos(video_duracao()); }
double video_buffer_fim(void) { return 0; }
// SAMSUNG (.tpk): o host so repassa a PORCENTAGEM do buffering (evento 7);
// nada de bytes, segundos, conexoes ou quadros. O host e o do link pedido.
void video_estatisticas(VideoStats *s) {
  video_stats_vazio(s);
  if (urlAtual[0]) s->bufferPct = bufPct;
  vazao_host_publico(urlAtual, s->host, sizeof s->host);
}
unsigned video_bufferando_ms(void) {
  unsigned ms;
  // Esperando para reconectar: o watchdog (app.c) nao troca de fonte.
  if (nv_recon_ativa(&recon)) return 0;
  ms = bufferando ? SDL_GetTicks() - bufferDesde : 0;
  // Reabrindo o convertido (seek no TS, troca): o anel de buffer aparece
  // (>= 600 ms), mas nunca os 30 s com que o app.c troca de fonte; o teto de
  // 60 s do reabrindo (reabrindoVigiar) devolve o estado real ao app.
  if (cv.reabrindo && ms > 25000u) ms = 25000u;
  return ms;
}
void video_definir_dv(int dv) { (void)dv; }
void video_definir_cabecalhos(const char *c) { snprintf(cabecalhos, sizeof cabecalhos, "%s", c ? c : ""); }
const char *video_cabecalhos(void) { return cabecalhos; }
void video_definir_mp4(int m) { fonteMp4 = m != 0; }
int  video_tocando(void) { return tocando; }
int  video_pronto(void) { return pronto || cv.reabrindo; }
int  video_ativo(void) { return ativo; }
int  video_falhou(void) { return falhou; }
int  video_audio_nao_suportado(void) { return cv.ativo ? 0 : audioNaoSup; }
// video.c (video_fonte_tocou): recarregar uma fonte que ja tocou nao e fonte
// que nao abre. So convertendo, para o "Desligado" seguir como o master.
int video_tpk_fonte_tocou(void) { return cv.ativo && cv.fonteTocou; }
// player.c: o cartao de erro de fonte para o convertido (senao ele segue
// tocando atras do cartao e da tela inicial).
int video_tpk_convertendo(void) { return cv.ativo || cv.sondando; }
// player.c: o player novo do convertido ainda nao mostrou quadro numa fonte que
// ja tocava (seek no TS, reconexao, troca): a tela cobre o preto com uma capa.
// Ate o player estar no alvo, nao so no primeiro quadro: no HLS o player novo
// toca um instante do seg0 (o comeco do filme) antes de o seek pousar.
int video_tpk_capa(void) { return cv.reabrindo; }
// video.c (video_dts_estado): o selo/aviso do DTS enquanto este arquivo converte.
// TrueHD nao tem selo de DTS: 0.
int video_tpk_dts_estado(void) {
  const VideoFaixa *f = audioAtual >= 0 && audioAtual < nAudio ? &faixaAudio[audioAtual] : NULL;
  if (!cv.ativo) return 0;
  // Sem cabecalho MKV (MP4): a lista e a do player (so a AC3); o conversor sabe.
  if (!cv.track) return strncmp(dts_tpk_caminho(), "DTS", 3) ? 0 : cv.tocou ? VIDEO_DTS_CONVERTIDO : VIDEO_DTS_PREPARANDO;
  if (!f || !video_faixa_dts(f)) return 0;
  return cv.tocou ? VIDEO_DTS_CONVERTIDO : VIDEO_DTS_PREPARANDO;
}
int  video_seek_desistiu(void) { return 0; }
int  video_terminou(void) { return terminou; }
int  video_conflito_recurso(void) { return conflito; }

// OUTRO APP COM O VIDEO DA TV (#178, rawldon AU7000 e mais 2 TVs nos registros
// 10257-10419). O host avisa a interrupcao so como EV_PAUSADO; aqui isso so
// zerava `tocando`, `pronto` seguia 1 e o trailer continuava "tocando" com o
// furo aberto — e o que aparecia no furo era o video do YouTube, dono do plano.
// A razao so chega pela linha de log do host (Video.cs: "interrompido: " +
// Reason); o clipe mudo do arranque loga "prime interrompido ..." e fica de
// fora de proposito. Conflito = a fonte falhou: o trailer fecha e volta a
// arte, o player do filme cai no caminho de erro de sempre.
// So conta com o Nuvio NA FRENTE. Nos logs da 1.6.0 metade dos ResourceConflict
// vem logo depois de "[janela] principal visivel=False": a pessoa saiu do app
// no meio do filme e a TV tomou o video, o que e so uma pausa. E o player
// PRINCIPAL nunca e marcado como falho aqui — isso faria o automatico trocar de
// fonte ao voltar; quem le `conflito` e so o trailer.
static volatile int janelaVisivel = 1;
void video_tpk_log_host(const char *linha) {
  if (!linha) return;
  // "erro ConnectionFailed" vem logo antes do evento 5 (Video.cs).
  if (!strncmp(linha, "erro ", 5) && nv_recon_rede_tpk(0, linha)) reconLinhaRede = 1;
  // O player acusa o codec sem lista vazia: mesmo aviso, com a linha de prova.
  // Video.cs loga o nome do enum PlayerError do Tizen.Multimedia, que e
  // "AudioCodecNotSupported" (log do S95C, 16/10); o outro e o nome do CAPI.
  if (!strncmp(linha, "erro NotSupportedAudioCodec", 27) || !strncmp(linha, "erro AudioCodecNotSupported", 27)) {
    printf("[audio] TV recusou o audio da fonte: player acusou NotSupportedAudioCodec (faixas da TV=%d)\n", nAudio);
    fflush(stdout);
    // Com a conversao possivel, o fio do app decide (convBombear); sem ela, o
    // aviso de sempre.
    if (!cv.ativo && convPossivel()) naoSupPend = 1;
    else if (!cv.ativo) audioNaoSup = 1;
  }
  // Video.cs: DisplayRoi recusado -> o plano foi para o encaixe (lx..lh).
  if (!strncmp(linha, "[aspecto] ", 10) && strstr(linha, " DisplayRoi") && emTrailer && emCartao) {
    cartaoSemCover = 1;
    if (temRoi) { planoX=ultLegX; planoY=ultLegY; planoW=ultLegW; planoH=ultLegH; }
    return;
  }
  if (strstr(linha, "[janela] principal visivel=")) {
    janelaVisivel = strstr(linha, "visivel=True") != NULL;
    return;
  }
  if (!strstr(linha, "interrompido: ResourceConflict") || !janelaVisivel) return;
  tocando = 0;
  if (!conflito) { printf("[video] tpk: outro app tomou o video da TV; trailers automaticos desligados nesta sessao\n"); fflush(stdout); }
  conflito = 1;
}
int  video_n_audio(void) { return nAudio; }
int  video_n_legenda(void) { return nLeg; }
const VideoFaixa *video_audio(int i) { return (i >= 0 && i < nAudio) ? &faixaAudio[i] : 0; }
const VideoFaixa *video_legenda(int i) { return (i >= 0 && i < nLeg) ? &faixaLeg[i] : 0; }
int  video_legenda_ordinal_mkv(int i) { return (i >= 0 && i < nLeg) ? faixaLeg[i].ordinalMkv : -1; }
// #269: a sonda de verdade (antes era sempre 2, "nao e MKV", e nenhuma faixa
// ia ao overlay). 0 enquanto procura, ou com o cabecalho lido e ainda nao
// aplicado as faixas (sondaMkv, no proximo bombear); 2 quando nao e MKV.
int  video_mkv_sondado(void) {
  if (!urlAtual[0] || fonteMp4 || mkvNaoMkv) return 2;
  if (mkvEstado == 1 || mkvEstado == 2) return 0;
  if (faixasNovas && mkvN > 0 && (nAudio || nLeg || faixasmkv_tem_imagem(mkvFx, mkvN))) return 0;
  return 1;
}
// Barato: a folha chama a cada quadro enquanto espera. O Range sai no proximo
// bombear, sem esperar os 5 s de video (precisa do prepare feito).
void video_sondar_mkv_agora(void) { if (mkvEstado == 1) mkvSondarJa = 1; }
int  video_audio_atual(void) { return audioAtual; }
int  video_legenda_atual(void) { return legAtual; }
void video_escolher_audio(int i) {
  if (i < 0 || i >= nAudio) return;
  // Faixa DTS/TrueHD do arquivo (a TV nao a lista): converte essa.
  if (faixaAudio[i].numero < 0 && faixaAudio[i].stream_id > 0) {
    const MkvFaixa *m = mkvTrack(faixaAudio[i].stream_id);
    if (m && faixaRecusada(m)) {
      if (cv.ativo && cv.track == faixaAudio[i].stream_id) { audioAtual = i; return; }
      if (convPossivel()) { audioAtual = i; convIniciar(faixaAudio[i].stream_id, video_pos(), "faixa escolhida no menu"); }
      return;
    }
    // Convertendo, uma faixa que a TV toca: volta ao original e escolhe ela.
    if (cv.ativo) {
      snprintf(cv.rotuloVolta, sizeof cv.rotuloVolta, "%s", faixaAudio[i].rotulo);
      audioAtual = i;
      convVoltar();
    }
    return;
  }
  audioAtual = i;
  // Deferred until the first tick: a write before Start is accepted and ignored.
  // The tick, not the frame - audio was measured working that way.
  if (!audioComecou) { audioPend = i; return; }
  if (hEscolher) hEscolher(0, faixaAudio[i].numero);
}
void video_escolher_legenda(int i) {
  if (i >= nLeg) return;
  // Convertendo, as legendas sao as do arquivo: texto vai ao overlay do app
  // (faixas.c/mkvass, lendo a fonte original); o fluxo convertido nao tem
  // legenda nenhuma, entao nada vai ao player.
  if (i >= 0 && cv.ativo) {
    legAtual = i; legPend = -1; legendaLimpar(1);
    printf("[dts-tpk] legenda %d (%s, %s) sem faixa no fluxo convertido: so o overlay do app desenha texto\n",
           i, faixaLeg[i].rotulo, faixaLeg[i].codec[0] ? faixaLeg[i].codec : "?");
    fflush(stdout);
    return;
  }
  // Escondida da TV (mkvfix.h): so o overlay do app desenha; nada vai ao player.
  if (i >= 0 && legOcultas()) {
    legAtual = i; legPend = -1; legendaLimpar(1); legendaMarcarEnvio(-1);
    return;
  }
  legAtual = i;
  // Every new choice supersedes an older deferred one, including immediate
  // choices made after the settle window but before the next pump.
  legPend = -1; legReescrever = -1; seekNearWrite = 0;
  legendaLimpar(1);
  // Faixa so do app (PGS, numero -1): nada a escrever no player.
  if (i >= 0 && faixaLeg[i].numero < 0) { legendaMarcarEnvio(-1); return; }
  if (i < 0) legendaMarcarEnvio(-1);
  if (i >= 0 && hEscolher) {
    if (!comecou || SDL_GetTicks() - comecouEm < LEG_ACOMODAR_MS ||
        atomic_load(&buscaAlvoMs) >= 0) { legPend = i; return; }
    legendaEnviar(i);
  }
}

int  video_legenda_nativa(char *d, int t) {
  if (!d || t < 2) return 0;
  d[0] = 0;
  if (!ativo || legAtual < 0 || !travaLeg) return 0;
  SDL_LockMutex(travaLeg);
  if (!legCuesBloqueados && legTexto[0] && (Sint32)(legAte - SDL_GetTicks()) > 0) snprintf(d, (size_t)t, "%s", legTexto);
  SDL_UnlockMutex(travaLeg);
  return d[0] != 0;
}
void video_legenda_externa(const char *u) { (void)u; }
// O atraso do estilo vale para a embutida (o player desloca o evento).
void video_legenda_estilo(const VideoLegendaEstilo *e) { if (e && hEscolher) hEscolher(2, e->atrasoMs); }
int  video_tem_atmos(void) { return 0; }
int  video_tem_dolby_vision(void) { return 0; }
const char *video_hdr(void) { return "none"; }
// Numeric host error is useful evidence even when no textual cause is exposed.
// Formatting happens on the app thread; the callback only publishes atomics.
const char *video_erro_texto(void) {
  static char texto[64];
  if (!atomic_load(&temErroDetalhe)) return "";
  snprintf(texto, sizeof texto, "Samsung player error 0x%08x",
           atomic_load(&erroDetalhe));
  return texto;
}
int video_decoder_anunciou(void) { return 1; }
// Escala do alvo de desenho (GPU adaptativa, LG): o host .tpk nao usa.
void video_escala_definir(int sw, int sh) { (void)sw; (void)sh; }
int  video_largura(void) { return largura; }
int  video_altura(void) { return altura; }
int  video_pode_forcar_sdr(void) { return 0; }
int  video_velocidade_suportada(void) { return !velRecusada && hEscolher != NULL; }
void video_velocidade(int c) {
  if (c <= 0 || c > 400) c = 100;
  if (!velRecusada) velPedida = c;
}
int  video_velocidade_atual(void) { return velRecusada ? 100 : velPedida; }
void video_velocidade_recusada(void) {
  velRecusada = 1; velPedida = 100;
  if (ativo && pronto && hEscolher && velEnviada != 100) hEscolher(3, 100);
  velEnviada = 100;
}
int  video_velocidade_bloqueada(void) { return 0; }
void video_forcar_sdr(void) {}
void video_encerrar(void) { video_parar(); }
#endif
