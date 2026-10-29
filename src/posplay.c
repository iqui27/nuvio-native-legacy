#include "posplay.h"
#include "intro.h"
#include "proximo.h"
#include "fontecache.h"
#include "proximopreparo.h"
#include "jfid.h"
#include "idioma.h"
#include "catalogo.h"
#include "extras.h"
#include "posterprov.h"
#include "gfx.h"
#include "text.h"
#include "sinopse.h"
#include "tex_cache.h"
#include "layout.h"
#include "anim.h"
#include "ajustes.h"
#include "vistoep.h"
#include "detail.h"
#include "descoberta.h"
#include "video.h"
#include "vistoep.h"
#include "player.h"
#include "plrui.h"
#include "plrilha.h"
#include "velocidade.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

// Contagem configuravel, 5 s de fabrica, desligada por padrao (#256).
// O web abre o painel de filme aos 90%; aqui, desde a 2.0.3, sem marcador e um
// tempo FIXO antes do fim (intro_fim_estimado_filme, intro.h).
// #470: a contagem comeca quando o cartao sobe, ainda durante os creditos.
// Guardas do #115 ("More like this aparece cedo demais — no comeco do
// filme"). Ver posplay_regra_filme.
#define PP_FILME_METADE   0.5     // nunca antes da metade da duracao
#define PP_DUR_ESTAVEL_S  8.0     // duracao parada ha tanto tempo, no minimo
// "Fim real" do arquivo para a cena pos-creditos: o pipeline para a uma fracao
// de segundo da duracao (video_pos nao chega exatamente nela), entao um >=
// exato nunca voltaria a abrir o cartao.
#define PP_FIM_TOL_S      1.5

// Cartazes dos relacionados, no tamanho da grade de "Ver tudo".
#define PP_CARD_W  212.0f
#define PP_CARD_H  318.0f
#define PP_GAP      24.0f
#define PP_MAX       8    // quantos cartazes cabem na ilha de 1728 (182 + 30)
#define PP_PAD      32.0f
#define PP_ROTULO_H 44.0f
// Cartao do proximo episodio, no molde do de episodios.c (thumb 184x130),
// ampliado para a distancia de quem esta deitado no sofa.
#define PP_EP_W     980.0f
#define PP_EP_H     260.0f
#define PP_EP_PAD    18.0f
#define PP_EP_THUMB_W 320.0f
#define PP_EP_RAIO    24.0f
// PASSO entre linhas, nao vao: txt_bloco poe a linha i em y + i*leading.
#define PP_LD_SIN     28.0f

static int    visivel, serie, idx = -1, foco;
#ifdef NV_TOUCH_UI
static ToqueRolagem toquePP;
static float scrollPP;
static int toquePPRolar(const PonteiroRolagem *e) {
  return visivel && !serie && telefoneui_ativo() ? toquerol_evento(&toquePP, e) : 0;
}
#endif
// O titulo de `idx` (#190). O catalogo e refeito com o player aberto e a mesma
// posicao passa a ser de outro titulo: sem o id, o cartao A seguir mostrava o
// episodio de outra serie. Ver fixarTitulo.
static char   idTitulo[64];
// DURACAO ESTAVEL. O player passa o que tiver: a duracao do metadado, a
// reserva de 114 min e, assim que o pipeline responde, a dele — e no primeiro
// instante o pipeline pode informar uma duracao pequena e provisoria (o
// arquivo ainda chegando — hipotese do conserto de 10/09, nao medida aqui).
// Com uma duracao de 30 s, a metade e o fim do filme chegam aos 15 s, e a
// regra de 2 min da serie vale na hora. Decidir so depois de a duracao ficar
// PARADA por PP_DUR_ESTAVEL_S tira esse instante do jogo sem atrasar o fim de
// verdade.
static double durVista;
static double durEstavel;

// DISPENSADO GRUDA. Sem isto o Voltar fechava o painel e o quadro seguinte o
// reabria na hora, porque a condicao de aparecer (passar de 90% do filme)
// continua verdadeira ate o fim — foi o "nao da pra sair, quebra tudo" que o
// dono viu. So volta a valer quando a reproducao SAI da zona, ou quando o
// player abre outro titulo.
static int    dispensado;
static float  anim;
static Uint32 fecharEm;              // 0 = sem contagem
static int    pedT, pedE, pedTitulo = -1;
static int    pedAutomatico, automaticoEntregue, recuou, aguardaBusca;
static int    proxT, proxE;          // proximo episodio, quando ha
static char   proxNome[120];
// Temporada0 e especial; quem distingue ausencia e o numero do episodio.
static int temProximo(void) { return serie && proxT >= 0 && proxE > 0; }
static int temPedidoEpisodio(void) { return pedT >= 0 && pedE > 0; }
static char   preEpisodio[64], preAlvo[64];
static int    preTentou;
static int    preVideoTentou, preVideoRenovou;
static FontecacheEscopo preEscopo;

static void cancelarPreparo(int apagar) {
  fontecache_cancelar_proximo();
  if (apagar && preAlvo[0])
    fontecache_vod_apagar(preAlvo, "series", "", &preEscopo);
}
// CENA POS-CREDITOS (TMDB 179430/179431 em CatItem.posCreditos). `aguardaCena`:
// ha cena e o arquivo ainda nao acabou — o cartao fica no ar mas NAO age (nem
// contagem, nem troca de titulo). `cenaPendente`: a pessoa escolheu continuar;
// o cartao volta inteiro no fim real. So a acao da UI espera: visto/progresso
// seguem no limiar de creditos (player.c, concluiuAgora).
static int    cenaPendente, cenaNoFim, aguardaCena;
int posplay_visivel(void) { return visivel; }
// Preserve a pending card/Next; a dismissed card leaves no choice at EOF.
int posplay_aguarda_escolha(void) {
  return !dispensado || visivel || temPedidoEpisodio() || automaticoEntregue;
}
// Cartao do proximo episodio: SOBRE o video em tela cheia (o video nao recua).
// Os relacionados do filme, bem mais altos, continuam recuando o video.
int posplay_sobre_video(void) { return visivel && (serie || aguardaCena); }

