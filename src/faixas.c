#include "faixas.h"
#include "idioma.h"
#include "player.h"
#include "video.h"
#include "addons.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "legenda.h"
#include "mkvass.h"
#include "assrender.h"
#include "ajustes.h"
#include "catalogo.h"
#include "linguas.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

// 1400 e nao 1180: com a terceira coluna, "Muito pequena" e "Escuro 100%" nao
// cabiam no espaco do valor e saiam cortados. A folha de AUDIO, que tem uma
// coluna so, usa uma fracao disto — ver faixas_desenhar.
#define FX_LARG   1400.0f
#define FX_LINHA   106.0f


// 3 e nao 2: a folha de legenda tem a LISTA e o ESTILO, e FX_COL_ESTILO e o
// indice 2. Com dois slots a coluna de estilo escrevia fora do vetor.
static int aberta, coluna, foco[3];
// ROLAGEM POR COLUNA, em LINHAS (nao em pixels): a folha desenhava todas as
// faixas a partir do topo e o painel tem altura limitada — com muitas legendas
// as ultimas caiam fora do painel e da tela. O foco chegava nelas, os olhos
// nao. Guardar quantas linhas foram roladas e o suficiente porque a altura da
// linha e fixa.
static int rolagem[3];
// Quantas linhas cabem no painel. Calculada no desenho (depende da altura
// escolhida ali) e lida pelo tratamento de tecla, que roda antes.
static int visiveis = 8;
static void ajustarRolagem(void);
static float anim;

static void corFocoFaixa(float *r, float *g, float *b) {
  float ar, ag, ab, lum, k = 0.74f;
  ajustes_acento(&ar, &ag, &ab);
  lum = 0.2126f * ar + 0.7152f * ag + 0.0722f * ab;
  if (lum > 0.88f) k = 0.88f;
  *r = 0.055f + (ar - 0.055f) * k;
  *g = 0.058f + (ag - 0.058f) * k;
  *b = 0.068f + (ab - 0.068f) * k;
}

static void superficieFocoFaixa(GfxRect r, float a) {
  float fr, fg, fb;
  corFocoFaixa(&fr, &fg, &fb);
  gfx_cor(r, 0.18f, fr, fg, fb, a);
  gfx_rect(r, 0, GFX_BRILHO_TOPO, 0.18f, 0.18f, 0, 0.5f,
           1, 1, 1, 0.10f * a);
}
// Qual legenda EXTERNA (OpenSubtitles) esta valendo, em indice da lista
// combinada — ou -1 quando a ativa e embutida ou nao ha nenhuma.
//
// Isto vive aqui e nao no video.c porque o pipeline nao devolve essa
// informacao: video_legenda_externa manda o setSubtitleSource com a URL e o
// legAtual do video.c fica intocado, apontando para a legenda EMBUTIDA de
// antes. Sem esta variavel, escolher uma legenda do OpenSubtitles fazia a
// marca de "ativa" ficar em outra linha (ou em "Desativada") e a folha
// reabria com o foco no lugar errado — a legenda certa tocava, so a folha
// mentia sobre qual era.
static int legExterna = -1;

// LEGENDA EMBUTIDA ASS PELO OVERLAY (#92, fase 3). `legOverlay` e o indice da
// faixa embutida cujo texto o mkvass.c esta colhendo do MKV por Range para o
// overlay do app desenhar — o pipeline da TV fica com a legenda DESLIGADA
// (video_escolher_legenda(-1)), entao video_legenda_atual() diz -1 e, sem
// esta variavel, a folha marcaria "Nenhuma" como ativa. Mesmo motivo do
// legExterna acima.
//
// `legOverlayNoGo` e a faixa em que o mkvass DESISTIU (arquivo sem indice da
// legenda, servidor sem Range): a folha voltou a entregar a faixa ao pipeline
// e mostra o motivo; escolher a mesma faixa de novo vai direto ao pipeline,
// sem tentar outra vez.
//
// Na LG o player nativo e silenciado enquanto o app coleta e compoe a faixa;
// em no-go, a selecao volta ao uMS. NA SAMSUNG ESTE RAMO NAO RODA: o
// video_tizen.c nao preenche `codec`, entao ehAss e sempre falso la e a faixa
// vai ao AVPlay, cujo texto o player desenha pelo onsubtitlechange (#122).
static int legOverlay = -1, legOverlayNoGo = -1;

// Faixa embutida escolhida ANTES de a sonda do cabecalho voltar (#92, webOS
// 25). Sem a sonda nao se sabe o codec nem o ordinal, e a 1.4.2 mandava a
// faixa para a TV EM SILENCIO — sem log, sem aviso — e la ficava: era o "liga
// e desliga, metade da frase" do relato, numa TV em que o buffer demorava a
// dar os 20 s que disparavam a sonda. Agora a faixa vai a TV so ENQUANTO a
// sonda corre (disparada na hora); quando ela volta, faixas_atualizar decide
// e diz no log por que ficou onde ficou.
static int legOverlayEsperando = -1;

// LEGENDA AUTOMATICA DA SESSAO (#129): 1 do inicio de uma reproducao ate a
// decisao (ligou, nao havia o que ligar, ou a pessoa escolheu na folha).
// `legAutoDesde` e o instante em que o video ficou pronto, base dos prazos.
static int legAuto;
static Uint32 legAutoDesde;

