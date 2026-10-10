#include "streams.h"
#include "plrui.h"
#include "ondever.h"
#include "naovideo.h"
#include "fonteantecipa.h"
#include "fonteparalela.h"
#include "tex_cache.h"
#include "livetv_regras.h"
#include "idioma.h"
#include "badges.h"
#include "selospacote.h"
#include "logotitulo.h"
#include "tex_cache.h"
#include "catalogo.h"
#include "limpa.h"
#include "vazao.h"
#include <ctype.h>
#include <strings.h>
#include <pthread.h>
#include <stdatomic.h>
#include "rede.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"   /* ajustes_qualidade: o teto de "Qualidade maxima" */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>
#include "addons.h"
#include "marco.h"
#include "debrid.h"
#include "fonteregra.h"
#include "p2p.h"
#include "p2pmotor.h"
#include "fonteauto.h"
#include "video.h"
#include "botoes.h"
#include "ponteiro.h"
#include "player.h"
#include "plrilha.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"

// A FOLHA DE FONTES (dono, 02/10: "muito infantil, nao ta polida como o
// resto"; aprovou o mockup "E" num canvas de tres rodadas). A folha encosta na
// borda direita e se funde com a arte por um degrade; a lista e AGRUPADA POR
// RESOLUCAO, e cada linha diz primeiro a QUALIDADE ("Dolby Vision", "BluRay
// Remux") — o nome do addon ("[AD] Debridio 4K") nao diz nada sobre a fonte
// e era a primeira coisa que a linha mostrava. O resto da linha de antes
// (provedor, descricao, meta, seis selos) lia como planilha.
// A ILHA DO MOCKUP APROVADO (Glass UI, design/glass-ilha "Fontes", e o
// player-mockup de 03/10): 820 de largura, a 40 das bordas, raio 36 — a
// mesma folha dos Episodios no player.
#define FOLHA_W        820.0f
#define FOLHA_MARGEM    40.0f
#define FOLHA_RAIO_IL   36.0f
#define FOLHA_PAD_E     48.0f  // da borda da folha ao cartao da linha
#define FOLHA_PAD_D     56.0f
#define FOLHA_TXT       26.0f  // do cartao ao texto
// NO PLAYER A FOLHA NASCE DA ILHA DO RELOGIO (dono, 03/10: "no player o
// componente do source tem que sair da ilha do relogio"), como Audio,
// Legendas e Episodios: plrilha.h cresce a pilula da hora ate o corpo, no
// canto do relogio, e a folha desenha dentro dele. Fora do player (detalhe)
// segue a folha da borda direita. `folhaOY` desce o conteudo para baixo do
// cabecalho da ilha e `folhaBase` e onde a lista termina.
static float folhaOY, folhaBase;
#define FOLHA_ILHA_Y     48.0f   // plrilha.c: Y_TOPO
#define FOLHA_ILHA_CAB   64.0f   // plrilha.c: CAB_H (a linha da hora)
#define FOLHA_ILHA_BASE  48.0f   // margem de baixo = a de cima
#define FOLHA_ILHA_PAD   22.0f   // do cabecalho ao kicker, como os Episodios
// CABECALHO COMPACTO (dono, 06/10: "subir os botoes para o lado do titulo e
// as abas em cima, para o cabecalho nao ficar tao grande"). Tres faixas, de
// cima: a linha do kicker (contexto, ou a ajuda do botao em foco), a linha do
// titulo com TODOS os botoes a direita (nunca mais desce uma linha, ver
// medirCabecalho) e as abas de addon. A lista comeca logo abaixo das abas.
#define FOLHA_CAB_Y    100.0f  // topo dos botoes, centrados no titulo
#define FOLHA_ABAS_Y   168.0f  // topo da faixa das abas
#define FOLHA_ABAS_H    56.0f
#define FOLHA_ABA_H     44.0f  // a pilula de cada aba, dentro da faixa
#define FOLHA_TOPO     (FOLHA_ABAS_Y + FOLHA_ABAS_H + 24.0f + folhaOY)
static void medirCabecalho(void);
#define FOLHA_LINHA_H  112.0f  // linha sem marca e sem arquivo
#define FOLHA_MARCA_H   30.0f  // "SUA ESCOLHA ANTERIOR" / "REPRODUZINDO AGORA"
#define FOLHA_ARQ_H     34.0f  // nome do arquivo, so na linha em foco
#define FOLHA_FIT_H     32.0f  // StreamFit: evidencia da conexao, so na linha em foco
#define FOLHA_LINHA_GAP  4.0f
#define FOLHA_SEC_H     64.0f  // cabecalho "4K  ULTRA HD ... 3 fontes"
#define FOLHA_SEC_GAP   26.0f
#define FOLHA_RAIO      22.0f
#define FOLHA_SELO_H    24.0f  // logos menores (dono, 02/10: "ficaram muito grande")
#define FOLHA_AUDIO_W  60.0f
#define FOLHA_AUDIO_N   8
#define FOLHA_AUDIO_BAR 4.0f
#define FOLHA_AUDIO_GAP 4.0f
// Canal 0..255 saturado: as tintas secundarias somam um degrau ao canal, e
// sobre realce escuro a principal ja e 255.
#define C8(v) ((v)>255?255:(v))

static Stream *lista;
static int n = 0;
// ORDEM DE EXIBICAO (#221). `lista` so cresce no fim (stream_lista_acrescentar)
// e e por isso que os indices dela nao mudam com a busca em andamento: a
// verificacao, a fonte tocando, a preferida e as excluidas sao todas indices.
// O que a pessoa ve, e a ordem que o automatico usa para desempatar, e por
// `chave` = (addon << 16) | posicao dentro da resposta dele: a mesma ordem da
// lista inteira de antes, montada na ordem dos addons. `exib[k]` e o indice
// da k-esima na tela. Lista inteira (stream_definir_lista): chave = indice.
static unsigned *chave;
static int *exib;
#define ORD(k) (exib ? exib[k] : (k))
#define AUTO_EXCL_MAX 32
static int automaticasExcluidas[AUTO_EXCL_MAX];
static int nAutomaticasExcluidas;
// #409: so Android publica estas capacidades. -1 preserva LG/Tizen/host.
static int decoder4k[4] = { -1, -1, -1, -1 }; // HEVC, AVC, VP9, AV1
static int decoderFalhaAltura[4];             // apenas a lista atual
static int excedeDecoder(const Stream *s);
static pthread_mutex_t autoExclTrava = PTHREAD_MUTEX_INITIALIZER;
// Trava entre a lista e os fios de verificacao (ver Lote, abaixo): a troca de
// lista e as leituras/escritas dos fios em `lista[]` passam por ela.
static pthread_mutex_t verTrava = PTHREAD_MUTEX_INITIALIZER;
// Sobe a cada lista nova, sob verTrava. A verificacao compara antes de gravar a
// url resolvida: indice de uma lista nao vale na seguinte (ver verificarUma).
static unsigned listaGeracao;
static int atual = -1, recarregar;
static char contexto[320];
// O ALVO DA LISTA — issue #101. Ver a nota longa em streams.h: `alvoPedido` e
// o carimbo do proximo pedido e `alvoLista` o da lista que esta em memoria.
// 64 e o mesmo tamanho que app.c usa para montar "tt1234567:99:99" e para os
// ids de canal do stalker/xtream, que sao os maiores que passam por aqui.
static char alvoPedido[64], alvoLista[64];
static pthread_mutex_t fitMetaTrava = PTHREAD_MUTEX_INITIALIZER;
// RUNTIME PER EXACT TARGET (F03). A few recent targets, each with its two
// provenances kept apart: metadata (TMDB/Cinemeta/addon runtime) and the
// player's real media duration. Media beats metadata of the same target; a
// target never inherits another's runtime. Different producers (the detail's
// TMDB sheet, the player) may report for different targets at once, which is
// why a single slot would lose data.
#define FIT_DUR_SLOTS 8
typedef struct { char alvo[64]; double metaSeg, midiaSeg; unsigned uso; } FitDur;
static FitDur fitDur[FIT_DUR_SLOTS];
static unsigned fitDurUso;
static double (*fitMetaFonte)(const char *alvo);
static char fitFotoAlvo[64];
static double fitFotoSeg;
static StreamfitDuracao fitFotoOrigem;
static StreamfitFoto fitFoto;
static StreamfitResultado *fitResultados;
static unsigned char *fitClasses;
static int fitCap, fitN;
static int aberta;

void stream_fit_duracao(const char *alvo, double seg, StreamfitDuracao origem) {
  FitDur *d = NULL, *velho = &fitDur[0];
  if (!alvo || !*alvo || strlen(alvo) >= sizeof fitDur[0].alvo) return;
  if (!isfinite(seg) || seg < 1 || seg > 86400) seg = 0;
  pthread_mutex_lock(&fitMetaTrava);
  for (int i = 0; i < FIT_DUR_SLOTS; i++) {
    if (!strcmp(fitDur[i].alvo, alvo)) { d = &fitDur[i]; break; }
    if (!fitDur[i].alvo[0]) { if (velho->alvo[0]) velho = &fitDur[i]; }
    else if (velho->alvo[0] && fitDur[i].uso < velho->uso) velho = &fitDur[i];
  }
  if (!d && seg > 0 && origem != SF_DUR_DESCONHECIDA) {
    d = velho; memset(d, 0, sizeof *d);
    snprintf(d->alvo, sizeof d->alvo, "%s", alvo);
  }
  if (d) {
    // Zero clears only the provenance it names; unknown clears both.
    if (origem == SF_DUR_MEDIA) d->midiaSeg = seg;
    else if (origem == SF_DUR_METADATA) d->metaSeg = seg;
    else d->midiaSeg = d->metaSeg = 0;
    d->uso = ++fitDurUso;
    if (!d->midiaSeg && !d->metaSeg) d->alvo[0] = 0;
  }
  pthread_mutex_unlock(&fitMetaTrava);
}
void stream_fit_fonte_metadados(double (*fonte)(const char *alvo)) { fitMetaFonte = fonte; }

static void fitAbrir(void) {
  double meta = 0, midia = 0;
  streamfit_foto(&fitFoto, streamfit_agora_ms());
  snprintf(fitFotoAlvo, sizeof fitFotoAlvo, "%s", alvoPedido);
  pthread_mutex_lock(&fitMetaTrava);
  if (fitFotoAlvo[0])
    for (int i = 0; i < FIT_DUR_SLOTS; i++)
      if (!strcmp(fitDur[i].alvo, fitFotoAlvo)) { meta = fitDur[i].metaSeg; midia = fitDur[i].midiaSeg; break; }
  pthread_mutex_unlock(&fitMetaTrava);
  // The catalog lookup runs on the UI thread (the sheet opens there) and only
  // answers for this exact target; a mismatch answers 0.
  if (!midia && !meta && fitFotoAlvo[0] && fitMetaFonte) {
    double v = fitMetaFonte(fitFotoAlvo);
    meta = isfinite(v) && v >= 1 && v <= 86400 ? v : 0;
  }
  fitFotoSeg = midia ? midia : meta;
  fitFotoOrigem = midia ? SF_DUR_MEDIA : meta ? SF_DUR_METADATA : SF_DUR_DESCONHECIDA;
  fitN = 0;
}

// Cache by stable raw index, not display position. URL resolution and new
// measurements while open do not reclassify an existing card. New arrivals
// use the same frozen host/network/runtime evidence as the initial cards.
static void fitAtualizar(void) {
  if (n > fitCap) {
    int cap = n + 32;
    StreamfitResultado *r = realloc(fitResultados, (size_t)cap * sizeof *r);
    unsigned char *c = realloc(fitClasses, (size_t)cap);
    if (r) fitResultados = r;
    if (c) fitClasses = c;
    if (!r || !c) return;
    fitCap = cap;
  }
  while (fitN < n && fitN < fitCap) {
    Stream *s = &lista[fitN];
    // A new title must never inherit the previous title's frozen runtime.
    double seg = !strcmp(alvoLista, fitFotoAlvo) ? fitFotoSeg : 0;
    fitClasses[fitN] = (unsigned char)streamfit_classificar(&fitFoto, s->url,
                                      s->tamanhoBytes, seg, &fitResultados[fitN]);
    fitN++;
  }
}

StreamfitClasse stream_fit_folha_estado(int indice, StreamfitResultado *saida) {
  StreamfitResultado r = {0};
  if (aberta && indice >= 0 && indice < fitN && indice < n) r = fitResultados[indice];
  if (saida) *saida = r;
  return r.classe;
}
// Platforms whose network epoch can become known (redemarca fed by the
// Android ConnectivityManager monitor). Tests may force it.
#ifndef STREAMFIT_REDE_EXISTE
#ifdef NV_ANDROID
#define STREAMFIT_REDE_EXISTE 1
#else
#define STREAMFIT_REDE_EXISTE 0
#endif
#endif
static int fitPesada(int i) { return i >= 0 && i < fitN && fitClasses && fitClasses[i] == SF_PESADA; }
static void fitMbps(char *dst, size_t n, double kbps) {
  vazao_fmt_mbps(dst, n, kbps > 2e9 ? 2000000000 : (int)(kbps + .5), '.');
  plrui_decimal(dst);
}
// As duas linhas de uma medida REAL (razao SF_BITRATE_ESTIMADO): de onde veio o
// numero, a idade, a velocidade sustentada do host e a demanda estimada.
static void fitLinhasDe(const StreamfitResultado *rp, char *l1, size_t n1, char *l2, size_t n2) {
  const StreamfitResultado r = *rp;
  char idade[48], sust[64], nec[24], orc[24], mb[24];
  unsigned long min = (unsigned long)(r.idadeMs / 60000u);
  if (min < 1) snprintf(idade, sizeof idade, "%s", i18n("agora"));
  else if (min < 60) snprintf(idade, sizeof idade, i18n("há %d min"), (int)min);
  else snprintf(idade, sizeof idade, i18n("há %d h"), (int)(min / 60));
  fitMbps(mb, sizeof mb, r.sustentadoKbps);
  snprintf(sust, sizeof sust, i18n("%s Mbps sustentados"), mb);
  snprintf(l1, n1, "%s · %s · %s",
           i18n(r.origem == SF_ORIGEM_PASSIVA ? "Reprodução recente" : "Diagnóstico"), idade, sust);
  fitMbps(nec, sizeof nec, r.necessarioKbps);
  fitMbps(orc, sizeof orc, r.otimoKbps);
  if (l2 && n2) snprintf(l2, n2, i18n("precisa ~%s de %s Mbps disponíveis"), nec, orc);
}
// THE FOCUSED ROW'S CONNECTION LINES (F03). Only real evidence is spelled
// out: where the number came from and how old it is, the sustained speed of
// THIS source's host, then the estimated average demand against the budget.
// Every unknown case says so on one line, with the reason. `l2` may be NULL.
// Returns the frozen class of raw index `i`.
static StreamfitClasse fitTexto2(int i, char *l1, size_t n1, char *l2, size_t n2) {
  StreamfitResultado r;
  StreamfitClasse c = stream_fit_folha_estado(i, &r);
  if (l2 && n2) l2[0] = 0;
  if (r.razao == SF_BITRATE_ESTIMADO) {
    fitLinhasDe(&r, l1, n1, l2, n2);
    return c;
  }
  switch (r.razao) {
    case SF_SEM_REDE:
      // Only a platform with a network epoch source (Android) can ever have
      // data here. LG, Samsung TPK/WGT and desktop never do: no line at all,
      // instead of a permanent "no data" on every focused row.
      if (STREAMFIT_REDE_EXISTE) snprintf(l1, n1, "%s", i18n("Conexão: sem dados desta rede"));
      else if (n1) l1[0] = 0;
      break;
    case SF_SEM_TAMANHO: snprintf(l1, n1, "%s", i18n("Conexão: tamanho do arquivo não informado")); break;
    case SF_SEM_DURACAO: snprintf(l1, n1, "%s", i18n("Conexão: duração do título desconhecida")); break;
    case SF_SEM_HOST:    snprintf(l1, n1, "%s", i18n("Conexão: servidor só conhecido ao tocar")); break;
    default:             snprintf(l1, n1, "%s", i18n("Conexão: servidor ainda não medido")); break;
  }
  return c;
}
// One-line form (tests and logs); the sheet draws the two lines itself.
__attribute__((unused)) static StreamfitClasse fitTexto(int i, char *dst, size_t n) {
  char l2[128];
  StreamfitClasse c = fitTexto2(i, dst, n, l2, sizeof l2);
  if (l2[0]) { size_t k = strlen(dst); snprintf(dst + k, n - k, " · %s", l2); }
  return c;
}
// Lines the focused row opens for it: two with evidence, one when unknown.
// Frozen with the snapshot, so a row never changes height while open.
static int fitLinhas(int i) {
  StreamfitResultado r;
  stream_fit_folha_estado(i, &r);
  if (r.razao == SF_SEM_REDE && !STREAMFIT_REDE_EXISTE) return 0;
  return r.razao == SF_BITRATE_ESTIMADO ? 2 : 1;
}
void stream_definir_alvo(const char *id) {
  snprintf(alvoPedido, sizeof alvoPedido, "%s", id ? id : "");
}
int stream_lista_do_alvo(const char *id) {
  if (!id || !*id || !alvoLista[0] || n < 1) return 0;
  return !strcmp(alvoLista, id);
}
void stream_definir_atual(int i) { atual = i >= 0 && i < n ? i : -1; }
int stream_atual(void) { return atual; }

// A FONTE QUE A PESSOA JA TINHA ESCOLHIDO neste titulo, quando ela existe
// nesta lista. Quem a encontra e fontepref.c (a regra de igualdade mora la);
// aqui ela e so um indice que entra NA FRENTE da fila de verificacao.
//
// Por que um indice guardado e nao um parametro de stream_primeira_boa: a
// verificacao roda em fio proprio, disparada por app.c com a lista ja pronta,
// e acrescentar parametro obrigaria o fio a carregar o id do titulo — que ele
// nao tem e nao deveria precisar ter.
static int preferida = -1;
void stream_preferir(int i) { preferida = (i >= 0 && i < n) ? i : -1; }
int stream_preferida(void) { return preferida; }
static int automaticaExcluidaSemTrava(int indice) {
  int i;
  for (i = 0; i < nAutomaticasExcluidas; i++)
    if (automaticasExcluidas[i] == indice) return 1;
  return 0;
}
static int automaticaExcluida(int indice) {
  int resultado;
  pthread_mutex_lock(&autoExclTrava);
  resultado = automaticaExcluidaSemTrava(indice);
  pthread_mutex_unlock(&autoExclTrava);
  return resultado;
}
static int ehInformativa(const Stream *s) {
  // `naoVideo`: a sonda ja viu a URL responder pagina; o nome (naovideo_nome)
  // pega a linha de aviso antes de qualquer pedido.
  return s->naoVideo || naovideo_nome(s->rotulo, s->descricao, s->altura, s->tamanhoMB);
}
// Fora do automatico: ja descartada nesta sessao OU linha de aviso/placeholder do
// addon ("Meteor - Not configured", "Embed69 - {}", "... error"). Esses so ficam
// na folha manual (#284: 75% Android / 60% Samsung / 41% LG falham quando o
// automatico os escolhe, contra 6/14/9% das fontes com resolucao).
static int foraDoAuto(int i) {
  return automaticaExcluida(i) || (i >= 0 && i < n && (ehInformativa(&lista[i]) || excedeDecoder(&lista[i])));
}
int stream_automatico_disponivel(int indice) { return indice >= 0 && indice < n && !automaticaExcluida(indice) && !excedeDecoder(&lista[indice]); }
int stream_automatico_excluir(int indice) {
  int resultado = 0;
  pthread_mutex_lock(&autoExclTrava);
  if (indice >= 0 && indice < n && !automaticaExcluidaSemTrava(indice) &&
      nAutomaticasExcluidas < AUTO_EXCL_MAX) {
    automaticasExcluidas[nAutomaticasExcluidas++] = indice;
    resultado = 1;
  }
  pthread_mutex_unlock(&autoExclTrava);
  return resultado;
}
// IRMAS DE UMA FONTE QUE O PLAYER NAO CONSEGUIU ABRIR. Nos logs do .tpk 1.6.0
// (10384, 10406, 10414, 10417) o automatico escolhia "4KHDHub 4K", o player
// dava ConnectionFailed, e as duas tentativas seguintes eram OUTROS links
// "4KHDHub 4K" do mesmo addon — que falhavam igual, e as tres vagas acabavam
// sem nunca chegar a outro provedor. Irma = mesmo addon e mesmo rotulo.
//
// SO EXCLUI SE SOBRAR OUTRA CANDIDATA. Com um addon so (Torrentio + debrid,
// onde todo link tem o mesmo nome) tirar as irmas zeraria a fila na primeira
// falha; ai fica o comportamento antigo, uma de cada vez.
// Devolve quantas sairam alem da propria.
static int irma(int i, int indice) {
  return i != indice && !strcmp(lista[i].provedor, lista[indice].provedor) &&
         !strcmp(lista[i].rotulo, lista[indice].rotulo);
}
int stream_automatico_excluir_irmas(int indice) {
  int i, k = 0, sobra = 0;
  pthread_mutex_lock(&verTrava);
  if (indice < 0 || indice >= n) { pthread_mutex_unlock(&verTrava); return 0; }
  for (i = 0; i < n && !sobra; i++)
    if (i != indice && !irma(i, indice) && !automaticaExcluida(i)) sobra = 1;
  if (sobra)
    for (i = 0; i < n; i++)
      if (irma(i, indice) && stream_automatico_excluir(i)) k++;
  pthread_mutex_unlock(&verTrava);
  return k;
}
void stream_folha_contexto(const char *s) { snprintf(contexto, sizeof contexto, "%s", s ? s : ""); }
int stream_folha_recarregar(void) { int r = recarregar; recarregar = 0; return r; }

static int aberta = 0, foco = 0, escolha = -1;
static float anim = 0.0f, rolagem = 0.0f;
// Velocidade da mola de 2a ordem da rolagem (anim_mola2): partida macia e
// cauda exponencial, a MESMA curva que a home mede. A de 1a ordem que estava
// aqui partia na velocidade maxima e o primeiro quadro ja saltava 12%.
static float velRol = 0.0f;
// Linha do realce, em unidades de ITEM (2.4 = entre o terceiro e o quarto). O
// realce escorrega entre as linhas em vez de saltar: com o salto seco a folha
// parecia trocar de conteudo a cada tecla, e num D-pad e a continuidade do
// realce que diz "ainda e a mesma lista, voce so andou".


static int soP2P(const Stream *s);
// "E MP4?" tem UMA resposta (streams.h). O cartao da fonte e o anuncio ao video
// (app.c, video_definir_mp4) perguntavam cada um do seu jeito: o cartao olhava
// o rotulo, o video nao — e na C9 (2.0.3) um MP4 com ".mp4" so no rotulo
// (AIOStreams, URL sem extensao) mostrava MP4 e abria a tela/sonda do Dolby
// Vision em MKV.
int stream_e_mp4(const Stream *s) {
  if (!s) return 0;
  return s->mp4 || strstr(s->url, ".mp4") || strstr(s->rotulo, ".mp4");
}
static const char *containerDa(const Stream *s) {
  // "P2P": torrent que so o servidor de streaming toca. Sigla igual nas duas
  // linguas, como MP4/MKV.
  if (soP2P(s)) return "P2P";
  if (stream_e_mp4(s)) return "MP4";
  if (strstr(s->url, ".mkv") || strstr(s->arquivo, ".mkv") || strstr(s->descricao, ".mkv")) return "MKV";
  if (strstr(s->url, ".m3u8") || strstr(s->rotulo, "HLS")) return "HLS";
  // A CHAVE PASSA POR i18n AQUI, e nao no chamador. As outras tres devolucoes
  // sao siglas iguais nas duas linguas (MP4, MKV, HLS) e nao tem o que
  // traduzir; so esta e palavra. Ficava crua porque o chamador monta a linha
  // com snprintf e a varredura de i18n nao cobre valor de retorno de funcao —
  // apareceu na foto do album em ingles, com "ARQUIVO" no meio de "Sources",
  // "Reload" e "Automatic pick".
  return i18n("ARQUIVO");
}
static int soP2P(const Stream *s) {
  return !s->url[0] && s->infoHash[0] && !debrid_ativo() && p2p_ativo();
}

static Uint32 recebidaEm;
static int temRecebidaEm;
// Torrents que a ultima lista jogou fora por falta de debrid: com 0 na lista e
// isto > 0, a causa da folha vazia e "falta conta de debrid", nao "os addons
// nao tem" (1.3.12: "145 torrents sem debrid descartados" e folha vazia).
static int descartadosSemDebrid;

Uint32 stream_idade_ms(void) {
  return temRecebidaEm ? SDL_GetTicks() - recebidaEm : 0xFFFFFFFFu;
}

void stream_definir_lista(const Stream *l, int qtd) {
  stream_definir_lista_idade(l, qtd, 0);
}

static int selosPacoteDa(Stream *s);
static void selosAgendar(int ini, int qtd);
void stream_definir_lista_idade(const Stream *l, int qtd, Uint32 idade) {
  int i, k = 0;
  Stream *nova = l && qtd > 0 ? malloc(sizeof(Stream) * (size_t)qtd) : NULL;
  if (l && qtd > 0 && !nova) return;
  // Torrent sem url so fica se ha debrid para resolve-lo; senao seria uma linha
  // que nunca toca (shouldListStream do web). O servidor P2P experimental
  // (p2p.h) tambem o resolve, e so quando ligado.
  for (i = 0; i < qtd && nova; i++)
    if (l[i].url[0] || debrid_ativo() || p2p_ativo()) nova[k++] = l[i];
  if (nova && qtd - k) printf("[fonte] %d torrents sem debrid descartados\n", qtd - k);
  descartadosSemDebrid = nova ? qtd - k : 0;
  for (i = 0; i < k && nova; i++) nova[i].selosPacoteVer = 0;   // os selos saem do fio de fundo (selosAgendar)
  pthread_mutex_lock(&verTrava);
  free(lista); lista = nova; n = nova ? k : 0; atual = -1;
  fitN = 0; // raw identities have been replaced; preserve an open snapshot
  free(chave); free(exib); chave = NULL; exib = NULL;
  if (n > 0) {
    chave = malloc(sizeof *chave * (size_t)n);
    exib = malloc(sizeof *exib * (size_t)n);
    if (!chave || !exib) { free(chave); free(exib); chave = NULL; exib = NULL; }
    else for (i = 0; i < n; i++) { chave[i] = (unsigned)i; exib[i] = i; }
  }
  recebidaEm = SDL_GetTicks() - idade;
  temRecebidaEm = 1;
  listaGeracao++;
  pthread_mutex_unlock(&verTrava);
  selosAgendar(0, n);
  pthread_mutex_lock(&autoExclTrava);
  nAutomaticasExcluidas = 0;
  memset(decoderFalhaAltura, 0, sizeof decoderFalhaAltura);
  pthread_mutex_unlock(&autoExclTrava);
  // LISTA NOVA, INDICE VELHO NAO VALE. A preferida e uma posicao na lista
  // ANTERIOR; mantida, ela apontaria para outra fonte do episodio seguinte —
  // o tipo de defeito que toca a coisa errada sem nenhum erro no log.
  preferida = -1;
  foco = 0;
  // A LISTA HERDA O CARIMBO DO PEDIDO (issue #101). Quem publica e addons.c,
  // que so conhece o alvo DELE — e o dele pode ja estar obsoleto quando a
  // resposta chega. O carimbo vem de quem pediu, em app.c, e e ele que permite
  // a qualquer consumidor perguntar "esta lista e do episodio que eu quero?".
  snprintf(alvoLista, sizeof alvoLista, "%s", alvoPedido);
}

