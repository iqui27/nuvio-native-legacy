// A cota de catalogos por addon escolhe pela ESCOLHA da pessoa, e nao pela
// posicao no manifesto (issue #126).
//
// O relato, pelo log da LG 50NANO75SPA: "[desc] Ultra MAX: 32 catalogo(s)
// declarado(s) (cota 32, manifesto tem 174 — 142 de fora por cota)". O que a
// pessoa queria na home estava entre os 142 e nenhum ajuste o trazia.
//
//   bash tests/cotacat.sh
#include "cotacat.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void confere(const char *o_que, int obtido, int esperado) {
  int ok = obtido == esperado;
  printf("  %-60s %s (obtido %d, esperado %d)\n", o_que, ok ? "ok   " : "FALHOU",
         obtido, esperado);
  if (!ok) falhas++;
}

#define N_ULTRA 174
#define COTA    32

static CotaPrio pr[N_ULTRA];
static char esc[N_ULTRA];

static void semEscolha(void) {
  int i;
  for (i = 0; i < N_ULTRA; i++) { pr[i].nivel = COTA_MANIFESTO; pr[i].pos = 0; }
}
static int marcados(void) {
  int i, k = 0;
  for (i = 0; i < N_ULTRA; i++) k += esc[i] ? 1 : 0;
  return k;
}

int main(void) {
  int prom;

  printf("sem escolha nenhuma, a regra de antes (os primeiros do manifesto):\n");
  semEscolha();
  prom = cota_escolher(pr, N_ULTRA, COTA, esc);
  confere("marca exatamente a cota", marcados(), COTA);
  confere("o primeiro entra", esc[0], 1);
  confere("o ultimo da cota entra", esc[COTA - 1], 1);
  confere("o seguinte fica de fora", esc[COTA], 0);
  confere("ninguem promovido", prom, 0);

  printf("\nna ordem da CONTA, alem da posicao 32 do manifesto (#126):\n");
  semEscolha();
  pr[150].nivel = COTA_ORDEM_CONTA; pr[150].pos = 0;
  pr[100].nivel = COTA_ORDEM_CONTA; pr[100].pos = 1;
  prom = cota_escolher(pr, N_ULTRA, COTA, esc);
  confere("o 151o do manifesto, primeiro da conta, entra", esc[150], 1);
  confere("o 101o, segundo da conta, entra", esc[100], 1);
  confere("a cota continua sendo 32", marcados(), COTA);
  confere("quem cede vaga e o ULTIMO sem escolha", esc[COTA - 1], 0);
  confere("o penultimo sem escolha fica", esc[COTA - 3], 1);
  confere("dois promovidos", prom, 2);

  printf("\nescolhido NA TV vence a ordem da conta:\n");
  semEscolha();
  { int i;
    for (i = 0; i < 40; i++) { pr[100 + i].nivel = COTA_ORDEM_CONTA; pr[100 + i].pos = i; } }
  pr[170].nivel = COTA_ESCOLHIDO_TV; pr[170].pos = 5;
  cota_escolher(pr, N_ULTRA, COTA, esc);
  confere("o escolhido na TV entra", esc[170], 1);
  confere("os 31 primeiros da conta entram", esc[100] && esc[130], 1);
  confere("o 32o da conta cede a vaga", esc[131], 0);
  confere("nenhum sem escolha entra com a conta cheia", esc[0], 0);

  printf("\ndesligado so fica com a vaga que sobrar:\n");
  semEscolha();
  { int i; for (i = 0; i < 10; i++) pr[i].nivel = COTA_DESLIGADO; }
  cota_escolher(pr, N_ULTRA, COTA, esc);
  confere("o primeiro do manifesto, desligado, sai", esc[0], 0);
  confere("e o 42o, ligado, entra no lugar", esc[41], 1);
  confere("o 43o nao", esc[42], 0);

  printf("\ncabem todos:\n");
  semEscolha();
  prom = cota_escolher(pr, 20, COTA, esc);
  confere("todos marcados", esc[0] && esc[19], 1);
  confere("sem promovidos", prom, 0);

  // VAGA GARANTIDA (#37, restrita em 24/09). Janela de 4; addon 0 com tres
  // candidatos na frente, addon 1 com um, addon 2 so no fim (posicoes 5 e 6).
  // Candidatos: c0..c3 do addon 0 e 1 na janela, c4 do 0, c5 e c6 do addon 2.
  printf("\nvaga garantida so para addon novo:\n");
  { static const int addonDe[7] = { 0, 0, 1, 0, 0, 2, 2 };
    char desl[7];
    CotaAddon ad[3];
    int ordem[7], va[4], vc[4], k, dadas;
#define REINICIA() do { for (k = 0; k < 7; k++) ordem[k] = k; memset(desl, 0, sizeof desl); \
      ad[0].ativo = ad[1].ativo = ad[2].ativo = 1; ad[0].novo = ad[1].novo = 0; ad[2].novo = 1; } while (0)

    REINICIA();
    dadas = cota_vaga_garantida(ordem, 7, addonDe, desl, ad, 3, 4, 0, va, vc, 4);
    confere("addon novo sem ordem propria ganha uma vaga", dadas, 1);
    confere("entra a primeira dele (c5)", ordem[3], 5);
    confere("quem cede e a segunda fileira do addon 0 (c3 sai da janela)", ordem[4], 3);
    confere("o log diz o addon", va[0], 2);
    confere("e o candidato", vc[0], 5);
    confere("a unica do addon 1 fica", ordem[2], 2);

    REINICIA();
    dadas = cota_vaga_garantida(ordem, 7, addonDe, desl, ad, 3, 4, 1, va, vc, 4);
    confere("com ORDEM PROPRIA nao ha vaga garantida", dadas, 0);
    confere("e a ordem fica intacta", ordem[3] == 3 && ordem[5] == 5, 1);

    REINICIA();
    ad[2].novo = 0;   // o perfil ja viu o addon, ou tirou fileira dele
    dadas = cota_vaga_garantida(ordem, 7, addonDe, desl, ad, 3, 4, 0, va, vc, 4);
    confere("addon ja visto (ou com fileira tirada) nao ganha vaga", dadas, 0);

    REINICIA();
    ad[2].ativo = 0;  // desligado na conta
    dadas = cota_vaga_garantida(ordem, 7, addonDe, desl, ad, 3, 4, 0, va, vc, 4);
    confere("addon desligado na conta nao ganha vaga", dadas, 0);

    REINICIA();
    desl[5] = desl[6] = 1;   // todas as fileiras dele desligadas
    dadas = cota_vaga_garantida(ordem, 7, addonDe, desl, ad, 3, 4, 0, va, vc, 4);
    confere("addon com todas as fileiras desligadas continua fora", dadas, 0);
    confere("nenhuma desligada entra na janela", ordem[3], 3);

    REINICIA();
    desl[5] = 1;             // a primeira desligada, a segunda nao
    dadas = cota_vaga_garantida(ordem, 7, addonDe, desl, ad, 3, 4, 0, va, vc, 4);
    confere("a vaga pula a desligada e leva a ligada (c6)", dadas == 1 && ordem[3] == 6, 1);
#undef REINICIA
  }

  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
