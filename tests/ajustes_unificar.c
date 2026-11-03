// Settings: one block per name in each category (no "Cores e vidro" twice),
// while every advanced option stays advanced, in the same block, reachable by
// search and shortcuts and hidden until "Mais opcoes" opens.
#include "../src/ajustes.c"
#include <assert.h>

// The advanced set and its block before the merge (2.0.3 catalog).
static const struct { int op; const char *bloco; } AVANCADAS[] = {
  { AJ_FONTE_REGEX, "Escolha da fonte" },
  { AJ_FONTE_REGEX_PADRAO, "Escolha da fonte" },
  { AJ_FONTE_REGEX_MODELO, "Escolha da fonte" },
  { AJ_FONTE_ESCOPO, "Escolha da fonte" },
  { AJ_FONTE_ADDONS_PERM, "Escolha da fonte" },
  { AJ_FONTE_PLUGINS_PERM, "Escolha da fonte" },
  { AJ_FONTE_ORDEM, "Escolha da fonte" },
  { AJ_FONTE_ORDEM_USO, "Escolha da fonte" },
  { AJ_FONTE_OUTROS, "Escolha da fonte" },
  { AJ_FONTE_AUTO, "Escolha da fonte" },
  { AJ_FONTE_TOCAR_CONFERINDO, "Escolha da fonte" },
  { AJ_FONTE_AQUECER, "Escolha da fonte" },
  { AJ_FONTE_CONFERIR_VARIAS, "Escolha da fonte" },
  { AJ_FONTE_PREPARAR, "Escolha da fonte" },
  { AJ_FONTE_PRECARREGAR, "Escolha da fonte" },
  { AJ_FONTE_OCULTAR_DV, "Escolha da fonte" },
  { AJ_FONTE_OCULTAR_AUDIO, "Escolha da fonte" },
  { AJ_FONTE_OCULTAR_VIDEO, "Escolha da fonte" },
  { AJ_TAM_MIN, "Escolha da fonte" },
  { AJ_FONTE_TEXTO, "Escolha da fonte" },
  { AJ_FONTE_ORDEM_ADDON, "Escolha da fonte" },
  { AJ_SELOS_CORES, "Escolha da fonte" },
  { AJ_SELOS_PACOTE, "Escolha da fonte" },
  { AJ_SELOS_PACOTE_ADD, "Escolha da fonte" },
  { AJ_SELOS_PACOTE_REM, "Escolha da fonte" },
  { AJ_DTS_AC3, "Som e vídeo desta TV" },
  { AJ_HDR10P_STRIP, "Som e vídeo desta TV" },
  { AJ_AFR, "Som e vídeo desta TV" },
  { AJ_TUNNELING, "Som e vídeo desta TV" },
  { AJ_DV_MKV, "Som e vídeo desta TV" },
  { AJ_DV7, "Som e vídeo desta TV" },
  { AJ_CACHE_SEEK, "Som e vídeo desta TV" },
  { AJ_MANTER_VIDEO, "Som e vídeo desta TV" },
  { AJ_RETOMADA_MORNA, "Som e vídeo desta TV" },
  { AJ_MANTER_PRAZO, "Som e vídeo desta TV" },
  { AJ_SEEKR_AJUSTE, "Som e vídeo desta TV" },
  { AJ_BUFFER, "Som e vídeo desta TV" },
  { AJ_PLAYER_EXTERNO, "Som e vídeo desta TV" },
  { AJ_PLAYER_ANIME, "Som e vídeo desta TV" },
  { AJ_P2P_LIGADO, "P2P" },
  { AJ_P2P_URL, "P2P" },
  { AJ_P2P_TESTAR, "P2P" },
  { AJ_P2P_LIMITE, "P2P" },
  { AJ_P2P_DESTINO, "P2P" },
  { AJ_TMDB_IDIOMA, "Idiomas" },
  { AJ_LIVETV_FORMATO, "Se o canal não abre" },
  { AJ_LIVETV_MODO, "Se o canal não abre" },
  { AJ_LIVETV_PROXY, "Se o canal não abre" },
  { AJ_HERO_TRANSICAO, "Estilo" },
  { AJ_ROTULOS, "Fileiras" },
  { AJ_NOME_ADDON, "Fileiras" },
  { AJ_SUFIXO_TIPO, "Fileiras" },
  { AJ_CW_FURTHEST, "Continuar assistindo" },
  { AJ_RAIL_BLUR, "Barra lateral" },
  { AJ_MIC_BUSCA, "Barra lateral" },
  { AJ_VIDRO_OPAC, "Cores e vidro" },
  { AJ_VIDRO_FOSCO, "Cores e vidro" },
  { AJ_VIDRO_QUALIDADE, "Cores e vidro" },
  { AJ_MDB_LIGADO, "Notas" },
  { AJ_MDB_IMDB, "Notas" },
  { AJ_MDB_TRAKT, "Notas" },
  { AJ_MDB_TMDB, "Notas" },
  { AJ_MDB_TOMATES, "Notas" },
  { AJ_MDB_AUDIENCIA, "Notas" },
  { AJ_MDB_META, "Notas" },
  { AJ_MDB_LETTER, "Notas" },
  { AJ_MDB_MAL, "Notas" },
  { AJ_TMDB_ARTE, "Dados do título" },
  { AJ_TMDB_BASICO, "Dados do título" },
  { AJ_TMDB_FICHA, "Dados do título" },
  { AJ_TMDB_DATAS, "Dados do título" },
  { AJ_TMDB_ELENCO, "Dados do título" },
  { AJ_TMDB_PROD, "Dados do título" },
  { AJ_TMDB_REDES, "Dados do título" },
  { AJ_TMDB_EPS, "Dados do título" },
  { AJ_TMDB_TRAILERS, "Dados do título" },
  { AJ_TMDB_MAIS, "Dados do título" },
  { AJ_TMDB_COL, "Dados do título" },
  { AJ_TMDB_CW, "Dados do título" },
  { AJ_DET_META_EXT, "Dados do título" },
  { AJ_DET_SO_CINEMETA, "Dados do título" },
  { AJ_GRAD_CLASSICO, "Cartaz em foco" },
  { AJ_MURAL_TITULOS, "Cartaz em foco" },
  { AJ_PROF_BRILHO, "Forma" },
  { AJ_ADDON_POSTER, "De onde vem a arte" },
  { AJ_ADDON_FUNDO, "De onde vem a arte" },
  { AJ_ADDON_LOGO, "De onde vem a arte" },
  { AJ_COL_ARTE_CONTA, "De onde vem a arte" },
  { AJ_POSTER_INST, "De onde vem a arte" },
  { AJ_POSTER_TOKEN, "De onde vem a arte" },
  { AJ_POSTER_EXTRA, "De onde vem a arte" },
  { AJ_POSTER_CHAVE, "De onde vem a arte" },
  { AJ_POSTER_MODELO, "De onde vem a arte" },
  { AJ_POSTER_TESTAR, "De onde vem a arte" },
  { AJ_HERO_TRAILER_ESPERA, "Trailers" },
  { AJ_TRAILER_QUAL, "Trailers" },
  { AJ_TRAILER_ASPECTO, "Trailers" },
  { AJ_TEX_MB, "Memória" }
};
#define N_AV (int)(sizeof AVANCADAS / sizeof *AVANCADAS)

