// PADROES DE FABRICA DOS AJUSTES, conferidos contra o comentario de cada um.
//
// `valor[]` em ajustes.c e POSICIONAL. Em 876742e entrou nele um padrao de
// uma opcao que nao existe no enum ("resetar foco ao iniciar") e cada padrao
// de AJ_RAIL a AJ_RESOLUCAO passou a cair na opcao seguinte — so reapareceu
// como defeito visivel em 22/09, quando uma captura rodou sem ajustes.txt e
// os cartoes sairam pilulas (arredondamento 126 dp, o padrao da LARGURA).
// Este teste le o vetor ja compilado, antes de qualquer arquivo, e confere a
// faixa que desalinhou mais as duas opcoes novas da arte do destaque.
#include <SDL2/SDL.h>
static int reportedDesktop(int display, SDL_DisplayMode *mode) {
  (void)display; mode->w = 3840; mode->h = 2160; return 0;
}
#define SDL_GetDesktopDisplayMode reportedDesktop
#include "../src/ajustes.c"
#undef SDL_GetDesktopDisplayMode
#include <assert.h>
#include "nvtmp.h"
#include <unistd.h>

int main(void) {
  for (int i = 0; i < AJ_N; i++)
    for (int j = i + 1; CHAVE[i][0] != '-' && j < AJ_N; j++)
      assert(strcmp(CHAVE[i], CHAVE[j]));
  // Fresh settings and Restore default must select Automatic on every platform.
  const int automatic[] = { AJ_AMPLIACAO_ALGO, AJ_AMPLIAR_UI, AJ_RESOLUCAO,
    AJ_GPU_EFEITOS, AJ_ANIM, AJ_VIDRO, AJ_VIDRO_FOSCO, AJ_VIDRO_QUALIDADE };
  for (unsigned i = 0; i < sizeof automatic / sizeof *automatic; i++) {
    int op = automatic[i];
    const char *label = OPCOES[op].valores[valor[op]];
    assert(!strcmp(label, "Automático") || !strcmp(label, "Automática"));
    assert(valor[op] == valorPadrao[op]);
    assert(uxValorPadrao(op) == valorPadrao[op]);
    assert(somenteDesteAparelho(op) && !dePerfil(op));
  }
  assert(valor[AJ_HDR_INTERFACE] == 0 && !ajustes_hdr_interface());
  assert(somenteDesteAparelho(AJ_HDR_INTERFACE) && !ajustePortavel(AJ_HDR_INTERFACE));
  assert(!strcmp(CHAVE[AJ_HDR_INTERFACE], "hdrInterfaceLocal"));
#ifndef NV_ANDROID
  assert(!ajOpcaoDisponivel(AJ_HDR_INTERFACE));
#endif
  valor[AJ_HDR_INTERFACE] = 1; assert(!ajustes_hdr_interface()); // experiment removed: saved "on" stays off
  valor[AJ_HDR_INTERFACE] = 0;
  // Merge regression: both appended settings keep their parallel table entries.
  assert(valor[AJ_XTREAM_CONEXOES] == 0 && valor[AJ_AMPLIAR_UI] == 2);
  assert(!strcmp(CHAVE[AJ_XTREAM_CONEXOES], "-xtreamConexoes"));
  assert(!strcmp(CHAVE[AJ_AMPLIAR_UI], "ampliarUiLocal"));
  // Upscaling: Automatic, sharpness 2; both device-local.
  assert(valor[AJ_AMPLIACAO_ALGO] == 0 && valor[AJ_AMPLIACAO_NITIDEZ] == 2);
  assert(!strcmp(CHAVE[AJ_AMPLIACAO_ALGO], "ampliarAlgoLocal") &&
         !strcmp(CHAVE[AJ_AMPLIACAO_NITIDEZ], "ampliarNitidezLocal"));
  assert(OPCOES[AJ_XTREAM_CONEXOES].n == 3 && OPCOES[AJ_AMPLIAR_UI].n == 3);
  // c2e07fe41: 0 = follow the primary, then 50%..300% every 10%; the #439
  // indices saved by 2.0.5 are migrated on load (viuLeg2Escala).
  assert(OPCOES[AJ_LEG2_TAMANHO].n == 27);
  for (int i = 0; i < 27; i++) {
    valor[AJ_LEG2_TAMANHO] = i;
    assert(ajustes_leg2_tamanho() == (i == 0 ? 0 : TXT_LEG_PCT_MIN + (i - 1) * 10));
  }
  valor[AJ_LEG2_TAMANHO] = 0;
  // TCL: opção ligada de fábrica, local, antes da sincronia por áudio.
  int syncAuto = -1, naTela = -1, audioTela = -1;
  for (int i = 0; i < AJ_N; i++) if (!strcmp(CHAVE[i], "legendaSyncAutoLocal")) syncAuto = i;
  assert(syncAuto >= 0);
  assert(valor[syncAuto] == 0 && somenteDesteAparelho(syncAuto));
  montarTela();
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC) {
    if (TELA[i].op == syncAuto) naTela = i;
    if (TELA[i].op == AJ_LEG_SYNC_AUDIO) audioTela = i;
  }
  assert(naTela >= 0 && (audioTela < 0 || audioTela == naTela + 1));
  assert(!strcmp(OPCOES[syncAuto].rotulo, "Sincronia automática da legenda"));
  assert(valor[AJ_LEG_MOSTRAR] == 0 && somenteDesteAparelho(AJ_LEG_MOSTRAR));
  assert(!strcmp(CHAVE[AJ_LEG_MOSTRAR], "legendaMostrarLocal"));
  assert(!ajustes_legendas_todas());
  valor[AJ_LEG_MOSTRAR] = 1; assert(ajustes_legendas_todas()); valor[AJ_LEG_MOSTRAR] = 0;
  assert(ajCenaSecao(AJ_LEG_MOSTRAR) == AJC_IDI_FAIXAS);
  // #205: Brilho da interface em HDR: Automatico de fabrica, local, chave propria.
  assert(valor[AJ_HDR_UI] == 0 && ajustes_hdr_ui() && somenteDesteAparelho(AJ_HDR_UI));
  assert(!strcmp(CHAVE[AJ_HDR_UI], "hdrUiLocal"));
  valor[AJ_HDR_UI] = 1; assert(!ajustes_hdr_ui()); valor[AJ_HDR_UI] = 0;
  // #205: a tela ao abrir o video nasce Cinema, local, em Reproducao > Durante.
  { int ab = -1;
    for (int i = 0; i < AJ_N; i++) if (!strcmp(CHAVE[i], "telaAberturaLocal")) ab = i;
    assert(ab >= 0 && valor[ab] == 0 && somenteDesteAparelho(ab) && OPCOES[ab].n == 2);
    assert(!ajustes_abertura_detalhada());
    valor[ab] = 1; assert(ajustes_abertura_detalhada()); valor[ab] = 0;
    assert(ajCenaSecao(ab) == AJC_REP_DURANTE); }
  assert(!ajustes_hero_sinopse_grande());
  assert(!strcmp(CHAVE[AJ_HERO_SINOPSE_TAM], "heroSynopsisSizeLocal"));
  assert(somenteDesteAparelho(AJ_HERO_SINOPSE_TAM));
  assert(!strcmp(OPCOES[AJ_HERO_SINOPSE_TAM].rotulo, "Tamanho da sinopse"));
  assert(!strcmp(V_HERO_SINOPSE[0], "Normal"));
  assert(!strcmp(V_HERO_SINOPSE[1], "Grande"));
  valor[AJ_HERO_SINOPSE_TAM] = 1;
  assert(ajustes_hero_sinopse_grande());
  valor[AJ_HERO_SINOPSE_TAM] = 0;
  assert(!strcmp(CHAVE[AJ_HERO_TAMANHO], "heroSizeLocal") && somenteDesteAparelho(AJ_HERO_TAMANHO));
  assert(OPCOES[AJ_HERO_TAMANHO].n == 3 && ajustes_hero_tamanho() == 0);
  assert(ajustes_hero_escala_logo() == 1.0f && ajustes_hero_escala_info() == 1.0f);
  valor[AJ_HERO_TAMANHO] = 1;
  assert(ajustes_hero_escala_logo() == 1.25f && ajustes_hero_escala_info() == 1.15f && ajustes_hero_sinopse_grande());
  valor[AJ_HERO_TAMANHO] = 2;
  assert(ajustes_hero_escala_logo() == 1.5f && ajustes_hero_escala_info() == 1.3f);
  valor[AJ_HERO_TAMANHO] = 9;
  assert(ajustes_hero_tamanho() == 0 && !ajustes_hero_sinopse_grande());
  valor[AJ_HERO_TAMANHO] = 0;
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
  assert(valor[AJ_PROF] == 0);                // profundidade ligada (2.0.5: reflexo do foco)
  assert(valor[AJ_PROF_BORDA] == 28);         // cardDepthEdgeStrength
  assert(valor[AJ_PROF_BRILHO] == 100);       // cardDepthSheenStrength = intensidade
  assert(valor[AJ_LARGURA_DP] == 126);
  assert(valor[AJ_RAIO_DP] == 12);            // canto normal, nao pilula
  assert(valor[AJ_QUALIDADE_IMG] == 1);       // Padrao
  assert(valor[AJ_IDIOMA] == 0);              // Automatico
  assert(ajustes_idioma() == IDIOMA_EN);      // sem conta nem TV: English (release publico)
  assert(valor[AJ_ANIM] == 2);                // Automatic
  assert(valor[AJ_RESOLUCAO] == RES_AUTO);    // Automatica (06/10: 1080p, nunca 720p sozinha)
  assert(!ajustes_4k() && !ajustes_720p());
  valor[AJ_RESOLUCAO] = RES_1080; assert(!ajustes_4k() && !ajustes_720p());
  valor[AJ_RESOLUCAO] = RES_4K;   assert(ajustes_4k() && !ajustes_720p());
  valor[AJ_RESOLUCAO] = RES_720;  assert(!ajustes_4k() && ajustes_720p());
  valor[AJ_RESOLUCAO] = RES_AUTO;
  assert(ajustes_relogio_ligado() && ajustes_relogio_pos() == 0);   // relogio: ligado, automatico
  assert(ajustes_selo_visto() && ajustes_selo_visto_modo() == 1);   // selo de visto no cartaz: ligado, Icone (#212)
  valor[AJ_SELO_VISTO] = 1; assert(!ajustes_selo_visto() && ajustes_selo_visto_modo() == 0);   // gravado antigo "Desligado" continua
  valor[AJ_SELO_VISTO] = 2; assert(ajustes_selo_visto() && ajustes_selo_visto_modo() == 2);    // Escrito
  valor[AJ_SELO_VISTO] = 0;
  assert(valor[AJ_TEMA] == AJ_TEMA_DINAMICA && valor[AJ_FUNDO] == 2);   // aparencia de fabrica da 2.0: Da arte + Frost
  assert(valor[AJ_COR_LOGO] == 0 && valor[AJ_VIDRO] == 2); // Logo color on, glass Automatic
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
  assert(valor[AJ_PULAR_CREDITOS] == 1 && !ajustes_pular_creditos());
  assert(!strcmp(CHAVE[AJ_PULAR_CREDITOS], "pularCreditosLocal"));
  assert(somenteDesteAparelho(AJ_PULAR_CREDITOS) && !dePerfil(AJ_PULAR_CREDITOS));
  assert(!strcmp(i18n(OPCOES[AJ_PULAR_CREDITOS].rotulo), "Skip the credits automatically"));
  assert(!ajustes_proximo_auto() && ajustes_proximo_contagem() == 5);
  assert(AJ_INTERFACE_MODO == AJ_PROXIMO_CONTAGEM + 1);
  assert(AJ_MIC_BUSCA == AJ_INTERFACE_MODO + 1);
  assert(AJ_MENU_TOPO == AJ_MIC_BUSCA + 1 && AJ_ABAS_ESTILO == AJ_MENU_TOPO + 1 && AJ_TECLADO_ANDROID == AJ_ABAS_ESTILO + 1 && AJ_LEG_CONTORNO_COR == AJ_TECLADO_ANDROID + 1);
  assert(AJ_FALHA_PROVEDORES == AJ_LEG_CONTORNO_COR + 1 && AJ_RETOMADA_MORNA == AJ_FALHA_ADDON_LIMITE + 1 && AJ_MANTER_PRAZO == AJ_RETOMADA_MORNA + 1 && AJ_ESTAT_TAMANHO == AJ_MANTER_PRAZO + 1 && AJ_OCULTAR_FILMES_VISTOS == AJ_ESTAT_TAMANHO + 1 && AJ_ESTAT_LADO == AJ_OCULTAR_FILMES_VISTOS + 1 && AJ_DICASCONTROLE == AJ_ESTAT_LADO + 1 && AJ_HERO_GENERO == AJ_DICASCONTROLE + 1 && AJ_ABERTURA_ADDON == AJ_HERO_GENERO + 1 && AJ_HERO_SINOPSE == AJ_ABERTURA_ADDON + 1 && AJ_HERO_NOTAS == AJ_HERO_SINOPSE + 1 && AJ_HERO_ANO == AJ_HERO_NOTAS + 1 && AJ_HERO_DURACAO == AJ_HERO_ANO + 1 && AJ_CURADORIA_URL == AJ_HERO_DURACAO + 1 && AJ_ABERTURA_FUNDO == AJ_CURADORIA_URL + 1 && AJ_HERO_SINOPSE_TAM == AJ_ABERTURA_FUNDO + 1 && AJ_EP_TAMANHO == AJ_HERO_SINOPSE_TAM + 1 && AJ_AMIGOS_ESTILO == AJ_MURAL_TITULOS + 1 && AJ_FONTE_IDIOMA_AUDIO == AJ_AMIGOS_ESTILO + 1 &&
  AJ_AUD_LINGUA2 == AJ_FONTE_IDIOMA_AUDIO + 1 &&
#ifdef NV_ANDROID
  AJ_BAIXAR_REGISTRO == AJ_AUD_LINGUA2 + 1 && AJ_FILTROS_FONTES == AJ_BAIXAR_REGISTRO + 1 &&
#else
  AJ_FILTROS_FONTES == AJ_AUD_LINGUA2 + 1 &&
#endif
  AJ_PLAYER_EXTERNO == AJ_COLECAO_EXIBICAO + 1 &&   // #501: player externo depois
  AJ_TRAILER_AUDIO == AJ_PLAYER_EXTERNO + 1 &&      // idioma do audio do trailer
  AJ_PLAYER_ANIME == AJ_TRAILER_AUDIO + 1 &&        // #205: player so para anime
  AJ_ABERTURA_ESTILO == AJ_PLAYER_ANIME + 1 &&      // #205: estilo da abertura (B3)
  AJ_MENU_CARTAZ == AJ_ABERTURA_ESTILO + 1 && AJ_TITULO_FIL == AJ_MENU_CARTAZ + 1 &&        // #205: titulo das fileiras
  AJ_VIDRO_QUALIDADE == AJ_TITULO_FIL + 1 &&   // glass quality (local)
  AJ_FUNDO_DIN == AJ_VIDRO_QUALIDADE + 1 &&         // #205: fundo da Dinamica
  AJ_FUNDO_DIN_COR == AJ_FUNDO_DIN + 1 &&
  AJ_ESCALA_TEXTO == AJ_FUNDO_DIN_COR + 1 &&   // text and component scale (local)
  AJ_FONTE_GRUPO == AJ_ESCALA_TEXTO + 1 &&      // source memory (local)
  AJ_FONTE_GRUPO_SESSOES == AJ_FONTE_GRUPO + 1 &&
  AJ_LINK_REUSAR == AJ_FONTE_GRUPO_SESSOES + 1 &&
  AJ_LINK_PRAZO == AJ_LINK_REUSAR + 1 &&
  AJ_DET_ESPACO == AJ_LINK_PRAZO + 1 &&   // title page row spacing (local)
  AJ_FOCO_ACENTO == AJ_DET_ESPACO + 1 &&   // accent focus plate (local)
  AJ_NT_ORDEM == AJ_FOCO_ACENTO + 1 &&
  AJ_ARTE_TOM == AJ_NT_ORDEM + 1 &&
  AJ_PALCO_FUNDO == AJ_ARTE_TOM + 1 &&
  AJ_URL_LISTA == AJ_PALCO_FUNDO + 1 &&   // URL lists and guides (local)
  AJ_URL_M3U_ADD == AJ_URL_LISTA + 1 &&
  AJ_URL_XMLTV_ADD == AJ_URL_M3U_ADD + 1 &&
  AJ_URL_REM == AJ_URL_XMLTV_ADD + 1 &&
  AJ_NT_EP == AJ_URL_REM + 1 &&
  AJ_LIVETV_SUGERIR == AJ_NT_EP + 1 &&
  AJ_AUDIO_OFFSET == AJ_LIVETV_SUGERIR + 1 &&
  AJ_XTREAM_CONEXOES == AJ_AUDIO_OFFSET + 1 &&
  AJ_AMPLIAR_UI == AJ_XTREAM_CONEXOES + 1 &&
  AJ_GUIA_FONTE == AJ_AMPLIAR_UI + 1 &&
  AJ_GUIA_RECARREGAR == AJ_GUIA_FONTE + 1 &&
  AJ_M3U_URL == AJ_GUIA_RECARREGAR + 1 &&
  AJ_M3U_EPG == AJ_M3U_URL + 1 &&
  AJ_M3U_LIMPAR == AJ_M3U_EPG + 1 &&
  AJ_IPTV_CONTA == AJ_M3U_LIMPAR + 1 &&
  AJ_IPTV_ADD_XTREAM == AJ_IPTV_CONTA + 1 &&
  AJ_IPTV_ADD_M3U == AJ_IPTV_ADD_XTREAM + 1 &&
  AJ_IPTV_ADD_STALKER == AJ_IPTV_ADD_M3U + 1 &&
  AJ_IPTV_NOME == AJ_IPTV_ADD_STALKER + 1 &&
  AJ_IPTV_ATIVA == AJ_IPTV_NOME + 1 &&
  AJ_IPTV_REMOVER == AJ_IPTV_ATIVA + 1 &&
  AJ_IPTV_LISTA_XTREAM == AJ_IPTV_REMOVER + 1 &&
  AJ_IPTV_LISTA_M3U == AJ_IPTV_LISTA_XTREAM + 1 &&
  AJ_IPTV_LISTA_STALKER == AJ_IPTV_LISTA_M3U + 1 &&
  AJ_IPTV_ACOES == AJ_IPTV_LISTA_STALKER + 1 &&
  AJ_IPTV_EXCLUIR == AJ_IPTV_ACOES + 1 &&
  AJ_IPTV_SALVAR == AJ_IPTV_EXCLUIR + 1 &&
  AJ_IPTV_CANCELAR == AJ_IPTV_SALVAR + 1 &&
  AJ_GUIA_ORDEM == AJ_IPTV_CANCELAR + 1 &&
  AJ_IPTV_ORDEM == AJ_GUIA_ORDEM + 1 &&
  AJ_EPG_MEMORIA == AJ_IPTV_ORDEM + 1 &&
  AJ_GUIA_CATEGORIAS == AJ_EPG_MEMORIA + 1 &&
  AJ_BIB_HOME == AJ_GUIA_CATEGORIAS + 1 &&
  AJ_AMPLIACAO_ALGO == AJ_BIB_HOME + 1 &&
  AJ_AMPLIACAO_NITIDEZ == AJ_AMPLIACAO_ALGO + 1 &&
  AJ_HDR_INTERFACE == AJ_AMPLIACAO_NITIDEZ + 1 &&
  AJ_LIMPAR_CACHE == AJ_HDR_INTERFACE + 1 &&
  AJ_TUNNELING == AJ_LIMPAR_CACHE + 1 && AJ_DV7 == AJ_TUNNELING + 1 &&
  AJ_LOG_REDE == AJ_DV7 + 1 && AJ_N > AJ_LOG_REDE);
  assert(valor[AJ_LOG_REDE] == 1 && valorPadrao[AJ_LOG_REDE] == 1 &&
         uxValorPadrao(AJ_LOG_REDE) == 1 && !ajustes_log_rede()); // V_LIGA: 1 = Off
  // Comentarios do Trakt no painel de pausa: ligado de fabrica; fora do blob do web (como reacaoCreditosLocal).
  assert(valor[AJ_PAUSA_COMENTARIOS] == 0 && valorPadrao[AJ_PAUSA_COMENTARIOS] == 0 && ajustes_pausa_comentarios());
  assert(!strcmp(CHAVE[AJ_PAUSA_COMENTARIOS], "pausaComentariosLocal") && somenteDesteAparelho(AJ_PAUSA_COMENTARIOS) && !dePerfil(AJ_PAUSA_COMENTARIOS));
  // Cada opcao nova confere a SUA posicao (depois das ja existentes), sem fixar
  // o fim da lista: duas opcoes acrescentadas em PRs diferentes nao se quebram.
  assert(AJ_PAUSA_COMENTARIOS > AJ_LOG_REDE && AJ_PAUSA_NOTAS == AJ_PAUSA_COMENTARIOS + 1 && AJ_PAUSA_ELENCO == AJ_PAUSA_NOTAS + 1 && AJ_N > AJ_PAUSA_ELENCO);
  assert(valor[AJ_PAUSA_ELENCO] == 1 && valorPadrao[AJ_PAUSA_ELENCO] == 1 && !ajustes_pausa_elenco());   // V_LIGA: 1 = Off
  assert(!strcmp(CHAVE[AJ_PAUSA_ELENCO], "pausaElencoLocal") && somenteDesteAparelho(AJ_PAUSA_ELENCO) && !dePerfil(AJ_PAUSA_ELENCO));
  assert(valor[AJ_PAUSA_NOTAS] == 0 && valorPadrao[AJ_PAUSA_NOTAS] == 0 && ajustes_pausa_notas());
  assert(!strcmp(CHAVE[AJ_PAUSA_NOTAS], "pausaNotasLocal") && somenteDesteAparelho(AJ_PAUSA_NOTAS) && !dePerfil(AJ_PAUSA_NOTAS));
  // #492: retomada morna e o prazo do video pronto
  assert(valor[AJ_RETOMADA_MORNA] == 0 && ajustes_retomada_morna());   // V_LIGA: 0 = Ligado
  assert(!strcmp(CHAVE[AJ_RETOMADA_MORNA], "retomadaMornaLocal") && somenteDesteAparelho(AJ_RETOMADA_MORNA) && !dePerfil(AJ_RETOMADA_MORNA));
  assert(valorPadrao[AJ_RETOMADA_MORNA] == 0 && ajustes_manter_video_ms() == 120000u);   // 2 min
  valor[AJ_MANTER_PRAZO] = 0; assert(ajustes_manter_video_ms() == 120000u);
  valor[AJ_MANTER_PRAZO] = 4; assert(ajustes_manter_video_ms() == 0u);
  valor[AJ_MANTER_PRAZO] = 99; assert(ajustes_manter_video_ms() == 120000u);
  valor[AJ_MANTER_PRAZO] = 2; assert(ajustes_manter_video_ms() == 600000u);
  assert(valor[AJ_HERO_GENERO] == 0 && ajustes_hero_genero());   // Ligado: o visual de sempre
  assert(!strcmp(CHAVE[AJ_HERO_GENERO], "heroGeneroLocal") && somenteDesteAparelho(AJ_HERO_GENERO));
  // #205: sinopse, notas, ano e duracao do destaque, cada um no seu ajuste,
  // LIGADOS de fabrica (o visual de sempre), locais como o genero.
  assert(valor[AJ_HERO_SINOPSE] == 0 && valor[AJ_HERO_NOTAS] == 0 && valor[AJ_HERO_ANO] == 0 && valor[AJ_HERO_DURACAO] == 0);
  assert(ajustes_hero_sinopse() && ajustes_hero_notas() && ajustes_hero_ano() && ajustes_hero_duracao());
  assert(!strcmp(CHAVE[AJ_HERO_SINOPSE], "heroSinopseLocal") && somenteDesteAparelho(AJ_HERO_SINOPSE));
  assert(!strcmp(CHAVE[AJ_HERO_NOTAS], "heroNotasLocal") && somenteDesteAparelho(AJ_HERO_NOTAS));
  assert(!strcmp(CHAVE[AJ_HERO_ANO], "heroAnoLocal") && somenteDesteAparelho(AJ_HERO_ANO));
  assert(!strcmp(CHAVE[AJ_HERO_DURACAO], "heroDuracaoLocal") && somenteDesteAparelho(AJ_HERO_DURACAO));
  valor[AJ_HERO_SINOPSE] = 1; assert(!ajustes_hero_sinopse()); valor[AJ_HERO_SINOPSE] = 0;
  valor[AJ_HERO_NOTAS] = 1; assert(!ajustes_hero_notas()); valor[AJ_HERO_NOTAS] = 0;
  valor[AJ_HERO_ANO] = 1; assert(!ajustes_hero_ano()); valor[AJ_HERO_ANO] = 0;
  valor[AJ_HERO_DURACAO] = 1; assert(!ajustes_hero_duracao()); valor[AJ_HERO_DURACAO] = 0;
  assert(!strcmp(i18n(OPCOES[AJ_HERO_SINOPSE].rotulo), "Show synopsis on hero"));
  assert(!strcmp(i18n(OPCOES[AJ_HERO_NOTAS].rotulo), "Show ratings on hero"));
  assert(!strcmp(i18n(OPCOES[AJ_HERO_ANO].rotulo), "Show year on hero"));
  assert(!strcmp(i18n(OPCOES[AJ_HERO_DURACAO].rotulo), "Show runtime on hero"));
  assert(valor[AJ_ABERTURA_ADDON] == 1 && !ajustes_abertura_addon());
  assert(!strcmp(CHAVE[AJ_ABERTURA_ADDON], "aberturaAddonLocal") && somenteDesteAparelho(AJ_ABERTURA_ADDON));
  valor[AJ_ABERTURA_ADDON] = 0; assert(ajustes_abertura_addon()); valor[AJ_ABERTURA_ADDON] = 1;
  // #508: o fundo da tela Cinema da abertura: Arte borrada por padrao (o de
  // sempre), local, com os valores na ordem Desligado/Arte/Arte borrada.
  assert(valor[AJ_ABERTURA_FUNDO] == 1 && ajustes_abertura_fundo() == 1);   // A3: sharp + veil
  assert(!strcmp(CHAVE[AJ_ABERTURA_FUNDO], "aberturaFundoLocal") && somenteDesteAparelho(AJ_ABERTURA_FUNDO));
  valor[AJ_ABERTURA_FUNDO] = 0; assert(ajustes_abertura_fundo() == 0);
  valor[AJ_ABERTURA_FUNDO] = 1; assert(ajustes_abertura_fundo() == 1);
  valor[AJ_ABERTURA_FUNDO] = 2; assert(ajustes_abertura_fundo() == 2);
  valor[AJ_ABERTURA_FUNDO] = 7; assert(ajustes_abertura_fundo() == 1);   // fora da lista: o padrao
  valor[AJ_ABERTURA_FUNDO] = 1;
  assert(OPCOES[AJ_ABERTURA_FUNDO].n == 3 && !strcmp(OPCOES[AJ_ABERTURA_FUNDO].valores[0], "Desligado") &&
         !strcmp(OPCOES[AJ_ABERTURA_FUNDO].valores[1], "Arte") && !strcmp(OPCOES[AJ_ABERTURA_FUNDO].valores[2], "Arte borrada"));
  // #205: estilo da tela de abertura: Centro (A3) por padrao, Canto (B3), local.
  assert(valor[AJ_ABERTURA_ESTILO] == 0 && ajustes_abertura_estilo() == 0);
  assert(!strcmp(CHAVE[AJ_ABERTURA_ESTILO], "aberturaEstiloLocal") && somenteDesteAparelho(AJ_ABERTURA_ESTILO) && !dePerfil(AJ_ABERTURA_ESTILO));
  valor[AJ_ABERTURA_ESTILO] = 1; assert(ajustes_abertura_estilo() == 1);
  valor[AJ_ABERTURA_ESTILO] = 0; assert(ajustes_abertura_estilo() == 0);
  assert(valor[AJ_VIDRO_QUALIDADE] == 0 && ajustes_vidro_qualidade() == 0 && OPCOES[AJ_VIDRO_QUALIDADE].n == 3);
  assert(!strcmp(CHAVE[AJ_VIDRO_QUALIDADE], "vidroQualidadeLocal") && somenteDesteAparelho(AJ_VIDRO_QUALIDADE) && !dePerfil(AJ_VIDRO_QUALIDADE));
  valor[AJ_VIDRO_QUALIDADE] = 1; assert(ajustes_vidro_qualidade() == 1);
  valor[AJ_VIDRO_QUALIDADE] = 2; assert(ajustes_vidro_qualidade() == 2);
  valor[AJ_VIDRO_QUALIDADE] = 0;
  assert(OPCOES[AJ_ABERTURA_ESTILO].n == 2 && !strcmp(OPCOES[AJ_ABERTURA_ESTILO].valores[0], "Centro") &&
         !strcmp(OPCOES[AJ_ABERTURA_ESTILO].valores[1], "Canto"));
  assert(!strcmp(i18n(OPCOES[AJ_ABERTURA_ESTILO].rotulo), "Opening screen style"));
  // #205: titulo das fileiras: Classico por padrao (nada muda ate escolher), local, 4 valores.
  assert(valor[AJ_TITULO_FIL] == 0 && ajustes_titulo_fil() == 0);
  assert(!strcmp(CHAVE[AJ_TITULO_FIL], "rowTitleStyleLocal") && somenteDesteAparelho(AJ_TITULO_FIL) && !dePerfil(AJ_TITULO_FIL));
  valor[AJ_TITULO_FIL] = 3; assert(ajustes_titulo_fil() == 3);
  valor[AJ_TITULO_FIL] = 9; assert(ajustes_titulo_fil() == 0);   // arquivo torto volta ao Classico
  valor[AJ_TITULO_FIL] = 0;
  assert(OPCOES[AJ_TITULO_FIL].n == 4 && !strcmp(OPCOES[AJ_TITULO_FIL].valores[0], "Clássico") &&
         !strcmp(OPCOES[AJ_TITULO_FIL].valores[1], "Ilha") && !strcmp(OPCOES[AJ_TITULO_FIL].valores[2], "Editorial") &&
         !strcmp(OPCOES[AJ_TITULO_FIL].valores[3], "Acento"));
  assert(!strcmp(i18n(OPCOES[AJ_TITULO_FIL].rotulo), "Row titles"));
  // #205: fundo da Dinamica: Cinza por padrao, depois Cor e Imersivo; a cor e local.
  assert(valor[AJ_FUNDO_DIN] == 0 && ajustes_fundo_din() == 0);
  assert(!strcmp(CHAVE[AJ_FUNDO_DIN], "fundoDinamicaLocal") && somenteDesteAparelho(AJ_FUNDO_DIN) && !dePerfil(AJ_FUNDO_DIN));
  assert(!strcmp(CHAVE[AJ_FUNDO_DIN_COR], "fundoDinamicaCorLocal") && somenteDesteAparelho(AJ_FUNDO_DIN_COR) && !dePerfil(AJ_FUNDO_DIN_COR));
  valor[AJ_FUNDO_DIN] = 2; assert(ajustes_fundo_din() == 2);
  valor[AJ_FUNDO_DIN] = 3; assert(ajustes_fundo_din() == 3);
  valor[AJ_FUNDO_DIN] = 9; assert(ajustes_fundo_din() == 0);   // arquivo de outra versao
  valor[AJ_FUNDO_DIN] = 0;
  assert(OPCOES[AJ_FUNDO_DIN].n == 4 && !strcmp(OPCOES[AJ_FUNDO_DIN].valores[0], "Cinza") &&
         !strcmp(OPCOES[AJ_FUNDO_DIN].valores[1], "Cor") && !strcmp(OPCOES[AJ_FUNDO_DIN].valores[2], "Imersivo") &&
         !strcmp(OPCOES[AJ_FUNDO_DIN].valores[3], "Cinza escuro"));
  assert(!strcmp(i18n(OPCOES[AJ_FUNDO_DIN].rotulo), "Dynamic layout background"));
  assert(!strcmp(i18n(OPCOES[AJ_FUNDO_DIN].valores[0]), "Gray") && !strcmp(i18n(OPCOES[AJ_FUNDO_DIN].valores[2]), "Immersive"));
  assert(OPCOES[AJ_FUNDO_DIN_COR].n == 17 && !strcmp(OPCOES[AJ_FUNDO_DIN_COR].valores[valor[AJ_FUNDO_DIN_COR]], "Oceano"));
  { float a[3], b[3];   // a cor segue a paleta do acento e o padrao e Oceano (#1276CE)
    ajustes_fundo_din_cor(a);
    assert(fabsf(a[0] - 0x12 / 255.0f) < 1e-3f && fabsf(a[2] - 0xce / 255.0f) < 1e-3f);
    valor[AJ_FUNDO_DIN_COR] = 99; ajustes_fundo_din_cor(b);   // fora da lista: volta ao padrao
    assert(!memcmp(a, b, sizeof a)); valor[AJ_FUNDO_DIN_COR] = 12; }
  // #205: o tamanho dos cards de episodio da Moderna — Medio de fabrica, local,
  // na secao da pagina do titulo, com tres valores.
  assert(!strcmp(CHAVE[AJ_EP_TAMANHO], "epTamanhoLocal"));
  assert(valor[AJ_EP_TAMANHO] == 1 && ajustes_ep_tamanho() == 1);
  assert(somenteDesteAparelho(AJ_EP_TAMANHO) && !dePerfil(AJ_EP_TAMANHO));
  assert(OPCOES[AJ_EP_TAMANHO].n == 3 && !strcmp(V_EP_TAMANHO[1], "Médio"));
  assert(!strcmp(i18n(OPCOES[AJ_EP_TAMANHO].rotulo), "Episode card size"));
  valor[AJ_EP_TAMANHO] = 0; assert(ajustes_ep_tamanho() == 0);
  valor[AJ_EP_TAMANHO] = 2; assert(ajustes_ep_tamanho() == 2);
  valor[AJ_EP_TAMANHO] = 7; assert(ajustes_ep_tamanho() == 1);   // fora da faixa: o medio
  valor[AJ_EP_TAMANHO] = 1;
  for (int i = 0; i < AJ_N; i++) assert(strcmp(CHAVE[i], "autoCreditosLocal"));
  // #338: o tamanho das estatisticas ao vivo: "Lembrar o ultimo" por padrao, local,
  // em Reproducao, com Lembrar + Completo/Compacto/Minimo.
  assert(!strcmp(CHAVE[AJ_ESTAT_TAMANHO], "estatTamanhoLocal"));
  assert(valor[AJ_ESTAT_TAMANHO] == 0 && ajustes_estat_tamanho() == -1);
  assert(somenteDesteAparelho(AJ_ESTAT_TAMANHO) && !dePerfil(AJ_ESTAT_TAMANHO) && OPCOES[AJ_ESTAT_TAMANHO].n == 4);
  valor[AJ_ESTAT_TAMANHO] = 1; assert(ajustes_estat_tamanho() == 0);
  valor[AJ_ESTAT_TAMANHO] = 3; assert(ajustes_estat_tamanho() == 2);
  valor[AJ_ESTAT_TAMANHO] = 0;
  assert(!strcmp(CHAVE[AJ_ESTAT_LADO], "estatLadoLocal") && somenteDesteAparelho(AJ_ESTAT_LADO) && OPCOES[AJ_ESTAT_LADO].n == 2);
  assert(valor[AJ_ESTAT_LADO] == 0 && ajustes_estat_lado() == 0);
  valor[AJ_ESTAT_LADO] = 1; assert(ajustes_estat_lado() == 1); valor[AJ_ESTAT_LADO] = 0;
  // 13/10: o layout "Apple TV" se chama Dinamica; o valor gravado nao muda.
  assert(HOME_LAYOUT_DINAMICA == 2 && HOME_LAYOUT_N == 3 && OPCOES[AJ_HOME_LAYOUT].n == 3);
  assert(!strcmp(V_HOME_LAYOUT[HOME_LAYOUT_DINAMICA], "Dinâmica"));
  assert(!strcmp(i18n(V_HOME_LAYOUT[HOME_LAYOUT_DINAMICA]), "Dynamic"));
  // Legacy manual choices migrate once; later choices survive repeated loads.
  char dir[1024], path[1200];
  snprintf(dir, sizeof dir, "%s/nuvio-perfauto-XXXXXX", nvt_dir());
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1); dados_iniciar(dir);
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  FILE *f = fopen(path, "w"); assert(f);
  for (unsigned i = 0; i < sizeof automatic / sizeof *automatic; i++) {
    int op = automatic[i];
    fprintf(f, "%s %d\n", CHAVE[op], valorPadrao[op] == 0 ? 1 : 0);
  }
  fprintf(f, "vidroOpacLocal 4\n");
  fclose(f);
  ajustes_dir(dir);
  for (unsigned i = 0; i < sizeof automatic / sizeof *automatic; i++)
    assert(valor[automatic[i]] == valorPadrao[automatic[i]]);
  for (unsigned i = 0; i < sizeof automatic / sizeof *automatic; i++)
    assert(definirValorDireto(automatic[i], valorPadrao[automatic[i]] == 0 ? 1 : 0));
  assert(gravar());
  for (int load = 0; load < 2; load++) {
    ajustes_dir(dir);
    for (unsigned i = 0; i < sizeof automatic / sizeof *automatic; i++)
      assert(valor[automatic[i]] == (valorPadrao[automatic[i]] == 0 ? 1 : 0));
  }
  assert(valor[AJ_VIDRO_OPAC] == 0);
  assert(definirValorDireto(AJ_VIDRO_OPAC, 3) && gravar());
  for (int load = 0; load < 2; load++) {
    ajustes_dir(dir); assert(valor[AJ_VIDRO_OPAC] == 3);
  }
  // Removed outline Automatic migrates to its old On default; manual Off stays.
  for (int outline = 0; outline <= 2; outline++) {
    f = fopen(path, "w"); assert(f);
    fprintf(f, "performanceAutoVersaoLocal 1\nvidroContornoLocal %d\n", outline);
    fclose(f); ajustes_dir(dir);
    assert(valor[AJ_VIDRO_CONTORNO] == (outline == 2 ? 0 : outline));
    assert(OPCOES[AJ_VIDRO_CONTORNO].n == 2);
  }
  // Fresh load records the migration too, before any user edit.
  unlink(path); ajustes_dir(dir);
  for (unsigned i = 0; i < sizeof automatic / sizeof *automatic; i++)
    assert(valor[automatic[i]] == valorPadrao[automatic[i]]);
  assert(definirValorDireto(AJ_ANIM, 1) && gravar());
  ajustes_dir(dir); assert(valor[AJ_ANIM] == 1);
  // Automatic keeps the existing panel eligibility; GPU support is gated downstream.
  valor[AJ_AMPLIAR_UI] = 2;
  assert(ajustes_ampliar_app());
  perfilSeguro = 1; assert(!ajustes_ampliar_app()); perfilSeguro = 0;
  // Exercise real settings through every automatic ladder step.
  gpun_preferencia(0);
  gpun_relatar(60, 40, 16.667, 1, 1); // Discard the initial scene window.
  for (int level = 0; level <= 4; level++) {
    valor[AJ_ANIM] = valor[AJ_VIDRO] = valor[AJ_VIDRO_FOSCO] = 2;
    valor[AJ_VIDRO_CONTORNO] = valor[AJ_VIDRO_QUALIDADE] = 0;
    if (level) for (int r = 0; r < 3; r++) gpun_relatar(30, 40, 16.667, 1, 1);
    if (level) for (int r = 0; r < 3; r++) gpun_relatar(34, 40, 16.667, 1, 1);
    assert(gpun_degrau() == level);
    assert(ajustes_animacoes_reduzidas() == (level >= 3));
    assert(ajustes_vidro() == (level < 4));
    assert(ajustes_vidro_contorno());
    assert(ajustes_vidro_fosco() == (level < 2));
    assert(ajustes_vidro_qualidade() == (level >= 1));
    valor[AJ_VIDRO_QUALIDADE] = 2; assert(ajustes_vidro_qualidade() == 2);
    valor[AJ_VIDRO_QUALIDADE] = 0;
    valor[AJ_ANIM] = 0; assert(!ajustes_animacoes_reduzidas());
    valor[AJ_ANIM] = 1; assert(ajustes_animacoes_reduzidas());
    valor[AJ_VIDRO] = 0; assert(ajustes_vidro());
    valor[AJ_VIDRO] = 1; assert(!ajustes_vidro());
    valor[AJ_VIDRO_CONTORNO] = 0; assert(ajustes_vidro_contorno());
    valor[AJ_VIDRO_CONTORNO] = 1; assert(!ajustes_vidro_contorno());
    valor[AJ_VIDRO_FOSCO] = 1; assert(ajustes_vidro_fosco());
    valor[AJ_VIDRO_FOSCO] = 0; assert(!ajustes_vidro_fosco());
  }
  assert(gpun_fosco_vetado());
  assert(definirValorDireto(AJ_VIDRO_FOSCO, 2));
  assert(!gpun_fosco_vetado());
  // Formato do player: de fabrica, o que o OSD sempre foi (marcas no canto de cima); local (o web nao tem).
  assert(AJ_PLR_FORMATO > AJ_LOG_REDE && AJ_PAUSA_COMENTARIOS == AJ_PLR_FORMATO + 1 && AJ_PLR_FORMATO_POS == AJ_PAUSA_ELENCO + 1 && AJ_N > AJ_PLR_FORMATO_POS);
  assert(valor[AJ_PLR_FORMATO_POS] == PLR_POS_CANTO && ajustes_plr_formato_pos() == PLR_POS_CANTO);
  assert(!strcmp(CHAVE[AJ_PLR_FORMATO_POS], "plrFormatoPosLocal") && somenteDesteAparelho(AJ_PLR_FORMATO_POS));
  valor[AJ_PLR_FORMATO_POS] = 99; assert(ajustes_plr_formato_pos() == PLR_POS_CANTO);   // fora da lista: o canto
  valor[AJ_PLR_FORMATO] = PLR_FMT_OFF; assert(inativa(AJ_PLR_FORMATO_POS));
  valor[AJ_PLR_FORMATO] = PLR_FMT_MARCAS; assert(!inativa(AJ_PLR_FORMATO_POS));
  valor[AJ_PLR_FORMATO_POS] = PLR_POS_CANTO;
  assert(valor[AJ_PLR_FORMATO] == PLR_FMT_MARCAS && ajustes_plr_formato() == PLR_FMT_MARCAS);
  assert(!strcmp(CHAVE[AJ_PLR_FORMATO], "plrFormatoLocal") && somenteDesteAparelho(AJ_PLR_FORMATO));
  valor[AJ_PLR_FORMATO] = 99; assert(ajustes_plr_formato() == PLR_FMT_MARCAS);   // valor fora da lista: as marcas
  valor[AJ_PLR_FORMATO] = PLR_FMT_MARCAS;
  // O codec do audio aparece de fabrica; a linha apaga com a fileira desligada.
  assert(AJ_PLR_CODEC == AJ_PLR_FORMATO_POS + 1 && AJ_N > AJ_PLR_CODEC);
  assert(valor[AJ_PLR_CODEC] == 0 && valorPadrao[AJ_PLR_CODEC] == 0 && ajustes_plr_codec());
  assert(!strcmp(CHAVE[AJ_PLR_CODEC], "plrCodecLocal") && somenteDesteAparelho(AJ_PLR_CODEC));
  valor[AJ_PLR_FORMATO] = PLR_FMT_OFF; assert(inativa(AJ_PLR_CODEC));
  valor[AJ_PLR_FORMATO] = PLR_FMT_MARCAS; assert(!inativa(AJ_PLR_CODEC));
  unlink(path);
  puts("ajustes_padroes: ok");
  return 0;
}
