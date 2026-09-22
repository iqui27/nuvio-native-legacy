// Ver listas.h para o que cada fonte entrega de verdade. Aqui fica o COMO.
#include "listas.h"
#include "trakt.h"
#include "simklauth.h"
#include "nuvem.h"
#include "descoberta.h"
#include "colecoes.h"
#include "fileiras.h"
#include "dados.h"
#include "perfis.h"
#include "rede.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>

// A TABELA E COMPARTILHADA COM UM FIO DE REDE, entao tudo que a tela le passa
// por esta trava. E a mesma disciplina de vtTrava em descoberta.c: o fio
// escreve o lote inteiro de uma vez e a tela nunca ve meia lista.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static LstLista  tabela[LST_MAX];
static int       nTabela;
static char      aviso[160];
static int       fioVivo;
static unsigned  geracao;          // invalida o resultado de um pedido trocado

// O QUE ESTE FIO FOI PEDIR. Copiado sob trava antes de sair, para o fio nunca
// ler um pedido que a pessoa ja trocou.
enum { PED_NADA = 0, PED_TRAKT_MINHAS, PED_TRAKT_BUSCA, PED_SIMKL_ITENS };
static int  pedido;
static char termoBusca[96];

// --------------------------------------------------------------- fixadas

typedef struct {
  int  fonte;
  char id[96];       // traktId em texto, colId, ou o estado do Simkl
  char midia[8];
  int  naHome;
  char titulo[128];
  char autor[64];
} LstFixa;
static LstFixa fixas[LST_FIX_MAX];
static int     nFixas;
static int     fixasLidas;
static int     perfilLido = -1;

const char *lst_arquivo(void) {
  static char nome[64];
  int p = perfis_ativo();
  if (p <= 0) { snprintf(nome, sizeof nome, "listas.txt"); return nome; }
  snprintf(nome, sizeof nome, "listas-p%d.txt", p);
  return nome;
}

// Campo de TSV: tabulacao e quebra viram espaco. Um titulo de lista com
// tabulacao dentro parte a linha em duas e a leitura seguinte pega lixo — a
// mesma guarda que agenda.c ja tem.
static void limpo(char *dst, size_t tam, const char *s) {
  size_t i = 0;
  if (!tam) return;
  if (!s) { dst[0] = 0; return; }
  for (; s[i] && i + 1 < tam; i++)
    dst[i] = (s[i] == '\t' || s[i] == '\n' || s[i] == '\r') ? ' ' : s[i];
  dst[i] = 0;
}

// A CHAVE de uma lista, que e o que sobrevive no arquivo. O id do Trakt e
// numerico, o da colecao e texto e o do Simkl e o nome do estado: um so campo
// cobre os tres porque eles nunca se misturam (a fonte vai na mesma linha).
static void chaveDe(const LstLista *l, char *dst, size_t n) {
  if (!l) { if (n) dst[0] = 0; return; }
  if (l->fonte == LST_TRAKT)      snprintf(dst, n, "%ld", l->traktId);
  else if (l->fonte == LST_NUVIO) snprintf(dst, n, "%s", l->colId);
  else                            snprintf(dst, n, "%s", l->estado);
}

static const char *nomeFonte(int f) {
  return f == LST_TRAKT ? "trakt" : f == LST_SIMKL ? "simkl" : "nuvio";
}
static int fonteDeNome(const char *s) {
  if (!strcmp(s, "trakt")) return LST_TRAKT;
  if (!strcmp(s, "simkl")) return LST_SIMKL;
  return LST_NUVIO;
}

static void aplicarHome(void);

static void gravarFixas(void) {
  char txt[LST_FIX_MAX * 320 + 256];
  size_t k = 0;
  int i;
  // O comentario vai NO ARQUIVO, pelo mesmo motivo de fileiras.c: quem o
  // encontrar primeiro sera alguem depurando a TV de outra pessoa.
  k += (size_t)snprintf(txt + k, sizeof txt - k,
      "# Listas fixadas na Biblioteca deste perfil.\n"
      "# fonte\\tid\\tmidia\\tnaHome\\ttitulo\\tautor\n");
  for (i = 0; i < nFixas && k + 320 < sizeof txt; i++)
    k += (size_t)snprintf(txt + k, sizeof txt - k, "%s\t%s\t%s\t%d\t%s\t%s\n",
                          nomeFonte(fixas[i].fonte), fixas[i].id,
                          fixas[i].midia[0] ? fixas[i].midia : "-",
                          fixas[i].naHome, fixas[i].titulo, fixas[i].autor);
  dados_gravar(lst_arquivo(), txt);
}

