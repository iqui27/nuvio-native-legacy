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
int  ajustes_pediu_diagnostico(void);
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
// 1 = ao mandar Reproduzir, ABRIR A FOLHA DE FONTES em vez de escolher
// sozinho. Padrao 0: quem nunca entrou em Ajustes continua com a escolha
// automatica de sempre.
//
// Pedido de um testador com o argumento certo: as fontes de um mesmo titulo
// diferem em resolucao, codec de video e faixa de audio, e essa escolha e de
// quem assiste. O automatico continua existindo porque tambem e verdade que
// perguntar em TODA reproducao cansa — por isso e opcao, e nao troca de
// comportamento.
//
// NAO VALE PARA CANAL AO VIVO: la a folha entraria entre um zap e outro, e o
// proprio fluxo do guia ja escolhe pela playlist que responde (ver tocarCanal).
int ajustes_fonte_manual(void);

// "Fonte automatica" = "Primeira da lista" (issue #130): o automatico toca a
// primeira fonte na ordem do addon e confere SO ela. 0 = "Melhor fonte", a
// regra de pontuacao de streams.c. Ver fonteauto.h.
int ajustes_fonte_primeira(void);
// "Outra fonte se falhar": quantas OUTRAS fontes o automatico tenta quando a
// escolhida nao abre (0..3; 0 = nenhuma). Nao vale para escolha manual nem
// para canal ao vivo, que tem o watchdog proprio em app.c.
int ajustes_fonte_repor(void);

int ajustes_idioma_ingles(void);

// COR DO ANEL DE FOCO, escolhida em "Cor de destaque" ou herdada da conta
// (selected_theme). Um "tema" neste app e so isto: ver TEMA_ACENTO em
// ajustes.c para o motivo de nao ser a paleta inteira. Branco e o padrao, que
// e exatamente o anel que sempre existiu.
void ajustes_acento(float *r, float *g, float *b);
// 0 = tema fixo; CORVIVA_SIMPLES ("Dinâmica") ou CORVIVA_ESTILIZADA ("Dinâmica
// estilizada"): o destaque segue a arte do titulo em cena (corviva.h). Os dois
// sao LOCAIS: nao sobem para a conta e a conta nao os desfaz (ver
// ajustes_aplicar_blob / ajustes_mesclar_blob).
int  ajustes_cor_viva(void);
// "Cor da logo": 1 = com tema dinamico, o destaque sai do logo do titulo.
int  ajustes_cor_logo(void);
// A MESMA cor mais a TINTA que contrasta com ela: devolve 0.067 (#111) sobre
// realce claro e 1.0 (branco) sobre realce escuro, luminancia Rec.709 com o
// degrau em 0,55. E a regra de FOCO de layout.h (preenchimento na cor de
// realce, sem anel) em uma chamada, para todo botao usar a mesma conta.
float ajustes_acento_tinta(float *r, float *g, float *b);
// A MESMA TINTA EM 0..255, para quem monta txt_linha: principal (255 ou 20)
// e secundaria (um degrau abaixo: 225 ou 60). Toda superficie pintada de
// ajustes_acento() escreve com estas duas — nunca com um 20 cravado.
int   ajustes_tinta_foco(void);
int   ajustes_tinta_foco2(void);
// "Automática", "4K", "1080p" ou "720p" — o rotulo exibido, para quem seleciona
// a fonte de video mostrar exatamente o que o usuario escolheu.
const char *ajustes_qualidade(void);