// SAIR DA TELA PRESERVANDO A ESCOLHA, e a diferenca com posplay_fechar e o
// issue #14 inteiro.
//
// posplay_fechar zera pedT/pedE/pedTitulo, e isso e certo no caso dele: o
// player abriu outro titulo e o pedido do anterior nao vale mais. Mas os
// tratadores de tecla ARMAVAM o pedido e chamavam posplay_fechar na linha
// seguinte, que apagava o pedido recem-armado. posplay_pediu_episodio nunca
// devolvia nada, e o OK do cartao de proximo episodio NUNCA funcionou, em
// nenhuma plataforma. A correcao de id do v1.0.10 (app.c, o `strcspn` no
// `ci->imdb` composto) fica a jusante deste portao: codigo certo que nunca
// rodava, o que explica o relato de que "o fix do 1.0.10 nao resolveu".
//
// `dispensado` vem junto e nao e detalhe. Sem ele a condicao de aparecer
// (janelaSerie) continua verdadeira e o painel volta no quadro seguinte —
// no video do relato o cartao nem sequer some depois do OK, que era este
// segundo defeito somado ao primeiro.
static void esconder(void) {
  automaticoEntregue = 0;
  cancelarPreparo(!temPedidoEpisodio());
  // O pedido Next consome no roteador antes de encerrar o player. Back/Down
  // descartam; uma escolha explicita de Next conserva o mesmo worker/token.
  if (!temPedidoEpisodio()) proximopreparo_cancelar();
  visivel = 0; fecharEm = 0; foco = 0; dispensado = 1;
}

void posplay_fechar(void) {
  // Uma lista JA completa continua disponivel ao buscarParaPlayer do proximo.
  // O fio em voo e cancelado sem esperar HTTP na UI; nunca publica depois daqui.
  cancelarPreparo(0);
  proximopreparo_cancelar();
  preTentou = 0; preEpisodio[0] = preAlvo[0] = 0;
  preVideoTentou = preVideoRenovou = 0;
  visivel = 0; fecharEm = 0; foco = 0;
  cenaPendente = cenaNoFim = aguardaCena = 0;
  durVista = 0.0; durEstavel = 0.0;   // titulo novo: a duracao comeca de novo
  pedT = pedE = 0; pedTitulo = -1;
  pedAutomatico = automaticoEntregue = recuou = aguardaBusca = 0;
  dispensado = 0;   // titulo novo: a dispensa do anterior nao vale mais
#ifdef NV_TOUCH_UI
  scrollPP = 0; memset(&toquePP, 0, sizeof toquePP);
#endif
}

static void cancelarAutomatico(void) {
  fecharEm = 0;
  if (pedAutomatico) {
    pedT = pedE = 0;
    pedAutomatico = 0;
    if (temProximo()) {
      visivel = 1;
      dispensado = 0;
    }
  }
}

// Busca explicita revoga tambem um Next que o roteador ja estava aguardando.
// O pulo automatico de creditos usa so aguardar_busca e conserva o buffer.
void posplay_busca_usuario(void) {
  cancelarAutomatico();
  if (automaticoEntregue && temProximo()) {
    visivel = 1;
    dispensado = 0;
  }
  automaticoEntregue = 0;
  cancelarPreparo(0);
  proximopreparo_cancelar();
  preTentou = preVideoTentou = preVideoRenovou = 0;
  aguardaBusca = 1;
}

void posplay_troca_manual(void) {
  cancelarPreparo(0);
  pedT = pedE = 0;
  pedAutomatico = automaticoEntregue = 0;
  visivel = 0; fecharEm = 0; foco = 0; dispensado = 1;
  // Nao chamar proximopreparo_cancelar: consumir/adotar vem antes de fechar
  // o player. Um pedido de Anterior cancela o alvo diferente no roteador.
}

// Recuar e pedir para ficar neste episodio. O cartao continua servindo ao OK,
// mas a contagem nao volta enquanto a pessoa ainda esta na mesma janela.
void posplay_recuar(void) {
  cancelarAutomatico();
  proximopreparo_cancelar();
  recuou = 1;
}

// A posicao da barra pode mostrar o destino antes de o seek chegar ao motor.
// Enquanto o player espera progresso real, Next continua sendo uma escolha.
void posplay_aguardar_busca(int aguardar) {
  aguardaBusca = aguardar != 0;
  if (aguardaBusca) cancelarAutomatico();
}

int posplay_pediu_episodio(int *t, int *e) {
  if (!temPedidoEpisodio()) return 0;
  if (t) *t = pedT;
  if (e) *e = pedE;
  pedT = pedE = 0;
  automaticoEntregue = pedAutomatico;
  pedAutomatico = 0;
  return 1;
}
int posplay_pediu_titulo(void) { int v = pedTitulo; pedTitulo = -1; return v; }

// Abertura A PEDIDO, pelo botao do player. Existe porque dispensar passou a
// grudar: sem uma porta de volta, quem apertasse Voltar uma vez nao veria mais
// os relacionados naquele filme. Limpa a dispensa de proposito — o pedido
// explicito vale mais que a recusa anterior.
// O indice que o player manda e o CORRENTE (idxAtual); guarda-se junto o id,
// para o desenho conferir (cat_indice_vivo) se uma troca de bloco caiu entre
// a atualizacao e ele.
static void fixarTitulo(int idxCatalogo) {
  const CatItem *ci = cat_item(idxCatalogo);
  idx = idxCatalogo;
  snprintf(idTitulo, sizeof idTitulo, "%s", ci ? ci->imdb : "");
}

int posplay_indice(void) { return cat_indice_vivo(idx, idTitulo); }

int posplay_abrir_relacionados(int idxCatalogo) {
  if (extras_n_relacionados() <= 0) return 0;
  fixarTitulo(idxCatalogo);
  serie = 0;
  foco = 0;
  fecharEm = 0;
  dispensado = 0;
  visivel = 1;
  return 1;
}