static void lerFixas(void) {
  char *b, *linha, *fim;
  int p = perfis_ativo();
  if (fixasLidas && p == perfilLido) return;
  nFixas = 0; fixasLidas = 1; perfilLido = p;
  b = dados_ler(lst_arquivo());
  if (!b) { aplicarHome(); return; }
  for (linha = b; *linha; linha = fim) {
    char *campo[6]; int c = 0; char *s;
    fim = strchr(linha, '\n');
    if (fim) *fim++ = 0; else fim = linha + strlen(linha);
    if (linha[0] == '#' || !linha[0]) continue;
    campo[c++] = linha;
    for (s = linha; *s && c < 6; s++) if (*s == '\t') { *s = 0; campo[c++] = s + 1; }
    if (c < 5 || nFixas >= LST_FIX_MAX) continue;
    { LstFixa *f = &fixas[nFixas];
      memset(f, 0, sizeof *f);
      f->fonte = fonteDeNome(campo[0]);
      snprintf(f->id, sizeof f->id, "%s", campo[1]);
      if (strcmp(campo[2], "-")) snprintf(f->midia, sizeof f->midia, "%s", campo[2]);
      f->naHome = atoi(campo[3]) ? 1 : 0;
      snprintf(f->titulo, sizeof f->titulo, "%s", campo[4]);
      if (c > 5) snprintf(f->autor, sizeof f->autor, "%s", campo[5]);
      if (f->id[0] && f->titulo[0]) nFixas++; }
  }
  free(b);
  // TROCA DE PERFIL: as pastas que a Home recebe sao as do perfil ATIVO. Sem
  // reaplicar aqui, quem trocasse de perfil continuaria com as fileiras do
  // perfil anterior ate reabrir a Biblioteca.
  aplicarHome();
}

static int acharFixa(const LstLista *l) {
  char ch[96];
  int i;
  if (!l) return -1;
  chaveDe(l, ch, sizeof ch);
  for (i = 0; i < nFixas; i++)
    if (fixas[i].fonte == l->fonte && !strcmp(fixas[i].id, ch)) return i;
  return -1;
}

int lst_fixada(const LstLista *l) { lerFixas(); return acharFixa(l) >= 0; }
int lst_na_home(const LstLista *l) {
  int i;
  lerFixas();
  i = acharFixa(l);
  return i >= 0 && fixas[i].naHome;
}

int lst_alternar_fixada(const LstLista *l) {
  int i;
  lerFixas();
  i = acharFixa(l);
  if (i >= 0) {
    // Desfixar tambem tira da Home: a fileira existia porque a lista estava
    // fixada aqui. Deixar a fileira orfa daria uma linha na home que nenhuma
    // tela sabe mais desligar.
    memmove(&fixas[i], &fixas[i + 1], sizeof fixas[0] * (size_t)(nFixas - i - 1));
    nFixas--;
    gravarFixas();
    aplicarHome();
    return 0;
  }
  if (nFixas >= LST_FIX_MAX) return 0;
  { LstFixa *f = &fixas[nFixas];
    memset(f, 0, sizeof *f);
    f->fonte = l->fonte;
    chaveDe(l, f->id, sizeof f->id);
    snprintf(f->midia, sizeof f->midia, "%s", l->midia);
    limpo(f->titulo, sizeof f->titulo, l->titulo);
    limpo(f->autor, sizeof f->autor, l->autor);
    nFixas++; }
  gravarFixas();
  return 1;
}

int lst_aceita_home(const LstLista *l, const char **porque) {
  if (porque) *porque = "";
  if (!l) return 0;
  if (l->fonte == LST_SIMKL) {
    // Ver listas.h: a fileira de colecao sabe pedir catalogo de addon, TMDB e
    // Trakt. Nao ha provedor Simkl em descoberta.c, e inventar um aqui seria
    // um segundo mecanismo de fileira para manter.
    if (porque) *porque = "A Home ainda não busca listas do Simkl";
    return 0;
  }
  if (l->fonte == LST_TRAKT && l->traktId <= 0) {
    if (porque) *porque = "Esta lista não tem id no Trakt";
    return 0;
  }
  if (l->fonte == LST_TRAKT && !nuvem_trakt_cliente()[0]) {
    if (porque) *porque = "Este pacote não tem a chave do Trakt";
    return 0;
  }
  return 1;
}