void stream_invalidar(const char *porque) {
  if (n > 0) {
    printf("[fonte] %d fontes de %s descartadas: %s\n", n,
           alvoLista[0] ? alvoLista : "(sem alvo)", porque ? porque : "");
    fflush(stdout);
  }
  stream_definir_lista(NULL, 0);
  alvoLista[0] = 0;
}

static int filtrado(int linha);
static int nOrdem, focoFixo = -1;
static float *linhaY, focoFixoY;
static void atualizarProvedores(void);
static int aberta, foco, grupo;
static float rolagem, velRol;

void stream_lista_acrescentar(const Stream *l, int qtd, int ordemAddon) {
  int i, k = 0, focoIdx = -1, linhaAntes = -1, total;
  Stream *nova, *tmp;
  unsigned *c2;
  int *e2;
  if (!l || qtd <= 0) return;
  nova = malloc(sizeof(Stream) * (size_t)qtd);
  if (!nova) return;
  for (i = 0; i < qtd; i++)
    if (l[i].url[0] || debrid_ativo() || p2p_ativo()) nova[k++] = l[i];
  if (qtd - k) printf("[fonte] %d torrents sem debrid descartados\n", qtd - k);
  descartadosSemDebrid += qtd - k;
  if (!k) { free(nova); return; }
  for (i = 0; i < k; i++) nova[i].selosPacoteVer = 0;
  // O CARTAO EM FOCO E O QUE FICA PARADO. Guardado pelo indice da lista (que
  // nao muda), e nao pela linha (que muda quando entra coisa acima).
  if (aberta && grupo == 1 && n > 0) {
    linhaAntes = foco;
    focoIdx = filtrado(foco);
    if (focoIdx < 0) linhaAntes = -1;
  }
  pthread_mutex_lock(&verTrava);
  total = n + k;
  tmp = realloc(lista, sizeof(Stream) * (size_t)total);
  c2 = realloc(chave, sizeof *c2 * (size_t)total);
  e2 = realloc(exib, sizeof *e2 * (size_t)total);
  if (tmp) lista = tmp;
  if (c2) chave = c2;
  if (e2) exib = e2;
  if (!tmp || !c2 || !e2) {
    pthread_mutex_unlock(&verTrava);
    free(nova);
    printf("[fonte] memoria insuficiente para %d fontes\n", k);
    return;
  }
  memcpy(lista + n, nova, sizeof(Stream) * (size_t)k);
  for (i = 0; i < k; i++)
    chave[n + i] = ((unsigned)(ordemAddon < 0 ? 0 : ordemAddon) << 16) | (unsigned)(i & 0xFFFF);
  // Insercao ordenada das novas em `exib`: as de antes ja estao em ordem, e
  // uma nova entra depois de toda chave menor OU IGUAL (estavel).
  for (i = 0; i < k; i++) {
    int idx = n + i, pos = n + i;
    while (pos > 0 && chave[exib[pos - 1]] > chave[idx]) { exib[pos] = exib[pos - 1]; pos--; }
    exib[pos] = idx;
  }
  n = total;
  if (!temRecebidaEm) { recebidaEm = SDL_GetTicks(); temRecebidaEm = 1; }
  pthread_mutex_unlock(&verTrava);
  selosAgendar(total - k, k);
  free(nova);
  printf("[fonte] +%d de %s (lista com %d)\n", k, l[0].provedor, n);
  fflush(stdout);
  if (aberta) atualizarProvedores();
  // A linha em foco muda de lugar quando entra fonte acima dela (os grupos tem
  // altura variavel): guardada aqui e reposta no proximo montar().
  if (focoIdx >= 0) { focoFixo = focoIdx; focoFixoY = linhaAntes < nOrdem ? linhaY[linhaAntes] : 0; }
}

int stream_ordem_addon(int i) {
  return i >= 0 && i < n && chave ? (int)(chave[i] >> 16) : 0;
}

int stream_n(void) {
  return n;
}

const Stream *stream_item(int i) {
  return i >= 0 && i < n ? &lista[i] : NULL;
}

// Pontuacao da regra do dono, do mais forte para o mais fraco:
//   MP4 4K Dolby Vision  >  4K Dolby Vision (qualquer container)
//   >  4K  >  Dolby Vision  >  resolucao  >  ordem de chegada
//
// Somar pesos em vez de comparar campo a campo deixa a regra num lugar so e
// legivel: mudar a preferencia e mexer num numero, nao reescrever um encadeado
// de ifs onde a ordem das comparacoes vira a regra escondida.
// TETO DE QUALIDADE ESCOLHIDO EM AJUSTES, que ate agora nao valia nada.
//
// "Qualidade maxima" (Automatica / 4K / 1080p / 720p) era gravada, sincronizada
// com a conta e lida por NINGUEM: `ajustes_qualidade()` nao tinha consumidor na
// escolha de fonte. Quem punha 1080p continuava abrindo a fonte 4K — inclusive
// em canal ao vivo, onde a 4K e a primeira da lista do addon e, numa conexao
// que nao a sustenta, ela e justamente a que demora ou nem abre.
//
// Devolve 0 para "sem teto".
static int alturaMax(void) {
  const char *q = ajustes_qualidade();
  if (!q || !q[0]) return 0;
  if (!strcmp(q, "4K"))    return 2160;
  if (!strcmp(q, "1080p")) return 1080;
  if (!strcmp(q, "720p"))  return 720;
  return 0;                                  // "Automatica"
}

// 1 quando a fonte cabe no teto. Fonte SEM altura declarada cabe: a maioria dos
// canais nao diz resolucao nenhuma, e recusar o que nao se sabe deixaria a
// pessoa sem fonte por causa de um campo que o addon nao preencheu.
static int cabeNaAltura(const Stream *s) {
  int teto = alturaMax();
  return !teto || !s->altura || s->altura <= teto;
}

// FAIXA DE TAMANHO DA ESCOLHA AUTOMATICA (Ajustes > Reproducao > Imagem e som:
// "Tamanho maximo" / "Tamanho minimo", em GB). Pedido de quem tem franquia de
// dados curta: o automatico so pega arquivo dentro da faixa. Mesma regra do
// teto de qualidade — preferencia, nao filtro: fora da faixa vai para o fim da
// fila (cabeNoTeto), e se so ha fonte fora dela a melhor ainda toca.
// Tamanho DESCONHECIDO cabe (como altura desconhecida). Bytes exatos do addon
// mandam; sem eles vale o tamanho em MB lido do texto. 1 GB = 1024 MB.
// MINIMO MAIOR QUE O MAXIMO: o minimo e ignorado (vale so o maximo), porque
// uma faixa vazia deixaria TODA fonte "fora" e o maximo e o pedido que protege
// a franquia. Devolve 0 (cabe) / 1 (acima do maximo) / 2 (abaixo do minimo).
static unsigned long long tamanhoMBFonte(const Stream *s) {
  if (s->tamanhoBytes) return s->tamanhoBytes >> 20;
  return s->tamanhoMB > 0 ? (unsigned long long)s->tamanhoMB : 0;
}
static int foraDaFaixaTamanho(const Stream *s) {
  int max = ajustes_tamanho_max_gb(), min = ajustes_tamanho_min_gb();
  unsigned long long mb = tamanhoMBFonte(s);
  if (!mb) return 0;
  if (max && min > max) min = 0;
  if (max && mb > (unsigned long long)max * 1024) return 1;
  if (min && mb < (unsigned long long)min * 1024) return 2;
  return 0;
}
static int cabeNoTeto(const Stream *s) {
  return cabeNaAltura(s) && !foraDaFaixaTamanho(s);
}

// PLUGIN / EMBED (R9, 04/10). Fonte de scraper QuickJS (MegaEmbed etc.): o
// bingeGroup que o adaptador de plugins poe e "nuvio-plugin|<id>|<q>". A
// resolucao que ela anuncia e o rotulo do embed, nunca medida, e o link e de
// hospedagem de terceiros. Nos logs da C9 (build 178dc061) o rotulo
// "MegaEmbed - 1080" liderava a lista de addon/debrid: pontos() nao conhecia a
// ORIGEM da fonte e ainda dava +5000 ao MP4 (os embeds sao MP4).
static int ehPlugin(const Stream *s) {
  return !strncmp(s->bingeGroup, "nuvio-plugin|", 13);
}

// StreamFit no automatico (R9): so uma fonte MEDIDA como pesada demais para a
// conexao desce. Exige o mesmo que a folha exige — bytes exatos, duracao real
// do alvo, janela de medida fresca no host final — e portanto sem medida, sem
// tamanho ou sem duracao a resposta e "nao sei" e a qualidade manda.
// Nunca chama a fonte de metadados (so a UI a chama): usa a duracao ja
// guardada em fitDur.
static pthread_mutex_t autoFotoTrava = PTHREAD_MUTEX_INITIALIZER;
static StreamfitFoto autoFoto;
static uint64_t autoFotoMs;
static StreamfitResultado fitAutoResultado(const Stream *s) {
  StreamfitResultado r = {0};
  uint64_t agora = streamfit_agora_ms();
  double seg = 0;
  if (!s->tamanhoBytes) { r.razao = SF_SEM_TAMANHO; return r; }
  pthread_mutex_lock(&fitMetaTrava);
  if (alvoLista[0])
    for (int i = 0; i < FIT_DUR_SLOTS; i++)
      if (!strcmp(fitDur[i].alvo, alvoLista)) {
        seg = fitDur[i].midiaSeg ? fitDur[i].midiaSeg : fitDur[i].metaSeg;
        break;
      }
  pthread_mutex_unlock(&fitMetaTrava);
  pthread_mutex_lock(&autoFotoTrava);
  if (!autoFotoMs || agora < autoFotoMs || agora - autoFotoMs > 200) {
    streamfit_foto(&autoFoto, agora);
    autoFotoMs = agora;
  }
  streamfit_classificar(&autoFoto, s->url, s->tamanhoBytes, seg, &r);
  pthread_mutex_unlock(&autoFotoTrava);
  return r;
}
static int fitPesadaAuto(const Stream *s) {
  // Com a folha aberta vale a classe congelada dela (o selo "Melhor" nao muda
  // de linha por uma medida que chegou com a folha na tela).
  if (aberta && fitClasses && s >= lista && s < lista + n && (int)(s - lista) < fitN)
    return fitClasses[s - lista] == SF_PESADA;
  return fitAutoResultado(s).classe == SF_PESADA;
}

// CAPACIDADE DA TELA (R9b). Nenhuma plataforma sabe responder ANTES de tocar se
// a TV mostra HDR/Dolby Vision (video_tem_dolby_vision so existe com o
// pipeline aberto), entao o padrao e -1 = desconhecido, e desconhecido NAO
// penaliza: so um 0 explicito tira o formato da conta.
static int telaHdr = -1, telaDv = -1;
void stream_definir_tela(int hdr, int dv) { telaHdr = hdr; telaDv = dv; }

static int acha(const char *t, const char *termo) {
  size_t n = strlen(termo);
  for (; *t; t++) if (!strncasecmp(t, termo, n)) return 1;
  return 0;
}

void stream_definir_decoder4k(int hevc, int avc, int vp9, int av1) {
  pthread_mutex_lock(&autoExclTrava);
  decoder4k[0] = hevc; decoder4k[1] = avc; decoder4k[2] = vp9; decoder4k[3] = av1;
  pthread_mutex_unlock(&autoExclTrava);
}
static int codecDaFonte(const Stream *s) {
  uint64_t b = s->badges | badges_detectar(s->rotulo) |
               badges_detectar(s->descricao) | badges_detectar(s->arquivo);
  if (b & badges_bit("co-av1")) return 3;
  if (b & badges_bit("co-x265")) return 0;
  if (b & badges_bit("co-x264")) return 1;
  const char *c[] = { s->rotulo, s->descricao, s->arquivo };
  for (int i = 0; i < 3; i++) {
    if (acha(c[i], "vp9") || acha(c[i], "vp09")) return 2;
    if (acha(c[i], "avc1")) return 1;
    if (acha(c[i], "av01")) return 3;
  }
  return -1; // Sem codec anunciado, nao ha evidencia para bloquear uma familia.
}
static int excedeDecoder(const Stream *s) {
  int c, r;
  if (s->altura <= 0) return 0;
  pthread_mutex_lock(&autoExclTrava);
  r = 0;
  for (int i = 0; i < 4; i++) r |= decoder4k[i] == 0 || decoderFalhaAltura[i] != 0;
  pthread_mutex_unlock(&autoExclTrava);
  if (!r) return 0;
  c = codecDaFonte(s);
  if (c < 0) return 0;
  pthread_mutex_lock(&autoExclTrava);
  r = (s->altura >= 2160 && decoder4k[c] == 0) || (decoderFalhaAltura[c] && s->altura >= decoderFalhaAltura[c]);
  pthread_mutex_unlock(&autoExclTrava);
  return r;
}
void stream_automatico_erro_decoder(int indice, int codigo, int renderer) {
  if (codigo != 4001 && codigo != 4003 && codigo != 4004 && codigo != 4005) return;
  // Renderer 0 = video, 1 = audio, 2/qualquer outro = desconhecido. Erros de
  // audio (EAC3/AC4 sem suporte) nao sao de codec de video: o caminho de
  // bloqueio por codec nao pode morder uma fonte HEVC 4K porque o EAC3 dela
  // falhou. O video_android.c so repassa o codigo se for renderer de video;
  // aqui o parametro explicito e a defesa em profundidade para futuros
  // chamadores (e o teste C, que nao liga o video_android.c).
  if (renderer != 0) {
    printf("[fonte] decoder: erro %d no renderer %d ignorado para codec de video (fonte %d)\n",
           codigo, renderer, indice);
    return;
  }
  pthread_mutex_lock(&verTrava);
  if (indice >= 0 && indice < n && lista[indice].altura > 0) {
    int c = codecDaFonte(&lista[indice]), h = lista[indice].altura;
    if (c < 0) { pthread_mutex_unlock(&verTrava); return; }
    pthread_mutex_lock(&autoExclTrava);
    if (!decoderFalhaAltura[c] || h < decoderFalhaAltura[c]) decoderFalhaAltura[c] = h;
    pthread_mutex_unlock(&autoExclTrava);
    printf("[fonte] decoder: erro %d, automatico evita codec=%d a partir de %dp nesta lista\n", codigo, c, h);
  }
  pthread_mutex_unlock(&verTrava);
}

// Perfil 8 declarado (ou camada base HDR10 marcada): a base HDR10 toca em tela
// sem Dolby Vision. Sem nenhuma das duas o perfil e desconhecido.
static int perfil8(const Stream *s) {
  const char *c[3] = { s->rotulo, s->descricao, s->arquivo };
  if (badges_fonte_hdr_marca(s->badges) >= 0) return 1;
  for (int i = 0; i < 3; i++)
    if (acha(c[i], "profile 8") || acha(c[i], "profile8") || acha(c[i], "dvhe.08") ||
        acha(c[i], "dvh1.08") || acha(c[i], "hdr10")) return 1;
  return 0;
}
static int perfil5(const Stream *s) {
  const char *c[3] = { s->rotulo, s->descricao, s->arquivo };
  for (int i = 0; i < 3; i++)
    if (acha(c[i], "profile 5") || acha(c[i], "profile5") || acha(c[i], "dvhe.05")) return 1;
  return 0;
}

#ifndef NV_STREAMS_LG
#if !defined(NV_ANDROID) && !defined(NV_TPK) && !defined(__EMSCRIPTEN__) && !defined(NV_LINUX_DESKTOP)
#define NV_STREAMS_LG 1
#else
#define NV_STREAMS_LG 0
#endif
#endif
// Nivel de HDR que a fonte ENTREGA nesta TV: 4 Dolby Vision, 3 HDR10+, 2 HDR10,
// 1 HDR generico/HLG, 0 SDR. Antes disto o HDR nem existia na pontuacao (so o
// DV em MP4), e as duas opcoes de Ajustes "Dolby Vision"/"Dolby Atmos" nao
// eram lidas por ninguem. DV so conta ligado em Ajustes, em tela que o aceita e
// onde o container toca (MP4; no Android qualquer um — na LG o MKV cai em
// HDR10, medido). Perfil 5 nao tem camada base HDR10: fora de TV Dolby Vision
// sai com cor errada, entao fica ABAIXO do HDR10.
static int nivelHdr(const Stream *s) {
  int flag = badges_fonte_hdr_marca(s->badges), nivel = 0;
  if (telaHdr == 0) return 0;
  if (flag == FMT_HDR10P) nivel = 3;
  else if (flag == FMT_HDR10) nivel = 2;
  else if (flag >= 0) nivel = 1;
  if (s->dolbyVision) {
    int toca = ajustes_dolby_vision() && telaDv != 0;
#ifndef NV_ANDROID
#ifdef NV_TPK
    toca = toca && stream_e_mp4(s);
#else
    // LG: DV em MKV so com o ajuste "Dolby Vision em MKV" ligado.
    toca = toca && (stream_e_mp4(s) || ajustes_dv_mkv());
#endif
#endif
    // LG, MP4: o perfil 5 toca como DV de verdade (webOS aciona o DV nativo no
    // MP4), entao conta como DV e nao fica abaixo do HDR10. Em MKV a LG cai em
    // HDR10 (IPT-PQ, cor lavada) e ele segue rebaixado; com DV desligado em
    // Ajustes ou tela sem DV tambem. Samsung e Android: nada muda.
    if (perfil5(s)) {
#if NV_STREAMS_LG
      if (ajustes_dolby_vision() && telaDv != 0 && stream_e_mp4(s)) nivel = 4;
      else
#endif
      if (nivel < 1) nivel = 1;
    }
    else if (toca) nivel = 4;
    // Tela SEM Dolby Vision (telaDv == 0 explicito, ex. Samsung): so o perfil 8
    // com base HDR10 toca como HDR10. DV sem perfil nem base declarada pode ser
    // perfil 5 e nao ha como saber: fica no nivel 1, abaixo do HDR10.
    else if (telaDv == 0 && !perfil8(s)) { if (nivel < 1) nivel = 1; }
    else if (nivel < 2) nivel = 2;        // perfil 8: a base HDR10 toca
  }
  return nivel;
}
static int degrauRes(const Stream *s) {
  return s->altura >= 2160 ? 3 : s->altura >= 1080 ? 2 : s->altura >= 720 ? 1 : 0;
}

// QUALIDADE do modo Equilibrio e Qualidade maxima, sempre < 50000 (as multas de
// origem/cache/teto abaixo sao >= 140000, entao nenhum HDR compensa um plugin
// ou uma fonte fora de cache).
static long qualidade(const Stream *s, int modo) {
  int h = nivelHdr(s), pref = ajustes_fonte_hdr(), d = degrauRes(s);
  long q;
  if (pref == 1) h = 0;                                   // Indiferente
  if (pref == 2) { q = d * 10000L - (h ? 3000 : 0); h = 0; }   // Evitar: SDR no mesmo degrau
  else if (modo == 0) q = (d + (h > 0)) * 10000L + h * 1200L;  // Equilibrio: HDR vale um degrau
  else q = d * 10000L + h * 1500L;                             // Maxima: resolucao, depois formato
  q += s->altura / 10;
  if (s->mp4) q += 300;
  if (s->dolbyAtmos && ajustes_dolby_atmos()) q += 200;
  return q;
}

#ifdef NV_TPK
// SAMSUNG NAO TOCA DTS NEM TRUEHD (#313; specs oficiais 2018+). Fonte cujo
// unico audio anunciado e DTS/DTS-HD/TrueHD abre mudo no .tpk. O selo vem do
// nome do arquivo: Atmos fica de fora (pode ser DD+ ou TrueHD) e qualquer selo
// Dolby Digital junto do DTS (nome com os dois) tira a multa. Pesa mais que um
// degrau de resolucao (10000) para o 1080p com AC3 passar na frente do 4K mudo,
// e continua longe das multas de cache/origem (>= 140000): so desempata a fila.
static long penalAudioTv(const Stream *s) {
  uint64_t recusa = badges_bit("a-dts") | badges_bit("a-dtshd") | badges_bit("a-dtshdma") |
                              badges_bit("a-dtsx") | badges_bit("a-truehd") | badges_bit("a-truehd-dv");
  uint64_t toca = badges_bit("a-dd") | badges_bit("a-ddp") | badges_bit("a-dd-dv") |
                            badges_bit("a-atmos") | badges_bit("a-atmos-dv");
  return (s->badges & recusa) && !(s->badges & toca) ? 20000 : 0;
}
#endif

// AS MULTAS de pontos() (teto, cache, origem, P2P, StreamFit, sem resolucao),
// em separado: o "MP4 primeiro" da LG (mp4PrimeiroNaFaixa) so reordena fontes
// com as MESMAS multas — e desempate, nunca passa por cima delas.
static long multasDe(const Stream *s) {
  long m = 0;
#ifdef NV_TPK
  m += penalAudioTv(s);
#endif
  // ACIMA DO TETO vai para o fim da fila, e nao para fora dela: o teto e
  // preferencia, nao filtro. Uma lista em que so ha 4K e com teto de 1080p tem
  // de continuar tocando — em 4K, com uma linha no log dizendo por que.
  // SEM RESOLUCAO (0p) vem depois de toda fonte com resolucao no mesmo estado de
  // cache: a altura desconhecida nao e prova de nada (#284). Maior que a
  // qualidade (< 50000), menor que cache/origem/teto.
  if (s->altura <= 0) m += 60000;
  if (!cabeNoTeto(s)) m += 1000000;
  // FORA DE CACHE NO DEBRID vai para depois das cacheadas, e tambem nao sai
  // da fila: o automatico prefere o que TOCA AGORA. Registro 1163 (1.3.12,
  // AIOStreams+TorBox): a verificacao aceitou "⏳ FHD", o link do AIOStreams
  // manda o TorBox baixar e devolve um clipe de aviso de 8 s — que a
  // verificacao nao tem como distinguir de filme. Com a marca do proprio
  // addon, a cacheada da mesma lista vem antes. Menor que o teto (1000000):
  // uma cacheada acima do teto ainda perde para uma fora de cache dentro dele,
  // como ja perdia para qualquer fonte dentro dele.
  if (s->foraCache) m += 500000;
  // P2P do servidor de streaming: o fim da fila. O automatico nem chega a
  // toca-lo (nao ha debrid que o resolva), mas a ORDEM da folha tambem conta:
  // link direto primeiro, torrent sem garantia de peers por ultimo.
  if (soP2P(s)) m += 600000;
  // ORIGEM, depois de qualidade/HDR/cache (R9): o plugin so ganha de uma fonte
  // de addon que esta FORA do cache (-500000, que so abre um aviso de 8 s) ou
  // que estoura o teto; de qualquer fonte de addon em cache, de qualquer
  // altura, ele perde. Sem outra opcao ele toca como sempre.
  if (ehPlugin(s)) m += 200000;
  // MEDIDA PESADA PARA A CONEXAO: abaixo de toda fonte normal (4K DV MP4
  // inteiro soma 137 mil), acima do plugin. So com medida confiavel.
  else if (ajustes_fonte_prioridade() != 1 && fitPesadaAuto(s)) m += 140000;
  return m;
}

static long pontos(const Stream *s) {
  long p = 0;
  // DOLBY VISION SO VALE PONTO EM MP4 — e isto e medida, nao teoria.
  //
  // Marcado no aparelho do dono (LG C9, webOS 4.10) tocando um MKV que o addon
  // anunciava como DV:
  //   hdr do pipeline: HDR10 (fonte DV=1)
  // A TV REBAIXOU para HDR10. E o comportamento ja relatado para Matroska —
  // webOS aciona DV nativo em MP4 e cai para HDR10 em MKV — agora confirmado
  // aqui em vez de citado.
  //
  // O que isso significa na pratica: num perfil 5 a camada base NAO e
  // compativel com HDR10 (e IPT-PQ), entao decodifica-la como HDR10 produz
  // exatamente as cores lavadas que o dono relatou. Preferir a versao DV em
  // MKV era escolher, de proposito, o arquivo que fica PIOR nesta TV.
  //
  // Nao ha como consertar a decodificacao pelo caminho da URI: o Kodi so
  // resolve descartando a camada de realce e reescrevendo o RPU, o que exige
  // demuxar e alimentar o pipeline por buffer — outro projeto, ja registrado em
  // video.c. O que ESTA ao alcance e parar de premiar a fonte que nao serve.
  { int modo = ajustes_fonte_prioridade();
    if (modo == 2) {
      // COMECAR RAPIDO: o que abre depressa na conexao desta TV. Resolucao
      // conta por degrau (nao pula 4K na frente de tudo), e cada GB do arquivo
      // custa 4000, ate 40000: um 1080p de 4 GB passa na frente de um 4K de
      // 60 GB. Cache e origem seguem decidindo antes (multas abaixo) e o
      // StreamFit rebaixa a fonte que a medida confiavel diz que nao cabe. O
      // formato so desempata (Preferir +, Evitar -).
      long gb = (long)(s->tamanhoBytes >> 30);
      p += degrauRes(s) * 10000L + s->altura / 10;
      if (s->mp4) p += 300;
      p -= gb > 10 ? 40000 : gb * 4000;
      { int pref = ajustes_fonte_hdr(), h = nivelHdr(s);
        if (pref == 0) p += h * 50; else if (pref == 2 && h) p -= 50; }
    } else p += qualidade(s, modo);
  }
  p -= multasDe(s);
  return p;
}

long stream_pontos(const Stream *s) { return s ? pontos(s) : 0; }