// O episodio SEGUINTE ao que esta tocando, na lista unica (ja ordenada por
// temporada e episodio). Devolve 0 quando o que toca e o ultimo.
// 2.0.3: a regra e prox_indice_seguinte, a MESMA de player_proximo_episodio
// (que abre a janela). Era "o indice seguinte ao do episodio tocando": com o
// episodio repetido na lista oferecia ele mesmo, e com o episodio fora da lista
// nao oferecia nada enquanto a janela ja estava aberta (e a legenda, subida).
static int acharProximo(int idxItem, int t, int e) {
  int n = cat_n_episodios(idxItem), i;
  const CatEp *base = n > 0 ? cat_episodio(idxItem, 0) : NULL, *px;
  if (!base) return 0;
  i = prox_indice_seguinte(base, n, t, e);
  px = i >= 0 ? cat_episodio(idxItem, i) : NULL;
  if (!px) return 0;
  proxT = px->temporada; proxE = px->episodio;
  snprintf(proxNome, sizeof proxNome, "%s", px->nome);
  return 1;
}

// So o gatilho e novo: identidade pelo catalogo, janela pelo player, consulta
// e fio pelo fontecache. Tudo que le catalogo/conta/perfil fica no fio da UI.
static void prepararProximo(int ehSerie, double resta) {
  FontecacheEscopo escopo;
  const CatEp *px;
  const CatItem *ci = cat_item(idx);
  long long quando;
  int t = 0, e = 0, n = cat_n_episodios(idx), proximo;
  proximopreparo_passo(preEpisodio);
  if (!ehSerie || !ci || jfid_e(ci->imdb)) {
    proximopreparo_cancelar(); return;
  }
  addons_capturar_escopo(&escopo);
  int video = ajustes_fonte_precarregar() && !ajustes_fonte_manual();
  if ((!ajustes_fonte_preparar() && !video) || ((preTentou || preVideoTentou) &&
      (escopo.geracao != preEscopo.geracao || escopo.addons != preEscopo.addons ||
       escopo.perfil != preEscopo.perfil || escopo.sessao != preEscopo.sessao ||
       strcmp(escopo.conta, preEscopo.conta)))) {
    cancelarPreparo(1);
    proximopreparo_cancelar();
    return;
  }
  // Uma renovacao, 10 s antes da contagem final: o preparo feito antes dos
  // creditos pode ter expirado. Nao consulta periodicamente durante o episodio.
  // O video ganha um minuto antes da janela para consultar os scrapers e
  // carregar amostras; o preparo antigo de metadados conserva os seus 10 s.
  if ((!video && (preTentou == 2 || (preTentou && resta > ajustes_proximo_contagem() + 10.0))) ||
      dispensado || recuou || aguardaBusca || durEstavel < PP_DUR_ESTAVEL_S ||
      !player_janela_proximo(video ? 60.0 : 10.0)) return;
  player_episodio_atual(&t, &e);
  if (t < 0 || e <= 0) return;
  proximo = prox_indice_seguinte(n ? cat_episodio(idx, 0) : NULL, n, t, e);
  px = proximo >= 0 ? cat_episodio(idx, proximo) : NULL;
  if (!px) return;
  quando = prox_episodio_data_ms(px);
  // Desconhecido nao prova que ja estreou: nao gasta rede especulativa.
  if (quando == PROX_SEM_DATA || quando > (long long)time(NULL) * 1000) return;
  if (!cat_id_stream(idx, px->temporada, px->episodio, preAlvo, sizeof preAlvo) ||
      jfid_e(preAlvo)) return;
  preEscopo = escopo;
  if (video) {
    // O novo toggle nao depende de preparar as fontes na pagina de titulo.
    // Usa lista/resolvedor proprios, sem substituir streams do episodio atual.
    if (!preVideoTentou) {
      preVideoTentou = 1;
      proximopreparo_agendar(idx, px->temporada, px->episodio,
                             preEpisodio, preAlvo, &preEscopo);
    } else if (!preVideoRenovou && resta <= ajustes_proximo_contagem() + 10.0) {
      if (proximopreparo_renovar(preEpisodio, preAlvo)) preVideoRenovou = 1;
    }
    return;
  }
  if (preTentou) {
    Stream *l = NULL;
    Uint32 idade = 0;
    int n;
    int acerto = fontecache_vod_pegar(preAlvo, "series", "", &preEscopo, &l, &n, &idade);
    free(l);
    if (acerto == FC_ACERTO && idade + fmax(resta, 0.0) * 1000 < FONTECACHE_VOD_VALIDADE_MS) {
      preTentou = 2;
      return;
    }
  }
  if (fontecache_precarregar_proximo(preAlvo, &preEscopo)) preTentou++;
}

// Marcador de creditos que o FILME aceita. Ver posplay_regra_filme.
// 2.0.3: a parte final do FILME e a de intro_creditos_janela_filme (12%,
// 5-15 min), a mesma do botao de pular creditos; era o ultimo quarto.
static int credAceito(double durSeg, double cred) {
  return cred > 1.0 && cred < durSeg && durSeg - cred <= intro_creditos_janela_filme(durSeg);
}

// A REGRA DO FILME, sem estado (o teste chama direto).
//
// #115, Owlphibia29: "More like this aparece cedo demais — no comeco do
// filme". A estimativa sem marcador ja nao subia no segundo zero (teto de
// metade da janela, 10/09), mas o MARCADOR passava sem conferencia nenhuma:
// `posSeg >= creditos` bastava. E os dois marcadores podem apontar para o
// comeco do filme:
//   - capitulo do Matroska: mkv_creditos_nomeados casava o PRIMEIRO nome com
//     "credit", e "Opening Credits" aos 90 s e capitulo comum em remux;
//   - TheIntroDB: dado de terceiro, e intro_creditos_seg devolvia o PRIMEIRO
//     trecho de creditos (um filme pode ter o de abertura e o final).
// Com qualquer um dos dois, o painel subia aos 90 s de filme. O cartao de
// proximo episodio tinha sanidade para isso desde o #34 (credJanela, no
// player.c); o de filme nunca teve.
//
// Agora, em ordem:
//   1. NUNCA ANTES DA METADE da duracao — vale para marcador e estimativa.
//   2. Marcador so vale no ULTIMO QUARTO (mesma regra que video_creditos ja
//      usa para o capitulo sem nome). Fora disso e recusado e cai na
//      estimativa, em vez de calar o painel para sempre.
//   3. Marcador aceito manda, e so ele (antes dele, a estimativa nao sobe).
//   4. Sem marcador, a estimativa proporcional de sempre.
int posplay_regra_filme(double posSeg, double durSeg, double creditosSeg) {
  double janela, resta;
  if (durSeg <= 1.0) return 0;
  if (posSeg < durSeg * PP_FILME_METADE) return 0;
  if (credAceito(durSeg, creditosSeg)) return posSeg >= creditosSeg;
  // FILME SEM MARCADOR (MP4 nao tem capitulo; o TheIntroDB nao tem todo
  // filme): tempo FIXO antes do fim, intro_fim_estimado_filme — 3 min, 90 s
  // em filme de menos de 1 h, nada abaixo de 10 min. Decisao do dono em
  // 07/10/2026: nada de porcentagem (era 4,5% da duracao, entre 150 e 330 s).
  janela = intro_fim_estimado_filme(durSeg);
  if (janela <= 0.0) return 0;
  resta = durSeg - posSeg;
  return resta > 0.0 && resta <= janela;
}

