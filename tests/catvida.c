// O CAMINHO DE ARTE QUE O DESENHO SEGURA NAO PODE MORRER NO MEIO DO QUADRO.
//
// O log de campo (LG, 1.3.3, 1.4.1 e 1.4.3) mostrou o cache de textura
// tentando abrir um "caminho" que era lixo binario:
//
//   [tex] decode falhou (Couldn't open ���̑C) tam=-1 magica=00000000: ���̑C
//
// logo depois de a home ser remontada com os catalogos dos addons. O cache
// copia a string na hora do pedido (tex_obter_limite), entao o lixo ja veio de
// quem pediu. E quem pede, na home, passa `item->backdrop` cru — um ponteiro
// para DENTRO do bloco de itens do catalogo, obtido por cat_item() no fio de
// desenho, sem trava.
//
// O bloco era trocado no fio da descoberta (cat_definir_tudo,
// cat_trocar_continuar, cat_acrescentar*) e o antigo morria na troca
// SEGUINTE. Duas trocas dentro de um quadro — a publicacao por fileira no
// arranque, ou a montagem publicando junto do fio de "Continuar assistindo" —
// liberavam o bloco em que o quadro ainda estava lendo. `backdrop` e o
// PRIMEIRO campo do CatItem: o backdrop do item 0 e o primeiro byte do bloco,
// justamente onde o malloc escreve o ponteiro da lista livre. Por isso o lixo
// tinha poucos bytes e terminava num NUL.
//
// O QUE ESTE TESTE PROVA (rode com SANITIZE=1 para o ASan acusar o uso):
//   1. um ponteiro de arte pego no comeco do quadro sobrevive a VARIAS trocas
//      dentro do mesmo quadro;
//   2. o mesmo vale misturando os tres tipos de troca;
//   3. o bloco aposentado e liberado depois que o quadro seguinte comecou —
//      a memoria nao cresce sem limite.
//
//   SANITIZE=1 bash tests/catvida.sh
#include "../src/catalogo.h"
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// --- DUBLES (os mesmos de tests/catfileira.c) --------------------------------
int         ajustes_idioma_ingles(void) { return 0; }
const char *i18n(const char *s)         { return s; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
const char *desc_genero_pt(const char *g) { return g; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; return 0;
}

#define N_ITENS 4
static CatItem lote[N_ITENS];
static CatFileira fil[1];

static void publicar(int geracao) {
  int i;
  memset(lote, 0, sizeof lote);
  memset(fil, 0, sizeof fil);
  for (i = 0; i < N_ITENS; i++) {
    snprintf(lote[i].backdrop, sizeof lote[i].backdrop,
             "https://images.metahub.space/background/medium/tt%07d/img", geracao * 10 + i);
    snprintf(lote[i].imdb, sizeof lote[i].imdb, "tt%07d", geracao * 10 + i);
    snprintf(lote[i].titulo, sizeof lote[i].titulo, "G%d-%d", geracao, i);
  }
  snprintf(fil[0].chave, sizeof fil[0].chave, "continue_watching");
  fil[0].ini = 0; fil[0].n = N_ITENS;
  cat_definir_tudo(lote, N_ITENS, fil, 1);
}

// O que tex_obter_limite faz com o caminho logo na entrada: percorre a string
// inteira (hashCaminho) e a copia para o slot.
static void pedirTextura(const char *caminho, char *slot, size_t n) {
  unsigned long h = 2166136261UL;
  const char *s;
  for (s = caminho; *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; }
  (void)h;
  strncpy(slot, caminho, n - 1);
  slot[n - 1] = 0;
}

int main(void) {
  char slot[512];

  // 1. Duas publicacoes da descoberta dentro de UM quadro da home.
  publicar(1);
  cat_quadro();
  {
    const CatItem *ci = cat_item(0);          // home: arte_por_identidade(0)
    const char *arte = ci->backdrop;           // artehero_url_card: cru
    publicar(2);                               // fio da descoberta
    publicar(3);                               // de novo, no mesmo quadro
    pedirTextura(arte, slot, sizeof slot);     // desenhaArteHero -> tex_obter_hero
    assert(!strcmp(slot, "https://images.metahub.space/background/medium/tt0000010/img"));
    printf("ok  arte pega no quadro sobrevive a duas trocas no mesmo quadro\n");
  }

  // 2. Os tres tipos de troca, cada um com seu lixo antes: a mistura liberava
  //    pela troca "do outro tipo" um bloco que o quadro ainda lia.
  cat_quadro();
  {
    const char *arte = cat_item(0)->backdrop;
    CatItem extra;
    memset(&extra, 0, sizeof extra);
    snprintf(extra.backdrop, sizeof extra.backdrop, "https://x.invalid/extra");
    snprintf(extra.imdb, sizeof extra.imdb, "tt9999999");
    cat_acrescentar(&extra);                   // detail/vertudo/biblioteca
    cat_acrescentar_lote(&extra, 1, NULL);     // busca/contalib
    cat_trocar_continuar(lote, 2);             // fioContinuar
    publicar(4);
    cat_acrescentar(&extra);
    cat_acrescentar_lote(&extra, 1, NULL);
    cat_trocar_continuar(lote, 3);
    pedirTextura(arte, slot, sizeof slot);
    assert(!strncmp(slot, "https://images.metahub.space/background/medium/", 47));
    printf("ok  sete trocas misturadas no mesmo quadro nao matam a arte em uso\n");
  }

  // 3. Passados os quadros, o que foi aposentado e liberado de fato.
  cat_quadro();
  cat_quadro();
  assert(cat_blocos_aposentados() <= 1);
  printf("ok  blocos aposentados sao liberados na virada do quadro (%d restando)\n",
         cat_blocos_aposentados());

  // 4. Sem quadro nenhum (fio de desenho parado), o teto segura a memoria.
  {
    int k;
    for (k = 0; k < 50; k++) publicar(10 + k);
    assert(cat_blocos_aposentados() <= 16);
    printf("ok  sem virada de quadro o numero de blocos aposentados tem teto (%d)\n",
           cat_blocos_aposentados());
  }

  printf("catvida: tudo ok\n");
  return 0;
}
