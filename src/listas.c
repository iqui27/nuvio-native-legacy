// Ver listas.h para o que cada fonte entrega de verdade. Aqui fica o COMO.
#include "listas.h"
#include "musica.h"
#include "trakt.h"
#include "simklauth.h"
#include "nuvem.h"
#include "descoberta.h"
#include "colecoes.h"
#include "addons.h"
#include "fileiras.h"
#include "dados.h"
#include "perfis.h"
#include "recomenda.h"
#include "rede.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <time.h>

#ifndef LST_SO_ITENS
// A TABELA E COMPARTILHADA COM UM FIO DE REDE, entao tudo que a tela le passa
// por esta trava. E a mesma disciplina de vtTrava em descoberta.c: o fio
// escreve o lote inteiro de uma vez e a tela nunca ve meia lista.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

// UMA TABELA POR ORIGEM (#202): a resposta do Trakt nao apaga o Simkl, e uma
// busca publica nao apaga "Minhas listas". A aba escolhida decide qual `tab`
// a tela le; as outras ficam prontas para a volta.
enum { LST_TAB_TRAKT = 0, LST_TAB_SIMKL, LST_TAB_NUVIO, LST_TAB_FIXADAS,
       LST_TAB_PUBLICAS, LST_TAB_N };
typedef struct {
  LstLista v[LST_MAX];
  int n;
  char aviso[160];
  int fioVivo;
  unsigned geracao;
} LstFonteTab;
static LstFonteTab tabs[LST_TAB_N];
static int fonteAtual = LST_TAB_TRAKT;

// O QUE ESTE FIO FOI PEDIR. Copiado sob trava antes de sair, para o fio nunca
// ler um pedido que a pessoa ja trocou.
enum { PED_NADA = 0, PED_TRAKT_MINHAS, PED_TRAKT_BUSCA, PED_SIMKL_ITENS };
static char termoBusca[96];

// --------------------------------------------------------------- fixadas

