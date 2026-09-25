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
  MKVASS_NOGO_REDE,        // Range falhou repetidamente (PASSAGEIRO: timeout, 5xx, 429, 403 com varias conexoes)
  MKVASS_NOGO_HTTP,        // o servidor RECUSOU de vez (404, 410, 401, 400, 416 no byte 0,
                           // 403 com uma conexao so) — ver mkvass_ultima_falha
  MKVASS_NOGO_RESTO        // o servidor recusou o RESTO de um Range cortado (0 bytes, #92
                           // v1.4.7): PASSAGEIRO, mas com recuo longo (20, 30, 45, 60 s)
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
// Quanto buffer de VIDEO ha a frente do playhead, em segundos; negativo =
// desconhecido. So a VARREDURA usa: abaixo de 20 s ela pausa, para nao
// disputar a conexao com o video. Chamar junto com mkvass_passo.
void mkvass_folga(double segundosAFrente);

// Nova tentativa da MESMA url e faixa do ultimo pedido, depois de um no-go
// passageiro (ver mkvass_recuo_ms). O documento que ja esta no overlay
// continua em tela: a primeira entrega do fio novo e uma atualizacao, nao uma
// carga do zero — desde que a legenda ainda seja da mesma geracao.
void mkvass_retomar(void);
// Igual, para quando a TV esta desenhando POR ENQUANTO (faixas.c): o fio NAO
// entrega o que ja tinha (sidecar parcial) — so a partir do primeiro bloco
// NOVO, ou do fim da faixa. Sem isto o overlay religava com o texto antigo por
// cima da legenda da TV ate a rede voltar de verdade.
void mkvass_retomar_segurando(void);

// POLITICA DE QUEDA PARA A TV. Recebe o no-go, quantas tentativas ja foram
// feitas nesta escolha de faixa e quantas vezes o Range ja foi recusado.
// Devolve o recuo em ms antes de tentar de novo, ou 0 quando a faixa deve
// voltar a TV DE VEZ.
//   PASSAGEIRA (rede, timeout, 5xx, freio do CDN, Range recusado uma vez):
//     2, 5, 15, 30 s e depois 60 s, SEM LIMITE de tentativas (#92: tres falhas
//     seguidas devolviam a faixa a TV para sempre, com "falha de rede").
//   RESTO RECUSADO (MKVASS_NOGO_RESTO): 20, 30, 45 e depois 60 s. O servidor
//     que recusou continuar um Range cortado recusa de novo 2 s depois.
//   DEFINITIVA (nao e MKV, codec nao ASS, sem indice, Range recusado de novo,
//     recusa HTTP definitiva): 0.
// Quem chama (faixas.c) mantem o overlay do app nas primeiras
// MKVASS_TENTATIVAS_OVERLAY tentativas; dali em diante a TV desenha POR
// ENQUANTO e o app segue tentando em segundo plano — quando uma tentativa
// volta a entregar, a faixa volta ao overlay.
#define MKVASS_TENTATIVAS_OVERLAY 2
long mkvass_recuo_ms(int estado, int falhas, int recusasRange);

// PRE-BUSCA ANTES DO VIDEO (#92, v1.4.7, webOS 25 + Real-Debrid). No registro
// do relato, todo Range do mkvass feito COM O VIDEO TOCANDO era cortado (77465
// e 11929 bytes, sempre os mesmos) e o pedido do resto voltava com zero bytes;
// o video, do mesmo arquivo, tocava. Uma das hipoteses (nao provada) e o CDN
// limitar conexoes ao mesmo arquivo enquanto o pipeline segura uma. Entao o
// que a legenda precisa para comecar — cabecalho, Tracks, fontes anexadas,
// Cues e os blocos dos primeiros minutos — e lido ANTES de a URL ir ao
// pipeline, com teto de MKVASS_PREBUSCA_MS; vencido o teto o video comeca com
// o que chegou e o fio segue em segundo plano, como sempre.
//
// `escolher` recebe os idiomas das legendas do arquivo NA ORDEM DAS TrackEntry
// (a mesma do ordinal) e devolve o ordinal a colher, ou -1. Se a escolhida nao
// for ASS/SSA, ou nada casar, o fio termina depois do cabecalho (um Range).
// `fracInicio` (0..1) e onde o player vai retomar, em fracao da duracao: a
// janela pre-buscada comeca ali (a duracao vem do Info do proprio arquivo).
//
// O fio NAO ENTREGA ao overlay enquanto ninguem o adota: quando faixas.c
// chama mkvass_iniciar_ordinal com a MESMA url e o MESMO ordinal, o fio vivo e
// ADOTADO — nada e pedido de novo — e a primeira entrega sai na hora. Faixa
// diferente, mkvass_parar ou outra url encerram a pre-busca.
// Devolve 1 se a pre-busca comecou.
#ifndef MKVASS_PREBUSCA_MS
#define MKVASS_PREBUSCA_MS 4000
#endif
typedef int (*MkvassEscolher)(const char *const *idiomas, int n);
int  mkvass_prebuscar(const char *url, MkvassEscolher escolher, double fracInicio);
// 0 = nenhuma pre-busca; 1 = correndo; 2 = acabou (pronta, desistiu ou nao
// havia faixa ASS a colher). O player segura o video ate != 1 ou o teto.
int  mkvass_prebusca_fase(void);
// Copia do INICIO do arquivo que a pre-busca leu (o mesmo trecho em que a
// sonda do cabecalho procura Tracks), para a sonda de video.c nao pedir de
// novo pela rede com o video tocando. 1 se havia; *buf e de quem chama.
// Com buf ou n NULL so responde se ha, sem copiar.
int  mkvass_cabecalho(const char *url, unsigned char **buf, long *n);
// O pipeline de video esta com a URL aberta? Diagnostico do #92: toda falha
// de Range diz se aconteceu "com video aberto" ou "antes do video", para o
// proximo registro separar "o CDN recusa enquanto o video toca" do resto.
void mkvass_video_aberto(int aberto);

// A ultima falha de Range da colheita atual: codigo HTTP (0 = sem resposta) e
// da libcurl (28 = prazo; 0 no Tizen). Para o log e o aviso da queda.
void mkvass_ultima_falha(int *http, int *curl);

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