// A CONTAGEM DO PROXIMO EPISODIO, sem estado (o teste chama direto). Devolve o
// novo `fecharEm` (0 = sem contagem). Regra (#470):
//   - autoplay desligado: nunca conta, o cartao espera a pessoa;
//   - ja contando: mantem o instante (cancelar e esconder o cartao, que zera);
//   - cartao apareceu: arma contagemSeg agora, limitada ao que resta.
// `restaReal` e o que falta do episodio em segundos de relogio.
Uint32 posplay_contagem(Uint32 agora, Uint32 fecharEm, double restaReal, int autoplay, int contagemSeg) {
  Uint32 alvo = 0;
  int armou = 0;
  if (!autoplay) return 0;
  if (fecharEm) return fecharEm;
  if (contagemSeg < 1) contagemSeg = 1;
  if (contagemSeg > 60) contagemSeg = 60;
  if (restaReal <= (double)contagemSeg) {
    if (restaReal < 0.0) restaReal = 0.0;
    alvo = agora + (Uint32)(restaReal * 1000.0); armou = 1;
  } else {
    alvo = agora + (Uint32)contagemSeg * 1000u; armou = 1;
  }
  if (armou && !alvo) alvo = 1;   // 0 quer dizer "sem contagem"
  return alvo;
}

