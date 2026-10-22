// Rotulos das faixas a partir do cabecalho do MKV, para o player que so sabe o
// idioma (#206, .tpk da Samsung).
//
// O Tizen.Multimedia.Player (tizen-tpk/Video.cs) entrega por faixa so o
// GetLanguageCode: nem o Name da TrackEntry, nem a FlagForced, nem os canais.
// O .wgt mostra "forced" porque video_tizen.c le o MKV e poe o Name no rotulo;
// o .tpk nao lia. Este modulo e a regra, sem rede e sem estado, para o teste do
// Mac poder exercitar o que a TV desenha.
//
// CASAMENTO POR ORDINAL DENTRO DO TIPO, com a mesma guarda do video_tizen.c:
// aplica quando a QUANTIDADE bate entre o player e o arquivo. Desde o #269,
// duas saidas a mais, as duas medidas no log do .tpk: sem as legendas de
// imagem (PGS/VobSub, que o player nao lista) e, quando o player lista MENOS,
// pelo comeco se os idiomas concordarem. Fora disso, rotulo errado e pior que
// "Audio 1": fica como veio.
#ifndef NV_FAIXASMKV_H
#define NV_FAIXASMKV_H

#include "video.h"
#include "mkv.h"

// Reescreve idioma/rotulo/letreiro de `aud` e `leg` com as TrackEntry de `fx`.
// Nao mexe em `numero` (quem escolhe a faixa e o player). #269: a legenda
// casada ganha tambem `codec` (CodecID) e `ordinalMkv` = posicao dela entre as
// TrackEntry de legenda do ARQUIVO (contando as de imagem que o player
// esconde) — o ordinal que mkvass_iniciar_ordinal espera. Legenda sem par fica
// com o que tinha.
// Devolve quantas faixas mudaram de rotulo.
int faixasmkv_aplicar(VideoFaixa *aud, int nAud, VideoFaixa *leg, int nLeg,
                      const MkvFaixa *fx, int n);

// Fallback de listas diferentes: apenas idiomas unicos, indices em fx ou -1.
int faixasmkv_legendas_por_idioma(const VideoFaixa *leg, int nLeg,
                                 const MkvFaixa *fx, int n, int *mapa);

// "2.0", "5.1", "7.1" ou "" (mesma tabela do sourceInfo da LG, video.c).
const char *faixasmkv_canais(int canais);

// Nome curto do CodecID de audio ("A_EAC3" -> "E-AC3", "A_AAC/MPEG4/LC" ->
// "AAC"); "" quando nao conhece. Vai no rotulo da faixa de audio (#269).
const char *faixasmkv_codec(const char *codecId);

// #269: a faixa de legenda deste CodecID e desenhada pelo overlay do app,
// lida do MKV pelo mkvass, em vez do player da TV? ASS/SSA sempre (#92).
// Texto simples (S_TEXT/UTF8 = SRT, S_TEXT/WEBVTT) so quando `textoSimples`
// — LG e .tpk (#421); Android recebe texto do Media3.
// Imagem (PGS/VobSub) e desconhecido: nunca.
int faixasmkv_overlay(const char *codecId, int textoSimples);

#endif
