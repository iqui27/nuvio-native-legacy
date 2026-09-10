// Tela de Ajustes: lista vertical de opcoes em secoes, rotulo a esquerda e
// valor a direita.
//
// As chaves de LAYOUT sao as mesmas de js/data/local/layoutPreferences.js do app
// web, com os mesmos nomes, os mesmos padroes de fabrica e — onde o port desenha
// a tela — o mesmo efeito. Nao sao preferencias inventadas para o port: o dono
// ja as muda na tela de Ajustes do web, e a home dele depende delas.
//
// Os valores sao gravados em <dir>/ajustes.txt, uma chave por linha.
#ifndef NV_AJUSTES_H
#define NV_AJUSTES_H
#include <SDL2/SDL.h>

int  ajustes_iniciar(void);

// Pasta onde os ajustes sao lidos e gravados. Chamar uma vez, no inicio.
void ajustes_dir(const char *dir);
void ajustes_evento(const SDL_Event *e);
void ajustes_atualizar(float dt, Uint32 agora);
void ajustes_desenhar(Uint32 agora);
int  ajustes_quer_sair(void);
// 1 quando a linha "Addons" foi acionada. Lido e zerado na chamada.
int  ajustes_pediu_addons(void);   // 1 quando o Back deve fechar a tela
void ajustes_encerrar(void);

// Leitura pelo resto do app. "Animacoes reduzidas" e a que mais importa: com
// ela ligada, quem anima deve ir direto ao alvo em vez de chamar anim_mola —
// e um ajuste de acessibilidade, nao um gosto, e uma tela que o ignora nao
// serve para quem o ligou.
int ajustes_animacoes_reduzidas(void);
int ajustes_dolby_vision(void);
int ajustes_dolby_atmos(void);
// pauseOverlayEnabled: o painel de ficha que sobe alguns segundos depois de
// pausar o video. Ver pausao.h.
int ajustes_pausa_overlay(void);
int ajustes_idioma_ingles(void);
// "Automática", "4K", "1080p" ou "720p" — o rotulo exibido, para quem seleciona
// a fonte de video mostrar exatamente o que o usuario escolheu.
const char *ajustes_qualidade(void);

// --- LAYOUT: estrutura da home ----------------------------------------------
int   ajustes_rail_recolhida(void);     // collapseSidebar
int   ajustes_rail_moderna(void);       // modernSidebar
int   ajustes_rail_moderna_blur(void);  // modernSidebarBlur
int   ajustes_hero_ligado(void);        // heroSectionEnabled
int   ajustes_hero_cheio(void);         // modernHeroFullScreenBackdropEnabled
int   ajustes_posteres_deitados(void);  // modernLandscapePostersEnabled
int   ajustes_gradiente_foco_classico(void); // classicFocusGradientEnabled
// x onde o conteudo comeca. Nao e constante: o recuo e sempre 104 e a rail
// soma os 144 dela quando esta fixa.
float ajustes_conteudo_x(void);

// --- LAYOUT: rotulos e metadados --------------------------------------------
int   ajustes_rotulos_poster(void);     // posterLabelsEnabled
int   ajustes_nome_addon(void);         // catalogAddonNameEnabled
int   ajustes_sufixo_tipo(void);        // catalogTypeSuffixEnabled
int   ajustes_ocultar_nao_lancados(void);   // hideUnreleasedContent
void  ajustes_definir_ocultar_nao_lancados(int ligado);
int   ajustes_data_completa(void);      // showFullReleaseDate
// Onde o "+" escreve. 1 = tambem na watchlist do Trakt, 0 = so na lista local
// desta TV. LOCAL: nao existe campo equivalente no perfil da conta, entao a
// escolha nunca sobe nem desce pelo blob — mas ela PRECISA sobreviver ao
// fechamento, e gravar() pula toda chave iniciada por "-"; por isso a chave em
// ajustes.txt ("salvosDestino") NAO leva o "-" das linhas de conta. (A primeira
// versao deste comentario dizia o contrario do codigo.) A lista local e escrita
// nos DOIS valores — ver a nota de V_SALVOS em ajustes.c e a abertura de salvos.h.
int   ajustes_salvos_no_trakt(void);
void  ajustes_definir_salvos_no_trakt(int noTrakt);
// homeImdbRatingsVisibility: 0 SHOW_ALL, 1 HIDE_ALL
int   ajustes_notas_home(void);
// discoverLocation: 0 in_search, 1 in_sidebar, 2 off
int   ajustes_local_descobrir(void);
int   ajustes_descobrir_na_busca(void); // searchDiscoverEnabled (derivado)

