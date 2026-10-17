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
#include "idiomacod.h"
#include "gfx.h"
#include "video.h"

// Targets with a system (IME) keyboard the "Teclado" setting can choose between:
// Android, LG webOS, Samsung Tizen (.tpk and .wgt). Elsewhere the option is hidden.
#if defined(NV_ANDROID) || defined(NV_WEBOS) || defined(NV_TPK) || defined(__EMSCRIPTEN__)
#define NV_TECLADO_SIS 1
#endif

int  ajustes_iniciar(void);
// Per device: 0 = TV (default), 1 = Mobile. Applied before the initial viewport.
int  ajustes_interface_mobile(void);

// Pasta onde os ajustes sao lidos e gravados. Chamar uma vez, no inicio.
void ajustes_dir(const char *dir);
void ajustes_evento(const SDL_Event *e);
void ajustes_atualizar(float dt, Uint32 agora);
void ajustes_fontes_atualizar(void);
void ajustes_desenhar(Uint32 agora);
int  ajustes_quer_sair(void);
// 1 quando a linha "Addons" foi acionada. Lido e zerado na chamada.
int  ajustes_pediu_addons(void);
int  ajustes_pediu_dicascontrole(void); // Settings > About and help
int  ajustes_pediu_novidades20(void);   // OK em Sobre e ajuda › Novidades 2.0: o app.c abre o guia
int  ajustes_pediu_plugins(void);   // OK em Plugins (F09): o app.c abre a tela   // 1 quando o Back deve fechar a tela
int  ajustes_pediu_diagnostico(void);
// OK em "Teste de velocidade" (Ajustes › Diagnóstico). Lido e zerado pelo app.c.
int  ajustes_pediu_velocidade(void);
// Itens da barra lateral que a pessoa pode esconder (#162). 1 = aparece.
int  ajustes_menu_explorar(void);
int  ajustes_menu_guia(void);
int  ajustes_menu_agenda(void);
int  ajustes_menu_perfil(void);
// "Trailer do cartaz em foco" (#124; tres modos desde 08/10/2026). Na conta
// continua o booleano do web (focusedPosterBackdropTrailerEnabled): true =
// qualquer modo ligado. O modo seguro devolve DESLIGADO.
//   DESLIGADO  nada toca com o foco num cartaz;
//   CHEIA      o trailer toca no lugar da arte do destaque e a home entra no
//              modo cinema, como o trailer do destaque (so o logo fica);
//   CARD       o trailer toca DENTRO do cartaz em foco (home.c: cartaz deitado
//              e 4:3 na hora; cartaz em pe so aberto por "Expandir poster").
enum { AJ_TRAILER_CARTAZ_DESLIGADO = 0, AJ_TRAILER_CARTAZ_CHEIA = 1, AJ_TRAILER_CARTAZ_CARD = 2 };
int  ajustes_trailer_cartaz(void);
// "Itens por fileira" da Home (#163): 12, 18 ou 24. Padrao 12.
int  ajustes_itens_fileira(void);
// Efeitos visuais do .tpk: 0 automatico, 1 completos (nivel 0), 2 leves (nivel 1).
int  ajustes_gpu_efeitos(void);
// A proxima abertura da tela (ajustes_iniciar) pousa o foco em "Cor de
// destaque" em vez da primeira linha. E o "Experimentar a cor viva" do cartao
// da 1.4.8: chamar ANTES de trocar para TELA_AJUSTES.
void ajustes_abrir_na_cor(void);
// Atalho do cartão de novidades para a tipografia da interface.
void ajustes_abrir_na_fonte(void);
// Atalhos do cartao da 1.6.0: Ajustes › Layout na linha "Layout da home", e
// Ajustes › Aparência na linha "Interface de vidro". Mesma regra da cor.
void ajustes_abrir_no_layout(void);
void ajustes_abrir_no_vidro(void);
// O "Reconectar" do modal do Trakt na ilha (02/10): pousa na linha do Trakt,
// onde o OK comeca o pareamento.
void ajustes_abrir_no_trakt(void);
// Fontes e addons > Escolha da fonte, na linha "Espera pelos add-ons": o destino
// do OK na explicacao da espera na ilha do player (#202).
void ajustes_abrir_na_espera_fonte(void);
// GUIA DE USO (Ajustes › Sobre e ajuda): a proxima abertura vai direto ao
// guia. `daNovidades` = 1 quando quem abriu foi o cartao da 1.8.0: o guia
// ganha "Voltar às novidades" e o Voltar dele devolve ao cartao
// (ajustes_pediu_novidades, lido e zerado pelo app.c).
void ajustes_abrir_no_guia(int daNovidades);
int  ajustes_pediu_novidades(void);
// O guia esta na tela (por cima das ilhas de Ajustes).
int  ajustes_guia_aberto(void);
// Os atalhos "Abrir o Guia de TV / a Biblioteca / a Agenda / o Explorar /
// Perfil e Stats" do guia: 1..5 na ordem, lido e zerado pelo app.c.
enum { AJ_TELA_GUIA_TV = 1, AJ_TELA_BIBLIOTECA, AJ_TELA_AGENDA, AJ_TELA_EXPLORAR, AJ_TELA_PERFIL };
int  ajustes_pediu_tela(void);
// O guia para o Spotlight no modo guia (spot_abrir_guia): as entradas que
// casam com `consulta` (titulo antes de texto), os textos de cada uma e a
// imagem dela; ajustes_guia_ir abre o guia na entrada.
int  ajustes_guia_buscar(const char *consulta, int *out, int max);
const char *ajustes_guia_titulo(int e);
const char *ajustes_guia_texto(int e);
const char *ajustes_guia_icone(int e);
const char *ajustes_guia_capitulo(int e);
const char *ajustes_guia_onde(int e);
int  ajustes_guia_novo(int e);
void ajustes_guia_imagem(int e, float x, float y, float w, float h);
void ajustes_guia_ir(int e);
// A linha em foco (indice AJ_*; -1 com o foco num grupo), para os testes
// conferirem onde a tela abriu.
int  ajustes_opcao_em_foco(void);
// 1 = o foco esta na coluna de categorias, que e onde a tela abre sempre —
// menos logo depois de ajustes_abrir_na_cor, que pousa na lista.
int  ajustes_foco_no_indice(void);
// Testes do ponteiro (#99): o editor aberto (0 fechado, 1 painel/linha,
// 2 dependencia), o valor pendente, se o foco esta no rodape "Restaurar
// padrao" e se a pergunta de restaurar esta na tela (com o botao em foco).
int  ajustes_teste_editor(int *pendente, int *rodape, int *restaurar, int *confirmar);
void ajustes_encerrar(void);

