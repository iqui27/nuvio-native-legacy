// Vai tocar nesta TV (4.2): o portao do envio (vtregenvio.c) com o ajuste, o
// perfil infantil e o POST trocados por dubles. Com um argumento, grava nele
// os corpos que SAIRIAM, um por linha, para servidor/recomendacoes/
// teste-vaitocar.mjs conferir contra a rota de verdade (tests/vtreg.sh).
#include "vtregenvio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int ajuste, infantil, n;
static char ultimo[400], ultimaUrl[200];
static FILE *saida;
int ajustes_vaitocar(void) { return ajuste; }
int perfil_infantil_ativo(void) { return infantil; }
static int postar(const char *url, const char *corpo) {
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  snprintf(ultimo, sizeof ultimo, "%s", corpo);
  if (saida) fprintf(saida, "%s\n", corpo);
  n++;
  return 1;
}

int main(int argc, char **argv) {
  VtRegEntrada e = { "linux", "x86_64", "6.8.0-generic", "matroska,webm", "hevc", "eac3", "hdr10", 18000, 2160, VTR_TOCOU, 0 };
  VtRegEntrada ruim = { "webos", "http://192.168.0.10/Filme.2024.mkv", "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567",
                        "mkv", "Filme.Secreto.2024.HEVC", "Titulo da faixa", "DolbyVision", 90000, 2160, VTR_DECODER, 1 };
  if (argc > 1) saida = fopen(argv[1], "w");

  // Sem servidor no pacote: nada.
  ajuste = 1;
  assert(!vtregenvio_enviar(&e) && n == 0);
  vtregenvio_teste_postar(postar);
  // Padrao (ajuste desligado): nada sai.
  ajuste = 0;
  assert(!vtregenvio_enviar(&e) && n == 0);
  // Ligado, mas perfil infantil: nada sai.
  ajuste = 1; infantil = 1;
  assert(!vtregenvio_enviar(&e) && n == 0);
  // Ligado: sai uma vez; a mesma combinacao nao se repete na sessao.
  infantil = 0;
  assert(vtregenvio_enviar(&e) && n == 1);
  assert(strstr(ultimaUrl, "/v1/vaitocar"));
  assert(strstr(ultimo, "\"container\":\"mkv\"") && strstr(ultimo, "\"hdr\":\"hdr10\"") && strstr(ultimo, "\"mbps\":20"));
  assert(!vtregenvio_enviar(&e) && n == 1);
  // Outro resultado e outro registro.
  e.resultado = VTR_NAO_ABRIU; e.trocou = 1;
  assert(vtregenvio_enviar(&e) && n == 2);
  // Desligar no meio para de vez.
  ajuste = 0; e.resultado = VTR_CAIU;
  assert(!vtregenvio_enviar(&e) && n == 2);
  ajuste = 1;
  // Entrada suja: o que sai nao tem titulo, endereco nem hash.
  assert(vtregenvio_enviar(&ruim) && n == 3);
  assert(!strstr(ultimo, "Filme") && !strstr(ultimo, "192.168") && !strstr(ultimo, "0123456789abcdef") &&
         !strstr(ultimo, "Titulo") && !strstr(ultimo, "magnet") && !strstr(ultimo, "http"));
  assert(strstr(ultimo, "\"modelo\":\"?\"") && strstr(ultimo, "\"firmware\":\"?\"") && strstr(ultimo, "\"hdr\":\"dv\""));
  if (saida) fclose(saida);
  puts("vtregenvio: tudo ok");
  return 0;
}
