// Leitura do CABECALHO de um Matroska, so para descobrir o idioma das faixas.
//
// POR QUE ISTO EXISTE. O pipeline da LG devolve, no sourceInfo, o idioma de
// cada faixa de AUDIO ("en", "es", "fr", "it") e NENHUM idioma de legenda:
// medido num arquivo do dono com 43 legendas, todas com
// "language":"(null)", e os unicos campos do subtitleTrackInfo sao trackNum,
// language, type e periodStart. Nao ha outro campo para ler — a informacao
// simplesmente nao sai do pipeline.
//
// O app web mostra os idiomas porque o NAVEGADOR demuxa o arquivo por conta
// propria e expoe textTracks. Este modulo faz a mesma coisa em pequeno: baixa
// os primeiros megabytes por Range e le o elemento Tracks do EBML.
//
// NAO E UM DEMUXER. Nao decodifica nada, nao segue Cues, nao le Clusters. Anda
// pela arvore de elementos ate Segment > Tracks e para. Qualquer coisa que nao
// case com o esperado faz a leitura desistir em silencio — o chamador continua
// com "Legenda N", que e o que havia antes.
#ifndef NV_MKV_H
#define NV_MKV_H

#define MKV_MAX_FAIXAS 64

typedef struct {
  int  numero;        // TrackNumber (NAO e o `trackNum` da LG: ver mkv_casar_legendas)
  int  tipo;          // 1 video, 2 audio, 17 legenda (TrackType do Matroska)
  char idioma[8];     // "por", "eng"... vazio quando o arquivo nao etiqueta
  char nome[48];      // Name, quando existe ("Forced", "SDH", "Full")
  char codec[24];     // CodecID ("S_TEXT/UTF8", "S_HDMV/PGS")
  int  forcado;       // FlagForced: so letreiros/falas em lingua estrangeira
  int  sdh;           // FlagHearingImpaired (#287): legenda para surdos
  int  canais;        // Audio > Channels (2, 6, 8); 0 quando nao informado
  // Dolby Vision configuration record (BlockAdditionMapping dvcC/dvvC) of a
  // video track: the file's own word on profile and layers. dvPerfil 0 = none.
  int  dvPerfil, dvNivel, dvRpu, dvEl, dvBl, dvCompat;
  // Diagnostico do DV (203-dvrpu): o que a TrackEntry trouxe, para separar
  // "arquivo sem dvcC" de "a sonda nao viu". cpN = bytes do CodecPrivate (hvcC),
  // hvccNal62 = o hvcC traz um array de NAL tipo 62 (RPU), bamN/bamTipo = quantos
  // BlockAdditionMapping e o BlockAddIDType do ultimo ('dvcC' = 0x64766343),
  // nalTam = lengthSizeMinusOne+1 do hvcC (0 sem hvcC), entradaInteira = a
  // TrackEntry foi lida ate o fim (0 = parou num elemento invalido).
  long cpN;
  int  hvccNal62, bamN, nalTam, entradaInteira;
  unsigned long bamTipo;
} MkvFaixa;

// O que a ultima sonda (mkv_faixas_e_caps / mkv_faixas_do_trecho) viu. Estado
// estatico, como o resto deste modulo: le-se no mesmo fio, logo depois.
typedef struct {
  long lidos;              // bytes do trecho varrido
  int  tracksAchado;       // o elemento Tracks apareceu no trecho
  int  tracksInteiro;      // ... e coube inteiro
  long tracksTam;          // tamanho declarado do Tracks
  // RPU de Dolby Vision EM BANDA (NAL HEVC tipo 62) nos primeiros quadros do
  // primeiro Cluster, para arquivos com DV no fluxo mas sem dvcC no cabecalho.
  int  rpu;                // 1 achou NAL 62; 0 quadro(s) inteiro(s) sem; -1 indeciso
  int  quadros;            // quadros de video inteiros varridos
  int  rpuTipo, rpuPerfil; // rpu_type e vdr_rpu_profile do 1o RPU (-1 = nao lido)
  long long blocoIni, blocoFim; // quadro de video cortado pelo trecho: [ini, fim) absolutos, -1 sem
} MkvDiag;
const MkvDiag *mkv_diag(void);
// Varre so os dados de UM quadro HEVC (com prefixo de tamanho de `nalTam`
// bytes): 1 = tem NAL 62, 0 = nao tem, -1 = quadro malformado. Puro.
int mkv_quadro_tem_rpu(const unsigned char *p, long n, int nalTam, int *rpuTipo, int *rpuPerfil);
// Quando a sonda ficou indecisa porque o 1o quadro de video passa do trecho:
// UM Range com o quadro inteiro, se ele tiver ate `teto` bytes. Atualiza o
// mkv_diag(). Devolve o novo diag->rpu. BLOQUEIA.
int mkv_rpu_alem(const char *url, int nalTam, long teto);

