// BUSCAS RECENTES: disco, dedupe, teto, ordem e separacao por perfil.
//
// perfis_ativo() e DESTE arquivo (o teste compila so buscasrec.c e dados.c):
// trocar de perfil aqui e mudar uma variavel, sem conta nem rede — o que se
// testa e que buscasrec rele o arquivo certo quando o perfil muda, nao perfis.c.
//
// Cada checagem que le o disco passa por recarregar(): alterna o perfil para
// um que nao se usa e volta, o que forca garantir() a ler o arquivo de novo.
// Sem isso o teste conferiria so a lista em memoria e um gravar() quebrado
// passaria.
#include "buscasrec.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int perfil = 1;
int perfis_ativo(void) { return perfil; }

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %-58s %s\n", o_que, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}
static void recarregar(void) { int p = perfil; perfil = 99; buscasrec_n(); perfil = p; }
static int existe(const char *nome) {
  char *b = dados_ler(nome);
  int e = b != NULL;
  free(b);
  return e;
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  char t[32];
  int i;
  if (!dir || !*dir) { puts("NUVIO_DADOS nao definido"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { puts("dados_dir nao e a pasta do teste"); return 2; }

  puts("regras de entrada:");
  confere("vazio no comeco", buscasrec_n() == 0);
  confere("1 caractere recusado", !buscasrec_registrar("a") && buscasrec_n() == 0);
  confere("so espacos recusado", !buscasrec_registrar("   x  ") && buscasrec_n() == 0);
  confere("2 caracteres aceito", buscasrec_registrar("up") && buscasrec_n() == 1);
  buscasrec_registrar("  matrix  ");
  confere("pontas aparadas", !strcmp(buscasrec_termo(0), "matrix"));
  buscasrec_registrar("dune");
  confere("mais recente primeiro", !strcmp(buscasrec_termo(0), "dune") &&
                                   !strcmp(buscasrec_termo(1), "matrix") &&
                                   !strcmp(buscasrec_termo(2), "up"));
  buscasrec_registrar("MATRIX ");
  confere("repetido sem caixa sobe, sem duplicar",
          buscasrec_n() == 3 && !strcmp(buscasrec_termo(0), "MATRIX") &&
          !strcmp(buscasrec_termo(1), "dune") && !strcmp(buscasrec_termo(2), "up"));
  recarregar();
  confere("ordem sobrevive ao disco",
          buscasrec_n() == 3 && !strcmp(buscasrec_termo(0), "MATRIX") &&
          !strcmp(buscasrec_termo(2), "up"));
  confere("arquivo e buscas-p1.txt", existe("buscas-p1.txt"));

  puts("teto de 10:");
  buscasrec_limpar();
  for (i = 0; i < 13; i++) { snprintf(t, sizeof t, "termo %02d", i); buscasrec_registrar(t); }
  recarregar();
  confere("no maximo 10", buscasrec_n() == BUSCASREC_MAX);
  confere("topo e o ultimo registrado", !strcmp(buscasrec_termo(0), "termo 12"));
  confere("os 3 mais antigos cairam", !strcmp(buscasrec_termo(9), "termo 03"));
  buscasrec_registrar("termo 05");
  recarregar();
  confere("repetido no meio sobe e a conta fica 10",
          buscasrec_n() == 10 && !strcmp(buscasrec_termo(0), "termo 05") &&
          !strcmp(buscasrec_termo(1), "termo 12") && !strcmp(buscasrec_termo(9), "termo 03"));

  puts("remover:");
  buscasrec_remover(1);
  recarregar();
  confere("remove so o termo pedido", buscasrec_n() == 9 &&
          !strcmp(buscasrec_termo(0), "termo 05") && !strcmp(buscasrec_termo(1), "termo 11"));
  buscasrec_remover(42);
  confere("indice fora da faixa nao mexe", buscasrec_n() == 9);
  buscasrec_limpar();
  recarregar();
  confere("limpar zera e apaga o arquivo", buscasrec_n() == 0 && !existe("buscas-p1.txt"));

  puts("arquivo editado a mao:");
  dados_gravar("buscas-p1.txt", "# cabecalho\nabc\n\nx\nABC\n  def  \n");
  recarregar();
  confere("saneado: sem curto, sem duplicata, aparado",
          buscasrec_n() == 2 && !strcmp(buscasrec_termo(0), "abc") &&
          !strcmp(buscasrec_termo(1), "def"));

  puts("perfis:");
  buscasrec_limpar();
  buscasrec_registrar("do perfil um");
  perfil = 2;
  confere("perfil 2 nao ve o do 1", buscasrec_n() == 0);
  buscasrec_registrar("do perfil dois");
  perfil = 1;
  confere("perfil 1 ve so o seu", buscasrec_n() == 1 &&
          !strcmp(buscasrec_termo(0), "do perfil um"));
  perfil = 2;
  confere("perfil 2 ve so o seu", buscasrec_n() == 1 &&
          !strcmp(buscasrec_termo(0), "do perfil dois"));
  perfil = 0;
  buscasrec_registrar("sem perfil");
  confere("perfil 0 grava em buscas.txt", existe("buscas.txt"));

  puts("sair da conta:");
  buscasrec_esquecer();
  confere("arquivos de todos os perfis apagados",
          !existe("buscas.txt") && !existe("buscas-p1.txt") && !existe("buscas-p2.txt"));
  perfil = 1;
  confere("perfil 1 volta vazio", buscasrec_n() == 0);

  if (falhas) { printf("FALHOU: %d\n", falhas); return 1; }
  puts("PASS: buscas recentes");
  return 0;
}