typedef struct {
  int  fonte;
  char id[96];       // traktId em texto, colId, ou o estado do Simkl
  char midia[8];
  int  naHome;
  char titulo[128];
  char autor[64];
  // A LISTA E PROPRIA? O GET de itens so leva o Bearer quando traktPropria
  // (descoberta.c), e a reconstrucao de uma fixada nao tem o objeto original:
  // o que decidiu no momento de fixar tem de sobreviver no arquivo — sem isto,
  // a lista privada do dono fixada e reaberta vinha 403. As linhas ANTIGAS nao
  // tem esta coluna: na leitura, o autor (username) e comparado ao slug da
  // conta Trakt ligada — no Trakt o username E o slug, e e assim que o pin
  // antigo migra; a proxima gravacao ja escreve a coluna.
  int  propria;
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
      "# fonte\\tid\\tmidia\\tnaHome\\ttitulo\\tautor\\tpropria\n");
  for (i = 0; i < nFixas && k + 320 < sizeof txt; i++)
    k += (size_t)snprintf(txt + k, sizeof txt - k, "%s\t%s\t%s\t%d\t%s\t%s\t%d\n",
                          nomeFonte(fixas[i].fonte), fixas[i].id,
                          fixas[i].midia[0] ? fixas[i].midia : "-",
                          fixas[i].naHome, fixas[i].titulo, fixas[i].autor,
                          fixas[i].propria);
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
    char *campo[7]; int c = 0; char *s;
    fim = strchr(linha, '\n');
    if (fim) *fim++ = 0; else fim = linha + strlen(linha);
    if (linha[0] == '#' || !linha[0]) continue;
    campo[c++] = linha;
    for (s = linha; *s && c < 7; s++) if (*s == '\t') { *s = 0; campo[c++] = s + 1; }
    if (c < 5 || nFixas >= LST_FIX_MAX) continue;
    { LstFixa *f = &fixas[nFixas];
      memset(f, 0, sizeof *f);
      f->fonte = fonteDeNome(campo[0]);
      snprintf(f->id, sizeof f->id, "%s", campo[1]);
      if (strcmp(campo[2], "-")) snprintf(f->midia, sizeof f->midia, "%s", campo[2]);
      f->naHome = atoi(campo[3]) ? 1 : 0;
      snprintf(f->titulo, sizeof f->titulo, "%s", campo[4]);
      if (c > 5) snprintf(f->autor, sizeof f->autor, "%s", campo[5]);
      if (c > 6) f->propria = atoi(campo[6]) ? 1 : 0;
      else {
        // LINHA ANTIGA, sem a coluna propria: migra comparando o dono gravado
        // ao slug da conta Trakt ligada (no Trakt, username e slug sao o mesmo).
        const char *eu = recomenda_identidade_trakt();
        f->propria = eu[0] && !strcmp(f->autor, eu);
      }
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
    f->propria = l->fonte == LST_TRAKT && l->propria;
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
  // static: eram LST_FIX_MAX pastas de 52 KB NA PILHA (1,2 MB). Com as fontes
  // fora da pasta (#255) cada uma tem ~4 KB, e col_extra_definir copia a fonte.
  static ColFolder pastas[LST_FIX_MAX];
  static ColSource fontes[LST_FIX_MAX];
  int n = 0, i;
  for (i = 0; i < nFixas && n < LST_FIX_MAX; i++) {
    ColFolder *v;
    if (!fixas[i].naHome || fixas[i].fonte != LST_TRAKT) continue;
    v = &pastas[n];
    memset(v, 0, sizeof *v);
    memset(&fontes[n], 0, sizeof fontes[n]);
    v->sources = &fontes[n];
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
  if (perfilLido != perfis_ativo()) lst_previa_limpar();
  fixasLidas = 0;
  lerFixas();
  aplicarHome();
}

void lst_esquecer(void) {
  pthread_mutex_lock(&trava);
  memset(tabs, 0, sizeof tabs);
  fonteAtual = LST_TAB_TRAKT;
  for (int i = 0; i < LST_TAB_N; i++) tabs[i].geracao++;
  pthread_mutex_unlock(&trava);
  nFixas = 0; fixasLidas = 0; perfilLido = -1;
  lst_previa_limpar();
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
  d->comentarios = -1;
  js_texto_raiz_em(p, f, "name", d->titulo, sizeof d->titulo);
  if (!d->titulo[0]) return 0;
  js_texto_raiz_em(p, f, "description", d->descricao, sizeof d->descricao);
  // `ids` E `user` sao objetos irmaos e os dois tem "slug". O id numerico vive
  // em ids.trakt; o apelido do dono vive em user.username. Ler com js_num/
  // js_texto crus casaria o primeiro que aparecesse no texto, entao cada um e
  // lido DENTRO do seu objeto.
  { char ids[256] = "";
    if (brutoRaiz(p, f, "ids", ids, sizeof ids)) {
      d->traktId = (long)js_num(ids, ids + strlen(ids), "trakt", 0.0);
      js_texto(ids, ids + strlen(ids), "slug", d->slug, sizeof d->slug); } }
  { char usr[700] = "";
    if (brutoRaiz(p, f, "user", usr, sizeof usr)) {
      const char *uf = usr + strlen(usr);
      char uids[200] = "";
      if (!js_texto_raiz_em(usr, uf, "username", d->autor, sizeof d->autor))
        js_texto(usr, uf, "slug", d->autor, sizeof d->autor);
      js_texto_raiz_em(usr, uf, "name", d->autorNome, sizeof d->autorNome);
      if (brutoRaiz(usr, uf, "ids", uids, sizeof uids))
        js_texto(uids, uids + strlen(uids), "slug", d->autorSlug, sizeof d->autorSlug); } }
  js_texto_raiz_em(p, f, "created_at", d->criada, sizeof d->criada);
  js_texto_raiz_em(p, f, "updated_at", d->atualizada, sizeof d->atualizada);
  { char pv[16] = "";
    d->privada = js_texto_raiz_em(p, f, "privacy", pv, sizeof pv) && strcmp(pv, "public") ? 1 : 0; }
  d->comentarios = (int)js_num(p, f, "comment_count", -1.0);
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

static int lerVetor(const char *json, int envelope, LstLista *dst, int max) {
  const char *p;
  int n = 0;
  if (!json) return 0;
  p = *json == '[' ? js_raiz_array(json) : NULL;
  for (; p && *p == '{' && n < max; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    if (envelope) {
      char dentro[1600];
      if (!brutoRaiz(p, f, "list", dentro, sizeof dentro)) continue;
      if (lerListaTrakt(dentro, dentro + strlen(dentro), &dst[n])) n++;
    } else if (lerListaTrakt(p, f, &dst[n])) { dst[n].propria = 1; n++; }
  }
  return n;
}

int lst_ler_trakt(const char *json, int envelope) {
  LstFonteTab *tab = &tabs[fonteAtual];
  tab->n = lerVetor(json, envelope, tab->v, LST_MAX);
  return tab->n;
}

// --- AS LISTAS DO DONO, FORA DA TABELA DA TELA --------------------------------
// "Adicionar a uma lista" (menu do titulo) precisa das listas proprias do Trakt
// sem pisar na tabela que a Biblioteca esta mostrando (uma busca publica, por
// exemplo). Tabela propria, um pedido por abertura do menu.
static LstLista minhas[LST_MAX];
static int      nMinhas, fioMinhas;

static void *fioMinhasListas(void *u) {
  const char *cab[4];
  char aut[200], chave[160];
  char *corpo = NULL;
  static LstLista tmp[LST_MAX];
  int n = 0;
  (void)u;
  if (trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave))
    corpo = rede_baixar_com("https://api.trakt.tv/users/me/lists", 15, cab);
  pthread_mutex_lock(&trava);
  n = lerVetor(corpo, 0, tmp, LST_MAX);
  memcpy(minhas, tmp, sizeof(LstLista) * (size_t)n);
  nMinhas = n;
  fioMinhas = 0;
  pthread_mutex_unlock(&trava);
  free(corpo);
  return NULL;
}

void lst_minhas_pedir(void) {
  pthread_t t;
  pthread_mutex_lock(&trava);
  if (fioMinhas) { pthread_mutex_unlock(&trava); return; }
  fioMinhas = 1;
  if (pthread_create(&t, NULL, fioMinhasListas, NULL) != 0) fioMinhas = 0;
  else pthread_detach(t);
  pthread_mutex_unlock(&trava);
}
int lst_minhas_carregando(void) {
  int v;
  pthread_mutex_lock(&trava); v = fioMinhas; pthread_mutex_unlock(&trava);
  return v;
}
int lst_minhas_n(void) {
  int v;
  pthread_mutex_lock(&trava); v = nMinhas; pthread_mutex_unlock(&trava);
  return v;
}
int lst_minha(int i, LstLista *dst) {
  int ok = 0;
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nMinhas && dst) { *dst = minhas[i]; ok = 1; }
  pthread_mutex_unlock(&trava);
  return ok;
}
void lst_descricao_atualizar(long id, const char *descricao) {
  if (id <= 0 || !descricao) return;
  pthread_mutex_lock(&trava);
  for (int p = 0; p < LST_TAB_N; p++)
    for (int i = 0; i < tabs[p].n; i++)
      if (tabs[p].v[i].fonte == LST_TRAKT && tabs[p].v[i].traktId == id)
        snprintf(tabs[p].v[i].descricao, sizeof tabs[p].v[i].descricao, "%s", descricao);
  for (int i = 0; i < nMinhas; i++)
    if (minhas[i].traktId == id)
      snprintf(minhas[i].descricao, sizeof minhas[i].descricao, "%s", descricao);
  pthread_mutex_unlock(&trava);
}

