// PROXY DE TS DA LIVE TV (#158). Na LG o motor HLS do uMS nao decodifica
// certos canais ao vivo (C4 do pasha, C9 do dono com "TNT Sports 1 HD": dado
// chega, VDEC concedido, decoder mudo; segmento unico direto da "203 AV Type
// Not Founded"), enquanto um .ts CONTINUO do mesmo provedor toca em 3,4 s.
//
// O proxy e um servidor HTTP em 127.0.0.1 (porta livre) dentro do app. O uMS
// pede http://127.0.0.1:P/live.ts?s=N; o proxy olha a fonte de verdade:
//   - se ela ja e TS continuo (comeca com 0x47), responde 302 para ela — o
//     caminho que ja tocava continua igual, sem o app no meio;
//   - se e playlist HLS (o .m3u8, ou o .ts que o painel responde com
//     #EXTM3U), le a playlist ao vivo, baixa os segmentos em ordem (refresh
//     pelo target duration, sem repetir, perto da ponta) e entrega ao uMS UM
//     fluxo MPEG-TS continuo (video/mp2t, sem Content-Length), com PAT/PMT
//     antes do primeiro pacote de cada segmento.
// Uma sessao por vez: pedir uma URL nova encerra a anterior e cancela o
// download dela (a conta Xtream costuma ter 1 tela). Logs [proxy-ts] sem URL.
// GETs simultaneos do uMS compartilham uma ingestao, com buffer de ate 16MiB;
// fechar uma sonda nao interrompe outro leitor. Sem leitores, encerra apos uma
// folga de 3s para a reabertura do uMS (ou ao terminar o download em curso).
#ifndef NV_PROXYTS_H
#define NV_PROXYTS_H
#include <stddef.h>

// 1 nesta plataforma (webOS, .tpk e Mac; nao no navegador nem no Android).
int  proxyts_disponivel(void);
// Abre uma sessao para `fonte` e escreve em `saida` a URL local que o player
// deve tocar. 0 = o proxy nao subiu (use a fonte direto).
int  proxyts_url(const char *fonte, char *saida, size_t n);
// Encerra a sessao em curso (troca de canal, saida do player).
void proxyts_parar(void);
// A FONTE MARCADA "nvproxy:<url>" (como resolverCanalXtream monta a lista)
// vira a URL local de uma sessao nova; qualquer outra fonte encerra a sessao em
// curso e volta como veio. Chamar logo antes de cada video_tocar.
#define PROXYTS_PREFIXO "nvproxy:"
const char *proxyts_resolver(const char *url, char *buf, size_t n);
// 1 quando `url` e uma URL deste proxy.
int  proxyts_e_url(const char *url);
// CABECALHOS DA PROXIMA SESSAO (#283): as linhas "Nome: valor" que o addon
// exigiu (behaviorHints.proxyHeaders.request). O proxy as manda em todo pedido
// a fonte — sonda, playlist e segmentos —, o que o motor HLS do uMS nao faz nos
// pedidos de segmento. Vazio/NULL = nenhum. Vale para a proxima proxyts_url;
// chamar antes de cada fonte, como video_definir_cabecalhos.
void proxyts_definir_cabecalhos(const char *cabs);
// 1 quando a sessao em curso so redirecionou o player para a fonte (ela nao
// era playlist HLS): tocar a mesma fonte direto seria repetir a tentativa.
int  proxyts_redirecionou(void);
// 1 se `url` (sem o prefixo) e candidata ao proxy num canal de addon: http(s)
// e nao arquivo progressivo (.mp4/.mkv). Puro, testavel em qualquer alvo.
int  proxyts_candidata(const char *url, int mp4);

#endif