#define MKV_MAX_CAPS 64

// CAPITULO. Existe pelo pos-reproducao: sem marcador, "quando comecam os
// creditos" vira chute, e um chute erra em minutos. Muitos lancamentos trazem
// um capitulo final chamado "End Credits"/"Creditos" — quando ele esta la, e a
// resposta exata, de graca, no cabecalho que ja baixamos para as faixas.
typedef struct {
  double inicio;      // segundos desde o inicio do arquivo
  char   nome[64];    // ChapString, quando o arquivo nomeia
} MkvCap;

// Le o cabecalho de `url` e preenche `saida`. Devolve quantas faixas achou, 0
// quando nao deu (nao e MKV, servidor sem Range, cabecalho maior que o trecho).
// BLOQUEIA: chamar de um fio proprio.
int mkv_faixas(const char *url, MkvFaixa *saida, int max);

// Mesma leitura, UMA viagem so, devolvendo tambem os capitulos. `caps` pode ser
// NULL. O numero de capitulos sai por `nCaps`.
//
// UMA VIAGEM E O PONTO: a nota no topo de mkv.c registra que esta leitura
// acontece com o video JA TOCANDO, pela mesma conexao — uma segunda descida de
// 320 KB para buscar capitulos custaria exatamente o engasgo que aquela nota
// descreve.
int mkv_faixas_e_caps(const char *url, MkvFaixa *saida, int max,
                      MkvCap *caps, int maxCaps, int *nCaps);

// O QUE A ULTIMA mkv_faixas_e_caps LEU no comeco do arquivo. "0 faixas" tinha
// duas causas que a sonda do DV (video.c) tratava igual: a rede nao trouxe nada
// (vale nova tentativa) e o arquivo chegou e NAO e Matroska — resposta
// definitiva. C9, 2.0.3: um MP4 sem extensao na URL levou 3 novas tentativas
// (3/8/20 s) com a tela do DV de pe.
#define MKV_CONT_NADA  0   // nada (ou < 4 bytes) chegou: rede, Range, prazo
#define MKV_CONT_MKV   1   // assinatura EBML 1A 45 DF A3
#define MKV_CONT_MP4   2   // caixa ISO-BMFF no byte 4 (ftyp, styp, moov, mdat, free, skip, wide)
#define MKV_CONT_TS    3   // MPEG-TS: 0x47 a cada 188 bytes (ou M2TS, 192 com 4 de prefixo)
#define MKV_CONT_TEXTO 4   // HTML/JSON: pagina de erro com 200, pode passar — vale nova tentativa
#define MKV_CONT_OUTRO 5   // binario que nao e nenhum dos acima: tambem nao e Matroska
int mkv_contentor(const unsigned char *p, long n);   // puro
int mkv_ultimo_contentor(void);
// 1 quando o contentor lido encerra a pergunta "e MKV?" sem nova tentativa.
int mkv_contentor_definitivo(int contentor);
const char *mkv_contentor_nome(int contentor);

// A MESMA leitura sobre um trecho que ja esta na memoria (o inicio do arquivo
// que a pre-busca do mkvass leu antes do video, #92 v1.4.7). Sem rede. Devolve
// 0 quando o trecho nao traz Tracks INTEIRO — ai quem chama vai a rede. Os
// capitulos so vem se couberem no trecho.
int mkv_faixas_do_trecho(const unsigned char *buf, long n, MkvFaixa *saida, int max,
                         MkvCap *caps, int maxCaps, int *nCaps);

