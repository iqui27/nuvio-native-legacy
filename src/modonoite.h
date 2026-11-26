// MODO NOITE / CLAREZA DE DIALOGO (2.1, pesquisa de mercado secao 3; Stremio
// #313, NuvioTV #3424): o dialogo baixo e a explosao alta. Ajuste "Modo noite"
// (Desligado de fabrica; Suave; Forte), LOCAL desta TV.
//
// O QUE CADA ALVO CONSEGUE FAZER (modonoite_capacidade):
//   - Android: o PCM decodificado passa pelo app (Media3 AudioProcessor, como
//     o reforco de volume do F07). La o modo PROCESSA o som: sobe o canal
//     central, baixa surround e LFE e comprime a faixa dinamica. A conta mora
//     aqui (modonoite_dsp, a referencia testada em tests/modonoite.c) e em
//     NoiteMath.kt, que a repete linha a linha. Com passthrough/bitstream as
//     amostras nao passam pelo app e o modo nao age (como o reforco).
//   - LG (uMS) e Samsung (AVPlay/.tpk/.wgt): o audio vai direto ao decodificador
//     da TV; o app nao toca nas amostras. O modo so PREFERE A FAIXA ESTEREO do
//     mesmo idioma (audioescolha.h, audioesc_estereo), quando o arquivo tem
//     uma: o downmix do estudio costuma trazer o dialogo mais presente que o
//     5.1 misturado pela TV. Nao comprime nada; o modo noturno da propria TV
//     continua sendo o ajuste dela.
//
// Regra pura: sem SDL, sem JNI, sem rede.
#ifndef NV_MODONOITE_H
#define NV_MODONOITE_H

enum { MN_DESLIGADO = 0, MN_SUAVE, MN_FORTE, MN_N };
enum { MN_CAP_ESTEREO = 0, MN_CAP_PROCESSA };

// O que este build faz com o modo (Android processa; os outros preferem estereo).
int modonoite_capacidade(void);
// Testes/capturas: finge a capacidade (MN_CAP_*); -1 = a do build.
void modonoite_simular_capacidade(int c);

// Os numeros de cada nivel. Desligado (ou fora da faixa) = tudo neutro.
typedef struct {
  float limiarDb;     // acima dele a compressao age
  float razao;        // N:1
  float compensaDb;   // ganho fixo: o que sobe o trecho baixo
  float centroDb;     // canal central (dialogo)
  float surroundDb;   // traseiros e laterais
  float lfeDb;        // subwoofer
  float ataqueMs, soltaMs;
} MnParam;
MnParam modonoite_param(int nivel);

// A curva estatica: ganho em dB para um nivel de entrada em dBFS.
double modonoite_ganho_db(int nivel, double entradaDb);

// O papel do canal `i` num quadro de `canais` na ordem do Android/Media3
// (FL FR FC LFE BL BR SL SR). Sem central (1, 2 e 4 canais) tudo e frente.
enum { MN_FRENTE = 0, MN_CENTRO, MN_LFE, MN_SURROUND };
int modonoite_canal(int canais, int i);

// A referencia do processamento: PCM float intercalado (-1..1), no lugar.
#define MN_CANAIS_MAX 8
typedef struct {
  int nivel, canais;
  float ganhoCanal[MN_CANAIS_MAX];
  float ataque, solta;      // coeficientes por quadro
  float env;                // envoltoria de pico (linear)
  MnParam p;
} MnDsp;
// 1 quando ha o que processar (nivel ligado, 1..8 canais, taxa valida).
int  modonoite_dsp_iniciar(MnDsp *d, int nivel, int canais, int taxaHz);
void modonoite_dsp(MnDsp *d, float *pcm, int quadros);

// O nivel em vigor para a PROXIMA abertura (player.c passa o do ajuste). Avisa
// o backend (Android) e a ordem do audio (audioesc_preferir_estereo).
void modonoite_armar(int nivel);
int  modonoite_nivel(void);
// Implementado em video_android.c; nos outros alvos, nada.
void modonoite_backend_nivel(int nivel);
#endif
