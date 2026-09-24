// Lista LOCAL de salvos: cresce alem do antigo teto de 300, vai inteira para o
// disco e, cheia, perde o MAIS ANTIGO — nunca o que acabou de ser salvo.
//
// O defeito (issue do Owlphibia29, "o contador nunca passou de 205; o que eu
// adiciono agora nao aparece nem substitui os antigos"): o 301o titulo era
// recusado com uma linha no log e o "+" acendia do mesmo jeito.
//
//   bash tests/salvos.sh
#include "salvos.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
static void confere(const char *o_que, int obtido, int esperado) {
  int ok = obtido == esperado;
  printf("  %-58s %s (obtido %d, esperado %d)\n", o_que, ok ? "ok   " : "FALHOU",
         obtido, esperado);
  if (!ok) falhas++;
}

// ---------------------------------------------------------------- dubles
// O disco e uma string: o que dados_gravar escreve, dados_ler devolve.
static char *disco;
int dados_gravar(const char *nome, const char *conteudo) {
  (void)nome; free(disco); disco = strdup(conteudo); return 1;
}
char *dados_ler(const char *nome) { (void)nome; return disco ? strdup(disco) : NULL; }
int dados_apagar(const char *nome) { (void)nome; free(disco); disco = NULL; return 1; }

// Catalogo de mentira. Vazio na primeira parte (o que se prova e a lista
// local); na parte da uniao ele recebe as copias que a TV tinha.
static CatItem cat[400];
static int nCat;
int cat_n(void) { return nCat; }
const CatItem *cat_item(int i) { return (i >= 0 && i < nCat) ? &cat[i] : NULL; }
int cat_indice_por_imdb(const char *imdb) {
  int i;
  for (i = 0; i < nCat; i++) if (salvos_mesmo_titulo(cat[i].imdb, imdb)) return i;
  return -1;
}
void cat_definir_na_lista(int i, int naLista) {
  if (i >= 0 && i < nCat) cat[i].naLista = naLista ? 1 : 0;
}
static void poeNoCatalogo(const char *imdb, const char *titulo, int naLista,
                          int progresso, int t, int e) {
  CatItem *c = &cat[nCat++];
  memset(c, 0, sizeof *c);
  snprintf(c->imdb, sizeof c->imdb, "%s", imdb);
  snprintf(c->tipo, sizeof c->tipo, "series");
  snprintf(c->titulo, sizeof c->titulo, "%s", titulo);
  c->naLista = naLista;
  c->progresso = progresso;
  c->temporada = t;
  c->episodio = e;
}
static CatItem serie;
static const CatItem *comId(const char *imdb, const char *titulo) {
  memset(&serie, 0, sizeof serie);
  snprintf(serie.imdb, sizeof serie.imdb, "%s", imdb);
  snprintf(serie.tipo, sizeof serie.tipo, "series");
  snprintf(serie.titulo, sizeof serie.titulo, "%s", titulo);
  return &serie;
}
// Quantas entradas da uniao sao o titulo `id`.
static int quantasVezes(const SalvosEntrada *u, int n, const char *id) {
  int i, k = 0;
  for (i = 0; i < n; i++) {
    const char *e = u[i].local >= 0 ? salvos_item(u[i].local)->id
                                    : cat_item(u[i].cat)->imdb;
    if (salvos_mesmo_titulo(e, id)) k++;
  }
  return k;
}

static CatItem ci;
static const CatItem *titulo(int i) {
  memset(&ci, 0, sizeof ci);
  snprintf(ci.imdb, sizeof ci.imdb, "tt%07d", 2000000 + i);
  snprintf(ci.tipo, sizeof ci.tipo, "movie");
  snprintf(ci.titulo, sizeof ci.titulo, "Filme %d", i);
  return &ci;
}


