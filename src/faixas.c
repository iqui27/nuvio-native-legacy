#include "faixas.h"
#include "legfontes.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#include "idioma.h"
#include "player.h"
#include "video.h"
#include "xtream.h"
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
#include "audioescolha.h"
#include "plrui.h"
#include "plrilha.h"
#include "legendasui.h"
#include "legsync.h"
#include "legauto.h"
#include "legmemoria.h"
#include "cacheboost.h"
#include "velocidade.h"
#include "faixasmkv.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

// 1400 e nao 1180: com a terceira coluna, "Muito pequena" e "Escuro 100%" nao
// cabiam no espaco do valor e saiam cortados. A folha de AUDIO, que tem uma
// coluna so, usa uma fracao disto — ver faixas_desenhar.


// 3 e nao 2: a folha de legenda tem a LISTA e o ESTILO, e FX_COL_ESTILO e o
// indice 2. Com dois slots a coluna de estilo escrevia fora do vetor.
static int aberta, coluna, foco[3];
// #202: foco na linha da velocidade e o ultimo valor pedido pela folha.
static int velFoco, velPedidaUi = VEL_NORMAL;
// ROLAGEM POR COLUNA, em LINHAS (nao em pixels): a folha desenhava todas as
// faixas a partir do topo e o painel tem altura limitada — com muitas legendas
// as ultimas caiam fora do painel e da tela. O foco chegava nelas, os olhos
// nao. Guardar quantas linhas foram roladas e o suficiente porque a altura da
// linha e fixa.
static int rolagem[3];
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueAudio;
static float toqueAudioOffset;
static int toqueAudioRolar(const PonteiroRolagem *e) {
  if (!aberta || faixas_estilo_topo()) return 0;
  return toquerol_evento(&toqueAudio, e);
}
#endif
// Quantas linhas cabem no painel. Calculada no desenho (depende da altura
// escolhida ali) e lida pelo tratamento de tecla, que roda antes.
static int visiveis = 8;
static void ajustarRolagem(void);
static float anim;
// 0..1: a folha de legenda virando a BARRA DE ESTILO no topo (barra_desenhar).
static float animTopo;
#define FX_BARRA_COLS 5
// As cores das legendas, na ordem de VIDEO_LEG_CORES (a mesma de player.c).
static void corLegenda(int i, int *r, int *g, int *b) {
  static const unsigned char c[VIDEO_LEG_NCORES][3] = {
    {255,255,255},{255,222,48},{64,224,112},{78,156,255},{255,80,80},{18,18,18}};
  if (i < 0 || i >= VIDEO_LEG_NCORES) i = 0;
  *r = c[i][0]; *g = c[i][1]; *b = c[i][2];
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
// F04: opaque identity of the active external subtitle (legendasui_id_addon),
// so the selector marks it by identity even if the addon list was replaced.
static char legExternaId[24];

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

int faixas_legenda_overlay(void) {
#ifdef NV_TPK
  // The .tpk host supplies text callbacks, not a firmware caption plane.
  // Keep collected cues visible while waiting for callbacks or retrying;
  // the primary renderer uses callbacks only when no collected cue is active.
  return 1;
#else
  return !legOverlayTV;
#endif
}

static Uint32 legOverlayRetomar;       // 0 = nada agendado

static int ehAss(const VideoFaixa *f) {
  return f && (!strncmp(f->codec, "S_TEXT/ASS", 10) || !strncmp(f->codec, "S_TEXT/SSA", 10));
}

// #421: texto embutido usa o mesmo overlay das externas. Android ja recebe
// os cues do Media3; la so ASS continua pelo colhedor.
#ifdef NV_ANDROID
#define FX_TEXTO_OVERLAY 0
#else
#define FX_TEXTO_OVERLAY 1
#endif
// PGS: o app decodifica e desenha (imgsub) no .tpk e na LG; as outras seguem.
static int ehPgs(const VideoFaixa *f) {
#if defined(NV_TPK) || defined(NV_WEBOS)
  return f && faixasmkv_imagem_overlay(f->codec);
#else
  (void)f; return 0;
#endif
}
static int vaiOverlay(const VideoFaixa *f) {
  return f && (faixasmkv_overlay(f->codec, FX_TEXTO_OVERLAY) || ehPgs(f));
}

// O overlay do app assume a faixa embutida `i` (ordinal `ord` no arquivo): a
// legenda nativa da TV e DESLIGADA quando o texto chegar (-1 manda
// setSubtitleEnable false ao uMS / desliga no AVPlay) e o mkvass comeca a
// colher. A linha de log e a prova de que o app assumiu — se a TV continuar
// desenhando por cima, o firmware ignorou o setSubtitleEnable, e isso e
// outro bug (a resposta do uMS sai logo abaixo como "[video] {...}").
static void overlayAssumir(int i, int ord) {
  if (xtream_url_limitada(video_url_atual())) { video_escolher_legenda(i); return; }
  const VideoFaixa *f = video_legenda(i);
  int texto = f && f->numero >= 0 && !ehAss(f) && !ehPgs(f);
  // Texto: a TV segura a legenda enquanto a rede ainda nao entregou cues.
  video_escolher_legenda(texto ? i : -1);
  mkvass_iniciar_ordinal(video_url_atual(), ord);
  legOverlay = i;
  legOverlayFalhas = legOverlayRecusas = 0; legOverlayRetomar = 0;
  legOverlayTV = texto; legOverlayColhidos = 0;
  printf("[legenda] faixa %d (%s, %s) -> app: ordinal %d; %s\n",
         i, f ? f->rotulo : "?", f ? f->codec : "?", ord, texto ? "TV enquanto aguarda texto" : "legenda nativa desligada");
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
  if (!vaiOverlay(f)) return FX_TEXTO_OVERLAY ? "codec nao e de texto (ASS/SRT/WebVTT): a TV desenha"
                                               : "codec nao e ASS/SSA: a TV desenha bem";
  if (video_legenda_ordinal_mkv(i) < 0) return "sem ordinal no arquivo";
  return "?";
}


// --- A PILULA DA LEGENDA AUTOMATICA (dono, 04/10) -----------------------------
// "Quando clicar na linguagem o componente tem que diminuir para avisar o que
// ta fazendo": escolhida a legenda (ou decidida sozinha), a ilha da folha ENCOLHE
// numa pilula (a mesma mola de plrilha.c: o ultimo corpo sai com o alfa caindo
// enquanto a forma vira a pilula) e narra: "Procurando legendas em Portugues…"
// -> "Sincronizando…" -> "Legenda aplicada · OpenSubtitles" (e some), ou "Nenhuma
// legenda em Portugues" com o OK de volta para a lista. Cada estado fica no
// minimo PIL_MIN_MS na tela, senao uma legenda que baixa em 200 ms piscaria.
enum { PIL_OFF = LEGSYNC_PIL_OFF, PIL_PROCURANDO, PIL_SINCRONIZANDO, PIL_APLICADA, PIL_FALHOU };
#define PIL_FALHOU_MS   8000u
#define PIL_APLICADA_MS LEGSYNC_PIL_APLICADA_MS
#define PIL_SEMSYNC_MS  LEGSYNC_PIL_SEMSYNC_MS
// A referencia de um longa leva minutos (um Range por fala, legsync.c): a
// ilha diz "Sincronizando…" uns segundos, volta para o relogio e fica fechada
// enquanto o plano trabalha; so o aviso final reabre (legsync_pil_passo).
static int emTroca;
// pil.final: 0 externa nao sincronizada, 1 externa sincronizada (offset aceito
// em vigor), 2 embutida (acompanha o video por natureza).
static LegSyncPil pil;
static int pilBusca, pilFalhaBaixar;
static char pilTexto[200];
static char pilIdioma[16], pilProvedor[64];
static uint64_t pilHash;
static uint64_t pilEsperaHash;   // a legenda que o plano esta tentando (para legauto_lembrar)

static Uint32 pilAgora(void) { return SDL_GetTicks(); }
static void pilIr(int e, Uint32 agora) { if (pil.estado != e) { pil.estado = e; pil.desde = agora ? agora : 1u; } }
static void pilZerar(void) { pil.estado = pil.rastreia = pilBusca = pil.final = pilFalhaBaixar = 0; pil.troca = 0; pilHash = 0; pilTexto[0] = 0; }

// Nome do provedor sem o sufixo de hospedagem ("AIOStreams | ElfHosted" -> "AIOStreams").
static void pilProvedorCurto(const char *src, char *dst, size_t n) {
  size_t k = 0;
  while (src && src[k] && k + 1 < n) { if (src[k] == '|') break; dst[k] = src[k]; k++; }
  while (k && dst[k - 1] == ' ') k--;
  dst[k] = 0;
}

static void pilBuscando(const char *idioma, Uint32 agora) {
  if (pilBusca || pil.rastreia) return;
  pilBusca = 1; snprintf(pilIdioma, sizeof pilIdioma, "%s", idioma);
  pilIr(PIL_PROCURANDO, agora);
}
static void pilFalhou(const char *idioma, Uint32 agora) {
  pilBusca = pil.rastreia = 0;
  snprintf(pilIdioma, sizeof pilIdioma, "%s", idioma);
  pilIr(PIL_FALHOU, agora);
}
static void pilExterna(const Legenda *l) {
  Uint32 agora = pilAgora();
  pilBusca = 0; pil.rastreia = 1; pil.final = 0; pilFalhaBaixar = 0; pil.espera = 0;
  snprintf(pilIdioma, sizeof pilIdioma, "%s", l->idioma);
  pilProvedorCurto(l->provedor[0] ? l->provedor : l->rotulo, pilProvedor, sizeof pilProvedor);
  pilHash = legsync_hash_url(l->url);
  pil.iniciou = agora;
  pil.estado = PIL_OFF;           // recomeca do inicio mesmo se a anterior ainda estava de pe
  pilIr(PIL_PROCURANDO, agora);
}
static void pilEmbutida(const char *rotulo, Uint32 agora) {
  pilBusca = pil.rastreia = 0; pil.final = 2; pil.espera = 0;
  snprintf(pilProvedor, sizeof pilProvedor, "%s", rotulo && *rotulo ? rotulo : i18n("Embutida"));
  pilIr(PIL_APLICADA, agora);
}

static void pilLembrar(void) {
  const CatItem *ci = cat_item(player_indice());
  if (ci && pilHash) legauto_lembrar(ci->imdb[0] ? ci->imdb : ci->titulo, pilIdioma, pilHash);
}

// Sync pedido a mao (seletor/menu): "Sincronizando…" na hora, mesmo ciclo do
// automatico; recusado vira o aviso final direto, sem fingir que trabalhou.
static unsigned pilManualSeq;
static void pilManual(Uint32 agora) {
  LegSyncVisao v = legsync_visao(0);
  pilBusca = 0; pil.rastreia = 1; pil.espera = 0; pil.troca = 0; pil.final = 0; pilFalhaBaixar = 0;
  pil.iniciou = agora; pil.estado = PIL_OFF;
  if (v.manualFase == 3) {
    pil.rastreia = 0;
    pil.final = legsync_pilula_final(&v, pilProvedor, pilTexto, sizeof pilTexto);
    pilIr(PIL_APLICADA, agora);
  } else pilIr(PIL_SINCRONIZANDO, agora);
}

static void pilAtualizar(Uint32 agora) {
  int r;
  LegSyncVisao v;
  unsigned seq = legsync_manual_seq();
  if (seq != pilManualSeq) { pilManualSeq = seq; if (player_aberto()) pilManual(agora); }
  if (!player_aberto()) pil.espera = 0;
  if (pil.estado == PIL_OFF && !pil.espera) return;
  if (pil.estado != PIL_OFF && (!player_aberto() || (pilBusca && !legAuto))) { pilZerar(); return; }
  if (pil.estado != PIL_FALHOU && (pil.rastreia || pil.espera)) {
    v = legsync_visao(0);
    r = legsync_pil_passo(&pil, &v, agora, pilProvedor, pilTexto, sizeof pilTexto);
    if (r & LEGSYNC_PIL_ESCONDEU) pilEsperaHash = pilHash;
    if (r & LEGSYNC_PIL_FINAL_TARDE) { pilHash = pilEsperaHash; pil.rastreia = 0; }
    if (r & LEGSYNC_PIL_LEMBRAR && pilHash) pilLembrar();
    if (r & LEGSYNC_PIL_BAIXAR) { pilFalhaBaixar = 1; pilFalhou(pilIdioma, agora); return; }
    if (r & LEGSYNC_PIL_ZERAR) pilZerar();
    return;
  }
  if (pil.estado == PIL_FALHOU && agora - pil.desde >= PIL_FALHOU_MS) pilZerar();
  else if (pil.estado == PIL_APLICADA && agora - pil.desde >= (pil.final == 0 ? PIL_SEMSYNC_MS : PIL_APLICADA_MS)) pilZerar();
}

// OK na pilula de falha: o caminho de volta para a lista de legendas.
int faixas_pilula_tecla(const SDL_Event *e) {
  if (pil.estado != PIL_FALHOU || faixas_aberta() || !player_aberto()) return 0;
  if (e->type != SDL_KEYDOWN) return 0;
  if (e->key.keysym.sym != SDLK_RETURN && e->key.keysym.sym != SDLK_KP_ENTER) return 0;
  pilZerar();
  faixas_abrir_em(1);
  return 1;
}

#ifdef NV_SHOT_HOOKS
// Capturas: poe a pilula num estado fixo (1 procurando, 2 sincronizando,
// 3 aplicada e sincronizada +2,5 s, 4 falhou, 5 aplicada sem sincronia, 0 apaga).
void faixas_shot_pilula(int estado, const char *idioma, const char *provedor, Uint32 agora) {
  pilZerar();
  snprintf(pilIdioma, sizeof pilIdioma, "%s", idioma ? idioma : "");
  snprintf(pilProvedor, sizeof pilProvedor, "%s", provedor ? provedor : "");
  if (estado == 3 || estado == 5) {
    LegSyncVisao v;
    memset(&v, 0, sizeof v);
    v.fase = estado == 3 ? LEGSYNC_ACEITA : LEGSYNC_RECUSADA; v.offsetAutoMs = 2500;
    pil.final = legsync_pilula_final(&v, pilProvedor, pilTexto, sizeof pilTexto);
    estado = PIL_APLICADA;
  }
  pil.estado = estado; pil.desde = agora | 1u;
}
#endif

// PONTEIRO (#99) NA PILULA DE FALHA: nada a focar (ela e o unico botao); o
// alvo existe para o clique virar o OK que faixas_pilula_tecla ja trata — sem
// ele o clique caia no fundo do player (pausar/tocar).
static void ponteiroPilula(int a, int b) { (void)a; (void)b; }

static void pilPedir(void) {
  static char texto[200], dir[48];
  char nome[64];
  PlrIlhaPedido p;
  if (pil.estado == PIL_OFF || aberta) return;
  memset(&p, 0, sizeof p);
  snprintf(nome, sizeof nome, "%s", i18n(ling_nome(pilIdioma)));
  switch (pil.estado) {
    case PIL_PROCURANDO:
      snprintf(texto, sizeof texto, i18n("Procurando legendas em %s…"), nome);
      p.respira = 1; break;
    case PIL_SINCRONIZANDO:
      snprintf(texto, sizeof texto, "%s", i18n("Sincronizando…"));
      p.respira = 1; break;
    case PIL_APLICADA:
      // Embutida: acompanha o video. Externa: so o texto que legsyncui montou
      // do estado real; sem sincronia o icone e o aviso ambar, nunca o check.
      if (pil.final == 2 || !pilTexto[0]) snprintf(texto, sizeof texto, i18n("Legenda aplicada · %s"), pilProvedor);
      else snprintf(texto, sizeof texto, "%s", pilTexto);
      if (pil.final == 0 && pilTexto[0]) { p.icone = "pl_triangle-alert"; p.corIcone = 1; }
      else { p.icone = "pl_check"; p.corIcone = 2; }
      break;
    default:
      snprintf(texto, sizeof texto, pilFalhaBaixar ? "%s" : i18n("Nenhuma legenda em %s"),
               pilFalhaBaixar ? i18n("Não deu para baixar a legenda") : nome);
      snprintf(dir, sizeof dir, "%s", i18n("OK abre a lista"));
      p.icone = "pl_triangle-alert"; p.corIcone = 1; p.direita = dir;
      p.ponteiroOk = ponteiroPilula; break;
  }
  p.texto = texto; p.semFim = 1; p.aberta = 1;
  plrilha_pedir(&p);
}

// Chamada quando uma sessao de reproducao nova comeca: a legenda externa e da
// sessao, nao do aparelho. Sem isto o titulo seguinte abriria a folha marcando
// como ativa uma legenda que nao foi escolhida para ele.
// R4: a sincronia automatica (legsync.c) pede OUTRA legenda do mesmo idioma
// quando a escolhida nao fecha com a referencia. Liga pelo mesmo caminho da
// escolha da pessoa. `voltar` devolve a primeira tentada (a escolha original).
static int autoTroca(const char *idioma, const uint64_t *tent, int n, int voltar,
                     char *nome, unsigned tamNome) {
  Legenda *v = malloc(sizeof *v * LEG_MAX);
  int nv, j, k;
  if (!v) return 0;
  nv = addons_legendas_copiar(v, LEG_MAX, NULL, NULL);
  for (j = 0; j < nv; j++) {
    int jaFoi = 0;
    uint64_t h = legsync_hash_url(v[j].url);
    if (!v[j].url[0]) continue;
    if (voltar) { if (n < 1 || h != tent[0]) continue; }
    else {
      if (!idioma || !idioma[0] || strcasecmp(idioma, v[j].idioma)) continue;
      for (k = 0; k < n; k++) if (tent[k] == h) jaFoi = 1;
      if (jaFoi) continue;
    }
    snprintf(nome, tamNome, "%s", v[j].provedor[0] ? v[j].provedor : v[j].rotulo);
    pil.troca = 1;   // a pilula segue em "Sincronizando…" com a nova
    emTroca = 1; faixas_escolher_externa(&v[j]); emTroca = 0;
    pilProvedorCurto(nome, pilProvedor, sizeof pilProvedor); pilHash = h;
    if (pil.espera) pilEsperaHash = h;
    if (pil.estado == PIL_APLICADA && !pil.espera) pilIr(PIL_SINCRONIZANDO, pilAgora());
    free(v);
    return 1;
  }
  free(v);
  return 0;
}

static void reiniciarFaixas(int preservarPrebusca) {
  legsync_definir_trocador(autoTroca);
  legExterna = -1; legExternaId[0] = 0; legOverlay = legOverlayNoGo = legOverlayEsperando = -1; aberta = 0;
  legOverlayFalhas = legOverlayRecusas = legOverlayNoGoEstado = 0; legOverlayRetomar = 0;
  legOverlayTV = legOverlayColhidos = 0;
  if (!preservarPrebusca) mkvass_parar();
  legenda_desligar(); legenda_pre_reiniciar();
  legAuto = 1; legAutoDesde = 0; pilZerar(); pil.espera = 0;
  legendasui_reiniciar();   // F04: the second subtitle belongs to the session too
  // #202: speed is per playback: a new title starts at 1x.
  velFoco = 0; velPedidaUi = VEL_NORMAL; video_velocidade(VEL_NORMAL);
}
void faixas_reiniciar(void) { reiniciarFaixas(0); }
#ifdef NV_ANDROID
void faixas_reiniciar_preparada(void) { reiniciarFaixas(1); }
#endif

// Indice da legenda que a folha deve marcar como ATIVA.
static int legendaAtiva(void) {
  if (legExternaId[0]) {
    char id[24];
    for (int j = 0; j < addons_n_legendas(); j++) {
      legendasui_id_addon(addons_legenda(j), id);
      if (!strcmp(id, legExternaId)) return legExterna = video_n_legenda() + j;
    }
    return legExterna = -1;
  }
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
#define FX_N_ESTILO   11
static int ctl = -1;       // ESTILO: item cujo controle esta aberto (-1 = a grade)

static int nLinhas(int col);
static int linhaDaLeg(int i);

// F07: VOLUME ROW at the top of the AUDIO sheet (0-200%, per session). Focus on
// it is `volFoco`; LEFT/RIGHT change it in 10% steps, live. Only focusable
// where the backend has the gain (cacheboost_suportado: Android); elsewhere
// it is drawn dimmed with "Não disponível nesta TV".
static int volFoco;
static int volFocavel(void) { return cacheboost_suportado(); }

// #202: SPEED ROW right below the volume, same anatomy and gesture (LEFT/RIGHT
// step through VEL_LISTA, live). Unlike the volume it is HIDDEN where the
// platform has no speed (no dead row): video_velocidade_suportada.
// `velPedidaUi` is what the person last asked; if the platform refuses
// (LG/.tpk answer later), the row goes away and one notice says why.
// Live channel: no speed (the live edge cannot be outrun).
static int velVisivel(void) { return video_velocidade_suportada() && !player_id_canal()[0]; }
// Visible but dimmed and not focusable while the audio goes out as passthrough
// (Android: ExoPlayer cannot change the tempo of a bitstream it does not decode).
static int velFocavel(void) { return velVisivel() && !video_velocidade_bloqueada(); }

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
  ctl = -1;
  volFoco = 0; velFoco = 0;
  foco[0] = video_audio_atual();
  // A legenda pode estar desligada (-1); a primeira linha da coluna e sempre
  // "Desativada", entao o indice da lista e deslocado em um.
  foco[1] = linhaDaLeg(legendaAtiva()) + 1;
  // Abriu a folha de LEGENDAS: e agora que idioma, codec e ordinal importam.
  // A sonda esperava 20 s de buffer (video.c) — numa fonte lenta a folha
  // abria com "Legenda 1..N" e sem selo ASS, e a escolha ia a TV.
  if (modo) { video_sondar_mkv_agora(); legendasui_abrir(); }
  // Clamp nas duas colunas. A lista de legendas CRESCE durante a sessao (as do
  // OpenSubtitles chegam depois) e a de audio so existe apos o sourceInfo:
  // guardar um indice de antes e reabrir sem conferir poe o foco fora do vetor.
  { int c; for (c = 0; c < 3; c++) {
      n = nLinhas(c);
      if (foco[c] >= n) foco[c] = n > 0 ? n - 1 : 0;
      if (foco[c] < 0)  foco[c] = 0;
    rolagem[c] = 0;
#ifdef NV_TOUCH_UI
    toquerol_limpar(&toqueAudio); toqueAudioOffset = 0;
#endif
    } }
  // No track list yet (the audio list arrives after the first frames): the
  // volume row is the only thing to focus.
  if (!modo && !nLinhas(0) && volFocavel()) volFoco = 1;
  else if (!modo && !nLinhas(0) && velFocavel()) velFoco = 1;
}

int faixas_aberta(void) { return aberta; }
int faixas_estilo_controle(void) { return ctl; }
int faixas_estilo_topo(void) { return aberta && modo && coluna == FX_COL_ESTILO; }
float faixas_anim(void) { return anim; }
void faixas_aba_estilo(int estilo) {
  if (!aberta || !modo) return;
  coluna = estilo ? FX_COL_ESTILO : 1;
  ctl = -1;
}
int faixas_teste_foco(int *col, int *vol, int *vel) {
  if (col) *col = coluna;
  if (vol) *vol = volFoco;
  if (vel) *vel = velFoco;
  return aberta ? foco[coluna] : -1;
}

// CANAL AO VIVO NAO TEM LEGENDA DE ADDON (dono, 03/10: "a legenda ta errada,
// ta pegando e uma de filmes"). A lista de addons.c e GLOBAL: guarda o que o
// ultimo filme/episodio pediu, e abrir um canal nao pede de novo (tocarCanal),
// entao a folha do canal listava o OpenSubtitles do filme anterior. No canal
// so valem as faixas do proprio fluxo (closed caption, DVB). tests/faixas_canal.
static int nLegAddon(void) { return player_id_canal()[0] ? 0 : addons_n_legendas(); }
static int nLegendas(void) {
  int n = video_n_legenda() + nLegAddon();
  return n;
}

// ORDEM DA LISTA: as embutidas de dialogo, depois as de LETREIROS ("Signs &
// Songs", forced), depois as dos addons. A faixa de letreiros so traduz placas
// e musicas; no topo da lista ela era a primeira "Português" que a pessoa
// escolhia, e a legenda parecia sumir no meio da conversa. `k` e a posicao na
// lista mostrada (sem a linha "Nenhuma"); devolve o indice combinado.
static int legDaLinha(int k) {
  int emb = video_n_legenda(), passo, i, n = 0;
  if (k < 0 || k >= emb) return k;
  for (passo = 0; passo < 2; passo++)
    for (i = 0; i < emb; i++) {
      const VideoFaixa *f = video_legenda(i);
      if ((f && f->letreiro) != passo) continue;
      if (n++ == k) return i;
    }
  return k;
}
static int linhaDaLeg(int i) {
  int emb = video_n_legenda(), k;
  if (i < 0 || i >= emb) return i;
  for (k = 0; k < emb; k++) if (legDaLinha(k) == i) return k;
  return i;
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
enum { EST_FONTE = 1, EST_COR = 2, EST_RESTAURAR = 9, EST_COR_BORDA = 10 };
static const char *const EST_ROT[FX_N_ESTILO] = {
  "Tamanho", "Fonte OpenSubtitles", "Cor", "Opacidade", "Fundo", "Posição", "Borda", "Atraso",
  "Negrito", "Restaurar padrão", "Cor da borda"
};
static const char *const EST_FUNDO[5] = { "Nenhum", "Escuro 25%", "Escuro 50%",
                                          "Escuro 75%", "Escuro 100%" };
static const char *const EST_BORDA[3] = { "Nenhuma", "Contorno", "Sombra" };
static const char *const EST_OPAC[4]  = { "100%", "75%", "50%", "25%" };

/* Com o ASS desenhado pelo libass, fonte, cor, fundo, posicao, borda, negrito
 * e cor da borda sao do arquivo ate a pessoa mexer no campo (aqui ou nos
 * Ajustes): so entao ela vence (ver player_leg_estilo_tocou em player.h). A
 * linha intocada diz "Preservado pelo ASS", esmaecida, mas continua editavel.
 * Tamanho vira escala proporcional; opacidade e atraso valem igual. */
static int estCampoAss(int linha) {
  switch (linha) {
    case EST_FONTE: return PLR_LEG_FONTE;
    case EST_COR: return PLR_LEG_COR;
    case 4: return PLR_LEG_FUNDO;
    case 5: return PLR_LEG_POSICAO;
    case 6: return PLR_LEG_BORDA;
    case 8: return PLR_LEG_NEGRITO;
    case EST_COR_BORDA: return PLR_LEG_COR_BORDA;
    default: return 0;
  }
}
static int estiloPreservadoAss(int linha) {
  int campo = estCampoAss(linha);
  return campo && assrender_ativo() && !assrender_texto_simples() && !player_leg_estilo_tocado(campo);
}

static void valorEstilo(int linha, char *dst, size_t tam) {
  const VideoLegendaEstilo *e = player_leg_estilo();
  if (estiloPreservadoAss(linha)) {
    snprintf(dst, tam, "%s", i18n("Preservado pelo ASS"));
    return;
  }
  switch (linha) {
    case 0:
      if (assrender_ativo() && !assrender_texto_simples())
        snprintf(dst, tam, "%d%% \xc2\xb7 ASS \xc3\x97%.2f", e->tamanho, e->tamanho / 120.0), idioma_decimal_texto(dst, ajustes_idioma());
      else
        snprintf(dst, tam, "%d%%", e->tamanho);
      break;
    case 1: snprintf(dst, tam, "%s", legfontes_selecionada(0) >= 0 ? legfontes_nome(legfontes_selecionada(0)) : TXT_FAMILIAS_PT[e->familia >= 0 && e->familia < TXT_FAMILIA_N ? e->familia : 0]); break;
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
      else    { snprintf(dst, tam, "%+.2f s", a / 1000.0f); plrui_decimal(dst); }
      break; }
    // Negrito vale para o que o app desenha (OpenSubtitles e legenda externa);
    // a faixa que o player da TV desenha segue o peso do aparelho.
    case 8: snprintf(dst, tam, "%s", i18n(e->negrito ? "Ligado" : "Desligado")); break;
    case EST_COR_BORDA: {
      int c = e->corBorda >= 0 && e->corBorda < VIDEO_LEG_NCORES ? e->corBorda : 0;
      snprintf(dst, tam, "%s", VIDEO_LEG_CORES_PT[c ? c - 1 : VIDEO_LEG_NCORES - 1]);
      break; }
    default: dst[0] = 0; break;   // "Restaurar padrao" e acao, nao valor
  }
}

// OK nas linhas SEM controle proprio (Fundo, Borda, Negrito, Restaurar): cicla
// o valor e aplica na hora. As demais (Tamanho, Fonte, Cor, Opacidade, Posicao,
// Atraso) abrem um controle dentro do painel (ver estiloTecla).
static void ciclarEstilo(int linha) {
  VideoLegendaEstilo *e = player_leg_estilo();
  switch (linha) {
    case 4: e->fundo   = (e->fundo + 1) % 5; player_leg_estilo_tocou(PLR_LEG_FUNDO); break;
    case 6: e->borda   = (e->borda + 1) % 3; player_leg_estilo_tocou(PLR_LEG_BORDA); break;
    case 8: e->negrito = !e->negrito; player_leg_estilo_tocou(PLR_LEG_NEGRITO); break;
    case EST_RESTAURAR:
      player_leg_estilo_restaurar();
      return;
    default: return;
  }
  player_leg_estilo_mudou();
}

// --- CONTROLES DA ABA ESTILO (dono, 11/10) ------------------------------------
// OK em Tamanho, Opacidade, Posicao e Atraso abre um SLIDER logo abaixo do item;
// Cor abre a paleta de amostras; Fonte, a lista com cada nome na propria fonte.
// O controle ocupa o lugar da grade, com a mola da tela. No Mobile a lista de
// fontes reserva as quatro fileiras inteiras. Tudo aplica ao vivo; OK/Voltar fecham o controle e so o
// Voltar seguinte fecha o painel.
typedef struct { int min, max, toque, segura, teto; } EstSlider;   // max == min: sem slider
// toque = passo de um toque; segura = passo de cada repeticao da tecla segurada;
// teto = quantas vezes o passo pode crescer segurando (aceleracao).
static const EstSlider EST_SL[FX_N_ESTILO] = {
  [0] = { 50, 300, 10, 20, 1 },                                   // Tamanho %
  [3] = { 0, 3, 1, 1, 1 },                                        // Opacidade: 25..100%
  [5] = { 0, 7, 1, 1, 1 },                                        // Posicao: 8 niveis
  [7] = { -LEGUI_ATRASO_MAX, LEGUI_ATRASO_MAX, 100, 500, 4 },     // Atraso: 0,1 s / 0,5 s
};
static int estSlider(int i) { return i >= 0 && i < FX_N_ESTILO && EST_SL[i].max > EST_SL[i].min; }
// 0 = sem controle, 1 = slider, 2 = paleta, 3 = lista de fontes.
static int estTipo(int i) { return estSlider(i) ? 1 : i == EST_COR || i == EST_COR_BORDA ? 2 : i == EST_FONTE ? 3 : 0; }

static int ctlVis = -1;    // o ultimo aberto: o controle continua desenhado enquanto fecha
static int ctlSub;         // amostra / fonte com o foco
static int ctlRep;         // repeticoes seguidas da tecla (aceleracao do slider)
static float ctlAnim;      // 0..1: grade -> controle (mola)
static float ctlFrac;      // posicao mostrada do marcador (mola: segurar desliza)
static float ctlAmostra[VIDEO_LEG_NCORES];   // 0..1: a amostra em foco cresce

static int estLer(int i) {
  const VideoLegendaEstilo *e = player_leg_estilo();
  switch (i) {
    case 0: return e->tamanho;
    case 3: return 3 - (e->opacidade < 0 ? 0 : e->opacidade > 3 ? 3 : e->opacidade);
    case 5: return e->posicao;
    default: return e->atrasoMs;
  }
}
static void estGravar(int i, int v) {
  VideoLegendaEstilo *e = player_leg_estilo();
  const EstSlider *s = &EST_SL[i];
  v = v < s->min ? s->min : v > s->max ? s->max : v;
  switch (i) {
    case 0: e->tamanho = v; break;
    case 3: e->opacidade = 3 - v; break;
    case 5: if (e->posicao != v) player_leg_estilo_tocou(PLR_LEG_POSICAO); e->posicao = v; break;
    default: e->atrasoMs = v; break;
  }
  player_leg_estilo_mudou();
}
static float estFrac(int i) { return (float)(estLer(i) - EST_SL[i].min) / (float)(EST_SL[i].max - EST_SL[i].min); }
// d = -1/+1; rep = a tecla esta repetindo (segurar): passo maior que cresce.
static void estPasso(int i, int d, int rep) {
  const EstSlider *s = &EST_SL[i];
  int mult = 1 + ctlRep / 12;
  if (mult > s->teto) mult = s->teto;
  estGravar(i, estLer(i) + d * (rep ? s->segura * mult : s->toque));
}
static void estDeFrac(int i, float f) {
  const EstSlider *s = &EST_SL[i];
  int v = s->min + (int)lroundf((f < 0 ? 0 : f > 1 ? 1 : f) * (float)(s->max - s->min) / (float)s->toque) * s->toque;
  estGravar(i, v);
}
static void estAbrir(int i) {
  ctl = ctlVis = i; ctlRep = 0;
  int fonteLocal = legfontes_selecionada(0);
  ctlSub = i == EST_COR ? player_leg_estilo()->cor
         : i == EST_FONTE && fonteLocal >= 0 ? TXT_FAMILIA_N + fonteLocal
         : i == EST_FONTE ? player_leg_estilo()->familia : 0;
  if (i == EST_COR_BORDA) {
    unsigned rgb = ajustes_leg_borda_rgb(player_leg_estilo()->corBorda);
    for (int c = 0; c < VIDEO_LEG_NCORES; c++)
      if (video_legenda_borda_rgb(c) == rgb) { ctlSub = c; break; }
  }
  if (estSlider(i)) ctlFrac = estFrac(i);
}
static void estEscolher(int i) {   // amostra / fonte `i`: aplica ao vivo
  VideoLegendaEstilo *e = player_leg_estilo();
  int pres = estiloPreservadoAss(ctl);   // OK no valor atual tambem vence o ASS
  ctlSub = i;
  if (ctl == EST_COR) {
    if (e->cor == i && !pres) return;
    e->cor = i;
    // COR: marca que a pessoa mexeu — dai em diante ela vence a cor que o
    // arquivo ASS pede (ver player_leg_estilo_tocou em player.h).
    player_leg_estilo_tocou(PLR_LEG_COR);
  } else if (ctl == EST_COR_BORDA) {
    if (e->corBorda == i && !pres) return;
    e->corBorda = i;
    player_leg_estilo_tocou(PLR_LEG_COR_BORDA);
  } else {
    if (i >= TXT_FAMILIA_N) {
      int local = i - TXT_FAMILIA_N;
      if (legfontes_selecionada(0) == local && !pres) return;
      player_leg_fonte_local_definir(local);
      return;
    }
    if (e->familia == i && !pres && legfontes_selecionada(0) < 0) return;
    player_leg_fonte_definir(i);
    return;
  }
  player_leg_estilo_mudou();
}

// Rotulo da linha `i` da coluna de legenda. Ate video_n_legenda() sao as
// embutidas; depois vem as dos addons.
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
    *marca = l->provedor[0] ? l->provedor : i18n("Legenda externa");
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
    int vaiAoApp = fe && vaiOverlay(fe) && i != legOverlayNoGo && video_url_atual()[0] &&
                   video_legenda_ordinal_mkv(i) >= 0;
    // Qualquer escolha encerra a colheita anterior: o fio do mkvass nao pode
    // continuar entregando ao overlay uma faixa que a pessoa acabou de trocar.
    // MENOS quando a escolha vai ao overlay: mkvass_iniciar_ordinal ja troca a
    // geracao (o fio velho sai sozinho) e, se for a faixa da PRE-BUSCA (#92,
    // v1.4.7), adota o fio vivo com o que ele ja leu antes do video — parar
    // aqui jogaria isso fora.
    if (!vaiAoApp) mkvass_parar();
    legOverlay = -1; legOverlayEsperando = -1; legOverlayRetomar = 0; legOverlayTV = 0;
    if (i < 0)        { pilZerar(); pil.espera = 0; video_escolher_legenda(-1); legenda_desligar(); legExterna = -1; legExternaId[0] = 0;
                        legsync_primaria_outra(0); }   // F05: sem externa, sem AutoSync
    else if (i < emb) {
      const VideoFaixa *f = video_legenda(i);
      int ord = video_legenda_ordinal_mkv(i);
      legsync_primaria_outra(1);   // F05: a embutida ja acompanha o video
      pilEmbutida(i18n("Embutida"), pilAgora());
      legenda_desligar(); legExterna = -1; legExternaId[0] = 0;
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
        // F05: legsync carrega pelo mesmo legenda.c e guarda o documento.
        video_escolher_legenda(-1); legsync_primaria_externa(l->url, legenda_idioma(l->idioma, l->arquivo), l->provedor); legExterna = i;
        if (!emTroca) pilExterna(l);
        legendasui_id_addon(l, legExternaId);
      }
    }
  }
}