// Leitura pelo resto do app. "Animacoes reduzidas" e a que mais importa: com
// ela ligada, quem anima deve ir direto ao alvo em vez de chamar anim_mola —
// e um ajuste de acessibilidade, nao um gosto, e uma tela que o ignora nao
// serve para quem o ligou.
int ajustes_animacoes_reduzidas(void);
// Protecao de OLED (esmaecer.h): indice de V_ESMAECER (padrao 2 = 5 min) e de
// V_BRILHO_PLAYER (padrao 1 = 80%).
int ajustes_esmaecer(void);
int ajustes_brilho_player(void);
int ajustes_hdr_ui(void);   // #205: 1 = Automatico (padrao), 0 = Desligado
// #202: -1 = ultimo modo usado (padrao); 0..7 = PlrAspecto aplicado ao abrir o video.
int ajustes_proporcao_padrao(void);
int ajustes_descanso_estilo(void);   // ESM_ESTILO_* (esmaecer.h)
int ajustes_descanso_fonte(void);    // DESC_FONTE_* (descanso.h)
int ajustes_dolby_vision(void);
int ajustes_dolby_atmos(void);
// pauseOverlayEnabled: o painel de ficha que sobe alguns segundos depois de
// pausar o video. Ver pausao.h.
int ajustes_pausa_overlay(void);
int ajustes_classif_player(void);   // 1 = mostra a classificacao/guia parental no player (padrao)
// "O que achou?" nos creditos (reacao.h). Ligado de fabrica.
int ajustes_reacao_creditos(void);
// #329: pula abertura/resumo sozinho quando ha marcador (desligado de fabrica).
int ajustes_pular_abertura(void);
int ajustes_pular_resumo(void);
// #331: a contagem do proximo episodio nos creditos (desligado de fabrica; contagem configuravel, padrao 5 s).
int ajustes_proximo_auto(void);
int ajustes_proximo_contagem(void);
// Pula os creditos sozinho, so com inicio e fim reais do marcador (desligado de fabrica).
int ajustes_pular_creditos(void);
// Medidor de desempenho na ilha do relogio (desempenho.h): Desempenho desta TV,
// local. 0 = desligado, 1 = Minimo, 2 = Menor, 3 = Grande (DS_* de desempenho.h).
int ajustes_medidor_desempenho(void);
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
// #400: ordem visual do addon, local; nao altera autoplay.
int ajustes_fonte_ordem_addon(void);
// 1 = a folha de Fontes mostra o nome e a descricao do addon como vieram.
int ajustes_fonte_texto_addon(void);
int ajustes_fonte_texto_logo(void);
// "Outra fonte se falhar": quantas OUTRAS fontes o automatico tenta quando a
// escolhida nao abre (0..3; 0 = nenhuma). Nao vale para escolha manual nem
// para canal ao vivo, que tem o watchdog proprio em app.c.
int ajustes_fonte_repor(void);
// Prazo, em ms, da escolha automatica com a lista ainda enchendo (#221);
// 0 = INSTANTANEO (#202), -1 = esperar todos os addons. "Espera pelos
// add-ons" em Ajustes.
int ajustes_fonte_prazo_ms(void);
// Auto-play como no oficial (#202, fonteregra.h): FR_ESCOPO_*, FR_REGEX_* e
// "Usar os outros se nao houver" (1 = ligado).
int ajustes_fonte_tocar_conferindo(void);
int ajustes_fonte_aquecer(void);
int ajustes_fonte_conferir_varias(void);
int ajustes_fonte_preparar(void);
int ajustes_fonte_precarregar(void);
int ajustes_fonte_escopo(void);
int ajustes_fonte_regex_modo(void);
// "Usar a ordem" dos add-ons (FR_ORDEM_*); 0 tambem quando nao ha ordem.
int ajustes_fonte_ordem_uso(void);
int ajustes_fonte_usar_outros(void);
// R9b: 0 Equilibrio, 1 Qualidade maxima, 2 Começar rápido / 0 Preferir, 1 Indiferente, 2 Evitar HDR e DV.
int ajustes_fonte_prioridade(void);
int ajustes_fonte_hdr(void);
// 1 = a escolha automatica prefere fontes que anunciam o Idioma do audio
// (ling_audio) ou audio duplo, na mesma resolucao; nunca filtra (fontenota.h).
int ajustes_fonte_idioma_audio(void);

