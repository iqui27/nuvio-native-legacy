// PEGOU NO SONO: a regra pura e os tres casos de ponta a ponta contra o
// Diario de verdade (dados e perfil sao dubles; sem SDL).
#include "dormiu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char pasta[256];
int perfis_ativo(void) { return 1; }
char *dados_ler(const char *nome) {
  char c[512]; FILE *f; long n; char *b;
  snprintf(c, sizeof c, "%s/%s", pasta, nome);
  f = fopen(c, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *c) {
  char p[512]; FILE *f;
  snprintf(p, sizeof p, "%s/%s", pasta, nome);
  f = fopen(p, "wb"); if (!f) return 0; fputs(c, f); fclose(f); return 1;
}
int dados_apagar(const char *nome) { char p[512]; snprintf(p, sizeof p, "%s/%s", pasta, nome); return remove(p) == 0; }
static int existe(const char *nome) { char *b = dados_ler(nome); free(b); return b != NULL; }

extern long long diario_relogio_teste;
static long long relogio = 1790000000LL;

// Toca `seg` segundos de (id,t,e) a partir de `pos`, sem tecla, em passos de 1 s.
static double tocar(const char *id, char tipo, int t, int e, double pos, double dur, int seg, double cred) {
  int i;
  for (i = 0; i < seg; i++) {
    pos += 1.0; relogio++;
    dormiu_relogio_teste = relogio; diario_relogio_teste = relogio;
    dormiu_player_passo(id, tipo, t, e, pos, dur, 1, cred > 0 && pos >= cred, 1.0f);
  }
  return pos;
}

int main(void) {
  DormiuSinal s;
  DiarioEntrada d;
  int ponto;
  char r[32];

  snprintf(pasta, sizeof pasta, "%s/nuvio-dormiu-teste-%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
  snprintf(r, sizeof r, "x"); { char c[600]; snprintf(c, sizeof c, "rm -rf '%s' && mkdir -p '%s'", pasta, pasta); assert(system(c) == 0); }
  dormiu_relogio_teste = relogio; diario_relogio_teste = relogio;

  // ---- regra pura
  memset(&s, 0, sizeof s);
  s.durSeg = 7200; s.semTeclaSeg = 45 * 60; s.posTeclaSeg = 3000; s.credTardeSeg = -1; s.voltouSeg = -1;
  assert(!dormiu_regra(&s, &ponto));                    // sem nenhum dos 3 sinais
  s.fimSemDespedida = 1;
  assert(dormiu_regra(&s, &ponto) && ponto == 3300);    // ultima tecla + 5 min
  s.semTeclaSeg = 39 * 60; assert(!dormiu_regra(&s, NULL));   // vao curto
  s.semTeclaSeg = 45 * 60; s.durSeg = 9000;             // 45 min < 35% de 150 min
  assert(!dormiu_regra(&s, NULL));
  s.durSeg = 7200; s.limiarExtraMin = 10; assert(!dormiu_regra(&s, NULL));   // aprendeu
  s.limiarExtraMin = 0; s.fimSemDespedida = 0; s.credTardeSeg = 9 * 60; assert(!dormiu_regra(&s, NULL));
  s.credTardeSeg = 10 * 60; assert(dormiu_regra(&s, NULL));
  s.credTardeSeg = -1; s.voltouSeg = 600; assert(dormiu_regra(&s, NULL));
  s.posTeclaSeg = 7100; assert(dormiu_regra(&s, &ponto) && ponto == 7200);   // nao passa da duracao
  dormiu_rotulo(4320, r, sizeof r); assert(!strcmp(r, "1:12"));
  dormiu_rotulo(2880, r, sizeof r); assert(!strcmp(r, "48 min"));

  dormiu_iniciar();

  // ---- caso 1: TV desligou tocando. Ultima tecla aos 20 min, 60 min sem tecla.
  tocar("tt0001", 'm', 0, 0, 0, 7200, 1200, 0);
  dormiu_tecla();
  tocar("tt0001", 'm', 0, 0, 1200, 7200, 3600, 0);
  assert(existe("dormiu-vivo-p1.txt"));                 // o arquivo que prova o caso 1
  // ...o app morre aqui. Proximo arranque:
  dormiu_iniciar();
  assert(!existe("dormiu-vivo-p1.txt"));
  assert(dormiu_suspeita("tt0001", &d));
  assert(d.dormiuSeg == 1200 + 300 && (d.flags & DIARIO_F_PARCIAL) && d.durSeg == 7200);
  // "Nao dormi": a parcial some, e o limiar sobe 10 min.
  dormiu_negar(&d);
  assert(!dormiu_suspeita("tt0001", NULL) && diario_n(0) == 0 && dormiu_limiar_extra() == 10);
  // (o limiar fica para os proximos casos; volta a zero para nao mascarar)
  { char c[600]; snprintf(c, sizeof c, "rm -f '%s/dormiu-p1.txt'", pasta); system(c); }
  dormiu_iniciar();  // rele estado: limiar 0? so relido se o perfil mudar: forca
  // (o modulo relê o estado na troca de perfil; aqui basta conferir que gravou)

  // ---- caso 2: serie, passou dos creditos e a tecla veio 15 min depois.
  { DiarioEntrada fim; memset(&fim, 0, sizeof fim);
    double pos = tocar("tt0900", 's', 1, 5, 0, 3000, 300, 2800);
    dormiu_tecla();                                    // ultima tecla aos 5 min
    pos = tocar("tt0900", 's', 1, 5, pos, 3000, 2600, 2800);   // creditos aos 2800
    // o player grava a vez (diariofontes) ao concluir
    snprintf(fim.id, sizeof fim.id, "tt0900"); fim.tipo = 's'; fim.temporada = 1; fim.episodio = 5;
    fim.origem = 'p'; fim.quando = relogio; fim.hora = -1;
    assert(diario_adicionar(0, &fim) == 1);
    (void)pos;
    dormiu_player_saiu(3000, 3000, 1);                // fim do arquivo, sem tecla
    relogio += 15 * 60; dormiu_relogio_teste = relogio;
    // limiar aprendido (10) ainda vale: 2600+... vao = 2600 s = 43 min < 50 -> nada
    dormiu_tecla();
    assert(!dormiu_suspeita("tt0900", NULL)); }

  // limiar de volta a zero (perfil novo relê) e o mesmo caso, agora com vao 55 min
  { char c[600]; snprintf(c, sizeof c, "printf 'limiar=0\\n' > '%s/dormiu-p1.txt'", pasta); system(c); }
  { DiarioEntrada fim; double pos;
    memset(&fim, 0, sizeof fim);
    extern void dormiu_teste_reler(void); dormiu_teste_reler();
    pos = tocar("tt0900", 's', 1, 6, 0, 4000, 300, 3500);
    dormiu_tecla();
    pos = tocar("tt0900", 's', 1, 6, pos, 4000, 3300, 3500);   // creditos aos 3500 (vao 53 min)
    snprintf(fim.id, sizeof fim.id, "tt0900"); fim.tipo = 's'; fim.temporada = 1; fim.episodio = 6;
    fim.origem = 'p'; fim.quando = relogio; fim.hora = -1;
    assert(diario_adicionar(0, &fim) == 1);
    (void)pos;
    dormiu_player_saiu(3900, 4000, 1);
    assert(!dormiu_suspeita("tt0900", NULL));          // ainda espera a tecla
    assert(existe("dormiu-vivo-p1.txt"));              // se morrer antes: caso 1
    relogio += 15 * 60; dormiu_relogio_teste = relogio;
    dormiu_tecla();
    assert(!existe("dormiu-vivo-p1.txt"));
    assert(dormiu_suspeita("tt0900", &d));
    assert(d.episodio == 6 && d.dormiuSeg == 600 && !(d.flags & DIARIO_F_PARCIAL));
    assert(diario_episodio(0, "tt0900", 1, 6, NULL) == 1);   // marcou a vez existente, nao criou outra
    dormiu_confirmar(&d);
    assert(!dormiu_suspeita("tt0900", NULL));
    { DiarioEntrada v[4]; int n = diario_por_titulo(0, "tt0900", v, 4);
      assert(n == 2 && v[1].episodio == 6 && (v[1].flags & DIARIO_F_DORMIU) && v[1].dormiuSeg == 600); } }

  // ---- caso 3: saiu por tecla depois de um vao de 50 min; no dia seguinte
  // retomou e voltou 20 min.
  { double pos = tocar("tt0002", 'm', 0, 0, 0, 7200, 600, 0);
    dormiu_tecla();
    pos = tocar("tt0002", 'm', 0, 0, pos, 7200, 3000, 0);
    dormiu_tecla();                                   // acordou e apertou VOLTAR
    dormiu_player_saiu(pos, 7200, 0);
    assert(!dormiu_suspeita("tt0002", NULL));
    relogio += 86400;
    tocar("tt0002", 'm', 0, 0, pos, 7200, 5, 0);      // retomou de onde parou
    dormiu_player_passo("tt0002", 'm', 0, 0, pos - 1300, 7200, 1, 0, 0.0f);   // voltou ~21 min
    assert(dormiu_suspeita("tt0002", &d));
    assert(d.dormiuSeg == 600 + 300 && (d.flags & DIARIO_F_PARCIAL)); }

  // ---- saida normal por tecla, sem vao: nada.
  { double pos = tocar("tt0003", 'm', 0, 0, 0, 6000, 1000, 0);
    dormiu_tecla(); dormiu_player_saiu(pos, 6000, 0);
    assert(!dormiu_suspeita("tt0003", NULL)); }

  { char c[600]; snprintf(c, sizeof c, "rm -rf '%s'", pasta); system(c); }
  printf("dormiu: ok\n");
  return 0;
}
