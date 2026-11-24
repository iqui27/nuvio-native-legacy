// Maquina de confirmacao do conserto do Farol (ADENDO 2.1 C, item 33).
// Compila src/farolconserto.c e src/farolanel.c REAIS (sem SDL, sem rede). O
// relogio do anel e o passado a farol_conserto_* sao o MESMO relogio falso:
// cada teste emite eventos no anel real com t_ms controlado e avanca o passo.
//
// farol_registrar_resultado/consertou sao definidos AQUI com o mesmo corpo de
// src/farol.c (que arrasta metade do app para o link); farol_conserto.sh
// confere por grep que o src/farol.c continua gravando esses mesmos eventos.
#include "farolconserto.h"
#include "farolanel.h"
#include "farolregra.h"
#include "faroltexto.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int falhas;
#define CK(c) do { if (!(c)) { fprintf(stderr, "FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

static uint32_t g_t;
static uint32_t relogio(void) { return g_t; }

void farol_registrar_resultado(int acao, int ok) {
  farol_ev(FM_FAROL, FE_FAROL_RESULTADO, ok ? 0 : 1, acao, ok ? 1 : 0, "r");
}
void farol_registrar_consertou(int acao) {
  farol_ev(FM_FAROL, FE_FAROL_CONSERTOU, 0, acao, 1, "c");
}

#define T0 100000u
static void emite(uint32_t t, int ev, int32_t a) {
  g_t = t;
  farol_ev(FM_PLAYER, ev, ev == FE_PRONTO ? 0 : 2, a, 0, "t");
}
static int contaEv(int ev, int a) {
  FarolEvento e[FAROL_ANEL_N];
  int i, n = farolanel_ler(e, FAROL_ANEL_N, 0), k = 0;
  for (i = 0; i < n; i++) if (e[i].ev == ev && (a < 0 || e[i].a == a)) k++;
  return k;
}
static int contaResultadoFalho(int acao) {
  FarolEvento e[FAROL_ANEL_N];
  int i, n = farolanel_ler(e, FAROL_ANEL_N, 0), k = 0;
  for (i = 0; i < n; i++)
    if (e[i].ev == FE_FAROL_RESULTADO && e[i].a == acao && e[i].b == 0) k++;
  return k;
}
static void zera(FarolConserto *c) {
  farolanel_zerar();
  farol_conserto_cancelar(c);
  g_t = T0;
}
// Fronteira lida ANTES da acao; o iniciar fica no relogio atual.
static int inicia(FarolConserto *c, int acao, int alt) {
  uint64_t f = farolanel_seq();
  return farol_conserto_iniciar(c, acao, alt, g_t, f);
}
static int passo(FarolConserto *c, uint32_t t) { g_t = t; return farol_conserto_passo(c, t); }

#define A1 FAROL_A_TENTAR_DE_NOVO
#define A2 FAROL_A_PROXIMA_FONTE

static void t_a_sucesso(void) {
  FarolConserto c = {0};
  zera(&c);
  CK(inicia(&c, A1, 0) == 1);
  CK(farol_conserto_estado(&c) == FCS_OBSERVANDO);
  CK(strcmp(farol_conserto_texto(&c), FT_OBSERVANDO_VIDEO) == 0);
  emite(T0 + 100, FE_ABRIR, 1);
  farol_ev(FM_REDE, FE_REDE_ESTADO, 0, 0, 0, "x");           // ruido de outro modulo
  emite(T0 + 1200, FE_PRONTO, 1200);
  CK(passo(&c, T0 + 1300) == FCP_NADA);
  CK(passo(&c, T0 + 6199) == FCP_NADA);
  CK(contaEv(FE_FAROL_CONSERTOU, -1) == 0);
  CK(passo(&c, T0 + 6200) == FCP_CONSERTOU);               // exatos 5000 ms
  CK(contaEv(FE_FAROL_CONSERTOU, A1) == 1);
  CK(farol_conserto_estado(&c) == FCS_CONSERTOU);
  CK(strcmp(farol_conserto_texto(&c), FT_VOLTOU_A_TOCAR) == 0);
  CK(farol_conserto_acoes(&c) == 0);                        // falha encerrada
  CK(passo(&c, T0 + 9000) == FCP_NADA);                     // uma vez so
  CK(passo(&c, T0 + 90000) == FCP_NADA);
  CK(contaEv(FE_FAROL_CONSERTOU, -1) == 1);
  CK(contaEv(FE_FAROL_RESULTADO, -1) == 0);
  // Consertada, uma falha nova comeca do zero (teto nao herdado).
  CK(inicia(&c, A2, 0) == 1);
  CK(farol_conserto_acoes(&c) == 1);
}