// A LISTA VIRA PASTA DE COLECAO. Nao ha formato novo: e o mesmo ColSource que
// colecoes.c monta quando o editor do site grava provider "trakt", e por isso
// desc_vertudo_fonte busca os itens sem uma linha de codigo nova.
#define LST_GRUPO_HOME "Minhas listas"

static void aplicarHome(void) {
  ColFolder pastas[LST_FIX_MAX];
  int n = 0, i;
  for (i = 0; i < nFixas && n < LST_FIX_MAX; i++) {
    ColFolder *v;
    if (!fixas[i].naHome || fixas[i].fonte != LST_TRAKT) continue;
    v = &pastas[n];
    memset(v, 0, sizeof *v);
    snprintf(v->group, sizeof v->group, "%s", LST_GRUPO_HOME);
    snprintf(v->groupId, sizeof v->groupId, "%s", "listas_fixadas");
    snprintf(v->id, sizeof v->id, "lista_trakt_%s", fixas[i].id);
    snprintf(v->title, sizeof v->title, "%s", fixas[i].titulo);
    { ColSource *s = &v->sources[0];
      snprintf(s->prov, sizeof s->prov, "trakt");
      s->traktLista = atol(fixas[i].id);
      snprintf(s->title, sizeof s->title, "%s", fixas[i].titulo);
      snprintf(s->midia, sizeof s->midia, "%s",
               fixas[i].midia[0] ? fixas[i].midia : "MOVIE");
      snprintf(s->ordenar, sizeof s->ordenar, "rank");
      snprintf(s->ordem, sizeof s->ordem, "asc");
      v->nSources = 1; }
    n++;
  }
  col_extra_definir(pastas, n);
}

int lst_alternar_home(const LstLista *l) {
  int i;
  const char *porque;
  if (!lst_aceita_home(l, &porque)) return 0;
  lerFixas();
  // Levar para a Home IMPLICA fixar: a pasta que a Home desenha e montada a
  // partir da linha do arquivo, e sem a linha nao ha o que montar.
  i = acharFixa(l);
  if (i < 0) { if (!lst_alternar_fixada(l)) return 0; i = acharFixa(l); }
  if (i < 0) return 0;
  fixas[i].naHome = !fixas[i].naHome;
  gravarFixas();
  if (l->fonte == LST_NUVIO) {
    // A pasta Nuvio JA e uma colecao e ja tem fileira propria na Home — a do
    // GRUPO dela. O que muda e o liga/desliga dessa fileira em fileiras.c, que
    // e onde essa escolha mora para todas as outras fileiras da home. Criar uma
    // segunda fileira para a mesma pasta a mostraria duas vezes.
    char chave[192] = "";
    int j, k;
    for (j = 0; j < col_n(); j++) {
      const ColFolder *f = col_folder(j);
      if (f && !strcmp(f->id, l->colId)) {
        col_chave_grupo(f->group, chave, sizeof chave); break; }
    }
    for (k = 0; chave[0] && k < fil_n(); k++) {
      if (strcmp(fil_chave(k), chave)) continue;
      if (fixas[i].naHome) { int e = 0; fil_adicionar(k, &e); }
      else                   fil_remover(k);
      break;
    }
  } else aplicarHome();
  return fixas[i].naHome;
}

void lst_iniciar(void) {
  fixasLidas = 0;
  lerFixas();
  aplicarHome();
}

void lst_esquecer(void) {
  pthread_mutex_lock(&trava);
  nTabela = 0; aviso[0] = 0; geracao++;
  pthread_mutex_unlock(&trava);
  nFixas = 0; fixasLidas = 0; perfilLido = -1;
  col_extra_definir(NULL, 0);
}

void lst_esquecer_conta(void) {
  // Apaga ANTES de soltar a memoria: lst_arquivo() depende de perfis_ativo(),
  // que continua valendo aqui, e nao do que ficou em `perfilLido`.
  dados_apagar(lst_arquivo());
  lst_esquecer();
}

// ------------------------------------------------------------ leitura JSON