// Idioma da interface: um IDIOMA_* de idiomacod.h (pt, en, ro, uk, ru, fr, de, es). Valor
// gravado fora do intervalo (arquivo editado a mao) cai em portugues.
int ajustes_idioma(void);
// 1 so quando e ingles. Os textos montados com "%d.%d" e o formato de data
// americano usam isto; romeno, ucraniano, russo, frances, alemao e espanhol usam virgula decimal e data
// dia-mes-ano, como o portugues, e por isso NAO entram aqui.
int ajustes_idioma_ingles(void);
// IDIOMA AUTOMATICO (ver "IDIOMA AUTOMATICO" em ajustes.c). Enquanto a pessoa
// nunca escolheu um idioma nesta TV, a interface segue a conta (tmdb_language,
// depois o idioma de legenda), depois a TV, depois o ingles.
// _iniciar: uma vez, depois de ajustes_dir (le o locale da TV e resolve).
//   `aoMudar` roda quando o automatico TROCA o idioma depois do arranque (a
//   conta chegou, a TV respondeu): `codigo` e o "pt"/"ro"..., `fonte` o IDA_*
//   de idiomaauto.h e `notificar` 0 se foi a propria pessoa que pediu
//   "Automático". main.c remonta as fileiras e avisa. Pode ser NULL.
// _tick: por quadro, recolhe o locale da webOS, que chega de um fio.
void ajustes_idioma_auto_iniciar(void (*aoMudar)(const char *codigo, int fonte, int notificar));
void ajustes_idioma_auto_tick(void);
void ajustes_log_vazar_tudo(void);   // saida do app: nada pendente fica sem sair
void ajustes_log_vazar(void);   // imprime as rajadas de mudanca paradas (a cada quadro)

// COR DO ANEL DE FOCO, escolhida em "Cor de destaque" ou herdada da conta
// (selected_theme). Um "tema" neste app e so isto: ver TEMA_ACENTO em
// ajustes.c para o motivo de nao ser a paleta inteira. Branco e o padrao, que
// e exatamente o anel que sempre existiu.
void ajustes_acento(float *r, float *g, float *b);
// 0 = tema fixo; CORVIVA_SIMPLES ("Da arte"), _GRADIENTE, _IMERSIVA,
// _TEXTURA ou _TEXTURA_SUTIL: o destaque segue a arte do titulo em cena
// (corviva.h). Sao LOCAIS: nao sobem para a conta e a conta nao os desfaz (ver
// ajustes_aplicar_blob / ajustes_mesclar_blob) — como os seis acentos fixos
// novos de 03/10, que o app web ainda nao tem.
int  ajustes_cor_viva(void);
// "Cor da logo": 1 = com tema dinamico, o destaque sai do logo do titulo.
int  ajustes_cor_logo(void);
// "Interface de vidro" (local, desligada de fabrica): 1 = paineis translucidos
// com borda fina e foco em contorno branco. Cada tela decide o seu desenho;
// o miolo comum esta em gfx_vidro_* (gfx.h).
int  ajustes_vidro(void);
// "Contorno do vidro": 1 = o fio fino dos cartoes/linhas em repouso aparece.
int  ajustes_vidro_contorno(void);
// Ajustes > Conta, "Usar os addons do perfil principal" (padrao ligado): o
// perfil que nao e o principal le os addons do principal. Ver perfis_ativo_addons.
int  ajustes_addons_do_principal(void);
// So para as capturas de teste e o atalho de quem ja sabe: grava como a tela.
void ajustes_definir_vidro(int ligado);
// P2P EXPERIMENTAL (p2p.h). Desligado de fabrica. O endereco (o servidor de
// streaming do Stremio na rede local) e por aparelho, em p2p.txt.
int  ajustes_jellyfin_ligado(void);   // F11: personal servers on AND strict HTTP here
int  ajustes_p2p_ligado(void);
void ajustes_definir_p2p_ligado(int ligado);
// Endereco ja normalizado ("http://192.168.1.5:11470"); "" quando nao ha.
const char *ajustes_p2p_url(void);
// Normaliza e grava; texto vazio esquece. 0 se o texto nao e um endereco (nada
// muda), 1 se gravou.
int  ajustes_definir_p2p_url(const char *texto);
// A MESMA cor mais a TINTA que contrasta com ela (acentos-mockup.html, 03/10):
// 0.071 (#121316) sobre os CLAROS e 1.0 (branco) sobre os PROFUNDOS — a que
// le a 4,5:1 ou mais (tests/acentos.sh). Com Textura e um titulo em cena, a
// tinta do recorte. E a regra de FOCO de layout.h (preenchimento na cor de
// realce, sem anel) em uma chamada, para todo botao usar a mesma conta.
float ajustes_acento_tinta(float *r, float *g, float *b);
// A MESMA TINTA EM 0..255, para quem monta txt_linha: principal (255 ou 18)
// e secundaria (um degrau abaixo: 238 ou 60). Toda superficie pintada de
// ajustes_acento() escreve com estas duas — nunca com um 20 cravado.
int   ajustes_tinta_foco(void);
int   ajustes_tinta_foco2(void);
// A COR DO ACENTO COMO TEXTO/SELO SOBRE O ESCURO (a ilha, o vidro): nos claros
// e o proprio preenchimento; nos profundos, a versao clara do mesmo matiz (L
// 0,84) — o profundo sobre #121316 fica abaixo de 4,5:1 para letra pequena.
// Use em "Ligado", "Só MP4", "4K", pontos de estado: tudo que e COR SOBRE O
// ESCURO, e nao superficie cheia.
void  ajustes_acento_marca(float *r, float *g, float *b);
// O "HDR" de grupo: metade marca, metade cinza (DESIGN.md §3).
void  ajustes_acento_hdr(float *r, float *g, float *b);
// A luz do Frost e da Imersiva: o matiz do acento com L 0,42 e croma <= 0,11.
void  ajustes_acento_luz(float *r, float *g, float *b);
// Uma vez por quadro, depois de corviva_quadro: com Textura, entrega ao gfx a
// textura do titulo em cena (gfx_textura_definir); senao a desliga. O recorte
// FRACO (chapado) e trocado pelo material CC0 de deploy/app/art/texturas/.
void  ajustes_textura_quadro(void);
// A pasta de arte do pacote (main.c passa o dirArte): onde os materiais CC0
// moram. Sem isto o recorte fraco apenas desliga a textura.
void  ajustes_texturas_dir(const char *dirArte);
// "Automática", "4K", "1080p" ou "720p" — o rotulo exibido, para quem seleciona
// a fonte de video mostrar exatamente o que o usuario escolheu.
const char *ajustes_qualidade(void);
// Faixa de tamanho da escolha automatica, em GB (0 = sem limite).
int ajustes_tamanho_max_gb(void);
int ajustes_tamanho_min_gb(void);
float ajustes_espaco_fator(int pct);   // 50..300 % -> 0,5..3,0 (fora da faixa e preso)
float ajustes_espaco_fileiras(void);   // espaco vertical entre fileiras da Home
float ajustes_espaco_titulos(void);    // espaco horizontal entre cartazes
// LIVE TV. Resolucao principal: 0 Automatica, 1 4K, 2 1080p, 3 720p, 4 SD
// (livetv_regras.h converte em altura). Formato do Xtream: 0 Automatico,
// 1 HLS, 2 TS. Espera: 0 = a automatica de cada caminho, senao o prazo em ms.
int   ajustes_livetv_resolucao(void);
int   ajustes_livetv_formato(void);
unsigned ajustes_livetv_espera_ms(void);
// O botao "Aplicar" do diagnostico da Live TV; -1 deixa o valor como esta.
void  ajustes_livetv_aplicar(int resolucao, int formato, int espera);
int   ajustes_pediu_livetv_diag(void);
// Modo do load do player nos canais: 0 A, 1 B, 2 C (video_definir_modo_live).
int   ajustes_livetv_modo(void);
int   ajustes_livetv_proxy(void);
void  ajustes_livetv_aplicar_proxy(int ligado);   // proxy de TS da Live TV (proxyts.c)
void  ajustes_livetv_aplicar_modo(int modo);