static void t_fronteira(void) {
  FarolConserto c = {0};
  zera(&c);
  // Eventos de ANTES da acao: um PRONTO antigo nao confirma, um erro antigo
  // nao reprova.
  emite(T0 - 9000, FE_PRONTO, 1);
  emite(T0 - 100, FE_ERRO_FONTE, 7);
  g_t = T0;
  CK(inicia(&c, A1, A2) == 1);
  CK(passo(&c, T0 + 6000) == FCP_NADA);                     // sem PRONTO novo
  CK(passo(&c, T0 + 19999) == FCP_NADA);
  CK(passo(&c, T0 + 20000) == FCP_EXECUTAR_ALT);            // so o prazo
  // Mesmo ao contrario: erro antigo + PRONTO novo = conserto.
  zera(&c);
  emite(T0 - 100, FE_ERRO_NATIVO, 7);
  g_t = T0;
  CK(inicia(&c, A1, A2) == 1);
  emite(T0 + 500, FE_PRONTO, 500);
  CK(passo(&c, T0 + 5500) == FCP_CONSERTOU);
  // Evento da propria acao (PRONTO rapido) tem de ser visto: fronteira lida
  // antes da acao, iniciar depois.
  zera(&c);
  {
    uint64_t f = farolanel_seq();
    emite(T0 + 10, FE_PRONTO, 10);                          // a acao ja produziu
    CK(farol_conserto_iniciar(&c, A1, 0, T0 + 20, f) == 1);
  }
  CK(passo(&c, T0 + 5010) == FCP_CONSERTOU);
}

