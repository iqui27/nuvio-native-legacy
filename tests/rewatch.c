// SESSAO DE REASSISTIR (2.1): src/rewatch.c, sem rede e sem disco de verdade.
// Cobre o que o dono decidiu: sessao por perfil e obra, selo "Nx" so dentro da
// sessao, sobrevive a fechar o app, viaja pela Conta (texto no blob de ajustes)
// sem ressuscitar sessao encerrada.
//
// ESTE ARQUIVO COMPILA NO COMMIT PAI de proposito e la FALHA nas assercoes
// (sem src/rewatch.c o .sh nao liga TEM_REWATCH).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TEM_REWATCH
#include "rewatch.h"
#endif

static int falhas;
static void confere(const char *o_que, long long obtido, long long esperado) {
  int ok = obtido == esperado;
  printf("  %-62s %s (obtido %lld, esperado %lld)\n", o_que, ok ? "ok    " : "FALHOU",
         obtido, esperado);
  if (!ok) falhas++;
}

// ---------------------------------------------------------------- dubles
static int perfil = 1, logada = 0;
static long long agora = 1759000000000LL;   // 27/09/2025 19:06:40 UTC
static long long relogioFalso(void) { return agora += 1000; }
int perfis_ativo(void) { return perfil; }
int sessao_logada(void) { return logada; }
const char *sessao_usuario(void) { return logada ? "u-123" : ""; }

#define DISCO_MAX 4
static struct { char nome[128]; char *txt; } disco[DISCO_MAX];
static int discoAcha(const char *n) {
  int i;
  for (i = 0; i < DISCO_MAX; i++) if (disco[i].txt && !strcmp(disco[i].nome, n)) return i;
  return -1;
}
char *dados_ler(const char *n) {
  int i = discoAcha(n);
  return i < 0 ? NULL : strdup(disco[i].txt);
}
int dados_gravar(const char *n, const char *c) {
  int i = discoAcha(n);
  if (i < 0) for (i = 0; i < DISCO_MAX && disco[i].txt; i++) {}
  if (i >= DISCO_MAX) return 0;
  snprintf(disco[i].nome, sizeof disco[i].nome, "%s", n);
  free(disco[i].txt);
  disco[i].txt = strdup(c);
  return 1;
}
int dados_apagar(const char *n) {
  int i = discoAcha(n);
  if (i >= 0) { free(disco[i].txt); disco[i].txt = NULL; }
  return 1;
}

#ifndef TEM_REWATCH
int main(void) {
  confere("src/rewatch.c existe (TEM_REWATCH)", 0, 1);
  return 1;
}
#else
#define SILO "tt14688458"

