#include "posplay.h"
#include "intro.h"
#include "idioma.h"
#include "catalogo.h"
#include "extras.h"
#include "gfx.h"
#include "text.h"
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
#include <stdio.h>
#include <math.h>
#include <string.h>

// Constantes do web 1.0.6 (postPlayRecommendationController), nao escolhidas
// aqui: 90% para filme, 5 s de contagem final.
// Recuo para filme SEM capitulo. Proporcional, com piso e teto: ver a nota em
// posplay_atualizar. Fica registrado que o web usa 90% para nao parecer que o
// numero se perdeu — 90% de 105 min sao dez minutos antes do fim.
#define PP_FILME_FRAC     0.045
#define PP_FILME_MIN_S    150.0
#define PP_FILME_MAX_S    330.0
#define PP_CONTAGEM_S     5
// Guardas do #115 ("More like this aparece cedo demais — no comeco do
// filme"). Ver posplay_regra_filme.
#define PP_FILME_METADE   0.5     // nunca antes da metade da duracao
#define PP_CRED_MIN_FRAC  0.75    // marcador antes disto nao e credito final
#define PP_DUR_ESTAVEL_S  8.0     // duracao parada ha tanto tempo, no minimo

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
static int    pedAutomatico, recuou, aguardaBusca;
static int    proxT, proxE;          // proximo episodio, quando ha
static char   proxNome[120];

int posplay_visivel(void) { return visivel; }
// Cartao do proximo episodio: SOBRE o video em tela cheia (o video nao recua).
// Os relacionados do filme, bem mais altos, continuam recuando o video.
int posplay_sobre_video(void) { return visivel && serie; }

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
static void esconder(void) { visivel = 0; fecharEm = 0; foco = 0; dispensado = 1; }

void posplay_fechar(void) {
  visivel = 0; fecharEm = 0; foco = 0;
  durVista = 0.0; durEstavel = 0.0;   // titulo novo: a duracao comeca de novo
  pedT = pedE = 0; pedTitulo = -1;
  pedAutomatico = recuou = aguardaBusca = 0;
  dispensado = 0;   // titulo novo: a dispensa do anterior nao vale mais
}

static void cancelarAutomatico(void) {
  fecharEm = 0;
  if (pedAutomatico) {
    pedT = pedE = 0;
    pedAutomatico = 0;
    if (serie && proxT > 0 && proxE > 0) {
      visivel = 1;
      dispensado = 0;
    }
  }
}

// Recuar e pedir para ficar neste episodio. O cartao continua servindo ao OK,
// mas a contagem nao volta enquanto a pessoa ainda esta na mesma janela.
void posplay_recuar(void) {
  cancelarAutomatico();
  recuou = 1;
}

// A posicao da barra pode mostrar o destino antes de o seek chegar ao motor.
// Enquanto o player espera progresso real, Next continua sendo uma escolha.
void posplay_aguardar_busca(int aguardar) {
  aguardaBusca = aguardar != 0;
  if (aguardaBusca) cancelarAutomatico();
}

