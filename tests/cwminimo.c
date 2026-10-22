// "Minimo para entrar no Continuar" (#326): o piso que era um 1% fixo agora e o
// ajuste ajustes_cw_minimo. Cobre prog_continuar_de_perfil_min (o criterio local
// que home/descoberta/perfilcont repetem) e o padrao: com piso 1 a resposta e a
// de sempre. Sem rede, sem SDL, sem disco.
#include "progresso.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *arquivo;
static int perfil = 1;
static long long agora = 1757000000000LL;

char *dados_ler(const char *nome) { (void)nome; return arquivo ? strdup(arquivo) : NULL; }
int dados_gravar(const char *nome, const char *c) { (void)nome; free(arquivo); arquivo = strdup(c); return 1; }
int dados_apagar(const char *nome) { (void)nome; free(arquivo); arquivo = NULL; return 1; }
int perfis_ativo(void) { return perfil; }
static long long relogio(void) { return agora; }

static void gravar(const char *id, double pos, double dur) {
  agora += 1000;
  assert(prog_gravar_local(id, 0, 0, pos, dur, -1));
}

int main(void) {
  ProgRegistro r;
  prog_definir_relogio(relogio);
  prog_invalidar();

  // 3% de um filme de 100 min: entra com o piso de fabrica (1), nao com 5.
  gravar("tt-tres", 180, 6000);
  assert(prog_continuar_de_perfil(1, 90, &r));
  assert(!strcmp(r.contentId, "tt-tres"));
  assert(prog_continuar_de_perfil_min(1, 1, 90, &r));
  assert(prog_continuar_de_perfil_min(1, 3, 90, &r));           // exatamente no piso entra
  assert(!prog_continuar_de_perfil_min(1, 4, 90, &r));          // abaixo do piso nao
  assert(!prog_continuar_de_perfil_min(1, 20, 90, &r));

  // O de 10% passa em ate 10, e o mais recente que passa e o escolhido.
  gravar("tt-dez", 600, 6000);
  assert(prog_continuar_de_perfil_min(1, 10, 90, &r));
  assert(!strcmp(r.contentId, "tt-dez"));
  gravar("tt-dois", 120, 6000);                                 // o mais novo, mas 2%
  assert(prog_continuar_de_perfil_min(1, 1, 90, &r));
  assert(!strcmp(r.contentId, "tt-dois"));
  assert(prog_continuar_de_perfil_min(1, 5, 90, &r));
  assert(!strcmp(r.contentId, "tt-dez"));                       // 2% e 3% ficaram de fora

  // O teto continua sendo o Percentual assistido, e abaixo de 1 minuto nunca entra.
  gravar("tt-fim", 5700, 6000);                                 // 95%
  assert(prog_continuar_de_perfil_min(1, 1, 90, &r));
  assert(strcmp(r.contentId, "tt-fim"));
  gravar("tt-curto", 30, 50);                                   // 50 s
  assert(strcmp((prog_continuar_de_perfil_min(1, 1, 90, &r), r.contentId), "tt-curto"));

  // Piso alto demais para tudo o que ha: nada entra.
  assert(!prog_continuar_de_perfil_min(1, 20, 60, &r) || r.posSeg / r.durSeg >= 0.20);
  printf("cwminimo: ok\n");
  free(arquivo);
  return 0;
}
