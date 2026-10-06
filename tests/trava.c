// Trava de serie: a regra do limite, o gate de spoiler, as frases, o arquivo e
// a fusao com o servidor. Sem SDL e sem rede (trava.c + dados.c + js.c).
#include "trava.h"
#include "dados.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// i18n identidade: as frases saem na chave em portugues.
const char *i18n(const char *s) { return s; }

static long long fixo = 1700000000;
static long long relogioFixo(void) { return fixo; }

static int eps10(void *ctx, int t) { (void)ctx; return t == 1 ? 10 : t == 2 ? 8 : 0; }
static const char *nomes[] = { "Henrique", "Jerry", "Sandra", "Ana" };
static const char *nomeDe(void *ctx, int i) { (void)ctx; return i >= 0 && i < 4 ? nomes[i] : "?"; }

static Trava base(int modo) {
  Trava t;
  int i;
  memset(&t, 0, sizeof t);
  snprintf(t.id, sizeof t.id, "x1");
  snprintf(t.serie, sizeof t.serie, "tt0001");
  t.modo = modo;
  t.n = 3;
  for (i = 0; i < 3; i++) { t.m[i].perfil = i + 1; t.m[i].estado = TRAVA_DENTRO; }
  return t;
}
static void pos(Trava *t, int i, int te, int e) { t->m[i].t = te; t->m[i].e = e; }

static void regra(void) {
  Trava t = base(TRAVA_SEPARADOS);
  int quem[8], n, lt, le;

  // Ninguem comecou: so o T1E1 esta liberado; o T1E2 espera os dois.
  assert(trava_liberado(&t, 0, 1, 1, 0, eps10, NULL, quem, &n) && n == 0);
  assert(!trava_liberado(&t, 0, 1, 2, 0, eps10, NULL, quem, &n) && n == 2);
  assert(trava_limite(&t, 0, eps10, NULL, &lt, &le) && lt == 1 && le == 1);

  // Jerry no E3, Sandra no E1: eu posso ate o E2 (o proximo depois de Sandra).
  pos(&t, 1, 1, 3); pos(&t, 2, 1, 1); pos(&t, 0, 1, 2);
  assert(trava_limite(&t, 0, eps10, NULL, &lt, &le) && lt == 1 && le == 2);
  assert(trava_liberado(&t, 0, 1, 2, 0, eps10, NULL, NULL, NULL));
  assert(!trava_liberado(&t, 0, 1, 3, 0, eps10, NULL, quem, &n));
  assert(n == 1 && quem[0] == 2);                       // so Sandra segura
  // Mais a frente, os dois seguram.
  assert(!trava_liberado(&t, 0, 1, 5, 0, eps10, NULL, quem, &n) && n == 2);

  // Rever: o que EU ja vi nunca e bloqueado.
  assert(trava_liberado(&t, 0, 1, 5, 1, eps10, NULL, NULL, NULL));
  // Especial (temporada 0) fica fora da regra.
  assert(trava_liberado(&t, 0, 0, 7, 0, eps10, NULL, NULL, NULL));

  // Passar a frente nao muda o limite dos outros: o meu ponto no E9 so
  // libera para eles ate o E10.
  pos(&t, 0, 1, 9);
  assert(trava_limite(&t, 2, eps10, NULL, &lt, &le) && lt == 1 && le == 4);   // de Jerry (E3) -> E4

  // Virada de temporada: com 10 episodios na T1, o proximo depois do E10 e o
  // T2E1. Sem a contagem (NULL), espera no T1E11 — o erro seguro.
  pos(&t, 1, 1, 10); pos(&t, 2, 1, 10);
  assert(trava_liberado(&t, 0, 2, 1, 0, eps10, NULL, NULL, NULL));
  assert(!trava_liberado(&t, 0, 2, 2, 0, eps10, NULL, NULL, NULL));
  assert(!trava_liberado(&t, 0, 2, 1, 0, NULL, NULL, NULL, NULL));

  // Juntos e pausada: ninguem espera.
  t.modo = TRAVA_JUNTOS;
  pos(&t, 1, 0, 0);
  assert(trava_liberado(&t, 0, 3, 3, 0, eps10, NULL, NULL, NULL));
  assert(!trava_limite(&t, 0, eps10, NULL, &lt, &le));
  t.modo = TRAVA_SEPARADOS; t.pausada = 1;
  assert(trava_liberado(&t, 0, 3, 3, 0, eps10, NULL, NULL, NULL));
  t.pausada = 0;

  // Quem saiu (ou ainda e convidado, a onda dos amigos) nao segura ninguem.
  t.m[1].estado = TRAVA_SAIU;
  assert(trava_liberado(&t, 0, 2, 1, 0, eps10, NULL, quem, &n) && n == 0);
  assert(!trava_liberado(&t, 0, 2, 2, 0, eps10, NULL, quem, &n) && n == 1 && quem[0] == 2);
  printf("regra do limite: ok\n");
}