// VALOR CRU DE UMA CHAVE DE PROFUNDIDADE 1, e nao js_bruto.
//
// js_bruto casa a PRIMEIRA ocorrencia do nome no texto, o que e certo nos
// documentos rasos para os quais ele nasceu e errado aqui: o objeto de lista do
// Trakt tem "ids" na raiz e OUTRO "ids" dentro de "user", e a ordem dos campos
// na resposta nao e contrato de API nenhum. Trocada a ordem, o id da lista
// viraria o slug do dono — em silencio, porque o sintoma seria uma lista que
// abre vazia. E o mesmo motivo por que js_texto_raiz_em existe.
//
// Exige ':' depois do nome: sem isso `{"type":"list","list":{...}}` casaria com
// o VALOR "list" antes de chegar na chave.
static int brutoRaiz(const char *ini, const char *fim, const char *chave,
                     char *dst, size_t tam) {
  const char *p;
  size_t nChave;
  int prof = 0;
  if (!ini || !chave || !dst || !tam) return 0;
  dst[0] = 0;
  nChave = strlen(chave);
  p = strchr(ini, '{');
  if (!p || (fim && p >= fim)) return 0;
  for (; *p && (!fim || p < fim); p++) {
    if (*p == '"') {
      const char *i2 = p + 1, *q = i2, *v, *f;
      size_t n;
      while (*q && *q != '"') q += (*q == '\\' && q[1]) ? 2 : 1;
      if (prof != 1 || (size_t)(q - i2) != nChave || strncmp(i2, chave, nChave)) {
        p = *q ? q : q - 1;
        continue;
      }
      v = q + 1;
      while (*v == ' ' || *v == '\n' || *v == '\t' || *v == '\r') v++;
      if (*v != ':') { p = *q ? q : q - 1; continue; }
      for (v++; *v == ' ' || *v == '\n' || *v == '\t' || *v == '\r'; v++) ;
      if (*v == '{' || *v == '[') f = js_fim(v);
      else if (*v == '"') {
        const char *r = v + 1;
        while (*r && *r != '"') r += (*r == '\\' && r[1]) ? 2 : 1;
        f = *r ? r + 1 : r;
      } else { const char *r = v;
        while (*r && *r != ',' && *r != '}' && *r != ']') r++; f = r; }
      n = (size_t)(f - v);
      if (n + 1 > tam) return 0;
      memcpy(dst, v, n);
      dst[n] = 0;
      return 1;
    }
    if (*p == '{' || *p == '[') prof++;
    else if (*p == '}' || *p == ']') { if (--prof <= 0) break; }
  }
  return 0;
}

// UM OBJETO DE LISTA DO TRAKT -> LstLista. Serve aos tres formatos porque os
// tres carregam o MESMO objeto; o que muda e o envelope em volta.
static int lerListaTrakt(const char *p, const char *f, LstLista *d) {
  memset(d, 0, sizeof *d);
  d->fonte = LST_TRAKT;
  d->curtidas = -1;
  d->itens = -1;
  js_texto_raiz_em(p, f, "name", d->titulo, sizeof d->titulo);
  if (!d->titulo[0]) return 0;
  js_texto_raiz_em(p, f, "description", d->descricao, sizeof d->descricao);
  // `ids` E `user` sao objetos irmaos e os dois tem "slug". O id numerico vive
  // em ids.trakt; o apelido do dono vive em user.username. Ler com js_num/
  // js_texto crus casaria o primeiro que aparecesse no texto, entao cada um e
  // lido DENTRO do seu objeto.
  { char ids[256] = "";
    if (brutoRaiz(p, f, "ids", ids, sizeof ids))
      d->traktId = (long)js_num(ids, ids + strlen(ids), "trakt", 0.0); }
  { char usr[400] = "";
    if (brutoRaiz(p, f, "user", usr, sizeof usr)) {
      const char *uf = usr + strlen(usr);
      if (!js_texto_raiz_em(usr, uf, "username", d->autor, sizeof d->autor))
        js_texto(usr, uf, "slug", d->autor, sizeof d->autor); } }
  d->itens    = (int)js_num(p, f, "item_count", -1.0);
  d->curtidas = (int)js_num(p, f, "likes", -1.0);
  if (d->curtidas < 0) d->curtidas = (int)js_num(p, f, "like_count", -1.0);
  // TIPO DE MIDIA: a lista do Trakt mistura filme e serie, e /lists/<id>/items
  // pede UM tipo por vez (e assim que descoberta.c ja monta a URL). O padrao e
  // filme e a tela oferece a troca — inventar "ambos" aqui seria prometer o que
  // o caminho de busca nao faz.
  snprintf(d->midia, sizeof d->midia, "MOVIE");
  return 1;
}