void lst_minhas_teste(const char *json) {
  pthread_mutex_lock(&trava);
  nMinhas = lerVetor(json, 0, minhas, LST_MAX);
  pthread_mutex_unlock(&trava);
}

// ITENS DO SIMKL. O envelope e {"movies":[{"movie":{...}}]} ou
// {"shows":[{"show":{...}}]}; o que interessa do item e o titulo, o ano e o
// ids.imdb — com o imdb, trakt_enfeitar_lote resolve arte e sinopse pelo
// Cinemeta sem credencial nenhuma, que e o mesmo caminho da fileira "Continuar
// assistindo".
int lst_ler_simkl_itens(const char *json, int serie, CatItem *dst, int max) {
  // Anime is a separate Simkl endpoint; anime_type selects Movies or TV.
  static const char *vetores[] = { "movies", "shows", "anime" };
  static const char *chaves[]  = { "movie", "show", "anime" };
  const char *p;
  int n = 0, k;
  if (!json || !dst) return 0;
  for (k = 0; k < 3 && n < max; k++)
  for (p = js_array(json, NULL, vetores[k]); p && *p == '{' && n < max;
       p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    if (k != 2 && k != (serie ? 1 : 0)) continue;
    char obj[900] = "";
    const char *oi = p, *of = f;
    CatItem *d = &dst[n];
    char imdb[32] = "";
    if (brutoRaiz(p, f, chaves[k], obj, sizeof obj) ||
        (k == 2 && brutoRaiz(p, f, "show", obj, sizeof obj))) {
      oi = obj; of = obj + strlen(obj);
    }
    if (k == 2) {
      char animeTipo[24] = "";
      js_texto_raiz_em(oi, of, "anime_type", animeTipo, sizeof animeTipo);
      if ((!strcmp(animeTipo, "movie")) == !!serie) continue;
    }
    memset(d, 0, sizeof *d);
    js_texto_raiz_em(oi, of, "title", d->titulo, sizeof d->titulo);
    { char ids[300] = "";
      if (brutoRaiz(oi, of, "ids", ids, sizeof ids)) {
        js_texto(ids, ids + strlen(ids), "imdb", imdb, sizeof imdb);
        d->tmdb = (long)js_num(ids, NULL, "tmdb", 0);
        if (!imdb[0] && d->tmdb > 0)
          snprintf(imdb, sizeof imdb, "tmdb:%ld", d->tmdb);
        if (!imdb[0]) {
          long id = (long)js_num(ids, NULL, "simkl", 0);
          if (id > 0) snprintf(imdb, sizeof imdb, "simkl:%ld", id);
        }
      } }
    if (!d->titulo[0]) continue;
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
  LstFonteTab *tab = &tabs[LST_TAB_NUVIO];
  int i, n = 0;
  for (i = 0; i < col_n() && n < LST_MAX; i++) {
    const ColFolder *f = col_folder(i);
    LstLista *d;
    if (!f || !f->title[0] || !f->nSources) continue;
    // A pasta que ESTA TELA criou para a Home nao volta como "lista do Nuvio":
    // ela e uma lista do Trakt fixada aqui, e apareceria duas vezes.
    if (!strcmp(f->group, LST_GRUPO_HOME)) continue;
    d = &tab->v[n];
    memset(d, 0, sizeof *d);
    d->fonte = LST_NUVIO;
    d->itens = -1;
    d->curtidas = -1;
    snprintf(d->titulo, sizeof d->titulo, "%s", f->title);
    snprintf(d->autor, sizeof d->autor, "%s", f->group);
    snprintf(d->colId, sizeof d->colId, "%s", f->id);
    n++;
  }
  if (n < LST_MAX) {
    LstLista *d = &tab->v[n++];
    memset(d, 0, sizeof *d);
    d->fonte = LST_NUVIO;
    snprintf(d->titulo, sizeof d->titulo, "Músicas");
    snprintf(d->colId, sizeof d->colId, "musicas");
    d->curtidas = -1;
    MusSalva saved[MUS_GUARDADAS_MAX];
    d->itens = mus_listar(saved, MUS_GUARDADAS_MAX);
  }
  tab->n = n;
  tab->aviso[0] = 0;
  if (!n) snprintf(tab->aviso, sizeof tab->aviso,
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
  LstFonteTab *tab = &tabs[LST_TAB_SIMKL];
  int i, n = 0;
  if (!simklauth_token()[0]) {
    tab->n = 0;
    snprintf(tab->aviso, sizeof tab->aviso,
             "Conecte sua conta do Simkl em Ajustes para ver estas listas");
    return 0;
  }
  tab->aviso[0] = 0;
  for (i = 0; i < (int)(sizeof SMK / sizeof SMK[0]); i++) {
    LstLista *d = &tab->v[n++];
    memset(d, 0, sizeof *d);
    d->fonte = LST_SIMKL;
    d->itens = -1;
    d->curtidas = -1;
    snprintf(d->titulo, sizeof d->titulo, "%s", SMK[i].rotulo);
    snprintf(d->estado, sizeof d->estado, "%s", SMK[i].estado);
    snprintf(d->midia, sizeof d->midia, "MOVIE");
  }
  tab->n = n;
  return n;
}

static int montarFixadas(void) {
  LstFonteTab *tab = &tabs[LST_TAB_FIXADAS];
  int i, n = 0;
  lerFixas();
  for (i = 0; i < nFixas && n < LST_MAX; i++) {
    LstLista *d = &tab->v[n++];
    memset(d, 0, sizeof *d);
    d->fonte = fixas[i].fonte;
    d->itens = -1;
    d->curtidas = -1;
    snprintf(d->titulo, sizeof d->titulo, "%s", fixas[i].titulo);
    snprintf(d->autor, sizeof d->autor, "%s", fixas[i].autor);
    snprintf(d->midia, sizeof d->midia, "%s", fixas[i].midia);
    if (fixas[i].fonte == LST_TRAKT) {
      d->traktId = atol(fixas[i].id);
      d->propria = fixas[i].propria;
    }
    else if (fixas[i].fonte == LST_NUVIO) snprintf(d->colId, sizeof d->colId, "%s", fixas[i].id);
    else                                  snprintf(d->estado, sizeof d->estado, "%s", fixas[i].id);
  }
  tab->n = n;
  tab->aviso[0] = 0;
  if (!n) snprintf(tab->aviso, sizeof tab->aviso,
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

typedef struct {
  unsigned geracao;
  uint64_t sessao;
  int qual, tab;
  char termo[96], aut[200], chave[200];
} ListaTarefa;

static void *fioListas(void *u) {
  ListaTarefa *t = u;
  LstFonteTab *tab = &tabs[t->tab];
  char url[300], termo[300], *corpo = NULL;
  int qual = t->qual, st = 0, tentativa;
  escapar(t->termo, termo, sizeof termo);
  if (perfis_sessao_atual(t->sessao)) {
    const char *cab[4];
    for (tentativa = 0; tentativa < 2; tentativa++) {
      int ra = 0;
      if (qual == PED_TRAKT_MINHAS && t->aut[0]) {
        cab[0] = t->aut; cab[1] = "trakt-api-version: 2"; cab[2] = t->chave; cab[3] = NULL;
        corpo = rede_baixar_st_retry("https://api.trakt.tv/users/me/lists", 15, cab, &st, &ra);
      } else if (qual != PED_TRAKT_MINHAS && t->chave[0]) {
        cab[0] = "trakt-api-version: 2"; cab[1] = t->chave; cab[2] = NULL;
        if (termo[0])
          snprintf(url, sizeof url, "https://api.trakt.tv/search/list?query=%s&limit=%d", termo, LST_MAX);
        else snprintf(url, sizeof url, "https://api.trakt.tv/lists/trending?limit=%d", LST_MAX);
        corpo = rede_baixar_st_retry(url, 15, cab, &st, &ra);
      }
      if (st != 429 || tentativa) break;
      free(corpo); corpo = NULL;
      { struct timespec z;
        if (ra <= 0) ra = 5;
        if (ra > 60) ra = 60;
        printf("[listas] trakt pediu pausa: %d s\n", ra);
        fflush(stdout);
        z.tv_sec = ra; z.tv_nsec = 0;
        nanosleep(&z, NULL);
      }
    }
  }
  // So 2xx vale: um corpo de erro (403 com JSON) nao e "zero listas".
  if (corpo && (st < 200 || st > 299)) { free(corpo); corpo = NULL; }
  // Publicacao e troca de sessao se excluem; a rede nunca segura essa trava.
  if (!perfis_sessao_travar(t->sessao)) {
    pthread_mutex_lock(&trava);
    if (t->geracao == tab->geracao && tab->fioVivo > 0) tab->fioVivo--;
    pthread_mutex_unlock(&trava);
  } else {
    pthread_mutex_lock(&trava);
    if (t->geracao == tab->geracao) {
      // Sem resposta util (rede, ou nao-2xx descartado acima): a tabela que ja
      // estava na origem fica; so um corpo 2xx decide se ha listas ou aviso.
      if (corpo) {
        tab->n = lerVetor(corpo, qual != PED_TRAKT_MINHAS, tab->v, LST_MAX);
        if (!tab->n) {
          if (qual == PED_TRAKT_MINHAS)
            snprintf(tab->aviso, sizeof tab->aviso, "%s", t->aut[0]
              ? "Você ainda não criou listas no Trakt"
              : "Conecte sua conta do Trakt em Ajustes para ver suas listas");
          else snprintf(tab->aviso, sizeof tab->aviso, "%s", t->chave[0]
              ? "Nenhuma lista pública para este termo" : "Este pacote não tem a chave do Trakt");
        }
      }
      if (tab->fioVivo > 0) tab->fioVivo--;
    }
    pthread_mutex_unlock(&trava);
    perfis_sessao_destravar();
  }
  free(corpo); memset(t, 0, sizeof *t); free(t);
  return NULL;
}

// UMA fila para os pedidos do Trakt (#202): as abas de Listas guardam
// tabelas separadas, mas a rede delas anda junto, um pedido por vez. 429
// espera Retry-After dentro do proprio pedido; o resto pega a folga fixa do
// trabalhador em vez de uma rajada por troca de aba.
#define LST_FILA_MAX 12
static ListaTarefa *fila[LST_FILA_MAX];
static int nFila, trabVivo;
static void dormirListaMs(unsigned ms) {
  struct timespec z = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
  nanosleep(&z, NULL);
}
static void *trabalhadorListas(void *u) {
  (void)u;
  for (;;) {
    ListaTarefa *t;
    pthread_mutex_lock(&trava);
    if (!nFila) { trabVivo = 0; pthread_mutex_unlock(&trava); break; }
    t = fila[0];
    memmove(&fila[0], &fila[1], sizeof fila[0] * (size_t)(nFila - 1));
    nFila--;
    pthread_mutex_unlock(&trava);
    fioListas(t);
    dormirListaMs(150);
  }
  return NULL;
}
static void enfileirar(ListaTarefa *t) {
  pthread_t fio;
  if (nFila >= LST_FILA_MAX) {
    tabs[t->tab].fioVivo--;
    memset(t, 0, sizeof *t); free(t); return;
  }
  fila[nFila++] = t;
  if (trabVivo) return;
  trabVivo = 1;
  if (pthread_create(&fio, NULL, trabalhadorListas, NULL) != 0) trabVivo = 0;
  else pthread_detach(fio);
}

static void disparar(int qual) {
  ListaTarefa *t = calloc(1, sizeof *t);
  uint64_t sessao = perfis_sessao_geracao();
  if (!t || !perfis_sessao_travar(sessao)) { free(t); return; }
  t->qual = qual; t->sessao = sessao;
  t->tab = qual == PED_TRAKT_MINHAS ? LST_TAB_TRAKT : LST_TAB_PUBLICAS;
  // Capturar identidade e pedido ANTES de criar o fio. Um fio atrasado nao
  // pode ler as credenciais nem a geracao do pedido seguinte.
  if (qual == PED_TRAKT_MINHAS) {
    const char *cab[4];
    trakt_cabecalhos(cab, t->aut, sizeof t->aut, t->chave, sizeof t->chave);
  } else if (nuvem_trakt_cliente()[0]) {
    snprintf(t->chave, sizeof t->chave, "trakt-api-key: %s", nuvem_trakt_cliente());
  }
  pthread_mutex_lock(&trava);
  { LstFonteTab *tab = &tabs[t->tab];
    t->geracao = ++tab->geracao;
    snprintf(t->termo, sizeof t->termo, "%s", termoBusca);
    tab->aviso[0] = 0; tab->fioVivo++;
    enfileirar(t);
  }
  pthread_mutex_unlock(&trava);
  perfis_sessao_destravar();
}

void lst_pedir(int fonte) {
  int tab = fonte == LST_NUVIO ? LST_TAB_NUVIO
          : fonte == LST_SIMKL ? LST_TAB_SIMKL : LST_TAB_TRAKT;
  pthread_mutex_lock(&trava);
  fonteAtual = tab;
  pthread_mutex_unlock(&trava);
  if (tab == LST_TAB_NUVIO)      { montarNuvio(); return; }
  if (tab == LST_TAB_SIMKL)      { montarSimkl(); return; }
  pthread_mutex_lock(&trava);
  { int pronta = tabs[tab].n > 0;
    pthread_mutex_unlock(&trava);
    if (pronta) return;
  }
  disparar(PED_TRAKT_MINHAS);
}

void lst_buscar(const char *termo) {
  pthread_mutex_lock(&trava);
  fonteAtual = LST_TAB_PUBLICAS;
  snprintf(termoBusca, sizeof termoBusca, "%s", termo ? termo : "");
  pthread_mutex_unlock(&trava);
  disparar(PED_TRAKT_BUSCA);
}

void lst_pedir_fixadas(void) {
  pthread_mutex_lock(&trava);
  fonteAtual = LST_TAB_FIXADAS;
  pthread_mutex_unlock(&trava);
  montarFixadas();
}

int lst_carregando(void) {
  int v;
  pthread_mutex_lock(&trava); v = tabs[fonteAtual].fioVivo; pthread_mutex_unlock(&trava);
  return v;
}
int lst_n(void) {
  int v;
  pthread_mutex_lock(&trava); v = tabs[fonteAtual].n; pthread_mutex_unlock(&trava);
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
  if (i >= 0 && i < tabs[fonteAtual].n) {
    anel[vez] = tabs[fonteAtual].v[i];
    r = &anel[vez];
    vez = (vez + 1) & 3;
  }
  pthread_mutex_unlock(&trava);
  return r;
}
const char *lst_aviso(void) {
  static char copia[160];
  pthread_mutex_lock(&trava);
  snprintf(copia, sizeof copia, "%s", tabs[fonteAtual].aviso);
  pthread_mutex_unlock(&trava);
  return copia;
}

// Bounded to 24 opened lists (about 52 KB of URLs), cleared with the profile.
static struct { int fonte; long id; char chave[96]; LstPrevia arte; } previas[24];
static unsigned previaVez;
static const char *previaChave(const LstLista *l) {
  return l->fonte == LST_NUVIO ? l->colId : l->estado;
}
void lst_previa_limpar(void) { memset(previas, 0, sizeof previas); previaVez = 0; }
int lst_previa(const LstLista *l, LstPrevia *out) {
  if (!l || !out) return 0;
  for (int i = 0; i < 24; i++)
    if (previas[i].arte.n && previas[i].fonte == l->fonte && previas[i].id == l->traktId &&
        !strcmp(previas[i].chave, previaChave(l))) { *out = previas[i].arte; return 1; }
  memset(out, 0, sizeof *out); return 0;
}
void lst_previa_registrar(const LstLista *l, const CatItem *v, int n) {
  if (!l || !v || n <= 0) return;
  int slot = -1;
  for (int i = 0; i < 24; i++)
    if (previas[i].arte.n && previas[i].fonte == l->fonte && previas[i].id == l->traktId &&
        !strcmp(previas[i].chave, previaChave(l))) { slot = i; break; }
  if (slot < 0) slot = previaVez++ % 24;
  previas[slot].fonte = l->fonte; previas[slot].id = l->traktId;
  snprintf(previas[slot].chave, sizeof previas[slot].chave, "%s", previaChave(l));
  LstPrevia *p = &previas[slot].arte; p->n = 0;
  for (int i = 0; i < n && p->n < 4; i++) if (v[i].poster[0])
    snprintf(p->poster[p->n++], sizeof p->poster[0], "%s", v[i].poster);
}

#endif
// ------------------------------------------------------------ itens da lista

static pthread_mutex_t itTrava = PTHREAD_MUTEX_INITIALIZER;
static CatItem *itens;
static int      nItens, itFioVivo, itSimkl;
static unsigned itGeracao;
static int      itInjetado;   // so os testes (lst_teste_itens)
static DescItemLista itExtraTeste[LST_ITENS_MAX];   // so os testes (lst_teste_extras)
static char     itEstado[16], itMidia[8];
// Ordem do dono ainda nao gravada (Reordenar): itPerm[i] e o indice REAL do item
// mostrado na posicao i. itPermN 0 = a ordem que o servidor mandou.
static int      itPerm[LST_ITENS_MAX], itPermN;
static int mapear(int i) {
  int r = i;
  pthread_mutex_lock(&itTrava);
  if (itPermN && i >= 0 && i < itPermN) r = itPerm[i];
  pthread_mutex_unlock(&itTrava);
  return r;
}

#ifndef LST_SO_ITENS
typedef struct {
  unsigned geracao;
  uint64_t sessao;
  char estado[16], midia[8], cid[200], nome[120], token[400];
} SimklTarefa;

static char *fioSimklBaixar(const char *caminho, const SimklTarefa *t,
                            const char *const *cab) {
  char url[500];
  snprintf(url, sizeof url,
           "https://api.simkl.com%s?client_id=%s&app-name=%s"
           "&app-version=1.0.1&extended=full",
           caminho, t->cid, t->nome);
  return rede_baixar_com(url, 20, cab);
}

static void *fioSimkl(void *u) {
  SimklTarefa *t = u;
  char url[40], aut[430];
  const char *cab[3];
  CatItem *lote = NULL;
  int n = 0, serie = !strcasecmp(t->midia, "TV");
  snprintf(url, sizeof url, "/sync/all-items/%s/%s", serie ? "shows" : "movies",
           t->estado);
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", t->token);
  cab[0] = aut; cab[1] = "Accept: application/json"; cab[2] = NULL;
  // All-items returns a complete snapshot. Size storage from the response,
  // including anime's usual "show" envelope, rather than truncating at 120.
  for (int part = 0; part < 2; part++) {
    if (part) snprintf(url, sizeof url, "/sync/all-items/anime/%s", t->estado);
    char *corpo = perfis_sessao_atual(t->sessao) ? fioSimklBaixar(url, t, cab) : NULL;
    if (!corpo) { free(lote); lote = NULL; n = 0; break; }
    int count = 0;
    static const char *keys[] = {"movies", "shows", "anime"};
    for (int k = 0; k < 3; k++)
      for (const char *p = js_array(corpo, NULL, keys[k]); p && *p == '{'; p = js_prox(js_fim(p))) count++;
    CatItem *grown = count ? realloc(lote, sizeof *lote * (size_t)(n + count)) : lote;
    if (count && !grown) { free(corpo); free(lote); lote = NULL; n = 0; break; }
    lote = grown;
    if (count) n += lst_ler_simkl_itens(corpo, serie, lote + n, count);
    free(corpo);
  }
  // Enrichment may compact unsupported IDs. Enrich copies so metadata failure
  // cannot remove account entries or merge distinct Simkl titles by IMDb.
  CatItem *enriched = malloc(sizeof *enriched * 32);
  for (int i = 0; enriched && i < n && perfis_sessao_atual(t->sessao); i += 32) {
    int end = n - i < 32 ? n - i : 32, count = 0;
    for (int j = 0; j < end; j++)
      if (!strncmp(lote[i + j].imdb, "tt", 2)) enriched[count++] = lote[i + j];
    if (count) count = trakt_enfeitar_lote_sessao(enriched, count, t->sessao);
    for (int j = 0; j < end; j++)
      for (int k = 0; k < count; k++)
        if (!strcmp(lote[i + j].imdb, enriched[k].imdb)) { lote[i + j] = enriched[k]; break; }
  }
  free(enriched);
  if (perfis_sessao_travar(t->sessao)) {
    pthread_mutex_lock(&itTrava);
    if (t->geracao == itGeracao) {
      free(itens); itens = lote; lote = NULL;
      nItens = n; itFioVivo = 0;
    }
    pthread_mutex_unlock(&itTrava);
    perfis_sessao_destravar();
  }
  free(lote); memset(t, 0, sizeof *t); free(t);
  return NULL;
}
#endif

void lst_abrir(const LstLista *l, const char *midia) {
  ColSource s;
  if (!l || itInjetado) return;
  pthread_mutex_lock(&itTrava);
  nItens = 0; itGeracao++; itPermN = 0;
  itSimkl = (l->fonte == LST_SIMKL);
  snprintf(itEstado, sizeof itEstado, "%s", l->estado);
  snprintf(itMidia, sizeof itMidia, "%s", midia && midia[0] ? midia : l->midia);
  pthread_mutex_unlock(&itTrava);

  if (l->fonte == LST_NUVIO && !strcmp(l->colId, "musicas")) {
    MusSalva saved[MUS_GUARDADAS_MAX];
    int n = mus_listar(saved, MUS_GUARDADAS_MAX);
    CatItem *copy = n ? calloc((size_t)n, sizeof *copy) : NULL;
    if (n && !copy) return;
    for (int i = 0; i < n; i++) {
      snprintf(copy[i].imdb, sizeof copy[i].imdb, "%s", saved[i].imdb);
      snprintf(copy[i].titulo, sizeof copy[i].titulo, "%s", saved[i].titulo);
      snprintf(copy[i].tipo, sizeof copy[i].tipo, "%s", saved[i].temp ? "series" : "movie");
    }
    pthread_mutex_lock(&itTrava);
    free(itens); itens = copy; nItens = n; itSimkl = 1;
    pthread_mutex_unlock(&itTrava);
    return;
  }
  if (l->fonte == LST_SIMKL) {
#ifndef LST_SO_ITENS
    pthread_t fio;
    SimklTarefa *t = calloc(1, sizeof *t);
    uint64_t sessao = perfis_sessao_geracao();
    if (!t || !perfis_sessao_travar(sessao)) { free(t); return; }
    t->sessao = sessao;
    nuvem_url_escapar(nuvem_simkl_cliente(), t->cid, sizeof t->cid);
    nuvem_url_escapar(nuvem_simkl_app()[0] ? nuvem_simkl_app() : "nuvio",
                      t->nome, sizeof t->nome);
    snprintf(t->token, sizeof t->token, "%s", simklauth_token());
    pthread_mutex_lock(&itTrava);
    t->geracao = itGeracao;
    snprintf(t->estado, sizeof t->estado, "%s", itEstado);
    snprintf(t->midia, sizeof t->midia, "%s", itMidia);
    itFioVivo = 1;
    if (pthread_create(&fio, NULL, fioSimkl, t) != 0) {
      itFioVivo = 0; memset(t, 0, sizeof *t); free(t);
    } else pthread_detach(fio);
    pthread_mutex_unlock(&itTrava);
    perfis_sessao_destravar();
#endif
    return;
  }

  memset(&s, 0, sizeof s);
  if (l->fonte == LST_TRAKT) {
    snprintf(s.prov, sizeof s.prov, "trakt");
    s.traktLista = l->traktId;
    s.traktPropria = l->propria;
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
      // Base vazia = nao coube no atalho da fonte (colecoes.c): a inteira esta
      // na tabela de addons.
      else desc_vertudo_filtro(f->sources[0].base[0] ? f->sources[0].base
                               : addons_base_por_id(f->sources[0].addonId), f->sources[0].type,
                               f->sources[0].catId, f->sources[0].genre);
      return;
    } }
}

// GANCHO DE TESTE/CAPTURA: poe `n` itens como se a lista aberta os tivesse
// baixado, e faz lst_abrir deixa-los em paz. Sem rede nenhuma lista tem item, e
// o resumo do menu de contexto (ctxmenu.c) so se julga com eles na tela.
void lst_teste_itens(const CatItem *v, int n) {
  CatItem *copy = n > 0 ? malloc(sizeof *copy * (size_t)n) : NULL;
  if (n > 0 && !copy) return;
  if (n > 0) memcpy(copy, v, sizeof *copy * (size_t)n);
  pthread_mutex_lock(&itTrava);
  free(itens); itens = copy;
  nItens = n; itSimkl = 1; itInjetado = n > 0; itGeracao++;
  pthread_mutex_unlock(&itTrava);
}

int lst_itens_n(void) {
  int v;
  pthread_mutex_lock(&itTrava);
  v = itSimkl ? nItens : -1;
  pthread_mutex_unlock(&itTrava);
  // #366: a Biblioteca mostra a lista recebida, nao a projecao da descoberta.
  return v >= 0 ? v : desc_vertudo_n_bruto();
}

int lst_item(int i, CatItem *dst) {
  int simkl, ok = 0;
  i = mapear(i);
  pthread_mutex_lock(&itTrava);
  simkl = itSimkl;
  if (simkl && dst && i >= 0 && i < nItens) { *dst = itens[i]; ok = 1; }
  pthread_mutex_unlock(&itTrava);
  if (simkl) return ok;
  return desc_vertudo_item_bruto(i, dst);
}

int lst_itens_carregando(void) {
  int v;
  pthread_mutex_lock(&itTrava);
  v = itSimkl ? itFioVivo : -1;
  pthread_mutex_unlock(&itTrava);
  return v >= 0 ? v : desc_vertudo_carregando();
}

int lst_item_extra(int i, DescItemLista *dst) {
  int simkl, r;
  pthread_mutex_lock(&itTrava);
  simkl = itSimkl;
  pthread_mutex_unlock(&itTrava);
  if (dst) memset(dst, 0, sizeof *dst);
  r = mapear(i);
  if (simkl) {
    if (i < 0 || i >= lst_itens_n()) return 0;
    if (dst && itInjetado) *dst = itExtraTeste[r];
    return 1;
  }
  return desc_vertudo_extra(r, dst);
}

// So para testes e capturas: rank/nota dos itens de lst_teste_itens, ja que sem
// rede o Trakt nao entrega nenhum.
void lst_teste_extras(const DescItemLista *v, int n) {
  if (n > LST_ITENS_MAX) n = LST_ITENS_MAX;
  pthread_mutex_lock(&itTrava);
  memset(itExtraTeste, 0, sizeof itExtraTeste);
  for (int i = 0; i < n; i++) itExtraTeste[i] = v[i];
  pthread_mutex_unlock(&itTrava);
}

int lst_ordem_mover(int de, int para) {
  int n = lst_itens_n(), v, k;
  if (de < 0 || para < 0 || de >= n || para >= n || n > LST_ITENS_MAX) return 0;
  pthread_mutex_lock(&itTrava);
  if (itPermN != n) {
    int novo[LST_ITENS_MAX];
    for (k = 0; k < n; k++) novo[k] = k < itPermN ? itPerm[k] : k;
    memcpy(itPerm, novo, sizeof(int) * (size_t)n);
    itPermN = n;
  }
  v = itPerm[de];
  for (k = de; k < para; k++) itPerm[k] = itPerm[k + 1];
  for (k = de; k > para; k--) itPerm[k] = itPerm[k - 1];
  itPerm[para] = v;
  pthread_mutex_unlock(&itTrava);
  return 1;
}

int lst_ordem_ids(long *ids, int max) {
  int n = lst_itens_n(), i;
  DescItemLista d;
  if (n < 1 || n > max) return 0;
  for (i = 0; i < n; i++) {
    if (!lst_item_extra(i, &d) || d.itemId <= 0) return 0;
    ids[i] = d.itemId;
  }
  return n;
}

int lst_url_publica(const LstLista *l, char *dst, size_t n) {
  if (!l || !dst || l->fonte != LST_TRAKT || l->privada || !l->autorSlug[0] || !l->slug[0]) return 0;
  return snprintf(dst, n, "https://trakt.tv/users/%s/lists/%s", l->autorSlug, l->slug) < (int)n;
}

void lst_itens_mais(void) {
  int simkl;
  pthread_mutex_lock(&itTrava);
  simkl = itSimkl;
  pthread_mutex_unlock(&itTrava);
  if (!simkl) desc_vertudo_mais();
}

#ifndef LST_SO_ITENS
void lst_abrir_sessao(void) {
  lst_esquecer();
  pthread_mutex_lock(&trava);
  termoBusca[0] = 0; for (int i = 0; i < LST_TAB_N; i++) tabs[i].fioVivo = 0;
  pthread_mutex_unlock(&trava);
  pthread_mutex_lock(&itTrava);
  free(itens); itens = NULL;
  itGeracao++; nItens = itFioVivo = itSimkl = itInjetado = 0;
  itEstado[0] = itMidia[0] = 0;
  pthread_mutex_unlock(&itTrava);
  lst_iniciar();
}
#endif