static void t_b_bordas(void) {
  FarolConserto c = {0};
  // PRONTO aos 15000: os 5 s fecham exatamente aos 20000 -> ainda vale.
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 15000, FE_PRONTO, 1);
  CK(passo(&c, T0 + 19999) == FCP_NADA);
  CK(passo(&c, T0 + 20000) == FCP_CONSERTOU);
  // PRONTO aos 15001: nao fecha dentro do prazo -> falha aos 20000.
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 15001, FE_PRONTO, 1);
  CK(passo(&c, T0 + 19999) == FCP_NADA);
  CK(passo(&c, T0 + 20000) == FCP_ESCALAR);
  CK(contaEv(FE_FAROL_CONSERTOU, -1) == 0);
  // PRONTO aos 18000 + 5 s andando: estoura o prazo total -> falha, mesmo
  // sondando tarde (23000) sem nenhum erro.
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 18000, FE_PRONTO, 1);
  CK(passo(&c, T0 + 23000) == FCP_ESCALAR);
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 18000, FE_PRONTO, 1);
  CK(passo(&c, T0 + 20000) == FCP_ESCALAR);
  // Sem nenhum evento: vence aos 20000 exatos.
  zera(&c); inicia(&c, A1, 0);
  CK(passo(&c, T0 + 19999) == FCP_NADA);
  CK(passo(&c, T0 + 20000) == FCP_ESCALAR);
  // Erro 2 s depois do PRONTO (recaida antes dos 5 s).
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 1000, FE_PRONTO, 1);
  emite(T0 + 3000, FE_ERRO_FONTE, 3);
  CK(passo(&c, T0 + 3000) == FCP_ESCALAR);
  // Erro em 4999 ms de caminhada: recaida. Em 5000: ja estava confirmado.
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 1000, FE_PRONTO, 1);
  emite(T0 + 5999, FE_ERRO_NATIVO, 3);
  CK(passo(&c, T0 + 6000) == FCP_ESCALAR);
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 1000, FE_PRONTO, 1);
  emite(T0 + 6000, FE_ERRO_NATIVO, 3);
  CK(passo(&c, T0 + 6000) == FCP_CONSERTOU);
  // Sondagem tardia: o erro DEPOIS dos 5 s nao desfaz a confirmacao.
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 1000, FE_PRONTO, 1);
  emite(T0 + 9000, FE_PIPELINE_MORREU, 40);
  CK(passo(&c, T0 + 30000) == FCP_CONSERTOU);
  // Erro antes do PRONTO tambem reprova.
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 3000, FE_ERRO_FONTE, 3);
  CK(passo(&c, T0 + 3000) == FCP_ESCALAR);
  // Pipeline morreu / erro nativo apos PRONTO.
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 500, FE_PRONTO, 1);
  emite(T0 + 900, FE_PIPELINE_MORREU, 40);
  CK(passo(&c, T0 + 1000) == FCP_ESCALAR);
  // PARADO: curto e ignorado, prolongado reprova.
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 500, FE_PRONTO, 1);
  emite(T0 + 1500, FE_PARADO, FAROL_CONSERTO_PARADO_MS - 1);
  CK(passo(&c, T0 + 1600) == FCP_NADA);
  CK(passo(&c, T0 + 5500) == FCP_CONSERTOU);
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 500, FE_PRONTO, 1);
  emite(T0 + 1500, FE_PARADO, FAROL_CONSERTO_PARADO_MS);
  CK(passo(&c, T0 + 1600) == FCP_ESCALAR);
  // Novo PRONTO (reabriu) recomeca a contagem dos 5 s.
  zera(&c); inicia(&c, A1, 0);
  emite(T0 + 1000, FE_PRONTO, 1);
  emite(T0 + 4000, FE_PRONTO, 1);
  CK(passo(&c, T0 + 6000) == FCP_NADA);
  CK(passo(&c, T0 + 8999) == FCP_NADA);
  CK(passo(&c, T0 + 9000) == FCP_CONSERTOU);
  // Anel com mais de 256 eventos entre sondagens: so o que ficou conta.
  {
    int i;
    zera(&c); inicia(&c, A1, 0);
    for (i = 0; i < 300; i++) emite(T0 + 10 + (uint32_t)i, FE_ABRIR, i);
    emite(T0 + 400, FE_PRONTO, 1);
    CK(passo(&c, T0 + 5400) == FCP_CONSERTOU);
  }
  // Virada do relogio de 32 bits no meio da observacao.
  {
    uint32_t base = 0xFFFFF000u;
    farolanel_zerar(); farol_conserto_cancelar(&c);
    g_t = base; inicia(&c, A1, 0);
    emite(base + 1000, FE_PRONTO, 1);                       // cruza 2^32
    CK(passo(&c, base + 5999) == FCP_NADA);
    CK(passo(&c, base + 6000) == FCP_CONSERTOU);
  }
}

static void t_c_alt(void) {
  FarolConserto c = {0};
  zera(&c);
  CK(inicia(&c, A1, A2) == 1);
  emite(T0 + 2000, FE_ERRO_FONTE, 3);
  CK(passo(&c, T0 + 2000) == FCP_EXECUTAR_ALT);
  CK(farol_conserto_acao(&c) == A2);
  CK(farol_conserto_estado(&c) == FCS_OBSERVANDO);
  CK(farol_conserto_acoes(&c) == 2);
  CK(contaResultadoFalho(A1) == 1);
  CK(contaEv(FE_FAROL_CONSERTOU, -1) == 0);
  CK(passo(&c, T0 + 2100) == FCP_NADA);                     // erro velho nao reaparece
  emite(T0 + 3000, FE_PRONTO, 1);
  CK(passo(&c, T0 + 7999) == FCP_NADA);
  CK(passo(&c, T0 + 8000) == FCP_CONSERTOU);
  CK(contaEv(FE_FAROL_CONSERTOU, A2) == 1);
  CK(contaEv(FE_FAROL_CONSERTOU, A1) == 0);
  // Prazo vencido da 1a tambem dispara a alternativa; o da 2a conta do rearme.
  zera(&c); inicia(&c, A1, A2);
  CK(passo(&c, T0 + 20000) == FCP_EXECUTAR_ALT);
  CK(passo(&c, T0 + 39999) == FCP_NADA);                    // 20000 apos o rearme
  emite(T0 + 21000, FE_PRONTO, 1);
  CK(passo(&c, T0 + 26000) == FCP_CONSERTOU);
}