// --- LAYOUT: estrutura da home ----------------------------------------------
int   ajustes_rail_recolhida(void);     // collapseSidebar
// #461: 1 = o menu e uma barra no topo (menu.c), sem rail na esquerda. 0 no
// layout Dinamica e no telefone em retrato.
int   ajustes_menu_topo(void);
int   ajustes_abas_estilo(void);   // 0 Chips, 1 Menu suspenso, 2 Barra central, 3 Paginas, 4 Menu no topo
void  ajustes_definir_abas_estilo(int estilo);
int   ajustes_rail_moderna(void);       // modernSidebar
int   ajustes_rail_moderna_blur(void);  // modernSidebarBlur
int   ajustes_hero_ligado(void);        // heroSectionEnabled
int   ajustes_hero_genero(void);        // heroGeneroLocal: genero sob o logo do destaque
int   ajustes_hero_sinopse(void);       // heroSinopseLocal: sinopse do destaque
int   ajustes_hero_notas(void);        // heroNotasLocal: nota do IMDb no destaque
int   ajustes_hero_ano(void);          // heroAnoLocal: ano no destaque
int   ajustes_hero_duracao(void);      // heroDuracaoLocal: duracao no destaque
int   ajustes_hero_sinopse_grande(void); // Normal (22/30) ou Grande (24/32)
int   ajustes_hero_cheio(void);         // modernHeroFullScreenBackdropEnabled
// LAYOUT DA HOME (local): a estrutura da tela inicial. Moderna e o desenho de
// sempre; Padrao contem o destaque num banner; Dinamica e o estilo Apple TV.
enum { HOME_LAYOUT_MODERNA = 0, HOME_LAYOUT_PADRAO = 1, HOME_LAYOUT_DINAMICA = 2,
       HOME_LAYOUT_N = 3 };
int   ajustes_home_layout(void);
int   ajustes_hero_fonte(void);         // origem local da arte do hero (ARTEHERO_*)
// 1 = destaque/detalhe com foto diferente da do card (regra em artehero.h).
int   ajustes_hero_arte_diferente(void);
int   ajustes_ps_fundo(void);            // 0 filmes, 1 listras, 2 arte do perfil, 3 luz, 4 projetor
int   ajustes_ps_fundo_automatico(void); // #90: fundo da escolha de perfil (psfundo.c)
// Teto de memoria para imagens escolhido em Ajustes, em MB; 0 = automatico.
int   ajustes_tex_mb(void);
int   ajustes_posteres_deitados(void);  // modernLandscapePostersEnabled
int   ajustes_gradiente_foco_classico(void); // classicFocusGradientEnabled
// #95: 1 = ao reabrir, fileiras comecam no primeiro tile (ignora home-pos.txt).
// x onde o conteudo comeca. Nao e constante: o recuo e sempre 104 e a rail
// soma os 144 dela quando esta fixa.
float ajustes_conteudo_x(void);
// Ajustes no Glass UI: onde a ilha do relogio fica (em cima da ilha de
// categorias) e se ela cabe agora (sem folha nem modal na frente).
float ajustes_ilha_x(void);
// A tela de addons (addonsui.c) desenhada no arranjo de Ajustes: indice,
// folha com os addons e o inspetor do manifesto do addon em `foco`.
void  ajustes_desenhar_addons(int foco);
// A tela de plugins (pluginsui.c, F09) no mesmo arranjo. nivel 0 = lista
// (liga/desliga, adicionar, repositorios); 1 = scrapers do repositorio `repo`.
typedef struct { int nivel, foco, repo, armado; const char *aviso; } AjPluginsVista;
void  ajustes_desenhar_plugins(const AjPluginsVista *v);
// 2.0.3: a lista de addons (0) ou de plugins (1) fechou: Ajustes reabre na
// categoria dela, com o foco na linha que a abriu.
void  ajustes_voltar_de_lista(int plugins);
// KIT DAS ILHAS (Glass UI) para as telas que saem de Ajustes: diagnostico,
// teste de velocidade e diagnostico da Live TV. Mesmo material e mesmas pecas
// da tela de Ajustes (ajustes_ux_ilha.inc).
void  ajustes_ui_fundo(void);                                  // arte + veu
void  ajustes_ui_arte(int n);   // amostra atras da tela sem catalogo (capturas)
void  ajustes_ui_ilha(GfxRect r, float raioPx, int modal);     // miolo, luz, sombra
void  ajustes_ui_veu(void);                                    // veu dos modais
float ajustes_ui_kicker(const char *s, float x, float y, float a);
float ajustes_ui_meta(const char *k, const char *v, float x, float y, float w);
float ajustes_ui_ef(const char *icone, const char *s, float x, float y, float w);
float ajustes_ui_botao(const char *rot, const char *icone, float x, float y, int foco);
float ajustes_ui_dicas(const char *const *teclas, const char *const *rotulos, int n, float x, float y, int desenha);
void  ajustes_ui_neutro(GfxRect r, float raioPx, float vidA);
void  ajustes_ui_foco_linha(GfxRect r, float raioPx);
float ajustes_ui_antes_depois(const char *rot, int a, int b, const char *unid, int max,
                              float x, float y, float w, int compacto, float cr, float cg, float cb);