// --- A ESCOLHA MANUAL LEMBRADA (2.0.3, legmemoria.c) --------------------------
// Toda legenda escolhida a mao (folha, seletor, "Desativada") vai para a
// memoria do perfil: o idioma vira a "ultima escolha" e a faixa exata fica
// presa ao titulo. A automatica e as trocas do AutoSync NAO passam por aqui.
static void tituloAtual(char *dst, unsigned tam) {
  const CatItem *ci = cat_item(player_indice());
  dst[0] = 0;
  if (ci) legmem_id_titulo(ci->imdb[0] ? ci->imdb : ci->titulo, dst, tam);
}
// Etiqueta que nao e idioma ("(null)" da LG antes da sonda, "und") vira vazio:
// ela nao diz o que a pessoa quer no proximo titulo.
static void idiomaLembravel(const char *raw, char *dst, unsigned tam) {
  const char *c;
  ling_normalizar(raw, dst, tam);
  if (!strcasecmp(dst, "und") || !strcasecmp(dst, "none")) { dst[0] = 0; return; }
  for (c = dst; *c; c++)
    if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || *c == '-')) { dst[0] = 0; return; }
}
static void lembrarEscolha(const LegMem *e) {
  legmem_guardar(e);
  printf("[legenda] escolha manual lembrada: '%s' (%s%s%s)\n", e->idioma[0] ? e->idioma : "?",
         e->tipo == 'e' ? "embutida" : e->tipo == 'a' ? "addon" : "nenhuma", e->tipo ? " " : "",
         e->tipo ? e->nome : "");
  fflush(stdout);
}
static void lembrarAddon(const Legenda *l) {
  LegMem e;
  if (!l || player_id_canal()[0]) return;
  memset(&e, 0, sizeof e);
  tituloAtual(e.titulo, sizeof e.titulo);
  e.tipo = 'a';
  legendasui_id_addon(l, e.id);
  idiomaLembravel(l->idioma, e.idioma, sizeof e.idioma);
  snprintf(e.nome, sizeof e.nome, "%s", l->provedor[0] ? l->provedor : l->rotulo);
  lembrarEscolha(&e);
}
// `i` na lista combinada (embutidas, depois addons); -1 = desligou a mao.
static void lembrarManual(int i) {
  LegMem e;
  int emb = video_n_legenda();
  if (player_id_canal()[0]) return;   // canal ao vivo: nada a lembrar
  memset(&e, 0, sizeof e);
  tituloAtual(e.titulo, sizeof e.titulo);
  if (i < 0) snprintf(e.idioma, sizeof e.idioma, "none");
  else if (i < emb) {
    const VideoFaixa *f = video_legenda(i);
    if (!f) return;
    e.tipo = 'e'; e.numero = f->numero;
    // #383: inteira explicita; id vazio de builds antigos e tipo desconhecido.
    e.id[0] = (f->tipoLeg == LING_LEG_FORCADA || f->tipoLeg == LING_LEG_LETREIROS || f->letreiro) ? 'f' :
              (f->tipoLeg == LING_LEG_COMPLETA || f->tipoLeg == LING_LEG_SDH) ? 'c' : 0;
    idiomaLembravel(f->idioma, e.idioma, sizeof e.idioma);
    snprintf(e.nome, sizeof e.nome, "%s", f->rotulo);
  } else { lembrarAddon(addons_legenda(i - emb)); return; }
  lembrarEscolha(&e);
}

