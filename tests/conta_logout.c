// Sair da conta apaga MESMO o usuario anterior?
//
// O defeito: sessao_sair() apagava so o token. Este teste monta um estado de
// usuario logado de verdade (sessao, addons vindos da conta, perfil ativo
// gravado, progresso no disco), sai, e confere item a item. Compilar nao prova
// nada aqui — o defeito era justamente codigo ausente, que compila perfeito.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dados.h"
#include "nuvem.h"
#include "sessao.h"
#include "perfis.h"
#include "sync.h"
#include "addons.h"
#include "trakt.h"
#include "js.h"

static int falhas;
static void confere(const char *o_que, int ok, const char *detalhe) {
  printf("  %-42s %s%s%s\n", o_que, ok ? "ok" : "FALHOU",
         detalhe && *detalhe ? "  " : "", detalhe ? detalhe : "");
  if (!ok) falhas++;
}
static int existe(const char *nome) {
  char *b = dados_ler(nome);
  int e = b != NULL;
  free(b);
  return e;
}

int main(void) {
  char *r, acesso[3000] = {0}, renov[3000] = {0}, arq[6400];
  int st = 0, i;

  dados_iniciar(getenv("NUVIO_DADOS"));
  if (!nuvem_configurar(NULL)) return 2;
  r = nuvem_post("/auth/v1/signup", "{\"data\":{\"tv_client\":\"webos\"}}", NULL, &st);
  if (!r || st >= 300) { printf("sessao anonima falhou\n"); return 3; }
  js_texto(r, r + strlen(r), "access_token", acesso, sizeof acesso);
  js_texto(r, r + strlen(r), "refresh_token", renov, sizeof renov);
  free(r);
  snprintf(arq, sizeof arq, "%s\n%s\n0\n", acesso, renov);
  dados_gravar("sessao.txt", arq);
  sessao_iniciar();

  // Estado de "usuario usando o app": sync traz addons, ele escolhe perfil e
  // assiste alguma coisa.
  sync_iniciar();
  for (i = 0; i < 60 && sync_estado() == SYNC_RODANDO; i++) usleep(500000);
  sync_passo(1000);
  perfis_definir_ativo(3);
  trakt_definir("token-de-quem-estava-logado", "cliente-do-app");
  dados_gravar("progresso.txt", "tt0111161\t1200\t8520\n");

  printf("\nANTES de sair:\n");
  printf("  addons=%d  trakt=%d  perfil ativo=%d  perfil.txt=%d  progresso.txt=%d\n",
         addons_n(), trakt_ativo(), perfis_ativo(),
         existe("perfil.txt"), existe("progresso.txt"));
  if (addons_n() == 0)
    printf("  AVISO: a conta de teste veio sem addon; o item de addons nao prova nada\n");

  sessao_sair();
  sync_esquecer_usuario();

  printf("\nDEPOIS de sair:\n");
  confere("sessao encerrada",              !sessao_logada(), "");
  confere("sessao.txt apagado",            !existe("sessao.txt"), "");
  confere("lista de addons vazia",         addons_n() == 0, "");
  confere("Trakt desligado",               !trakt_ativo(), "");
  confere("nenhum perfil em memoria",      perfis_n() == 0, "");
  confere("perfil ativo de volta ao 1",    perfis_ativo() == 1, "");
  confere("perfil.txt apagado",            !existe("perfil.txt"), "");
  confere("progresso.txt apagado",         !existe("progresso.txt"), "");
  confere("dono da conta esquecido",       perfis_dono()[0] == 0, "");
  // O cache do catalogo guarda a home montada da conta que saiu, e dentro
  // dele a `base` de cada fileira — que no Xperience leva um JWT no caminho.
  // Sair da conta tem de tirar o arquivo do aparelho, nao so recusa-lo na
  // proxima leitura.
  confere("cache do catalogo apagado",     !existe("catalogo-rede.bin"), "");

  // O ciclo seguinte NAO pode reaplicar o que ficou na caixa do fio.
  sync_passo(2000);
  confere("sync_passo nao ressuscita addons", addons_n() == 0, "");

  printf("\n%s\n", falhas ? "TEM FALHA" : "TUDO APAGADO");
  return falhas ? 1 : 0;
}
