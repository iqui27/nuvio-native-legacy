// Testes do registro local de progresso. Sem rede, sem SDL, sem disco: dados_*
// e perfis_ativo sao substituidos aqui por versoes em memoria.
#include "progresso.h"
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- dublês ----------------------------------------------------------------
static char *arquivo;          // conteudo de progresso.txt, ou NULL
static int   perfil = 1;
static long long agora = 1757000000000LL;

char *dados_ler(const char *nome) { (void)nome; return arquivo ? strdup(arquivo) : NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; free(arquivo); arquivo = strdup(c); return 1; }
int   dados_apagar(const char *nome) { (void)nome; free(arquivo); arquivo = NULL; return 1; }
int   perfis_ativo(void) { return perfil; }
static long long relogio(void) { return agora; }
// No teste ha um fio so: um ponteiro para uma copia estatica basta.
static const ProgRegistro *porChave(const char *chave) {
  static ProgRegistro c;
  return prog_por_chave(chave, &c) ? &c : NULL;
}

static void zerar(const char *conteudo) {
  free(arquivo);
  arquivo = conteudo ? strdup(conteudo) : NULL;
  prog_invalidar();
}

static int contaLinhas(const char *s, const char *trecho) {
  int n = 0;
  for (; s && (s = strstr(s, trecho)); s++) n++;
  return n;
}

// ---- casos -----------------------------------------------------------------

static void chaveIgualAoWeb(void) {
  char k[48];
  prog_chave(k, sizeof k, "tt1234567", 4, 9);   assert(!strcmp(k, "tt1234567_s4e9"));
  prog_chave(k, sizeof k, "tt1234567", 0, 0);   assert(!strcmp(k, "tt1234567"));
  prog_chave(k, sizeof k, "tt1234567", 0, 3);   assert(!strcmp(k, "tt1234567_s0e3"));  // especiais: season 0
  prog_chave(k, sizeof k, "tt1234567:4:9", 0, 0); assert(!strcmp(k, "tt1234567"));   // id composto e cortado
  { int t = -1, e = -1; char id[24];
    prog_content_id(id, sizeof id, "tt1234567:4:9", &t, &e);
    assert(!strcmp(id, "tt1234567") && t == 4 && e == 9); }
  puts("ok  chave igual ao toProgressKey do web");
}

static void migraFormatoAntigo(void) {
  ProgRegistro r[8];
  int n;
  zerar("tt1234567\t1432\t2640\t4\t9\n"      // serie de catalogo, 5 colunas
        "tt7654321\t5400\t7200\t0\t0\n"      // filme, 5 colunas
        "tt5550000:2:3\t100\t1000\n"         // item do Trakt, id composto, 3 colunas
        "tt9990000\t10\t1\n");               // dur <= 1: ruido, fora
  n = prog_ler(r, 8);
  assert(n == 3);
  { const ProgRegistro *s = porChave("tt1234567_s4e9");
    assert(s && !strcmp(s->tipo, "series") && s->temporada == 4 && s->episodio == 9);
    assert(s->pendente == 1 && s->lastWatchedMs == 0 && s->posSeg == 1432 && s->durSeg == 2640); }
  { const ProgRegistro *m = porChave("tt7654321");
    assert(m && !strcmp(m->tipo, "movie") && m->episodio == 0 && m->pendente); }
  { const ProgRegistro *t = porChave("tt5550000_s2e3");
    assert(t && !strcmp(t->contentId, "tt5550000") && t->temporada == 2 && t->episodio == 3); }
  assert(!porChave("tt9990000"));
  // Tudo antigo e pendente: vai subir de novo com a chave certa.
  assert(prog_pendentes(r, 8) == 3);
  // Primeira gravacao converte o arquivo.
  agora += 1000;
  assert(prog_gravar_local("tt0000001", 0, 0, 30, 3600, -1));
  assert(!strncmp(arquivo, "#nvprog2\n", 9));
  assert(contaLinhas(arquivo, "\n") == 5);   // cabecalho + 4
  prog_invalidar();
  assert(prog_ler(r, 8) == 4);
  puts("ok  formato antigo migrado como pendente");
}