static int naTela(int op) {
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == op) return i;
  return -1;
}

int main(void) {
  int i, j, s, n = 0, total = 0;
  montarTela();
  // 1. No category repeats a block name.
  for (s = 0; s < nSecoes; s++)
    for (i = secIni[s] + 1; i < secFim(s); i++) if (TELA[i].tipo == IT_ROT)
      for (j = i + 1; j < secFim(s); j++)
        if (TELA[j].tipo == IT_ROT && !strcmp(TELA[i].titulo, TELA[j].titulo)) {
          fprintf(stderr, "duplicate block: %s / %s\n", TELA[secIni[s]].titulo, TELA[i].titulo);
          n++;
        }
  assert(n == 0);
  // 2. Same advanced set, same block (search path), and nothing else advanced.
  for (i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && uxAvancada(TELA[i].op)) total++;
  for (n = 0, i = 0; i < N_AV; i++) {
    int op = AVANCADAS[i].op;
    if (naTela(op) < 0) continue;   // not in this build
    n++;
    assert(uxAvancada(op));
    assert(!strcmp(uxBloco(op), AVANCADAS[i].bloco));
  }
  assert(n == total);
  // 3. "Mais opcoes" counts every advanced option of the category.
  for (s = 0; s < nSecoes; s++) {
    int k = 0;
    for (i = secIni[s] + 1; i < secFim(s); i++) if (TELA[i].tipo == IT_OPC && uxAvancada(TELA[i].op)) k++;
    assert(maisContar(s, NULL, 0) == k);
    assert(!k || maisItem[s] >= 0);
  }
  // 4. Hidden while closed; search/shortcut opens "Mais opcoes" and focuses it.
  for (i = 0; i < N_AV; i++) {
    int op = AVANCADAS[i].op, it = naTela(op);
    if (it < 0 || !ajOpcaoDisponivel(op) || !ajIptvLinhaVisivel(op)) continue;
    memset(maisAberto, 0, sizeof maisAberto); valor[AJ_AVANCADAS] = 1;
    assert(!visivel(it));
    focarOpcao(op);
    if (!visivel(it)) continue;   // other conditions (e.g. the art key) still hide it
    assert(maisAberto[secDoItem[it]] && focoOp == op && focoItem == it);
  }
  puts("ajustes_unificar: OK");
  return 0;
}