void posplay_atualizar(float dt, Uint32 agora, double posSeg, double durSeg,
                       int ehSerie, int idxCatalogo, int janelaSerie) {
  int deveAparecer = 0;
  // DUAS FONTES DE MARCADOR, nesta ordem e por este motivo: o capitulo do
  // Matroska vem do PROPRIO arquivo que esta tocando, entao ele descreve esta
  // copia; o TheIntroDB descreve o LANCAMENTO, e uma copia com abertura
  // diferente ou com anuncio na frente sai deslocada. Quando o arquivo diz,
  // ele ganha.
  //
  // O segundo cobre o caso que antes so tinha estimativa: filme em MP4, que nao
  // tem capitulo nenhum para ler.
  double creditosSeg = video_creditos();
  if (creditosSeg <= 1.0) creditosSeg = intro_creditos_seg();
  anim = anim_mola(anim, visivel ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  // A CADA QUADRO, e nao so na abertura do painel (#190): com o cartao no ar o
  // `idx` guardado envelhecia junto com o catalogo.
  { char episodio[64] = "";
    int t = 0, e = 0;
    player_episodio_atual(&t, &e);
    if (ehSerie && t >= 0 && e > 0)
      cat_id_stream(idxCatalogo, t, e, episodio, sizeof episodio);
    if (preEpisodio[0] && strcmp(preEpisodio, episodio)) {
      cancelarPreparo(1);
      posplay_fechar();
    }
    snprintf(preEpisodio, sizeof preEpisodio, "%s", episodio);
  }
  fixarTitulo(idxCatalogo);
  if (durSeg - durVista > 2.0 || durVista - durSeg > 2.0) {
    durVista = durSeg;
    durEstavel = 0.0;
  } else {
    durEstavel += dt;
  }
  prepararProximo(ehSerie, vel_tempo_real(durSeg - posSeg, player_velocidade_efetiva()));
  if (durSeg <= 1.0) return;

  if (ehSerie && !janelaSerie && !aguardaBusca && recuou) {
    recuou = 0;
    preVideoTentou = preVideoRenovou = 0;
  }

  if (ehSerie) {
    // A JANELA quem decide e o player: ele e o unico que sabe se os creditos
    // comecaram (intro_ativo/INTRO_CREDITOS) alem dos dois minutos finais. Era
    // a mesma condicao que a caixa antiga de "Próximo episódio" usava, e passar
    // a usa-la aqui foi o que permitiu apagar aquela caixa — havia DUAS
    // interfaces de proximo episodio na mesma tela, uma feia e uma nova.
    //
    // A contagem comeca com o cartao se autoplay estiver ligado.
    deveAparecer = janelaSerie && durEstavel >= PP_DUR_ESTAVEL_S;
  } else {
    // FILME: a regra inteira mora em posplay_regra_filme, logo acima, que o
    // teste chama sem player nem rede. Aqui entra so a DURACAO ESTAVEL — ver
    // durEstavel. Um cat_item so: entre duas chamadas a troca do catalogo pode
    // publicar n = 0 e a segunda dar NULL.
    const CatItem *ciPos = cat_item(idx);
    deveAparecer = durEstavel >= PP_DUR_ESTAVEL_S &&
                   (posplay_regra_filme(posSeg, durSeg, creditosSeg) ||
                    (ciPos && ciPos->posCreditos && durSeg - posSeg <= PP_FIM_TOL_S));
  }

  // Saiu da zona (o dono voltou o filme): a dispensa perde a validade e o
  // painel pode subir de novo quando ele chegar ao fim outra vez.
  if (!deveAparecer) dispensado = 0;
  { const CatItem *ci = cat_item(idx);
    cenaNoFim = ci && ci->posCreditos && durSeg - posSeg <= PP_FIM_TOL_S;
    aguardaCena = ci && ci->posCreditos && !cenaNoFim;
    if (cenaNoFim && cenaPendente) { cenaPendente = 0; dispensado = 0; }
  }

  if (deveAparecer && !visivel && !dispensado) {
    if (!ehSerie)
      printf("[posplay] relacionados em %.0fs de %.0fs (%s)\n", posSeg, durSeg,
             credAceito(durSeg, creditosSeg) ? "marcador de creditos"
             : creditosSeg > 1.0 ? "estimativa, marcador recusado"
             : "estimativa, sem marcador");
    serie = ehSerie;
    foco = 0;
    proxT = proxE = 0; proxNome[0] = 0;
    if (serie) {
      int t = 0, e = 0;
      const CatItem *ci = cat_item(idx);
      // QUAL EPISODIO ESTA TOCANDO: pergunta ao PLAYER, e nao ao item do
      // catalogo.
      //
      // MEDIDO NA TV, e so por isso encontrado: tocando Silo T2E8, o painel
      // ofereceu T2E7 e o log confirmou ("automatico (verificado): Silo S02
      // E07"). O item ainda apontava para T2E6 — foi por ele que a serie
      // entrou, pela fileira "Continuar assistindo" — e nada o move quando a
      // reproducao comeca pelo botao da pagina de titulo. "O proximo de E6" e
      // E7, e era literalmente isso que o codigo pedia.
      //
      // epT/epE do player sao a unica fonte que acompanha a reproducao de
      // verdade: player_definir_episodio os escreve em todo caminho que abre
      // um episodio. O item continua valendo de reserva para o caso de o
      // player nao ter episodio nenhum (idx trocado, dado incompleto).
      player_episodio_atual(&t, &e);
      if (!(t >= 0 && e > 0) && ci) { t = ci->temporada; e = ci->episodio; }
      if (t >= 0 && e > 0 && acharProximo(idx, t, e)) {
        fecharEm = 0;            // arma neste quadro, depois de abrir o cartao
        visivel = 1;
      }
      // Sem proximo episodio (fim da serie): nao aparece nada. Mostrar um
      // painel vazio no ultimo episodio seria pior que nao mostrar.
    } else if (extras_n_relacionados() > 0) {
      fecharEm = 0;              // filme nao tem contagem: o dono escolhe
      visivel = 1;
    }
  }

  // Arma ao abrir o cartao. Desligado, espera OK.
  // Recuo e busca pendente suspendem a mesma contagem configuravel.
  if (visivel && serie && janelaSerie && !recuou && !aguardaBusca) {
    // #202: a 1,5x os segundos do arquivo passam mais depressa que os do relogio.
    double resta = vel_tempo_real(durSeg - posSeg, player_velocidade_efetiva());
    if (aguardaCena) fecharEm = 0;
    else fecharEm = posplay_contagem(agora, fecharEm, resta, ajustes_proximo_auto(), ajustes_proximo_contagem());
  }

  // A contagem so vale para o proximo episodio.
  if (visivel && fecharEm && agora >= fecharEm) {
    pedT = proxT; pedE = proxE;
    pedAutomatico = 1;
    esconder();
  }
}

int posplay_evento(const SDL_Event *e) {
  int k;
  if (!visivel || e->type != SDL_KEYDOWN) return 0;
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) toquerol_limpar(&toquePP);
#endif
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    // Dispensar CANCELA a contagem e deixa o video terminar em paz. E GRUDA:
    // fechar sem marcar fazia o painel voltar no quadro seguinte. E esconder,
    // e nao posplay_fechar, justamente porque a marca tem de sobreviver — o
    // par "fechar; marcar de novo" que estava aqui era o mesmo tropeco que
    // apagava o pedido do OK logo abaixo.
    esconder();
    return 1;
  }
  // BAIXO tira o painel do caminho E devolve os controles. Pedido do dono: com
  // o painel no ar ele quer poder descer para a barra de tempo sem perder a
  // reproducao. Voltar apenas dispensa; BAIXO dispensa e mostra o player.
  if (k == SDLK_DOWN) {
    esconder();
    return 2;
  }
  // Cena pos-creditos (serie ou filme): OK = continuar assistindo, sem trocar
  // de titulo. O cartao inteiro volta no fim real do arquivo.
  // Serie com proximo: um segundo botao leva ao proximo episodio na mao.
  if (aguardaCena) {
    int temProx = temProximo();
    if (temProx && k == SDLK_RIGHT && foco == 0) { foco = 1; return 1; }
    if (temProx && k == SDLK_LEFT && foco == 1)  { foco = 0; return 1; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (temProx && foco == 1) { player_aprender_creditos(); pedT = proxT; pedE = proxE; }
      else cenaPendente = 1;
      esconder(); return 1;
    }
    return 0;
  }
  if (serie) {
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      player_aprender_creditos(); pedT = proxT; pedE = proxE; pedAutomatico = 0; esconder(); return 1;
    }
    return 0;
  }
  { int n = extras_n_relacionados();
    if (n > PP_MAX) n = PP_MAX;
    if (k == SDLK_RIGHT && foco + 1 < n) { foco++; return 1; }
    if (k == SDLK_LEFT  && foco > 0)     { foco--; return 1; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      const char *id = extras_relacionado_imdb(foco);
      int alvo = id[0] ? cat_indice_por_imdb(id) : -1;
      if (alvo >= 0) { pedTitulo = alvo; esconder(); }
      // NAO ESTA NO CATALOGO LOCAL, que e o caso NORMAL: o relacionado vem do
      // Trakt/TMDB. Mesmo caminho do detalhe (detail.c, desc_pedir_titulo_semente):
      // "tmdb:<n>" (recomendacao do TMDB, sem imdb) abre pelo id do TMDB —
      // desc_pedir_titulo cortava no ':' e buscava o id "tmdb", o OK morria mudo.
      else if (!strncmp(id, "tmdb:", 5))
        { desc_pedir_titulo_semente("", atol(id + 5), "movie", extras_relacionado_titulo(foco),
                                    extras_relacionado_ano(foco), extras_relacionado_poster(foco));
          esconder(); }
      else if (id[0])
        { desc_pedir_titulo_semente(id, 0, "movie", extras_relacionado_titulo(foco),
                                    extras_relacionado_ano(foco), extras_relacionado_poster(foco));
          esconder(); }
      return 1;
    } }
  return 0;
}

// #177: a mesma regra da lista de episodios (#133). O proximo episodio e, por
// definicao, o que a pessoa ainda nao viu: fica desfocado enquanto o ajuste
// estiver ligado, salvo se o mapa afirma que ja foi visto (reassistindo).
int posplay_desfocar_thumb(int idxCatalogo, int temporada, int episodio) {
  const CatItem *ci;
  if (!ajustes_desfocar_nao_assistidos()) return 0;
  ci = cat_item(idxCatalogo);
  return !(ci && vistoep_estado(ci->imdb, temporada, episodio) == 1);
}