static void gravarLocalEhPendenteComHora(void) {
  const ProgRegistro *s;
  zerar(NULL);
  agora = 1757000100000LL;
  // serie: id puro + temporada/episodio explicitos (caso do catalogo)
  assert(prog_gravar_local("tt1234567", 4, 9, 1432, 2640, -1));
  s = porChave("tt1234567_s4e9");
  assert(s && s->pendente && s->lastWatchedMs == agora && !strcmp(s->tipo, "series"));
  // serie: id composto sem explicitos (caso do Trakt)
  assert(prog_gravar_local("tt1234567:4:10", 0, 0, 5, 2640, -1));
  s = porChave("tt1234567_s4e10");
  assert(s && s->temporada == 4 && s->episodio == 10 && !strcmp(s->contentId, "tt1234567"));
  // regravar a mesma chave substitui, nao duplica
  agora += 60000;
  assert(prog_gravar_local("tt1234567", 4, 9, 1500, 2640, -1));
  s = porChave("tt1234567_s4e9");
  assert(s && s->posSeg == 1500 && s->lastWatchedMs == agora);
  { ProgRegistro r[8]; assert(prog_ler(r, 8) == 2); }
  // ruido do player nao grava
  assert(!prog_gravar_local("tt1234567", 4, 9, 0, 0, -1));
  assert(!prog_gravar_local("tt1234567", 4, 9, 1, 1, -1));
  puts("ok  escrita local: pendente, com hora, sem duplicata");
}

static void pendenteVenceServidor(void) {
  ProgRegistro rem;
  const ProgRegistro *s;
  ProgRegistro enviado;
  zerar(NULL);
  agora = 1757000200000LL;
  assert(prog_gravar_local("tt1234567", 4, 9, 1500, 2640, -1));
  assert(prog_por_chave("tt1234567_s4e9", &enviado));

  // Cenario 1.3 do plano: pull traz o servidor ANTIGO depois de assistir aqui.
  memset(&rem, 0, sizeof rem);
  snprintf(rem.contentId, sizeof rem.contentId, "tt1234567");
  rem.temporada = 4; rem.episodio = 9; rem.posSeg = 900; rem.durSeg = 2640;
  rem.lastWatchedMs = agora - 300000;                  // 5 min mais velho
  assert(prog_aplicar_remoto(&rem) == 0);
  s = porChave("tt1234567_s4e9"); assert(s->posSeg == 1500);

  // Mesmo um remoto MAIS NOVO nao passa enquanto o local nao subiu.
  rem.lastWatchedMs = agora + 60000; rem.posSeg = 2000;
  assert(prog_aplicar_remoto(&rem) == 0);
  s = porChave("tt1234567_s4e9"); assert(s->posSeg == 1500 && s->pendente);

  // Depois do push, remoto mais novo vence; mais velho nao.
  prog_confirmar_empurrados(&enviado, 1);
  s = porChave("tt1234567_s4e9"); assert(!s->pendente);
  assert(prog_aplicar_remoto(&rem) == 1);
  s = porChave("tt1234567_s4e9"); assert(s->posSeg == 2000 && !s->pendente);
  rem.lastWatchedMs = agora - 1; rem.posSeg = 100;
  assert(prog_aplicar_remoto(&rem) == 0);
  s = porChave("tt1234567_s4e9"); assert(s->posSeg == 2000);
  // Empate mantem o local.
  rem.lastWatchedMs = agora + 60000; rem.posSeg = 1;
  assert(prog_aplicar_remoto(&rem) == 0);

  // Titulo que nao existe aqui e RETIDO (item 1.4), com a chave do web.
  memset(&rem, 0, sizeof rem);
  snprintf(rem.contentId, sizeof rem.contentId, "tt4444444");
  rem.posSeg = 10; rem.durSeg = 5000; rem.lastWatchedMs = agora;
  assert(prog_aplicar_remoto(&rem) == 1);
  s = porChave("tt4444444"); assert(s && !s->pendente && !strcmp(s->tipo, "movie"));
  { ProgRegistro r[8]; assert(prog_pendentes(r, 8) == 0); }
  puts("ok  conflito: pendente vence, depois o mais novo vence");
}