// NO-GO PASSAGEIRO x DEFINITIVO. Um Range que falhou (rede, timeout, 5xx,
// freio do CDN, o servidor que devolveu o arquivo inteiro uma vez) nao e
// motivo para entregar a faixa a TV DE VEZ: o mkvass tenta de novo com recuo
// (mkvass_recuo_ms: 2, 5, 15, 30, 60 s...), SEM LIMITE enquanto a faixa estiver
// escolhida. Nas primeiras MKVASS_TENTATIVAS_OVERLAY o overlay fica com o que
// ja colheu; dali em diante (ou logo, se nao colheu nada) a TV desenha POR
// ENQUANTO (`legOverlayTV`) e, quando uma tentativa volta a entregar fala
// nova, a faixa volta ao app. So o definitivo (nao e MKV, codec, sem indice,
// Range recusado de novo, recusa HTTP definitiva) devolve a faixa de vez, com o
// motivo no log e no aviso. Contado por ESCOLHA de faixa; `legOverlayFalhas`
// zera quando chega fala nova (`legOverlayColhidos` e a marca).
//
// #92, 1.4.5: tres falhas seguidas davam "a TV vai desenhar (falha de rede)"
// e a faixa ficava na TV — que corta metade das falas — ate o fim do episodio.
static int legOverlayFalhas, legOverlayRecusas, legOverlayNoGoEstado;
static int legOverlayTV, legOverlayColhidos;
static Uint32 legOverlayRetomar;       // 0 = nada agendado

static int ehAss(const VideoFaixa *f) {
  return f && (!strncmp(f->codec, "S_TEXT/ASS", 10) || !strncmp(f->codec, "S_TEXT/SSA", 10));
}

// O overlay do app assume a faixa embutida `i` (ordinal `ord` no arquivo): a
// legenda nativa da TV e DESLIGADA (video_escolher_legenda(-1) manda
// setSubtitleEnable false ao uMS / desliga no AVPlay) e o mkvass comeca a
// colher. A linha de log e a prova de que o app assumiu — se a TV continuar
// desenhando por cima, o firmware ignorou o setSubtitleEnable, e isso e
// outro bug (a resposta do uMS sai logo abaixo como "[video] {...}").
static void overlayAssumir(int i, int ord) {
  const VideoFaixa *f = video_legenda(i);
  video_escolher_legenda(-1);
  mkvass_iniciar_ordinal(video_url_atual(), ord);
  legOverlay = i;
  legOverlayFalhas = legOverlayRecusas = 0; legOverlayRetomar = 0;
  legOverlayTV = legOverlayColhidos = 0;
  printf("[legenda] faixa %d (%s, %s) -> app: ordinal %d; legenda nativa desligada\n",
         i, f ? f->rotulo : "?", f ? f->codec : "?", ord);
  fflush(stdout);
}

// Por que a faixa embutida `i` NAO vai ao overlay do app. Uma string, para o
// log e para a folha nao divergirem.
static const char *motivoTV(int i) {
  const VideoFaixa *f = video_legenda(i);
  int sond = video_mkv_sondado();
  if (!f) return "faixa inexistente";
  if (!video_url_atual()[0]) return "sem URL da fonte";
  if (i == legOverlayNoGo) return "mkvass ja desistiu desta faixa nesta sessao";
  if (sond == 2) return "fonte nao e MKV (nao ha sonda)";
  if (sond == 0) return "sonda do cabecalho ainda nao voltou";
  if (!f->codec[0]) return "sonda voltou sem par para esta faixa (ver [mkv] legendas da TV x arquivo)";
  if (!ehAss(f)) return "codec nao e ASS/SSA: a TV desenha bem";
  if (video_legenda_ordinal_mkv(i) < 0) return "sem ordinal no arquivo";
  return "?";
}

// Chamada quando uma sessao de reproducao nova comeca: a legenda externa e da
// sessao, nao do aparelho. Sem isto o titulo seguinte abriria a folha marcando
// como ativa uma legenda que nao foi escolhida para ele.
void faixas_reiniciar(void) {
  legExterna = -1; legOverlay = legOverlayNoGo = legOverlayEsperando = -1; aberta = 0;
  legOverlayFalhas = legOverlayRecusas = legOverlayNoGoEstado = 0; legOverlayRetomar = 0;
  legOverlayTV = legOverlayColhidos = 0;
  mkvass_parar(); legenda_desligar();
  legAuto = 1; legAutoDesde = 0;
}

// Indice da legenda que a folha deve marcar como ATIVA.
static int legendaAtiva(void) {
  if (legExterna >= 0) return legExterna;
  if (legOverlay >= 0) return legOverlay;
  return video_legenda_atual();
}

// FOLHAS SEPARADAS: 0 = so AUDIO, 1 = LEGENDA (lista + estilo).
//
// Elas eram UMA folha com as duas colunas lado a lado, por decisao minha: "duas
// telas obrigariam a sair e voltar para conferir o par". O dono pediu separado,
// e a referencia lhe da razao — a TCL tem um overlay proprio de legenda
// (SubtitleSelectionOverlay), com o seletor de estilo dentro dele. Comparar o
// par audio+legenda ao mesmo tempo era um caso que eu supus e ninguem pediu.
static int modo;
// Coluna dentro da folha de LEGENDA: 0 = lista, 1 = estilo.
#define FX_COL_ESTILO 2
#define FX_N_ESTILO   9

static int nLinhas(int col);

void faixas_abrir(void) { faixas_abrir_em(0); }