int lst_ler_trakt(const char *json, int envelope) {
  const char *p;
  int n = 0;
  if (!json) return 0;
  p = *json == '[' ? js_raiz_array(json) : NULL;
  for (; p && *p == '{' && n < LST_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    if (envelope) {
      char dentro[1600];
      if (!brutoRaiz(p, f, "list", dentro, sizeof dentro)) continue;
      if (lerListaTrakt(dentro, dentro + strlen(dentro), &tabela[n])) n++;
    } else if (lerListaTrakt(p, f, &tabela[n])) n++;
  }
  nTabela = n;
  return n;
}

// ITENS DO SIMKL. O envelope e {"movies":[{"movie":{...}}]} ou
// {"shows":[{"show":{...}}]}; o que interessa do item e o titulo, o ano e o
// ids.imdb — com o imdb, trakt_enfeitar_lote resolve arte e sinopse pelo
// Cinemeta sem credencial nenhuma, que e o mesmo caminho da fileira "Continuar
// assistindo".
int lst_ler_simkl_itens(const char *json, int serie, CatItem *dst, int max) {
  const char *vetor = serie ? "shows" : "movies";
  const char *interno = serie ? "show" : "movie";
  const char *p;
  int n = 0;
  if (!json || !dst) return 0;
  for (p = js_array(json, NULL, vetor); p && *p == '{' && n < max;
       p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    char obj[900] = "";
    const char *oi = p, *of = f;
    CatItem *d = &dst[n];
    char imdb[32] = "";
    if (brutoRaiz(p, f, interno, obj, sizeof obj)) { oi = obj; of = obj + strlen(obj); }
    memset(d, 0, sizeof *d);
    js_texto_raiz_em(oi, of, "title", d->titulo, sizeof d->titulo);
    { char ids[300] = "";
      if (brutoRaiz(oi, of, "ids", ids, sizeof ids))
        js_texto(ids, ids + strlen(ids), "imdb", imdb, sizeof imdb); }
    if (!d->titulo[0] || !imdb[0]) continue;
    snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
    snprintf(d->tipo, sizeof d->tipo, "%s", serie ? "series" : "movie");
    { int ano = (int)js_num(oi, of, "year", 0.0);
      if (ano > 1800) snprintf(d->meta, sizeof d->meta, "%d", ano); }
    n++;
  }
  return n;
}

// ------------------------------------------------------------ fontes locais

// NUVIO: as pastas de colecao da conta. Zero pedidos — ja estao em memoria
// (sync_pull_collections -> col_definir_json). Uma pasta por lista, com o GRUPO
// como autor: e assim que a pessoa reconhece de onde ela veio na conta.
static int montarNuvio(void) {
  int i, n = 0;
  for (i = 0; i < col_n() && n < LST_MAX; i++) {
    const ColFolder *f = col_folder(i);
    LstLista *d;
    if (!f || !f->title[0] || !f->nSources) continue;
    // A pasta que ESTA TELA criou para a Home nao volta como "lista do Nuvio":
    // ela e uma lista do Trakt fixada aqui, e apareceria duas vezes.
    if (!strcmp(f->group, LST_GRUPO_HOME)) continue;
    d = &tabela[n];
    memset(d, 0, sizeof *d);
    d->fonte = LST_NUVIO;
    d->itens = -1;
    d->curtidas = -1;
    snprintf(d->titulo, sizeof d->titulo, "%s", f->title);
    snprintf(d->autor, sizeof d->autor, "%s", f->group);
    snprintf(d->colId, sizeof d->colId, "%s", f->id);
    n++;
  }
  nTabela = n;
  if (!n) snprintf(aviso, sizeof aviso,
                   "Nenhuma coleção na sua conta Nuvio ainda");
  return n;
}

// SIMKL: os cinco estados de acompanhamento. Nao ha pedido para LISTAR — os
// estados sao fixos na API; so o que a pessoa abrir e baixado.
static const struct { const char *estado; const char *rotulo; } SMK[] = {
  { "watching",    "Assistindo" },
  { "plantowatch", "Quero assistir" },
  { "completed",   "Concluídos" },
  { "hold",        "Em pausa" },
  { "dropped",     "Abandonados" },
};
static int montarSimkl(void) {
  int i, n = 0;
  if (!simklauth_token()[0]) {
    nTabela = 0;
    snprintf(aviso, sizeof aviso,
             "Conecte sua conta do Simkl em Ajustes para ver estas listas");
    return 0;
  }
  for (i = 0; i < (int)(sizeof SMK / sizeof SMK[0]); i++) {
    LstLista *d = &tabela[n++];
    memset(d, 0, sizeof *d);
    d->fonte = LST_SIMKL;
    d->itens = -1;
    d->curtidas = -1;
    snprintf(d->titulo, sizeof d->titulo, "%s", SMK[i].rotulo);
    snprintf(d->estado, sizeof d->estado, "%s", SMK[i].estado);
    snprintf(d->midia, sizeof d->midia, "MOVIE");
  }
  nTabela = n;
  return n;
}

