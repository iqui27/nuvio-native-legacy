// PADROES DE FABRICA DOS AJUSTES, conferidos contra o comentario de cada um.
//
// `valor[]` em ajustes.c e POSICIONAL. Em 876742e entrou nele um padrao de
// uma opcao que nao existe no enum ("resetar foco ao iniciar") e cada padrao
// de AJ_RAIL a AJ_RESOLUCAO passou a cair na opcao seguinte — so reapareceu
// como defeito visivel em 22/09, quando uma captura rodou sem ajustes.txt e
// os cartoes sairam pilulas (arredondamento 126 dp, o padrao da LARGURA).
// Este teste le o vetor ja compilado, antes de qualquer arquivo, e confere a
// faixa que desalinhou mais as duas opcoes novas da arte do destaque.
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  // Home: layout e arte do destaque.
  assert(valor[AJ_HERO_FUNDO] == 0);          // Automatico
  assert(valor[AJ_HERO_ARTE_DIF] == 1);       // Desligado: mesma foto do card
  assert(valor[AJ_HERO_TRAILER] == 1);        // Desligado de fabrica (30/09)
  // Som e espera do trailer do destaque: mudo, 2,2 s (decimos), deste aparelho.
  assert(valor[AJ_HERO_TRAILER_SOM] == 1);
  assert(valor[AJ_HERO_TRAILER_ESPERA] == 22);
  assert(ajustes_trailer_hero_espera_ms() == 2200);
  assert(!strcmp(CHAVE[AJ_HERO_TRAILER_SOM], "trailerDestaqueSomLocal"));
  assert(!strcmp(CHAVE[AJ_HERO_TRAILER_ESPERA], "trailerDestaqueEsperaLocal"));
  assert(somenteDesteAparelho(AJ_HERO_TRAILER_SOM));
  assert(somenteDesteAparelho(AJ_HERO_TRAILER_ESPERA));
  // Separador decimal do idioma da interface: "2,2 s" ou "2.2 s".
  assert(!strcmp(textoValor(AJ_HERO_TRAILER_ESPERA), "2,2 s") ||
         !strcmp(textoValor(AJ_HERO_TRAILER_ESPERA), "2.2 s"));
  // A faixa que andava uma casa.
  assert(valor[AJ_RAIL] == 0);                // recolhida
  assert(valor[AJ_RAIL_MODERNA] == 1);        // desligada
  assert(valor[AJ_RAIL_BLUR] == 0);
  assert(valor[AJ_ROTULOS] == 1);             // rotulos desligados
  assert(valor[AJ_CW_LIGADO] == 0);
  assert(valor[AJ_DET_VEU] == 100);
  // Fonte do trailer (trailerfonte.h): nasce Automatica, e deste aparelho,
  // e a chave nao colide com nada da conta.
  assert(valor[AJ_TRAILER_FONTE] == 0);
  assert(OPCOES[AJ_TRAILER_FONTE].n == 4);
  assert(!strcmp(CHAVE[AJ_TRAILER_FONTE], "trailerFonteLocal"));
  assert(!strcmp(CHAVE[AJ_TRAILER_ASPECTO], "trailerAspecto"));
  assert(!strcmp(CHAVE[AJ_TRAILER_FONTE + 1], "focusedPosterBackdropExpandEnabled"));
  assert(somenteDesteAparelho(AJ_TRAILER_FONTE));
  assert(valor[AJ_EXPANDIR] == 0);
  assert(valor[AJ_EXPANDIR_ATRASO] == 3);
  assert(valor[AJ_PROF] == 1);                // profundidade desligada
  assert(valor[AJ_PROF_BORDA] == 28);         // cardDepthEdgeStrength
  assert(valor[AJ_PROF_BRILHO] == 10);        // cardDepthSheenStrength
  assert(valor[AJ_LARGURA_DP] == 126);
  assert(valor[AJ_RAIO_DP] == 12);            // canto normal, nao pilula
  assert(valor[AJ_QUALIDADE_IMG] == 1);       // Padrao
  assert(valor[AJ_IDIOMA] == 0);              // Automatico
  assert(ajustes_idioma() == IDIOMA_EN);      // sem conta nem TV: English (release publico)
  assert(valor[AJ_ANIM] == 0);                // animacoes completas
  assert(valor[AJ_RESOLUCAO] == RES_AUTO);    // Automatica (06/10: 1080p, nunca 720p sozinha)
  assert(!ajustes_4k() && !ajustes_720p());
  valor[AJ_RESOLUCAO] = RES_1080; assert(!ajustes_4k() && !ajustes_720p());
  valor[AJ_RESOLUCAO] = RES_4K;   assert(ajustes_4k() && !ajustes_720p());
  valor[AJ_RESOLUCAO] = RES_720;  assert(!ajustes_4k() && ajustes_720p());
  valor[AJ_RESOLUCAO] = RES_AUTO;
  assert(ajustes_relogio_ligado() && ajustes_relogio_pos() == 0);   // relogio: ligado, automatico
  assert(ajustes_selo_visto());   // selo de visto no cartaz: ligado (#212)
  assert(valor[AJ_TEMA] == AJ_TEMA_DINAMICA && valor[AJ_FUNDO] == 2);   // aparencia de fabrica da 2.0: Da arte + Frost
  assert(valor[AJ_COR_LOGO] == 0 && valor[AJ_VIDRO] == 1);               // Cor da logo ligada, vidro desligado
  // O "+" salva no Trakt. Este assert passava POR ACASO de 22/09 ate o #149:
  // o vetor estava sete casas curto (as linhas do Stalker e do Xtream), e o 1
  // que caia aqui era o do envio automatico. As duas pontas da faixa agora.
  assert(valor[AJ_SALVOS_DEST] == 1);
  assert(valor[AJ_STALKER_PORTAL] == 0 && valor[AJ_XTREAM_LIMPAR] == 0);
  assert(valor[AJ_ENVIO_AUTO] == 0);          // envio sozinho: LIGADO (dono)
  assert(valor[AJ_TEX_MB] == 0);              // memoria para imagens: auto
  // #163: itens por fileira nasce em 12 (o de sempre); 18 e 24 sao escolha,
  // com o aviso de memoria para TV de 1 GB. E a opcao vive neste aparelho.
  assert(valor[AJ_ITENS_FILEIRA] == 0 && ajustes_itens_fileira() == 12);
  assert(OPCOES[AJ_ITENS_FILEIRA].n == 3);
  // Fileiras da home: 3 a 40 (o teto do catalogo), 7 de fabrica. Acima de 16 a
  // tela pede confirmacao (riscoPedirConfirmacao); acima de 12 o diario do modo
  // seguro vigia. O limite nunca passa do vetor de fileiras do catalogo.
  assert(OPCOES[AJ_FIL_LIMITE].min == 3 && OPCOES[AJ_FIL_LIMITE].max == 40);
  assert(FIL_LIMITE_MAX == CAT_FIL_MAX);
  assert(FIL_LIMITE_PADRAO == 7 && FIL_LIMITE_SEGURO < FIL_LIMITE_MAX && FIL_LIMITE_VIGIADO < FIL_LIMITE_SEGURO);
  fil_definir_limite(1);   assert(fil_limite_gravado() == FIL_LIMITE_MIN);
  fil_definir_limite(999); assert(fil_limite_gravado() == FIL_LIMITE_MAX);
  fil_definir_teto_sessao(12); assert(fil_limite() == 12 && fil_limite_gravado() == FIL_LIMITE_MAX);
  fil_definir_teto_sessao(0);  assert(fil_limite() == FIL_LIMITE_MAX);
  fil_definir_limite(FIL_LIMITE_PADRAO);
  // Todo ajuste vigiado existe, tem chave unica e mora numa opcao de verdade.
  { int i, j; for (i = 0; i < N_RISCOS; i++) {
      assert(RISCOS[i].op >= 0 && RISCOS[i].op < AJ_N && strlen(RISCOS[i].chave) < SEG_CHAVE);
      for (j = i + 1; j < N_RISCOS; j++) assert(strcmp(RISCOS[i].chave, RISCOS[j].chave));
    } }
  assert(somenteDesteAparelho(AJ_ITENS_FILEIRA));
  // 23/09: tres fontes novas NO FIM de "Background do hero", com o mesmo
  // indice do contrato ARTEHERO_* (o gravado em heroFundoLocal).
  assert(OPCOES[AJ_HERO_FUNDO].n == ARTEHERO_N_ESCOLHAS);
  assert(!strcmp(V_HERO_FONTE[ARTEHERO_TRAKT], "Trakt"));
  assert(!strcmp(V_HERO_FONTE[ARTEHERO_APPLE], "Apple TV"));
  assert(!strcmp(V_HERO_FONTE[ARTEHERO_FANART], "fanart.tv"));
  assert(!strcmp(V_HERO_FONTE[ARTEHERO_ANIME], "Anime (Kitsu / AniList)"));
  assert(!strcmp(CHAVE[AJ_HERO_FUNDO], "heroFundoLocal"));
  // A chave do fanart.tv: linha de acao, fora do ajustes.txt ("-"), e as
  // vizinhas no mesmo lugar (MDBList antes, Diagnostico depois).
  assert(OPCOES[AJ_FANART_CHAVE].tipo == OP_ACAO);
  assert(!strcmp(CHAVE[AJ_FANART_CHAVE], "-fanartChave"));
  assert(!strcmp(CHAVE[AJ_FANART_CHAVE - 1], "mdblist_show_mal"));
  assert(!strcmp(CHAVE[AJ_FANART_CHAVE + 1], "-diagnostico"));
  assert(valor[AJ_FANART_CHAVE] == 0 && valor[AJ_DIAGNOSTICO] == 0);
  assert(valor[AJ_MDB_MAL] == 0);
  assert(valor[AJ_AUTO_CREDITOS] == 1 && !ajustes_auto_creditos());
  assert(!strcmp(CHAVE[AJ_AUTO_CREDITOS], "autoCreditosLocal"));
  assert(somenteDesteAparelho(AJ_AUTO_CREDITOS) && dePerfil(AJ_AUTO_CREDITOS));
  assert(!strcmp(i18n(OPCOES[AJ_AUTO_CREDITOS].rotulo), "Automatically skip credits"));
  puts("ajustes_padroes: ok");
  return 0;
}
