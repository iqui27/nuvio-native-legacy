// spikebin — executavel estatico ARMv7, replica o padrao do tailscale-tizen:
// Process.Start(spikebin) a partir do app .NET, escreve um arquivo, sai.
// Prova o caminho "subprocesso" (sem janela/GL, so E/S e exit code) —
// alternativa se DllImport de .so propria for barrado.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  const char *destino = (argc > 1) ? argv[1] : "/tmp/nvspike-spikebin.txt";
  FILE *f = fopen(destino, "w");
  if (!f) return 1;
  fprintf(f, "spikebin rodou, pid=%d\n", (int)getpid());
  fclose(f);
  return 0;
}