static int montarFixadas(void) {
  int i, n = 0;
  lerFixas();
  for (i = 0; i < nFixas && n < LST_MAX; i++) {
    LstLista *d = &tabela[n++];
    memset(d, 0, sizeof *d);
    d->fonte = fixas[i].fonte;
    d->itens = -1;
    d->curtidas = -1;
    snprintf(d->titulo, sizeof d->titulo, "%s", fixas[i].titulo);
    snprintf(d->autor, sizeof d->autor, "%s", fixas[i].autor);
    snprintf(d->midia, sizeof d->midia, "%s", fixas[i].midia);
    if (fixas[i].fonte == LST_TRAKT)      d->traktId = atol(fixas[i].id);
    else if (fixas[i].fonte == LST_NUVIO) snprintf(d->colId, sizeof d->colId, "%s", fixas[i].id);
    else                                  snprintf(d->estado, sizeof d->estado, "%s", fixas[i].id);
  }
  nTabela = n;
  if (!n) snprintf(aviso, sizeof aviso,
                   "Nada fixado ainda. Abra uma lista e escolha Fixar na Biblioteca.");
  return n;
}

// ------------------------------------------------------------ fio de rede

static void escapar(const char *s, char *dst, size_t n) {
  size_t k = 0;
  for (; *s && k + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') dst[k++] = (char)c;
    else { snprintf(dst + k, n - k, "%%%02X", c); k += 3; }
  }
  dst[k] = 0;
}

static void *fioListas(void *u) {
  char url[300], termo[300], *corpo = NULL;
  int qual;
  unsigned minha;
  (void)u;
  pthread_mutex_lock(&trava);
  qual = pedido; minha = geracao;
  escapar(termoBusca, termo, sizeof termo);
  pthread_mutex_unlock(&trava);

  if (qual == PED_TRAKT_MINHAS) {
    const char *cab[4];
    char aut[200], chave[160];
    if (trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave))
      corpo = rede_baixar_com("https://api.trakt.tv/users/me/lists", 15, cab);
  } else {
    // PUBLICAS: so o client id do APLICATIVO, sem token — o mesmo cabecalho
    // que descoberta.c usa em /lists/<id>/items e que o web usa em
    // buildTraktHeaders. Quem nao tem conta do Trakt ainda consegue procurar.
    const char *cli = nuvem_trakt_cliente();
    if (cli[0]) {
      char chave[200];
      const char *cab[] = { "trakt-api-version: 2", NULL, NULL };
      snprintf(chave, sizeof chave, "trakt-api-key: %s", cli);
      cab[1] = chave;
      if (termo[0])
        snprintf(url, sizeof url,
                 "https://api.trakt.tv/search/list?query=%s&limit=%d", termo, LST_MAX);
      else
        snprintf(url, sizeof url,
                 "https://api.trakt.tv/lists/trending?limit=%d", LST_MAX);
      corpo = rede_baixar_com(url, 15, cab);
    }
  }

  pthread_mutex_lock(&trava);
  if (minha != geracao) { pthread_mutex_unlock(&trava); free(corpo); return NULL; }
  nTabela = 0;
  if (corpo) lst_ler_trakt(corpo, qual != PED_TRAKT_MINHAS);
  if (!nTabela) {
    if (qual == PED_TRAKT_MINHAS)
      snprintf(aviso, sizeof aviso, trakt_ativo()
               ? "Você ainda não criou listas no Trakt"
               : "Conecte sua conta do Trakt em Ajustes para ver suas listas");
    else if (!nuvem_trakt_cliente()[0])
      snprintf(aviso, sizeof aviso, "Este pacote não tem a chave do Trakt");
    else
      snprintf(aviso, sizeof aviso, "Nenhuma lista pública para este termo");
  }
  fioVivo = 0;
  pthread_mutex_unlock(&trava);
  free(corpo);
  printf("[listas] %s: %d\n",
         qual == PED_TRAKT_MINHAS ? "minhas" : "publicas", nTabela);
  fflush(stdout);
  return NULL;
}