static void aplicar(void) {
  if (coluna == 0) {
    // F06: outra faixa de audio = outras falas; a escuta em curso nao vale mais.
    if (video_audio_atual() != foco[0]) legsync_audio_trocou();
    // A ordem automatica (audioescolha.h) volta a esta faixa neste titulo, e
    // nos outros episodios: gravada no perfil (audiomemoria.h), com o numero
    // dela para a mesma release (bingeGroup).
    { const VideoFaixa *f = video_audio(foco[0]);
      if (f && !player_id_canal()[0])
        audioesc_lembrar_faixa(f->idioma, f->codec, f->rotulo, f->canais,
                              video_audio_numero_memoria(f)); }
    video_escolher_audio(foco[0]);
  } else {
    // A pessoa escolheu: a automatica nao mexe mais nesta sessao, nem se a
    // escolha foi "Desativada".
    legAuto = 0;
    { int i = legDaLinha(foco[1] - 1); escolherLegenda(i); lembrarManual(i); }
  }
}

// LEGENDA AUTOMATICA (#129). Roda a cada quadro enquanto `legAuto` esta de pe,
// e so decide quando da para decidir bem (ling_legenda_auto). Os prazos existem
// porque as duas listas podem nunca "fechar": a sonda do MKV so dispara com
// buffer saudavel, e numa fonte lenta isso demora; o fio de legendas consulta
// cada addon com 25 s de teto. Vencido o prazo, decide-se com o que ha.
#define FX_AUTO_EMB_MS  10000u   // espera pelos idiomas das embutidas (era 30 s: a legenda do addon ja estava na mao)
#define FX_AUTO_FIM_MS  30000u   // desiste de vez: legenda ligada no minuto 5 assusta
// A mesma decisao de faixa exata antes e depois do sourceInfo.
static int legendaExata(const LegMem *mt, int nEmb, int nAdd) {
  int nums[NV_FAIXA_MAX];
  const char *emb[NV_FAIXA_MAX], *nomes[NV_FAIXA_MAX], *ids[LEG_MAX];
  char idsBuf[LEG_MAX][24];
  int i;
  for (i = 0; i < nEmb; i++) {
    const VideoFaixa *f = video_legenda(i);
    nums[i] = f ? f->numero : -1;
    emb[i] = f ? f->idioma : "";
    nomes[i] = f ? f->rotulo : "";
  }
  for (i = 0; i < nAdd; i++) { legendasui_id_addon(addons_legenda(i), idsBuf[i]); ids[i] = idsBuf[i]; }
  return legmem_exata(mt, nums, emb, nomes, nEmb, ids, nAdd);
}
static void legendaPreDownload(void) {
  char tit[64]; int origem, r, n;
  const LegMem *mt;
  const CatItem *ci;
  if (!legAuto || !player_aberto() || player_com_video() || player_id_canal()[0]) return;
  ci = cat_item(player_indice());
  if (!ci || !ci->imdb[0]) return;
  tituloAtual(tit, sizeof tit);
  mt = tit[0] ? legmem_do_titulo(tit) : NULL;
  legmem_preferencia(ling_legenda(), mt, legmem_ultima(), &origem);
  if (origem != LEGMEM_DE_TITULO || mt->tipo != 'a') return;
  n = addons_n_legendas(); if (n > LEG_MAX) n = LEG_MAX;
  r = legendaExata(mt, 0, n);
  if (r >= 0) {
    const Legenda *l = addons_legenda(r);
    if (l) legenda_pre_baixar(l->url);
  }
}
static void legendaAutomatica(Uint32 agora) {
  const char *emb[NV_FAIXA_MAX], *add[LEG_MAX], *audio, *pref;
  const VideoFaixa *fa;
  const CatItem *ci;
  const LegMem *mt;
  char tit[64], addIdiomas[LEG_MAX][sizeof ((Legenda *)0)->idioma];
  int tipos[NV_FAIXA_MAX];
  int nEmb, nAdd = 0, embFechado, addFechado = 1, i, r, soForcada, origem, forcadaAuto;
  Uint32 passou;
  if (!legAuto || aberta || !player_aberto() || !player_com_video()) return;
  // Canal ao vivo nao tem legenda de addon nem idioma no arquivo que valha.
  if (player_id_canal()[0]) { legAuto = 0; return; }
  // A lista de faixas so existe depois do sourceInfo (LG) / lerFaixas (Tizen).
  // Antes dele a sonda "ja voltou" por falta de pendencia (ver
  // video_mkv_sondado) e as embutidas pareceriam fechadas e vazias.
  if (!legAutoDesde) legAutoDesde = agora | 1u;
  // `| 1` poe o marco 1 ms A FRENTE de um `agora` par: sem a guarda, o
  // primeiro quadro dava agora - marco = -1, e o log dizia "aos 4294967295 ms".
  passou = (Sint32)(agora - legAutoDesde) > 0 ? agora - legAutoDesde : 0;
  if (!video_n_audio() && !video_n_legenda() && passou < 8000u) return;
  // 2.0.3: A PREFERENCIA EM VIGOR nao e mais so a de Ajustes/da conta. Antes,
  // sem idioma em Ajustes (o caso da TCL do dono) isto decidia NADA no
  // primeiro quadro e a legenda escolhida a mao nunca voltava. Agora: a
  // escolha deste titulo > Ajustes > a ultima escolha a mao (legmemoria.h).
  ci = cat_item(player_indice());
  tituloAtual(tit, sizeof tit);
  mt = tit[0] ? legmem_do_titulo(tit) : NULL;
  pref = legmem_preferencia(ling_legenda(), mt, legmem_ultima(), &origem);
  nEmb = video_n_legenda();
  if (nEmb > NV_FAIXA_MAX) nEmb = NV_FAIXA_MAX;
  // Faixa de LETREIROS/FORCADA nunca liga como a legenda inteira: nao traduz
  // o dialogo, e ligada pela preferencia parece legenda quebrada. #287: ela so
  // liga quando o AUDIO ja esta no idioma da legenda (ling_legenda_auto_tipo).
  for (i = 0; i < nEmb; i++) {
    const VideoFaixa *f = video_legenda(i);
    emb[i] = f ? f->idioma : "";
    tipos[i] = !f ? LING_LEG_COMUM : f->tipoLeg ? f->tipoLeg : f->letreiro ? LING_LEG_LETREIROS : LING_LEG_COMUM;
  }
  fa = video_audio(video_audio_atual());
  audio = fa ? fa->idioma : "";
  // #383: A LEGENDA INTEIRA ESCOLHIDA A MAO NESTE TITULO vence o #287. A faixa
  // exata quase nunca existe no episodio seguinte (addon: o id leva a URL, que
  // e de UM episodio; embutida: outro numero em outra fonte) e o que sobra e o
  // idioma — com o audio no mesmo idioma o #287 respondia "so a forcada" e a
  // escolha sumia: serie em ingles com legenda em ingles abria sem legenda.
  forcadaAuto = ajustes_legenda_forcada_auto() &&
                !(origem == LEGMEM_DE_TITULO &&
                  (mt->tipo == 'a' || (mt->tipo == 'e' && !strcmp(mt->id, "c"))));
  soForcada = forcadaAuto && audio[0] && pref[0] &&
              strcasecmp(pref, "none") && ling_casa(audio, pref);
  embFechado = video_mkv_sondado() != 0 || passou >= FX_AUTO_EMB_MS;
  // So confia na lista dos addons quando ela e DESTE titulo: sem imdb o app
  // nao pede legenda nenhuma (app.c), e o que estiver em memoria e do anterior.
  if (ci && ci->imdb[0]) {
    nAdd = addons_n_legendas();
    if (nAdd > LEG_MAX) nAdd = LEG_MAX;
    for (i = 0; i < nAdd; i++) {
      const Legenda *l = addons_legenda(i);
      snprintf(addIdiomas[i], sizeof addIdiomas[i], "%s", l ? l->idioma : "");
      add[i] = addIdiomas[i];
    }
    addFechado = addons_legendas_prontas();
  }
  if (passou >= FX_AUTO_FIM_MS) embFechado = addFechado = 1;
  // O MESMO TITULO: a faixa exata que a pessoa escolheu, se ainda existe. Ela
  // vale mais que "a primeira do idioma" (o arquivo pode ter pt e pt-BR, ou a
  // completa e a SDH). Ainda nao apareceu e a lista dela nao fechou: espera.
  if (origem == LEGMEM_DE_TITULO && mt->tipo) {
    r = legendaExata(mt, nEmb, nAdd);
    if (r < 0 && !(mt->tipo == 'e' ? embFechado : addFechado)) return;
    if (r >= 0) {
      legAuto = 0;
      printf("[legenda] automatica: ultima escolha '%s' -> %s %d (faixa exata deste titulo) aos %u ms\n",
             mt->idioma[0] ? mt->idioma : "?", r < nEmb ? "embutida" : "addon",
             r < nEmb ? r : r - nEmb, (unsigned)passou);
      fflush(stdout);
      if (r == legendaAtiva() &&
          !(r < nEmb && legOverlay != r && vaiOverlay(video_legenda(r)) && video_legenda_ordinal_mkv(r) >= 0))
        return;
      escolherLegenda(r);
      return;
    }
  }
  r = ling_legenda_auto_tipo(pref, audio, forcadaAuto,
                             emb, tipos, nEmb, embFechado, add, nAdd, addFechado);
  if (r == LING_AUTO_ESPERA) {
    // Passou do primeiro instante e ainda procura: a ilha diz o que esta fazendo.
    if (!soForcada && passou >= 1200u && pref[0] && strcasecmp(pref, "none")) pilBuscando(pref, agora);
    return;
  }
  legAuto = 0;
  if (r == LING_AUTO_NADA && soForcada) {
    // #287: o audio ja esta no idioma da legenda e o arquivo nao tem forcada.
    // Sem legenda e o pedido, nao uma falha: a ilha fica quieta.
    pilBusca = 0;
    printf("[legenda] automatica: audio em '%s' = legenda '%s', sem forcada: nenhuma\n",
           audio, pref);
    fflush(stdout);
    return;
  }
  if (r == LING_AUTO_NADA) {
    pilBusca = 0;
    // A PESSOA DESLIGOU A MAO (neste titulo, ou por ultimo): nenhuma, mesmo
    // que o arquivo marque uma faixa como padrao.
    if (!strcasecmp(pref, "none") && (origem == LEGMEM_DE_TITULO || origem == LEGMEM_DE_ULTIMA)) {
      printf("[legenda] automatica: ultima escolha 'none' -> nenhuma\n");
      fflush(stdout);
      if (legendaAtiva() >= 0) escolherLegenda(-1);
      return;
    }
    if (pref[0] && strcasecmp(pref, "none")) {
      // Antes era silencio: a pessoa nao sabia se o app tentou. Agora a ilha diz.
      pilFalhou(pref, agora);
      printf("[legenda] automatica: nada em '%s' (%d embutida(s), %d de addon)\n",
             pref, nEmb, nAdd);
    }
    fflush(stdout);
    return;
  }
  // Ja esta nela (o arquivo marcou a faixa como padrao): nao religa — a nao
  // ser que seja ASS com a TV desenhando: ai o overlay do app assume, que e o
  // motivo do #92 (e o que adota a pre-busca feita antes do video).
  if (r == legendaAtiva() &&
      !(r < nEmb && legOverlay != r && vaiOverlay(video_legenda(r)) && video_legenda_ordinal_mkv(r) >= 0))
    return;
  // Varias legendas do idioma: a melhor, nao a primeira que respondeu (idioma
  // exato, nome parecido com o arquivo, a que ja deu certo neste titulo).
  if (r >= nEmb) {
    Legenda *v = malloc(sizeof *v * LEG_MAX);
    if (v) {
      int nv = addons_legendas_copiar(v, LEG_MAX, NULL, NULL);
      uint64_t lem = ci ? legauto_lembrada(ci->imdb[0] ? ci->imdb : ci->titulo, pref) : 0;
      int b = legauto_escolher(v, nv, pref, legauto_midia(video_url_atual()), NULL, 0, lem);
      if (b >= 0 && b < nAdd) r = nEmb + b;
      free(v);
    }
  }
  if (origem == LEGMEM_DE_TITULO || origem == LEGMEM_DE_ULTIMA)
    printf("[legenda] automatica: ultima escolha '%s' -> %s %d (%s%s) aos %u ms\n", pref,
           r < nEmb ? "embutida" : "addon", r < nEmb ? r : r - nEmb,
           r < nEmb ? emb[r] : add[r - nEmb],
           r < nEmb && tipos[r] == LING_LEG_FORCADA ? ", forcada: audio no mesmo idioma" :
           r < nEmb && tipos[r] == LING_LEG_LETREIROS ? ", letreiros: audio no mesmo idioma" : "",
           (unsigned)passou);
  else
    printf("[legenda] automatica: '%s' -> %s %d (%s%s) aos %u ms\n", pref,
           r < nEmb ? "embutida" : "addon", r < nEmb ? r : r - nEmb,
           r < nEmb ? emb[r] : add[r - nEmb],
           r < nEmb && tipos[r] == LING_LEG_FORCADA ? ", forcada: audio no mesmo idioma" :
           r < nEmb && tipos[r] == LING_LEG_LETREIROS ? ", letreiros: audio no mesmo idioma" : "",
           (unsigned)passou);
  fflush(stdout);
  escolherLegenda(r);
}

