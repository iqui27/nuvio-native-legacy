// A FAIXA DE AUDIO ESCOLHIDA A MAO VOLTA EM TODO EPISODIO (audiomemoria.h).
//
// O relato: anime com audio duplo, mesmo bingeGroup episodio apos episodio, e
// a pessoa trocava para japones a mao em todo episodio. Roda audioescolha.c e
// audiomemoria.c de verdade, ligados como app.c liga, com o disco falso, e
// prova:
//   1. japones escolhido a mao no E1 -> E2 abre em japones (preferido: ingles);
//   2. depois de "fechar o app" (a tabela relida do disco) -> japones;
//   3. outro titulo no meio -> japones de novo ao voltar;
//   4. a mesma release (bingeGroup): a faixa exata, mesmo com duas iguais;
//   5. outra release: a parecenca (o primeiro japones), nunca o numero;
//   6. a memoria e do PERFIL; o perfil 2 segue o preferido;
//   7. sair da conta apaga tudo.
//
//   bash tests/audiomemoria.sh
#include "audiomemoria.h"
#include "audioescolha.h"
#include "dados.h"
#include "linguas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }

// --- disco falso -----------------------------------------------------------------
#define DISCO_MAX 8
static char *discoNome[DISCO_MAX], *discoDado[DISCO_MAX];
// strdup nao existe em -std=c11.
static char *copia(const char *s) {
  size_t n = strlen(s) + 1;
  char *d = malloc(n);
  if (d) memcpy(d, s, n);
  return d;
}
static int achar(const char *n, int criar) {
  int i, livre = -1;
  for (i = 0; i < DISCO_MAX; i++) {
    if (discoNome[i] && !strcmp(discoNome[i], n)) return i;
    if (!discoNome[i] && livre < 0) livre = i;
  }
  if (!criar || livre < 0) return -1;
  discoNome[livre] = copia(n);
  return livre;
}
int dados_gravar(const char *n, const char *c) {
  int i = achar(n, 1);
  if (i < 0) return 0;
  free(discoDado[i]); discoDado[i] = copia(c);
  return 1;
}
char *dados_ler(const char *n) { int i = achar(n, 0); return i < 0 || !discoDado[i] ? NULL : copia(discoDado[i]); }
int dados_apagar(const char *n) {
  int i = achar(n, 0);
  if (i >= 0) { free(discoDado[i]); discoDado[i] = NULL; }
  return 1;
}

// --- o episodio: ingles e japones, na mesma ordem em toda a temporada ----------
#define A(i, cod, rot, num) { i, cod, rot, 2, 0, 1, num }
static AudioCand ep[] = {
  A("en", "audio/mp4a-latm", "English · AAC 2.0", 0),
  A("ja", "audio/mp4a-latm", "Japanese · AAC 2.0", 1),
  A("ja", "audio/mp4a-latm", "Japanese · AAC 2.0", 2),   // comentario, mesmo rotulo
};
#define NEP 3

// O que o alvo faz ao receber as faixas de um episodio (video_android.c).
static int abrir(const char *titulo, const char *binge, int *origem) {
  audioesc_fonte_binge(binge);
  audioesc_titulo(titulo);
  return audioesc_escolher(ep, NEP, origem);
}
// O que faixas.c faz quando a pessoa escolhe na folha de audio.
static void escolherAMao(int i) {
  audioesc_lembrar_faixa(ep[i].idioma, ep[i].codec, ep[i].rotulo, ep[i].canais, ep[i].numero);
}
// "Fechar o app": a sessao esquece, a tabela e relida do disco no perfil.
static void reiniciarApp(int perfil) {
  audiomem_definir_perfil(perfil == 1 ? 2 : 1);
  audiomem_definir_perfil(perfil);
}

int main(void) {
  const char *SP = "aio|SubsPlease|1080p", *ERAI = "aio|Erai|1080p";
  int o;
  ling_local_audio("en");                         // preferido: ingles (dublado)
  audioesc_definir_memoria(audiomem_ler, audiomem_gravar);   // como app.c
  audiomem_definir_perfil(1);

  ok(abrir("tt9000001", SP, &o) == 0 && o == AUDIOESC_PREF1, "E1 abre no ingles preferido");
  escolherAMao(1);
  ok(dados_ler("audiomemoria-p1.txt") != NULL, "a escolha foi gravada no arquivo do perfil 1");

  // A sessao ja segurava isto antes; o caso e o mesmo titulo de novo (E2).
  ok(abrir("tt9000001", SP, &o) == 1 && o == AUDIOESC_LEMBRADA, "E2 na mesma sessao: japones");

  reiniciarApp(1);
  ok(!audioesc_tem_lembrada(), "app reiniciado: nada na sessao ate abrir o titulo");
  ok(abrir("tt9000001", SP, &o) == 1 && o == AUDIOESC_LEMBRADA,
     "E3 depois de fechar o app: japones (o defeito: voltava ao ingles)");

  ok(abrir("tt7777777", SP, &o) == 0 && o == AUDIOESC_PREF1, "outro titulo no meio: o preferido dele");
  ok(abrir("tt9000001", SP, &o) == 1 && o == AUDIOESC_LEMBRADA, "de volta ao anime: japones");
  ok(abrir("tt9000001:1:4", SP, &o) == 1, "id com temporada:episodio vale a serie");

  // A mesma release: a faixa exata (o comentario, numero 2), que a parecenca
  // sozinha nunca acharia (dois japoneses iguais).
  escolherAMao(2);
  reiniciarApp(1);
  ok(abrir("tt9000001", SP, &o) == 2 && o == AUDIOESC_LEMBRADA,
     "mesmo bingeGroup, depois de reiniciar: a faixa exata (a segunda japonesa)");
  ok(abrir("tt9000001", ERAI, &o) == 1 && o == AUDIOESC_LEMBRADA,
     "outro bingeGroup: o numero nao vale, o primeiro japones");
  ok(abrir("tt9000001", "", &o) == 1, "fonte sem bingeGroup: o primeiro japones");

  // O perfil 2 nunca escolheu: o japones do perfil 1 nao vaza.
  audiomem_definir_perfil(2);
  ok(abrir("tt9000001", SP, &o) == 0 && o == AUDIOESC_PREF1, "perfil 2: ingles, o preferido");
  audiomem_definir_perfil(1);
  ok(abrir("tt9000001", SP, &o) == 2, "perfil 1 de novo: a escolha dele");

  // Texto livre com TAB nao quebra a linha do arquivo.
  ep[0].rotulo = "English\tDub · AAC 2.0";
  audioesc_fonte_binge("aio|x\ty");
  audioesc_titulo("tt5555555");
  escolherAMao(0);
  ep[0].rotulo = "English · AAC 2.0";
  reiniciarApp(1);
  ok(abrir("tt9000001", SP, &o) == 2, "linha com TAB gravada sem estragar as outras");

  audiomem_esquecer();
  ok(!dados_ler("audiomemoria-p1.txt") && !dados_ler("audiomemoria-p2.txt"), "sair da conta apaga os arquivos");
  ok(abrir("tt9000001", SP, &o) == 0 && o == AUDIOESC_PREF1, "e a escolha nao volta");

  printf(falhas ? "audiomemoria: %d FAIL(S)\n" : "audiomemoria: ok\n", falhas);
  return falhas != 0;
}