static void frases(void) {
  Trava t = base(TRAVA_SEPARADOS);
  char s[256];
  int q[2] = { 1, 2 }, q3[3] = { 1, 2, 3 };
  assert(trava_frase(&t, 0, nomeDe, NULL, s, sizeof s) == TRAVA_FRASE_NAO_COMECARAM);
  pos(&t, 0, 1, 1); pos(&t, 1, 1, 1); pos(&t, 2, 1, 1);
  assert(trava_frase(&t, 0, nomeDe, NULL, s, sizeof s) == TRAVA_FRASE_MESMO);
  assert(!strcmp(s, "Vocês estão no mesmo episódio, T1E1."));
  pos(&t, 0, 1, 4); pos(&t, 1, 1, 4); pos(&t, 2, 1, 2);
  assert(trava_frase(&t, 0, nomeDe, NULL, s, sizeof s) == TRAVA_FRASE_ESPERA_UM);
  assert(!strcmp(s, "Sandra está 2 episódios atrás: você espera no T1E5."));
  pos(&t, 2, 1, 3);
  trava_frase(&t, 0, nomeDe, NULL, s, sizeof s);
  assert(!strcmp(s, "Sandra está 1 episódio atrás: você espera no T1E5."));
  pos(&t, 1, 1, 2);
  assert(trava_frase(&t, 0, nomeDe, NULL, s, sizeof s) == TRAVA_FRASE_ESPERA_VARIOS);
  assert(!strcmp(s, "Você espera por Jerry e Sandra no T1E5."));
  assert(trava_frase(&t, 1, nomeDe, NULL, s, sizeof s) == TRAVA_FRASE_ATRAS);
  assert(!strcmp(s, "Você está atrás: Henrique está no T1E4."));
  pos(&t, 2, 0, 0);
  pos(&t, 1, 1, 4);
  trava_frase(&t, 0, nomeDe, NULL, s, sizeof s);
  assert(!strcmp(s, "Sandra ainda não começou: você espera no T1E5."));
  t.modo = TRAVA_JUNTOS;
  assert(trava_frase(&t, 0, nomeDe, NULL, s, sizeof s) == TRAVA_FRASE_JUNTOS);
  t.pausada = 1;
  assert(trava_frase(&t, 0, nomeDe, NULL, s, sizeof s) == TRAVA_FRASE_PAUSADA);
  trava_esperando_texto(q, 1, nomeDe, NULL, s, sizeof s); assert(!strcmp(s, "Esperando Jerry"));
  trava_esperando_texto(q, 2, nomeDe, NULL, s, sizeof s); assert(!strcmp(s, "Esperando Jerry e Sandra"));
  trava_esperando_texto(q3, 3, nomeDe, NULL, s, sizeof s); assert(!strcmp(s, "Esperando Jerry e mais 2"));
  printf("frases: ok\n");
}

