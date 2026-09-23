// Legenda ASS EMBUTIDA num Matroska, colhida por HTTP Range e entregue ao
// overlay do app — sem baixar o filme (#92, fase 3).
//
// POR QUE EXISTE. O pipeline uMS da LG desenha S_TEXT/ASS como texto corrido:
// sem posicao, sem cor, e engolindo eventos simultaneos (a fala embaixo e o
// letreiro em cima). O app ja tem um overlay que entende ASS (legenda.c +
// player.c), mas so recebia arquivo EXTERNO. O que faltava era obter o TEXTO
// da faixa de dentro do MKV que ja esta tocando.
//
// COMO. O spike tests/spike_mkv_ass.py provou que ffmpeg e mkvmerge escrevem
// um CuePoint POR BLOCO da faixa de legenda, com CueRelativePosition. Entao:
// SeekHead -> Cues (normalmente no FIM do arquivo) -> CuePoints da faixa ->
// um Range pequeno por bloco. Custo medido: 0,07 % dos bytes; o custo real e
// LATENCIA (~2 Ranges por fala), e por isso a colheita e feita A FRENTE DO
// PLAYHEAD, numa janela de MKVASS_JANELA_SEG segundos, com teto de pedidos
// por segundo — e nao tudo de uma vez.
//
// NAO E UM DEMUXER. Nao decodifica nada. Com indice da faixa, le SO os bytes
// que ele aponta. SEM indice da faixa (mkvmerge --cues none, remux que so
// indexa o video, CuePoint sem CueRelativePosition) VARRE os Clusters em
// janelas, pulando o payload das outras faixas — custa banda, e por isso so
// entra nesse caso e avisa no log (#92: antes era no-go e a faixa voltava ao
// renderizador da TV, que pisca e corta metade da frase).
//
// SIDECAR. Ao terminar (ou ao sair com o que colheu), o corpo ASS montado vai
// para dados_dir()/mkvass-<hash da url>.ass; na proxima abertura da MESMA URL
// ele volta de la sem um Range sequer. URL de debrid que muda a cada sessao
// nao reaproveita — e o preco de nao ter outro identificador do arquivo.
//
// TEMPO. O Start de cada evento e o timestamp do Cluster + o relativo do
// bloco, em TimestampScale do Segment (1 ms por padrao), contado do INICIO DO
// ARQUIVO. O posSeg do player vem do currentTime do uMS, que TAMBEM conta do
// inicio do arquivo — a mesma origem que a legenda externa SRT ja usa e que
// funciona. Nao ha compensacao de offset: se um dia um arquivo comecar num
// Cluster com Timestamp != 0 e o uMS reportar 0 ali, o desvio aparece igual
// para o SRT externo, e o ajuste de "Atraso" da folha e o remedio.
//
// SAMSUNG (Tizen): a URL do AVPlay tambem alimenta este coletor. O overlay usa
// Range via XHR do rede.c; enquanto coleta, a faixa nativa fica em silencio.
// Se o servidor/arquivo nao permitir extracao recuperavel, o modulo devolve a
// faixa ao AVPlay como fallback.
#ifndef NV_MKVASS_H
#define NV_MKVASS_H

// Estados. Os >= MKVASS_NOGO sao terminais: a folha mostra o motivo e volta a
// deixar o pipeline desenhar.
enum {
  MKVASS_OCIOSO = 0,       // nada pedido
  MKVASS_PREPARANDO,       // lendo cabecalho e Cues
  MKVASS_COLHENDO,         // indice pronto, colhendo a frente do playhead
  MKVASS_COMPLETO,         // todos os blocos entregues (ou sidecar completo)
  MKVASS_NOGO = 10,
  MKVASS_NOGO_NAO_MKV = 10, // sem assinatura EBML (MP4, HTML de erro...)
  MKVASS_NOGO_SEM_RANGE,   // servidor devolveu o inicio do arquivo onde pedimos o fim
  MKVASS_NOGO_FAIXA,       // a faixa pedida nao e S_TEXT/ASS nem S_TEXT/SSA
  MKVASS_NOGO_SEM_INDICE,  // sem Cues, ou sem CuePoint da faixa de legenda
  MKVASS_NOGO_SEM_REL,     // CuePoints existem mas sem CueRelativePosition
  MKVASS_NOGO_REDE         // Range falhou repetidamente
};

// Comeca a colher a faixa `numeroFaixa` (TrackNumber do Matroska, o mesmo
// `numero` do VideoFaixa) de `url`, em fio proprio. Substitui qualquer
// colheita anterior. NAO bloqueia.
void mkvass_iniciar(const char *url, int numeroFaixa);
// Variante Tizen: usa o ordinal de subtitleTrack publicado pelo AVPlay e o
// resolve para TrackNumber lendo Tracks antes de consultar os CuePoints.
void mkvass_iniciar_ordinal(const char *url, int ordinalFaixa);

// Chamar UMA VEZ POR QUADRO com a posicao do player: e o que move a janela de
// colheita. Barato — so compara e acorda o fio quando a posicao andou.
void mkvass_passo(double posSeg);

// Para a colheita e grava o sidecar parcial com o que ja veio. NAO bloqueia:
// o fio termina sozinho (pode estar no meio de um Range).
void mkvass_parar(void);

int  mkvass_estado(void);
// 1 quando o estado e um dos no-go.
int  mkvass_nogo(void);
// 1 quando a colheita e por VARREDURA dos Clusters (o indice nao apontava os
// blocos da faixa: sem CuePoint dela, ou sem CueRelativePosition). Custa
// banda; a folha diz isso ao lado da faixa.
int  mkvass_varredura(void);
// 1 enquanto ha fio de colheita vivo (o teste espera por isto).
int  mkvass_ocupado(void);

// Telemetria: Ranges feitos, bytes baixados, blocos colhidos e total de
// blocos indexados. Qualquer ponteiro pode ser NULL.
void mkvass_estatisticas(long *pedidos, long *bytes, int *colhidos, int *total);

#endif
