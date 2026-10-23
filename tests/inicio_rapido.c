// #202, tempo de inicio do stream: latencia por add-on, prazo da abertura e a
// explicacao da ilha. Puro (sem SDL, sem rede).
#include "addonstats.h"
#include "inicio.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); falhas++; } } while (0)

static void stats(void) {
  char buf[4096];
  int i;
  addonstats_zerar();
  CHECK(!addonstats_ignoravel("PenguPlay", 5000));
  CHECK(addonstats_segunda_s("PenguPlay", 30) == 30);   // sem historico: o padrao

  // Mudo: uma vez = tentativa curta; duas = sai da espera; tres = sem segunda.
  addonstats_registrar("WebStreamr", 30000, 0);
  CHECK(addonstats_mudo_seguidas("webstreamr") == 1);   // sem diferenciar caixa
  CHECK(!addonstats_ignoravel("WebStreamr", 5000));
  CHECK(addonstats_segunda_s("WebStreamr", 30) == ADDONSTATS_SEGUNDA_MUDO_S);
  addonstats_registrar("WebStreamr", 30000, 0);
  CHECK(addonstats_ignoravel("WebStreamr", 5000));
  CHECK(addonstats_segunda_s("WebStreamr", 30) == ADDONSTATS_SEGUNDA_MUDO_S);
  addonstats_registrar("WebStreamr", 30000, 0);
  CHECK(addonstats_segunda_s("WebStreamr", 30) == 0);
  // Respondeu: zera a sequencia e volta a ser esperado.
  addonstats_registrar("WebStreamr", 900, 1);
  CHECK(addonstats_mudo_seguidas("WebStreamr") == 0);
  CHECK(addonstats_segunda_s("WebStreamr", 30) == 30);
  // Ainda "lento" (3 de 4 passaram da espera) ate voltar a responder rapido.
  CHECK(addonstats_ignoravel("WebStreamr", 5000));
  addonstats_registrar("WebStreamr", 800, 1); addonstats_registrar("WebStreamr", 700, 1);
  addonstats_registrar("WebStreamr", 600, 1);
  CHECK(!addonstats_ignoravel("WebStreamr", 5000));
  // "Todos os add-ons" / Instantaneo (limite 0): ninguem e ignorado.
  addonstats_registrar("Mudo", 30000, 0); addonstats_registrar("Mudo", 30000, 0);
  CHECK(addonstats_ignoravel("Mudo", 5000));
  CHECK(!addonstats_ignoravel("Mudo", 0));

  // Lento: passa da espera em 2 de cada 3 amostras (minimo 3).
  addonstats_registrar("Penguplay", 6000, 1);
  addonstats_registrar("Penguplay", 7000, 1);
  CHECK(!addonstats_lento("Penguplay", 5000));          // so 2 amostras
  addonstats_registrar("Penguplay", 2000, 1);
  CHECK(addonstats_lento("Penguplay", 5000));           // 2 de 3
  CHECK(addonstats_ignoravel("Penguplay", 5000));
  CHECK(!addonstats_ignoravel("Penguplay", 8000));      // com espera maior ele chega a tempo
  CHECK(addonstats_mediana("Penguplay") == 6000);
  // Rapido nunca sai.
  for (i = 0; i < 5; i++) addonstats_registrar("Torrentio", 500 + (unsigned)i * 100, 1);
  CHECK(!addonstats_ignoravel("Torrentio", 3000));
  // Janela: so as ultimas ADDONSTATS_JANELA contam.
  for (i = 0; i < ADDONSTATS_JANELA; i++) addonstats_registrar("Rolante", 20000, 1);
  CHECK(addonstats_lento("Rolante", 5000));
  for (i = 0; i < ADDONSTATS_JANELA; i++) addonstats_registrar("Rolante", 300, 1);
  CHECK(!addonstats_lento("Rolante", 5000));

  // Arquivo: serializa e relê igual.
  addonstats_serializar(buf, sizeof buf);
  addonstats_zerar();
  CHECK(!addonstats_ignoravel("Mudo", 5000));
  addonstats_carregar(buf);
  CHECK(addonstats_ignoravel("Mudo", 5000));
  CHECK(addonstats_lento("Penguplay", 5000));
  CHECK(addonstats_mudo_seguidas("Mudo") == 2);
  addonstats_carregar("lixo sem tab\n\tsem nome\nX\tabc\nY\t10:1 20:x 30:0\n");
  CHECK(addonstats_mudo_seguidas("Y") >= 0);   // arquivo estragado nao quebra

  // 128 add-ons + 160 scrapers cabem sem expulsar o primeiro nome.
  addonstats_zerar();
  for (i = 0; i < 288; i++) {
    char nome[48];
    snprintf(nome, sizeof nome, "Origem %d", i);
    addonstats_registrar(nome, 30000, 0);
  }
  CHECK(addonstats_mudo_seguidas("Origem 0") == 1);
  CHECK(addonstats_mudo_seguidas("Origem 287") == 1);
}

