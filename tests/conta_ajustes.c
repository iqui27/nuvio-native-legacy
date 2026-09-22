// O blob da conta chega nos ajustes com o SENTIDO certo?
//
// Este e o teste que importa mais nesta area, porque o defeito aqui e MUDO:
// um mapeamento invertido nao da erro, nao trava, nao aparece no log — a
// pessoa so acha que a TV "veio com outras opcoes". Cada caso abaixo foi
// conferido contra o valor literal que o app web grava.
//
// Dois casos sao propositalmente traicoeiros:
//   - `true` do web vira INDICE 0 ("Ligado"), nao 1;
//   - `collapseSidebar: true` vira "Recolhida", que tambem e o indice 0.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ajustes.h"

static int falhas;
static void confere(const char *o_que, int obtido, int esperado) {
  int ok = obtido == esperado;
  printf("  %-46s %s (obtido %d, esperado %d)\n", o_que, ok ? "ok    " : "FALHOU", obtido, esperado);
  if (!ok) falhas++;
}

int main(void) {
  // Blob com TODO valor no oposto do padrao de fabrica, para que qualquer
  // opcao que nao seja aplicada apareca como falha em vez de coincidir.
  // FORMATO REAL, medido contra a conta na TV: version + features, chave em
  // snake_case, valor embrulhado em {"type","value"}. O teste antigo usava um
  // mapa plano de camelCase — passava, e o app nao aplicava NADA no aparelho.
  // Teste que nao usa o formato do servidor nao prova nada.
  static const char *BLOB =
    "{\"version\":1,\"features\":{\"layout_settings\":{"
    "\"modern_landscape_posters_enabled\":{\"type\":\"boolean\",\"value\":true},"
    "\"modern_hero_full_screen_backdrop_enabled\":{\"type\":\"boolean\",\"value\":false},"
    "\"collapse_sidebar\":{\"type\":\"boolean\",\"value\":true},"
    "\"modern_sidebar\":{\"type\":\"boolean\",\"value\":false},"
    "\"hero_section_enabled\":{\"type\":\"boolean\",\"value\":false},"
    "\"discover_location\":{\"type\":\"string\",\"value\":\"IN_SIDEBAR\"},"
    "\"poster_labels_enabled\":{\"type\":\"boolean\",\"value\":false},"
    "\"hide_unreleased_content\":{\"type\":\"boolean\",\"value\":true},"
    "\"home_imdb_ratings_visibility\":{\"type\":\"string\",\"value\":\"HIDE_ALL\"},"
    "\"continue_watching_enabled\":{\"type\":\"boolean\",\"value\":false},"
    "\"continue_watching_card_style\":{\"type\":\"string\",\"value\":\"POSTER\"},"
    "\"continue_watching_sort_mode\":{\"type\":\"string\",\"value\":\"streaming_style\"},"
    "\"blur_unwatched_episodes\":{\"type\":\"boolean\",\"value\":true},"
    "\"detail_page_trailer_button_enabled\":{\"type\":\"boolean\",\"value\":false},"
    "\"focused_poster_backdrop_expand_delay_seconds\":{\"type\":\"int\",\"value\":4},"
    "\"card_depth_enabled\":{\"type\":\"boolean\",\"value\":false},"
    "\"card_depth_edge_strength\":{\"type\":\"int\",\"value\":80},"
    "\"poster_card_width_dp\":{\"type\":\"int\",\"value\":150},"
    "\"poster_card_corner_radius_dp\":{\"type\":\"int\",\"value\":12},"
    "\"chave_que_este_app_nao_conhece\":{\"type\":\"string\",\"value\":\"x\"}"
    "}}}";

  printf("aplicando blob da conta...\n");
  ajustes_aplicar_blob(BLOB);

  printf("\nbooleanos (true do web = LIGADO):\n");
  confere("modernLandscapePostersEnabled:true -> ligado", ajustes_posteres_deitados(), 1);
  confere("modernHeroFullScreenBackdropEnabled:false",    ajustes_hero_cheio(), 0);
  confere("modernSidebar:false -> desligado",             ajustes_rail_moderna(), 0);
  confere("heroSectionEnabled:false -> desligado",        ajustes_hero_ligado(), 0);
  confere("posterLabelsEnabled:false",                    ajustes_rotulos_poster(), 0);
  confere("hideUnreleasedContent:true",                   ajustes_ocultar_nao_lancados(), 1);
  confere("continueWatchingEnabled:false",                ajustes_cw_ligado(), 0);
  confere("blurUnwatchedEpisodes:true",                   ajustes_desfocar_nao_assistidos(), 1);
  confere("detailPageTrailerButtonEnabled:false",         ajustes_botao_trailer(), 0);
  confere("cardDepthEnabled:false",                       ajustes_profundidade(), 0);

  printf("\ncollapseSidebar:true = \"Recolhida\" (indice 0):\n");
  confere("rail recolhida",                               ajustes_rail_recolhida(), 1);

  printf("\nenums de texto:\n");
  confere("discoverLocation \"in_sidebar\" -> 1",         ajustes_local_descobrir(), 1);
  confere("homeImdbRatingsVisibility \"HIDE_ALL\"",       ajustes_notas_home(), 0);
  confere("continueWatchingCardStyle \"poster\" -> 2",    ajustes_cw_estilo(), 2);
  confere("continueWatchingSortMode \"streaming_style\"", ajustes_cw_ordem(), 1);

  printf("\nnumeros:\n");
  confere("focusedPosterBackdropExpandDelaySeconds 4",
          (int)(ajustes_expandir_poster_atraso() + 0.5f), 4);
  confere("cardDepthEdgeStrength 80 -> 0.80",
          (int)(ajustes_profundidade_borda() * 100.0f + 0.5f), 80);
  confere("posterCardWidthDp 150",                        ajustes_largura_poster_dp(), 150);
  confere("posterCardCornerRadiusDp 12",                  ajustes_raio_poster_dp(), 12);

  // Segunda rodada: valor de texto que este app NAO conhece (versao nova do
  // web, opcao nova). A opcao tem de FICAR COMO ESTA. Cair num padrao aqui
  // inventaria uma preferencia que a pessoa nunca marcou — e ela veria a TV
  // mudar sozinha sem ter mexido em nada.
  printf("\nvalor desconhecido nao inventa preferencia:\n");
  { int antesDescobrir = ajustes_local_descobrir();
    int antesEstilo    = ajustes_cw_estilo();
    ajustes_aplicar_blob(
      "{\"features\":{\"layout_settings\":{"
      "\"discover_location\":{\"type\":\"string\",\"value\":\"modo_que_nao_existe\"},"
      "\"continue_watching_card_style\":{\"type\":\"string\",\"value\":\"3d\"}}}}");
    confere("discoverLocation invalido -> mantido", ajustes_local_descobrir(), antesDescobrir);
    confere("cardStyle invalido -> mantido",        ajustes_cw_estilo(), antesEstilo); }

  // E chave ausente tambem nao mexe: um blob de uma opcao so nao pode zerar as
  // outras 39.
  printf("\nchave ausente nao mexe no resto:\n");
  { int antes = ajustes_largura_poster_dp();
    ajustes_aplicar_blob("{\"features\":{\"layout_settings\":{"
                         "\"hero_section_enabled\":{\"type\":\"boolean\",\"value\":true}}}}");
    confere("largura preservada por blob parcial", ajustes_largura_poster_dp(), antes);
    confere("a chave que veio foi aplicada",       ajustes_hero_ligado(), 1); }

  // --- O CAMINHO DE VOLTA (#85) ---------------------------------------------
  //
  // O defeito que estes casos cercam e o pior desta area: um push de ajustes
  // mal formado nao da erro, ele APAGA da conta o que so o app web usa. Cada
  // caso abaixo e uma das travas de ajustes_mesclar_blob.
  printf("\ncostura de subida (#85):\n");
  { static const char *BASE =
      "{\"version\":1,\"features\":{\"layout_settings\":{"
      "\"hero_section_enabled\":{\"type\":\"boolean\",\"value\":false},"
      "\"poster_card_width_dp\":{\"type\":\"int\",\"value\":150},"
      "\"continue_watching_card_style\":{\"type\":\"string\",\"value\":\"POSTER\"},"
      "\"chave_que_este_app_nao_conhece\":{\"type\":\"string\",\"value\":\"x\"}"
      "},\"outra_feature\":{\"coisa_do_web\":{\"type\":\"int\",\"value\":7}}}}";
    char *saida = NULL;
    int n;
    // Estado local diferente da base nas tres chaves conhecidas.
    ajustes_aplicar_blob("{\"features\":{\"layout_settings\":{"
      "\"hero_section_enabled\":{\"type\":\"boolean\",\"value\":true},"
      "\"poster_card_width_dp\":{\"type\":\"int\",\"value\":132},"
      "\"continue_watching_card_style\":{\"type\":\"string\",\"value\":\"WIDE\"}}}}");
    n = ajustes_mesclar_blob(BASE, &saida);
    confere("tres chaves reescritas", n, 3);
    if (saida) {
      confere("booleano local sobe",
              strstr(saida, "\"hero_section_enabled\":{\"type\":\"boolean\",\"value\":true}") != NULL, 1);
      confere("numero local sobe",
              strstr(saida, "\"poster_card_width_dp\":{\"type\":\"int\",\"value\":132}") != NULL, 1);
      // CAIXA DO SERVIDOR: a base guardava "POSTER" em maiuscula, entao o valor
      // novo tambem sai em maiuscula — o web guarda estes enums assim.
      confere("enum sobe na caixa que a conta usava",
              strstr(saida, "\"value\":\"WIDE\"") != NULL, 1);
      // O QUE ESTE APP NAO CONHECE FICA INTACTO. E a trava que impede a TV de
      // apagar da conta as preferencias que so o app web tem.
      confere("chave desconhecida preservada",
              strstr(saida, "\"chave_que_este_app_nao_conhece\":{\"type\":\"string\",\"value\":\"x\"}") != NULL, 1);
      confere("feature inteira desconhecida preservada",
              strstr(saida, "\"coisa_do_web\":{\"type\":\"int\",\"value\":7}") != NULL, 1);
      // E NADA E INVENTADO: chave que a conta nao tem nao aparece no blob.
      confere("chave ausente na conta nao e criada",
              strstr(saida, "card_depth_enabled") == NULL, 1);
      free(saida);
    } else falhas++;

    // Segunda costura, agora com o local IGUAL a base: nada a mandar. Serve de
    // freio no sync — um ciclo sem diferenca real nao gera push.
    saida = NULL;
    n = ajustes_mesclar_blob(
      "{\"features\":{\"layout_settings\":{"
      "\"hero_section_enabled\":{\"type\":\"boolean\",\"value\":true}}}}", &saida);
    confere("sem diferenca -> nada a subir", n, 0);
    confere("e sem saida alocada", saida == NULL, 1);

    // Blob vazio/ausente: sem base nao ha costura (a trava 1 do empurrarAjustes).
    confere("sem base -> 0", ajustes_mesclar_blob(NULL, &saida), 0);
    confere("base vazia -> 0", ajustes_mesclar_blob("", &saida), 0);

    // AJUSTE DE APARELHO NAO SOBE, nem que a conta tenha a chave: a TV da sala
    // e a do quarto nao tem a mesma RAM nem a mesma tela.
    saida = NULL;
    n = ajustes_mesclar_blob(
      "{\"features\":{\"layout_settings\":{"
      "\"resolucao_ui\":{\"type\":\"int\",\"value\":0},"
      "\"texturas_mb\":{\"type\":\"int\",\"value\":0}}}}", &saida);
    confere("ajuste de aparelho fica local", n, 0);
    free(saida); }

  printf("\n%s\n", falhas ? "TEM FALHA" : "TODOS OS AJUSTES CHEGARAM CERTOS");
  return falhas ? 1 : 0;
}