// A UNIAO QUADRATICA DE ANTES, copiada como referencia. salvos_uniao passou a
// ser uma passada so com tabela de espalhamento (o painel de Salvos a chama
// em toda reconstrucao e, na TV, cada leitura de CatItem era um desencontro de
// cache); a troca so vale se o resultado for IGUAL, na mesma ordem.
static int refMelhorCopia(const char *id) {
  int i, primeira = -1;
  for (i = 0; i < nCat; i++) {
    const CatItem *c = &cat[i];
    if (!c->imdb[0] || !salvos_mesmo_titulo(c->imdb, id)) continue;
    if (c->progresso > 0) return i;
    if (primeira < 0) primeira = i;
  }
  return primeira;
}
static int refUniao(SalvosEntrada *out, int cap) {
  int i, k = 0;
  for (i = 0; i < salvos_n() && k < cap; i++) {
    out[k].local = i; out[k].cat = refMelhorCopia(salvos_item(i)->id); k++;
  }
  for (i = 0; i < nCat && k < cap; i++) {
    const CatItem *c = &cat[i];
    int j, achou = -1;
    if (!c->naLista || !c->imdb[0] || salvos_tem(c->imdb)) continue;
    for (j = 0; j < k && achou < 0; j++)
      if (out[j].local < 0 && salvos_mesmo_titulo(cat[out[j].cat].imdb, c->imdb)) achou = j;
    if (achou >= 0) {
      if (c->progresso > 0 && cat[out[achou].cat].progresso <= 0) out[achou].cat = i;
      continue;
    }
    out[k].local = -1; out[k].cat = i; k++;
  }
  return k;
}
static unsigned semente = 12345u;
static int sorteio(int n) { semente = semente * 1103515245u + 12345u; return (int)((semente >> 8) % (unsigned)n); }