// Os passos de ESQUERDA/DIREITA do volume, da velocidade e do atraso da barra
// de estilo: um caminho so para a seta e para os discos < > do ponteiro.
static void passoVolume(int d) {
  int antes = cacheboost_volume(), v = cacheboost_volume_passo(d);
  if (v != antes) cacheboost_backend_ganho(v);
}
static void passoVelocidade(int d) {
  int antes = video_velocidade_atual(), v = vel_passo(antes, d);
  if (v != antes) { video_velocidade(v); velPedidaUi = v; }
}
// As teclas da aba Estilo. Devolve 0 so para "Voltar" sem controle aberto (o
// chamador fecha o painel); o resto e sempre tratado aqui.
static int estiloTecla(SDL_Keycode k, int rep) {
  int *f = &foco[FX_COL_ESTILO];
  int voltar = k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE;
  int ok = k == SDLK_RETURN || k == SDLK_KP_ENTER;
  int dx = k == SDLK_RIGHT ? 1 : k == SDLK_LEFT ? -1 : 0, dy = k == SDLK_DOWN ? 1 : k == SDLK_UP ? -1 : 0;
  ctlRep = rep ? ctlRep + 1 : 0;
  if (ctl >= 0) {
    int t = estTipo(ctl);
    if (voltar) { ctl = -1; return 1; }
    if (ok) { if (t > 1) estEscolher(ctlSub); ctl = -1; return 1; }
    if (t == 1) { if (dx) estPasso(ctl, dx, rep); }
    else if (t == 2) { if (dx) estEscolher((ctlSub + dx + VIDEO_LEG_NCORES) % VIDEO_LEG_NCORES); }
    else {   // fontes: 2 colunas, bundled followed by imported faces
      int n = ctlSub + dx + dy * 2;
      if (n >= 0 && n < TXT_FAMILIA_N + legfontes_n()) estEscolher(n);
    }
    return 1;
  }
  if (voltar) return 0;
  // A grade de FX_BARRA_COLS colunas: as quatro setas andam nela; a esquerda da
  // primeira coluna volta para a lista de legendas.
  if (dx < 0) { if (*f % FX_BARRA_COLS) (*f)--; else coluna = 1; }
  else if (dx > 0) { if (*f % FX_BARRA_COLS < FX_BARRA_COLS - 1 && *f + 1 < FX_N_ESTILO) (*f)++; }
  else if (dy < 0) { if (*f >= FX_BARRA_COLS) *f -= FX_BARRA_COLS; }
  else if (dy > 0) { if (*f + FX_BARRA_COLS < FX_N_ESTILO) *f += FX_BARRA_COLS; }
  else if (ok) {
    if (estTipo(*f)) estAbrir(*f); else ciclarEstilo(*f);
  }
  return 1;
}

void faixas_evento(const SDL_Event *e) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo() && toquerol_navegacao(e)) {
    rolagem[0] = (int)(toqueAudioOffset / 92.0f);
    toquerol_limpar(&toqueAudio);
  }