// Segundo do capitulo que se IDENTIFICA como creditos pelo nome, ou 0. Nao
// chuta pela posicao: quem sabe a duracao do filme e quem chama, e sem ela
// "ultimo capitulo" nao distingue creditos de cena final.
double mkv_creditos_nomeados(const MkvCap *caps, int n);
// O inicio do ULTIMO capitulo que nao e previa do proximo episodio ("Preview",
// "Next Episode"...), para a regra posicional de video_creditos; 0 com menos
// de dois capitulos. 2.0.3: em anime o ultimo capitulo e a previa, nao o ED.
double mkv_creditos_ultimo(const MkvCap *caps, int n);

// Capitulos que o trecho do cabecalho NAO trouxe (203-capitulos): se `cab`
// (o inicio do arquivo, ja baixado) traz Chapters inteiro, devolve; senao segue
// o SeekHead com UM Range e le o elemento inteiro. Devolve quantos capitulos;
// `status` (opcional) recebe -1 quando nao houve pedido extra (resultado
// definitivo) ou o HTTP do pedido extra (0 = sem resposta).
// BLOQUEIA: chamar de fio proprio.
int mkv_capitulos_alem(const char *url, const unsigned char *cab, long cabN,
                       MkvCap *caps, int maxCaps, int *status);

// Capitulo de ABERTURA pelo nome ("Opening", "OP", "Intro", "Abertura"): [ini,
// fim) vai ate o inicio do capitulo seguinte. 0 = nao ha.
int mkv_intro_nomeada(const MkvCap *caps, int n, double *ini, double *fim);
// Inicio do ULTIMO capitulo quando ele e previa do proximo episodio ("Preview",
// "Next Episode"); 0 caso contrario. Nao e credito: marca ate onde os creditos vao.
double mkv_previa_nomeada(const MkvCap *caps, int n);

// CASA as legendas que a TV lista com as TrackEntry de legenda do arquivo (#92).
//
// O `trackNum` do subtitleTrackInfo da LG NAO e o TrackNumber do Matroska. E o
// ORDINAL da faixa entre as legendas do arquivo, contado de 0 na ordem das
// TrackEntry — o mesmo que o AVPlay da Samsung chama de track_num. MEDIDO na
// C9 (webOS 4.5) em 22/09/2026, num Erai-raws de One Piece com 8 legendas ASS:
// sourceInfo com trackNum 0..7, e as legendas do arquivo com TrackNumber
// 3..10 (1 e video, 2 e audio). TrackNumber 0 nem existe no formato. Casar
// por TrackNumber, como o video.c fazia, dava a legenda 0 a ninguem, a 1 ao
// VIDEO, a 2 ao AUDIO e da 3 em diante a legenda TRES posicoes antes — era o
// "Italian" do #92 mostrando falas em ingles, e o "English" sem o selo ASS
// caindo no desenho da TV, que pisca e come metade das falas.
//
// `tvNum[i]` e o trackNum da i-esima legenda da TV; `idxFx[i]` recebe o indice
// em `fx` da TrackEntry correspondente, ou -1. Devolve o modo usado:
//   MKV_CASA_ORDINAL  todos os trackNum cabem em [0, legendas do arquivo) e as
//                     duas contagens batem — o caso medido;
//   MKV_CASA_NUMERO   todo trackNum e o TrackNumber de uma legenda do arquivo
//                     (a leitura antiga; fica como segunda tentativa, nunca
//                     misturada com a primeira);
//   MKV_CASA_NADA     nenhuma das duas fecha: nada e casado. Idioma errado e
//                     pior que idioma nenhum, e codec errado manda a faixa
//                     errada para o overlay.
enum { MKV_CASA_NADA = 0, MKV_CASA_ORDINAL = 1, MKV_CASA_NUMERO = 2 };
int mkv_casar_legendas(const MkvFaixa *fx, int n, const int *tvNum, int nTv,
                       int *idxFx);

#endif