void  ajustes_ui_grafico_memoria(float x, float y, float w, float h);
// O mesmo grafico com o historico de exemplo (previa das Novidades da 1.8.0).
void  ajustes_ui_grafico_exemplo(float x, float y, float w, float h);
int   ajustes_relogio_cabe(void);
int   ajustes_relogio_navegavel(void);
int   ajustes_pediu_relogio(void);
int   ajustes_edicao_ativa(void);
// A FAIXA QUE A RAIL FIXA RESERVA na borda esquerda, em px de tela: com ela
// presa, a borda da pilula de icones do menu (Moderna 136, Padrao 88, vezes o
// Tamanho da interface) + 64 de vao - os 104 do recuo do conteudo; 0 recolhida
// ou no layout Dinamica. E a UNICA fonte desse numero: tela nenhuma deve somar
// a largura da rail por conta propria.
float ajustes_rail_largura_fixa(void);
// Area util de uma tela que nasceu desenhada para a tela inteira (#rail fixa,
// 26/09): `padEsq` e `padDir` sao os recuos que ela ja usava (80, 96, 104...).
// Devolve x = rail + padEsq e w = NV_TELA_W - padDir - x. Recolhida, e a conta
// de antes, byte por byte; fixa, o conteudo anda para a direita e ENCOLHE —
// quem tem grade tira colunas a partir de `w`, nao escala nem corta.
void  ajustes_area_conteudo(float padEsq, float padDir, float *x, float *w);

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
// O mesmo, para os tres destinos (AJ_SALVOS_*): a pergunta da ilha (ilhasalvar.c).
void  ajustes_definir_salvos_destino(int destino);
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
int   ajustes_trailer_hero(void);      // trailer no destaque da home
// 0 livre, 1 modo seguro, 2 GPU vetada (Mali-4xx); ver ajustes.c.
int   ajustes_trailer_hero_vetado(void);
// GPU que nao aguenta o trailer automatico da home (Utgard): desliga o do
// destaque e o do cartaz em foco nesta sessao, sem gravar. Uma linha no log.
void  ajustes_trailer_hero_vetar(const char *gpu);
int   ajustes_hero_deslizar(void);     // troca do destaque desliza de lado (senao esmaece)
// ARTE DO ADDON (locais, desligadas de fabrica). 1 = a imagem que o addon
// mandou no meta vence a substituicao do app; sem ela, a fonte de sempre.
//   poster:  vence o provedor de posteres (posterprov_card_addon)
//   fundo:   vence "Background do hero" e "Destaque com outra arte" (artehero)
//   logo:    nao e trocado pelo logo do TMDB ao abrir o titulo (descoberta.c)
//   colecao: pasta do pacote usa capa/fundo/logo da conta (col_arte_conta)
int   ajustes_poster_addon(void);
int   ajustes_fundo_addon(void);
int   ajustes_logo_addon(void);
int   ajustes_col_arte_conta(void);
// Selos da folha de fontes em peca colorida por tipo (#198). Padrao ligado.
int   ajustes_selos_coloridos(void);
int   ajustes_trailer_som(void);         // UM ajuste de som para todo trailer que toca sozinho (Samsung .wgt sempre mudo)
Uint32 ajustes_trailer_hero_espera_ms(void); // repouso no titulo antes do trailer do destaque
int   ajustes_trailer_qualidade(void); // teto em linhas (1080/720/480); 0 = a maior
const char *ajustes_trailer_audio(void);  // "" auto (idioma da UI), "~" original, ou codigo ISO
int   ajustes_trailer_fonte(void);     // TRF_* de trailerfonte.h; 0 = automatico
float ajustes_trailer_zoom(void);      // ampliacao do trailer (1.0 = quadro inteiro)
void  ajustes_definir_envio_auto(int ligado);
// N3: "Receber enquetes" (padrao ligado). O espelho local do opt-out da conta:
// enquete.c grava aqui o que o servidor disse, sem refazer o pedido.
int   ajustes_enquetes(void);
void  ajustes_espelhar_enquetes(int ligado);
// Fonte do destaque (ARTEHERO_*) e "Destaque com outra arte", gravados na hora.
void  ajustes_definir_destaque(int fonte, int diferente);
// homeImdbRatingsVisibility: 0 SHOW_ALL, 1 HIDE_ALL
int   ajustes_notas_home(void);
// discoverLocation: 0 in_search, 1 in_sidebar, 2 off
int   ajustes_local_descobrir(void);
int   ajustes_descobrir_na_busca(void); // searchDiscoverEnabled (derivado)