#endif
  SDL_Keycode k;
  if (!aberta) return;
  // F04: the subtitle LIST is the selector of legendasui.c (simple view and
  // "Mais opções"); the Estilo bar below stays here.
  if (modo && coluna == 1) {
    int r = legendasui_evento(e);
    if (r == LEGUI_FECHAR) aberta = 0;
    else if (r == LEGUI_ESTILO) coluna = FX_COL_ESTILO;
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  // ESTILO: Voltar fecha antes o controle aberto (slider, paleta, fontes).
  if (coluna == FX_COL_ESTILO && estiloTecla(k, e->key.repeat)) return;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE) { aberta = 0; return; }
  // F07: the volume row of the audio sheet. The change is live (no OK needed);
  // OK just closes, like Back.
  if (!modo && volFoco) {
    if (!volFocavel()) { volFoco = 0; return; }
    if (k == SDLK_LEFT || k == SDLK_RIGHT) { passoVolume(k == SDLK_RIGHT ? 1 : -1); return; }
    if (k == SDLK_DOWN) {
      if (velFocavel()) { volFoco = 0; velFoco = 1; }
      else if (nLinhas(0) > 0) volFoco = 0;
      return;
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { aberta = 0; return; }
    return;
  }
  // #202: the speed row. The change is live, like the volume; OK closes.
  if (!modo && velFoco) {
    if (!velFocavel()) { velFoco = 0; return; }
    if (k == SDLK_LEFT || k == SDLK_RIGHT) { passoVelocidade(k == SDLK_RIGHT ? 1 : -1); return; }
    if (k == SDLK_UP) { if (volFocavel()) { velFoco = 0; volFoco = 1; } return; }
    if (k == SDLK_DOWN) { if (nLinhas(0) > 0) velFoco = 0; return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { aberta = 0; return; }
    return;
  }
  if (!modo && k == SDLK_UP && foco[0] == 0 && velFocavel()) { velFoco = 1; return; }
  if (!modo && k == SDLK_UP && foco[0] == 0 && volFocavel()) { volFoco = 1; return; }
  // Esquerda/direita andam entre a LISTA e o ESTILO, e so na folha de legenda.
  // Na de audio nao ha para onde ir — antes elas pulavam para a coluna de
  // legenda, que e justamente o que fazia os dois botoes do player parecerem o
  // mesmo botao.
  if (k == SDLK_LEFT || k == SDLK_RIGHT) return;
  if (k == SDLK_UP)    { if (foco[coluna] > 0) foco[coluna]--; ajustarRolagem(); return; }
  if (k == SDLK_DOWN)  { if (foco[coluna] < nLinhas(coluna) - 1) foco[coluna]++;
                         ajustarRolagem(); return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
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
  player_toast_ex(b, 6000, "aj_triangle-alert", 1);
}

void faixas_atualizar(float dt, Uint32 agora) {
  if (aberta && modo && coluna == 1) legendasui_atualizar(dt);
  // #202: the platform refused the speed (LG/.tpk answer after the request):
  // the row is gone and playback is back at 1x; say it once.
  if (velPedidaUi != VEL_NORMAL && !video_velocidade_suportada()) {
    velPedidaUi = VEL_NORMAL; velFoco = 0;
    player_toast_ex(i18n("Esta TV não aceitou mudar a velocidade."), 6000, "aj_triangle-alert", 1);
  }
  // Audio turned passthrough while sped up (another track, a receiver): the
  // pipeline cannot keep the speed, so it goes back to 1x and the row says why.
  if (video_velocidade_bloqueada() && video_velocidade_atual() != VEL_NORMAL) {
    vel_log(video_velocidade_atual(), "indisponivel: audio em passthrough, volta a 1x");
    video_velocidade(VEL_NORMAL); velPedidaUi = VEL_NORMAL; velFoco = 0;
  }
  anim = anim_mola(anim, aberta ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  // ESTILO: a grade vira o controle aberto (mola da tela); o marcador do
  // slider e a amostra em foco perseguem o alvo (segurar a seta desliza).
  if (!aberta) ctl = -1;
  ctlAnim = anim_mola(ctlAnim, ctl >= 0 ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (estSlider(ctl)) ctlFrac = anim_mola(ctlFrac, estFrac(ctl), dt, NV_MOLA_FOCO);
  { int i; for (i = 0; i < VIDEO_LEG_NCORES; i++)
      ctlAmostra[i] = anim_mola(ctlAmostra[i], ctl == EST_COR && i == ctlSub ? 1.0f : 0.0f, dt, NV_MOLA_FOCO); }
  // Fechando a partir da barra, ela sai como barra: sem isto a folha inteira
  // piscaria no caminho de volta.
  if (aberta || anim < .01f) animTopo = anim_mola(animTopo, faixas_estilo_topo() ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  legendaPreDownload();
  legendaAutomatica(agora);
  if (!legAuto) legenda_pre_descartar("automatica encerrada");
  pilAtualizar(agora);
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
      // "VOLTA ao app" e toast so quando o app tinha DEIXADO de desenhar
      // (falha(s) antes). Faixa de TEXTO no primeiro cue e o handoff normal —
      // "a TV segura ate os cues chegarem" — e nao tem "voltou" nenhum.
      int voltou = legOverlayFalhas > 0;
      if (voltou)
        printf("[legenda] faixa %d: o mkvass voltou a entregar (%d blocos, depois de %d falha(s))%s\n",
               legOverlay, col, legOverlayFalhas,
               legOverlayTV ? ": a faixa VOLTA ao app (nativa desligada)" : "");
      else
        printf("[legenda] faixa %d: %d blocos de texto chegaram: o app assume (nativa desligada)\n",
               legOverlay, col);
      fflush(stdout);
      if (legOverlayTV) {
        video_escolher_legenda(-1);
        if (voltou)
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
        player_toast_ex(i18n("Legenda ASS: a TV desenha por enquanto (falha de rede); o app tenta de novo"), 6000, "aj_wifi-off", 1);
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
      if (f && vaiOverlay(f) && ord >= 0 && video_legenda_atual() == i && video_url_atual()[0]) {
        printf("[legenda] sonda voltou: faixa %d (%s), o app assume\n", i, f->codec);
        overlayAssumir(i, ord);
      } else {
        printf("[legenda] sonda voltou: faixa %d (%s, codec=%s) fica na TV: %s\n", i,
               f ? f->rotulo : "?", f && f->codec[0] ? f->codec : "?", motivoTV(i));
        fflush(stdout);
        // Sem par no arquivo e uma falha do casamento, nao da TV: avisa, para
        // a pessoa poder mandar o log em vez de achar que a legenda e assim.
        if (f && !f->codec[0] && video_mkv_sondado() == 1)
          player_toast_ex(i18n("Legenda: n\xc3\xa3o deu para casar as faixas com o arquivo; a TV desenha"), 6000, "aj_triangle-alert", 1);
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

// --- O DESENHO: A ILHA DO RELOGIO CRESCIDA (Glass UI, mockup de 03/10) ---------
//
// A folha cobria a tela com um veu de .80-.88 e duas colunas lado a lado, com
// contorno em cada linha e anel de acento no foco. Agora a pilula do canto
// CRESCE ate a lista (plrilha.h): a linha da pilula (hora · termina as) fica
// como cabecalho, embaixo o kicker com o titulo, "Audio"/"Legendas", as linhas
// (o rosto do idioma, nome, a linha de baixo do rotulo) e o rodape com a
// posicao e as dicas. A faixa em uso leva o check no acento (estado); o foco e
// a superficie um degrau mais clara. Com o foco no Estilo a MESMA ilha se
// alarga no topo (5 x 2 opcoes) e a previa fica no video, onde a legenda toca.
#define IL_W        720.0f
#define IL_PAD_X     18.0f
#define IL_PAD_Y     22.0f
#define IL_TIT_H     42.0f
#define IL_LN_H      88.0f
#define IL_LN_VAO     4.0f
#define IL_PE_H      47.0f
// Linhas da lista a vista: 6, ou menos se a ilha nao couber na altura da tela
// virtual (escala.h; em 150% sao 4). A conta: o miolo fixo da ilha (183), a
// pilula da hora por cima (~60) e as margens de cima e de baixo.
static int ilVisN(int extra) {
  int n = (int)((NV_TELA_H - 48.0f - 40.0f - 60.0f - 183.0f + 4.0f) / (88.0f + 4.0f)) - extra;
  return n > 6 ? 6 : n < 2 ? 2 : n;
}
#define IL_VIS        ilVisN(0)
// The audio sheet spends one line on the volume row (F07) and one more on the
// speed row (#202) where the platform has speed.
#define IL_VIS_COL(c) ilVisN((c) == 0 ? 1 + (velVisivel() ? 1 : 0) : 0)
// O canvas Mobile normaliza a borda curta em 1080: 144 unidades sao
// 48 dp num telefone de 360 dp, antes da escala escolhida para a interface.
static float estiloToqueMin(void) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) {
    float s = gfx_escala_ui();
    return s > 0 ? 144.0f / s : 0;
  }
#endif
  return 0;
}
static float estiloCelulaH(void) {
  float min = estiloToqueMin();
  return min > 92.0f ? min : 92.0f;
}
static float estiloPadX(void) { return telefoneui_ativo() ? 12.0f : IL_PAD_X; }
#define IL_EST_CEL_H estiloCelulaH()
#define IL_EST_TOPO  56.0f

// "Ingles  ·  Atmos 7.1" -> "Ingles" e "Atmos 7.1": o nome na linha forte, o
// resto do rotulo (canal, Atmos) embaixo, como no mockup.
static void partirRotulo(const char *r, char *nome, size_t tn, char *sub, size_t ts) {
  const char *p = strstr(r, "\xc2\xb7");
  size_t n = p ? (size_t)(p - r) : strlen(r);
  while (n && r[n - 1] == ' ') n--;
  if (n >= tn) n = tn - 1;
  memcpy(nome, r, n); nome[n] = 0;
  sub[0] = 0;
  if (p) { p += 2; while (*p == ' ') p++; snprintf(sub, ts, "%s", p); }
  plrui_limpar_sep(nome); plrui_limpar_sep(sub);
}

// O "rosto" do idioma: o codigo em caixa alta num quadrado de 52.
static void rostoIdioma(GfxRect r, const char *idioma, const char *icone, int sel, float a) {
  if (ajustes_vidro()) gfx_cor(r, 16.0f / r.h, 1, 1, 1, (sel ? 0.12f : 0.07f) * a);
  else if (sel) gfx_cor(r, 16.0f / r.h, 0.22f, 0.227f, 0.259f, a);
  else gfx_cor(r, 16.0f / r.h, 0.125f, 0.129f, 0.153f, a);
  if (icone) {
    gfx_icone((GfxRect){ r.x + 15.0f, r.y + 15.0f, 22.0f, 22.0f }, icone, 1, 1, 1, (sel ? 1.0f : 0.7f) * a);
  } else if (idioma && idioma[0]) {
    const char *c = ling_selo(idioma);
    int k = sel ? 255 : 178;
    TxtLinha l = txt_linha(strlen(c) > 3 ? TXT_ILHA_APOIO : TXT_G16B, c, k, k, k, 255);
    txt_desenhar_alpha(l, r.x + (r.w - (float)l.w) * 0.5f, r.y + (r.h - (float)l.h) * 0.5f, a);
  }
}

// Uma linha da lista (audio ou legenda) em (x, y), largura w.
static void linhaLista(int col, int i, float x, float y, float w, float a) {
  int sel = col == coluna && i == foco[col] && !(col == 0 && (volFoco || velFoco));
  const char *marca = NULL, *icone = NULL, *idioma = NULL;
  char nome[64], sub[96];
  int ativo;
  GfxRect lr = { x, y, w, IL_LN_H };
  nome[0] = sub[0] = 0;
  if (!col) {
    const VideoFaixa *f = video_audio(i);
    partirRotulo(f ? f->rotulo : "", nome, sizeof nome, sub, sizeof sub);
    idioma = f ? f->idioma : NULL;
  } else if (i == 0) {
    snprintf(nome, sizeof nome, "%s", i18n("Nenhuma"));
    snprintf(sub, sizeof sub, "%s", i18n(player_id_canal()[0] && !nLegendas()
                                         ? "Sem legendas neste canal" : "Sem legenda"));
    icone = "pl_eye-off";
  } else {
    int k = legDaLinha(i - 1), emb = video_n_legenda();
    const char *rot = rotuloLegenda(k, &marca);
    snprintf(nome, sizeof nome, "%s", rot);
    snprintf(sub, sizeof sub, "%s", marca ? marca : i18n("Incorporada"));
    if (k < emb) { const VideoFaixa *f = video_legenda(k); idioma = f ? f->idioma : NULL; }
    else { const Legenda *l = addons_legenda(k - emb); idioma = l ? l->idioma : NULL; }
  }
  if (sel) plrui_linha_foco(lr, 22.0f, a);
  rostoIdioma((GfxRect){ x + 22.0f, y + (IL_LN_H - 52.0f) * 0.5f, 52.0f, 52.0f }, idioma, icone, sel, a);
  ativo = col == 0 ? i == video_audio_atual() : legDaLinha(i - 1) == legendaAtiva();
  { float tx = x + 22.0f + 52.0f + 18.0f, tw = w - (tx - x) - 22.0f - (ativo ? 46.0f : 0.0f);
    int c = sel ? 255 : 219, cs = 122;
    TxtLinha ln = txt_linha_corta(TXT_ILHA_FORTE, nome, c, c, c - 2, 255, tw);
    TxtLinha ls = txt_linha_corta(TXT_ILHA_GENERO, sub, cs, cs, cs - 2, 255, tw);
    float th = (float)ln.h + (sub[0] ? 5.0f + (float)ls.h : 0.0f), ty = y + (IL_LN_H - th) * 0.5f;
    txt_desenhar_alpha(ln, tx, ty, a);
    if (sub[0]) txt_desenhar_alpha(ls, tx, ty + (float)ln.h + 5.0f, a); }
  if (ativo) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    // Icone e nao o caractere (#186): so a Inter tem o glifo.
    gfx_icone((GfxRect){ x + w - 22.0f - 28.0f, y + (IL_LN_H - 28.0f) * 0.5f, 28.0f, 28.0f },
              "pl_check", ar, ag, ab, a);
  }
}

static int visiveisLista(int col) { int n = nLinhas(col); return n < IL_VIS_COL(col) ? n : IL_VIS_COL(col); }

// PONTEIRO (#99) NAS LINHAS DE VOLUME E VELOCIDADE. Passar por cima poe o foco
// como as setas (volFoco/velFoco, zerando o outro). O OK delas FECHA a folha,
// e clicar no controle para fechar seria surpresa: o clique na LINHA so foca
// (ativar proprio = o mesmo focar) e os discos < >, que aparecem com a linha
// em foco, sao os botoes — ESQUERDA/DIREITA, o mesmo passo da seta.
static void ponteiroVol(int a, int b) {
  (void)a; (void)b;
  if (!aberta || modo || !volFocavel() || (volFoco && coluna == 0)) return;
  coluna = 0; volFoco = 1; velFoco = 0;
}
static void ponteiroVel(int a, int b) {
  (void)a; (void)b;
  if (!aberta || modo || !velFocavel() || (velFoco && coluna == 0)) return;
  coluna = 0; velFoco = 1; volFoco = 0;
}
static void ponteiroVolPasso(int d, int b) {
  if (!aberta || modo || !volFocavel()) return;
  ponteiroVol(d, b); passoVolume(d);
}
static void ponteiroVelPasso(int d, int b) {
  if (!aberta || modo || !velFocavel()) return;
  ponteiroVel(d, b); passoVelocidade(d);
}
// Disco < > clicavel, com folga em volta (ele tem 34 px).
static void alvoDisco(GfxRect d, PonteiroFn focar, PonteiroFn ativar, int passo) {
  ponteiro_alvo(d.x - 6.0f, d.y - 6.0f, d.w + 12.0f, d.h + 12.0f, focar, ativar, passo, 0);
}

// F07: the island's red (the live dot of ilha.c), the app's alert color.
#define VOL_VERMELHO 1.0f, 0.353f, 0.322f
#define VOL_BARRA_W 112.0f
#define VOL_VAO     12.0f   // gap between the volume row and the track list

// The volume row: the same anatomy as a track row (face, name, line below)
// and, on the right, the 0-200% bar with the 100% tick and the value. Above
// 100 the value and the part of the bar past the tick turn red.
static void linhaVolume(float x, float y, float w, float a) {
  int disp = volFocavel(), pt = cacheboost_ganho_estado() == CB_GANHO_PASSTHROUGH;
  int sel = disp && volFoco && !modo, v = cacheboost_volume(), acima = v > CB_VOL_NORMAL;
  float da = disp ? a : a * 0.5f, dir = x + w - 22.0f, tx, tw;
  GfxRect lr = { x, y, w, IL_LN_H };
  int pont = a > 0.99f && ponteiro_ativo();   // so com a ilha assentada
  const char *sub = !disp ? "Não disponível nesta TV"
                  : pt    ? "Passthrough: o receptor controla o volume"
                  : acima ? "Reforço ativo, só neste vídeo" : "Só neste vídeo";
  if (sel) plrui_linha_foco(lr, 22.0f, a);
  if (pont && disp && !modo) ponteiro_alvo(x, y, w, IL_LN_H, ponteiroVol, ponteiroVol, 0, 0);
  rostoIdioma((GfxRect){ x + 22.0f, y + (IL_LN_H - 52.0f) * 0.5f, 52.0f, 52.0f }, NULL, "pl_audio-lines", sel, da);
  if (disp) {
    char val[16];
    float yc = y + IL_LN_H * 0.5f, bx, fr = (float)v / (float)CB_VOL_MAX, meio;
    int c = sel ? 255 : 219;
    TxtLinha lv;
    snprintf(val, sizeof val, "%d%%", v);
    lv = acima ? txt_linha(TXT_ILHA_FORTE, val, 255, 90, 82, 255) : txt_linha(TXT_ILHA_FORTE, val, c, c, c - 2, 255);
    // The value has a fixed slot ("200%") so the bar does not walk while it changes.
    { float slot = (float)txt_largura(TXT_ILHA_FORTE, "200%");
      if (sel) {
        GfxRect d = { dir - 34.0f, yc - 17.0f, 34.0f, 34.0f };
        gfx_cor(d, 0.5f, 1, 1, 1, 0.10f * a);
        gfx_icone((GfxRect){ d.x + 7.0f, d.y + 7.0f, 20.0f, 20.0f }, "pl_chevron-right", 1, 1, 1, a);
        if (pont) alvoDisco(d, ponteiroVol, ponteiroVolPasso, 1);
        dir -= 34.0f + 10.0f;
      }
      txt_desenhar_alpha(lv, dir - (float)lv.w, yc - (float)lv.h * 0.5f, a);
      dir -= slot + 16.0f;
      if (sel) {
        GfxRect d = { dir - 34.0f, yc - 17.0f, 34.0f, 34.0f };
        gfx_cor(d, 0.5f, 1, 1, 1, 0.10f * a);
        gfx_icone((GfxRect){ d.x + 7.0f, d.y + 7.0f, 20.0f, 20.0f }, "pl_chevron-left", 1, 1, 1, a);
        if (pont) alvoDisco(d, ponteiroVol, ponteiroVolPasso, -1);
        dir -= 34.0f + 14.0f;
      } }
    // Passthrough: no bar (there is no boost to show); the reason gets the room.
    if (pt) goto semBarra;
    bx = dir - VOL_BARRA_W;
    meio = bx + VOL_BARRA_W * 0.5f;
    gfx_cor((GfxRect){ bx, yc - 3.0f, VOL_BARRA_W, 6.0f }, 0.5f, 1, 1, 1, 0.16f * a);
    if (fr > 0.0f) {
      float fim = bx + VOL_BARRA_W * (fr > 1.0f ? 1.0f : fr);
      gfx_cor((GfxRect){ bx, yc - 3.0f, (fim < meio ? fim : meio) - bx, 6.0f }, 0.5f, 1, 1, 1, 0.85f * a);
      if (fim > meio) gfx_cor((GfxRect){ meio, yc - 3.0f, fim - meio, 6.0f }, 0.5f, VOL_VERMELHO, a);
    }
    // Tick at 100%: the boundary between the normal volume and the boost.
    gfx_cor((GfxRect){ meio - 1.0f, yc - 9.0f, 2.0f, 18.0f }, 0.0f, 1, 1, 1, (pt ? 0.25f : 0.45f) * a);
    dir = bx - 18.0f;
  semBarra:;
  }
  tx = x + 22.0f + 52.0f + 18.0f;
  tw = dir - tx;
  { int c = sel ? 255 : 219, cs = 122;
    TxtLinha ln = txt_linha_corta(TXT_ILHA_FORTE, "Volume", c, c, c - 2, 255, tw);
    TxtLinha ls = txt_linha_corta(TXT_ILHA_GENERO, sub, cs, cs, cs - 2, 255, tw);
    float th = (float)ln.h + 5.0f + (float)ls.h, ty = y + (IL_LN_H - th) * 0.5f;
    txt_desenhar_alpha(ln, tx, ty, da);
    txt_desenhar_alpha(ls, tx, ty + (float)ln.h + 5.0f, da); }
}

// #202: the speed row, the volume row's anatomy (face, name, line below) with
// the value between the < > discs when focused. Away from 1x the value takes
// the accent, as the island's pill does.
static void linhaVelocidade(float x, float y, float w, float a) {
  int bloq = video_velocidade_bloqueada();
  int sel = velFoco && !modo && !bloq, v = video_velocidade_atual();
  float da = bloq ? a * 0.5f : a, dir = x + w - 22.0f, yc = y + IL_LN_H * 0.5f, tx, tw;
  GfxRect lr = { x, y, w, IL_LN_H };
  int pont = a > 0.99f && ponteiro_ativo() && !bloq && !modo;   // so com a ilha assentada
  char val[16];
  TxtLinha lv;
  if (sel) plrui_linha_foco(lr, 22.0f, a);
  if (pont) ponteiro_alvo(x, y, w, IL_LN_H, ponteiroVel, ponteiroVel, 0, 0);
  rostoIdioma((GfxRect){ x + 22.0f, y + (IL_LN_H - 52.0f) * 0.5f, 52.0f, 52.0f }, NULL, "aj_gauge", sel, da);
  vel_rotulo(val, sizeof val, v);
  plrui_decimal(val);
  if (v != VEL_NORMAL) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    lv = txt_linha(TXT_ILHA_FORTE, val, (int)(ar * 255.0f), (int)(ag * 255.0f), (int)(ab * 255.0f), 255);
  } else { int c = sel ? 255 : 219; lv = txt_linha(TXT_ILHA_FORTE, val, c, c, c - 2, 255); }
  // Fixed slot ("1,75x") so the discs do not walk while the value changes.
  { char larga[16]; float slot;
    vel_rotulo(larga, sizeof larga, 175); plrui_decimal(larga);
    slot = (float)txt_largura(TXT_ILHA_FORTE, larga);
    if (sel) {
      GfxRect d = { dir - 34.0f, yc - 17.0f, 34.0f, 34.0f };
      gfx_cor(d, 0.5f, 1, 1, 1, 0.10f * a);
      gfx_icone((GfxRect){ d.x + 7.0f, d.y + 7.0f, 20.0f, 20.0f }, "pl_chevron-right", 1, 1, 1, a);
      if (pont) alvoDisco(d, ponteiroVel, ponteiroVelPasso, 1);
      dir -= 34.0f + 10.0f;
    }
    txt_desenhar_alpha(lv, dir - slot + (slot - (float)lv.w) * 0.5f, yc - (float)lv.h * 0.5f, da);
    dir -= slot + 10.0f;
    if (sel) {
      GfxRect d = { dir - 34.0f, yc - 17.0f, 34.0f, 34.0f };
      gfx_cor(d, 0.5f, 1, 1, 1, 0.10f * a);
      gfx_icone((GfxRect){ d.x + 7.0f, d.y + 7.0f, 20.0f, 20.0f }, "pl_chevron-left", 1, 1, 1, a);
      if (pont) alvoDisco(d, ponteiroVel, ponteiroVelPasso, -1);
      dir -= 34.0f;
    }
    dir -= 18.0f; }
  tx = x + 22.0f + 52.0f + 18.0f;
  tw = dir - tx;
  { int c = sel ? 255 : 219, cs = 122;
    TxtLinha ln = txt_linha_corta(TXT_ILHA_FORTE, "Velocidade", c, c, c - 2, 255, tw);
    TxtLinha ls = txt_linha_corta(TXT_ILHA_GENERO, bloq ? "Indisponível com áudio pelo receptor"
                                  : v == VEL_NORMAL ? "Normal" : "Só neste vídeo",
                                  cs, cs, cs - 2, 255, tw);
    float th = (float)ln.h + 5.0f + (float)ls.h, ty = y + (IL_LN_H - th) * 0.5f;
    txt_desenhar_alpha(ln, tx, ty, da);
    txt_desenhar_alpha(ls, tx, ty + (float)ln.h + 5.0f, da); }
}

static float alturaLista(int col) {
  int n = visiveisLista(col);
  float h = IL_PAD_Y + IL_TIT_H + 14.0f;
  if (col == 0) h += IL_LN_H + VOL_VAO;
  if (col == 0 && velVisivel()) h += IL_LN_H + IL_LN_VAO;
  if (n > 0) h += n * IL_LN_H + (n - 1) * IL_LN_VAO;
  else h += 60.0f;
  return h + 14.0f + IL_PE_H + IL_PAD_Y;
}

// PONTEIRO (#99) NA FOLHA DE AUDIO. A faixa sob o cursor ganha o foco pelas
// mesmas variaveis das setas; o OK do clique aplica e fecha, como o do controle.
static void ponteiroFaixa(int i, int b) {
  (void)b;
  if (!aberta || modo || i < 0 || i >= nLinhas(0)) return;
  coluna = 0; volFoco = velFoco = 0; foco[0] = i;
}

static void corpoLista(GfxRect c, float a) {
  int col = modo ? 1 : 0, n = nLinhas(col), r, fim, i;
  float x0 = c.x + IL_PAD_X, w = c.w - IL_PAD_X * 2.0f, y = c.y + IL_PAD_Y;
  // Titulo (o filme ja esta no cabecalho); a direita, a contagem (audio) ou o segmentado (legendas).
  { TxtLinha t = txt_linha(TXT_ILHA_PERGUNTA, modo ? "Legendas" : "\xc3\x81udio", 243, 242, 239, 255);
    float ty = y;
    txt_desenhar_alpha(t, x0 + 10.0f, ty, a);
    if (!modo) {
      char q[32];
      snprintf(q, sizeof q, i18n("%d faixas"), n);
      { TxtLinha l = txt_linha(TXT_G18R, q, 243, 242, 239, 115);
        txt_desenhar_alpha(l, x0 + w - 10.0f - l.w, ty + (float)t.h - (float)l.h - 8.0f, a); }
    } else {
      const char *rot[2] = { "Faixas", "Estilo" };
      int cont[2] = { n, -1 };
      float sw = plrui_seg(rot, cont, 2, coluna == FX_COL_ESTILO ? 1 : 0, 0, -1.0f, 0, a);
      plrui_seg(rot, cont, 2, coluna == FX_COL_ESTILO ? 1 : 0, 0, x0 + w - 10.0f - sw,
                ty + (float)t.h - 54.0f, a);
    } }
  y += IL_TIT_H + 14.0f;
  if (!modo) {
    if (velVisivel()) { linhaVolume(x0, y, w, a); y += IL_LN_H + IL_LN_VAO;
                        linhaVelocidade(x0, y, w, a); y += IL_LN_H + VOL_VAO; }
    else { linhaVolume(x0, y, w, a); y += IL_LN_H + VOL_VAO; }
  }
  visiveis = IL_VIS_COL(col);
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) {
    if (!toquerol_livre(&toqueAudio)) {
      ajustarRolagem();
      toqueAudioOffset = rolagem[col] * (IL_LN_H + IL_LN_VAO);
    }
    { float passo = IL_LN_H + IL_LN_VAO;
    float area = fminf(n, visiveis) * passo - (n > 0 ? IL_LN_VAO : 0);
    GfxRect ilha;
    toquerol_vincular(&toqueAudio, (GfxRect){x0, y, w, area}, gfx_escala(),
                     0, n * passo - IL_LN_VAO - area, 1, &toqueAudioOffset);
    if (aberta && a > 0.99f) ponteiro_rolagem(toqueAudioRolar);
    gfx_recorte(x0, y, w, area);
    for (i = 0; i < n; i++) {
      float ry = y + i * passo - toqueAudioOffset;
      if (ry + IL_LN_H < y || ry > y + area) continue;
      if (!modo && a > 0.99f) ponteiro_alvo_faixa(x0, ry, w, IL_LN_H, y, y + area, ponteiroFaixa, NULL, i, 0);
      linhaLista(col, i, x0, ry, w, a);
    }
    if (plrilha_rect(&ilha)) gfx_recorte(ilha.x, ilha.y, ilha.w, ilha.h);
    else gfx_recorte(c.x, c.y, c.w, c.h);
    r = 0; fim = n < visiveis ? n : visiveis;
    }
  } else
#endif
  {
    ajustarRolagem();
    r = rolagem[col]; fim = r + visiveis; if (fim > n) fim = n;
    for (i = r; i < fim; i++) {
      if (!modo && a > 0.99f) ponteiro_alvo(x0, y + (i - r) * (IL_LN_H + IL_LN_VAO), w, IL_LN_H, ponteiroFaixa, NULL, i, 0);
      linhaLista(col, i, x0, y + (i - r) * (IL_LN_H + IL_LN_VAO), w, a);
    }
  }
  if (!n) txt_bloco(TXT_ILHA_TEXTO, "Nenhuma faixa disponível nesta fonte.", 160, 160, 158, x0 + 10.0f, y + 12.0f, w - 20.0f, 28, a, 2);
  y += (n ? (fim - r) * IL_LN_H + (fim - r - 1) * IL_LN_VAO : 60.0f) + 14.0f;
  // RODAPE: "2 de 4" e as dicas do codigo, sob um fio de 1 px.
  gfx_cor((GfxRect){ x0, y, w, 1.0f }, 0.0f, 1, 1, 1, 0.07f * a);
  { char q[32];
    float yc = y + 16.0f + 15.0f;
    if (!modo && volFoco) {
      // The real gain in decibels: 200% is +6.0 dB, not "twice as loud".
      int v = cacheboost_volume();
      if (v > 0) { snprintf(q, sizeof q, "%+.1f dB", cacheboost_volume_db(v)); plrui_decimal(q); }
      else snprintf(q, sizeof q, "-\xe2\x88\x9e dB");
    } else if (!modo && velFoco) snprintf(q, sizeof q, i18n("%d de %d"), vel_indice(video_velocidade_atual()) + 1, VEL_N);
    else snprintf(q, sizeof q, i18n("%d de %d"), n ? foco[col] + 1 : 0, n);
    { TxtLinha l = txt_linha(TXT_ILHA_APOIO, q, 243, 242, 239, 115);
      txt_desenhar_alpha(l, x0 + 10.0f, yc - (float)l.h * 0.5f, a); }
    if (!modo && (volFoco || velFoco)) {
      const char *k[2] = { "\xe2\x86\x90 \xe2\x86\x92", "Voltar" }, *rt[2] = { "Ajustar", "Fechar" };
      plrui_dicas(k, rt, 2, x0 + w - 10.0f, yc, 1, a);
    } else if (!modo) {
      const char *k[2] = { "OK", "Voltar" }, *rt[2] = { "Usar esta faixa", "Fechar" };
      plrui_dicas(k, rt, 2, x0 + w - 10.0f, yc, 1, a);
    } else {
      const char *k[3] = { "OK", "\xe2\x86\x92", "Voltar" }, *rt[3] = { "Usar", "Estilo", "Fechar" };
      plrui_dicas(k, rt, 3, x0 + w - 10.0f, yc, 1, a);
    } }
}

// PONTEIRO (#99) NA BARRA DE ESTILO. Passar por cima FOCA (a celula, a amostra,
// a fonte: so o anel, nada muda); o CLIQUE e o OK. No slider, clicar ou
// arrastar poe o marcador sob o cursor. A aba "Faixas" do segmentado volta para
// a lista (o ESQUERDA da primeira coluna).
static void ponteiroEstilo(int i, int b) {
  (void)b;
  if (!faixas_estilo_topo() || ctl >= 0 || i < 0 || i >= FX_N_ESTILO || foco[FX_COL_ESTILO] == i) return;
  foco[FX_COL_ESTILO] = i;
}
static void ponteiroSub(int i, int b) { (void)b; if (ctl >= 0 && i >= 0) ctlSub = i; }
static void ponteiroEscolher(int i, int b) { (void)b; if (ctl >= 0 && i >= 0) { estEscolher(i); ctl = -1; } }
static void ponteiroFecharCtl(int a, int b) { (void)a; (void)b; ctl = -1; }
static float sliderTx, sliderTw;   // o trilho do slider desenhado, em coordenadas virtuais
static void ponteiroSlider(int a, int b) {
  (void)a; (void)b;
  if (ctl >= 0 && estSlider(ctl) && sliderTw > 1.0f)
    estDeFrac(ctl, (ponteiro_x() / gfx_escala() - sliderTx) / sliderTw);
}
static void ponteiroAba(int estilo, int b) { (void)b; faixas_aba_estilo(estilo); }

#ifdef NV_TOUCH_UI
static void celulaEstiloTelefone(int i, GfxRect r, float a, int ativa) {
  int sel = coluna == FX_COL_ESTILO && i == foco[FX_COL_ESTILO];
  int pres = estiloPreservadoAss(i);
  float x = r.x + 20, w = r.w - 40;
  if (sel) plrui_linha_foco(r, 22, a);
  if (ativa) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroEstilo, NULL, i, 0);
  if (i == EST_RESTAURAR) {
    int compacto = r.w < 154;
    float tx = compacto ? r.x + 10 : r.x + 54, tw = compacto ? r.w - 20 : r.w - 74;
    float h = txt_bloco_corta(TXT_G20B, EST_ROT[i], 243, 242, 239, 0, 0, tw, 24, 0, compacto ? 2 : 3);
    gfx_icone((GfxRect){compacto ? r.x + (r.w - 22) * .5f : r.x + 20,
                       compacto ? r.y + 8 : r.y + (r.h - 22) * .5f, 22, 22},
              "pl_rotate-ccw", 1, 1, 1, (sel ? 1 : .75f) * a);
    txt_bloco_corta(TXT_G20B, EST_ROT[i], 243, 242, 239, tx,
                    compacto ? r.y + 36 : r.y + (r.h - h) * .5f, tw, 24, a, compacto ? 2 : 3);
    return;
  }
  char v[48];
  int cor = (i == EST_COR || i == EST_COR_BORDA) && !pres, c = pres ? 120 : sel ? 255 : 217;
  valorEstilo(i, v, sizeof v);
  float lh = txt_bloco_corta(TXT_G16B, EST_ROT[i], 243, 242, 239, 0, 0, w, 20, 0, 2);
  TxtLinha lv = txt_linha_corta(TXT_ILHA_SECAO, v, c, c, c - 2, 255, w - (cor ? 30 : 0));
  float vh = lv.h, total = lh + 6 + vh, y = r.y + (r.h - total) * .5f;
  txt_bloco_corta(TXT_G16B, EST_ROT[i], 243, 242, 239, x, y, w, 20, a * .5f, 2);
  float yv = y + lh + 6;
  if (cor) {
    int cr,cg,cb;
    if (i == EST_COR_BORDA) { unsigned rgb = video_legenda_borda_rgb(player_leg_estilo()->corBorda); cr = (rgb >> 16) & 255; cg = (rgb >> 8) & 255; cb = rgb & 255; }
    else corLegenda(player_leg_estilo()->cor, &cr,&cg,&cb);
    gfx_cor((GfxRect){x,yv+(vh-20)*.5f,20,20},.5f,cr/255.0f,cg/255.0f,cb/255.0f,a);
    x += 30;
  }
  txt_desenhar_alpha(lv, x, yv, a);
}
#endif

// Uma celula do ESTILO: rotulo em cima, valor embaixo; "Restaurar padrao" e acao.
static void celulaEstilo(int i, GfxRect r, float a, int ativa) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { celulaEstiloTelefone(i, r, a, ativa); return; }
#endif
  int sel = coluna == FX_COL_ESTILO && i == foco[FX_COL_ESTILO];
  int pres = estiloPreservadoAss(i);
  if (sel) plrui_linha_foco(r, 22.0f, a);
  if (ativa) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroEstilo, NULL, i, 0);
  if (i == EST_RESTAURAR) {
    TxtLinha l = txt_linha(TXT_G20B, EST_ROT[i], 243, 242, 239, sel ? 255 : 191);
    gfx_icone((GfxRect){ r.x + 20.0f, r.y + (r.h - 22.0f) * 0.5f, 22.0f, 22.0f }, "pl_rotate-ccw", 1, 1, 1, (sel ? 1.0f : 0.75f) * a);
    txt_desenhar_alpha(l, r.x + 20.0f + 22.0f + 12.0f, r.y + (r.h - (float)l.h) * 0.5f, a);
    return;
  }
  { char v[48];
    TxtLinha lr = txt_linha(TXT_G16B, EST_ROT[i], 243, 242, 239, 128), lv;
    float x = r.x + 20.0f, yv;
    int c = pres ? 120 : sel ? 255 : 217;
    valorEstilo(i, v, sizeof v);
    lv = txt_linha_corta(TXT_ILHA_SECAO, v, c, c, c - 2, 255, r.w - 40.0f);
    yv = r.y + (r.h - ((float)lr.h + 6.0f + (float)lv.h)) * 0.5f;
    txt_desenhar_alpha(lr, x, yv, a);
    yv += (float)lr.h + 6.0f;
    if ((i == EST_COR || i == EST_COR_BORDA) && !pres) {
      int cr, cg, cb;
      if (i == EST_COR_BORDA) { unsigned rgb = video_legenda_borda_rgb(player_leg_estilo()->corBorda); cr = (rgb >> 16) & 255; cg = (rgb >> 8) & 255; cb = rgb & 255; }
      else corLegenda(player_leg_estilo()->cor, &cr, &cg, &cb);
      gfx_cor((GfxRect){ x, yv + ((float)lv.h - 20.0f) * 0.5f, 20.0f, 20.0f }, 0.5f, cr / 255.0f, cg / 255.0f, cb / 255.0f, a);
      x += 20.0f + 10.0f;
    }
    txt_desenhar_alpha(lv, x, yv, a); }
}

