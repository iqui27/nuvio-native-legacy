// Farol na Ilha (issue 20): a politica de silencio com relogio injetado.
// Ver src/farolilha.h. Sem SDL, sem disco, sem rede.
#include "farolilha.h"
#include "farolregra.h"
#include "faroltexto.h"
#include <stdio.h>
#include <string.h>

static long long seg = 1800000000LL;
static unsigned ms = 1000u;
static long long relSeg(void) { return seg; }
static unsigned relMs(void) { return ms; }

static int falhas;
#define CHECA(c, msg) do { if (!(c)) { printf("FALHOU: %s (linha %d)\n", msg, __LINE__); falhas++; } \
                           else printf("ok: %s\n", msg); } while (0)

static FarolIlhaFatos fatos(int f, int p, int player, int video, const char *t, int cod) {
  FarolIlhaFatos x;
  memset(&x, 0, sizeof x);
  x.falhasSeguidas = f; x.paradas5min = p; x.playerAberto = player; x.videoAndando = video;
  x.titulo = t; x.codigo = cod;
  return x;
}

static void reiniciar(void) {
  farolilha_zerar();
  seg = 1800000000LL;
  ms = 1000u;
}

int main(void) {
  FarolIlhaFatos f;
  char buf[512];
  farolilha_relogio(relSeg, relMs);

  // --- aviso proativo: limiar -----------------------------------------------
  reiniciar();
  CHECA(farolilha_ativo() == 1, "gancho #27 ligado por padrao");
  f = fatos(1, 1, 0, 0, "tt1", FAROL_C_FONTE_RECUSOU);
  CHECA(!farolilha_proativo(&f), "1 falha e 1 parada: cala");
  f = fatos(2, 0, 0, 0, "tt1", FAROL_C_FONTE_RECUSOU);
  CHECA(farolilha_proativo(&f), "2 falhas fora do player: fala");
  CHECA(farolilha_codigo_vez() == FAROL_C_FONTE_RECUSOU, "codigo da vez e o do veredito");
  CHECA(farolilha_sintoma(&f) == RES_S_NAO_COMECA, "falhas abrem o Farol em 'nao comeca'");

  reiniciar();
  f = fatos(0, 2, 0, 0, "tt2", FAROL_C_REDE_CURTA);
  CHECA(farolilha_proativo(&f), "2 paradas em 5 min fora do player: fala");
  CHECA(farolilha_sintoma(&f) == RES_S_TRAVA, "paradas abrem o Farol em 'trava'");

  // --- nunca com video andando ----------------------------------------------
  reiniciar();
  f = fatos(3, 0, 1, 1, "tt1", FAROL_C_FONTE_RECUSOU);
  CHECA(!farolilha_proativo(&f), "player aberto com video andando: cala");
  f = fatos(3, 0, 1, 0, "tt1", FAROL_C_FONTE_RECUSOU);
  CHECA(!farolilha_proativo(&f), "player aberto (saindo): cala");
  f = fatos(3, 0, 0, 1, "tt1", FAROL_C_FONTE_RECUSOU);
  CHECA(!farolilha_proativo(&f), "video andando fora do player (retido): cala");
  f = fatos(3, 0, 0, 0, "tt1", FAROL_C_FONTE_RECUSOU);
  CHECA(farolilha_proativo(&f), "calar dentro do player nao gasta o teto: fala ao sair");

  // --- teto: 1 por titulo por sessao ----------------------------------------
  CHECA(!farolilha_proativo(&f), "2a vez no mesmo titulo/sessao: cala");
  f = fatos(5, 4, 0, 0, "tt1", FAROL_C_TV_FORMATO);
  CHECA(!farolilha_proativo(&f), "mesmo titulo, outro codigo, mesma sessao: cala");
  f = fatos(2, 0, 0, 0, "tt9", FAROL_C_FONTE_RECUSOU);
  CHECA(farolilha_proativo(&f), "troca de titulo: pode falar");
  f = fatos(2, 0, 0, 0, NULL, FAROL_C_FONTE_RECUSOU);
  CHECA(farolilha_proativo(&f), "sem titulo conhecido conta como um titulo: fala uma vez");
  CHECA(!farolilha_proativo(&f), "sem titulo conhecido: a 2a cala");
  farolilha_sessao_nova();
  f = fatos(2, 0, 0, 0, "tt1", FAROL_C_FONTE_RECUSOU);
  CHECA(farolilha_proativo(&f), "sessao nova: o mesmo titulo pode falar de novo");

  // --- dispensado com Voltar: 24 h por codigo -------------------------------
  reiniciar();
  f = fatos(2, 0, 0, 0, "tt1", FAROL_C_AUDIO_DTS_SEM_SAIDA);
  CHECA(farolilha_proativo(&f), "aviso do DTS fala");
  farolilha_dispensar(farolilha_codigo_vez());
  CHECA(farolilha_dispensado(FAROL_C_AUDIO_DTS_SEM_SAIDA), "Voltar marca o codigo como dispensado");
  f = fatos(2, 0, 0, 0, "tt2", FAROL_C_AUDIO_DTS_SEM_SAIDA);
  CHECA(!farolilha_proativo(&f), "dispensado: outro titulo com o mesmo codigo cala");
  f = fatos(2, 0, 0, 0, "tt3", FAROL_C_REDE_CURTA);
  CHECA(farolilha_proativo(&f), "dispensado: outro codigo fala");
  farolilha_sessao_nova();
  seg += FAROLILHA_DISPENSA_S - 1;
  f = fatos(2, 0, 0, 0, "tt4", FAROL_C_AUDIO_DTS_SEM_SAIDA);
  CHECA(!farolilha_proativo(&f), "dispensado: cala ate 86399 s, mesmo em sessao nova");
  seg += 1;
  CHECA(!farolilha_dispensado(FAROL_C_AUDIO_DTS_SEM_SAIDA), "dispensa vence em 86400 s");
  CHECA(farolilha_proativo(&f), "depois de 86400 s o mesmo codigo volta a falar");

  // relogio para tras: na duvida, cala
  reiniciar();
  farolilha_dispensar(FAROL_C_TV_FORMATO);
  seg -= 1000;
  CHECA(farolilha_dispensado(FAROL_C_TV_FORMATO), "relogio voltou: dispensa continua");

  // --- persistencia das dispensas ------------------------------------------
  reiniciar();
  farolilha_dispensar(FAROL_C_TV_FORMATO);
  seg += 10;
  farolilha_dispensar(FAROL_C_REDE_CURTA);
  CHECA(farolilha_exportar(buf, sizeof buf) > 0, "exporta as dispensas");
  farolilha_zerar();
  CHECA(!farolilha_dispensado(FAROL_C_TV_FORMATO), "zerar esquece as dispensas");
  farolilha_importar(buf);
  CHECA(farolilha_dispensado(FAROL_C_TV_FORMATO) && farolilha_dispensado(FAROL_C_REDE_CURTA),
        "importa as dispensas de volta");
  seg += FAROLILHA_DISPENSA_S;
  farolilha_importar(buf);
  CHECA(!farolilha_dispensado(FAROL_C_TV_FORMATO) && !farolilha_dispensado(FAROL_C_REDE_CURTA),
        "importar ignora dispensas vencidas");
  farolilha_importar("lixo\n7\n9999 5\n-1 9\n3 abc\n");
  CHECA(farolilha_exportar(buf, sizeof buf) == 0, "importar ignora linhas malformadas");
  farolilha_importar(NULL);
  CHECA(farolilha_exportar(buf, 1) == 0 && buf[0] == 0, "exportar com buffer minimo e NULL seguros");
  // buffer pequeno: corta na linha, nunca no meio
  reiniciar();
  farolilha_dispensar(1); farolilha_dispensar(2); farolilha_dispensar(3);
  { size_t n = farolilha_exportar(buf, 16);
    CHECA(n < 16 && (n == 0 || buf[n - 1] == '\n') && strlen(buf) == n, "exportar corta em linha inteira"); }
  // teto de dispensas: a mais velha sai
  reiniciar();
  { int i;
    for (i = 0; i < FAROLILHA_DISPENSAS + 1; i++) { farolilha_dispensar(1000 + i); seg++; }
    CHECA(!farolilha_dispensado(1000) && farolilha_dispensado(1000 + FAROLILHA_DISPENSAS),
          "dispensas cheias: sai a mais velha"); }
  // teto de titulos: o mais velho sai
  reiniciar();
  { int i; char t[16];
    for (i = 0; i < FAROLILHA_TITULOS + 1; i++) {
      snprintf(t, sizeof t, "tt%d", i);
      f = fatos(2, 0, 0, 0, t, FAROL_C_FONTE_RECUSOU);
      farolilha_proativo(&f);
    }
    f = fatos(2, 0, 0, 0, "tt0", FAROL_C_FONTE_RECUSOU);
    CHECA(farolilha_proativo(&f), "lista de titulos cheia: o mais velho sai");
    snprintf(t, sizeof t, "tt%d", FAROLILHA_TITULOS);
    f = fatos(2, 0, 0, 0, t, FAROL_C_FONTE_RECUSOU);
    CHECA(!farolilha_proativo(&f), "lista de titulos cheia: o mais novo continua avisado"); }

  // --- conserto calado -----------------------------------------------------
  reiniciar();
  CHECA(farolilha_conserto(FAROL_A_PROXIMA_FONTE, 0) == NULL, "conserto feito pela pessoa: cala");
  CHECA(farolilha_conserto(FAROL_A_PROXIMA_FONTE, 1) != NULL &&
        !strcmp(farolilha_conserto(FAROL_A_CONVERTER_AUDIO, 1), FT_AUDIO_CONVERTIDO),
        "conserto sozinho: frase de faroltexto.h");
  CHECA(farolilha_conserto(FAROL_A_CONVERTER_AUDIO, 1) == NULL, "mesmo conserto dentro de 4 s: uma pilula so");
  ms += FAROLILHA_CONSERTO_MS;
  CHECA(farolilha_conserto(FAROL_A_CONVERTER_AUDIO, 1) != NULL, "mesmo conserto depois de 4 s: fala");
  CHECA(farolilha_conserto(FAROL_A_ENVIAR_REGISTRO, 1) == NULL, "acao sem frase calada: cala");
  CHECA(farolilha_conserto(FAROL_A_NENHUMA, 1) == NULL && farolilha_conserto(-3, 1) == NULL &&
        farolilha_conserto(FAROL_A_N, 1) == NULL, "acao fora da tabela: cala");
  CHECA(!strcmp(farolilha_conserto_frase(FAROL_A_PROXIMA_FONTE), FT_FONTE_TROCADA_SIMPLES),
        "troca de fonte usa FT_FONTE_TROCADA_SIMPLES");
  { int a, comFrase = 0;
    for (a = 0; a < FAROL_A_N; a++) {
      const char *s = farolilha_conserto_frase(a);
      if (!s) continue;
      comFrase++;
      if (!s[0] || strlen(s) > 124) { printf("FALHOU: frase do conserto %d vazia ou longa\n", a); falhas++; }
    }
    CHECA(comFrase >= 9, "toda acao automatica tem frase calada"); }

  // --- limiar de 300 ms ----------------------------------------------------
  reiniciar();
  CHECA(!farolilha_atividade(0), "livre: sem atividade");
  CHECA(!farolilha_atividade(1), "teste comecou: nada ainda");
  ms += FAROLILHA_ATIVIDADE_MS - 1;
  CHECA(!farolilha_atividade(1), "299 ms: nada ainda");
  ms += 1;
  CHECA(farolilha_atividade(1), "300 ms: 'Testando…' aparece");
  CHECA(!farolilha_atividade(0), "acabou: some");
  ms += 5000;
  CHECA(!farolilha_atividade(1), "teste novo: o limiar conta de novo");
  ms += 100;
  CHECA(!farolilha_atividade(1) && !farolilha_atividade(0), "teste de 100 ms nunca pisca");
  // relogio ms que da a volta (49 dias)
  ms = 0xFFFFFF00u;
  farolilha_atividade(1);
  ms += 0x200u;
  CHECA(farolilha_atividade(1), "limiar atravessa a volta do relogio de 32 bits");

  // --- gancho da #27 -------------------------------------------------------
  reiniciar();
  farolilha_definir_ativo(0);
  CHECA(farolilha_ativo() == 0, "gancho #27 desligado");
  f = fatos(5, 5, 0, 0, "tt1", FAROL_C_FONTE_RECUSOU);
  CHECA(!farolilha_proativo(&f), "desligado: cala o proativo");
  CHECA(farolilha_conserto(FAROL_A_PROXIMA_FONTE, 1) == NULL, "desligado: cala o conserto calado");
  farolilha_definir_ativo(7);
  CHECA(farolilha_ativo() == 1, "religado (qualquer valor nao zero = 1)");
  CHECA(farolilha_proativo(&f), "religado: o titulo calado enquanto desligado pode falar");
  CHECA(farolilha_proativo(NULL) == 0, "fatos NULL: cala");

  if (falhas) { printf("farol_ilha: %d falha(s)\n", falhas); return 1; }
  printf("farol_ilha: tudo ok\n");
  return 0;
}
