// #149: "ENVIAR REGISTROS SOZINHO" NAO FICA LIGADO.
//
// O relato: "depois de ligar, volta a desligado toda vez que fecho o app".
// O ajuste fica FORA do vetor posicional `valor[]` (as ultimas entradas do
// enum ficam no zero da inicializacao parcial, e 0 e LIGADO em V_LIGA), e o
// padrao de fabrica — desligado — era escrito em ajustes_iniciar(). Essa
// funcao roda TODA VEZ que a tela de Ajustes abre, depois de ajustes_dir() ja
// ter lido o arquivo: a escolha da pessoa era apagada na abertura seguinte da
// tela, e a proxima gravacao levava o "desligado" ao disco. E o mesmo defeito
// do #82/#86 (autoplay do trailer), no mesmo lugar.
//
// O remendo existia porque o vetor estava sete casas curto (as linhas do
// Stalker e do Xtream) e o padrao escrito nele caia em outra opcao. O padrao
// agora mora no vetor: LIGADO de fabrica (decisao do dono, 26/09/2026).
//
// Este teste le o disco como o arranque le (ajustes_dir sem a tela aberta) e
// abre a tela duas vezes, conferindo o valor e o que fica gravado.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

static void escrever(const char *caminho, const char *conteudo) {
  FILE *f = fopen(caminho, "w");
  assert(f);
  fputs(conteudo, f);
  fclose(f);
}

// Valor de "envioAuto" no arquivo; -1 se a linha nao existe.
static int noDisco(const char *caminho) {
  char linha[96];
  int v = -1, x;
  FILE *f = fopen(caminho, "r");
  if (!f) return -1;
  while (fgets(linha, sizeof linha, f))
    if (sscanf(linha, "envioAuto %d", &x) == 1) v = x;
  fclose(f);
  return v;
}

int main(void) {
  char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-aj-envio-XXXXXX");
  char caminho[700];
  assert(mkdtemp(dir));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);

  // 0. MIGRACAO UNICA (1.5.1): quem chega da 1.5.0 com "desligado" no disco
  //    (quase sempre gravado pelo defeito) volta ligado UMA vez; a marca
  //    envio-151.txt impede a segunda. Diretorio proprio, sem marca.
  { char d0[NVT_MAX], c0[700], m0[700]; nvt_set(d0, sizeof d0, "/nuvio-aj-envio0-XXXXXX");
    FILE *mf;
    assert(mkdtemp(d0));
    snprintf(c0, sizeof c0, "%s/ajustes.txt", d0);
    snprintf(m0, sizeof m0, "%s/envio-151.txt", d0);
    escrever(c0, "envioAuto 1\n");
    ajustes_dir(d0);
    assert(ajustes_envio_auto());
    assert(noDisco(c0) == 0);
    mf = fopen(m0, "r"); assert(mf); fclose(mf);
    // Depois da migracao, desligar vale de novo e sobrevive ao arranque.
    escrever(c0, "envioAuto 1\n");
    ajustes_dir(d0);
    assert(!ajustes_envio_auto());
    unlink(c0); unlink(m0);
    { char t0[700]; snprintf(t0, sizeof t0, "%s/ajustes.tmp", d0); unlink(t0); }
    rmdir(d0); }

  // 1. Arranque sem linha no arquivo: LIGADO de fabrica, e abrir a tela nao
  //    muda isso.
  escrever(caminho, "idioma 0\n");
  ajustes_dir(dir);
  assert(ajustes_envio_auto());
  ajustes_iniciar();
  assert(ajustes_envio_auto());

  // 1b. Quem desligou (envioAuto 1) continua desligado depois de abrir a tela.
  escrever(caminho, "envioAuto 1\n");
  ajustes_dir(dir);
  assert(!ajustes_envio_auto());
  ajustes_iniciar();
  assert(!ajustes_envio_auto());

  // 2. Arranque com a escolha gravada (envioAuto 0 = ligado): continua ligado
  //    no arranque e depois de abrir a tela, uma e duas vezes.
  escrever(caminho, "envioAuto 0\n");
  ajustes_dir(dir);
  assert(ajustes_envio_auto());
  ajustes_iniciar();
  assert(ajustes_envio_auto());
  ajustes_iniciar();
  assert(ajustes_envio_auto());

  // 3. Ligar pela tela (ou pelo cartao do Tizen) grava; abrir a tela de novo
  //    nao desfaz, e o disco guarda "ligado" para o proximo arranque.
  ajustes_definir_envio_auto(0);
  assert(!ajustes_envio_auto());
  ajustes_definir_envio_auto(1);
  ajustes_iniciar();
  assert(ajustes_envio_auto());
  assert(noDisco(caminho) == 0);

  // 4. Desligar tambem fica.
  ajustes_definir_envio_auto(0);
  ajustes_iniciar();
  assert(!ajustes_envio_auto());
  assert(noDisco(caminho) == 1);

  unlink(caminho);
  { char tmp[700]; snprintf(tmp, sizeof tmp, "%s/ajustes.tmp", dir); unlink(tmp);
    snprintf(tmp, sizeof tmp, "%s/envio-151.txt", dir); unlink(tmp); }
  rmdir(dir);
  puts("ajustes_envio_auto: ok");
  return 0;
}
