// LEGENDA TEMPORARIA (pura, sem SDL nem rede): a legenda que liga sozinha
// quando a TV fica muda ou quando a pessoa volta alguns segundos ("o que ele
// disse?"), e desliga sozinha quando o motivo acaba. Ideia do tvOS 18.
//
// So age quando NAO ha legenda ligada: quem ja assiste com legenda nao ve
// diferenca. O modulo decide QUANDO; quem liga/desliga e escolhe a faixa e
// faixas.c (faixas_legenda_temporaria), e quem alimenta e player.c.
//
//   MUDO    liga ao mutar, desliga ao voltar o som.
//   VOLTA   um recuo de LEGTEMP_VOLTA_MIN..MAX segundos liga ate o video
//           passar de novo do ponto de onde a pessoa saiu (+ uma folga para a
//           fala em curso terminar). Recuos seguidos somam; passando do teto
//           (ou pulando para a frente do ponto) a pessoa esta navegando, nao
//           perguntando "o que ele disse?", e o motivo cai.
#ifndef NV_LEGTEMP_H
#define NV_LEGTEMP_H

#define LEGTEMP_MUDO  1
#define LEGTEMP_VOLTA 2

#define LEGTEMP_VOLTA_MIN   2.0   // menos que isso e ajuste fino de posicao
#define LEGTEMP_VOLTA_MAX  45.0   // mais que isso e rever a cena, nao uma fala
#define LEGTEMP_VOLTA_FOLGA 2.0   // segundos alem do ponto de origem

enum { LEGTEMP_NADA = 0, LEGTEMP_LIGAR, LEGTEMP_DESLIGAR };

typedef struct {
  int motivos;      // LEGTEMP_MUDO | LEGTEMP_VOLTA em vigor
  int mudo;         // ultimo estado de mudo visto (0/1)
  double voltaFim;
  int longa;        // recuo passou do teto: fica fora ate voltaFim  // posicao em que o motivo VOLTA acaba
} LegTemp;

void legtemp_zerar(LegTemp *t);
// As tres entradas devolvem LEGTEMP_LIGAR quando o primeiro motivo nasce,
// LEGTEMP_DESLIGAR quando o ultimo cai, LEGTEMP_NADA no resto.
// `mudo` < 0 = a plataforma nao sabe: nada muda.
int legtemp_mudo(LegTemp *t, int mudo);
// Busca pedida pela pessoa, de `de` para `para` (segundos).
int legtemp_busca(LegTemp *t, double de, double para);
// A cada quadro, com a posicao do video.
int legtemp_quadro(LegTemp *t, double pos);

// Idiomas a tentar, em ordem, para a legenda temporaria: a preferencia em
// vigor (`pref`: Ajustes ou a ultima escolha a mao), o idioma do app (`ui`) e
// por fim o do audio que toca (legenda na lingua da fala ainda responde "o que
// ele disse?"). Vazios, "none" e "und" ficam de fora, repetidos tambem.
// Devolve quantos gravou em `out` (0..3).
int legtemp_idiomas(const char *pref, const char *ui, const char *audio, const char *out[3]);
#endif