// LG, AUTO-PLAY: MP4 PRIMEIRO NA MESMA FAIXA DE RESOLUCAO (pedido do dono,
// 2.0.3, C9). Com auto-play ligado, HDR em "Preferir" e Dolby Vision ligado, o
// MP4 vem antes de MKV e outros DA MESMA faixa (degrauRes) — ate de DV em MKV:
// MP4 DV > MP4 HDR > MP4 SDR > o resto pela regra de sempre. Na LG o MP4 abre
// direto no player da TV, com o DV dela; o MKV DV depende do nosso demux.
//   * nunca desce de resolucao: so reordena dentro da faixa, e o MP4 sobe so
//     ate logo acima da melhor nao-MP4 da faixa (um 4K MKV que ganhava de toda
//     a faixa 1080p continua ganhando de um 1080p MP4);
//   * as regras de auto-play (grupo do fonteregra) e as multas (cache, origem,
//     teto, StreamFit) ficam acima: so fontes do mesmo grupo e com as mesmas
//     multas entram na comparacao;
//   * Samsung (.tpk/.wgt), Android e o desktop Linux: nada muda.
static int lgMp4Primeiro(void) {
#if NV_STREAMS_LG
  return !ajustes_fonte_manual() && ajustes_fonte_hdr() == 0 && ajustes_dolby_vision();
#else
  return 0;
#endif
}
// 0 = nao e MP4; 1 MP4 SDR, 2 MP4 HDR, 3 MP4 com DV que esta TV toca.
static int classeMp4(const Stream *s) {
  int h;
  if (!stream_e_mp4(s)) return 0;
  h = nivelHdr(s);
  return h == 4 ? 3 : h > 0 ? 2 : 1;
}
// `pts` (na ordem de `idx`, ou de ORD(q) com idx NULL) e ajustado no lugar:
// em cada (grupo, faixa, multas) com alguma nao-MP4 elegivel, os MP4 sobem
// para logo acima da melhor delas, em ordem de classe e depois de pontos. So
// sobe, nunca desce. `grp` e `excl` podem ser NULL. Devolve quantos subiram.
static int mp4PrimeiroNaFaixa(const int *idx, long *pts, const signed char *grp,
                              const unsigned char *excl, int total) {
  int q, r, subiram = 0;
  long *orig;
  if (total < 2) return 0;
  // A comparacao e sobre os pontos de ANTES: o MP4 que ja subiu nao muda o
  // degrau dos outros.
  orig = malloc(sizeof *orig * (size_t)total);
  if (!orig) return 0;
  memcpy(orig, pts, sizeof *orig * (size_t)total);
  for (q = 0; q < total; q++) {
    const Stream *s = &lista[idx ? idx[q] : ORD(q)];
    int g = grp ? grp[q] : 0, d = degrauRes(s), c = classeMp4(s), acima = 0;
    long mq = multasDe(s), teto = 0, novo;
    int temNao = 0;
    if (!c || (excl && excl[q]) || g < 0) continue;
    for (r = 0; r < total; r++) {
      const Stream *o = &lista[idx ? idx[r] : ORD(r)];
      int co;
      if (r == q || (excl && excl[r]) || (grp ? grp[r] : 0) != g || degrauRes(o) != d ||
          multasDe(o) != mq) continue;
      co = classeMp4(o);
      if (!co) { if (!temNao || orig[r] > teto) teto = orig[r]; temNao = 1; continue; }
      // Outro MP4 da mesma faixa: quantos ficam ABAIXO deste (classe, pontos,
      // e a ordem da lista no empate) decide o degrau dele acima da nao-MP4.
      if (co < c || (co == c && (orig[r] < orig[q] || (orig[r] == orig[q] && r > q)))) acima++;
    }
    if (!temNao) continue;
    novo = teto + 1 + acima;
    if (novo > orig[q]) { pts[q] = novo; subiram++; }
  }
  free(orig);
  return subiram;
}
static const char *nomeFaixa(const Stream *s) {
  static const char *const N[] = { "SD", "720p", "1080p", "2160p" };
  return N[degrauRes(s)];
}

// QUAIS FONTES O AUTOMATICO PODE TOCAR, E EM QUE ORDEM DE GRUPO (#202). A
// regra e de fonteregra.h; aqui so o texto que a regex le (o mesmo do
// oficial: addon, nome, titulo/descricao e url) e um cache por indice, porque
// autoParcialPronto pergunta a cada quadro e a regex custa. O cache cai com a
// lista nova (geracao), com a regra mudada (versao) ou com Ajustes mudados.
static FonteRegraCfg regraCfg(void) {
  FonteRegraCfg c;
  c.escopo = ajustes_fonte_escopo();
  c.regexModo = ajustes_fonte_regex_modo();
  c.usarOutros = ajustes_fonte_usar_outros();
  return c;
}
static pthread_mutex_t grupoTrava = PTHREAD_MUTEX_INITIALIZER;
static signed char *grupoCache;
static int grupoCacheN, grupoCacheCap;
static unsigned grupoCacheGer, grupoCacheVer;
static FonteRegraCfg grupoCacheCfg;
static int grupoDaFonte(const FonteRegraCfg *c, const Stream *s) {
  char t[sizeof s->provedor + sizeof s->rotulo + sizeof s->descricao + sizeof s->arquivo + 1200];
  snprintf(t, sizeof t, "%s %s %s %s %.1024s %s", s->provedor, s->rotulo, s->descricao,
           s->arquivo, s->url, s->infoHash);
  return fonteregra_grupo(c, s->provedor, ehPlugin(s), t);
}
// Chamar com a lista estavel (verTrava, ou o fio da tela que e quem a troca).
static int grupoDe(int i) {
  FonteRegraCfg c = regraCfg();
  unsigned v = fonteregra_versao();
  int g;
  if (i < 0 || i >= n) return -1;
  pthread_mutex_lock(&grupoTrava);
  if (grupoCacheGer != listaGeracao || grupoCacheVer != v ||
      memcmp(&grupoCacheCfg, &c, sizeof c)) {
    grupoCacheN = 0; grupoCacheGer = listaGeracao; grupoCacheVer = v; grupoCacheCfg = c;
  }
  if (i >= grupoCacheN) {
    if (n > grupoCacheCap) {
      signed char *nv = realloc(grupoCache, (size_t)n);
      if (!nv) { pthread_mutex_unlock(&grupoTrava); return grupoDaFonte(&c, &lista[i]); }
      grupoCache = nv; grupoCacheCap = n;
    }
    while (grupoCacheN < n) { grupoCache[grupoCacheN] = (signed char)grupoDaFonte(&c, &lista[grupoCacheN]); grupoCacheN++; }
  }
  g = grupoCache[i];
  pthread_mutex_unlock(&grupoTrava);
  return g;
}
int stream_grupo_regra(int i) { return grupoDe(i); }
// A ultima escolha nao achou candidata SO por causa das regras (#202): ha
// fonte nao excluida, mas nenhuma e permitida. app.c abre a folha de fontes,
// como o oficial faz quando o auto-play nao acha nada.
static volatile int regraBloqueou;
int stream_regra_bloqueou(void) { return regraBloqueou; }
static int pendenteGrupoCb(const char *nome, int plugin, void *u) {
  return fonteregra_grupo_pendente((const FonteRegraCfg *)u, nome, plugin);
}
// A posicao, na ordem dos add-ons, do melhor add-on que ainda nao respondeu
// (FR_ORDEM_SEM = fora da ordem; quem nunca toca por regra nao conta).
static int pendenteRankCb(const char *nome, int plugin, void *u) {
  if (fonteregra_grupo_pendente((const FonteRegraCfg *)u, nome, plugin) < 0) return -1;
  return fonteregra_ordem_rank(nome);
}
int  stream_cabe_no_teto(const Stream *s) { return s ? cabeNoTeto(s) : 1; }

// Endereco de aviso e nao de conteudo. Estes dois foram MEDIDOS no aparelho:
// o AIOStreams manda para slate.m3u8/slate.mp4 ("This playback link couldn't be
// verified") quando o link expirou, e o Debridio para downloading.mp4 quando o
// arquivo ainda nao esta em cache no Real-Debrid. Os dois sao MP4 validos de
// ~120s que TOCAM NORMALMENTE — nao ha erro para detectar, so o endereco.
static int enderecoDeAviso(const char *u) {
  return strstr(u, "downloading.mp4") || strstr(u, "/slate") ||
         strstr(u, "slate.mp4") || strstr(u, "slate.m3u8") ? 1 : 0;
}

// PLAYLIST HLS SEM UM SEGMENTO SEQUER e fonte MORTA, e ela passava na
// verificacao.
//
// MEDIDO na LG, canal da FrostView que o dono relatou como "nao toca mais": o
// proxy devolve HTTP 200, `application/vnd.apple.mpegurl`, 98 bytes:
//     #EXTM3U
//     #EXT-X-VERSION:3
//     #EXT-X-TARGETDURATION:2
//     #EXT-X-MEDIA-SEQUENCE:0
//     #EXT-X-PLAYLIST-TYPE:LIVE
// Cabecalho e mais nada — nenhum #EXTINF, nenhum .ts, nenhuma variante. A
// origem (praia13.com) responde 522, ou seja o canal caiu e o proxy passou a
// servir um esqueleto. rede_url_final so pergunta "a URL resolve?", e resolve:
// a fonte era marcada OK, ia para o pipeline, e o webOS respondia
// `errorCode 100 "Playing error"` — que e o sintoma sem nenhuma pista.
// Outro canal do mesmo addon devolve 1585 bytes e toca, entao isto separa
// fonte morta de fonte viva no mesmo servidor.
//
// So para .m3u8: o custo e um GET de poucos KB na fonte que ja ia ser usada, e
// so acontece na candidata que chegou ate aqui. MP4 continua julgado pelo
// endereco, como antes.
static int playlistVazia(const char *url, const char *cabecalhos) {
  char *corpo;
  const char *vetor[8];
  char copia[512];
  long n = 0;
  int vazia, status = 0, nc = 0;
  if (!strstr(url, ".m3u8") && !strstr(url, "m3u8")) return 0;
  // Os cabecalhos que o addon exigiu (behaviorHints.proxyHeaders). Sem eles um
  // CDN que confere Referer devolve 403 — medido: 403 sem, 200 com.
  if (cabecalhos && *cabecalhos) {
    char *l, *ctx = NULL;
    snprintf(copia, sizeof copia, "%s", cabecalhos);
    for (l = strtok_r(copia, "\n", &ctx); l && nc < 7; l = strtok_r(NULL, "\n", &ctx))
      vetor[nc++] = l;
  }
  vetor[nc] = NULL;
  corpo = rede_baixar_st(url, 8, nc ? vetor : NULL, &status);
  if (corpo) n = (long)strlen(corpo);
  if (!corpo) return 0;    // nao baixou: nao e prova de vazia, deixa passar
  // STATUS FORA DE 2xx NAO E PLAYLIST VAZIA — E RECUSA, E O CORPO E A PAGINA DE
  // ERRO.
  //
  // Este ramo e o defeito do relato de 17/09 no alvo Samsung. La a requisicao
  // sai por XHR, e o navegador PROIBE definir Referer, Origin e User-Agent:
  // os cabecalhos acima sao ignorados em silencio e o CDN responde 403. O corpo
  // do 403 (4,5 KB de HTML) voltava como se fosse a playlist, nao tinha
  // #EXTINF, e o canal era marcado "fora do ar" ANTES de o AVPlay tentar —
  // que e quem realmente sabe mandar cabecalho na TV. A tela dizia "nao foi
  // possivel abrir a fonte" sem nunca ter tentado abrir.
  //
  // Deixar passar e o certo: esta funcao existe para descartar canal
  // comprovadamente morto, e 403 nao prova isso.
  if (status && (status < 200 || status >= 300)) {
    printf("[fonte] playlist respondeu HTTP %d, nao julgo (pode ser cabecalho que o player manda)\n",
           status);
    free(corpo);
    return 0;
  }
  // Um segmento (#EXTINF) ou uma variante (#EXT-X-STREAM-INF) bastam. A lista
  // mestre so tem variantes; a de midia so tem segmentos.
  vazia = !strstr(corpo, "#EXTINF") && !strstr(corpo, "#EXT-X-STREAM-INF");
  if (vazia)
    printf("[fonte] playlist sem segmento (%ld B)\n", n);
  free(corpo);
  return vazia;
}

// VERIFICACAO DAS CANDIDATAS: EM SERIE, E SO ATE A PRIMEIRA QUE SERVE — #130.
//
// HISTORICO, para ninguem desfazer por velocidade. Ate a 1.4.3 isto era um
// lote de 4 fios sobre ate 8 candidatas (+ a preferida). A regra de escolha era
// a mesma ("a de maior pontuacao que resolve"), so que os 4 fios saiam JUNTOS
// e cada um, ao terminar, pegava a proxima da fila enquanto a primeira ainda
// nao tinha resposta. Com a primeira sendo um torrent lento no TorBox (cache,
// createtorrent, ate 3 olhadas no mylist, requestdl), os outros tres fios
// passavam por 4, 5, 6... Resultado no painel do relator do #130: SEIS
// arquivos da Lioness carregados para UMA reproducao — e cada um conta na cota
// do mes do servico.
//
// Conferir uma candidata de debrid nao e so perguntar. O GET com Range que
// rede_url_final faz no link de reproducao do AIOStreams e o que manda o
// AIOStreams adicionar o torrent na conta; o infoHash sem url passa por
// debrid_resolver, que faz createtorrent/addMagnet. Nos dois casos a conta da
// pessoa ganha um arquivo.
//
// O QUE A SERIE CUSTA EM TEMPO: nada, quando a primeira serve — o lote tambem
// esperava o veredito da PRIMEIRA DA ORDEM antes de decidir (issue #61). So
// quando ha falhas na frente elas passam a somar em vez de se sobrepor, e a
// falha tipica (fora de cache, aviso, 4xx) responde em segundos.
//
// A fila e montada por fonteauto.c: no modo "Primeira da lista" ela tem UMA
// candidata, e nenhuma outra URL e tocada.
#define VER_MAX  16

// A lista pode ser trocada por stream_definir_lista enquanto uma candidata e
// conferida fora da trava. A geracao diz se o indice ainda e da mesma lista;
// sem ela, a url resolvida de um episodio ia parar na linha de mesmo numero
// do episodio seguinte.
typedef struct { unsigned geracao; _Atomic int abortou; unsigned rodada; int antecipada; } Conferencia;
// A CONFERENCIA DA CORRIDA PARALELA mora no heap, com contagem: os fios de
// fonteparalela que ainda conferem quando ela volta continuam lendo e
// escrevendo nela (c->abortou, c->rodada). Na pilha de stream_primeira_boa, o
// quadro ja estava desfeito — o ASAN pegou SEGV em verificarOuParar no Mac.
// Duas referencias: a de quem pediu e a da corrida (solta pelo ultimo fio).
typedef struct { Conferencia c; _Atomic int refs; } ConfDona;
static void soltarConf(void *u) {
  ConfDona *d = u;   // `c` e o primeiro campo: o ponteiro e o mesmo
  if (atomic_fetch_sub(&d->refs, 1) == 1) free(d);
}

static int resolverUrl(const char *url, const char *cabecalhos, int segundos,
                        char *fim, unsigned tam, char *mime, unsigned mimeTam, long *corpo) {
  const char *vetor[8];
  char copia[512];
  int nc = 0, http = 0;
#ifdef __EMSCRIPTEN__
  int restrito = 0;
#endif
  if (cabecalhos && *cabecalhos) {
    char *l, *ctx = NULL;
    snprintf(copia, sizeof copia, "%s", cabecalhos);
    for (l = strtok_r(copia, "\n", &ctx); l && nc < 7; l = strtok_r(NULL, "\n", &ctx)) {
      vetor[nc++] = l;
#ifdef __EMSCRIPTEN__
      if (!strncasecmp(l, "Referer:", 8) || !strncasecmp(l, "Origin:", 7) ||
          !strncasecmp(l, "User-Agent:", 11)) restrito = 1;
#endif
    }
  }
  vetor[nc] = NULL;
  // O tipo e o tamanho do corpo vem da MESMA sonda: nenhum pedido a mais.
  if (rede_url_final_tipo(url, segundos, nc ? vetor : NULL, fim, tam, &http,
                          mime, mimeTam, corpo)) return 1;
#ifdef __EMSCRIPTEN__
  // XHR nao manda estes cabecalhos; AVPlay manda. A recusa nao prova que a
  // fonte morreu (mesmo contrato de playlistVazia). 5xx nao entram aqui.
  if ((http == 401 || http == 403) && restrito && strlen(url) < tam) {
    memcpy(fim, url, strlen(url) + 1);
    printf("[fonte] sonda HTTP %d com cabecalho controlado pelo navegador: quem decide e o player\n", http);
    return 1;
  }
#endif
  return 0;
}

static int verificarUma(int i, Conferencia *c) {
  char fim[4096], url[4096], cab[512], mime[96], addon[sizeof lista->provedor];
  int fileIdx, ok = 0;
  long corpo = -1;
  char infoHash[48];
  mime[0] = 0;
  pthread_mutex_lock(&verTrava);
  // Copia do que precisa, sob a trava: fora dela `lista[]` pode ser trocada.
  // Lista trocada por baixo: a candidata nao existe mais, e nada adiante dela
  // vale ser conferido — parar aqui e o que nao toca uma URL a toa.
  if (listaGeracao != c->geracao || i >= n) {
    c->abortou = 1;
    pthread_mutex_unlock(&verTrava);
    return 0;
  }
  if (excedeDecoder(&lista[i])) { pthread_mutex_unlock(&verTrava); return 0; }
  snprintf(url, sizeof url, "%s", lista[i].url);
  snprintf(cab, sizeof cab, "%s", lista[i].cabecalhos);
  snprintf(infoHash, sizeof infoHash, "%s", lista[i].infoHash);
  snprintf(addon, sizeof addon, "%s", lista[i].provedor);
  fileIdx = lista[i].fileIdx;
  pthread_mutex_unlock(&verTrava);

  if (!url[0] && infoHash[0]) {
    // Link recem-saido do unrestrict: nao precisa da segunda viagem abaixo.
    if (debrid_resolver(infoHash, fileIdx, url, sizeof url)) {
      pthread_mutex_lock(&verTrava);
      if (listaGeracao == c->geracao && i < n) {
        snprintf(lista[i].url, sizeof lista[i].url, "%s", url);
        ok = 1;
      } else c->abortou = 1;
      pthread_mutex_unlock(&verTrava);
    } else printf("[fonte] %d torrent nao resolveu no debrid\n", i);
  } else if (!url[0]) {
    ok = 0;
  } else if (!resolverUrl(url, cab, 10, fim, sizeof fim, mime, sizeof mime, &corpo)) {
    printf("[fonte] %d nao resolveu\n", i);
  } else if (naovideo_mime(mime, fim, corpo)) {
    // Linha de aviso do addon (doacao, Discord, "indisponivel"): a URL leva a
    // uma pagina, nao a um video. Fica marcada na folha e o automatico passa
    // para a proxima da ordem da pessoa sem abrir o player.
    printf("[fonte] nao e video: %s %s\n", addon[0] ? addon : "?",
           mime[0] ? mime : (corpo >= 0 ? "corpo-minusculo" : "sem-tipo"));
    fflush(stdout);
    pthread_mutex_lock(&verTrava);
    if (listaGeracao == c->geracao && i < n) lista[i].naoVideo = 1;
    pthread_mutex_unlock(&verTrava);
  } else if (enderecoDeAviso(fim)) {
    printf("[fonte] %d e aviso (%.60s)\n", i, fim);
  } else if (playlistVazia(fim, cab)) {
    printf("[fonte] %d tem playlist vazia (canal fora do ar)\n", i);
  } else ok = 1;
  return ok;
}

// A MESMA CONFERENCIA de verificarUma, para uma URL avulsa (fontevolta.c):
// segue os redirecionamentos com um GET de 64 bytes e recusa o endereco de
// aviso do debrid e a playlist sem segmento. 5 s e nao 10: quem chama ja esta
// tocando a URL em paralelo e so quer saber cedo se ela morreu.
int stream_url_serve(const char *url, const char *cabecalhos) {
  char fim[4096], mime[96];
  long corpo = -1;
  if (!url || !*url) return 0;
  if (!resolverUrl(url, cabecalhos, 5, fim, sizeof fim, mime, sizeof mime, &corpo)) return 0;
  if (naovideo_mime(mime, fim, corpo)) return 0;
  if (enderecoDeAviso(fim)) return 0;
  if (playlistVazia(fim, cabecalhos)) return 0;
  return 1;
}

// fonteauto_primeira nao sabe de lista trocada: depois de uma conferencia
// abortada, as candidatas seguintes "falham" sem tocar a rede.
static int verificarOuParar(int i, void *u) {
  Conferencia *c = u;
  int ok;
  if (c->abortou) return 0;
  ok = verificarUma(i, c);
  // A antecipada pode ter falhado no decoder enquanto esta URL era conferida.
  pthread_mutex_lock(&verTrava);
  if (listaGeracao != c->geracao) c->abortou = 1;
  if (c->abortou || i >= n || excedeDecoder(&lista[i])) ok = 0;
  pthread_mutex_unlock(&verTrava);
  // A candidata que o player ja abriu (fonteantecipa.h): o veredito vai para o
  // fio principal, e um erro do player ANTES dele vale como "nao serviu".
  if (i == c->antecipada) {
    if (ok && fa_player_falhou_foi(c->rodada, i)) {
      ok = 0;
      printf("[fonte] %d ja tinha falhado no player; descartada\n", i);
    }
    fa_concluir(c->rodada, i, ok);
  }
  return ok;
}
// Candidata que nao serviu sai da fila DESTA lista: a proxima escolha (o
// reenvio de tentarProximaFonteVOD em app.c) nao a confere de novo — seria
// mais um arquivo no painel do debrid por uma fonte que ja se sabe ruim.
static void falhouUma(int i, void *u) {
  Conferencia *c = u;
  if (!c->abortou) stream_automatico_excluir(i);
}

// URLs (so o host importa) para aquecer.c abrir a conexao antes: as primeiras
// fontes da lista que o automatico tentaria. Torrent sem link entra pela API do
// debrid, que e o host que a resolucao vai pedir. Nunca resolve nada.
int stream_urls_para_aquecer(char dst[][256], int max) {
  const char *api[4];
  int k, q = 0, a, na = 0, vistas = 0;
  pthread_mutex_lock(&verTrava);
  for (k = 0; k < n && q < max && vistas < 3; k++) {
    int i = ORD(k);
    if (foraDoAuto(i)) continue;
    if (lista[i].url[0]) {
      snprintf(dst[q++], 256, "%s", lista[i].url);
      vistas++;
    } else if (lista[i].infoHash[0] && debrid_ativo()) {
      if (!na) na = debrid_origens(api, 4);
      for (a = 0; a < na && q < max; a++) snprintf(dst[q++], 256, "%s", api[a]);
      vistas++;
    }
  }
  pthread_mutex_unlock(&verTrava);
  return q;
}

// A candidata que o player pode abrir ja (fonteantecipa.h), so se ainda for da
// lista em memoria. -1 = nenhuma.
int stream_antecipada(int *estado) {
  unsigned ger;
  int e = FA_NADA, i = fa_ver(&e, &ger);
  pthread_mutex_lock(&verTrava);
  if (i < 0 || i >= n || ger != listaGeracao) { i = -1; e = FA_NADA; }
  pthread_mutex_unlock(&verTrava);
  if (estado) *estado = e;
  return i;
}

int stream_qtd_torrents(void) {
  int i, q = 0;
  pthread_mutex_lock(&verTrava);
  for (i = 0; i < n; i++) if (!lista[i].url[0] && lista[i].infoHash[0]) q++;
  pthread_mutex_unlock(&verTrava);
  return q;
}

unsigned stream_lista_geracao(void) {
  unsigned g;
  pthread_mutex_lock(&verTrava);
  g = listaGeracao;
  pthread_mutex_unlock(&verTrava);
  return g;
}

int stream_resolver_escolhida(int i, unsigned geracao, char *url, unsigned nu,
                              char *servico, unsigned ns, int *pct) {
  char infoHash[48], fontes[sizeof lista->fontes];
  int fileIdx, r;
  if (url && nu) url[0] = 0;
  if (!url || !nu) return 0;
  pthread_mutex_lock(&verTrava);
  if (listaGeracao != geracao || i < 0 || i >= n) {
    pthread_mutex_unlock(&verTrava);
    return -1;
  }
  // Ja resolvida (pelo automatico, ou por uma escolha anterior desta lista):
  // e a url pronta, sem ir a rede.
  if (lista[i].url[0]) {
    snprintf(url, nu, "%s", lista[i].url);
    pthread_mutex_unlock(&verTrava);
    return 1;
  }
  snprintf(infoHash, sizeof infoHash, "%s", lista[i].infoHash);
  snprintf(fontes, sizeof fontes, "%s", lista[i].fontes);
  fileIdx = lista[i].fileIdx;
  pthread_mutex_unlock(&verTrava);
  if (!infoHash[0]) return 0;

  r = debrid_resolver_escolhido(infoHash, fileIdx, url, nu, servico, ns, pct);
  // SEM DEBRID QUE RESOLVA (sem chave, ou a conta recusou): o servidor P2P
  // experimental, se a pessoa ligou. "Baixando" (2) NAO cai aqui: o debrid ja
  // tem o torrent na conta e o certo e esperar por ele, nao abrir um segundo
  // caminho para o mesmo arquivo.
  if (r == 0 && p2p_ativo()) {
    if (p2p_resolver(infoHash, fileIdx, fontes, url, nu) == P2P_OK) r = 1;
    else { url[0] = 0; r = STREAM_P2P_FALHOU; }
  }
  if (r == 1) {
    pthread_mutex_lock(&verTrava);
    // A url do motor embutido NAO fica na lista: ela morre quando outro
    // torrent e pedido (um por vez) ou o player fecha, e escolher esta fonte
    // de novo tem de passar pelo motor outra vez.
    if (listaGeracao != geracao || i >= n) r = -1;
    else if (!p2pmotor_e_url(url)) snprintf(lista[i].url, sizeof lista[i].url, "%s", url);
    pthread_mutex_unlock(&verTrava);
    if (r < 0) url[0] = 0;
  } else if (r == STREAM_P2P_FALHOU) {
    printf("[fonte] %d torrent escolhido nao abriu no servidor P2P (erro %d)\n", i,
           p2p_ultimo_erro());
  } else if (r == DEBRID_BAIXANDO) {
    printf("[fonte] %d torrent escolhido esta baixando no %s (%d%%)\n", i,
           servico && servico[0] ? servico : "debrid", pct ? *pct : -1);
  } else {
    printf("[fonte] %d torrent escolhido nao resolveu no debrid\n", i);
  }
  return r;
}

// UMA LINHA QUE EXPLICA A ESCOLHA (R9): vencedora, por que ela ficou na frente,
// o orcamento do StreamFit com a confianca dele, e a vice. Em ingles, como o
// resto do log de diagnostico novo. So le a lista; nunca muda a escolha.
static void faixaPontos(const Stream *s, char *dst, size_t tam) {
  static const char *const hdrNome[] = { "SDR", "HDR", "HDR10", "HDR10+", "DV" };
  snprintf(dst, tam, "%dp %s%s%s%s%s", s->altura, hdrNome[nivelHdr(s)], s->mp4 ? " mp4" : "",
           s->foraCache ? " uncached" : " cached", ehPlugin(s) ? " plugin" : " addon",
           !cabeNaAltura(s) ? " over-cap" : foraDaFaixaTamanho(s) == 1 ? " over-size"
           : foraDaFaixaTamanho(s) == 2 ? " under-size" : "");
}
static void logarEscolha(int escolhida, int pref, int modoPrimeira) {
  char a[96], b[96] = "none", fit[96];
  int vice = -1, i, k;
  long pv = 0, pe;
  StreamfitResultado r;
  const Stream *w;
  int foraTam = 0, tamMax = ajustes_tamanho_max_gb(), tamMin = ajustes_tamanho_min_gb();
  char tam[64] = "off";
  pthread_mutex_lock(&verTrava);
  if (escolhida < 0 || escolhida >= n) { pthread_mutex_unlock(&verTrava); return; }
  w = &lista[escolhida];
  for (k = 0; k < n; k++) if (foraDaFaixaTamanho(&lista[k])) foraTam++;
  if (tamMax || tamMin) {
    if (tamMax && tamMin > tamMax) tamMin = 0;   // minimo maior que o maximo e ignorado
    snprintf(tam, sizeof tam, "min=%dGB max=%dGB out-of-range=%d/%d%s", tamMin, tamMax, foraTam, n,
             foraDaFaixaTamanho(w) ? " (winner outside: nothing inside)" : "");
  }
  pe = pontos(w);
  for (k = 0; k < n; k++) {
    long p;
    i = ORD(k);
    if (i == escolhida || foraDoAuto(i)) continue;
    p = pontos(&lista[i]);
    if (vice < 0 || p > pv) { vice = i; pv = p; }
  }
  faixaPontos(w, a, sizeof a);
  if (vice >= 0) faixaPontos(&lista[vice], b, sizeof b);
  r = fitAutoResultado(w);
  if (r.razao == SF_BITRATE_ESTIMADO)
    snprintf(fit, sizeof fit, "%s need=%.0fkbps budget=%dkbps (%d samples)",
             r.classe == SF_PESADA ? "heavy" : "fits", r.necessarioKbps, r.otimoKbps, r.amostras);
  else
    snprintf(fit, sizeof fit, "unknown (%s, quality not downgraded)",
             r.razao == SF_SEM_REDE ? "no network epoch" : r.razao == SF_SEM_TAMANHO ? "no exact size" :
             r.razao == SF_SEM_DURACAO ? "no runtime" : r.razao == SF_SEM_HOST ? "no host" : "no measurement");
  printf("[fonte] auto pick: \"%s\" [%s] points=%ld reason=%s | prefs: priority=%s hdr=%s dv=%s | size=%s | fit=%s | runner-up=",
         w->rotulo, a, pe, escolhida == pref ? "remembered for this title" : modoPrimeira ? "first in addon order"
         : "best score (quality/HDR > cached > addon over plugin)",
         ajustes_fonte_prioridade() == 1 ? "max-quality" : ajustes_fonte_prioridade() == 2 ? "smoothness" : "balanced",
         ajustes_fonte_hdr() == 1 ? "indifferent" : ajustes_fonte_hdr() == 2 ? "avoid" : "prefer",
         ajustes_dolby_vision() ? "on" : "off", tam, fit);
  if (vice >= 0) printf("\"%s\" [%s] points=%ld\n", lista[vice].rotulo, b, pv);
  else printf("none\n");
  pthread_mutex_unlock(&verTrava);
}