static void disparar(int qual) {
  pthread_t t;
  pthread_mutex_lock(&trava);
  // UM FIO PENDENTE NAO E MOTIVO PARA RECUSAR. `geracao` ja invalida o
  // resultado dele na volta: recusar aqui deixaria a tela presa no resultado
  // anterior se a pessoa trocasse de aba durante um pedido lento.
  pedido = qual;
  geracao++;
  nTabela = 0;
  aviso[0] = 0;
  fioVivo = 1;
  if (pthread_create(&t, NULL, fioListas, NULL) != 0) fioVivo = 0;
  else pthread_detach(t);
  pthread_mutex_unlock(&trava);
}

void lst_pedir(int fonte) {
  pthread_mutex_lock(&trava);
  aviso[0] = 0; nTabela = 0; geracao++;
  pthread_mutex_unlock(&trava);
  if (fonte == LST_NUVIO)      { montarNuvio(); return; }
  if (fonte == LST_SIMKL)      { montarSimkl(); return; }
  disparar(PED_TRAKT_MINHAS);
}

void lst_buscar(const char *termo) {
  pthread_mutex_lock(&trava);
  snprintf(termoBusca, sizeof termoBusca, "%s", termo ? termo : "");
  pthread_mutex_unlock(&trava);
  disparar(PED_TRAKT_BUSCA);
}

void lst_pedir_fixadas(void) {
  pthread_mutex_lock(&trava);
  aviso[0] = 0; geracao++;
  pthread_mutex_unlock(&trava);
  montarFixadas();
}

int lst_carregando(void) {
  int v;
  pthread_mutex_lock(&trava); v = fioVivo; pthread_mutex_unlock(&trava);
  return v;
}
int lst_n(void) {
  int v;
  pthread_mutex_lock(&trava); v = nTabela; pthread_mutex_unlock(&trava);
  return v;
}
// COPIA, e nao ponteiro para a tabela: o fio de rede troca `tabela` inteira sob
// trava, e um ponteiro cru viveria ate o proximo lote chegar no meio de um
// quadro. O anel de quatro existe porque `lst_lista(0)` e `lst_lista(1)` na
// MESMA expressao sao normais em quem desenha e comparam duas listas — com um
// slot so, os dois ponteiros apontariam para a segunda. Nao e teoria: foi o que
// o teste apanhou. O valor vale ate a QUINTA chamada seguinte.
const LstLista *lst_lista(int i) {
  static LstLista anel[4];
  static int vez;
  const LstLista *r = NULL;
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nTabela) {
    anel[vez] = tabela[i];
    r = &anel[vez];
    vez = (vez + 1) & 3;
  }
  pthread_mutex_unlock(&trava);
  return r;
}
const char *lst_aviso(void) {
  static char copia[160];
  pthread_mutex_lock(&trava);
  snprintf(copia, sizeof copia, "%s", aviso);
  pthread_mutex_unlock(&trava);
  return copia;
}

// ------------------------------------------------------------ itens da lista

static pthread_mutex_t itTrava = PTHREAD_MUTEX_INITIALIZER;
static CatItem  itens[LST_ITENS_MAX];
static int      nItens, itFioVivo, itSimkl;
static unsigned itGeracao;
static char     itEstado[16], itMidia[8];

