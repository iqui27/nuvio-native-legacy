// A EVIDENCIA DE QUALIDADE DE UMA FONTE, alem da resolucao e do HDR.
//
// O defeito que isto conserta (Code Geass S1E3, 14/10): a lista tinha um
// Blu-ray REMUX 1080p de ~36 Mbps (6,57 GB, FLAC) e um WEB-DL 1080p de ~2 Mbps
// (421 MB, AAC, MP4). A pontuacao de streams.c so via resolucao, HDR, MP4 e
// Atmos: as duas eram "1080p SDR", e o +300 do MP4 dava a vitoria ao WEB-DL.
// O tipo de lancamento (q-remux, q-webdl...) ja era detectado pelos selos e
// nunca entrava na conta.
//
// Aqui mora so a REGRA, sem rede, sem SDL e sem a lista de streams: le os
// textos que o addon mandou (nome, descricao, arquivo), o tamanho e a duracao
// conhecida, e devolve uma nota explicavel. streams.c soma a nota DENTRO da
// faixa de resolucao (nunca passa de um degrau para o outro), e so nos modos
// de qualidade; tests/fontenota.c fixa os casos reais.
//
// PRINCIPIOS (do pedido do dono):
//   - nada e inventado: sem o dado, a parcela e 0 e a ordem de antes decide;
//   - tamanho NAO e bitrate: so vira bitrate com a duracao conhecida;
//   - dado implausivel nao ganha ponto (bitrate acima do possivel para a
//     resolucao, REMUX pequeno demais, bitrate declarado que contradiz o
//     tamanho);
//   - nenhum codec e "melhor" por si: HEVC/AV1 10-bit ganha um desempate
//     minimo; AVC 10-bit (Hi10P) e INCOMPATIVEL com decodificador de hardware
//     de TV e desce;
//   - idioma e PREFERENCIA, nunca filtro: so soma, e so quando ligado.
#ifndef NV_FONTENOTA_H
#define NV_FONTENOTA_H

#include <stddef.h>
#include <stdint.h>

// Tipo de lancamento, do pior para o melhor. CAM e negativo de proposito.
enum {
  FN_TIER_CAM = -1, FN_TIER_DESCONHECIDO = 0, FN_TIER_HDTV = 1, FN_TIER_WEBRIP = 2,
  FN_TIER_WEBDL = 3, FN_TIER_BLURAY = 4, FN_TIER_REMUX = 5
};
// De onde veio o bitrate usado (ou por que nao ha nenhum).
enum { FN_KBPS_NENHUM = 0, FN_KBPS_DECLARADO = 1, FN_KBPS_TAMANHO = 2 };
// Audio, do mais simples ao sem perda.
enum { FN_AUD_DESCONHECIDO = 0, FN_AUD_BASICO = 1, FN_AUD_SURROUND = 2, FN_AUD_SEM_PERDA = 3 };
enum { FN_CODEC_DESCONHECIDO = 0, FN_CODEC_AVC = 1, FN_CODEC_HEVC = 2, FN_CODEC_AV1 = 3 };
// Idioma em relacao a preferencia: 0 sem dado (ou outro idioma), 1 dual/multi
// sem dizer quais, 2 o idioma pedido esta escrito.
enum { FN_IDIOMA_NADA = 0, FN_IDIOMA_DUAL = 1, FN_IDIOMA_EXPLICITO = 2 };

// Alertas: o dado existia mas nao foi aceito (vao para o log).
#define FN_A_REMUX_IMPLAUSIVEL    0x01  // "REMUX" com bitrate de encode
#define FN_A_BITRATE_IMPLAUSIVEL  0x02  // acima do possivel (pacote de temporada?)
#define FN_A_BITRATE_CONFLITO     0x04  // declarado x tamanho/duracao discordam
#define FN_A_UPSCALE              0x08  // 4K que e upscale (nome ou bitrate)
#define FN_A_HI10P                0x10  // AVC 10-bit: sem decodificador de hardware