int main(void) {
  char *txt, *velho, *blob, *costurado;
  int t = 0, e = 0, pct = 0;
  rewatch_relogio(relogioFalso);

  printf("sem sessao:\n");
  confere("ativa", rewatch_ativa(SILO), 0);
  confere("registrar sem sessao nao conta", rewatch_registrar(SILO, 1, 1), 0);
  confere("vezes sem sessao (sem selo)", rewatch_vezes(SILO, 1, 1), 0);

  printf("comecar e contar:\n");
  confere("iniciar cria", rewatch_iniciar(SILO, RW_ORIGEM_TITULO), 1);
  confere("iniciar de novo nao duplica", rewatch_iniciar(SILO, RW_ORIGEM_CONTINUAR), 0);
  confere("ativa", rewatch_ativa(SILO), 1);
  confere("ativa pelo id de episodio tt:2:8", rewatch_ativa(SILO ":2:8"), 1);
  confere("origem guardada", rewatch_origem(SILO), RW_ORIGEM_TITULO);
  confere("episodio ainda nao revisto: sem selo", rewatch_vezes(SILO, 1, 1), 0);
  confere("1o episodio concluido: 2x", rewatch_registrar(SILO, 1, 1), 2);
  confere("mesmo episodio de novo: 3x", rewatch_registrar(SILO, 1, 1), 3);
  confere("selo do episodio", rewatch_vezes(SILO, 1, 1), 3);
  confere("outro episodio: 2x", rewatch_registrar(SILO, 1, 2), 2);
  confere("episodios distintos revistos", rewatch_feitos(SILO), 2);
  confere("episodio nao revisto segue sem selo", rewatch_vezes(SILO, 1, 3), 0);

  printf("ponto parcial:\n");
  rewatch_parcial(SILO, 1, 3, 42);
  confere("parcial guardado", rewatch_parcial_de(SILO, &t, &e, &pct), 1);
  confere("  temporada", t, 1);
  confere("  episodio", e, 3);
  confere("  pct", pct, 42);
  confere("concluir limpa o parcial", (rewatch_registrar(SILO, 1, 3), rewatch_parcial_de(SILO, &t, &e, &pct)), 0);
  rewatch_parcial(SILO, 1, 4, 100);
  confere("pct 100 nao e parcial", rewatch_parcial_de(SILO, &t, &e, &pct), 0);

  printf("perfil e disco:\n");
  perfil = 2;
  confere("outro perfil: sem sessao", rewatch_ativa(SILO), 0);
  perfil = 1;
  rewatch_esquecer();           // fechou o app
  confere("depois de reabrir: sessao volta", rewatch_ativa(SILO), 1);
  confere("depois de reabrir: contagem volta", rewatch_vezes(SILO, 1, 1), 3);
  confere("depois de reabrir: feitos", rewatch_feitos(SILO), 3);
  confere("lista de ativas", rewatch_n_ativas(), 1);

  printf("conta Nuvio:\n");
  confere("algo local para subir", rewatch_sujo(), 1);
  confere("sujo limpa", rewatch_sujo(), 0);
  velho = rewatch_texto();
  confere("texto da conta nao vazio", velho && *velho, 1);
  rewatch_encerrar(SILO);
  confere("encerrar fecha", rewatch_ativa(SILO), 0);
  confere("encerrar tira o selo", rewatch_vezes(SILO, 1, 1), 0);
  confere("encerrar marca para subir", rewatch_sujo(), 1);
  confere("sessao velha da conta NAO ressuscita a encerrada", rewatch_mesclar(velho), 0);
  confere("  continua encerrada", rewatch_ativa(SILO), 0);
  txt = rewatch_texto();
  rewatch_esquecer();
  dados_apagar("rewatch-anon.txt");      // "outra TV" sem nada no disco
  confere("outra TV aprende a lapide", rewatch_mesclar(txt) >= 1, 1);
  confere("  continua encerrada", rewatch_ativa(SILO), 0);
  rewatch_esquecer();
  dados_apagar("rewatch-anon.txt");
  confere("outra TV recebe a sessao ativa", rewatch_mesclar(velho) >= 1, 1);
  confere("  ativa", rewatch_ativa(SILO), 1);
  confere("  contagem viajou", rewatch_vezes(SILO, 1, 1), 3);
  confere("  feitos viajou", rewatch_feitos(SILO), 3);
  confere("  mesmo texto de novo nao muda nada", rewatch_mesclar(velho), 0);
  free(txt);
  free(velho);

  printf("blob de ajustes:\n");
  costurado = rewatch_blob_costurar("{\"theme\":\"dark\"}");
  confere("acrescenta a chave", costurado && strstr(costurado, "\"rewatchSessions\":\"") && strstr(costurado, "\"theme\":\"dark\"") ? 1 : 0, 1);
  blob = rewatch_blob_costurar(costurado);
  confere("troca a chave sem duplicar", blob && strstr(blob, "rewatchSessions") && !strstr(strstr(blob, "rewatchSessions") + 1, "rewatchSessions") ? 1 : 0, 1);
  confere("blob invalido -> NULL", rewatch_blob_costurar("sem chaves") == NULL, 1);
  rewatch_encerrar(SILO);
  rewatch_esquecer();
  dados_apagar("rewatch-anon.txt");
  confere("ler do blob em TV limpa", rewatch_blob_ler(costurado) >= 1, 1);
  confere("  sessao ativa", rewatch_ativa(SILO), 1);
  confere("ler blob sem a chave", rewatch_blob_ler("{\"a\":1}"), 0);
  free(costurado);
  free(blob);

  printf("lapide vence 30 dias:\n");
  rewatch_encerrar(SILO);
  agora += 31LL * 86400000LL;
  rewatch_iniciar("tt0000001", RW_ORIGEM_CONTINUAR);
  txt = rewatch_texto();
  confere("lapide velha podada", strstr(txt, SILO) == NULL, 1);
  confere("sessao nova esta no texto", strstr(txt, "tt0000001") != NULL, 1);
  free(txt);

  printf("%s\n", falhas ? "FALHOU" : "tudo ok");
  return falhas ? 1 : 0;
}
#endif