// --- LAYOUT: estrutura da home ----------------------------------------------
int   ajustes_rail_recolhida(void);     // collapseSidebar
int   ajustes_rail_moderna(void);       // modernSidebar
int   ajustes_rail_moderna_blur(void);  // modernSidebarBlur
int   ajustes_hero_ligado(void);        // heroSectionEnabled
int   ajustes_hero_cheio(void);         // modernHeroFullScreenBackdropEnabled
int   ajustes_hero_fonte(void);         // origem local da arte do hero (ARTEHERO_*)
// 1 = destaque/detalhe com foto diferente da do card (regra em artehero.h).
int   ajustes_hero_arte_diferente(void);
int   ajustes_ps_fundo_automatico(void); // #90: fundo da escolha de perfil (psfundo.c)
// Teto de memoria para imagens escolhido em Ajustes, em MB; 0 = automatico.
int   ajustes_tex_mb(void);
int   ajustes_posteres_deitados(void);  // modernLandscapePostersEnabled
int   ajustes_gradiente_foco_classico(void); // classicFocusGradientEnabled
// #95: 1 = ao reabrir, fileiras comecam no primeiro tile (ignora home-pos.txt).
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
// 1 = o "+" tambem publica no Plan to Watch do Simkl (#110).
int   ajustes_salvos_no_simkl(void);
// Os INDICES GRAVADOS de "Onde o + salva" (salvosDestino) e da fonte do
// "Continuar assistindo" (cwFonteLocal). Sao contrato com o ajustes.txt de quem
// ja usa o app: valor novo entra no fim, nunca no meio. tests/simkl_cw.sh
// confere cada um contra o rotulo em ajustes.c.
enum { AJ_SALVOS_LOCAL = 0, AJ_SALVOS_TRAKT = 1, AJ_SALVOS_SIMKL = 2 };
enum { AJ_CWF_AMBAS = 0, AJ_CWF_CONTA = 1, AJ_CWF_TRAKT = 2, AJ_CWF_SIMKL = 3 };
// Envio automatico do registro (Sobre). 1 = ligado.
int   ajustes_envio_auto(void);
// Forca da vinheta do fundo do titulo, 0..1 (1 = a medida do web).
float ajustes_detalhe_veu(void);
int   ajustes_trailer_auto(void);      // trailer mudo no fundo da pagina de titulo
int   ajustes_trailer_hero(void);      // trailer mudo no destaque da home
int   ajustes_trailer_qualidade(void); // teto em linhas (1080/720/480); 0 = a maior
int   ajustes_trailer_fonte(void);     // TRF_* de trailerfonte.h; 0 = automatico
float ajustes_trailer_zoom(void);      // ampliacao do trailer (1.0 = quadro inteiro)
void  ajustes_definir_envio_auto(int ligado);
// Fonte do destaque (ARTEHERO_*) e "Destaque com outra arte", gravados na hora.
void  ajustes_definir_destaque(int fonte, int diferente);
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
int   ajustes_cw_ok_toca(void);         // OK no card: 1 = toca direto (cwOkLocal)
int   ajustes_cw_estilo(void);          // 0 card, 1 largo (wide), 2 poster
int   ajustes_cw_thumb_episodio(void);  // useEpisodeThumbnailsInCw
// AJ_CWF_*: 0 = todas (conta primeiro, Trakt e — com vinculo — Simkl
// completando), 1 = so a conta Nuvio, 2 = so o Trakt, 3 = so o Simkl. Local: o
// app oficial nao tem esta escolha.
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
// 1 = desenhar o anel colorido no cartaz em foco da Home. Desligado, o foco
// continua dito pelo TAMANHO do cartaz e pela animacao; o que sai e so a borda.
// Opcao LOCAL: o app oficial nao tem equivalente, entao ela nao vem nem vai no
// blob da conta (ver a nota de salvosDestino em CHAVE[]).
int   ajustes_borda_foco(void);

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
// QUALIDADE DA IMAGEM: 0 baixa, 1 padrão, 2 alta.
//
// Vale para a arte que ENTRAR daqui para frente — o que já está decodificado
// continua como está até ser despejado. Quem consome é tex_cache (o teto de
// decodificação de cada pedido) e artehero (qual versão da url pedir).
int   ajustes_qualidade_imagem(void);
float ajustes_raio_poster_px(void);   // raio em px (dp x 2)

// --- INTEGRACOES --------------------------------------------------------------
// Chaves snake_case identicas as de tmdb_settings / mdblist_settings do blob
// da conta (profileSettingsSyncService.js do web) — ajustes_aplicar_blob as
// aplica sozinha. Cada accessor ja combina o master: com o integracao
// desligada, TODOS os ajustes_tmdb_* / ajustes_mdblist_fonte() devolvem 0.
int         ajustes_tmdb_ligado(void);          // tmdb_enabled
const char *ajustes_tmdb_idioma(void);          // "pt-BR", "en-US"… (TMDB)
int         ajustes_tmdb_arte(void);            // tmdb_use_artwork
int         ajustes_tmdb_basico(void);          // tmdb_use_basic_info
int         ajustes_tmdb_ficha(void);           // tmdb_use_details
int         ajustes_tmdb_datas(void);           // tmdb_use_release_dates
int         ajustes_tmdb_elenco(void);          // tmdb_use_credits
int         ajustes_tmdb_prod(void);            // tmdb_use_productions
int         ajustes_tmdb_redes(void);           // tmdb_use_networks
int         ajustes_tmdb_eps(void);             // tmdb_use_episodes
int         ajustes_tmdb_trailers(void);        // tmdb_use_trailers
int         ajustes_tmdb_mais(void);            // tmdb_use_more_like_this
int         ajustes_tmdb_col(void);             // tmdb_use_collections
int         ajustes_tmdb_cw(void);              // tmdb_enrich_continue_watching

int         ajustes_mdblist_ligado(void);       // mdblist_enabled
// `fonte` e um ExFonte de extras.h (trakt, imdb, tmdb, tomatoes, audience,
// metacritic, letterboxd). 0 = esconder a nota dessa fonte na fileira.
int         ajustes_mdblist_fonte(int fonte);   // mdblist_show_*

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

// O CAMINHO DE VOLTA (#85): devolve em *saida uma copia do blob `base` (o mesmo
// objeto `settings_json` que ajustes_aplicar_blob le) com os valores DESTA TV
// escritos por cima. Devolve quantas chaves foram reescritas; 0 quando nao havia
// nada a mudar, e nesse caso *saida fica NULL. Quem chama libera com free().
//
// O QUE ELA NAO FAZ, e e o ponto: nao INVENTA chave. So o que ja existe no blob
// e reescrito, e so o valor. Remontar o objeto aqui mandaria de volta um blob
// com apenas as chaves que este app conhece — e o servidor guarda o que vier,
// entao a TV apagaria da conta tudo que so o app web usa.
//
// Nao sobem os ajustes de APARELHO (superficie 4K, teto de memoria de imagem,
// qualidade da arte, idioma da interface, animacoes reduzidas, envio de
// registro, limite/ordem de fileiras, reset de foco, borda do foco, fonte
// manual, destino dos salvos): eles descrevem esta TV, nao o gosto da pessoa, e
// a conta e uma so para a TV da sala e a do quarto. Ver somenteDesteAparelho.
int ajustes_mesclar_blob(const char *base, char **saida);

#endif