static void *fioSimkl(void *u) {
  char url[500], cid[200], nome[120], aut[400];
  const char *cab[3];
  char *corpo;
  CatItem lote[LST_ITENS_MAX];
  int n = 0, serie;
  unsigned minha;
  char estado[16], midia[8];
  (void)u;
  pthread_mutex_lock(&itTrava);
  minha = itGeracao;
  snprintf(estado, sizeof estado, "%s", itEstado);
  snprintf(midia, sizeof midia, "%s", itMidia);
  pthread_mutex_unlock(&itTrava);
  serie = !strcasecmp(midia, "TV");

  nuvem_url_escapar(nuvem_simkl_cliente(), cid, sizeof cid);
  nuvem_url_escapar(nuvem_simkl_app()[0] ? nuvem_simkl_app() : "nuvio", nome, sizeof nome);
  snprintf(url, sizeof url,
           "https://api.simkl.com/sync/all-items/%s/%s"
           "?client_id=%s&app-name=%s&app-version=1.0.1&extended=full",
           serie ? "shows" : "movies", estado, cid, nome);
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", simklauth_token());
  cab[0] = aut; cab[1] = "Accept: application/json"; cab[2] = NULL;
  corpo = rede_baixar_com(url, 20, cab);
  if (corpo) {
    n = lst_ler_simkl_itens(corpo, serie, lote, LST_ITENS_MAX);
    free(corpo);
  }
  // Arte pelo metahub (e Cinemeta so para texto), em lote e por imdb — o
  // mesmo caminho do "Continuar assistindo", sem credencial do Trakt.
  if (n) n = trakt_enfeitar_lote(lote, n);
  pthread_mutex_lock(&itTrava);
  if (minha == itGeracao) {
    memcpy(itens, lote, sizeof(CatItem) * (size_t)n);
    nItens = n;
  }
  itFioVivo = 0;
  pthread_mutex_unlock(&itTrava);
  printf("[listas] simkl %s/%s: %d\n", serie ? "shows" : "movies", estado, n);
  fflush(stdout);
  return NULL;
}

void lst_abrir(const LstLista *l, const char *midia) {
  ColSource s;
  if (!l) return;
  pthread_mutex_lock(&itTrava);
  nItens = 0; itGeracao++;
  itSimkl = (l->fonte == LST_SIMKL);
  snprintf(itEstado, sizeof itEstado, "%s", l->estado);
  snprintf(itMidia, sizeof itMidia, "%s", midia && midia[0] ? midia : l->midia);
  pthread_mutex_unlock(&itTrava);

  if (l->fonte == LST_SIMKL) {
    pthread_t t;
    pthread_mutex_lock(&itTrava);
    if (itFioVivo) { pthread_mutex_unlock(&itTrava); return; }
    itFioVivo = 1;
    if (pthread_create(&t, NULL, fioSimkl, NULL) != 0) itFioVivo = 0;
    else pthread_detach(t);
    pthread_mutex_unlock(&itTrava);
    return;
  }

  memset(&s, 0, sizeof s);
  if (l->fonte == LST_TRAKT) {
    snprintf(s.prov, sizeof s.prov, "trakt");
    s.traktLista = l->traktId;
    snprintf(s.title, sizeof s.title, "%s", l->titulo);
    snprintf(s.midia, sizeof s.midia, "%s", midia && midia[0] ? midia : l->midia);
    snprintf(s.ordenar, sizeof s.ordenar, "rank");
    snprintf(s.ordem, sizeof s.ordem, "asc");
    desc_vertudo_fonte(&s);
    return;
  }
  // NUVIO: a pasta ja tem as fontes dela. A primeira e a que a tela de colecao
  // (vertudo.c) tambem abriria — mesma regra, mesmo resultado.
  { int i;
    for (i = 0; i < col_n(); i++) {
      const ColFolder *f = col_folder(i);
      if (!f || strcmp(f->id, l->colId) || !f->nSources) continue;
      if (f->sources[0].prov[0]) desc_vertudo_fonte(&f->sources[0]);
      else desc_vertudo_filtro(f->sources[0].base, f->sources[0].type,
                               f->sources[0].catId, f->sources[0].genre);
      return;
    } }
}

int lst_itens_n(void) {
  int v;
  pthread_mutex_lock(&itTrava);
  v = itSimkl ? nItens : -1;
  pthread_mutex_unlock(&itTrava);
  return v >= 0 ? v : desc_vertudo_n();
}

int lst_item(int i, CatItem *dst) {
  int simkl, ok = 0;
  pthread_mutex_lock(&itTrava);
  simkl = itSimkl;
  if (simkl && dst && i >= 0 && i < nItens) { *dst = itens[i]; ok = 1; }
  pthread_mutex_unlock(&itTrava);
  if (simkl) return ok;
  return desc_vertudo_item(i, dst);
}

int lst_itens_carregando(void) {
  int v;
  pthread_mutex_lock(&itTrava);
  v = itSimkl ? itFioVivo : -1;
  pthread_mutex_unlock(&itTrava);
  return v >= 0 ? v : desc_vertudo_carregando();
}

void lst_itens_mais(void) {
  int simkl;
  pthread_mutex_lock(&itTrava);
  simkl = itSimkl;
  pthread_mutex_unlock(&itTrava);
  if (!simkl) desc_vertudo_mais();
}