// --- LAYOUT: continuar assistindo -------------------------------------------
// "Resolucao da interface" (RES_* em resolucao.h; padrao Automatica = 1080p).
// ajustes_4k: 1 = a pessoa escolheu 4K: pedir uma superficie 3840x2160 na
// criacao da janela. A TV pode ignorar — a linha `janela=... drawable=...` do
// log diz o que ela respondeu — e se conceder e nao aguentar, main.c recua
// para 1080p (resolucao.h). Ver a nota em main.c.
int   ajustes_4k(void);
// 1 = Automatica: 1080p, sondando o 4K onde a tela for 4K (resolucao.h, RES_AUTO_*).
int   ajustes_res_auto(void);
// 1 = a pessoa escolheu 720p: desenhar em 1280x720 e ampliar (gpun_forcar_720).
// Nunca automatico.
int   ajustes_720p(void);
// Ilha do relogio (ilha.h). _ligado: 0 = sem pilula em repouso (os avisos
// continuam saindo dela). _pos: 0 automatica, 1 esquerda, 2 direita.
int   ajustes_relogio_ligado(void);
int   ajustes_relogio_pos(void);
int   ajustes_relogio_12h(void);   // 1 = 12 h com AM/PM (relogio.h)
// Local interface factor: 0.8..1.5, or automatic from physical panel size.
// #477: synopsis typography follows the local Synopsis size (Normal/Grande).
int ajustes_sinopse_tamanho(void);
// #362: default for collections without an explicit view, active local profile.
// Main UI thread: validated VtLayout ID (vertudo.h); 0 keeps automatic behavior.
int ajustes_colecao_exibicao(void);
float ajustes_tamanho_ui(void);
// Settings only: 0.8/0.9/1.0, default 0.9; independent of global UI zoom.
float ajustes_tamanho_ajustes(void);
// #339: 1 = Settings as a single stacked list (Ajustes › Aparência › Layout dos Ajustes). LOCAL.
int   ajustes_layout_lista(void);
int   ajustes_esconder_logo_trailer(void);   // 1 = hide the corner title logo while a trailer plays
// R4: second subtitle placement and own style (local). junto: 1 = stacked right above the
// primary at the bottom, 0 = top band. tamanho: percent (50..300), 0 = automatic (90% of the
// primary). cor/fundo/borda: the same indices as the primary style (VideoLegendaEstilo), -1 = same as primary.
int   ajustes_legendas_todas(void); // #425: abrir todas as faixas
// #205: 1 = tela de abertura Detalhada (o cartao com fonte e passos); 0 = Cinema.
int   ajustes_abertura_detalhada(void);
// #205: 1 = mostra o nome do add-on na tela Cinema da abertura (padrao: 0).
int   ajustes_abertura_addon(void);
// #508: fundo da tela Cinema da abertura: 0 = Desligado, 1 = Arte (nitida com
// veu escuro), 2 = Arte borrada (padrao).
int   ajustes_abertura_fundo(void);
int   ajustes_titulo_fil(void);        // 0 Classico, 1 Ilha, 2 Editorial, 3 Acento
int   ajustes_abertura_estilo(void);   // 0 Centro (A3), 1 Canto (B3)
int   ajustes_fundo_din(void);         // #205: fundo da Dinamica, 0 Cinza (padrao), 1 Cor, 2 Imersivo
void  ajustes_fundo_din_cor(float rgb[3]);   // #205: a cor escolhida para o fundo "Cor" (sRGB 0..1)
// #205: tamanho dos cards de episodio (Moderna): 0 pequeno, 1 medio (padrao), 2 grande.
int   ajustes_menu_cartaz_simples(void); // 0 Full (default), 1 Simple; this TV only.
int   ajustes_mural_titulos(void);      // #521: 1 = nome dentro do ladrilho do Mural
int   ajustes_ep_tamanho(void);
// #338: tamanho de abertura das Estatisticas ao vivo (EV_TAM_*); -1 = o ultimo usado.
int   ajustes_estat_tamanho(void);
int   ajustes_estat_lado(void);                 // 0 Direita, 1 Esquerda
void  ajustes_definir_estat_lado(int esquerda);
int   ajustes_leg2_junto(void);
int   ajustes_leg2_tamanho(void);
int   ajustes_leg2_cor(void);
int   ajustes_leg2_fundo(void);
int   ajustes_leg2_borda(void);
void  ajustes_leg_contorno_rgb(int *r, int *g, int *b);   // #475: cor do contorno/sombra da legenda (0..255)
unsigned ajustes_leg_borda_rgb(int corBorda);   // corBorda >= 0: explicit colour; -1: legacy #475 colour
// Independent secondary overrides; absent fields inherit the main style.
int ajustes_leg2_fonte_principal(void);
void ajustes_leg2_estilo(const VideoLegendaEstilo *principal, VideoLegendaEstilo *out);
int ajustes_leg2_posicao(void); // -1 keeps the existing Top/Stacked placement
int ajustes_leg2_atraso(void);
int ajustes_leg2_definir_atraso(int ms);
int   ajustes_legenda_forcada_auto(void);    // #287: 1 = audio in the subtitle language -> only the forced track (local, default on)
int   ajustes_legenda_sync_auto(void);       // local, ligada por padrão
int   ajustes_legenda_sync_audio(void);      // 1 = offer "Por audio" in subtitle AutoSync (F06; local, default off)
int   ajustes_trailer_zoom_tpk(void);        // #241: 1 = experimental trailer zoom on the native .tpk (local, default off)
int   ajustes_teclado_na_tela(void);         // times the Teclado option is on the Ajustes list (0 = hidden here)
int   ajustes_teclado_android(void);          // Android keyboard: 0 automatic, 1 app grid, 2 system IME (local)
int   ajustes_mic_busca(void);               // Samsung (.tpk/.wgt) search mic button: 0 automatic, 1 on, 2 off (local)
unsigned ajustes_p2p_limite_mb(void);           // #334: P2P space limit in MB (0 = Automatic)
int   ajustes_p2p_usb(void);                    // #205: 1 = cache P2P em pendrive USB
int   ajustes_player_externo(int anime);     // #501/#205: 0 Interno, 1 Externo, 2 Perguntar, 3 app (pacote em ajustes_player_pacote); anime: -1 = Igual ao Player
const char *ajustes_player_pacote(int anime); // #205: pacote do app escolhido ("" = nenhum)
int   ajustes_buffer_preset(void);           // #338: 0 Automatico, 1 = 64, 2 = 128, 3 = 256 MiB
int   ajustes_cache_seek_mb(void);           // F07: seek cache limit in MB for the next video (0 = off / not on this TV)
#ifdef AJUSTES_TESTE
void  ajustes_teste_escala(int percentual); // fixture only; does not persist
#endif
// Fundo atras dos paineis (Aparencia › Fundo): 0 Arte, 1 Arte borrada, 2 Frost.
int   ajustes_fundo(void);
float ajustes_vidro_opacidade(void);   // 0,60..0,92; 0,78 = o vidro de sempre
int   ajustes_vidro_fosco(void);
int   ajustes_vidro_qualidade(void);   // 0 Automatica (frost + governor), 1 Desempenho (smoked glass, no blur), 2 Alta (frost)
void  ajustes_teste_vidro_env(void);   // so capturas        // 1 = arte borrada atras do vidro
int   ajustes_icone_app(void);
// 2.0 (N1): 0 = Novo (default), 1 = Classico. Local to this TV.
int   ajustes_logo_app(void);
// 0 = Padrao, 1 = So esmaece, 2 = Direto. Local to this TV.
int   ajustes_abertura(void);
// Sair do player no meio vai para a HOME, minimizando o titulo na ilha (o
// relogio ligado e Ao sair do player = home). 0 = a pagina do titulo, como antes.
int   ajustes_saida_player_home(void);
// "Manter o video pronto ao sair" (Avancado, padrao Desligado): o player pode
// reter a sessao pausada ao sair para a ilha (player.c, PLR_RETIDO_MS). So com
// a saida para a home valendo; sempre 0 no perfil seguro.
int   ajustes_manter_video(void);
int   ajustes_retomada_morna(void);
unsigned ajustes_manter_video_ms(void); // 0 = unlimited
// Selo de visto no cartaz da home (#212). 1 = ligado (o de fabrica).
int   ajustes_selo_visto(void);
int   ajustes_selo_visto_modo(void);   // 0 desligado, 1 icone (check), 2 escrito
// #366: oculta filmes vistos so na descoberta, por perfil neste aparelho.
// Desligado de fabrica; busca, biblioteca e historico nao sao filtrados.
int   ajustes_ocultar_filmes_vistos(void);
void  ajustes_definir_ocultar_filmes_vistos(int ligado);
// MODO SEGURO (seguro.h). Chamar no arranque, DEPOIS de ajustes_dir e de
// avisos_iniciar: `caiu` = a sessao anterior nao se despediu. Desfaz o ajuste
// arriscado que estava em prova, liga o perfil seguro se as quedas se repetem e
// avisa a pessoa. Sem efeito nas builds de teste (nada em prova, nada a avisar).
void  ajustes_seguro_iniciar(int caiu);
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
int   ajustes_cw_proximo(void);   // 1 = 'a seguir' entra em Continuar assistindo (padrao)
int   ajustes_cw_mostrar_nao_exibidos(void);  // showUnairedNextUp
// continueWatchingSortMode: 0 default, 1 streaming_style, 2 split_upcoming
int   ajustes_cw_ordem(void);
int   ajustes_dts_ac3(void);             // 1 = DTS convertido em AC3 5.1 (LG, e padrao no .tpk); 0 = AAC estereo (.tpk: ou desligado)
int   ajustes_dts_tpk_modo(void);        // .tpk: DTSTPK_HLS, ou DTSTPK_DESLIGADO com a conversao desligada
int   ajustes_ps_continuar(void);        // #303: 0 = sem o cartao "continuar" na escolha de perfil
int   ajustes_hist_conta(void);          // 1 = this profile's history goes to the Nuvio account (per profile, default on)
int   ajustes_amigos_estilo(void); // 0 Feed, 1 Live, 2 Stories, 3 Presence, 4 Poster, 5 Classic
int   ajustes_social(void);              // 1 = friends/social features on (per profile, default on)
int   ajustes_dv_mkv(void);              // 1 = Dolby Vision MKV through our demux (webOS, local, default off)
int   ajustes_cw_retido_tambem(void);   // 1 = o titulo da ilha/Retomar tambem fica em Continuar assistindo
// Percentual (70-98, padrao 90) a partir do qual o episodio conta como
// assistido: sai do Continuar assistindo e o card passa ao proximo. Local.
int   ajustes_cw_concluido(void);
int   ajustes_cw_minimo(void);    // 1..20 %: piso do Continuar assistindo (#326); 1 = como sempre