int main(void) {
  int i, entraram = 0, linhas = 0;
  const char *p;
  char id[24];

  printf("a lista local passa do antigo teto de 300:\n");
  salvos_iniciar();
  for (i = 0; i < 450; i++) entraram += salvos_definir(titulo(i), 1);
  confere("os 450 entraram", entraram, 450);
  confere("e estao na lista", salvos_n(), 450);
  confere("o ultimo salvo esta la", salvos_tem("tt2000449"), 1);
  for (p = disco; p && (p = strchr(p, '\n')); p++) linhas++;
  confere("o arquivo tem cabecalho + 450 linhas", linhas, 451);

  printf("\ncheia, sai o MAIS ANTIGO e o novo entra:\n");
  for (i = 450; i < SALVOS_MAX; i++) salvos_definir(titulo(i), 1);
  confere("lista no teto", salvos_n(), SALVOS_MAX);
  confere("o novo entrou mesmo com a lista cheia",
          salvos_definir(titulo(SALVOS_MAX), 1), 1);
  snprintf(id, sizeof id, "tt%07d", 2000000 + SALVOS_MAX);
  confere("e esta la", salvos_tem(id), 1);
  confere("o mais antigo saiu", salvos_tem("tt2000000"), 0);
  confere("o segundo mais antigo ficou", salvos_tem("tt2000001"), 1);
  confere("o tamanho nao passou do teto", salvos_n(), SALVOS_MAX);
  confere("e o mais novo e o ultimo da lista",
          strcmp(salvos_item(salvos_n() - 1)->id, id) == 0, 1);

  printf("\nremover continua valendo:\n");
  confere("tira o mais novo", salvos_definir(titulo(SALVOS_MAX), 0), 1);
  confere("e ele sumiu", salvos_tem(id), 0);

  salvos_esquecer();
  confere("esquecer zera a lista", salvos_n(), 0);
  confere("e apaga o arquivo", disco == NULL, 1);

  // O RELATO DA C9: o painel de Salvos mostrou Widows Bay duas vezes, as duas
  // no mesmo episodio. Lista local com "tt123", conta com "tt123" e o catalogo
  // com a copia de "Continuar assistindo" em "tt123:1:2" (marcada naLista pela
  // conta, porque cat_indice_por_imdb acha ela primeiro) e a da watchlist em
  // "tt123". O painel deduplicava com strcmp.
  printf("\no mesmo titulo com id de titulo e id de episodio:\n");
  {
    char b[64];
    salvos_id_titulo("tt14000001:1:2", b, sizeof b);
    confere("tt14000001:1:2 vira tt14000001", strcmp(b, "tt14000001") == 0, 1);
    salvos_id_titulo("tmdb:55:3:4", b, sizeof b);
    confere("tmdb:55:3:4 vira tmdb:55", strcmp(b, "tmdb:55") == 0, 1);
    salvos_id_titulo("tmdb:55", b, sizeof b);
    confere("tmdb:55 fica inteiro", strcmp(b, "tmdb:55") == 0, 1);
    salvos_id_titulo("cs:channel:abc", b, sizeof b);
    confere("canal cs:channel:abc fica inteiro", strcmp(b, "cs:channel:abc") == 0, 1);
    salvos_id_titulo("kitsu:12:1", b, sizeof b);
    confere("kitsu:12:1 fica inteiro (\"kitsu\" nao e titulo)",
            strcmp(b, "kitsu:12:1") == 0, 1);
    confere("tt123 e tt123:1:2 sao o mesmo titulo",
            salvos_mesmo_titulo("tt123", "tt123:1:2"), 1);
    confere("tt123:1:2 e tt123:2:5 (dois episodios) sao o mesmo titulo",
            salvos_mesmo_titulo("tt123:1:2", "tt123:2:5"), 1);
    confere("kitsu:12:1 e kitsu:12:2 nao sao (episodios de anime)",
            salvos_mesmo_titulo("kitsu:12:1", "kitsu:12:2"), 0);
    confere("tt123 e tt1234 nao sao",
            salvos_mesmo_titulo("tt123", "tt1234"), 0);
    confere("tmdb:55 e tmdb:5 nao sao",
            salvos_mesmo_titulo("tmdb:55", "tmdb:5"), 0);
    confere("cs:channel:a e cs:channel:b nao sao",
            salvos_mesmo_titulo("cs:channel:a", "cs:channel:b"), 0);
  }

  printf("\na uniao do painel mostra Widows Bay UMA vez:\n");
  {
    SalvosEntrada u[16];
    int n, i, achou = -1;
    salvos_definir(comId("tt14000001", "Widows Bay"), 1);
    salvos_definir(comId("tt0000002", "Outro"), 1);
    nCat = 0;
    poeNoCatalogo("tt14000001:1:2", "Widows Bay", 1, 40, 1, 2); // Continuar
    poeNoCatalogo("tt0000009", "Da conta", 1, 0, 0, 0);
    poeNoCatalogo("tt14000001", "Widows Bay", 1, 0, 0, 0);      // watchlist
    poeNoCatalogo("tt0000009", "Da conta", 1, 0, 0, 0);         // outra fileira
    poeNoCatalogo("tt0000010", "Nao salvo", 0, 0, 0, 0);
    n = salvos_uniao(u, 16);
    confere("tres titulos: 2 locais + 1 so da conta", n, 3);
    confere("Widows Bay uma vez", quantasVezes(u, n, "tt14000001"), 1);
    confere("o da conta uma vez", quantasVezes(u, n, "tt0000009"), 1);
    confere("o nao salvo nao entra", quantasVezes(u, n, "tt0000010"), 0);
    for (i = 0; i < n; i++)
      if (u[i].local >= 0 && !strcmp(salvos_item(u[i].local)->id, "tt14000001"))
        achou = i;
    confere("a linha local leva a copia COM progresso (T1E2)",
            achou >= 0 && u[achou].cat == 0, 1);

    // Copia sem progresso primeiro, com progresso depois, nenhuma local.
    salvos_definir(comId("tt14000001", "Widows Bay"), 0);
    nCat = 0;
    poeNoCatalogo("tt14000001", "Widows Bay", 1, 0, 0, 0);
    poeNoCatalogo("tt14000001:1:2", "Widows Bay", 1, 40, 1, 2);
    n = salvos_uniao(u, 16);
    confere("so no catalogo, duas copias: uma entrada", quantasVezes(u, n, "tt14000001"), 1);
    for (i = 0, achou = -1; i < n; i++) if (u[i].local < 0) achou = i;
    confere("e ela aponta para a copia com progresso",
            achou >= 0 && cat_item(u[achou].cat)->progresso == 40, 1);
    nCat = 0;
  }

  printf("\no + do card de Continuar guarda o id do titulo:\n");
  salvos_esquecer();
  confere("salvar tt14000001:1:2", salvos_definir(comId("tt14000001:1:2", "Widows Bay"), 1), 1);
  confere("fica guardado como tt14000001",
          strcmp(salvos_item(0)->id, "tt14000001") == 0, 1);
  confere("salvar tt14000001 depois nao duplica",
          salvos_definir(comId("tt14000001", "Widows Bay"), 1), 0);
  confere("a lista tem 1", salvos_n(), 1);
  confere("remover pelo id do episodio tira o titulo",
          salvos_definir(comId("tt14000001:2:1", "Widows Bay"), 0), 1);
  confere("e a lista fica vazia", salvos_n(), 0);

  printf("\narquivo antigo com o titulo duas vezes e lido uma vez:\n");
  salvos_esquecer();
  dados_gravar("salvos.txt",
               "# nuvio salvos v1\n"
               "tt14000001:1:2\tseries\t100\t0\t2026\t\tWidows Bay\n"
               "tt14000001\tseries\t200\t0\t2026\t\tWidows Bay\n"
               "tt0000002\tmovie\t300\t0\t2020\t\tOutro\n");
  salvos_iniciar();
  confere("dois titulos", salvos_n(), 2);
  confere("o primeiro com o id do titulo",
          strcmp(salvos_item(0)->id, "tt14000001") == 0, 1);
  printf("\na uniao em uma passada e igual a quadratica (300 sorteios):\n");
  { static SalvosEntrada a[800], b[800];
    int rodada, iguais = 0;
    for (rodada = 0; rodada < 300; rodada++) {
      int nt = 3 + sorteio(40), k, na, nb, ok = 1;
      salvos_esquecer();
      salvos_iniciar();
      nCat = 0;
      // Poucos titulos, muitas copias: e onde as duas regras de copia
      // (primeira com progresso; copia marcada so troca se so ela tem) se
      // distinguem. Ids simples, com episodio, de canal e kitsu.
      for (k = 0; k < 2 + sorteio(12); k++) {
        char idl[32];
        snprintf(idl, sizeof idl, "tt%07d", 100 + sorteio(nt));
        salvos_definir(comId(idl, "Local"), 1);
      }
      for (k = 0; k < 20 + sorteio(300) && nCat < 400; k++) {
        char idc[64];
        int t = 100 + sorteio(nt), forma = sorteio(5);
        if (forma == 0) snprintf(idc, sizeof idc, "tt%07d:%d:%d", t, 1 + sorteio(3), 1 + sorteio(9));
        else if (forma == 1) snprintf(idc, sizeof idc, "cs:channel:canal-%d", t);
        else if (forma == 2) snprintf(idc, sizeof idc, "kitsu:%d:%d", t, 1 + sorteio(4));
        else snprintf(idc, sizeof idc, "tt%07d", t);
        poeNoCatalogo(idc, "Copia", sorteio(3) == 0, sorteio(4) == 0 ? 10 + sorteio(80) : 0, 0, 0);
      }
      na = salvos_uniao(a, 800);
      nb = refUniao(b, 800);
      if (na != nb) ok = 0;
      for (k = 0; ok && k < na; k++)
        if (a[k].local != b[k].local || a[k].cat != b[k].cat) ok = 0;
      if (ok) iguais++;
      else if (iguais == rodada) printf("  primeira divergencia na rodada %d: %d x %d entradas\n", rodada, na, nb);
    }
    confere("as 300 rodadas deram a mesma uniao", iguais, 300);
    nCat = 0; }

  printf("\na revisao da lista sobe a cada mudanca:\n");
  { unsigned r0;
    salvos_esquecer();
    salvos_iniciar();
    r0 = salvos_revisao();
    salvos_definir(comId("tt0000777", "Um"), 1);
    confere("salvar sobe a revisao", salvos_revisao() != r0, 1);
    r0 = salvos_revisao();
    salvos_definir(comId("tt0000777", "Um"), 1);
    confere("salvar o que ja esta nao sobe", salvos_revisao() == r0, 1);
    salvos_definir(comId("tt0000777", "Um"), 0);
    confere("remover sobe", salvos_revisao() != r0, 1);
    r0 = salvos_revisao();
    salvos_esquecer();
    confere("esquecer sobe", salvos_revisao() != r0, 1); }
  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