// Abre JA NA COLUNA que o botao pediu. O player tem um icone de audio e um de
// legenda, e os dois abriam esta folha do mesmo jeito, com o foco no audio:
// apertar "legendas" e cair no audio faz os dois botoes parecerem o mesmo
// botao — foi exatamente o que o dono relatou. O painel continua sendo UM so,
// com as duas colunas lado a lado (comparar o par escolhido e o motivo dele
// existir); o que muda e onde o foco comeca.
void faixas_abrir_em(int col) {
  int n;
  aberta = 1;
  modo = (col == 1) ? 1 : 0;
  coluna = modo;                 // audio -> col 0; legenda -> col 1
  foco[0] = video_audio_atual();
  // A legenda pode estar desligada (-1); a primeira linha da coluna e sempre
  // "Desativada", entao o indice da lista e deslocado em um.
  foco[1] = legendaAtiva() + 1;
  // Abriu a folha de LEGENDAS: e agora que idioma, codec e ordinal importam.
  // A sonda esperava 20 s de buffer (video.c) — numa fonte lenta a folha
  // abria com "Legenda 1..N" e sem selo ASS, e a escolha ia a TV.
  if (modo) video_sondar_mkv_agora();
  // Clamp nas duas colunas. A lista de legendas CRESCE durante a sessao (as do
  // OpenSubtitles chegam depois) e a de audio so existe apos o sourceInfo:
  // guardar um indice de antes e reabrir sem conferir poe o foco fora do vetor.
  { int c; for (c = 0; c < 3; c++) {
      n = nLinhas(c);
      if (foco[c] >= n) foco[c] = n > 0 ? n - 1 : 0;
      if (foco[c] < 0)  foco[c] = 0;
    rolagem[c] = 0;
    } }
}

int faixas_aberta(void) { return aberta; }

static int nLegendas(void) {
  int n = video_n_legenda() + addons_n_legendas();
  return n;
}

static int nLinhas(int col) {
  if (col == FX_COL_ESTILO) return FX_N_ESTILO;
  if (col == 0) { int n = video_n_audio(); return n; }
  return nLegendas() + 1;   // +1 pela linha "Desativada"
}

// --- COLUNA DE ESTILO --------------------------------------------------------
//
// Oito linhas "rotulo: valor". OK cicla o valor e aplica NA HORA — as
// personalizacoes abaixo usam somente metodos presentes no firmware. A ultima
// linha restaura o conjunto inteiro sem exigir dezenas de toques no controle.
static const char *const EST_ROT[FX_N_ESTILO] = {
  "Tamanho", "Fonte OpenSubtitles", "Cor", "Opacidade", "Fundo", "Posição", "Borda", "Atraso",
  "Restaurar padrão"
};
static const char *const EST_FUNDO[5] = { "Nenhum", "Escuro 25%", "Escuro 50%",
                                          "Escuro 75%", "Escuro 100%" };
static const char *const EST_BORDA[3] = { "Nenhuma", "Contorno", "Sombra" };
static const char *const EST_OPAC[4]  = { "100%", "75%", "50%", "25%" };

/* Com o ASS desenhado pelo libass, fonte, cor, fundo, posicao e borda sao do
 * arquivo: mexer nelas desmontava karaoke e placas (ou nao fazia nada). A
 * linha continua na folha, esmaecida, dizendo por que nao muda — como o app
 * web faz desde o 1.2.0. Tamanho vira escala proporcional; opacidade e atraso
 * valem igual. */
static int estiloPreservadoAss(int linha) {
  return assrender_ativo() && (linha == 1 || linha == 2 || linha == 4 ||
                               linha == 5 || linha == 6);
}

static void valorEstilo(int linha, char *dst, size_t tam) {
  const VideoLegendaEstilo *e = player_leg_estilo();
  if (estiloPreservadoAss(linha)) {
    snprintf(dst, tam, "%s", i18n("Preservado pelo ASS"));
    return;
  }
  switch (linha) {
    case 0:
      if (assrender_ativo())
        snprintf(dst, tam, "%d%% \xc2\xb7 ASS \xc3\x97%.2f", e->tamanho, e->tamanho / 120.0);
      else
        snprintf(dst, tam, "%d%%", e->tamanho);
      break;
    case 1: snprintf(dst, tam, "%s", TXT_FAMILIAS_PT[e->familia >= 0 && e->familia < TXT_FAMILIA_N ? e->familia : 0]); break;
    case 2: snprintf(dst, tam, "%s", VIDEO_LEG_CORES_PT[e->cor % VIDEO_LEG_NCORES]); break;
    case 3: snprintf(dst, tam, "%s", EST_OPAC[e->opacidade > 3 ? 3 : e->opacidade]); break;
    case 4: snprintf(dst, tam, "%s", EST_FUNDO[e->fundo > 4 ? 4 : e->fundo]); break;
    // O uMS aceita -3..4; a folha mostra 1..8 porque "posicao -3" nao diz nada
    // a quem esta olhando a tela.
    case 5: snprintf(dst, tam, i18n("%d de 8"), e->posicao + 1); break;
    case 6: snprintf(dst, tam, "%s", EST_BORDA[e->borda > 2 ? 2 : e->borda]); break;
    case 7: {
      int a = e->atrasoMs;
      if (!a) snprintf(dst, tam, "0 s");
      else    snprintf(dst, tam, "%+.2f s", a / 1000.0f);
      break; }
    default: snprintf(dst, tam, "Aplicar"); break;
  }
}

