// Watch Together: relogio, decisao de correcao e mensagens (puros).
//   bash tests/juntossync.sh
#include "juntosproto.h"
#include "juntossync.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define OK(c, ...) do { if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define PERTO(a, b, tol) (fabs((a) - (b)) <= (tol))

static void relogio(void) {
  JsRelogio r;
  double verdade = 5000.0;   // servidor = local + 5000
  int i;
  jsr_zerar(&r);
  // RTT assimetrico (ida 10, volta 90) e uma amostra ruim (RTT 900): fica a de menor RTT.
  {
    double idas[8] = { 400, 10, 30, 60, 20, 15, 80, 25 }, voltas[8] = { 500, 90, 30, 60, 20, 15, 80, 25 };
    for (i = 0; i < 8; i++) {
      double c = 1000.0 * i, s = c + verdade + idas[i];
      jsr_amostra(&r, c, s, c + idas[i] + voltas[i]);
    }
  }
  OK(PERTO(r.offset, verdade, 15.0 + 0.5), "offset %.1f (erro <= rtt/2 da melhor)", r.offset);
  OK(PERTO(r.melhorRtt, 30.0, 0.01), "melhor rtt %.1f", r.melhorRtt);
  // Anel cheio: um desvio pequeno anda por media (30%)...
  jsr_amostra(&r, 9000, 9000 + verdade + 100 + 5, 9010);   // offset medido +100 com rtt 10
  OK(r.offset > verdade && r.offset < verdade + 100, "EMA: %.1f", r.offset);
  // ...e um salto de mais de 250 ms (TV acordou) entra direto.
  for (i = 0; i < 8; i++) jsr_amostra(&r, 20000 + i, 20000 + i + verdade + 2000 + 2, 20000 + i + 4);
  OK(PERTO(r.offset, verdade + 2000, 1.0), "salto direto: %.1f", r.offset);
  // Ecos absurdos sao ignorados.
  jsr_amostra(&r, 50000, 0, 40000);
  OK(PERTO(r.offset, verdade + 2000, 1.0), "rtt negativo ignorado");
  OK(PERTO(jsr_local(&r, jsr_servidor(&r, 123.0)), 123.0, 1e-9), "local <-> servidor");
}

static void alvo(void) {
  JsRel rel = { JSE_TOCANDO, 100.0, 50000.0, 3 };
  OK(PERTO(jsr_pos_alvo(&rel, 60000.0), 110.0, 1e-9), "tocando extrapola");
  OK(PERTO(jsr_pos_alvo(&rel, 49000.0), 100.0, 1e-9), "play agendado no futuro: alvo e o pos0");
  rel.estado = JSE_PAUSADO;
  OK(PERTO(jsr_pos_alvo(&rel, 90000.0), 100.0, 1e-9), "pausado nao anda");
  // Quem entra 1 h depois do t0 cai certo (o bug #8579 do Jellyfin).
  rel.estado = JSE_TOCANDO;
  OK(PERTO(jsr_pos_alvo(&rel, 50000.0 + 3600000.0), 3700.0, 1e-6), "entrar no meio");
  OK(jsr_estado_de("esperando") == JSE_ESPERANDO && jsr_estado_de("x") == -1, "nomes");
  OK(PERTO(jss_pos_local(10.0, 1000.0, 1200.0, 1, 1.0), 10.2, 1e-9), "interpola 200 ms");
  OK(PERTO(jss_pos_local(10.0, 1000.0, 1200.0, 0, 1.0), 10.0, 1e-9), "pausado nao interpola");
  OK(PERTO(jss_pos_local(10.0, 1000.0, 9000.0, 1, 1.0), 12.0, 1e-9), "nao inventa mais de 2 s");
}

