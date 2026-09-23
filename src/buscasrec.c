// Buscas recentes: lista por perfil em disco. Regras e porques em buscasrec.h.
//
// FORMATO: texto, um termo por linha, o mais recente primeiro, com uma linha de
// comentario "#" no topo para quem abrir o arquivo por ssh na TV saber o que e.
// Texto e nao binario pelo mesmo motivo de bibliotecaui-p<N>.txt: ~500 bytes no
// pior caso (10 x 47), e ler com `cat` vale mais que qualquer economia.
//
// GRAVACAO POR dados_gravar (a atomica, descarga de 700 ms no Tizen), e nao
// dados_gravar_leve: e dado do usuario, nao cache re-obtivel — perder a lista
// porque a TV desligou 10 s depois da busca e justamente o defeito a evitar. O
// custo e uma gravacao por busca CONFIRMADA (nao por letra; ver busca.c).
#include "buscasrec.h"
#include "dados.h"
#include "perfis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Ate onde buscasrec_esquecer varre. CONTA_PERFIL_MAX e 8 hoje; 16 cobre o
// dobro pelo mesmo motivo de FONTEPREF_PERFIS em fontepref.c — sao
// dados_apagar em arquivo que quase sempre nao existe, custo zero.
#define BR_PERFIS 16

static char termos[BUSCASREC_MAX][BUSCASREC_TERMO];
static int  nTermos = 0;
static int  perfilCarregado = -1;   // -1 = nada lido ainda

static const char *arquivo(int p) {
  static char nome[48];
  if (p <= 0) snprintf(nome, sizeof nome, "buscas.txt");
  else        snprintf(nome, sizeof nome, "buscas-p%d.txt", p);
  return nome;
}

// Copia `s` aparado nas pontas (espaco, tab, quebra de linha) para `dst`, e
// troca controle no MEIO por espaco — uma quebra de linha dentro do termo
// partiria o registro em dois na proxima leitura.
static void aparar(const char *s, char *dst, size_t tam) {
  const char *ini = s, *fim;
  size_t n, i;
  if (!s) { dst[0] = 0; return; }
  while (*ini == ' ' || *ini == '\t' || *ini == '\n' || *ini == '\r') ini++;
  fim = ini + strlen(ini);
  while (fim > ini && (fim[-1] == ' ' || fim[-1] == '\t' ||
                       fim[-1] == '\n' || fim[-1] == '\r')) fim--;
  n = (size_t)(fim - ini);
  if (n >= tam) n = tam - 1;
  for (i = 0; i < n; i++) {
    unsigned char c = (unsigned char)ini[i];
    dst[i] = c < 0x20 ? ' ' : (char)c;
  }
  dst[n] = 0;
}

// Igualdade SEM CAIXA so no ASCII: o teclado da tela so produz a-z0-9 e
// espaco, e o fisico passa pelo mesmo filtro em busca_evento. Byte acima de
// 0x7F compara exato, o que e o certo para UTF-8 sem tabela de caixa.
static int iguais(const char *a, const char *b) {
  for (;; a++, b++) {
    unsigned char x = (unsigned char)*a, y = (unsigned char)*b;
    if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32);
    if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32);
    if (x != y) return 0;
    if (!x) return 1;
  }
}

static void gravar(void) {
  char txt[64 + BUSCASREC_MAX * (BUSCASREC_TERMO + 1)];
  size_t k;
  int i;
  if (nTermos == 0) { dados_apagar(arquivo(perfilCarregado)); return; }
  k = (size_t)snprintf(txt, sizeof txt,
                       "# Buscas recentes deste perfil, mais recente primeiro.\n");
  for (i = 0; i < nTermos && k < sizeof txt; i++)
    k += (size_t)snprintf(txt + k, sizeof txt - k, "%s\n", termos[i]);
  dados_gravar(arquivo(perfilCarregado), txt);
}

// Le o perfil ativo se ele mudou. Arquivo editado a mao com duplicata, termo
// curto ou mais de 10 linhas e SANEADO na leitura pelas mesmas regras da
// gravacao — a lista em memoria nunca viola o contrato de buscasrec.h.
static void garantir(void) {
  int p = perfis_ativo();
  char *b, *linha, *prox;
  if (p == perfilCarregado) return;
  perfilCarregado = p;
  nTermos = 0;
  b = dados_ler(arquivo(p));
  if (!b) return;
  for (linha = b; linha && *linha && nTermos < BUSCASREC_MAX; linha = prox) {
    char t[BUSCASREC_TERMO];
    int i, dup = 0;
    prox = strchr(linha, '\n');
    if (prox) *prox++ = 0;
    if (linha[0] == '#') continue;
    aparar(linha, t, sizeof t);
    if (strlen(t) < 2) continue;
    for (i = 0; i < nTermos && !dup; i++) dup = iguais(termos[i], t);
    if (dup) continue;
    memcpy(termos[nTermos++], t, sizeof t);
  }
  free(b);
}

int buscasrec_n(void) { garantir(); return nTermos; }

const char *buscasrec_termo(int i) {
  garantir();
  return (i >= 0 && i < nTermos) ? termos[i] : "";
}

int buscasrec_registrar(const char *termo) {
  char t[BUSCASREC_TERMO];
  int i, achou = -1;
  garantir();
  aparar(termo, t, sizeof t);
  if (strlen(t) < 2) return 0;
  for (i = 0; i < nTermos; i++)
    if (iguais(termos[i], t)) { achou = i; break; }
  // Ja no topo com a MESMA grafia: nada muda, e nao regravar poupa uma
  // descarga para o IndexedDB a cada entrada nos resultados da mesma busca.
  if (achou == 0 && !strcmp(termos[0], t)) return 1;
  // Desloca para baixo quem estava acima do repetido (ou todos, ate o teto,
  // se e novo) e poe o termo no topo. O decimo primeiro cai fora.
  { int ate = achou >= 0 ? achou : (nTermos < BUSCASREC_MAX ? nTermos : BUSCASREC_MAX - 1);
    memmove(termos[1], termos[0], (size_t)ate * BUSCASREC_TERMO);
    if (achou < 0 && nTermos < BUSCASREC_MAX) nTermos++; }
  memcpy(termos[0], t, sizeof t);
  gravar();
  return 1;
}

void buscasrec_remover(int i) {
  garantir();
  if (i < 0 || i >= nTermos) return;
  memmove(termos[i], termos[i + 1], (size_t)(nTermos - i - 1) * BUSCASREC_TERMO);
  nTermos--;
  gravar();
}

void buscasrec_limpar(void) {
  garantir();
  nTermos = 0;
  gravar();
}

void buscasrec_esquecer(void) {
  int p;
  // TODOS os perfis, e nao so o ativo: ao sair da conta os perfis somem junto
  // (perfis_esquecer), e um buscas-p3.txt que ficasse seria lido pela proxima
  // pessoa que caisse no perfil 3 da conta dela. Mesmo argumento de
  // fontepref_esquecer.
  for (p = 0; p <= BR_PERFIS; p++) dados_apagar(arquivo(p));
  nTermos = 0;
  perfilCarregado = -1;
}