static void ciclarEstilo(int linha) {
  VideoLegendaEstilo *e = player_leg_estilo();
  if (estiloPreservadoAss(linha)) return;
  switch (linha) {
    case 0: e->tamanho += 10; if (e->tamanho > 200) e->tamanho = 50; break;
    case 1: e->familia = (e->familia + 1) % TXT_FAMILIA_N; break;
    // COR: marca que a pessoa mexeu — dai em diante ela vence a cor que o
    // arquivo ASS pede (ver player_leg_estilo_tocou em player.h).
    case 2: e->cor     = (e->cor + 1) % VIDEO_LEG_NCORES; player_leg_estilo_tocou(PLR_LEG_COR); break;
    case 3: e->opacidade = (e->opacidade + 1) % 4; break;
    case 4: e->fundo   = (e->fundo + 1) % 5; break;
    case 5: e->posicao = (e->posicao + 1) % 8; break;
    case 6: e->borda   = (e->borda + 1) % 3; break;
    // -5 s a +5 s de 250 em 250 ms, voltando ao inicio. Passo menor exigiria
    // dezenas de toques para sair do lugar num controle remoto.
    case 7:
      e->atrasoMs += 250;
      if (e->atrasoMs > 5000) e->atrasoMs = -5000;
      break;
    default:
      *e = (VideoLegendaEstilo){ 120, 0, 0, 3, 1, 0, 0, TXT_FAMILIA_INTER };
      // Restaurar e voltar ao normal do app, e o normal e respeitar o arquivo.
      player_leg_estilo_tocou(PLR_LEG_NADA);
      break;
  }
  player_leg_estilo_mudou();
}

// Rotulo da linha `i` da coluna de legenda. Ate video_n_legenda() sao as
// embutidas; depois vem as do OpenSubtitles.
static const char *rotuloLegenda(int i, const char **marca) {
  int emb = video_n_legenda();
  *marca = NULL;
  if (i < emb) {
    const VideoFaixa *f = video_legenda(i);
    // SELO "ASS" (#92): a faixa S_TEXT/ASS e a que o pipeline da TV desenha
    // sem posicao e comendo eventos simultaneos. Dizer isso na folha e o que
    // permite a pessoa preferir uma legenda externa enquanto a faixa
    // embutida nao passa pelo overlay proprio.
    if (i == legOverlayEsperando)
      *marca = i18n("Incorporada (lendo o \xc3\xadndice do arquivo\xe2\x80\xa6)");
    else if (ehAss(f)) {
      if (i == legOverlay) {
        int e = mkvass_estado();
        *marca = legOverlayTV
               ? i18n("ASS: a TV desenha por enquanto (o app tenta de novo\xe2\x80\xa6)")
               : legOverlayRetomar
               ? i18n("Incorporada \xc2\xb7 ASS (desenhada pelo app, tentando de novo\xe2\x80\xa6)")
               : e == MKVASS_PREPARANDO
               ? i18n("Incorporada \xc2\xb7 ASS (lendo o \xc3\xadndice do arquivo\xe2\x80\xa6)")
               : mkvass_varredura()
               ? i18n("Incorporada \xc2\xb7 ASS (desenhada pelo app, varrendo o arquivo)")
               : i18n("Incorporada \xc2\xb7 ASS (desenhada pelo app)");
      } else if (i == legOverlayNoGo) {
        int e = legOverlayNoGoEstado;
        *marca = e == MKVASS_NOGO_SEM_RANGE ? i18n("ASS: a TV desenha (servidor sem Range)")
               : e == MKVASS_NOGO_HTTP     ? i18n("ASS: a TV desenha (o servidor recusou)")
               : e == MKVASS_NOGO_NAO_MKV  ? i18n("ASS: a TV desenha (a fonte n\xc3\xa3o \xc3\xa9 MKV)")
               : e == MKVASS_NOGO_FAIXA    ? i18n("ASS: a TV desenha (faixa n\xc3\xa3o \xc3\xa9 ASS)")
               : i18n("ASS: a TV desenha (arquivo sem \xc3\xadndice)");
      } else
        *marca = i18n("Incorporada \xc2\xb7 ASS (a TV pode cortar falas)");
    }
    return f ? f->rotulo : "";
  }
  { const Legenda *l = addons_legenda(i - emb);
    if (!l) return "";
    *marca = "OpenSubtitles";
    return l->rotulo; }
}

