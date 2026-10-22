// A LEGENDA QUE A PESSOA ESCOLHEU A MAO, lembrada por perfil (2.0.3).
//
// O DEFEITO: "a legenda que escolhi nao volta quando retomo o filme; se a
// pessoa usa legenda, ela tem que vir ligada no idioma que ela escolheu"
// (dono, log da TCL). A legenda automatica (#129, faixas.c) so olhava a
// preferencia de Ajustes/da conta (ling_legenda). Sem ela — o caso de quem
// nunca abriu Ajustes e nao tem idioma na conta — a decisao era NADA logo no
// primeiro quadro, calada, e a escolha feita a mao na folha nao ficava em
// lugar nenhum: so legauto_lembrar, que e da execucao e so da legenda de addon
// que o AutoSync aprovou.
//
// O QUE FICA GUARDADO, num arquivo por perfil (legmemoria-p<N>.txt, como
// fontepref-p<N>.txt e pela mesma razao: a legenda da mae nao e a do filho):
//   - a ULTIMA ESCOLHA: o idioma da ultima legenda escolhida a mao, em
//     qualquer titulo, ou "none" quando a pessoa desligou a mao;
//   - por TITULO: a faixa exata (embutida: numero da faixa + idioma; addon: o
//     id de legendasui_id_addon) e o idioma dela.
//
// A ORDEM DA DECISAO (legmem_preferencia):
//   1. o MESMO titulo com escolha guardada: ela (a faixa exata se ainda
//      existir, senao o idioma dela; "none" = nenhuma). E a decisao mais
//      recente e mais especifica que a pessoa tomou sobre ESTE titulo.
//   2. o idioma de Ajustes/da conta, quando e um idioma de verdade.
//   3. a ultima escolha a mao, em outro titulo.
//   4. nada (o comportamento de antes).
// A regra do #287 (audio ja no idioma da legenda = so a forcada) continua
// valendo para o que sai daqui: quem decide a faixa pelo idioma e
// ling_legenda_auto_tipo, igual. MENOS no passo 1 quando a escolha foi uma
// legenda INTEIRA (#383): quem ligou a legenda em ingles numa serie em ingles
// quer a inteira no episodio seguinte, nao "so a forcada".
//
// SO A ESCOLHA MANUAL E GRAVADA. O que a automatica liga e o que o AutoSync
// troca nao viram escolha — senao a primeira decisao do app se perpetuaria
// como se a pessoa a tivesse feito.
#ifndef NV_LEGMEMORIA_H
#define NV_LEGMEMORIA_H

// 150 titulos x ~170 bytes = ~25 KB, estatico. Cheia, sai a mais antiga.
#define LEGMEM_MAX 150

typedef struct {
  char titulo[64];    // imdb do TITULO (sem ":temporada:episodio") ou o nome
  char idioma[16];    // codigo normalizado ("pt", "pob", "en"), "none", ou "" (faixa sem etiqueta)
  char tipo;          // 'e' embutida, 'a' addon, 0 = nenhuma
  int  numero;        // embutida: VideoFaixa.numero
  char id[24];        // addon: legendasui_id_addon; embutida: "c" inteira, "f" forcada/letreiros, "" antiga/desconhecida
  char nome[48];      // so para o log (rotulo da faixa / provedor)
  long long quandoS;
} LegMem;

// De onde veio a preferencia que legmem_preferencia devolveu.
#define LEGMEM_DE_NADA    0
#define LEGMEM_DE_TITULO  1
#define LEGMEM_DE_AJUSTE  2
#define LEGMEM_DE_ULTIMA  3

// Perfil corrente (perfis.c). Solta a tabela; relida na proxima consulta.
void legmem_definir_perfil(int perfil);
// Apaga do aparelho, de todos os perfis (sync_esquecer_usuario).
void legmem_esquecer(void);

// "tt1234567:2:4" -> "tt1234567": a serie inteira segue a mesma legenda.
void legmem_id_titulo(const char *id, char *dst, unsigned tam);

// Guarda a escolha MANUAL e grava. `e->titulo` pode vir com episodio. Com
// idioma conhecido (ou "none") ela vira tambem a ultima escolha.
void legmem_guardar(const LegMem *e);

const char   *legmem_ultima(void);                 // "" | "none" | codigo
const LegMem *legmem_do_titulo(const char *titulo); // NULL = sem escolha guardada

// A preferencia em vigor para este titulo (ver a ordem acima). `ajuste` =
// ling_legenda(). Puro.
const char *legmem_preferencia(const char *ajuste, const LegMem *doTitulo,
                               const char *ultima, int *origem);

// A FAIXA EXATA da escolha guardada na lista de agora, ou -1. Indice na lista
// combinada (embutidas, depois addons), como ling_legenda_auto. Puro.
//   embutida: mesmo numero e mesmo idioma (ling_casa); faixa sem etiqueta dos
//             dois lados casa pelo numero e pelo nome.
//   addon:    mesmo id (provedor + url).
int legmem_exata(const LegMem *m,
                 const int *numeros, const char *const *idiomas, const char *const *nomes, int nEmb,
                 const char *const *idsAdd, int nAdd);
#endif