// Fonte pronta no debrid para conferir junto com outras: tem link tocavel, o
// addon nao a marcou como fora do cache e nao e torrent. Chamada com verTrava.
static int prontaNoDebrid(int i, void *u) {
  (void)u;
  return i >= 0 && i < n && lista[i].url[0] && !lista[i].foraCache && !soP2P(&lista[i]);
}

int stream_primeira_boa(int tentativas) {
  int fila[VER_MAX], nf, q, tocadas = 0, escolhida, total, pref, livres = 0, kk;
  int modo = ajustes_fonte_primeira() ? FONTEAUTO_PRIMEIRA : FONTEAUTO_MELHOR;
  long *pts;
  unsigned char *acima, *excl;
  signed char *grp;
  int *rk, ordemUso = ajustes_fonte_ordem_uso();
  Conferencia c = { 0, 0, 0, -1 };
  Uint32 tVerif = 0;
  tentativas = fonteauto_tentativas(modo, tentativas);
  pthread_mutex_lock(&verTrava);
  total = n;
  pref = preferida;
  c.geracao = listaGeracao;
  if (total < 1) { pthread_mutex_unlock(&verTrava); return -1; }
  if (tentativas > total) tentativas = total;
  if (tentativas > VER_MAX) tentativas = VER_MAX;
  pts = malloc(sizeof *pts * (size_t)total);
  acima = calloc((size_t)total, 1);
  excl = calloc((size_t)total, 1);
  grp = calloc((size_t)total, 1);
  rk = calloc((size_t)total, sizeof *rk);
  regraBloqueou = 0;
  if (!pts || !acima || !excl || !grp || !rk) {
    pthread_mutex_unlock(&verTrava);
    free(pts); free(acima); free(excl); free(grp); free(rk);
    return -1;
  }
  // A FILA E MONTADA NA ORDEM DE EXIBICAO (#221) e traduzida de volta para
  // indice: o desempate de fonteauto_fila ("o de menor indice, que e a ordem
  // do addon") so vale se a posicao for a da lista inteira, e nao a de chegada.
  { int *ordem = malloc(sizeof *ordem * (size_t)total), posPref = -1;
    FonteRegraCfg cfgEscopo = regraCfg();
    if (!ordem) {
      pthread_mutex_unlock(&verTrava);
      free(pts); free(acima); free(excl); free(grp); free(rk);
      return -1;
    }
    for (q = 0; q < total; q++) {
      int i = ORD(q);
      ordem[q] = i;
      // A lembrada entra na frente da fila, mas nunca fora do escopo: plugin
      // escolhido a mao num dia nao toca sozinho com "so add-ons" (bloqueador
      // 2.0.3).
      if (i == pref && fonteregra_no_escopo(&cfgEscopo, ehPlugin(&lista[i]))) posPref = q;
      pts[q] = pontos(&lista[i]);
      acima[q] = (unsigned char)!cabeNoTeto(&lista[i]);
      excl[q] = (unsigned char)foraDoAuto(i);
      grp[q] = (signed char)grupoDe(i);
      rk[q] = fonteregra_ordem_rank(lista[i].provedor);
      if (!excl[q]) livres++;
    }
    // LG: o MP4 da mesma faixa na frente, a mesma regra do automatico.
    if (lgMp4Primeiro()) mp4PrimeiroNaFaixa(ordem, pts, grp, excl, total);
    pthread_mutex_unlock(&verTrava);
    nf = fonteauto_fila_o(modo, total, posPref, pts, acima, excl, grp, rk, ordemUso, tentativas, fila);
    for (q = 0; q < nf; q++) fila[q] = ordem[fila[q]];
    free(ordem); }
  free(pts); free(acima); free(excl); free(grp); free(rk);
  if (nf < 1) {
    // Havia fonte, mas as regras de Ajustes nao deixam nenhuma (#202).
    if (livres > 0) {
      regraBloqueou = 1;
      printf("[fonte] auto-play: %d fonte(s), nenhuma passa nas regras (permitidos/regex); abrindo a lista\n", livres);
    }
    return -1;
  }

  // TOCAR ENQUANTO CONFERE: a primeira da fila so e aberta no player desde ja
  // quando ja tem link tocavel (nada de torrent a resolver no debrid antes) e
  // o ajuste esta ligado. A conferencia dela segue logo abaixo, a mesma.
  c.rodada = fa_rodada_atual();
  if (ajustes_fonte_tocar_conferindo()) {
    pthread_mutex_lock(&verTrava);
    if (listaGeracao == c.geracao && fila[0] < n && lista[fila[0]].url[0] &&
        !soP2P(&lista[fila[0]]))
      c.antecipada = fila[0];
    pthread_mutex_unlock(&verTrava);
    if (c.antecipada >= 0) fa_publicar(c.rodada, c.antecipada, c.geracao);
  }
  marco("fonte: verificacao inicio");
  tVerif = SDL_GetTicks();
  // CONFERIR VARIAS AO MESMO TEMPO (ligado de fabrica): so as primeiras da fila
  // (ate 3) que ja estao PRONTAS no debrid (nao `foraCache`, nao torrent) sao
  // conferidas juntas, escolhida a primeira que serve NA ORDEM DA FILA
  // (fonteparalela.h). Conferir uma fonte em cache nao baixa nada, entao nao ha
  // arquivo extra no painel; a primeira fora do cache corta a corrida e dai em
  // diante tudo segue em serie, uma por vez (#130).
  kk = 0;
  if (ajustes_fonte_conferir_varias() && nf >= 2 && modo == FONTEAUTO_MELHOR) {
    pthread_mutex_lock(&verTrava);
    if (listaGeracao == c.geracao)
      kk = fonteparalela_prefixo(fila, nf, 3, prontaNoDebrid, NULL);
    pthread_mutex_unlock(&verTrava);
  }
  if (kk >= 2) {
    int t1 = 0, t2 = 0;
    printf("[fonte] conferencia paralela %d\n", kk);
    fflush(stdout);
    ConfDona *d = malloc(sizeof *d);
    if (d) {
      d->c.geracao = c.geracao; d->c.rodada = c.rodada; d->c.antecipada = c.antecipada;
      atomic_init(&d->c.abortou, (int)c.abortou);
      atomic_init(&d->refs, 2);
      escolhida = fonteparalela_soltando(fila, nf, kk, verificarOuParar, falhouUma, &d->c, &t1,
                                         20000, soltarConf);
      tocadas = t1;
      if (escolhida < 0 && !d->c.abortou && nf > kk) {
        escolhida = fonteauto_primeira(fila + kk, nf - kk, verificarOuParar, falhouUma, &d->c, &t2);
        tocadas += t2;
      }
      if (d->c.abortou) c.abortou = 1;
      soltarConf(d);
    } else
      escolhida = fonteauto_primeira(fila, nf, verificarOuParar, falhouUma, &c, &tocadas);
  } else
  escolhida = fonteauto_primeira(fila, nf, verificarOuParar, falhouUma, &c, &tocadas);
  pthread_mutex_lock(&verTrava);
  if (c.abortou || listaGeracao != c.geracao ||
      (escolhida >= 0 && (escolhida >= n || excedeDecoder(&lista[escolhida])))) escolhida = -1;
  pthread_mutex_unlock(&verTrava);
  marco(escolhida >= 0 ? "fonte: verificacao ok" : "fonte: verificacao sem resultado");
  // QUANTO CUSTOU A VERIFICACAO, DEBRID INCLUIDO (#202): o log so tinha a conta
  // das candidatas. Entre a decisao e o primeiro quadro e a maior fatia do
  // inicio (3,8 a 7,4 s de mediana no D1) e nao se separava.
  printf("[fonte] verificacao/debrid em %u ms (%d candidata(s))\n",
         (unsigned)(SDL_GetTicks() - tVerif), tocadas);
  printf("[fonte] verificacao (%s): %d de %d candidata(s) conferida(s)%s\n",
         modo == FONTEAUTO_PRIMEIRA ? "primeira da lista" : "melhor fonte",
         tocadas, nf, c.abortou ? ", lista trocada no meio" : "");
  if (escolhida >= 0) { printf("[fonte] %d ok\n", escolhida); logarEscolha(escolhida, pref, modo == FONTEAUTO_PRIMEIRA); }
  { FonteRegraCfg c = regraCfg();
    if (escolhida >= 0 && (fonteregra_ativa(&c) || ordemUso)) {
      static const char *const ESC_[3] = { "all", "addons-only", "plugins-only" };
      static const char *const RX[3] = { "off", "require", "prefer" };
      static const char *const GR[4] = { "allowed+match", "allowed", "other+match", "other" };
      int g = grupoDe(escolhida);
      static const char *const OU[3] = { "off", "tie-break", "strict" };
      printf("[fonte] auto-play rules: order=%s(%d) scope=%s addons=%d plugins=%d regex=%s(%s) others=%s -> winner %s\n",
             OU[ordemUso], fonteregra_ordem_n(), ESC_[c.escopo], fonteregra_n(0), fonteregra_n(1), RX[c.regexModo],
             fonteregra_regex_estado() > 0 ? "ok" : fonteregra_regex_estado() < 0 ? "invalid, ignored" : "empty",
             c.usarOutros ? "on" : "off", g >= 0 && g < 4 ? GR[g] : "remembered");
    } }
  return escolhida;
}

// BOA O SUFICIENTE PARA NAO ESPERAR O RESTO (#221): dentro do teto, em cache
// no debrid, com link (nao P2P) e na resolucao do teto — 4K quando o teto e
// "Automatica". E a faixa de cima da pontuacao: um addon que ainda nao
// respondeu so passaria na frente dela com MP4/Dolby Vision/Atmos, que sao
// desempates dentro da mesma resolucao.
static int boaParaJa(const Stream *s) {
  int teto = alturaMax();
  if (!cabeNoTeto(s) || s->foraCache || soP2P(s)) return 0;
  return s->altura >= (teto ? teto : 2160);
}

static int pendenteAntesCb(int addon, void *u) { (void)u; return addons_pendente_antes(addon); }

int stream_auto_pode_decidir(int preferida, int prefPendente, int prazoPassou, int instantaneo) {
  FonteautoParcial p;
  long *pts; unsigned char *acima, *excl, *boa; int *ad;
  signed char *grp;
  int *rk;
  int q, total, r, posPref = -1;
  FonteRegraCfg rc = regraCfg();
  memset(&p, 0, sizeof p);
  pthread_mutex_lock(&verTrava);
  total = n;
  if (total < 1) { pthread_mutex_unlock(&verTrava); return 0; }
  pts = malloc(sizeof *pts * (size_t)total);
  acima = calloc((size_t)total, 1); excl = calloc((size_t)total, 1);
  boa = calloc((size_t)total, 1); ad = malloc(sizeof *ad * (size_t)total);
  grp = calloc((size_t)total, 1);
  rk = calloc((size_t)total, sizeof *rk);
  if (!pts || !acima || !excl || !boa || !ad || !grp || !rk) {
    pthread_mutex_unlock(&verTrava);
    free(pts); free(acima); free(excl); free(boa); free(ad); free(grp); free(rk);
    return 0;
  }
  for (q = 0; q < total; q++) {
    int i = ORD(q);
    if (i == preferida) posPref = q;
    pts[q] = pontos(&lista[i]);
    acima[q] = (unsigned char)!cabeNoTeto(&lista[i]);
    excl[q] = (unsigned char)foraDoAuto(i);
    boa[q] = (unsigned char)boaParaJa(&lista[i]);
    ad[q] = chave ? (int)(chave[i] >> 16) : 0;
    grp[q] = (signed char)grupoDe(i);
    rk[q] = fonteregra_ordem_rank(lista[i].provedor);
  }
  if (lgMp4Primeiro()) mp4PrimeiroNaFaixa(NULL, pts, grp, excl, total);
  pthread_mutex_unlock(&verTrava);
  p.modo = ajustes_fonte_primeira() ? FONTEAUTO_PRIMEIRA : FONTEAUTO_MELHOR;
  p.total = total; p.preferida = posPref; p.prefPendente = prefPendente;
  p.prazoPassou = prazoPassou; p.algumPendente = addons_faltam_decisivos() > 0;
  p.pontos = pts; p.acimaTeto = acima; p.excluida = excl; p.boa = boa;
  p.addon = ad; p.pendenteAntes = pendenteAntesCb;
  p.grupo = grp; p.instantaneo = instantaneo;
  p.pendenteGrupoMin = addons_pendente_grupo_min(pendenteGrupoCb, &rc);
  p.rank = rk; p.ordemUso = ajustes_fonte_ordem_uso();
  p.pendenteRankMin = p.ordemUso == FR_ORDEM_ESTRITA ? addons_pendente_grupo_min(pendenteRankCb, &rc) : 99;
  r = fonteauto_pode_decidir(&p);
  free(pts); free(acima); free(excl); free(boa); free(ad); free(grp); free(rk);
  return r;
}

int stream_n_candidatas(void) {
  int i, k = 0;
  pthread_mutex_lock(&verTrava);
  for (i = 0; i < n; i++) if (!foraDoAuto(i)) k++;
  pthread_mutex_unlock(&verTrava);
  return k;
}

// A PRIMEIRA FONTE DE CANAL QUE ESTA VIVA, conferida em paralelo e por
// PLAYLIST, nao por pipeline.
//
// MEDIDO na LG, canal da FrostView com seis candidatas mortas: o canal ia
// DIRETO para a primeira da lista e o watchdog de app.c dava 12 s a cada uma
// antes de passar para a proxima — 12, 24, 36, 48, 60 s no log, quase dois
// minutos ate tocar. E o prazo nao pode ser curto: 12 s e o que um canal VIVO
// leva para abrir num 4K pesado.
//
// O barato aqui e que uma fonte morta se denuncia em MEIO SEGUNDO: o proxy
// responde 200 com uma playlist de 98 bytes, cabecalho e nenhum segmento (ver
// playlistVazia). Entao, em vez de esperar o pipeline falhar, pergunta-se a
// playlist — as candidatas em paralelo, e a primeira viva vai para o player.
//
// ORDEM DA LISTA, e nao pontuacao: para canal ao vivo o addon ja manda
// FHD/HD/SD ordenado, e essa ordem e o ranking dele. (O caminho de filme, em
// stream_primeira_boa, continua por pontuacao.)
//
// NAO chama rede_url_final: o link de canal nao passa por debrid nem por
// redirecionamento de expiracao, e aquela chamada custa outro pedido com
// timeout de 10 s. Aqui o unico pedido e o da propria playlist, com 5 s.
// 8 fios e nao 4: a conferencia inteira precisa caber numa rodada so, senao a
// segunda leva outro CANAL_PRAZO_S. Sao pedidos de poucos KB.
#define CANAL_FIOS   8
// 3 s. MEDIDO na LG: canal vivo devolve a playlist em 0,5 s; o proxy que
// pendurou nao devolve em 5, 12 nem 30 (curl do proprio aparelho: `http=000`,
// zero byte). Esperar mais so adia o inevitavel — e este prazo entra INTEIRO no
// tempo que a pessoa fica olhando para a tela preta quando o canal esta fora.
#define CANAL_PRAZO_S 3
static int canalProx, canalN;
// Classe de cada candidata: 0 = nao conferida ainda, 1 = VIVA (playlist com
// segmento), 2 = MORTA (respondeu sem segmento), 3 = MUDA (nao respondeu),
// 4 = INCERTA (respondeu com corpo grande que nao e playlist — ver fioCanal).
static unsigned char *canalClasse;
static int classeEscolhida;
// A FILA INTEIRA, e nao so a primeira. MEDIDO na C9 em 25/09 (HBO Mundi,
// FrostView, 7 fontes): a sonda deu 2 "mortas" (a 4K entre elas) e 5 mudas,
// escolheu a 1 e o watchdog seguia `canalFonteIdx + 1` — 1, 2, 3... e a 0,
// que era a UNICA que abria (11,8 s ate loadCompleted, escolhida a mao na
// folha), nunca entrava. Agora a sonda deixa a ordem de tentativa pronta e o
// watchdog anda por ela; a morta vai para o fim, mas vai.
#define CANAL_ORDEM_MAX 64
static int canalOrdem[CANAL_ORDEM_MAX], canalOrdemN;
static unsigned char canalOrdemClasse[CANAL_ORDEM_MAX];
static unsigned canalOrdemGeracao;
static int canalInformativa;
static pthread_mutex_t canalTrava = PTHREAD_MUTEX_INITIALIZER;

// Acima disto, resposta sem segmento e INCERTA, nao morta (ver fioCanal).
#define CANAL_MORTA_MAX_B 4096

// So o TIPO do corpo, para o log — nunca o conteudo: playlist de canal traz
// usuario e senha do provedor nas urls dos segmentos.
static const char *tipoCorpo(const char *c) {
  if (strstr(c, "#EXTM3U")) return "m3u";
  if (strstr(c, "<MPD") || strstr(c, "<mpd")) return "mpd";
  if (strstr(c, "<html") || strstr(c, "<HTML") || strstr(c, "<!DOCTYPE") ||
      strstr(c, "<!doctype")) return "html";
  if (c[0] == '{' || c[0] == '[') return "json";
  return "outro";
}

static void *fioCanal(void *u) {
  (void)u;
  for (;;) {
    int meu;
    char *corpo;
    long n = 0;
    pthread_mutex_lock(&canalTrava);
    if (canalProx >= canalN) { pthread_mutex_unlock(&canalTrava); return NULL; }
    meu = canalProx++;
    pthread_mutex_unlock(&canalTrava);
    if (!lista[meu].url[0]) { canalClasse[meu] = 2; continue; }
    { const char *vetor[8];
      char copia[512];
      int nc = 0, status = 0;
      if (lista[meu].cabecalhos[0]) {
        char *l, *ctx = NULL;
        snprintf(copia, sizeof copia, "%s", lista[meu].cabecalhos);
        for (l = strtok_r(copia, "\n", &ctx); l && nc < 7; l = strtok_r(NULL, "\n", &ctx))
          vetor[nc++] = l;
      }
      vetor[nc] = NULL;
      corpo = rede_baixar_st(lista[meu].url, CANAL_PRAZO_S, nc ? vetor : NULL, &status);
      if (corpo) n = (long)strlen(corpo);
      // RECUSA NAO E MORTE, QUANDO O ADDON PEDIU CABECALHO.
      //
      // Este e o caminho do relato de 17/09: canal aparece no guia e some ao
      // abrir, com "nao foi possivel abrir a fonte". Medido contra o addon do
      // relator: o CDN responde 403 sem Referer/Origin/User-Agent e 200 com
      // eles. O corpo do 403 — 4,5 KB de HTML — chegava aqui, nao tinha
      // #EXTINF, e o canal virava classe 2 (MORTA). Todas mortas, nenhuma para
      // tocar, erro na tela.
      //
      // No alvo Samsung isto nao se resolve mandando o cabecalho daqui: a
      // requisicao sai por XHR e o navegador PROIBE definir Referer, Origin e
      // User-Agent — sao cabecalhos controlados pelo agente. Quem sabe manda-los
      // na TV e o AVPlay, que e nativo. Ou seja, esta sonda NAO E AUTORIDADE
      // sobre esta fonte: ela responde 403 para nos e 200 para o player.
      //
      // Entao, quando o addon DECLAROU cabecalhos e a resposta foi recusa, a
      // fonte entra como VIVA e quem decide e o player. Sem cabecalho
      // declarado, 403 continua valendo como morta — ali a sonda e o player
      // veem a mesma coisa.
      if (corpo && status >= 400 && lista[meu].cabecalhos[0]) {
        canalClasse[meu] = 1;
        printf("[fonte] %d respondeu HTTP %d, mas o addon exige cabecalho: quem decide e o player\n",
               meu, status);
        free(corpo);
        continue;
      }
    }
    if (!corpo) {
      // MUDA. MEDIDO na LG com o curl do proprio aparelho: estas URLs do proxy
      // do FrostView nao devolvem NADA — `http=000`, 12 s de espera, zero byte.
      // Nao e a rede da casa: outra URL do mesmo host responde 200 em 0,5 s.
      // Fonte que nao entrega a playlist em CANAL_PRAZO_S tambem nao vai
      // entregar segmento ao pipeline, entao ela cai para ultimo recurso — mas
      // NAO e descartada, porque uma rede ruim de verdade se pareceria com isto.
      canalClasse[meu] = 3;
      printf("[fonte] %d muda: playlist nao respondeu em %ds\n", meu, CANAL_PRAZO_S);
    } else if (strstr(corpo, "#EXTINF") || strstr(corpo, "#EXT-X-STREAM-INF")) {
      canalClasse[meu] = 1;
    } else if (n > CANAL_MORTA_MAX_B) {
      // GRANDE DEMAIS PARA SER A MORTA CONHECIDA. A morta medida e uma
      // playlist de 98 B, cabecalho e nada. Na C9 em 25/09 a 4K da HBO Mundi
      // devolveu 71751 B sem #EXTINF a sonda e TOCOU quando escolhida a mao
      // (relay que ainda estava aquecendo, ou formato que a sonda nao le).
      // Corpo grande sem marca de HLS nao prova nada: quem decide e o player.
      canalClasse[meu] = 4;
      printf("[fonte] %d incerta: %ld B sem segmento (%s); quem decide e o player\n",
             meu, n, tipoCorpo(corpo));
    } else {
      canalClasse[meu] = 2;
      printf("[fonte] %d morta: playlist com %ld B e nenhum segmento (%s)\n", meu, n,
             tipoCorpo(corpo));
    }
    free(corpo);
  }
}

// Altura de uma fonte de canal: a que o parser leu (2160/1080/720) ou a MARCA
// do rotulo/descricao (FHD, HD, SD), que e como as listas de canal dizem.
static int alturaCanal(const Stream *s) {
  int a = s->altura;
  if (!a) a = nv_res_do_texto(s->rotulo);
  if (!a) a = nv_res_do_texto(s->descricao);
  return a;
}

int stream_canal_primeira_viva(int tentativas) {
  int total = stream_n(), q, criados = 0, escolhida = -1;
  pthread_t fios[CANAL_FIOS];
  if (total < 1) return -1;
  if (tentativas < 1 || tentativas > total) tentativas = total;
  canalClasse = calloc((size_t)tentativas, 1);
  if (!canalClasse) return -1;
  marco("canal: conferindo playlists");
  canalProx = 0; canalN = tentativas;
  for (q = 0; q < CANAL_FIOS && q < tentativas; q++)
    if (pthread_create(&fios[criados], NULL, fioCanal, NULL) == 0) criados++;
  if (!criados) fioCanal(NULL);
  for (q = 0; q < criados; q++) pthread_join(fios[q], NULL);

  // PREFERENCIA POR CLASSE, e dentro da classe pela ORDEM DO ADDON — que para
  // canal ao vivo e o ranking dele (FHD/HD/SD). Viva ganha de muda; muda ganha
  // de nada. Morta nao e escolhida: ela JA respondeu sem segmento — mas fica
  // no fim da fila do watchdog (canalOrdem), porque "morta" pela sonda ja
  // tocou pelo player.
  // Ordem de preferencia: viva dentro do teto, viva acima do teto, muda dentro
  // do teto, muda. O teto nunca tira a ultima fonte da mesa.
  // A mesma regra monta a FILA INTEIRA: viva, incerta, muda (cada uma dentro
  // do teto antes de acima dele), a morta por ultimo e, depois das conferidas,
  // as que ficaram fora da sonda na ordem do addon. A escolhida e a cabeca da
  // fila, se nao for morta.
  // RESOLUCAO PRINCIPAL (Ajustes > Live TV): dentro de cada classe e de cada
  // lado do teto, a fonte da resolucao escolhida vem antes das outras — a
  // classe continua mandando (uma viva em HD ganha de uma muda em FHD).
  { static const unsigned char ordemClasse[] = { 1, 4, 3, 2 };
    int c, t, pr, alvo = nv_res_opcao_altura(ajustes_livetv_resolucao());
    canalOrdemN = 0;
    for (c = 0; c < 4; c++)
      for (t = 1; t >= 0; t--)
        for (pr = 1; pr >= 0; pr--)
          for (q = 0; q < tentativas && canalOrdemN < CANAL_ORDEM_MAX; q++)
            if (canalClasse[q] == ordemClasse[c] && cabeNoTeto(&lista[q]) == t &&
                nv_res_preferida(alturaCanal(&lista[q]), alvo) == pr) {
              canalOrdemClasse[canalOrdemN] = canalClasse[q];
              canalOrdem[canalOrdemN++] = q;
            }
    for (q = tentativas; q < total && canalOrdemN < CANAL_ORDEM_MAX; q++) {
      canalOrdemClasse[canalOrdemN] = 0;
      canalOrdem[canalOrdemN++] = q;
    }
    pthread_mutex_lock(&verTrava);
    canalOrdemGeracao = listaGeracao;
    pthread_mutex_unlock(&verTrava);
    canalInformativa = 0;
    for (q = 0; q < tentativas; q++) if (canalClasse[q] == 1) canalInformativa = 1;
    if (canalOrdemN > 0 && canalOrdemClasse[0] != 2) escolhida = canalOrdem[0];
  }
  if (escolhida >= 0 && !cabeNoTeto(&lista[escolhida]))
    printf("[fonte] canal: nenhuma fonte dentro do teto de %dp; usando %dp\n",
           alturaMax(), lista[escolhida].altura);
  { int vivas = 0, mortas = 0, mudas = 0;
    int incertas = 0;
    for (q = 0; q < tentativas; q++) {
      if (canalClasse[q] == 1) vivas++;
      else if (canalClasse[q] == 2) mortas++;
      else if (canalClasse[q] == 3) mudas++;
      else if (canalClasse[q] == 4) incertas++;
    }
    printf("[fonte] canal: %d viva(s), %d incerta(s), %d morta(s), %d muda(s) de %d; escolhida %d\n",
           vivas, incertas, mortas, mudas, tentativas, escolhida); }
  marco(escolhida >= 0 ? "canal: fonte escolhida por playlist"
                       : "canal: nenhuma playlist utilizavel");
  // A CLASSE DA ESCOLHIDA fica disponivel para quem chamou: uma fonte MUDA que
  // entrou por falta de opcao nao merece o mesmo prazo de uma viva. Ver
  // stream_canal_classe_escolhida e o watchdog em app.c.
  classeEscolhida = (escolhida >= 0) ? canalClasse[escolhida] : 0;
  free(canalClasse); canalClasse = NULL;
  return escolhida;
}