// --- LAYOUT: continuar assistindo -------------------------------------------
// 1 = pedir uma superficie 3840x2160 na criacao da janela. A TV pode ignorar,
// e a maioria ignora — a linha `janela=... drawable=...` do log diz o que ela
// respondeu. Ver a nota em main.c.
int   ajustes_4k(void);
int   ajustes_cw_ligado(void);          // continueWatchingEnabled
int   ajustes_cw_estilo(void);          // 0 card, 1 largo (wide), 2 poster
int   ajustes_cw_thumb_episodio(void);  // useEpisodeThumbnailsInCw
// 0 = as duas fontes (conta primeiro, Trakt completando), 1 = so a conta
// Nuvio, 2 = so o Trakt. Local: o app oficial nao tem esta escolha.
int   ajustes_cw_fonte(void);
int   ajustes_cw_desfocar_proximo(void);// blurContinueWatchingNextUp
int   ajustes_cw_do_episodio_mais_alto(void); // nextUpFromFurthestEpisode
int   ajustes_cw_mostrar_nao_exibidos(void);  // showUnairedNextUp
// continueWatchingSortMode: 0 default, 1 streaming_style, 2 split_upcoming
int   ajustes_cw_ordem(void);

// --- LAYOUT: pagina de detalhe (efeito vive em detail.c) ---------------------
int   ajustes_desfocar_nao_assistidos(void); // blurUnwatchedEpisodes
int   ajustes_botao_trailer(void);           // detailPageTrailerButtonEnabled
int   ajustes_meta_externo(void);            // preferExternalMetaAddonDetail

// --- LAYOUT: foco no poster --------------------------------------------------
int   ajustes_expandir_poster(void);         // focusedPosterBackdropExpandEnabled
float ajustes_expandir_poster_atraso(void);  // em segundos
int   ajustes_navegacao_horizontal_rapida(void); // fastHorizontalNavigationEnabled

// --- LAYOUT: profundidade dos cartoes ---------------------------------------
int   ajustes_profundidade(void);            // cardDepthEnabled
float ajustes_profundidade_borda(void);      // 0..1 (cardDepthEdgeStrength/100)
float ajustes_profundidade_brilho(void);     // 0..1 (cardDepthSheenStrength/100)
float ajustes_profundidade_cobertura(void);  // 0..1 (cardDepthEdgeCoverage/100)
int   ajustes_profundidade_posters(void);
int   ajustes_profundidade_cw(void);
int   ajustes_profundidade_episodios(void);
int   ajustes_profundidade_elenco(void);
int   ajustes_profundidade_trailers(void);

// --- LAYOUT: tamanho do item -------------------------------------------------
// ATENCAO, e a armadilha que ja custou uma medida errada: no layout MODERNO
// `posterCardWidthDp` NAO muda o tamanho do poster. `buildModernHomeSizingStyle`
// gera --home-poster-width: 218px para 120dp, mas a folha do layout moderno
// redefine a variavel para 212px em .home-screen-shell.home-layout-modern
// (components.css:6462) e e ela que vence — CONFERIDO no app rodando: mudar a
// variavel inline de 218 para 300 nao mexeu um pixel no card. O que sai da
// preferencia e so o RAIO.
int   ajustes_largura_poster_dp(void);
int   ajustes_raio_poster_dp(void);
float ajustes_raio_poster_px(void);   // raio em px (dp x 2)

// --- AJUSTES QUE VEM DA CONTA ------------------------------------------------
// Aplica o blob de `sync_pull_profile_settings_blob` (o objeto `settings_json`,
// como texto JSON cru) sobre os valores locais. Devolve quantas opcoes mudaram.
//
// POR QUE ISTO EXISTE: as ~40 chaves deste arquivo (`heroSectionEnabled`,
// `continueWatchingCardStyle`, `cardDepthEnabled`, `posterCardWidthDp`...) sao
// as MESMAS do app web, e os padroes daqui foram transcritos a mao do perfil de
// quem montou o pacote. Sem aplicar o blob, quem instalar recebe o layout de
// outra pessoa e nao o proprio — o mesmo defeito que o art/addons.txt tinha.
//
// Chave ausente no blob NAO mexe na opcao, e valor de texto que este app nao
// reconhece tambem nao: trocar por um padrao seria inventar uma escolha que o
// usuario nunca fez.
int ajustes_aplicar_blob(const char *json);

#endif
