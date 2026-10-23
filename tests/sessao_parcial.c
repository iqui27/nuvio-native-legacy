// SESSAO PELA METADE NAO BLOQUEIA O ARRANQUE (#223).
//
// TCL Android 11: o login falhava e o proximo arranque ficava em branco ate
// `pm clear`. Este teste roda o sessao.c REAL com um nuvem dublado e confere:
//   1. sessao.txt truncado / sem JWT / conta sem `sub` / vazio: o arranque
//      descarta (memoria e disco) e sessao_logada() = 0 -> app abre no login;
//   2. sessao.txt boa continua restaurada;
//   3. login por e-mail sem resposta do servidor (transporte, curl 35): nada e
//      gravado em disco, o estado volta a DESLOGADO e a tela recebe o codigo da
//      libcurl; pedido de QR sem resposta idem (estado ERRO, sem sessao);
//   4. resposta de autenticacao com token que nao e JWT nao vira sessao.
#include "sessao.h"
#include "nuvem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nvtmp.h"

static const char *pasta(void) { const char *d = getenv("NV_T_DIR"); return d && *d ? d : nvt_dir(); }
char *dados_ler(const char *nome) {
  char c[600]; FILE *f; long n; char *b;
  snprintf(c, sizeof c, "%s/%s", pasta(), nome);
  f = fopen(c, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); n = (long)fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[600]; FILE *f;
  snprintf(c, sizeof c, "%s/%s", pasta(), nome);
  f = fopen(c, "wb"); if (!f) return 0; fputs(conteudo ? conteudo : "", f); fclose(f); return 1;
}
int dados_apagar(const char *nome) {
  char c[600]; snprintf(c, sizeof c, "%s/%s", pasta(), nome); return remove(c) == 0;
}
void dados_uuid(char *dst, unsigned tam) { snprintf(dst, tam, "00000000-0000-4000-8000-000000000000"); }
const char *i18n(const char *s) { return s; }

#define JWT_BOM "x.eyJzdWIiOiJ1MSIsImV4cCI6NDEwMjQ0NDgwMH0.y"
#define JWT_SEM_SUB "x.eyJleHAiOjQxMDI0NDQ4MDB9.y"   /* {"exp":4102444800} */
static int modo;   /* 0 sem resposta (transporte) ; 1 resposta 200 com token torto */
int nuvem_pronta(void) { return 1; }
const char *nuvem_base_login(void) { return "https://login.exemplo/tv"; }
int nuvem_erro_ausente(const char *c) { (void)c; return 0; }
void nuvem_falhou(void) {}
void nuvem_ok(void) {}
const char *nuvem_ultimo_erro(void) { return "curl 35: SSL connect error"; }
char *nuvem_post(const char *caminho, const char *corpo, const char *bearer, int *status) {
  (void)caminho; (void)corpo; (void)bearer;
  if (modo == 1) { *status = 200; return strdup("{\"access_token\":\"naoejwt\",\"refresh_token\":\"r\"}"); }
  *status = 0; return NULL;
}
char *nuvem_rpc_com(const char *f, const char *c, const char *b, int *s) { return nuvem_post(f, c, b, s); }
char *nuvem_tabela(const char *t, const char *q, const char *b, int *s) { (void)t; (void)q; (void)b; *s = 0; return NULL; }

static int falhas;
static void confere(const char *d, int ok) {
  printf("  %-66s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}
static int temArquivo(void) { char *b = dados_ler("sessao.txt"); int ok = b != NULL; free(b); return ok; }
static void esperar(void) {
  unsigned t = 1000; int i;
  for (i = 0; i < 4000; i++) { sessao_passo(t += 20); usleep(2000);
    if (sessao_estado() != SES_PEDINDO && sessao_estado() != SES_EMAIL) { sessao_passo(t += 20); sessao_passo(t += 20); break; } }
}
static void arranque(const char *conteudo, const char *rotulo, int esperaLogada) {
  char d[200]; (void)d;
  if (conteudo) dados_gravar("sessao.txt", conteudo); else dados_apagar("sessao.txt");
  sessao_iniciar();
  confere(rotulo, sessao_logada() == esperaLogada);
}

int main(void) {
  /* sessao_iniciar guarda estado estatico: cada caso parte de uma sessao limpa. */
  arranque(JWT_BOM "\nr\n0\n", "sessao boa e restaurada", 1);
  sessao_sair();
  arranque(JWT_BOM "\nr", "sem quebra de linha final: ainda e uma sessao valida", 1);
  sessao_sair();
  arranque("", "arquivo vazio: sem sessao", 0);
  arranque("lixo-truncado", "token que nao e JWT: descartado", 0);
  confere("  e apagado do disco", !temArquivo());
  arranque("x.eyJzdWIi", "JWT truncado (sem 3a parte): descartado", 0);
  confere("  e apagado do disco", !temArquivo());
  arranque(JWT_SEM_SUB "\nr\n0\n", "conta sem sub: descartada", 0);
  confere("  e apagado do disco", !temArquivo());
  arranque(JWT_BOM "\nr\n1\n", "sessao anonima nao conta como conta", 0);
  sessao_sair();

  /* login por e-mail sem transporte */
  dados_apagar("sessao.txt");
  sessao_iniciar();
  sessao_login_email("a@b.test", "senha");
  esperar();
  confere("e-mail sem servidor: volta a DESLOGADO", sessao_estado() == SES_DESLOGADO);
  confere("  nada gravado em disco", !temArquivo());
  confere("  nao esta logada", !sessao_logada());
  confere("  tela traz o codigo da libcurl", strstr(sessao_erro_email(), "[TLS; curl 35]") != NULL);
  /* QR sem transporte */
  sessao_login_comecar();
  esperar();
  confere("QR sem servidor: ERRO com o codigo da libcurl",
          sessao_estado() == SES_ERRO && strstr(sessao_erro(), "[TLS; curl 35]") != NULL);
  confere("  nada gravado em disco", !temArquivo());
  confere("  nao esta logada", !sessao_logada());
  /* token torto numa resposta 200 */
  modo = 1;
  sessao_cancelar();
  sessao_login_email("a@b.test", "senha");
  esperar();
  confere("200 com token que nao e JWT: nao vira sessao", !sessao_logada() && !temArquivo());
  confere("  estado volta a DESLOGADO", sessao_estado() == SES_DESLOGADO);
  printf(falhas ? "FALHOU\n" : "PASSOU\n");
  return falhas ? 1 : 0;
}
