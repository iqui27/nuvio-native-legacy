// LOGIN POR E-MAIL E SENHA (#216), de ponta a ponta contra um servidor FALSO
// local (tests/sessao_email.sh sobe um http.server em 127.0.0.1). Roda o
// sessao.c, o nuvem.c e o rede.c REAIS: o pedido sai pela mesma pilha HTTP do
// app. Nenhuma conta real, nenhuma credencial real.
//
//   1. 200: sessao de USUARIO gravada em sessao.txt igual a do QR (acesso,
//      refresh, 0), pedido com apikey + corpo {"email","password"} escapado;
//   2. 400 nos dois formatos do GoTrue: "E-mail ou senha incorretos.";
//   3. 400 email_not_confirmed, 429, 503 e servidor fora: frase propria;
//   4. em nenhum caso o log (stdout) traz o e-mail, a senha ou o token.
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

static int falhas;
static void confere(const char *d, int ok) {
  printf("  %-66s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

int main(int argc, char **argv) {
  const char *caso = argc > 1 ? argv[1] : "";
  const char *esperado = argc > 2 ? argv[2] : "";
  unsigned agora = 1000;
  int i;
  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("-- sessao_email: %s\n", caso);
  confere("pacote com servidor (falso) configurado", nuvem_pronta());
  // Senha com aspas, barra e acento: o corpo tem de sair JSON valido.
  sessao_login_email("pessoa@exemplo.test", "s3nh\"a\\fals@-ção");
  confere("pedido em andamento", sessao_estado() == SES_EMAIL);
  for (i = 0; i < 4000 && sessao_estado() == SES_EMAIL; i++) { sessao_passo(agora += 20); usleep(1000); }
  if (!strcmp(caso, "ok")) {
    char *b = dados_ler("sessao.txt");
    confere("logado como usuario", sessao_logada() && sessao_estado() == SES_LOGADO);
    confere("sessao.txt = acesso, refresh, 0 (igual ao QR)",
            b && !strcmp(b, "x.eyJzdWIiOiJ1MSIsImV4cCI6NDEwMjQ0NDgwMH0.y\nrefresh-falso\n0\n"));
    confere("sub do JWT lido", !strcmp(sessao_usuario(), "u1"));
    confere("sem frase de erro", !sessao_erro_email()[0]);
    free(b);
  } else {
    char *b = dados_ler("sessao.txt");
    printf("  tela: %s\n", sessao_erro_email());
    confere("nao logado, volta a DESLOGADO", !sessao_logada() && sessao_estado() == SES_DESLOGADO);
    confere("nada gravado", !b);
    confere("frase esperada", !strncmp(sessao_erro_email(), esperado, strlen(esperado)));
    // #223: sem HTTP, o codigo da libcurl vai junto na tela (e no log).
    if (!strcmp(caso, "semservidor"))
      confere("codigo curl na tela", strstr(sessao_erro_email(), "[connection; curl 7]") != NULL);
    free(b);
    // Tentar de novo funciona (o pedido nao fica preso).
    sessao_login_email("pessoa@exemplo.test", "outra");
    confere("segunda tentativa sai", sessao_estado() == SES_EMAIL);
    for (i = 0; i < 4000 && sessao_estado() == SES_EMAIL; i++) { sessao_passo(agora += 20); usleep(1000); }
  }
  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
