// NORMALIZACAO DA BUSCA (#176): a busca so entendia a-z e Latin-1. Termo em
// cirilico ou com ș/ț/ă virava espacos (o que casava com qualquer titulo) e o
// "apagar" cortava UTF-8 no meio do byte.
//
//   bash tests/buscanorm.sh
#include "../src/buscanorm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void igual(const char *entrada, const char *esperado) {
  char d[128];
  busca_normalizar(entrada, d, sizeof d);
  if (strcmp(d, esperado)) {
    printf("FALHOU: \"%s\" -> \"%s\" (esperado \"%s\")\n", entrada, d, esperado);
    assert(0);
  }
}

int main(void) {
  // Latino: o de sempre.
  igual("The Invite", "the invite");
  igual("Funda\xc3\xa7\xc3\xa3o", "fundacao");
  igual("A\xc3\xa7\xc3\xa3o \xc3\x89 Caf\xc3\xa9", "acao e cafe");
  puts("ok  latino de sempre (Latin-1) continua dobrando");

  // Romeno: com virgula (ș ț), com cedilha (ş ţ) e ă â î.
  igual("\xc8\x98tefan \xc8\x9a\xc4\x83r\xc4\x83", "stefan tara");       /* Ștefan Țără */
  igual("\xc5\x9f\xc5\xa3 \xc3\xa2\xc3\xae \xc4\x82", "st ai a");        /* ş ţ â î Ă */
  igual("Pl\xc4\x83" "ce\xc8\x9b" "i", "placeti");
  puts("ok  romeno (ă â î ș ț, virgula e cedilha) dobra para ASCII");

  // Cirilico: minusculo, sem virar espaco; ё -> е.
  igual("\xd0\x94\xd1\x80\xd1\x83\xd0\xb7\xd1\x8c\xd1\x8f",              /* Друзья */
        "\xd0\xb4\xd1\x80\xd1\x83\xd0\xb7\xd1\x8c\xd1\x8f");
  igual("\xd0\x81\xd0\xbb\xd0\xba\xd0\xb0", "\xd0\xb5\xd0\xbb\xd0\xba\xd0\xb0");   /* Ёлка -> елка */
  igual("\xd2\x90\xd0\xb0\xd0\xbd\xd0\xb3", "\xd2\x91\xd0\xb0\xd0\xbd\xd0\xb3");   /* Ґанг -> ґанг */
  igual("\xd0\x86\xd0\xb3\xd0\xbe\xd1\x80", "\xd1\x96\xd0\xb3\xd0\xbe\xd1\x80");   /* Ігор */
  { char d[64];
    busca_normalizar("\xd0\x94\xd1\x80\xd1\x83\xd0\xb7\xd1\x8c\xd1\x8f", d, sizeof d);
    assert(strchr(d, ' ') == NULL);   /* antes: 12 espacos */ }
  puts("ok  cirilico: minusculo, ё=е, nunca vira espaco");

  // Outras escritas passam como vieram; byte invalido vira espaco.
  igual("\xe6\x97\xa5\xe6\x9c\xac", "\xe6\x97\xa5\xe6\x9c\xac");
  igual("a\xff" "b", "a b");
  igual("", "");
  puts("ok  outras escritas passam; byte invalido vira espaco");

  // Nao corta no meio de uma sequencia quando o destino e curto.
  { char d[4];
    busca_normalizar("\xd0\x90\xd0\x91\xd0\x92", d, sizeof d);   /* 3 letras, 6 bytes */
    assert(strlen(d) == 2 && !strcmp(d, "\xd0\xb0"));            /* cabe 1 (2 bytes) */ }
  puts("ok  destino curto: para na fronteira de caractere");

  assert(busca_codepoints("abc") == 3);
  assert(busca_codepoints("\xd0\xb0\xd0\xb1") == 2);
  assert(busca_codepoints("") == 0);
  { char s[16] = "ab\xc8\x99";
    size_t n = busca_apagar_ultimo(s, strlen(s));
    assert(n == 2 && !strcmp(s, "ab"));
    n = busca_apagar_ultimo(s, n);
    assert(n == 1 && !strcmp(s, "a"));
    n = busca_apagar_ultimo(s, n); n = busca_apagar_ultimo(s, n);
    assert(n == 0 && s[0] == 0); }
  puts("ok  contagem por caractere e apagar tira um caractere inteiro");
  // Cursor do teclado do app: inserir e apagar no meio, por caractere.
  { char s[8] = "ab\xc8\x99";   /* "abș", 4 bytes */
    int c = -1;
    size_t n = strlen(s);
    assert(busca_byte_cursor(s, -1) == 4 && busca_byte_cursor(s, 2) == 2 && busca_byte_cursor(s, 9) == 4);
    assert(busca_cursor_mover(s, &c, 1) == 0 && c == -1);   /* Direita no fim: passa adiante */
    assert(busca_cursor_mover(s, &c, -1) && c == 2);
    assert(busca_cursor_mover(s, &c, -1) && c == 1);
    n = busca_inserir(s, n, sizeof s, &c, "X");
    assert(!strcmp(s, "aXb\xc8\x99") && c == 2 && n == 5);
    n = busca_inserir(s, n, sizeof s, &c, "\xc8\x99YZ");   /* nao cabe inteiro: nada muda */
    assert(n == 5 && c == 2);
    n = busca_apagar_antes(s, n, &c);
    assert(!strcmp(s, "ab\xc8\x99") && c == 1 && n == 4);
    n = busca_apagar_antes(s, n, &c);
    assert(!strcmp(s, "b\xc8\x99") && c == 0 && n == 3);
    assert(busca_apagar_antes(s, n, &c) == 3 && c == 0);     /* no comeco: nada */
    assert(busca_cursor_mover(s, &c, -1) == 0 && c == 0);    /* Esquerda no comeco */
    assert(busca_cursor_mover(s, &c, 1) && c == 1);
    assert(busca_cursor_mover(s, &c, 1) && c == -1);         /* chegou ao fim */
    c = -1; n = busca_apagar_antes(s, n, &c);
    assert(!strcmp(s, "b") && c == -1 && n == 1);
    n = busca_inserir(s, n, sizeof s, &c, "c");
    assert(!strcmp(s, "bc") && c == -1); }
  puts("ok  cursor: inserir/apagar no meio por caractere, pontas e fim");
  puts("buscanorm: tudo ok");
  return 0;
}