static void t_d_teto(void) {
  FarolConserto c = {0};
  zera(&c);
  CK(inicia(&c, A1, A2) == 1);
  emite(T0 + 1000, FE_ERRO_FONTE, 3);
  CK(passo(&c, T0 + 1000) == FCP_EXECUTAR_ALT);
  emite(T0 + 2000, FE_ERRO_NATIVO, 3);
  CK(passo(&c, T0 + 2000) == FCP_ESCALAR);
  CK(farol_conserto_estado(&c) == FCS_ESCALOU);
  CK(farol_conserto_texto(&c) == NULL);
  CK(contaResultadoFalho(A1) == 1);
  CK(contaResultadoFalho(A2) == 1);
  CK(contaEv(FE_FAROL_CONSERTOU, -1) == 0);
  // Nunca uma 3a acao: iniciar na mesma falha e recusado, sempre.
  CK(inicia(&c, A1, A2) == 0);
  CK(inicia(&c, FAROL_A_LIMPAR_CACHE, 0) == 0);
  CK(farol_conserto_estado(&c) == FCS_ESCALOU);
  CK(farol_conserto_acoes(&c) == 2);
  CK(passo(&c, T0 + 90000) == FCP_NADA);                    // escala uma vez so
  CK(passo(&c, T0 + 99999) == FCP_NADA);
  CK(contaResultadoFalho(A2) == 1);
  // Teto vale pela estrutura, nao pela chamada: 3 iniciar seguidos.
  zera(&c);
  CK(inicia(&c, A1, A2) == 1);
  CK(inicia(&c, A1, A2) == 1);                              // 2a acao da mesma falha
  CK(inicia(&c, A1, A2) == 0);                              // 3a: recusada
  CK(farol_conserto_acoes(&c) == 2);
  CK(farol_conserto_estado(&c) == FCS_OBSERVANDO);          // a 2a segue observada
  CK(passo(&c, T0 + 20000) == FCP_ESCALAR);                 // sem alt na 2a
  CK(inicia(&c, A2, 0) == 0);
  // So o cancelar libera uma falha nova.
  farol_conserto_cancelar(&c);
  CK(inicia(&c, A2, 0) == 1);
  CK(farol_conserto_acoes(&c) == 1);
}

static void t_e_sem_alt(void) {
  FarolConserto c = {0};
  int i;
  int altsInvalidas[3];
  altsInvalidas[0] = FAROL_A_NENHUMA;
  altsInvalidas[1] = FAROL_A_ENVIAR_REGISTRO;
  altsInvalidas[2] = FAROL_A_N + 3;
  for (i = 0; i < 3; i++) {
    zera(&c);
    CK(inicia(&c, A1, altsInvalidas[i]) == 1);
    emite(T0 + 1000, FE_ERRO_FONTE, 3);
    CK(passo(&c, T0 + 1000) == FCP_ESCALAR);
    CK(farol_conserto_estado(&c) == FCS_ESCALOU);
    CK(contaResultadoFalho(A1) == 1);
    CK(contaEv(FE_FAROL_ACAO, -1) == 0);
    CK(passo(&c, T0 + 50000) == FCP_NADA);
  }
  zera(&c);                                                 // alt igual a acao = sem alt
  CK(inicia(&c, A1, A1) == 1);
  CK(passo(&c, T0 + 20000) == FCP_ESCALAR);
  // Argumentos invalidos nao armam nada.
  zera(&c);
  CK(farol_conserto_iniciar(NULL, A1, 0, T0, 0) == 0);
  CK(inicia(&c, FAROL_A_NENHUMA, 0) == 0);
  CK(inicia(&c, FAROL_A_ENVIAR_REGISTRO, 0) == 0);
  CK(inicia(&c, FAROL_A_N, 0) == 0);
  CK(inicia(&c, -1, 0) == 0);
  CK(farol_conserto_estado(&c) == FCS_OCIOSO);
  CK(passo(&c, T0 + 90000) == FCP_NADA);
  CK(farol_conserto_passo(NULL, T0) == FCP_NADA);
  CK(farol_conserto_texto(NULL) == NULL);
}