// Liga a legenda `i` da lista combinada (-1 desliga; embutidas primeiro, depois
// as de addon). E o OK da folha, e tambem o que a legenda automatica usa: os
// dois tem de passar pelo mesmo caminho, senao uma faixa ASS escolhida sozinha
// iria a TV sem o overlay e sem a linha de log que a folha deixa.
static void escolherLegenda(int i) {
  {
    int emb = video_n_legenda();
    const VideoFaixa *fe = (i >= 0 && i < emb) ? video_legenda(i) : NULL;
    int vaiAoApp = fe && ehAss(fe) && i != legOverlayNoGo && video_url_atual()[0] &&
                   video_legenda_ordinal_mkv(i) >= 0;
    // Qualquer escolha encerra a colheita anterior: o fio do mkvass nao pode
    // continuar entregando ao overlay uma faixa que a pessoa acabou de trocar.
    // MENOS quando a escolha vai ao overlay: mkvass_iniciar_ordinal ja troca a
    // geracao (o fio velho sai sozinho) e, se for a faixa da PRE-BUSCA (#92,
    // v1.4.7), adota o fio vivo com o que ele ja leu antes do video — parar
    // aqui jogaria isso fora.
    if (!vaiAoApp) mkvass_parar();
    legOverlay = -1; legOverlayEsperando = -1; legOverlayRetomar = 0; legOverlayTV = 0;
    if (i < 0)        { video_escolher_legenda(-1); legenda_desligar(); legExterna = -1; }
    else if (i < emb) {
      const VideoFaixa *f = video_legenda(i);
      int ord = video_legenda_ordinal_mkv(i);
      legenda_desligar(); legExterna = -1;
      // FAIXA ASS: o overlay do app assume (#92). O pipeline fica com a legenda
      // desligada e o mkvass colhe o texto do MKV a frente do playhead; se ele
      // declarar no-go, faixas_atualizar devolve a faixa ao pipeline. Uma
      // faixa em que ja desistimos vai direto ao pipeline.
      //
      // PELO ORDINAL NOS DOIS ALVOS (#92). Na LG isto passava f->numero — o
      // trackNum da TV — como se fosse TrackNumber do Matroska, e o overlay
      // colhia a faixa de outra lingua. O ordinal e resolvido contra as
      // TrackEntry na sonda do cabecalho.
      //
      // SEM ORDINAL AINDA (sonda nao voltou): a faixa vai a TV por enquanto,
      // a sonda e disparada ja e faixas_atualizar troca para o overlay quando
      // ela voltar com o par. NUNCA em silencio: cada caminho deixa uma linha
      // "[legenda] faixa N -> TV: motivo" — e a linha que faltou no #92 para
      // separar "o app desistiu" de "a TV desenha mal".
      if (vaiAoApp)
        overlayAssumir(i, ord);
      else {
        const char *motivo = motivoTV(i);
        if (f && i != legOverlayNoGo && video_url_atual()[0] && video_mkv_sondado() == 0) {
          legOverlayEsperando = i;
          video_sondar_mkv_agora();
        }
        printf("[legenda] faixa %d (%s, codec=%s) -> TV: %s\n", i, f ? f->rotulo : "?",
               f && f->codec[0] ? f->codec : "?", motivo);
        fflush(stdout);
        video_escolher_legenda(i);
      }
    }
    else {
      const Legenda *l = addons_legenda(i - emb);
      // So marca como ativa se houve o que aplicar: sem a URL o uMS nao recebe
      // nada, e a folha diria "ativa" sobre uma legenda que nunca subiu.
      if (l) {
        /* A fonte e os 16 tamanhos agora sao nossos, nao do firmware webOS. */
        video_escolher_legenda(-1); legenda_carregar(l->url); legExterna = i;
      }
    }
  }
}

static void aplicar(void) {
  if (coluna == 0) {
    video_escolher_audio(foco[0]);
  } else {
    // A pessoa escolheu: a automatica nao mexe mais nesta sessao, nem se a
    // escolha foi "Desativada".
    legAuto = 0;
    escolherLegenda(foco[1] - 1);
  }
}

// LEGENDA AUTOMATICA (#129). Roda a cada quadro enquanto `legAuto` esta de pe,
// e so decide quando da para decidir bem (ling_legenda_auto). Os prazos existem
// porque as duas listas podem nunca "fechar": a sonda do MKV so dispara com
// buffer saudavel, e numa fonte lenta isso demora; o fio de legendas consulta
// cada addon com 25 s de teto. Vencido o prazo, decide-se com o que ha.
#define FX_AUTO_EMB_MS  30000u   // espera pelos idiomas das embutidas
#define FX_AUTO_FIM_MS  60000u   // desiste de vez: legenda ligada no minuto 5 assusta
static void legendaAutomatica(Uint32 agora) {
  const char *emb[NV_FAIXA_MAX], *add[LEG_MAX];
  const CatItem *ci;
  int nEmb, nAdd = 0, embFechado, addFechado = 1, i, r;
  Uint32 passou;
  if (!legAuto || aberta || !player_aberto() || !player_com_video()) return;
  // Canal ao vivo nao tem legenda de addon nem idioma no arquivo que valha.
  if (player_id_canal()[0]) { legAuto = 0; return; }
  // A lista de faixas so existe depois do sourceInfo (LG) / lerFaixas (Tizen).
  // Antes dele a sonda "ja voltou" por falta de pendencia (ver
  // video_mkv_sondado) e as embutidas pareceriam fechadas e vazias.
  if (!legAutoDesde) legAutoDesde = agora | 1u;
  passou = agora - legAutoDesde;
  if (!video_n_audio() && !video_n_legenda() && passou < 8000u) return;
  nEmb = video_n_legenda();
  if (nEmb > NV_FAIXA_MAX) nEmb = NV_FAIXA_MAX;
  for (i = 0; i < nEmb; i++) { const VideoFaixa *f = video_legenda(i); emb[i] = f ? f->idioma : ""; }
  embFechado = video_mkv_sondado() != 0 || passou >= FX_AUTO_EMB_MS;
  // So confia na lista dos addons quando ela e DESTE titulo: sem imdb o app
  // nao pede legenda nenhuma (app.c), e o que estiver em memoria e do anterior.
  ci = cat_item(player_indice());
  if (ci && ci->imdb[0]) {
    nAdd = addons_n_legendas();
    if (nAdd > LEG_MAX) nAdd = LEG_MAX;
    for (i = 0; i < nAdd; i++) { const Legenda *l = addons_legenda(i); add[i] = l ? l->idioma : ""; }
    addFechado = addons_legendas_prontas();
  }
  if (passou >= FX_AUTO_FIM_MS) embFechado = addFechado = 1;
  r = ling_legenda_auto(ling_legenda(), emb, nEmb, embFechado, add, nAdd, addFechado);
  if (r == LING_AUTO_ESPERA) return;
  legAuto = 0;
  if (r == LING_AUTO_NADA) {
    if (ling_legenda()[0] && strcasecmp(ling_legenda(), "none"))
      printf("[legenda] automatica: nada em '%s' (%d embutida(s), %d de addon)\n",
             ling_legenda(), nEmb, nAdd);
    fflush(stdout);
    return;
  }
  // Ja esta nela (o arquivo marcou a faixa como padrao): nao religa — a nao
  // ser que seja ASS com a TV desenhando: ai o overlay do app assume, que e o
  // motivo do #92 (e o que adota a pre-busca feita antes do video).
  if (r == legendaAtiva() &&
      !(r < nEmb && legOverlay != r && ehAss(video_legenda(r)) && video_legenda_ordinal_mkv(r) >= 0))
    return;
  printf("[legenda] automatica: '%s' -> %s %d (%s) aos %u ms\n", ling_legenda(),
         r < nEmb ? "embutida" : "addon", r < nEmb ? r : r - nEmb,
         r < nEmb ? emb[r] : add[r - nEmb], (unsigned)passou);
  fflush(stdout);
  escolherLegenda(r);
}

