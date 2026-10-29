// LINK DO CDN PARA O PLAYER DA TV (.tpk, PR #567, 16/10/2026).
//
// O player da Samsung (Tizen.Multimedia) desiste de abrir em ~3 s quando o
// PRIMEIRO pedido demora: ConnectionFailed (0xfe6c0026) com buffering -1%.
// O MediaFusion leva 2-4 s para responder o primeiro pedido (resolve o link no
// TorBox e redireciona ao CDN). Medido no S95C: o MESMO arquivo (Kaguya BD T5,
// 31 222 190 877 bytes, mesmo nexus-040.neur.tb-cdn.st) abre em 2 s pelo
// AIOStreams e falha sempre em 3,1 s pelo MediaFusion. O BD T3 so tem links do
// MediaFusion, e por isso nunca tocou.
//
// A correcao: o app segue o redirecionamento ELE MESMO (o mesmo GET Range de
// 64 bytes da verificacao) e entrega ao player o endereco final no CDN. O
// player fala direto com o CDN; nenhum byte muda, nada e convertido.
//
// O LINK ORIGINAL CONTINUA SENDO A IDENTIDADE em todo o resto: conversor DTS,
// "Reusar o ultimo link", memoria de links mortos, legendas, leitura do
// cabecalho. So o player recebe o do CDN.
//
// VALIDADE: o link do TorBox CDN vale ~3 h. Uma conexao ja aberta continua,
// mas toda conexao NOVA do player (seek, rebuffer, queda de rede) usaria o
// link velho. Por isso o video_tpk.c pede um link novo (abrindo de novo o
// link do addon, que pede um novo ao TorBox com a chave da pessoa):
//   - em toda abertura (e na reconexao);
//   - num seek ou ao retomar a pausa com o link mais velho que
//     cdnlink_validade_ms() (2 h 30);
//   - num erro depois de a fonte ter tocado (reconexao com link novo).
//
// Logs: so hosts, nunca o link (o do CDN carrega um token).
#ifndef NUVIO_CDNLINK_H
#define NUVIO_CDNLINK_H
#include <stdint.h>

// Prazo da resolucao na abertura. O app ja mostra "Abrindo fonte"; o prazo de
// abertura do app.c e de 15 s sem sinal. 10 s e nao 6: o MediaFusion ja levou
// mais de 6 s no S95C (16/10), e cair para o link do addon nesse caso nao
// adianta — o player desiste dele em ~3 s pelo mesmo motivo.
#define CDNLINK_PRAZO_S 10
// O que a verificacao resolveu ate este tempo atras serve sem pedido novo.
#define CDNLINK_REUSO_MS 60000u
// Validade usada nas decisoes (o TorBox da ~3 h).
#define CDNLINK_VALIDADE_MS (150u * 60u * 1000u)

// 1 se `orig` pode passar pelo CDN: http(s), nao e 127.0.0.1/localhost (proxy
// e conversor locais) e nao e playlist (.m3u8/.mpd: canal e HLS seguem como
// estao).
int cdnlink_origem_ok(const char *orig);

// 1 se `final` e um destino util para o player no lugar de `orig`: http(s),
// diferente do original e com host diferente (sem redirecionamento nao ha o
// que ganhar).
int cdnlink_aceita(const char *orig, const char *final);

// Guarda orig -> final (a verificacao de streams.c chama pelo ponteiro
// stream_cdn_lembrar, ligado no tpk.c). Headers and captured profile session
// own each entry; stale workers cannot publish. Invalid redirects are ignored.
void cdnlink_lembrar(const char *orig, const char *final, const char *cabecalhos, uint64_t sessao);

// 1 e `dst` = final guardado para `orig` ha no maximo `maxMs`; `*idadeMs`
// (opcional) recebe a idade.
int cdnlink_recente(const char *orig, const char *cabecalhos, unsigned maxMs, char *dst, unsigned tam, unsigned *idadeMs);

// Resolve agora (bloqueia ate `segundos`): segue os redirecionamentos de
// `orig` com os cabecalhos do addon (linhas "Nome: valor" separadas por \n).
// 1 e `dst` = endereco final aceito; guarda no cache e loga a linha
// [fonte-cdn] com os hosts e o tempo. `*ms` (opcional) = quanto levou.
int cdnlink_resolver(const char *orig, const char *cabecalhos, int segundos,
                     char *dst, unsigned tam, unsigned *ms);

// Validade em ms.
unsigned cdnlink_validade_ms(void);

// Host de `url` (sem esquema, porta nem caminho) em `b`. Para logs.
const char *cdnlink_host(const char *url, char *b, unsigned n);

// O player do .tpk usa o cdnlink por estes ganchos, ligados no tpk.c
// (video_tpk_cdn_ganchos): sem eles (testes do video_tpk.c, outros alvos) o
// player abre o link do addon, como sempre.
typedef struct {
  int (*origem_ok)(const char *orig);
  int (*recente)(const char *orig, const char *cabecalhos, unsigned maxMs, char *dst, unsigned tam, unsigned *idadeMs);
  int (*resolver)(const char *orig, const char *cabecalhos, int segundos,
                  char *dst, unsigned tam, unsigned *ms);
  unsigned (*validade_ms)(void);
} CdnlinkGanchos;
void video_tpk_cdn_ganchos(const CdnlinkGanchos *g);   // em video_tpk.c

// Para teste: esquece o cache e define o relogio (0 = o de verdade).
void cdnlink_zerar(void);
void cdnlink_relogio_teste(unsigned (*fn)(void));

#endif