#define IL_EST_FILEIRAS ((FX_N_ESTILO + FX_BARRA_COLS - 1) / FX_BARRA_COLS)
static float estiloGradeH(void) {
  float h = IL_EST_CEL_H * IL_EST_FILEIRAS + 6.0f * (IL_EST_FILEIRAS - 1);
  if (ctl == EST_FONTE || (ctlVis == EST_FONTE && ctlAnim > .005f)) {
    int rows = (TXT_FAMILIA_N + legfontes_n() + 1) / 2;
    float fontes = estiloCelulaH() * rows + 8.0f * (rows - 1);
    if (fontes > h) h = fontes;
  }
  return h;
}
#define IL_EST_GRADE_H estiloGradeH()

static void fundoCtl(GfxRect r, float a) {
  float raio = 22.0f / r.h;
  if (ajustes_vidro()) gfx_cor(r, raio, 1, 1, 1, 0.07f * a);
  else gfx_cor(r, raio, 0.118f, 0.122f, 0.145f, a);
}

// A MINIATURA da tela (16:9) com a legenda no nivel da Posicao. A conta e a do
// player (base 1000, 48 px por nivel acima do padrao 3, 20 px abaixo).
static void miniaturaPosicao(GfxRect m, float pf, float a) {
  float base = pf >= 3.0f ? 1000.0f - (pf - 3.0f) * 48.0f : 1000.0f + (3.0f - pf) * 20.0f;
  float yb = m.y + m.h * (base / 1080.0f), ar, ag, ab;
  float larg[2] = { 0.56f, 0.38f };   // a de cima e a de baixo
  int i;
  ajustes_acento_marca(&ar, &ag, &ab);
  gfx_cor(m, 0.12f, 0.04f, 0.045f, 0.055f, a);
  gfx_anel(m, 0.12f, 2.0f, 1, 1, 1, 0.22f * a);
  for (i = 0; i < 2; i++) {   // duas linhas de legenda, a de baixo ancora no nivel
    float lw = m.w * larg[i], lh = 6.0f, y = yb - 4.0f - lh - (1 - i) * 9.0f;
    if (y + lh > m.y + m.h - 3.0f) y = m.y + m.h - 3.0f - lh;   // nunca fora do quadro
    gfx_cor((GfxRect){ m.x + (m.w - lw) * 0.5f, y, lw, lh }, 0.5f, ar, ag, ab, a);
  }
}