static void faixas(void) {
  JsSync s;
  JsAcao a;
  double t = 100000;
  // Nivel A
  jss_zerar(&s, 'A');
  a = jss_decidir(&s, t, 10.05, 10.0, 1);
  OK(a.tipo == JSA_NADA, "zona morta A");
  a = jss_decidir(&s, t, 10.5, 10.0, 1);
  OK(a.tipo == JSA_VELOCIDADE && a.cent == 95, "adiantado 0,5 s: 0,95x (%d)", a.cent);
  OK(PERTO(s.velAte, t + 10000.0, 1.0), "0,5 s a 5%% = 10 s");
  a = jss_decidir(&s, t + 500, 10.3, 10.0, 1);
  OK(a.tipo == JSA_NADA, "empurrao em curso");
  a = jss_decidir(&s, t + 1000, 9.85, 10.0, 1);
  OK(a.tipo == JSA_VELOCIDADE && a.cent == 100, "passou do ponto: volta a 1x");
  a = jss_decidir(&s, t + 1500, 10.0, 11.0, 1);
  OK(a.tipo == JSA_VELOCIDADE && a.cent == 105, "atrasado: 1,05x");
  a = jss_decidir(&s, t + 2000, 10.0, 12.6, 1);
  OK(a.tipo == JSA_SEEK && a.pos > 12.6, "desvio cresceu acima de 1,5 s no empurrao: seek");
  OK(a.aviso, "pulo de 2,6 s mostra \"Sincronizando\"");
  // Nivel B
  jss_zerar(&s, 'B');
  OK(jss_decidir(&s, t, 10.5, 10.0, 1).tipo == JSA_NADA, "B tolera 0,5 s");
  a = jss_decidir(&s, t, 9.2, 10.0, 1);
  OK(a.tipo == JSA_SEEK && PERTO(a.pos, 10.0 + 0.25, 1e-9), "B atrasado: seek com a latencia padrao (%.3f)", a.pos);
  OK(jss_decidir(&s, t + 1000, 8.0, 10.0, 1).tipo == JSA_NADA, "cooldown de 3 s");
  a = jss_decidir(&s, t + 3100, 10.8, 10.0, 1);
  OK(a.tipo == JSA_SEGURAR && PERTO(a.ms, 800, 1e-6) && !a.aviso, "B adiantado: segura 0,8 s em vez de seek");
  OK(jss_decidir(&s, t + 3600, 10.8, 10.0, 1).tipo == JSA_NADA, "segurando: nada ate soltar + 1,5 s");
  a = jss_decidir(&s, t + 9000, 16.0, 10.0, 1);
  OK(a.tipo == JSA_SEGURAR && a.aviso, "segurar grande avisa");
  a = jss_decidir(&s, t + 30000, 25.0, 10.0, 1);
  OK(a.tipo == JSA_SEEK && a.aviso, "adiantado demais (15 s): seek, com aviso");
  // Invalido (buffering) nao decide e encerra empurrao.
  jss_zerar(&s, 'A');
  jss_decidir(&s, t, 10.3, 10.0, 1);
  a = jss_decidir(&s, t + 100, 10.3, 10.0, 0);
  OK(a.tipo == JSA_VELOCIDADE && a.cent == 100 && !s.velAte, "invalido encerra o empurrao");
}

static void protecoes(void) {
  JsSync s;
  JsAcao a;
  double t = 0;
  int i, seeks = 0, frouxo = 0;
  // Fonte que sempre cai 2 s antes (keyframe): no maximo 3 seeks por minuto,
  // depois sync frouxo (tolerancia 2 s), e nada de loop.
  jss_zerar(&s, 'B');
  for (i = 0; i < 120; i++, t += 500) {
    a = jss_decidir(&s, t, 98.0, 100.0, 1);
    if (a.tipo == JSA_SEEK) seeks++;
    frouxo += a.frouxo;
  }
  OK(seeks == 3, "seeks em 60 s: %d", seeks);
  OK(frouxo == 1 && s.frouxo, "frouxo uma vez");
  OK(jss_decidir(&s, t + 70000, 98.5, 100.0, 1).tipo == JSA_NADA, "frouxo tolera 1,5 s de atraso");
  OK(jss_decidir(&s, t + 70000, 101.5, 100.0, 1).tipo == JSA_SEGURAR, "adiantado segura mesmo frouxo (nao entra em loop)");
  OK(jss_decidir(&s, t + 200000, 98.5, 100.0, 1).tipo == JSA_SEEK && !s.frouxo, "frouxo acaba em 2 min");
  // Latencia de seek medida entra no proximo pedido; o residuo de keyframe
  // libera UM segundo seek curto no nivel B, sem esperar o cooldown.
  jss_zerar(&s, 'B');
  a = jss_decidir(&s, 0, 90.0, 100.0, 1);
  OK(a.tipo == JSA_SEEK, "seek 1");
  jss_seek_concluiu(&s, 800, -1.6);
  OK(PERTO(s.latSeekMs, 800, 1e-9), "latencia medida");
  a = jss_decidir(&s, 900, 98.4, 100.0, 1);
  // margem do keyframe aprendida: 1,6 + 0,25 -> cai a frente e segura
  OK(a.tipo == JSA_SEEK && PERTO(a.pos, 100.0 + 0.8 + 1.85, 1e-9), "segundo seek curto, alem do keyframe (%.2f)", a.pos);
  jss_seek_concluiu(&s, 800, -1.6);
  OK(jss_decidir(&s, 1500, 98.4, 100.0, 1).tipo == JSA_NADA, "um so");
  // Velocidade provada: dois empurroes que andaram a 1x derrubam para B.
  jss_zerar(&s, 'A');
  jss_decidir(&s, 0, 10.3, 10.0, 1);
  OK(jss_vel_medida(&s, 3000, 13.3) == 0, "primeira falha nao decide");
  s.velAte = 0;
  jss_decidir(&s, 4000, 15.3, 15.0, 1);
  OK(jss_vel_medida(&s, 7000, 18.3) == 1, "segunda falha: nivel B");
  jss_zerar(&s, 'A');
  jss_decidir(&s, 0, 10.3, 10.0, 1);
  OK(jss_vel_medida(&s, 4000, 10.3 + 4 * 0.95) == 0 && s.velFalhas == 0, "0,95x de verdade passa");
}

