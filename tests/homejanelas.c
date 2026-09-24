// JANELAS DAS FILEIRAS APONTANDO PARA ITENS DE OUTRO BLOCO.
//
// Medido na LG C9 do dono (A/B contra o master 226af57): com o trabalho do
// Codex a fileira "Amigos assistindo" mostrava "The Martian" e "Project Hail
// Mary" sem nome e sem acao, e "Continuar assistindo" mostrava Fallout/Widow's
// Bay em vez de Imperfect Women/Adolescence. Titulos do CATALOGO DO PACOTE nas
// janelas (ini, n) das fileiras sinteticas.
//
// A sequencia do log (codex-run.log): o pacote esta na tela (cat_n() > 0, logo
// montar() NAO publica em partes); a assinatura do homeestado muda no meio da
// montagem (perfil/colecoes/catordem chegando), o fim de montar() descarta o
// lote SEM publicar — mas filsMontadas ja tinha as janelas desse lote; o sync
// entrega as colecoes e chama desc_remontar_fileiras(), que republica
// filsMontadas (janelas do lote descartado) por cima do bloco que esta na tela
// (o do pacote). Resultado: "continue_watching" ini=0 n=2 aponta para os dois
// primeiros titulos do pacote, "social_activity" para o terceiro.
//
// O catalogo aqui e um FALSO fiel: guarda o bloco publicado e corta as janelas
// como catalogo.c (cat_definir_tudo / cat_republicar_fileiras), e
// cat_copiar_fileira copia do bloco publicado. O teste confere, fileira por
// fileira, que cada item veio de quem produziu aquela fileira.
#include <assert.h>
#include <unistd.h>
// Duples da remocao do Continuar assistindo (fix/cw-remover): este teste nao
// exercita a remocao, e o catalogo aqui e falso.
void prog_remover(const char *chave) { (void)chave; }
void prog_marcar_removido(const char *imdb) { (void)imdb; }
int  prog_removido_vence(const char *imdb, long long instanteMs) { (void)imdb; (void)instanteMs; return 0; }
int  cat_tirar_continuar(const char *imdb) { (void)imdb; return 0; }
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

#include "../src/descoberta.c"

#define BASE "https://addon.example/abc"
#define AID  "app.addon.demo"

static const char *MANIFESTO =
  "{\"id\":\"" AID "\",\"name\":\"Addon\",\"resources\":[\"catalog\"],\"catalogs\":["
  "{\"type\":\"movie\",\"id\":\"cat_a\",\"name\":\"Cat A\"},"
  "{\"type\":\"movie\",\"id\":\"cat_b\",\"name\":\"Cat B\"}]}";

