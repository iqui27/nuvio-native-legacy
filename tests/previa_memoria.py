"""Extrai as funcoes REAIS de selecao do provedor de previas de src/ajustes.c
(previaOpenSeek, previaLocalCapaz, previaLocal, previaAplicar,
ajEpgMemoriaAplicar) para um programa C que as compila e as dirige — codigo de
producao rodando, nao leitura de texto.

Cenario sob teste: com "No aparelho" escolhido e aplicado, mudar a "Memoria do
guia" para Baixa tem de desligar o provedor local NA MESMA chamada
(ajEpgMemoriaAplicar), nao no proximo heartbeat do worker. Na versao reprovada
o provider local ficava aplicado depois da troca. Uso:

    python3 tests/previa_memoria.py <saida.c>
"""
import re
import sys
from pathlib import Path

raiz = Path(__file__).resolve().parent.parent
fonte = (raiz / "src" / "ajustes.c").read_text()


def funcao(nome):
    m = re.search(r"^\w[^\n]*\b" + nome + r"\([^\n]*\) \{.*?^\}", fonte, re.M | re.S)
    if not m:
        sys.exit(f"funcao {nome} nao encontrada em src/ajustes.c")
    return m[0]


corpo = "\n".join(
    funcao(f)
    for f in ("previaOpenSeek", "previaLocalCapaz", "previaLocal",
              "previaAplicar", "ajEpgMemoriaAplicar")
)

Path(sys.argv[1]).write_text(
    '''// Gerado por tests/previa_memoria.py: as funcoes abaixo sao extraidas
// verbatim de src/ajustes.c — codigo de producao compilado, nao copia.
#include <stdio.h>
#include <string.h>
#include "memoria.h"            // a regra do guia de verdade (epg_memoria_eh_baixa)

enum { AJ_PREVIA_PROVEDOR, AJ_EPG_MEMORIA, AJ_N };
static int valor[AJ_N];
static long ramMB = 4096;       // o que tex_orcamento_info reporta
static int tools = 1;           // o pacote tem as ferramentas FFmpeg
static int provLocal, provOpen; // objetos de provedor de mentira
static int aplicado;            // 2 local, 1 OpenSeek, 0 Seekr
static int aplicadas;           // quantas vezes seekr_usar_provedor rodou

void tex_orcamento_info(void *a, long *r, void *b, void *c) {
  (void)a; (void)b; (void)c; *r = ramMB;
}
int SDL_GetSystemRAM(void) { return 0; }
// mesma regra que miniatura_tv_capaz aplica (memoria.h), sem o probe do pacote
int miniatura_tv_capaz(int modo, long ram) { return tools && !epg_memoria_eh_baixa(modo, ram); }
void epg_memoria_definir(int modo, long ram) { (void)modo; (void)ram; }
void *miniatura_provedor(void) { return &provLocal; }
void *openseek_provedor(void) { return &provOpen; }
void seekr_usar_provedor(void *p) {
  aplicado = p == &provLocal ? 2 : p == &provOpen ? 1 : 0;
  aplicadas++;
}

static void previaAplicar(void);
'''
    + corpo
    + '''
static int falhas;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FALHOU: " __VA_ARGS__); printf("\\n"); falhas++; } } while (0)

int main(void) {
  // "No aparelho" escolhido: o provedor local e aplicado.
  valor[AJ_PREVIA_PROVEDOR] = 2;
  previaAplicar();
  CHECK(aplicado == 2, "com ferramentas e RAM o provedor local e aplicado (aplicado=%d)", aplicado);
  CHECK(previaLocal(), "previaLocal() reconhece a escolha");

  // A pessoa escolhe "Baixa" na Memoria do guia: o provedor local tem de sair
  // na MESMA chamada. Na versao reprovada ficava aplicado ate o heartbeat.
  valor[AJ_EPG_MEMORIA] = EPG_MEMORIA_BAIXA;
  ajEpgMemoriaAplicar();
  CHECK(!previaLocal(), "Baixa: o provedor local nao e mais capaz");
  CHECK(aplicado != 2, "Baixa desliga o provedor local na hora (aplicado=%d)", aplicado);

  // De volta a um modo capaz, a escolha gravada volta a valer.
  valor[AJ_EPG_MEMORIA] = EPG_MEMORIA_AUTO;
  ramMB = 4096;
  ajEpgMemoriaAplicar();
  CHECK(aplicado == 2, "Auto com RAM suficiente reaplica o provedor local (aplicado=%d)", aplicado);

  // Auto com pouca RAM segue a mesma regra do guia: provedor local de fora.
  ramMB = 1024;
  ajEpgMemoriaAplicar();
  CHECK(aplicado != 2, "Auto com 1024 MB nao e capaz (aplicado=%d)", aplicado);

  // Completa ignora a leitura de RAM: sempre capaz com as ferramentas.
  valor[AJ_EPG_MEMORIA] = EPG_MEMORIA_COMPLETA;
  ajEpgMemoriaAplicar();
  CHECK(aplicado == 2, "Completa aplica o provedor local mesmo com pouca RAM (aplicado=%d)", aplicado);

  // Sem as ferramentas do pacote o provedor local nunca aplica.
  tools = 0;
  previaAplicar();
  CHECK(aplicado != 2, "sem ferramentas o provedor local nao aplica (aplicado=%d)", aplicado);
  tools = 1;

  // O OpenSeek nao depende da regra de memoria: Baixa nao o tira.
  valor[AJ_PREVIA_PROVEDOR] = 1;
  valor[AJ_EPG_MEMORIA] = EPG_MEMORIA_BAIXA;
  previaAplicar();
  CHECK(aplicado == 1, "OpenSeek escolhido aplica (aplicado=%d)", aplicado);
  ajEpgMemoriaAplicar();
  CHECK(aplicado == 1, "Baixa nao tira o OpenSeek (aplicado=%d)", aplicado);

  printf(falhas ? "FAIL: %d assercoes\\n"
                : "PASS: Baixa desliga o provedor local na hora; regra do guia re-aplica\\n", falhas);
  return falhas ? 1 : 0;
}
'''
)
