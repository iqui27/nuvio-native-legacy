// Lembretes de programa do guia (src/lembrete.c): marcar, desmarcar, casar
// com a grade que mudou de horario, avisar uma vez, expirar, sobreviver ao
// reinicio (arquivo por perfil) e nao misturar perfis.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/lembrete.h"

// Disco de mentira: o que dados_gravar escreveu, dados_ler devolve.
static char arqNome[4][64], *arqCorpo[4];
static int acharArq(const char *nome, int criar) {
  int i;
  for (i = 0; i < 4; i++) if (arqCorpo[i] && !strcmp(arqNome[i], nome)) return i;
  if (!criar) return -1;
  for (i = 0; i < 4; i++) if (!arqCorpo[i]) { snprintf(arqNome[i], 64, "%s", nome); return i; }
  return -1;
}
int dados_gravar(const char *nome, const char *c) {
  int i = acharArq(nome, 1);
  free(arqCorpo[i]); arqCorpo[i] = strdup(c); return 1;
}
char *dados_ler(const char *nome) {
  int i = acharArq(nome, 0);
  return i < 0 ? NULL : strdup(arqCorpo[i]);
}
int dados_apagar(const char *nome) {
  int i = acharArq(nome, 0);
  if (i < 0) return 0;
  free(arqCorpo[i]); arqCorpo[i] = NULL; return 1;
}

static int falhas;
static void confere(const char *o, int ok) {
  printf("  %-60s %s\n", o, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

int main(void) {
  time_t agora = 1800000000;   // fixo: o teste nao depende do relogio
  time_t ini = agora + 3600, fim = ini + 1800;

  lembrete_carregar(1);
  confere("perfil sem arquivo comeca vazio", lembrete_n() == 0);
  confere("marcar devolve 1",
          lembrete_alternar("c1", "Globo RJ", "Jornal\tda Noite", "https://a", ini, fim) == 1);
  confere("tabulacao do titulo nao quebra o arquivo",
          lembrete_achar("c1", ini, "Jornal da Noite") == 0);
  confere("grade que atrasou 10 min ainda casa",
          lembrete_achar("c1", ini + 600, "Jornal da Noite") == 0);
  confere("outro titulo no mesmo horario nao casa",
          lembrete_achar("c1", ini, "Novela") < 0);
  confere("outro canal nao casa", lembrete_achar("c2", ini, "Jornal da Noite") < 0);
  lembrete_ajustar(0, ini + 600, fim + 600);
  confere("ajustar leva o lembrete para o horario novo", lembrete_item(0)->ini == ini + 600);

  confere("nada vence uma hora antes", lembrete_vencido(agora) < 0);
  confere("vence um minuto antes do inicio",
          lembrete_vencido(ini + 600 - LEMBRETE_ANTECEDE_S) == 0);
  lembrete_marcar_avisado(0);
  confere("avisado nao vence de novo", lembrete_vencido(ini + 700) < 0);

  // Reinicio: o arquivo do perfil traz a lista e o "avisado".
  lembrete_carregar(1);
  confere("reinicio le a lista do perfil", lembrete_n() == 1 && lembrete_item(0)->avisado);
  lembrete_carregar(2);
  confere("outro perfil tem a propria lista", lembrete_n() == 0);
  lembrete_alternar("c9", "SporTV", "Futebol", "", ini, fim);
  lembrete_carregar(1);
  confere("voltar ao perfil 1 nao traz o do perfil 2",
          lembrete_n() == 1 && !strcmp(lembrete_item(0)->canal, "c1"));

  confere("desmarcar devolve 0",
          lembrete_alternar("c1", "Globo RJ", "Jornal da Noite", "", ini + 600, fim) == 0);
  confere("desmarcado some", lembrete_n() == 0);

  lembrete_alternar("c1", "Globo RJ", "A", "", ini, fim);
  lembrete_alternar("c1", "Globo RJ", "B", "", fim, fim + 1800);
  confere("podar antes do fim nao tira nada", lembrete_podar(fim - 1) == 0);
  confere("podar tira o que acabou", lembrete_podar(fim) == 1 && lembrete_n() == 1 &&
          !strcmp(lembrete_item(0)->titulo, "B"));

  { int k, r = 0;
    for (k = 0; k < LEMBRETE_MAX + 2; k++) {
      char t[16]; snprintf(t, sizeof t, "P%d", k);
      r = lembrete_alternar("c3", "X", t, "", ini + k * 60, ini + k * 60 + 30);
    }
    confere("lista cheia devolve -1", r == -1 && lembrete_n() == LEMBRETE_MAX); }

  { char buf[256]; int m;
    snprintf(buf, sizeof buf, "lixo\n%lld\t%lld\t0\tc5\tCanal\tProg\t\n",
             (long long)ini, (long long)fim);
    m = lembrete_de_texto(buf);
    confere("linha quebrada no arquivo e pulada", m == 1 && !strcmp(lembrete_item(0)->canal, "c5")); }

  lembrete_esquecer_todos();
  lembrete_carregar(1);
  confere("logout apaga as listas", lembrete_n() == 0);

  if (falhas) { printf("FALHOU: %d\n", falhas); return 1; }
  puts("PASS: lembretes de programa (marcar, casar, avisar, expirar, perfis).");
  return 0;
}