static void perfisCasa(int ativo) {
  TravaPerfil p[3];
  memset(p, 0, sizeof p);
  p[0].indice = 1; p[0].primario = 1; snprintf(p[0].nome, sizeof p[0].nome, "Henrique");
  p[1].indice = 2; p[1].temPin = 1;  snprintf(p[1].nome, sizeof p[1].nome, "Jerry");
  p[2].indice = 3;                   snprintf(p[2].nome, sizeof p[2].nome, "Sandra");
  trava_definir_perfis(p, 3, ativo);
}

static void estado(void) {
  int pf[2] = { 2, 3 }, i, idx[4];
  char pess[96];
  TravaOp op;
  TravaAlcance ev;
  const Trava *t;
  trava_definir_conta("abc-123");
  perfisCasa(1);
  trava_pessoa_do_perfil(1, pess, sizeof pess); assert(!strcmp(pess, "nuvio:abc-123"));
  trava_pessoa_do_perfil(3, pess, sizeof pess); assert(!strcmp(pess, "nuvio:abc-123:3"));

  i = trava_criar("tt0001:1:2", "Severance", "https://x/p.jpg", TRAVA_SEPARADOS, 1, pf, 2);
  assert(i == 0 && trava_n() == 1);
  t = trava_i(0);
  assert(t->local && t->n == 3 && t->m[0].perfil == 1 && !strcmp(t->serie, "tt0001"));
  assert(!strcmp(t->m[2].pessoa, "nuvio:abc-123:3"));
  assert(trava_fila_n() == 1 && trava_fila_ver(0, &op) && !strcmp(op.op, "criar"));
  // Uma trava so com o proprio perfil nao existe.
  assert(trava_criar("tt0002", "X", "", TRAVA_JUNTOS, 0, NULL, 0) < 0);

  assert(trava_da_serie("tt0001", 3, idx, 4) == 1 && idx[0] == 0);
  assert(trava_da_serie("tt0001", 4, idx, 4) == 0);

  // Passo: so avanca; desfazer volta ao ponto exato.
  assert(trava_passo("tt0001", 1, 1, 3, 0) == 1);
  assert(trava_passo("tt0001", 1, 1, 2, 0) == 0);
  assert(trava_i(0)->m[0].t == 1 && trava_i(0)->m[0].e == 3);
  assert(trava_passo("tt0001", 1, 1, 2, 1) == 1 && trava_i(0)->m[0].e == 2);
  assert(trava_passo("tt0001", 1, 0, 0, 0) == 0);
  assert(trava_passo("tt0001", 3, 0, 0, 1) == 0);   // ja estava em 0,0: nada muda
  assert(trava_fila_n() == 3);
  trava_fila_ver(1, &op);
  assert(!strcmp(op.op, "passo") && op.a == 1 && op.b == 3 && !strcmp(op.pessoa, "nuvio:abc-123"));

  // "Alcancou voce": Sandra (3) passa do meu ponto (E2).
  assert(!trava_alcance_proximo(&ev));
  assert(trava_posicao(trava_i(0)->id, 3, 1, 2, fixo));
  assert(trava_alcance_proximo(&ev) && ev.membro == 2 && ev.e == 2);
  assert(!trava_alcance_proximo(&ev));

  // Gestao.
  assert(trava_definir_modo(trava_i(0)->id, TRAVA_JUNTOS) && trava_i(0)->modo == TRAVA_JUNTOS);
  assert(!trava_definir_modo(trava_i(0)->id, TRAVA_JUNTOS));
  { int so[1] = { 2 };
    assert(trava_definir_membros(trava_i(0)->id, so, 1) && trava_i(0)->n == 2); }

  // O ARQUIVO volta igual.
  { Trava antes = *trava_i(0);
    int nf = trava_fila_n();
    trava_carregar();
    assert(trava_n() == 1 && trava_fila_n() == nf);
    t = trava_i(0);
    assert(!strcmp(t->id, antes.id) && t->n == antes.n && t->modo == antes.modo &&
           t->pin == 1 && !strcmp(t->titulo, "Severance") && t->m[0].e == 2); }

  // O SERVIDOR confirma e manda as posicoes.
  { char idLocal[40];
    snprintf(idLocal, sizeof idLocal, "%s", trava_i(0)->id);
    trava_confirmar(idLocal, "srv1");
    assert(!trava_i(0)->local && !strcmp(trava_i(0)->id, "srv1"));
    trava_fila_ver(0, &op);
    assert(!strcmp(op.id, "srv1"));
    trava_fila_feita(trava_fila_n());
    assert(trava_fila_n() == 0); }
  assert(trava_aplicar_servidor(
    "{\"rev\":7,\"travas\":[{\"id\":\"srv1\",\"serie\":\"tt0001\",\"titulo\":\"Severance\","
    "\"poster\":\"\",\"modo\":1,\"pin\":1,\"pausada\":0,\"criador\":\"nuvio:abc-123\","
    "\"criado\":1,\"atualizado\":2,\"membros\":["
    "{\"pessoa\":\"nuvio:abc-123\",\"estado\":1,\"t\":1,\"e\":1,\"em\":5},"
    "{\"pessoa\":\"nuvio:abc-123:2\",\"estado\":1,\"t\":1,\"e\":6,\"em\":6}]},"
    "{\"id\":\"srv2\",\"serie\":\"tt0009\",\"titulo\":\"Other\",\"modo\":0,\"membros\":["
    "{\"pessoa\":\"nuvio:abc-123:3\",\"estado\":1,\"t\":0,\"e\":0,\"em\":0},"
    "{\"pessoa\":\"trakt:amigo\",\"estado\":0,\"t\":2,\"e\":2,\"em\":0}]}]}", 1));
  assert(trava_n() == 2);
  t = trava_por_id("srv1");
  // A minha posicao local (E2) e mais adiantada que a do servidor (E1): fica.
  assert(t && t->m[0].perfil == 1 && t->m[0].e == 2 && t->m[1].perfil == 2 && t->m[1].e == 6);
  t = trava_por_id("srv2");
  assert(t && t->m[0].perfil == 3 && t->m[1].perfil == 0 && t->m[1].estado == TRAVA_CONVIDADO);
  // Jerry (2) passou de mim (E2) pelo servidor: alcance.
  assert(trava_alcance_proximo(&ev) && ev.membro == 1);

  // Apagar trava confirmada vira operacao; trava local some com a fila dela.
  assert(trava_apagar("srv2") && trava_n() == 1 && trava_fila_n() == 1);
  { int b[1] = { 3 };
    int k = trava_criar("tt0003", "Local", "", TRAVA_SEPARADOS, 0, b, 1);
    char id[40];
    snprintf(id, sizeof id, "%s", trava_i(k)->id);
    assert(trava_fila_n() == 2);
    assert(trava_apagar(id) && trava_fila_n() == 1); }

  // Outra conta nao ve nada desta.
  trava_definir_conta("outra");
  assert(trava_n() == 0);
  trava_definir_conta("abc-123");
  assert(trava_n() == 1);
  // A resposta e POR PERFIL: a lista vazia pedida pelo perfil 3 (que nao e
  // membro de srv1) nao apaga srv1; a do perfil 2 (membro) apaga.
  assert(trava_aplicar_servidor("{\"rev\":9,\"travas\":[]}", 3) && trava_n() == 1);
  assert(trava_aplicar_servidor("{\"rev\":9,\"travas\":[]}", 2) && trava_n() == 0);
  printf("estado, arquivo e servidor: ok\n");
}

int main(void) {
  char tmpl[512];
  char *dir;
  snprintf(tmpl, sizeof tmpl, "%s/nv-trava-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  dir = mkdtemp(tmpl);
  assert(dir);
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  trava_relogio(relogioFixo);
  regra();
  frases();
  estado();
  printf("trava: tudo ok\n");
  return 0;
}
