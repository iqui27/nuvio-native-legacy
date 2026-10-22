// FONTE QUE NAO E VIDEO (2.0.2). Os logs da 2.0.1 mostraram 29 pessoas em que
// o automatico escolheu a linha de AVISO de um addon ("support the project!",
// "Donation needed", "Join the Discord", "Unavailable") cuja URL devolve uma
// pagina HTML ou JSON; o player falhava (Android 2000/2004/3003/4003, webOS
// "Playing error", Samsung prepare 0xffffffff) depois de segundos perdidos.
// Duas regras puras, sem rede nem lista de streams (teste: tests/naovideo.c):
//   naovideo_mime : a resposta FINAL da sonda que ja existe (rede_url_final_tipo)
//                   diz que nao e video;
//   naovideo_nome : o NOME parece linha de aviso do addon, sem nenhum dado de
//                   arquivo de video.
#ifndef NV_NAOVIDEO_H
#define NV_NAOVIDEO_H

// 1 = a resposta nao e video. `mime` em minusculo e sem parametros (vazio =
// o servidor nao disse), `url` e a final, `corpo` o tamanho do corpo quando o
// 200 veio inteiro (-1 = desconhecido).
int naovideo_mime(const char *mime, const char *url, long corpo);

// 1 = o nome (rotulo + descricao) e linha informativa do addon. Exige ausencia
// de resolucao, tamanho e de qualquer token de video no texto: um filme chamado
// "Discord" tem 1080p/x265/GB na descricao, e um aviso nao.
int naovideo_nome(const char *rotulo, const char *descricao, int altura, long tamanhoMB);

#endif