int   addons_n(void)                   { return 1; }
const char *addons_base(int i)         { (void)i; return BASE; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *addons_nome(int i)         { (void)i; return "addon"; }
unsigned addons_versao(void)           { return 1; }
const char *addons_base_por_id(const char *id) { return id && !strcmp(id, AID) ? BASE : ""; }
void addons_manifesto_lido(int i, const char *c) { (void)i; (void)c; }

// ------------------------------------------------------------ o homeestado
// Fiel no que importa: a geracao muda quando o contexto muda, e o snapshot
// so vale depois de salvo na geracao corrente.
static volatile unsigned geracaoEstado = 1;
static int snapshotValido;
static char snapChaves[CAT_FIL_MAX][192];
static int nSnap;
unsigned homeestado_geracao(void) { return geracaoEstado; }
int homeestado_contexto_valido(void) { return snapshotValido; }
int homeestado_ordem_fileira(const char *c) {
  int i;
  if (!snapshotValido || !c) return -1;
  for (i = 0; i < nSnap; i++) if (!strcmp(snapChaves[i], c)) return i;
  return -1;
}
int homeestado_tem_fileira(const char *c) { return homeestado_ordem_fileira(c) >= 0; }
int homeestado_salvar_se_geracao(const CatFileira *f, int n, unsigned g) {
  int i;
  if (g != geracaoEstado) return 0;
  for (i = 0, nSnap = 0; i < n && i < CAT_FIL_MAX; i++)
    snprintf(snapChaves[nSnap++], sizeof snapChaves[0], "%s", f[i].chave);
  snapshotValido = 1;
  return 1;
}
void homeestado_salvar(const CatFileira *f, int n) { homeestado_salvar_se_geracao(f, n, geracaoEstado); }
int homeestado_identidade_geracao(unsigned g, char *d, unsigned z, int *p) {
  if (g != geracaoEstado) return 0;
  if (d && z) d[0] = 0;
  if (p) *p = 1;
  return 1;
}
// DESDE A 1.4.5 a montagem pergunta O QUE mudou, e so identidade/addons
// descartam. O que este teste muda no meio (colecoes/ordem chegando) e
// ESTRUTURA: a geracao entra na parte das colecoes, e o fim de montar() segue
// o caminho "publica e remonta sem rede" — que tambem tem de manter cada
// janela apontando para os itens da propria fileira.
void homeestado_contexto(HomeContexto *c) {
  *c = (HomeContexto){0}; c->perfil = 1; c->colecoes = geracaoEstado; }
int homeestado_mudancas(const HomeContexto *a, const HomeContexto *b) {
  return a->colecoes != b->colecoes ? HOMEESTADO_MUDOU_COLECOES : 0; }
const char *homeestado_mudancas_texto(int m, char *b, unsigned t) {
  if (b && t) snprintf(b, t, "%s", m ? "colecoes" : "nada"); return b; }
const char *sessao_usuario(void) { return ""; }

// ------------------------------------------------------ o catalogo publicado
static pthread_mutex_t pubTravaT = PTHREAD_MUTEX_INITIALIZER;
static CatItem pub[4096];
static int nPub;
static CatFileira pubFils[CAT_FIL_MAX];
static int nPubFils;

// Mesmo corte de catalogo.c: janela fora do bloco sai, janela que passa do
// fim e cortada. E exatamente esse corte que fazia a janela velha "caber" no
// bloco do pacote.
static void pubFileiras(const CatFileira *f, int q) {
  int k, v = 0;
  for (k = 0; k < q && k < CAT_FIL_MAX; k++) {
    CatFileira x = f[k];
    if (x.ini < 0 || x.ini > nPub || (x.n < 1 && !x.estado)) continue;
    if (x.ini + x.n > nPub) x.n = nPub - x.ini;
    pubFils[v++] = x;
  }
  nPubFils = v;
}
void cat_definir_tudo(const CatItem *l, int q, const CatFileira *f, int n) {
  pthread_mutex_lock(&pubTravaT);
  if (q > (int)(sizeof pub / sizeof *pub)) q = (int)(sizeof pub / sizeof *pub);
  if (q > 0) memcpy(pub, l, sizeof(CatItem) * (size_t)q);
  nPub = q < 0 ? 0 : q;
  pubFileiras(f, n);
  pthread_mutex_unlock(&pubTravaT);
}
void cat_republicar_fileiras(const CatFileira *f, int n) {
  pthread_mutex_lock(&pubTravaT); pubFileiras(f, n); pthread_mutex_unlock(&pubTravaT);
}
int cat_n(void) { return nPub; }
int cat_n_fileiras(void) { return nPubFils; }
const CatFileira *cat_fileira(int r) { return (r >= 0 && r < nPubFils) ? &pubFils[r] : NULL; }
int cat_copiar_fileira(const char *chave, CatItem *saida, int max, CatFileira *meta) {
  int r, qtd, got = 0;
  pthread_mutex_lock(&pubTravaT);
  for (r = 0; r < nPubFils; r++) {
    CatFileira *f = &pubFils[r];
    if (strcmp(f->chave, chave) || f->n < 1) continue;
    qtd = f->n < max ? f->n : max;
    if (f->ini < 0 || f->ini + qtd > nPub) break;
    memcpy(saida, pub + f->ini, sizeof(CatItem) * (size_t)qtd);
    if (meta) *meta = *f;
    got = qtd;
    break;
  }
  pthread_mutex_unlock(&pubTravaT);
  return got;
}
int cat_gravar_cache_se_identidade(const char *d, const char *u, int p) { (void)d; (void)u; (void)p; return 1; }
int   cat_do_cache(void)              { return 0; }
unsigned long cat_assinatura(void)    { return 0; }
unsigned long cat_assinatura_de(const CatItem *l, int q, const CatFileira *f, int n) {
  (void)l; (void)q; (void)f; (void)n; return 1; }   // sempre "mudou": publica

// ------------------------------------------------------------------ a rede
// Cada catalogo devolve 3 titulos cujo nome comeca com o catId: e assim que a
// conferencia sabe de onde o item veio.
static volatile int pedidosCatalogo, catBMudo;
// O ULTRA MAX DO #126: 174 catalogos declarados, e mais um que so responde com
// busca (nao pode gastar vaga da cota).
#define ULTRA_N 174
static char *manifestoUltra(void) {
  size_t cap = 64u + (size_t)(ULTRA_N + 1) * 96u, k = 0;
  char *b = (char *)malloc(cap);
  int i;
  k += (size_t)snprintf(b + k, cap - k, "{\"id\":\"ultramax\",\"name\":\"Ultra MAX\",\"catalogs\":[");
  k += (size_t)snprintf(b + k, cap - k,
                        "{\"type\":\"movie\",\"id\":\"busca\",\"name\":\"Busca\","
                        "\"extra\":[{\"name\":\"search\",\"isRequired\":true}]}");
  for (i = 0; i < ULTRA_N; i++)
    k += (size_t)snprintf(b + k, cap - k,
                          ",{\"type\":\"movie\",\"id\":\"u%d\",\"name\":\"U%d\"}", i, i);
  snprintf(b + k, cap - k, "]}");
  return b;
}
static volatile int bumpNoPrimeiroCatalogo = 1;
char *rede_baixar(const char *url, int t) {
  char buf[512];
  const char *id;
  (void)t;
  if (strstr(url, "ultramax.test/manifest.json")) return manifestoUltra();
  if (strstr(url, "/manifest.json")) return strdup(MANIFESTO);
  if (!strstr(url, "/catalog/")) return NULL;
  // PERFIL/CONFIG MUDANDO NO MEIO DA PRIMEIRA MONTAGEM: no log da LG o
  // "[perfis] ... ativo=1" e as colecoes chegam enquanto os catalogos baixam.
  if (__sync_fetch_and_add(&pedidosCatalogo, 1) == 0 && bumpNoPrimeiroCatalogo)
    geracaoEstado++;
  id = strstr(url, "cat_a") ? "cat_a" : strstr(url, "cat_b") ? "cat_b" : NULL;
  if (!id) return NULL;
  if (catBMudo && !strcmp(id, "cat_b")) return NULL;   // timeout/sem resposta
  snprintf(buf, sizeof buf,
           "{\"metas\":[{\"id\":\"tt%s1\",\"type\":\"movie\",\"name\":\"%s 1\",\"poster\":\"p.jpg\"},"
           "{\"id\":\"tt%s2\",\"type\":\"movie\",\"name\":\"%s 2\",\"poster\":\"p.jpg\"},"
           "{\"id\":\"tt%s3\",\"type\":\"movie\",\"name\":\"%s 3\",\"poster\":\"p.jpg\"}]}",
           id + 4, id, id + 4, id, id + 4, id);
  return strdup(buf);
}
char *rede_baixar_com(const char *u, int t, const char *const *c) { (void)c; return rede_baixar(u, t); }

// ------------------------------------------------------------- o Trakt
int trakt_continuar(CatItem *s, int m) {
  int i;
  for (i = 0; i < 2 && i < m; i++) {
    memset(&s[i], 0, sizeof s[i]);
    snprintf(s[i].imdb, sizeof s[i].imdb, "tt900%d", i + 1);
    snprintf(s[i].tipo, sizeof s[i].tipo, "series");
    snprintf(s[i].titulo, sizeof s[i].titulo, "CW %d", i + 1);
    s[i].progresso = 50; s[i].temporada = 1; s[i].episodio = i + 1;
  }
  return i;
}
// Segunda chamada = segunda montagem. E ali, no log, entre "trakt continuar
// assistindo" e "trakt atividade dos amigos", que o sync entrega as colecoes:
// "[desc] fileiras remontadas sem rede: 12 de 12". A config muda junto.
static volatile int chamadasSocial, errosNoRemontar = -1;
static int conferir(const char *rotulo);
int trakt_social(CatItem *s, int m) {
  if (++chamadasSocial == 2) {
    // O QUE A HOME MOSTRA NESTE INSTANTE, e nao so no fim: uma volta posterior
    // que publique certo esconderia a janela torta que ficou na tela ate ela.
    desc_remontar_fileiras();
    errosNoRemontar = conferir("logo depois de desc_remontar_fileiras");
    geracaoEstado++;
  }
  if (m < 1) return 0;
  memset(&s[0], 0, sizeof s[0]);
  snprintf(s[0].imdb, sizeof s[0].imdb, "tt9101");
  snprintf(s[0].tipo, sizeof s[0].tipo, "series");
  snprintf(s[0].titulo, sizeof s[0].titulo, "Social 1");
  snprintf(s[0].socialNome, sizeof s[0].socialNome, "Kevin");
  snprintf(s[0].pais, sizeof s[0].pais, "Kevin");
  snprintf(s[0].socialAcao, sizeof s[0].socialAcao, "checkin");
  return 1;
}

// ------------------------------------------------------------------ o resto
void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
int   ajustes_cw_fonte(void)               { return AJ_CWF_TRAKT; }
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
int   ajustes_idioma_ingles(void)          { return 0; }
int   ajustes_tmdb_ligado(void)            { return 0; }
int   ajustes_tmdb_basico(void)            { return 0; }
int   trakt_e_a_seguir(const char *id)     { (void)id; return 0; }
const char *ajustes_tmdb_idioma(void)      { return "pt-BR"; }
int   cat_acrescentar(const CatItem *i)    { (void)i; return -1; }
void  cat_atualizar_item(int i, const CatItem *n) { (void)i; (void)n; }
void  cat_cache_substituido(void)          { }
void  cat_definir_episodios(int i, const CatEp *l, int n) { (void)i; (void)l; (void)n; }
int   cat_gravar_cache(const char *d)      { (void)d; return 0; }
int   cat_indice_por_imdb(const char *s)   { (void)s; return -1; }
const CatItem *cat_item(int i)             { return (i >= 0 && i < nPub) ? &pub[i] : NULL; }
int   cat_n_episodios(int i)               { (void)i; return 0; }
void  fil_gravar_registro(void)            { }
int   fil_podar_catalogos(const char *const *ids, const char *const *bases, int n) {
  (void)ids; (void)bases; (void)n; return 0; }
int   fil_limite(void)                     { return 16; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
// Dubles da escolha da cota (#126): nada escolhido na TV, e o registro dos
// catalogos fora da cota nao interessa a este teste.
int fil_escolhida(const char *c) { (void)c; return -1; }
void fil_registrar_se_couber(const char *c, const char *t, const char *a,
                             const char *tp) { (void)c; (void)t; (void)a; (void)tp; }
void  fil_registrar(const char *c, const char *t, const char *a, const char *tp, int itens) {
  (void)c; (void)t; (void)a; (void)tp; (void)itens; }
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i; }
const char *i18n(const char *s)            { return s; }
void  marco(const char *n)                 { (void)n; }
void  prog_chave(char *d, unsigned n, const char *c, int t, int e) { (void)c; (void)t; (void)e; if (n) d[0] = 0; }
void  prog_content_id(char *d, unsigned n, const char *i, int *t, int *e) { (void)i; (void)t; (void)e; if (n) d[0] = 0; }
int   prog_ler(ProgRegistro *s, int m)     { (void)s; (void)m; return 0; }
int   prog_por_chave(const char *c, ProgRegistro *s) { (void)c; (void)s; return 0; }
int   simkl_ativo(void)                    { return 0; }
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; (void)n; return 0; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
void  cat_trocar_continuar(const CatItem *l, int q) { (void)l; (void)q; }
// trakt_lista devolve 0 aqui: nada para mesclar.
int   cat_mesclar_listas(const CatItem *v, int q) { (void)v; (void)q; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }

// ------------------------------------------------------------------ o teste
// Quieto = sem montagem no ar por 300 ms seguidos. O fim de montar() zera
// `buscando` e pode disparar outra volta logo em seguida (repetirAoFim).
static void esperarQuieto(void) {
  int i, quieto = 0;
  for (i = 0; i < 500 && !buscando; i++) usleep(2000);
  for (i = 0; i < 10000 && quieto < 150; i++) {
    usleep(2000);
    quieto = buscando ? 0 : quieto + 1;
  }
  assert(quieto >= 150);
}

static void semear_pacote(void) {
  static CatItem pk[30];
  CatFileira f[3];
  int i;
  memset(pk, 0, sizeof pk); memset(f, 0, sizeof f);
  for (i = 0; i < 30; i++) {
    snprintf(pk[i].imdb, sizeof pk[i].imdb, "ttpkg%02d", i);
    snprintf(pk[i].tipo, sizeof pk[i].tipo, "movie");
    snprintf(pk[i].titulo, sizeof pk[i].titulo, "pacote %d", i);
  }
  for (i = 0; i < 3; i++) {
    snprintf(f[i].chave, sizeof f[i].chave, "pkg_%d", i);
    f[i].ini = i * 10; f[i].n = 10;
  }
  cat_definir_tudo(pk, 30, f, 3);
}

static int conta_linha(const char *chave) {
  int r;
  for (r = 0; r < nPubFils; r++) if (!strcmp(pubFils[r].chave, chave)) return r;
  return -1;
}

// O CONTRATO: cada item publicado numa fileira foi produzido por ela.
static int conferir(const char *rotulo) {
  int r, i, erros = 0;
  printf("    %s: %d fileiras, %d itens\n", rotulo, nPubFils, nPub);
  for (r = 0; r < nPubFils; r++) {
    const CatFileira *f = &pubFils[r];
    printf("      %-18s ini=%-3d n=%d:", f->chave, f->ini, f->n);
    for (i = 0; i < f->n; i++) {
      const CatItem *it = &pub[f->ini + i];
      int ok;
      printf(" [%s]", it->titulo);
      if (!strcmp(f->chave, "continue_watching"))
        ok = !strncmp(it->titulo, "CW ", 3);
      else if (!strcmp(f->chave, "social_activity"))
        ok = it->socialNome[0] && !strncmp(it->titulo, "Social ", 7);
      else if (!strncmp(f->chave, "pkg_", 4))
        ok = !strncmp(it->titulo, "pacote ", 7);
      else
        ok = f->catId[0] && !strncmp(it->titulo, f->catId, strlen(f->catId));
      if (!ok) { erros++; printf("<ERRADO>"); }
    }
    printf("\n");
  }
  return erros;
}

int main(void) {
  // ------------------------------------------------------------- caso 0
  // #126: a cota de declaracoes le PELA ORDEM DA CONTA, e nao os primeiros do
  // manifesto. O catalogo 151 do Ultra MAX, primeiro na ordem da conta, era um
  // dos 142 cortados.
  { static Decl d[32];
    int real = 0, prom = 0, n, i, tem150 = 0, tem100 = 0, tem31 = 0, temBusca = 0;
    assert(catordem_ler("[{\"settings_json\":{\"items\":["
                        "{\"addon_id\":\"ultramax\",\"type\":\"movie\",\"catalog_id\":\"u150\",\"order\":0},"
                        "{\"addon_id\":\"ultramax\",\"type\":\"movie\",\"catalog_id\":\"u100\",\"order\":1}"
                        "]}}]") >= 0);
    n = lerManifesto(0, "https://ultramax.test", d, 32, &real, &prom);
    for (i = 0; i < n; i++) {
      if (!strcmp(d[i].id, "u150")) tem150 = 1;
      if (!strcmp(d[i].id, "u100")) tem100 = 1;
      if (!strcmp(d[i].id, "u31"))  tem31 = 1;
      if (!strcmp(d[i].id, "busca")) temBusca = 1;
    }
    assert(n == 32);
    assert(real == ULTRA_N);           // o que exige busca nao conta como declarado
    assert(tem150 && tem100);          // os da conta entram, alem da posicao 32
    assert(!tem31);                    // quem cede a vaga e o ultimo sem escolha
    assert(!temBusca);                 // o de busca nao gasta vaga
    assert(prom == 2);
    assert(nForaCota == ULTRA_N - 32); // os de fora ficam para a lista de fileiras
    // Ordem do manifesto preservada entre os escolhidos: u100 antes de u150.
    { int p100 = -1, p150 = -1;
      for (i = 0; i < n; i++) { if (!strcmp(d[i].id, "u100")) p100 = i; if (!strcmp(d[i].id, "u150")) p150 = i; }
      assert(p100 < p150); }
    foraCotaSoltar();
    nSoBuscaVolta = 0;
    catordem_esquecer(); }
  puts("ok  a cota le os catalogos da ordem da conta, nao so os primeiros (#126)");

  // ------------------------------------------------------------- caso 1
  // A sequencia da LG: pacote na tela, perfil/config mudando no meio da
  // primeira montagem, colecoes chegando no meio da segunda.
  semear_pacote();
  desc_iniciar();
  esperarQuieto();
  desc_repetir();          // o "[sync] addons: perfil 1 -> 12 linha(s)" do log
  esperarQuieto();
  assert(chamadasSocial >= 2);
  assert(errosNoRemontar == 0);
  assert(conferir("caso 1") == 0);
  assert(conta_linha("continue_watching") >= 0);
  assert(conta_linha("social_activity") >= 0);
  assert(conta_linha("pkg_0") < 0);   // a home da conta substituiu o pacote
  puts("ok  janelas publicadas apontam para os itens da propria fileira");

  // ------------------------------------------------------------- caso 2
  // Snapshot valido (o caso 1 salvou) + cat_b sem resposta: a janela de cat_b
  // sai do bloco publicado por cat_copiar_fileira (linhaAnterior) e entra no
  // lote novo sem deslocar as vizinhas.
  assert(snapshotValido && homeestado_tem_fileira("social_activity"));
  catBMudo = 1;
  bumpNoPrimeiroCatalogo = 0;
  desc_repetir();
  esperarQuieto();
  assert(conferir("caso 2") == 0);
  assert(conta_linha("continue_watching") >= 0 && conta_linha("social_activity") >= 0);
  { int k, achou = 0;
    for (k = 0; k < nPubFils; k++)
      if (!strcmp(pubFils[k].catId, "cat_b") && pubFils[k].n == 3) achou = 1;
    assert(achou); }
  puts("ok  snapshot + timeout: fileira preservada traz os proprios itens");

  // ------------------------------------------------------------- caso 3
  // Snapshot valido + publicacao PROGRESSIVA (tela vazia): o bloco antecipado
  // de continue_watching/social_activity e as publicacoes por fileira.
  catBMudo = 0;
  pthread_mutex_lock(&pubTravaT); nPub = 0; nPubFils = 0; pthread_mutex_unlock(&pubTravaT);
  desc_repetir();
  esperarQuieto();
  assert(conferir("caso 3") == 0);
  assert(conta_linha("continue_watching") >= 0 && conta_linha("social_activity") >= 0);
  puts("ok  snapshot + progressivo: janelas do lote que foi publicado");

  puts("homejanelas: tudo ok");
  return 0;
}