static void sliderCtl(int i, GfxRect r, float a, int ativo) {
  float cy = r.y + r.h * 0.5f, x1 = r.x + 30.0f, x2 = r.x + r.w - 30.0f, f = anim_clamp(ctlFrac, 0.0f, 1.0f);
  float px, ar, ag, ab;
  ajustes_acento_marca(&ar, &ag, &ab);
  fundoCtl(r, a);
  if (i == 5) {
    GfxRect m = { x1, cy - 34.0f, 121.0f, 68.0f };
    miniaturaPosicao(m, f * 7.0f, a);
    x1 += m.w + 34.0f;
  } else {
    char lo[24], hi[24];
    if (i == 7) { snprintf(lo, sizeof lo, "-%d s", LEGUI_ATRASO_MAX / 1000); snprintf(hi, sizeof hi, "+%d s", LEGUI_ATRASO_MAX / 1000); }
    else { snprintf(lo, sizeof lo, "%d%%", i == 3 ? 25 : EST_SL[i].min); snprintf(hi, sizeof hi, "%d%%", i == 3 ? 100 : EST_SL[i].max); }
    { TxtLinha tl = txt_linha(TXT_G16B, lo, 243, 242, 239, 150), th = txt_linha(TXT_G16B, hi, 243, 242, 239, 150);
      txt_desenhar_alpha(tl, x1, cy - (float)tl.h * 0.5f, a);
      txt_desenhar_alpha(th, x2 - (float)th.w, cy - (float)th.h * 0.5f, a);
      x1 += (float)tl.w + 22.0f; x2 -= (float)th.w + 22.0f; }
  }
  sliderTx = x1; sliderTw = x2 - x1;
  px = x1 + (x2 - x1) * f;
  gfx_cor((GfxRect){ x1, cy - 4.0f, x2 - x1, 8.0f }, 0.5f, 1, 1, 1, 0.16f * a);
  { float z = i == 7 ? 0.5f : 0.0f, fa = (z < f ? z : f), fb = (z < f ? f : z);
    if (fb - fa > 0.001f) gfx_cor((GfxRect){ x1 + (x2 - x1) * fa, cy - 4.0f, (x2 - x1) * (fb - fa), 8.0f }, 0.5f, ar, ag, ab, a); }
  if (i == 7) gfx_cor((GfxRect){ (x1 + x2) * 0.5f - 1.0f, cy - 11.0f, 2.0f, 22.0f }, 0, 1, 1, 1, 0.38f * a);
  if (i == 3 || i == 5) {   // niveis discretos: um ponto por parada
    int n = i == 3 ? 4 : 8, k;
    for (k = 0; k < n; k++) {
      float tx = x1 + (x2 - x1) * (float)k / (float)(n - 1);
      if (fabsf(tx - px) > 18.0f) gfx_cor((GfxRect){ tx - 3.0f, cy - 3.0f, 6.0f, 6.0f }, 0.5f, 1, 1, 1, 0.45f * a);
    }
  }
  gfx_cor((GfxRect){ px - 17.0f, cy - 17.0f, 34.0f, 34.0f }, 0.5f, 0.055f, 0.059f, 0.071f, a);
  gfx_cor((GfxRect){ px - 12.0f, cy - 12.0f, 24.0f, 24.0f }, 0.5f, 0.953f, 0.949f, 0.937f, a);
  if (ativo) {
    ponteiro_alvo(x1 - 24.0f, r.y, x2 - x1 + 48.0f, r.h, NULL, ponteiroSlider, 0, 0);
    ponteiro_alvo_arrastavel();
  }
}