// #205: coluna 11 = como a posicao foi alcancada (1 tocando, 0 seek, -1 nao
// se sabe). Linha de 10 colunas e registro remoto valem -1; o eco do proprio
// push (mesma posicao e duracao) mantem o bit local.
static void bitTocouSobreviveAoSync(void) {
  ProgRegistro rem, enviado;
  zerar("#nvprog2\n1\ttt7654321\ttt7654321\tmovie\t0\t0\t2295\t2700\t1757000000000\t0\n");
  assert(prog_tocou("tt7654321") == -1);
  agora = 1757000300000LL;
  assert(prog_gravar_local("tt7654321", 0, 0, 2824, 2843, 0));
  prog_invalidar();
  assert(prog_tocou("tt7654321") == 0);
  assert(prog_por_chave("tt7654321", &enviado));
  prog_confirmar_empurrados(&enviado, 1);

  memset(&rem, 0, sizeof rem);
  snprintf(rem.contentId, sizeof rem.contentId, "tt7654321");
  rem.posSeg = 2826; rem.durSeg = 2843; rem.lastWatchedMs = agora + 1000;
  assert(prog_aplicar_remoto(&rem) == 1);
  assert(prog_tocou("tt7654321") == 0);
  rem.posSeg = 2700; rem.lastWatchedMs = agora + 2000;
  assert(prog_aplicar_remoto(&rem) == 1);
  assert(prog_tocou("tt7654321") == -1);

  memset(&rem, 0, sizeof rem);
  snprintf(rem.contentId, sizeof rem.contentId, "tt4444445");
  rem.posSeg = 10; rem.durSeg = 5000; rem.lastWatchedMs = agora;
  assert(prog_aplicar_remoto(&rem) == 1);
  assert(prog_tocou("tt4444445") == -1);
  assert(prog_tocou("tt0000404") == -1);
  puts("ok  bit tocou: legado e remoto -1, eco do sync mantem o local");
}

static void perfisNaoSeMisturam(void) {
  ProgRegistro r[8], rem;
  zerar(NULL);
  perfil = 1; agora = 1757000300000LL;
  assert(prog_gravar_local("tt1111111", 0, 0, 100, 6000, -1));
  perfil = 2; prog_invalidar();
  assert(prog_ler(r, 8) == 0);
  assert(!porChave("tt1111111"));
  memset(&rem, 0, sizeof rem);
  snprintf(rem.contentId, sizeof rem.contentId, "tt1111111");
  rem.posSeg = 5000; rem.durSeg = 6000; rem.lastWatchedMs = agora + 10;
  assert(prog_aplicar_remoto(&rem) == 1);        // perfil 2 recebe o seu
  perfil = 1; prog_invalidar();
  assert(porChave("tt1111111")->posSeg == 100);   // perfil 1 intacto
  assert(prog_pendentes(r, 8) == 1);
  perfil = 2; prog_invalidar();
  assert(prog_pendentes(r, 8) == 0);
  perfil = 1;
  puts("ok  perfil 2 nao escreve por cima do perfil 1");
}

static void ordemMaisNovoPrimeiro(void) {
  ProgRegistro r[8];
  zerar(NULL);
  agora = 1757000400000LL; assert(prog_gravar_local("tt0000002", 0, 0, 10, 600, -1));
  agora += 5000;           assert(prog_gravar_local("tt0000003", 0, 0, 10, 600, -1));
  agora += 5000;           assert(prog_gravar_local("tt0000001", 0, 0, 10, 600, -1));
  assert(prog_ler(r, 8) == 3);
  assert(!strcmp(r[0].chave, "tt0000001") && !strcmp(r[1].chave, "tt0000003") && !strcmp(r[2].chave, "tt0000002"));
  prog_remover("tt0000003");
  assert(prog_ler(r, 8) == 2 && !porChave("tt0000003"));
  prog_esquecer_tudo();
  assert(arquivo == NULL && prog_ler(r, 8) == 0);
  puts("ok  ordem, remover, esquecer");
}