int stream_canal_classe_escolhida(void) { return classeEscolhida; }

static int ordemValida(void) {
  int ok;
  pthread_mutex_lock(&verTrava);
  ok = canalOrdemN > 0 && canalOrdemGeracao == listaGeracao;
  pthread_mutex_unlock(&verTrava);
  return ok;
}

int stream_canal_proxima(int atualIdx) {
  int q;
  if (ordemValida())
    for (q = 0; q < canalOrdemN; q++)
      if (canalOrdem[q] == atualIdx) return q + 1 < canalOrdemN ? canalOrdem[q + 1] : -1;
  // Sem fila (lista que nao passou pela sonda, ou indice fora dela): a ordem
  // do addon, como sempre foi.
  return atualIdx + 1 < stream_n() ? atualIdx + 1 : -1;
}

int stream_canal_prazo_longo(int idx) {
  int q;
  // Sonda que nao achou NENHUMA viva nao informou nada: todas mudas/incertas
  // e o que se ve quando o relay e lento (HBO Mundi, 25/09: curl 28 a 3 s em
  // todas, e a 4K abriu em 11,8 s). Ai o prazo curto so garantia a falha.
  if (!ordemValida() || !canalInformativa) return 1;
  for (q = 0; q < canalOrdemN; q++)
    if (canalOrdem[q] == idx) return canalOrdemClasse[q] == 1 || canalOrdemClasse[q] == 4;
  return 1;
}

// LINHA INFORMATIVA DO ADDON, nao filme. Log da 2.0.0 (Samsung): a lista
// trouxe so "✨ | support the project!" e "Note: Start...", o automatico
// escolheu a primeira (points=0) quatro vezes e o player tentou abrir um link
// de doacao. Fica na folha (a pessoa pode querer abrir), sai do automatico.
// So sem altura e sem tamanho: um filme de verdade com "support" no nome tem
// pelo menos um dos dois.

static int canalFolha;
static unsigned lgMp4LogGer; static int lgMp4LogIdx = -1;
static int automaticoCom(int regras) {
  if (!stream_n()) return -1;
  int melhor = -1, gMelhor = 0, m = 0;
  long maior = 0;
  int lg = regras && lgMp4Primeiro();
  int *idx = lg ? malloc(sizeof *idx * (size_t)n) : NULL;
  long *pts = lg ? malloc(sizeof *pts * (size_t)n) : NULL;
  signed char *grp = lg ? malloc((size_t)n) : NULL;
  if (lg && (!idx || !pts || !grp)) lg = 0;
  // NA ORDEM DE EXIBICAO (#221), que e a da lista inteira: com a lista
  // enchendo por addon o indice e a ordem de CHEGADA, nao a dos addons.
  for (int k = 0; k < n; k++) {
    int i = ORD(k), g = regras ? grupoDe(i) : 0;
    if (automaticaExcluida(i) || ehInformativa(&lista[i]) ||
        (regras && excedeDecoder(&lista[i])) || g < 0) continue;
    long p = pontos(&lista[i]);
    if (lg) { idx[m] = i; pts[m] = p; grp[m] = (signed char)g; m++; }
    // O GRUPO das regras (#202) vem antes da pontuacao: permitida primeiro.
    // `>` e nao `>=`: em empate fica o PRIMEIRO da lista, que e a ordem em que
    // o addon devolveu — e ele costuma saber algo que a pontuacao nao ve.
    if (melhor < 0 || g < gMelhor || (g == gMelhor && p > maior)) { maior = p; melhor = i; gMelhor = g; }
  }
  // LG: o MP4 da mesma faixa na frente (mp4PrimeiroNaFaixa). Mesma escolha,
  // agora sobre os pontos ajustados.
  if (lg && m > 1 && mp4PrimeiroNaFaixa(idx, pts, grp, NULL, m)) {
    int antes = melhor;
    melhor = -1;
    for (int q = 0; q < m; q++)
      if (melhor < 0 || grp[q] < gMelhor || (grp[q] == gMelhor && pts[q] > maior)) {
        maior = pts[q]; melhor = idx[q]; gMelhor = grp[q];
      }
    if (melhor != antes && (lgMp4LogGer != listaGeracao || lgMp4LogIdx != melhor)) {
      lgMp4LogGer = listaGeracao; lgMp4LogIdx = melhor;
      printf("[fonte] LG: mp4 primeiro na faixa %s (autoplay+hdr+dv): \"%s\" no lugar de \"%s\"\n",
             nomeFaixa(&lista[melhor]), lista[melhor].rotulo, lista[antes].rotulo);
      fflush(stdout);
    }
  }
  free(idx); free(pts); free(grp);
  return melhor;
}
// As regras de auto-play (#202) sao de filme e serie, como no oficial: canal
// ao vivo escolhe sem elas (stream_automatico_canal).
// A PROXIMA CANDIDATA DO AUTOMATICO, SE A ATUAL FALHAR, NAO E PIOR? (#202)
// Mesma ordem de automaticoCom, sem a atual. "Pior" = menos resolucao ou sem
// o Dolby Vision que a atual tem. Serve ao watchdog de abertura: trocar de
// fonte por demora so e aceitavel quando nao baixa a qualidade — essa decisao
// e da pessoa, nao de um relogio.
int stream_proxima_sem_perda(int atual) {
  int melhor = -1, gMelhor = 0;
  long maior = 0;
  if (atual < 0 || atual >= n) return 0;
  for (int k = 0; k < n; k++) {
    int i = ORD(k), g = grupoDe(i);
    if (i == atual || foraDoAuto(i) || g < 0) continue;
    long p = pontos(&lista[i]);
    if (melhor < 0 || g < gMelhor || (g == gMelhor && p > maior)) { maior = p; melhor = i; gMelhor = g; }
  }
  if (melhor < 0) return 0;
  return lista[melhor].altura >= lista[atual].altura &&
         lista[melhor].dolbyVision >= lista[atual].dolbyVision;
}
int stream_automatico(void) { return automaticoCom(1); }
int stream_automatico_canal(void) { return automaticoCom(0); }


static int grupo, filtro, soMp4, soCache, soDub;

// BOTOES DO CABECALHO. "Sem HDR" so existe onde ha o que renegociar (webOS);
// ver o bloco "TELA PRETA COM AUDIO TOCANDO" em video.h. Oferecer um botao que
// nao faz nada seria pior que nao oferecer: a pessoa aperta, nada muda, e passa
// a duvidar dos outros. "Só MP4" (#91) filtra a lista — permanece na folha.
// Ordem visivel, da esquerda: [Sem HDR] e MP4 como pilulas com rotulo,
// Recarregar e Fechar como discos de icone (Lucide rotate-cw e x).
//
// "EM CACHE" E "DUBLADO" (dono, 02/10, escolheu entre tres): filtros como o
// MP4, e nao grupos — resolucao x HDR/SDR ja da ate 8 grupos, e mais um eixo
// picotaria a lista em grupos de 1 ou 2 fontes. Em cache = o debrid ja tem o
// arquivo (Stream.foraCache e 0), toca agora; Dublado = audio em portugues.
enum { BT_RECARREGAR, BT_SEM_HDR, BT_SO_MP4, BT_CACHE, BT_DUB, BT_FECHAR };
static int botaoDe(int i) {
  if (!video_pode_forcar_sdr()) i++;
  if (i == 0) return BT_SEM_HDR;
  if (i == 1) return BT_SO_MP4;
  if (i == 2) return BT_CACHE;
  if (i == 3) return BT_DUB;
  if (i == 4) return BT_RECARREGAR;
  return BT_FECHAR;
}
static int nBotoes(void) { return video_pode_forcar_sdr() ? 6 : 5; }
static const char *rotuloBotao(int b) {
  if (b == BT_SEM_HDR) return "Sem HDR";
  if (b == BT_SO_MP4)  return "Só MP4";
  if (b == BT_CACHE)   return "Em cache";
  if (b == BT_DUB)     return "Dublado";
  return NULL;
}
static int botaoLigado(int b) {
  return (b == BT_SO_MP4 && soMp4) || (b == BT_CACHE && soCache) || (b == BT_DUB && soDub);
}
static const char *iconeBotao(int b) {
  return b == BT_RECARREGAR ? "aj_rotate-cw" : b == BT_FECHAR ? "aj_x" : NULL;
}
static char provedores[13][96];
static int nProvedores;

static int temAudioPt(const Stream *s);
// Os chips (MP4, cache, dublado) valem para a lista E para a contagem de cada
// aba; o addon so para a lista.
static int passaChips(int i) {
  if (soMp4 && !lista[i].mp4) return 0;
  if (soCache && lista[i].foraCache) return 0;
  if (soDub && !temAudioPt(&lista[i])) return 0;
  return 1;
}
static int passaFiltro(int i) {
  if (filtro < 0) return 0;
  if (!passaChips(i)) return 0;
  if (filtro && strcmp(lista[i].provedor, provedores[filtro])) return 0;
  return 1;
}

// Abas na ORDEM DE EXIBICAO (a dos addons), e a aba escolhida segue pelo
// NOME: com a lista enchendo (#221) um addon novo pode entrar antes dela.
static void atualizarProvedores(void) {
  char escolhido[96];
  snprintf(escolhido, sizeof escolhido, "%s", filtro > 0 && filtro < nProvedores ? provedores[filtro] : "");
  nProvedores = 1;
  snprintf(provedores[0],sizeof provedores[0],"Todos");
  for (int k=0;k<n;k++) {
    int i=ORD(k), j;
    for(j=1;j<nProvedores;j++) if(!strcmp(provedores[j],lista[i].provedor)) break;
    if(j==nProvedores && nProvedores<13)
      snprintf(provedores[nProvedores++],96,"%s",lista[i].provedor);
  }
  if (escolhido[0]) {
    int j;
    for (j = 1; j < nProvedores; j++) if (!strcmp(provedores[j], escolhido)) break;
    filtro = j < nProvedores ? j : 0;
  }
  if(filtro>=nProvedores) filtro=0;
}
static int nFiltrados(void) {
  if (filtro == -1) return ondever_n(alvoPedido);
  int k=0;
  for(int i=0;i<n;i++) if(passaFiltro(i)) k++;
  return k;
}

// A FONTE QUE O AUTOMATICO ESCOLHERIA. E a resposta a "se eu nao escolher
// nada, o que toca?". A ordem e a mesma de stream_primeira_boa: a lembrada vai
// na frente quando existe; senao, a de maior pontuacao. Pedido do dono, 16/09.
// Uma vez por quadro, nunca por linha: stream_automatico percorre a lista.
static int automaticaDaFolha(void) {
  return preferida >= 0 && !automaticaExcluida(preferida) &&
         (canalFolha || !excedeDecoder(&lista[preferida])) ? preferida : automaticoCom(!canalFolha);
}

// A LISTA AGRUPADA POR RESOLUCAO. A ordem da lista (a pontuacao) vale DENTRO
// de cada grupo; os grupos vao do maior para o menor. `ordem[linha]` e o indice
// em lista[] e e o que o OK escolhe — a navegacao anda nesta ordem, nao na da
// lista, senao a seta desceria por uma linha e o realce apareceria em outra.
//
// As alturas sao por linha: a marca ("Sua escolha anterior") e o nome do
// arquivo (so na linha em foco, abrindo por mola) somam altura. Montado a cada
// quadro em atualizar e em desenhar: n e da ordem de dezenas, e guardar entre
// quadros exigiria invalidar em cada filtro, recarga e troca de lista.
//
// CADA RESOLUCAO SE PARTE EM HDR E SDR (dono, 02/10: "alem de 4K como
// categoria, colocar HDR e SDR"). Grupo = resolucao*2 + (SDR ? 1 : 0), entao a
// ordem fica 4K HDR, 4K SDR, 1080p HDR, 1080p SDR... — numa TV HDR, a fonte
// que liga o modo vem antes da que nao liga, dentro da mesma resolucao. HDR e
// qualquer sinal de imagem estendida: Dolby Vision (inclusive o que so chega
// pelo bit do audio combinado), HDR10+, HDR10, HDR e HLG.
#define FOLHA_RES    4
#define FOLHA_GRUPOS (FOLHA_RES * 2)
static const char *const GRUPO_NOME[FOLHA_RES] = { "4K", "1080p", "720p", "Outras" };
static const char *const GRUPO_SUB[FOLHA_RES]  = { "ULTRA HD", "FULL HD", "HD", "" };
static int ehHdr(const Stream *s) {
  return s->dolbyVision ||
         (s->badges & (badges_bit("v-dv") | badges_bit("v-hdr10plus") | badges_bit("v-hdr10") |
                       badges_bit("v-hdr") | badges_bit("v-hlg") | badges_bit("a-atmos-dv") |
                       badges_bit("a-truehd-dv") | badges_bit("a-dd-dv"))) != 0;
}
// CANAL AO VIVO (dono, 03/10): a folha de Fontes de um canal nao trazia
// resolucao nenhuma — o parser de filme le "1080p" de nome de arquivo, e as
// listas M3U/Xtream dizem "UHD", "FHD", "HD", "SD", "H.265", "50fps" no nome do
// canal. So para canal (stream_folha_canal): a altura vem do texto (rotulo,
// descricao) e os selos de resolucao/codec entram no mesmo caminho do filme,
// entao o grupo e os selos saem iguais. Idempotente. Filme nao passa aqui.
static int canalFolha;
void stream_folha_canal(int sim) { canalFolha = sim != 0; }
void stream_canal_enriquecer(Stream *s) {
  static const char *const HEVC[] = { "hevc", "h265", "h.265", "x265", NULL };
  int a, k;
  if (!s) return;
  a = alturaCanal(s);
  if (!s->altura) s->altura = a;
  if (a >= 1800)      s->badges |= badges_bit("r-4k");
  else if (a >= 1000) s->badges |= badges_bit("r-1080");
  else if (a >= 700)  s->badges |= badges_bit("r-720");
  else if (a > 0)     s->badges |= badges_bit("r-sd");
  for (k = 0; HEVC[k]; k++)
    if (nv_res_tem(s->rotulo, HEVC[k]) || nv_res_tem(s->descricao, HEVC[k])) {
      s->badges |= badges_bit("co-x265"); break;
    }
}
static int grupoRes(const Stream *s) {
  int r;
  if (canalFolha) stream_canal_enriquecer((Stream *)s);
  if (s->altura >= 1800 || (s->badges & badges_bit("r-4k"))) r = 0;
  else if (s->altura >= 1000 || (s->badges & badges_bit("r-1080"))) r = 1;
  else if (s->altura >= 700  || (s->badges & badges_bit("r-720"))) r = 2;
  else r = 3;
  return r * 2 + (ehHdr(s) ? 0 : 1);
}
// A resolucao e a faixa de brilho da PROPRIA fileira (#202): a fonte "Melhor
// para esta TV" fica acima de todos os cabecalhos de grupo e nao dizia a
// resolucao, e o dono pediu o rotulo dentro da linha em foco. Sai como texto
// universal ("4K", "1080p", "DV", "HDR10", "SDR"), sem palavra para traduzir.
static void rotuloQualidade(const Stream *s, char *res, size_t nr, char *faixa, size_t nf, int *hdr) {
  static const char *const RES[FOLHA_RES] = { "4K", "1080p", "720p", "SD" };
  uint64_t b = s->badges;
  if (s->altura <= 0 && !(b & (badges_bit("r-4k") | badges_bit("r-1080") | badges_bit("r-720")))) snprintf(res, nr, "?");
  else snprintf(res, nr, "%s", RES[grupoRes(s) / 2]);
  *hdr = ehHdr(s);
  if (s->dolbyVision || (b & (badges_bit("v-dv") | badges_bit("a-atmos-dv") | badges_bit("a-truehd-dv") | badges_bit("a-dd-dv")))) snprintf(faixa, nf, "DV");
  else if (b & badges_bit("v-hdr10plus")) snprintf(faixa, nf, "HDR10+");
  else if (b & badges_bit("v-hdr10")) snprintf(faixa, nf, "HDR10");
  else if (b & badges_bit("v-hlg")) snprintf(faixa, nf, "HLG");
  else if (*hdr) snprintf(faixa, nf, "HDR");
  else snprintf(faixa, nf, "SDR");
}
static int *ordem;
static float *linhaY, *linhaH;
static int ordemCap, nOrdem;
static float secY[FOLHA_GRUPOS], alturaTotal;
static int secN[FOLHA_GRUPOS];
// A linha em foco abre (abreFoco 0->1) e a que perdeu o foco fecha (abreAnt
// 1->0) ao mesmo tempo: abrir uma e fechar a outra no mesmo quadro empurraria
// a lista inteira de uma vez.
static float abreFoco = 1.0f, abreAnt;
static int linhaAnt = -1, focoVisto = -1;


// MODO "DO ADDON" (Ajustes > Texto das fontes; dono, 02/10: "tem que ter a
// opcao de receber pronto o texto que alguns addons mandam"). O titulo e o
// `name` do addon numa linha so, e embaixo a `description` linha a linha, como
// veio — o AIOStreams do dono manda "11.1 GB | 30.1 Mbps |", o grupo, os
// idiomas e o arquivo, cada um numa linha. Glifo que a Inter nao tem (o ⚡ e o
// ⚑ do formatador) cai fora em text.c, sem virar quadrado.
#define FOLHA_ADDON_LINHAS 5
// Fora do foco a linha mostra so as 2 primeiras linhas do addon; a linha em
// foco abre (mola abreFoco) e mostra todas, ate FOLHA_ADDON_LINHAS. Com 5
// linhas em todas as fontes a lista caberia em 3 telas; assim ela continua
// densa e a que se esta lendo abre inteira.
#define FOLHA_ADDON_FECHADA 2
#define FOLHA_ADDON_LD     28.0f
static void linhaLimpa(char *d, size_t tam, const char *ini, size_t n) {
  size_t k = 0;
  while (n && (*ini == ' ' || *ini == '\t')) { ini++; n--; }
  while (n && (ini[n-1] == ' ' || ini[n-1] == '\t' || ini[n-1] == '\r')) n--;
  for (size_t j = 0; j < n && k + 1 < tam; j++) {
    unsigned char c = (unsigned char)ini[j];
    d[k++] = c < 32 ? ' ' : (char)c;
  }
  d[k] = 0;
}
// Emoji do formatador -> icone Lucide (aj_<nome>.png). A TV nao tem emoji
// colorido; o que nao esta aqui sai limpo em nv_limpar_texto (sem quadrado).
// Um emoji pode vir com o seletor de variacao U+FE0F (EF B8 8F) colado.
static const struct { const char *emoji, *icone; } EMOJI_ICONE[] = {
  { "\xF0\x9F\x92\xBE", "aj_hard-drive" },   // 💾
  { "\xF0\x9F\x91\xA4", "aj_users" },        // 👤
  { "\xF0\x9F\x8E\x9E", "aj_film" },         // 🎞
  { "\xF0\x9F\x94\x8A", "aj_volume-2" },     // 🔊
  { "\xF0\x9F\x8C\x90", "aj_globe" },        // 🌐
  { "\xE2\x9A\x99",     "aj_settings-2" },  // ⚙
  { "\xE2\x9A\xA1",     "aj_zap" },         // ⚡
  { "\xF0\x9F\x93\xA6", "aj_package" },      // 📦
  { "\xF0\x9F\x8F\xB7", "aj_tag" },          // 🏷
  { "\xE2\x8F\xB1",     "aj_clock" },       // ⏱
};
#define ADDON_PECAS 6
typedef struct { int n; const char *ic[ADDON_PECAS]; char tx[ADDON_PECAS][96]; } LinhaAddon;
static const char *emojiIcone(const char *p, size_t *len) {
  for (size_t k = 0; k < sizeof EMOJI_ICONE / sizeof EMOJI_ICONE[0]; k++) {
    size_t n = strlen(EMOJI_ICONE[k].emoji);
    if (strncmp(p, EMOJI_ICONE[k].emoji, n)) continue;
    if (!strncmp(p + n, "\xEF\xB8\x8F", 3)) n += 3;
    *len = n;
    return EMOJI_ICONE[k].icone;
  }
  return NULL;
}
// Uma linha crua do addon vira pecas "[icone] texto". O texto de cada peca
// passa pelo limpador do #144 (versalete, subscrito, glifo que a fonte nao
// tem) antes de ir para a tela, como o resto da folha.
static void pecasAddon(const char *ini, size_t n, LinhaAddon *L) {
  char cru[1024];
  const char *p, *ate;
  size_t len;
  L->n = 0;
  linhaLimpa(cru, sizeof cru, ini, n);
  p = cru;
  ate = cru + strlen(cru);
  while (p < ate && L->n < ADDON_PECAS) {
    const char *ic = NULL, *q = p;
    char seg[512];
    size_t k;
    // O icone (se a peca abre com um) e o texto ate o proximo emoji conhecido.
    if (!(ic = emojiIcone(p, &len))) len = 0;
    q = p + len;
    for (k = 0; q + k < ate; k++) { size_t l2; if (emojiIcone(q + k, &l2)) break; }
    snprintf(seg, sizeof seg, "%.*s", (int)(k < sizeof seg - 1 ? k : sizeof seg - 1), q);
    nv_limpar_texto(seg, L->tx[L->n], sizeof L->tx[0], NV_LIMPA_UMA_LINHA);
    nv_aparar_separadores(L->tx[L->n]);   // "31.4 GB | 43.2 Mbps |" -> sem o | solto
    if (ic || L->tx[L->n][0]) L->ic[L->n++] = ic;
    p = q + k;
  }
}
static int linhasAddon(const Stream *s, LinhaAddon *out, int max) {
  const char *p = s->descricao;
  int nl = 0;
  while (*p && nl < max) {
    const char *f = strchr(p, '\n');
    size_t n = f ? (size_t)(f - p) : strlen(p);
    pecasAddon(p, n, &out[nl]);
    if (out[nl].n) nl++;
    if (!f) break;
    p = f + 1;
  }
  return nl;
}
static void tituloAddon(const Stream *s, char *buf, size_t tam) {
  char t[sizeof s->rotulo], lim[sizeof s->rotulo];
  size_t k = 0;
  int esp = 0;
  for (const char *p = s->rotulo; *p && k + 1 < sizeof t; p++) {
    unsigned char c = (unsigned char)*p;
    if (c < 33) { esp = k > 0; continue; }
    if (esp) { t[k++] = ' '; esp = 0; }
    t[k++] = (char)c;
  }
  t[k] = 0;
  nv_limpar_texto(t, lim, sizeof lim, NV_LIMPA_UMA_LINHA);
  snprintf(buf, tam, "%s", lim[0] ? lim : s->provedor);
}

static int temPalavra(const char *s, const char *p) {
  size_t n = strlen(p);
  for (const char *q = s; *q; q++) {
    if (strncasecmp(q, p, n)) continue;
    if (q > s && isalnum((unsigned char)q[-1])) continue;
    if (isalnum((unsigned char)q[n])) continue;
    return 1;
  }
  return 0;
}
static int temNoTexto(const Stream *s, const char *p) {
  return temPalavra(s->rotulo, p) || temPalavra(s->descricao, p) || temPalavra(s->arquivo, p);
}
// O IDIOMA nao tem logo no pacote de selos, entao vai como texto no fim da
// fileira. Lido por palavra no nome, na descricao e no arquivo.
// Portugues na LISTA DE IDIOMAS do formatador (o "⚑ English | ... |
// Portuguese" do AIOStreams e a lista de AUDIO do arquivo) conta como dual:
// tem a faixa em portugues mesmo sem a palavra "dublado".
static const char *idiomaDa(const Stream *s) {
  if (temNoTexto(s, "dublado") || temNoTexto(s, "dub")) return "Dublado";
  if (temNoTexto(s, "dual") || temNoTexto(s, "portuguese") || temNoTexto(s, "português") ||
      temNoTexto(s, "pt-br") || temNoTexto(s, "ptbr")) return "Dual áudio";
  if (temNoTexto(s, "legendado") || temNoTexto(s, "leg")) return "Legendado";
  return NULL;
}
static int temAudioPt(const Stream *s) {
  const char *id = idiomaDa(s);
  return id && strcmp(id, "Legendado");
}

// O TITULO DA LINHA, modo "Do Nuvio": o NOME DO CONTEUDO (dono, 02/10: "deixar
// as badges embaixo e o nome mesmo em cima — Silo Season 2 Episode 5; se fosse
// filme so o nome do filme"). Toda a qualidade fica nos logos embaixo. O nome
// vem de app.c (stream_folha_nome, junto de cada abertura da folha); temporada
// e episodio vem do ALVO DA LISTA ("tt...:2:5"), que e de que episodio estas
// fontes sao — e nao do episodio em foco no detalhe, que pode ser outro.
static char nomeFolha[160];
static int itemFolha = -1;
void stream_folha_item(int indice) { itemFolha = indice; }
void stream_folha_nome(const char *nome) {
  snprintf(nomeFolha, sizeof nomeFolha, "%s", nome ? nome : "");
}
// Duas partes: o NOME (grande) e o EPISODIO ("Temporada 2 Episodio 5"),
// desenhado menor e mais apagado ao lado (dono, 02/10: "pode deixar menor e
// mais delicado"). Filme e canal ficam so com o nome.
static void tituloConteudo(const Stream *s, char *nome, size_t tn, char *ep, size_t te) {
  const char *c1 = strchr(alvoLista, ':'), *c2 = c1 ? strchr(c1 + 1, ':') : NULL;
  ep[0] = 0;
  // Sem nome (canal, ou quem abriu a folha nao disse), o addon: e o que
  // sobra que diz de onde a linha vem.
  snprintf(nome, tn, "%s", nomeFolha[0] ? nomeFolha : s->provedor);
  if (nomeFolha[0] && c1 && c2 && atoi(c1 + 1) > 0 && atoi(c2 + 1) > 0)
    snprintf(ep, te, i18n("Temporada %d Episódio %d"), atoi(c1 + 1), atoi(c2 + 1));
}

// PACOTE DE SELOS ATIVO: os filtros do pacote que casam com a fonte, ordem do
// pacote. Calculado uma vez por fonte (ver Stream.selosPacote); com o pacote
// "Do Nuvio" devolve 0 e a fileira usa a deteccao embutida. Se nenhum filtro
// casa tambem devolve 0: a fileira cai na deteccao embutida em vez de ficar
// vazia.
_Static_assert(SELOS_MAX_CASADOS == sizeof(((Stream *)0)->selosPacote) / sizeof(unsigned short),
               "Stream.selosPacote e SELOS_MAX_CASADOS tem de ter o mesmo tamanho");