static int paletaColunas(float largura) {
  float min = estiloToqueMin();
  return min > 0 && largura + .01f < VIDEO_LEG_NCORES * min ? 3 : VIDEO_LEG_NCORES;
}
static void paletaCtl(GfxRect r, float a, int ativo) {
  float d = 56.0f, vao = 34.0f, tot = d * VIDEO_LEG_NCORES + vao * (VIDEO_LEG_NCORES - 1);
  float x = r.x + (r.w - tot) * 0.5f, cy = r.y + r.h * 0.5f, ar, ag, ab;
  int i, telefone = telefoneui_ativo(), cols = paletaColunas(r.w);
  float cw = 0, ch = 0;
  if (telefone) {
    int rows = (VIDEO_LEG_NCORES + cols - 1) / cols;
    cw = estiloToqueMin(); if (cw < d + vao) cw = d + vao;
    ch = (r.h - 6.0f * (rows - 1)) / rows;
    x = r.x + (r.w - cw * cols) * .5f;
  }
  ajustes_acento_marca(&ar, &ag, &ab);
  fundoCtl(r, a);
  for (i = 0; i < VIDEO_LEG_NCORES; i++) {
    int cr, cg, cb;
    float s = ctlAmostra[i], dd = d * (1.0f + 0.16f * s);
    float ai = a * anim_clamp(ctlAnim * 1.7f - (float)i * 0.12f, 0.0f, 1.0f);
    float cx = telefone ? x + cw * (i % cols + .5f) : x + d * .5f;
    float yc = telefone ? r.y + (ch + 6.0f) * (i / cols) + ch * .5f : cy;
    GfxRect c = { cx - dd * .5f, yc - dd * .5f, dd, dd };
    if (ctlVis == EST_COR_BORDA) { unsigned rgb = video_legenda_borda_rgb(i); cr = (rgb >> 16) & 255; cg = (rgb >> 8) & 255; cb = rgb & 255; }
    else corLegenda(i, &cr, &cg, &cb);
    gfx_cor(c, 0.5f, cr / 255.0f, cg / 255.0f, cb / 255.0f, ai);
    gfx_anel(c, 0.5f, 1.0f, 1, 1, 1, 0.22f * ai);                 // a amostra escura nao some no fundo
    if (i == (ctlVis == EST_COR_BORDA ? player_leg_estilo()->corBorda : player_leg_estilo()->cor)) gfx_anel_fora(c, 0.5f, 5.0f, 3.0f, ar, ag, ab, ai);   // a em uso
    else if (i == ctlSub) gfx_anel_fora(c, 0.5f, 5.0f, 2.0f, 1, 1, 1, 0.7f * ai);            // so o foco
    if (ativo) {
      if (telefone) ponteiro_alvo(x + cw * (i % cols), r.y + (ch + 6.0f) * (i / cols), cw, ch, ponteiroSub, ponteiroEscolher, i, 0);
      else ponteiro_alvo(x - 12.0f, r.y, d + 24.0f, r.h, ponteiroSub, ponteiroEscolher, i, 0);
    }
    if (!telefone) x += d + vao;
  }
}

// A LISTA de fontes: cada nome escrito NA PROPRIA fonte, 2 colunas, no lugar da
// grade (mesma altura). A em uso leva o check no acento; o foco, a superficie.
static void fontesCtl(GfxRect r, float a, int ativo) {
  int total = TXT_FAMILIA_N + legfontes_n(), rows = (total + 1) / 2;
  float vao = 8.0f, cw = (r.w - vao) * 0.5f, ch = (r.h - vao * (rows - 1)) / rows, ar, ag, ab;
  int i;
  ajustes_acento_marca(&ar, &ag, &ab);
  int fonteLocal = legfontes_selecionada(0);
  for (i = 0; i < total; i++) {
    GfxRect c = { r.x + (i % 2) * (cw + vao), r.y + (i / 2) * (ch + vao), cw, ch };
    float ai = a * anim_clamp(ctlAnim * 1.7f - (float)i * 0.06f, 0.0f, 1.0f);
    int importada = i >= TXT_FAMILIA_N;
    int em = importada ? fonteLocal == i - TXT_FAMILIA_N
                       : fonteLocal < 0 && i == player_leg_estilo()->familia;
    int sel = i == ctlSub;
    float tw = cw - 40.0f - 34.0f;
    const char *nome = importada ? legfontes_nome(i - TXT_FAMILIA_N) : TXT_FAMILIAS_PT[i];
    TxtLinha n = importada
               ? txt_linha_corta(TXT_G30B, nome, 243, 242, 239, sel ? 255 : 220, tw)
               : txt_linha_corta_familia(TXT_G30B, nome, 243, 242, 239, sel ? 255 : 220, tw, (TxtFamilia)i);
    if (sel) plrui_linha_foco(c, 22.0f, ai); else fundoCtl(c, ai);
    txt_desenhar_alpha(n, c.x + 22.0f, c.y + (c.h - (float)n.h) * 0.5f, ai);
    if (em) gfx_icone((GfxRect){ c.x + c.w - 22.0f - 26.0f, c.y + (c.h - 26.0f) * 0.5f, 26.0f, 26.0f }, "pl_check", ar, ag, ab, ai);
    if (ativo) ponteiro_alvo(c.x, c.y, c.w, c.h, ponteiroSub, ponteiroEscolher, i, 0);
  }
}

static void controleEstilo(int i, GfxRect g, float a, int ativo) {
  if (estTipo(i) == 3) { fontesCtl(g, a, ativo); return; }
  { GfxRect ra = { g.x, g.y, g.w, IL_EST_CEL_H }, rb = { g.x, g.y + IL_EST_CEL_H + 6.0f, g.w, IL_EST_CEL_H };
    if (telefoneui_ativo() && estTipo(i) == 2 && paletaColunas(g.w) < VIDEO_LEG_NCORES)
      rb.h = g.h - (rb.y - g.y);
    celulaEstilo(i, ra, a, 0);                          // o item, com o valor ao vivo
    if (ativo) ponteiro_alvo(ra.x, ra.y, ra.w, ra.h, NULL, ponteiroFecharCtl, 0, 0);
    if (estTipo(i) == 1) sliderCtl(i, rb, a, ativo); else paletaCtl(rb, a, ativo); }
}

static float alturaEstilo(void) { return IL_PAD_Y + IL_EST_TOPO + 14.0f + IL_EST_GRADE_H + IL_PAD_Y; }

static void corpoEstilo(GfxRect c, float a) {
  float pad = estiloPadX(), x0 = c.x + pad, w = c.w - pad * 2.0f, y = c.y + IL_PAD_Y;
  float cw = (w - 4.0f * 6.0f) / FX_BARRA_COLS;
  int i, assentada = a > 0.99f && ponteiro_ativo();
  { TxtLinha t = txt_linha(TXT_G30B, "Legendas", 243, 242, 239, 255);
    const char *rot[2] = { "Faixas", "Estilo" };
    int cont[2] = { nLinhas(1), -1 };
    float yc = y + IL_EST_TOPO * 0.5f;
    txt_desenhar_alpha(t, x0 + 10.0f, yc - (float)t.h * 0.5f, a);
    plrui_seg(rot, cont, 2, 1, 0, x0 + 10.0f + (float)t.w + 18.0f, yc - 27.0f, a);
    // A aba "Faixas": a largura do primeiro segmento e a do segmentado de um so.
    if (assentada)
      ponteiro_alvo(x0 + 10.0f + (float)t.w + 18.0f, yc - 27.0f, plrui_seg(rot, cont, 1, 1, 0, -1.0f, 0, a),
                    54.0f, NULL, ponteiroAba, 0, 0);
    { const char *k[3] = { "\xe2\x86\x90 \xe2\x86\x92", "OK", "Voltar" }, *rt[3];
      int n = 3, t0 = ctl >= 0 ? estTipo(ctl) : 0;
      if (ctl >= 0) {
        rt[0] = t0 == 1 ? "Ajustar" : "Escolher"; rt[1] = "Pronto"; rt[2] = "Voltar";
        if (t0 == 3) k[0] = "\xe2\x86\x91 \xe2\x86\x93 \xe2\x86\x90 \xe2\x86\x92";
      } else {
        rt[0] = "Op\xc3\xa7\xc3\xa3o"; rt[1] = estTipo(foco[FX_COL_ESTILO]) ? "Abrir" : "Mudar"; rt[2] = "Fechar";
      }
      plrui_dicas(k, rt, n, x0 + w - 10.0f, yc, 1, a); } }
  y += IL_EST_TOPO + 14.0f;
  // A grade e o controle se trocam NO MESMO retangulo (a altura nao muda): a
  // grade some e sobe um pouco, o controle entra de baixo, pela mola da tela.
  if (ctlAnim < 0.995f) {
    float ag = a * (1.0f - anim_clamp(ctlAnim * 1.6f, 0.0f, 1.0f)), dy = -10.0f * ctlAnim;
    for (i = 0; i < FX_N_ESTILO; i++) {
      GfxRect r = { x0 + (i % FX_BARRA_COLS) * (cw + 6.0f), y + dy + (i / FX_BARRA_COLS) * (IL_EST_CEL_H + 6.0f),
                    cw, IL_EST_CEL_H };
      celulaEstilo(i, r, ag, assentada && ctlAnim < 0.01f);
    }
  }
  if (ctlAnim > 0.005f && ctlVis >= 0) {
    float ac = a * anim_clamp((ctlAnim - 0.1f) * 1.4f, 0.0f, 1.0f);
    controleEstilo(ctlVis, (GfxRect){ x0, y + 12.0f * (1.0f - ctlAnim), w, IL_EST_GRADE_H }, ac,
                   assentada && ctl >= 0 && ctlAnim > 0.99f);
  }
}

static void corpoIlha(GfxRect c, float a, void *u) {
  (void)u;
  if (faixas_estilo_topo() || (!aberta && animTopo > 0.5f)) corpoEstilo(c, a);
  else if (modo) legendasui_corpo(c, a);
  else corpoLista(c, a);
}

static void faixas_desenharCorpo_(Uint32 agora);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void faixas_desenhar(Uint32 agora) {
  ESCALA_INI();
  faixas_desenharCorpo_(agora);
  ESCALA_FIM();
}
static void faixas_desenharCorpo_(Uint32 agora) {
  (void)agora;
  pilPedir();   // a pilula da legenda automatica (so com a folha fechada)
  if (anim < .01f) return;
  { int dir = plrilha_direita(), est = faixas_estilo_topo();
    // O video fica sem veu cheio: so o degrade do lado da ilha (a lista) ou o
    // de cima (o estilo, com a previa no video), pelo shader com dither.
    if (est) gfx_veu_css((GfxRect){ 0, 0, NV_TELA_W, 260.0f }, 1, 1.38f, 1.0f, 0.52f * anim);
    else gfx_veu_css((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, dir ? 3 : 2, 1.38f, 1.0f, 0.42f * anim);
    if (!aberta) return;   // fechando: a ilha encolhe com o ultimo corpo
    { PlrIlhaPedido p;
      memset(&p, 0, sizeof p);
      p.w = est ? NV_TELA_W - 192.0f : IL_W;
      p.h = est ? alturaEstilo() : modo ? legendasui_altura() : alturaLista(0);
      p.corpo = corpoIlha;
      plrilha_pedir(&p); } }
}

// --- F04: the selector (legendasui.c) acts on the primary through these -------
int faixas_legenda_ativa(void) { return legendaAtiva(); }
const char *faixas_legenda_externa_id(void) { return legExternaId; }
void faixas_escolher_embutida(int i) {
  legAuto = 0;
  i = i < 0 ? -1 : i < video_n_legenda() ? i : -1;
  escolherLegenda(i);
  lembrarManual(i);   // so o seletor chama isto: e sempre a mao
}
// From the selector's COPY of the addon entry (addons_legendas_copiar): the
// live list may have been replaced since the snapshot, so the index is
// resolved again by identity and the URL comes from the copy.
void faixas_escolher_externa(const Legenda *l) {
  Legenda *v;
  char id[24], idv[24];
  int n, j;
  if (!l || !l->url[0]) return;
  legAuto = 0;
  mkvass_parar();
  legOverlay = -1; legOverlayEsperando = -1; legOverlayRetomar = 0; legOverlayTV = 0;
  video_escolher_legenda(-1);
  legsync_primaria_externa(l->url, legenda_idioma(l->idioma, l->arquivo), l->provedor);   // F05: carrega e vira documento do AutoSync
  // emTroca = o AutoSync trocando sozinho: nao e escolha da pessoa.
  if (!emTroca) { pilExterna(l); lembrarAddon(l); }
  legendasui_id_addon(l, id);
  legExterna = -1;
  v = malloc(sizeof *v * LEG_MAX);
  n = v ? addons_legendas_copiar(v, LEG_MAX, NULL, NULL) : 0;
  for (j = 0; j < n; j++) {
    legendasui_id_addon(&v[j], idv);
    if (!strcmp(id, idv)) { legExterna = video_n_legenda() + j; break; }
  }
  free(v);
  snprintf(legExternaId, sizeof legExternaId, "%s", id);
}
const char *faixas_legenda_marca(int i) {
  const char *marca = NULL;
  if (i < 0 || i >= video_n_legenda()) return NULL;
  rotuloLegenda(i, &marca);
  return marca;
}
