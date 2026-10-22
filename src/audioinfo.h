// Codec e canais da faixa de audio como TEXTO ("E-AC-3 5.1", "TrueHD 7.1
// Atmos", "DTS-HD MA 5.1"), #293. Codigo puro, sem estado: o player e a folha
// de Audio chamam na troca de faixa, nunca por quadro, e o teste do Mac
// exercita a tabela inteira.
#ifndef NV_AUDIOINFO_H
#define NV_AUDIOINFO_H
#include <stddef.h>

// Nome amigavel do codec a partir de QUALQUER das grafias que as plataformas
// entregam: CodecID do Matroska ("A_EAC3", "A_DTS/LOSSLESS"), nome do uMS da
// LG ("EAC3", "TRUEHD"), nome do FFmpeg ("eac3", "pcm_s16le") e mime do
// ExoPlayer ("audio/eac3-joc", "audio/vnd.dts.hd"). "" quando nao conhece:
// quem chama esconde em vez de chutar.
const char *audioinfo_codec(const char *codec);

// "2.0", "5.1", "7.1" (e "1.0", "3.0", "4.0", "5.0", "6.1", "9.1") a partir do
// numero de canais; "" para 0 ou numero estranho.
const char *audioinfo_canais(int canais);

// 1 se o codec e Atmos por si so (mime "audio/eac3-joc").
int audioinfo_codec_atmos(const char *codec);

// "<codec> <canais>[ Atmos]". `atmos` soma ao que o codec ja diz. Devolve o
// comprimento escrito; 0 (e out vazio) quando o codec e desconhecido.
int audioinfo_texto(const char *codec, int canais, int atmos, char *out, size_t n);

#endif