// O CALCULO (fio de fundo): os filtros do pacote ativo contra os textos da fonte.
static void selosCalcular(Stream *s) {
  const char *campos[5];
  unsigned ver = selospacote_versao();
  campos[0] = s->arquivo; campos[1] = s->rotulo; campos[2] = s->descricao; campos[3] = s->provedor;
  s->nSelosPacote = (unsigned char)selospacote_casar(campos, 4, s->selosPacote, SELOS_MAX_CASADOS);
  // Fonte de servidor de midia (Plex/Jellyfin) sabe do Dolby Vision pela faixa
  // e nao pelo nome: se o texto nao o disse, entra a palavra e casa de novo.
  if (s->dolbyVision && selospacote_ativo() < 0) {
    int k, tem = 0;
    for (k = 0; k < (int)s->nSelosPacote; k++) {
      const SeloFiltro *f = selospacote_filtro(s->selosPacote[k]);
      if (f && (strstr(f->nome, "DV") || strstr(f->nome, "Dolby Vision"))) tem = 1;
    }
    if (!tem) {
      campos[4] = "Dolby Vision";
      s->nSelosPacote = (unsigned char)selospacote_casar(campos, 5, s->selosPacote, SELOS_MAX_CASADOS);
    }
  }
  s->selosPacoteVer = ver;
}
// FORA DA THREAD PRINCIPAL. Sao centenas de regex por fonte: feito aqui dentro,
// na chegada de cada addon, a pagina do titulo parava ~2 s por resposta (TCL,
// 05/10/2026: "[quadro] upd=1667.5" com 20 fontes, 2052 com 46). O fio leva uma
// COPIA do trecho da lista e devolve so os tres campos, sob verTrava e so se a
// lista ainda e a mesma (listaGeracao; acrescentar nao muda indice). Ate
// chegar, selosPacoteDa devolve 0 e a fileira usa a deteccao embutida.
typedef struct { unsigned ger; int ini, k; Stream v[]; } SelosLote;
static unsigned selosPedVer, selosPedGer;
static void *selosFio(void *u) {
  SelosLote *L = u;
  int i;
  for (i = 0; i < L->k; i++) selosCalcular(&L->v[i]);
  pthread_mutex_lock(&verTrava);
  if (L->ger == listaGeracao)
    for (i = 0; i < L->k && L->ini + i < n; i++) {
      Stream *d = &lista[L->ini + i];
      memcpy(d->selosPacote, L->v[i].selosPacote, sizeof d->selosPacote);
      d->nSelosPacote = L->v[i].nSelosPacote;
      d->selosPacoteVer = L->v[i].selosPacoteVer;
    }
  pthread_mutex_unlock(&verTrava);
  free(L);
  return NULL;
}
static void selosAgendar(int ini, int qtd) {
  SelosLote *L;
  pthread_t fio;
  pthread_attr_t at;
  selosPedVer = selospacote_versao(); selosPedGer = listaGeracao;
  if (qtd <= 0 || ini < 0 || ini + qtd > n) return;
  selospacote_colorido(ajustes_selos_coloridos());
  if (selospacote_ativo() < 0 && !selospacote_embutidos_ok()) return;
  selosPedVer = selospacote_versao();
  L = malloc(sizeof *L + sizeof(Stream) * (size_t)qtd);
  if (!L) return;
  L->ger = listaGeracao; L->ini = ini; L->k = qtd;
  memcpy(L->v, lista + ini, sizeof(Stream) * (size_t)qtd);
  pthread_attr_init(&at);
  pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&fio, &at, selosFio, L)) free(L);
  pthread_attr_destroy(&at);
}
static int selosPacoteDa(Stream *s) {
  unsigned ver;
  // O modo colorido vale para a lista inteira: ligar/desligar nos Ajustes
  // sobe a versao do pacote e a lista e recalculada (no fio) na proxima olhada.
  selospacote_colorido(ajustes_selos_coloridos());
  if (selospacote_ativo() < 0 && !selospacote_embutidos_ok()) return 0;
  ver = selospacote_versao();
  if (s->selosPacoteVer == ver) return s->nSelosPacote;
  // Pacote trocado com a lista em memoria: pede a lista inteira, uma vez.
  if (selosPedVer != ver || selosPedGer != listaGeracao) selosAgendar(0, n);
  // COPIA de uma fonte (cartao do player, retrato da folha): pega o resultado
  // da entrada da lista com a mesma url, se ja saiu.
  if (s < lista || s >= lista + n) {
    int j;
    for (j = 0; j < n; j++)
      if (lista[j].selosPacoteVer == ver && !strcmp(lista[j].url, s->url) && !strcmp(lista[j].rotulo, s->rotulo)) {
        memcpy(s->selosPacote, lista[j].selosPacote, sizeof s->selosPacote);
        s->nSelosPacote = lista[j].nSelosPacote; s->selosPacoteVer = ver;
        return s->nSelosPacote;
      }
  }
  return 0;
}

// Quantos selos do pacote a FILEIRA desenha: com os pacotes embutidos a
// resolucao e o grupo da folha e nao entra nela (como em logosDa), e uma fonte
// so com a resolucao cai na linha de descricao como sempre caiu.
static int selosPacoteVisiveis(Stream *s) {
  int k, n = 0;
  if (!selosPacoteDa(s)) return 0;
  for (k = 0; k < (int)s->nSelosPacote; k++) {
    const SeloFiltro *f = selospacote_filtro(s->selosPacote[k]);
    if (f && !(f->arte != SELO_ARTE_PACOTE && f->resolucao)) n++;
  }
  return n;
}

// A fileira mostra TODOS os logos, inclusive o que tambem esta no titulo
// (dono, 02/10: "tem que colocar as badges do dolby vision tb" — o titulo e
// para ler, o logo e a marca que o olho reconhece de longe). Sai so a
// resolucao, que ja e o grupo.
//
// Os logos de audio COMBINADOS ("Dolby Atmos · Vision", "TrueHD · Vision",
// "Digital · Vision") trazem o Vision dentro, e badges_detectar NAO poe v-dv
// quando ha um deles. Numa fonte Dolby Vision a fileira mostra entao o logo
// do Vision sozinho e o audio sem o Vision, em vez de dizer Vision duas vezes.
static uint64_t logosDa(const Stream *s, uint64_t tira) {
  static const char *const COMB[][2] = {
    { "a-atmos-dv", "a-atmos" }, { "a-truehd-dv", "a-truehd" }, { "a-dd-dv", "a-ddp" },
  };
  uint64_t m = s->badges & ~(badges_bit("r-4k") | badges_bit("r-1080") | badges_bit("r-720") | badges_bit("r-sd"));
  (void)tira;
  if (s->dolbyVision || (m & (badges_bit("v-dv") | badges_bit("a-atmos-dv") |
                              badges_bit("a-truehd-dv") | badges_bit("a-dd-dv")))) {
    m |= badges_bit("v-dv");
    for (size_t k = 0; k < sizeof COMB / sizeof COMB[0]; k++)
      if (m & badges_bit(COMB[k][0])) m = (m & ~badges_bit(COMB[k][0])) | badges_bit(COMB[k][1]);
  }
  return m;
}
// A linha de arquivo existe quando ha arquivo, ou quando a descricao NAO foi
// usada no lugar da fileira de logos (linha sem selo e sem MP4). A altura da
// linha e o desenho perguntam aqui, senao a linha abre um vao vazio.
//
// Da descricao vale so a ULTIMA linha: formatadores como o AIOStreams poem
// tamanho e taxa em cima e o nome do arquivo no fim, e a linha inteira
// repetia os numeros que a coluna da direita ja mostra. Buffer estatico: so
// o fio principal chama, e cada chamador usa o texto antes da seguinte.
static const char *arquivoDa(const Stream *s) {
  static char ult[192];
  const char *p, *f;
  if (s->arquivo[0]) return s->arquivo;
  if (!logosDa(s, 0) && !selosPacoteVisiveis((Stream *)s) && strcmp(containerDa(s), "MP4") && !idiomaDa(s)) return "";
  ult[0] = 0;
  for (p = s->descricao; *p; p = f + 1) {
    f = strchr(p, '\n');
    char l[192];
    linhaLimpa(l, sizeof l, p, f ? (size_t)(f - p) : strlen(p));
    if (l[0]) snprintf(ult, sizeof ult, "%s", l);
    if (!f) break;
  }
  return ult;
}

// A linha de marca e so para o que esta tocando e para a escolha anterior.
// A fonte que o automatico tocaria ganha o selo "Melhor para esta TV" ao
// lado do titulo (melhorDaFolha), e nao mais uma linha "Escolha automatica".
static int melhorFolha = -1;   // stream_automatico() do quadro, posto antes de montar()
static int temMarca(int i, int automatica) {
  (void)automatica;
  // StreamFit (F03): a measured-heavy source carries its reason on the mark line.
  return i >= 0 && (i == atual || i == preferida || (i == melhorFolha && nFiltrados() > 1) ||
                    fitPesada(i));
}
// Altura de uma linha: base, marca, e o nome do arquivo que abre por mola na
// linha em foco. Usa nOrdem como o indice que a linha vai receber.
static float alturaLinha(int i, int automatica) {
  float h = FOLHA_LINHA_H + (temMarca(i, automatica) ? FOLHA_MARCA_H : 0);
  if (ajustes_fonte_texto_addon()) {
    LinhaAddon tmp[FOLHA_ADDON_LINHAS];
    int nl = linhasAddon(&lista[i], tmp, FOLHA_ADDON_LINHAS);
    float ab = grupo == 1 && nOrdem == foco ? abreFoco : nOrdem == linhaAnt ? abreAnt : 0.0f;
    float vis = nl > FOLHA_ADDON_FECHADA ? FOLHA_ADDON_FECHADA + (nl - FOLHA_ADDON_FECHADA) * ab : (float)nl;
    h = 20 + 40 + vis * FOLHA_ADDON_LD + 18 + (temMarca(i, automatica) ? FOLHA_MARCA_H : 0);
    if (h < 96) h = 96;
  } else {
    // The connection line opens with the focused row even without a file name.
    float extra = (arquivoDa(&lista[i])[0] ? FOLHA_ARQ_H : 0) + FOLHA_FIT_H * fitLinhas(i);
    if (grupo == 1 && nOrdem == foco) h += extra * abreFoco;
    else if (nOrdem == linhaAnt)      h += extra * abreAnt;
  }
  return h;
}
static void porLinha(int i, float *y, int automatica) {
  float h = alturaLinha(i, automatica);
  ordem[nOrdem] = i;
  linhaY[nOrdem] = *y;
  linhaH[nOrdem] = h;
  *y += h + FOLHA_LINHA_GAP;
  nOrdem++;
}
// Tamanho para ordenar: desconhecido (0) vai para o fim do grupo.
static long tamanhoOrdem(int i) { return lista[i].tamanhoMB > 0 ? lista[i].tamanhoMB : -1; }

// POR QUALIDADE (dono, 02/10): a fonte "Melhor para esta TV" e a PRIMEIRA linha,
// sozinha e sem cabecalho — o selo dela ja diz o que ela e —, e cada grupo
// vem do MAIOR arquivo para o menor. Tamanho e o que mais separa duas fontes
// do mesmo grupo de qualidade (Remux de 60 GB contra encode de 4 GB).
// "Do addon" (#400) preserva ORD sem aplicar esta organizacao.
static int *grupoTmp;
static int *grupoFitTmp;
static int grupoTmpCap;
static void montar(int automatica) {
  float y = 0;
  int g, i, k, m = -1, nt;
  int needed = filtro == -1 ? ondever_n(alvoPedido) : n;
  if (ordemCap < needed) {
    int cap = needed + 32;
    int *o = realloc(ordem, cap * sizeof *o);
    if (o) ordem = o;
    float *a = realloc(linhaY, cap * sizeof *a);
    if (a) linhaY = a;
    float *b = realloc(linhaH, cap * sizeof *b);
    if (b) linhaH = b;
    if (!o || !a || !b) { nOrdem = 0; return; }
    ordemCap = cap;
  }
  if (filtro == -1) {
    memset(secN, 0, sizeof secN);
    nOrdem = needed;
    for (k = 0; k < needed; k++) {
      ordem[k] = -2-k; linhaY[k] = y; linhaH[k] = FOLHA_LINHA_H;
      y += linhaH[k] + FOLHA_LINHA_GAP;
    }
    alturaTotal = y;
    return;
  }
  // #400: ordem visual do addon, sem promover, agrupar ou particionar fontes.
  // A folha chama montar() a cada desenho: mudar o ajuste vale no proximo quadro.
  if (ajustes_fonte_ordem_addon()) {
    // A classificacao do StreamFit (avisos "Acima da conexao") e o
    // enriquecimento de canal (grupoRes -> stream_canal_enriquecer) continuam;
    // so a ordem e a do addon.
    fitAtualizar();
    memset(secN, 0, sizeof secN);
    nOrdem = 0;
    for (k = 0; k < n; k++) {
      i = ORD(k);
      if (!passaFiltro(i)) continue;
      (void)grupoRes(&lista[i]);
      porLinha(i, &y, automatica);
    }
    alturaTotal = y;
    return;
  }
  if (grupoTmpCap < n) {
    int *t = realloc(grupoTmp, (n + 32) * sizeof *t);
    int *f = realloc(grupoFitTmp, (n + 32) * sizeof *f);
    if (t) grupoTmp = t;
    if (f) grupoFitTmp = f;
    if (!t || !f) { nOrdem = 0; return; }
    grupoTmpCap = n + 32;
  }
  fitAtualizar();
  nOrdem = 0;
  if (melhorFolha >= 0 && melhorFolha < n && passaFiltro(melhorFolha) && nFiltrados() > 1 &&
      (melhorFolha >= fitN || fitClasses[melhorFolha] != SF_PESADA)) {
    m = melhorFolha;
    porLinha(m, &y, automatica);
    y += FOLHA_SEC_GAP - FOLHA_LINHA_GAP;
  }
  for (g = 0; g < FOLHA_GRUPOS; g++) {
    secN[g] = 0;
    nt = 0;
    // Na ordem dos addons (ORD), nao na de chegada: a lista enche addon a
    // addon (#221) e o empate de tamanho fica com a ordem do addon.
    for (k = 0; k < n; k++) {
      i = ORD(k);
      if (i != m && passaFiltro(i) && grupoRes(&lista[i]) == g) grupoTmp[nt++] = i;
    }
    // Insercao estavel: em tamanho igual vale a ordem do addon.
    for (k = 1; k < nt; k++) {
      int v = grupoTmp[k], j = k - 1;
      while (j >= 0 && tamanhoOrdem(grupoTmp[j]) < tamanhoOrdem(v)) { grupoTmp[j + 1] = grupoTmp[j]; j--; }
      grupoTmp[j + 1] = v;
    }
    // Preserve the approved quality groups and descending-size base order.
    // Only measured-heavy sources move back within their own group.
    streamfit_particionar(grupoTmp, nt, fitClasses, fitN, grupoFitTmp);
    if (!nt) continue;
    if (nOrdem) y += FOLHA_SEC_GAP;
    secY[g] = y;
    y += FOLHA_SEC_H;
    secN[g] = nt;
    for (k = 0; k < nt; k++) porLinha(grupoTmp[k], &y, automatica);
  }
  alturaTotal = y;
}
static int filtrado(int linha) {
  return linha >= 0 && linha < nOrdem ? ordem[linha] : -1;
}
static int linhaDe(int indice) {
  for (int r = 0; r < nOrdem; r++) if (ordem[r] == indice) return r;
  return -1;
}
static int folhaIlha(void) { return player_aberto(); }
static float folhaIlhaH(void) { return NV_TELA_H - FOLHA_ILHA_Y - FOLHA_ILHA_CAB - FOLHA_ILHA_BASE; }
static void folhaGeometria(void) {
  if (folhaIlha()) {
    folhaOY = FOLHA_ILHA_Y + FOLHA_ILHA_CAB + FOLHA_ILHA_PAD - 74.0f;
    folhaBase = NV_TELA_H - FOLHA_ILHA_BASE - 16.0f;
  } else {
    folhaOY = 0.0f;
    folhaBase = NV_TELA_H - FOLHA_MARGEM - 16.0f;
  }
}
static float areaLista(void) { return folhaBase - FOLHA_TOPO; }
// A linha em foco no meio da area; a primeira de um grupo leva o cabecalho
// junto, senao subir ate ela deixaria "4K" escondido acima da borda.
static float alvoRolagem(void) {
  float area = areaLista(), alvo, max;
  if (grupo != 1 || foco >= nOrdem) return rolagem;
  alvo = linhaY[foco] - (area - linhaH[foco]) * .5f;
  for (int g = 0; g < FOLHA_GRUPOS; g++)
    if (secN[g] && secY[g] + FOLHA_SEC_H == linhaY[foco] && alvo > secY[g]) alvo = secY[g];
  max = alturaTotal - area + 40.0f;
  if (alvo > max) alvo = max;
  if (alvo < 0) alvo = 0;
  return alvo;
}

// O foco do cabecalho e do seletor de addon segue o botao primario do app:
// fill accent limpo e halo macio atras do alvo. A LINHA nao usa este foco —
// la a superficie clara com contorno fino e o que o mockup aprovado mostra,
// e um bloco cheio de acento de 112 px de altura era justamente o "infantil".
static void focoFonte(GfxRect r, float raio, float alfa) {
  float sr, sg, sb;
  if (alfa <= 0.01f) return;
  ajustes_acento(&sr, &sg, &sb);
  botao_luz(r, 0.55f, alfa);
  gfx_cor(r, raio, sr, sg, sb, alfa);
}

// EQUALIZADOR DO "REPRODUZINDO AGORA". O player nativo nao expoe amplitude
// de audio por quadro, entao isto NAO finge ser medidor: e uma assinatura visual
// discreta de que a fonte esta ativa. `alt` e a altura da caixa: na linha da
// marca as barras tem a altura das letras.
static void desenharAudioBars(float x, float y, float alt, float alfa, Uint32 agora) {
  static const float parado[FOLHA_AUDIO_N] = { .35f, .58f, .82f, .52f, .72f, .44f, .64f, .48f };
  float cr, cg, cb;
  ajustes_acento(&cr, &cg, &cb);
  for (int i = 0; i < FOLHA_AUDIO_N; i++) {
    float nivel = parado[i], h;
    if (!ajustes_animacoes_reduzidas())
      nivel = .22f + .78f * (.5f + .5f * sinf((float)agora * .0042f + i * .82f));
    h = alt * (.25f + .75f * nivel);
    gfx_cor((GfxRect){ x + i * (FOLHA_AUDIO_BAR * .5f + FOLHA_AUDIO_GAP * .6f), y + alt - h,
                       FOLHA_AUDIO_BAR * .5f, h }, .5f, cr, cg, cb, alfa * .92f);
  }
}

void stream_folha_abrir(void) {
  int excl, aut, alvo;
  fitAbrir();
  aberta=1; escolha=-1; grupo=1; filtro=0; soMp4=0; soCache=0; soDub=0; recarregar=0;
  folhaGeometria();
  atualizarProvedores();
  ondever_apps_atualizar();
  // A FOLHA ABRE NA FONTE QUE IMPORTA: a que esta tocando, senao a que o
  // automatico tocaria. Com a lista agrupada por resolucao a primeira linha ja
  // nao e a de maior pontuacao, entao abrir na linha 0 poria o realce numa
  // fonte qualquer de 4K enquanto a marca "automatica" fica la embaixo.
  aut = automaticaDaFolha();
  melhorFolha = automaticoCom(!canalFolha);
  alvo = atual >= 0 ? atual : aut;
  abreFoco = 1; abreAnt = 0; linhaAnt = -1; foco = 0;
  montar(aut);
  foco = linhaDe(alvo);
  if (foco < 0) foco = 0;
  focoVisto = foco;
  montar(aut);
  rolagem = alvoRolagem(); velRol = 0;
  // A CONTAGEM DA FOLHA NO LOG (#132). O log tinha "[addons] X: N fontes" e
  // "[addons] total N" de um lado e nada do que a folha mostrou do outro: um
  // relato de "so 1 fonte listada" nao tinha como dizer se a queda foi no
  // addon, no descarte de torrent sem debrid ou na folha.
  pthread_mutex_lock(&autoExclTrava);
  excl = nAutomaticasExcluidas;
  pthread_mutex_unlock(&autoExclTrava);
  printf("[fonte] folha: %d de %d na lista (%d addon(s); %d torrent(s) sem debrid "
         "descartado(s); %d ja recusada(s) pelo automatico, continuam na folha)\n",
         nFiltrados(), n, nProvedores - 1, descartadosSemDebrid, excl);
  int conhecidos = 0, pesados = 0, diag = 0, pass = 0;
  for (int i = 0; i < fitN; i++) {
    conhecidos += fitClasses[i] != SF_DESCONHECIDA;
    pesados += fitClasses[i] == SF_PESADA;
  }
  for (int i = 0; i < fitFoto.n; i++) {
    diag += fitFoto.hosts[i].origem == SF_ORIGEM_DIAGNOSTICO;
    pass += fitFoto.hosts[i].origem == SF_ORIGEM_PASSIVA;
  }
  printf("[stream_fit] snapshot hosts=%d diagnostic=%d passive=%d classified=%d heavy=%d runtime=%s\n",
         fitFoto.n, diag, pass, conhecidos, pesados,
         fitFotoOrigem == SF_DUR_MEDIA ? "media" : fitFotoOrigem == SF_DUR_METADATA ? "metadata" : "unknown");
  fflush(stdout);
}
int stream_folha_aberta(void) { return aberta; }
float stream_folha_anim(void) { return anim; }

// A MESMA FONTE entre duas montagens da lista (ver stream_atualizar_lista).
// Url sozinha nao basta: torrent sem debrid resolvido tem url vazia e se
// distingue pelo hash e pelo arquivo.
static int mesmaFonte(const Stream *a, const Stream *b) {
  return a->fileIdx == b->fileIdx && !strcmp(a->provedor, b->provedor) &&
         !strcmp(a->url, b->url) && !strcmp(a->infoHash, b->infoHash) &&
         !strcmp(a->rotulo, b->rotulo);
}
static int acharFonte(const Stream *alvo) {
  for (int i = 0; i < n; i++) if (mesmaFonte(&lista[i], alvo)) return i;
  return -1;
}

// LISTA CRESCENDO COM A FOLHA ABERTA (issue #221). A pessoa pode estar
// descendo a lista quando o addon seguinte responde: o realce fica na MESMA
// fonte, mesmo que ela mude de linha (grupo de resolucao novo acima, ou addon
// anterior na ordem). Com a folha aberta ainda vazia, a primeira leva poe o
// realce na que o automatico tocaria — o mesmo criterio de stream_folha_abrir.
void stream_atualizar_lista(const Stream *l, int qtd) {
  static Stream marca[3];   // ~8 KB cada: fora da pilha
  int tem[3] = {0}, idx[3], g = grupo, f = foco, k;
  idx[0] = atual; idx[1] = preferida;
  idx[2] = aberta && grupo == 1 ? filtrado(foco) : -1;
  for (k = 0; k < 3; k++)
    if (idx[k] >= 0 && idx[k] < n) { marca[k] = lista[idx[k]]; tem[k] = 1; }
  stream_definir_lista(l, qtd);
  if (tem[0]) atual = acharFonte(&marca[0]);
  if (tem[1]) preferida = acharFonte(&marca[1]);
  foco = f;   // fora da lista, `foco` e o botao do cabecalho
  if (aberta && g == 1) {
    int alvo = tem[2] ? acharFonte(&marca[2]) : -1, r;
    atualizarProvedores();
    montar(automaticaDaFolha());
    if (alvo < 0 && !tem[2]) alvo = atual >= 0 ? atual : automaticaDaFolha();
    r = alvo >= 0 ? linhaDe(alvo) : -1;
    if (r >= 0) foco = r;
    // Mudar de linha por causa da lista nao e a pessoa andando: sem isto a
    // linha em foco fecharia e reabriria o nome do arquivo a cada addon.
    focoVisto = foco; linhaAnt = -1;
  }
}
int stream_folha_n(void) { return nFiltrados(); }
void stream_folha_evento(const SDL_Event *e) {
  if(!aberta || e->type!=SDL_KEYDOWN) return;
  SDL_Keycode k=e->key.keysym.sym;
  if(k==SDLK_ESCAPE || k==SDLK_AC_BACK || k==SDLK_BACKSPACE || k==SDLK_DELETE) {aberta=0;return;}
  if(k==SDLK_r) {recarregar=1;return;}
  montar(automaticaDaFolha());
  int nf=nOrdem;
  // ORDEM DO FOCO, de cima: fileira de botoes (grupo -1) -> abas de addon
  // (grupo 0) -> lista (grupo 1). CIMA na primeira linha vai as abas; CIMA
  // nas abas vai ao ultimo botao (Fechar, o mais perto do canto); BAIXO desce
  // na mesma ordem. ESQUERDA/DIREITA andam dentro da fileira de botoes, e nas
  // abas e na lista trocam de aba. Voltar fecha a folha de qualquer grupo.
  // `foco` e indice de BOTAO no cabecalho e de LINHA na lista: ao trocar de
  // grupo ele recomeca, senao descer do quarto botao caia na quarta linha.
  if(k==SDLK_UP) {
    if(grupo==1 && foco>0) foco--;
    else if(grupo==1) grupo=0;
    else if(grupo==0) {grupo=-1;foco=nBotoes()-1;}
  }
  if(k==SDLK_DOWN) {
    if(grupo==-1) grupo=0;
    else if(grupo==0) {grupo=1;foco=0;}
    else if(foco<nf-1) foco++;
  }
  // ESQUERDA/DIREITA NUMA FONTE TROCAM A ABA DE ADDON (dono, 02/10), como no
  // seletor: a lista nao tem nada na horizontal, e subir ate as abas para
  // trocar de addon eram duas teclas a mais a cada troca.
  if(grupo>=0 && (k==SDLK_LEFT || k==SDLK_RIGHT)) {
    filtro+=k==SDLK_RIGHT?1:-1;
    if(filtro < -1) filtro=-1;
    if(filtro>=nProvedores) filtro=nProvedores-1;
    foco=0;rolagem=0;velRol=0;
  }
  if(grupo==-1 && (k==SDLK_LEFT || k==SDLK_RIGHT)) {
    foco+=k==SDLK_RIGHT?1:-1;
    if(foco<0) foco=0;
    if(foco>=nBotoes()) foco=nBotoes()-1;
  }
  if(k==SDLK_RETURN || k==SDLK_KP_ENTER) {
    if(grupo==-1) {
      switch(botaoDe(foco)) {
        case BT_RECARREGAR: recarregar=1; break;
        // Fecha a folha junto: a imagem volta (ou nao) na propria tela do
        // player, e deixar a folha aberta em cima esconderia o resultado.
        case BT_SEM_HDR:    video_forcar_sdr(); aberta=0; break;
        case BT_SO_MP4:     soMp4 = !soMp4; rolagem=0;velRol=0; break;
        case BT_CACHE:      soCache = !soCache; rolagem=0;velRol=0; break;
        case BT_DUB:        soDub = !soDub; rolagem=0;velRol=0; break;
        default:            aberta=0; break;
      }
    }
    else if(grupo==0) {grupo=1;foco=0;}
    else {
      int i = filtrado(foco);
      if (i >= 0) { escolha=i; aberta=0; }
      else if (i <= -2) {
        OndeVer o;
        if (ondever_item(alvoPedido,-i-2,&o) && ondever_abrir(o.nome)!=ONDE_INFO) aberta=0;
      }
    }
  }
}
void stream_folha_atualizar(float dt, Uint32 agora) {
  int nf;
  medirCabecalho();
  folhaGeometria();
  (void)agora;
  anim=anim_mola(anim,aberta?1:0,dt,NV_MOLA_TELA);
  // FOLHA FECHADA E JA FORA DA TELA: nada a montar. Esta funcao roda todo
  // quadro em qualquer tela (app.c), e montar + stream_automatico percorrem
  // a lista inteira de fontes duas vezes: MEDIDO no Mac (06/10/2026), 1 ms
  // por quadro na pagina do titulo com 201 fontes e a folha fechada (~97% do
  // `upd`). Abrir (stream_folha_abrir), as teclas e o desenho montam de novo
  // por conta propria; o foco pedido com a folha fechada era sobrescrito ao
  // abrir, entao sai daqui tambem.
  if (!aberta && anim < .005f) { focoFixo = -1; return; }
  atualizarProvedores();
  montar(automaticaDaFolha());
  nf=nOrdem;
  if(grupo==1 && foco>=nf) foco=nf>0?nf-1:0;
  {
    int linha = grupo==1 ? foco : -1;
    if (linha != focoVisto) {
      linhaAnt = focoVisto; abreAnt = abreFoco; abreFoco = 0; focoVisto = linha;
    }
  }
  abreFoco = anim_mola(abreFoco, 1, dt, NV_MOLA_TELA);
  abreAnt  = anim_mola(abreAnt, 0, dt, NV_MOLA_TELA);
  melhorFolha = automaticoCom(!canalFolha);
  montar(automaticaDaFolha());
  if (focoFixo >= 0) {
    int r = grupo == 1 ? linhaDe(focoFixo) : -1;
    if (r >= 0) { rolagem += linhaY[r] - focoFixoY; foco = r; focoVisto = r; }
    focoFixo = -1;
  }
  rolagem=anim_mola2(&velRol,rolagem,alvoRolagem(),dt,NV_MOLA2_SCROLL);
}
int stream_folha_escolheu(int *out) {
  if(escolha<0) return 0;
  if(out) *out=escolha;
  escolha=-1;return 1;
}
// PONTEIRO (#99): as mesmas variaveis das setas (grupo, foco, filtro). O
// provedor so troca no CLIQUE — trocar ao passar por cima mudaria a lista
// debaixo da mao. Clicar fora do painel fecha, como o Voltar.
static void ponteiroFolhaBotao(int i, int b) { (void)b; grupo = -1; foco = i; }
static void ponteiroFolhaLinha(int row, int b) { (void)b; grupo = 1; foco = row; }
static void ponteiroFolhaFiltro(int i, int b) {
  (void)b;
  if (i < -1 || i >= nProvedores) return;
  filtro = i; foco = 0; rolagem = 0; grupo = 1;
}
static void ponteiroFolhaFora(int a, int b) { (void)a; (void)b; aberta = 0; }