// Pesos (pontos de streams.c; um degrau de resolucao = 10000). A soma das
// parcelas positivas fica em FN_MAX_EVIDENCIA, longe do degrau.
// O tipo pesa mais que o bitrate: um REMUX plausivel sem bitrate publicado
// continua na frente de um encode que publica o dele; entre encodes, o
// bitrate pode virar a ordem (WEB-DL generoso x Blu-ray espremido).
#define FN_PTS_REMUX       1000
#define FN_PTS_BLURAY       550
#define FN_PTS_WEBDL        350
#define FN_PTS_WEBRIP       250
#define FN_PTS_HDTV         100
#define FN_PTS_CAM        (-3000)
#define FN_PTS_BITRATE_MAX  300
#define FN_PTS_AUD_SEM_PERDA 150
#define FN_PTS_AUD_SURROUND  100
#define FN_PTS_AUD_BASICO     30
#define FN_PTS_10BIT          50
#define FN_PTS_IDIOMA       2000
#define FN_PTS_IDIOMA_DUAL  1200
#define FN_PTS_HI10P      (-2500)
#define FN_PTS_UPSCALE   (-10000)  // um degrau inteiro: compete como 1080p
#define FN_MAX_EVIDENCIA (FN_PTS_REMUX + FN_PTS_BITRATE_MAX + FN_PTS_AUD_SEM_PERDA + FN_PTS_10BIT)

typedef struct {
  const char *rotulo, *descricao, *arquivo;  // textos do addon (NULL = vazio)
  int altura;                // 2160, 1080... (0 = desconhecida)
  uint64_t bytes;            // tamanho exato do arquivo (0 = desconhecido)
  long tamanhoMB;            // tamanho lido do texto, se nao ha bytes (0 = desconhecido)
  double duracaoSeg;         // duracao do titulo (0 = desconhecida)
  const char *idiomaPref;    // "en", "ja", "pt-BR"...; NULL/"" = sem preferencia
  int semHi10p;              // 1 = este aparelho nao decodifica AVC 10-bit
  // DICAS DOS SELOS (badges.c), usadas so quando o texto nao diz nada: o
  // parser classifica os selos sobre nome + descricao + `title` + arquivo, e
  // o `title` nao fica guardado quando o addon manda tambem a descricao.
  int tierDica, audioDica, codecDica;   // FN_TIER_* / FN_AUD_* / FN_CODEC_*; 0 = nenhuma
  // AUDIO DECLARADO PELO ADDON (Stream.audioIdiomas): quando ha lista, ela
  // decide o idioma e o texto da release nao conta — a mesma regra do chip de
  // idioma da folha. idiomaDeclarado e FN_IDIOMA_EXPLICITO (a preferencia esta
  // na lista) ou FN_IDIOMA_NADA (a lista nao a tem).
  int temIdiomaDeclarado, idiomaDeclarado;
} FonteNotaEntrada;

typedef struct {
  int tier, tierAnunciado;   // aceito / o que o nome dizia
  int kbps, kbpsOrigem;      // bitrate aceito (0 = nenhum) e a origem dele
  int codec, bits10;
  int audio;
  int idioma;
  int alertas;
  long ptsTier, ptsBitrate, ptsAudio, ptsCodec, ptsIdioma, ptsCompat;
} FonteNota;

void fontenota_avaliar(const FonteNotaEntrada *e, FonteNota *n);
// So a evidencia de qualidade (tipo, bitrate, audio, codec): os modos de
// qualidade a somam. Nunca passa de FN_MAX_EVIDENCIA.
long fontenota_evidencia(const FonteNota *n);
// Preferencia (idioma) e compatibilidade (Hi10P, upscale): valem em todo modo.
long fontenota_ajuste(const FonteNota *n);
// A SOMA QUE streams.c FAZ, num lugar so (e testavel): `base` e a qualidade
// de sempre (degrau de resolucao * FN_DEGRAU + HDR + desempates). A evidencia
// (se `comEvidencia`) entra DENTRO do degrau: o total nunca alcanca o degrau
// de cima, entao nenhum REMUX 1080p passa de um 4K. O ajuste (idioma,
// compatibilidade) soma depois; o upscale desce um degrau inteiro de proposito.
#define FN_DEGRAU 10000L
long fontenota_somar(long base, const FonteNota *n, int comEvidencia);
// Linha curta e SEGURA para o log ("tier=remux kbps=36000(declared) ..."):
// so campos derivados, nunca texto nem URL da fonte.
void fontenota_explicar(const FonteNota *n, char *dst, size_t tam);

// Pecas usadas acima, expostas para o teste e para quem quiser so uma delas.
int fontenota_bitrate_declarado(const char *texto);          // kbps; 0 = nenhum
int fontenota_idioma(const char *texto, const char *pref);   // FN_IDIOMA_*

#endif