// O ANEL DA CONTAGEM (26, traco 3): o trilho a 16% e o que resta no acento,
// em pontos ao longo do circulo (nao ha arco no gfx).
static void anelContagem(float cx, float cy, float frac, float a) {
  float ar, ag, ab;
  int k, n = 36;
  ajustes_acento(&ar, &ag, &ab);
  gfx_anel((GfxRect){ cx - 12.0f, cy - 12.0f, 24.0f, 24.0f }, 0.5f, 3.0f, 1, 1, 1, 0.16f * a);
  for (k = 0; k < n; k++) {
    float t = (float)k / (float)n, ang = t * 6.2831853f;
    if (t > frac) break;
    gfx_cor((GfxRect){ cx + sinf(ang) * 10.5f - 1.5f, cy - cosf(ang) * 10.5f - 1.5f, 3.0f, 3.0f }, 0.5f, ar, ag, ab, a);
  }
}

// GLASS UI (mockup de 03/10, "proximo" e "mais-como-este"): UMA ILHA. No filme
// o video recua para o topo (player.c) e a ilha fica embaixo dele; na serie o
// video segue em tela cheia e a ilha vai por cima (posplay_sobre_video, #249). Na serie, o
// cartao do proximo episodio sem o retangulo de acento que fingia contorno:
// still grande com "T1E4 · 56 min", "A seguir em 8 s" com o anel da
// contagem, nome, data e duracao, sinopse e "Comecar agora" (o que o OK faz);
// as dicas de Baixo/Voltar vao DENTRO da ilha (caiam em y~1046, overscan).
// No filme, os relacionados na margem de 96 (era 64), o cartaz focado sobe
// com escala e sombra (saiu o anel de 4 px) e o nome dele vai ao cabecalho.
// PONTEIRO (#99). O painel fica por cima dos controles do player (que param de
// registrar com ele no ar): um ANTEPARO na tela inteira absorve o clique no
// vazio — sem ele a camada do player ficaria sem alvos e qualquer clique
// viraria OK, comecando o proximo episodio sem querer. Hover so move foco:
// na serie o botao "Comecar agora" e o unico item (ja em foco) e o focar nao
// mexe em nada — o episodio so comeca pelo OK de um clique NESSE botao. A
// contagem automatica nao e tocada. No filme o foco segue a variavel das
// setas (o cartaz); o OK abre o titulo, nao toca nada.
static void ponteiroComecar(int a, int b) { (void)a; (void)b; }
static void ponteiroCena(int i, int b) { (void)b; if (visivel && aguardaCena && temProximo()) foco = i; }
static void ponteiroCartaz(int i, int b) {
  int n = extras_n_relacionados();
  (void)b;
  if (n > PP_MAX) n = PP_MAX;
  if (!visivel || serie || i < 0 || i >= n || foco == i) return;
  foco = i;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toquePP);
#endif
}
int posplay_teste_foco(void) { return foco; }

static const char *const dicasTeclas[2] = { "\xe2\x86\x93", "Voltar" };
static const char *const dicasRotulos[2] = { "Voltar ao player", "Ficar nos créditos" };

// Keep the approved row until it needs more room; cap at the existing 40px margin.
static int ilhaDicas(GfxRect *ilha, float buttonX, const char *button) {
  float hints = plrui_dicas_video(dicasTeclas, dicasRotulos, 2, -1, 0, 0, 0);
  float needed = buttonX + plrui_botao_largura(button, "pl_play-f") + 22 + hints + 28;
  ilha->w = fminf(fmaxf(ilha->w, needed), NV_TELA_W - 80);
  if (ilha->x + ilha->w > NV_TELA_W - 40) ilha->x = (NV_TELA_W - ilha->w) * .5f;
  if (needed <= ilha->w) return 0;
  ilha->h += 52;
  return 1;
}