static void agendado(void) {
  JsSync s;
  JsRelogio r;
  jss_zerar(&s, 'A');
  jsr_zerar(&r);
  r.offset = 2000;
  r.tem = 1;
  jss_play_concluiu(&s, 300);
  OK(PERTO(jss_instante_play(&s, &r, 12000), 12000 - 2000 - 300, 1e-9), "play pedido antes do t0 pela latencia");
}

static void mensagens(void) {
  const char *j = "{\"t\":\"sala\",\"seq\":18,\"s\":1759750000123,\"rel\":{\"ep\":\"tt1160419:1:2\",\"estado\":\"esperando\","
    "\"pos0\":812.4,\"t0\":1759750001323,\"taxa\":1,\"autor\":\"nuvio:r\",\"op\":\"esperar\",\"esperando\":[\"nuvio:f\",\"nuvio:\\\"x\"],"
    "\"prazo\":1759750021323},\"membros\":[{\"id\":\"nuvio:r\",\"nome\":\"Rafa\",\"av\":\"\",\"st\":\"pronto\",\"anf\":true,\"on\":true,"
    "\"nivel\":\"A\",\"plat\":\"lg\"},{\"id\":\"nuvio:f\",\"nome\":\"Fabi \\u00e9\",\"av\":\"\",\"st\":\"carregando\",\"anf\":false,\"on\":true,"
    "\"pct\":42,\"alc\":true,\"cel\":true}],\"controle\":\"anfitriao\",\"max\":5,\"cfg\":{\"ep\":\"tt1160419\",\"titulo\":\"Duna\","
    "\"poster\":\"https://image.tmdb.org/p.jpg\",\"imp\":{\"hash\":\"abababababababababababababababababababab\",\"idx\":2,"
    "\"bytes\":2100000000,\"arquivo\":\"Dune.2024.1080p.mkv\",\"durMs\":9930000},\"codigo\":\"K7M2QX\",\"espera\":20}}";
  JsMsg m;
  JsSala s;
  char b[512];
  JsImpressao i;
  OK(jsp_ler(j, &m, &s) == JSM_SALA, "tipo sala");
  OK(s.seq == 18 && s.rel.seq == 18 && s.rel.estado == JSE_ESPERANDO, "seq/estado");
  OK(PERTO(s.rel.pos0, 812.4, 1e-9) && PERTO(s.rel.t0, 1759750001323.0, 0.5), "pos0/t0");
  OK(!strcmp(s.ep, "tt1160419:1:2") && !strcmp(s.autor, "nuvio:r") && !strcmp(s.op, "esperar"), "ep/autor/op");
  OK(s.nEsperando == 2 && !strcmp(s.esperando[0], "nuvio:f") && !strcmp(s.esperando[1], "nuvio:\"x"), "esperando %d", s.nEsperando);
  OK(s.n == 2 && s.m[0].anf && !s.m[1].anf && s.m[1].alc && s.m[1].cel && s.m[1].pct == 42, "membros");
  OK(!strcmp(s.m[1].nome, "Fabi \xc3\xa9") && s.m[0].nivel == 'A', "nome utf-8 / nivel");
  OK(s.controleAnf && s.max == 5 && !strcmp(s.codigo, "K7M2QX") && s.espera == 20, "cfg");
  OK(!strcmp(s.imp.hash, "abababababababababababababababababababab") && s.imp.idx == 2 && s.imp.bytes == 2100000000LL
     && s.imp.durMs == 9930000 && !strcmp(s.imp.arquivo, "Dune.2024.1080p.mkv"), "impressao");
  OK(jsp_ler("{\"t\":\"pong\",\"c\":123,\"s\":1759750000123}", &m, NULL) == JSM_PONG && m.c == 123 && m.s == 1759750000123.0, "pong");
  OK(jsp_ler("{\"t\":\"aviso\",\"k\":\"esperando\",\"quem\":\"nuvio:f\",\"prazo\":20}", &m, NULL) == JSM_AVISO
     && !strcmp(m.k, "esperando") && m.prazo == 20, "aviso");
  OK(jsp_ler("{\"t\":\"pedido\",\"quem\":{\"id\":\"nuvio:z\",\"nome\":\"Zeca\",\"av\":\"\"}}", &m, NULL) == JSM_PEDIDO
     && !strcmp(m.quem, "nuvio:z") && !strcmp(m.nome, "Zeca"), "pedido");
  OK(jsp_ler("{\"t\":\"fim\",\"motivo\":\"anfitriao\",\"dur\":8040}", &m, NULL) == JSM_FIM && m.dur == 8040, "fim");
  OK(jsp_ler("{\"t\":\"membro\",\"id\":\"nuvio:f\",\"st\":\"pronto\",\"on\":true}", &m, NULL) == JSM_MEMBRO
     && !strcmp(m.membro.st, "pronto"), "membro");
  OK(jsp_ler("nada", &m, NULL) == JSM_NADA && jsp_ler("{\"t\":\"zzz\"}", &m, NULL) == JSM_NADA, "lixo");
  // Montagem
  jsp_cmd(b, sizeof b, "pausa", 812.4, 17, NULL, NULL);
  OK(!strcmp(b, "{\"t\":\"cmd\",\"op\":\"pausa\",\"pos\":812.400,\"seqBase\":17}"), "cmd: %s", b);
  jsp_cmd(b, sizeof b, "proximo", -1, 3, "tt1:1:\"2", NULL);
  OK(!strcmp(b, "{\"t\":\"cmd\",\"op\":\"proximo\",\"seqBase\":3,\"ep\":\"tt1:1:\\\"2\"}"), "proximo: %s", b);
  jsp_cmd(b, sizeof b, "espera", -1, 3, NULL, "60");
  OK(strstr(b, "\"valor\":60}") != NULL, "espera numerica: %s", b);
  jsp_estado(b, sizeof b, "carregando", 40, 1.5, 100, "parecida", 42, 900);
  OK(strstr(b, "\"fonte\":{\"tipo\":\"parecida\"}") && strstr(b, "\"pct\":40") && strstr(b, "\"prep\":900"), "estado: %s", b);
  memset(&i, 0, sizeof i);
  i.idx = -1;
  snprintf(i.arquivo, sizeof i.arquivo, "Duna \"2\".mkv");
  i.durMs = 5000;
  jsp_impressao(b, sizeof b, &i);
  OK(!strcmp(b, "{\"durMs\":5000,\"arquivo\":\"Duna \\\"2\\\".mkv\",\"binge\":\"\",\"rotulo\":\"\"}"), "imp: %s", b);
  {
    char c[600];
    snprintf(c, sizeof c, "{\"imp\":%s}", b);
    jsp_ler_impressao(c, NULL, &i);
    OK(!strcmp(i.arquivo, "Duna \"2\".mkv") && i.durMs == 5000 && i.idx == -1, "ida e volta");
  }
  OK(jsp_ping(b, 4, 1234567) == -1, "nao coube = -1");
}

int main(void) {
  relogio();
  alvo();
  faixas();
  protecoes();
  agendado();
  mensagens();
  if (falhas) { printf("juntossync: %d falha(s)\n", falhas); return 1; }
  printf("juntossync: relogio, alvo, faixas A/B, protecoes, play agendado e mensagens ok\n");
  return 0;
}