int posplay_pediu_episodio(int *t, int *e) {
  if (!pedT) return 0;
  if (t) *t = pedT;
  if (e) *e = pedE;
  pedT = pedE = 0;
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
static int acharProximo(int idxItem, int t, int e) {
  int n = cat_n_episodios(idxItem), i;
  for (i = 0; i < n; i++) {
    const CatEp *ep = cat_episodio(idxItem, i);
    if (!ep || ep->temporada != t || ep->episodio != e) continue;
    { const CatEp *px = cat_episodio(idxItem, i + 1);
      if (!px) return 0;
      proxT = px->temporada; proxE = px->episodio;
      snprintf(proxNome, sizeof proxNome, "%s", px->nome);
      return 1; }
  }
  return 0;
}

// Marcador de creditos que o FILME aceita. Ver posplay_regra_filme.
static int credAceito(double durSeg, double cred) {
  return cred > 1.0 && cred >= durSeg * PP_CRED_MIN_FRAC && cred < durSeg;
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
  // FILME SEM CAPITULOS — e o caso comum, porque MUITA fonte e MP4 e nao
  // Matroska. MEDIDO no log da TV: "mkv: fonte e MP4, sonda dispensada".
  // Capitulo so existe no MKV; num MP4 nao ha o que ler e nao ha marcador.
  //
  // Sem marcador, so resta estimar, e a estimativa e PROPORCIONAL a duracao.
  // Fixar minutos erra nos dois extremos: 3 min sobem com os creditos ja
  // rolando num filme longo (a queixa) e 8 min roubam o desfecho de um curto.
  // Credito costuma ficar perto de 4,5% do filme, com piso e teto para os
  // casos que fogem da regra.
  janela = durSeg * PP_FILME_FRAC;
  if (janela < PP_FILME_MIN_S) janela = PP_FILME_MIN_S;
  if (janela > PP_FILME_MAX_S) janela = PP_FILME_MAX_S;
  // A JANELA NUNCA PASSA DE METADE DO FILME, e este teto vem por ultimo — de
  // proposito, depois do piso, senao o piso o desfaz.
  //
  // Sem ele o piso de 150 s virava a regra em qualquer coisa mais curta que
  // isso: `resta` comeca valendo a duracao inteira, entao no segundo ZERO ja
  // era `resta <= janela` e o painel subia junto com o filme.
  if (janela > durSeg * 0.5) janela = durSeg * 0.5;
  resta = durSeg - posSeg;
  return resta > 0.0 && resta <= janela;
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
  fixarTitulo(idxCatalogo);
  if (durSeg - durVista > 2.0 || durVista - durSeg > 2.0) {
    durVista = durSeg;
    durEstavel = 0.0;
  } else {
    durEstavel += dt;
  }
  if (durSeg <= 1.0) return;

  if (ehSerie && !janelaSerie && !aguardaBusca) recuou = 0;

  if (ehSerie) {
    // A JANELA quem decide e o player: ele e o unico que sabe se os creditos
    // comecaram (intro_ativo/INTRO_CREDITOS) alem dos dois minutos finais. Era
    // a mesma condicao que a caixa antiga de "Próximo episódio" usava, e passar
    // a usa-la aqui foi o que permitiu apagar aquela caixa — havia DUAS
    // interfaces de proximo episodio na mesma tela, uma feia e uma nova.
    //
    // A CONTAGEM continua sendo a do web: so nos 5 s finais. Aparecer cedo e
    // util; comecar a contar cedo tiraria do dono o fim do episodio.
    deveAparecer = janelaSerie && durEstavel >= PP_DUR_ESTAVEL_S;
  } else {
    // FILME: a regra inteira mora em posplay_regra_filme, logo acima, que o
    // teste chama sem player nem rede. Aqui entra so a DURACAO ESTAVEL — ver
    // durEstavel.
    deveAparecer = durEstavel >= PP_DUR_ESTAVEL_S &&
                   posplay_regra_filme(posSeg, durSeg, creditosSeg);
  }

  // Saiu da zona (o dono voltou o filme): a dispensa perde a validade e o
  // painel pode subir de novo quando ele chegar ao fim outra vez.
  if (!deveAparecer) dispensado = 0;

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
      if (!(t > 0 && e > 0) && ci) { t = ci->temporada; e = ci->episodio; }
      if (t > 0 && e > 0 && acharProximo(idx, t, e)) {
        fecharEm = 0;            // a contagem entra so nos segundos finais
        visivel = 1;
      }
      // Sem proximo episodio (fim da serie): nao aparece nada. Mostrar um
      // painel vazio no ultimo episodio seria pior que nao mostrar.
    } else if (extras_n_relacionados() > 0) {
      fecharEm = 0;              // filme nao tem contagem: o dono escolhe
      visivel = 1;
    }
  }

  // CONTAGEM FINAL, os 5 s que posplay.h documenta
  // (POST_PLAY_RECOMMENDATION_FINAL_COUNTDOWN_SECONDS do web). O cabecalho
  // "A seguir em %d s", a constante PP_CONTAGEM_S e o bloco logo abaixo que
  // consome `fecharEm` estavam aqui desde o primeiro commit deste arquivo;
  // faltava a unica linha que ARMA o relogio. O painel ficava em "A seguir"
  // para sempre e nada tocava sozinho — no video do issue #14 e o que se ve.
  //
  // DEPOIS do bloco que abre o painel, e nao antes: no quadro em que o painel
  // sobe, `visivel` so vira 1 ali em cima. Armar antes deixaria o primeiro
  // quadro de fora, e se ele ja for o dos segundos finais o relogio nunca
  // arma — foi assim que este teste falhou da primeira vez.
  //
  // Conta o que RESTA de verdade, e nao 5 s a partir de agora: os creditos
  // comecam muito antes do fim em algumas series, e um relogio fixo
  // dispararia no meio deles.
  if (visivel && serie && !fecharEm && !recuou && !aguardaBusca) {
    // #202: a 1,5x os segundos do arquivo passam mais depressa que os do relogio.
    double resta = vel_tempo_real(durSeg - posSeg, player_velocidade_efetiva());
    if (resta <= (double)PP_CONTAGEM_S) {
      if (resta < 0.0) resta = 0.0;
      fecharEm = agora + (Uint32)(resta * 1000.0);
      if (!fecharEm) fecharEm = 1;   // 0 quer dizer "sem contagem"
    }
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
      // Trakt e o catalogo tem os titulos das fileiras da home. Antes o OK
      // simplesmente nao fazia nada — o "nao da pra clicar" do relatorio.
      // desc_pedir_titulo e o mesmo caminho que a pagina de titulo ja usa
      // (detail.c:938) para abrir um relacionado que ainda nao temos.
      else if (id[0]) { desc_pedir_titulo(id); esconder(); }
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
static void posplay_desenharCorpo_(Uint32 agora, float baseY);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void posplay_desenhar(Uint32 agora, float baseY) {
  ESCALA_INI();
  posplay_desenharCorpo_(agora, baseY);
  ESCALA_FIM();
}
static void posplay_desenharCorpo_(Uint32 agora, float baseY) {
  float a = anim, x = 96.0f;
  (void)baseY;
  if (a < 0.01f) return;
  { int i = cat_indice_vivo(idx, idTitulo);
    if (i < 0) return;
    idx = i; }

  if (serie) {
    const CatItem *ci = cat_item(idx);
    const CatEp *px = NULL;
    int i, n = cat_n_episodios(idx);
    float sobe = (1.0f - a) * 20.0f;
    // Ancorada na base da tela virtual (escala.h): 660 em 1080.
    GfxRect ilha = { x, NV_TELA_H - 420.0f + sobe, 1180.0f, 308.0f }, tr;
    int resta = fecharEm > agora ? (int)((fecharEm - agora + 999) / 1000) : 0;
    char cab[64], num[64], dur[32];
    // Na tela virtual estreita (150%: 1280) a margem de 96 nao deixa os 1180
    // da ilha: ela centra, com a largura inteira (as dicas vao ate a borda).
    if (ilha.x + ilha.w > NV_TELA_W - 40.0f) ilha.x = (NV_TELA_W - ilha.w) * 0.5f;
    for (i = 0; i < n; i++) {
      const CatEp *e = cat_episodio(idx, i);
      if (e && e->temporada == proxT && e->episodio == proxE) { px = e; break; }
    }
    // O VIDEO SEGUE EM TELA CHEIA atras do cartao (R8): um veu suave na base
    // garante a leitura sobre qualquer cena, sem escurecer o video todo.
    gfx_rect((GfxRect){ 0.0f, NV_TELA_H - 560.0f, NV_TELA_W, 560.0f }, 0, GFX_VEU_BAIXO,
             0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.62f * a);
    plrui_material(ilha, 36.0f, 0, a);
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
      if (ajustes_vidro()) gfx_cor(chip, 0.5f, 0.055f, 0.059f, 0.071f, 0.80f * a);
      else gfx_cor(chip, 0.5f, 0.082f, 0.086f, 0.102f, a);
      txt_desenhar_alpha(l, chip.x + 12.0f, chip.y + (32.0f - (float)l.h) * 0.5f, a); }
    { float tx = tr.x + tr.w + 30.0f, tw = ilha.x + ilha.w - 28.0f - tx, ty = ilha.y + 28.0f;
      if (resta > 0) snprintf(cab, sizeof cab, i18n("A seguir em %d s"), resta);
      else snprintf(cab, sizeof cab, "%s", i18n("A seguir"));
      anelContagem(tx + 13.0f, ty + 13.0f, resta > 0 ? (float)resta / (float)PP_CONTAGEM_S : 1.0f, a);
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
        txt_bloco_corta(TXT_ILHA_SUB, px->sinopse, 243, 242, 239, tx, ty, tw, 28.5f, a * 0.68f, 2);
      { float by = ilha.y + ilha.h - 28.0f - 60.0f;
        float bw = plrui_botao(tx, by, "Começar agora", "pl_play-f", 1.0f, a);
        const char *k[2] = { "\xe2\x86\x93", "Voltar" }, *r[2] = { "Voltar ao player", "Ficar nos créditos" };
        plrui_dicas(k, r, 2, tx + bw + 22.0f, by + 30.0f, 0, a); } }
    return;
  }

  // FILME: os relacionados que o Trakt ja deu ao abrir o titulo.
  { int n = extras_n_relacionados(), i;
    float sobe = (1.0f - a) * 20.0f;
    GfxRect ilha = { x, NV_TELA_H - 444.0f + sobe, NV_TELA_W - 192.0f, 444.0f - 40.0f };   // 636 em 1080
    float y = ilha.y + 26.0f, cx;
    if (n > PP_MAX) n = PP_MAX;
    plrui_material(ilha, 36.0f, 0, a);
    { float kx = ilha.x + 34.0f, yc = y + 15.0f;
      kx += plrui_kicker("Mais como este", kx, yc - 9.0f, 243, 242, 239, a * 0.45f) + 16.0f;
      if (foco < n) {
        TxtLinha t = txt_linha_corta(TXT_ILHA_SECAO, extras_relacionado_titulo(foco), 243, 242, 239, 255, 700.0f);
        txt_desenhar_alpha(t, kx, yc - (float)t.h * 0.5f, a);
        kx += (float)t.w + 16.0f;
        if (extras_relacionado_ano(foco)[0]) {
          TxtLinha l = txt_linha(TXT_G18R, extras_relacionado_ano(foco), 243, 242, 239, 128);
          txt_desenhar_alpha(l, kx, yc - (float)l.h * 0.5f + 2.0f, a);
        }
      }
      { const char *k[3] = { "OK", "\xe2\x86\x93", "Voltar" }, *r[3] = { "Abrir", "Voltar ao player", "Dispensar" };
        plrui_dicas(k, r, 3, ilha.x + ilha.w - 34.0f, yc, 1, a); } }
    y += 30.0f + 22.0f;
    cx = ilha.x + 34.0f;
    gfx_recorte(ilha.x, ilha.y - 40.0f, ilha.w, ilha.h + 40.0f);
    for (i = 0; i < n; i++) {
      const char *po = extras_relacionado_poster(i);
      GLuint t = po[0] ? tex_obter_larg(po, 200.0f) : 0;
      int sel = (i == foco);
      float k = sel ? 1.08f : 1.0f, w = 182.0f * k, h = 273.0f * k;
      GfxRect r = { cx + (182.0f - w) * 0.5f, y + 273.0f - h, w, h };
      if (cx + 182.0f > ilha.x + ilha.w - 20.0f) break;
      if (sel) gfx_rect((GfxRect){ r.x - 20.0f, r.y + 4.0f, r.w + 40.0f, r.h + 40.0f }, 0, GFX_SOMBRA,
                        1.0f, 0, 0, 0.5f, 0, 0, 0, 0.55f * a);
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(po);
        gfx_rect(r, t, GFX_CARD, 0, 0, 0, 14.0f / r.h, 0, 0, 0, a * (sel ? 1.0f : 0.82f));
        gfx_tex_aspect_atual = 0.0f;
      } else gfx_cor(r, 14.0f / r.h, .133f, .133f, .133f, a);
      cx += 182.0f + 30.0f + (sel ? 8.0f : 0.0f);
    }
    gfx_sem_recorte(); }
}

// O TOPO do que o painel ocupa, para quem empilha acima dele (o cartao de
// reacao, reacao.h). As contas sao as de posplay_desenhar, sem a mola.
float posplay_topo(float baseY) {
  if (anim < 0.01f) return baseY;
  return serie ? NV_TELA_H - 420.0f - 16.0f : NV_TELA_H - 444.0f - 16.0f;
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