static void t_f_cancela(void) {
  FarolConserto c = {0};
  int n;
  // Voltar no meio da observacao: nada de CONSERTOU nem escalada tardia.
  zera(&c);
  CK(inicia(&c, A1, A2) == 1);
  emite(T0 + 500, FE_PRONTO, 1);
  farol_conserto_cancelar(&c);
  CK(farol_conserto_estado(&c) == FCS_OCIOSO);
  CK(passo(&c, T0 + 6000) == FCP_NADA);
  CK(contaEv(FE_FAROL_CONSERTOU, -1) == 0);
  CK(contaEv(FE_FAROL_RESULTADO, -1) == 0);
  // Erro depois do cancelar + prazo vencido: continua mudo.
  emite(T0 + 7000, FE_ERRO_FONTE, 3);
  CK(passo(&c, T0 + 60000) == FCP_NADA);
  CK(contaEv(FE_FAROL_RESULTADO, -1) == 0);
  CK(farol_conserto_texto(&c) == NULL);
  // Troca de titulo: o erro do titulo antigo, anterior a nova fronteira, nao
  // contamina a observacao nova.
  zera(&c);
  inicia(&c, A1, A2);
  emite(T0 + 1000, FE_ERRO_FONTE, 3);
  farol_conserto_cancelar(&c);
  n = contaEv(FE_FAROL_RESULTADO, -1);
  CK(inicia(&c, A1, A2) == 1);
  emite(T0 + 2000, FE_PRONTO, 1);
  CK(passo(&c, T0 + 7000) == FCP_CONSERTOU);
  CK(contaEv(FE_FAROL_RESULTADO, -1) == n);
  // Cancelar depois de escalar libera uma falha nova, com teto zerado.
  zera(&c);
  inicia(&c, A1, 0);
  CK(passo(&c, T0 + 20000) == FCP_ESCALAR);
  farol_conserto_cancelar(&c);
  CK(farol_conserto_acoes(&c) == 0);
  CK(inicia(&c, A1, A2) == 1);
  CK(farol_conserto_acoes(&c) == 1);
  // Cancelar a 2a acao tambem e mudo.
  zera(&c);
  inicia(&c, A1, A2);
  emite(T0 + 1000, FE_ERRO_FONTE, 3);
  CK(passo(&c, T0 + 1000) == FCP_EXECUTAR_ALT);
  farol_conserto_cancelar(&c);
  CK(passo(&c, T0 + 90000) == FCP_NADA);
  CK(contaEv(FE_FAROL_CONSERTOU, -1) == 0);
  CK(contaResultadoFalho(A2) == 0);
  farol_conserto_cancelar(NULL);                            // nao cai
}

int main(void) {
  farolanel_relogio(relogio);
  g_t = T0;
  t_a_sucesso();
  t_fronteira();
  t_b_bordas();
  t_c_alt();
  t_d_teto();
  t_e_sem_alt();
  t_f_cancela();
  if (falhas) { fprintf(stderr, "farol_conserto: %d falha(s)\n", falhas); return 1; }
  puts("farol_conserto: asserts ok");
  return 0;
}