static void sobreviveAoDisco(void) {
  ProgRegistro r[8];
  zerar(NULL);
  agora = 1757000500000LL;
  assert(prog_gravar_local("tt1234567", 4, 9, 1432, 2640, -1));
  prog_invalidar();                                  // forca reler o texto gravado
  assert(prog_ler(r, 8) == 1);
  assert(!strcmp(r[0].chave, "tt1234567_s4e9") && !strcmp(r[0].contentId, "tt1234567"));
  assert(!strcmp(r[0].tipo, "series") && r[0].temporada == 4 && r[0].episodio == 9);
  assert(r[0].posSeg == 1432 && r[0].durSeg == 2640 && r[0].lastWatchedMs == agora && r[0].pendente == 1);
  puts("ok  gravado e relido sem perda");
}

static void temposInvalidosNaoEntram(void) {
  ProgRegistro rem, r[8];
  zerar(NULL);
  assert(!prog_gravar_local("tt1", 0, 0, NAN, 600, -1));
  assert(!prog_gravar_local("tt1", 0, 0, 10, NAN, -1));
  assert(!prog_gravar_local("tt1", 0, 0, INFINITY, 600, -1));
  assert(!prog_gravar_local("tt1", 0, 0, 10, DBL_MAX, -1));
  assert(!prog_gravar_local("tt1", 0, 0, 10, (double)LLONG_MAX / 1000.0, -1));
  assert(prog_ler(r, 8) == 0 && !arquivo);
  memset(&rem, 0, sizeof rem);
  snprintf(rem.contentId, sizeof rem.contentId, "tt1");
  rem.posSeg = 10; rem.durSeg = INFINITY;
  assert(!prog_aplicar_remoto(&rem));
  rem.durSeg = 600; memset(rem.chave, 'x', sizeof rem.chave);
  assert(!prog_aplicar_remoto(&rem));
  zerar("tt1 nan 600\ntt2 10 inf\ntt3 10 1e308\ntt4 -5 600\n");
  assert(prog_ler(r, 8) == 1 && !strcmp(r[0].contentId, "tt4") && r[0].posSeg == 0);
  zerar("#nvprog2\n1\ttt1\ttt1\tmovie\t0\t0\tnan\t600\t1\t1\n"
        "1\ttt2\ttt2\tmovie\t0\t0\t10\tinf\t1\t1\n"
        "1\ttt3\ttt3\tmovie\t0\t0\t10\t0\t1\t1\n");
  assert(prog_ler(r, 8) == 0);
  assert(prog_ler(NULL, 8) == 0 && prog_pendentes(NULL, 8) == 0);
  assert(prog_ler(r, -1) == 0 && prog_pendentes(r, -1) == 0);
  puts("ok  tempos invalidos do player, servidor e disco sao recusados");
}

static void limiteEscolheOsMaisNovos(void) {
  ProgRegistro r[2];
  zerar(NULL);
  agora = 1000; assert(prog_gravar_local("tt1", 0, 0, 10, 600, -1));
  agora = 2000; assert(prog_gravar_local("tt2", 0, 0, 10, 600, -1));
  agora = 3000; assert(prog_gravar_local("tt3", 0, 0, 10, 600, -1));
  assert(prog_ler(r, 2) == 2);
  assert(!strcmp(r[0].contentId, "tt3") && !strcmp(r[1].contentId, "tt2"));
  agora = 4000; assert(prog_gravar_local("tt1", 0, 0, 10, 600, -1));
  assert(prog_ler(r, 1) == 1 && !strcmp(r[0].contentId, "tt1"));
  prog_invalidar();
  assert(prog_ler(r, 2) == 2);
  assert(!strcmp(r[0].contentId, "tt1") && !strcmp(r[1].contentId, "tt3"));
  puts("ok  leitura limitada inclui os mais novos antes de truncar");
}

int main(void) {
  prog_definir_relogio(relogio);
  chaveIgualAoWeb();
  migraFormatoAntigo();
  gravarLocalEhPendenteComHora();
  pendenteVenceServidor();
  bitTocouSobreviveAoSync();
  perfisNaoSeMisturam();
  ordemMaisNovoPrimeiro();
  sobreviveAoDisco();
  temposInvalidosNaoEntram();
  limiteEscolheOsMaisNovos();
  puts("progresso: tudo ok");
  return 0;
}