void faixas_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberta || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE) { aberta = 0; return; }
  // Esquerda/direita andam entre a LISTA e o ESTILO, e so na folha de legenda.
  // Na de audio nao ha para onde ir — antes elas pulavam para a coluna de
  // legenda, que e justamente o que fazia os dois botoes do player parecerem o
  // mesmo botao.
  if (k == SDLK_LEFT)  { if (modo && coluna == FX_COL_ESTILO) coluna = 1; return; }
  if (k == SDLK_RIGHT) { if (modo && coluna == 1) coluna = FX_COL_ESTILO; return; }
  if (k == SDLK_UP)    { if (foco[coluna] > 0) foco[coluna]--; ajustarRolagem(); return; }
  if (k == SDLK_DOWN)  { if (foco[coluna] < nLinhas(coluna) - 1) foco[coluna]++;
                         ajustarRolagem(); return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    // Na coluna de ESTILO o OK CICLA o valor e aplica na hora, sem fechar: o
    // dono precisa VER a legenda mudar para escolher, e fechar a folha a cada
    // toque tiraria a lista de baixo dos olhos dele.
    if (coluna == FX_COL_ESTILO) { ciclarEstilo(foco[FX_COL_ESTILO]); return; }
    aplicar(); aberta = 0; return;
  }
}

static const char *motivoNoGo(int e) {
  return e == MKVASS_NOGO_NAO_MKV    ? "nao e MKV"
       : e == MKVASS_NOGO_SEM_RANGE  ? "servidor sem Range"
       : e == MKVASS_NOGO_FAIXA      ? "faixa nao e ASS"
       : e == MKVASS_NOGO_SEM_INDICE ? "sem indice da faixa"
       : e == MKVASS_NOGO_SEM_REL    ? "sem CueRelativePosition"
       : e == MKVASS_NOGO_REDE       ? "rede"
       : e == MKVASS_NOGO_HTTP       ? "servidor recusou"
       : e == MKVASS_NOGO_RESTO      ? "servidor recusou o resto" : "?";
}

// O aviso da queda DEFINITIVA, com o motivo que o mkvass viu. Antes todo
// no-go que nao fosse "sem Range" ou "rede" dizia "arquivo sem indice".
static void avisarQueda(int e) {
  char b[160]; int http = 0;
  mkvass_ultima_falha(&http, NULL);
  if (e == MKVASS_NOGO_HTTP && http > 0)
    snprintf(b, sizeof b, i18n("Legenda ASS: a TV vai desenhar (o servidor recusou: HTTP %d)"), http);
  else
    snprintf(b, sizeof b, "%s",
             e == MKVASS_NOGO_SEM_RANGE ? i18n("Legenda ASS: a TV vai desenhar (servidor sem Range)")
             : e == MKVASS_NOGO_HTTP    ? i18n("Legenda ASS: a TV vai desenhar (o servidor recusou)")
             : e == MKVASS_NOGO_NAO_MKV ? i18n("Legenda ASS: a TV vai desenhar (a fonte n\xc3\xa3o \xc3\xa9 MKV)")
             : e == MKVASS_NOGO_FAIXA   ? i18n("Legenda ASS: a TV vai desenhar (faixa n\xc3\xa3o \xc3\xa9 ASS)")
             : i18n("Legenda ASS: a TV vai desenhar (arquivo sem \xc3\xadndice)"));
  player_toast(b, 6000);
}

