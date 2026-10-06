// WATCH TOGETHER — a fonte de cada um (plano, 6.9).
//
// Ninguem passa link de ninguem: os links de debrid sao da conta da pessoa e
// muitas vezes presos ao IP. A sala carrega so a IMPRESSAO DIGITAL do release
// que o anfitriao escolheu (juntosproto.h, JsImpressao), e cada convidado
// procura esse mesmo release nos PROPRIOS addons, nesta ordem de forca:
//
//   1. infoHash + fileIdx                       -> mesma
//   2. tamanho exato em bytes + nome do arquivo -> mesma
//   3. bingeGroup + altura + nome parecido      -> parecida (mesmo addon de outra conta)
//   4. mesma altura (ou a mais perto), primeira da lista -> parecida
//   nada na lista                               -> nenhuma
//
// Depois que o video carrega, a DURACAO decide: mais de 2 s de diferenca vira
// "diferente" (pode desalinhar em cenas); menos de metade da duracao da sala e
// o MP4 de aviso de 120 s de algum addon, e conta como "nenhuma". A razao
// 25/23,976 (lancamento PAL acelerado) e marcada para o aviso; o ajuste de
// taxa por membro fica para a v2.
//
// A decisao do dono (06/10, decisao 1): parecida e aceita, com aviso; o
// veredito NUNCA bloqueia a entrada.
//
// Puro (sem rede, sem SDL alem do tipo Stream): tests/juntosfonte.c.
#ifndef NV_JUNTOSFONTE_H
#define NV_JUNTOSFONTE_H
#include "juntosproto.h"
#include "streams.h"

typedef enum { JSF_NENHUMA = 0, JSF_DIFERENTE, JSF_PARECIDA, JSF_MESMA } JsVeredito;

// Impressao de uma fonte (a do anfitriao). `durS` 0 = ainda desconhecida.
void jsf_impressao(const Stream *s, double durS, JsImpressao *out);
// "1080p WEB-DL", "2160p REMUX": a linha da fonte no cartao da sala.
void jsf_rotulo(const Stream *s, char *dst, size_t n);

// Procura a impressao da sala na lista do convidado. Devolve o veredito e, em
// *indice, a fonte escolhida (-1 em JSF_NENHUMA). `ignorar` (pode ser NULL)
// marca indices ja tentados que falharam (reserva no meio, fontevolta).
JsVeredito jsf_casar(const JsImpressao *sala, const Stream *lista, int n, const unsigned char *ignorar, int *indice);

// Depois do load: corrige o veredito pela duracao. `*pal` = 1 quando a razao
// e a de um lancamento PAL acelerado (25/23,976).
JsVeredito jsf_duracao(JsVeredito v, long long salaDurMs, double minhaDurS, int *pal);

const char *jsf_nome(JsVeredito v);   // "mesma" "parecida" "diferente" "nenhuma"

#endif