// --- LAYOUT: pagina de detalhe (efeito vive em detail.c) ---------------------
int   ajustes_desfocar_nao_assistidos(void); // blurUnwatchedEpisodes
int   ajustes_botao_trailer(void);           // detailPageTrailerButtonEnabled
int   ajustes_meta_externo(void);            // preferExternalMetaAddonDetail
// "Usar sempre o Cinemeta": 1 = a ficha e os episodios vem so do Cinemeta (como
// antes); 0 (padrao) = catalogo primeiro, ver descoberta.c (metaCatalogo).
int   ajustes_busca_nuvio(void);      // 0 = Primeiro (padrao), 1 = Por ultimo, 2 = Desligado (#311)
int   ajustes_busca_origem(void);     // 0 = sem "de <fonte>" nos grupos da busca (#311)
int   ajustes_busca_cinemeta(void);   // 0 = Cinemeta fora da busca (#231)
int   ajustes_meta_so_cinemeta(void);

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
// Pais da grade do Guia de TV (#158): "" = automatico, senao "RO", "BR"...
const char *ajustes_epg_pais(void);
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
// Miniaturas do Seekr na barra de tempo: ajuste ligado E chave definida.
int         ajustes_seekr_ligado(void);
int         ajustes_seekr_habilitado(void); // opcao local, inclusive sem chave
int         ajustes_seekr_fita(void);       // anterior/atual/seguinte
int         ajustes_seekr_ajuste_s(void);   // sincronia, em segundos (-60..60)
// `fonte` e um ExFonte de extras.h (trakt, imdb, tmdb, tomatoes, audience,
// metacritic, letterboxd). 0 = esconder a nota dessa fonte na fileira.
int         ajustes_mdblist_fonte(int fonte);   // mdblist_show_*
// A fonte (ExFonte) entra na linha do titulo? Escolha local + disponibilidade.
int         ajustes_nota_titulo(int fonte);

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
// EXCECAO (#378): os idiomas de legenda, legenda secundaria e audio da conta.
// Eles nao sao opcao desta tabela, sao a parte "da conta" de linguas.c, e o
// blob e o retrato inteiro dela: chave ausente = a conta nao tem idioma (sem
// isso, os do perfil anterior ficavam valendo). A escolha local nao e tocada.
int ajustes_aplicar_blob(const char *json);
// #378: so os idiomas de legenda/audio da conta, para o blob que chega com os
// ajustes locais protegidos (sync.c nao o aplica). Nao mexe em escolha local.
// NULL esquece os idiomas da conta (troca de perfil/conta).
void ajustes_idiomas_da_conta(const char *json);