// Availability lives in its own tab so asynchronous responses cannot shift
// source indexes, focus or an in-progress debrid resolution.
//
// A LINHA DO SERVICO E A LINHA DA FONTE (dono, 03/10: "no source, a aba de
// streaming ta fora do padrao"). Era um cartao proprio: fundo em toda linha,
// foco num bloco CHEIO de acento com halo, logo a esquerda empurrando o
// texto para fora da coluna das outras abas e a acao numa terceira linha.
// Agora e a mesma anatomia da fonte: nome no TXT_CALLOUT na coluna `tx`,
// linha de apoio cinza onde a fonte tem os selos, e a coluna da direita com
// a marca do servico (as cores da marca ficam, como as logos dos selos) e a
// acao em cinza embaixo, no lugar do tamanho e do addon. Foco so pela
// superficie clara, sem acento e sem contorno.
static void desenharOnde(GfxRect r, float tx, float tr, const OndeVer *o, int selected, float a) {
  const int c = selected ? 255 : 205;
  const int cd = selected ? 180 : 130;
  const int cp = selected ? 150 : 110;
  const float side = 56.0f;   // era 40 (dono, 05/10: "muito pequeno"); cabe nos 112 da linha
  float colW, cy = r.y + 16.0f;
  const char *action;
  TxtLinha la;
  int state = ondever_estado(o->nome);
  // Logo de servico pedido pela largura com que desenha (side=56, cap 128
  // pelo piso) — o 640 unico decodificava ~1,6 MB para um selo de 40. Ver
  // tests/artemenor.c.
  GLuint logo = o->logo[0] ? tex_obter_larg(o->logo, side) : 0;
  action = state==ONDE_ABRIR ? "Abrir app" : state==ONDE_LOJA ? "Ver na loja"
         : state==ONDE_PROCURAR ? "Procurar na loja" : "Disponível neste serviço";
  la = txt_linha_corta(TXT_PG_FIM, action, cp, cp, cp - 2, 255, 260);
  colW = (float)la.w > side ? (float)la.w : side;
  if (logo) gfx_rect((GfxRect){tr - side, cy, side, side}, logo, GFX_CARD, 0, 0, 0, .2f, 1, 1, 1,
                     a * (selected ? 1.0f : .85f));
  txt_desenhar_alpha(la, tr - la.w, cy + side + 6.0f, a);
  txt_desenhar_alpha(txt_linha_corta(TXT_CALLOUT, o->nome, c, c, c - 2, 255, tr - tx - colW - 28.0f), tx, cy + 4.0f, a);
  txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META, o->gratis ? "Grátis / com anúncios" : "Na assinatura",
                                     cd, cd, cd, 255, tr - tx - colW - 28.0f),
                     tx, cy + 44.0f + (FOLHA_SELO_H - 26.0f) * .5f, a);
}