static void abertura(void) {
  InicioAbertura g;
  memset(&g, 0, sizeof g);
  // Sem proxima ou com dado chegando: o prazo longo de sempre.
  g.desdeMs = 9000; g.proximaSemPerda = 0;
  CHECK(!inicio_abre_vencida(&g));
  CHECK(inicio_abre_prazo_ms(&g) == INICIO_ABRE_COM_DADO_MS);
  g.proximaSemPerda = 1; g.dadoChegando = 1;
  CHECK(!inicio_abre_vencida(&g));
  CHECK(inicio_abre_prazo_ms(&g) == INICIO_ABRE_COM_DADO_MS);
  g.desdeMs = 30001;
  CHECK(inicio_abre_vencida(&g));
  // Sem sinal e com proxima que nao e pior: prazo curto.
  g.dadoChegando = 0; g.desdeMs = INICIO_ABRE_SEM_SINAL_MS - 1;
  CHECK(!inicio_abre_vencida(&g));
  g.desdeMs = INICIO_ABRE_SEM_SINAL_MS + 1;
  CHECK(inicio_abre_vencida(&g));
  CHECK(inicio_abre_prazo_ms(&g) == INICIO_ABRE_SEM_SINAL_MS);
  // Proxima pior (1080p depois de um 4K): nao vence por tempo antes dos 30 s.
  g.proximaSemPerda = 0; g.desdeMs = 20000;
  CHECK(!inicio_abre_vencida(&g));
  // Quem ja tocou nunca vence.
  g.tocou = 1; g.proximaSemPerda = 1; g.desdeMs = 99999;
  CHECK(!inicio_abre_vencida(&g));
  // Quadro ja desenhado (DV na TCL, sem "pronto" ainda) nunca vence (12/10).
  g.tocou = 0; g.quadro = 1;
  CHECK(!inicio_abre_vencida(&g));
  // 4K/DV sem quadro: 60 s, nao 30 (Silo 2160p DV levou 24 s ao quadro).
  g.quadro = 0; g.pesado = 1; g.dadoChegando = 1; g.desdeMs = 45000;
  CHECK(inicio_abre_prazo_ms(&g) == INICIO_ABRE_PESADO_MS);
  CHECK(!inicio_abre_vencida(&g));
  g.desdeMs = INICIO_ABRE_PESADO_MS + 1;
  CHECK(inicio_abre_vencida(&g));
}

static void motivo(void) {
  InicioSinais s;
  memset(&s, 0, sizeof s);
  s.tentativa = 1; s.fase = INI_FASE_BUSCA; s.pendentes = 1;
  s.desdeMs = 3999;
  CHECK(inicio_motivo(&s) == INI_NADA);                 // antes de 4 s: calado
  s.desdeMs = 4000;
  CHECK(inicio_motivo(&s) == INI_ESPERA_UM);
  s.pendenteMudo = 1;
  CHECK(inicio_motivo(&s) == INI_ESPERA_MUDO);
  s.fontesChegaram = 1;
  CHECK(inicio_motivo(&s) == INI_ESPERA_VARIOS);
  s.fontesChegaram = 0;
  CHECK(inicio_motivo(&s) == INI_ESPERA_MUDO);
  s.pendentes = 3;
  CHECK(inicio_motivo(&s) == INI_ESPERA_VARIOS);
  s.pendentes = 0; s.semResposta = 1;
  CHECK(inicio_motivo(&s) == INI_NAO_RESPONDEU);
  s.fontesChegaram = 1;
  CHECK(inicio_motivo(&s) == INI_NADA);
  s.fontesChegaram = 0; s.semResposta = 2;
  CHECK(inicio_motivo(&s) == INI_NADA);
  s.semResposta = 0;
  CHECK(inicio_motivo(&s) == INI_NADA);
  s.fase = INI_FASE_VERIFICA;
  CHECK(inicio_motivo(&s) == INI_VERIFICANDO);
  s.fase = INI_FASE_ABRE; s.viaDebrid = 1;
  CHECK(inicio_motivo(&s) == INI_ABRINDO_DEBRID);
  s.viaDebrid = 0;
  CHECK(inicio_motivo(&s) == INI_NADA);
  // A troca de fonte se explica na hora, sem esperar os 4 s.
  s.tentativa = 2; s.desdeMs = 100;
  CHECK(inicio_motivo(&s) == INI_FALLBACK);
  CHECK(inicio_motivo(NULL) == INI_NADA);
}

int main(void) {
  stats(); abertura(); motivo();
  if (falhas) { printf("inicio_rapido: %d falha(s)\n", falhas); return 1; }
  printf("inicio_rapido: ok\n");
  return 0;
}
