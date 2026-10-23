// FILEIRAS ILIMITADAS NO ANDROID (2.0.4). Pedido do dono: "deixar ilimitado
// para android as fileiras". Compilado DUAS vezes por fileiras_android.sh: com
// -DNV_ANDROID (sem teto de 40, sem fila, rodadas fatiadas) e sem (TVs: tudo
// como era — teto 40, fila, folha de memoria acima de 16).
#include "fileiras.h"
#include "catalogo.h"
#include "descoberta.h"
#include <assert.h>
unsigned addons_versao(void) { return 0; }
#include <stdio.h>
#include <string.h>

// Dubles de dados.c: lista em memoria, nada no disco.
int dados_gravar(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 1; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) { (void)dst; (void)tam; (void)nome; return NULL; }

static int indice(const char *chave) {
  int i;
  for (i = 0; i < fil_n(); i++) if (!strcmp(fil_chave(i), chave)) return i;
  return -1;
}

#define N_CAT 150

int main(void) {
  int i, naFila = 0, naHome = 0;
  char chave[64];

  // --- estrutura ------------------------------------------------------------
#ifdef NV_ANDROID
  assert(FIL_LIMITE_MAX >= 150);
  assert(CAT_FIL_MAX >= FIL_LIMITE_MAX + 16);       // folga das fixas
  assert(CAT_MAX >= CAT_FIL_MAX * DESC_ITENS_POR_FILEIRA);
  puts("ok  android: teto de fileiras 200 (Ilimitado), vetor 224, itens 8000");
#else
  assert(FIL_LIMITE_MAX == 40 && CAT_FIL_MAX == 40 && CAT_MAX == 2000);
  puts("ok  TVs: teto 40, vetor 40, itens 2000 (inalterado)");
#endif

  // --- seletor de Ajustes ----------------------------------------------------
  assert(fil_limite_passo(7, 1) == 8);
  assert(fil_limite_passo(3, -1) == 3);
  assert(fil_limite_passo(39, 1) == 40);
#ifdef NV_ANDROID
  assert(fil_limite_passo(40, 1) == FIL_LIMITE_MAX);     // 40 -> Ilimitado
  assert(fil_limite_passo(FIL_LIMITE_MAX, 1) == FIL_LIMITE_MAX);
  assert(fil_limite_passo(FIL_LIMITE_MAX, -1) == 40);    // Ilimitado -> 40
  assert(fil_limite_ilimitado(FIL_LIMITE_MAX) && !fil_limite_ilimitado(40));
  assert(!fil_limite_pede_aviso(7, FIL_LIMITE_MAX));     // sem folha de memoria
  puts("ok  android: o passo depois de 40 e Ilimitado, sem folha de aviso");
#else
  assert(fil_limite_passo(40, 1) == 40);
  assert(!fil_limite_ilimitado(40));
  assert(fil_limite_pede_aviso(7, 17) && !fil_limite_pede_aviso(7, 16));
  puts("ok  TVs: seletor para em 40 e a folha aparece acima de 16");
#endif

  // --- 150 catalogos, limite no maximo -----------------------------------------
  fil_registrar("continue_watching", "Continuar assistindo", "", "", 5);
  for (i = 0; i < N_CAT; i++) {
    snprintf(chave, sizeof chave, "addon.x_movie_cat%03d", i);
    fil_registrar(chave, chave, "Addon", "movie", 24);
  }
  fil_definir_limite(FIL_LIMITE_MAX);
  assert(fil_limite() == FIL_LIMITE_MAX);
  // Tira tudo e poe de volta um a um, como a pessoa faz no editor.
  for (i = 0; i < N_CAT; i++) {
    snprintf(chave, sizeof chave, "addon.x_movie_cat%03d", i);
    fil_remover(indice(chave));
  }
  for (i = 0; i < N_CAT; i++) {
    int estado = -1;
    snprintf(chave, sizeof chave, "addon.x_movie_cat%03d", i);
    fil_adicionar(indice(chave), &estado);
    if (estado == FIL_NA_FILA) naFila++;
    else if (estado == FIL_NA_HOME) naHome++;
  }
#ifdef NV_ANDROID
  assert(naFila == 0 && naHome == N_CAT);
  assert(fil_n_fila() == 0);
  assert(fil_n_capacidade() == N_CAT);
  puts("ok  android: 150 fileiras entram na home, nenhuma na fila");
#else
  assert(naHome == 40 && naFila == N_CAT - 40);
  assert(fil_n_fila() == N_CAT - 40);
  assert(fil_n_capacidade() == 40);
  puts("ok  TVs: 40 na home, 110 na fila (inalterado)");
#endif

  // --- modo seguro: o teto da sessao vale nos dois -----------------------------
  fil_definir_teto_sessao(FIL_LIMITE_VIGIADO);
  assert(fil_limite() == FIL_LIMITE_VIGIADO);          // home monta 12
  assert(fil_limite_gravado() == FIL_LIMITE_MAX);      // escolha da pessoa intacta
  fil_definir_teto_sessao(0);
  assert(fil_limite() == FIL_LIMITE_MAX);
  puts("ok  modo seguro: duas quedas => 12 fileiras nesta sessao, escolha gravada intacta");

  // --- ritmo dos pedidos -------------------------------------------------------
  assert(desc_catalogos_em_voo() == 3);
  { int teto = 100, feitas = 0, rodadas = 0, maior = 0;
    while (feitas < teto) {
      int r = desc_catalogos_por_rodada(teto - feitas);
      assert(r > 0);
      if (r > maior) maior = r;
      feitas += r; rodadas++;
    }
#ifdef NV_ANDROID
    assert(maior == 24 && rodadas == 5);
    printf("ok  android: 100 fileiras = %d rodadas de ate %d, %d GET em voo\n",
           rodadas, maior, desc_catalogos_em_voo());
#else
    (void)rodadas; (void)maior;
    assert(desc_catalogos_por_rodada(40) == 40);   // TV: uma rodada so, como antes
    puts("ok  TVs: 40 fileiras numa rodada so, 3 GET em voo (inalterado)");
#endif
  }
  return 0;
}