void faixas_atualizar(float dt, Uint32 agora) {
  anim = anim_mola(anim, aberta ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  legendaAutomatica(agora);
  // Recuo vencido: a MESMA faixa de novo. O overlay nao foi desligado — o que
  // ja estava colhido continua na tela, e o fio novo retoma do sidecar parcial.
  if (legOverlay >= 0 && legOverlayRetomar && (Sint32)(agora - legOverlayRetomar) >= 0) {
    legOverlayRetomar = 0;
    printf("[legenda] faixa %d: nova tentativa %d do mkvass (%s)\n", legOverlay, legOverlayFalhas,
           legOverlayTV ? "a TV desenha enquanto isso" : "overlay do app mantido");
    fflush(stdout);
    // Com a TV desenhando, o fio novo so religa o overlay com fala NOVA: o
    // sidecar parcial nao volta por cima da legenda da TV.
    if (legOverlayTV) mkvass_retomar_segurando(); else mkvass_retomar();
  }
  // PROGRESSO: chegou fala nova desde a ultima falha (ou a faixa fechou).
  // Zera a contagem, e se a TV estava desenhando por enquanto, a faixa VOLTA
  // ao overlay: nativa desligada, o app desenha o que acabou de entregar.
  if (legOverlay >= 0 && !legOverlayRetomar && !mkvass_nogo() &&
      (legOverlayFalhas || legOverlayTV)) {
    int col = 0, e = mkvass_estado();
    mkvass_estatisticas(NULL, NULL, &col, NULL);
    if ((e == MKVASS_COMPLETO || (e == MKVASS_COLHENDO && col > legOverlayColhidos)) &&
        legenda_ligada_em(legenda_geracao())) {
      printf("[legenda] faixa %d: o mkvass voltou a entregar (%d blocos, depois de %d falha(s))%s\n",
             legOverlay, col, legOverlayFalhas, legOverlayTV ? ": a faixa VOLTA ao app (nativa desligada)" : "");
      fflush(stdout);
      if (legOverlayTV) {
        video_escolher_legenda(-1);
        player_toast(i18n("Legenda ASS: o app voltou a desenhar"), 4000);
      }
      legOverlayTV = 0; legOverlayFalhas = 0; legOverlayColhidos = col;
    }
  }
  // O mkvass declarou no-go. Passageiro: agenda outra tentativa — sempre — e
  // a faixa fica com o app nas primeiras; depois a TV desenha por enquanto.
  // Definitivo: devolve a faixa ao pipeline da TV de vez, e a folha diz por
  // que. Polling por quadro e o que ha: o no-go nasce num fio de rede e este
  // modulo nao tem callback — e uma comparacao de inteiro.
  if (legOverlay >= 0 && !legOverlayRetomar && mkvass_nogo()) {
    int i = legOverlay, e = mkvass_estado(), http = 0, curl = 0, col = 0;
    long recuo = mkvass_recuo_ms(e, legOverlayFalhas, legOverlayRecusas);
    mkvass_ultima_falha(&http, &curl);
    mkvass_estatisticas(NULL, NULL, &col, NULL);
    if (recuo > 0) {
      legOverlayFalhas++;
      if (e == MKVASS_NOGO_SEM_RANGE) legOverlayRecusas++;
      legOverlayRetomar = (agora + (Uint32)recuo) | 1u;
      if (col > legOverlayColhidos) legOverlayColhidos = col;
      printf("[legenda] mkvass falha PASSAGEIRA %d (%s, HTTP %d, curl %d) na faixa %d: tentativa %d em %ld ms, %s\n",
             e, motivoNoGo(e), http, curl, i, legOverlayFalhas, recuo,
             legOverlayTV ? "a TV segue desenhando por enquanto" : "overlay do app mantido");
      // Recuo LONGO, e dito com todas as letras: o registro do relato mostrava
      // tentativas a 2 s e 5 s batendo na mesma recusa.
      if (e == MKVASS_NOGO_RESTO)
        printf("[mkvass] servidor recusou o resto: esperando %ld s\n", recuo / 1000);
      fflush(stdout);
      // Sem nada colhido nao ha o que manter no overlay; depois de
      // MKVASS_TENTATIVAS_OVERLAY tentativas sem fala nova, a pessoa ja
      // ficou tempo demais sem legenda. Nos dois casos a TV desenha POR
      // ENQUANTO, e a tentativa seguinte que entregar traz a faixa de volta.
      if (!legOverlayTV && (col == 0 || legOverlayFalhas > MKVASS_TENTATIVAS_OVERLAY)) {
        legOverlayTV = 1;
        printf("[legenda] faixa %d: a TV desenha POR ENQUANTO (%d colhidos), o app segue tentando\n", i, col);
        fflush(stdout);
        player_toast(i18n("Legenda ASS: a TV desenha por enquanto (falha de rede); o app tenta de novo"), 6000);
        legenda_desligar();
        video_escolher_legenda(i);
      }
    } else {
      int estavaNaTV = legOverlayTV;
      legOverlay = -1; legOverlayNoGo = i; legOverlayNoGoEstado = e; legOverlayTV = 0;
      printf("[legenda] mkvass no-go %d (%s, HTTP %d, curl %d) na faixa %d: a legenda VOLTA para a TV "
             "(nativa religada)\n", e, motivoNoGo(e), http, curl, i);
      fflush(stdout);
      // Aviso na tela: antes a queda era muda e a pessoa so via a legenda
      // piscar e cortar, sem saber que o app tinha desistido.
      avisarQueda(e);
      // O overlay tinha o que colheu antes de desistir: sai, senao a TV e o
      // app desenhariam a mesma fala.
      legenda_desligar();
      if (!estavaNaTV) video_escolher_legenda(i);
    }
  }
  // A sonda voltou para uma faixa escolhida antes dela: agora da para decidir.
  if (legOverlayEsperando >= 0) {
    int i = legOverlayEsperando;
    const VideoFaixa *f = video_legenda(i);
    video_sondar_mkv_agora();          // se o sourceInfo chegou depois da escolha
    if (video_mkv_sondado() != 0) {
      int ord = video_legenda_ordinal_mkv(i);
      legOverlayEsperando = -1;
      if (f && ehAss(f) && ord >= 0 && video_legenda_atual() == i && video_url_atual()[0]) {
        printf("[legenda] sonda voltou: faixa %d e ASS, o app assume\n", i);
        overlayAssumir(i, ord);
      } else {
        printf("[legenda] sonda voltou: faixa %d (%s, codec=%s) fica na TV: %s\n", i,
               f ? f->rotulo : "?", f && f->codec[0] ? f->codec : "?", motivoTV(i));
        fflush(stdout);
        // Sem par no arquivo e uma falha do casamento, nao da TV: avisa, para
        // a pessoa poder mandar o log em vez de achar que a legenda e assim.
        if (f && !f->codec[0] && video_mkv_sondado() == 1)
          player_toast(i18n("Legenda: n\xc3\xa3o deu para casar as faixas com o arquivo; a TV desenha"), 6000);
      }
    }
  }
}

// Traz a linha focada para dentro da janela visivel, mexendo o MINIMO: so
// quando o foco passa de uma das bordas. Rolar sempre para centralizar faria a
// lista inteira andar a cada tecla, que num D-pad e desorientador.
static void ajustarRolagem(void) {
  int n = nLinhas(coluna), f = foco[coluna], *r = &rolagem[coluna];
  if (visiveis < 1) return;
  if (f < *r) *r = f;
  else if (f >= *r + visiveis) *r = f - visiveis + 1;
  if (*r > n - visiveis) *r = n - visiveis;
  if (*r < 0) *r = 0;
}

static void coluna_desenhar(int col, float x, float larg, float y0, float a) {
  const char *titulo=col==FX_COL_ESTILO?"Estilo":col?"Legendas":"Faixas de áudio";
  txt_desenhar_alpha(txt_linha(TXT_PG_ROTULO,titulo,188,190,196,255),x,y0,a);
  int n=nLinhas(col), r=rolagem[col], fim=r+visiveis;
  if(fim>n) fim=n;
  for(int i=r;i<fim;i++) {
    float y=y0+64+(i-r)*FX_LINHA;
    int sel=col==coluna && i==foco[col];
    const char *marca=NULL,*rot;
    char valor[48];
    if(col==FX_COL_ESTILO) {
      valorEstilo(i,valor,sizeof valor); rot=EST_ROT[i]; marca=valor;
    } else if(!col) {
      const VideoFaixa *f=video_audio(i);
      rot=f?f->rotulo:""; marca=f?f->idioma:NULL;
    } else {
      rot=i==0?"Nenhuma":rotuloLegenda(i-1,&marca);
      if(i && !marca) marca=i18n("Incorporada");
    }
    float fr, fg, fb;
    corFocoFaixa(&fr, &fg, &fb);
    if(sel) superficieFocoFaixa((GfxRect){x-20,y-14,larg+20,92},a);
    int c=sel?ajustes_tinta_foco():230, sub=sel?ajustes_tinta_foco2():174;
    if(col==FX_COL_ESTILO && estiloPreservadoAss(i)) { c=sel?c:128; sub=sel?sub:112; }
    txt_desenhar_alpha(txt_linha_corta(TXT_PAINEL_ITEM,rot,c,c,c,255,larg-72),x,y,a);
    if(marca && *marca)
      txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,marca,sub,sub,sub,255,larg-72),x,y+34,a);
    int ativo=col==0?i==video_audio_atual():
      col==1?i-1==legendaAtiva():0;
    if(ativo) {
      int cr = sel ? c : (int)(fr * 255.0f + 0.5f);
      int cg = sel ? c : (int)(fg * 255.0f + 0.5f);
      int cb = sel ? c : (int)(fb * 255.0f + 0.5f);
      txt_desenhar_alpha(txt_linha(TXT_BODY,"✓",cr,cg,cb,255),x+larg-44,y+12,a);
    }
  }
  if(!n) txt_bloco(TXT_PG_FIM,"Nenhuma faixa disponível nesta fonte.",178,180,186,x,y0+68,larg,28,a,2);
  if(n>visiveis) {
    char num[48]; snprintf(num,sizeof num,i18n("%d de %d"),foco[col]+1,n);
    txt_desenhar_alpha(txt_linha(TXT_MINI,num,174,176,182,255),x,y0+64+visiveis*FX_LINHA,a);
  }
}

void faixas_desenhar(Uint32 agora) {
  (void)agora;
  if(anim<.01f) return;
  float a=anim;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,.025f,.025f,.03f,.88f*a);
  txt_desenhar_alpha(txt_linha(TXT_PAINEL_TITULO,modo?"Legendas":"Áudio",242,243,245,255),56,48,a);
  txt_desenhar_alpha(txt_linha(TXT_PG_FIM,"Voltar para fechar",180,182,188,255),NV_TELA_W-250,60,a);
  visiveis=7;
  ajustarRolagem();
  if(!modo) coluna_desenhar(0,76,720,138,a);
  else {
    coluna_desenhar(1,76,990,138,a);
    coluna_desenhar(FX_COL_ESTILO,1190,650,138,a);
  }
}