static void posplay_desenharCorpo_(Uint32 agora, float baseY);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void posplay_desenhar(Uint32 agora, float baseY) {
  ESCALA_INI();
  posplay_desenharCorpo_(agora, baseY);
  ESCALA_FIM();
}
static void posplay_desenharCorpo_(Uint32 agora, float baseY) {
  float a = anim, x = NV_ILHA_MARGEM_D;   // a margem do player (PLR_MARGEM)
  (void)baseY;
  if (a < 0.01f) return;
  if (visivel) ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);   // anteparo
  { int i = cat_indice_vivo(idx, idTitulo);
    if (i < 0) return;
    idx = i; }

  // FILME COM CENA POS-CREDITOS: uma ilha so com o botao, sobre o video cheio
  // (os cartazes de relacionados recuariam a imagem e cobririam a cena).
  if (!serie && aguardaCena) {
    float sobe = (1.0f - a) * 20.0f;
    GfxRect ilha = { x, NV_TELA_H - 252.0f + sobe, 1180.0f, 140.0f };
    float by, bw;
    int wrap = ilhaDicas(&ilha, 28, "Cena pós-créditos — continuar assistindo");
    by = ilha.y + 40.0f;
    gfx_rect((GfxRect){ 0.0f, NV_TELA_H - 400.0f, NV_TELA_W, 400.0f }, 0, GFX_VEU_BAIXO,
             0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.62f * a);
    plrui_material_video(ilha, 36.0f, a);
    bw = plrui_botao_video(ilha.x + 28.0f, by, "Cena pós-créditos — continuar assistindo", "pl_play-f", 1.0f, a);
    if (visivel && a > 0.99f && a < 1.01f)
      ponteiro_alvo(ilha.x + 28.0f, by, bw, 60.0f, ponteiroComecar, NULL, 0, 0);
    plrui_dicas_video(dicasTeclas, dicasRotulos, 2,
                      ilha.x + 28 + (wrap ? 0 : bw + 22), by + (wrap ? 82 : 30), 0, a);
    return;
  }

  if (serie) {
    const CatItem *ci = cat_item(idx);
    const CatEp *px = NULL;
    int i, n = cat_n_episodios(idx);
    float sobe = (1.0f - a) * 20.0f;
    // Ancorada na base da tela virtual (escala.h): 660 em 1080.
    GfxRect ilha = { x, NV_TELA_H - 420.0f + sobe, 1180.0f, 308.0f }, tr;
    int resta = fecharEm > agora ? (int)((fecharEm - agora + 999) / 1000) : 0;
    char cab[64], num[64], dur[32];
    int wrap = 0;
    if (!aguardaCena || !temProximo())
      wrap = ilhaDicas(&ilha, 28 + 448 + 30,
                      aguardaCena ? "Cena pós-créditos — continuar assistindo" : "Começar agora");
    if (ilha.x + ilha.w > NV_TELA_W - 40.0f) ilha.x = (NV_TELA_W - ilha.w) * 0.5f;
    for (i = 0; i < n; i++) {
      const CatEp *e = cat_episodio(idx, i);
      if (e && e->temporada == proxT && e->episodio == proxE) { px = e; break; }
    }
    // O VIDEO SEGUE EM TELA CHEIA atras do cartao (R8): um veu suave na base
    // garante a leitura sobre qualquer cena, sem escurecer o video todo.
    gfx_rect((GfxRect){ 0.0f, NV_TELA_H - 560.0f, NV_TELA_W, 560.0f }, 0, GFX_VEU_BAIXO,
             0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.62f * a);
    plrui_material_video(ilha, 36.0f, a);
    tr = (GfxRect){ ilha.x + 28.0f, ilha.y + 28.0f, 448.0f, 252.0f };
    { const char *arte = (px && px->thumb[0]) ? px->thumb : (ci ? ci->backdrop : "");
      GLuint t = arte[0] ? tex_obter_larg(arte, tr.w) : 0;
      gfx_cor(tr, 22.0f / tr.h, 0.102f, 0.106f, 0.125f, a);
      // DESFOCAR NAO ASSISTIDOS (#177): o proximo e o que a pessoa nao viu.
      if (t && posplay_desfocar_thumb(idx, proxT, proxE))
        t = gfx_desfocado(t, arte);
      // Sem copia desfocada pronta, gfx_desfocado devolve 0 e fica o fundo.
      // A textura retornada ja esta desfocada: nunca a envie de novo ao cache.
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(arte);
        gfx_rect(tr, t, GFX_CARD, 0, 0, 0, 22.0f / tr.h, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } }
    dur[0] = 0;
    if (px) desc_duracao_txt(px->duracao, dur, sizeof dur);
    snprintf(num, sizeof num, i18n("T%dE%d"), proxT, proxE);
    if (dur[0]) { size_t k = strlen(num); snprintf(num + k, sizeof num - k, " \xc2\xb7 %s", dur); }
    { TxtLinha l = txt_linha(TXT_MINI, num, 243, 242, 239, 255);
      GfxRect chip = { tr.x + 14.0f, tr.y + tr.h - 14.0f - 32.0f, (float)l.w + 24.0f, 32.0f };
      if (ajustes_vidro()) gfx_vidro_chip(chip, 0.5f, 0.80f, a);
      else gfx_cor(chip, 0.5f, 0.082f, 0.086f, 0.102f, a);
      txt_desenhar_alpha(l, chip.x + 12.0f, chip.y + (32.0f - (float)l.h) * 0.5f, a); }
    { float tx = tr.x + tr.w + 30.0f, tw = ilha.x + ilha.w - 28.0f - tx, ty = ilha.y + 28.0f;
      if (aguardaCena) snprintf(cab, sizeof cab, "%s", i18n("Cena pós-créditos"));
      else if (resta > 0) snprintf(cab, sizeof cab, i18n("A seguir em %d s"), resta);
      else snprintf(cab, sizeof cab, "%s", i18n("A seguir"));
      anelContagem(tx + 13.0f, ty + 13.0f, resta > 0 ? (float)resta / (float)ajustes_proximo_contagem() : 1.0f, a);
      plrui_kicker(cab, tx + 26.0f + 12.0f, ty + 4.0f, 243, 242, 239, a * 0.62f);
      ty += 26.0f + 12.0f;
      { TxtLinha t = txt_linha_corta(TXT_ILHA_TITULO, (px && px->nome[0]) ? px->nome : num, 243, 242, 239, 255, tw);
        txt_desenhar_alpha(t, tx, ty, a); ty += (float)t.h + 6.0f; }
      if (px && (px->data[0] || dur[0])) {
        char est[96];
        snprintf(est, sizeof est, "%s%s%s", px->data, px->data[0] && dur[0] ? " \xc2\xb7 " : "", dur);
        { TxtLinha l = txt_linha_corta(TXT_ILHA_SUB, est, 243, 242, 239, 140, tw);
          txt_desenhar_alpha(l, tx, ty, a); ty += (float)l.h + 10.0f; } }
      if (px && px->sinopse[0])
        sinopse_bloco_corta(TXT_ILHA_SUB, px->sinopse, 243, 242, 239, tx, ty, tw, 28.5f, a * 0.68f, 2);
      { float by = ilha.y + ilha.h - 28.0f - 60.0f - (wrap ? 52 : 0);
        if (aguardaCena && temProximo()) {   // continuar | proximo episodio, foco no OK
          float b0 = plrui_botao_video(tx, by, "Continuar assistindo", "pl_play-f", foco == 0 ? 1.0f : 0.0f, a);
          float b1 = plrui_botao_video(tx + b0 + 16.0f, by, "Próximo episódio", NULL, foco == 1 ? 1.0f : 0.0f, a);
          if (visivel && a > 0.99f && a < 1.01f) {
            ponteiro_alvo(tx, by, b0, 60.0f, ponteiroCena, NULL, 0, 0);
            ponteiro_alvo(tx + b0 + 16.0f, by, b1, 60.0f, ponteiroCena, NULL, 1, 0);
          }
          return;
        }
        float bw = plrui_botao_video(tx, by, aguardaCena ? "Cena pós-créditos — continuar assistindo" : "Começar agora", "pl_play-f", 1.0f, a);
        if (visivel && a > 0.99f && a < 1.01f)   // so assentado (sem a subida)
          ponteiro_alvo(tx, by, bw, 60.0f, ponteiroComecar, NULL, 0, 0);
        plrui_dicas_video(dicasTeclas, dicasRotulos, 2,
                          wrap ? ilha.x + 28 : tx + bw + 22, by + (wrap ? 82 : 30), 0, a); } }
    return;
  }

  // FILME: os relacionados que o Trakt ja deu ao abrir o titulo.
  { int n = extras_n_relacionados(), i;
    float sobe = (1.0f - a) * 20.0f;
    float extra = telefoneui_ativo() ? 40.0f : 0;
    GfxRect ilha = { x, NV_TELA_H - 444.0f - extra + sobe, NV_TELA_W - 2.0f * x, 444.0f + extra - 40.0f };
    float y = ilha.y + 26.0f, cx;
    if (n > PP_MAX) n = PP_MAX;
    plrui_material_video(ilha, 36.0f, a);
    { float kx = ilha.x + 34.0f, yc = y + 15.0f;
      float kickerW = plrui_kicker("Mais como este", kx, yc - 9.0f, 243, 242, 239, a * 0.45f);
      if (telefoneui_ativo()) yc += 34;
      else kx += kickerW + 16;
      if (foco < n) {
        float nomeW = 700;
        TxtLinha ano = txt_linha(TXT_G18R, extras_relacionado_ano(foco), 243, 242, 239, 128);
        if (telefoneui_ativo()) nomeW = ilha.w - 68 - (ano.w ? ano.w + 16 : 0);
        TxtLinha t = txt_linha_corta(TXT_ILHA_SECAO, extras_relacionado_titulo(foco), 243, 242, 239, 255, nomeW);
        txt_desenhar_alpha(t, kx, yc - (float)t.h * 0.5f, a);
        kx += (float)t.w + 16.0f;
        if (extras_relacionado_ano(foco)[0]) {
          txt_desenhar_alpha(ano, kx, yc - (float)ano.h * 0.5f + 2.0f, a);
        }
      }
      { const char *k[3] = { "OK", "\xe2\x86\x93", "Voltar" }, *r[3] = { "Abrir", "Voltar ao player", "Dispensar" };
        if (!telefoneui_ativo()) plrui_dicas_video(k, r, 3, ilha.x + ilha.w - 34.0f, yc, 1, a); } }
    y += 30.0f + 22.0f + extra;
    cx = ilha.x + 34.0f;
#ifdef NV_TOUCH_UI
    if (telefoneui_ativo()) {
      float vista = ilha.w - 54.0f, max = fmaxf(0, n * 212.0f - 22.0f - vista);
      toquerol_vincular(&toquePP, (GfxRect){cx, y - 20, vista, 313}, gfx_escala(), 0, max, 0, &scrollPP);
      if (!toquerol_livre(&toquePP)) {
        float esq = foco * 212.0f, dir = esq + 190;
        if (esq < scrollPP) scrollPP = esq;
        if (dir > scrollPP + vista) scrollPP = dir - vista;
        scrollPP = toquerol_clamp(scrollPP, 0, max);
      }
      if (visivel && a > .99f) ponteiro_rolagem(toquePPRolar);
      cx -= scrollPP;
    }
#endif
    gfx_recorte(ilha.x, ilha.y - 40.0f, ilha.w, ilha.h + 40.0f);
    for (i = 0; i < n; i++) {
      const char *po = posterprov_card(extras_relacionado_imdb(i), 0, "movie",
                                      extras_relacionado_poster(i));
      GLuint t = po[0] ? tex_obter_larg(po, 200.0f) : 0;
      int sel = (i == foco);
      float k = sel ? 1.08f : 1.0f, w = 182.0f * k, h = 273.0f * k;
      GfxRect r = { cx + (182.0f - w) * 0.5f, y + 273.0f - h, w, h };
      if (!telefoneui_ativo() && cx + 182.0f > ilha.x + ilha.w - 20.0f) break;
      // A faixa do cartaz inteira, vao incluido: as faixas se encostam e o
      // deslocamento do focado (+8) nao faz o foco oscilar na borda.
      if (visivel && a > 0.99f && a < 1.01f) {
        float ax = cx, aw = 182.0f + 30.0f + (sel ? 8.0f : 0.0f);
        if (telefoneui_ativo()) {
          float right = fminf(ax + aw, ilha.x + ilha.w - 20);
          ax = fmaxf(ax, ilha.x + 34); aw = right - ax;
        }
        if (aw > 0) ponteiro_alvo(ax, y - 20.0f, aw, 313.0f, ponteiroCartaz, NULL, i, 0);
      }
      if (sel) gfx_rect((GfxRect){ r.x - 20.0f, r.y + 4.0f, r.w + 40.0f, r.h + 40.0f }, 0, GFX_SOMBRA,
                        1.0f, 0, 0, 0.5f, 0, 0, 0, 0.55f * a);
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(po);
        gfx_rect(r, t, GFX_CARD, 0, 0, 0, 14.0f / r.h, 0, 0, 0, a * (sel ? 1.0f : 0.82f));
        gfx_tex_aspect_atual = 0.0f;
      } else gfx_cor(r, 14.0f / r.h, .133f, .133f, .133f, a);
      cx += 182.0f + 30.0f + (telefoneui_ativo() ? 0 : sel ? 8.0f : 0.0f);
    }
    gfx_sem_recorte(); }
}

// O TOPO do que o painel ocupa, para quem empilha acima dele (o cartao de
// reacao, reacao.h). As contas sao as de posplay_desenhar, sem a mola.
float posplay_topo(float baseY) {
  if (anim < 0.01f) return baseY;
  return serie ? NV_TELA_H - 420.0f - 16.0f : aguardaCena ? NV_TELA_H - 252.0f - 16.0f : NV_TELA_H - 444.0f - (telefoneui_ativo() ? 40 : 0) - 16.0f;
}

#ifdef NV_SHOT_HOOKS
// Capturas: o cartao de proximo episodio (serie) ou os relacionados (filme)
// no ar, sem esperar o fim do titulo. `fecha` = o instante da contagem.
void posplay_shot(int idxCatalogo, int ehSerie, int t, int e, Uint32 fecha) {
  fixarTitulo(idxCatalogo);
  visivel = 1; anim = 1.0f; serie = ehSerie; foco = 0; dispensado = 0;
  proxT = t; proxE = e; fecharEm = fecha;
}
#endif