// AJUSTES POR PERFIL NESTA TV (ajustes-p<N>.txt). _guardar grava os ajustes que
// sao do perfil (os mesmos que a conta guarda; nunca os deste aparelho).
// _restaurar os traz de volta e devolve 1 quando havia copia, 0 quando o perfil
// nunca foi usado nesta TV. _esquecer apaga todas as copias (logout). Quem
// decide quando cada uma roda e sync_trocar_perfil.
void ajustes_perfil_guardar(int perfil);
int  ajustes_perfil_restaurar(int perfil);
void ajustes_perfil_esquecer(void);

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
// #187: uma linha "[tmdb] idioma dos metadados: ..." com o que a TV pede ao
// TMDB, de onde vem (ajuste desta TV) e o tmdb_language cru da conta.
void ajustes_tmdb_idioma_relatar(const char *blob);

// CENTRAL DE CONTROLE (central.h): uma opcao de escolha curta pela chave do
// ajustes.txt ("dolbyVision"). _op: -1 se a chave nao existe, nao e escolha de
// 2 a 6 valores ou nao esta na tela deste build. _rotulo/_valor: texto cru em
// portugues (text.c traduz ao desenhar). _ligado: 1/0 num interruptor, -1 nos
// outros. _passo: proximo (dir 1) ou anterior (-1) valor, gravado e com os
// mesmos efeitos do OK na tela de Ajustes; 0 se nao gravou.
// "corDestaque" nao e uma opcao de 2 a 6 valores: o op e o AJ_RAPIDO_COR e a
// central abre o seletor de cores (ajustes_rapido_cor_*, logo abaixo).
#define AJ_RAPIDO_COR 9000
int         ajustes_rapido_op(const char *chave);
const char *ajustes_rapido_rotulo(int op);
const char *ajustes_rapido_valor(int op);
int         ajustes_rapido_ligado(int op);
int         ajustes_rapido_passo(int op, int dir);
// A COR DE DESTAQUE (seletor da central): os 18 acentos fixos na ordem da
// regua de Ajustes (0-8 claros, 9-17 profundos). `cor` devolve a tinta que
// contrasta com o preenchimento (como ajustes_acento_tinta); `atual` = -1 com
// um acento dinamico; `definir` grava como a tela de Ajustes (0 = falhou).
int         ajustes_rapido_cor_n(void);
const char *ajustes_rapido_cor_nome(int i);
float       ajustes_rapido_cor(int i, float *r, float *g, float *b);
int         ajustes_rapido_cor_atual(void);
const char *ajustes_rapido_cor_atual_nome(void);
int         ajustes_rapido_cor_definir(int i);

// Contas nativas compartilhadas (legado) ou uma copia separada por conta+perfil.
int  ajustes_provedores_compartilhados(void);
int  ajustes_definir_provedores_compartilhados(int compartilhados);
void ajustes_provedores_abrir_sessao(void);
int  ajustes_provedores_arquivo(char *dst, unsigned tam, const char *base);
void ajustes_perfil_padrao(void);

#ifdef NV_SHOT_HOOKS
int ajustes_shot_valor(const char *chave, int v);   // capturas: opcao pela chave
#endif

#endif