// Texto em maiusculas espacadas da linha de marca e dos cabecalhos de grupo.
// i18n antes da caixa alta: a tabela de idioma guarda a frase normal.
// A FILEIRA DO PACOTE DE SELOS (selospacote.h): cada filtro que casou, na ordem
// do pacote. A imagem sai COMO O PACOTE A FEZ (textura normal, sem tinta e sem
// a marca clara — o pacote e colorido), na altura `h` e com a proporcao dela;
// sem imagem (ou imagem que nao baixou) vira uma pilula com o NOME e as cores
// do filtro. Para no `maxW`. Devolve a largura usada.
static float desenharSelosPacote(const Stream *s, float x, float y, float maxW, float h, float a,
                                 float tom, int semResolucao) {
  const float gap = 8.0f, padX = 9.0f, imgMaxW = 220.0f;
  float x0 = x;
  int k, colorido = ajustes_selos_coloridos();
  for (k = 0; k < (int)s->nSelosPacote; k++) {
    const SeloFiltro *f = selospacote_filtro(s->selosPacote[k]);
    float w, r, g, b, al;
    int pronta = 0;
    if (!f) continue;
    // A resolucao e o grupo da folha (e o titulo da fonte): fora da fileira.
    if (semResolucao && f->resolucao) continue;
    if (f->imagem[0]) {
      GLuint t = tex_obter_larg(f->imagem, 128);
      float asp = t ? tex_aspecto(f->imagem) : 0.0f;
      if (t && asp > 0.01f && f->arte != SELO_ARTE_PACOTE) {
        // PACOTES EMBUTIDOS (selospacote.h). Padrao: a arte e branca e a forma
        // mora no alfa, entao tinge no cinza da linha como os logos de sempre.
        // Colorido: cada selo numa PECA de base escura (a arte colorida e feita
        // para fundo escuro, e a linha em foco e clara), e o selo padrao que
        // cobre o que o colorido nao tem entra na mesma peca, em branco.
        if (colorido) {
          const float pad = 6.0f, sobra = 3.0f, ch = h + sobra * 2.0f, raio = 6.0f / ch;
          float ih = h;
          w = ih * asp;
          if (w > 144.0f) { w = 144.0f; ih = w / asp; }
          if (x + w + pad * 2.0f > x0 + maxW) break;
          { GfxRect p = (GfxRect){x, y - sobra, w + pad * 2.0f, ch};
            gfx_cor(p, raio, 0.10f, 0.11f, 0.13f, 0.88f * a);
            gfx_anel(p, raio, 1.5f, 1.0f, 1.0f, 1.0f, 0.20f * a); }
          if (f->arte == SELO_ARTE_COR) {
            gfx_tex_aspect_atual = 0.0f;
            gfx_rect((GfxRect){x + pad, y + (h - ih) * .5f, w, ih}, t, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
          } else
            gfx_rect((GfxRect){x + pad, y + (h - ih) * .5f, w, ih}, t, GFX_MARCA, 0, 0, 0, 0.0f, 1, 1, 1, a);
          x += w + pad * 2.0f + gap;
        } else {
          float ih = h;
          w = ih * asp;
          if (w > 160.0f) { w = 160.0f; ih = w / asp; }
          if (x + w > x0 + maxW) break;
          gfx_rect((GfxRect){x, y + (h - ih) * .5f, w, ih}, t, GFX_MARCA, 0, 0, 0, 0.0f, tom, tom, tom, a);
          x += w + 16.0f;
        }
        continue;
      }
      if (t && asp > 0.01f) {
        float ih = h;
        w = ih * asp;
        if (w > imgMaxW) { w = imgMaxW; ih = w / asp; }
        if (x + w > x0 + maxW) break;
        gfx_tex_aspect_atual = 0.0f;
        gfx_rect((GfxRect){x, y + (h - ih) * .5f, w, ih}, t, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
        x += w + gap;
        continue;
      }
      // Ainda baixando: guarda o lugar (a fileira nao pula quando chega) e nao
      // desenha. Se falhou, mostra o nome.
      pronta = tex_falhou(f->imagem);
      if (!pronta) {
        w = h * 2.0f;
        if (x + w > x0 + maxW) break;
        x += w + gap;
        continue;
      }
    }
    // pilula de texto
    { int tr = f->temTexto ? (int)(f->texto[0] * 255.0f + .5f) : 235,
          tg = f->temTexto ? (int)(f->texto[1] * 255.0f + .5f) : 235,
          tb = f->temTexto ? (int)(f->texto[2] * 255.0f + .5f) : 238;
      TxtLinha l = txt_linha_corta(TXT_MINI, f->nome, tr, tg, tb, 255, 200.0f);
      float ph = h + 6.0f, raio = 6.0f / ph;
      GfxRect p;
      w = (float)l.w + padX * 2.0f;
      if (x + w > x0 + maxW) break;
      p = (GfxRect){x, y - 3.0f, w, ph};
      if (f->temTag) { r = f->tag[0]; g = f->tag[1]; b = f->tag[2]; al = f->tag[3]; }
      else { r = .10f; g = .11f; b = .13f; al = .88f; }
      if (al > 0.01f) gfx_cor(p, raio, r, g, b, al * a);
      if (f->temBorda && f->borda[3] > 0.01f)
        gfx_anel(p, raio, 1.5f, f->borda[0], f->borda[1], f->borda[2], f->borda[3] * a);
      else if (!f->temTag) gfx_anel(p, raio, 1.5f, .35f, .36f, .40f, .60f * a);
      txt_desenhar_alpha(l, x + padX, y + (h - (float)l.h) * .5f, a);
      x += w + gap; }
  }
  return x > x0 ? x - x0 - gap : 0.0f;
}

int stream_selos_ha(const Stream *s) { return s && selosPacoteDa((Stream *)s) > 0; }
float stream_selos_fileira(const Stream *s, float x, float y, float maxW, float h, float tom, float a) {
  if (!s || !selosPacoteDa((Stream *)s)) return 0.0f;
  return desenharSelosPacote(s, x, y, maxW, h,
                             ajustes_selos_coloridos() ? a * 0.9f : a, tom, 0);
}

static float caixaAlta(const char *s, int r, int g, int b, float x, float y, float a) {
  char up[160];
  size_t k;
  snprintf(up, sizeof up, "%s", i18n(s));
  for (k = 0; up[k]; k++)
    if (up[k] >= 'a' && up[k] <= 'z') up[k] = (char)(up[k] - 32);
    else if ((unsigned char)up[k] == 0xC3 && up[k+1] && (unsigned char)up[k+1] >= 0xA0 && (unsigned char)up[k+1] <= 0xBE)
      { up[k+1] = (char)((unsigned char)up[k+1] - 0x20); k++; }   /* à..þ -> À..Þ */
  return txt_tracking(TXT_MINI, up, r, g, b, x, y, a, 1.8f);
}


// CHIP DO CABECALHO: mais baixo e mais leve que a pilula primaria do app —
// e acao secundaria de uma folha, nao o Play. Repouso em branco a 8%, foco no
// acento com a tinta calculada, e `ligado` (o filtro MP4 ativo) num acento a
// 22% com texto no acento: estado, nao foco. Com icone E rotulo (o filtro em
// foco na fileira compacta), o icone vai a esquerda do rotulo.
#define FOLHA_CHIP_H 56.0f
#define FOLHA_CHIP_GAP 10.0f
#define FOLHA_LOGO_H 36.0f   // logo do titulo na linha: caixa da altura do nome
#define FOLHA_LOGO_VAO 12.0f // folga da logo ate a fileira de selos (dono, 05/10: "grudada")
#define FOLHA_LOGO_W 220.0f
static void chipFolha(GfxRect r, const char *rot, float rotW, const char *icone, int foco, int ligado, float a) {
  float ar, ag, ab;
  int c = 225, cr, cg, cb;
  ajustes_acento(&ar, &ag, &ab);
  if (foco) {
    if (ajustes_vidro()) gfx_vidro_pilula_cheia(r, .5f, 1.0f, a);
    else { botao_luz(r, .55f, a); gfx_cor(r, .5f, ar, ag, ab, a); }
    c = ajustes_tinta_foco();
  }
  else if (ligado) gfx_cor(r, .5f, ar, ag, ab, .22f * a);
  else if (ajustes_vidro()) gfx_cor(r, .5f, 1, 1, 1, .08f * a);
  else gfx_cor(r, .5f, .14f, .148f, .17f, a);
  cr = cg = cb = c;
  if (!foco && ligado) { cr = (int)(ar * 255); cg = (int)(ag * 255); cb = (int)(ab * 255); }
  float g = r.h * .42f;
  if (icone && !rot) {
    gfx_icone((GfxRect){ r.x + (r.w - g) * .5f, r.y + (r.h - g) * .5f, g, g }, icone,
              cr / 255.0f, cg / 255.0f, cb / 255.0f, a);
  } else if (icone) {
    TxtLinha l = txt_linha_corta(TXT_HERO_META, rot, cr, cg, cb, 255, rotW);
    float x0 = r.x + (r.w - g - 10.0f - l.w) * .5f;
    gfx_icone((GfxRect){ x0, r.y + (r.h - g) * .5f, g, g }, icone, cr / 255.0f, cg / 255.0f, cb / 255.0f, a);
    txt_desenhar_alpha(l, x0 + g + 10.0f, r.y + (r.h - l.h) * .5f, a);
  } else {
    TxtLinha l = txt_linha_corta(TXT_HERO_META, rot, cr, cg, cb, 255, rotW);
    txt_desenhar_alpha(l, r.x + (r.w - l.w) * .5f, r.y + (r.h - l.h) * .5f, a);
  }
}

// TODOS OS BOTOES NA LINHA DO TITULO (#202). Antes, com seis botoes (a LG tem
// "Sem HDR") ou em ingles/alemao, os filtros desciam para uma linha propria e
// a lista comecava 68 px mais baixo. Agora a fileira encolhe ate caber, em
// degraus, MEDIDO e nao suposto por lingua:
//   0. rotulos inteiros, folga de 20 de cada lado;
//   1. rotulos inteiros, folga de 14;
//   2. filtros viram discos de icone (Lucide), como Recarregar e Fechar; o
//      filtro EM FOCO abre o rotulo ao lado do icone quando o rotulo cabe
//      INTEIRO (reticencias em "Em cache" viravam "Em..."), e a linha do
//      kicker explica o que ele faz em qualquer caso.
// No degrau 2 o filtro ligado continua no acento, entao "qual esta ativo" se
// le sem foco. Se nem os discos couberem (titulo enorme), o titulo e cortado.
static float bwCab[6], rotWCab[6], titMaxCab, gapCab;
static const char *rotCab[6], *icoCab[6];
static int nivelCab;
static const char *iconeFiltro(int b) {
  return b == BT_SEM_HDR ? "aj_sun-dim" : b == BT_SO_MP4 ? "aj_film"
       : b == BT_CACHE ? "aj_zap" : b == BT_DUB ? "aj_languages" : NULL;
}
static void medirCabecalho(void) {
  float rw = FOLHA_W - FOLHA_PAD_E - FOLHA_PAD_D;
  float tit = (float)txt_linha(TXT_ILHA_TITULO, "Fontes", 255, 255, 255, 255).w;
  float livre = rw - FOLHA_TXT - tit - 24.0f, soma = 0;
  int nbt = nBotoes();
  for (nivelCab = 0; nivelCab < 3; nivelCab++) {
    float pad = nivelCab == 0 ? 20.0f : 14.0f;
    gapCab = nivelCab < 2 ? FOLHA_CHIP_GAP : 8.0f;
    soma = (nbt - 1) * gapCab;
    for (int i = 0; i < nbt; i++) {
      int b = botaoDe(i);
      const char *ic = iconeBotao(b);
      rotWCab[i] = 0;
      if (ic) { icoCab[i] = ic; rotCab[i] = NULL; bwCab[i] = FOLHA_CHIP_H; }
      else if (nivelCab < 2) {
        icoCab[i] = NULL; rotCab[i] = rotuloBotao(b);
        rotWCab[i] = (float)txt_linha(TXT_HERO_META, rotCab[i], 255, 255, 255, 255).w;
        bwCab[i] = rotWCab[i] + 2 * pad;
      } else {
        icoCab[i] = iconeFiltro(b); rotCab[i] = NULL; bwCab[i] = FOLHA_CHIP_H;
      }
      soma += bwCab[i];
    }
    if (soma <= livre) break;
  }
  if (nivelCab == 3) nivelCab = 2, gapCab = 8.0f;
  // Degrau 2: o filtro em foco abre o rotulo no espaco que sobrou.
  if (nivelCab == 2 && grupo == -1 && foco >= 0 && foco < nbt && !iconeBotao(botaoDe(foco))) {
    float g = FOLHA_CHIP_H * .42f, w;
    rotCab[foco] = rotuloBotao(botaoDe(foco));
    w = (float)txt_linha(TXT_HERO_META, rotCab[foco], 255, 255, 255, 255).w + 1.0f;
    if (soma - FOLHA_CHIP_H + 16.0f + g + 10.0f + w + 18.0f > livre) rotCab[foco] = NULL;
    else { rotWCab[foco] = w; bwCab[foco] = 16.0f + g + 10.0f + w + 18.0f; soma += bwCab[foco] - FOLHA_CHIP_H; }
  }
  titMaxCab = rw - FOLHA_TXT - 24.0f - soma;
  if (titMaxCab < tit) titMaxCab = titMaxCab < 80.0f ? 80.0f : titMaxCab;
  else titMaxCab = tit + 1.0f;
}

static void stream_folha_desenharCorpo_(Uint32 agora);
static void corpoFolha(float x, float w, float anim, Uint32 agora, int ilha);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void stream_folha_desenhar(Uint32 agora) {
  ESCALA_INI();
  stream_folha_desenharCorpo_(agora);
  ESCALA_FIM();
}
static Uint32 folhaAgora;
// O corpo dentro da ilha (plrilha.h): `a` ja traz a entrada e a saida da
// forma, e a ilha segue chamando o ultimo corpo enquanto encolhe.
static void corpoIlhaFolha(GfxRect c, float a, void *u) {
  (void)u;
  corpoFolha(c.x, c.w, a, folhaAgora, 1);
}
static void stream_folha_desenharCorpo_(Uint32 agora) {
  if(anim<.005f) return;
  folhaGeometria();
  folhaAgora = agora;
  if (folhaIlha()) {
    // O video sem veu cheio: so o degrade do lado da ilha, como Episodios e
    // Audio/Legendas.
    gfx_veu_css((GfxRect){0,0,NV_TELA_W,NV_TELA_H},plrilha_direita()?3:2,1.38f,1.0f,.42f*anim);
    if (!aberta) return;   // fechando: a ilha encolhe com o ultimo corpo
    { PlrIlhaPedido p;
      memset(&p,0,sizeof p);
      p.w = FOLHA_W;
      p.h = folhaIlhaH();
      p.corpo = corpoIlhaFolha;
      plrilha_pedir(&p); }
    return;
  }
  float x=NV_TELA_W-FOLHA_W-FOLHA_MARGEM+(1-anim)*(FOLHA_W+FOLHA_MARGEM);
  // A FOLHA E UMA ILHA (dono, 02/10, mockups "Glass UI — ilha"): flutua a
  // NV_FOLHA_MARGEM das tres bordas, raio NV_FOLHA_RAIO, sombra curta e uma
  // luz larga no canto de cima — o mesmo material da ilha do relogio. O ajuste
  // de vidro escolhe o miolo: translucido (gfx_vidro_folha) ou solido.
  { const int vid = ajustes_vidro();
    GfxRect corpo={x,FOLHA_MARGEM,FOLHA_W,NV_TELA_H-2*FOLHA_MARGEM};
    gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,0,0,0,(vid?.30f:.42f)*anim);
    if (vid) {
      gfx_rect((GfxRect){corpo.x-18.0f,corpo.y-8.0f,corpo.w+36.0f,corpo.h+40.0f},
               0,GFX_SOMBRA,1.0f,0,0,.5f,0,0,0,.38f*anim);
      gfx_vidro_folha(corpo,FOLHA_RAIO_IL/corpo.h,anim);
    } else plrui_material(corpo,FOLHA_RAIO_IL,0,anim); }
  corpoFolha(x, FOLHA_W, anim, agora, 0);
}
// `anim` aqui e o alfa do corpo (sombra a variavel da mola de proposito: na
// ilha quem manda na opacidade e a forma, nao a mola da folha).
static void corpoFolha(float x, float w, float anim, Uint32 agora, int ilha) {
  float ar, ag, ab;
  int ai, nf, automatica, melhor;
  float lx=x+FOLHA_PAD_E, rw=w-FOLHA_PAD_E-FOLHA_PAD_D;
  float tx=lx+FOLHA_TXT, tr=lx+rw-FOLHA_TXT;
  const float oy=folhaOY;
  GfxRect ilhaR={x,0,w,NV_TELA_H};
  ajustes_acento(&ar,&ag,&ab);
  ai=(int)(ar*255.0f+.5f);
  if (ilha) plrilha_rect(&ilhaR);
  int ptr = aberta && anim > .5f && ponteiro_ativo();
  if (ptr && ilha) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroFolhaFora, 0, 0);
    ponteiro_alvo(ilhaR.x, ilhaR.y, ilhaR.w, ilhaR.h, NULL, NULL, 0, 0);
  } else if (ptr) {
    ponteiro_alvo(0, 0, x, NV_TELA_H, NULL, ponteiroFolhaFora, 0, 0);
    // O painel em si absorve o clique no vazio (nao fecha, nao da OK).
    ponteiro_alvo(x, 0, FOLHA_W, NV_TELA_H, NULL, NULL, 0, 0);
  }
  // CABECALHO, tres faixas (ver FOLHA_CAB_Y): kicker, titulo com os botoes a
  // direita, abas de addon.
  //
  // A LINHA DE AJUDA so aparece com a fileira de botoes em foco, NO LUGAR do
  // kicker: "Sem HDR" nao se explica pelo rotulo (e no degrau compacto o
  // filtro e so um icone), e uma linha propria empurrava as abas para baixo
  // a cada vez que o foco subia.
  const char *ajuda=NULL;
  if (grupo==-1) {
    if(botaoDe(foco)==BT_SEM_HDR)
      ajuda="Imagem preta com o áudio tocando? Recarrega esta fonte sem HDR nem Dolby Vision.";
    else if(botaoDe(foco)==BT_RECARREGAR)
      ajuda="Pergunta as fontes de novo a todos os addons.";
    else if(botaoDe(foco)==BT_CACHE)
      ajuda=soCache
        ? "Mostrando só fontes que o debrid já tem: tocam na hora. OK tira o filtro."
        : "Filtra para fontes que o debrid já tem (tocam na hora, sem baixar antes). OK liga o filtro.";
    else if(botaoDe(foco)==BT_DUB)
      ajuda=soDub
        ? "Mostrando só fontes com áudio em português. OK tira o filtro."
        : "Filtra para fontes dubladas ou com áudio em português. OK liga o filtro.";
    else if(botaoDe(foco)==BT_SO_MP4)
      ajuda=soMp4
        ? "Mostrando só containers MP4 (útil para achar Dolby Vision em MP4). OK tira o filtro."
        : "Filtra a lista para fontes em MP4. OK liga o filtro.";
  }
  if (ajuda) txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,ajuda,160,160,158,255,rw-FOLHA_TXT),tx,oy+70,anim);
  else { float cw = 0, ch = 0;
    if (contexto[0]) {
      // Kicker do Glass UI: 15/700 em caixa alta espacada, cinza 45%.
      cw = plrui_kicker(contexto,tx,oy+74,243,242,239,anim*.45f) + 22.0f; ch = 18.0f;
    }
    // AINDA HA ADDON RESPONDENDO, com fonte ja na lista (#221): a lista vai
    // crescer, e quem escolhe agora escolhe entre o que chegou. Na linha do
    // contexto, no acento, para nao disputar com o titulo nem com a ajuda.
    if (n > 0 && addons_ocupado() && rw-360-cw > 80) {
      if (cw > 0) gfx_cor((GfxRect){tx+cw-13.5f,oy+74+ch*.5f-2.5f,5,5},.5f,.5f,.5f,.49f,anim);
      txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,"Buscando mais fontes…",ai,(int)(ag*255),(int)(ab*255),255,rw-360-cw),tx+cw,oy+72,anim);
    } }
  medirCabecalho();
  txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_TITULO,"Fontes",243,242,239,255,titMaxCab),tx,oy+94,anim);
  // Os botoes alinhados a direita, centrados no titulo. Ordem do foco = ordem
  // visivel, da esquerda: [Sem HDR] Só MP4, Em cache, Dublado, Recarregar, Fechar.
  { int nbt=nBotoes(); float bx=lx+rw;
    for(int i=0;i<nbt;i++) bx-=bwCab[i]+(i?gapCab:0.0f);
    for(int i=0;i<nbt;i++){
      int b=botaoDe(i);
      GfxRect r={bx,oy+FOLHA_CAB_Y,bwCab[i],FOLHA_CHIP_H};
      if (ptr) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroFolhaBotao, NULL, i, 0);
      chipFolha(r,rotCab[i],rotWCab[i],icoCab[i],grupo==-1&&foco==i,botaoLigado(b),anim);
      bx+=bwCab[i]+gapCab;
    } }
  // SELETOR DE ADDON, segmentado: o selecionado em superficie clara, o foco
  // no acento. Quantas fontes cada addon tem, ao lado do nome.
  { int cnt[13]={0}; float iw[14], sx, segY=FOLHA_ABAS_Y+oy, maxW=rw+4.0f;
    const float pin=(FOLHA_ABAS_H-FOLHA_ABA_H)*.5f;
    TxtLinha nome[14], num[14];
    for(int i=0;i<n;i++){ if(!passaChips(i)) continue; cnt[0]++;
      for(int j=1;j<nProvedores;j++) if(!strcmp(provedores[j],lista[i].provedor)){cnt[j]++;break;} }
    for(int i=-1;i<nProvedores;i++){
      int qidx=i+1;
      int sel=i==filtro, foc=sel&&grupo==0, c=foc?ajustes_tinta_foco():sel?250:150;
      char q[16]; snprintf(q,sizeof q,"%d",i<0?ondever_n(alvoPedido):cnt[i]);
      nome[qidx]=txt_linha_corta(TXT_HERO_META,i<0?"Onde ver":i?provedores[i]:"Todos",c,c,c,255,260);
      num[qidx]=txt_linha(TXT_PG_FIM,q,foc?c:sel?170:100,foc?c:sel?170:100,foc?c:sel?170:100,255);
      iw[qidx]=nome[qidx].w+10.0f+num[qidx].w+44.0f; }
    int ini=-1; float soma;
    for(;;){ soma=2*pin; for(int i=ini;i<=filtro&&i<nProvedores;i++) soma+=iw[i+1]+6.0f;
      if(soma<=maxW||ini>=filtro) break; ini++; }
    soma=2*pin; int fim=ini;
    while(fim<nProvedores && soma+iw[fim+1]+6.0f<=maxW){ soma+=iw[fim+1]+6.0f; fim++; }
    if(fim==ini) fim=ini+1;
    sx=lx-2.0f;
    if (ajustes_vidro()) gfx_cor((GfxRect){sx,segY,soma-6.0f,FOLHA_ABAS_H},.5f,1,1,1,.05f*anim);
    else gfx_cor((GfxRect){sx,segY,soma-6.0f,FOLHA_ABAS_H},.5f,.113f,.118f,.137f,anim);
    sx+=pin;
    for(int i=ini;i<fim;i++){
      int qidx=i+1;
      GfxRect r={sx,segY+pin,iw[qidx],FOLHA_ABA_H}; int sel=i==filtro;
      if (ptr) ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, ponteiroFolhaFiltro, i, 0);
      if(sel&&grupo==0) focoFonte(r,.5f,anim);
      else if(sel) { if (ajustes_vidro()) gfx_cor(r,.5f,1,1,1,.12f*anim); else gfx_cor(r,.5f,.204f,.212f,.243f,anim); }
      txt_desenhar_alpha(nome[qidx],r.x+22,r.y+(FOLHA_ABA_H-nome[qidx].h)*.5f,anim);
      txt_desenhar_alpha(num[qidx],r.x+22+nome[qidx].w+10,r.y+(FOLHA_ABA_H-num[qidx].h)*.5f+1,anim);
      sx+=iw[qidx]+6.0f; } }
  automatica = automaticaDaFolha();
  melhor = melhorFolha = automaticoCom(!canalFolha);
  montar(automatica);
  nf=nOrdem;
  gfx_recorte(x,FOLHA_TOPO-8,w,folhaBase-(FOLHA_TOPO-8));
  // CABECALHOS DE GRUPO: "4K  ULTRA HD ........ 3 fontes", com um fio embaixo.
  for(int g=0;g<FOLHA_GRUPOS;g++){
    float y; char q[48];
    if(!secN[g]) continue;
    y=FOLHA_TOPO+secY[g]-rolagem;
    if(y+FOLHA_SEC_H<FOLHA_TOPO-8||y>NV_TELA_H) continue;
    { TxtLinha l=txt_linha(TXT_PAINEL_ITEM,GRUPO_NOME[g/2],242,242,240,255);
      txt_desenhar_alpha(l,tx,y+8,anim);
      // "4K  ULTRA HD  HDR": o nome da resolucao fica (dono gostou), e o
      // HDR vem no acento (e o que liga o modo da TV), o SDR no cinza.
      // Mesmo peso pequeno e espacado do "ULTRA HD" (dono, 02/10: "com o
      // mesmo peso menor e outra cor, que tava elegante"): o HDR num tom
      // champanhe — o acento puxado 60% para o cinza da legenda, para nao
      // gritar como selo — e o SDR num cinza um degrau mais claro.
      float hx=tx+l.w+16;
      if(GRUPO_SUB[g/2][0]) {
        hx+=caixaAlta(GRUPO_SUB[g/2],120,120,118,hx,y+15,anim)+10;
        hx+=caixaAlta("·",90,90,88,hx,y+15,anim)+10;
      }
      if(g%2==0) caixaAlta("HDR",(int)(120+(ai-120)*.45f),(int)(120+(ag*255-120)*.45f),(int)(118+(ab*255-118)*.45f)+18,hx,y+15,anim);
      else       caixaAlta("SDR",150,150,148,hx,y+15,anim); }
    snprintf(q,sizeof q,i18n(secN[g]==1?"%d fonte":"%d fontes"),secN[g]);
    { TxtLinha l=txt_linha(TXT_PG_FIM,q,110,110,108,255);
      txt_desenhar_alpha(l,tr-l.w,y+12,anim); }
    gfx_cor((GfxRect){lx,y+FOLHA_SEC_H-14,rw,1},0,1,1,1,.08f*anim);
  }
  for(int row=0;row<nf;row++) {
    float y=FOLHA_TOPO+linhaY[row]-rolagem, h=linhaH[row], cy, colW, txtW;
    int i=ordem[row], sel=grupo==1&&foco==row;
    const Stream *s=i>=0?&lista[i]:NULL;
    uint64_t tira, logos;
    if(y+h<FOLHA_TOPO-8 || y>NV_TELA_H) continue;
    GfxRect r={lx,y,rw,h};
    if (i <= -2) {
      OndeVer o;
      if (ptr) ponteiro_alvo(r.x,r.y,r.w,r.h,ponteiroFolhaLinha,NULL,row,0);
      if (sel) {
        if (ajustes_vidro()) gfx_cor(r,FOLHA_RAIO/h,1,1,1,.12f*anim);
        else gfx_cor(r,FOLHA_RAIO/h,.17f,.176f,.204f,anim);
      }
      if (ondever_item(alvoPedido,-i-2,&o)) desenharOnde(r,tx,tr,&o,sel,anim);
      continue;
    }
    if (i < 0) continue;
    if (ptr) {
      // So o que o recorte da lista deixa ver.
      float t = y < FOLHA_TOPO ? FOLHA_TOPO : y;
      float b = y + r.h > NV_TELA_H ? NV_TELA_H : y + r.h;
      if (b > t) ponteiro_alvo(r.x, t, r.w, b - t, ponteiroFolhaLinha, NULL, row, 0);
    }
    if(sel){
      // Foco so pela superficie clara: o contorno saiu a pedido do dono (02/10).
      // No solido a superficie e um cinza opaco um degrau acima do miolo.
      if (ajustes_vidro()) gfx_cor(r,FOLHA_RAIO/h,1,1,1,.12f*anim);
      else gfx_cor(r,FOLHA_RAIO/h,.17f,.176f,.204f,anim);
    }
    // COLUNA DA DIREITA: tamanho grande, addon embaixo. Medida primeiro: o
    // titulo e a fileira de logos param antes dela.
    { char gb[24]="";
      TxtLinha lg, lu, lp;
      int cg=sel?250:218;
      // No modo do addon o tamanho ja vem no texto dele; so o addon fica.
      if(s->tamanhoMB && !ajustes_fonte_texto_addon()) { snprintf(gb,sizeof gb,s->tamanhoMB>=102400?"%.0f":"%.1f",s->tamanhoMB/1024.0); plrui_decimal(gb); }
      lg=txt_linha(TXT_CW_TITULO,gb,cg,cg,cg-2,255);
      lu=txt_linha(TXT_PG_FIM,"GB",120,120,118,255);
      // TORRENT SEM DEBRID: quantos semeiam vai junto do addon. E o numero que
      // diz se a fonte abre (log da 2.0.0 no Android: "metadados nao chegaram
      // em 30 s"); a pessoa escolhia as cegas. "seeds" fica sem traducao: e o
      // termo que quem usa torrent conhece em qualquer idioma.
      { char pv[160];
        if(!s->url[0] && s->infoHash[0] && s->temSemeadores)
          snprintf(pv,sizeof pv,"%d seeds · %s",s->semeadores,s->provedor);
        else snprintf(pv,sizeof pv,"%s",s->provedor);
        lp=txt_linha_corta(TXT_PG_FIM,pv,sel?150:110,sel?150:110,sel?148:108,255,340); }
      colW=lp.w;
      if(gb[0] && lg.w+6+lu.w>colW) colW=lg.w+6+lu.w;
      cy=y+20+(temMarca(i,automatica)?FOLHA_MARCA_H:0);
      if(gb[0]){
        txt_desenhar_alpha(lu,tr-lu.w,cy+(lg.h-lu.h)-3,anim);
        txt_desenhar_alpha(lg,tr-lu.w-6-lg.w,cy,anim); }
      txt_desenhar_alpha(lp,tr-lp.w,gb[0]?cy+40:cy+6,anim); }
    txtW=tr-tx-colW-28;
    cy=y+20;
    // A MARCA: o que esta tocando (com o equalizador), a escolha anterior e a
    // automatica. Texto espacado no acento, e nao pilula: a pilula cheia era a
    // forma mais pesada da linha e o olho ia nela antes da qualidade.
    // O SELO "MELHOR PARA ESTA TV" mora na linha de marca, depois do texto
    // dela (ou sozinho): ao lado do titulo ele cortava o nome do conteudo.
    // So na fonte de maior pontuacao (stream_automatico: MP4/DV que a LG toca
    // primeiro), e e a unica forma cheia da lista — o olho vai nela.
    if(temMarca(i,automatica)){
      float mx=tx;
      int tf=ajustes_tinta_foco();
      if(i==atual){
        float w=caixaAlta("Reproduzindo agora",ai,(int)(ag*255),(int)(ab*255),tx,cy,anim);
        desenharAudioBars(tx+w+12,cy+1,14,anim,agora);
        mx=tx+w+12+FOLHA_AUDIO_N*(FOLHA_AUDIO_BAR*.5f+FOLHA_AUDIO_GAP*.6f)+18;
      } else if(i==preferida){
        const char *rot = i == automatica ? "Sua escolha anterior · automática" : "Sua escolha anterior";
        gfx_cor((GfxRect){tx,cy+5,7,7},.5f,ar,ag,ab,anim);
        mx=tx+16+caixaAlta(rot,ai,(int)(ag*255),(int)(ab*255),tx+16,cy,anim)+18;
      }
      // "MELHOR PARA ESTA TV" E MARCA, nao botao (Glass UI do player, 03/10):
      // o ponto e o texto espacado no acento, como as outras marcas — a
      // pilula cheia de acento era a forma mais pesada da lista.
      if(i==melhor && nOrdem>1){
        (void)tf;
        gfx_cor((GfxRect){mx,cy+5,7,7},.5f,ar,ag,ab,anim);
        // StreamFit (F03): the automatic pick is NOT changed by the measurement.
        // When its host was measured too slow for it, the mark keeps naming the
        // real choice but stops calling it the best for this TV.
        mx+=16+caixaAlta(fitPesada(i) ? "Escolha automática" : "Melhor para esta TV",
                         ai,(int)(ag*255),(int)(ab*255),mx+16,cy,anim)+18;
        // #284: no modo "do addon" a fileira de selos nao existe e a linha
        // nao dizia a resolucao; ela vai junto da marca ("4K · HDR10").
        if(ajustes_fonte_texto_addon()){
          char rs[16], fx[16], rq[40]; int hd;
          rotuloQualidade(s,rs,sizeof rs,fx,sizeof fx,&hd);
          snprintf(rq,sizeof rq,"%s · %s",rs,fx);
          mx+=caixaAlta(rq,ai,(int)(ag*255),(int)(ab*255),mx,cy,anim)+18;
        }
      }
      // StreamFit (F03): "above the connection" is a condition of this source on
      // this network, not a defect: champagne-grey like the HDR label, after any
      // other mark. It is why the row sits at the end of its group.
      if(fitPesada(i)){
        gfx_cor((GfxRect){mx,cy+5,7,7},.5f,.80f,.70f,.52f,anim);
        caixaAlta("Acima da conexão",204,178,132,mx+16,cy,anim);
      }
      cy+=FOLHA_MARCA_H;
    }
    // TITULO: o nome do conteudo (tituloConteudo). Fora do foco o titulo apaga um
    // degrau: com todas as linhas no mesmo branco a lista lia como uma massa
    // so (dono, 02/10: "tudo muito parecido").
    // O SELO "MELHOR PARA ESTA TV" vai ao lado do titulo, so na fonte de maior
    // pontuacao (stream_automatico: MP4/DV que a LG toca primeiro). E a unica
    // forma cheia da lista inteira, e por isso o olho vai nela.
    { char nome[96];
      int c=s->naoVideo?(sel?150:105):(sel?255:205);   // apagada: e aviso, nao video
      char ep[64]="";
      tira=0;
      if(ajustes_fonte_texto_addon()) tituloAddon(s,nome,sizeof nome);
      else tituloConteudo(s,nome,sizeof nome,ep,sizeof ep);
      { TxtLinha l=txt_linha_corta(TXT_CALLOUT,nome,c,c,c-2,255,txtW);
        // LOGO DO TITULO no lugar do nome (ajuste em teste): a mesma logo do
        // hero/detalhe, encostada a esquerda numa caixa da altura do nome.
        // Sem logo (ausente ou ainda baixando) fica o nome escrito.
        const CatItem *ci = ajustes_fonte_texto_logo() && itemFolha >= 0 ? cat_item(itemFolha) : NULL;
        const char *lu = ci ? logotitulo_url(ci, FOLHA_LOGO_W) : NULL;
        GLuint lt = lu ? tex_obter_larg_qualquer(lu, FOLHA_LOGO_W) : 0;
        float la = lt ? tex_aspecto(lu) : 0.0f;
        if (lt && la > .01f) {
          float lh = FOLHA_LOGO_H, lw = lh * la;
          if (lw > FOLHA_LOGO_W) { lw = FOLHA_LOGO_W; lh = lw / la; }
          gfx_tex_aspect_atual = 0.0f;
          // A base da logo fica FOLHA_LOGO_VAO acima dos selos (que comecam em
          // cy + 40), e nao no meio da altura do nome: centrada, a logo de 40 px
          // encostava nos selos.
          gfx_rect((GfxRect){tx, floorf(cy + 40.0f - FOLHA_LOGO_VAO - lh), lw, lh}, lt,
                   tex_marca_escura(lu) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1,
                   anim * (sel ? 1.0f : .80f));
          l.w = (int)(lw + .5f);
        } else
        txt_desenhar_alpha(l,tx,cy,anim);
        if(ep[0]){ int ce=sel?175:135;
          TxtLinha le=txt_linha(TXT_PG_FIM,ep,ce,ce,ce,255);
          // Na linha de base do nome, nao no meio da altura dele.
          if(l.w+14+le.w<=txtW) txt_desenhar_alpha(le,tx+l.w+14,cy+l.h-le.h-3,anim); } } }
    cy+=40;
    if(ajustes_fonte_texto_addon()) {
      LinhaAddon ls[FOLHA_ADDON_LINHAS];
      int nl=linhasAddon(s,ls,FOLHA_ADDON_LINHAS);
      // abertura da linha: a mesma mola da altura (alturaLinha)
      float abr = grupo==1 && row==foco ? abreFoco : row==linhaAnt ? abreAnt : 0.0f;
      for(int k=0;k<nl;k++){
        float ak = anim;
        if(k>=FOLHA_ADDON_FECHADA){ if(abr<.02f) break; ak = anim*abr; }
        // A primeira linha do addon costuma ser a de numeros (tamanho, taxa):
        // um degrau mais clara. As outras no cinza da especificacao.
        int c = k==0 ? (sel?215:180) : (sel?170:130);
        float px=tx, maxw=k==0?txtW:tr-tx, ly=cy+k*FOLHA_ADDON_LD;
        for(int pc=0;pc<ls[k].n && maxw-(px-tx)>30;pc++){
          if(ls[k].ic[pc]){
            // O icone na altura do texto, na mesma tinta; so vale se couber.
            gfx_icone((GfxRect){px,ly+3,22,22},ls[k].ic[pc],c/255.0f,c/255.0f,c/255.0f,ak);
            px+=22+8;
          }
          if(ls[k].tx[pc][0] && maxw-(px-tx)>30){
            TxtLinha lp=txt_linha_corta(TXT_PG_FIM,ls[k].tx[pc],c,c,c,255,maxw-(px-tx));
            txt_desenhar_alpha(lp,px,ly,ak);
            px+=lp.w+18;
          }
        }
      }
      continue;
    }
    // FILEIRA DE LOGOS: o pacote branco que o app ja embarca (deploy/app/art/
    // badges, o mesmo "Ghost" do Xperience), tingido no cinza da linha. MP4
    // no acento no fim:
    // na LG e o container que vale escolher (ver pontos()).
    logos=logosDa(s,tira);
    { float t=sel?.78f:.52f, lw=0, mpW=0;
      // ROTULO DE QUALIDADE no inicio da fileira (#202): "4K · HDR10". A
      // resolucao no branco da linha, a faixa no champanhe do HDR ou no cinza
      // do SDR (as cores do cabecalho do grupo), e o resto da fileira anda
      // para a direita por ele.
      float tx0=tx, txtW0=txtW;
      { char rs[16], fx[16]; int hd;
        rotuloQualidade(s,rs,sizeof rs,fx,sizeof fx,&hd);
        int cr=sel?240:200;
        TxtLinha lr=txt_linha(TXT_HERO_META,rs,cr,cr,cr-2,255);
        TxtLinha ls=txt_linha(TXT_HERO_META,"·",100,100,98,255);
        TxtLinha lf=hd ? txt_linha(TXT_HERO_META,fx,(int)(120+(ai-120)*.45f),(int)(120+(ag*255-120)*.45f),(int)(118+(ab*255-118)*.45f)+18,255)
                       : txt_linha(TXT_HERO_META,fx,sel?170:140,sel?170:140,sel?168:138,255);
        float lx2=tx;
        txt_desenhar_alpha(lr,lx2,cy+(FOLHA_SELO_H-lr.h)*.5f,anim); lx2+=lr.w+9;
        txt_desenhar_alpha(ls,lx2,cy+(FOLHA_SELO_H-ls.h)*.5f,anim); lx2+=ls.w+9;
        txt_desenhar_alpha(lf,lx2,cy+(FOLHA_SELO_H-lf.h)*.5f,anim); lx2+=lf.w+26;
        tx0=lx2; txtW0=txtW-(lx2-tx); if(txtW0<80) txtW0=80; }
      int ehMp4=!strcmp(containerDa(s),"MP4");
      TxtLinha mp;
      if(ehMp4){ mp=txt_linha(TXT_HERO_META,"MP4",ai,(int)(ag*255),(int)(ab*255),255); mpW=mp.w+18; }
      // TODOS OS LOGOS NA MESMA TINTA (dono, 02/10: "podemos deixar elas
      // todas brancas"). A rodada de "cor so no premium" deixava uns logos no
      // acento e outros brancos, e na TV isso lia como inconsistencia, nao
      // como hierarquia. O peso vem do brilho: apagado fora do foco, claro nele.
      // SELOS COLORIDOS (Ajustes, #198): o dono manteve a opcao (03/10). Ligada,
      // cada selo na peca da cor do seu grupo; desligada (o padrao do Glass
      // UI), todos na mesma tinta branca.
      if(selosPacoteVisiveis((Stream *)s)) {
        lw=desenharSelosPacote(s,tx0,cy,txtW0-mpW,FOLHA_SELO_H,
                               ajustes_selos_coloridos()?anim*(sel?1.0f:.85f):anim,t,1);
        // O Crave (servico canadense) nao esta em nenhum dos dois pacotes
        // embutidos: sai pela arte antiga para nao ser perdido.
        if(selospacote_ativo()<0 && (logos&badges_bit("p-crave"))) {
          float cw=badges_desenhar_tom(badges_bit("p-crave"),tx0+(lw>0?lw+16:0),cy,txtW0-mpW-lw-16,FOLHA_SELO_H,t,t,t,anim);
          if(cw>0) lw+=(lw>0?16:0)+cw; }
      } else if(logos) lw=ajustes_selos_coloridos()
        ? badges_desenhar_selos(logos,tx0,cy,txtW0-mpW,FOLHA_SELO_H,anim*(sel?1.0f:.85f))
        : badges_desenhar_tom(logos,tx0,cy,txtW0-mpW,FOLHA_SELO_H,t,t,t,anim);
      else if(!ehMp4){ char d[sizeof s->descricao];
        snprintf(d,sizeof d,"%s",s->descricao);
        for(char *p=d;*p;p++)if((unsigned char)*p<32)*p=' ';
        int c=sel?180:130;
        txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,d,c,c,c,255,txtW0),tx0,cy+(FOLHA_SELO_H-26)*.5f,anim); }
      if(ehMp4){ txt_desenhar_alpha(mp,tx0+(lw>0?lw+18:0),cy+(FOLHA_SELO_H-mp.h)*.5f,anim); lw+=(lw>0?18:0)+mp.w; }
      // O IDIOMA, em texto no fim da fileira (nao ha logo para ele).
      { const char *id=idiomaDa(s);
        if(id){ int c=sel?230:190;
          TxtLinha li=txt_linha(TXT_HERO_META,id,c,c,c,255);
          if(lw+18+li.w<=txtW0){ txt_desenhar_alpha(li,tx0+(lw>0?lw+18:0),cy+(FOLHA_SELO_H-li.h)*.5f,anim); lw+=(lw>0?18:0)+li.w; } } }
      // FORA DO CACHE: o debrid ainda vai baixar; tocar agora da o clipe de
      // aviso (ver o toast em app.c). Discreto, no fim da fileira — e uma
      // condicao da fonte, nao um defeito dela.
      if(s->naoVideo){ int c=sel?185:140;
        TxtLinha ln=txt_linha(TXT_HERO_META,"Não é vídeo",c,c-6,c-14,255);
        if(lw+18+ln.w<=txtW0) { txt_desenhar_alpha(ln,tx0+(lw>0?lw+18:0),cy+(FOLHA_SELO_H-ln.h)*.5f,anim); lw+=(lw>0?18:0)+ln.w; } }
      if(s->foraCache){ int c=sel?185:140;
        TxtLinha lf=txt_linha(TXT_HERO_META,"Fora do cache",c,c-6,c-14,255);
        if(lw+18+lf.w<=txtW0) { txt_desenhar_alpha(lf,tx0+(lw>0?lw+18:0),cy+(FOLHA_SELO_H-lf.h)*.5f,anim); lw+=(lw>0?18:0)+lf.w; } } }
    cy+=FOLHA_SELO_H+12;
    // O ARQUIVO, so na linha em foco: e o que distingue duas fontes iguais
    // (grupo de release, versao), e em toda linha era ruido.
    { float a=(sel?abreFoco:row==linhaAnt?abreAnt:0);
      const char *arq=arquivoDa(s);
      if(a>.02f && arq[0]){ char d[512];
        snprintf(d,sizeof d,"%s",arq);
        for(char *p=d;*p;p++)if((unsigned char)*p<32)*p=' ';
        txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,d,125,125,123,255,tr-tx),tx,cy,anim*a); }
      if(a>.02f){ char f1[160], f2[128]; StreamfitClasse fc=fitTexto2(i,f1,sizeof f1,f2,sizeof f2);
        if(f1[0]){
        float fy=cy+(arq[0]?FOLHA_ARQ_H:0);
        int c=fc==SF_DESCONHECIDA?118:150;
        txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,f1,c,c,c-2,255,tr-tx),tx,fy,anim*a);
        if(f2[0]){
          TxtLinha l2=fc==SF_PESADA ? txt_linha_corta(TXT_PG_FIM,f2,204,178,132,255,tr-tx)
                                    : txt_linha_corta(TXT_PG_FIM,f2,c,c,c-2,255,tr-tx);
          txt_desenhar_alpha(l2,tx,fy+FOLHA_FIT_H,anim*a); } } } }
  }
  // O TOPO DA LISTA ESMAECE em vez de cortar seco embaixo do seletor: a linha
  // que sobe some aos poucos, na cor da folha. So com a lista rolada: parada
  // no topo, o esmaecido apagaria o cabecalho "4K" do primeiro grupo.
  // Um degrade so no shader (GFX_BRILHO_TOPO, rampa de cima para baixo na
  // cor da folha) e NAO faixas empilhadas: o painel de 8 bits da OLED mostra
  // cada degrau de 1/255 como contorno num escuro de pouco contraste (medido
  // na C9 em 25/09, ver nv_dither em gfx.c), e faixas solidas sao degraus por
  // construcao. O modo passa pelo nv_dither; cor escura, entao nao sai no
  // nivel de efeitos leves.
  if (rolagem > 1.0f) { float e=rolagem>30.0f?1.0f:rolagem/30.0f;
    gfx_rect((GfxRect){x,FOLHA_TOPO-8,w,34},0,GFX_BRILHO_TOPO,0,1.0f,0,0,
             .071f,.075f,.086f,(ajustes_vidro()?.78f:.98f)*e*anim); }
  // A FONTE DOS DADOS, como rodape da lista (era uma linha solta entre o
  // cabecalho e as abas, na borda do cartao e nao na coluna do texto).
  if (nf && filtro == -1)
    txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,"Disponibilidade: TMDB / JustWatch. Abre o app, não o título.",110,110,108,255,tr-tx),
                       tx,FOLHA_TOPO+alturaTotal-rolagem+12.0f,anim);
  if(!nf && filtro == -1) {
    const int state=ondever_status(alvoPedido);
    const char *message=state==ONDE_BUSCANDO ? "Buscando onde assistir…" : state==ONDE_FALHOU ? "Não foi possível consultar a disponibilidade." : "Nenhum serviço informado para esta região.";
    txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,message,160,164,172,255,rw),lx,FOLHA_TOPO+36,anim);
  }
  if(!nFiltrados() && filtro != -1) {
    // A FOLHA VAZIA DIZ A CAUSA (B6/#107, D5). So quando a lista esta vazia
    // de verdade (n == 0): lista cheia com filtro de provedor que nao casa
    // nada fica na frase generica, porque ali a causa e o filtro na tela.
    // Ordem: torrent descartado por falta de debrid primeiro (se havia fonte,
    // "os addons nao tem" seria falso), depois o resumo da consulta.
    char causa[160], frase[320];
    const char *s=addons_estado()==ADD_BUSCANDO?"Buscando fontes nos addons…":"Nenhuma fonte direta disponível. Use Recarregar para tentar novamente.";
    if (addons_estado()!=ADD_BUSCANDO && n==0) {
      int tem = 0;
      if (descartadosSemDebrid > 0) {
        snprintf(causa,sizeof causa,i18n("%d fontes precisam de uma conta de debrid, e nenhuma está ligada"),descartadosSemDebrid);
        tem = 1;
      } else tem = addons_motivo_vazio(causa,sizeof causa);
      if (tem) { snprintf(frase,sizeof frase,"%s. %s",causa,i18n("Use Recarregar para tentar novamente.")); s=frase; }
    }
    // Com servicos de streaming na lista, a frase vem DEPOIS deles.
    txt_bloco(TXT_PG_FIM,s,196,199,204,tx,FOLHA_TOPO+20,rw-52,28,anim,3);
  }
  // Na ilha, de volta ao recorte dela (plrilha corta a forma inteira).
  if (ilha) gfx_recorte(ilhaR.x,ilhaR.y,ilhaR.w,ilhaR.h);
  else gfx_sem_recorte();
}

// A MEDIDA DE REDE DA FONTE QUE ESTA ABRINDO (o player expande o cartao de
// "Abrindo fonte"): a mesma evidencia da folha, agora, sem congelar nada. So
// devolve 1 quando ha medida real do host desta fonte (SF_BITRATE_ESTIMADO);
// qualquer outro caso (sem rede conhecida, sem tamanho, sem duracao, sem
// medida) e 0 e as linhas ficam vazias — nada de numero inventado.
int stream_fit_abrindo(const Stream *s, char *l1, size_t n1, char *l2, size_t n2) {
  StreamfitResultado r;
  if (n1) l1[0] = 0;
  if (n2) l2[0] = 0;
  if (!s) return 0;
  r = fitAutoResultado(s);
  if (r.razao != SF_BITRATE_ESTIMADO) return 0;
  fitLinhasDe(&r, l1, n1, l2, n2);
  return 1;
}
