// Ajustes por intencao: categorias, blocos e avancados da sessao.
// Todas as preferencias abrem um seletor com rascunho; OK confirma e Voltar
// cancela. A lista mostra valor e chevron, a ficha explica alcance e padrao.
// IDs, chaves persistidas e indices de valores continuam os legados.
#include "ajustes.h"
#include "ajlog.h"
#include "horafmt.h"
#include "ajustes_ux.h"
#include "trailerfonte.h"   // NV_TRAILER_CONTINUA_DETALHE, nas ajudas do trailer
#include "dados.h"
#include "resolucao.h"
#include "gpunivel.h"
#include "enquete.h"
#include "stalker.h"
#include "xtream.h"
#include "xtepg.h"
#include "teclado.h"
#include "descoberta.h"
#include "extras.h"
#include "fileiras.h"
#include "listas.h"
#include "idioma.h"
#include "idiomacod.h"
#include "idiomaauto.h"
#include "linguas.h"
#include "addons.h"
#include "plugins.h"
#include "gfx.h"
#include "escala.h"
#include "text.h"
#include "tex_cache.h"
#include "badges.h"
#include "anim.h"
#include "layout.h"
#include "sessao.h"
#include "sync.h"
#include "perfis.h"
#include "traktauth.h"
#include "simklauth.h"
#include "discord.h"
#include "simkl.h"
#include "qr.h"
#include "atualizacao.h"
#include "avisos.h"
#include "registro.h"
#include "seguro.h"
#include "botoes.h"
#include "ponteiro.h"
#include "addonsui.h"
#include "pluginsui.h"
#include <math.h>
#include <time.h>
#include "js.h"
#include "artehero.h"
#include "jellyfin.h"
#include "plex.h"
#include "artereserva.h"
#include "corviva.h"
#include "fundo.h"
#include "p2p.h"
#include "p2pmotor.h"
#include "pessoas.h"
#include "recomenda.h"
#include "posterprov.h"
#include "rede.h"
#include "debrid.h"
#include "seekr.h"
#include "selospacote.h"
#include "fonteregra.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iconeapp.h"
#include "logoapp.h"
#include "abertura.h"
#include "apoio.h"

// Settings has its own canvas and scale, independent of the global UI zoom.
// Layout, text measurement, drawing and pointer targets share this factor.
#undef NV_VTELA_W
#undef NV_VTELA_H
#define NV_VTELA_W (1920.0f / ajustes_tamanho_ajustes())
#define NV_VTELA_H (1080.0f / ajustes_tamanho_ajustes())
#define AJ_ESCALA_INI() float ajEscalaAnt_ = gfx_escala(); gfx_escala_sair(ajustes_tamanho_ajustes())
#define AJ_ESCALA_FIM() gfx_escala_sair(ajEscalaAnt_)

// Versao do app: vem do build (-DNV_VERSAO, que tools/env.sh le do
// appinfo.json). Era um literal aqui e ficou parado em 1.0.44 por nove
// releases — a tela de Ajustes mentia a versao. "dev" so aparece numa
// compilacao a mao, sem o env.sh.
#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
#define AJ_VERSAO       NV_VERSAO

// TEXTO SOBRE A COR DE REALCE: BRANCO, a nao ser que o realce seja branco (ou
// quase). Regra do dono (21/09/2026): "se nao for branco o accent, a cor de
// texto tem que ser branca". Antes era 20 cravado, do tempo em que o realce
// era sempre branco; com o rosa o texto escuro ficava sujo. A medida mora em
// ajustes_acento_tinta, que todos os modulos usam.
#define AJ_TEXTO_ESCURO  (tintaFoco())
#define AJ_TEXTO_ESCURO2 (tintaFoco() > 128 ? 232 : 50)   // valor, um degrau abaixo
// Ajustes usa superficies opacas: contraste vem do acento, inclusive com Vidro.
static int tintaFoco(void) { return ajustes_acento_tinta(NULL, NULL, NULL) > 0.5f ? 255 : 20; }
static int focoEscuro(void) { return tintaFoco() < 128; }   // superficie do foco e clara?
// PILULA SOLIDA NO ACENTO (coluna de categorias, abas e botoes da folha de
// fileiras): ela e opaca COM e SEM vidro, entao o atalho do vidro acima nao
// vale — com acento branco/Dinamica (que nasce branco) e vidro ligado, o texto
// saia branco sobre branco (#202). Aqui manda so a tinta por contraste.
#define AJ_TEXTO_SOLIDO  (ajustes_tinta_foco())
#define AJ_LINHA_H       80.0f
#define AJ_LINHA_GAP      8.0f
// Cabecalho da categoria no topo da lista: titulo + subtitulo, o
// `settings-content-header` do web. 38 do headline + 12 + 22 do caption, e o
// respiro ate a primeira linha.
#define AJ_SEC_CABEC    112.0f
// Rotulo de BLOCO no meio da categoria (ROT em TELA), sem foco.
#define AJ_SUB_CABEC     46.0f
// GRUPO RECOLHIVEL: linha de duas alturas, titulo e descricao — a mesma
// composicao das linhas de addon do guia (TXT_BODY em cima, TXT_CAPTION a 36
// px), porque a descricao e o que diz o que ha dentro antes de abrir.
#define AJ_GRUPO_H      104.0f
// As opcoes de um grupo aberto entram RECUADAS: sem o recuo, a primeira linha
// do grupo e a primeira do grupo seguinte leem como a mesma lista.
#define AJ_RECUO         40.0f
// Nao e constante: acompanha a rail, como todo o resto do conteudo. Com a
// barra recolhida a lista tambem comeca em 104 — deixar 248 cravado aqui fazia
// a tela de Ajustes ser a unica desalinhada das outras.
// COLUNA DE CATEGORIAS a esquerda da lista. Ela existe por um motivo de
// controle, nao de estetica: o unico atalho entre secoes era PgUp/PgDn, e NAO
// EXISTE PgUp nem PgDn num controle de TV. Aqui as categorias recebem foco de
// verdade, alcancadas pela tecla que todo controle tem — Voltar, ou a seta
// para a esquerda.
//
// 280 de largura e 76 de altura: sao NOVE categorias (as do web), e o nome da
// categoria tem de caber INTEIRO — ate a 1.4.4 a coluna mostrava um apelido
// ("Retomar", "Cartazes") e o cabecalho outro nome ("Continuar assistindo",
// "Pôsteres e cards"), duas palavras para o mesmo lugar. Agora o nome da
// coluna e o titulo da lista sao a mesma string. A lista perde 40 px e o
// painel de ajuda fica exatamente onde estava (1320).
#define AJ_IDX_X        ajustes_conteudo_x()
#define AJ_IDX_W        310.0f
#define AJ_IDX_GAP       48.0f
#define AJ_IDX_H         52.0f
#define AJ_IDX_ICONE     32.0f    // lado do icone dentro da linha da categoria
#define AJ_LISTA_X      (AJ_IDX_X + AJ_IDX_W + AJ_IDX_GAP)
#define AJ_LISTA_W      866.0f
#define AJ_PAD           24.0f    // borda da linha ao texto
// A janela da LISTA dentro da folha (Glass UI): abaixo do cabecalho da folha
// (kicker, titulo e sub) ate 18 px da base da ilha.
// A3 (04/10): o cabecalho compacto da lista (titulo da categoria + chip
// Avancados, 96) e o rodape proprio (dicas e o aviso "Ajuste salvo", 72) que o
// aviso nao cubra mais a ultima linha.
#define AJ_TOPO        (112.0f / ajustes_tamanho_ajustes() + AJ_CAB_PAGINA + AJ_A3_CAB)
// 2.0.2: o cabecalho da pagina da categoria (o cartao da grade crescido), acima
// da arte e da lista; o titulo saiu de dentro da ilha da lista.
#define AJ_CAB_PAGINA  136.0f
#define AJ_BASE        (NV_VTELA_H - 40.0f / ajustes_tamanho_ajustes() - AJ_A3_RODAPE)
#define AJ_A3_CAB       28.0f
#define AJ_A3_RODAPE    72.0f
// Raio da linha em fracao do menor lado (o SDF do shader e normalizado):
// 12px sobre 88 de altura.
#define AJ_RAIO           0.14f

// Ordem do enum = ordem no arquivo de chaves e nas tabelas. Acrescentar no MEIO
// e seguro: o arquivo e por chave, nao posicional (ver ajustes_dir). NAO e a
// ordem da TELA: onde cada opcao aparece e decidido em TELA[], e os
// comentarios de grupo abaixo sao os da versao em que o enum era a tela.
typedef enum {
  // Reproducao
  AJ_QUALIDADE, AJ_DV, AJ_ATMOS, AJ_LEG_LINGUA, AJ_AUD_LINGUA,
  AJ_PAUSA_OVERLAY, AJ_FONTE_MANUAL, AJ_FONTE_AUTO, AJ_FONTE_REPOR, AJ_FONTE_TEXTO,
  // Layout da Home
  AJ_LANDSCAPE, AJ_HERO_CHEIO, AJ_HERO_FUNDO, AJ_HERO_ARTE_DIF, AJ_HERO_TRAILER,
  // Fileiras da Home
  AJ_FIL_LIMITE, AJ_FIL_ORDEM,
  // Conteudo da Home
  AJ_RAIL, AJ_RAIL_MODERNA, AJ_RAIL_BLUR, AJ_HERO, AJ_HERO_CATALOGOS,
  AJ_PS_FUNDO,
  AJ_DESCOBRIR, AJ_ROTULOS, AJ_NOME_ADDON, AJ_SUFIXO_TIPO,
  AJ_OCULTAR_NLANC, AJ_NOTAS_HOME, AJ_GRAD_CLASSICO,
  // Continuar assistindo
  AJ_CW_LIGADO, AJ_CW_OK, AJ_CW_FONTE, AJ_CW_ESTILO, AJ_CW_THUMB, AJ_CW_BLUR_PROX,
  AJ_CW_FURTHEST, AJ_CW_NAO_EXIBIDOS, AJ_CW_ORDEM,
  // Pagina de detalhe
  AJ_DET_BLUR_NAO_VISTOS, AJ_DET_TRAILER, AJ_DET_META_EXT, AJ_DET_DATA_CHEIA,
  AJ_DET_VEU, AJ_DET_TRAILER_AUTO, AJ_TRAILER_QUAL, AJ_TRAILER_ASPECTO,
  AJ_TRAILER_FONTE,
  // Foco no poster
  AJ_EXPANDIR, AJ_EXPANDIR_ATRASO, AJ_NAV_RAPIDA, AJ_BORDA_FOCO,
  // Profundidade
  AJ_PROF, AJ_PROF_BORDA, AJ_PROF_BRILHO, AJ_PROF_COBERTURA,
  AJ_PROF_POSTERS, AJ_PROF_CW, AJ_PROF_EPS, AJ_PROF_ELENCO, AJ_PROF_TRAILERS,
  // Tamanho do item
  AJ_LARGURA_DP, AJ_RAIO_DP, AJ_QUALIDADE_IMG,
  // Interface
  AJ_IDIOMA, AJ_ANIM, AJ_RESOLUCAO, AJ_TEMA, AJ_COR_LOGO,
  // Conta
  AJ_PERFIL_ATIVO, AJ_SYNC, AJ_ADDONS,
  AJ_STALKER_PORTAL, AJ_STALKER_MAC, AJ_STALKER_LIMPAR,
  AJ_XTREAM_SERVIDOR, AJ_XTREAM_USUARIO, AJ_XTREAM_SENHA, AJ_XTREAM_LIMPAR,
  AJ_XTREAM_CONTA, AJ_EPG_PAIS,
  AJ_SALVOS_DEST, AJ_TRAKT, AJ_SIMKL, AJ_SAIR,
  // Sobre
  AJ_VERSAO_I, AJ_ATUALIZAR, AJ_ENVIAR_LOG, AJ_ENVIO_AUTO, AJ_ESPACO, AJ_TEX_MB,
  // Integracoes — TMDB (tmdb_settings do blob da conta, ver
  // profileSettingsSyncService.js do web)
  AJ_TMDB_LIGADO, AJ_TMDB_IDIOMA, AJ_TMDB_ARTE, AJ_TMDB_BASICO, AJ_TMDB_FICHA,
  AJ_TMDB_DATAS,
  AJ_TMDB_ELENCO, AJ_TMDB_PROD, AJ_TMDB_REDES, AJ_TMDB_EPS, AJ_TMDB_TRAILERS,
  AJ_TMDB_MAIS, AJ_TMDB_COL, AJ_TMDB_CW,
  // Integracoes — MDBList (mdblist_settings do blob)
  AJ_MDB_LIGADO, AJ_MDB_CHAVE, AJ_MDB_TRAKT, AJ_MDB_IMDB, AJ_MDB_TMDB,
  AJ_MDB_LETTER, AJ_MDB_TOMATES, AJ_MDB_AUDIENCIA, AJ_MDB_META, AJ_MDB_MAL,
  // Integracoes — fanart.tv (fonte do destaque, so com chave pessoal)
  AJ_FANART_CHAVE,
  AJ_DIAGNOSTICO,
  // ATALHO DO TESTE DE VELOCIDADE, logo abaixo do diagnostico (dono,
  // 25/09/2026): a mesma tela de diagnostico, aberta ja no teste — o botao
  // "Teste de velocidade" de dentro dela exigia passar pela escolha do
  // objetivo so para chegar la.
  AJ_VELOCIDADE,
  // ITENS DA BARRA LATERAL QUE SE ESCONDEM (#162: "nao uso Trakt nem TV ao
  // vivo, nao quero Perfil e Stats nem o Guia"). NO FIM do enum de proposito:
  // valor[] e CHAVE sao posicionais, e inserir no meio deslocaria o padrao de
  // tudo o que vem depois (o defeito do #149).
  AJ_MENU_EXPLORAR, AJ_MENU_GUIA, AJ_MENU_AGENDA, AJ_MENU_PERFIL,
  // Trailer do cartaz em foco no destaque (#124). No fim pelo mesmo motivo.
  AJ_FOCO_TRAILER,
  // Itens por fileira da Home (#163). No fim pelo mesmo motivo.
  AJ_ITENS_FILEIRA,
  // A fonte da interface é independente da família das legendas.
  AJ_FONTE_UI,
  // Efeitos visuais do .tpk (#180): automatico / completos / leves. No fim
  // pelo mesmo motivo; so aparece na tela do .tpk.
  AJ_GPU_EFEITOS,
  // Interface de vidro: visual translucido, so desta TV. No fim pelo mesmo
  // motivo dos outros: valor[] e CHAVE[] sao posicionais.
  AJ_VIDRO,
  // P2P experimental (p2p.h). No fim pelo mesmo motivo: valor[] e CHAVE[] sao
  // posicionais.
  AJ_P2P_LIGADO, AJ_P2P_URL, AJ_P2P_TESTAR,
  // Posteres personalizados (posterprov.h). No fim pelo mesmo motivo.
  AJ_POSTER_PROV, AJ_POSTER_INST, AJ_POSTER_TOKEN, AJ_POSTER_EXTRA,
  AJ_POSTER_CHAVE, AJ_POSTER_MODELO, AJ_POSTER_TESTAR,
  // Layout da home (Moderna / Padrao / Dinamica). No fim pelo mesmo motivo.
  AJ_HOME_LAYOUT,
  // Chaves de debrid digitadas nesta TV (debrid.h). No fim pelo mesmo motivo.
  // AllDebrid primeiro: e o unico que a conta nao traz.
  AJ_DEBRID_AD, AJ_DEBRID_AD_TESTAR, AJ_DEBRID_RD, AJ_DEBRID_TB, AJ_DEBRID_PM,
  // Ficha do titulo: catalogo primeiro (descoberta.c, metaCatalogo). Ligado =
  // "Usar sempre o Cinemeta" (o comportamento de antes). No fim pelo mesmo
  // motivo: valor[] e CHAVE[] sao posicionais.
  AJ_DET_SO_CINEMETA,
  // Quais notas aparecem na LINHA DO TITULO (hero da pagina de detalhe). Na
  // ordem em que a linha as desenha (notasfontes.c: nf_posicao). LOCAIS: o app
  // oficial nao tem esta linha, entao nao ha campo dela na conta. No fim pelo
  // mesmo motivo dos outros: valor[] e CHAVE[] sao posicionais.
  AJ_NT_IMDB, AJ_NT_TOMATES, AJ_NT_AUDIENCIA, AJ_NT_META, AJ_NT_METAUSER,
  AJ_NT_TRAKT, AJ_NT_TMDB, AJ_NT_LETTER, AJ_NT_MAL, AJ_NT_EBERT, AJ_NT_SCORE,
  // Entre amigos alem do Trakt (pessoas.h): "Perfil pesquisavel" e o editor do
  // perfil publico. No fim pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_PERFIL_PESQ, AJ_PERFIL_EDITAR,
  // Fio dos cartoes e linhas em repouso no vidro (gfx_vidro_aro). No fim pelo
  // mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_VIDRO_CONTORNO,
  // Som e espera do trailer no destaque da home (home_trailer_passo). No fim
  // pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_HERO_TRAILER_SOM, AJ_HERO_TRAILER_ESPERA,
  // A partir de quantos por cento o episodio conta como ASSISTIDO e o
  // Continuar assistindo passa ao proximo. No fim pelo mesmo motivo: valor[] e
  // CHAVE[] sao posicionais.
  AJ_CW_CONCLUIDO,
  // Perfil que nao e o principal le os addons do principal (perfis_ativo_addons).
  // No fim pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_ADDONS_PRINCIPAL,
  // Como o destaque troca de titulo: deslizando de lado ou esmaecendo
  // (home.c, heroDesliza). No fim pelo mesmo motivo: valor[] e CHAVE[] sao
  // posicionais.
  AJ_HERO_TRANSICAO,
  // ARTE DO ADDON (grupo "Arte do addon" no Layout): usar a imagem que o
  // proprio addon manda no meta (poster, background, logo) e a arte das pastas
  // da conta, antes das substituicoes do app. Todas desligadas de fabrica = o
  // visual de sempre. No fim pelo mesmo motivo: valor[] e CHAVE[] sao
  // posicionais.
  AJ_ADDON_POSTER, AJ_ADDON_FUNDO, AJ_ADDON_LOGO, AJ_COL_ARTE_CONTA,
  // Selos coloridos na folha de fontes (#198, badges_desenhar_selos). No fim
  // pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_SELOS_CORES,
  // Som do trailer automatico da pagina do titulo (detail.c). No fim pelo mesmo
  // motivo: valor[] e CHAVE[] sao posicionais.
  AJ_DET_TRAILER_SOM,
  // LIVE TV (grupo "Live TV" em Conteudo): resolucao preferida das fontes de
  // canal, formato do Xtream, espera para abrir e o diagnostico da Live TV
  // (livetvdiag.c). No fim pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_LIVETV_RES, AJ_LIVETV_FORMATO, AJ_LIVETV_ESPERA, AJ_LIVETV_DIAG,
  // Modo do load do player nos canais (video_definir_modo_live, #158). No fim
  // pelo mesmo motivo.
  AJ_LIVETV_MODO,
  // Proxy de TS da Live TV (proxyts.c, #158). No fim pelo mesmo motivo.
  AJ_LIVETV_PROXY,
  // Ilha do relogio (ilha.h): mostrar a pilula do relogio em repouso e em que
  // canto. LOCAIS. No fim pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_RELOGIO, AJ_RELOGIO_POS,
  // Selo de visto no canto do cartaz (home.c, cat_visto, #212). LOCAL. No fim
  // pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_SELO_VISTO,
  // SEEKR (seekr.h): miniatura da barra de tempo. Ligado e LOCAL; a chave mora
  // em seekr.txt (dados), por aparelho, como a do fanart.tv; o teste e acao.
  // No fim pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_SEEKR_LIGADO, AJ_SEEKR_CHAVE, AJ_SEEKR_TESTAR,
  // Fita (anterior/atual/seguinte) e sincronia da miniatura do Seekr. LOCAIS.
  // No fim pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_SEEKR_FITA, AJ_SEEKR_AJUSTE,
  // Ao sair do player no meio: a home com o titulo minimizado na ilha (o
  // "quadro do video" encolhe ate a mini capa da pilula) ou a pagina do titulo.
  // So vale com o relogio ligado. LOCAL. No fim pelo mesmo motivo.
  AJ_SAIDA_PLAYER,
  // "O que achou?" nos creditos (reacao.h). LOCAL. No fim pelo mesmo motivo:
  // valor[] e CHAVE[] sao posicionais.
  AJ_REACAO_CREDITOS,
  // Quanto a escolha automatica de fonte espera os addons lentos (#221). LOCAL.
  // No fim pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_FONTE_PRAZO,
  // REGISTRO DO APP NO GLASS UI (03/10): "Ver o registro na tela" (Sobre e
  // ajuda; abre o painel do botao vermelho, que nem todo controle LG tem) e o
  // "Medidor de desempenho" (Desempenho desta TV; desempenho.h). LOCAIS. No
  // fim pelo mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_VER_REGISTRO, AJ_MEDIDOR,
  // GUIA DE USO (Ajustes › Sobre e ajuda, a primeira linha): abre a tela do
  // guia (ajustes_ux_guia.inc). Acao. No fim pelo mesmo motivo: valor[] e
  // CHAVE[] sao posicionais.
  AJ_GUIA,
  // PACOTE DE SELOS (selospacote.h): qual pacote desenha os selos da folha de
  // Fontes (0 = "Do Nuvio", o embutido; 1.. = os pacotes da conta e desta TV),
  // adicionar um por URL e remover o escolhido. A escolha mora por perfil em
  // selos-p<N>.txt (nao em ajustes.txt: o valor so espelha). No fim pelo mesmo
  // motivo: valor[] e CHAVE[] sao posicionais.
  AJ_SELOS_PACOTE, AJ_SELOS_PACOTE_ADD, AJ_SELOS_PACOTE_REM,
  // TAMANHO DA INTERFACE (gfx.h: gfx_escala_ui): player, folhas, ilhas e
  // modais ampliados por 1,2 / 1,3 / 1,5; a home e a pagina do titulo
  // ficam como estao. LOCAL (o tamanho e da tela, nao da conta). No fim pelo
  // mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_TAMANHO_UI,
  // FUNDO (Aparencia, Ajustes v2): o que fica atras dos paineis — a arte do
  // titulo (o de sempre), a mesma arte desfocada (fundo.c, gfx_desfocado) ou o
  // Frost (superficie fria tingida pelo acento). LOCAL: o desfoque custa GPU e
  // o Frost depende do acento desta TV. No fim pelo mesmo motivo: valor[] e
  // CHAVE[] sao posicionais.
  AJ_FUNDO,
  // TESTE DO VIDRO (03/10): quanto de opacidade tem o miolo dos paineis de vidro e
  // se ele leva a arte borrada atras ("fosco"). LOCAIS, avancados. No fim pelo
  // mesmo motivo: valor[] e CHAVE[] sao posicionais.
  AJ_VIDRO_OPAC, AJ_VIDRO_FOSCO,
  AJ_ICONE_APP, // append-only: keeps Glass option indices
  AJ_DISCORD,
  AJ_TAMANHO_AJUSTES, // local, append-only; 80/90/100%, default 80%
  AJ_LOGO_TRAILER,    // local, append-only; hide the corner title logo while a trailer plays (OLED)
  // Idioma da legenda SECUNDARIA (F04, 1.8): a mesma lista e a mesma regra da
  // principal ("Da conta" segue subtitle_secondary_language do perfil). No fim
  // pelo mesmo motivo dos outros: valor[] e CHAVE[] sao posicionais.
  AJ_LEG_LINGUA2,
  // SINCRONIA POR AUDIO (F06, 1.8): oferece "Por audio" na linha de
  // sincronizacao automatica do seletor de legendas, onde o backend entrega o
  // PCM decodificado (hoje so Android). LOCAL (o PCM e o passthrough sao desta
  // TV), padrao DESLIGADO. No fim pelo mesmo motivo: valor[] e CHAVE[] sao
  // posicionais.
  AJ_LEG_SYNC_AUDIO,
  // F07 (1.8): seek cache on disk for the Android player (cacheboost.h). LOCAL
  // (disk and backend are this TV's), append-only like every option above.
  AJ_CACHE_SEEK,
  // #241 (1.8): experimental zoom of the trailer video plane, native .tpk only.
  // LOCAL (each Samsung model reacts differently), default Desligado.
  AJ_TRAILER_ZOOM_TPK,
  // Plugins Nuvio (F09): abre a tela; o liga/desliga mora em plugins.c.
  AJ_PLUGINS,
  // Servidores pessoais (F11, jellyfin.h). Append-only: valor[] e CHAVE[]
  // sao posicionais. O ligado e LOCAL; endereco/token moram em
  // jellyfin-p<N>.txt (por perfil, 0600), nunca em ajustes.txt nem na conta.
  AJ_JF_LIGADO, AJ_JF_SERVIDOR, AJ_JF_ENTRAR, AJ_JF_SAIR,
  // R6 (04/10): interruptor GLOBAL das opcoes avancadas (substitui o "Avancados"
  // por categoria). LOCAL, V_LIGA: 1 = Desligado. No fim: valor[]/CHAVE[] posicionais.
  AJ_AVANCADAS,
  // R4: SEGUNDA LEGENDA, posicao e estilo proprios. Todos LOCAIS (o estilo da
  // principal tambem e desta TV: player.txt). "Igual a principal" = como era.
  AJ_LEG2_POS, AJ_LEG2_TAMANHO, AJ_LEG2_COR, AJ_LEG2_FUNDO, AJ_LEG2_BORDA,
  // Emby e Plex, ao lado do Jellyfin (mesmo interruptor AJ_JF_LIGADO). Estado em
  // emby-p<N>.txt / plex-p<N>.txt (por perfil, 0600): nunca em ajustes.txt.
  AJ_EM_SERVIDOR, AJ_EM_ENTRAR, AJ_EM_SAIR,
  AJ_PX_ENTRAR, AJ_PX_SERVIDOR, AJ_PX_SAIR,
  // R9b: o que a escolha automatica de fonte prioriza (Equilibrio / Qualidade
  // maxima / Começar rápido) e o que fazer com HDR e Dolby Vision (Preferir /
  // Indiferente / Evitar). LOCAIS. No fim: valor[]/CHAVE[] posicionais.
  AJ_FONTE_PRIORIDADE, AJ_FONTE_HDR,
  // 2.0 (N1): logo do app (Novo | Classico) and opening style (Padrao | So esmaece |
  // Direto). LOCAL, for everyone (no supporter gate). Append-only.
  AJ_LOGO_APP, AJ_ABERTURA,
  // N3: "Receber enquetes" (ligado por padrao). LOCAL, espelho do opt-out que
  // mora na conta (enquete.c). No fim: valor[]/CHAVE[] posicionais.
  AJ_ENQUETES,
  // #231: "Buscar no Cinemeta" (Ligado = como sempre). LOCAL. No fim: valor[]/CHAVE[] posicionais.
  AJ_BUSCA_CINEMETA,
  // Protecao de OLED (esmaecer.h): esmaecer a tela parada (Desligado/2/5/10 min,
  // padrao 5) e brilho da interface do player (100/80/65/50%, padrao 80%). LOCAIS.
  // No fim: valor[]/CHAVE[] posicionais.
  AJ_ESMAECER, AJ_BRILHO_PLAYER,
  // 2.0: "Novidades 2.0" (Sobre e ajuda) reabre o guia da 2.0 (novidades20.h).
  // Acao. No fim: valor[]/CHAVE[] posicionais.
  AJ_NOVIDADES20,
  // RETOMADA (05/10): manter a sessao do player pausada por ate 2 min ao sair
  // para a ilha (player.c, PLR_RETIDO_MS). Avancada, LOCAL, padrao DESLIGADO:
  // segura o decoder e o pipeline unico (sem trailer na home nesse tempo). Sem
  // ela o Retomar abre pela fonte guardada (fontevolta.h). No fim: valor[]/CHAVE[]
  // posicionais.
  AJ_MANTER_VIDEO,
  // Tela de descanso (2.0, descanso.h): o que aparece depois do escurecer
  // (Vitrine/Relogio/So escurecer) e de onde a vitrine tira os titulos. LOCAIS.
  // No fim: valor[]/CHAVE[] posicionais.
  AJ_DESCANSO_ESTILO, AJ_DESCANSO_FONTE,
  // Formato do relogio (2.0, pedido de usuario Samsung): 24 h ou 12 h com
  // AM/PM, em toda hora DE TELA (relogio.h). LOCAL. No fim: posicional.
  AJ_RELOGIO_12H,
  // Faixa de tamanho da escolha automatica (pedido de quem tem franquia de
  // dados limitada): tamanho maximo e minimo do arquivo, em GB. LOCAIS (o web
  // nao tem estas chaves). No fim: valor[]/CHAVE[] posicionais.
  AJ_TAM_MAX, AJ_TAM_MIN,
  // APOIAR O PROJETO (Sobre e ajuda, apoio.h): a previa mostra os QRs do
  // Patreon e do Ko-fi. Acao, sem valor. No fim: valor[]/CHAVE[] posicionais.
  AJ_APOIAR,
  // #287: com o audio no idioma da legenda, ligar so a legenda FORCADA desse
  // idioma (ou nenhuma). LOCAL, padrao Ligado. No fim: valor[]/CHAVE[] posicionais.
  AJ_LEG_FORCADA,
  // Classificacao indicativa no player (pedido do dono, 06/10): Ligado (padrao)
  // = a guia parental sobe da ilha com a faixa etaria ("Guia parental · 12");
  // Desligado = nada dela aparece. LOCAL. No fim: valor[]/CHAVE[] posicionais.
  AJ_PLR_CLASSIF,
  // AUTO-PLAY COMO NO NUVIO OFICIAL (#202, fonteregra.h): escopo, add-ons e
  // plugins permitidos, "usar os outros se nao houver" e a regex (modo, padrao
  // e modelos). As listas e o padrao moram em fonteregra.c (valor[] nao guarda
  // texto); o escopo e o modo sao numeros daqui. No fim: posicionais.
  AJ_FONTE_ESCOPO, AJ_FONTE_ADDONS_PERM, AJ_FONTE_PLUGINS_PERM, AJ_FONTE_OUTROS,
  AJ_FONTE_REGEX, AJ_FONTE_REGEX_PADRAO, AJ_FONTE_REGEX_MODELO,
  // Espaco entre fileiras e entre titulos da Home (home.c, fileiraGap/gapDe), em
  // por cento do valor medido (100 = o de sempre). LOCAIS: o web nao tem. No fim:
  // valor[]/CHAVE[] posicionais.
  AJ_ESPACO_FILEIRAS, AJ_ESPACO_TITULOS,
  // #202: "Proporção padrão" do player (Último usado | os 8 modos de player.h).
  // LOCAL, desta TV. No fim: valor[]/CHAVE[] posicionais.
  AJ_PROPORCAO_PADRAO,
  // 2.0.2: "Tambem em Continuar assistindo". Desligado (padrao) = um titulo, um
  // lugar (cwretido.h): na ilha / em Retomar agora ele some da fileira. Ligado =
  // aparece nos dois. LOCAL, desta TV. No fim: valor[]/CHAVE[] posicionais.
  AJ_CW_RETIDO_TAMBEM,
  // #303: "Continuar na escolha de perfil". Ligado (padrao) = o cartao grande
  // "Continuar assistindo" de cada perfil aparece na tela de escolha de perfil;
  // Desligado = nao e desenhado. LOCAL, desta TV. No fim: valor[]/CHAVE[] posicionais.
  AJ_PS_CONTINUAR,
  // DTS convertido em: 0 = Estereo (AAC, padrao, o provado), 1 = 5.1 (Dolby
  // Digital / AC3 640 kbps). So o caminho webOS que converte DTS no app
  // (src/dts). LOCAL, desta TV. No fim: valor[]/CHAVE[] posicionais.
  AJ_DTS_AC3,
  // 2.0.2: "Dolby Vision em MKV (experimental)". Ligado = MKV whose dvcC says
  // profile 5/8 single layer plays through our demux with Dolby Vision (webOS
  // only, video.c). LOCAL, desta TV; padrao Desligado. No fim: valor[]/CHAVE[].
  AJ_DV_MKV,
  // 2.0.2 (pedido de quem tem 2 perfis): por PERFIL desta TV, nao da conta.
  // AJ_HIST_CONTA: Ligado (padrao) = o que se assiste neste perfil sobe para a
  // conta Nuvio (progresso e vistos); Desligado = so vai para Trakt/Simkl.
  // AJ_SOCIAL: Ligado (padrao) = amigos, atividade e recomendacoes; Desligado
  // tira tudo isso do app e para as chamadas de rede do social. No fim:
  // valor[]/CHAVE[] posicionais.
  AJ_HIST_CONTA, AJ_SOCIAL,
  // 2.0.2: ORDEM DOS ADD-ONS no automatico (fonteregra.h): a folha para subir e
  // descer add-ons e plugins, e "Usar a ordem" (Nao | Para desempatar | Ordem
  // estrita). POR PERFIL, so desta TV (o oficial nao tem chave equivalente). A
  // ordem mora em fonteregra.c; aqui so o modo. No fim: valor[]/CHAVE[] posicionais.
  AJ_FONTE_ORDEM, AJ_FONTE_ORDEM_USO,
  AJ_FONTE_TOCAR_CONFERINDO,
  AJ_FONTE_AQUECER,
  AJ_FONTE_CONFERIR_VARIAS,
  AJ_FONTE_PREPARAR,
  // 2.0.2 (#203): "Proximo episodio no Continuar". Ligado (padrao) = os 'a seguir' das
  // series entram na fileira; Desligado = so o que esta em andamento. Por perfil, local
  // (como AJ_CW_FONTE). No fim: valor[]/CHAVE[] posicionais.
  AJ_CW_PROXIMO,
  // #311: onde ficam os resultados do catalogo do Nuvio na busca (Primeiro | Por
  // ultimo | Desligado) e se cada grupo diz "de <fonte>". LOCAIS. No fim:
  // valor[]/CHAVE[] posicionais.
  AJ_BUSCA_NUVIO, AJ_BUSCA_ORIGEM,
  // #339: o arranjo dos Ajustes nesta TV. 0 = Painel (a grade de categorias e,
  // dentro delas, a arte ao lado da lista; o de sempre), 1 = Lista (categorias e
  // opcoes uma embaixo da outra, a explicacao logo abaixo da linha em foco).
  // LOCAL. No fim: valor[]/CHAVE[] posicionais.
  AJ_LAYOUT_AJUSTES,
  // #334: "Limite de espaço do P2P" do motor embutido (p2pmotor_teto_duro):
  // Automatico (o de sempre) ou 2/4/8/16 GB, sempre deixando 512 MB livres.
  // LOCAL (o disco e desta TV). No fim: valor[]/CHAVE[] posicionais.
  AJ_P2P_LIMITE,
  // #400: LOCAL, deste aparelho. No fim: valor[]/CHAVE[] posicionais.
  AJ_FONTE_ORDEM_ADDON,
  AJ_LEG_SYNC_AUTO, // local, ligada de fábrica; append-only
  AJ_N
} OpcaoId;

// "Pacote de selos": so o TAMANHO importa aqui (3 pacotes + "Do Nuvio"); o texto
// de cada valor vem de selospacote_nome (textoValor / uxValorTexto).
static const char *V_SELOS_PACOTE[] = { "Do Nuvio", "Pacote 1", "Pacote 2", "Pacote 3" };

static const char *V_ICONE_APP[ICONEAPP_N] = {
  "Original", "Fênix", "N verde-água", "TV laranja", "N pixel",
  "Play tricolor", "Arco", "TV viva", "Clube retrô", "Arcade N",
};
static const char *V_FONTE_ORDEM_ADDON[] = { "Por qualidade", "Do addon" };
static const char *V_QUALIDADE[] = { "Automática", "4K", "1080p", "720p" };
static const char *V_LIGA[]      = { "Ligado", "Desligado" };
// Medidor de desempenho NA ILHA DO RELOGIO (desempenho.h, 03/10). O indice e o
// gravado em "medidorFormaLocal"; a chave antiga (V_LIGA) migra no carregar.
static const char *V_MEDIDOR[]   = { "Desligado", "Mínimo", "Menor", "Grande" };
static const char *V_LIVETV_RES[] = { "Automática", "4K", "1080p", "720p", "SD" };
static const char *V_LIVETV_FMT[] = { "Automático", "HLS (.m3u8)", "TS (.ts)" };
static const char *V_LIVETV_ESPERA[] = { "Automática", "25 s", "45 s" };
// Canto do relogio. 0 = Automatica (o de sempre: esquerda, e direita no layout
// Dinamica, onde a pilula da barra lateral ocupa o canto esquerdo). 1 e 2 sao
// escolha explicita; Esquerda no layout Dinamica fica AO LADO da pilula.
static const char *V_RELOGIO_POS[] = { "Automática", "Esquerda", "Direita" };
// Indice gravado em fontePrazoLocal; ver ajustes_fonte_prazo_ms.
// Indices gravados em fontePrioridadeLocal / fonteHdrLocal (ajustes_fonte_prioridade/_hdr).
static const char *V_FONTE_PRIORIDADE[] = { "Equilíbrio", "Qualidade máxima", "Começar rápido" };
static const char *V_FONTE_HDR[] = { "Preferir", "Indiferente", "Evitar" };
// 2.0: "ate 30 s" (dono, 05/10). Indices = esmaecer.h (ESM_ESCOLHAS); o
// ajuste nunca saiu numa versao publicada, entao os indices antigos nao migram.
static const char *V_ESMAECER[] = { "Desligado", "30 s", "1 min", "2 min", "5 min", "10 min" };
static const char *V_DESCANSO_ESTILO[] = { "Vitrine", "Relógio", "Só escurecer" };
// Faixa de tamanho: o INDICE e o gravado; ajustes_tamanho_*_gb() devolve os GB.
static const char *V_TAMANHO_GB[] = { "Sem limite", "1 GB", "2 GB", "4 GB", "8 GB", "15 GB", "30 GB" };
static const int   TAMANHO_GB[]   = { 0, 1, 2, 4, 8, 15, 30 };
static const char *V_RELOGIO_12H[] = { "24 horas", "12 horas (AM/PM)" };
static const char *V_LAYOUT_AJUSTES[] = { "Painel", "Lista" };   // #339
static const char *V_DESCANSO_FONTE[]  = { "Catálogo", "Minha lista e Continuar" };
static const char *V_BRILHO_PLAYER[] = { "100%", "80%", "65%", "50%" };
// #202: 0 = o ultimo modo usado (comportamento de sempre); 1..8 = PlrAspecto + 1.
static const char *V_DTS_SAIDA[] = { "Estéreo (AAC)", "5.1 (Dolby Digital)" };
static const char *V_PROPORCAO[] = { "Último usado", "Original", "Recortar", "Esticar", "Zoom leve", "Zoom cinema", "Zoom ultra", "Ajustar altura", "Ajustar largura" };
static const char *V_LOGO_APP[] = { "Novo", "Clássico" };
static const char *V_BUSCA_NUVIO[] = { "Primeiro", "Por último", "Desligado" };
static const char *V_ABERTURA[] = { "Padrão", "Só esmaece", "Direto" };
// #202: "Instantâneo" e 15/30 s, como o "Stream Selection Timeout" do
// oficial (0 = instantaneo; Unlimited = todos). O indice e o gravado em
// fonteEsperaLocal; o fontePrazoLocal de antes (3 s, 5 s, 8 s, Todos) migra na
// leitura (ver prazoAntigo em ajustes_dir).
static const char *V_FONTE_PRAZO[] = { "Instantâneo", "3 s", "5 s", "8 s", "15 s", "30 s", "Todos os add-ons" };
static const int   FONTE_PRAZO_MS[] = { 0, 3000, 5000, 8000, 15000, 30000, -1 };
// O que o automatico considera (stream_auto_play_source do oficial). Os
// literais sao os do enum do oficial (W_FONTE_ESCOPO), que a conta guarda.
static const char *V_FONTE_ESCOPO[] = { "Todas as fontes", "Só add-ons instalados", "Só plugins ligados" };
static const char *W_FONTE_ESCOPO[] = { "ALL_SOURCES", "INSTALLED_ADDONS_ONLY", "ENABLED_PLUGINS_ONLY", NULL };
// Regex: desligada, exigir (o REGEX_MATCH do oficial: so casa) ou preferir.
static const char *V_FONTE_REGEX[] = { "Desligada", "Exigir", "Preferir" };
// Modelos de regex (fonteregra_modelo). 0 = o padrao digitado.
static const char *V_FONTE_MODELO[] = { "Personalizado", "Espanhol", "Português", "Inglês",
                                        "Multi-áudio (MULTI)", "Áudio duplo (DUAL)", "4K e Remux" };
// "Usar a ordem" dos add-ons (fonteregra.h FR_ORDEM_*): o indice e o gravado.
static const char *V_FONTE_ORDEM_USO[] = { "Não", "Para desempatar", "Ordem estrita" };
// Indice gravado em tamanhoUiLocal; o fator sai de ajustes_tamanho_ui.
static const char *V_TAMANHO_UI[] = { "100%", "120%", "130%", "150%" };
static const char *V_TAMANHO_AJUSTES[] = { "80%", "90%", "100%" };
// F07: the order is cacheboost_cache_mb's (0, 256, 512, 1024 MB).
static const char *V_CACHE_SEEK[] = { "Desligado", "256 MB", "512 MB", "1 GB" };
// #334: indice gravado em p2pLimiteLocal; ajustes_p2p_limite_mb devolve os MB.
static const char *V_P2P_LIMITE[] = { "Automático", "2 GB", "4 GB", "8 GB", "16 GB" };
static const unsigned P2P_LIMITE_MB[] = { 0, 2048, 4096, 8192, 16384 };
// Only the Android player has an app-controlled disk cache (cacheboost.h);
// LG/Samsung show the row with "Não disponível nesta TV". Compile-time, so the
// many tests that compile ajustes.c alone need no extra source.
// DTS -> AC3/AAC conversion lives only in the webOS player (src/dts).
static int dtsConversaoExiste(void) {
#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP) || defined(NV_TPK) || defined(NV_ANDROID) || defined(__EMSCRIPTEN__)
  return 0;
#else
  return 1;
#endif
}
static int cacheSeekExiste(void) {
#ifdef NV_ANDROID
  return 1;
#else
  return 0;
#endif
}
// Indice gravado em fundoLocal (ajustes_fundo).
static const char *V_FUNDO[] = { "Arte", "Arte borrada", "Frost" };
static const char *V_VIDRO_OPAC[] = { "60%", "70%", "78%", "86%", "92%" };
static const char *V_VIDRO_FOSCO[] = { "Desligado", "Ligado" };
// R4: segunda legenda. 0 = No topo (como sempre); 1 = empilhada logo acima da
// principal, no pe da tela.
static const char *V_LEG2_POS[] = { "No topo", "Junto da principal" };
static const char *V_LEG2_TAMANHO[] = { "Automático", "60%", "80%", "100%", "120%", "140%", "160%" };
static const char *V_LEG2_COR[] = { "Igual à principal", "Branco", "Amarelo", "Verde", "Azul", "Vermelho", "Preto" };
static const char *V_LEG2_FUNDO[] = { "Igual à principal", "Nenhum", "Escuro 25%", "Escuro 50%", "Escuro 75%", "Escuro 100%" };
static const char *V_LEG2_BORDA[] = { "Igual à principal", "Nenhuma", "Contorno", "Sombra" };
static const char *V_SAIDA_PLAYER[] = { "Voltar para a home (minimizar na ilha)",
                                        "Voltar para a página do título" };
static const char *V_LIVETV_MODO[] = { "A (padrão)", "B (sem seleção de faixa)", "C (payload de live)" };
// Troca do destaque: 0 = a arte nova entra colada na velha, de lado; 1 = o
// esvanecimento de antes.
static const char *V_HERO_TRANSICAO[] = { "Deslizar", "Esmaecer" };
// Provedor dos posteres personalizados. O INDICE e o gravado ("posterProvLocal")
// e o PP_* de posterprov.h: so acrescentar no fim.
static const char *V_POSTER_PROV[] = { "Desligado", "SpatialPosters", "RPDB", "Modelo próprio" };
// LAYOUT DA HOME. O INDICE e o gravado (homeLayoutLocal) e o HOME_LAYOUT_* de
// ajustes.h: 0 = Moderna (a de sempre, e o padrao), 1 = Padrao, 2 = Dinamica.
//
// O app oficial tem `homeLayout` (layoutPreferences.js) com "modern", "grid" e
// "classic", que a conta guarda como selected_layout = MODERN | GRID | CLASSIC.
// Moderna = modern e Padrao = classic (destaque contido, fileiras num fundo
// liso), mas a escolha e LOCAL: a Dinamica nao tem par na conta, e gravar
// "DINAMICA" num enum que o app web valida faria os outros aparelhos cairem no
// padrao. Ver somenteDesteAparelho.
//
// O rotulo da terceira leva "(Apple TV)" porque "Dinâmica" a secas e a chave da
// traducao dos temas de cor ("Matching color"): o mesmo texto nas duas linhas
// deixaria a home em ingles com o nome do tema.
static const char *V_HOME_LAYOUT[] = { "Moderna", "Padrão", "Dinâmica (Apple TV)" };
// "Fonte automatica" (issue #130). O INDICE e o gravado (fonteAutoLocal) e o
// FONTEAUTO_* de fonteauto.h: 0 = a regra de pontuacao, 1 = a primeira da
// lista do addon, e so ela — o "Auto-play first source" do Nuvio.
static const char *V_FONTE_AUTO[] = { "Melhor fonte", "Primeira da lista" };
// Quantas OUTRAS fontes o automatico tenta quando a escolhida nao abre. O
// indice e o numero (fonteReporLocal); ate a 1.4.3 eram 7, fixos, e cada
// tentativa e mais um arquivo na conta de debrid da pessoa.
static const char *V_FONTE_REPOR[] = { "Desligado", "1 fonte", "2 fontes", "3 fontes" };
// O que cada linha da folha de Fontes mostra (dono, 02/10). 0 = o texto do
// Nuvio (nome do conteudo em cima, logos de qualidade embaixo, tamanho); 1 =
// o nome e a descricao como o addon formatou — quem monta o proprio formato no
// AIOStreams quer ver o dele.
static const char *V_FONTE_TEXTO[] = { "Do Nuvio", "Do addon", "Logo do título" };
// #90: o fundo de arte da tela de escolha de perfil (psfundo.c). "Automático"
// e o comportamento atual (perfil primeiro, catalogo como reserva, com
// rotacao); "Desligado" volta a tela ao que era antes da issue — psfundo nao
// desenha nada e so `profile_background_url` do proprio perfil (se houver)
// aparece. NAO tem as fontes de fil_hero_fonte() (topo do catalogo / fileira
// escolhida): aquela preferencia e por PERFIL, e aqui, antes de alguem
// escolher um perfil, nao ha perfil para ler a preferencia dele (ver a nota em
// perfilsel.c).
// Indices GRAVADOS em ajustes.txt: 0 = mural de capas (era "Automático"),
// 1 = listras do login (era "Desligado"), 2 = arte do perfil em foco (2.0).
// Valor novo SEMPRE no fim: quem ja tinha 0 ou 1 continua com o mesmo fundo.
// 2.0: o "Mural" virou "Filmes" (a parede de cada perfil) e entram "Luz" e
// "Projetor" no fim — os indices 0..2 continuam os mesmos.
static const char *V_PS_FUNDO[]  = { "Filmes", "Listras", "Arte do perfil", "Luz", "Projetor" };
// Teto de memoria para imagens. O indice vira MB em ajustes_tex_mb; 0 e a
// regra automatica pela RAM (tex_cache.c). Escolha por APARELHO: fica no
// ajustes.txt e nunca vai para a conta — a TV da sala e a do quarto nao tem
// a mesma RAM. Relato #71: a linha "Memoria usada por imagens" parecia um
// ajuste e nao era; este e.
// 400 e 512 so passam da trava em TV com 3 GB ou mais (C1/C2/C3); a C9 de 2,2
// GB para em 300, que e o unico valor acima do automatico medido em aparelho.
static const char *V_TEX_MB[]    = { "Automático", "96 MB", "160 MB", "240 MB", "300 MB", "400 MB", "512 MB" };
static const int   TEX_MB_DE[]   = { 0, 96, 160, 240, 300, 400, 512 };
// TRES PADROES DE IMAGEM. O nome diz o que a pessoa ganha, nao o que o cache
// faz: "Alta" e mais pixel de arte e mais memoria; "Baixa" e arte que chega
// antes e cabe em TV com pouca RAM.
static const char *V_QUALIMG[]   = { "Baixa", "Padrão", "Alta" };
static const char *V_QUALTRAIL[] = { "Máxima", "1080p", "720p", "480p" };
static const char *V_ASPTRAIL[]  = { "Zoom cinema", "Zoom leve", "Zoom ultra", "Original" };
// Fonte do trailer (trailerfonte.h). O indice e o gravado e o TRF_* do
// modulo: 0 Automatico, 1 Apple, 2 IMDb, 3 YouTube — nao reordenar.
static const char *V_TRAILFONTE[] = { "Automático", "Apple TV", "IMDb", "YouTube" };
// Nomes NATIVOS, sem i18n: quem trocou para um idioma que nao le precisa achar o seu.
// A ordem e a de IDIOMA_* (idiomacod.h) e a do valor gravado: so acrescentar no fim.
// EXCECAO: o primeiro rotulo, "Automático", e o unico que passa por i18n (os
// demais sao nativos de proposito). Ele nao e um idioma: valor[AJ_IDIOMA] e o
// INDICE DESTA LISTA (0 = automatico, 1 + IDIOMA_* = escolha manual), e o que
// vai para o disco e outra coisa (ver "IDIOMA AUTOMATICO" mais abaixo).
static const char *V_IDIOMA[]    = { "Automático", "Português", "English", "Română", "Українська", "Русский",
                                     "Français", "Deutsch", "Español",
                                     // Os 22 de 2026-09, na ordem de IDIOMA_IT... IDIOMA_ZHTW. Os tres
                                     // ultimos (日本語, 简体中文, 繁體中文) so desenham porque text.c manda
                                     // a linha para a fonte de reserva CJK (ver fonteDe).
                                     "Italiano", "Nederlands", "Polski", "Türkçe", "Português (Portugal)",
                                     "Svenska", "Dansk", "Norsk", "Čeština", "Slovenčina", "Slovenščina",
                                     "Magyar", "Lietuvių", "Bosanski", "Srpski", "Български", "Ελληνικά",
                                     "Bahasa Indonesia", "Tiếng Việt", "日本語", "简体中文", "繁體中文" };
static const char *V_ANIM[]      = { "Completas", "Reduzidas" };
static const char *V_FONTE_UI[]  = { "Inter", "LG Display", "Droid Sans",
                                     "Montserrat", "Roboto",
                                     "Atkinson Hyperlegible Next" };
// A ORDEM IMPORTA: o indice 0 e o padrao (ver a lista de padroes, que e
// posicional), e o padrao tem de ser 1080p. Numa TV que NAO concede a
// superficie 4K a escolha nao faz nada, e numa que concede ela quadruplica o
// preenchimento — nao e coisa para ligar sozinha em aparelho nenhum.
// 720p entra no FIM (valor 2) para nao mexer no que ja esta gravado: 0 = 1080p e
// 1 = 4K continuam como eram. 720p = alvo de desenho interno 1280x720 ampliado
// para a janela (o nivel 3 de gpunivel.h), em qualquer plataforma.
// Order = RES_* (resolucao.h). The old order lives only in res_migrar.
static const char *V_RESOLUCAO[] = { "Automática", "1080p", "4K (experimental)", "720p (leve)" };
// `collapseSidebar`: recolhida = a rail some e o conteudo comeca em 104.
static const char *V_RAIL[]      = { "Recolhida", "Fixa" };
// `continueWatchingCardStyle`, validado em layoutPreferences.js contra
// exatamente estes tres valores.
static const char *V_CW[]        = { "Card", "Largo", "P\xc3\xb4ster" };
// FONTE do "Continuar assistindo". As duas ja existem e ja sao fundidas em
// montarContinuar (descoberta.c); isto so escolhe quais entram.
//   Todas  = conta primeiro, Trakt e Simkl preenchendo o que falta
//   Conta  = so o progresso da conta Nuvio (syncprog.c)
//   Trakt  = so o /sync/playback do Trakt
//   Simkl  = so o /sync/playback e o "watching" do Simkl (simkl.c, #110)
// O INDICE E O QUE ESTA GRAVADO em ajustes.txt ("cwFonteLocal N"), entao a
// ordem e contrato: "Simkl" entrou NO FIM para quem ja tinha 1 ou 2 continuar
// lendo Conta e Trakt. Os numeros tem nome em ajustes.h (AJ_CWF_*), e
// tests/simkl_cw.sh confere rotulo por indice. O rotulo era "Ambas"
// (ambas o que? #312); agora "Todas as fontes": o Simkl so entra com vinculo
// feito, e a ajuda da linha diz a regra inteira. O valor gravado nao mudou.
static const char *V_CW_FONTE[]  = { "Todas as fontes", "Conta Nuvio", "Trakt", "Simkl" };
// `continueWatchingSortMode`, normalizado em normalizeContinueWatchingSortMode.
static const char *V_CW_ORDEM[]  = { "Padrão", "Estilo streaming", "Separar futuros" };
// Itens por fileira da Home (#163). O INDICE e o que fica gravado; o numero de
// cada um esta em ajustes_itens_fileira. 24 e o teto de DESC_ITENS_POR_FILEIRA.
static const char *V_ITENS_FIL[] = { "12", "18", "24" };
static const char *V_GPU_EF[]    = { "Automático", "Completos", "Leves" };
// O que o toque curto de OK faz num card da retomada (issue #93): abre o
// episodio direto no player, ou abre a pagina do titulo como sempre fez.
// Local, como cwFonteLocal — o app oficial nao tem esta escolha.
static const char *V_CW_OK[]     = { "Retomar", "Abrir página" };
// `discoverLocation`, validado contra estes tres.
static const char *V_DESCOBRIR[] = { "Mostrar na Busca", "Na barra lateral", "Desligado" };
// `homeImdbRatingsVisibility` — normalizeHomeImdbRatingsVisibility so aceita
// SHOW_ALL e HIDE_ALL.
static const char *V_NOTAS[]     = { "Mostrar", "Ocultar" };
// Fonte da arte do hero. O valor 0 mantém o comportamento atual, incluindo
// still de episódio quando a fileira é Continuar assistindo. Os outros valores
// pedem a arte daquela fonte PARA O TITULO — a que veio no item ou, para TMDB
// e Trakt, a buscada pelo id do IMDb (url virtual, artereserva.h); nunca um
// fundo generico. Vale para destaque, detalhe e card deitado (artehero.h).
// O INDICE e o gravado ("heroFundoLocal N"): ordem e contrato (ARTEHERO_*).
// 23/09/2026: Apple TV, fanart.tv e Anime entraram NO FIM (5, 6, 7), pelo
// mesmo contrato — quem ja tinha 1..4 gravado continua lendo a mesma fonte.
static const char *V_HERO_FONTE[] = {
  "Automático", "Catálogo / Cinemeta", "IMDb / Metahub", "TMDB", "Trakt",
  "Apple TV", "fanart.tv", "Anime (Kitsu / AniList)"
};
// ONDE O "+" ESCREVE ALEM DA LISTA LOCAL.
//
// A lista local (salvos.c) e escrita SEMPRE, nos dois valores, e isso nao e
// esquecimento: antes dela o "+" nao guardava nada em disco, entao quem nao
// tinha Trakt vinculado perdia tudo no primeiro ciclo de descoberta. Ver a nota
// de abertura de salvos.h. O que esta escolha decide e se o "+" TAMBEM publica
// na watchlist do Trakt.
//
// Padrao "Watchlist do Trakt" = o comportamento que o app ja tinha. Trocar o
// padrao para a lista local faria o "+" de quem usa Trakt parar de publicar la
// depois de uma atualizacao, sem ninguem ter pedido.
//
// "Plan to Watch do Simkl" (#110) entrou NO FIM pelo mesmo motivo de
// V_CW_FONTE: o indice e o gravado ("salvosDestino N"), e 1 tem de continuar
// sendo o Trakt. Com ele o "+" publica no Plan to Watch, e o Plan to Watch
// passa a entrar nos Salvos (descoberta.c). Nomes em ajustes.h (AJ_SALVOS_*).
static const char *V_SALVOS[]    = { "Lista do Nuvio", "Watchlist do Trakt",
                                     "Plan to Watch do Simkl" };
// PAIS DA GRADE DO GUIA (#158). O indice e o gravado ("epgPaisLocal N"):
// novos entram NO FIM. O nome de cada pais vai na propria lingua dele, que e
// como quem mora la o procura numa lista, e dispensa traducao. O codigo na
// frente e o que epg_paises_definir recebe (ver EPG_PAIS_COD).
static const char *V_EPG_PAIS[]  = {
  "Automático", "RO · România", "BR · Brasil", "PT · Portugal", "ES · España",
  "MX · México", "AR · Argentina", "IT · Italia", "FR · France",
  "DE · Deutschland", "UK · United Kingdom", "TR · Türkiye", "GR · Ελλάδα",
  "HU · Magyarország", "BG · България", "RS · Srbija", "HR · Hrvatska",
  "NL · Nederland", "AL · Shqipëri", "CZ · Česko", "SE · Sverige",
  "CL · Chile", "CO · Colombia", "PE · Perú"
};
#define AJ_N_EPG_PAIS ((int)(sizeof V_EPG_PAIS / sizeof *V_EPG_PAIS))
// `tmdb_language` no blob da conta guarda so o idioma BASE ("pt", "en") —
// normalizeTmdbLanguageForAndroid corta a regiao. A lista aqui e curta de
// proposito: a do web e gerada de AVAILABLE_LANGUAGES inteiro, e atravessar
// 40 idiomas numa seta de controle e pior que cobrir os que fazem sentido
// nesta TV. "Da interface" preserva o comportamento anterior: pt-BR quando a
// interface esta em portugues, en-US em ingles.
static const char *V_TMDB_LING[] = {
  "Da interface", "Português (Brasil)", "English", "Español", "Français",
  "Deutsch", "Italiano", "Português (Portugal)", "日本語", "한국어", "中文",
  "Română", "Українська", "Русский",
  // Acrescentados com os 22 idiomas da interface (o indice gravado dos 14
  // primeiros nao muda). "中文" acima e o simplificado; o tradicional vem no fim.
  "Nederlands", "Polski", "Türkçe", "Svenska", "Dansk", "Norsk", "Čeština",
  "Slovenčina", "Slovenščina", "Magyar", "Lietuvių", "Bosanski", "Srpski",
  "Български", "Ελληνικά", "Bahasa Indonesia", "Tiếng Việt", "繁體中文",
  // Arabe (relato do .tpk 4/5): sem ele o TMDB nunca era pedido em arabe e a
  // arte localizada (logo/fundo "ar") nunca chegava. A interface nao tem arabe;
  // e so o idioma dos metadados.
  "العربية"
};
_Static_assert(sizeof V_TMDB_LING / sizeof *V_TMDB_LING == 33,
               "V_TMDB_LING casa com ESC(..., 33), W_TMDB_LING e L[] de ajustes_tmdb_idioma");
_Static_assert(sizeof V_IDIOMA / sizeof *V_IDIOMA == IDIOMA_N + 1,
               "V_IDIOMA: \"Automático\" e um rotulo por IDIOMA_* de idiomacod.h");
// Preenchido em rotulosDeIdioma(), no arranque: os nomes saem de linguas.c em
// vez de serem uma segunda lista escrita a mao aqui. LING_MAX_OPC e folga: se
// linguas.c crescer, o excedente simplesmente nao aparece — melhor que ler
// fora do vetor.
// 32: a lista de linguas.c tem 30 entradas (2 acoes + 28 idiomas). O valor
// anterior era 24 e TRUNCAVA em silencio — os idiomas do fim da lista existiam
// em linguas.c e nao apareciam na tela.
#define LING_MAX_OPC 32
// TEMA: a cor de DESTAQUE, e so ela.
//
// O app web tem doze temas e cada um troca onze variaveis de CSS, o fundo
// incluido. Aqui entra UMA: `--focus-color`, o anel que marca onde o foco
// esta. E a escolha honesta para este app, e a razao esta medida: os tons de
// cinza desta interface foram calibrados um a um contra o fundo #0D0D0D (ver
// NV_COR_FUNDO em layout.h, e o defeito de contraste 1,0:1 que ele descreve).
// Trocar o fundo por tema invalidaria essa calibragem inteira, sem ninguem
// para refaze-la. O anel, ao contrario, e sempre branco hoje: tingi-lo nao
// depende de recalibrar nada e e o elemento que o olho segue no sofa.
//
// OS ACENTOS DE 03/10/2026 (acentos-mockup.html, aprovado pelo dono). Os doze
// de antes eram os `--focus-color` de themeColors.js do app web, copiados — e
// medidos aqui, dez de doze deixavam o rotulo da pilula abaixo de 4,5:1 (texto
// branco a 1,4:1 no Dourado). Agora sao DUAS familias, cada cor saida de uma
// especificacao em OKLCH com duas travas medidas (tests/acentos.sh):
//   CLAROS    (tinta escura #121316): a pilula a 8-16:1;
//   PROFUNDOS (tinta branca): o maior L em que o branco ainda le a 4,6:1.
// Cada uma traz as outras tres cores que o acento pinta: a MARCA sobre o
// escuro (nos profundos, a versao clara do matiz, L 0,84), o "HDR" de grupo e
// a LUZ do Frost/Imersiva (L 0,42, croma <= 0,11). O chip ligado e o
// preenchimento a 22% sobre a ilha, desenhado com alfa.
//
// SO NA TV (decisao do dono): o web segue com os hex antigos, e o INDICE e o
// mesmo — a conta continua guardando "OCEAN", e a TV desenha o Oceano dela.
// Os seis novos entram DEPOIS dos dinamicos (16-21), para o ajustes.txt de
// quem ja escolheu continuar lendo igual; sao locais como os dinamicos.
// Esmeralda e Carmesim ficam onde estao, perto das cores de aviso OK/erro
// (ilha.c): o icone do aviso diferencia (decisao do dono).
typedef struct { unsigned fill, marca, hdr, luz; char fam; } AcentoFixo;   // fam: 'c' claro, 'p' profundo, 0 dinamico
static const AcentoFixo TEMA_ACENTO[] = {
  { 0xf4f2ee, 0xf4f2ee, 0xcccac5, 0x4e4d49, 'c' },   //  0 Branco       (WHITE)
  { 0xd53b45, 0xfeb5b1, 0xd0aba7, 0x7e2f31, 'p' },   //  1 Carmesim     (CRIMSON)
  { 0x1276ce, 0xa5cffe, 0xa4b8cd, 0x154e87, 'p' },   //  2 Oceano       (OCEAN)
  { 0x9b54c8, 0xe0b8fe, 0xc1accd, 0x5f3979, 'p' },   //  3 Violeta      (VIOLET)
  { 0x218649, 0x92e0a7, 0x9ac0a1, 0x015d2d, 'p' },   //  4 Esmeralda    (EMERALD)
  { 0xb85a09, 0xffb88e, 0xd1ad95, 0x783904, 'p' },   //  5 Âmbar        (AMBER)
  { 0xc93c87, 0xfeafd1, 0xd1a8b7, 0x782f53, 'p' },   //  6 Rosa         (ROSE)
  { 0xf2cd64, 0xf2cd64, 0xcbb780, 0x5f4a06, 'c' },   //  7 Dourado      (GOLD)
  { 0x83e5bc, 0x83e5bc, 0x93c3ac, 0x065b41, 'c' },   //  8 Jade         (JADE)
  { 0xf1bdab, 0xf1bdab, 0xcaafa4, 0x6b4031, 'c' },   //  9 Ouro rosé    (ROSE_GOLD)
  { 0x88e0f6, 0x88e0f6, 0x96c1c9, 0x025766, 'c' },   // 10 Azul ártico  (ARCTIC_BLUE)
  { 0xa6abb2, 0xa6abb2, 0xa4a6a7, 0x494d54, 'c' },   // 11 Grafite      (GRAPHITE)
  { 0, 0, 0, 0, 0 },                                 // 12 Da arte
  { 0, 0, 0, 0, 0 },                                 // 13 (Estilizada: vira Da arte + Frost)
  { 0, 0, 0, 0, 0 },                                 // 14 Gradiente
  { 0, 0, 0, 0, 0 },                                 // 15 Imersiva
  { 0xe6d3b5, 0xe6d3b5, 0xc4baa9, 0x5a4a30, 'c' },   // 16 Champagne
  { 0xb2d5b8, 0xb2d5b8, 0xaabbaa, 0x37563d, 'c' },   // 17 Sálvia
  { 0xc7bbf0, 0xc7bbf0, 0xb5aec6, 0x504472, 'c' },   // 18 Lavanda
  { 0xaa5e68, 0xfdb4bc, 0xd0abac, 0x7a323e, 'p' },   // 19 Vinho
  { 0x038191, 0x89d9e8, 0x96bdc2, 0x045762, 'p' },   // 20 Petróleo
  { 0x6468d9, 0xbec6fe, 0xb0b4cd, 0x404488, 'p' },   // 21 Índigo
  { 0, 0, 0, 0, 0 },                                 // 22 Textura
  { 0, 0, 0, 0, 0 },                                 // 23 Textura sutil
};
#define AJ_N_TEMAS 12   // os da conta (W_TEMA)
// OS DINAMICOS (cor viva, corviva.h) vem DEPOIS dos doze da conta, e a posicao
// nao e acaso: os indices 0..11 continuam sendo os de W_TEMA, e o ajustes.txt
// de quem ja escolheu um tema le igual. 12 = "Da arte" (era "Dinâmica"; o
// destaque segue a arte do titulo em cena). 13 era "Dinâmica estilizada": saiu
// (03/10) e quem a tinha gravada abre em Da arte + Fundo Frost
// (na leitura do ajustes.txt). 14/15 depois do relato do dono de 25/09 ("quero algo
// mais imersivo ainda"); 22/23 = Textura (o material do titulo na pilula).
#define AJ_TEMA_DINAMICA    AJ_N_TEMAS
#define AJ_TEMA_ESTILIZADA  (AJ_N_TEMAS + 1)
#define AJ_TEMA_GRADIENTE   (AJ_N_TEMAS + 2)
#define AJ_TEMA_IMERSIVA    (AJ_N_TEMAS + 3)
#define AJ_TEMA_TEXTURA     22
#define AJ_TEMA_TEXTURA_SUTIL 23
#define AJ_N_TEMAS_OPC      24
_Static_assert(sizeof TEMA_ACENTO / sizeof *TEMA_ACENTO == AJ_N_TEMAS_OPC,
               "TEMA_ACENTO: uma linha por indice de V_TEMA");
static const char *V_TEMA[] = {
  "Branco", "Carmesim", "Oceano", "Violeta", "Esmeralda", "Âmbar",
  "Rosa", "Dourado", "Jade", "Ouro rosé", "Azul ártico", "Grafite",
  "Da arte", "Dinâmica estilizada", "Gradiente", "Imersiva",
  "Champagne", "Sálvia", "Lavanda", "Vinho", "Petróleo", "Índigo",
  "Textura", "Textura sutil"
};
_Static_assert(sizeof V_TEMA / sizeof *V_TEMA == AJ_N_TEMAS_OPC,
               "V_TEMA: um nome por indice de TEMA_ACENTO");
// A REGUA (Ajustes › Aparência › Cor de destaque): nove claros e nove
// profundos em ordem de matiz, e as dinamicas. E a ordem da TELA; o indice
// gravado continua o de V_TEMA.
static const int REGUA_CLAROS[9]    = { 0, 16, 7, 9, 17, 8, 10, 18, 11 };
static const int REGUA_PROFUNDOS[9] = { 1, 19, 6, 5, 4, 20, 2, 21, 3 };
static const int REGUA_DIN[5]       = { AJ_TEMA_DINAMICA, AJ_TEMA_GRADIENTE, AJ_TEMA_IMERSIVA,
                                        AJ_TEMA_TEXTURA, AJ_TEMA_TEXTURA_SUTIL };
static void corDeHex(unsigned h, float *r, float *g, float *b) {
  if (r) *r = ((h >> 16) & 255) / 255.0f;
  if (g) *g = ((h >> 8) & 255) / 255.0f;
  if (b) *b = (h & 255) / 255.0f;
}
// MESMA ORDEM de TEMA_ACENTO e de V_TEMA: e o indice que liga os tres.
//
// SEM LITERAL PARA OS DINAMICOS, de proposito: o app web nao tem esse tema, e
// e esta lista que decide o que a conta aceita e o que sobe. Um "DYNAMIC" aqui
// deixaria um blob futuro ligar o dinamico sem a pessoa pedir nesta TV, e na
// subida gravaria na conta um valor que o web nao sabe desenhar. A regra das
// duas direcoes esta em ajustes_aplicar_blob e ajustes_mesclar_blob
// (temaDinamico).
static const char *W_TEMA[] = {
  "WHITE", "CRIMSON", "OCEAN", "VIOLET", "EMERALD", "AMBER",
  "ROSE", "GOLD", "JADE", "ROSE_GOLD", "ARCTIC_BLUE", "GRAPHITE", NULL
};

static const char *V_LINGUA[LING_MAX_OPC];
static int         nLingua;
static void rotulosDeIdioma(void) {
  int i, n = ling_opcao_n();
  if (n > LING_MAX_OPC) n = LING_MAX_OPC;
  for (i = 0; i < n; i++) {
    const char *c = ling_opcao_codigo(i);
    // "" = seguir a conta; "*" = mostrar tudo. Os dois primeiros sao acoes, nao
    // idiomas, e por isso tem rotulo proprio.
    // SEM i18n() AQUI DE PROPOSITO: rotulosDeIdioma() roda uma vez por abertura
    // da tela (ajustes_iniciar/ajustes_dir), nao a cada quadro. Se traduzisse
    // aqui, trocar "Idioma da interface" para Ingles DENTRO da mesma sessao de
    // Ajustes deixaria a lista de idiomas presa no idioma antigo ate a tela
    // reabrir. Guardando o nome cru, quem traduz e desenhaLinha->txt_linha_corta
    // a cada quadro (text.c aplica i18n() no que for desenhado, sempre com o
    // idioma CORRENTE) — o mesmo motivo por que os demais rotulos desta tabela
    // (V_QUALIDADE etc.) tambem ficam em portugues aqui.
    V_LINGUA[i] = !c[0] ? "Da conta"
                : !strcmp(c, "*") ? "Todas"
                : !strcmp(c, "~") ? "Original do título"
                : ling_nome(c);
  }
  nLingua = n;
}

// Natureza da linha.
// OP_ACAO responde ao OK, nao a esquerda/direita. Ela NAO e leitura: uma linha
// que faz alguma coisa tem de ter o mesmo destaque de quem muda valor, senao o
// usuario aperta OK esperando que nada aconteca.
typedef enum { OP_ESCOLHA, OP_NUMERO, OP_LEITURA, OP_ACAO } OpcaoTipo;

typedef struct {
  const char  *rotulo;
  OpcaoTipo    tipo;
  const char **valores;   // OP_ESCOLHA
  int          n;         // OP_ESCOLHA: quantos valores
  int          min, max, passo;   // OP_NUMERO
  const char  *sufixo;            // OP_NUMERO: "%", "s", "dp"
} Opcao;

#define ESC(rot, vals, qtd) { rot, OP_ESCOLHA, vals, qtd, 0, 0, 0, NULL }
#define NUM(rot, lo, hi, st, suf) { rot, OP_NUMERO, NULL, 0, lo, hi, st, suf }
#define LER(rot)            { rot, OP_LEITURA, NULL, 0, 0, 0, 0, NULL }
#define ACAO(rot)           { rot, OP_ACAO,    NULL, 0, 0, 0, 0, NULL }

// Qual campo do portal esta sendo digitado: 0 = nenhum, AJ_STALKER_PORTAL ou
// AJ_STALKER_MAC. A modal e uma so; o destino do que sair dela e isto.
static int stCampo;

// Alfabetos da modal. Sao diferentes porque os campos sao diferentes: endereco
// precisa de ponto, dois pontos e hifen; MAC so de hexadecimal e dois pontos, e
// oferecer o resto so daria chance de digitar um MAC invalido.
static const char *ST_ALFA_PORTAL =
  "abcdefghijklmnopqrstuvwxyz0123456789.:-/_";
static const char *ST_ALFA_MAC = "0123456789abcdef:";
// Usuario e senha de Xtream sao o que o provedor gerou: letras dos dois casos,
// digitos e uns poucos sinais. Sem espaco — nenhum painel Xtream o aceita.
static const char *XT_ALFA_CONTA =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-@!#$%&*+=";
// Endereco do Jellyfin: esquema e prefixo de proxy reverso (https://x/jellyfin).
static const char *JF_ALFA_URL =
  "abcdefghijklmnopqrstuvwxyz0123456789.:-/_";
// Senha do Jellyfin: o servidor aceita espaco.
static const char *JF_ALFA_SENHA =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-@!#$%&*+=?/ ";

static const Opcao OPCOES[AJ_N] = {
  ESC("Qualidade máxima",           V_QUALIDADE, 4),
  ESC("Dolby Vision",               V_LIGA, 2),
  ESC("Dolby Atmos",                V_LIGA, 2),
  // O `n` real e escrito por rotulosDeIdioma(); 2 aqui so mantem a tabela
  // valida antes do arranque.
  ESC("Idioma da legenda",          V_LINGUA, 2),
  ESC("Idioma do áudio",            V_LINGUA, 2),
  // `playback_pause_overlay` (settingsScreen.js:6320). Liga o painel que
  // sobe cinco segundos depois de pausar; ver pausao.h.
  ESC("Painel ao pausar",           V_LIGA, 2),   // pauseOverlayEnabled
  ESC("Escolher a fonte ao reproduzir", V_LIGA, 2), // local: ver ajustes_fonte_manual
  ESC("Fonte automática",           V_FONTE_AUTO, 2),  // local: ver fonteauto.h
  ESC("Outra fonte se falhar",      V_FONTE_REPOR, 4), // local: ver ajustes_fonte_repor
  ESC("Texto das fontes",           V_FONTE_TEXTO, 3), // local: ver ajustes_fonte_texto_addon

  ESC("Pôsteres horizontais",       V_LIGA, 2),   // modernLandscapePostersEnabled
  ESC("Fundo em tela cheia",        V_LIGA, 2),   // modernHeroFullScreenBackdropEnabled
  ESC("Background do hero",         V_HERO_FONTE, 8), // local: fonte real da arte
  // Card e destaque com fotos diferentes (dono, 22/09: "tem que ter um toggle
  // de ter uma versao diferente do que mostra no card do que ta na hero").
  // DESLIGADO por padrao: a mesma foto nos dois e o pedido de 19/09 (ver
  // artehero_url em artehero.c). A regra inteira esta em artehero.h.
  ESC("Destaque com outra arte",    V_LIGA, 2),   // local: heroDifferentFromCard
  // Trailer mudo no destaque do topo, alguns segundos depois de o foco parar
  // nele (dono, 20/09/2026: "coloca para tocar no hero tb", "nos ajustes o de
  // tocar no hero separadamente"). Separado do da pagina de titulo.
  ESC("Trailer no destaque",        V_LIGA, 2),

  // O limite NAO tem valor proprio em valor[]: ele mora em fileiras.c, que e
  // quem grava fileirasui.txt e quem a descoberta e a home consultam. A linha
  // aqui e um ESPELHO, sincronizado em ajustes_dir/ajustes_iniciar — duas
  // copias do mesmo numero divergem no primeiro caminho que esquecer uma.
  NUM("Limite de fileiras",         FIL_LIMITE_MIN, FIL_LIMITE_MAX, 1, NULL),
  ACAO("Ordenar e ativar fileiras"),
  // LOCAL (#95): a home grava coluna/rolagem em home-pos.txt. Ligado = ao
  // reabrir, cada fileira comeca no primeiro tile (o pedido da issue).

  ESC("Barra lateral",              V_RAIL, 2),   // collapseSidebar
  ESC("Barra lateral moderna",      V_LIGA, 2),   // modernSidebar
  ESC("Desfoque da barra moderna",  V_LIGA, 2),   // modernSidebarBlur
  ESC("Mostrar destaque",           V_LIGA, 2),   // heroSectionEnabled
  // #160: era LER com a CONTAGEM de heroCatalogKeys, e ninguem preenchia a
  // contagem nem o destaque lia essas chaves: a linha dizia "Todos" para
  // sempre e nao fazia nada. Agora mostra e troca a fonte real do destaque
  // (fil_hero_fonte), a mesma da folha de fileiras. A chave continua "-":
  // quem grava a escolha e fileiras.c.
  ACAO("Catálogos do destaque"),                  // fil_hero_fonte
  ESC("Fundo da escolha de perfil", V_PS_FUNDO, 5), // local: ver psfundo.c
  ESC("Local do Descobrir",         V_DESCOBRIR, 3), // discoverLocation
  ESC("Rótulos nos pôsteres",       V_LIGA, 2),   // posterLabelsEnabled
  ESC("Nome do addon no catálogo",  V_LIGA, 2),   // catalogAddonNameEnabled
  ESC("Tipo de conteúdo",           V_LIGA, 2),   // catalogTypeSuffixEnabled
  ESC("Ocultar não lançados",       V_LIGA, 2),   // hideUnreleasedContent
  ESC("Avaliações gerais",          V_NOTAS, 2),  // homeImdbRatingsVisibility
  ESC("Gradiente de foco clássico", V_LIGA, 2),   // classicFocusGradientEnabled

  ESC("Mostrar \"Continuar assistindo\"", V_LIGA, 2), // continueWatchingEnabled
  ESC("OK no card",                     V_CW_OK, 2),  // local, ver V_CW_OK
  ESC("Fonte do \"Continuar assistindo\"", V_CW_FONTE, 4),   // local, ver V_CW_FONTE
  ESC("Estilo do \"Continuar assistindo\"", V_CW, 3), // continueWatchingCardStyle
  ESC("Miniatura do episódio",      V_LIGA, 2),   // useEpisodeThumbnailsInCw
  ESC("Desfocar próximo episódio",  V_LIGA, 2),   // blurContinueWatchingNextUp
  ESC("Próximo do episódio mais alto", V_LIGA, 2),// nextUpFromFurthestEpisode
  ESC("Mostrar episódios não exibidos", V_LIGA, 2),// showUnairedNextUp
  ESC("Ordenação",                  V_CW_ORDEM, 3), // continueWatchingSortMode

  ESC("Desfocar não assistidos",    V_LIGA, 2),   // blurUnwatchedEpisodes
  ESC("Botão de trailer",           V_LIGA, 2),   // detailPageTrailerButtonEnabled
  ESC("Priorizar metadados externos", V_LIGA, 2), // preferExternalMetaAddonDetail
  ESC("Data de lançamento completa", V_LIGA, 2),  // showFullReleaseDate
  // Forca da vinheta escura sobre o fundo do titulo (dono, 20/09/2026: "mexer
  // na opacidade desse layer escuro, ate tirar"). 100 = a vinheta medida no
  // web; 0 = arte limpa. Local, sem chave no blob da conta.
  NUM("Escurecimento do fundo",     0, 100, 10, "%"),
  // Trailer mudo no lugar da arte, depois de a pagina assentar (dono,
  // 20/09/2026). Samsung: embed do YouTube; LG: MP4 do IMDb (trailer.h).
  ESC("Trailer automático",         V_LIGA, 2),
  // Definicao do MP4 do IMDb (trailerimdb.h): "Máxima" pega a maior que o
  // IMDb tem (1080p hoje); as outras sao tetos.
  ESC("Qualidade do trailer",       V_QUALTRAIL, 4),
  // Mesmos zooms do player (player.h): o trailer do IMDb vem 16:9 com a
  // tarja do 2.39:1 embutida, e "Zoom cinema" (1,34) e o que a tira.
  ESC("Proporção do trailer",       V_ASPTRAIL, 4),
  // De onde vem o trailer (dono, 22/09/2026: "deixa o toggle no settings de
  // qual o source do trailer"). Vale para a pagina de titulo e o destaque;
  // a regra inteira, com o que cada TV toca, esta em trailerfonte.h.
  ESC("Fonte do trailer",           V_TRAILFONTE, 4),

  ESC("Expandir pôster ao focar",   V_LIGA, 2),   // focusedPosterBackdropExpandEnabled
  NUM("Atraso da expansão",         0, 10, 1, " s"), // ...ExpandDelaySeconds
  ESC("Navegação horizontal rápida", V_LIGA, 2),  // fastHorizontalNavigationEnabled
  ESC("Borda no cartaz em foco",    V_LIGA, 2),   // local: ver bordaFocoCartaz

  ESC("Efeito de profundidade",     V_LIGA, 2),   // cardDepthEnabled
  NUM("Brilho da borda",            0, 100, 2, "%"), // cardDepthEdgeStrength
  NUM("Reflexo",                    0, 100, 2, "%"), // cardDepthSheenStrength
  NUM("Cobertura da borda",         0, 100, 2, "%"), // cardDepthEdgeCoverage
  ESC("Profundidade nos pôsteres",  V_LIGA, 2),
  ESC("Profundidade no \"Continuar\"", V_LIGA, 2),
  ESC("Profundidade nos episódios", V_LIGA, 2),
  ESC("Profundidade no elenco",     V_LIGA, 2),
  ESC("Profundidade nos trailers",  V_LIGA, 2),

  NUM("Largura do item",            72, 200, 2, " dp"), // posterCardWidthDp
  NUM("Arredondamento",             0, 40, 1, " dp"),   // posterCardCornerRadiusDp
  ESC("Qualidade da imagem",        V_QUALIMG, 3),

  ESC("Idioma do app",              V_IDIOMA, IDIOMA_N + 1),
  ESC("Animações",                  V_ANIM, 2),
  ESC("Resolução da interface",     V_RESOLUCAO, 4),
  ESC("Cor de destaque",            V_TEMA, AJ_N_TEMAS_OPC),  // selected_theme (+4 locais)
  // So vale com um tema dinamico: o destaque sai do LOGO do titulo em vez da
  // arte de fundo (dono, 25/09/2026: "matching color da logo tambem como
  // toggle"). LIGADO de fabrica: o logo e a cor que o estudio escolheu para o
  // titulo, e foi a falta dele que deu o botao laranja-pele no "Prenda-me".
  ESC("Cor da logo",                V_LIGA, 2),   // local: corDaLogoLocal

  LER("Perfil"),
  LER("Sincronização"),
  ACAO("Addons"),
  ACAO("Portal Stalker (MAC)"),
  ACAO("MAC do portal"),
  ACAO("Remover o portal Stalker"),
  ACAO("Servidor Xtream"),
  ACAO("Usuário Xtream"),
  ACAO("Senha Xtream"),
  ACAO("Remover o Xtream"),
  LER("Conta Xtream"),
  ESC("Grade de programação",      V_EPG_PAIS, AJ_N_EPG_PAIS),
  ESC("Onde o + salva",             V_SALVOS, 3),
  ACAO("Trakt"),
  ACAO("Simkl"),
  ACAO("Sair da conta"),
  LER("Versão"),
  // A porta de saida para quem dispensou o cartao. Ele aparece UMA VEZ por
  // versao (a marca em atualizacao-vista.txt), e sem esta linha "Depois"
  // significava "nunca mais nesta versao".
  ACAO("Atualizar o aplicativo"),
  // Manda o log DESTA sessao ao servico de recomendacoes (dono, 20/09/2026):
  // ate aqui so o cartao do crash mandava, e so o da sessao anterior.
  ACAO("Enviar registro"),
  // ENVIO AUTOMATICO (dono, 20/09/2026: "temos que pegar todos os logs para
  // resolver a Samsung"). Desligado de fabrica; o cartao de primeira vez no
  // Tizen pergunta (telemetria.c) e grava aqui. Ligado, avisos.c manda o
  // registro da sessao anterior no arranque e o desta a cada minuto.
  ESC("Enviar registros sozinho",   V_LIGA, 2),
  LER("Memória usada por imagens"),
  ESC("Memória para imagens",       V_TEX_MB, 7),

  // Integracoes — TMDB. Os rotulos seguem a pagina integration:tmdb do web
  // (settingsScreen.js): um master + um toggle por recurso que o enriquecimento
  // toca. LIGADO por padrao em tudo: diferente do web, onde o TMDB e opt-in, o
  // nativo sempre enriqueceu por ele — nascer desligado apagaria elenco com
  // foto, ficha e trailers de quem ja usa o app sem nunca ter visto o ajuste.
  ESC("TMDB",                       V_LIGA, 2),   // tmdb_enabled
  ESC("Idioma dos metadados",       V_TMDB_LING, 33), // tmdb_language
  ESC("Arte localizada",            V_LIGA, 2),   // tmdb_use_artwork
  ESC("Título e sinopse",           V_LIGA, 2),   // tmdb_use_basic_info
  ESC("Ficha técnica",              V_LIGA, 2),   // tmdb_use_details
  ESC("Datas de lançamento",        V_LIGA, 2),   // tmdb_use_release_dates
  ESC("Elenco e equipe",            V_LIGA, 2),   // tmdb_use_credits
  ESC("Produtoras",                 V_LIGA, 2),   // tmdb_use_productions
  ESC("Redes e estúdios",           V_LIGA, 2),   // tmdb_use_networks
  ESC("Episódios",                  V_LIGA, 2),   // tmdb_use_episodes
  ESC("Trailers",                   V_LIGA, 2),   // tmdb_use_trailers
  ESC("\"Mais como este\"",         V_LIGA, 2),   // tmdb_use_more_like_this
  ESC("Coleções e sagas",           V_LIGA, 2),   // tmdb_use_collections
  ESC("Enriquecer \"Continuar assistindo\"", V_LIGA, 2), // tmdb_enrich_continue_watching

  // Integracoes — MDBList. O master liga/desliga a consulta; os demais
  // escolhem quais fontes de nota viram cartao na pagina de titulo.
  ESC("MDBList",                    V_LIGA, 2),   // mdblist_enabled
  LER("Chave do MDBList"),                        // mdblist_api_key (status)
  ESC("Notas do Trakt",             V_LIGA, 2),   // mdblist_show_trakt
  ESC("Notas do IMDb",              V_LIGA, 2),   // mdblist_show_imdb
  ESC("Notas do TMDB",              V_LIGA, 2),   // mdblist_show_tmdb
  ESC("Notas do Letterboxd",        V_LIGA, 2),   // mdblist_show_letterboxd
  ESC("Notas do Rotten Tomatoes",   V_LIGA, 2),   // mdblist_show_tomatoes
  ESC("Nota da audiência",          V_LIGA, 2),   // mdblist_show_audience
  ESC("Notas do Metacritic",        V_LIGA, 2),   // mdblist_show_metacritic
  ESC("Notas do MyAnimeList",       V_LIGA, 2),   // mdblist_show_mal
  // Chave PESSOAL do fanart.tv (fonte "fanart.tv" do destaque). Nunca vem no
  // pacote: a chave de projeto do fanart.tv e por aplicativo e nao pode ser
  // publicada; a pessoal cada um tira em fanart.tv/get-an-api-key.
  ACAO("Chave do fanart.tv"),
  ACAO("Diagnóstico e otimização"),
  ACAO("Teste de velocidade"),
  ESC("Explorar na barra lateral",       V_LIGA, 2),   // local: menuExplorarLocal
  ESC("Guia TV na barra lateral",        V_LIGA, 2),   // local: menuGuiaLocal
  ESC("Agenda na barra lateral",         V_LIGA, 2),   // local: menuAgendaLocal
  ESC("Perfil e Stats na barra lateral", V_LIGA, 2),   // local: menuPerfilLocal
  ESC("Trailer do cartaz em foco",       V_LIGA, 2),   // focusedPosterBackdropTrailerEnabled
  ESC("Itens por fileira",               V_ITENS_FIL, 3), // local: itensFileiraLocal
  ESC("Fonte da interface", V_FONTE_UI, 6),
  ESC("Efeitos visuais", V_GPU_EF, 3),   // local: gpuEfeitosLocal (.tpk)
  ESC("Interface de vidro",              V_LIGA, 2),   // local: vidroLocal
  // EXPERIMENTAL (p2p.h). Tocar torrent sem debrid, por um servidor de
  // streaming do Stremio na rede local. LOCAL: o web nao tem esta escolha.
  ESC("Servidor P2P (experimental)",     V_LIGA, 2),   // local: p2pLocal
  ACAO("Endereço do servidor P2P"),
  ACAO("Testar servidor P2P"),
  // POSTERES PERSONALIZADOS (posterprov.h). Desligado de fabrica. LOCAL: o web
  // nao tem esta escolha e o servico e por aparelho/rede.
  ESC("Pôsteres personalizados",         V_POSTER_PROV, 4),   // local: posterProvLocal
  ACAO("Endereço do SpatialPosters"),
  ACAO("Token do SpatialPosters"),
  ACAO("Parâmetros do SpatialPosters"),
  ACAO("Chave do RPDB"),
  ACAO("Modelo de URL dos pôsteres"),
  ACAO("Testar pôsteres"),
  ESC("Layout da home",             V_HOME_LAYOUT, 3), // local: ver V_HOME_LAYOUT
  ACAO("Chave do AllDebrid"),
  ACAO("Testar chave do AllDebrid"),
  ACAO("Chave do Real-Debrid"),
  ACAO("Chave do TorBox"),
  ACAO("Chave do Premiumize"),
  ESC("Usar sempre o catálogo do Nuvio",          V_LIGA, 2),   // local: soCinemetaLocal
  // Notas na linha do titulo (ver o enum). Ligado/Desligado como as demais.
  ESC("IMDb",                       V_LIGA, 2),   // local: notaTituloImdb
  ESC("Rotten Tomatoes (crítica)",  V_LIGA, 2),   // local: notaTituloTomates
  ESC("Popcornmeter (público)",     V_LIGA, 2),   // local: notaTituloAudiencia
  ESC("Metacritic (crítica)",       V_LIGA, 2),   // local: notaTituloMeta
  ESC("Metacritic (usuários)",      V_LIGA, 2),   // local: notaTituloMetaUser
  ESC("Trakt",                      V_LIGA, 2),   // local: notaTituloTrakt
  ESC("TMDB",                       V_LIGA, 2),   // local: notaTituloTmdb
  ESC("Letterboxd",                 V_LIGA, 2),   // local: notaTituloLetter
  ESC("MyAnimeList",                V_LIGA, 2),   // local: notaTituloMal
  ESC("Roger Ebert (crítica)",      V_LIGA, 2),   // local: notaTituloEbert
  ESC("Nota do MDBList",            V_LIGA, 2),   // local: notaTituloScore
  // O ESTADO DESTE INTERRUPTOR NAO MORA EM valor[]: ele e o que recomenda.c diz
  // (recomenda_pesquisavel), reescrito a cada quadro em ajustes_atualizar.
  // Duas fontes da verdade para "estou aparecendo para os outros?" divergiriam
  // no primeiro sair/entrar de conta — e o erro seria a pessoa achar que esta
  // escondida estando visivel.
  ESC("Perfil pesquisável",              V_LIGA, 2),
  ACAO("Meu perfil público"),
  ESC("Contorno do vidro",               V_LIGA, 2),   // local: vidroContornoLocal
  ESC("Som do trailer no destaque",      V_LIGA, 2),   // local: trailerDestaqueSomLocal
  // Em DECIMOS de segundo (0,2 s a 10 s): textoValor escreve "2,2 s".
  NUM("Espera do trailer no destaque",   2, 100, 2, " s"), // local: trailerDestaqueEsperaLocal
  // Era o 90 fixo de montarContinuar/home_registrar_retorno/proximo.h. Local:
  // o web nao tem a escolha.
  NUM("Percentual assistido",            70, 98, 1, "%"),  // local: cwConcluidoLocal
  ESC("Usar os addons do perfil principal", V_LIGA, 2), // local: addonsPrincipalLocal
  ESC("Transição do destaque",   V_HERO_TRANSICAO, 2), // local: heroTransicaoLocal
  ESC("Pôsteres do addon",               V_LIGA, 2),   // local: posterAddonLocal
  ESC("Fundo do destaque do addon",      V_LIGA, 2),   // local: fundoAddonLocal
  ESC("Logo do addon",                   V_LIGA, 2),   // local: logoAddonLocal
  ESC("Arte das pastas da conta",        V_LIGA, 2),   // local: colArteContaLocal
  ESC("Selos coloridos",                 V_LIGA, 2),   // local: selosColoridosLocal
  ESC("Som do trailer na página do título", V_LIGA, 2), // local: trailerDetalheSomLocal
  ESC("Resolução principal",             V_LIVETV_RES, 5),    // local: liveTvResolucaoLocal
  ESC("Formato do Xtream",               V_LIVETV_FMT, 3),    // local: xtreamFormatoLocal
  ESC("Espera para abrir o canal",       V_LIVETV_ESPERA, 3), // local: liveTvEsperaLocal
  ACAO("Diagnóstico da Live TV"),
  ESC("Modo do player da Live TV",       V_LIVETV_MODO, 3),   // local: liveTvModoLocal
  ESC("Proxy de TS da Live TV",          V_LIGA, 2),          // local: liveTvProxyLocal
  ESC("Relógio na tela",                 V_LIGA, 2),          // local: relogioTelaLocal
  ESC("Posição do relógio",              V_RELOGIO_POS, 3),   // local: relogioPosLocal
  ESC("Selo de assistido nos pôsteres",  V_LIGA, 2),          // local: seloVistoLocal
  ESC("Miniaturas na barra de tempo",    V_LIGA, 2),          // local: seekrLocal
  ACAO("Chave do Seekr"),
  ACAO("Testar chave do Seekr"),
  ESC("Fita de miniaturas",              V_LIGA, 2),          // local: seekrFitaLocal
  NUM("Sincronia da miniatura",          -60, 60, 1, " s"),   // local: seekrAjusteLocal
  ESC("Ao sair do player",               V_SAIDA_PLAYER, 2),  // local: saidaPlayerLocal
  ESC("Perguntar o que achou nos créditos", V_LIGA, 2),     // local: reacaoCreditosLocal
  ESC("Espera pelos add-ons",            V_FONTE_PRAZO, 7),   // local: fonteEsperaLocal
  ACAO("Ver o registro na tela"),
  ESC("Medidor de desempenho",           V_MEDIDOR, 4),       // local: medidorFormaLocal
  ACAO("Guia de uso"),
  ESC("Pacote de selos",                 V_SELOS_PACOTE, 4),   // por perfil: selospacote.c
  ACAO("Adicionar pacote de selos"),
  ACAO("Remover pacote"),
  ESC("Tamanho da interface",            V_TAMANHO_UI, 4),    // local: tamanhoUiLocal
  ESC("Fundo",                           V_FUNDO, 3),         // local: fundoLocal
  ESC("Opacidade do vidro",              V_VIDRO_OPAC, 5),    // local: vidroOpacLocal
  ESC("Vidro fosco",                     V_VIDRO_FOSCO, 2),   // local: vidroFoscoLocal
  ESC("Ícone do app", V_ICONE_APP, ICONEAPP_N),
  ACAO("Discord"),
  ESC("Tamanho dos ajustes", V_TAMANHO_AJUSTES, 3),
  ESC("Esconder logo durante o trailer", V_LIGA, 2),
  ESC("Idioma da legenda secundária",    V_LINGUA, 2),
  ESC("Sincronia por áudio",             V_LIGA, 2),
  ESC("Cache de seek em disco",          V_CACHE_SEEK, 4),
  ESC("Zoom no trailer e no player (experimental)", V_LIGA, 2),   // local: trailerZoomTpkLocal (.tpk)
  ACAO("Plugins"),
  ESC("Servidores pessoais (experimental)", V_LIGA, 2),   // local: jellyfinLocal
  ACAO("Endereço do Jellyfin"),
  ACAO("Entrar no Jellyfin"),
  ACAO("Sair do Jellyfin"),
  ESC("Mostrar opções avançadas",       V_LIGA, 2),   // local: avancadasLocal
  ESC("Posição da segunda legenda",      V_LEG2_POS, 2),      // local: legenda2PosLocal
  ESC("Tamanho da segunda legenda",      V_LEG2_TAMANHO, 7),  // local: legenda2TamanhoLocal
  ESC("Cor da segunda legenda",          V_LEG2_COR, 7),      // local: legenda2CorLocal
  ESC("Fundo da segunda legenda",        V_LEG2_FUNDO, 6),    // local: legenda2FundoLocal
  ESC("Borda da segunda legenda",        V_LEG2_BORDA, 4),    // local: legenda2BordaLocal
  ACAO("Endereço do Emby"),
  ACAO("Entrar no Emby"),
  ACAO("Sair do Emby"),
  ACAO("Entrar no Plex"),
  ACAO("Servidor do Plex"),
  ACAO("Sair do Plex"),
  ESC("A escolha automática prioriza",   V_FONTE_PRIORIDADE, 3), // local: fontePrioridadeLocal
  ESC("HDR e Dolby Vision",              V_FONTE_HDR, 3),        // local: fonteHdrLocal
  ESC("Logo do app",                     V_LOGO_APP, 2),         // local: logoAppLocal
  ESC("Abertura do app",                 V_ABERTURA, 3),         // local: aberturaAppLocal
  ESC("Receber enquetes",                V_LIGA, 2),             // local: enquetesLocal (espelho do opt-out da conta)
  ESC("Buscar no Cinemeta",              V_LIGA, 2),             // local: buscaCinemetaLocal
  ESC("Tela de descanso",                V_ESMAECER, 6),         // local: esmaecerLocal
  ESC("Brilho da interface no player",   V_BRILHO_PLAYER, 4),    // local: brilhoPlayerLocal
  ACAO("Novidades 2.0"),
  ESC("Manter o vídeo pronto ao sair",   V_LIGA, 2),             // local: manterVideoLocal
  ESC("Estilo do descanso",              V_DESCANSO_ESTILO, 3),  // local: descansoEstiloLocal
  ESC("Títulos da vitrine",              V_DESCANSO_FONTE, 2),   // local: descansoFonteLocal
  ESC("Formato do relógio",              V_RELOGIO_12H, 2),      // local: relogio12hLocal
  ESC("Tamanho máximo",                  V_TAMANHO_GB, 7),       // local: tamanhoMaxLocal
  ESC("Tamanho mínimo",                  V_TAMANHO_GB, 7),       // local: tamanhoMinLocal
  ACAO("Apoiar o projeto"),
  ESC("Legenda forçada automática quando o áudio for no seu idioma", V_LIGA, 2), // local: legendaForcadaLocal
  ESC("Classificação no player",         V_LIGA, 2),             // local: classifPlayerLocal
  ESC("Fontes do automático",            V_FONTE_ESCOPO, 3),  // por perfil: stream_auto_play_source
  ACAO("Add-ons permitidos"),                                  // fonteregra.c (conta)
  ACAO("Plugins permitidos"),                                  // fonteregra.c (conta)
  ESC("Usar os outros se não houver",    V_LIGA, 2),          // por perfil: fonteOutrosLocal
  ESC("Coincidência por regex",          V_FONTE_REGEX, 3),   // por perfil: fonteRegexLocal
  ACAO("Padrão da regex"),                                     // fonteregra.c (conta)
  ESC("Modelo de regex",                 V_FONTE_MODELO, 7),  // espelho de fonteregra_modelo_atual
  NUM("Espaço entre fileiras",           50, 150, 10, "%"),     // local: espacoFileirasLocal
  NUM("Espaço entre títulos",            50, 150, 10, "%"),     // local: espacoTitulosLocal
  ESC("Proporção padrão",                V_PROPORCAO, 9),        // local: proporcaoPadraoLocal
  ESC("Também em Continuar assistindo",  V_LIGA, 2),             // local: cwRetidoTambemLocal
  ESC("Continuar na escolha de perfil",  V_LIGA, 2),             // local: psContinuarLocal
  ESC("DTS convertido em",               V_DTS_SAIDA, 2),        // local: dtsSaidaLocal
  ESC("Dolby Vision em MKV (experimental)", V_LIGA, 2),          // local: dvMkvLocal
  ESC("Enviar histórico para a conta Nuvio", V_LIGA, 2),          // por perfil: histContaLocal
  ESC("Recursos sociais",                V_LIGA, 2),             // por perfil: socialLocal
  ACAO("Ordem dos add-ons"),                                   // fonteregra.c (local)
  ESC("Usar a ordem",                    V_FONTE_ORDEM_USO, 3), // por perfil: fonteOrdemUsoLocal
  ESC("Tocar enquanto confere a fonte", V_LIGA, 2),   // local: fonteTocarConferindoLocal
  ESC("Aquecer conexões ao abrir o título", V_LIGA, 2),   // local: fonteAquecerLocal
  ESC("Conferir várias fontes ao mesmo tempo", V_LIGA, 2),   // local: fonteConferirVariasLocal
  ESC("Preparar a fonte ao abrir o título", V_LIGA, 2),   // local: fontePrepararLocal
  ESC("Próximo episódio no Continuar",   V_LIGA, 2),   // por perfil: cwProximoLocal
  ESC("Resultados do catálogo do Nuvio na busca", V_BUSCA_NUVIO, 3), // local: buscaNuvioLocal
  ESC("Mostrar a fonte nos resultados da busca", V_LIGA, 2),   // local: buscaOrigemLocal
  ESC("Layout dos Ajustes",              V_LAYOUT_AJUSTES, 2),   // local: ajustesLayoutLocal (#339)
  ESC("Limite de espaço do P2P",         V_P2P_LIMITE, 5),   // local: p2pLimiteLocal (#334)
  ESC("Ordem das fontes", V_FONTE_ORDEM_ADDON, 2), // local (#400)
  ESC("Sincronia automática da legenda", V_LIGA, 2),
};

// Nome de cada opcao no arquivo. O formato era POSICIONAL — uma linha por
// opcao, na ordem do enum — e por isso acrescentar uma opcao no meio fazia o
// arquivo de quem ja tinha o app aplicar os valores errados, em silencio. Com
// chave por linha, opcao nova nasce no padrao e as antigas continuam onde
// estavam. Os nomes seguem os do app web onde existe correspondente.
static const char *CHAVE[] = {
  "qualidade", "dolbyVision", "dolbyAtmos",
  "legendaIdioma", "audioIdioma", "pauseOverlayEnabled",
  // Sem "-": grava no ajustes.txt como qualquer outra. Nao tem equivalente na
  // conta (o app web nao expoe esta escolha), entao o blob simplesmente nao
  // traz a chave e o valor local fica de pe.
  "escolherFonteManual",
  // LOCAIS (#130), mesma razao: o app web nao tem estas escolhas.
  "fonteAutoLocal", "fonteReporLocal", "fonteTextoLocal",
  "modernLandscapePostersEnabled", "modernHeroFullScreenBackdropEnabled", "heroFundoLocal",
  // LOCAL, e em ingles como pedido: o app web nao tem esta escolha (grep em
  // NuvioWeb 0.3.38 por hero/backdrop: so buildHeroBackdropSources, sem
  // preferencia), entao nao ha chave dele para reusar.
  "heroDifferentFromCard", "trailerHero",
  // "-": local, nao vem da conta e nao vai para ajustes.txt. Os dois vivem em
  // fileirasui.txt (fileiras.c) e a conta nao tem chave equivalente — o teto do
  // web para este runtime e uma CONSTANTE (HOME_MAX_ROWS_LEGACY_TV), nao uma
  // preferencia, e a ordem da conta nunca pode ser ESCRITA pela TV.
  "-limiteFileiras", "-ordenarFileiras",
  // LOCAL (#95): nao ha chave no blob da conta — o app web nao expoe isto.
  "collapseSidebar", "modernSidebar", "modernSidebarBlur",
  "heroSectionEnabled", "-heroCatalogKeys",
  // LOCAL, e nao do web: o app oficial nao tem tela de escolha de perfil com
  // fundo de arte, entao nao ha campo equivalente no blob da conta.
  "perfilFundoLocal",
  "discoverLocation", "posterLabelsEnabled", "catalogAddonNameEnabled",
  "catalogTypeSuffixEnabled", "hideUnreleasedContent",
  "homeImdbRatingsVisibility", "classicFocusGradientEnabled",
  "continueWatchingEnabled",
  // LOCAL, e nao do web: o app oficial nao tem esta escolha, entao nao ha
  // campo dela no blob da conta. Chave propria para nao colidir com um nome
  // que o servidor possa criar depois.
  "cwOkLocal",
  // LOCAL, e nao do web: o app oficial nao tem esta escolha, entao nao ha
  // campo dela no blob da conta. Chave propria para nao colidir com um nome
  // que o servidor possa criar depois.
  "cwFonteLocal",
  "continueWatchingCardStyle",
  "useEpisodeThumbnailsInCw", "blurContinueWatchingNextUp",
  "nextUpFromFurthestEpisode", "showUnairedNextUp", "continueWatchingSortMode",
  "blurUnwatchedEpisodes", "detailPageTrailerButtonEnabled",
  "preferExternalMetaAddonDetail", "showFullReleaseDate", "detalheVeu", "trailerAuto", "trailerQualidade", "trailerAspecto",
  // LOCAL: o app web nao tem esta escolha (NuvioWeb 0.3.38: trailerSource e
  // estado interno da tela de detalhe, nenhuma chave em settings/), entao o
  // nome e proprio e somenteDesteAparelho o segura aqui.
  "trailerFonteLocal",
  "focusedPosterBackdropExpandEnabled", "focusedPosterBackdropExpandDelaySeconds",
  "fastHorizontalNavigationEnabled",
  "bordaFocoCartaz",
  "cardDepthEnabled", "cardDepthEdgeStrength", "cardDepthSheenStrength",
  "cardDepthEdgeCoverage", "cardDepthPostersEnabled",
  "cardDepthContinueWatchingEnabled", "cardDepthEpisodeCardsEnabled",
  "cardDepthCastEnabled", "cardDepthTrailersEnabled",
  "posterCardWidthDp", "posterCardCornerRadiusDp", "qualidadeImagem",
  "idioma", "animacoes", "resolucaoUi",
  // A conta JA MANDAVA esta chave e o app a jogava fora: ela vem dentro de
  // theme_settings no blob de ajustes (profileSettingsSyncService.js), e o
  // laco de ajustes_aplicar_blob so procura as chaves que estao nesta lista.
  // Quem escolher JADE no app web ganha o anel jade na TV sem configurar de
  // novo — e o contrario tambem vale.
  "selected_theme",
  // LOCAL, sem o "-" para sobreviver ao fechamento: o web nao tem esta escolha.
  "corDaLogoLocal",
  // Conta: sao linhas locais, nao vem nem vao para o perfil na nuvem.
  // "salvosDestino" e LOCAL como cwFonteLocal, e por isso SEM o "-": o app
  // oficial nao tem esta escolha, entao nao ha campo dela no blob da conta —
  // mas ela precisa sobreviver ao fechamento, e gravar() pula toda chave
  // iniciada por "-".
  "-perfil", "-sync", "-addons",
  // Portal IPTV: os VALORES moram em stalker-p<N>.txt, por perfil, porque um
  // deles e credencial (ver stalker.h). Aqui sao so linhas de tela, e por isso
  // levam "-": nada delas entra no ajustes.txt nem no blob da conta.
  "-stalkerPortal", "-stalkerMac", "-stalkerLimpar",
  "-xtreamServidor", "-xtreamUsuario", "-xtreamSenha", "-xtreamLimpar",
  // A grade e LOCAL e sem "-": o app oficial nao tem a escolha (#158).
  "-xtreamConta", "epgPaisLocal",
  "salvosDestino", "-trakt", "-simkl", "-sair",
  "-versao", "-atualizar", "-registro", "envioAuto", "-espaco", "texturasMB",
  // Integracoes: os nomes sao exatamente os que profileSettingsSyncService.js
  // exporta dentro de tmdb_settings / mdblist_settings — a conta aplica e a
  // TV respeita a escolha feita no app web, e vice-versa.
  "tmdb_enabled", "tmdb_language", "tmdb_use_artwork", "tmdb_use_basic_info",
  "tmdb_use_details",
  "tmdb_use_release_dates", "tmdb_use_credits", "tmdb_use_productions",
  "tmdb_use_networks", "tmdb_use_episodes", "tmdb_use_trailers",
  "tmdb_use_more_like_this", "tmdb_use_collections",
  "tmdb_enrich_continue_watching",
  // A chave do mdblist chega pelas CREDENCIAIS da conta (sync.c), nao pelo
  // blob de ajustes; a linha aqui so mostra o estado, por isso o "-".
  "mdblist_enabled", "-mdblistChave",
  "mdblist_show_trakt", "mdblist_show_imdb", "mdblist_show_tmdb",
  "mdblist_show_letterboxd", "mdblist_show_tomatoes", "mdblist_show_audience",
  "mdblist_show_metacritic", "mdblist_show_mal",
  // "-": o VALOR mora em fanart.txt (dados), por aparelho; e credencial, nao
  // entra no ajustes.txt nem no blob da conta.
  "-fanartChave",
  "-diagnostico",
  "-velocidade",
  // Locais e SEM o "-": sobrevivem ao fechamento, e o web nao tem a escolha.
  "menuExplorarLocal", "menuGuiaLocal", "menuAgendaLocal", "menuPerfilLocal",
  // A MESMA chave do web (layoutPreferences), entao segue a conta.
  "focusedPosterBackdropTrailerEnabled",
  "itensFileiraLocal",
  "fonteInterface",
  "gpuEfeitosLocal",
  // LOCAL e SEM o "-": o web nao tem esta escolha e ela precisa sobreviver.
  "vidroLocal",
  // Ligado: LOCAL e SEM o "-" (sobrevive ao fechamento). O endereco mora em
  // p2p.txt (dados), por aparelho; o teste e so uma acao.
  "p2pLocal", "-p2pEndereco", "-p2pTestar",
  // Escolha LOCAL e sem "-". Os campos moram em posteres.txt (dados), por
  // aparelho, e o teste e so uma acao.
  "posterProvLocal", "-posterInst", "-posterToken", "-posterExtra",
  "-posterChave", "-posterModelo", "-posterTestar",
  // LOCAL e SEM o "-": a Dinamica nao existe na conta (ver V_HOME_LAYOUT).
  "homeLayoutLocal",
  // Credenciais: moram em debrid.txt (dados), por aparelho, nunca aqui.
  "-debridAD", "-debridADTestar", "-debridRD", "-debridTB", "-debridPM",
  // LOCAL e SEM o "-": o web nao tem esta escolha e ela precisa sobreviver.
  "soCinemetaLocal",
  // LOCAIS e SEM o "-": sobrevivem ao fechamento; o web nao tem a linha.
  "notaTituloImdb", "notaTituloTomates", "notaTituloAudiencia", "notaTituloMeta",
  "notaTituloMetaUser", "notaTituloTrakt", "notaTituloTmdb", "notaTituloLetter",
  "notaTituloMal", "notaTituloEbert", "notaTituloScore",
  // Sem gravar: o estado vive em recomendacoes-perfil.txt (recomenda.c), por conta.
  "-perfilPesquisavel", "-perfilEditar",
  // LOCAL e SEM o "-": visual desta TV, como a propria Interface de vidro.
  "vidroContornoLocal",
  // Locais: o app oficial nao tem estas duas escolhas.
  "trailerDestaqueSomLocal", "trailerDestaqueEsperaLocal",
  // LOCAL e SEM o "-": o web nao tem esta escolha e ela precisa sobreviver.
  "cwConcluidoLocal",
  // LOCAL e SEM o "-": escolha desta TV, somada a marca uses_primary_addons da
  // conta (ver perfis_ativo_addons).
  "addonsPrincipalLocal",
  // LOCAL e SEM o "-": o web nao tem esta escolha.
  "heroTransicaoLocal",
  // LOCAIS e SEM o "-": o web nao tem estas escolhas e elas precisam sobreviver.
  "posterAddonLocal", "fundoAddonLocal", "logoAddonLocal", "colArteContaLocal",
  // LOCAL e SEM o "-": o web nao tem esta escolha (la a cor vem do pacote).
  "selosColoridosLocal",
  // LOCAL e SEM o "-": o web nao tem esta escolha.
  "trailerDetalheSomLocal",
  // LOCAIS e SEM o "-": a rede, o provedor e a TV sao desta casa.
  "liveTvResolucaoLocal", "xtreamFormatoLocal", "liveTvEsperaLocal",
  "-liveTvDiag",
  "liveTvModoLocal",
  "liveTvProxyLocal",
  "relogioTelaLocal",
  "relogioPosLocal",
  "seloVistoLocal",
  // LOCAL e SEM o "-"; a chave e credencial e mora em seekr.txt (dados).
  "seekrLocal", "-seekrChave", "-seekrTestar",
  "seekrFitaLocal", "seekrAjusteLocal",
  "saidaPlayerLocal",
  // LOCAL e SEM o "-": o web nao tem a pergunta.
  "reacaoCreditosLocal",
  "fonteEsperaLocal",
  // "-": acao, nao grava. O medidor e LOCAL e SEM o "-": e desta TV.
  "-verRegistro", "medidorFormaLocal",   // era "medidorDesempenhoLocal" (V_LIGA): ver ajustes_dir
  "-guiaUso",
  // "-": a escolha mora em selos-p<N>.txt (por perfil), nao em ajustes.txt.
  "-selosPacote", "-selosPacoteAdd", "-selosPacoteRem",
  "tamanhoUiLocal",
  "fundoLocal",
  "vidroOpacLocal", "vidroFoscoLocal",
  "iconeAppLocal",
  "-discord",
  "tamanhoAjustesLocal",
  "logoTrailerLocal",
  "legendaSecundariaIdioma",
  "legendaSyncAudioLocal",
  "cacheSeekLocal",
  "trailerZoomTpkLocal",
  "-plugins",
  // LOCAL e SEM o "-" (sobrevive ao fechamento); o resto e acao/estado.
  "jellyfinLocal", "-jellyfinServidor", "-jellyfinEntrar", "-jellyfinSair",
  "avancadasLocal",
  "legenda2PosLocal", "legenda2TamanhoLocal", "legenda2CorLocal", "legenda2FundoLocal", "legenda2BordaLocal",
  "-embyServidor", "-embyEntrar", "-embySair", "-plexEntrar", "-plexServidor", "-plexSair",
  "fontePrioridadeLocal", "fonteHdrLocal",
  "logoAppLocal", "aberturaAppLocal",
  "enquetesLocal",
  "buscaCinemetaLocal",
  "esmaecerLocal", "brilhoPlayerLocal",
  "-novidades20",
  "manterVideoLocal",
  "descansoEstiloLocal", "descansoFonteLocal",
  "relogio12hLocal",
  "tamanhoMaxLocal", "tamanhoMinLocal",
  "-apoiar",
  "legendaForcadaLocal",
  "classifPlayerLocal",
  // #202. O escopo usa o NOME do oficial (stream_auto_play_source no blob)
  // para ir e voltar da conta; as listas e o padrao sao de fonteregra.c. O
  // "usar os outros" e o modo da regex o oficial nao tem: nascem POR PERFIL
  // (nao estao em somenteDesteAparelho) e o blob simplesmente nao os traz.
  "streamAutoPlaySource", "-fonteAddonsPermitidos", "-fontePluginsPermitidos",
  "fonteOutrosLocal", "fonteRegexLocal", "-fonteRegexPadrao", "-fonteRegexModelo",
  "espacoFileirasLocal", "espacoTitulosLocal",
  "proporcaoPadraoLocal",
  "cwRetidoTambemLocal",
  "psContinuarLocal",
  "dtsSaidaLocal",
  "dvMkvLocal",
  "histContaLocal",
  "socialLocal",
  "-fonteOrdem", "fonteOrdemUsoLocal",
  "fonteTocarConferindoLocal",
  "fonteAquecerLocal",
  "fonteConferirVariasLocal",
  "fontePrepararLocal",
  "cwProximoLocal",
  "buscaNuvioLocal", "buscaOrigemLocal",
  "ajustesLayoutLocal",
  "p2pLimiteLocal",
  "fonteOrdemLocal",
  "legendaSyncAutoLocal",
};
// QUATRO VETORES PARALELOS indexados pelo mesmo enum AJ_*: OPCOES, CHAVE,
// valor e as secoes. OPCOES ja e declarado [AJ_N], e `valor` aceita inicializacao
// parcial em silencio — CHAVE nao tem nenhuma protecao. Inserir uma opcao no
// meio do enum e esquecer UMA das listas desloca todas as seguintes: a chave de
// um ajuste passa a gravar o valor de outro, e o arquivo de quem ja usava o app
// volta trocado. Barato de conferir, caro de descobrir.
_Static_assert(sizeof CHAVE / sizeof *CHAVE == AJ_N,
               "CHAVE fora de sincronia com o enum AJ_*");


// O compilador CONFERE que ha uma chave por opcao. Sem isto, acrescentar uma
// opcao no enum e esquecer a chave deixa as ultimas entradas em NULL e
// DESALINHA todas as chaves depois do ponto de insercao — e o defeito nao
// aparece na hora: so quando o ajustes.txt passa a existir, o strcmp(NULL,...)
// derruba o app no arranque seguinte. Foi exatamente o que aconteceu, e o
// unico sintoma na TV foi o app abrir e fechar.
typedef char conferi_uma_chave_por_opcao[
  (sizeof CHAVE / sizeof *CHAVE == AJ_N) ? 1 : -1];

// A TELA, EM ORDEM DE LEITURA — separada do enum de proposito.
//
// Ate a 1.4.4 a tela era o proprio enum fatiado em faixas contiguas (SECOES):
// uma opcao so podia aparecer onde tinha sido declarada. Foi isso que deixou
// "Interface e conta" com 24 linhas misturando idioma, portal IPTV, Trakt,
// versao e memoria de imagens — mover uma linha de categoria exigia mexer no
// enum, e o enum e o INDICE do vetor posicional de padroes (`valor[]`) e de
// CHAVE[]. Relato do dono (25/09/2026): "a interface de settings ta confusa —
// pega como ta no web oficial e transpoe".
//
// Agora o enum continua como estava (nenhuma chave, nenhum padrao, nenhum
// indice gravado muda) e esta tabela diz so ONDE cada opcao aparece. A
// arquitetura e a do app web (SECTION_META de settingsScreen.js, na mesma
// ordem, sem as secoes que este app nao tem): Conta, Aparencia, Layout,
// Conteudo, Integracoes, Reproducao, Trakt e Simkl, Avancado, Sobre. Dentro de
// Layout e Integracoes os blocos sao GRUPOS RECOLHIVEIS, como os
// `settings-group` do web (createDefaultExpandedState: todos fechados) — a
// secao Layout tem 51 linhas, e mostrada inteira ela volta a ser "muita lista".
//
// Quatro tipos de item:
//   SEC  cabecalho de categoria: nome na coluna da esquerda, titulo + subtitulo
//        no topo da lista (o `settings-content-header` do web);
//   GRP  grupo recolhivel: linha com foco, titulo + descricao, OK abre/fecha;
//   ROT  rotulo fixo de bloco, sem foco — para categorias curtas, onde fechar
//        as linhas atras de um OK a mais so esconderia o que se usa;
//   OPC  uma opcao do enum.
// Toda opcao do enum aparece aqui UMA vez: conferirTela() grita no log no
// arranque e tests/ajustes_secoes.sh falha na suite.
// IT_MAIS (2.0.3): a linha "Mais opcoes · N" no fim da categoria. Tudo o que
// vem depois dela, na mesma categoria, e avancado (uxAvancada).
typedef enum { IT_SEC, IT_GRP, IT_ROT, IT_OPC, IT_MAIS } ItemTipo;
typedef struct {
  ItemTipo    tipo;
  int         op;               // IT_OPC
  const char *titulo, *sub;     // SEC/GRP/ROT; `sub` e o subtitulo (SEC, GRP)
  const char *icone;            // SEC, GRP: basename em art/icones (aj_* do Lucide)
} Item;
#define SEC(t, s, ic) { IT_SEC, -1, t, s, ic }
#define GRP(t, s, ic) { IT_GRP, -1, t, s, ic }
#define ROT(t)        { IT_ROT, -1, t, NULL, NULL }
// Rotulo com a nota a direita no lugar da contagem ("para quem faz o app").
#define ROTS(t, s)    { IT_ROT, -1, t, s, NULL }
#define OPC(o)        { IT_OPC, o, NULL, NULL, NULL }
#define MAIS()        { IT_MAIS, -1, "Mais opções", NULL, "aj_sliders-horizontal" }

#include "ajustes_ux_tela.inc"

// Derivados de TELA uma vez, no arranque (montarTela). Sao so indices: nada
// aqui e estado de usuario.
static int nSecoes;
static int secIni[AJ_MAX_SECOES];      // indice em TELA do SEC de cada categoria
static int secDoItem[AJ_N_TELA];       // categoria de cada item
static int grupoDoItem[AJ_N_TELA];     // GRP que contem o item (-1 = nenhum)
static int telaMontada;
// Grupo aberto em cada categoria (indice em TELA, -1 = todos fechados). UM por
// categoria, em sanfona: o web deixa abrir varios, mas no sofa dois grupos
// abertos ja sao uma lista de trinta linhas — o problema que os grupos existem
// para resolver. Dura a sessao do app, para voltar a uma categoria e achar o
// bloco onde estava.
static int grupoAberto[AJ_MAX_SECOES];
// "Mais opcoes" aberto em cada categoria (OK na linha IT_MAIS, ou um atalho que
// leva a uma avancada). Dura a sessao; nao vai para o disco.
static int maisAberto[AJ_MAX_SECOES];
static int maisItem[AJ_MAX_SECOES];   // indice em TELA do IT_MAIS da categoria (-1 = nao tem)

static void montarTela(void) {
  int i, s = -1, g = -1, depois = 0;
  if (telaMontada) return;
  memset(uxAvancadaTab, 0, sizeof uxAvancadaTab);
  for (i = 0; i < AJ_MAX_SECOES; i++) maisItem[i] = -1;
  for (i = 0; i < AJ_N_TELA; i++) {
    const Item *it = &TELA[i];
    if (it->tipo == IT_SEC) {
      if (s + 1 < AJ_MAX_SECOES) secIni[++s] = i;
      g = -1; depois = 0;
    } else if (it->tipo == IT_GRP) g = i;
    else if (it->tipo == IT_ROT) g = -1;   // rotulo fecha o grupo anterior
    else if (it->tipo == IT_MAIS) { depois = 1; if (s >= 0) maisItem[s] = i; }
    else if (it->tipo == IT_OPC && depois && it->op >= 0 && it->op < AJ_N) uxAvancadaTab[it->op] = 1;
    secDoItem[i] = s < 0 ? 0 : s;
    grupoDoItem[i] = it->tipo == IT_OPC ? g : -1;
  }
  nSecoes = s + 1;
  for (i = 0; i < AJ_MAX_SECOES; i++) grupoAberto[i] = -1;
  telaMontada = 1;
}
static int secFim(int s) { return s + 1 < nSecoes ? secIni[s + 1] : AJ_N_TELA; }

// Valor de cada opcao. Para OP_ESCOLHA e o indice; para OP_NUMERO e o proprio
// numero. Os padroes sao os DEFAULTS de layoutPreferences.js, com UMA excecao
// anotada linha a linha: as quatro que o perfil do dono diverge de fabrica
// nascem como ele as deixou, porque e o que ele ve hoje. Todas sao trocaveis
// aqui, que era o ponto.
// Definidas mais abaixo, junto do desenho das linhas; declaradas aqui porque a
// leitura do arquivo e o tratamento de tecla vem antes no arquivo.
static int  nValores(int op);
// Definida junto da leitura do arquivo, bem abaixo; declarada aqui porque o
// setter de "onde o + salva" grava na hora e vem antes dela.
static int inativa(int op);
static void pstAplicar(void);
static void pstCarregar(void);
static void pstDefinir(int op, const char *texto);
static void pstAtivar(int op);
static void pstTesteRecolher(void);
static const char *pstTexto(int op);
static int gravar(void);
static void aplicarIdioma(int op);
static int somenteDesteAparelho(int op);
// Segundos restantes antes de refazer a busca de legendas. Declarada aqui, e
// nao junto de aplicarIdioma, porque ajustes_atualizar a le e vem ANTES dela no
// arquivo. Ver o comentario em aplicarIdioma.
static float legendaEspera;

// SEM TAMANHO DECLARADO, de proposito, e conferido logo depois da lista: com
// `valor[AJ_N]` a inicializacao parcial passava em silencio e as ultimas
// opcoes ficavam no zero. Foi o que aconteceu com as sete linhas do Stalker e
// do Xtream (#149, ver a nota delas abaixo).
static int valor[] = {
#include "ajustes_ux_padrao.inc"
};
static const int valorPadrao[] = {
#include "ajustes_ux_padrao.inc"
};
_Static_assert(sizeof valor / sizeof *valor == AJ_N,
               "valor[]: um padrao por opcao do enum AJ_*, na ordem dele");
// LOG DE MUDANCA (ajlog.h): gravar() e o unico lugar por onde todo valor muda de
// verdade, entao ele compara `valor[]` com o que gravou da ultima vez. A origem
// so existe quando o caminho que chamou diz quem mexeu (ajustes, central,
// primeira); sem ela (carga, migracao, conta, espelhos) so a sombra se atualiza.
static int ajSombra[AJ_N];
static int ajSombraOk;
static int ajOrigem;   // AjlogOrigem; 0 = silencioso
static void ajSombraSync(void) { memcpy(ajSombra, valor, sizeof ajSombra); ajSombraOk = 1; }
static void ajLogDiff(void) {
  int i;
  Uint32 agora = SDL_GetTicks();
  if (!ajSombraOk) { ajSombraSync(); return; }
  for (i = 0; i < AJ_N; i++) {
    if (ajSombra[i] == valor[i]) continue;
    // Espelhos recalculados por quadro: nao sao escolha de ninguem.
    if (i != AJ_PERFIL_PESQ && i != AJ_FONTE_REGEX_MODELO && ajOrigem && CHAVE[i] && CHAVE[i][0] != '-' &&
        OPCOES[i].tipo != OP_LEITURA && OPCOES[i].tipo != OP_ACAO)
      ajlog_mudou(CHAVE[i], ajSombra[i], valor[i], (AjlogOrigem)ajOrigem, agora);
  }
  ajSombraSync();
}
void ajustes_log_vazar(void) { ajlog_vazar(SDL_GetTicks()); }
void ajustes_log_vazar_tudo(void) { ajlog_vazar_tudo(); }
// Roda `chamada` com a origem marcada e a devolve ao silencio.
#define AJ_COM_ORIGEM(o, chamada) do { ajOrigem = (o); chamada; ajOrigem = 0; } while (0)



// Pedido de abrir a lista de addons, lido e zerado pelo app.c. A tela nao e
// aberta daqui porque quem troca de tela e o app.c — ajustes.c nao conhece as
// outras telas, e ganhar essa dependencia agora era o comeco de um no.
static int pediuAddons;
int ajustes_pediu_addons(void) { int v = pediuAddons; pediuAddons = 0; return v; }
static int pediuPlugins;
int ajustes_pediu_plugins(void) { int v = pediuPlugins; pediuPlugins = 0; return v; }
static int pediuNovidades20;
int ajustes_pediu_novidades20(void) { int v = pediuNovidades20; pediuNovidades20 = 0; return v; }
static int pediuDiagnostico;
int ajustes_pediu_diagnostico(void) { int v = pediuDiagnostico; pediuDiagnostico = 0; return v; }
static int pediuVelocidade;
int ajustes_pediu_velocidade(void) { int v = pediuVelocidade; pediuVelocidade = 0; return v; }
static int pediuLivetvDiag;
int ajustes_pediu_livetv_diag(void) { int v = pediuLivetvDiag; pediuLivetvDiag = 0; return v; }

// O FOCO E UM ITEM DE TELA[], nao uma opcao: um grupo recolhivel tambem
// recebe foco. `focoOp` e o derivado que o resto do arquivo le (ajuda, efeito,
// previa, acoes): a opcao em foco, ou -1 com o foco num grupo. So focar() os
// escreve, e por isso os dois nunca discordam.
static int focoItem = 0;
static int focoOp = -1;
// Pedido do cartao de novidades ("Experimentar a cor viva"): a proxima
// abertura pousa na linha da cor, dentro de Aparencia (ver ajustes_iniciar).
static int abrirNaCor, abrirNaFonte, abrirNoLayout, abrirNoVidro, abrirNoTrakt, abrirNaEspera;
void ajustes_abrir_na_cor(void) { abrirNaCor = 1; }
void ajustes_abrir_na_fonte(void) { abrirNaFonte = 1; }
void ajustes_abrir_no_layout(void) { abrirNoLayout = 1; }
void ajustes_abrir_no_vidro(void) { abrirNoVidro = 1; }
void ajustes_abrir_no_trakt(void) { abrirNoTrakt = 1; }
void ajustes_abrir_na_espera_fonte(void) { abrirNaEspera = 1; }
static int abrirNoGuia, guiaDaNovidades, pediuNovidades;
void ajustes_abrir_no_guia(int daNovidades) { abrirNoGuia = 1; guiaDaNovidades = daNovidades ? 1 : 0; }
int  ajustes_pediu_novidades(void) { int v = pediuNovidades; pediuNovidades = 0; return v; }
// A tela do Guia de uso (ajustes_ux_guia.inc), aberta por cima das ilhas.
static void guiaAbrir(int daNov);
static void guiaFechar(void);
static void guiaEvento(const SDL_Event *e);
static void guiaDesenhar(void);
static void guiaAtualizar(float dt);
static int guiaAberto;
static int apoioAberto;   // painel com os QRs de "Apoiar o projeto" (ajustes_ux_apoio.inc)
static void apoioAbrir(void);
static void apoioEvento(const SDL_Event *e);
static void apoioDesenhar(void);
int  ajustes_opcao_em_foco(void) { return focoOp; }
// Categoria mostrada na lista. Com o foco no indice ela e a categoria em foco
// la; com o foco na lista, a do item.
static int secAtual = 0;
// 1 = o foco esta na COLUNA DE CATEGORIAS. E ONDE A TELA ABRE (como o web, que
// abre na navegacao lateral): a primeira coisa que a pessoa ve e o mapa das
// nove categorias, e nao a quinta linha de uma delas.
static int focoIndice = 1;
int ajustes_foco_no_indice(void) { return focoIndice; }
// ACOES QUE APAGAM pedem o OK DUAS vezes: o primeiro arma, qualquer outra
// tecla desarma. "Sair da conta" apaga sessao, addons e progresso desta TV (um
// OK errado custava um novo login por QR); remover o portal Stalker ou o
// Xtream apaga uma credencial que foi digitada letra a letra no D-pad.
// `sairArmado` guarda QUAL linha esta armada (0 = nenhuma; nenhuma das tres
// e a opcao 0 do enum).
static int sairArmado;
static int pedeConfirmacao(int op) {
  return op == AJ_SAIR || op == AJ_STALKER_LIMPAR || op == AJ_XTREAM_LIMPAR || op == AJ_JF_SAIR ||
         op == AJ_EM_SAIR || op == AJ_PX_SAIR;
}

// --- folha "Ordenar e ativar fileiras" --------------------------------------
// Modal dentro desta tela, e nao uma tela nova: quem troca de tela e o app.c e
// ajustes.c nao conhece as outras telas (a mesma razao registrada em
// ajustes_pediu_addons). Uma folha aqui nao pede nada ao app.c.
#define AJ_FIL_CAMPOS 4          // nome/mover, estado, card, tamanho
static int filAberta;
static int filFoco;
static int filCampo;
// 0 = nada na mao, 1 = fileira na mao, 2 = BLOCO do addon na mao.
// O bloco e o conjunto de fileiras contiguas do mesmo addon — mover o bloco
// inteiro e o "mover de uma vez" que a pessoa pede quando ha 60 fileiras de 5
// addons e ela quer subir "o Xperience" sem subir cada catalogo dele.
static int filPegou;
static int filPegouDe;           // de onde ele saiu, para Voltar cancelar
// MODO EDICAO da lista principal. Antes esquerda/direita trocava o valor da
// linha em foco DIRETO — cada toque de navegacao que errasse a linha mudava
// um ajuste sem a pessoa pedir ("fica estranho, nao intuitivo"). Agora OK
// trava a linha para edicao e so entao as setas ajustam; OK ou Voltar soltam.
static int emEdicao;
static int filTopo;              // primeira linha desenhada (rolagem)
// Uma lista de UMA coluna nao precisa do focus.h: a memoria de coluna que ele
// existe para resolver nao tem o que lembrar aqui, e o indice cru deixa o
// "pula o cabecalho da secao" ser uma soma em vez de um mapa de fileiras.
static float animItem[AJ_N_TELA];
static float scrollY = 0.0f;
// Velocidade da mola de 2a ordem da rolagem (anim_mola2): partida macia e
// cauda exponencial, a MESMA curva que a home mede. A de 1a ordem que estava
// aqui partia na velocidade maxima e o primeiro quadro ja saltava 12%.
static float velY = 0.0f;
static float paginaA = 1.0f;   // entrada da pagina da categoria (0..1)
static int sair = 0;

// Rascunho e navegação separados dos valores persistentes.
static int uxIndice = 2;
static int uxChipAv;   // (menu antigo) foco no chip "Avancadas"; a grade da 2.0.3 usa uxTopo
// 2.0.3: o alvo em foco no ALTO da grade (AJ2_T_*: Buscar, Avancadas, Perfil,
// O que o Nuvio faz?, Resolver um problema), -1 = uma categoria (uxIndice).
static int uxTopo = -1;
// #339: o foco no segmentado "Painel | Lista" do cabecalho da categoria (a
// linha focoItem segue a mesma, so descansa).
static int uxCabLayout;
static int guiaDoIndice;   // o guia foi aberto por um cartao da grade: Voltar volta para ele
// A fileira de cada pilula do alto (0 Buscar, 1 Diferentes, 2 Avancadas): quem
// mede e o desenho (aj2ChipsMedir, pelo texto traduzido); a navegacao so le.
static int uxChipLinha[3];
static int uxUltimoItem[AJ_MAX_SECOES];
static int uxAbrirOp = -1, uxPediuBusca, uxVeioBusca, uxRetornarOp = -1;
// 2.0.3: ao voltar das listas de addons/plugins, Ajustes reabre na categoria
// de onde elas sairam, com o foco na linha (e nao no indice).
static int uxVoltarOp = -1;
void ajustes_voltar_de_lista(int plugins) { uxVoltarOp = plugins ? AJ_PLUGINS : AJ_ADDONS; }
static int uxDifs[AJ_N], uxNDifs, uxDifFoco;
static int uxEditor, uxOp, uxPendente, uxOriginal, uxRodape;
static int uxRestaurar, uxConfirmar, uxAvisoRisco;
int  ajustes_teste_editor(int *pendente, int *rodape, int *restaurar, int *confirmar) {
  if (pendente) *pendente = uxPendente;
  if (rodape) *rodape = uxRodape;
  if (restaurar) *restaurar = uxRestaurar;
  if (confirmar) *confirmar = uxConfirmar;
  return uxEditor;
}
static char uxAviso[160];
static Uint32 uxAvisoAte;
static const char *textoValor(int op);
static int uxTemPadrao(int op);
static int uxDiferente(int op);
static void uxCancelar(void);

int ajustes_pediu_busca(void) { int p = uxPediuBusca; uxPediuBusca = 0; return p; }
void ajustes_abrir_opcao(int op) {
  if (op < 0 || op >= AJ_N) return;
  uxAbrirOp = op; uxVeioBusca = 1;
}


// Rotulo da fonte do destaque (fil_hero_fonte). Definida junto da folha de
// fileiras, mais abaixo.
static const char *heroFonteRotulo(void);
static void heroFonteCiclar(int dir);

static int lig(int op)  { return valor[op] == 0; }
// #311: "Buscar no Cinemeta" so governa a BUSCA (o add-on Cinemeta e a reserva
// do catalogo do Nuvio). Sem o add-on instalado e com o ajuste no padrao ele nao
// tem o que mostrar, e a pessoa o lia como "esconder o Cinemeta da Home": some da
// folha. Desligado continua visivel, para poder ser religado.
static int cinemetaInstalado(void) {
  int i, n = addons_n();
  for (i = 0; i < n; i++) {
    const char *b = addons_base(i);
    if (b && strstr(b, "cinemeta")) return 1;
  }
  return 0;
}

int ajustes_animacoes_reduzidas(void) { return valor[AJ_ANIM] == 1; }
// Lido UMA vez, na criacao da janela, antes de qualquer desenho: trocar isto
// com o app aberto nao redimensiona a superficie. Ver main.c.
// PERFIL SEGURO (seguro.h): quando ligado, os acessores dos ajustes que pesam
// devolvem o valor seguro SEM tocar em valor[] nem no arquivo. Ler o valor por
// cima em vez de sobrescreve-lo e o que garante que gravar() (que escreve valor[]
// inteiro) e o blob da conta nunca levam o valor de emergencia para o disco.
// Uma variavel daqui (e nao seguro_perfil_ativo()) para os acessores nao puxarem
// seguro.c: varios testes compilam ajustes.c com uma lista curta de fontes.
static int perfilSeguro;
#define SEGURO perfilSeguro
int ajustes_4k(void)                  { return valor[AJ_RESOLUCAO] == RES_4K && !SEGURO; }
int ajustes_res_auto(void)            { return valor[AJ_RESOLUCAO] == RES_AUTO && !SEGURO; }
int ajustes_720p(void)                { return valor[AJ_RESOLUCAO] == RES_720 && !SEGURO; }
int ajustes_dolby_vision(void)        { return lig(AJ_DV); }
int ajustes_dolby_atmos(void)         { return lig(AJ_ATMOS); }
int ajustes_pausa_overlay(void)       { return lig(AJ_PAUSA_OVERLAY); }
int ajustes_reacao_creditos(void)     { return lig(AJ_REACAO_CREDITOS); }
int ajustes_medidor_desempenho(void)  { return valor[AJ_MEDIDOR]; }
int ajustes_fonte_manual(void)        { return lig(AJ_FONTE_MANUAL); }
int ajustes_fonte_primeira(void)      { return valor[AJ_FONTE_AUTO] == 1; }
int ajustes_fonte_prioridade(void) { int v = valor[AJ_FONTE_PRIORIDADE]; return v < 0 || v > 2 ? 0 : v; }
int ajustes_enquetes(void)         { return lig(AJ_ENQUETES); }
int ajustes_busca_cinemeta(void)   { return lig(AJ_BUSCA_CINEMETA); }
int ajustes_esmaecer(void)         { int v = valor[AJ_ESMAECER]; return v < 0 || v > 5 ? 3 : v; }
int ajustes_descanso_estilo(void)  { int v = valor[AJ_DESCANSO_ESTILO]; return v < 0 || v > 2 ? 0 : v; }
int ajustes_descanso_fonte(void)   { int v = valor[AJ_DESCANSO_FONTE]; return v < 0 || v > 1 ? 0 : v; }
// #202: -1 = ultimo modo usado; 0..7 = PlrAspecto a aplicar ao abrir o video.
int ajustes_proporcao_padrao(void) { int v = valor[AJ_PROPORCAO_PADRAO]; return v < 1 || v > 8 ? -1 : v - 1; }
int ajustes_brilho_player(void)    { int v = valor[AJ_BRILHO_PLAYER]; return v < 0 || v > 3 ? 1 : v; }
void ajustes_espelhar_enquetes(int ligado) { int n = ligado ? 0 : 1; if (valor[AJ_ENQUETES] != n) { valor[AJ_ENQUETES] = n; AJ_COM_ORIGEM(AJLOG_CENTRAL, gravar()); } }
int ajustes_fonte_hdr(void)        { int v = valor[AJ_FONTE_HDR]; return v < 0 || v > 2 ? 0 : v; }
int ajustes_fonte_ordem_addon(void)   { return valor[AJ_FONTE_ORDEM_ADDON] == 1; }
int ajustes_fonte_texto_addon(void)   { return valor[AJ_FONTE_TEXTO] == 1; }
// "Logo do titulo" (dono, 03/10, em teste): o layout do Nuvio com a logo do
// conteudo no lugar do nome escrito em cada linha. Indice 2: o 0 e o 1 ja
// estavam gravados em fonteTextoLocal.
int ajustes_fonte_texto_logo(void)    { return valor[AJ_FONTE_TEXTO] == 2; }
// PRAZO DA ESCOLHA AUTOMATICA COM A LISTA AINDA ENCHENDO (#221), em ms; 0 =
// esperar todos os addons (o comportamento ate a 1.7.0). 5 s de fabrica:
// medido no D1 da 1.7.0 (.tpk), a primeira fonte chega em 0,85 s no p90 e o
// ultimo addon em 12 s — o prazo cobre a cauda dos rapidos sem pagar a dos
// mudos.
//
// #202: 0 = INSTANTANEO (a primeira fonte aceitavel do melhor grupo sai na
// hora); -1 = esperar todos os addons.
int ajustes_fonte_prazo_ms(void) {
  int v = valor[AJ_FONTE_PRAZO];
  return v < 0 || v >= (int)(sizeof FONTE_PRAZO_MS / sizeof *FONTE_PRAZO_MS) ? 5000 : FONTE_PRAZO_MS[v];
}
// #202 (fonteregra.h).
int ajustes_fonte_tocar_conferindo(void) { return lig(AJ_FONTE_TOCAR_CONFERINDO); }
int ajustes_fonte_aquecer(void) { return lig(AJ_FONTE_AQUECER); }
int ajustes_fonte_conferir_varias(void) { return lig(AJ_FONTE_CONFERIR_VARIAS); }
int ajustes_fonte_preparar(void) { return lig(AJ_FONTE_PREPARAR); }
int ajustes_cw_proximo(void) { return lig(AJ_CW_PROXIMO); }
int ajustes_busca_nuvio(void) { int v = valor[AJ_BUSCA_NUVIO]; return v < 0 || v > 2 ? 0 : v; }
int ajustes_busca_origem(void) { return lig(AJ_BUSCA_ORIGEM); }
int ajustes_fonte_escopo(void)      { int v = valor[AJ_FONTE_ESCOPO]; return v < 0 || v > 2 ? 0 : v; }
int ajustes_fonte_regex_modo(void)  { int v = valor[AJ_FONTE_REGEX]; return v < 0 || v > 2 ? 0 : v; }
int ajustes_fonte_usar_outros(void) { return lig(AJ_FONTE_OUTROS); }
int ajustes_fonte_ordem_uso(void)   { int v = valor[AJ_FONTE_ORDEM_USO]; return v < 0 || v > 2 || !fonteregra_ordem_n() ? 0 : v; }
int ajustes_fonte_repor(void) {
  int v = valor[AJ_FONTE_REPOR];
  return v < 0 ? 0 : v > 3 ? 3 : v;     // arquivo editado a mao: dentro da tabela
}
// IDIOMA AUTOMATICO DA INTERFACE.
//
// ESTADO. valor[AJ_IDIOMA] e o INDICE DA LISTA da tela: 0 = "Automático", e
// 1 + IDIOMA_* = a escolha manual. Assim a lista mostra "Automático" primeiro
// sem que nenhum outro codigo do app mude de numero. Quem quer o idioma usa
// ajustes_idioma(), que devolve sempre um IDIOMA_*: o RESOLVIDO no automatico,
// o escolhido no manual.
//
// DISCO (ajustes.txt). O numero do idioma continua sendo o que sempre foi:
//   idioma N           IDIOMA_* em vigor (no automatico, o ultimo resolvido:
//                      e o que a TV mostra no arranque, antes da conta chegar)
//   idiomaAutoLocal 1  automatico ligado. Ausente = escolha manual.
//   idiomaFonteLocal N IDA_* de onde o automatico tirou o idioma. Serve so para
//                      o arranque: um idioma que veio da CONTA nao e refeito com
//                      o locale da TV (a conta ainda nao chegou e o idioma
//                      piscaria em cada abertura); o que veio da TV ou do
//                      padrao e refeito na hora.
// QUEM JA TINHA O APP. Um ajustes.txt com "idioma" e sem "idiomaAutoLocal" veio
// de antes desta chave (ou de alguem que escolheu): nao ha como distinguir, e
// mudar a lingua de quem ja estava lendo em uma seria a pior surpresa possivel,
// entao conta como MANUAL. So nasce automatico quem nunca gravou um "idioma".
static int idiomaEfetivo = IDIOMA_EN;   // o resolvido; so vale com valor[AJ_IDIOMA] == 0
static int idiomaPosArranque;           // 1 depois de ajustes_idioma_auto_iniciar
static int idiomaUltimaFonte = -1;      // a ultima decisao logada (nao repetir a linha)
static int idiomaFonteGravada = IDA_PADRAO;  // idiomaFonteLocal: de onde veio o gravado
static void (*idiomaGancho)(const char *codigo, int fonte, int notificar);
static int sistemaPendente;             // 1 = a TV ainda nao respondeu o locale (webOS)
static char contaTmdbLing[24];      // tmdb_language cru do blob da conta
static char contaLegLing[24];       // subtitle_preferred_language cru
static char sistemaLoc[32];         // locale da TV ("pt-BR"), "" = desconhecido

int ajustes_idioma(void) {
  if (valor[AJ_IDIOMA] == 0) return idiomaEfetivo;
  { int v = valor[AJ_IDIOMA] - 1;
    return v >= 0 && v < IDIOMA_N ? v : IDIOMA_PT; }
}
int ajustes_idioma_ingles(void)       { return ajustes_idioma() == IDIOMA_EN; }

// 1 = o tema escolhido e um dos dinamicos (cor viva), que so existem nesta TV.
static int temaEhDinamico(int v) {
  return v >= 0 && v < AJ_N_TEMAS_OPC && v >= AJ_N_TEMAS && !TEMA_ACENTO[v].fam;
}
static int temaDinamico(void) { return temaEhDinamico(valor[AJ_TEMA]); }
// LOCAL = nao existe no app web: os dinamicos e os seis fixos de 03/10. Nao
// sobe para a conta e a conta nao o desfaz (ajustes_aplicar_blob,
// ajustes_mesclar_blob).
static int temaLocal(void) { return valor[AJ_TEMA] >= AJ_N_TEMAS && valor[AJ_TEMA] < AJ_N_TEMAS_OPC; }
int ajustes_cor_viva(void) {
  if (SEGURO) return CORVIVA_DESLIGADA;   // qualquer tema dinamico: a cor viva anima a arte inteira
  return valor[AJ_TEMA] == AJ_TEMA_DINAMICA   ? CORVIVA_SIMPLES
       : valor[AJ_TEMA] == AJ_TEMA_ESTILIZADA ? CORVIVA_SIMPLES   // so ate o arranque migrar
       : valor[AJ_TEMA] == AJ_TEMA_GRADIENTE  ? CORVIVA_GRADIENTE
       : valor[AJ_TEMA] == AJ_TEMA_IMERSIVA   ? CORVIVA_IMERSIVA
       : valor[AJ_TEMA] == AJ_TEMA_TEXTURA    ? CORVIVA_TEXTURA
       : valor[AJ_TEMA] == AJ_TEMA_TEXTURA_SUTIL ? CORVIVA_TEXTURA_SUTIL
       : CORVIVA_DESLIGADA;
}
int ajustes_cor_logo(void) { return lig(AJ_COR_LOGO); }
// Lida por todo desenho de painel/pilula/foco (uma comparacao): so o visual
// muda, nenhum layout, e desligada nada do desenho antigo e tocado.
#ifdef NV_VIDRO_TESTE
// So nas capturas (tests/vidro_shots.sh): liga o vidro sem passar pelo arquivo.
int ajustes_vidro(void) { return 1; }
#else
int ajustes_vidro(void) { return lig(AJ_VIDRO) && !SEGURO; }
int ajustes_fundo(void) { int v = valor[AJ_FUNDO]; return v >= 0 && v < 3 ? v : 0; }
int ajustes_vidro_contorno(void) { return lig(AJ_VIDRO_CONTORNO); }
int ajustes_addons_do_principal(void) { return lig(AJ_ADDONS_PRINCIPAL); }
#endif
float ajustes_vidro_opacidade(void) {
  static const float F[] = { 0.60f, 0.70f, 0.78f, 0.86f, 0.92f };
  int v = valor[AJ_VIDRO_OPAC];
  return v >= 0 && v < 5 ? F[v] : 0.78f;
}
int ajustes_vidro_fosco(void) { return valor[AJ_VIDRO_FOSCO] == 1; }
// Capturas do teste do vidro: NUVIO_SHOT_VIDRO_OPAC=60|70|78|86|92 e
// NUVIO_SHOT_VIDRO_FOSCO=1 (sem gravar nada). INTENCIONAL no release: so os
// tests/*_shot.c e ajustes_teste_ux_captura a chamam; nenhum caminho de src/ chega aqui na TV.
void ajustes_teste_vidro_env(void) {
  const char *o = getenv("NUVIO_SHOT_VIDRO_OPAC"), *f = getenv("NUVIO_SHOT_VIDRO_FOSCO");
  if (o && *o) { int n = atoi(o); valor[AJ_VIDRO_OPAC] = n <= 60 ? 0 : n <= 70 ? 1 : n <= 78 ? 2 : n <= 86 ? 3 : 4; }
  if (f && *f) valor[AJ_VIDRO_FOSCO] = atoi(f) ? 1 : 0;
  if ((o && *o) || (f && *f)) valor[AJ_VIDRO] = 0;   // o teste e do vidro: liga
}
int ajustes_relogio_ligado(void) { return lig(AJ_RELOGIO); }
int ajustes_relogio_pos(void) { return valor[AJ_RELOGIO_POS]; }
int ajustes_classif_player(void) { return lig(AJ_PLR_CLASSIF); }
int ajustes_relogio_12h(void) { return valor[AJ_RELOGIO_12H] == 1; }
// GB da faixa de tamanho da escolha automatica; 0 = "Sem limite".
int ajustes_tamanho_max_gb(void) { int v = valor[AJ_TAM_MAX]; return v < 0 || v >= (int)(sizeof TAMANHO_GB / sizeof *TAMANHO_GB) ? 0 : TAMANHO_GB[v]; }
int ajustes_tamanho_min_gb(void) { int v = valor[AJ_TAM_MIN]; return v < 0 || v >= (int)(sizeof TAMANHO_GB / sizeof *TAMANHO_GB) ? 0 : TAMANHO_GB[v]; }
float ajustes_tamanho_ui(void) {
  static const float F[] = { 1.0f, 1.2f, 1.3f, 1.5f };
  int v = valor[AJ_TAMANHO_UI];
  return v >= 0 && v < 4 ? F[v] : 1.0f;
}
#ifdef AJUSTES_TESTE
static int ajEscalaTestePct;
void ajustes_teste_escala(int percentual) {
  ajEscalaTestePct = percentual == 80 || percentual == 90 || percentual == 100 ? percentual : 0;
}
#endif
#ifdef AJUSTES_TESTE
// #339: as capturas e os testes escolhem o arranjo sem gravar arquivo.
void ajustes_teste_layout(int lista) { valor[AJ_LAYOUT_AJUSTES] = lista ? 1 : 0; }
#endif
float ajustes_tamanho_ajustes(void) {
  static const float F[] = { 0.8f, 0.9f, 1.0f };
  int v = valor[AJ_TAMANHO_AJUSTES];
#ifdef AJUSTES_TESTE
  if (ajEscalaTestePct) return ajEscalaTestePct / 100.0f;
#endif
  return v >= 0 && v < 3 ? F[v] : 0.8f;
}
int ajustes_layout_lista(void) { return valor[AJ_LAYOUT_AJUSTES] == 1; }
int ajustes_esconder_logo_trailer(void) { return lig(AJ_LOGO_TRAILER); }
int ajustes_legenda_sync_auto(void) { return lig(AJ_LEG_SYNC_AUTO); }
int ajustes_trailer_zoom_tpk(void) { return lig(AJ_TRAILER_ZOOM_TPK); }   // 1 = Ligado
#ifdef NV_ANDROID
int ajustes_legenda_sync_audio(void) { return lig(AJ_LEG_SYNC_AUDIO); }
#else
int ajustes_legenda_sync_audio(void) { return 0; }   // escondida fora do Android
#endif
int ajustes_legenda_forcada_auto(void) { return lig(AJ_LEG_FORCADA); }   // #287, padrao Ligado
int ajustes_cache_seek_mb(void) {
  static const int MB[] = { 0, 256, 512, 1024 };
  int v = valor[AJ_CACHE_SEEK];
  return cacheSeekExiste() && v >= 0 && v < 4 ? MB[v] : 0;
}
int ajustes_icone_app(void) { return valor[AJ_ICONE_APP]; }
int ajustes_logo_app(void) { int v = valor[AJ_LOGO_APP]; return v < 0 || v > 1 ? 0 : v; }
int ajustes_abertura(void) { int v = valor[AJ_ABERTURA]; return v < 0 || v > 2 ? 0 : v; }
// R4: estilo proprio da segunda legenda. -1 / 0 = segue a principal.
int ajustes_leg2_junto(void) { return valor[AJ_LEG2_POS] == 1; }
int ajustes_leg2_tamanho(void) {
  static const int P[] = { 0, 60, 80, 100, 120, 140, 160 };
  int v = valor[AJ_LEG2_TAMANHO];
  return v >= 0 && v < 7 ? P[v] : 0;
}
int ajustes_leg2_cor(void) { int v = valor[AJ_LEG2_COR]; return v >= 1 && v <= 6 ? v - 1 : -1; }
int ajustes_leg2_fundo(void) { int v = valor[AJ_LEG2_FUNDO]; return v >= 1 && v <= 5 ? v - 1 : -1; }
int ajustes_leg2_borda(void) { int v = valor[AJ_LEG2_BORDA]; return v >= 1 && v <= 3 ? v - 1 : -1; }
int ajustes_saida_player_home(void) { return lig(AJ_RELOGIO) && valor[AJ_SAIDA_PLAYER] == 0; }
int ajustes_manter_video(void) { return ajustes_saida_player_home() && lig(AJ_MANTER_VIDEO) && !SEGURO; }
int ajustes_selo_visto(void) { return lig(AJ_SELO_VISTO); }
static void riscoNotar(int op, int antes);
void ajustes_definir_vidro(int ligado) { int a = valor[AJ_VIDRO]; valor[AJ_VIDRO] = ligado ? 0 : 1; AJ_COM_ORIGEM(AJLOG_CENTRAL, gravar()); riscoNotar(AJ_VIDRO, a); }
#ifdef __EMSCRIPTEN__
// .wgt: o navegador nao abre socket TCP/UDP (motor impossivel) e o P2P fica
// escondido la (ajustes_ux_tela.inc), inclusive o do servidor Stremio
// (decisao do dono, a8e7685d). Nem um "p2p" ligado de ajustes.txt antigo vale.
int ajustes_p2p_ligado(void) { return 0; }
unsigned ajustes_p2p_limite_mb(void) { return 0; }
#else
int ajustes_p2p_ligado(void) { return lig(AJ_P2P_LIGADO) && !SEGURO; }
unsigned ajustes_p2p_limite_mb(void) {
  int v = valor[AJ_P2P_LIMITE];
  return v >= 0 && v < (int)(sizeof P2P_LIMITE_MB / sizeof *P2P_LIMITE_MB) ? P2P_LIMITE_MB[v] : 0;
}
#endif
// Servidores pessoais: ligado E com HTTP estrito neste backend (o WGT nao tem:
// jellyfin_disponivel() e 0 la). Fica FORA do #ifdef acima: dentro do #else ela
// sumia do WASM e o link do .wgt falhava (app.c e descoberta.c a chamam).
int ajustes_jellyfin_ligado(void) { return lig(AJ_JF_LIGADO) && jellyfin_disponivel(); }
void ajustes_definir_p2p_ligado(int ligado) { int a = valor[AJ_P2P_LIGADO]; valor[AJ_P2P_LIGADO] = ligado ? 0 : 1; AJ_COM_ORIGEM(AJLOG_CENTRAL, gravar()); riscoNotar(AJ_P2P_LIGADO, a); }

// Cor do ANEL DE FOCO. Ver TEMA_ACENTO: um tema aqui e so isto.
//
// Com o tema dinamico, a cor e a que corviva_quadro ja calculou para ESTE
// quadro (o laco principal anima uma vez; aqui so se copia). ~43 arquivos
// chamam esta funcao, varias vezes por quadro: nenhuma conta mora aqui.
void ajustes_acento(float *r, float *g, float *b) {
  int i = valor[AJ_TEMA];
  if (SEGURO && temaDinamico()) i = 0;    // sem cor viva, o realce fixo padrao
  else if (temaDinamico()) { corviva_acento(r, g, b); return; }
  if (i < 0 || i >= AJ_N_TEMAS_OPC || !TEMA_ACENTO[i].fam) i = 0;   // arquivo de outra versao: branco
  corDeHex(TEMA_ACENTO[i].fill, r, g, b);
}
// As cores derivadas de um acento DINAMICO (marca, HDR, luz, tinta) sairiam
// de contas em OKLCH a cada chamada; o destaque so muda numa transicao, entao
// a ultima conta fica guardada pela cor.
static const CorvivaTokens *tokensVivos(void) {
  static float ult[3] = { -1, -1, -1 };
  static CorvivaTokens t;
  float c[3];
  corviva_acento(&c[0], &c[1], &c[2]);
  if (memcmp(c, ult, sizeof c)) { memcpy(ult, c, sizeof c); corviva_tokens(c, &t); }
  return &t;
}
static int temaFixoAtual(void) {
  int i = valor[AJ_TEMA];
  if (SEGURO && temaDinamico()) return 0;
  if (temaDinamico()) return -1;
  return (i < 0 || i >= AJ_N_TEMAS_OPC || !TEMA_ACENTO[i].fam) ? 0 : i;
}
static int texturaAtiva(void) {
  return !SEGURO && (valor[AJ_TEMA] == AJ_TEMA_TEXTURA || valor[AJ_TEMA] == AJ_TEMA_TEXTURA_SUTIL) &&
         nv_textura_viva.ok;
}

#define AJ_TINTA_ESCURA (18.0f / 255.0f)   // #121316
float ajustes_acento_tinta(float *r, float *g, float *b) {
  int i = temaFixoAtual();
  ajustes_acento(r, g, b);
  // TINTA ESCURA NOS CLAROS (dono, 03/10/2026), no lugar da regra de 21/09 ("se
  // nao for branco o accent, a cor de texto tem que ser branca"), que punha
  // branco a 1,4:1 no Dourado. A familia de cada fixo esta na tabela; a de um
  // dinamico e a tinta que contrasta mais (corviva_tokens) — a mesma conta.
  if (texturaAtiva()) return nv_textura_viva.tintaBranca ? 1.0f : AJ_TINTA_ESCURA;
  if (i >= 0) return TEMA_ACENTO[i].fam == 'c' ? AJ_TINTA_ESCURA : 1.0f;
  return tokensVivos()->tintaBranca ? 1.0f : AJ_TINTA_ESCURA;
}
int ajustes_tinta_foco(void)  { return ajustes_acento_tinta(NULL, NULL, NULL) > 0.5f ? 255 : 18; }
// Secundario sobre realce colorido: 238, nao 225 — a 3 m, sobre rosa, 225
// ja lia como cinza (dono, 21/09/2026).
int ajustes_tinta_foco2(void) { return ajustes_acento_tinta(NULL, NULL, NULL) > 0.5f ? 238 : 60; }
void ajustes_acento_marca(float *r, float *g, float *b) {
  int i = temaFixoAtual();
  if (i >= 0) { corDeHex(TEMA_ACENTO[i].marca, r, g, b); return; }
  { const CorvivaTokens *t = tokensVivos();
    if (r) *r = t->marca[0];
    if (g) *g = t->marca[1];
    if (b) *b = t->marca[2]; }
}
void ajustes_acento_hdr(float *r, float *g, float *b) {
  int i = temaFixoAtual();
  if (i >= 0) { corDeHex(TEMA_ACENTO[i].hdr, r, g, b); return; }
  { const CorvivaTokens *t = tokensVivos();
    if (r) *r = t->hdr[0];
    if (g) *g = t->hdr[1];
    if (b) *b = t->hdr[2]; }
}
void ajustes_acento_luz(float *r, float *g, float *b) {
  int i = temaFixoAtual();
  if (i >= 0) { corDeHex(TEMA_ACENTO[i].luz, r, g, b); return; }
  { const CorvivaTokens *t = tokensVivos();
    if (r) *r = t->luz[0];
    if (g) *g = t->luz[1];
    if (b) *b = t->luz[2]; }
}
// TEXTURA: a textura do titulo em cena vai ao gfx, que a usa em todo
// GFX_COR cheio pintado exatamente com o destaque (gfx_rect). Sem titulo, sem
// textura decodificada ou fora da Textura: desliga, e a pilula fica no Da arte.
void ajustes_textura_quadro(void) {
  const CorvivaTextura *T = &nv_textura_viva;
  GLuint tex = 0;
  float asp = 0.0f;
  if (texturaAtiva() && T->url[0]) {
    tex = T->logo ? tex_obter_larg_qualquer(T->url, 640.0f) : tex_obter_hero(T->url);
    asp = tex_aspecto(T->url);
  }
  if (!tex || asp <= 0.0f) { gfx_textura_definir(0, NULL, 0, 0, 0, 0); return; }
  gfx_textura_definir(tex, T->janela, asp, T->forca, T->veu, !T->tintaBranca);
}

// `collapseSidebar: modernSidebar ? false : Boolean(collapseSidebar)` — a barra
// moderna DESLIGA o recolhimento, e nao o contrario. Copiado de
// normalizeLayoutPreferences para nao inventar precedencia.
int ajustes_rail_moderna(void)        { return lig(AJ_RAIL_MODERNA); }
int ajustes_rail_recolhida(void)      { return ajustes_rail_moderna() ? 0 : lig(AJ_RAIL); }
int ajustes_rail_moderna_blur(void)   { return lig(AJ_RAIL_BLUR); }
int ajustes_hero_ligado(void)         { return lig(AJ_HERO); }
int ajustes_hero_cheio(void)          { return lig(AJ_HERO_CHEIO); }
int ajustes_home_layout(void) {
  int v = valor[AJ_HOME_LAYOUT];
  return v >= 0 && v < HOME_LAYOUT_N ? v : HOME_LAYOUT_MODERNA;
}
int ajustes_hero_arte_diferente(void) { return lig(AJ_HERO_ARTE_DIF); }
int ajustes_hero_fonte(void) {
  int v = valor[AJ_HERO_FUNDO];
  return v >= 0 && v < (int)(sizeof V_HERO_FONTE / sizeof *V_HERO_FONTE) ? v : 0;
}
// #90: "Automático" (indice 0, padrao de fabrica) = psfundo.c continua
// escolhendo arte do catalogo como reserva; "Desligado" (indice 1) = a tela de
// perfil volta a nao desenhar nada alem do que o proprio perfil traz.
int ajustes_ps_fundo_automatico(void) { return lig(AJ_PS_FUNDO); }
int ajustes_ps_fundo(void) { int v = valor[AJ_PS_FUNDO]; return v >= 0 && v <= 4 ? v : 0; }
int ajustes_tex_mb(void) {
  int i = valor[AJ_TEX_MB];
  if (SEGURO && i >= 5) i = 0;            // 400/512 MB: volta ao automatico da RAM
  return (i >= 0 && i < 7) ? TEX_MB_DE[i] : 0;
}
int ajustes_posteres_deitados(void)   { return lig(AJ_LANDSCAPE); }
int ajustes_gradiente_foco_classico(void) { return lig(AJ_GRAD_CLASSICO); }


int ajustes_rotulos_poster(void)      { return lig(AJ_ROTULOS); }
int ajustes_nome_addon(void)          { return lig(AJ_NOME_ADDON); }
int ajustes_sufixo_tipo(void)         { return lig(AJ_SUFIXO_TIPO); }
int ajustes_ocultar_nao_lancados(void){ return lig(AJ_OCULTAR_NLANC); }
// O blob da ordem de catalogos (plataforma home_catalog_shared) traz esta mesma
// opcao, separada do blob de ajustes do perfil. Setter proprio porque so vale
// quando a chave EXISTE la: ausente nao e `false`, e "mantem o que esta na TV".
void ajustes_definir_ocultar_nao_lancados(int ligado) {
  valor[AJ_OCULTAR_NLANC] = ligado ? 0 : 1;
}
// 1 = o "+" tambem publica na watchlist do Trakt. A lista LOCAL e escrita nos
// dois casos; ver a nota de V_SALVOS e a de abertura de salvos.h.
int ajustes_salvos_no_trakt(void)     { return valor[AJ_SALVOS_DEST] == AJ_SALVOS_TRAKT; }
int ajustes_salvos_no_simkl(void)     { return valor[AJ_SALVOS_DEST] == AJ_SALVOS_SIMKL; }
// Setter para o explicador de primeira vez (salvosintro.c), que faz esta
// pergunta antes de a pessoa chegar em Ajustes. Grava na hora: quem respondeu e
// desligou a TV nao deve ser perguntado de novo.
void ajustes_definir_salvos_no_trakt(int noTrakt) {
  valor[AJ_SALVOS_DEST] = noTrakt ? 1 : 0;
  AJ_COM_ORIGEM(AJLOG_PRIMEIRA, gravar());
}
void ajustes_definir_salvos_destino(int destino) {
  if (destino < AJ_SALVOS_LOCAL || destino > AJ_SALVOS_SIMKL) return;
  valor[AJ_SALVOS_DEST] = destino;
  AJ_COM_ORIGEM(AJLOG_CENTRAL, gravar());
  desc_repetir();   // o mesmo que a linha de Ajustes faz ao mudar
}
int ajustes_data_completa(void)       { return lig(AJ_DET_DATA_CHEIA); }
float ajustes_detalhe_veu(void)       { int v = valor[AJ_DET_VEU]; return (v < 0 ? 0 : v > 100 ? 100 : v) / 100.0f; }
int   ajustes_trailer_auto(void)      { return lig(AJ_DET_TRAILER_AUTO); }
// TRAILER AUTOMATICO DA HOME VETADO PELA GPU. Utgard (Mali-400/450/470) roda
// a home a 8-24 fps ja no nivel 2 de efeitos (gpunivel.c); o video tocando
// por baixo do furo do destaque so tira mais quadro. Vale para a sessao e nao
// grava nada: o ajuste da pessoa fica como esta, e o trailer da pagina de
// titulo (botao) continua.
static int trailerHeroVetado;
void ajustes_trailer_hero_vetar(const char *gpu) {
  if (trailerHeroVetado) return;
  trailerHeroVetado = 1;
  printf("[trailer] desligado no hero: %s (Mali-4xx)\n", gpu ? gpu : "?");
}
int   ajustes_trailer_hero(void)      { return lig(AJ_HERO_TRAILER) && !SEGURO && !trailerHeroVetado; }
int   ajustes_trailer_hero_som(void)  { return lig(AJ_HERO_TRAILER_SOM); }
int   ajustes_trailer_detalhe_som(void) { return lig(AJ_DET_TRAILER_SOM); }
int   ajustes_hero_deslizar(void)     { return valor[AJ_HERO_TRANSICAO] == 0; }
int   ajustes_poster_addon(void)      { return lig(AJ_ADDON_POSTER); }
int   ajustes_fundo_addon(void)       { return lig(AJ_ADDON_FUNDO); }
int   ajustes_logo_addon(void)        { return lig(AJ_ADDON_LOGO); }
int   ajustes_col_arte_conta(void)    { return lig(AJ_COL_ARTE_CONTA); }
int   ajustes_selos_coloridos(void)   { return lig(AJ_SELOS_CORES); }
int   ajustes_livetv_resolucao(void)  { return valor[AJ_LIVETV_RES]; }
int   ajustes_livetv_formato(void)    { return valor[AJ_LIVETV_FORMATO]; }
int   ajustes_livetv_modo(void)       { return valor[AJ_LIVETV_MODO]; }
int   ajustes_livetv_proxy(void)      { return lig(AJ_LIVETV_PROXY); }
void  ajustes_livetv_aplicar_proxy(int l) { valor[AJ_LIVETV_PROXY] = l ? 0 : 1; AJ_COM_ORIGEM(AJLOG_CENTRAL, gravar()); }
void  ajustes_livetv_aplicar_modo(int m) { if (m >= 0 && m < 3) { valor[AJ_LIVETV_MODO] = m; AJ_COM_ORIGEM(AJLOG_CENTRAL, gravar()); } }
unsigned ajustes_livetv_espera_ms(void) {
  return valor[AJ_LIVETV_ESPERA] == 1 ? 25000u : valor[AJ_LIVETV_ESPERA] == 2 ? 45000u : 0u;
}
void  ajustes_livetv_aplicar(int resolucao, int formato, int espera) {
  if (resolucao >= 0 && resolucao < 5) valor[AJ_LIVETV_RES] = resolucao;
  if (formato >= 0 && formato < 3) valor[AJ_LIVETV_FORMATO] = formato;
  if (espera >= 0 && espera < 3) valor[AJ_LIVETV_ESPERA] = espera;
  AJ_COM_ORIGEM(AJLOG_CENTRAL, gravar());
}
// valor[] guarda decimos de segundo, preso ao intervalo de OPCOES (o disco
// pode trazer qualquer numero).
Uint32 ajustes_trailer_hero_espera_ms(void) {
  const Opcao *o = &OPCOES[AJ_HERO_TRAILER_ESPERA];
  int v = valor[AJ_HERO_TRAILER_ESPERA];
  if (v < o->min) v = o->min;
  if (v > o->max) v = o->max;
  return (Uint32)v * 100u;
}
float ajustes_trailer_zoom(void)      { static const float z[] = { 1.34f, 1.15f, 1.55f, 1.0f }; int v = valor[AJ_TRAILER_ASPECTO]; return (v >= 0 && v < 4) ? z[v] : 1.34f; }
// Teto de definicao do trailer: 0 = a maior que houver.
int   ajustes_trailer_qualidade(void) { static const int t[] = { 0, 1080, 720, 480 }; int v = valor[AJ_TRAILER_QUAL]; return (v >= 0 && v < 4) ? t[v] : 0; }
// Fonte do trailer: o TRF_* de trailerfonte.h. Fora da lista le Automatico.
int   ajustes_trailer_fonte(void)     { int v = valor[AJ_TRAILER_FONTE]; return (v >= 0 && v < nValores(AJ_TRAILER_FONTE)) ? v : 0; }
int  ajustes_envio_auto(void)         { return lig(AJ_ENVIO_AUTO); }
int  ajustes_menu_explorar(void)      { return lig(AJ_MENU_EXPLORAR); }
int  ajustes_menu_guia(void)          { return lig(AJ_MENU_GUIA); }
int  ajustes_menu_agenda(void)        { return lig(AJ_MENU_AGENDA); }
int  ajustes_menu_perfil(void)        { return lig(AJ_MENU_PERFIL); }
int  ajustes_gpu_efeitos(void) { return valor[AJ_GPU_EFEITOS]; }
int  ajustes_itens_fileira(void) {
  static const int N[] = { 12, 18, 24 };
  int i = valor[AJ_ITENS_FILEIRA];
  if (i < 0 || i >= (int)(sizeof N / sizeof *N) || SEGURO) i = 0;
  return N[i];
}
int  ajustes_trailer_cartaz(void) {
  // A MESMA dependencia de inativa(AJ_FOCO_TRAILER), escrita aqui porque
  // inativa() vem bem mais abaixo no arquivo.
  return lig(AJ_FOCO_TRAILER) && !SEGURO && !trailerHeroVetado && (lig(AJ_EXPANDIR) || valor[AJ_LANDSCAPE] == 0);
}
void ajustes_definir_envio_auto(int ligado) { valor[AJ_ENVIO_AUTO] = ligado ? 0 : 1; AJ_COM_ORIGEM(AJLOG_CENTRAL, gravar()); }
// ARTE DO DESTAQUE ESCOLHIDA PELO DIAGNOSTICO, e so depois de a pessoa ver a
// proposta na tela e apertar OK no botao (diagnostico.c): nunca sozinho. Os
// dois ajustes sao locais (ver somenteDesteAparelho), entao nao ha blob de conta para
// avisar. `fonte` e o indice ARTEHERO_* de V_HERO_FONTE; fora da faixa fica.
void ajustes_definir_destaque(int fonte, int diferente) {
  if (fonte >= 0 && fonte < (int)(sizeof V_HERO_FONTE / sizeof *V_HERO_FONTE))
    valor[AJ_HERO_FUNDO] = fonte;
  valor[AJ_HERO_ARTE_DIF] = diferente ? 0 : 1;
  AJ_COM_ORIGEM(AJLOG_CENTRAL, gravar());
}
int ajustes_notas_home(void)          { return valor[AJ_NOTAS_HOME] == 0; }
int ajustes_local_descobrir(void)     { return valor[AJ_DESCOBRIR]; }
int ajustes_descobrir_na_busca(void)  { return valor[AJ_DESCOBRIR] == 0; }

int ajustes_cw_ligado(void)           { return lig(AJ_CW_LIGADO); }
int ajustes_cw_ok_toca(void)          { return valor[AJ_CW_OK] == 0; }
int ajustes_cw_estilo(void)           { return valor[AJ_CW_ESTILO]; }
int ajustes_cw_fonte(void)            { return valor[AJ_CW_FONTE]; }
int ajustes_cw_thumb_episodio(void)   { return lig(AJ_CW_THUMB); }
int ajustes_cw_desfocar_proximo(void) { return lig(AJ_CW_BLUR_PROX); }
int ajustes_cw_do_episodio_mais_alto(void) { return lig(AJ_CW_FURTHEST); }
int ajustes_cw_mostrar_nao_exibidos(void)  { return lig(AJ_CW_NAO_EXIBIDOS); }
int ajustes_cw_ordem(void)            { return valor[AJ_CW_ORDEM]; }
int ajustes_cw_retido_tambem(void)    { return lig(AJ_CW_RETIDO_TAMBEM); }
int ajustes_ps_continuar(void)        { return lig(AJ_PS_CONTINUAR); }
int ajustes_dts_ac3(void)             { return valor[AJ_DTS_AC3] == 1; }
int ajustes_dv_mkv(void)              { return lig(AJ_DV_MKV); }
int ajustes_hist_conta(void)          { return lig(AJ_HIST_CONTA); }
int ajustes_social(void)              { return lig(AJ_SOCIAL); }
// Espaco da Home em fracao do valor medido. Fora da faixa do NUM (arquivo torto)
// volta para dentro dela; 0 (arquivo sem a chave) nao chega aqui: o padrao e 100.
float ajustes_espaco_fator(int pct) {
  return (pct < 50 ? 50 : pct > 150 ? 150 : pct) / 100.0f;
}
float ajustes_espaco_fileiras(void) { return ajustes_espaco_fator(valor[AJ_ESPACO_FILEIRAS]); }
float ajustes_espaco_titulos(void)  { return ajustes_espaco_fator(valor[AJ_ESPACO_TITULOS]); }
int ajustes_cw_concluido(void) {
  int v = valor[AJ_CW_CONCLUIDO];
  return v < 70 ? 70 : v > 98 ? 98 : v;   // a faixa do NUM, se o arquivo vier torto
}

int ajustes_desfocar_nao_assistidos(void) { return lig(AJ_DET_BLUR_NAO_VISTOS); }
int ajustes_botao_trailer(void)       { return lig(AJ_DET_TRAILER); }
int ajustes_meta_externo(void)        { return lig(AJ_DET_META_EXT); }
int ajustes_meta_so_cinemeta(void)    { return lig(AJ_DET_SO_CINEMETA); }

int   ajustes_expandir_poster(void)   { return lig(AJ_EXPANDIR); }
float ajustes_expandir_poster_atraso(void) { return (float)valor[AJ_EXPANDIR_ATRASO]; }
int   ajustes_navegacao_horizontal_rapida(void) { return lig(AJ_NAV_RAPIDA); }
int   ajustes_borda_foco(void) { return lig(AJ_BORDA_FOCO); }

int   ajustes_profundidade(void)      { return lig(AJ_PROF); }
float ajustes_profundidade_borda(void)     { return valor[AJ_PROF_BORDA] / 100.0f; }
float ajustes_profundidade_brilho(void)    { return valor[AJ_PROF_BRILHO] / 100.0f; }
float ajustes_profundidade_cobertura(void) { return valor[AJ_PROF_COBERTURA] / 100.0f; }
int   ajustes_profundidade_posters(void)   { return lig(AJ_PROF_POSTERS); }
int   ajustes_profundidade_cw(void)        { return lig(AJ_PROF_CW); }
int   ajustes_profundidade_episodios(void) { return lig(AJ_PROF_EPS); }
int   ajustes_profundidade_elenco(void)    { return lig(AJ_PROF_ELENCO); }
int   ajustes_profundidade_trailers(void)  { return lig(AJ_PROF_TRAILERS); }

int   ajustes_largura_poster_dp(void) { return valor[AJ_LARGURA_DP]; }
int   ajustes_raio_poster_dp(void)    { return valor[AJ_RAIO_DP]; }
// 0 baixa, 1 padrao, 2 alta. Quem consome sao tex_cache (teto de decodificacao)
// e artehero (qual url pedir para a arte de tela cheia).
int   ajustes_qualidade_imagem(void)  { return SEGURO && valor[AJ_QUALIDADE_IMG] == 2 ? 1 : valor[AJ_QUALIDADE_IMG]; }
// dpToPx = 2 em buildModernHomeSizingStyle. 12dp -> 24px, que e o raio medido.
float ajustes_raio_poster_px(void)    { return (float)valor[AJ_RAIO_DP] * 2.0f; }

// A regra do web, e nao dois layouts: o conteudo tem sempre 104 de recuo e a
// rail acrescenta os 144 dela quando esta fixa.
float ajustes_conteudo_x(void) {
  return ajustes_rail_largura_fixa() + NV_CONTENT_PAD;
}
// A RAIL FIXA segue a pilula de icones que o menu desenha (menu.c, layouts
// Moderna e Padrao do Glass UI): o conteudo comeca NV_MENU_RAIL_VAO depois da
// borda direita dela — sem sobrepor e sem o vao largo da rail antiga de 144.
// O menu desenha numa escala FIXA (90 %, menu.c), que nao segue o Tamanho da
// interface: a borda ja esta em px da tela REAL.
float ajustes_rail_largura_fixa(void) {
  float borda;
  // Layout Dinamica: a barra e a pilula da Apple TV (menu.c), sem rail fixa.
  if (ajustes_home_layout() == HOME_LAYOUT_DINAMICA) return 0.0f;
  if (ajustes_rail_recolhida()) return 0.0f;
  borda = ajustes_home_layout() == HOME_LAYOUT_PADRAO ? NV_MENU_RAIL_BORDA_PADRAO
                                                      : NV_MENU_RAIL_BORDA_MODERNA;
  return borda + NV_MENU_RAIL_VAO - NV_CONTENT_PAD;
}
void ajustes_area_conteudo(float padEsq, float padDir, float *x, float *w) {
  float x0 = ajustes_rail_largura_fixa() + padEsq;
  if (x) *x = x0;
  if (w) *w = NV_TELA_W - padDir - x0;
}
const char *ajustes_qualidade(void)   { return V_QUALIDADE[valor[AJ_QUALIDADE]]; }

// --- Integracoes ------------------------------------------------------------
//
// TMDB: cada sub-toggle vale sozinho, mas o CONSUMIDOR so deve ler
// `ajustes_tmdb_*` DEPOIS do portao — `desc_chave_tmdb()` devolve "" quando
// ajustes_tmdb_ligado() e 0, e sem chave nenhum pedido ao TMDB sai. Ainda
// assim os acessores ja retornam 0 com o master desligado, para quem os usar
// nao precisar lembrar da segunda pergunta.
int ajustes_tmdb_ligado(void)         { return lig(AJ_TMDB_LIGADO); }
// Codigo no formato da API do TMDB ("pt-BR", "en-US"). "Da interface" (0)
// segue o idioma do app, que e o comportamento que desc_tmdb_idioma() sempre
// teve.
// "" = automatico; senao o codigo de 2 letras do pais (ver V_EPG_PAIS).
const char *ajustes_epg_pais(void) {
  static char c[3];
  int v = valor[AJ_EPG_PAIS];
  if (v <= 0 || v >= AJ_N_EPG_PAIS) return "";
  c[0] = V_EPG_PAIS[v][0]; c[1] = V_EPG_PAIS[v][1]; c[2] = 0;
  return c;
}

const char *ajustes_tmdb_idioma(void) {
  static const char *L[] = {
    NULL, "pt-BR", "en-US", "es-ES", "fr-FR", "de-DE", "it-IT", "pt-PT",
    "ja-JP", "ko-KR", "zh-CN", "ro-RO", "uk-UA", "ru-RU",
    "nl-NL", "pl-PL", "tr-TR", "sv-SE", "da-DK", "nb-NO", "cs-CZ", "sk-SK",
    "sl-SI", "hu-HU", "lt-LT", "bs-BA", "sr-RS", "bg-BG", "el-GR", "id-ID",
    "vi-VN", "zh-TW", "ar-SA"
  };
  int v = valor[AJ_TMDB_IDIOMA];
  if (v < 0 || v >= (int)(sizeof L / sizeof *L)) v = 0;
  // "Da interface" resolve AQUI, na hora de perguntar, e nao na gravacao:
  // trocar o idioma do app tem de refletir sem tocar neste ajuste.
  if (!L[v]) {
    switch (ajustes_idioma()) {
      case IDIOMA_EN: return "en-US";
      case IDIOMA_RO: return "ro-RO";
      case IDIOMA_UK: return "uk-UA";
      case IDIOMA_RU: return "ru-RU";
      case IDIOMA_FR: return "fr-FR";
      case IDIOMA_DE: return "de-DE";
      case IDIOMA_ES: return "es-ES";
      case IDIOMA_IT: return "it-IT";
      case IDIOMA_NL: return "nl-NL";
      case IDIOMA_PL: return "pl-PL";
      case IDIOMA_TR: return "tr-TR";
      case IDIOMA_PTPT: return "pt-PT";
      case IDIOMA_SV: return "sv-SE";
      case IDIOMA_DA: return "da-DK";
      case IDIOMA_NO: return "nb-NO";    // o TMDB chama o bokmal de "nb"
      case IDIOMA_CS: return "cs-CZ";
      case IDIOMA_SK: return "sk-SK";
      case IDIOMA_SL: return "sl-SI";
      case IDIOMA_HU: return "hu-HU";
      case IDIOMA_LT: return "lt-LT";
      case IDIOMA_BS: return "bs-BA";
      case IDIOMA_SR: return "sr-RS";
      case IDIOMA_BG: return "bg-BG";
      case IDIOMA_EL: return "el-GR";
      case IDIOMA_ID: return "id-ID";
      case IDIOMA_VI: return "vi-VN";
      case IDIOMA_JA: return "ja-JP";
      case IDIOMA_ZHCN: return "zh-CN";
      case IDIOMA_ZHTW: return "zh-TW";
      default:        return "pt-BR";
    }
  }
  return L[v];
}
#define TMDB_USA(op) (lig(AJ_TMDB_LIGADO) && lig(op))
int ajustes_tmdb_arte(void)           { return TMDB_USA(AJ_TMDB_ARTE); }
int ajustes_tmdb_basico(void)         { return TMDB_USA(AJ_TMDB_BASICO); }
int ajustes_tmdb_ficha(void)          { return TMDB_USA(AJ_TMDB_FICHA); }
int ajustes_tmdb_datas(void)          { return TMDB_USA(AJ_TMDB_DATAS); }
int ajustes_tmdb_elenco(void)         { return TMDB_USA(AJ_TMDB_ELENCO); }
int ajustes_tmdb_prod(void)           { return TMDB_USA(AJ_TMDB_PROD); }
int ajustes_tmdb_redes(void)          { return TMDB_USA(AJ_TMDB_REDES); }
int ajustes_tmdb_eps(void)            { return TMDB_USA(AJ_TMDB_EPS); }
int ajustes_tmdb_trailers(void)       { return TMDB_USA(AJ_TMDB_TRAILERS); }
int ajustes_tmdb_mais(void)           { return TMDB_USA(AJ_TMDB_MAIS); }
int ajustes_tmdb_col(void)            { return TMDB_USA(AJ_TMDB_COL); }
int ajustes_tmdb_cw(void)             { return TMDB_USA(AJ_TMDB_CW); }

int ajustes_mdblist_ligado(void)      { return lig(AJ_MDB_LIGADO); }
// `fonte` e um ExFonte de extras.h (a ordem dele, nao a das linhas aqui).
// NAO combina com o master de proposito: o master corta a CONSULTA ao mdbList,
// e as notas Trakt/IMDb que o app tem por conta propria (sem chave nenhuma)
// nao sao dados do mdbList — esconde-las junto seria punir o usuario pelo que
// outro servico faz. Cada show_* continua valendo sobre a sua fonte.
int ajustes_mdblist_fonte(int fonte) {
  static const int OP[] = {
    AJ_MDB_TRAKT, AJ_MDB_IMDB, AJ_MDB_TMDB, AJ_MDB_TOMATES,
    AJ_MDB_AUDIENCIA, AJ_MDB_META, AJ_MDB_LETTER,
    // As quatro seguintes: usuarios do Metacritic segue o interruptor do
    // Metacritic; MyAnimeList tem o dele (mdblist_show_mal). Roger Ebert e a
    // nota agregada NAO existem na conta — sao sempre "disponiveis" e quem
    // manda e a escolha da linha do titulo.
    AJ_MDB_META, AJ_MDB_MAL, -1, -1
  };
  if (fonte < 0 || fonte >= (int)(sizeof OP / sizeof *OP)) return 0;
  if (OP[fonte] < 0) return 1;
  return lig(OP[fonte]);
}

// A fonte entra na LINHA DO TITULO? Duas condicoes: a pessoa a ligou (aqui) E a
// fonte esta disponivel (mdblist_show_* da conta, via ajustes_mdblist_fonte —
// o mesmo interruptor que ja escondia o cartao da aba). Trakt e IMDb nao
// dependem do master do MDBList, como no resto do arquivo.
int ajustes_nota_titulo(int fonte) {
  static const int OP[EX_NFONTES] = {
    /* EX_TRAKT */ AJ_NT_TRAKT, /* EX_IMDB */ AJ_NT_IMDB, /* EX_TMDB */ AJ_NT_TMDB,
    /* EX_TOMATOES */ AJ_NT_TOMATES, /* EX_AUDIENCE */ AJ_NT_AUDIENCIA,
    /* EX_METACRITIC */ AJ_NT_META, /* EX_LETTERBOXD */ AJ_NT_LETTER,
    /* EX_METAUSER */ AJ_NT_METAUSER, /* EX_MAL */ AJ_NT_MAL,
    /* EX_EBERT */ AJ_NT_EBERT, /* EX_MDBSCORE */ AJ_NT_SCORE
  };
  if (fonte < 0 || fonte >= EX_NFONTES) return 0;
  return lig(OP[fonte]) && ajustes_mdblist_fonte(fonte);
}

// Onde os ajustes ficam. Ate a versao anterior nada era gravado: mexer numa
// opcao valia so enquanto o app estivesse aberto, e voltar depois mostrava tudo
// no padrao — o que faz a tela inteira parecer decorativa.
static char dirAjustes[512];


// Valores LITERAIS que o app web grava nas opcoes que nao sao booleanas. A
// ordem casa, uma a uma, com a do vetor de rotulos correspondente — e essa
// correspondencia e o contrato: mexer num vetor sem mexer no outro troca o
// ajuste da pessoa em silencio. Todos conferidos no codigo do app web.
static const char *W_DESCOBRIR[] = { "in_search", "in_sidebar", "off", NULL };
static const char *W_NOTAS[]     = { "SHOW_ALL", "HIDE_ALL", NULL };
static const char *W_CW[]        = { "card", "wide", "poster", NULL };
static const char *W_CW_ORDEM[]  = { "default", "streaming_style", "split_upcoming", NULL };
// `tmdb_language` chega da conta ja cortado na BASE ("pt", "en" — ver
// normalizeTmdbLanguageForAndroid no web). Posicional com V_TMDB_LING: "pt"
// vira Portugues (Brasil), e pt-PT e inalcancavel pelo blob — fica como
// escolha local apenas. O indice 0 e um sentinela: a conta sempre manda um
// idioma de verdade, e um idioma que a lista nao tem (digamos "nl") mantem o
// valor atual em vez de inventar um.
static const char *W_TMDB_LING[] = {
  "interface", "pt", "en", "es", "fr", "de", "it", "pt-pt", "ja", "ko", "zh",
  "ro", "uk", "ru",
  "nl", "pl", "tr", "sv", "da", "no", "cs", "sk", "sl", "hu", "lt", "bs", "sr",
  "bg", "el", "id", "vi", "zh-tw", "ar", NULL
};

// `heroSectionEnabled` -> `hero_section_enabled`. Uma sequencia de maiusculas
// conta como uma palavra so (`homeImdbRatingsVisibility` ->
// `home_imdb_ratings_visibility`, e nao `home_i_m_d_b_...`).
static void camelParaSnake(const char *src, char *dst, size_t tam) {
  size_t w = 0;
  int i;
  for (i = 0; src[i] && w + 2 < tam; i++) {
    int alto = src[i] >= 'A' && src[i] <= 'Z';
    if (alto && w > 0) {
      int anteriorBaixo = src[i - 1] >= 'a' && src[i - 1] <= 'z';
      int anteriorDigito = src[i - 1] >= '0' && src[i - 1] <= '9';
      int proximoBaixo = src[i + 1] >= 'a' && src[i + 1] <= 'z';
      if (anteriorBaixo || anteriorDigito || proximoBaixo) dst[w++] = '_';
    }
    dst[w++] = alto ? (char)(src[i] - 'A' + 'a') : src[i];
  }
  dst[w] = 0;
}

static int igualSemCaixa(const char *a, const char *b) {
  for (; *a && *b; a++, b++) {
    char x = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
    char y = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;
    if (x != y) return 1;
  }
  return *a || *b;   // 0 quando iguais, como strcmp
}

static const char *const *literaisDe(int op) {
  switch (op) {
    case AJ_DESCOBRIR:  return W_DESCOBRIR;
    case AJ_NOTAS_HOME: return W_NOTAS;
    // AJ_CW_FONTE NAO TEM LITERAIS DO WEB, e devolvia W_CW ("card", "wide",
    // "poster") — copia da linha de baixo. Nao mordia porque a chave e local
    // (cwFonteLocal nunca esta no blob) e somenteDesteAparelho a barra na
    // subida; mas com o "Simkl" no indice 3 um blob com "cw_fonte_local":
    // "poster" viraria Simkl. Sem literais, texto desconhecido e "mantido".
    case AJ_CW_ESTILO:  return W_CW;
    case AJ_CW_ORDEM:   return W_CW_ORDEM;
    case AJ_TMDB_IDIOMA: return W_TMDB_LING;
    case AJ_TEMA:       return W_TEMA;
    case AJ_FONTE_ESCOPO: return W_FONTE_ESCOPO;
    default:            return NULL;
  }
}

static int limita(int op, int v) {
  const Opcao *o = &OPCOES[op];
  if (o->tipo == OP_ESCOLHA) return (v >= 0 && v < nValores(op)) ? v : valor[op];
  if (o->tipo == OP_NUMERO)  return v < o->min ? o->min : (v > o->max ? o->max : v);
  return valor[op];
}

// CHAVE PESSOAL DO FANART.TV. Mora em fanart.txt na pasta de dados (por
// aparelho, como o portal IPTV), nunca no ajustes.txt nem na conta, e so
// aparece mascarada. Quem usa e artereserva.c (fonte "fanart.tv" do destaque).
static char fanartChave[64];
static void fanartAplicar(void) {
  arte_fonte_chave_fanart(fanartChave);
  artehero_fanart_disponivel(fanartChave[0] != 0);
}
static void fanartCarregar(void) {
  char *t = dados_ler("fanart.txt");
  size_t i, k = 0;
  fanartChave[0] = 0;
  // So hexadecimal: e o formato da chave pessoal, e assim uma linha torta no
  // arquivo nao vira cabecalho nem pedaco de url.
  for (i = 0; t && t[i] && k + 1 < sizeof fanartChave; i++)
    if ((t[i] >= '0' && t[i] <= '9') || (t[i] >= 'a' && t[i] <= 'f')) fanartChave[k++] = t[i];
  fanartChave[k] = 0;
  free(t);
  fanartAplicar();
}
static void fanartDefinir(const char *txt) {
  size_t i, k = 0;
  char nova[64];
  for (i = 0; txt && txt[i] && k + 1 < sizeof nova; i++)
    if ((txt[i] >= '0' && txt[i] <= '9') || (txt[i] >= 'a' && txt[i] <= 'f')) nova[k++] = txt[i];
  nova[k] = 0;
  snprintf(fanartChave, sizeof fanartChave, "%s", nova);
  // Campo vazio = esquecer a chave.
  if (fanartChave[0]) dados_gravar("fanart.txt", fanartChave);
  else dados_apagar("fanart.txt");
  fanartAplicar();
}
static const char *fanartMascarada(void) {
  static char m[24];
  size_t n = strlen(fanartChave);
  if (!n) return i18n("Não configurado");
  snprintf(m, sizeof m, "····%s", n > 4 ? fanartChave + n - 4 : "");
  return m;
}

// Two local key sources: the package default (NV_SEEKR_API_KEY) and a personal
// override in seekr.txt, never in account preferences. The personal key is
// masked and takes priority; removing it restores the package default.
// The TV's budget of 50 lookups per day applies to the package default only;
// a personal key follows its own Seekr plan (no local cap, no fixed number).
#ifndef NV_SEEKR_API_KEY
#define NV_SEEKR_API_KEY ""
#endif
static int seekrEmbutida(void) { return NV_SEEKR_API_KEY[0] != 0; }
static char seekrChave[96];
static int seekrSalvarFalhou;
static const char *SEEKR_ALFA =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
static void seekrLimpar(char *dst, size_t n, const char *t) {
  size_t i, k = 0;
  for (i = 0; t && t[i] && k + 1 < n; i++)
    if (strchr(SEEKR_ALFA, t[i])) dst[k++] = t[i];
  dst[k] = 0;
}
static void seekrCarregar(void) {
  char *t;
  t = dados_ler("seekr.txt");
  seekrLimpar(seekrChave, sizeof seekrChave, t);
  free(t);
  seekr_chave_propria(seekrChave[0] != 0);
  seekr_definir_chave(seekrChave[0] ? seekrChave : NV_SEEKR_API_KEY);
}
static void seekrDefinir(const char *txt) {
  char nova[sizeof seekrChave];
  seekrLimpar(nova, sizeof nova, txt);
  seekrSalvarFalhou = 0;
  if (!strcmp(nova, seekrChave)) return;
  if (nova[0] ? !dados_gravar("seekr.txt", nova) : !dados_apagar("seekr.txt")) {
    seekrSalvarFalhou = 1;
    return; // Keep the effective and durable old key consistent on failure.
  }
  snprintf(seekrChave, sizeof seekrChave, "%s", nova);
  seekr_chave_propria(seekrChave[0] != 0);
  seekr_definir_chave(seekrChave[0] ? seekrChave : NV_SEEKR_API_KEY);
}
static const char *seekrMascarada(void) {
  static char m[24];
  size_t n = strlen(seekrChave);
  if (!n) return i18n(seekrEmbutida() ? "Chave padrão do aplicativo" : "Não configurado");
  snprintf(m, sizeof m, "····%s", n > 4 ? seekrChave + n - 4 : "");
  return m;
}
int ajustes_seekr_habilitado(void) { return lig(AJ_SEEKR_LIGADO); }
int ajustes_seekr_ligado(void) { return ajustes_seekr_habilitado() && (seekrChave[0] || seekrEmbutida()); }
int ajustes_seekr_fita(void)   { return lig(AJ_SEEKR_FITA); }
int ajustes_seekr_ajuste_s(void) { return valor[AJ_SEEKR_AJUSTE]; }

// ENDERECO DO SERVIDOR P2P (p2p.h). Mora em p2p.txt na pasta de dados, por
// aparelho: e o IP de um PC/NAS da casa desta TV, sem sentido em outra.
static char p2pEndereco[200];
static void p2pCarregar(void) {
  char *t = dados_ler("p2p.txt");
  p2pEndereco[0] = 0;
  // Reaplica a normalizacao: arquivo editado a mao ou de outra versao nao pode
  // virar URL torta.
  if (t && !p2p_normalizar_url(t, p2pEndereco, sizeof p2pEndereco)) p2pEndereco[0] = 0;
  free(t);
}
const char *ajustes_p2p_url(void) { return p2pEndereco; }
int ajustes_definir_p2p_url(const char *texto) {
  char nova[200] = "";
  size_t i = 0;
  while (texto && (texto[i] == ' ' || texto[i] == '\t')) i++;
  if (texto && texto[i] && !p2p_normalizar_url(texto, nova, sizeof nova)) return 0;
  snprintf(p2pEndereco, sizeof p2pEndereco, "%s", nova);
  if (nova[0]) dados_gravar("p2p.txt", nova);
  else dados_apagar("p2p.txt");
  return 1;
}

// "TESTAR SERVIDOR P2P": p2p_testar espera ate P2P_PRAZO_TESTE s pela rede, e a
// TV nao pode parar de desenhar. Um fio por vez; ajustes_atualizar recolhe.
static pthread_t p2pFio;
static int p2pFioVivo;
static _Atomic int p2pTeste;            // 0 nunca/livre, 1 testando, 2 pronto
static int p2pTesteErro;
static char p2pTesteVersao[32];
static void *p2pTesteFio(void *u) {
  char v[32];
  int e = p2p_testar(v, sizeof v);
  (void)u;
  p2pTesteErro = e;
  snprintf(p2pTesteVersao, sizeof p2pTesteVersao, "%s", v);
  atomic_store_explicit(&p2pTeste, 3, memory_order_release);
  return NULL;
}
static void p2pTesteIniciar(void) {
  if (p2pFioVivo) return;
  atomic_store_explicit(&p2pTeste, 1, memory_order_release);
  if (pthread_create(&p2pFio, NULL, p2pTesteFio, NULL) != 0) {
    p2pTesteErro = P2P_ERR_SERVIDOR;
    atomic_store_explicit(&p2pTeste, 2, memory_order_release);
    return;
  }
  p2pFioVivo = 1;
}
static void p2pTesteRecolher(void) {
  if (p2pFioVivo && atomic_load_explicit(&p2pTeste, memory_order_acquire) == 3) {
    pthread_join(p2pFio, NULL);
    p2pFioVivo = 0;
    atomic_store_explicit(&p2pTeste, 2, memory_order_release);
  }
}
static const char *p2pTesteTexto(void) {
  static char buf[192];
  int e = atomic_load_explicit(&p2pTeste, memory_order_acquire);
  if (e == 0) return i18n("OK testa");
  if (e == 1 || e == 3) return i18n("testando…");
  switch (p2pTesteErro) {
    case P2P_OK:
      // Sem endereco o teste fala do motor embutido (p2p_testar ->
      // p2pmotor_resumo): versao e o teto DURO de disco que ele teria agora.
      if (!p2pEndereco[0] && p2pmotor_disponivel()) {
        char v[64], d[96];
        snprintf(v, sizeof v, "%s", p2pTesteVersao);
        { char *b = strstr(v, " /"); if (b) *b = 0; }   // "0.1.3 / libtorrent ..." -> "0.1.3"
        snprintf(d, sizeof d, "%s · %u MB", v, p2pmotor_teto_mb());
        snprintf(buf, sizeof buf, i18n("motor desta TV · versão %s"), d);
      } else
        snprintf(buf, sizeof buf, i18n("conectado · versão %s"), p2pTesteVersao);
      return buf;
    case P2P_ERR_SEM_ESPACO:  return i18n("Falta espaço livre na TV para o P2P");
    case P2P_ERR_DISCO:       return i18n("Não foi possível medir o espaço livre da TV");
    case P2P_ERR_DESLIGADO:
      // Sem endereco e sem motor neste pacote: dizer as duas coisas.
      return p2pmotor_disponivel() ? i18n("informe o endereço primeiro")
                                   : i18n("sem motor neste pacote · informe o endereço");
    case P2P_ERR_NAO_STREMIO: return i18n("respondeu, mas não é um servidor Stremio");
    default:                  return i18n("sem resposta do servidor");
  }
}

// CHAVES DE DEBRID DIGITADAS NESTA TV (debrid.h). Moram em debrid.txt na pasta
// de dados, uma linha "servico=chave" por servico, por aparelho — credencial do
// mesmo grau do fanart.txt. tools/arm.sh a tira do .ipk (ARQ_DE_PESSOA) e a tela
// so a mostra mascarada. O valor NUNCA volta para o campo da modal.
static const char *DEB_SERV[5] = { "alldebrid", "alldebrid", "realdebrid", "torbox", "premiumize" };
static char debLocal[5][100];
static int debIdx(int op) {
  switch (op) {
    case AJ_DEBRID_AD: return 0;
    case AJ_DEBRID_RD: return 2;
    case AJ_DEBRID_TB: return 3;
    case AJ_DEBRID_PM: return 4;
    default: return -1;
  }
}
static const char *DEB_ALFA =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.";
static void debLimpar(char *dst, size_t n, const char *t) {
  size_t i, k = 0;
  for (i = 0; t && t[i] && k + 1 < n; i++)
    if (strchr(DEB_ALFA, t[i]) && t[i]) dst[k++] = t[i];
  dst[k] = 0;
}
static void debGravar(void) {
  char out[5 * 128];
  int i, u = 0, algum = 0;
  out[0] = 0;
  for (i = 0; i < 5; i++) {
    if (i == 1 || !debLocal[i][0]) continue;
    u += snprintf(out + u, sizeof out - (size_t)u, "%s=%s\n", DEB_SERV[i], debLocal[i]);
    algum = 1;
  }
  if (algum) dados_gravar("debrid.txt", out);
  else dados_apagar("debrid.txt");
}
static void debCarregar(void) {
  char *t = dados_ler("debrid.txt"), *l, *fim;
  int i;
  memset(debLocal, 0, sizeof debLocal);
  for (l = t; l && *l; l = fim ? fim + 1 : NULL) {
    char *eq;
    fim = strchr(l, '\n');
    if (fim) *fim = 0;
    eq = strchr(l, '=');
    if (eq) {
      *eq = 0;
      for (i = 0; i < 5; i++)
        if (i != 1 && !strcmp(l, DEB_SERV[i])) debLimpar(debLocal[i], sizeof debLocal[i], eq + 1);
    }
    if (!fim) break;
  }
  free(t);
  for (i = 0; i < 5; i++) if (i != 1 && debLocal[i][0]) debrid_definir_chave_local(DEB_SERV[i], debLocal[i]);
}
static void debDefinir(int op, const char *txt) {
  int i = debIdx(op);
  if (i < 0) return;
  debLimpar(debLocal[i], sizeof debLocal[i], txt);
  debrid_definir_chave_local(DEB_SERV[i], debLocal[i]);   // vazio apaga
  debGravar();
}
static const char *debValor(int op) {
  static char buf[64], m[24];
  int i = debIdx(op);
  const char *serv;
  if (i < 0) return "";
  serv = DEB_SERV[i];
  debrid_chave_mascarada(serv, m, sizeof m);
  switch (debrid_origem(serv)) {
    case 2: snprintf(buf, sizeof buf, "%s", m[0] ? m : "····"); return buf;
    case 1: snprintf(buf, sizeof buf, i18n("da conta · %s"), m[0] ? m : "····"); return buf;
    default: return i18n("Não configurado");
  }
}

// "TESTAR CHAVE DO ALLDEBRID": um GET v4/user, que espera a rede; fio proprio
// como o teste do P2P para a TV nao parar de desenhar.
static pthread_t adFio;
static int adFioVivo;
static _Atomic int adTeste;             // 0 nunca/livre, 1 testando, 2 pronto, 3 fio terminou
static int adTesteOk;
static char adTesteMsg[64], adTesteData[16];
static void *adTesteFio(void *u) {
  char m[64], d[16];
  (void)u;
  adTesteOk = debrid_testar_alldebrid(m, sizeof m, d, sizeof d);
  snprintf(adTesteData, sizeof adTesteData, "%s", d);
  snprintf(adTesteMsg, sizeof adTesteMsg, "%s", m);
  atomic_store_explicit(&adTeste, 3, memory_order_release);
  return NULL;
}
static void adTesteIniciar(void) {
  if (adFioVivo) return;
  atomic_store_explicit(&adTeste, 1, memory_order_release);
  if (pthread_create(&adFio, NULL, adTesteFio, NULL) != 0) {
    snprintf(adTesteMsg, sizeof adTesteMsg, "sem resposta do servidor");
    adTesteOk = 0;
    atomic_store_explicit(&adTeste, 2, memory_order_release);
    return;
  }
  adFioVivo = 1;
}
static void adTesteRecolher(void) {
  if (adFioVivo && atomic_load_explicit(&adTeste, memory_order_acquire) == 3) {
    pthread_join(adFio, NULL);
    adFioVivo = 0;
    atomic_store_explicit(&adTeste, 2, memory_order_release);
  }
}
// O texto vem de debrid_testar_alldebrid como CHAVE de i18n (frases fixas,
// listadas em idioma_tab.h); a de "premium até %s" leva a data a parte.
static const char *adTesteTexto(void) {
  static char buf[64];
  int e = atomic_load_explicit(&adTeste, memory_order_acquire);
  if (e == 0) return debrid_origem("alldebrid") ? i18n("OK testa") : i18n("informe a chave primeiro");
  if (e == 1 || e == 3) return i18n("testando…");
  if (adTesteOk && adTesteData[0]) {
    snprintf(buf, sizeof buf, i18n("premium até %s"), adTesteData);
    return buf;
  }
  return i18n(adTesteMsg);
}

// "TESTAR CHAVE DO SEEKR": GET /v1/keys/validate, em fio proprio como o do
// AllDebrid. 0 nunca, 1 testando, 2 pronto, 3 fio terminou.
static pthread_t skFio;
static int skFioVivo;
static _Atomic int skTeste;
static int skTesteRes;
static char skTesteChave[96];
static void *skTesteFioF(void *u) {
  (void)u;
  skTesteRes = seekr_validar(skTesteChave);
  atomic_store_explicit(&skTeste, 3, memory_order_release);
  return NULL;
}
static void skTesteIniciar(void) {
  if (skFioVivo || (!seekrChave[0] && !seekrEmbutida())) return;
  snprintf(skTesteChave, sizeof skTesteChave, "%s", seekrChave[0] ? seekrChave : NV_SEEKR_API_KEY);
  atomic_store_explicit(&skTeste, 1, memory_order_release);
  if (pthread_create(&skFio, NULL, skTesteFioF, NULL) != 0) {
    skTesteRes = -1;
    atomic_store_explicit(&skTeste, 2, memory_order_release);
    return;
  }
  skFioVivo = 1;
}
static void skTesteRecolher(void) {
  if (skFioVivo && atomic_load_explicit(&skTeste, memory_order_acquire) == 3) {
    pthread_join(skFio, NULL);
    skFioVivo = 0;
    atomic_store_explicit(&skTeste, 2, memory_order_release);
  }
}
static const char *skTesteTexto(void) {
  int e = atomic_load_explicit(&skTeste, memory_order_acquire);
  if (!seekrChave[0] && !seekrEmbutida()) return i18n("informe a chave primeiro");
  if (e == 0) return i18n("OK testa");
  if (strcmp(skTesteChave, seekrChave[0] ? seekrChave : NV_SEEKR_API_KEY)) return i18n("OK testa");
  if (e == 1 || e == 3) return i18n("testando…");
  if (skTesteRes > 0) return i18n("chave válida");
  if (skTesteRes == 0) return i18n("chave recusada");
  return i18n("sem resposta do servidor");
}

static const char *seekrAjuda(int op) {
  static char texto[900];
  char uso[140], libera[100] = "", hora[48];
  SeekrUso u; seekr_uso(&u);
  const char *situacao = i18n(seekr_estado_rotulo(seekr_estado()));
  if (seekrSalvarFalhou)
    situacao = i18n("Não foi possível salvar a chave. A chave anterior foi mantida.");
  else if (!u.chavePropria && !u.persistente)
    situacao = i18n(seekr_estado_rotulo(SEEKR_ARMAZENAMENTO_INDISPONIVEL));
  else if (!u.chavePropria && u.relogioAtrasado)
    situacao = i18n("Confira a data e a hora desta TV. O contador não foi reiniciado.");
  if (u.chavePropria) uso[0] = 0;   // plano da chave pessoal: sem numero fixo aqui
  else if (u.persistente)
    snprintf(uso, sizeof uso, i18n("%d de %d consultas hoje (UTC)"), u.usadas, u.limite);
  else snprintf(uso, sizeof uso, "%s", i18n("Uso do Seekr indisponível"));
  long long quando = seekr_estado() == SEEKR_LIMITE_PROVEDOR ? u.retryUtc : u.reinicioUtc;
  if ((!u.chavePropria || seekr_estado() == SEEKR_LIMITE_PROVEDOR) && u.persistente && !u.relogioAtrasado && seekr_horario_local(quando, hora, sizeof hora))
    snprintf(libera, sizeof libera, i18n(seekr_estado() == SEEKR_LIMITE_PROVEDOR ?
             "Tente após %s (hora local)" : "Renova em %s (hora local)"), hora);
  const char *sobre = op == AJ_SEEKR_TESTAR ?
    (u.chavePropria ? "Testar a chave não gasta consultas. Os limites são os do plano da sua chave no Seekr." :
    "Testar a chave não gasta consultas. Os limites do Seekr são separados do limite desta TV.") :
    u.chavePropria ? "Com a sua chave, valem os limites do seu plano no Seekr; esta TV não impõe um limite próprio. Cache não gasta consultas." :
    "Até 50 consultas por instalação e dia UTC, compartilhadas por todos os perfis e chaves. Cache não gasta consultas; uma tentativa enviada à rede gasta mesmo se falhar.";
  // The Settings inspector has four lines. Keep live usage/reset/state ahead
  // of explanatory copy so a long translation cannot hide the actionable data.
  snprintf(texto, sizeof texto, "%s%s%s%s%s\n%s", uso, uso[0] ? "\n" : "", libera,
           libera[0] ? "\n" : "", situacao, i18n(sobre));
  return texto;
}

// "ADICIONAR PACOTE DE SELOS": baixa o JSON do pacote por URL (fio proprio, como
// o teste do Seekr: rede_baixar bloqueia), e na thread principal valida e
// guarda (selospacote_adicionar). 0 livre, 1 baixando, 3 fio terminou, 2 pronto.
static pthread_t spFio;
static int spFioVivo;
static _Atomic int spEstado;
static char spUrl[512];
static char *spCorpo;
static int spResultado = -1;      // -1 nada; SelosResultado; 100 sem resposta; 101 endereco invalido
static void *spBaixarFio(void *u) {
  (void)u;
  spCorpo = rede_baixar(spUrl, 20);
  atomic_store_explicit(&spEstado, 3, memory_order_release);
  return NULL;
}
static void spAdicionar(const char *url) {
  char t[512];
  size_t n;
  snprintf(t, sizeof t, "%s", url ? url : "");
  n = strlen(t);
  while (n && (t[n - 1] == ' ' || t[n - 1] == '\n')) t[--n] = 0;
  if (!n || spFioVivo) return;
  if (strncmp(t, "http://", 7) && strncmp(t, "https://", 8)) { spResultado = 101; return; }
  snprintf(spUrl, sizeof spUrl, "%s", t);
  spResultado = -1;
  atomic_store_explicit(&spEstado, 1, memory_order_release);
  if (pthread_create(&spFio, NULL, spBaixarFio, NULL) != 0) {
    spResultado = 100;
    atomic_store_explicit(&spEstado, 2, memory_order_release);
    return;
  }
  spFioVivo = 1;
}
// Espelho da escolha: o valor da linha e selospacote_ativo() + 1.
static void spEspelhar(void) { valor[AJ_SELOS_PACOTE] = selospacote_ativo() + 1; }
static void spRecolher(void) {
  if (spFioVivo && atomic_load_explicit(&spEstado, memory_order_acquire) == 3) {
    pthread_join(spFio, NULL);
    spFioVivo = 0;
    if (!spCorpo || strlen(spCorpo) > 4u * 1024u * 1024u) spResultado = spCorpo ? SELOS_ERR_JSON : 100;
    else spResultado = selospacote_adicionar(spCorpo, spUrl);
    free(spCorpo); spCorpo = NULL;
    atomic_store_explicit(&spEstado, 2, memory_order_release);
    if (spResultado == SELOS_OK) printf("[selos] pacote adicionado: %s\n", rede_url_publica(spUrl, (char[160]){0}, 160));
    fflush(stdout);
  }
  spEspelhar();
}
static const char *spAddTexto(void) {
  if (atomic_load_explicit(&spEstado, memory_order_acquire) == 1 ||
      atomic_load_explicit(&spEstado, memory_order_acquire) == 3) return i18n("baixando…");
  switch (spResultado) {
    case SELOS_OK:            return i18n("adicionado");
    case SELOS_ERR_JSON:      return i18n("isso não é um JSON de selos");
    case SELOS_ERR_VAZIO:     return i18n("nenhum selo válido nele");
    case SELOS_ERR_LIMITE:    return i18n("limite de 3 pacotes");
    case SELOS_ERR_DUPLICADO: return i18n("já está na conta");
    case 100:                 return i18n("sem resposta do servidor");
    case 101:                 return i18n("o endereço precisa começar com http");
    default:                  return i18n("OK adiciona");
  }
}
static const char *SP_ALFA_URL =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/.-_?=&%#+~@!,;";

void ajustes_dir(const char *dir) {
  FILE *f;
  char caminho[600], linha[96];
  if (!dir || !*dir) return;
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  fonteregra_carregar();
  fanartCarregar();
  seekrCarregar();
  p2pCarregar();
  pstCarregar();
  debCarregar();
  // ANTES DO LACO, e nao so no fim (#129): limita() confere as duas linhas de
  // idioma contra nValores() -> nLingua, e quem preenche nLingua e esta
  // chamada. No arranque ela ainda nao tinha rodado: a lista tinha "1 valor",
  // "legendaIdioma 3" era recusado como fora da faixa e a legenda voltava a
  // "Da conta". Quem salvava era a SEGUNDA leitura de main.c — e a migracao do
  // Tizen logo abaixo grava no meio da primeira, levando o padrao ao disco.
  rotulosDeIdioma();
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dirAjustes);
  f = fopen(caminho, "r");
  if (!f) {
    // Nunca gravou nada: o idioma nasce automatico (o padrao de valor[]).
    valor[AJ_IDIOMA] = 0;
    ajSombraSync();
    return;
  }
  { int viuIdioma = 0, viuAuto = 0, idiomaGravado = IDIOMA_EN, autoGravado = 0;
    int medidorAntigo = -1, viuMedidor = 0, resAntiga = -1, viuRes = 0, prazoAntigo = -1, viuEspera = 0;
  while (fgets(linha, sizeof linha, f)) {
    char chave[64]; int v, i;
    if (sscanf(linha, "%63s %d", chave, &v) != 2) continue;
    // O idioma nao passa pelo laco: o numero do disco e um IDIOMA_*, e o
    // indice da lista da tela e outro (ver "IDIOMA AUTOMATICO").
    if (!strcmp(chave, "idioma")) {
      if (v >= 0 && v < IDIOMA_N) { idiomaGravado = v; viuIdioma = 1; }
      continue;
    }
    if (!strcmp(chave, "idiomaAutoLocal")) { autoGravado = v == 1; viuAuto = 1; continue; }
    // "resolucao_ui" (ate a 2.0.1) virou "resolucaoUi" com Automatica na
    // frente: ver res_migrar. A chave nova, quando existe, vence.
    if (!strcmp(chave, "resolucao_ui")) { resAntiga = v; continue; }
    // O MEDIDOR ERA LIGA/DESLIGA ("medidorDesempenhoLocal", V_LIGA: 0 = ligado)
    // e virou a forma na ilha (V_MEDIDOR). Ligado continua visivel: Grande, o
    // painel inteiro de antes; desligado continua desligado.
    if (!strcmp(chave, "medidorDesempenhoLocal")) { medidorAntigo = v; continue; }
    // "Espera pelos add-ons" ganhou Instantaneo/15 s/30 s (#202): o indice de
    // antes (3 s, 5 s, 8 s, Todos) vira o da lista nova.
    if (!strcmp(chave, "fontePrazoLocal")) { prazoAntigo = v; continue; }
    if (!strcmp(chave, "idiomaFonteLocal")) {
      if (v >= IDA_TMDB && v <= IDA_PADRAO) idiomaFonteGravada = v;
      continue;
    }
    for (i = 0; i < AJ_N; i++) {
      if (!CHAVE[i] || strcmp(CHAVE[i], chave)) continue;
      if (OPCOES[i].tipo == OP_LEITURA || OPCOES[i].tipo == OP_ACAO) continue;
      // Valor fora da faixa (arquivo de outra versao, ou editado a mao) cai no
      // padrao em vez de indexar fora do vetor.
      valor[i] = limita(i, v);
      if (i == AJ_MEDIDOR) viuMedidor = 1;
      if (i == AJ_RESOLUCAO) viuRes = 1;
      if (i == AJ_FONTE_PRAZO) viuEspera = 1;
      break;
    }
  }
  if (!viuMedidor && medidorAntigo >= 0) valor[AJ_MEDIDOR] = medidorAntigo == 0 ? 3 : 0;
  if (!viuRes && resAntiga >= 0) {
    valor[AJ_RESOLUCAO] = res_migrar(resAntiga);
    printf("[ajustes] resolucao da interface: resolucao_ui %d -> resolucaoUi %d\n", resAntiga, valor[AJ_RESOLUCAO]);
  }
  if (!viuEspera && prazoAntigo >= 0) {
    static const int DE_PARA[4] = { 1, 2, 3, 6 };   // 3 s, 5 s, 8 s, Todos
    valor[AJ_FONTE_PRAZO] = prazoAntigo < 4 ? DE_PARA[prazoAntigo] : valorPadrao[AJ_FONTE_PRAZO];
  }
  // Escolha manual: "idioma" gravado e SEM a marca de automatico (arquivo de
  // antes da marca, ou de quem escolheu). Sem "idioma" nenhum, nunca houve
  // escolha e o automatico vale.
  if (viuIdioma && !(viuAuto && autoGravado)) valor[AJ_IDIOMA] = 1 + idiomaGravado;
  else valor[AJ_IDIOMA] = 0;
  // No automatico, o gravado e o ultimo resolvido: a TV abre nele e a conta,
  // quando chegar, corrige.
  if (valor[AJ_IDIOMA] == 0 && viuIdioma) idiomaEfetivo = idiomaGravado;
  }
  fclose(f);
  // "DINAMICA ESTILIZADA" SAIU (03/10/2026): a base tingida virou o Fundo
  // Frost, que vale para qualquer acento. Quem a tinha abre em Da arte + Frost
  // — a mesma cor viva, o mesmo fundo de um matiz so — e o arquivo e regravado.
  if (valor[AJ_TEMA] == AJ_TEMA_ESTILIZADA) {
    valor[AJ_TEMA] = AJ_TEMA_DINAMICA;
    valor[AJ_FUNDO] = FUNDO_FROST;
    printf("[ajustes] tema estilizado -> Da arte + Fundo Frost\n");
    gravar();
  }
  // MIGRACAO UNICA (2.0.2, owner): everyone goes to Automatic once, 4K stored
  // or not (resolucao.h: Automatic now probes 4K by itself). The flag is per
  // TV (data folder); any change made AFTER it is respected.
  { char *m = dados_ler(RES_ARQ_MIGRADA);
    if (m) free(m);
    else {
      if (valor[AJ_RESOLUCAO] != RES_AUTO)
        printf("[ajustes] resolucao da interface %d -> Automatica (migracao unica da 2.0.2)\n", valor[AJ_RESOLUCAO]);
      valor[AJ_RESOLUCAO] = RES_AUTO;
      dados_apagar(RES_ARQ_RECUO); dados_apagar(RES_ARQ_AUTO);
      dados_gravar(RES_ARQ_MIGRADA, "1\n");
      gravar();
    } }
  // MIGRACAO UNICA (1.5.1, #149): religa o envio automatico. Ate a 1.5.0 a
  // abertura da tela de Ajustes desligava o envio e a gravacao seguinte
  // levava o "desligado" ao disco — quem o tem no arquivo, na maioria, nao
  // escolheu isso. O dono decidiu religar uma vez. A unica recusa que se sabe
  // distinguir e o "nao" do cartao de consentimento do Tizen
  // (telemetria-perguntado.txt = 0): essa fica. A marca mora ao lado do
  // ajustes.txt, e o gravar() abaixo e quem avisa o IDBFS no Tizen.
  { char marca[640];
    FILE *m;
    snprintf(marca, sizeof marca, "%s/envio-151.txt", dirAjustes);
    m = fopen(marca, "r");
    if (m) fclose(m);
    else {
      char *resp = dados_ler("telemetria-perguntado.txt");
      int disseNao = resp && resp[0] == '0';
      free(resp);
      if (!disseNao && valor[AJ_ENVIO_AUTO] != 0) {
        valor[AJ_ENVIO_AUTO] = 0;
        printf("[ajustes] envio automatico religado (migracao unica do #149)\n");
        fflush(stdout);
      }
      m = fopen(marca, "w");
      if (m) { fputs("1\n", m); fclose(m); }
      gravar();
    } }
#if (defined(__EMSCRIPTEN__) || defined(NV_TPK)) && !defined(NV_TRAILER_AUTO_TIZEN)
  // MIGRACAO UNICA (1.3.10): o .wgt da 1.3.9 saiu de uma build com
  // NV_TRAILER_AUTO_TIZEN (a das fotos das notas), entao na Samsung o
  // autoplay do trailer nasceu LIGADO — e qualquer gravacao de ajustes
  // naquela versao escreveu "trailerAuto 0" no arquivo, o que o padrao novo
  // acima nao alcanca. Uma vez, marcada em disco, os dois voltam a
  // desligado; quem quiser liga de novo e a escolha fica.
  { char *m = dados_ler("trailer-1310.txt");
    if (m) free(m);
    else {
      // V_LIGA e { Ligado, Desligado }: 1 e DESLIGADO (ver lig()).
      valor[AJ_DET_TRAILER_AUTO] = 1;
      valor[AJ_HERO_TRAILER] = 1;
      dados_gravar("trailer-1310.txt", "1\n");
      gravar();
    } }
#endif
  // MIGRACAO UNICA (2.0.3, dono): o tamanho dos Ajustes volta ao padrao de 80%
  // para quem ainda esta no padrao antigo (90%, ate e87ecb3c). O arquivo grava
  // o indice, entao 90% guardado e quase sempre o padrao que ninguem escolheu.
  // Quem tem 100% escolheu, e quem trocar depois desta marca, fica.
  { char *m = dados_ler("ajustesescala-203.txt");
    if (m) free(m);
    else {
      if (valor[AJ_TAMANHO_AJUSTES] == 1) {
        valor[AJ_TAMANHO_AJUSTES] = 0;
        printf("[ajustes] tamanho dos ajustes 90%% -> 80%% (migracao unica da 2.0.3)\n");
      }
      dados_gravar("ajustesescala-203.txt", "1\n");
      gravar();
    } }
  // MIGRACAO UNICA (2.0): o fundo da escolha de perfil volta a Filmes (0, a
  // parede de cartazes de cada perfil, PS_FUNDO_FILMES em psestilos.h) para
  // todo mundo, uma vez. O dono decidiu: quem tinha Listras ou Arte do perfil
  // de antes nunca veria a tela nova. Quem trocar depois, fica. Em GPU fraca
  // perfilsel.c ainda desenha Luz no lugar.
  { char *m = dados_ler("perfilfundo-20.txt");
    if (m) free(m);
    else {
      if (valor[AJ_PS_FUNDO] != 0)
        printf("[ajustes] fundo da escolha de perfil -> Filmes (migracao unica da 2.0)\n");
      valor[AJ_PS_FUNDO] = 0;
      dados_gravar("perfilfundo-20.txt", "1\n");
      gravar();
    } }
  // MIGRACAO UNICA (2.0): a aparencia de quem ja tinha o app vira a de fabrica
  // da 2.0 — Da arte (era Imersiva ate o dono trocar, 05/10), Cor da logo ligada, fundo Frost e interface de vidro
  // desligada. Decisao do dono (05/10/2026): "para novos e antigos usuarios".
  // Uma vez so; quem trocar depois, fica. Em modo seguro (SEGURO) a cor
  // dinamica continua desligada na leitura, como sempre.
  { char *m = dados_ler("aparencia-20.txt");
    if (m) free(m);
    else {
      printf("[ajustes] aparencia -> Da arte + Cor da logo + Frost, sem vidro (migracao unica da 2.0)\n");
      valor[AJ_TEMA] = AJ_TEMA_DINAMICA;
      valor[AJ_COR_LOGO] = 0;        // V_LIGA: 0 = Ligado
      valor[AJ_FUNDO] = FUNDO_FROST;
      valor[AJ_VIDRO] = 1;           // V_LIGA: 1 = Desligado
      dados_gravar("aparencia-20.txt", "1\n");
      gravar();
    } }
  // O limite mora em fileiras.c; esta linha e so o espelho dele. Ler daqui em
  // vez de gravar evita a divergencia: o arquivo de ajustes nao guarda o
  // numero, entao nao ha como os dois discordarem.
  valor[AJ_FIL_LIMITE] = fil_limite_gravado();
  // A escolha lida do disco so existe de verdade quando chega em linguas.c.
  rotulosDeIdioma();
  aplicarIdioma(AJ_LEG_LINGUA);
  aplicarIdioma(AJ_LEG_LINGUA2);
  aplicarIdioma(AJ_AUD_LINGUA);
  txt_definir_fonte_interface((TxtFamilia)valor[AJ_FONTE_UI]);
  gfx_escala_ui_definir(ajustes_tamanho_ui());
  // O teto de imagens escolhido vale desde o arranque, nao so quando a tela
  // de Ajustes e aberta. tex_iniciar ja rodou (main.c); isto so o corrige.
  if (valor[AJ_TEX_MB] > 0) tex_definir_orcamento_mb(ajustes_tex_mb());
  pstAplicar();
  ajSombraSync();   // o que veio do disco nao e mudanca de ninguem
}

static int gravar(void) {
  char caminho[600], tmp[600];
  FILE *f;
  int i;
  if (!dirAjustes[0]) return 0;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dirAjustes);
  snprintf(tmp, sizeof tmp, "%s/ajustes.tmp", dirAjustes);
  f = fopen(tmp, "w");
  if (!f) return 0;
  for (i = 0; i < AJ_N; i++) {
    // "-" marca linha local (versao, espaco, conta): nao tem valor para
    // guardar. Acao tambem nao. E chave ausente NUNCA vai para o arquivo — foi
    // um "(null) 0" gravado assim que derrubou o app na leitura seguinte.
    if (!CHAVE[i] || CHAVE[i][0] == '-') continue;
    if (OPCOES[i].tipo == OP_LEITURA || OPCOES[i].tipo == OP_ACAO) continue;
    if (i == AJ_IDIOMA) {
      // O numero do disco e o IDIOMA_* em vigor; a marca diz se e automatico.
      fprintf(f, "%s %d\n", CHAVE[i], ajustes_idioma());
      fprintf(f, "idiomaAutoLocal %d\n", valor[i] == 0);
      fprintf(f, "idiomaFonteLocal %d\n", idiomaFonteGravada);
      continue;
    }
    fprintf(f, "%s %d\n", CHAVE[i], valor[i]);
  }
  { int erro = ferror(f);
    if (fclose(f)) erro = 1;
    if (erro || rename(tmp, caminho)) { remove(tmp); return 0; } }
  // SAMSUNG (#85, 21/09/2026): o arquivo fica no IDBFS, e o IDBFS so vai ao
  // IndexedDB quando alguem marca o sistema de arquivos como sujo. Este
  // gravador escreve por fora de dados_gravar e nunca marcava: qualquer
  // ajuste local (borda do cartaz, tamanho do poster, profundidade, cor de
  // destaque, fonte ao reproduzir) sobrevivia so ate a proxima descarga que
  // OUTRO modulo pedisse — e num app que so navegou, ate o proximo arranque,
  // onde voltava ao padrao. Na LG o disco e real e nada disto acontecia.
  dados_marcar_sujo(0);
  ajLogDiff();
  // Provedor de poster ou idioma da interface mudaram? So reconfigura se a
  // configuracao final for outra (reconfigurar zera a memoria de falhas).
  pstAplicar();
  return 1;
}


// POSTERES PERSONALIZADOS (posterprov.h). Os campos moram em posteres.txt
// (chave=valor por linha) na pasta de dados: por aparelho, como fanart.txt e
// p2p.txt. O provedor escolhido mora em ajustes.txt (posterProvLocal).
//
// NADA DISTO VAI PARA O LOG NEM PARA A TELA POR INTEIRO: o token e a chave
// aparecem mascarados ("····abcd"), como a chave do fanart.tv.
static char pstInst[PP_INSTANCIA_MAX], pstToken[PP_TOKEN_MAX + 1], pstExtra[PP_EXTRA_MAX + 1];
static char pstChave[PP_CHAVE_MAX], pstModelo[PP_MODELO_MAX];
// Ultima recusa de um campo digitado (0 = nenhuma); aparece na linha "Testar".
enum { PST_OK = 0, PST_TOKEN_RUIM, PST_INST_RUIM, PST_EXTRA_RUIM, PST_CHAVE_RUIM, PST_MODELO_RUIM,
       PST_MODELO_LONGO };
static int pstAviso;

static const char *pstCodigoLingua(void) {
  static const char *L[] = { "pt", "en", "ro", "uk", "ru", "fr", "de", "es" };
  int i = ajustes_idioma();
  return (i >= 0 && i < 8) ? L[i] : "pt";
}
// Copia sem estourar `n` (e sem o aviso de truncamento do snprintf).
static void pstCopia(char *dst, size_t n, const char *src) {
  size_t k = src ? strlen(src) : 0;
  if (k >= n) k = n - 1;
  if (k) memcpy(dst, src, k);
  dst[k] = 0;
}
static void pstAplicar(void) {
  PosterProvCfg c;
  memset(&c, 0, sizeof c);
  c.prov = valor[AJ_POSTER_PROV];
  pstCopia(c.instancia, sizeof c.instancia, pstInst);
  pstCopia(c.token, sizeof c.token, pstToken);
  pstCopia(c.extra, sizeof c.extra, pstExtra);
  pstCopia(c.chave, sizeof c.chave, pstChave);
  pstCopia(c.modelo, sizeof c.modelo, pstModelo);
  pstCopia(c.lang, sizeof c.lang, pstCodigoLingua());
  if (memcmp(&c, posterprov_cfg(), sizeof c)) posterprov_configurar(&c);
}
static void pstSalvar(void) {
  char b[PP_INSTANCIA_MAX + PP_TOKEN_MAX + PP_EXTRA_MAX + PP_CHAVE_MAX + PP_MODELO_MAX + 64];
  if (!pstInst[0] && !pstToken[0] && !pstExtra[0] && !pstChave[0] && !pstModelo[0]) {
    dados_apagar("posteres.txt");
    return;
  }
  snprintf(b, sizeof b, "inst=%s\ntoken=%s\nextra=%s\nchave=%s\nmodelo=%s\n",
           pstInst, pstToken, pstExtra, pstChave, pstModelo);
  dados_gravar("posteres.txt", b);
}
static void pstCarregar(void) {
  char *t = dados_ler("posteres.txt"), *p, *fim;
  char v[PP_MODELO_MAX + 8];
  pstInst[0] = pstToken[0] = pstExtra[0] = pstChave[0] = pstModelo[0] = 0;
  for (p = t; p && *p; p = fim ? fim + 1 : NULL) {
    char *eq;
    size_t n;
    fim = strchr(p, '\n');
    n = fim ? (size_t)(fim - p) : strlen(p);
    eq = memchr(p, '=', n);
    if (!eq) continue;
    { size_t nv = n - (size_t)(eq + 1 - p);
      if (nv >= sizeof v) continue;
      memcpy(v, eq + 1, nv); v[nv] = 0;
      if (nv && v[nv - 1] == '\r') v[nv - 1] = 0; }
    // Reaplica as MESMAS validacoes de quando se digita: arquivo editado a mao
    // ou de outra versao nao pode virar URL torta.
    if (!strncmp(p, "inst=", 5)) { if (!posterprov_normalizar_instancia(v, pstInst, sizeof pstInst)) pstInst[0] = 0; }
    else if (!strncmp(p, "token=", 6)) { if (!posterprov_extrair_token(v, pstToken, sizeof pstToken, NULL, 0)) pstToken[0] = 0; }
    else if (!strncmp(p, "extra=", 6)) { if (!posterprov_extra_normalizar(v, pstExtra, sizeof pstExtra)) pstExtra[0] = 0; }
    else if (!strncmp(p, "chave=", 6)) { pstCopia(pstChave, sizeof pstChave, v); }
    else if (!strncmp(p, "modelo=", 7)) { if (posterprov_modelo_valido(v)) pstCopia(pstModelo, sizeof pstModelo, v); }
  }
  free(t);
}
// O que a pessoa digitou/colou num campo. Vazio apaga.
static void pstDefinir(int op, const char *texto) {
  char b[PP_MODELO_MAX + 8], inst[PP_INSTANCIA_MAX];
  size_t i = 0, k;
  pstAviso = PST_OK;
  while (texto && (texto[i] == ' ' || texto[i] == '\t')) i++;
  pstCopia(b, sizeof b, texto ? texto + i : "");
  k = strlen(b);
  while (k && (b[k - 1] == ' ' || b[k - 1] == '\t')) b[--k] = 0;
  switch (op) {
    case AJ_POSTER_INST:
      if (!b[0]) pstInst[0] = 0;
      else if (!posterprov_normalizar_instancia(b, pstInst, sizeof pstInst)) { pstAviso = PST_INST_RUIM; return; }
      break;
    case AJ_POSTER_TOKEN:
      // Aceita o manifest colado inteiro: o host vira a instancia.
      if (!posterprov_extrair_token(b, pstToken, sizeof pstToken, inst, sizeof inst)) { pstAviso = PST_TOKEN_RUIM; return; }
      if (inst[0]) pstCopia(pstInst, sizeof pstInst, inst);
      break;
    case AJ_POSTER_EXTRA:
      if (!posterprov_extra_normalizar(b, pstExtra, sizeof pstExtra)) { pstAviso = PST_EXTRA_RUIM; return; }
      break;
    case AJ_POSTER_CHAVE: {
      PosterProvCfg c;
      char u[PP_URL_MAX];
      memset(&c, 0, sizeof c);
      c.prov = PP_RPDB;
      pstCopia(c.chave, sizeof c.chave, b);
      if (b[0] && !posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u)) { pstAviso = PST_CHAVE_RUIM; return; }
      pstCopia(pstChave, sizeof pstChave, b);
      break; }
    case AJ_POSTER_MODELO:
      // Sintaxe certa mas endereco pronto acima de PP_URL_MAX: aviso de tamanho,
      // nao o de sintaxe (que mandaria corrigir o que esta certo).
      if (b[0] && !posterprov_modelo_cabe(b)) {
        pstAviso = posterprov_modelo_valido(b) ? PST_MODELO_LONGO : PST_MODELO_RUIM;
        return;
      }
      pstCopia(pstModelo, sizeof pstModelo, b);
      break;
  }
  pstSalvar();
  pstAplicar();
}

// "TESTAR POSTERES": baixa o cartaz de um filme conhecido. Um cartaz frio e
// montado no servidor (medido ~3 s na instancia publica), entao o prazo e de
// 20 s e a TV nao pode parar de desenhar: um fio por vez, recolhido em
// ajustes_atualizar (mesmo desenho do teste do servidor P2P).
static pthread_t pstFio;
static int pstFioVivo;
static _Atomic int pstTeste;            // 0 nunca, 1 testando, 2 pronto, 3 fio acabou
enum { PST_T_OK = 0, PST_T_CONFIG, PST_T_SEM_RESPOSTA, PST_T_NAO_IMAGEM };
static int pstTesteRes;
static long pstTesteKB, pstTesteMs;
static void *pstTesteFio(void *u) {
  PosterProvCfg c = *posterprov_cfg();
  char url[PP_URL_MAX];
  long n = 0;
  (void)u;
  if (!posterprov_montar_url(&c, "tt0111161", 278, "movie", url, sizeof url)) {
    pstTesteRes = PST_T_CONFIG;
  } else {
    Uint32 t0 = SDL_GetTicks();
    char *r = rede_baixar_bin(url, 20, &n);
    pstTesteMs = (long)(SDL_GetTicks() - t0);
    pstTesteKB = (n + 512) / 1024;
    if (!r || n <= 512) pstTesteRes = PST_T_SEM_RESPOSTA;
    else {
      const unsigned char *b0 = (const unsigned char *)r;
      int img = (b0[0] == 0xFF && b0[1] == 0xD8) || (b0[0] == 0x89 && b0[1] == 'P') ||
                (b0[0] == 'R' && b0[1] == 'I' && b0[2] == 'F' && b0[3] == 'F');
      pstTesteRes = img ? PST_T_OK : PST_T_NAO_IMAGEM;
    }
    free(r);
  }
  atomic_store_explicit(&pstTeste, 3, memory_order_release);
  return NULL;
}
static void pstTesteIniciar(void) {
  if (pstFioVivo) return;
  pstAviso = PST_OK;
  pstAplicar();
  atomic_store_explicit(&pstTeste, 1, memory_order_release);
  if (pthread_create(&pstFio, NULL, pstTesteFio, NULL) != 0) {
    pstTesteRes = PST_T_SEM_RESPOSTA;
    atomic_store_explicit(&pstTeste, 2, memory_order_release);
    return;
  }
  pstFioVivo = 1;
}
static void pstTesteRecolher(void) {
  if (pstFioVivo && atomic_load_explicit(&pstTeste, memory_order_acquire) == 3) {
    pthread_join(pstFio, NULL);
    pstFioVivo = 0;
    atomic_store_explicit(&pstTeste, 2, memory_order_release);
  }
}
static const char *pstMascara(const char *seg) {
  static char m[24];
  size_t n = strlen(seg);
  if (!n) return i18n("Não configurado");
  snprintf(m, sizeof m, "····%s", n > 4 ? seg + n - 4 : "");
  return m;
}
static const char *pstTexto(int op) {
  static char buf[96];
  switch (op) {
    case AJ_POSTER_INST:
      return pstInst[0] ? pstInst : i18n("Instância pública");
    case AJ_POSTER_TOKEN:  return pstMascara(pstToken);
    case AJ_POSTER_EXTRA:  return pstExtra[0] ? pstExtra : i18n("Nenhum");
    case AJ_POSTER_CHAVE:  return pstMascara(pstChave);
    case AJ_POSTER_MODELO:
      if (!pstModelo[0]) return i18n("Não configurado");
      return posterprov_redigir(pstModelo, buf, sizeof buf);   // so o host
    default: break;
  }
  // AJ_POSTER_TESTAR
  switch (pstAviso) {
    case PST_TOKEN_RUIM:  return i18n("token inválido (até 400 letras, números e _ . ~ = -)");
    case PST_INST_RUIM:   return i18n("endereço inválido");
    case PST_EXTRA_RUIM:  return i18n("parâmetros inválidos (fmt, format, config e c não valem)");
    case PST_CHAVE_RUIM:  return i18n("chave inválida");
    case PST_MODELO_RUIM: return i18n("modelo inválido: use http(s):// e {imdb}, {tmdb}, {type} ou {tipo_tmdb}");
    case PST_MODELO_LONGO: return i18n("modelo longo demais: o endereço pronto passa de 512 caracteres");
    default: break;
  }
  { int e = atomic_load_explicit(&pstTeste, memory_order_acquire);
    if (e == 0) return i18n("OK testa");
    if (e == 1 || e == 3) return i18n("testando…");
    switch (pstTesteRes) {
      case PST_T_OK:
        snprintf(buf, sizeof buf, i18n("funcionou · %ld KB em %ld ms"), pstTesteKB, pstTesteMs);
        return buf;
      case PST_T_CONFIG:       return i18n("configuração incompleta ou grande demais");
      case PST_T_NAO_IMAGEM:   return i18n("respondeu, mas não é uma imagem");
      default:                 return i18n("sem resposta do serviço");
    } }
}
// Teclado de cada campo. O TOKEN e o MODELO sao longos: usam o teclado LONGO.
static const char *PST_ALFA_INST   = "abcdefghijklmnopqrstuvwxyz0123456789.:-/_";
static const char *PST_ALFA_TOKEN  =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.~=-:/";
static const char *PST_ALFA_EXTRA  = "abcdefghijklmnopqrstuvwxyz0123456789=&_.,-%";
static const char *PST_ALFA_CHAVE  =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
// O modelo leva a chave do servico no caminho: caixa mista e os sinais que
// uma URL aceita. So minusculas baixava a caixa da chave (#390).
static const char *PST_ALFA_MODELO =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/.-_?=&{}%~+,;!*@$()";
static void pstAtivar(int op) {
  if (inativa(op)) return;
  switch (op) {
    case AJ_POSTER_INST:
      stCampo = op;
      teclado_abrir_com("Endereço do SpatialPosters", "Ex.: posters.meudominio.com ou 192.168.1.5:3000. Vazio usa a pública.",
                        PP_INSTANCIA_MAX - 1, PST_ALFA_INST, pstInst[0] ? pstInst : NULL);
      break;
    case AJ_POSTER_TOKEN:
      // O token NAO volta para o campo (a modal fica na tela e a tela vira foto).
      stCampo = op;
      teclado_abrir_com("Token do SpatialPosters", "Token ou endereço do manifest (…/c/TOKEN/manifest.json). Vazio apaga.",
                        PP_TOKEN_MAX, PST_ALFA_TOKEN, NULL);
      break;
    case AJ_POSTER_EXTRA:
      stCampo = op;
      teclado_abrir_com("Parâmetros do SpatialPosters", "Ex.: bs=vetro&side=right. Vazio apaga.",
                        PP_EXTRA_MAX, PST_ALFA_EXTRA, pstExtra[0] ? pstExtra : NULL);
      break;
    case AJ_POSTER_CHAVE:
      stCampo = op;
      teclado_abrir_com("Chave do RPDB", "Sua chave em ratingposterdb.com. Vazio apaga.",
                        PP_CHAVE_MAX - 1, PST_ALFA_CHAVE, NULL);
      break;
    case AJ_POSTER_MODELO:
      stCampo = op;
      teclado_abrir_com("Modelo de URL dos pôsteres", "Ex.: https://meu.servidor/{type}/{imdb}.jpg. Vazio apaga.",
                        PP_MODELO_MAX - 1, PST_ALFA_MODELO, pstModelo[0] ? pstModelo : NULL);
      break;
    case AJ_POSTER_TESTAR:
      pstTesteIniciar();
      break;
  }
}
#ifdef AJUSTES_TESTE
// #390: "Modelo de URL dos posteres" pelo caminho de verdade — a modal do
// campo, o texto que chega do celular, pstDefinir, posteres.txt e a leitura de
// volta. Devolve o aviso (PST_OK = 0) e o modelo relido em `lido`.
void teclado_teste_sistema(const char *t);
int ajustes_teste_poster_modelo(const char *colado, char *lido, size_t n) {
  valor[AJ_POSTER_PROV] = PP_MODELO;
  pstAtivar(AJ_POSTER_MODELO);
  if (!teclado_aberto()) return -1;
  teclado_teste_sistema(colado);
  pstDefinir(AJ_POSTER_MODELO, teclado_texto());
  teclado_esquecer();
  pstModelo[0] = 0;
  pstCarregar();
  snprintf(lido, n, "%s", pstModelo);
  return pstAviso;
}
// So a leitura de posteres.txt (o que outra versao gravou), sem digitar nada.
void ajustes_teste_poster_recarregar(char *lido, size_t n) {
  pstModelo[0] = 0;
  pstCarregar();
  snprintf(lido, n, "%s", pstModelo);
}
#endif


// IDIOMA AUTOMATICO — o resto (estado e regra: ver ajustes_idioma e idiomaauto.h).
//
// Aplica a regra sobre o que se sabe AGORA (conta, TV) e, se o idioma mudou,
// grava, remonta as fileiras e avisa. `notificar` e 0 no arranque (a TV ainda
// nem desenhou nada) e 1 depois; sem ajustes_idioma_auto_iniciar (os testes) a
// remontagem e o aviso ficam de fora, porque descoberta e avisos nao existem.
static void idiomaResolver(int notificar) {
  int fonte, novo, mudou;
  if (valor[AJ_IDIOMA] != 0) return;          // escolha manual: o automatico nao mexe
  novo = idiomaauto_resolver(contaTmdbLing, contaLegLing, sistemaLoc, &fonte);
  mudou = novo != idiomaEfetivo;
  // Sem conta nem locale ainda (a TV responde depois): cair no ingles agora
  // trocaria o idioma gravado por um que ja vai ser corrigido em instantes.
  if (fonte == IDA_PADRAO && sistemaPendente) return;
  if (!mudou && fonte == idiomaUltimaFonte) return;   // nada novo: sem linha repetida
  idiomaUltimaFonte = fonte;
  idiomaFonteGravada = fonte;
  printf("[idioma] automatico: %s (fonte: %s)\n", idiomaauto_codigo(novo),
         idiomaauto_fonte_nome(fonte));
  fflush(stdout);
  if (!mudou) { gravar(); return; }           // so a fonte mudou: fica gravada
  idiomaEfetivo = novo;
  gravar();
  // Remontar as fileiras e avisar e do main.c (o gancho): estes dois modulos
  // nao existem nos testes que incluem ajustes.c, e nem no arranque.
  if (idiomaPosArranque && idiomaGancho)
    idiomaGancho(idiomaauto_codigo(novo), fonte, notificar);
}

// A pessoa mexeu na linha de idioma (valor[AJ_IDIOMA] ja e o novo indice).
// Voltar a "Automático" resolve de novo agora; qualquer outro valor e escolha
// manual e o automatico nao toca mais no idioma.
static void idiomaEscolhido(void) {
  if (valor[AJ_IDIOMA] != 0) return;
  idiomaUltimaFonte = -1;
  // "O que estava na tela" era o idioma manual de que a pessoa acabou de sair;
  // o resolvido pode coincidir com o guardado de antes e ainda assim precisa
  // ser calculado de novo.
  idiomaEfetivo = -1;
  idiomaResolver(0);
  if (idiomaEfetivo < 0) idiomaEfetivo = IDIOMA_EN;   // a TV ainda nao respondeu (webOS)
}

// O LOCALE DA TV.
//   Tizen  navigator.language (segue a lingua da TV), lido na hora.
//   Mac    NUVIO_LOCALE, LC_ALL / LC_MESSAGES / LANG; so para a previa.
//   webOS  luna://com.webos.settingsservice/getSystemSettings localeInfo
//          (locales.UI, "pt-BR"). Vai por luna-send como extras.c faz para o
//          navegador, e num fio: o processo leva algumas centenas de ms na TV
//          e o arranque nao espera por ele. O laco principal recolhe o
//          resultado em ajustes_idioma_auto_tick.
// NUNCA chamado por ajustes_dir: os testes que incluem ajustes.c nao dependem
// do locale de quem os roda.
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
static void sistemaConsultar(void) {
  EM_ASM({
    try {
      var b = new TextEncoder().encode(navigator.language || '');
      var n = Math.min(b.length, $1 - 1);
      HEAPU8.set(b.subarray(0, n), $0);
      HEAPU8[$0 + n] = 0;
    } catch (e) { HEAPU8[$0] = 0; }
  }, sistemaLoc, (int)sizeof sistemaLoc);
}
static void sistemaRecolher(void) {}
#elif defined(NV_WEBOS) || defined(NV_TPK)
// WEBOS AND NATIVE TIZEN: the TV builds. The webOS compiler defines __linux__
// like any other Linux target and the toolchain has no webOS macro of its
// own, so the target identity is spelled by the build: -DNV_WEBOS in
// tools/arm.sh, the same way every other target carries its macro (NV_TPK
// from tools/tpk.sh, NV_ANDROID from android/app/src/main/cpp/CMakeLists.txt).
// The native Tizen tpk has always landed in this same branch (it used to be
// the #else fallthrough): luna-send does not exist on Tizen, the popen() comes
// back empty, and the automatic language stays on the saved/default choice -
// upstream behaviour, kept on purpose, because reading env vars on a TV could
// pick up a LANG and switch the language out of nowhere.
//
// The lookup itself: luna://com.webos.settingsservice/getSystemSettings
// (locales.UI, "pt-BR"), read on a thread - the process takes a few hundred
// ms on the TV and startup must not wait for it. The main loop collects the
// result in ajustes_idioma_auto_tick.
#include <pthread.h>
static char sistemaBruto[32];
static volatile int sistemaPronto;             // 1 = o fio deixou o resultado
static int sistemaIniciado;
static void *sistemaFio(void *u) {
  char buf[1024];
  size_t n = 0;
  FILE *p = popen("luna-send -n 1 -f luna://com.webos.settingsservice/getSystemSettings "
                  "'{\"keys\":[\"localeInfo\"]}' 2>/dev/null", "r");
  (void)u;
  if (p) { n = fread(buf, 1, sizeof buf - 1, p); pclose(p); }
  buf[n] = 0;
  sistemaBruto[0] = 0;
  js_texto(buf, buf + n, "UI", sistemaBruto, sizeof sistemaBruto);
  sistemaPronto = 1;
  return NULL;
}
static void sistemaConsultar(void) {
  pthread_t t;
  if (sistemaIniciado) return;
  sistemaIniciado = 1;
  sistemaPendente = 1;
  if (pthread_create(&t, NULL, sistemaFio, NULL) == 0) pthread_detach(t);
  else sistemaPronto = 1;
}
static void sistemaRecolher(void) {
  if (!sistemaPronto) return;
  sistemaPronto = 0;
  snprintf(sistemaLoc, sizeof sistemaLoc, "%s", sistemaBruto);
  sistemaPendente = 0;
  printf("[idioma] locale da TV: \"%s\"\n", sistemaLoc);
  fflush(stdout);
  idiomaResolver(1);
}
#else
// MAC, DESKTOP LINUX AND ANDROID, all without luna-send: read the environment.
//
// Linux is here because it is the test bench (galaxy), not a TV target: the
// TV builds carry their target macros (NV_WEBOS, NV_TPK) and this build has
// none, so it reaches this branch instead of the webOS lookup above, which
// popen()s luna-send - a binary that does not exist off webOS. Without that,
// every automatic-language test failed on the bench for a platform reason,
// not a code one.
//
// Android reads the same variables (the upstream addition); the body is
// identical, so the two share one branch. NUVIO_LOCALE=ro-RO simulates a TV in
// another language on the preview and in the tests.
static void sistemaConsultar(void) {
  // NUVIO_LOCALE=ro-RO simula a TV em outra lingua na previa (e nos testes).
  const char *v = getenv("NUVIO_LOCALE");
  if (!v || !*v) v = getenv("LC_ALL");
  if (!v || !*v) v = getenv("LC_MESSAGES");
  if (!v || !*v) v = getenv("LANG");
  snprintf(sistemaLoc, sizeof sistemaLoc, "%s", v ? v : "");
}
static void sistemaRecolher(void) {}
#endif

void ajustes_idioma_auto_iniciar(void (*aoMudar)(const char *codigo, int fonte, int notificar)) {
  idiomaGancho = aoMudar;
  if (valor[AJ_IDIOMA] == 0) {
    sistemaConsultar();
    if (sistemaLoc[0]) {
      printf("[idioma] locale da TV: \"%s\"\n", sistemaLoc);
      fflush(stdout);
    }
    if (idiomaFonteGravada == IDA_TMDB || idiomaFonteGravada == IDA_LEGENDA) {
      // Veio da conta na ultima vez: fica assim ate o blob chegar.
      idiomaUltimaFonte = idiomaFonteGravada;
      printf("[idioma] automatico: %s (fonte: %s)\n", idiomaauto_codigo(idiomaEfetivo),
             idiomaauto_fonte_nome(idiomaFonteGravada));
      fflush(stdout);
    } else idiomaResolver(0);
  }
  idiomaPosArranque = 1;
}
void ajustes_idioma_auto_tick(void) { sistemaRecolher(); }

// tmdb_language / subtitle_preferred_language CRUS do blob (a conta manda
// "pt-BR", "ro"...). O laco das opcoes guarda so o INDICE de V_TMDB_LING, que
// perde a regiao e o que a lista nao tem; aqui interessa o codigo inteiro.
// O texto cru de `chave` no blob, desembrulhado de {"type":...,"value":X} e
// sem aspas; "" para null. 0 se a chave nao existe (dst intocado).
static int textoCruDoBlob(const char *json, const char *fim, const char *chave,
                          char *dst, size_t tam) {
  char bruto[80], texto[80];
  size_t n;
  if (!js_bruto(json, fim, chave, bruto, sizeof bruto)) return 0;
  if (bruto[0] == '{' &&
      !js_bruto(bruto, bruto + strlen(bruto), "value", texto, sizeof texto))
    return 0;
  if (bruto[0] != '{') snprintf(texto, sizeof texto, "%s", bruto);
  n = strlen(texto);
  if (n >= 2 && texto[0] == '"') { memmove(texto, texto + 1, n - 2); texto[n - 2] = 0; }
  else if (!strcmp(texto, "null")) texto[0] = 0;
  snprintf(dst, tam, "%s", texto);
  return 1;
}

static void idiomaContaDoBlob(const char *json, const char *fim) {
  textoCruDoBlob(json, fim, "tmdb_language", contaTmdbLing, sizeof contaTmdbLing);
  textoCruDoBlob(json, fim, "subtitle_preferred_language", contaLegLing, sizeof contaLegLing);
}

// #187: o registro dizia so "tmdb language=ko-KR" no Guia, e nada dizia DE
// ONDE. Com os ajustes protegidos (ajustes-locais.txt) a conta nao reaplica e
// a escolha desta TV vale sozinha — esta linha poe as duas lado a lado a cada
// blob que chega, aplicado ou nao. So leitura e printf.
void ajustes_tmdb_idioma_relatar(const char *blob) {
  char conta[24] = "?";
  int v = valor[AJ_TMDB_IDIOMA];
  if (!blob) return;
  textoCruDoBlob(blob, blob + strlen(blob), "tmdb_language", conta, sizeof conta);
  printf("[tmdb] idioma dos metadados: %s (ajuste desta TV: %s); conta: tmdb_language=\"%s\"\n",
         ajustes_tmdb_idioma(),
         v <= 0 || v >= (int)(sizeof W_TMDB_LING / sizeof *W_TMDB_LING) - 1
           ? "da interface" : W_TMDB_LING[v],
         conta);
  fflush(stdout);
}

// Idiomas de audio e legenda do blob. NAO passam pelo laco das opcoes abaixo
// porque o valor deles nao e um indice de enum, e um codigo ISO ("en", "pt") —
// e porque as sentinelas do web ("DEVICE", "none", "off") precisam virar
// "sem filtro" em vez de virar um idioma inventado. Ver linguas.h.
//
// MEDIDO no app web (profileSettingsSyncService.js:1066): as quatro chaves
// vivem sob `player_settings`, ja em snake_case, como o resto do blob.
static void idiomasDoBlob(const char *json, const char *fim) {
  static const struct { const char *chave; void (*aplica)(const char *); } M[] = {
    { "subtitle_preferred_language",         ling_conta_legenda  },
    { "subtitle_secondary_language",         ling_conta_legenda2 },
    { "preferred_audio_language",            ling_conta_audio    },
  };
  size_t k;
  // #378 (revisao): o blob e o retrato INTEIRO da conta. Chave ausente = a
  // conta nao tem idioma; sem limpar, o do perfil anterior ficava valendo.
  // So a parte da conta: a escolha desta TV (ling_local_*) nao e tocada.
  for (k = 0; k < sizeof M / sizeof *M; k++) M[k].aplica("");
  for (k = 0; k < sizeof M / sizeof *M; k++) {
    char bruto[80], texto[80];
    size_t n;
    if (!js_bruto(json, fim, M[k].chave, bruto, sizeof bruto)) continue;
    if (bruto[0] == '{' &&
        !js_bruto(bruto, bruto + strlen(bruto), "value", texto, sizeof texto))
      continue;
    if (bruto[0] != '{') snprintf(texto, sizeof texto, "%s", bruto);
    n = strlen(texto);
    if (n >= 2 && texto[0] == '"') { memmove(texto, texto + 1, n - 2); texto[n - 2] = 0; }
    else if (!strcmp(texto, "null")) texto[0] = 0;
    M[k].aplica(texto);
  }
  // A CONTA e o que VALE separados (#378): antes so saia o em vigor, que com
  // escolha local nesta TV nao dizia nada sobre a conta.
  printf("[ajustes] idiomas da conta: legenda=\"%s\" legenda2=\"%s\" audio=\"%s\"; "
         "em vigor: legenda=\"%s\" audio=\"%s\"\n",
         ling_conta_legenda_valor(), ling_conta_legenda2_valor(), ling_conta_audio_valor(),
         ling_legenda(), ling_audio());
  fflush(stdout);
}

// #378: so os idiomas da conta, para o blob que chega com os ajustes locais
// protegidos (sync.c nao o aplica). Ver a nota em sync.c.
void ajustes_idiomas_da_conta(const char *json) {
  if (json && *json) { idiomasDoBlob(json, json + strlen(json)); return; }
  // NULL: troca de perfil/conta (sync_reaplicar_ajustes) — esquece os da conta.
  ling_conta_legenda(""); ling_conta_legenda2(""); ling_conta_audio("");
}

int ajustes_aplicar_blob(const char *json) {
  const char *fim;
  int i, mudou = 0, reconhecidas = 0;
  if (!json || !*json) return 0;
  // Os pacotes de selos da conta (features.stream_badge_settings) vivem em
  // selospacote.c, que guarda por perfil; nao sao uma opcao desta tabela.
  selospacote_conta_do_blob(json);
  // Add-ons/plugins permitidos e o padrao da regex do auto-play (#202), com as
  // chaves do oficial: texto e listas, que valor[] nao guarda.
  fonteregra_do_blob(json);
  fim = json + strlen(json);
  idiomasDoBlob(json, fim);
  idiomaContaDoBlob(json, fim);
  idiomaResolver(1);

  for (i = 0; i < AJ_N; i++) {
    char snake[80], embrulho[400], bruto[160];
    int novo;
    // Linha de leitura/acao nao tem valor; chave com "-" e marcador local
    // (heroCatalogKeys, versao, espaco) e nao vem do blob.
    if (OPCOES[i].tipo == OP_LEITURA || OPCOES[i].tipo == OP_ACAO) continue;
    if (!CHAVE[i] || CHAVE[i][0] == '-') continue;
    if (somenteDesteAparelho(i)) continue;
    // MEDIDO na TV, com uma conta de verdade: o blob NAO e um mapa plano de
    // camelCase. Ele e
    //   {"version":1,"features":{"layout_settings":{
    //      "hero_section_enabled":{"type":"boolean","value":true}, ...}}}
    // — chave em snake_case, aninhada por "feature", e o valor EMBRULHADO num
    // objeto com tipo. Procurando por `heroSectionEnabled` o app achava zero
    // chaves em 12 KB de ajustes e nao aplicava nada, sem erro nenhum.
    //
    // A busca por nome ignora o aninhamento de proposito: js_bruto varre o
    // texto inteiro, e os nomes destas chaves sao unicos no documento.
    camelParaSnake(CHAVE[i], snake, sizeof snake);
    if (!js_bruto(json, fim, snake, embrulho, sizeof embrulho) &&
        !js_bruto(json, fim, CHAVE[i], embrulho, sizeof embrulho)) continue;
    if (embrulho[0] == '{') {
      // Desembrulha {"type":...,"value":X}. O `type` vem antes do `value` no
      // codificador do web, entao a primeira chave "value" e a certa.
      if (!js_bruto(embrulho, embrulho + strlen(embrulho), "value",
                    bruto, sizeof bruto)) continue;
    } else {
      snprintf(bruto, sizeof bruto, "%s", embrulho);
    }

    if (!strcmp(bruto, "true") || !strcmp(bruto, "false")) {
      // O primeiro rotulo de V_LIGA e "Ligado" e o de V_RAIL e "Recolhida" —
      // nos dois, o indice 0 e o `true` do web. Coincidencia util, mas
      // coincidencia: se um vetor novo comecar pelo estado desligado, ele
      // precisa de literais proprios em literaisDe().
      novo = !strcmp(bruto, "true") ? 0 : 1;
    } else if (bruto[0] == '"') {
      const char *const *lit = literaisDe(i);
      char texto[128];
      size_t n = strlen(bruto);
      if (n < 2) continue;
      if (n - 2 >= sizeof texto) continue;
      memcpy(texto, bruto + 1, n - 2);
      texto[n - 2] = 0;
      novo = -1;
      // Comparacao SEM CAIXA. MEDIDO na TV: o servidor guarda estes enums em
      // MAIUSCULA ("IN_SEARCH", "CARD", "DEFAULT") enquanto o codigo JS do app
      // web os escreve em minuscula. Ler so o codigo do web levava a rejeitar
      // o valor de verdade — e a rejeicao era CORRETA (melhor manter que
      // inventar), mas o efeito era o ajuste nunca chegar.
      if (lit) { int k; for (k = 0; lit[k]; k++) if (!igualSemCaixa(lit[k], texto)) { novo = k; break; } }
      // IDIOMA COM REGIAO: contas antigas guardam "pt-br"/"pt-BR" (22 logs
      // no D1 em 01/10/2026) e a lista so tem a base. Sem casar exato,
      // tenta a base antes do '-' ("pt-br" -> "pt"); "pt-pt" e "zh-tw" ja
      // casaram exato acima.
      if (novo < 0 && lit && i == AJ_TMDB_IDIOMA && strchr(texto, '-')) {
        char base[16]; int k; size_t b = (size_t)(strchr(texto, '-') - texto);
        if (b > 0 && b < sizeof base) {
          memcpy(base, texto, b); base[b] = 0;
          for (k = 1; lit[k]; k++) if (!igualSemCaixa(lit[k], base)) { novo = k; break; }
        }
      }
      if (novo < 0) {
        // Valor que este app nao conhece (versao nova do web, opcao nova).
        // Manter o que esta e a resposta certa: escolher um padrao aqui
        // inventaria uma preferencia que a pessoa nunca marcou.
        printf("[ajustes] %s=\"%s\" nao reconhecido; mantido\n", CHAVE[i], texto);
        continue;
      }
    } else if ((bruto[0] >= '0' && bruto[0] <= '9') || bruto[0] == '-' || bruto[0] == '.') {
      novo = (int)(atof(bruto) + 0.5);
    } else {
      continue;   // null, objeto, array: nao ha o que aplicar
    }

    reconhecidas++;
    // TEMA DINAMICO E DESTA TV, e a conta nao o desfaz. O blob so conhece os
    // doze temas do web (W_TEMA), entao o que vier dele e sempre um desses — e
    // aplica-lo sobre "Dinâmica" trocaria, a cada sincronizacao, a escolha que
    // a pessoa fez aqui pela que ela fez no celular. Com um tema FIXO aqui, a
    // conta continua mandando como sempre mandou.
    if (i == AJ_TEMA && temaLocal()) {
      printf("[ajustes] selected_theme da conta mantido na conta: tema desta TV e local\n");
      continue;
    }
    novo = limita(i, novo);
    if (novo != valor[i]) { valor[i] = novo; mudou++; }
  }

  if (mudou) gravar();   // o que veio da conta tem de sobreviver ao arranque
  // Registra SEMPRE, inclusive zero. "Nenhuma linha no log" tem duas leituras
  // opostas — o blob nao foi aplicado, ou foi aplicado e ja estava tudo igual —
  // e sem o numero nao da para saber qual. Foi exatamente a duvida que sobrou
  // na primeira verificacao na TV.
  printf("[ajustes] blob da conta: %d chave(s) reconhecida(s), %d mudou(aram)\n",
         reconhecidas, mudou);
  return mudou;
}

// ---------------------------------------------------------------- subir (#85)
//
// O CAMINHO DE VOLTA do blob: o que a pessoa mudou NESTA TV entra no objeto
// `settings_json` da conta e sync.c o empurra. Antes disto a TV so LIA o blob,
// e a unica defesa contra o proximo arranque desfazer a mudanca local era
// parar de aplicar a conta (ajustes-locais.txt) — isto e, divergir em silencio.
//
// A COSTURA E TEXTUAL, e nao um objeto remontado campo a campo. Duas razoes, as
// duas medidas neste projeto:
//   - o blob real tem MUITO mais chaves do que este app conhece (foi o que
//     estourou o vetor de 4096 bytes da primeira versao do leitor). Remontar o
//     objeto aqui mandaria de volta um blob com so as ~40 chaves daqui, e o
//     servidor guarda o que vier: a TV APAGARIA as preferencias que so o app
//     web tem. E o mesmo defeito da "lista vazia apaga tudo" da secao 1.6.
//   - a chave e aninhada por feature ("layout_settings", "player_settings",
//     "theme_settings"...) e este app nao sabe em qual feature cada chave vive.
//     Reescrevendo no lugar, nao precisa saber.
//
// Por isso a regra e estrita: SO CHAVE QUE JA EXISTE NO BLOB e reescrita, e
// somente o valor dela. Chave que a conta nao tem nao e inventada — inventa-la
// exigiria adivinhar a feature e o `type`, e um blob com forma errada e pior
// que uma chave a menos.
static int somenteDesteAparelho(int op) {
  if (op == AJ_ICONE_APP || op == AJ_LOGO_APP || op == AJ_ABERTURA) return 1;
  switch (op) {
    // LAYOUT/APARELHO: nao sobem nem que o blob tenha a chave.
    // A regra que separa: se o valor descreve ESTA TV (RAM, painel, rede,
    // consentimento de registro) ou uma escolha que so este port tem, ele fica.
    // A TV da sala e a do quarto nao tem a mesma memoria nem a mesma tela, e a
    // conta e uma so.
    case AJ_RESOLUCAO:      /* pedir superficie 4K: depende do painel */
    case AJ_TEX_MB:         /* teto de memoria de imagem: depende da RAM */
    case AJ_QUALIDADE_IMG:  /* resolucao da arte decodificada: idem */
    case AJ_ENVIO_AUTO:     /* consentimento de envio de registro deste aparelho */
    case AJ_ANIM:           /* animacoes reduzidas: acessibilidade nesta TV */
    case AJ_IDIOMA:         /* idioma da interface deste aparelho */
    case AJ_FIL_LIMITE:     /* fileiras da home: por aparelho (fileirasui-p<N>.txt) */
    case AJ_FIL_ORDEM:
    case AJ_CW_FONTE:
    case AJ_CW_PROXIMO:
    case AJ_BORDA_FOCO:
    case AJ_FONTE_UI:
    case AJ_FONTE_MANUAL:
    case AJ_FONTE_AUTO:
    case AJ_FONTE_PRIORIDADE: case AJ_FONTE_HDR:   /* o que esta TV mostra e o que a rede dela aguenta */
    case AJ_FONTE_REPOR:
    case AJ_FONTE_TOCAR_CONFERINDO:
    case AJ_FONTE_AQUECER:
    case AJ_FONTE_CONFERIR_VARIAS:
    case AJ_FONTE_PREPARAR:
    case AJ_FONTE_TEXTO: case AJ_FONTE_ORDEM_ADDON:
    case AJ_SALVOS_DEST:
    case AJ_EPG_PAIS:       /* pais da grade: por aparelho, o web nao tem */
    // Arte do destaque: o web nao tem as chaves (heroFundoLocal,
    // heroDifferentFromCard); ficam neste aparelho mesmo que um blob futuro
    // traga algo com o mesmo nome.
    case AJ_HERO_FUNDO:
    case AJ_HERO_ARTE_DIF:
    // Fonte do trailer: o que toca depende da TV (YouTube so na Samsung; o
    // IMDb da Samsung depende do servico de recomendacoes), entao a escolha e
    // deste aparelho.
    case AJ_TRAILER_FONTE:
    case AJ_HERO_TRAILER_SOM: case AJ_HERO_TRAILER_ESPERA: /* o web nao tem */
    case AJ_COR_LOGO:       /* so existe com os temas dinamicos, que sao locais */
    case AJ_VIDRO:          /* visual desta TV: a GPU de cada uma aguenta diferente */
    case AJ_VIDRO_CONTORNO:
    case AJ_CW_CONCLUIDO:   /* o web nao tem esta escolha */
    case AJ_ADDONS_PRINCIPAL: /* escolha desta TV; a conta tem uses_primary_addons */
    case AJ_HERO_TRANSICAO: /* o web nao tem esta escolha */
    case AJ_ADDON_POSTER: case AJ_ADDON_FUNDO: case AJ_ADDON_LOGO:
    case AJ_DET_TRAILER_SOM: /* o web nao tem */
    case AJ_COL_ARTE_CONTA: /* arte do addon: o web nao tem estas escolhas */
    case AJ_SELOS_CORES:    /* no web a cor vem do pacote de selos importado */
    case AJ_SELOS_PACOTE: case AJ_SELOS_PACOTE_ADD: case AJ_SELOS_PACOTE_REM: /* a escolha e por perfil, em selospacote.c */
    case AJ_LIVETV_RES: case AJ_LIVETV_FORMATO: case AJ_LIVETV_ESPERA:
    case AJ_LIVETV_DIAG: case AJ_LIVETV_MODO: case AJ_LIVETV_PROXY: /* rede e provedor desta casa: o web nao tem */
    case AJ_HOME_LAYOUT:    /* a Dinamica nao tem par na conta (selected_layout) */
    case AJ_PERFIL_PESQ:    /* estado em recomenda.c, por conta: nunca no blob */
    case AJ_PERFIL_EDITAR:
    case AJ_P2P_LIGADO:     /* o servidor P2P e um aparelho da rede desta casa */
    case AJ_P2P_LIMITE:     /* #334: o disco e desta TV */
    case AJ_JF_LIGADO:
    case AJ_AVANCADAS:      /* so a vista desta TV */
    case AJ_POSTER_PROV:    /* servico e rede desta casa: nao segue a conta */
    case AJ_DET_SO_CINEMETA: /* o web nao tem esta escolha */
    case AJ_ITENS_FILEIRA:  /* memoria desta TV: 1 GB aguenta menos */
    case AJ_GPU_EFEITOS:    /* a GPU e desta TV */
    case AJ_BUSCA_CINEMETA: /* o web nao tem esta escolha */
    case AJ_BUSCA_NUVIO: case AJ_BUSCA_ORIGEM: /* idem (#311) */
    case AJ_ESMAECER: case AJ_BRILHO_PLAYER: /* o painel OLED e desta TV */
    case AJ_PROPORCAO_PADRAO: /* o aspecto e a tela desta TV */
    case AJ_MANTER_VIDEO:   /* a memoria e o decoder sao desta TV */
    case AJ_DESCANSO_ESTILO: case AJ_DESCANSO_FONTE: /* tela de descanso: desta TV */
    case AJ_ENQUETES:       /* o web nao tem a ilha; a conta guarda o opt-out por outro caminho (enquete.c) */
    case AJ_RELOGIO: case AJ_RELOGIO_POS: case AJ_SAIDA_PLAYER: /* o web nao tem a ilha */
    case AJ_RELOGIO_12H:    /* formato da hora: desta TV */
    case AJ_PLR_CLASSIF:    /* o web nao tem esta escolha */
    case AJ_CW_RETIDO_TAMBEM: /* o web nao tem a ilha */
    case AJ_PS_CONTINUAR:   /* o web nao tem este cartao; escolha desta TV */
    case AJ_DTS_AC3:        /* o conversor de DTS e desta TV */
    case AJ_DV_MKV:         /* o demux e o decoder sao desta TV */
    case AJ_HIST_CONTA: case AJ_SOCIAL: /* por perfil desta TV (perfilLocal); o web nao tem */
    case AJ_TAM_MAX: case AJ_TAM_MIN: /* o web nao tem a faixa de tamanho */
    case AJ_ESPACO_FILEIRAS: case AJ_ESPACO_TITULOS: /* o web nao tem espacamento */
    case AJ_FONTE_PRAZO:    /* o web nao tem: a rede e os addons sao desta casa */
    case AJ_MEDIDOR:        /* o medidor e da GPU desta TV; o web nao tem */
    case AJ_TAMANHO_UI:     /* o tamanho e desta tela, e o web nao tem */
    case AJ_TAMANHO_AJUSTES:
    case AJ_LAYOUT_AJUSTES: /* #339: o arranjo dos Ajustes e desta TV */
    case AJ_LOGO_TRAILER:   /* so a protecao de OLED desta TV */
    case AJ_LEG_SYNC_AUTO: /* escolha local desta TV */
    case AJ_LEG_SYNC_AUDIO: /* PCM e passthrough sao desta TV; o web nao tem */
    case AJ_LEG_FORCADA:    /* #287: o web nao tem esta escolha */
    case AJ_LEG2_POS: case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA: /* estilo da legenda e desta TV */
    case AJ_CACHE_SEEK:     /* F07: o disco e o player sao desta TV */
    case AJ_TRAILER_ZOOM_TPK: /* #241: o firmware de cada Samsung reage de um jeito */
    case AJ_FUNDO:          /* o desfoque custa GPU desta TV; o web nao tem */
    case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO: /* teste do vidro: visual desta TV */
    case AJ_SELO_VISTO:     /* o web nao tem a escolha */
    case AJ_REACAO_CREDITOS: /* o web nao tem a pergunta */
    case AJ_SEEKR_LIGADO: case AJ_SEEKR_FITA: case AJ_SEEKR_AJUSTE: /* o web nao tem o Seekr */
    case AJ_MENU_EXPLORAR: case AJ_MENU_GUIA: case AJ_MENU_AGENDA: case AJ_MENU_PERFIL:
    // Linha do titulo: o web nao tem, e nenhuma conta pode desliga-las aqui.
    case AJ_NT_IMDB: case AJ_NT_TOMATES: case AJ_NT_AUDIENCIA: case AJ_NT_META:
    case AJ_NT_METAUSER: case AJ_NT_TRAKT: case AJ_NT_TMDB: case AJ_NT_LETTER:
    case AJ_NT_MAL: case AJ_NT_EBERT: case AJ_NT_SCORE:
      return 1;
    default:
      return 0;
  }
}

// AJUSTES POR PERFIL NESTA TV. ajustes.txt e um so por aparelho; a conta guarda
// um blob por perfil, mas um perfil que nunca salvou ajustes na conta nao tem
// blob — e ai a troca de perfil nao trazia nada, ficava o que o perfil anterior
// deixou, e o que a pessoa mudava na TV para esse perfil era sobrescrito pelo
// blob do outro na troca seguinte e nunca mais voltava.
//
// A copia por perfil guarda SO o que e do perfil: o que a conta tambem guarda
// (o mesmo conjunto que ajustes_mesclar_blob considera). O que descreve esta
// TV (somenteDesteAparelho, idioma, fonte da interface) nao muda com o perfil.
// O que e do PERFIL mas a conta nao guarda (o web nao tem estas escolhas):
// onde o historico vai e se o social existe. Entram na copia por perfil
// (ajustes-p<N>.txt) e continuam FORA do blob (somenteDesteAparelho).
static int perfilLocal(int i) {
  return i == AJ_CW_FONTE || i == AJ_CW_PROXIMO || i == AJ_SALVOS_DEST || i == AJ_HIST_CONTA || i == AJ_SOCIAL;
}

static int dePerfil(int i) {
  if (OPCOES[i].tipo == OP_LEITURA || OPCOES[i].tipo == OP_ACAO) return 0;
  if (!CHAVE[i] || CHAVE[i][0] == '-') return 0;
  if (i == AJ_FONTE_UI || i == AJ_IDIOMA) return 0;
  if (perfilLocal(i)) return 1;
  return !somenteDesteAparelho(i);
}

static void nomePerfil(char *dst, size_t tam, int perfil) {
  snprintf(dst, tam, "ajustes-p%d.txt", perfil);
}

void ajustes_perfil_guardar(int perfil) {
  char nome[32], buf[AJ_N * 56];
  size_t p = 0;
  int i;
  if (perfil <= 0) return;
  buf[0] = 0;
  for (i = 0; i < AJ_N; i++) {
    int k;
    if (!dePerfil(i)) continue;
    k = snprintf(buf + p, sizeof buf - p, "%s %d\n", CHAVE[i], valor[i]);
    if (k < 0 || (size_t)k >= sizeof buf - p) return;   // nunca um arquivo pela metade
    p += (size_t)k;
  }
  nomePerfil(nome, sizeof nome, perfil);
  dados_gravar(nome, buf);
  fonteregra_perfil_guardar(perfil);
}

int ajustes_perfil_restaurar(int perfil) {
  char nome[32], *t, *l;
  int mudou = 0;
  if (perfil <= 0) return 0;
  // As listas e a regex do auto-play (#202) seguem o perfil do mesmo jeito.
  fonteregra_perfil_restaurar(perfil);
  nomePerfil(nome, sizeof nome, perfil);
  t = dados_ler(nome);
  if (!t) return 0;
  for (l = t; l && *l; ) {
    char chave[64], *fim = strchr(l, '\n');
    int v, i;
    if (fim) *fim = 0;
    if (sscanf(l, "%63s %d", chave, &v) == 2)
      for (i = 0; i < AJ_N; i++) {
        if (!dePerfil(i) || strcmp(CHAVE[i], chave)) continue;
        v = limita(i, v);
        if (v != valor[i]) { valor[i] = v; mudou++; }
        break;
      }
    l = fim ? fim + 1 : NULL;
  }
  free(t);
  if (mudou) {
    gravar();
    aplicarIdioma(AJ_LEG_LINGUA);
    aplicarIdioma(AJ_LEG_LINGUA2);
    aplicarIdioma(AJ_AUD_LINGUA);
  }
  printf("[ajustes] ajustes do perfil %d restaurados desta TV (%d mudaram)\n",
         perfil, mudou);
  fflush(stdout);
  return 1;
}

void ajustes_perfil_esquecer(void) {
  char nome[32];
  int i;
  // Os indices de perfil da conta sao pequenos (CONTA_PERFIL_MAX perfis); 32
  // cobre com folga. Apagar arquivo que nao existe nao custa nada.
  for (i = 1; i <= 32; i++) { nomePerfil(nome, sizeof nome, i); dados_apagar(nome); }
  fonteregra_perfil_esquecer();
}

// Onde esta o valor de `chave` dentro de [ini,fim): *vi aponta o primeiro
// caractere do valor e *vf o seguinte ao ultimo. 1 quando achou.
//
// Procura a chave ENTRE ASPAS e exige os dois-pontos: sem isso, "value" casaria
// com o pedaco de "values" e com qualquer texto que contivesse a palavra.
static int acharValor(const char *ini, const char *fim, const char *chave,
                      const char **vi, const char **vf) {
  char alvo[96];
  const char *p;
  size_t n;
  n = (size_t)snprintf(alvo, sizeof alvo, "\"%s\"", chave);
  if (n >= sizeof alvo) return 0;
  for (p = ini; p && (p = strstr(p, alvo)) != NULL && p < fim; p += n) {
    const char *v = p + n;
    while (v < fim && (unsigned char)*v <= ' ') v++;
    if (v >= fim || *v != ':') continue;
    v++;
    while (v < fim && (unsigned char)*v <= ' ') v++;
    if (v >= fim) return 0;
    if (*v == '{' || *v == '[') { *vf = js_fim(v); }
    else if (*v == '"') {
      const char *q = v + 1;
      while (q < fim && *q != '"') q += (*q == '\\' && q + 1 < fim) ? 2 : 1;
      *vf = (q < fim) ? q + 1 : fim;
    } else {
      const char *q = v;
      while (q < fim && *q != ',' && *q != '}' && *q != ']' &&
             (unsigned char)*q > ' ') q++;
      *vf = q;
    }
    if (!*vf || *vf > fim) return 0;
    *vi = v;
    return 1;
  }
  return 0;
}

// O valor local de `op` no MESMO formato do que ja esta no blob. 0 quando nao
// da para escrever com fidelidade — e ai a chave nao e tocada, que e sempre a
// resposta certa: mandar um tipo diferente do que o servidor guarda faria o app
// web ler a preferencia errada, ou nenhuma.
static int textoDoValor(int op, const char *vi, const char *vf,
                        char *dst, size_t tam) {
  size_t n = (size_t)(vf - vi);
  if (n >= 4 && !strncmp(vi, "true", 4))  { snprintf(dst, tam, "%s", valor[op] == 0 ? "true" : "false"); return 1; }
  if (n >= 5 && !strncmp(vi, "false", 5)) { snprintf(dst, tam, "%s", valor[op] == 0 ? "true" : "false"); return 1; }
  if (*vi == '"') {
    const char *const *lit = literaisDe(op);
    int k, maiuscula = 0, temBaixa = 0;
    size_t i;
    if (!lit) return 0;
    // "DA INTERFACE" NAO SOBE (#187). O indice 0 de W_TMDB_LING e um
    // sentinela desta TV ("interface"), nao um idioma: o web nao o conhece. A
    // 1.5.3 nao sabia ler "ru" e ficava no 0 — e a costura seguinte escrevia
    // "interface" por cima do "ru" da conta (medido no registro da issue: "ru
    // nao reconhecido; mantido" e, no mesmo arranque, "1 ajuste(s) desta TV
    // entram no blob"). A conta perdia o idioma que a pessoa escolheu e nunca
    // mais o devolvia. Sem forma fiel, a chave nao e tocada.
    if (op == AJ_TMDB_IDIOMA && valor[op] == 0) return 0;
    for (k = 0; k <= valor[op]; k++) if (!lit[k]) return 0;   // fora da lista
    // A CAIXA DO SERVIDOR, e nao a do codigo JS. MEDIDO na TV: a conta guarda
    // estes enums em MAIUSCULA ("IN_SEARCH", "CARD") enquanto o web os escreve
    // em minuscula, e a leitura daqui e sem caixa justamente por isso. Na
    // escrita nao ha essa folga — devolver a caixa que o servidor ja usava e o
    // unico jeito de nao trocar o valor de forma para todo mundo.
    for (i = 1; i + 1 < n; i++) {
      if (vi[i] >= 'a' && vi[i] <= 'z') temBaixa = 1;
      if (vi[i] >= 'A' && vi[i] <= 'Z') maiuscula = 1;
    }
    maiuscula = maiuscula && !temBaixa;
    snprintf(dst, tam, "\"%s\"", lit[valor[op]]);
    if (maiuscula) for (i = 0; dst[i]; i++)
      if (dst[i] >= 'a' && dst[i] <= 'z') dst[i] = (char)(dst[i] - 'a' + 'A');
    return 1;
  }
  if ((*vi >= '0' && *vi <= '9') || *vi == '-' || *vi == '.') {
    snprintf(dst, tam, "%d", valor[op]);
    return 1;
  }
  return 0;   // null, objeto, array: nao ha o que escrever
}

typedef struct { const char *vi, *vf; char texto[160]; } Troca;

int ajustes_mesclar_blob(const char *base, char **saida) {
  Troca troca[AJ_N];
  int n = 0, i, j;
  const char *fim, *p;
  char *out;
  size_t cap, w = 0;

  if (saida) *saida = NULL;
  if (!base || !*base || !saida) return 0;
  fim = base + strlen(base);

  for (i = 0; i < AJ_N; i++) {
    char snake[80];
    const char *vi, *vf, *ei, *ef;
    if (OPCOES[i].tipo == OP_LEITURA || OPCOES[i].tipo == OP_ACAO) continue;
    if (!CHAVE[i] || CHAVE[i][0] == '-') continue;
    if (somenteDesteAparelho(i)) continue;
    // TEMA DINAMICO NAO SOBE. O web nao tem esse tema: gravar "DYNAMIC" na
    // conta deixaria o web sem tema que ele saiba desenhar, e gravar o padrao
    // ("WHITE") no lugar APAGARIA o tema que a pessoa escolheu la — o JADE do
    // celular viraria branco porque ela ligou o dinamico na TV. O valor da
    // conta fica exatamente como esta (a costura nao toca a chave), que e o
    // "padrao" certo: o ultimo tema fixo que a conta conhece.
    if (i == AJ_TEMA && temaLocal()) continue;
    camelParaSnake(CHAVE[i], snake, sizeof snake);
    if (!acharValor(base, fim, snake, &vi, &vf) &&
        !acharValor(base, fim, CHAVE[i], &vi, &vf)) continue;
    // Valor embrulhado em {"type":...,"value":X}: o que se reescreve e o X.
    if (*vi == '{' && acharValor(vi, vf, "value", &ei, &ef)) { vi = ei; vf = ef; }
    if (!textoDoValor(i, vi, vf, troca[n].texto, sizeof troca[n].texto)) {
      printf("[ajustes] %s nao tem forma para subir; mantido como esta na conta\n",
             CHAVE[i]);
      continue;
    }
    // Ja igual: nao entra na costura. Serve tambem de freio — um ciclo em que
    // nada mudou de verdade nao gera push.
    if ((size_t)(vf - vi) == strlen(troca[n].texto) &&
        !strncmp(vi, troca[n].texto, strlen(troca[n].texto))) continue;
    troca[n].vi = vi; troca[n].vf = vf;
    n++;
  }
  if (!n) return fonteregra_mesclar(base, saida);

  // Por posicao, para a costura ser uma passada so. n e pequeno (dezenas) e a
  // insercao simples e mais curta de conferir que a alternativa.
  for (i = 1; i < n; i++) {
    for (j = i; j > 0 && troca[j - 1].vi > troca[j].vi; j--) {
      Troca t = troca[j - 1];
      troca[j - 1] = troca[j]; troca[j] = t;
    }
  }

  cap = strlen(base) + 1;
  for (i = 0; i < n; i++) cap += strlen(troca[i].texto);
  out = (char *)malloc(cap);
  if (!out) return 0;
  p = base;
  for (i = 0; i < n; i++) {
    size_t pre = (size_t)(troca[i].vi - p), t = strlen(troca[i].texto);
    memcpy(out + w, p, pre); w += pre;
    memcpy(out + w, troca[i].texto, t); w += t;
    p = troca[i].vf;
  }
  memcpy(out + w, p, (size_t)(fim - p)); w += (size_t)(fim - p);
  out[w] = 0;
  *saida = out;
  // As chaves de texto/lista do auto-play (#202) por cima do que ja foi
  // costurado: a mesma regra, so chave que existe e no mesmo tipo.
  { char *mais = NULL;
    int m = fonteregra_mesclar(out, &mais);
    if (m > 0 && mais) { free(out); *saida = mais; n += m; } }
  printf("[ajustes] %d ajuste(s) desta TV entram no blob da conta\n", n);
  return n;
}

// TODA OPCAO APARECE NA TELA, E UMA VEZ SO. Nao da para exigir isso do
// compilador (TELA e uma lista de itens, nao um vetor indexado pelo enum),
// entao a conferencia e no arranque e GRITA no log; tests/ajustes_secoes.sh
// faz a mesma conta lendo o codigo, na suite.
//
// O que o erro custaria sem ela: uma opcao fora de TELA nao e desenhada nem
// alcancada — o ajuste some da TV sem aviso, que foi o defeito de
// "Arredondamento do cartaz" quando as faixas de SECOES descasaram do enum.
static void focarSecao(int s);
static void focarOpcao(int op);
// Opcoes que ajustes_ux_tela.inc DEIXA DE FORA desta plataforma de proposito.
// Mantida em par com os #if de la: a lista antiga cobria so tres opcoes e a
// conferencia gritava "aparece 0 vez(es)" para o resto em quase toda TV (2.0.1,
// D1: "Posicao do relogio" 238 pessoas, "Sincronia por audio" 157, "Zoom do
// trailer" 86, P2P no .wgt 6), escondendo o aviso que importa.
static int foraDestaPlataforma(int op) {
  if (op == AJ_RELOGIO_POS) return 1;   // saiu da tela em 7d69643a: a ilha e sempre no canto direito
  // 2.0.3: o Descobrir do web nao existe na TV; "Avancadas" e a pilula do alto
  // do indice; os oito AJ_MDB_* seguem as "Notas no titulo" (notaLigarPar).
  if (op == AJ_DESCOBRIR || op == AJ_AVANCADAS) return 1;
  if (op >= AJ_MDB_TRAKT && op <= AJ_MDB_MAL) return 1;
#ifndef NV_TPK
  if (op == AJ_TRAILER_ZOOM_TPK) return 1;
#endif
#if defined(NV_TPK) || defined(NV_ANDROID)
  if (op == AJ_DV_MKV) return 1;
#else
  if (op == AJ_GPU_EFEITOS) return 1;
#endif
#ifndef NV_ANDROID
  if (op == AJ_LEG_SYNC_AUDIO) return 1;
#endif
#ifdef __EMSCRIPTEN__
  if (op == AJ_P2P_LIGADO || op == AJ_P2P_URL || op == AJ_P2P_TESTAR || op == AJ_P2P_LIMITE) return 1;
#endif
  return 0;
}
static void conferirTela(void) {
  int i, vezes[AJ_N] = { 0 };
  for (i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op >= 0 && TELA[i].op < AJ_N)
      vezes[TELA[i].op]++;
  for (i = 0; i < AJ_N; i++)
    if (vezes[i] != 1 && !(vezes[i] == 0 && foraDestaPlataforma(i)))
      printf("[ajustes] opcao %d (\"%s\") aparece %d vez(es) em TELA\n",
             i, OPCOES[i].rotulo, vezes[i]);
  if (TELA[0].tipo != IT_SEC)
    printf("[ajustes] TELA nao comeca por uma categoria\n");
  if (nSecoes != (int)(sizeof SECAO_AJUDA / sizeof *SECAO_AJUDA))
    printf("[ajustes] %d categorias e %d frases de ajuda\n", nSecoes,
           (int)(sizeof SECAO_AJUDA / sizeof *SECAO_AJUDA));
  fflush(stdout);
}

// TODO PADRAO CABE NA LISTA DELE. `valor[]` e posicional e escrito a mao, e
// uma opcao inserida no meio do enum sem o inicializador correspondente desloca
// todos os padroes seguintes — foi assim que o 7 do limite de fileiras foi
// parar numa lista de dois itens e a tela quebrou ao desenhar o valor.
//
// Conferir custa 91 comparacoes uma vez por arranque, e o que ela encontra e
// CORRIGIDO na hora: um padrao fora da lista vira o primeiro item. A linha no
// log diz qual opcao, para o conserto de verdade (o inicializador que falta)
// acontecer no lugar certo.
static int nValores(int op);
// O LIMITE E O DE nValores, e nao o `n` da tabela (#129). As duas linhas de
// idioma tem n=2 na tabela const e a lista real (nLingua) — conferir contra 2
// zerava todo idioma escolhido alem de "Da conta" a cada abertura desta tela,
// e a gravacao seguinte levava o zero ao disco: "o ingles nao fica salvo".
// Visto nos logs de campo da 1.4.1 a 1.4.3: "padrao fora da lista em 4
// (\"Idioma do áudio\"): 30 de 2 valores".
static void conferirPadroes(void) {
  int i;
  for (i = 0; i < AJ_N; i++) {
    const Opcao *o = &OPCOES[i];
    int n = nValores(i);
    if (o->tipo == OP_ESCOLHA && n > 0 && (valor[i] < 0 || valor[i] >= n)) {
      printf("[ajustes] padrao fora da lista em %d (\"%s\"): %d de %d valores"
             " — vetor `valor[]` desalinhado; usando o primeiro\n",
             i, o->rotulo, valor[i], n);
      valor[i] = 0;
    }
  }
  fflush(stdout);
}

static void ajMovReiniciar(void);
int ajustes_iniciar(void) {
  ajMovReiniciar();
  uxVeioBusca = uxAbrirOp >= 0;
  montarTela();
  conferirTela();
  // ANTES de conferir: e rotulosDeIdioma quem preenche nLingua.
  rotulosDeIdioma();
  conferirPadroes();
  txt_definir_fonte_interface((TxtFamilia)valor[AJ_FONTE_UI]);
  // #149: aqui havia `valor[AJ_ENVIO_AUTO] = 1` (desligado), o padrao do envio
  // automatico escrito fora do vetor porque o vetor estava sete casas curto. Mesmo
  // defeito do bloco abaixo: esta funcao roda a cada abertura da tela, depois
  // do arquivo lido, e o "arquivo, lido depois, sobrescreve" do comentario nao
  // acontecia — ligar o envio durava ate a proxima visita aos Ajustes. O
  // padrao mora no vetor agora, conferido pelo _Static_assert dele.
  //
  // BUG (#82/#86, 1.3.9 e 1.3.10): havia aqui um bloco Samsung escrevendo
  // AJ_DET_TRAILER_AUTO = AJ_HERO_TRAILER = 1 (desligado) como "padrao de
  // fabrica". So que ajustes_iniciar() roda TODA VEZ que a tela de Ajustes
  // abre (app.c, TELA_AJUSTES), depois de ajustes_dir() ja ter lido o
  // arquivo — e nada o relê. Quem ligava o trailer, saia e voltava
  // encontrava os dois desligados de novo, e a proxima gravacao levava o
  // desligado ao disco: e o "the settings aren't retained" do rawldon. O
  // padrao de fabrica e o valor[] posicional (1 = desligado nas duas
  // linhas), e quem veio da 1.3.9 com o autoplay nascido ligado recebe o
  // reset unico em ajustes_dir() (marca trailer-1310.txt).
  scrollY = 0.0f; velY = 0.0f; sair = 0; sairArmado = 0;
  // Reabre na categoria em que estava, com o foco no indice (ver focoIndice).
  if (secAtual < 0 || secAtual >= nSecoes) secAtual = 0;
  focoIndice = 1; uxChipAv = 0;
  focarSecao(secAtual);
  uxTopo = -1; uxCabLayout = 0;
  // "Experimentar a cor viva" (cartao de novidades): abre em Aparencia com o
  // foco JA na linha da cor, e nao no indice — quem apertou o botao quer
  // trocar a cor, nao achar onde ela mora.
  if (abrirNaCor) { abrirNaCor = 0; focarOpcao(AJ_TEMA); }
  if (abrirNaFonte) { abrirNaFonte = 0; focarOpcao(AJ_FONTE_UI); }
  // Os dois atalhos do cartao da 1.6.0: o layout da home (dentro do grupo
  // "Layout da Home", que focarOpcao abre) e a Interface de vidro.
  if (abrirNoLayout) { abrirNoLayout = 0; focarOpcao(AJ_HOME_LAYOUT); }
  if (abrirNoVidro) { abrirNoVidro = 0; focarOpcao(AJ_VIDRO); }
  if (abrirNoTrakt) { abrirNoTrakt = 0; focarOpcao(AJ_TRAKT); }
  // #202: o OK da explicacao de espera na ilha do player cai na linha que manda
  // nela, dentro de Fontes e addons > Escolha da fonte.
  if (abrirNaEspera) { abrirNaEspera = 0; focarOpcao(AJ_FONTE_PRAZO); }
  // "Abrir o guia" do cartao da 1.8.0: a linha do guia em Sobre e ajuda e o
  // guia aberto por cima dela (Voltar devolve ao cartao).
  guiaFechar();
  if (abrirNoGuia) { abrirNoGuia = 0; focarOpcao(AJ_GUIA); guiaAbrir(guiaDaNovidades); guiaDaNovidades = 0; }
  if (uxAbrirOp >= 0) { int op = uxAbrirOp; uxAbrirOp = -1; focarOpcao(op); }
  if (uxVoltarOp >= 0) { int op = uxVoltarOp; uxVoltarOp = -1; focarOpcao(op); }
  uxCancelar(); uxAviso[0] = 0; uxRetornarOp = -1;
  filAberta = 0; filFoco = 0; filCampo = 0; filPegou = 0; filTopo = 0;
  emEdicao = 0;
  fil_confirmar_limite();   // rajada de uma visita anterior que nao fechou
  valor[AJ_FIL_LIMITE] = fil_limite_gravado();
  // Tambem aqui, e nao so em ajustes_dir: sem arquivo de ajustes aquele caminho
  // volta cedo e os rotulos ficariam vazios na primeira abertura da tela.
  rotulosDeIdioma();
  return 1;
}
void ajustes_encerrar(void) { uxCancelar(); uxRetornarOp = -1; guiaFechar(); ajlog_vazar_tudo(); }
int ajustes_quer_sair(void) { return sair; }

// Valor das linhas so de leitura. O espaco em disco NAO e um numero inventado:
// vem do cache de texturas, que e exatamente o que "imagens" consome no
// aparelho — um numero fixo aqui seria mentira e nunca mudaria.
// O ROTULO DA LINHA. Quase todos sao fixos (OPCOES[]); AJ_ATUALIZAR muda com
// o que se sabe: com versao nova e "Atualizar o aplicativo" (abre o cartao),
// sem ela e "Procurar atualização" (consulta agora).
// 2.0.3 (M2, estudo ajustes-ux): ROTULO EM LINGUAGEM COMUM. Onde o nome da
// linha era jargao ou marca ("Coincidencia por regex", "Background do hero"), a
// linha passa a dizer o que faz. O nome antigo continua em OPCOES[] e aparece
// no inspetor como "Nome tecnico", entra no indice da busca e nas trilhas do
// guia; nada no disco ou na conta depende do rotulo.
static const char *rotuloNovo(int op) {
  switch (op) {
    case AJ_FONTE_HDR: return "Dolby Vision e HDR";
    case AJ_ATMOS: return "Preferir som Dolby Atmos";
    case AJ_FONTE_MANUAL: return "Escolher a fonte você mesmo";
    case AJ_FONTE_PRIORIDADE: return "O que o Nuvio prioriza";
    case AJ_FONTE_PRAZO: return "Quanto espera pelos add-ons";
    case AJ_TAM_MAX: return "Tamanho máximo do arquivo";
    case AJ_TAM_MIN: return "Tamanho mínimo do arquivo";
    case AJ_PLR_CLASSIF: return "Classificação indicativa no player";
    case AJ_SAIDA_PLAYER: return "Ao sair no meio do filme";
    case AJ_FONTE_REGEX: return "Filtrar fontes pelo nome (regex)";
    case AJ_FONTE_REGEX_PADRAO: return "Padrão do filtro";
    case AJ_FONTE_REGEX_MODELO: return "Modelo pronto do filtro";
    case AJ_DTS_AC3: return "Converter DTS para a TV ouvir";
    case AJ_CACHE_SEEK: return "Guardar o trecho baixado no disco";
    case AJ_LEG_FORCADA: return "Só os letreiros quando o áudio já é no seu idioma";
    case AJ_TMDB_IDIOMA: return "Idioma dos dados do título";
    case AJ_HERO_FUNDO: return "Imagem de fundo do destaque";
    case AJ_GRAD_CLASSICO: return "Realce antigo no cartaz em foco";
    case AJ_LARGURA_DP: return "Largura do cartaz";
    case AJ_RAIO_DP: return "Cantos do cartaz";
    case AJ_TMDB_LIGADO: return "Dados do TMDB";
    case AJ_MDB_LIGADO: return "Notas do MDBList";
    case AJ_STALKER_PORTAL: return "Portal com MAC (Stalker)";
    case AJ_LIVETV_PROXY: return "Repassar o canal pelo app (proxy)";
    case AJ_LIVETV_MODO: return "Modo do player dos canais";
    case AJ_ESPACO: return "Imagens guardadas na memória";
    case AJ_NOTAS_HOME: return "Nota do IMDb nos cartazes";
    case AJ_SUFIXO_TIPO: return "Filme ou série no nome da fileira";
    case AJ_NOME_ADDON: return "Nome do addon na fileira";
    default: return NULL;
  }
}
static const char *rotuloOpcao(int op) {
  if (op == AJ_ATUALIZAR && !atualizacao_nova()[0]) return "Procurar atualização";
  if (rotuloNovo(op)) return rotuloNovo(op);
  return OPCOES[op].rotulo;
}
// O nome de antes (OPCOES[]), so quando a linha ganhou um rotulo novo.
static const char *rotuloTecnico(int op) {
  return op >= 0 && op < AJ_N && rotuloNovo(op) ? OPCOES[op].rotulo : NULL;
}
// A trilha do guia ainda diz o nome antigo de algumas linhas: troca pelo novo.
static const char *rotuloAtualDoAntigo(const char *s) {
  int k;
  for (k = 0; k < AJ_N; k++) if (rotuloNovo(k) && !strcmp(OPCOES[k].rotulo, s)) return rotuloNovo(k);
  return s;
}

// SERVIDORES PESSOAIS (jellyfin.h). Texto das linhas: so dado de exibicao
// (host sem esquema, nome do usuario, codigo do Quick Connect); token e senha
// nunca chegam aqui. A falha vem como codigo e e traduzida aqui.
static char jfUsuario[128];   // entre o teclado do usuario e o da senha
static const char *jfTexto(int op) {
  static char buf[192];
  char det[160];
  JfEstado e;
  if (!jellyfin_disponivel()) return i18n("Indisponível nesta plataforma");
  e = jellyfin_estado(det, sizeof det);
  if (op == AJ_JF_SERVIDOR) {
    const char *s = jellyfin_servidor_curto();
    if (e == JF_EST_VERIFICANDO) return i18n("Conferindo o servidor…");
    if (e == JF_EST_ERRO && jellyfin_ultimo_erro() == JF_ERR_FORMATO) return i18n("Não é um servidor Jellyfin");
    if (e == JF_EST_ERRO && jellyfin_ultimo_erro() == JF_ERR_ENTRADA) return i18n("Falhou");
    if (e == JF_EST_ERRO && jellyfin_ultimo_erro() == JF_ERR_REDE && !s[0])
      return i18n("O servidor não respondeu dentro do prazo.");
    return s[0] ? s : i18n("Não configurado");
  }
  if (op == AJ_JF_ENTRAR) {
    switch (e) {
      case JF_EST_CONECTADO:
        snprintf(buf, sizeof buf, i18n("Conectado: %s"), jellyfin_usuario());
        return buf;
      case JF_EST_QC_CODIGO:
        snprintf(buf, sizeof buf, i18n("Código %s · aprove no Quick Connect"), det);
        return buf;
      case JF_EST_ENTRANDO: return i18n("Entrando…");
      case JF_EST_EXPIROU: return i18n("expirou — reconectar");
      case JF_EST_ERRO:
        switch (jellyfin_ultimo_erro()) {
          case JF_ERR_AUTH: return i18n("Usuário ou senha incorretos");
          case JF_ERR_EXPIRADO: return i18n("o código expirou — OK pede outro");
          case JF_ERR_REDE: return i18n("O servidor não respondeu dentro do prazo.");
          default: return i18n("Falhou");
        }
      case JF_EST_SERVIDOR_OK: return det[0] ? (snprintf(buf, sizeof buf, "%s", det), buf) : "";
      default: return i18n("Não configurado");
    }
  }
  return "";
}
static void jfAtivar(int op) {
  JfEstado e;
  if (!ajustes_jellyfin_ligado()) return;
  e = jellyfin_estado(NULL, 0);
  if (op == AJ_JF_SERVIDOR) {
    stCampo = op;
    teclado_abrir_com("Endereço do Jellyfin",
                      "IP e porta do servidor, como 192.168.1.5:8096, ou o endereço com https://",
                      120, JF_ALFA_URL, jellyfin_servidor_curto()[0] ? jellyfin_servidor_curto() : NULL);
    return;
  }
  if (op == AJ_JF_SAIR) { jellyfin_esquecer(); desc_repetir_silencioso(); return; }
  if (op != AJ_JF_ENTRAR) return;
  if (e == JF_EST_QC_CODIGO || e == JF_EST_ENTRANDO) { jellyfin_cancelar_entrada(); return; }
  if (e == JF_EST_CONECTADO) { jellyfin_recarregar_bibliotecas(); return; }
  if (e == JF_EST_SEM_SERVIDOR || e == JF_EST_VERIFICANDO) return;
  // Quick Connect when the server allows it: nothing is typed on the TV.
  if (jellyfin_qc_permitido() == 1 && jellyfin_entrar_quick_connect()) return;
  stCampo = AJ_JF_ENTRAR;
  jfUsuario[0] = 0;
  teclado_abrir_com("Usuário do Jellyfin", "", 64, XT_ALFA_CONTA, NULL);
}

// EMBY: o mesmo desenho do Jellyfin, sem Quick Connect (so usuario e senha).
static const char *emTexto(int op) {
  static char buf[192];
  char det[160];
  JfEstado e;
  if (!jellyfin_disponivel()) return i18n("Indisponível nesta plataforma");
  e = emby_estado(det, sizeof det);
  if (op == AJ_EM_SERVIDOR) {
    const char *s = emby_servidor_curto();
    if (e == JF_EST_VERIFICANDO) return i18n("Conferindo o servidor…");
    if (e == JF_EST_ERRO && emby_ultimo_erro() == JF_ERR_FORMATO) return i18n("Não é um servidor Emby");
    if (e == JF_EST_ERRO && emby_ultimo_erro() == JF_ERR_ENTRADA) return i18n("Falhou");
    if (e == JF_EST_ERRO && emby_ultimo_erro() == JF_ERR_REDE && !s[0])
      return i18n("O servidor não respondeu dentro do prazo.");
    return s[0] ? s : i18n("Não configurado");
  }
  if (op == AJ_EM_ENTRAR) {
    switch (e) {
      case JF_EST_CONECTADO:
        snprintf(buf, sizeof buf, i18n("Conectado: %s"), emby_usuario());
        return buf;
      case JF_EST_ENTRANDO: return i18n("Entrando…");
      case JF_EST_EXPIROU: return i18n("expirou — reconectar");
      case JF_EST_ERRO:
        switch (emby_ultimo_erro()) {
          case JF_ERR_AUTH: return i18n("Usuário ou senha incorretos");
          case JF_ERR_REDE: return i18n("O servidor não respondeu dentro do prazo.");
          default: return i18n("Falhou");
        }
      case JF_EST_SERVIDOR_OK: return det[0] ? (snprintf(buf, sizeof buf, "%s", det), buf) : "";
      default: return i18n("Não configurado");
    }
  }
  return "";
}
static void emAtivar(int op) {
  JfEstado e;
  if (!ajustes_jellyfin_ligado()) return;
  e = emby_estado(NULL, 0);
  if (op == AJ_EM_SERVIDOR) {
    stCampo = op;
    teclado_abrir_com("Endereço do Emby",
                      "IP e porta do servidor, como 192.168.1.5:8096, ou o endereço com https://",
                      120, JF_ALFA_URL, emby_servidor_curto()[0] ? emby_servidor_curto() : NULL);
    return;
  }
  if (op == AJ_EM_SAIR) { emby_esquecer(); desc_repetir_silencioso(); return; }
  if (op != AJ_EM_ENTRAR) return;
  if (e == JF_EST_ENTRANDO) { emby_cancelar_entrada(); return; }
  if (e == JF_EST_CONECTADO) { emby_recarregar_bibliotecas(); return; }
  if (e == JF_EST_SEM_SERVIDOR || e == JF_EST_VERIFICANDO) return;
  stCampo = AJ_EM_ENTRAR;
  jfUsuario[0] = 0;
  teclado_abrir_com("Usuário do Emby", "", 64, XT_ALFA_CONTA, NULL);
}

// PLEX: sem teclado. A TV mostra o codigo de 4 letras e a pessoa o digita em
// plex.tv/link no celular; o token chega por polling.
static const char *pxTexto(int op) {
  static char buf[192];
  char det[160];
  PxEstado e;
  if (!plex_disponivel()) return i18n("Indisponível nesta plataforma");
  e = plex_estado(det, sizeof det);
  if (op == AJ_PX_SERVIDOR) {
    const char *s = plex_servidor_nome();
    if (e == PX_EST_ENTRANDO) return i18n("Entrando…");
    return s[0] && e == PX_EST_CONECTADO ? s : i18n("Não configurado");
  }
  if (op == AJ_PX_ENTRAR) {
    switch (e) {
      case PX_EST_CONECTADO:
        snprintf(buf, sizeof buf, i18n("Conectado: %s"), plex_usuario()[0] ? plex_usuario() : plex_servidor_nome());
        return buf;
      case PX_EST_CODIGO:
        if (!det[0]) return i18n("Entrando…");
        snprintf(buf, sizeof buf, i18n("Código %s · digite em plex.tv/link"), det);
        return buf;
      case PX_EST_ENTRANDO: return i18n("Entrando…");
      case PX_EST_EXPIROU: return i18n("expirou — reconectar");
      case PX_EST_ERRO:
        switch (plex_ultimo_erro()) {
          case PX_ERR_EXPIRADO: return i18n("o código expirou — OK pede outro");
          case PX_ERR_SEM_SERVIDOR: return i18n("Nenhum servidor Plex nesta conta");
          case PX_ERR_REDE: return i18n("O servidor não respondeu dentro do prazo.");
          default: return i18n("Falhou");
        }
      default: return i18n("Não configurado");
    }
  }
  return "";
}
static void pxAtivar(int op) {
  PxEstado e;
  if (!ajustes_jellyfin_ligado()) return;
  e = plex_estado(NULL, 0);
  if (op == AJ_PX_SERVIDOR) { plex_proximo_servidor(); return; }
  if (op == AJ_PX_SAIR) { plex_esquecer(); desc_repetir_silencioso(); return; }
  if (op != AJ_PX_ENTRAR) return;
  if (e == PX_EST_CODIGO || e == PX_EST_ENTRANDO) { plex_cancelar_entrada(); return; }
  if (e == PX_EST_CONECTADO) { plex_recarregar_bibliotecas(); return; }
  plex_entrar();
}

static const char *frResumo(int plugin);
static const char *frOrdemResumo(void);
static const char *frRegexTexto(void);
static const char *textoLeitura(int op) {
  static char buf[64];
  // MASCARADO, sempre. Esta tela e fotografada e colada em issue — foi assim
  // que chegou o relato do painel de Tracking. O portal sai sem esquema e o MAC
  // so com os dois ultimos octetos: o bastante para a pessoa reconhecer QUAL
  // cadastro esta ali, insuficiente para alguem usar o acesso dela.
  if (op == AJ_STALKER_PORTAL)
    return stalker_configurado() ? stalker_portal_curto() : i18n("Não configurado");
  if (op == AJ_STALKER_MAC)
    return stalker_configurado() ? stalker_mac_mascarado() : i18n("Não configurado");
  if (op == AJ_XTREAM_SERVIDOR)
    return strcmp(xtream_servidor_curto(), "-") ? xtream_servidor_curto() : i18n("Não configurado");
  if (op == AJ_XTREAM_USUARIO)
    return strcmp(xtream_usuario(), "-") ? xtream_usuario() : i18n("Não configurado");
  if (op == AJ_XTREAM_SENHA)
    return strcmp(xtream_senha_mascarada(), "-") ? xtream_senha_mascarada() : i18n("Não configurado");
  if (op == AJ_XTREAM_CONTA) {
    // So o que a conta diz de si (status, vencimento, telas). Sem usuario:
    // a linha acima ja o mostra, e esta tela vai para foto de issue.
    static char bufConta[160];
    XtreamConta c;
    long long agora = (long long)time(NULL);
    if (!xtream_configurado()) return i18n("Não configurado");
    if (!xtream_conta(&c)) return i18n("abra o Guia para conferir");
    if (!c.auth) return i18n("recusada pelo servidor");
    { int a = xtream_conta_aviso(&c, agora);
      if (a == XA_EXPIRADA) return i18n("vencida");
      if (a == XA_DESATIVADA) return i18n("desativada pelo provedor");
      if (c.expira > 0) {
        time_t t = (time_t)c.expira;
        struct tm *m = localtime(&t);
        char d[16];
        strftime(d, sizeof d, "%d/%m/%Y", m);
        if (c.maxConexoes > 0)
          snprintf(bufConta, sizeof bufConta, i18n("ativa até %s · %d de %d telas"), d, c.conexoes, c.maxConexoes);
        else snprintf(bufConta, sizeof bufConta, i18n("ativa até %s"), d);
      } else if (c.maxConexoes > 0)
        snprintf(bufConta, sizeof bufConta, i18n("ativa · %d de %d telas"), c.conexoes, c.maxConexoes);
      else snprintf(bufConta, sizeof bufConta, "%s", i18n("ativa"));
      return bufConta; }
  }
  if (op == AJ_FANART_CHAVE) return fanartMascarada();
  if (op == AJ_SEEKR_CHAVE) return seekrMascarada();
  if (op == AJ_SEEKR_TESTAR) {
    static char seekrValor[160];
    SeekrUso u; seekr_uso(&u);
    if (u.chavePropria) snprintf(seekrValor, sizeof seekrValor, "%s", skTesteTexto());
    else if (u.persistente)
      snprintf(seekrValor, sizeof seekrValor, i18n("%s · %d/%d"), skTesteTexto(), u.usadas, u.limite);
    else snprintf(seekrValor, sizeof seekrValor, "%s", i18n("Uso do Seekr indisponível"));
    return seekrValor;
  }
  if (op == AJ_SELOS_PACOTE_ADD) return spAddTexto();
  if (op == AJ_SELOS_PACOTE_REM) {
    int a = selospacote_ativo();
    if (a < 0) return i18n("nenhum pacote escolhido");
    return selospacote_da_tv(a) ? i18n("OK remove") : i18n("só na conta, no Nuvio web");
  }
  if (op == AJ_PERFIL_EDITAR) {
    // static: o texto devolvido e lido DEPOIS do return (era endereco de
    // variavel local, -Wreturn-stack-address).
    static RecPerfil pf;
    recomenda_perfil(&pf);
    return pf.apelido[0] ? pf.apelido : i18n("Não configurado");
  }
  if (op == AJ_FONTE_ORDEM) return frOrdemResumo();
  if (op == AJ_FONTE_ADDONS_PERM) return frResumo(0);
  if (op == AJ_FONTE_PLUGINS_PERM) return frResumo(1);
  if (op == AJ_FONTE_REGEX_PADRAO) return frRegexTexto();
  if (op == AJ_P2P_URL) return p2pEndereco[0] ? p2pEndereco
                             : p2pmotor_disponivel() ? i18n("Nesta TV") : i18n("Não configurado");
  if (op >= AJ_JF_SERVIDOR && op <= AJ_JF_SAIR) return jfTexto(op);
  if (op >= AJ_EM_SERVIDOR && op <= AJ_EM_SAIR) return emTexto(op);
  if (op >= AJ_PX_ENTRAR && op <= AJ_PX_SAIR) return pxTexto(op);
  if (op == AJ_P2P_URL) return p2pEndereco[0] ? p2pEndereco : i18n("Não configurado");
  if (op == AJ_P2P_TESTAR) return p2pTesteTexto();
  if (op >= AJ_POSTER_INST && op <= AJ_POSTER_TESTAR) return pstTexto(op);
  if (debIdx(op) >= 0) return debValor(op);
  if (op == AJ_DEBRID_AD_TESTAR) return adTesteTexto();
  if (op == AJ_ENVIAR_LOG) {
    static char b[48];
    AvisosEnvio env;
    switch (avisos_envio_info(&env)) {
      case 1:  return i18n("enviando…");
      case 2:
        // O codigo fica na linha ate a sessao acabar (mockup quadro 9).
        if (env.codigo[0]) { snprintf(b, sizeof b, i18n("enviado · %s"), env.codigo); return b; }
        return i18n("enviado. Obrigado.");
      case 3:  return i18n("não foi possível enviar");
      default: return i18n("OK envia");
    }
  }
  if (op == AJ_VER_REGISTRO)
#ifdef NV_ANDROID
    return i18n("ou botão Info");
#else
    return i18n("ou botão vermelho");
#endif
  if (op == AJ_ATUALIZAR && !atualizacao_nova()[0]) {
    // "Procurar atualização": a resposta da ultima consulta (a automatica
    // tambem conta — ela e tao verdadeira quanto a pedida).
    // CURTA aqui (a frase inteira nao cabe ao lado do rotulo); a frase
    // completa vai na ajuda, em efeitoOpcao.
    switch (atualizacao_busca()) {
      case ATUALIZACAO_BUSCA_PROCURANDO: return i18n("Procurando…");
      case ATUALIZACAO_BUSCA_EM_DIA:
        snprintf(buf, sizeof buf, i18n("Em dia (%s)"), AJ_VERSAO);
        return buf;
      case ATUALIZACAO_BUSCA_ERRO: return i18n("Sem resposta");
      default: return i18n("OK procura");
    }
  }
  if (op == AJ_VERSAO_I) {
    // Com release mais nova no GitHub, a linha diz as duas.
    if (atualizacao_nova()[0]) {
      snprintf(buf, sizeof buf, i18n("%s · nova: %s"), AJ_VERSAO, atualizacao_nova());
      return buf;
    }
    return AJ_VERSAO;
  }
  if (op == AJ_PERFIL_ATIVO) {
    static char bufp[80];
    int i;
    for (i = 0; i < perfis_n(); i++)
      if (perfis_item(i)->indice == perfis_ativo()) return perfis_item(i)->nome;
    // Sem lista de perfis, dizer "Perfil 1" e mais honesto que deixar vazio: e
    // literalmente o que o app esta usando em p_profile_id.
    snprintf(bufp, sizeof bufp, i18n("Perfil %d"), perfis_ativo());
    return bufp;
  }
  // TODO VALOR DAQUI VAI PARA A TELA, ENTAO TODO VALOR PASSA POR i18n().
  //
  // Estes voltavam CRUS e a varredura nao os via: ela olha o literal entregue a
  // uma funcao de DESENHO, e aqui o literal e devolvido por um `return` — quem
  // desenha recebe um `const char *` e nao tem como saber de onde veio. Doze
  // literais atravessaram assim, e o relator do #23 os leu na TV em ingles:
  // "conectado" e "conectar" nas linhas do Trakt e do Simkl.
  //
  // Regra para quem editar esta funcao: se o texto aparece na lista de Ajustes,
  // ele e interface. Nao ha valor "tecnico demais para traduzir" aqui.
  if (op == AJ_SYNC) {
    switch (sync_estado()) {
      case SYNC_RODANDO: return i18n("sincronizando…");
      case SYNC_FALHOU:  return i18n("falhou");
      case SYNC_PRONTO:  return sync_resumo();
      default:           return sessao_logada() ? i18n("aguardando") : i18n("sem conta");
    }
  }
  if (op == AJ_TRAKT) {
    switch (traktauth_estado()) {
      case TRA_LIGADO:     return i18n("conectado");
      case TRA_PEDINDO:    return i18n("preparando…");
      case TRA_AGUARDANDO: return i18n("aguardando");
      case TRA_ERRO:       return i18n("falhou");
      case TRA_INVALIDO:   return i18n("expirou — reconectar");
      default:             return i18n("conectar");
    }
  }
  if (op == AJ_DISCORD) {
    if (!discord_disponivel()) return i18n("indisponível nesta versão");
    switch (discord_estado()) {
      case DIS_LIGADO:     return i18n("conectado — OK desconecta");
      case DIS_PEDINDO:    return i18n("preparando…");
      case DIS_AGUARDANDO: return i18n("aguardando");
      case DIS_ERRO:       return i18n("falhou");
      case DIS_INVALIDO:   return i18n("expirou — reconectar");
      default:             return i18n("conectar");
    }
  }
  if (op == AJ_SIMKL) {
    switch (simklauth_estado()) {
      case SMK_LIGADO:     return i18n("conectado");
      case SMK_PEDINDO:    return i18n("preparando…");
      case SMK_AGUARDANDO: return i18n("aguardando");
      case SMK_ERRO:       return i18n("falhou");
      default:             return i18n("conectar");
    }
  }
  if (op == AJ_PLUGINS) {
    if (!plugins_disponivel()) return i18n("Indisponível nesta plataforma");
    if (!plugins_ligado()) return i18n("Desligado");
    snprintf(buf, sizeof buf, i18n("%d repositórios"), plugins_n_repos());
    return buf;
  }
  if (op == AJ_ADDONS) {
    int i, lig = 0, n = addons_n();
    for (i = 0; i < n; i++) if (addons_ativo(i)) lig++;
    snprintf(buf, sizeof buf, i18n("%d de %d"), lig, n);
    return buf;
  }
  // Armada pelo primeiro OK, a linha diz o que o segundo faz. Em repouso nao
  // diz nada: o chevron ja e o "OK faz alguma coisa aqui".
  // Com o servidor da conta fora do ar, sair agora e ficar sem conta ate ele
  // voltar: o login fala com o mesmo servidor (#215 — a pessoa saiu para
  // "consertar" os addons sumidos e nao conseguiu entrar de novo). A linha
  // armada diz isso antes do segundo OK.
  if (op == AJ_SAIR)
    return sairArmado != op ? ""
         : sync_servidor_fora() ? i18n("Servidor da conta fora do ar: OK de novo sai mesmo assim")
         : i18n("OK de novo para sair");
  if (op == AJ_STALKER_LIMPAR || op == AJ_XTREAM_LIMPAR)
    return sairArmado == op ? i18n("OK de novo para remover") : "";
  if (op == AJ_MDB_CHAVE) {
    // A chave chega pela CONTA (sync.c -> extras_definir_chave) ou pelo
    // arquivo art/mdblist.txt. Mostra so o estado, nunca os caracteres — a
    // linha e de leitura justamente porque nao ha teclado nesta tela.
    return extras_mdblist_tem_chave() ? i18n("definida") : i18n("ausente");
  }
  if (op == AJ_HERO_CATALOGOS) return heroFonteRotulo();
  if (op == AJ_ESPACO) {
    // CURTO O BASTANTE PARA CABER NA COLUNA: "201.0 MB em 209 imagens" era
    // cortado em "209..." na TV, e o numero que sobrava era o menos util. O
    // detalhe (orcamento, grafico, o que esta na tela) vai no painel da
    // direita, que tem espaco — ver desenhaPainelImagens.
    int itens = 0; long bytes = 0;
    tex_estatisticas(&itens, NULL, &bytes, NULL, NULL);
    snprintf(buf, sizeof buf, i18n("%.1f MB · %d imagens"), bytes / 1048576.0, itens);
    idioma_decimal_texto(buf, ajustes_idioma());
    return buf;
  }
  // ACAO SEM VALOR PROPRIO. Este `return` era o da memoria de imagens, e toda
  // acao que nao tinha ramo acima caia nele: "Ordenar e ativar fileiras"
  // mostrava "100.1 MB em 119..." na coluna do valor (foto do dono, 16/09).
  // Nem "Abrir": o chevron "›" da linha ja diz que o OK leva a algum lugar, e
  // a palavra repetida em toda acao so empurrava o rotulo para o corte.
  if (OPCOES[op].tipo == OP_ACAO) return "";
  return "";
}

// Uma opcao pode ficar INATIVA por causa de outra — o web esconde a linha
// (`model.layout.modernSidebar ? "" : renderToggleRow(...)`), mas esconder num
// D-pad muda a contagem de linhas embaixo do dedo do usuario a cada toque. Aqui
// ela continua no lugar, apagada e sem setas: a dependencia fica visivel em vez
// de a linha sumir.
static int inativa(int op) {
  switch (op) {
    case AJ_VIDRO_CONTORNO: case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO: return !lig(AJ_VIDRO);
    case AJ_RAIL:         return ajustes_rail_moderna();
    case AJ_RAIL_BLUR:    return !ajustes_rail_moderna();
    case AJ_HERO_CATALOGOS: return !ajustes_hero_ligado();
    // O fundo em tela cheia e da Moderna: no Padrao o destaque e um banner e na
    // Dinamica ele e sempre de ponta a ponta e rola junto com as fileiras.
    case AJ_HERO_CHEIO:   return ajustes_home_layout() != HOME_LAYOUT_MODERNA;
    // #162: o Descobrir do app web (navegar catalogos por tipo e genero) ainda
    // nao existe nesta TV — o "Explorar" daqui e outra tela. A escolha vem e
    // vai para a conta, mas aqui nao muda nada, e a linha tem de dizer isso.
    case AJ_DESCOBRIR:    return 1;
    // Tela de descanso: o estilo depende de haver descanso; os titulos, de ele
    // ser a vitrine.
    case AJ_DESCANSO_ESTILO: return valor[AJ_ESMAECER] == 0;
    case AJ_DESCANSO_FONTE:  return valor[AJ_ESMAECER] == 0 || ajustes_descanso_estilo() != 0;
    case AJ_CW_OK: case AJ_CW_FONTE: case AJ_CW_PROXIMO:
    case AJ_CW_ESTILO: case AJ_CW_THUMB: case AJ_CW_FURTHEST:
    case AJ_CW_NAO_EXIBIDOS: case AJ_CW_ORDEM: case AJ_CW_CONCLUIDO: case AJ_CW_RETIDO_TAMBEM:
      return !ajustes_cw_ligado();
    case AJ_CW_BLUR_PROX: return !ajustes_cw_ligado() || !ajustes_cw_thumb_episodio();
    case AJ_EXPANDIR_ATRASO: return !ajustes_expandir_poster();
    case AJ_SEEKR_FITA: case AJ_SEEKR_AJUSTE: return !lig(AJ_SEEKR_LIGADO);
    // Remover so vale para o pacote escolhido que foi posto nesta TV.
    case AJ_SELOS_PACOTE_REM: return selospacote_ativo() < 0 || !selospacote_da_tv(selospacote_ativo());
    // Sem o relogio nao ha ilha para onde minimizar: sai para a pagina, como antes.
    case AJ_SAIDA_PLAYER: return !lig(AJ_RELOGIO);
    // So existe sessao para manter quando a saida vai para a ilha.
    case AJ_MANTER_VIDEO: return !ajustes_saida_player_home();
    // AJ_FONTE_PRAZO NAO DEPENDE DE NADA (#238). Era desligada com "Escolher a
    // fonte ao reproduzir", mas app.c (autoParcialPronto) ainda usa o prazo
    // quando ha fonte lembrada para o titulo, mesmo escolhendo a mao.
    case AJ_CACHE_SEEK:  return !cacheSeekExiste();
    // #334: so o motor embutido usa o limite; com servidor na rede quem guarda e ele.
    case AJ_P2P_LIMITE:  return !p2pmotor_disponivel() || p2pEndereco[0];
    case AJ_DTS_AC3:     return !dtsConversaoExiste();
    // Som: na Samsung (.wgt) o trailer e sempre mudo (trailerfonte_com_som).
    case AJ_HERO_TRAILER_SOM:
      return !lig(AJ_HERO_TRAILER) || !trailerfonte_com_som(trailerfonte_tizen());
    case AJ_DET_TRAILER_SOM:
      return !lig(AJ_DET_TRAILER_AUTO) || !trailerfonte_com_som(trailerfonte_tizen());
    case AJ_HERO_TRAILER_ESPERA: return !lig(AJ_HERO_TRAILER);
    // Como no web (getFocusedPosterFlowConfig): o trailer do cartaz so existe
    // com o cartaz expandindo ou com cartazes deitados.
    case AJ_FOCO_TRAILER: return !ajustes_expandir_poster() && valor[AJ_LANDSCAPE] != 0;
    // AJ_ATUALIZAR NAO APAGA MAIS (01/10/2026): sem versao nova conhecida
    // ela vira "Procurar atualização" e consulta o GitHub na hora.
    case AJ_PROF_BORDA: case AJ_PROF_BRILHO: case AJ_PROF_COBERTURA:
    case AJ_PROF_POSTERS: case AJ_PROF_CW: case AJ_PROF_EPS:
    case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS:
      return !ajustes_profundidade();
    // Integracoes: cada recurso depende do master da sua integracao, como o
    // `disabled: !enabled` das linhas do web.
    case AJ_TMDB_IDIOMA: case AJ_TMDB_ARTE: case AJ_TMDB_FICHA:
    case AJ_TMDB_DATAS: case AJ_TMDB_ELENCO: case AJ_TMDB_PROD:
    case AJ_TMDB_REDES: case AJ_TMDB_EPS: case AJ_TMDB_TRAILERS:
    case AJ_TMDB_MAIS: case AJ_TMDB_COL: case AJ_TMDB_CW:
      return !ajustes_tmdb_ligado();
    // A chave e so leitura (vem da conta): nunca apagada; o MDBList desligado
    // esta em Mais opcoes e as notas o religam (notaLigarPar).
    case AJ_MDB_CHAVE: return 0;
    case AJ_MDB_TRAKT: case AJ_MDB_IMDB: case AJ_MDB_TMDB:
    case AJ_MDB_LETTER: case AJ_MDB_TOMATES: case AJ_MDB_AUDIENCIA:
    case AJ_MDB_META: case AJ_MDB_MAL:
      return !ajustes_mdblist_ligado();
    // Linha do titulo: a nota que so o MDBList traz fica APAGADA sem a chave (ou
    // com o master desligado) — ligar nao faria aparecer nada, e a linha diz
    // por que (motivo abaixo). IMDb e Trakt funcionam sem chave.
    case AJ_NT_TOMATES: case AJ_NT_AUDIENCIA: case AJ_NT_META:
    case AJ_NT_METAUSER: case AJ_NT_TMDB: case AJ_NT_LETTER: case AJ_NT_MAL:
    case AJ_NT_EBERT: case AJ_NT_SCORE:
      return !extras_mdblist_tem_chave();
    // Cada campo so vale para o provedor dele; o teste, para qualquer um ligado.
    case AJ_POSTER_INST: case AJ_POSTER_TOKEN: case AJ_POSTER_EXTRA:
      return valor[AJ_POSTER_PROV] != PP_SPATIAL;
    case AJ_POSTER_CHAVE:  return valor[AJ_POSTER_PROV] != PP_RPDB;
    case AJ_POSTER_MODELO: return valor[AJ_POSTER_PROV] != PP_MODELO;
    case AJ_POSTER_TESTAR: return valor[AJ_POSTER_PROV] == PP_DESLIGADO;
    // Sem servico de posteres o cartaz ja e o do addon: nao ha o que escolher.
    case AJ_ADDON_POSTER:  return valor[AJ_POSTER_PROV] == PP_DESLIGADO;
    // A unica troca do logo do addon e a "Arte localizada" do TMDB.
    case AJ_ADDON_LOGO:    return !ajustes_tmdb_ligado() || !ajustes_tmdb_arte();
    default: return 0;
  }
}

// Acao NAO e leitura (tem o destaque de linha ativa), mas tambem NAO e mutavel
// (esquerda/direita nao fazem nada nela). As duas respostas sao diferentes de
// proposito, e e por isso que sao duas funcoes.
static int soLeitura(int op) { return OPCOES[op].tipo == OP_LEITURA; }
static int mutavel(int op)   { return OPCOES[op].tipo != OP_LEITURA &&
                                      OPCOES[op].tipo != OP_ACAO && !inativa(op); }

// "MAIS OPCOES" DA CATEGORIA (2.0.3): quantas avancadas ela tem neste build e,
// se `resumo`, os primeiros rotulos ("DTS, Dolby Vision em MKV, ..."), ja
// traduzidos, para a linha fechada dizer o que ha dentro.
static int maisContar(int sec, char *resumo, size_t n) {
  int i, k = 0;
  size_t u = 0;
  if (resumo && n) resumo[0] = 0;
  if (sec < 0 || sec >= nSecoes || maisItem[sec] < 0) return 0;
  for (i = maisItem[sec] + 1; i < secFim(sec); i++) {
    int op = TELA[i].op;
    if (TELA[i].tipo != IT_OPC) continue;
    if ((op == AJ_PERFIL_PESQ || op == AJ_PERFIL_EDITAR) && !recomenda_ativo()) continue;
    if (op == AJ_ICONE_APP && !apoiador_ativo()) continue;
    if (resumo && u + 4 < n && k < 6) {
      const char *r = i18n(rotuloOpcao(op));
      u += (size_t)snprintf(resumo + u, n - u, "%s%s", k ? ", " : "", r);
      if (u >= n) u = n - 1;
    }
    k++;
  }
  return k;
}

// --- NAVEGACAO SOBRE TELA[] ---------------------------------------------------
// Item desenhado agora? Opcao de grupo fechado nao e — nem desenhada, nem
// alcancada pelo cima/baixo.
static int visivel(int i) {
  if (i >= 0 && i < AJ_N_TELA && TELA[i].tipo == IT_OPC &&
      TELA[i].op == AJ_ICONE_APP && !apoiador_ativo()) return 0;
  if (i < 0 || i >= AJ_N_TELA) return 0;
  if (TELA[i].tipo == IT_ROT) {
    int j;
    for (j = i + 1; j < AJ_N_TELA && TELA[j].tipo == IT_OPC; j++)
      if (visivel(j)) return 1;
    return 0;
  }
  if (TELA[i].tipo == IT_OPC) {
    int op = TELA[i].op;
    if ((op == AJ_PERFIL_PESQ || op == AJ_PERFIL_EDITAR) && !recomenda_ativo()) return 0;
    if (op == AJ_BUSCA_CINEMETA && valor[op] == 0 && !cinemetaInstalado()) return 0;
    if (uxAvancada(op) && !lig(AJ_AVANCADAS) && !maisAberto[secDoItem[i]]) return 0;
  }
  // A linha "Mais opcoes" some com a pilula Avancadas ligada (ja esta tudo a vista).
  if (TELA[i].tipo == IT_MAIS) return !lig(AJ_AVANCADAS) && maisContar(secDoItem[i], NULL, 0) > 0;
  return 1;
}
static int focavel(int i) {
  return (TELA[i].tipo == IT_GRP || TELA[i].tipo == IT_OPC || TELA[i].tipo == IT_MAIS) && visivel(i);
}
static void focar(int i) {
  if (i < 0 || i >= AJ_N_TELA) return;
  focoItem = i;
  focoOp = TELA[i].tipo == IT_OPC ? TELA[i].op : -1;
  secAtual = secDoItem[i];
  uxIndice = secAtual + 2; uxChipAv = 0; uxTopo = -1; uxCabLayout = 0;
  uxUltimoItem[secAtual] = i;
  emEdicao = 0;
  sairArmado = 0;
}
// Primeiro item com foco da categoria. Categoria sem nenhum nao existe (a
// conferencia do arranque pegaria), mas cair no proprio SEC e melhor que -1.
static int primeiroDaSecao(int s) {
  int i;
  for (i = secIni[s] + 1; i < secFim(s); i++) if (focavel(i)) return i;
  return secIni[s];
}
static void focarSecao(int s) {
  if (s < 0 || s >= nSecoes) return;
  { int i = uxUltimoItem[s];
    focar(i > secIni[s] && i < secFim(s) && focavel(i) ? i : primeiroDaSecao(s)); }
}
static void abrirGrupo(int g) {
  int s = secDoItem[g];
  grupoAberto[s] = (grupoAberto[s] == g) ? -1 : g;
}
// Foco direto numa opcao, de fora da navegacao (ajustes_abrir_na_cor): abre o
// grupo que a contem (se houver) e poe o foco na LISTA, na linha dela.
static void focarOpcao(int op) {
  int i;
  for (i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].tipo != IT_OPC || TELA[i].op != op) continue;
    // Busca/atalho para uma avancada: abre o "Mais opcoes" da categoria dela
    // (so nesta sessao) em vez de esconder o destino.
    if (uxAvancada(op) && !lig(AJ_AVANCADAS)) maisAberto[secDoItem[i]] = 1;
    focar(i);
    focoIndice = 0;
    return;
  }
}

// UMA FRASE POR OPCAO, sem excecao.
//
// O relato foi "tem pouca informacao". Ele nao era impressao: das 58 linhas,
// quinze tinham frase propria e as OUTRAS QUARENTA E TRES caiam num texto
// generico ("Use as setas laterais para escolher") que nao diz o que a opcao
// faz — ou seja, a area de ajuda ocupava um terco da tela para nao informar
// nada em tres de cada quatro linhas. O `default` continua existindo como rede
// de seguranca para opcao nova, mas nenhuma opcao de hoje cai nele.
//
// A frase responde "o que isto E". O que MUDA na pratica vai em efeitoOpcao,
// separado de proposito: as duas perguntas sao diferentes e juntas viram um
// paragrafo que ninguem le do sofa.
static const char *ajudaOpcaoBase(int op) {
  if (op == AJ_ICONE_APP) return "Escolha uma marca alternativa para o Nuvio nesta TV. Um agradecimento a quem apoia o projeto.";
  if (inativa(op)) {
    if (op == AJ_VIDRO_CONTORNO) return "Ative a interface de vidro para ajustar o contorno.";
    if (op == AJ_VIDRO_OPAC || op == AJ_VIDRO_FOSCO) return "Ative a interface de vidro para ajustar o vidro.";
    if (op == AJ_RAIL) return "Desative a barra lateral moderna para escolher entre recolhida e fixa.";
    if (op == AJ_RAIL_BLUR) return "Ative a barra lateral moderna para usar o desfoque.";
    if (op == AJ_HERO_CATALOGOS) return "Ative Mostrar destaque para exibir os catálogos no topo da Home.";
    if (op == AJ_HERO_CHEIO) return "Só vale no layout Moderna. No Padrão o destaque é um banner, e na Dinâmica ele ocupa a largura toda e sobe junto com a rolagem.";
    if (op == AJ_DTS_AC3) return "Não disponível nesta TV. Só a LG converte o DTS dentro do app.";
    if (op == AJ_CACHE_SEEK) return "Não disponível nesta TV. O player da LG e da Samsung não deixa o app guardar o vídeo em disco.";
    if (op == AJ_DESCOBRIR) return "A tela Descobrir do app web ainda não existe nesta TV. A escolha fica guardada na conta.";
    if ((op >= AJ_CW_OK && op <= AJ_CW_ORDEM) || op == AJ_CW_PROXIMO || op == AJ_CW_CONCLUIDO || op == AJ_CW_RETIDO_TAMBEM)
      return op == AJ_CW_BLUR_PROX && ajustes_cw_ligado()
        ? "Ative Miniatura do episódio para desfocar a imagem do próximo episódio."
        : "Ative Continuar assistindo para ajustar os cards de retomada.";
    if (op == AJ_EXPANDIR_ATRASO) return "Ative Expandir pôster ao focar para ajustar o tempo de espera.";
    if (op == AJ_HERO_TRAILER_SOM && lig(AJ_HERO_TRAILER))
      return "Nesta TV o trailer do destaque toca sempre sem som.";
    if (op == AJ_DET_TRAILER_SOM && lig(AJ_DET_TRAILER_AUTO))
      return "Nesta TV o trailer da página do título toca sempre sem som.";
    if (op == AJ_DET_TRAILER_SOM)
      return "Ative o trailer automático da página do título para ajustar o som.";
    if (op == AJ_HERO_TRAILER_SOM || op == AJ_HERO_TRAILER_ESPERA)
      return "Ative Trailer no destaque para ajustar o trailer do topo da Home.";
    if (op == AJ_FOCO_TRAILER) return "Ative Expandir pôster ao focar, ou Pôsteres horizontais, para usar o trailer do cartaz em foco.";
    if (op > AJ_TMDB_LIGADO && op <= AJ_TMDB_CW)
      return "Ative TMDB para ajustar o que ele enriquece.";
    if (op > AJ_MDB_LIGADO && op <= AJ_MDB_MAL)
      return "Ative MDBList para escolher as fontes de nota.";
    if (op >= AJ_NT_IMDB && op <= AJ_NT_SCORE)
      return extras_mdblist_tem_chave()
        ? "Ative MDBList para mostrar esta nota."
        : "Esta nota vem do MDBList e precisa da chave dele na sua conta Nuvio.";
    if (op == AJ_ADDON_POSTER)
      return "Sem Pôsteres personalizados ligado o pôster já é o que o addon manda. Escolha um serviço para decidir quem vence.";
    if (op == AJ_ADDON_LOGO)
      return "O logo do addon só é trocado pela Arte localizada do TMDB. Ative TMDB e Arte localizada para escolher.";
    // #238: estas cinco caiam na frase da profundidade, que nao tem nada a ver
    // com elas ("Espera pelos add-ons" mandava ligar o Efeito de profundidade).
    if (op == AJ_SEEKR_FITA || op == AJ_SEEKR_AJUSTE)
      return "Ative Miniaturas na barra de tempo para ajustar as miniaturas.";
    if (op == AJ_SELOS_PACOTE_REM)
      return "Só dá para remover um pacote que foi adicionado nesta TV.";
    if (op == AJ_SAIDA_PLAYER)
      return "Ative Relógio na tela: sem ele o player não tem para onde minimizar.";
    if (op == AJ_MANTER_VIDEO)
      return "Só vale com Ao sair do player em Voltar para a home e o Relógio na tela ligado.";
    if (op == AJ_POSTER_INST || op == AJ_POSTER_TOKEN || op == AJ_POSTER_EXTRA ||
        op == AJ_POSTER_CHAVE || op == AJ_POSTER_MODELO || op == AJ_POSTER_TESTAR)
      return "Este campo vale para outro serviço. Escolha o serviço em Pôsteres personalizados.";
    if (op == AJ_PROF_BORDA || op == AJ_PROF_BRILHO || op == AJ_PROF_COBERTURA ||
        op == AJ_PROF_POSTERS || op == AJ_PROF_CW || op == AJ_PROF_EPS ||
        op == AJ_PROF_ELENCO || op == AJ_PROF_TRAILERS)
      return "Ative Efeito de profundidade para personalizar este detalhe.";
    return "Indisponível com os ajustes atuais.";
  }
  switch (op) {
    // --- Reproducao
    case AJ_QUALIDADE: return "Define a preferência de resolução. A disponibilidade depende das fontes do addon.";
    case AJ_DV: case AJ_ATMOS: return "Preferência para fontes compatíveis. O formato disponível também depende do arquivo e da TV.";
    case AJ_LEG_SYNC_AUTO: return "Ajusta a legenda automaticamente. Desligue para manter a escolhida. Vale só nesta TV.";
    case AJ_LEG_SYNC_AUDIO: return "Compara as falas do áudio com a legenda externa para acertar o atraso. Só onde o player entrega o áudio decodificado e sem passthrough; vale só nesta TV.";
    case AJ_LEG_FORCADA: return "Quando o áudio já está no idioma da legenda, liga só a legenda forçada desse idioma (falas em outra língua e placas), ou nenhuma se o arquivo não tiver. Desligado, liga a legenda completa como antes.";
    case AJ_LEG_LINGUA2: return "Segunda legenda, mostrada no alto da tela junto com a principal. Só arquivos SRT/VTT dos addons. \"Da conta\" segue o que está no seu perfil.";
    case AJ_LEG2_POS: return "No topo, a segunda legenda fica no alto da tela. Junto da principal, ela fica logo acima da principal, no pé da tela.";
    case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA:
      return "Aparência só da segunda legenda. \"Igual à principal\" segue o estilo da principal, definido no player.";
    case AJ_LEG_LINGUA: return "Idioma ligado sozinho quando o vídeo começa. Legendas dos addons aparecem em inglês e no idioma escolhido aqui. \"Da conta\" segue o que está no seu perfil.";
    case AJ_AUD_LINGUA: return "Faixa de áudio escolhida quando o arquivo tem mais de uma. Se o idioma não existir no arquivo, o player usa a primeira.";
    case AJ_PAUSA_OVERLAY: return "Ao pausar, sobe uma ficha com a sinopse e os dados do que você está vendo.";
    case AJ_REACAO_CREDITOS: return "Nos créditos de um filme, ou no fim de uma temporada, um cartão pequeno pergunta o que você achou. Some sozinho em 8 s. Com o Trakt ligado, a resposta vira nota lá.";
    case AJ_STALKER_PORTAL: return "Os canais do portal entram no Guia de TV, junto com os dos addons. Endereço sem http:// e sem barra no fim: meu-portal.exemplo.tv:8080";
    case AJ_STALKER_MAC: return "O MAC que o provedor cadastrou para você. É credencial: vale como senha, e só aparece nesta tela mascarado.";
    case AJ_STALKER_LIMPAR: return "Apaga o portal e o MAC deste perfil, e os canais dele somem do Guia. Sair da conta também apaga.";
    case AJ_XTREAM_SERVIDOR: return "Endereço e porta que o provedor mandou, sem http:// e sem barra no fim: meu-servidor.tv:8080. Os canais entram no Guia de TV com usuário e senha preenchidos.";
    case AJ_XTREAM_USUARIO: {
      // #88: a linha da lista corta em ~360 px; o valor completo mora aqui no
      // painel de ajuda, onde ha largura de sobra para ler/conferir.
      static char buf[220];
      const char *u = xtream_usuario();
      if (strcmp(u, "-")) {
        snprintf(buf, sizeof buf, i18n("O usuário da sua assinatura Xtream.\n\nAgora: %s"), u);
        return buf;
      }
      return "O usuário da sua assinatura Xtream.";
    }
    case AJ_XTREAM_SENHA: return "A senha da assinatura. É credencial: vai dentro de cada URL de canal e nunca aparece nesta tela em claro.";
    case AJ_XTREAM_LIMPAR: return "Apaga servidor, usuário e senha deste perfil, e os canais somem do Guia. Sair da conta também apaga.";
    case AJ_XTREAM_CONTA: return "O que o servidor Xtream disse da assinatura na última carga do Guia: se está ativa, quando vence e quantas telas estão em uso.";
    case AJ_EPG_PAIS: return "De que país vem a programação dos canais no Guia. Automático escolhe pelo idioma e pelos nomes dos canais (RO:, |RO|…). A grade do próprio provedor Xtream entra sempre que existir.";
    case AJ_FONTE_MANUAL: return "Ao mandar reproduzir, abre a lista de fontes em vez de escolher sozinho. Canal ao vivo não pergunta.";
    case AJ_FONTE_AUTO: return "Melhor fonte: prefere 4K, Dolby Vision e MP4 e confere uma fonte por vez. Primeira da lista: toca a primeira que o addon mandou e não confere nenhuma outra — para quem já filtra e ordena no AIOStreams.";
    case AJ_FONTE_PRIORIDADE: return "Equilíbrio: HDR e Dolby Vision ganham de SDR na mesma resolução ou numa abaixo, e uma fonte que a sua conexão claramente não sustenta desce. Qualidade máxima: a maior resolução permitida, depois Dolby Vision, HDR10+, HDR10 e SDR, sem rebaixar por velocidade. Começar rápido: prefere fontes em cache e arquivos menores, que abrem mais depressa. Sempre rebaixa uma fonte que a conexão medida não sustenta.";
    case AJ_LOGO_APP: return "O símbolo que o Nuvio mostra na abertura e nas telas de entrada. Novo é o play em degradê; Clássico é a TV retrô de sempre. Vale só nesta TV.";
    case AJ_ABERTURA: return "Como o logo some quando o app abre. Padrão para um instante, aproxima e esmaece; Só esmaece não aproxima; Direto abre a Home sem parar. Vale só nesta TV.";
    case AJ_FONTE_HDR: return "Preferir: fontes com HDR ou Dolby Vision vêm na frente. Indiferente: o formato não conta. Evitar: prefere SDR na mesma resolução. O Dolby Vision só entra se estiver ligado em Imagem e som; perfil 5 sem HDR10 fica atrás do HDR10, porque sai com cores erradas fora de TV Dolby Vision. Só vale para a escolha automática.";
    case AJ_FONTE_ORDEM_ADDON: return "Por qualidade: as fontes vêm agrupadas por resolução, da maior para a menor, e do maior arquivo para o menor. Do addon: cada addon na ordem em que você o instalou, e as fontes de cada um na ordem em que ele mandou — para quem já ordena no AIOStreams. A escolha automática não muda.";
    case AJ_FONTE_TEXTO: return "Do Nuvio: o nome do título em cima e os logos de qualidade embaixo. Do addon: o nome e a descrição exatamente como o addon manda — para quem já formata o texto no AIOStreams. Logo do título: a logo do título no lugar do nome escrito.";
    case AJ_FONTE_PRAZO: return "As fontes aparecem na lista assim que cada add-on responde. A escolha automática não espera o mais lento: sai quando já há uma fonte boa ou depois deste tempo. Com uma fonte escolhida antes neste título, o add-on dela é sempre esperado.";
    case AJ_FONTE_TOCAR_CONFERINDO: return "Abre o player com a primeira fonte do automático na hora em que a conferência dela começa, em vez de esperar o resultado. Se a conferência reprovar a fonte, volta a esperar e segue para a próxima, sem baixar a qualidade. Torrent e fonte que precisa ser resolvida antes seguem o caminho de sempre.";
    case AJ_FONTE_AQUECER: return "Ao abrir a página de um título e quando as fontes chegam, abre antes a conexão com o debrid e com os hosts das primeiras fontes, para o Play não gastar esse tempo. Não resolve nenhum link e não cria arquivo no debrid.";
    case AJ_FONTE_CONFERIR_VARIAS: return "Confere até 3 fontes já prontas no debrid ao mesmo tempo, em vez de uma por vez, e toca a melhor que servir. Fontes que ainda não estão prontas seguem uma por vez, sem baixar nada a mais. Só vale com a Fonte automática em Melhor fonte.";
    case AJ_FONTE_PREPARAR: return "Ao abrir a página do título, já escolhe e prepara a primeira fonte do automático, para o Play abrir na hora. Isso adiciona o arquivo ao painel do seu debrid mesmo que você não assista.";
    case AJ_FONTE_REPOR: return "Quantas outras fontes o automático tenta quando a escolhida não abre. Cada tentativa pode adicionar um arquivo na sua conta de debrid.";

    // --- Home
    case AJ_LANDSCAPE: return "Usa a arte deitada (16:9) no lugar do cartaz em pé nas fileiras que têm as duas.";
    case AJ_HERO_CHEIO: return "O destaque do topo ocupa a tela inteira atrás das fileiras, em vez de ficar num bloco.";
    case AJ_HERO_FUNDO: return "De onde vem a arte de fundo do destaque, da página do título e dos cards deitados: catálogo/Cinemeta, IMDb/Metahub, TMDB, Trakt, Apple TV, fanart.tv (com chave) ou Anime (Kitsu/AniList). Automático usa a do catálogo. MDBList fornece notas, não imagens.";
    case AJ_HERO_ARTE_DIF: return "Desligado: card, destaque e página do título mostram a mesma imagem. Ligado: o card fica com a arte do catálogo e o destaque usa outra foto — TMDB vira outro fundo do TMDB; em Automático, ou se a escolhida repetir o card, usa Apple TV, outro fundo do TMDB, fanart.tv, anime ou Trakt.";
    case AJ_HERO_TRAILER: return "Com o foco parado no destaque do topo, o trailer do título toca no lugar da arte, sem som a menos que Som do trailer no destaque esteja ligado. Mover o foco volta para a arte.";
    case AJ_HERO_TRAILER_SOM: return "Ligado: o trailer do destaque do topo toca com som. Desligado: toca sem som.";
    case AJ_DET_TRAILER_SOM: return "Ligado: o trailer que toca sozinho na página do título sai com som. Desligado: toca sem som; OK abre em tela cheia com som.";
    case AJ_HERO_TRAILER_ESPERA: return "Quanto tempo o destaque fica parado num título antes de trocar a arte pelo trailer.";
    case AJ_ADDON_POSTER: return "Com um serviço de pôsteres ligado, o título que veio de um catálogo de addon fica com o pôster que o próprio addon manda, e o serviço só entra nos outros (Continuar assistindo, listas do Trakt, pôster genérico do Cinemeta). Sem o serviço, o pôster já é o do addon.";
    case AJ_ADDON_FUNDO: return "O card deitado, o destaque e a página do título usam o fundo que o addon manda no catálogo, mesmo com outro Background do hero ou com Destaque com outra arte. O addon que não manda fundo cai na fonte escolhida. A arte escolhida à mão em Trocar arte continua valendo mais.";
    case AJ_ADDON_LOGO: return "O logo do título que o addon manda não é trocado pelo logo traduzido do TMDB (Arte localizada) ao abrir o título. O addon que não manda logo continua recebendo o do TMDB.";
    case AJ_LIVETV_RES: return "Quando o canal tem várias fontes (FHD, HD, SD ou as versões do mesmo canal no Xtream), a desta resolução entra primeiro. Se ela não abrir, entra a próxima.";
    case AJ_LIVETV_FORMATO: return "Automático usa o formato que a conta Xtream declara e lembra o que tocou. HLS ou TS pede esse formato primeiro, mesmo quando a conta só declara o outro.";
    case AJ_LIVETV_ESPERA: return "Quanto tempo o canal tem para mostrar a imagem antes de o app passar para a próxima fonte ou mostrar o erro. Automática espera 15 s no Xtream e 25 s nos addons.";
    case AJ_LIVETV_PROXY: return "Nos canais Xtream que chegam como HLS, o app junta os segmentos num fluxo TS contínuo e entrega à TV, que toca TS contínuo e não toca o HLS desses canais. Canal que já é TS contínuo vai direto. Desligado, a TV recebe o HLS do provedor.";
    case AJ_LIVETV_MODO: return "Teste para TVs em que o canal chega mas não aparece. A é o modo de sempre; B não escolhe a faixa de vídeo antes de o canal abrir; C também manda o pedido no formato de transmissão ao vivo. O diagnóstico da Live TV testa os três.";
    case AJ_LIVETV_DIAG: return "Mede a rede até o provedor, lê a conta Xtream e testa vários canais: se tocam, em que formato, com que resolução e em quanto tempo. No fim sugere ajustes da Live TV e pode aplicá-los.";
    case AJ_SELOS_CORES: return "Na lista de fontes, cada selo (4K, HDR, Dolby, codec, serviço) ganha a cor do seu tipo, como no pacote de selos do Nuvio. Desligado, os selos ficam brancos.";
    case AJ_SELOS_PACOTE: return "Qual pacote de selos desenha a lista de fontes. Do Nuvio é o pacote que vem no app; os outros são os pacotes da sua conta (importados no Nuvio web) e os que você adiciona aqui. Vale para este perfil. Se nenhum selo do pacote combinar com a fonte, ela usa os selos do app.";
    case AJ_SELOS_PACOTE_ADD: return "Adiciona um pacote de selos pelo link do JSON dele (no máximo 3 no total, contando os da conta). O pacote entra já escolhido.";
    case AJ_SELOS_PACOTE_REM: return "Tira desta TV o pacote escolhido. Pacote que veio da conta só se remove na conta, no Nuvio web.";
    case AJ_COL_ARTE_CONTA: return "As pastas de coleção que o app já traz com arte própria passam a usar a capa, o fundo e o logo que estão na sua conta (editor de coleções do site). O que a conta não tiver continua com a arte do app.";
    case AJ_HERO_TRANSICAO: return "Deslizar: quando o destaque troca de título, a arte e o texto saem para o lado e o próximo entra colado, como num carrossel. Esmaecer: a arte apaga e a nova aparece no lugar. Com Animações reduzidas a troca é sempre sem movimento.";
    case AJ_FIL_LIMITE: return "Quantas fileiras a Home monta, de 3 a 40. Menos fileiras também significam menos catálogos pedidos pela rede, e não fileiras invisíveis. Mais fileiras usam mais memória e rede: em TV com 1 GB de memória a Home pode ficar lenta ou fechar. Se o app fechar depois de você aumentar, ele volta sozinho ao valor anterior.";
    case AJ_ITENS_FILEIRA: return "Quantos títulos cada fileira da Home mostra antes do Ver tudo. Mais itens usam mais memória: em TV com 1 GB de memória a Home pode ficar mais lenta ou fechar. Se o app fechar depois de você aumentar, ele volta sozinho ao valor anterior. Aumentar vale na próxima vez que o app abrir.";
    case AJ_FIL_ORDEM: return "Abre a lista de fileiras para reordenar, ligar, desligar e escolher o card de cada uma. É lá que dá para ver de onde cada fileira vem.";
    case AJ_RAIL: return "A barra de navegação da esquerda fica sempre aberta, ou recolhida até você ir até ela.";
    case AJ_RAIL_MODERNA: return "Troca a barra lateral pela versão nova, com ícones maiores. Ela ignora a escolha entre recolhida e fixa.";
    case AJ_RAIL_BLUR: return "Desfoca a arte atrás da barra lateral moderna em vez de usar um fundo sólido.";
    case AJ_HERO: return "O bloco grande no topo da Home, com a arte e o nome de um título em destaque.";
    case AJ_HERO_CATALOGOS: return "De onde vêm os títulos do destaque: os primeiros do catálogo, um sorteio, ou uma fileira da Home. OK troca.";
    case AJ_PS_FUNDO: return "Fundo da tela de escolha de perfil. Filmes: os cartazes do que cada perfil assistiu, numa parede inclinada. Luz: preto com uma luz na cor do perfil. Projetor: sala de cinema antes da sessão. Arte do perfil: o fundo escolhido ao editar o perfil no app Nuvio; sem ele, o avatar desfocado. Ou as listras do login.";
    case AJ_DESCOBRIR: return "Onde fica a tela Descobrir: junto da Busca, como item próprio na barra lateral, ou em lugar nenhum.";
    case AJ_SELO_VISTO: return "Um check pequeno no canto de cima do pôster dos títulos que você já assistiu, pelo Trakt, pela conta ou marcados nesta TV.";
    case AJ_ROTULOS: return "Escreve o nome do título abaixo do cartaz. A maior parte da arte já traz o nome impresso.";
    case AJ_NOME_ADDON: return "Acrescenta o nome do addon ao título da fileira, para separar dois catálogos com o mesmo nome.";
    case AJ_SUFIXO_TIPO: return "Acrescenta \"Filme\" ou \"Série\" ao título da fileira, para separar as duas versões do mesmo catálogo.";
    case AJ_OCULTAR_NLANC: return "Esconde das fileiras o que ainda não estreou. Título sem fonte nenhuma ocupa lugar e não abre.";
    case AJ_NOTAS_HOME: return "Mostra a nota do IMDb no canto dos cartazes da Home.";
    case AJ_GRAD_CLASSICO: return "Volta ao degradê antigo sob o cartaz em foco, no lugar do realce atual.";

    // --- Continuar assistindo
    case AJ_CW_LIGADO: return "A fileira de retomada, com o que você deixou pela metade e o próximo episódio das séries que acompanha.";
    case AJ_CW_OK: return "O que o OK faz no card da retomada: toca de onde parou, ou abre a página do título. Segurar OK abre o menu nos dois casos.";
    case AJ_CW_FONTE: return "De onde vem a fileira de retomada. \"Todas as fontes\" usa a conta Nuvio e completa com o Trakt e, se estiver vinculado, com o Simkl.";
    case AJ_CW_ESTILO: return "A forma do card da retomada: quadrado com a arte, deitado largo, ou o cartaz em pé.";
    case AJ_CW_THUMB: return "Usa a imagem do próprio episódio no card, em vez da arte da série.";
    case AJ_CW_BLUR_PROX: case AJ_DET_BLUR_NAO_VISTOS: return "Oculta detalhes da miniatura para evitar spoilers de episódios ainda não assistidos.";
    case AJ_CW_FURTHEST: return "Escolhe o próximo episódio a partir do mais avançado marcado como assistido.";
    case AJ_CW_PROXIMO: return "Ligado, a fileira mostra também o próximo episódio das séries que você acompanha. Desligado, mostra só o que você deixou pela metade.";
    case AJ_CW_NAO_EXIBIDOS: return "A retomada mostra o próximo episódio antes de ir ao ar.";
    case AJ_CW_ORDEM: return "Como a retomada se ordena: pelo mais recente, no estilo dos streamings, ou com os episódios futuros num bloco separado.";
    case AJ_CW_CONCLUIDO: return "A partir de quanto do episódio ele conta como assistido e a retomada passa ao próximo.";

    // --- Pagina de detalhe
    case AJ_DET_TRAILER:
#ifdef __EMSCRIPTEN__
      // Samsung: a tela cheia e muda (trailerfonte_com_som) — a ajuda diz, em
      // vez de o botao prometer um som que nao vem.
      return "Mostra o botão de trailer na tela do título, quando existe um trailer conhecido. Nesta TV o trailer toca sem som.";
#else
      return "Mostra o botão de trailer na tela do título, quando existe um trailer conhecido.";
#endif
    case AJ_DET_SO_CINEMETA: return "Desligado (padrão): a ficha do título vem primeiro do add-on em cujo catálogo ele apareceu, com episódios e ids próprios (Kitsu, Xperience, AIOMetadata…), e o catálogo do Nuvio completa o que faltar (o Cinemeta só entra se o catálogo do Nuvio falhar). Ligado: só o catálogo do Nuvio, sem os add-ons.";
    case AJ_DET_META_EXT: return "Prefere a ficha do addon de metadados à do Cinemeta. Útil quando o seu addon tem sinopse e elenco melhores.";
    case AJ_DET_DATA_CHEIA: return "Escreve a data de estreia por extenso em vez de só o ano.";
    case AJ_DET_VEU: return "Quanto a vinheta escura cobre a arte na tela do título. Cem por cento é o padrão; zero mostra a arte limpa — o texto pode ficar difícil de ler sobre cenas claras.";
    case AJ_DET_TRAILER_AUTO:
#if defined(NV_TPK) && NV_TRAILER_CONTINUA_DETALHE
      // O trailer do cartaz continua na pagina, com som (detail_abrir).
      return "Alguns segundos depois de abrir um título, o trailer toca sem som no lugar da arte de fundo. Se o trailer do cartaz já estava tocando, ele continua na página, com som. Rolar a página ou sair dela volta para a arte.";
#else
      return "Alguns segundos depois de abrir um título, o trailer toca sem som no lugar da arte de fundo. Rolar a página ou sair dela volta para a arte.";
#endif
    case AJ_TRAILER_QUAL: return "Definição do vídeo do trailer. Máxima usa a maior que existir para o título; as outras são um teto, para conexões mais lentas.";
    case AJ_TRAILER_ASPECTO: return "Quanto o trailer é ampliado para encher a tela. Zoom cinema tira a tarja preta de um trailer de cinema; Original mostra o quadro inteiro, com tarja.";
    case AJ_TRAILER_FONTE:
      // O que cada TV toca (trailerfonte.c, existe): a ajuda nomeia a fonte que
      // falta AQUI, senao escolher IMDb na Samsung e ficar sem trailer parece
      // defeito.
#ifdef __EMSCRIPTEN__
      return "De onde vem o trailer da tela do título e do destaque. Automático tenta a Apple TV, depois o IMDb e, sem os dois, o YouTube (só na tela do título); uma fonte escolhida é a única tentada. Nesta TV o trailer toca sempre sem som.";
#elif defined(NV_TPK)
      // .tpk: a Apple toca sem audio (trailerapple.c, varianteMidia), entao o
      // botao Trailer em Automatico prefere o IMDb (trailerfonte_ordem_cheia).
#if NV_TRAILER_CONTINUA_DETALHE
      return "De onde vem o trailer da tela do título e do destaque. Automático tenta a Apple TV e, sem ela, o IMDb; no destaque e no botão de trailer o IMDb vem primeiro, porque o da Apple toca sem som nesta TV. Uma fonte escolhida é a única tentada. O YouTube não toca nesta TV.";
#else
      return "De onde vem o trailer da tela do título e do destaque. Automático tenta a Apple TV e, sem ela, o IMDb; no botão de trailer, que toca com som, o IMDb vem primeiro, porque o da Apple toca sem som nesta TV. Uma fonte escolhida é a única tentada. O YouTube não toca nesta TV.";
#endif
#else
      return "De onde vem o trailer da tela do título e do destaque. Automático tenta a Apple TV e, sem ela, o IMDb; uma fonte escolhida é a única tentada. O YouTube não toca nesta TV.";
#endif

    // --- Posteres e cards
    case AJ_EXPANDIR: return "O cartaz em foco cresce e abre a arte deitada atrás dele depois de um instante parado.";
    case AJ_EXPANDIR_ATRASO: return "Quanto tempo o foco precisa ficar parado antes de o cartaz expandir.";
    case AJ_FOCO_TRAILER: return "Com o foco parado num cartaz, o trailer toca sem som no lugar da arte do destaque, depois do mesmo tempo de espera da expansão.";
    case AJ_NAV_RAPIDA: return "Andar de lado numa fileira não espera a animação terminar. Serve para controle que repete rápido.";
    case AJ_BORDA_FOCO: return "O anel colorido que marca o cartaz em foco na Home. Desligado, o foco fica só pelo tamanho do cartaz.";
    case AJ_PROF: return "Dá relevo aos cartazes: borda iluminada e um reflexo que acompanha o foco.";
    case AJ_PROF_BORDA: return "Quanto a borda do cartaz em foco acende.";
    case AJ_PROF_BRILHO: return "Quanto o reflexo passa por cima da arte do cartaz em foco.";
    case AJ_PROF_COBERTURA: return "Que parte da volta do cartaz a borda iluminada percorre.";
    case AJ_PROF_POSTERS: case AJ_PROF_CW: case AJ_PROF_EPS:
    case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS:
      return "Onde o relevo é aplicado. Desligar em alguns lugares alivia o desenho sem perder o efeito onde ele importa.";
    case AJ_LARGURA_DP: return "Ajusta a largura dos pôsteres nas fileiras que usam o tamanho personalizável.";
    case AJ_RAIO_DP: return "Controla o arredondamento dos cantos dos pôsteres.";
    case AJ_QUALIDADE_IMG: return "Quanto de pixel a arte carrega. Alta pede a versão grande de cada imagem e gasta mais memória; Baixa pede a menor, carrega antes e cabe em TV com pouca RAM.";

    // --- Interface e conta
    case AJ_IDIOMA: return "Idioma de toda a interface. Automático segue a sua conta e, sem ela, o idioma da TV. Não muda o idioma das legendas nem do áudio.";
    case AJ_GPU_EFEITOS: return "Automático mede a TV nos primeiros segundos e, se ela não der conta, tira os efeitos mais pesados. Completos mantém tudo; Leves tira desfoque e brilho para deixar a navegação mais lisa.";
    case AJ_FONTE_UI: return "Altera a tipografia dos menus. A fonte das legendas é escolhida separadamente no player.";
    case AJ_TAMANHO_UI: return "Aumenta os controles do player, os painéis e os avisos. Os Ajustes têm um tamanho próprio.";
    case AJ_TAMANHO_AJUSTES: return i18n("Muda só o tamanho dos Ajustes nesta TV. O padrão é 80%.");
    case AJ_LAYOUT_AJUSTES: return "Painel mostra uma prévia ao lado das opções. Lista põe as categorias e as opções uma embaixo da outra, com a explicação logo abaixo da linha em foco.";
    case AJ_TEMA: return "Cor do botão em foco e das marcas de estado. Os claros levam texto escuro, os profundos texto branco — sempre a 4,5:1 ou mais.";
    case AJ_VIDRO_OPAC: return "Teste: quanto os painéis de vidro deixam a arte aparecer. O valor do meio é o de hoje; menos é mais transparente, mais é mais escuro e fácil de ler.";
    case AJ_VIDRO_FOSCO: return "Teste: põe a arte borrada atrás de cada painel de vidro, como um vidro jateado. Onde não há arte borrada, o vidro fica como sempre.";
    case AJ_FUNDO: return "O que fica atrás dos painéis. Arte: a imagem do título, nítida. Arte borrada: a imagem do título desfocada. Frost: superfície fosca tingida pela cor de destaque. Com a interface de vidro desligada os painéis são opacos e o efeito é pequeno.";
    case AJ_P2P_LIGADO:
      // Com o motor neste pacote o aviso diz o que a TV passa a fazer (baixar
      // e COMPARTILHAR), o teto de disco e o risco legal.
      if (p2pmotor_disponivel())
        return "Experimental. Deixa escolher, na lista de fontes, torrents que o addon manda sem link (P2P). Sem endereço de servidor, a própria TV baixa o torrent e compartilha pedaços com outras pessoas enquanto toca, guardando o que baixa no armazenamento (nunca mais que metade do espaço livre) e apagando tudo ao fechar o player. O automático nunca escolhe P2P. Baixar ou compartilhar conteúdo sem autorização pode ser ilegal no seu país: a responsabilidade é sua.";
      return "Experimental. Deixa escolher, na lista de fontes, torrents que o addon manda sem link (P2P), tocando-os por um servidor de streaming do Stremio que você roda na sua rede (PC, NAS ou Docker). A TV não baixa nada. O automático nunca escolhe P2P. Sem servidor na rede, deixe desligado.";
    case AJ_P2P_URL:
      if (p2pmotor_disponivel())
        return "Opcional. Vazio, a TV baixa sozinha. Com o IP e a porta de um servidor de streaming do Stremio na sua rede (por exemplo 192.168.1.5:11470), quem baixa é ele e a TV só toca.";
      return "IP e porta do servidor de streaming do Stremio na sua rede, por exemplo 192.168.1.5:11470. Em Docker: docker run -p 11470:11470 stremio/server.";
    case AJ_JF_LIGADO: return "Experimental. Mostra na Home os filmes e séries do seu servidor Jellyfin/Emby/Plex e toca por ele. O token fica só nesta TV e neste perfil; a senha nunca é guardada.";
    case AJ_EM_SERVIDOR: return "IP e porta do servidor, como 192.168.1.5:8096, ou o endereço com https://";
    case AJ_EM_ENTRAR: return "Pede usuário e senha do Emby. A senha só vai ao servidor e nunca é guardada; fica só um token nesta TV e neste perfil.";
    case AJ_EM_SAIR: return "Pede o OK duas vezes. Encerra a sessão no servidor e apaga o token desta TV.";
    case AJ_PX_ENTRAR: return "Mostra um código de 4 letras: digite em plex.tv/link no celular. Nada é digitado nesta TV; só um token fica aqui.";
    case AJ_PX_SERVIDOR: return "OK passa para o próximo servidor da sua conta Plex.";
    case AJ_PX_SAIR: return "Pede o OK duas vezes. Apaga os tokens desta TV; o aparelho continua listado em plex.tv/devices até você remover.";
    case AJ_JF_SERVIDOR: return "IP e porta do servidor, como 192.168.1.5:8096, ou o endereço com https://";
    case AJ_JF_ENTRAR: return "Usa o Quick Connect se o servidor permitir: aprove o código em outro app do Jellyfin. Senão, pede usuário e senha.";
    case AJ_JF_SAIR: return "Pede o OK duas vezes. Encerra a sessão no servidor e apaga o token desta TV.";
    case AJ_DEBRID_AD: return "Sua chave de API do AllDebrid (alldebrid.com/apikeys). Com ela os torrents das fontes tocam pelo AllDebrid, que precisa de conta premium. Fica só nesta TV, aparece mascarada e vale no lugar da que vier da conta Nuvio.";
    case AJ_DEBRID_AD_TESTAR: return "Pergunta ao AllDebrid se a chave vale e até quando a conta é premium. Não mostra seu usuário nem e-mail.";
    case AJ_DEBRID_RD: return "Chave de API do Real-Debrid (real-debrid.com/apitoken). Só precisa se a sua conta Nuvio não a traz. Fica só nesta TV e aparece mascarada.";
    case AJ_DEBRID_TB: return "Chave de API do TorBox. Só precisa se a sua conta Nuvio não a traz. Fica só nesta TV e aparece mascarada.";
    case AJ_DEBRID_PM: return "Chave de API do Premiumize. Só precisa se a sua conta Nuvio não a traz. Fica só nesta TV e aparece mascarada.";
    case AJ_PERFIL_PESQ: return "Desligado por padrão. Ligado, outras pessoas do Nuvio podem te achar pelo apelido e ver o que você escolher mostrar: bio, gêneros favoritos, foto e o que assistiu recentemente. Nunca aparecem e-mail, conta, addons nem aparelho. Desligar apaga o perfil do servidor na hora.";
    case AJ_PERFIL_EDITAR: return "Apelido, bio, gêneros e o que mostrar no perfil; a atividade compartilhada só com amigos (desligada por padrão); pedidos de amizade recebidos e a lista de bloqueados.";
    case AJ_P2P_LIMITE: return "Quanto do armazenamento desta TV o P2P pode ocupar enquanto toca. Automático usa até metade do espaço livre (no máximo 1,5 GB). Um valor fixo sempre deixa 512 MB livres. O vídeo inteiro fica guardado até fechar o player: um arquivo maior que o limite para no meio.";
    case AJ_P2P_TESTAR: return "Pergunta ao servidor se ele responde e qual a versão. Funciona mesmo com o P2P desligado, para conferir o endereço antes de ligar.";
    case AJ_POSTER_PROV: return "Troca os cartazes retrato por um pronto de um serviço externo, com notas, selos 4K/HDR e faixa Top 10 no próprio cartaz. SpatialPosters (instância pública ou a sua), RPDB (com chave) ou um modelo de URL seu. Só cartazes de card: o destaque e os fundos não mudam. Se o serviço não responde, volta ao cartaz normal.";
    case AJ_POSTER_INST: return "Endereço da instância do SpatialPosters. Vazio usa a pública (spatial-posters.vercel.app), que é gratuita e compartilhada; para muitos cartazes, rode a sua com Docker.";
    case AJ_POSTER_TOKEN: return "Opcional. Token de configuração do SpatialPosters, ou o endereço do manifest colado inteiro. Um token completo costuma ter mais de 500 letras e não cabe aqui; prefira os parâmetros curtos ao lado ou os padrões da sua instância.";
    case AJ_POSTER_EXTRA: return "Opcional. Ajustes curtos do cartaz, no formato do SpatialPosters, por exemplo bs=vetro&side=right (selo de vidro, faixa à direita). O idioma da interface já vai sozinho.";
    case AJ_POSTER_CHAVE: return "Sua chave do RPDB (ratingposterdb.com). Fica só nesta TV e nunca aparece nos registros.";
    case AJ_POSTER_MODELO: return "Endereço com {imdb}, {tmdb}, {type} (movie ou series) e {tipo_tmdb} (movie ou tv), por exemplo https://meu.servidor/{type}/{imdb}.jpg. Quem não tiver o dado que o modelo pede fica com o cartaz normal.";
    case AJ_POSTER_TESTAR: return "Baixa o cartaz de um filme conhecido com a configuração atual e mostra se deu certo. O primeiro cartaz de cada título é montado no servidor e pode levar alguns segundos.";
    case AJ_HOME_LAYOUT: return "Moderna: destaque atrás das fileiras. Padrão: destaque num banner no topo. Dinâmica: estilo Apple TV, com destaques grandes e Top 10.";
    case AJ_VIDRO: return "Painéis, botões e menus viram ilhas translúcidas que deixam a arte aparecer. Desligado, as mesmas ilhas ficam opacas. Só muda o visual; nada muda de lugar.";
    case AJ_ADDONS_PRINCIPAL: return "Os outros perfis desta conta usam os addons do perfil principal. Desligado, cada perfil usa os seus — a não ser que a conta já diga para usar os do principal.";
    case AJ_VIDRO_CONTORNO: return "O contorno das linhas e dos cartões, inclusive o do foco. Desligado, o item em foco é marcado só por um fundo mais claro na cor de destaque.";
    case AJ_COR_LOGO: return "Com um tema dinâmico, a cor sai do logo do título em vez da arte de fundo. Logo branco ou preto usa a arte.";
    case AJ_MANTER_VIDEO: return "Ao sair de um filme para a home, o vídeo fica pausado e carregado por até 2 minutos, e o Retomar volta na hora. Custa caro: a memória do vídeo e o player ficam presos, o trailer da home não toca nesse tempo e TVs mais fracas podem ficar lentas. Desligado (padrão), o player é liberado ao sair e o Retomar reabre direto pela fonte que estava tocando, sem procurar nos add-ons.";
    case AJ_ESMAECER: return "Quanto tempo sem apertar nada até a tela escurecer e entrar o descanso. Qualquer tecla acorda (a primeira só acorda, não faz nada). Nunca com o filme tocando; com ele pausado, a tela só escurece.";
    case AJ_DESCANSO_ESTILO: return "Vitrine mostra títulos do catálogo em tela cheia, um de cada vez; OK abre o que está na tela. Relógio mostra a hora grande e a próxima estreia da Agenda. Só escurecer apaga a tela quase toda, com um relógio pequeno. Nos três nada fica parado no mesmo lugar.";
    case AJ_DESCANSO_FONTE: return "De onde a vitrine tira os títulos: o catálogo inteiro, ou só o que está na sua lista e em Continuar assistindo.";
    case AJ_PROPORCAO_PADRAO: return "Como a imagem do filme ocupa a tela quando o vídeo começa. Último usado mantém o que você escolheu por último no player. Os outros modos valem em todo vídeo, e você ainda pode trocar durante a reprodução com a tecla 0 ou o botão Proporção. Em TVs que não recortam a imagem, os modos de zoom caem para Original.";
    case AJ_BRILHO_PLAYER: return "Escurece os controles, o título e a barra do player (as legendas não mudam). Com o filme tocando e a barra parada, ela ainda baixa um degrau até você apertar uma tecla.";
    case AJ_BUSCA_CINEMETA: return "Só a busca. Ligado (padrão): consulta o catálogo do Nuvio e, se ele falhar, o Cinemeta; um add-on Cinemeta instalado também responde. Desligado: o Cinemeta fica de fora da busca. Não mexe nas fileiras da Home (use Fileiras da Home) nem na ficha do título.";
    case AJ_BUSCA_NUVIO: return "Onde ficam os resultados do catálogo do Nuvio (os grupos Filmes e Séries da busca). Primeiro (padrão): antes dos seus add-ons. Por último: depois deles, para os seus resultados virem na frente. Desligado: a busca não consulta o catálogo do Nuvio. O Continuar assistindo não muda.";
    case AJ_BUSCA_ORIGEM: return "Ligado (padrão): cada grupo de resultados diz de qual fonte veio, como de AIOMetadata. Desligado: só o nome do grupo. Com uma única fonte de busca ativa, a fonte nunca aparece.";
    case AJ_ENQUETES: return "Ligado, o Nuvio pode convidar você a votar numa enquete curta na ilha do relógio. Desligado, nenhuma aparece. A escolha fica na sua conta.";
    case AJ_RELOGIO: return "Desligado, a pílula do relógio não fica na tela em repouso. Os avisos continuam saindo dela: ela aparece só para o aviso e some depois.";
    case AJ_SAIDA_PLAYER: return "Ao sair de um filme ou episódio no meio. Home: o vídeo encolhe até a pílula do relógio, que fica com o título para você retomar (CH+ ou AZUL). Página do título: volta para onde você estava. Só vale com o relógio na tela; terminar o título segue para o próximo episódio como sempre.";
    case AJ_TAM_MAX: return "Na escolha automática, fontes maiores que este tamanho ficam para o fim da fila. Serve para quem tem franquia de internet limitada. Só vale para arquivos com tamanho conhecido; se só houver fontes fora da faixa, a melhor delas ainda toca.";
    case AJ_FONTE_ESCOPO: return "De onde o automático pode tocar: todas as fontes, só os add-ons instalados ou só os plugins ligados. Fica no seu perfil.";
    case AJ_FONTE_ADDONS_PERM: return "Os add-ons que o automático usa primeiro, como os do seu idioma. Nenhum marcado = todos. A lista de fontes continua mostrando tudo.";
    case AJ_FONTE_PLUGINS_PERM: return "Os plugins que o automático usa primeiro. Nenhum marcado = todos os plugins ligados.";
    case AJ_FONTE_OUTROS: return "Ligado: sem fonte nos permitidos dentro da espera, o automático usa os outros. Desligado: só os permitidos; sem fonte neles, a lista de fontes abre.";
    case AJ_FONTE_REGEX: return "Compara uma regex com o nome, a descrição e o add-on de cada fonte, sem diferenciar maiúsculas. Exigir: só toca o que casa. Preferir: o que casa vem primeiro.";
    case AJ_FONTE_REGEX_PADRAO: return "O padrão da regex, como no Nuvio oficial. Ex.: ESP|Latino|Castellano. Dá para digitar pelo celular. Um padrão inválido é ignorado.";
    case AJ_FONTE_ORDEM: return "A ordem dos add-ons e plugins no automático: suba e desça cada um. Quem fica fora da ordem vem depois de todos. Só vale com a opção Usar a ordem ligada.";
    case AJ_FONTE_ORDEM_USO: return "Não: a escolha de sempre. Para desempatar: na mesma faixa de qualidade e no mesmo grupo, vence o add-on mais cedo na ordem, sem fazer esperar. Ordem estrita: toma a melhor fonte válida do primeiro add-on da ordem que tiver uma, respeitando teto, tamanho e regex, mesmo que um add-on mais abaixo tenha qualidade maior; decide assim que ele responde. Fontes acima do teto ficam por último.";
    case AJ_FONTE_REGEX_MODELO: return "Padrões prontos: idiomas como os add-ons escrevem, áudio MULTI ou DUAL, 4K e Remux. Escolher um troca o padrão da regex.";
    case AJ_ESPACO_FILEIRAS: return "O espaço vertical entre uma fileira da Home e a seguinte. Com menos cabem mais fileiras na tela, com mais a Home fica mais arejada.";
    case AJ_ESPACO_TITULOS: return "O espaço horizontal entre os cartazes de uma fileira da Home.";
    case AJ_TAM_MIN: return "Na escolha automática, fontes menores que este tamanho ficam para o fim da fila. Se for maior que o tamanho máximo, o mínimo é ignorado. Só vale para arquivos com tamanho conhecido; se só houver fontes fora da faixa, a melhor delas ainda toca.";
    case AJ_DTS_AC3: return "Como o áudio DTS é entregue à TV quando o app o converte (só na LG). Estéreo (AAC) é o modo provado e serve qualquer TV. 5.1 (Dolby Digital) mantém o som surround para barra de som e receiver por ARC/eARC; vale a partir do próximo vídeo.";
    case AJ_PS_CONTINUAR: return "Ligado, cada perfil mostra na escolha de perfil o cartão com o que ele estava assistindo. Desligado, a tela mostra só os perfis.";
    case AJ_HIST_CONTA: return "Ligado (padrão): o que você assiste neste perfil sobe para a conta Nuvio, com o progresso e os vistos. Desligado: nada disso é enviado à conta; o Trakt e o Simkl vinculados a este perfil continuam recebendo. Vale só para este perfil.";
    case AJ_SOCIAL: return "Ligado (padrão): amigos, atividade deles e recomendações. Desligado: tudo isso some, com a fileira de amigos da Home, e o app para de falar com o serviço social. Salvos e Avisos continuam. Vale só para este perfil.";
    case AJ_DV_MKV: return "Arquivos MKV em Dolby Vision perfil 5 ou 8 tocam com o Dolby Vision ligado, em vez de HDR10. O áudio Dolby Digital e Dolby Digital Plus vai direto para a TV. Se a conexão não acompanhar, o vídeo continua em HDR10. Só em TVs LG com webOS 4 ou mais novo.";
    case AJ_CW_RETIDO_TAMBEM: return "Quando o título está na ilha ou em Retomar agora, mostra também na fileira Continuar assistindo.";
    case AJ_PLR_CLASSIF: return "Ligado (padrão): no começo do filme, a ilha mostra a classificação indicativa e os avisos do guia parental (violência, nudez, palavrões). Desligado: o player não mostra nada disso.";
    case AJ_RELOGIO_12H: return "Como a hora aparece no relógio, na tela de descanso, no fim do filme e no guia de TV: 18:30 ou 6:30 PM.";
    case AJ_RELOGIO_POS: return "Em que canto de cima fica a pílula do relógio e dos avisos. Automática fica à direita, em qualquer layout. Esquerda no layout Dinâmica fica ao lado da pílula do menu.";
    case AJ_AVANCADAS: return "Mostra, em todas as categorias, as opções técnicas marcadas como Avançado. Vale só para esta TV.";
    case AJ_LOGO_TRAILER: return "Para TVs OLED: não deixa a logo parada na tela enquanto o trailer toca.";
    case AJ_TRAILER_ZOOM_TPK: return "Amplia a imagem do trailer e libera os modos de recorte e zoom da proporção no player; em algumas TVs Samsung pode deixar a tela preta ou mostrar a tela inicial da TV.";
    case AJ_CACHE_SEEK: return "Guarda no disco o trecho já baixado do vídeo, para voltar sem baixar de novo. Apagado ao fechar o player.";
    case AJ_ANIM: return "Use Reduzidas para movimentos mais discretos ao navegar pela interface.";
    case AJ_RESOLUCAO: return "Automática começa em 1080p e testa o 4K sozinha: só usa 4K se a tela for 4K e a TV aguentar com folga; se travar depois, volta para 1080p. Nunca baixa para 720p. Escolher 4K força 4K (se a TV não aguentar, o app volta para 1080p e avisa). 720p desenha em 1280x720 e amplia: mais leve, com texto mais suave. Reinicie o app depois de mudar. O vídeo não muda.";
    case AJ_PERFIL_ATIVO: return "Perfil em uso nesta TV. Trocar de perfil é feito na tela de perfis, ao abrir o app.";
    case AJ_SYNC: return "Estado da última troca de dados com a sua conta: addons, progresso, coleções e preferências.";
    case AJ_ADDONS: return "Abre a lista de addons da sua conta, para ligar e desligar cada um nesta TV.";
    case AJ_PLUGINS: return "Scrapers em JavaScript dos repositórios de plugins do Nuvio, como mais uma fonte na lista, ao lado dos add-ons. Os repositórios vêm da sua conta. Desligado por padrão.";
    case AJ_TRAKT: return "Conecta a sua conta do Trakt para marcar o que assistiu e usar a sua lista.";
    case AJ_SIMKL: return "Conecta a sua conta do Simkl, uma alternativa ao Trakt para acompanhar séries.";
    case AJ_DISCORD: return "Mostra no seu perfil do Discord o que você está assistindo, com o cartaz e o tempo. Só sai alguma coisa enquanto um vídeo toca neste perfil.";
    case AJ_SAIR: return "Sai da conta nesta TV e apaga daqui a sessão, os addons e o progresso guardados.";
    case AJ_ESPACO: return "Uso atual de memória pelo cache de imagens, não espaço ocupado no armazenamento da TV.";
    case AJ_TEX_MB: return "Quanta memória o cache de imagens pode usar. Automático escolhe pela RAM da TV. Um valor acima do que esta TV suporta é reduzido ao máximo dela — o painel ao lado mostra o teto em vigor.";
    case AJ_VERSAO_I: return "Versão do aplicativo. Esta informação não pode ser alterada.";
    case AJ_ATUALIZAR:
      return atualizacao_nova()[0]
        ? "Abre o cartão da versão nova, com o que mudou e o botão de instalar."
        : "Procura agora uma versão nova do Nuvio. Se houver, abre o cartão com o que mudou e o botão de instalar. O app também confere sozinho a cada 6 horas.";
    case AJ_VER_REGISTRO: return "Abre o registro do app por cima desta tela, ao vivo: o mesmo painel do botão vermelho do controle, para quem não tem esse botão.";
    case AJ_MEDIDOR: return "Mostra quadros por segundo, o pior quadro e a memória dentro da ilha do relógio, atualizados a cada 3 s. Mínimo fica na linha da hora, Menor ganha uma segunda linha e Grande abre o painel completo. Quando a ilha mostra um aviso, o medidor se recolhe e volta depois.";
    case AJ_GUIA: return "O que o Nuvio faz, em 12 capítulos. Cada recurso diz onde fica e tem um atalho para ele.";
    case AJ_NOVIDADES20: return "O tour do que mudou na 2.0, capítulo por capítulo, com o que vale neste aparelho. Abre do começo.";
    case AJ_APOIAR: return "Se o Nuvio te ajuda e você quiser apoiar quem faz o app, aponte a câmera do celular para um dos códigos. É opcional: nada muda no app.";
    case AJ_ENVIAR_LOG: return "Manda os últimos 200 KB do registro desta sessão (sem senhas nem chaves) para quem faz o app. Use quando algo estiver errado agora.";
    case AJ_ENVIO_AUTO: return "Ligado, o app manda o registro sozinho: o da sessão anterior ao abrir e o desta a cada minuto. Sem senhas nem chaves; serve para achar o que trava a Samsung. Desligue quando quiser.";
    case AJ_DIAGNOSTICO: return "Testa manifestos, fontes e artes dos addons, mede os tempos e aplica um perfil seguro de Qualidade ou Desempenho. O teste não marca títulos como assistidos.";
    case AJ_MENU_EXPLORAR: case AJ_MENU_GUIA: case AJ_MENU_AGENDA: case AJ_MENU_PERFIL:
      return "Desligado, o item some da barra lateral. Nada é apagado: ligue de novo para ele voltar.";
    case AJ_VELOCIDADE: return "Mede a velocidade dos seus addons e das fontes nesta TV e diz até quantos GB por filme e por episódio tocam sem travar. Não muda nenhum ajuste.";

    // --- Integracoes
    case AJ_TMDB_LIGADO: return "O TMDB enriquece títulos com sinopse, elenco com foto, ficha técnica e trailers. Desligar corta tudo isso de uma vez.";
    case AJ_TMDB_IDIOMA: return "Idioma dos textos que o TMDB traz (sinopse, títulos). \"Da interface\" segue o idioma do app.";
    case AJ_TMDB_ARTE: return "Prefere pôster e fundo traduzidos pelo TMDB quando o título tem arte no seu idioma.";
    case AJ_TMDB_BASICO: return "Usa título e sinopse do TMDB no lugar dos que vieram no catálogo do addon.";
    case AJ_TMDB_FICHA: return "Preenche a ficha técnica da página do título (status, duração, países).";
    case AJ_TMDB_DATAS: return "Datas de estreia e classificação etária do seu país, pelo TMDB.";
    case AJ_TMDB_ELENCO: return "Fotos do elenco e nomes dos papéis, vindos do TMDB.";
    case AJ_TMDB_PROD: return "Lista as produtoras na página do título; tocar num logo abre os títulos dela.";
    case AJ_TMDB_REDES: return "Lista as redes (HBO, Netflix…) na página da série; tocar num logo abre os títulos dela.";
    case AJ_TMDB_EPS: return "Busca dados de episódios no TMDB para complementar os que vêm do addon.";
    case AJ_TMDB_TRAILERS: return "A fileira de trailers da página do título. Desligue para esconder os cards.";
    case AJ_TMDB_MAIS: return "A aba \"Mais como este\" passa a usar as recomendações do TMDB.";
    case AJ_TMDB_COL: return "A aba de coleção/saga (as outras partes da franquia) na página do filme.";
    case AJ_TMDB_CW: return "Usa o TMDB para preencher os cartazes da fileira de retomada.";
    case AJ_MDB_LIGADO: return "O MDBList junta notas de várias fontes na página do título. Desligar esconde a fileira inteira.";
    case AJ_MDB_CHAVE: return "A chave vem da sua conta Nuvio ou do arquivo do pacote. Não dá para digitar nesta TV.";
    case AJ_SEEKR_LIGADO:
      return seekrAjuda(op);
    case AJ_SEEKR_CHAVE: return seekrSalvarFalhou ? seekrAjuda(op) : "Sua chave pessoal do Seekr, gratuita na prévia em seekr.tv. Fica só nesta TV e aparece mascarada.";
    case AJ_SEEKR_FITA: return "Mostra o quadro anterior e o seguinte ao lado da miniatura, com o tempo de cada um. Deixa claro que há um quadro a cada 10 segundos.";
    case AJ_SEEKR_AJUSTE: return "Use quando a miniatura mostra sempre a cena de alguns segundos antes ou depois. Acontece quando a sua versão do título é diferente da usada pelo Seekr (outro corte, abertura mais longa). Vale para todos os títulos; volte a 0 ao trocar de filme.";
    case AJ_SEEKR_TESTAR: return seekrAjuda(op);
    case AJ_FANART_CHAVE: return "Sua chave pessoal do fanart.tv, gratuita em fanart.tv/get-an-api-key. Com ela a fonte fanart.tv entra no Background do hero. Fica só nesta TV e aparece mascarada.";
    case AJ_MDB_TRAKT: case AJ_MDB_IMDB: case AJ_MDB_TMDB:
    case AJ_MDB_LETTER: case AJ_MDB_TOMATES: case AJ_MDB_AUDIENCIA:
    case AJ_MDB_META: case AJ_MDB_MAL:
      return "Mostra ou esconde esta fonte na fileira de notas da página do título.";
    case AJ_NT_IMDB: case AJ_NT_TOMATES: case AJ_NT_AUDIENCIA: case AJ_NT_META:
    case AJ_NT_METAUSER: case AJ_NT_TRAKT: case AJ_NT_TMDB: case AJ_NT_LETTER:
    case AJ_NT_MAL: case AJ_NT_EBERT: case AJ_NT_SCORE:
      return "Mostra esta nota na linha do título, com a marca e a escala do próprio site. Se a linha não couber, saem primeiro as menos importantes. A aba de notas continua mostrando todas.";
    default: return "Use as setas laterais para escolher. A preferência é aplicada ao alterar o valor.";
  }
}

// #410B: aviso primeiro para ficar visivel tambem nas ajudas compactas.
// Traduz cada frase antes de juntar: i18n nao encontra uma chave composta.
static const char *ajudaOpcao(int op) {
  const char *base = ajudaOpcaoBase(op);
  static char b[2048];
  if (gpun_efeitos_automaticos() &&
      (op == AJ_VIDRO || op == AJ_VIDRO_CONTORNO || op == AJ_VIDRO_OPAC ||
       op == AJ_VIDRO_FOSCO || (op >= AJ_PROF && op <= AJ_PROF_TRAILERS))) {
    const char *aviso = gpun_nivel() >= 2
      ? "Efeitos visuais está em mínimos nesta TV; a opção tem pouco efeito. Ajuste em Esta TV › Efeitos visuais."
      : "Efeitos visuais está em leves nesta TV; a opção tem pouco efeito. Ajuste em Esta TV › Efeitos visuais.";
    snprintf(b, sizeof b, "%s\n%s", i18n(aviso), i18n(base));
    return b;
  }
  return base;
}

// O QUE MUDA NA PRATICA quando esta opcao muda. NULL quando nao ha nada
// honesto a dizer — inventar uma consequencia para cada linha encheria a tela
// de texto e ensinaria a pessoa a nao ler nenhum.
//
// So entram as consequencias que a pessoa NAO adivinha olhando a linha: custo
// de rede, escopo (esta TV x a conta), e dependencia entre opcoes.
static const char *efeitoOpcao(int op) {
  if (inativa(op)) return NULL;
  // A RESPOSTA DE "Procurar atualização", por extenso (a linha so tem espaco
  // para a versao curta).
  if (op == AJ_ATUALIZAR && !atualizacao_nova()[0]) {
    static char bufAt[192];   // traduzida (ru, uk, el) passa de 64 bytes
    switch (atualizacao_busca()) {
      case ATUALIZACAO_BUSCA_PROCURANDO: return i18n("Procurando…");
      case ATUALIZACAO_BUSCA_EM_DIA:
        snprintf(bufAt, sizeof bufAt, i18n("Você está na versão mais recente (%s)"), AJ_VERSAO);
        return bufAt;
      case ATUALIZACAO_BUSCA_ERRO: return i18n("Não deu para consultar agora");
      default: return NULL;
    }
  }
  if (op == AJ_ICONE_APP)
#ifdef NV_ANDROID
    return "Também troca o ícone e o banner na tela inicial da TV quando você sai do app. O launcher pode levar alguns segundos para atualizar e pode mudar o app de lugar na lista.";
#else
    return "O ícone na lista de apps da TV é o do pacote instalado e não muda: a troca vale dentro do app.";
#endif
  switch (op) {
    case AJ_FIL_LIMITE:
      return "Vale só nesta TV. Cada fileira a mais é um pedido a mais pela rede quando a Home monta.";
    case AJ_FIL_ORDEM:
      return "Vale só nesta TV: não altera a Home dos seus outros aparelhos.";
    case AJ_CW_OK:
      return "Vale só nesta TV: não altera a Home dos seus outros aparelhos.";
    case AJ_CW_PROXIMO:
      return "Vale para este perfil. Ao mudar, a fileira é remontada na hora.";
    case AJ_CW_FONTE:
      // SEM VINCULO, "Simkl" e uma fileira vazia. Dizer isso aqui, na linha
      // onde a escolha e feita, e o que impede a Home de so perder a fileira
      // sem explicacao. Mesma frase de simkl.h, que as outras telas usam.
      if (valor[op] == AJ_CWF_SIMKL && !simklauth_token()[0])
        return "Vincule o Simkl em Ajustes: sem o vínculo, a fileira fica vazia.";
      return "Vale para este perfil. Ao mudar, a fileira é remontada na hora.";
    case AJ_IDIOMA:
      return "Ao mudar, as fileiras são remontadas para os títulos saírem no idioma novo.";
    case AJ_TRAILER_ZOOM_TPK:
      return "Vale a partir do próximo vídeo. Se a tela ficar preta ou aparecer a tela inicial da TV, desligue.";
    case AJ_CACHE_SEEK:
      return "Vale a partir do próximo vídeo. Sem espaço livre, o cache fica menor ou desligado.";
    case AJ_LEG_LINGUA: case AJ_LEG_LINGUA2:
      return "Se um título já estiver aberto, a busca de legendas é refeita um instante depois.";
    case AJ_PROF:
      return "É o ajuste mais caro desta tela para a TV desenhar. Desligue se a rolagem engasgar.";
    case AJ_STALKER_LIMPAR: case AJ_XTREAM_LIMPAR:
      return "Pede o OK duas vezes. Para voltar a usar é preciso digitar tudo de novo.";
    case AJ_SAIR:
      return "Pede o OK duas vezes. Para voltar é preciso entrar de novo pelo QR.";
    case AJ_SALVOS_DEST:
      if (valor[op] == AJ_SALVOS_SIMKL && !simklauth_token()[0])
        return "Vincule o Simkl em Ajustes: sem o vínculo, o + guarda só na lista desta TV.";
      return "A lista desta TV recebe o título em todos os casos. Isto decide se ele também vai para o Trakt ou para o Simkl.";
    case AJ_TRAKT: case AJ_SIMKL:
      return (op == AJ_TRAKT ? traktauth_estado() == TRA_LIGADO : simklauth_estado() == SMK_LIGADO)
        ? "OK abre o vínculo de novo, com QR e código. As setas laterais não fazem nada nesta linha."
        : "OK abre o vínculo, com QR e código. As setas laterais não fazem nada nesta linha.";
    case AJ_ADDONS: case AJ_DISCORD: case AJ_PLUGINS:
      return "OK abre. As setas laterais não fazem nada nesta linha.";
    // 2.0.3 (M2): o "O que muda" das linhas do mockup v3 e das que dependem
    // de outra (a pessoa liga e nao ve nada: #232, #177, #229).
    case AJ_QUALIDADE:
      return "A escolha automática fica com fontes até esta resolução. Automática não põe limite.";
    case AJ_LEG_FORCADA:
      return "Ao abrir um vídeo com áudio no seu idioma, a legenda forçada é escolhida; sem ela, nenhuma.";
    case AJ_FONTE_REGEX:
      return "Preferir sobe para o topo as fontes que casam com o padrão; Exigir esconde as outras.";
    case AJ_FONTE_MANUAL:
      return "Ligado, a lista de fontes abre a cada play. Desligado, o Nuvio escolhe sozinho com as regras logo abaixo.";
    case AJ_CW_THUMB:
      return "Desfocar o próximo episódio só funciona com esta opção ligada.";
    case AJ_NT_TOMATES: case AJ_NT_AUDIENCIA: case AJ_NT_META: case AJ_NT_METAUSER:
    case AJ_NT_TMDB: case AJ_NT_LETTER: case AJ_NT_MAL: case AJ_NT_EBERT: case AJ_NT_SCORE:
      return "Esta nota vem do MDBList: ligar aqui liga também a consulta ao MDBList.";
    case AJ_HERO_TRAILER:
      return "O som do trailer no destaque é uma opção à parte, em Som e imagem.";
    default: return NULL;
  }
}

// A FRASE DE BENEFICIO (2.0.3, M2): uma linha sob o rotulo da linha em foco,
// "o que isto faz por voce" (mockup v3, tela 02). Onde a primeira frase da
// ajuda ja diz isso, ela e usada (ja traduzida); onde a ajuda abre listando
// valores ("Equilibrio: ...") ou e longa demais, a frase e propria.
static const char *beneficioProprio(int op) {
  switch (op) {
    case AJ_QUALIDADE: return "O teto de resolução das fontes que o Nuvio escolhe.";
    case AJ_FONTE_HDR: return "Se fontes com HDR ou Dolby Vision passam na frente das outras.";
    case AJ_DV: return "Deixa tocar fontes em Dolby Vision quando a TV aceita.";
    case AJ_ATMOS: return "Fontes com som Dolby Atmos passam na frente.";
    case AJ_FONTE_PRIORIDADE: return "O critério da escolha automática: equilíbrio, qualidade ou começar rápido.";
    case AJ_FONTE_PRAZO: return "Quanto tempo o Nuvio espera os add-ons antes de escolher.";
    case AJ_PLR_CLASSIF: return "No começo do filme, a ilha mostra a classificação e os avisos.";
    case AJ_SAIDA_PLAYER: return "O filme pode ficar na ilha do relógio para você voltar depois.";
    case AJ_LEG_FORCADA: return "Placas e falas em outra língua aparecem; o resto, não.";
    case AJ_BUSCA_CINEMETA: return "A busca também consulta o Cinemeta se o catálogo do Nuvio falhar.";
    case AJ_HOME_LAYOUT: return "Como a Home se organiza: Moderna, Padrão ou Dinâmica.";
    case AJ_HERO_FUNDO: return "De onde vem a arte grande do destaque e da página do título.";
    case AJ_HERO_ARTE_DIF: return "O destaque pode usar outra arte que não a do cartaz.";
    case AJ_HERO_TRAILER: return "Com o foco parado no destaque, o trailer toca no lugar da arte.";
    case AJ_FOCO_TRAILER: return "Com o foco parado num cartaz, o trailer toca no destaque.";
    case AJ_RESOLUCAO: return "Automática começa em 1080p e só usa 4K se a TV aguentar.";
    case AJ_PERFIL_PESQ: return "Deixa amigos acharem você pela busca.";
    case AJ_HIST_CONTA: return "O que você assiste neste perfil vai para a conta Nuvio.";
    case AJ_SOCIAL: return "Amigos, a atividade deles e recomendações.";
    case AJ_LIVETV_DIAG: return "Testa a rede, a conta e vários canais de uma vez.";
    case AJ_LIVETV_RES: return "Qual versão do canal entra primeiro: FHD, HD ou SD.";
    case AJ_XTREAM_CONTA: return "Se a assinatura está ativa, quando vence e quantas telas usa.";
    case AJ_PERFIL_EDITAR: return "Apelido, bio e o que os amigos veem no seu perfil.";
    case AJ_SELO_VISTO: return "Um check no cartaz do que você já assistiu.";
    case AJ_CW_ORDEM: return "Em que ordem a retomada mostra os títulos.";
    case AJ_FONTE_REGEX: return "Sobe ou exige fontes cujo nome casa com um padrão.";
    case AJ_DTS_AC3: return "Para TVs que não tocam DTS: o som é convertido na hora.";
    case AJ_CACHE_SEEK: return "Guarda o que já baixou para voltar no filme sem esperar.";
    default: return NULL;
  }
}
// A primeira frase de `s` (ja traduzida): ate ". ", "! ", "? " ou "。".
static const char *primeiraFrase(const char *s, char *b, size_t n) {
  size_t i, fim = 0;
  if (!s) { b[0] = 0; return b; }
  for (i = 0; s[i] && i + 1 < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c == '.' || c == '!' || c == '?') && (s[i + 1] == ' ' || !s[i + 1])) { fim = i + 1; break; }
    if (c == 0xE3 && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x82) { fim = i + 3; break; }
  }
  if (!fim) fim = i;
  if (fim >= n) fim = n - 1;
  memcpy(b, s, fim); b[fim] = 0;
  return b;
}
static const char *ajudaOpcao(int op);
static const char *uxBeneficio(int op) {
  static char b[400];
  const char *p = beneficioProprio(op);
  if (p) return i18n(p);
  return primeiraFrase(i18n(ajudaOpcao(op)), b, sizeof b);
}


// Deslocamento vertical do topo da lista ate a linha `op`, contando os
// cabecalhos das secoes E das subsecoes que vieram antes. Tem de casar
// exatamente com o laco de desenho em ajustes_desenhar: duas contas do mesmo
// layout sao duas chances de discordar, e quando discordam a rolagem para na
// linha errada.
// UMA CATEGORIA POR PAGINA (dono, 20/09/2026: "separar por categorias em vez
// de mostrar tudo de uma vez"). A lista desenha so a categoria da opcao em
// foco, entao o y de uma opcao e medido do topo da PROPRIA categoria — e
// trocar de categoria zera a rolagem (ajustes_atualizar).
// ALTURA DE CADA ITEM NA LISTA, e a posicao de cada um medida do topo da
// categoria. Tem de casar exatamente com o laco de desenho em
// ajustes_desenhar: duas contas do mesmo layout sao duas chances de discordar,
// e quando discordam a rolagem para na linha errada. UMA CATEGORIA POR PAGINA
// (dono, 20/09/2026), entao trocar de categoria zera a rolagem.
static float ajAlturaItem(int i);
static float ajLinhaH(int i);
static float alturaItem(int i) { return ajAlturaItem(i); }
static float yDoItem(int item) {
  int s = secDoItem[item], i;
  float y = 0.0f;
  for (i = secIni[s]; i < item && i < secFim(s); i++) y += alturaItem(i);
  return y;
}
static float alturaFoco(int i) { return TELA[i].tipo == IT_GRP ? AJ_GRUPO_H : TELA[i].tipo == IT_MAIS ? alturaItem(i) : ajLinhaH(i); }

// Tecla dentro da folha de fileiras.
//
// O GESTO. "OK para pegar, cima/baixo para mover, OK para soltar" e o padrao de
// reordenar em TV e e o que a folha usa — mas so na PRIMEIRA coluna, a do nome.
// Se OK pegasse em qualquer coluna, nao sobraria tecla nenhuma para ligar,
// desligar e trocar o card: as coloridas do controle nao chegam aos dois alvos
// (no webOS so a azul tem scancode conhecido, e no Tizen a vermelha ja e o
// painel de diagnostico do shell), e esquerda/direita sao a navegacao entre
// colunas. Entao: esquerda/direita escolhem A COLUNA, OK age NA COLUNA, e a
// coluna do nome e a alca de mover. A folha escreve isso na tela.
// REAGIR A UMA MUDANCA NA FOLHA. Mexer na ordem/liga-desliga so gravava a
// preferencia — a home seguia com o arranjo velho e o rotulo "Fora do limite"
// ficava preso, porque naHome so e re-marcado quando a home remonta. Duas
// reacoes, na ordem do barato para o caro:
//
// 1. desc_remontar_fileiras(): sem rede, reaplica ordem/limite sobre o que ja
//    foi baixado. Resolve na hora tudo que ja tem dado no aparelho.
// 2. desc_repetir(): a fileira promovida para dentro do limite que nunca foi
//    buscada (vista na declaracao, nunca naHome) nao tem item nenhum salvo —
//    nao ha o que a remontagem desenhar. So um ciclo de rede a traz. Varre so
//    as primeiras `limite` posicoes: fora dele ela continuaria cortada de
//    qualquer jeito, e o ciclo seria pago sem efeito.
static void fileirasReagir(void) {
  int i, teto = fil_limite(), n = fil_n();
  desc_remontar_fileiras();
  for (i = 0; i < n && i < teto; i++)
    if (!fil_linha_oculta(i) && fil_linha_vista(i) && !fil_linha_na_home(i)) {
      printf("[ajustes] %d dentro do limite sem dados: ciclo de rede\n", i);
      fflush(stdout);
      desc_repetir();
      return;
    }
}

// DUAS ABAS: "Na Home" e "Fora da Home". Desenho do dono.
//
// A lista unica anterior misturava tudo — 192 linhas com ligadas no meio,
// desligadas, addon que sumiu, catalogos de busca — e a pessoa nao achava as
// 16 que importam (relato do @rawldon: "the active ones should be grouped at
// the top"). Agora:
//
//   NA HOME      as ligadas, na ordem em que a home monta. Ate `limite` delas
//                estao na home; as que passam do limite estao NA FILA, abaixo
//                de um separador, e entram sozinhas quando alguem sai. Aqui se
//                reordena, remove e escolhe card e tamanho.
//
//   FORA DA HOME tudo o que esta desligado, agrupado por addon e em ordem
//                alfabetica, com salto por letra ao segurar cima/baixo. OK
//                adiciona: entra na home se cabe, senao entra na fila — e a
//                tela DIZ "Home cheia".
//
// A fila nao e estrutura nova: e a ordem de sempre lida de outro jeito (ver
// fil_estado em fileiras.h). Tudo continua no mesmo arquivo, agora por perfil.
static int filAba;                    // 0 = Na Home, 1 = Fora da Home
static int filNaBarra;                // foco na barra de abas
// FIL_MAX + 1: a aba "Na Home" escreve o DESTAQUE antes das fileiras, e com a
// tabela cheia (320) a lista teria 321 entradas. Um a mais aqui custa 4 bytes e
// tira do caminho um estouro que so apareceria na TV de quem tem addon demais.
static int filLista[FIL_MAX + 1];     // indices fil_* da aba corrente, na ordem da tela
static int filListaN;
static int filSep;                    // posicao na lista onde comeca a fila (-1 = nao ha)
static char   filAviso[120];          // "Home cheia..." por alguns segundos
static Uint32 filAvisoAte;
// Salto por letra: cima/baixo SEGURADO. O firmware repete o KEYDOWN; tres
// repeticoes seguidas dentro de FIL_RAJADA_MS viram salto para a proxima letra.
static Uint32 filUltTecla; static int filRajada; static SDL_Keycode filRajadaTecla;
#define FIL_RAJADA_MS   260
#define FIL_RAJADA_MIN    3

static void filAvisar(const char *txt) {
  snprintf(filAviso, sizeof filAviso, "%s", txt);
  filAvisoAte = SDL_GetTicks() + 2600;
}

// Ordem da aba "Fora": origem (app, colecao, catalogo), depois addon, depois
// titulo — os tres sem caixa. Estavel: empate fica na ordem da lista.
static int filForaAgrupada = 1;   // aba "Fora": por addon (1) ou alfabetica unica (0)
static int filCmpFora(const void *pa, const void *pb) {
  int a = *(const int *)pa, b = *(const int *)pb, c;
  if (filForaAgrupada) {
    // Addon primeiro; grupo de colecao entra no addon dele quando tem um
    // (col_grupo_addon), senao fica no bloco "Colecao" com os sem addon.
    c = strcasecmp(fil_linha_addon(a), fil_linha_addon(b));
    if (c) return c;
    c = fil_linha_origem(a) - fil_linha_origem(b);
    if (c) return c;
  }
  c = strcasecmp(fil_titulo(a), fil_titulo(b));
  if (c) return c;
  return a - b;
}

// O DESTAQUE E UMA LINHA DA LISTA, com indice proprio.
//
// Ele nao e uma fileira (nao tem catalogo, nao entra na fila, nao se move), mas
// a pessoa o procura onde procura as fileiras — foi o pedido: "no reorder tem
// que ter o hero para poder substituir e colocar o que quiser lá". Entrar como
// SENTINELA dentro de filLista, em vez de deslocar as posicoes de todo mundo,
// mantem a navegacao, a rolagem, o salto por letra e o arrastar exatamente
// como estavam: tudo isso ja passa por filIdx, e todo caminho que age sobre uma
// fileira ja tinha o `if (idx < 0) return` que o botao do rodape exigia.
#define AJ_FIL_DESTAQUE (-2)

static void filMontarLista(void) {
  int i, n = fil_n();
  filListaN = 0; filSep = -1;
  if (filAba == 0) {
    filLista[filListaN++] = AJ_FIL_DESTAQUE;
    for (i = 0; i < n; i++) {
      int e = fil_estado(i);
      if (e == FIL_FORA) continue;
      if (e == FIL_NA_FILA && filSep < 0) filSep = filListaN;
      filLista[filListaN++] = i;
    }
  } else {
    for (i = 0; i < n; i++) if (fil_estado(i) == FIL_FORA) filLista[filListaN++] = i;
    qsort(filLista, (size_t)filListaN, sizeof *filLista, filCmpFora);
  }
  { int max = filListaN + (filAba == 1 ? 1 : 0);
    if (filFoco > max) filFoco = max; }
  if (filFoco < 0) filFoco = 0;
}

// Linha da tela -> indice em fil_*. -1 no botao (aba 0, posicao filListaN).
static int filIdx(int pos) {
  return (pos >= 0 && pos < filListaN) ? filLista[pos] : -1;
}

// Primeira letra "de ordem" do titulo, em caixa alta; digito e simbolo viram '#'.
static char filLetra(int i) {
  const char *t = fil_titulo(i);
  unsigned char c = (unsigned char)(t && t[0] ? t[0] : '#');
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c >= 'A' && c <= 'Z') return (char)c;
  return '#';
}

// Rotulo da fonte do destaque, para a coluna de valor.
static const char *heroFonteRotulo(void) {
  const char *f = fil_hero_fonte();
  int i, n;
  if (!f[0]) return i18n("Automático");
  if (f[0] == '*' && !f[1]) return i18n("Aleatório do catálogo");
  n = fil_n();
  for (i = 0; i < n; i++)
    if (!strcmp(fil_chave(i), f)) return fil_titulo(i);
  // A fileira saiu (addon removido). A escolha NAO e apagada aqui — ver
  // fileiras.h —, entao o rotulo tem de dizer o que esta acontecendo em vez de
  // mostrar "Automático" e fingir que ninguem escolheu nada.
  return i18n("Fileira indisponível");
}

// Percorre as fontes possiveis: automatico, sorteio, e cada fileira que esta na
// home, na ordem em que ela aparece. As fileiras FORA da home nao entram: o
// destaque mostraria titulos de uma fileira que a pessoa desligou.
static void heroFonteCiclar(int dir) {
  char ops[FIL_MAX + 2][FIL_CHAVE];
  int n = 0, i, atual = 0, total = fil_n();
  snprintf(ops[n++], FIL_CHAVE, "%s", "");
  snprintf(ops[n++], FIL_CHAVE, "%s", "*");
  for (i = 0; i < total && n < (int)(sizeof ops / sizeof *ops); i++) {
    // #327: pasta de colecao nao tem titulos para o destaque (escolhe-la o
    // deixava vazio), e um catalogo que mora dentro de uma pasta ou fora da
    // Home e uma fonte valida: a descoberta o baixa so para o destaque.
    int o = fil_origem_de(fil_chave(i));
    if (o == FIL_ORIGEM_COLECAO) continue;
    if (fil_estado(i) == FIL_NA_HOME || (o == FIL_ORIGEM_CATALOGO && !fil_linha_sem_addon(i)))
      snprintf(ops[n++], FIL_CHAVE, "%s", fil_chave(i));
  }
  { const char *f = fil_hero_fonte();
    for (i = 0; i < n; i++) if (!strcmp(ops[i], f)) { atual = i; break; } }
  atual += dir;
  if (atual < 0) atual = n - 1;
  if (atual >= n) atual = 0;
  fil_definir_hero_fonte(ops[atual]);
}

static void eventoFileiras(SDL_Keycode k) {
  int n;
  filMontarLista();
  n = filListaN;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) {
    if (filPegou) {
      // Voltar com o item na mao DESFAZ o movimento. Soltar e cancelar tem de
      // ser teclas diferentes: sem cancelamento, um movimento errado num
      // controle de TV so se conserta contando os passos de volta.
      //
      // O `passos` nao e paranoia: se a lista encolher enquanto o item esta na
      // mao (logout, ou um addon que sumiu), fil_mover devolve o MESMO indice
      // e um `while` sem teto fica preso — trava o app com o controle na mao
      // da pessoa. Com o teto, o pior caso e o item ficar onde esta.
      int passos = FIL_MAX + 1;
      int idx = filIdx(filFoco);
      while (idx >= 0 && idx != filPegouDe && passos-- > 0) {
        int antes = idx;
        idx = (filPegou == 2) ? fil_mover_grupo(idx, filPegouDe > idx ? 1 : -1)
                              : fil_mover(idx, filPegouDe > idx ? 1 : -1);
        if (idx == antes) break;
      }
      filPegou = 0;
      filMontarLista();
      { int p; for (p = 0; p < filListaN; p++) if (filLista[p] == idx) filFoco = p; }
    } else {
      filAberta = 0;
    }
    return;
  }

  // BARRA DE ABAS: ← → trocam a aba, ↓ volta para a lista.
  if (filNaBarra) {
    if (k == SDLK_LEFT || k == SDLK_RIGHT) {
      filAba = !filAba; filFoco = 0; filTopo = 0; filCampo = 0; filPegou = 0;
      filMontarLista();
    } else if (k == SDLK_DOWN || k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      filNaBarra = 0;
    }
    return;
  }

  if (k == SDLK_DOWN || k == SDLK_UP) {
    int dir = (k == SDLK_DOWN) ? 1 : -1;
    Uint32 agora = SDL_GetTicks();
    // Rajada: a mesma tecla repetida sem folga.
    if (k == filRajadaTecla && agora - filUltTecla < FIL_RAJADA_MS) filRajada++;
    else filRajada = 0;
    filRajadaTecla = k; filUltTecla = agora;

    if (filPegou == 2) { int idx = fil_mover_grupo(filIdx(filFoco), dir); filMontarLista();
                         { int p; for (p = 0; p < filListaN; p++) if (filLista[p] == idx) filFoco = p; } return; }
    if (filPegou)      { int idx = fil_mover(filIdx(filFoco), dir); filMontarLista();
                         { int p; for (p = 0; p < filListaN; p++) if (filLista[p] == idx) filFoco = p; } return; }
    if (k == SDLK_UP && filFoco == 0) { filNaBarra = 1; return; }
    // SALTO POR LETRA na aba "Fora": segurando, pula para a proxima letra em
    // vez de andar linha a linha — com 200 linhas e o unico jeito de chegar
    // ao fim sem soltar o dedo por um minuto.
    if (filAba == 1 && filRajada >= FIL_RAJADA_MIN && n > 0 && filFoco < n) {
      int p = filFoco, letra = filLetra(filIdx(filFoco));
      while (p + dir >= 0 && p + dir < n && filLetra(filIdx(p + dir)) == letra) p += dir;
      if (p + dir >= 0 && p + dir < n) p += dir;
      // Sem proxima letra o salto nao anda — e ai o passo normal vale, senao a
      // tecla segurada nunca chegava ao botao "Atualizar tudo" no fim da lista.
      if (p != filFoco) { filFoco = p; return; }
    }
    // Botoes no fim: aba 0 tem "Atualizar tudo" em n; aba 1 tem "Agrupar por
    // addon" em n e "Atualizar tudo" em n+1.
    { int max = (filAba == 0) ? n : n + 1;
      if (filFoco + dir >= 0 && filFoco + dir <= max) filFoco += dir; }
    return;
  }
  filRajada = 0;

  if (k == SDLK_LEFT || k == SDLK_RIGHT) {
    if (filPegou) {
      // Com o item na mao, esquerda/direita alterna entre mover a FILEIRA e
      // mover o BLOCO do addon inteiro. O texto da folha diz qual e o modo.
      filPegou = (filPegou == 1) ? 2 : 1;
      return;
    }
    if (filIdx(filFoco) == AJ_FIL_DESTAQUE) {
      // A linha do destaque nao tem colunas: ela tem um valor, e as setas o
      // trocam — a mesma gramatica das linhas de escolha da lista principal.
      heroFonteCiclar(k == SDLK_RIGHT ? 1 : -1);
      return;
    }
    if (filAba == 1 || filFoco >= n) return;   // uma coluna so; os botoes nao tem colunas
    filCampo += (k == SDLK_RIGHT) ? 1 : -1;
    if (filCampo < 0) filCampo = 0;
    if (filCampo > AJ_FIL_CAMPOS - 1) filCampo = AJ_FIL_CAMPOS - 1;
    return;
  }

  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    int idx = filIdx(filFoco);
    // ATUALIZAR TUDO: um sync da conta (addons, colecoes, ajustes) e uma volta
    // completa da descoberta. E o botao para "instalei/removi um addon no
    // celular e quero ver aqui agora", sem sair da conta nem esperar o ciclo.
    // A poda de fantasmas roda dentro da volta.
    if ((filAba == 0 && filFoco == n) || (filAba == 1 && filFoco == n + 1)) {
      sync_iniciar();
      desc_repetir();
      filAvisar(i18n("Atualizando: addons, coleções e fileiras — a Home se refaz uma vez, no fim"));
      return;
    }
    if (filIdx(filFoco) == AJ_FIL_DESTAQUE) { heroFonteCiclar(1); return; }
    if (n < 1) return;
    if (filAba == 1 && filFoco == n) {
      // AGRUPAR POR ADDON e uma alternancia desta aba: agrupado (padrao) ou
      // uma lista alfabetica unica. Na aba "Na Home" nao existe — la a ordem e
      // a da home, e a pessoa e quem a arruma.
      filForaAgrupada = !filForaAgrupada;
      filFoco = 0; filTopo = 0;
      filMontarLista();
      return;
    }
    if (filAba == 1) {
      // ADICIONAR. Cabe: entra na home. Nao cabe: entra na fila, e a tela diz.
      int est = -1;
      if (idx < 0) return;
      fil_adicionar(idx, &est);
      if (est == FIL_NA_FILA) {
        char b[120];
        snprintf(b, sizeof b, i18n("Home cheia (%d de %d) · entrou na fila e sobe quando abrir vaga"),
                 fil_limite(), fil_limite());
        filAvisar(b);
      } else filAvisar(i18n("Adicionada à Home"));
      fileirasReagir();
      filMontarLista();
      if (filFoco >= filListaN) filFoco = filListaN - 1;
      if (filFoco < 0) filFoco = 0;
      return;
    }
    if (idx < 0) return;
    switch (filCampo) {
      case 0: if (!filPegou) { filPegou = 1; filPegouDe = idx; }
              else { filPegou = 0;
                     // Soltou em lugar diferente: a home nao sabe ainda.
                     if (idx != filPegouDe) fileirasReagir(); }
              break;
      case 1: // REMOVER: vai para "Fora da Home"; quem estava na fila sobe.
              fil_remover(idx);
              filAvisar(i18n("Removida da Home"));
              fileirasReagir(); filMontarLista();
              if (filFoco >= filListaN) filFoco = filListaN > 0 ? filListaN - 1 : 0;
              break;
      case 2: if (fil_aceita_tipo(idx)) { fil_ciclar_tipo(idx); fileirasReagir(); } break;
      default: fil_ciclar_tam(idx); break;
    }
  }
}

// --- MODO SEGURO: quais ajustes vigiar e como desfaze-los ---------------------
// A regra geral e o cabecalho de seguro.h. Aqui mora o que so este arquivo sabe:
// QUAIS ajustes pesam, o que e "mais arriscado" para cada um e como se restaura.
//
// SO ENTRA O QUE EXISTE E PESA (memoria de imagem, GPU, preenchimento, rede):
//   fileirasLimite   Fileiras da home acima de 12 (era o teto de 16; agora 40)
//   itensFileira     Itens por fileira acima de 12 (18, 24)
//   resolucao4k      Resolucao da interface em 4K (experimental)
//   vidro            Interface de vidro (paineis translucidos = mais preenchimento)
//   temaImersivo     Cor de destaque "Dinamica imersiva" (arte vazando como luz)
//   trailerDestaque  Trailer no destaque do topo (decodifica video na home)
//   trailerCartaz    Trailer do cartaz em foco (idem, a cada foco parado)
//   p2p              Servidor P2P (experimental)
//   qualidadeImagem  Qualidade da imagem "Alta" (a versao grande de cada arte)
//   memoriaImagens   Memoria para imagens de 400 ou 512 MB (alto-cache)
// FORA, POR NAO EXISTIREM neste tree: capas GIF/WebP animadas (nao ha ajuste; o
// GIF do foco e da build, NV_LEVE), posteres personalizados e layout "Dinamica".
// O "alto cache" que o dono usa na C9 e uma FLAG DE BUILD (arm.sh --alto-cache),
// nao um ajuste: o que ha no app e "Memoria para imagens", ja incluida.
typedef struct {
  int         op;
  const char *chave;              // id estavel no diario; nunca traduzir
  int       (*nivel)(int v);      // 0 = seguro; maior = mais arriscado
} Risco;
static int nvFileiras(int v) { return v > FIL_LIMITE_VIGIADO ? v : 0; }
static int nvItens(int v)    { return v > 0 ? v : 0; }
static int nvLigado(int v)   { return v == 0; }        // V_LIGA: 0 = Ligado
static int nv4k(int v)       { return v == RES_4K; }
static int nvImersiva(int v) { return v == AJ_TEMA_IMERSIVA; }
static int nvQualAlta(int v) { return v == 2; }
static int nvTexAlto(int v)  { return v >= 5 ? v : 0; }   // 400 e 512 MB
static const Risco RISCOS[] = {
  { AJ_FIL_LIMITE,     "fileirasLimite",  nvFileiras },
  { AJ_ITENS_FILEIRA,  "itensFileira",    nvItens },
  { AJ_RESOLUCAO,      "resolucao4k",     nv4k },
  { AJ_VIDRO,          "vidro",           nvLigado },
  { AJ_TEMA,           "temaImersivo",    nvImersiva },
  { AJ_HERO_TRAILER,   "trailerDestaque", nvLigado },
  { AJ_FOCO_TRAILER,   "trailerCartaz",   nvLigado },
  { AJ_P2P_LIGADO,     "p2p",             nvLigado },
  { AJ_QUALIDADE_IMG,  "qualidadeImagem", nvQualAlta },
  { AJ_TEX_MB,         "memoriaImagens",  nvTexAlto },
};
#define N_RISCOS ((int)(sizeof RISCOS / sizeof *RISCOS))
// O valor DE VERDADE: as fileiras moram em fileiras.c e valor[] guarda so o
// espelho; o resto e valor[]. Nunca o efetivo do perfil seguro.
static int riscoAtual(int op) { return op == AJ_FIL_LIMITE ? fil_limite_gravado() : valor[op]; }
static const Risco *riscoDe(int op) {
  int i;
  for (i = 0; i < N_RISCOS; i++) if (RISCOS[i].op == op) return &RISCOS[i];
  return NULL;
}
// Chamar DEPOIS de mudar o ajuste `op`, com o valor que ele tinha ANTES.
// "ANTES" DE UMA RAJADA. Subir as fileiras de 7 para 30 sao 23 toques (a seta
// repete), e o valor "anterior" que a pessoa entende e o 7, nao o 12 em que o
// contador cruzou o limiar do diario. Toques a menos de 2,5 s um do outro contam
// como UMA edicao, e a origem dela e o valor do primeiro toque.
#define RISCO_RAJADA_MS 2500
static Uint32 riscoT[AJ_N];
static int    riscoOrigem[AJ_N];
static char   riscoTem[AJ_N];   // riscoT valido (SDL_GetTicks() pode ser 0 no comeco)
static void riscoNotar(int op, int antes) {
  const Risco *r = riscoDe(op);
  int depois;
  if (!r) return;
  depois = riscoAtual(op);
  if (r->nivel(depois) > r->nivel(antes) && depois != antes)
    seguro_mudou(r->chave, antes, depois, (long)time(NULL), SDL_GetTicks() / 1000);
  else
    seguro_ajustou(r->chave, depois, r->nivel(depois) > 0);
}
// O gancho de seguro_iniciar: devolve o ajuste ao valor de antes, SO se ele ainda
// vale o que a mudanca deixou. Quem chama depois e ajustes_dir (segunda leitura,
// de main.c) le o arquivo que gravar() acabou de escrever, entao os dois concordam.
static int riscoAplicar(const char *chave, int novo, int ant) {
  int i;
  for (i = 0; i < N_RISCOS; i++) {
    const Risco *r = &RISCOS[i];
    if (strcmp(r->chave, chave)) continue;
    if (riscoAtual(r->op) != novo) return 0;
    if (r->op == AJ_FIL_LIMITE) {
      fil_definir_limite(ant);
      valor[AJ_FIL_LIMITE] = fil_limite_gravado();
    } else valor[r->op] = ant;
    gravar();
    return 1;
  }
  return 0;   // chave de outra versao: nada a desfazer aqui
}
// Texto de um valor para os avisos: o mesmo rotulo que a linha mostra.
static void riscoRotulo(int op, int v, char *dst, size_t tam) {
  const Opcao *o = &OPCOES[op];
  if (op == AJ_FIL_LIMITE) snprintf(dst, tam, "%d", v);
  else if (o->tipo == OP_ESCOLHA && v >= 0 && v < o->n) snprintf(dst, tam, "%s", i18n(o->valores[v]));
  else snprintf(dst, tam, "%d", v);
}
// Perfil seguro GRAVADO: a segunda queda rapida, ja no perfil seguro. Escreve no
// arquivo os valores que o perfil seguro so simulava, para o proximo arranque nao
// cair no mesmo laco. Fileiras e itens vao ao padrao de fabrica (7 e 12), abaixo
// do teto de 12 do perfil de sessao — se 12 ja derrubou, 12 nao serve.
static void riscoGravarSeguro(void) {
  int i;
  for (i = 0; i < N_RISCOS; i++) {
    const Risco *r = &RISCOS[i];
    int v = riscoAtual(r->op);
    if (r->op == AJ_TEMA) { if (temaDinamico()) valor[AJ_TEMA] = 0; continue; }
    if (r->nivel(v) <= 0) continue;
    if (r->op == AJ_FIL_LIMITE) { fil_definir_limite(FIL_LIMITE_PADRAO); valor[AJ_FIL_LIMITE] = fil_limite_gravado(); }
    else if (r->op == AJ_ITENS_FILEIRA || r->op == AJ_RESOLUCAO || r->op == AJ_TEX_MB) valor[r->op] = 0;
    else if (r->op == AJ_QUALIDADE_IMG) valor[r->op] = 1;
    else valor[r->op] = 1;   // interruptores V_LIGA: 1 = Desligado
  }
  gravar();
}

void ajustes_seguro_iniciar(int caiu) {
  const SegDecisao *d = seguro_iniciar(caiu, (long)time(NULL), riscoAplicar);
  char id[72], tit[80], txt[420];
  int i;
  perfilSeguro = d->modo == SEG_PERFIL_SEGURO;
  for (i = 0; i < d->nRevertidas; i++) {
    const SegMud *m = &d->revertidas[i];
    int k;
    for (k = 0; k < N_RISCOS; k++) if (!strcmp(RISCOS[k].chave, m->chave)) break;
    if (k < N_RISCOS) {
      char nome[80], novo[48], ant[48];
      snprintf(nome, sizeof nome, "%s", i18n(OPCOES[RISCOS[k].op].rotulo));
      riscoRotulo(RISCOS[k].op, m->novo, novo, sizeof novo);
      riscoRotulo(RISCOS[k].op, m->ant, ant, sizeof ant);
      snprintf(id, sizeof id, "seguro:%d:%s", d->sessao, m->chave);
      snprintf(tit, sizeof tit, "%s", i18n("Ajuste desfeito"));
      snprintf(txt, sizeof txt, i18n("O app fechou depois de mudar \"%s\" para %s. Voltei para %s para ele abrir de novo. Você pode tentar outra vez em Ajustes."),
               nome, novo, ant);
      avisos_modo_seguro(id, tit, txt);
    }
  }
  if (d->modo == SEG_PERFIL_SEGURO) {
    // Fileiras: teto de sessao em fileiras.c (nao mexe no arquivo); o resto sao
    // os acessores acima, que leem seguro_perfil_ativo().
    fil_definir_teto_sessao(FIL_LIMITE_VIGIADO);
    snprintf(id, sizeof id, "seguro:%d:%s", d->sessao, "perfil");
    snprintf(tit, sizeof tit, "%s", i18n("Modo seguro ligado"));
    snprintf(txt, sizeof txt, "%s", i18n("O app fechou duas vezes seguidas logo depois de abrir. Nesta sessão ele roda sem vidro, 4K, tema imersivo e trailers, e com menos fileiras e itens. Seus ajustes salvos não mudaram."));
    avisos_modo_seguro(id, tit, txt);
  } else if (d->modo == SEG_PERSISTIR_SEGURO) {
    riscoGravarSeguro();
    snprintf(id, sizeof id, "seguro:%d:%s", d->sessao, "gravado");
    snprintf(tit, sizeof tit, "%s", i18n("Ajustes seguros gravados"));
    snprintf(txt, sizeof txt, "%s", i18n("O app continuou fechando mesmo no modo seguro. Gravei os ajustes seguros: sem vidro, 4K, tema imersivo e trailers, e com as fileiras e os itens de fábrica. Você pode mudar tudo de novo em Ajustes."));
    avisos_modo_seguro(id, tit, txt);
  }
}

// --- FOLHA DE CONFIRMACAO: mais fileiras / mais itens -------------------------
// Primeira vez que a pessoa passa do que sempre coube, uma folha explica o preco.
// Modal dentro desta tela (mesma razao da folha de fileiras: ajustes.c nao pede
// nada a app.c). Depois de aceita uma vez, vale por aparelho (seguro.txt) e as
// proximas mudancas nao perguntam — mas continuam vigiadas pelo diario.
static int  riscoFolha;          // 0 fechada; senao SEG_AVISO_*
static int  riscoFolhaOp, riscoFolhaDir;
static int  riscoFolhaFoco;      // 0 = Continuar, 1 = Cancelar
static void mudarValor(int op, int dir);
// O valor a que `dir` levaria `op`, sem aplicar. Espelha mudarValor.
static int passoAdiante(int op, int dir) {
  const Opcao *o = &OPCOES[op];
  if (o->tipo == OP_NUMERO) return limita(op, valor[op] + dir * o->passo);
  { int n = nValores(op); return (valor[op] + (dir > 0 ? 1 : n - 1)) % n; }
}
// 1 quando a mudanca precisa da folha (e a abriu).
static int riscoPedirConfirmacao(int op, int dir) {
  int bit, de, para;
  if (op == AJ_FIL_LIMITE) {
    bit = SEG_AVISO_FILEIRAS;
    de = riscoAtual(op); para = passoAdiante(op, dir);
    if (!(para > FIL_LIMITE_SEGURO && para > de)) return 0;
  } else if (op == AJ_ITENS_FILEIRA) {
    bit = SEG_AVISO_ITENS;
    de = valor[op]; para = passoAdiante(op, dir);
    if (!(para > 0 && para > de)) return 0;
  } else return 0;
  if (seguro_aviso_visto(bit)) return 0;
  riscoFolha = bit; riscoFolhaOp = op; riscoFolhaDir = dir; riscoFolhaFoco = 1;   // Cancelar de partida
  return 1;
}
static const char *riscoFolhaTitulo(void) {
  return riscoFolha == SEG_AVISO_FILEIRAS ? "Mais fileiras na Home" : "Mais itens por fileira";
}
static const char *riscoFolhaTexto(void) {
  return riscoFolha == SEG_AVISO_FILEIRAS
    ? "Mais fileiras usam mais memória e rede; em TVs com 1 GB a Home pode ficar lenta ou fechar. Se o app fechar, ele volta sozinho ao valor anterior."
    : "Mais itens por fileira usam mais memória; em TVs com 1 GB a Home pode ficar lenta ou fechar. Se o app fechar, ele volta sozinho ao valor anterior.";
}
static void riscoFolhaEvento(SDL_Keycode k) {
  if (k == SDLK_LEFT || k == SDLK_RIGHT) { riscoFolhaFoco = k == SDLK_LEFT ? 0 : 1; return; }
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE) { riscoFolha = 0; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    int bit = riscoFolha, op = riscoFolhaOp, dir = riscoFolhaDir, ok = riscoFolhaFoco == 0;
    riscoFolha = 0;
    if (!ok) return;
    seguro_aviso_marcar(bit);
    mudarValor(op, dir);   // agora vista: aplica de verdade
  }
}
static void desenhaRiscoFolha(void);

// --- folha "Add-ons permitidos" / "Plugins permitidos" (#202) ----------------
// Modal dentro desta tela, como a de fileiras: a lista dos instalados (e dos
// marcados que ja nao estao instalados, para poder tira-los), com "Todos" no
// topo. OK marca/desmarca e grava na hora (fonteregra.c); Voltar fecha.
#define FR_FOLHA_MAX 128
static int frAberta;            // 0 fechada; 1 add-ons; 2 plugins; 3 ordem dos add-ons
static int frFoco;
static int frCol;               // folha da ordem: 0 = subir, 1 = descer
static int frN;
static char frNome[FR_FOLHA_MAX][FR_NOME_MAX];
static char frApoio[FR_FOLHA_MAX][64];
static void uxNotificar(const char *s);
static void frPor(const char *nome, const char *apoio) {
  int k;
  if (!nome || !nome[0] || frN >= FR_FOLHA_MAX) return;
  for (k = 0; k < frN; k++) if (!strcasecmp(frNome[k], nome)) return;
  snprintf(frNome[frN], FR_NOME_MAX, "%s", nome);
  snprintf(frApoio[frN], sizeof frApoio[frN], "%s", apoio ? apoio : "");
  frN++;
}
static int frAchar(const char *nome) {
  int k;
  for (k = 0; k < frN; k++) if (!strcasecmp(frNome[k], nome)) return k;
  return -1;
}
// A folha da ORDEM: primeiro os nomes da ordem, na ordem dela, depois os
// instalados que ainda nao estao nela (add-ons, depois plugins).
static void frMontarOrdem(void) {
  int i, j, k;
  char nm[FR_NOME_MAX];
  frN = 0;
  for (k = 0; k < fonteregra_ordem_n() && k < FR_ORDEM_MAX; k++)
    if (fonteregra_ordem_nome(k, nm, sizeof nm)) frPor(nm, "Não instalado");
  for (i = 0; i < addons_n(); i++)
    if (!addons_sondado(i) || addons_fornece(i, ADD_STREAM)) {
      const char *nome = addons_nome(i), *ap = addons_ativo(i) ? "" : "Desligado";
      int at = frAchar(nome);
      if (at >= 0) snprintf(frApoio[at], sizeof frApoio[at], "%s", ap);
      else if (frN < FR_ORDEM_MAX) frPor(nome, ap[0] ? ap : "Fora da ordem");
    }
  for (i = 0; i < plugins_n_repos(); i++) {
    int tot = plugins_repo_scrapers(i, NULL);
    for (j = 0; j < tot; j++) {
      PlugScraper sc;
      if (plugins_scraper(i, j, &sc)) {
        int at = frAchar(sc.nome);
        const char *ap = sc.ativo ? "Plugin" : "Desligado";
        if (at >= 0) snprintf(frApoio[at], sizeof frApoio[at], "%s", ap);
        else if (frN < FR_ORDEM_MAX) frPor(sc.nome, ap);
      }
    }
  }
  if (frFoco > frN) frFoco = frN;
  if (frFoco < 0) frFoco = 0;
}
static void frMontar(void) {
  int plugin = frAberta == 2, i, j, k;
  char nm[FR_NOME_MAX];
  if (frAberta == 3) { frMontarOrdem(); return; }
  frN = 0;
  if (!plugin) {
    for (i = 0; i < addons_n(); i++)
      if (!addons_sondado(i) || addons_fornece(i, ADD_STREAM))
        frPor(addons_nome(i), addons_ativo(i) ? "" : "Desligado");
  } else {
    for (i = 0; i < plugins_n_repos(); i++) {
      int tot = plugins_repo_scrapers(i, NULL);
      for (j = 0; j < tot; j++) {
        PlugScraper sc;
        if (plugins_scraper(i, j, &sc)) frPor(sc.nome, sc.ativo ? "" : "Desligado");
      }
    }
  }
  for (k = 0; k < fonteregra_n(plugin); k++)
    if (fonteregra_nome(plugin, k, nm, sizeof nm)) frPor(nm, "Não instalado");
  if (frFoco > frN) frFoco = frN;
  if (frFoco < 0) frFoco = 0;
}
static void frAbrir(int qual) { frAberta = qual == 3 ? 3 : qual ? 2 : 1; frFoco = 0; frCol = 0; frMontar(); }
// Sobe ou desce o item `k` (0..frN-1) da folha da ordem. A lista que a folha
// mostra vira a ordem (quem estava fora entra), com a troca feita.
static void frOrdemMover(int k, int dir) {
  const char *ptr[FR_FOLHA_MAX];
  char tmp[FR_NOME_MAX], ap[64];
  int j = k + dir, i;
  if (k < 0 || k >= frN || j < 0 || j >= frN) return;
  snprintf(tmp, sizeof tmp, "%s", frNome[k]); snprintf(frNome[k], FR_NOME_MAX, "%s", frNome[j]); snprintf(frNome[j], FR_NOME_MAX, "%s", tmp);
  snprintf(ap, sizeof ap, "%s", frApoio[k]); snprintf(frApoio[k], sizeof frApoio[k], "%s", frApoio[j]); snprintf(frApoio[j], sizeof frApoio[j], "%s", ap);
  for (i = 0; i < frN; i++) ptr[i] = frNome[i];
  fonteregra_ordem_definir(ptr, frN);
  frFoco = j + 1;
  sync_proteger_ajustes_locais();
}
static void frEventoOrdem(SDL_Keycode k) {
  frMontar();
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE) { frAberta = 0; return; }
  if (k == SDLK_UP && frFoco > 0) frFoco--;
  if (k == SDLK_DOWN && frFoco < frN) frFoco++;
  if (k == SDLK_LEFT) frCol = 0;
  if (k == SDLK_RIGHT) frCol = 1;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (frFoco == 0) { fonteregra_ordem_definir(NULL, 0); sync_proteger_ajustes_locais(); uxNotificar("Ordem removida."); }
    else frOrdemMover(frFoco - 1, frCol ? 1 : -1);
  }
}
static void frEvento(SDL_Keycode k) {
  int plugin = frAberta == 2;
  if (frAberta == 3) { frEventoOrdem(k); return; }
  frMontar();
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE || k == SDLK_LEFT) {
    frAberta = 0; return;
  }
  if (k == SDLK_UP && frFoco > 0) frFoco--;
  if (k == SDLK_DOWN && frFoco < frN) frFoco++;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (frFoco == 0) fonteregra_limpar(plugin);
    else fonteregra_alternar(plugin, frNome[frFoco - 1]);
    // Sobe para a conta como os outros ajustes do perfil (#85).
    sync_proteger_ajustes_locais();
    uxNotificar("Ajuste salvo nesta TV.");
  }
}
// Resumo na linha: "Todos" ou os nomes marcados.
static const char *frResumo(int plugin) {
  static char b[2][160];
  char nm[FR_NOME_MAX];
  int n = fonteregra_n(plugin), k;
  char *d = b[plugin ? 1 : 0];
  if (!n) return i18n(plugin ? "Todos os plugins ligados" : "Todos os add-ons");
  d[0] = 0;
  for (k = 0; k < n && k < 3; k++) {
    if (!fonteregra_nome(plugin, k, nm, sizeof nm)) continue;
    if (d[0]) strncat(d, ", ", 160 - strlen(d) - 1);
    strncat(d, nm, 160 - strlen(d) - 1);
  }
  if (n > 3) { char m[24]; snprintf(m, sizeof m, " +%d", n - 3); strncat(d, m, 160 - strlen(d) - 1); }
  return d;
}
// "A > B > C" na linha da ordem, ou "Nenhuma".
static const char *frOrdemResumo(void) {
  static char b[160];
  if (!fonteregra_ordem_n()) return i18n("Nenhuma");
  fonteregra_ordem_texto(b, sizeof b);
  return b;
}
// O padrao na linha, ou o aviso de que esta vazio/invalido.
static const char *frRegexTexto(void) {
  static char b[96];
  char p[FR_REGEX_MAX + 1];
  int e = fonteregra_regex_estado();
  fonteregra_regex(p, sizeof p);
  if (e < 0) return i18n("Regex inválida: ignorada");
  if (!p[0]) return i18n("Nenhum");
  snprintf(b, sizeof b, "%.60s%s", p, strlen(p) > 60 ? "…" : "");
  return b;
}
// Sinais de regex que o teclado da TV oferece (o celular digita o que quiser,
// dentro do mesmo alfabeto).
static const char *FR_ALFA_REGEX =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789|()[]{}.*+?^$\\-_ :!/,";

// UM PASSO NO VALOR DA OPCAO `op` (dir = +1 ou -1), com tudo o que a mudanca
// tem de disparar, e a gravacao. Um lugar so para as setas do modo edicao e
// para o OK do interruptor: dois caminhos para o mesmo valor eram duas listas
// de efeitos colaterais para manter iguais.
// Uma escolha final: persiste uma vez antes dos efeitos externos. Navegar no
// seletor nunca passa por aqui, inclusive para idiomas e limite de fileiras.
// NOTAS NO TITULO (decisao do dono, 07/10): os oito "qual nota o MDBList baixa"
// (AJ_MDB_*) sairam da tela. Ligar uma nota liga o par dela na conta e, se a
// nota vem do MDBList, o MDBList tambem — senao a linha ligada nao mostraria
// nada. Desligar nao mexe no par (outro aparelho pode usar).
static void notaLigarPar(int op) {
  int par = -1, mdb = 1;
  switch (op) {
    case AJ_NT_IMDB: par = AJ_MDB_IMDB; mdb = 0; break;
    case AJ_NT_TRAKT: par = AJ_MDB_TRAKT; mdb = 0; break;
    case AJ_NT_TMDB: par = AJ_MDB_TMDB; break;
    case AJ_NT_TOMATES: par = AJ_MDB_TOMATES; break;
    case AJ_NT_AUDIENCIA: par = AJ_MDB_AUDIENCIA; break;
    case AJ_NT_META: case AJ_NT_METAUSER: par = AJ_MDB_META; break;
    case AJ_NT_LETTER: par = AJ_MDB_LETTER; break;
    case AJ_NT_MAL: par = AJ_MDB_MAL; break;
    case AJ_NT_EBERT: case AJ_NT_SCORE: break;
    default: return;
  }
  if (par >= 0) valor[par] = 0;      // 0 = Ligado
  if (mdb) valor[AJ_MDB_LIGADO] = 0;
}
static int ajOrigemDireto = AJLOG_AJUSTES;   // quem chamou: a tela, ou a Central via ajustes_rapido_passo
static int definirValorDireto(int op, int novo) {
  int antes;
  if (op < 0 || op >= AJ_N ||
      (OPCOES[op].tipo != OP_ESCOLHA && OPCOES[op].tipo != OP_NUMERO)) return 0;
  novo = limita(op, novo);
  if (op == AJ_PERFIL_PESQ) {
    pessoas_definir_pesquisavel(novo == 0);
    return 1;
  }
  antes = valor[op];
  if (novo == antes) return 1;
  valor[op] = novo;
  if (novo == 0) notaLigarPar(op);
  { int ok; ajOrigem = ajOrigemDireto; ok = gravar(); ajOrigem = 0; if (!ok) { valor[op] = antes; return 0; } }
  if (op == AJ_FIL_LIMITE) {
    fil_ajustar_limite(novo);
    valor[op] = fil_limite_gravado();
    fil_confirmar_limite();
  }
  if (op == AJ_LEG_LINGUA || op == AJ_LEG_LINGUA2 || op == AJ_AUD_LINGUA) aplicarIdioma(op);
  if (op == AJ_IDIOMA) { idiomaEscolhido(); desc_repetir(); }
  if (op == AJ_FONTE_UI) txt_definir_fonte_interface((TxtFamilia)novo);
  if (op == AJ_TAMANHO_UI) gfx_escala_ui_definir(ajustes_tamanho_ui());
  if (op == AJ_ICONE_APP) iconeapp_aplicar_plataforma();
  if (op == AJ_CW_FONTE || op == AJ_SALVOS_DEST || op == AJ_SOCIAL) desc_repetir();
  // A fonte decide so esta fileira: refaz-la, alem do ciclo completo (#244).
  if (op == AJ_CW_FONTE || op == AJ_CW_PROXIMO) desc_refazer_continuar();
  if (op == AJ_CW_ORDEM || op == AJ_CW_NAO_EXIBIDOS || op == AJ_CW_CONCLUIDO || op == AJ_CW_RETIDO_TAMBEM)
    desc_refazer_continuar();
  if (op == AJ_TEX_MB) tex_definir_orcamento_mb(ajustes_tex_mb());
  // Picking 4K again = try again: forget that this TV did not hold it.
  if (op == AJ_RESOLUCAO && novo == RES_4K) {
    char *m = dados_ler(RES_ARQ_RECUO);
    if (m) { free(m); dados_apagar(RES_ARQ_RECUO); printf("[4k] 4K escolhido de novo: tenta outra vez no proximo arranque\n"); }
  }
  // Back to Automatic = measure again at the next launch.
  if (op == AJ_RESOLUCAO && novo == RES_AUTO) dados_apagar(RES_ARQ_AUTO);
  if (op == AJ_ENQUETES) enquete_definir_optout(novo != 0);
  if (op == AJ_ADDONS_PRINCIPAL) sync_iniciar();
  if (op == AJ_SELOS_PACOTE) { selospacote_escolher(novo - 1); spEspelhar(); }
  // Modelo de regex (#202): escolher um troca o padrao; "Personalizado" so
  // deixa o padrao como esta.
  if (op == AJ_FONTE_REGEX_MODELO && novo > 0) fonteregra_definir_regex(fonteregra_modelo(novo));
  ajSombra[op] = valor[op];   // espelhos acima (limite, pacote) ja valem como gravados
  sync_proteger_ajustes_locais();
  return 1;
}
static void mudarValorDireto(int op, int dir) {
  definirValorDireto(op, passoAdiante(op, dir));
}

// A porta de entrada: pede a folha de aviso quando a mudanca passa do que sempre
// coube e, feita a mudanca, avisa o diario do modo seguro.
static void mudarValor(int op, int dir) {
  int antes = riscoDe(op) ? riscoAtual(op) : 0;
  Uint32 agora = SDL_GetTicks();
  if (riscoPedirConfirmacao(op, dir)) return;
  if (!riscoTem[op] || agora - riscoT[op] > RISCO_RAJADA_MS) riscoOrigem[op] = antes;
  riscoT[op] = agora; riscoTem[op] = 1;
  mudarValorDireto(op, dir);
  riscoNotar(op, riscoOrigem[op]);
}

// Interruptor = escolha entre Ligado e Desligado. E o `renderToggleRow` do web:
// OK troca, e o desenho e a pilula do guia, nao um valor em texto.
static int ehInterruptor(int op) {
  return op >= 0 && OPCOES[op].tipo == OP_ESCOLHA && OPCOES[op].valores == V_LIGA;
}

#include "ajustes_ux_dados.inc"
#include "ajustes_ux_interacao.inc"

static const char *uxCaminho(int op);
static const char *uxBloco(int op);
static void eventoTela(const SDL_Event *e);
void ajustes_evento(const SDL_Event *e) {
  AJ_ESCALA_INI();
  if (guiaAberto) { guiaEvento(e); AJ_ESCALA_FIM(); return; }
  if (apoioAberto) { apoioEvento(e); AJ_ESCALA_FIM(); return; }
  eventoTela(e);
  // FIM DA EDICAO DO LIMITE (issue #197): qualquer tecla que solte a linha
  // (OK, Voltar, cima/baixo, sair da tela) confirma a rajada. Sem rajada em
  // curso e um no-op.
  if (!(emEdicao && focoOp == AJ_FIL_LIMITE) || sair) fil_confirmar_limite();
  AJ_ESCALA_FIM();
}

static void eventoTela(const SDL_Event *e) {
  // A MODAL DE DIGITACAO VEM ANTES DE TUDO, como em biblioteca.c e recenviar.c:
  // enquanto ela esta em pe, nenhuma tecla pertence a lista de opcoes atras.
  // Sem esta linha, o D-pad moveria o foco da lista por baixo da modal.
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (e->key.repeat && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) return;
  montarTela();   // barato depois da primeira vez; ver ajustes_iniciar

  // Vinculo em andamento e uma pergunta: enquanto ele esta em pe, nada mais na
  // tela responde ao controle.
  { TraEstado ta = traktauth_estado();
    SmkEstado sa = simklauth_estado();
    int traAtivo = (ta == TRA_PEDINDO || ta == TRA_AGUARDANDO || ta == TRA_ERRO);
    int smkAtivo = (sa == SMK_PEDINDO || sa == SMK_AGUARDANDO || sa == SMK_ERRO);
    DisEstado da = discord_estado();
    int disAtivo = (da == DIS_PEDINDO || da == DIS_AGUARDANDO || da == DIS_ERRO);
    if (traAtivo || smkAtivo || disAtivo) {
      if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE) {
        if (traAtivo) traktauth_cancelar();
        else if (smkAtivo) simklauth_cancelar();
        else discord_cancelar();
      } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        if (disAtivo && da == DIS_ERRO) discord_comecar();
        // OK so refaz o pedido quando deu erro; com o codigo na tela ele nao
        // faz nada de proposito, para nao trocar o codigo que a pessoa acabou
        // de digitar no celular.
        if (traAtivo && ta == TRA_ERRO) traktauth_comecar();
        else if (smkAtivo && sa == SMK_ERRO) simklauth_comecar();
      }
      return;
    } }
  // A folha de aviso de memoria (mais fileiras/itens) e modal e vem antes de tudo.
  if (riscoFolha) { riscoFolhaEvento(k); return; }
  if (frAberta) { frEvento(k); return; }
  // A folha de fileiras e modal, como a do vinculo acima.
  if (filAberta) { eventoFileiras(k); return; }
  if (uxEvento(k)) return;

  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    // OK NUM GRUPO abre ou fecha (sanfona: abrir um fecha o outro). O foco
    // fica no cabecalho; baixo entra nas opcoes.
    if (focoOp < 0) {
      if (TELA[focoItem].tipo == IT_GRP) abrirGrupo(focoItem);
      // "Mais opcoes": abre (ou fecha) no lugar; o foco fica na linha e o
      // Baixo entra nas avancadas.
      if (TELA[focoItem].tipo == IT_MAIS) maisAberto[secAtual] = !maisAberto[secAtual];
      return;
    }
    if (OPCOES[focoOp].tipo != OP_ACAO) { uxAbrirEditor(focoOp); return; }
    // O contexto da modal de digitacao, se esta acao abrir uma: "Secao · Bloco".
    { char kc[200]; const char *b = uxBloco(focoOp);
      if (b[0]) snprintf(kc, sizeof kc, "%s · %s", i18n(uxCaminho(focoOp)), i18n(b));
      else snprintf(kc, sizeof kc, "%s", i18n(uxCaminho(focoOp)));
      teclado_contexto(kc); }
    if (focoOp == AJ_FIL_ORDEM) {
      filAberta = 1; filFoco = 0; filCampo = 0; filPegou = 0; filTopo = 0;
      filAba = 0; filNaBarra = 0; filAviso[0] = 0;
      // O que passou do limite sem ter sido pedido vai para "Fora da Home"
      // antes de a lista aparecer — ver fil_normalizar.
      fil_normalizar();
      emEdicao = 0;
      return;
    }
    if (focoOp == AJ_ATUALIZAR) {
      if (atualizacao_nova()[0]) atualizacao_abrir();
      else if (atualizacao_busca() != ATUALIZACAO_BUSCA_PROCURANDO) atualizacao_procurar_agora();
      return;
    }
    // O painel de envio (registro.h) dispara o envio e mostra o codigo ou o
    // motivo da falha; antes so o texto da linha mudava.
    if (focoOp == AJ_ENVIAR_LOG) { registro_envio_abrir(); return; }
    if (focoOp == AJ_VER_REGISTRO) { registro_abrir(); return; }
    if (focoOp == AJ_GUIA) { guiaAbrir(0); return; }
    if (focoOp == AJ_NOVIDADES20) { pediuNovidades20 = 1; return; }
    if (focoOp == AJ_APOIAR) { apoioAbrir(); return; }
    if (focoOp == AJ_ADDONS) { pediuAddons = 1; return; }
    if (focoOp == AJ_PLUGINS) { pediuPlugins = 1; return; }
    if (focoOp == AJ_FONTE_ORDEM) { frAbrir(3); return; }
    if (focoOp == AJ_FONTE_ADDONS_PERM || focoOp == AJ_FONTE_PLUGINS_PERM) {
      frAbrir(focoOp == AJ_FONTE_PLUGINS_PERM); return;
    }
    if (focoOp == AJ_FONTE_REGEX_PADRAO) {
      char atual[FR_REGEX_MAX + 1];
      fonteregra_regex(atual, sizeof atual);
      stCampo = focoOp;
      // O padrao volta para o campo: corrigir um sinal nao pode obrigar a
      // redigitar tudo. Vazio apaga.
      teclado_abrir_com("Padrão da regex", "Ex.: ESP|Latino|Castellano. Vazio apaga.",
                        TECLADO_LONGO, FR_ALFA_REGEX, atual[0] ? atual : NULL);
      return;
    }
    if (focoOp == AJ_DIAGNOSTICO) { pediuDiagnostico = 1; return; }
    if (focoOp == AJ_HERO_CATALOGOS) { if (!inativa(focoOp)) heroFonteCiclar(+1); return; }
    if (focoOp == AJ_VELOCIDADE) { pediuVelocidade = 1; return; }
    if (focoOp == AJ_LIVETV_DIAG) { pediuLivetvDiag = 1; return; }
    if (focoOp == AJ_STALKER_PORTAL || focoOp == AJ_STALKER_MAC) {
      int mac = focoOp == AJ_STALKER_MAC;
      stCampo = focoOp;
      // O valor atual volta para o campo: trocar a porta de um portal nao pode
      // obrigar a redigitar o endereco inteiro no D-pad. O MAC e a excecao —
      // ele nunca e devolvido em claro, nem para o proprio dono, porque a
      // modal fica na tela e a tela vira foto.
      teclado_abrir_com(mac ? "MAC do portal" : "Portal Stalker (MAC)",
                        mac ? "Formato 00:1a:79:xx:xx:xx"
                            : "Endereço e porta, sem http://",
                        mac ? 17 : 64,
                        mac ? ST_ALFA_MAC : ST_ALFA_PORTAL,
                        (!mac && stalker_configurado()) ? stalker_portal_curto() : NULL);
      return;
    }
    // O primeiro OK so arma (ver pedeConfirmacao); o segundo age.
    if (pedeConfirmacao(focoOp) && sairArmado != focoOp) { sairArmado = focoOp; return; }
    sairArmado = 0;
    if (focoOp == AJ_STALKER_LIMPAR) { stalker_esquecer(); return; }
    if (focoOp == AJ_XTREAM_SERVIDOR || focoOp == AJ_XTREAM_USUARIO || focoOp == AJ_XTREAM_SENHA) {
      int srv = focoOp == AJ_XTREAM_SERVIDOR, sen = focoOp == AJ_XTREAM_SENHA;
      stCampo = focoOp;
      // Servidor e usuario voltam para o campo (corrigir uma letra nao pode
      // obrigar a redigitar tudo no D-pad); a senha nao — a modal fica na
      // tela e a tela vira foto.
      teclado_abrir_com(srv ? "Servidor Xtream" : sen ? "Senha Xtream" : "Usuário Xtream",
                        srv ? "Endereço e porta, sem http://" : sen ? "Como o provedor mandou" : "Como o provedor mandou",
                        srv ? 64 : 48,
                        srv ? ST_ALFA_PORTAL : XT_ALFA_CONTA,
                        srv ? (strcmp(xtream_servidor_curto(), "-") ? xtream_servidor_curto() : NULL)
                            : (!sen && strcmp(xtream_usuario(), "-")) ? xtream_usuario() : NULL);
      return;
    }
    // A grade curta guardada era da conta que saiu (#158).
    if (focoOp == AJ_XTREAM_LIMPAR) { xtream_esquecer(); xtepg_limpar(); return; }
    if (focoOp == AJ_SEEKR_CHAVE) {
      // Como o fanart: a chave NUNCA volta para o campo; vazio apaga.
      stCampo = focoOp;
      teclado_abrir_com("Chave do Seekr", "Chave pessoal: seekr.tv. Vazio apaga.",
                        90, SEEKR_ALFA, NULL);
      return;
    }
    if (focoOp == AJ_SEEKR_TESTAR) { skTesteIniciar(); return; }
    if (focoOp == AJ_SELOS_PACOTE_ADD) {
      if (spFioVivo) return;
      stCampo = focoOp;
      teclado_abrir_com("Endereço do pacote de selos", "O link do JSON do pacote (https://…). Vazio cancela.",
                        200, SP_ALFA_URL, NULL);
      return;
    }
    if (focoOp == AJ_SELOS_PACOTE_REM) {
      int a = selospacote_ativo();
      if (a >= 0 && selospacote_da_tv(a)) { selospacote_remover(a); spResultado = -1; spEspelhar(); }
      return;
    }
    if (focoOp == AJ_FANART_CHAVE) {
      // A chave NUNCA volta para o campo (a modal fica na tela e a tela vira
      // foto); confirmar vazio esquece a que estava.
      stCampo = focoOp;
      teclado_abrir_com("Chave do fanart.tv", "Chave pessoal: fanart.tv/get-an-api-key. Vazio apaga.",
                        40, "0123456789abcdef", NULL);
      return;
    }
    if (focoOp == AJ_P2P_URL) {
      stCampo = focoOp;
      // O endereco volta para o campo: corrigir um digito do IP nao pode obrigar
      // a redigitar tudo no D-pad. Vazio esquece.
      teclado_abrir_com("Endereço do servidor P2P", "IP e porta do servidor Stremio: 192.168.1.5:11470. Vazio apaga.",
                        64, ST_ALFA_PORTAL, p2pEndereco[0] ? p2pEndereco : NULL);
      return;
    }
    if (focoOp >= AJ_JF_SERVIDOR && focoOp <= AJ_JF_SAIR) { jfAtivar(focoOp); return; }
    if (focoOp >= AJ_EM_SERVIDOR && focoOp <= AJ_EM_SAIR) { emAtivar(focoOp); return; }
    if (focoOp >= AJ_PX_ENTRAR && focoOp <= AJ_PX_SAIR) { pxAtivar(focoOp); return; }
    if (focoOp == AJ_PERFIL_EDITAR) { pessoas_abrir_perfil(); return; }
    if (focoOp == AJ_P2P_TESTAR) { p2pTesteIniciar(); return; }
    if (focoOp >= AJ_POSTER_INST && focoOp <= AJ_POSTER_TESTAR) { pstAtivar(focoOp); return; }
    if (debIdx(focoOp) >= 0) {
      // Como o fanart: a chave NUNCA volta para o campo; vazio apaga.
      stCampo = focoOp;
      teclado_abrir_com(focoOp == AJ_DEBRID_AD ? "Chave do AllDebrid"
                        : focoOp == AJ_DEBRID_RD ? "Chave do Real-Debrid"
                        : focoOp == AJ_DEBRID_TB ? "Chave do TorBox" : "Chave do Premiumize",
                        "Chave de API da sua conta. Vazio apaga.",
                        96, DEB_ALFA, NULL);
      return;
    }
    if (focoOp == AJ_DEBRID_AD_TESTAR) { adTesteIniciar(); return; }
    if (focoOp == AJ_TRAKT) { traktauth_comecar(); return; }
    if (focoOp == AJ_SIMKL) { simklauth_comecar(); return; }
    if (focoOp == AJ_DISCORD) {
      if (discord_estado() == DIS_LIGADO) discord_esquecer(); else discord_comecar();
      return;
    }
    if (focoOp == AJ_SAIR) {
      // Sair apaga a sessao do disco. Chega aqui so no SEGUNDO OK (ver
      // pedeConfirmacao, acima): o primeiro arma e a linha diz o que o
      // proximo faz; qualquer movimento desarma (focar).
      sessao_sair();
      traktauth_esquecer();
      simklauth_esquecer();
      simkl_esquecer();
      // A ordem e o liga/desliga das fileiras sao da home de QUEM SAIU, como a
      // ordem que vem da conta (ver catordem_esquecer). Sem isto, a proxima
      // pessoa herda a home montada pela anterior.
      fil_esquecer();
      // As listas FIXADAS tambem sao da conta que saiu: uma lista do Trakt
      // presa na Biblioteca continuaria ali, com o nome de quem foi embora, e
      // a fileira dela na home tentaria buscar itens com o token novo.
      lst_esquecer_conta();
      // A sessao sozinha nao basta: addons, Trakt, perfil e progresso ficariam
      // para a proxima pessoa. Ver o cabecalho de sync_esquecer_usuario.
      sync_esquecer_usuario();
      sair = 1;   // volta para a home, que cai no login no proximo quadro
    }
  }
  else if (k == SDLK_PAGEUP || k == SDLK_PAGEDOWN) {
    int s = secAtual + (k == SDLK_PAGEDOWN ? 1 : -1);
    if (s >= 0 && s < nSecoes) focarSecao(s);
  }

}

static void aj2Atualizar(float dt);
void ajustes_atualizar(float dt, Uint32 agora) {
  AJ_ESCALA_INI();
  guiaAtualizar(dt);
  if (uxAviso[0] && SDL_TICKS_PASSED(agora, uxAvisoAte)) uxAviso[0] = 0;
  montarTela();
  valor[AJ_PERFIL_PESQ] = recomenda_pesquisavel() ? 0 : 1;   // V_LIGA: 0 = Ligado
  valor[AJ_FONTE_REGEX_MODELO] = fonteregra_modelo_atual();   // espelho do padrao (#202)
  p2pTesteRecolher();
  pstTesteRecolher();
  adTesteRecolher();
  skTesteRecolher();
  spRecolher();
  // "Procurar atualização" achou versao nova: abre o cartao por cima dos
  // Ajustes, como o OK em "Atualizar o aplicativo" ja fazia.
  if (atualizacao_busca_achou() && !atualizacao_aberta()) atualizacao_abrir();
  if (teclado_aberto() && !pessoas_aberto()) teclado_atualizar(dt, agora);
  // O resultado e CONSUMIDO NA LEITURA (ver teclado.h): ler duas vezes daria
  // TECLADO_NADA na segunda, e por isso a gravacao acontece aqui, uma vez.
  // COM A MODAL DE PESSOAS ABERTA O TECLADO E DELA: ler aqui consumiria o
  // resultado antes de ela ver (e o apelido digitado se perderia em silencio).
  { int r = pessoas_aberto() ? TECLADO_NADA : teclado_resultado();
    if (r == TECLADO_PRONTO && stCampo) {
      if (stCampo == AJ_STALKER_MAC) stalker_definir_mac(teclado_texto());
      else if (stCampo == AJ_XTREAM_SERVIDOR) xtream_definir_servidor(teclado_texto());
      else if (stCampo == AJ_XTREAM_USUARIO)  xtream_definir_usuario(teclado_texto());
      else if (stCampo == AJ_XTREAM_SENHA)    xtream_definir_senha(teclado_texto());
      else if (stCampo == AJ_FANART_CHAVE)    fanartDefinir(teclado_texto());
      else if (stCampo == AJ_SEEKR_CHAVE)     { seekrDefinir(teclado_texto()); atomic_store_explicit(&skTeste, 0, memory_order_release); }
      else if (stCampo == AJ_SELOS_PACOTE_ADD) spAdicionar(teclado_texto());
      else if (stCampo == AJ_P2P_URL)         ajustes_definir_p2p_url(teclado_texto());
      else if (stCampo == AJ_FONTE_REGEX_PADRAO) {
        int e = fonteregra_definir_regex(teclado_texto());
        sync_proteger_ajustes_locais();
        uxNotificar(e < 0 ? "Regex inválida: ignorada" : "Ajuste salvo nesta TV.");
      }
      else if (stCampo == AJ_JF_SERVIDOR)     jellyfin_definir_servidor(teclado_texto());
      else if (stCampo == AJ_EM_SERVIDOR)     emby_definir_servidor(teclado_texto());
      else if ((stCampo == AJ_JF_ENTRAR || stCampo == AJ_EM_ENTRAR) && !jfUsuario[0] && teclado_texto()[0]) {
        // Username typed: the password modal opens next, masked. The keyboard
        // buffer is wiped as soon as each value is handed over.
        snprintf(jfUsuario, sizeof jfUsuario, "%s", teclado_texto());
        teclado_esquecer();
        teclado_abrir_com(stCampo == AJ_EM_ENTRAR ? "Senha do Emby" : "Senha do Jellyfin", "", 128,
                          JF_ALFA_SENHA, NULL);
        teclado_tipo(TECLADO_TIPO_SENHA);
        teclado_mascarar(1);
        r = TECLADO_NADA;   // stay on this field: next result is the password
      } else if ((stCampo == AJ_JF_ENTRAR || stCampo == AJ_EM_ENTRAR) && jfUsuario[0]) {
        char senha[160];
        snprintf(senha, sizeof senha, "%s", teclado_texto());
        teclado_esquecer();
        if (stCampo == AJ_EM_ENTRAR) emby_entrar_senha(jfUsuario, senha);   // wipes senha
        else jellyfin_entrar_senha(jfUsuario, senha);                       // wipes senha
        memset(jfUsuario, 0, sizeof jfUsuario);
      }
      else if (stCampo >= AJ_POSTER_INST && stCampo <= AJ_POSTER_MODELO) pstDefinir(stCampo, teclado_texto());
      else if (debIdx(stCampo) >= 0)          debDefinir(stCampo, teclado_texto());
      else if (stCampo != AJ_JF_ENTRAR && stCampo != AJ_EM_ENTRAR) stalker_definir_portal(teclado_texto());
      if (r == TECLADO_PRONTO) stCampo = 0;
    } else if (r == TECLADO_CANCELOU) {
      if (stCampo == AJ_JF_ENTRAR || stCampo == AJ_EM_ENTRAR) { teclado_esquecer(); memset(jfUsuario, 0, sizeof jfUsuario); }
      stCampo = 0;
    } }
  // Repouso da escolha de idioma de legenda: ver aplicarIdioma.
  if (legendaEspera > 0.0f) {
    legendaEspera -= dt;
    if (legendaEspera <= 0.0f) { legendaEspera = 0.0f; addons_legendas_reiniciar(); }
  }
  for (int i = 0; i < AJ_N_TELA; i++) {
    // Com o foco na coluna de categorias a linha DESCANSA: o preenchimento
    // de realce e o do foco, e o foco esta na categoria — duas superficies
    // claras ao mesmo tempo diriam "voce esta em dois lugares".
    float alvo = (i == focoItem && !focoIndice && !uxCabLayout && uxIndice >= 2) ? 1.0f : 0.0f;
    animItem[i] = ajustes_animacoes_reduzidas() ? alvo : anim_mola(animItem[i], alvo, dt,
                            alvo > animItem[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  // Rola o minimo para o item focado caber, e leva junto o que o rotula: o
  // cabecalho da categoria quando ele e o primeiro com foco dela (sem isso,
  // entrar numa categoria mostra a linha sem dizer onde se esta) e o rotulo de
  // bloco logo acima dele — um bloco que comeca fora da tela vira uma lista
  // sem titulo.
  float topo = yDoItem(focoItem);
  if (focoItem == primeiroDaSecao(secAtual)) topo = 0.0f;
  else if (focoItem > 0 && TELA[focoItem - 1].tipo == IT_ROT) topo -= alturaItem(focoItem - 1);
  float base = yDoItem(focoItem) + alturaFoco(focoItem);
  // UM GRUPO ABERTO QUER SER VISTO INTEIRO, ou quanto couber: com o foco no
  // cabecalho, a rolagem estica a base ate a ultima opcao dele. Sem isto o OK
  // abria o grupo abaixo da borda e parecia nao ter feito nada.
  // "Mais opcoes" aberto: o mesmo, ate o fim da categoria (o que coube).
  if (TELA[focoItem].tipo == IT_MAIS && maisAberto[secAtual]) {
    float fim = yDoItem(secFim(secAtual) - 1) + alturaItem(secFim(secAtual) - 1);
    if (fim - topo > AJ_BASE - AJ_TOPO) fim = topo + (AJ_BASE - AJ_TOPO);
    if (fim > base) base = fim;
  }
  if (TELA[focoItem].tipo == IT_GRP && grupoAberto[secAtual] == focoItem) {
    int i = focoItem + 1;
    while (i < secFim(secAtual) && grupoDoItem[i] == focoItem) i++;
    { float fim = yDoItem(i - 1) + AJ_LINHA_H;
      if (fim - topo > AJ_BASE - AJ_TOPO) fim = topo + (AJ_BASE - AJ_TOPO);
      if (fim > base) base = fim; }
  }
  float alvo = scrollY;
  // PAGINA NOVA: a lista passa a ser outra categoria. A rolagem nao anima de
  // uma lista para a outra — recomeca do topo, e a pagina entra por
  // `paginaA` (um deslize curto na lista, nao no indice).
  { static int secVista = -1;
    int secAgora = secAtual;
    if (secAgora != secVista) {
      if (secVista >= 0) paginaA = 0.0f;
      secVista = secAgora; scrollY = 0.0f; velY = 0.0f; alvo = 0.0f;
    } }
  paginaA = ajustes_animacoes_reduzidas() ? 1.0f : anim_rampa(paginaA, 1.0f, dt, 220.0f);
  // GLASS UI: a linha em foco fica no MEIO da janela (a lista some nos 80 px
  // de baixo e, rolada, nos 60 de cima — no meio ela nunca encosta no
  // esvanecer), presa entre o topo e o fim da categoria.
  { float janela = AJ_BASE - AJ_TOPO, fim = 0.0f;
    int k;
    for (k = secIni[secAtual]; k < secFim(secAtual); k++) fim += alturaItem(k);
    alvo = (topo + base) * 0.5f - janela * 0.5f;
    if (alvo > fim - janela) alvo = fim - janela;
    if (topo - alvo < 0.0f) alvo = topo; }
  if (alvo < 0.0f) alvo = 0.0f;
  // (O "cabecalho inteiro ou nenhum" saiu: o cabecalho agora e fixo na folha.)
  scrollY = anim_mola2_reduzida(&velY, scrollY, alvo, dt, NV_MOLA2_SCROLL,
                                ajustes_animacoes_reduzidas());
  aj2Atualizar(dt);
  AJ_ESCALA_FIM();
}

// Leva a escolha da linha para linguas.c. "Da conta" (indice 0) manda string
// vazia, que e como linguas.c representa "sem escolha local, siga a conta".
static void aplicarIdioma(int op) {
  const char *c = ling_opcao_codigo(valor[op]);
  if (op == AJ_LEG_LINGUA || op == AJ_LEG_LINGUA2) {
    if (op == AJ_LEG_LINGUA) ling_local_legenda(c); else ling_local_legenda2(c);
    // A lista de legendas do titulo carregado foi montada com o idioma ANTERIOR
    // e nao se refaz sozinha (ver addons_legendas_reiniciar). Mas NAO refazer
    // aqui, e sim depois de a pessoa PARAR de mexer: esta funcao roda a cada
    // toque de esquerda/direita, a lista tem trinta idiomas, e no controle da
    // TV a seta repete sozinha — atravessar a lista dispararia dezenas de
    // buscas, cada uma consultando todos os addons de legenda.
    legendaEspera = 0.7f;
  } else {
    // O audio nao precisa de nada equivalente: as faixas de audio vem do
    // arquivo que esta tocando e a preferencia so escolhe entre as que ja
    // existem (video.c), sem consultar a rede.
    ling_local_audio(c);
  }
}

// Quantos valores uma linha de escolha tem. As duas linhas de idioma sao as
// unicas dinamicas: a lista vem de linguas.c e nao da tabela OPCOES, que e
// const e foi escrita antes de linguas.c existir.
static int nValores(int op) {
  if (op == AJ_LEG_LINGUA || op == AJ_LEG_LINGUA2 || op == AJ_AUD_LINGUA) return nLingua > 0 ? nLingua : 1;
  if (op == AJ_SELOS_PACOTE) return 1 + selospacote_n();
#ifndef __EMSCRIPTEN__
  // "YouTube" (o 4o valor) so toca no .wgt da Samsung (trailerfonte.c, existe).
  // Aqui ele era escolhivel e deixava a TV sem trailer nenhum: sem trailer no
  // destaque e sem botao Trailer na pagina (#204, log da 1.6.5: "detalhe: sem
  // trailer (ajuste 3, apple tem, imdb tem, youtube n/a)"). Fora da lista, o 3
  // ja gravado le como fora da faixa e fica o padrao, Automatico (limita).
  if (op == AJ_TRAILER_FONTE) return 3;
#endif
  return OPCOES[op].n;
}

// Texto do valor de uma linha. Buffer estatico porque so uma linha e desenhada
// por vez dentro de desenhaLinha.
static const char *textoValor(int op) {
  static char buf[48];
  const Opcao *o = &OPCOES[op];
  if (o->tipo == OP_LEITURA || o->tipo == OP_ACAO) return textoLeitura(op);
  if (op == AJ_HERO_TRAILER_ESPERA) {
    // Decimos de segundo: 22 le "2,2 s".
    int v = valor[op];
    snprintf(buf, sizeof buf, i18n("%d,%d s"), v / 10, v % 10);
    return buf;
  }
  if (o->tipo == OP_NUMERO) {
    snprintf(buf, sizeof buf, "%d%s", valor[op], o->sufixo ? o->sufixo : "");
    return buf;
  }
  if (op == AJ_LEG_LINGUA || op == AJ_LEG_LINGUA2 || op == AJ_AUD_LINGUA) {
    int v = valor[op];
    return (v >= 0 && v < nLingua && V_LINGUA[v]) ? V_LINGUA[v] : "Da conta";
  }
  if (op == AJ_CACHE_SEEK && !cacheSeekExiste()) return "Não disponível nesta TV";
  if (op == AJ_DTS_AC3 && !dtsConversaoExiste()) return "Não disponível nesta TV";
  if (op == AJ_SELOS_PACOTE) {
    int v = valor[op];
    return v > 0 && v <= selospacote_n() ? selospacote_nome(v - 1) : "Do Nuvio";
  }
  // FORA DO INTERVALO NAO LE FORA DO VETOR.
  //
  // Esta linha era `return o->valores[valor[op]]`, sem conferir nada, e foi o
  // segfault que o harness de Ajustes pegou: `valor[]` e um vetor POSICIONAL
  // inicializado a mao, e uma opcao nova inserida no meio do enum desloca todos
  // os padroes seguintes — o 7 do limite de fileiras foi parar numa lista de
  // dois itens, e `valores[7]` e lixo que vira ponteiro de string.
  //
  // O deslocamento e um defeito a parte e esta consertado logo abaixo; ESTA
  // guarda fica de qualquer jeito, porque `valor[]` tambem vem do DISCO: um
  // ajustes.txt de outra versao, ou editado a mao, derruba o app na primeira
  // vez que a linha aparece na tela. Valor invalido tem de ler como "o
  // primeiro", nunca como um endereco qualquer da memoria.
  if (!o->valores || o->n <= 0) return "";
  { int v = valor[op];
    if (v < 0 || v >= o->n) v = 0;
    return o->valores[v] ? o->valores[v] : ""; }
}

// SEM ICONE POR LINHA. Havia um (dono, 20/09/2026), escolhido pela FAMILIA da
// opcao — e por isso repetido: as nove linhas do Continuar assistindo levavam
// o mesmo simbolo, e as quatorze do TMDB tambem. Um icone igual em toda linha
// nao ajuda a achar nenhuma, e comia 60 px de rotulo. A familia agora e dita
// pelo GRUPO (a linha com titulo e descricao acima do bloco), como no web,
// onde as linhas de ajuste nao tem icone. O icone continua onde distingue: na
// coluna de categorias, no cabecalho dos grupos e no painel da direita — os
// aj_* do Lucide de TELA[]. O mapa por opcao (iconeOpcao, 99 casos, merge da
// 1.5) saiu junto: sem linha que o desenhe ele so segurava PNG no pacote.

// Superficies calmas como no prototipo: o foco tem borda precisa, sem halo.
// Opacas para manter o contraste dos textos mesmo com Vidro ligado na Home.
static void desenhaSuperficie(GfxRect r, float raio, float f, float a) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  if (ar > 0.97f && ag > 0.97f && ab > 0.97f) { ar = 0.933f; ag = 0.949f; ab = 0.98f; }
  gfx_cor(r, raio, 0.090f, 0.106f, 0.137f, a);
  if (f > 0.01f) {
    gfx_cor(r, raio, ar, ag, ab, f * a);
    gfx_anel_fora(r, raio, 4, 2, 0.82f, 0.87f, 0.98f, f * a);
  }
}

// LIGADO/DESLIGADO: a pilula das linhas de addon do guia (guia.c), e nao um
// interruptor de celular — la ja esta escrito por que: a 3 m o trilho com a
// bolinha nao se le, e a palavra se le. Ligado = pilula preenchida; Desligado
// = so o anel. No foco a pilula usa a TINTA do acento (ajustes_tinta_foco),
// para continuar contrastando sobre qualquer cor de destaque.
#define AJ_PILULA_W 136.0f
#define AJ_PILULA_H  40.0f
static void desenhaInterruptor(float xDir, float y, float h, int ligado,
                               int emFoco, float a) {
  GfxRect pill = { xDir - AJ_PILULA_W, y + (h - AJ_PILULA_H) * 0.5f,
                   AJ_PILULA_W, AJ_PILULA_H };
  int tf = ajustes_tinta_foco();
  TxtLinha t;
  if (ligado) {
    int ct;
    if (emFoco) { float c = tf / 255.0f; gfx_cor(pill, 0.5f, c, c, c, a); ct = tf > 128 ? 20 : 240; }
    else        { gfx_cor(pill, 0.5f, 0.86f, 0.865f, 0.88f, a); ct = 20; }
    t = txt_linha(TXT_CAPTION, i18n("Ligado"), ct, ct, ct, 255);
  } else {
    float c = emFoco ? tf / 255.0f : 0.72f;
    int ct = emFoco ? tf : 190;
    gfx_rect(pill, 0, GFX_ANEL, 0, 0.05f, 0, 0.5f, c, c, c, 0.9f * a);
    t = txt_linha(TXT_CAPTION, i18n("Desligado"), ct, ct, ct, 255);
  }
  txt_desenhar_alpha(t, pill.x + (pill.w - t.w) * 0.5f, pill.y + (pill.h - t.h) * 0.5f, a);
}

// MARCAS DE FORMATO NAS LINHAS (29/09/2026). O titulo de "Dolby Vision" e
// "Dolby Atmos" ganha o logo ao lado; o valor "4K/1080p/720p" da qualidade
// maxima vira a marca. Os valores continuam sendo as mesmas strings (o que
// ajustes_qualidade() devolve e streams.c compara): so o DESENHO muda.
#define AJ_MARCA_ROTULO_H 44.0f
#define AJ_MARCA_VALOR_H  40.0f
static int marcaDaOpcao(int op) {
  return op == AJ_DV ? FMT_DV : op == AJ_ATMOS ? FMT_ATMOS : -1;
}
static int marcaDoValor(int op, const char *v) {
  if (op != AJ_QUALIDADE || !v) return -1;
  if (!strcmp(v, "4K"))    return FMT_4K;
  if (!strcmp(v, "1080p")) return FMT_1080;
  if (!strcmp(v, "720p"))  return FMT_720;
  return -1;
}
static void desenhaValorLinha(TxtLinha val, int fv, float x, float yLinha, float vy,
                              int cv, float a) {
  if (fv < 0) { txt_desenhar_alpha(val, x, vy, a); return; }
  { float k = cv / 255.0f;
    marca_formato((FormatoMarca)fv, x, yLinha + (AJ_LINHA_H - AJ_MARCA_VALOR_H) * 0.5f,
                  AJ_MARCA_VALOR_H, k, k, k, a); }
}

// TRES NATUREZAS DE LINHA, TRES CAUDAS (a regra do topo do arquivo, agora no
// vocabulario do web): interruptor = pilula Ligado/Desligado; lista de valores,
// numero e acao = valor + chevron "›" (o `renderActionRow` do web: "tem mais
// atras deste OK"); leitura = so o valor, apagado, sem chevron.
static void desenhaLinha(int item, float y, float f, float dx, float aPag) {
  int op = TELA[item].op;
  if (y + AJ_LINHA_H < AJ_TOPO - 40.0f || y > AJ_BASE + 40.0f) return;
  // Some antes de cruzar o titulo da tela, como as secoes da pagina de detalhe:
  // texto passando por baixo de texto se le como borrao.
  float a = anim_clamp((y - (AJ_TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f) * aPag;
  if (a <= 0.005f) return;

  int desligada = inativa(op);
  int podeMudar = mutavel(op);
  int acao = OPCOES[op].tipo == OP_ACAO;
  float recuo = grupoDoItem[item] >= 0 ? AJ_RECUO : 0.0f;
  GfxRect linha = { AJ_LISTA_X + dx + recuo, y, AJ_LISTA_W - recuo, AJ_LINHA_H };
  desenhaSuperficie(linha, AJ_RAIO, f, a);
  // Texto ja rasterizado nao muda de cor, e pedir uma rasterizacao por passo
  // da mola encheria o cache de linhas: a cor troca no meio do caminho.
  int emFoco = (f > 0.5f);

  // Uma linha inativa fica visivelmente mais apagada QUE a de leitura: leitura e
  // informacao, inativa e "isto existe mas depende de outra coisa".
  float aTexto = a * (desligada ? 0.65f : 1.0f);
  int cr = emFoco ? AJ_TEXTO_ESCURO : ((podeMudar || acao) ? 240 : 192);
  float xDir = linha.x + linha.w - AJ_PAD;
  float cauda;   // largura ocupada pela cauda, para o rotulo cortar antes dela

  {
    const char *v = textoValor(op);
    int cv = emFoco ? AJ_TEXTO_ESCURO2 : (podeMudar ? 184 : 168);
    // O "›" so onde o OK leva a algum lugar: escolha, numero e acao. Numa linha
    // inativa ele sai, porque ali o OK nao faz nada.
    int chevron = !desligada && (podeMudar || acao);
    TxtLinha chv = txt_linha(TXT_HEADLINE, "\xe2\x80\xba", cv, cv, cv, 255);
    float valorDir = chevron ? xDir - chv.w - 16.0f : xDir;
    TxtLinha val = txt_linha_corta(TXT_CAPTION, v, cv, cv, cv, 255, 340.0f);
    float vy = y + (AJ_LINHA_H - val.h) * 0.5f;
    // "4K", "1080p" e "720p" da qualidade maxima saem como MARCA. A largura da
    // marca substitui a do texto para o resto da conta (cauda, pilula de edicao).
    int fv = marcaDoValor(op, v);
    if (fv >= 0) val.w = (int)(marca_formato_largura((FormatoMarca)fv, AJ_MARCA_VALOR_H) + 0.5f);
    cauda = xDir - (valorDir - val.w);

    // MODO EDICAO: as setas trocam de lugar com o chevron. "◀ valor ▶" e o
    // sinal de que agora esquerda e direita mudam o valor, e so existe
    // depois do OK.
    if (podeMudar && emEdicao && f > 0.02f) {
      TxtLinha dir = txt_linha(TXT_CAPTION2, "\xe2\x96\xb6", cv, cv, cv, 255);
      TxtLinha esq = txt_linha(TXT_CAPTION2, "\xe2\x97\x80", cv, cv, cv, 255);
      float vd = xDir - dir.w - 16.0f;
      GfxRect pill = { vd - val.w - esq.w - 30.0f, y + (AJ_LINHA_H - 40.0f) * 0.5f,
                       val.w + esq.w + dir.w + 60.0f, 40.0f };
      gfx_cor(pill, 0.5f, 0.0f, 0.0f, 0.0f, 0.14f * a);
      txt_desenhar_alpha(dir, xDir - dir.w, y + (AJ_LINHA_H - dir.h) * 0.5f, aTexto * f);
      txt_desenhar_alpha(esq, vd - val.w - 14.0f - esq.w,
                         y + (AJ_LINHA_H - esq.h) * 0.5f, aTexto * f);
      desenhaValorLinha(val, fv, vd - val.w, y, vy, cv, aTexto);
    } else {
      if (chevron)
        txt_desenhar_alpha(chv, xDir - chv.w,
                           vy + (val.h - chv.h) * 0.5f - 2.0f, aTexto);
      desenhaValorLinha(val, fv, valorDir - val.w, y, vy, cv, aTexto);
    }
  }

  TxtLinha rot = txt_linha_corta(TXT_BODY, rotuloOpcao(op), cr, cr, cr, 255,
                                 linha.w - AJ_PAD * 2.0f - cauda - 28.0f);
  txt_desenhar_alpha(rot, linha.x + AJ_PAD, y + (AJ_LINHA_H - rot.h) * 0.5f, aTexto);
  if (uxDiferente(op) && rot.w + cauda + AJ_PAD * 2 + 48 < linha.w) {
    gfx_cor((GfxRect){linha.x + AJ_PAD + rot.w + 12, y + AJ_LINHA_H * 0.5f - 3, 6, 6},
            0.5f, 0.56f, 0.65f, 0.80f, aTexto);
  }
  // A MARCA AO LADO DO TITULO: "Dolby Vision" e "Dolby Atmos" mostram o logo
  // que a opcao liga. Fica depois do texto, centrada na linha, e so entra se
  // couber antes da cauda (a marca some antes de cortar o rotulo).
  { int fm = marcaDaOpcao(op);
    if (fm >= 0) {
      float mw = marca_formato_largura((FormatoMarca)fm, AJ_MARCA_ROTULO_H);
      float mx = linha.x + AJ_PAD + (float)rot.w + 22.0f;
      if (mx + mw < linha.x + linha.w - AJ_PAD - cauda - 20.0f) {
        float k = cr / 255.0f;
        marca_formato((FormatoMarca)fm, mx, y + (AJ_LINHA_H - AJ_MARCA_ROTULO_H) * 0.5f,
                      AJ_MARCA_ROTULO_H, k, k, k, aTexto);
      }
    } }
}

// GRUPO RECOLHIVEL: titulo e descricao a esquerda (a composicao das linhas de
// addon do guia), e a direita quantas opcoes ha dentro e o sinal de abrir.
// "+"/"−" e nao seta: a fonte embarcada nao tem o chevron para baixo do web,
// e o par mais/menos le a 3 m sem ambiguidade.
static void desenhaGrupo(int item, float y, float f, float dx, float aPag) {
  if (y + AJ_GRUPO_H < AJ_TOPO - 40.0f || y > AJ_BASE + 40.0f) return;
  float a = anim_clamp((y - (AJ_TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f) * aPag;
  if (a <= 0.005f) return;
  GfxRect linha = { AJ_LISTA_X + dx, y, AJ_LISTA_W, AJ_GRUPO_H };
  int aberto = grupoAberto[secDoItem[item]] == item;
  int emFoco = (f > 0.5f), n = 0, i;
  char qtd[48];
  desenhaSuperficie(linha, 12.0f / AJ_GRUPO_H, f, a);
  for (i = item + 1; i < AJ_N_TELA && grupoDoItem[i] == item; i++) n++;
  { int c1 = emFoco ? AJ_TEXTO_ESCURO : 240;
    int c2 = emFoco ? AJ_TEXTO_ESCURO2 : 168;
    TxtLinha sinal = txt_linha(TXT_HEADLINE, aberto ? "\xe2\x88\x92" : "+", c1, c1, c1, 255);
    float xDir = linha.x + linha.w - AJ_PAD;
    TxtLinha tq;
    float larg;
    snprintf(qtd, sizeof qtd, n == 1 ? i18n("%d opção") : i18n("%d opções"), n);
    tq = txt_linha(TXT_CAPTION, qtd, c2, c2, c2, 255);
    txt_desenhar_alpha(sinal, xDir - sinal.w, y + (AJ_GRUPO_H - sinal.h) * 0.5f - 2.0f, a);
    txt_desenhar_alpha(tq, xDir - sinal.w - 22.0f - tq.w, y + (AJ_GRUPO_H - tq.h) * 0.5f, a);
    // O ICONE DO BLOCO (Lucide, TELA[]): e ele, e nao a linha, que diz a
    // familia das opcoes de dentro. Mesmo lado do da coluna de categorias (32)
    // e a mesma tinta do titulo, para ler junto dele sobre o foco claro.
    float tx = linha.x + AJ_PAD;
    if (TELA[item].icone) {
      float ci = emFoco ? (focoEscuro() ? 0.10f : 1.0f) : 0.86f;
      gfx_icone((GfxRect){ tx, y + (AJ_GRUPO_H - AJ_IDX_ICONE) * 0.5f,
                           AJ_IDX_ICONE, AJ_IDX_ICONE },
                TELA[item].icone, ci, ci, ci, a);
      tx += AJ_IDX_ICONE + 20.0f;
    }
    larg = linha.x + linha.w - AJ_PAD - sinal.w - tq.w - 60.0f - tx;
    { TxtLinha t = txt_linha_corta(TXT_CALLOUT, TELA[item].titulo, c1, c1, c1, 255, larg);
      txt_desenhar_alpha(t, tx, y + 16.0f, a); }
    { TxtLinha t = txt_linha_corta(TXT_CAPTION, TELA[item].sub, c2, c2, c2, 255, larg);
      txt_desenhar_alpha(t, tx, y + 58.0f, a); } }
}

// Sobreposicao do vinculo (Trakt ou Simkl). O codigo destes dois e CURTO — 8
// caracteres no Trakt — e o endereco e fixo, entao da para ler da TV e digitar
// no celular. Nao precisa de QR, ao contrario dos 32 digitos hexadecimais do
// login da conta.
// O QR DO VINCULO. A folha so mostrava o codigo curto e o endereco — a pessoa
// tinha de abrir o navegador do celular, digitar trakt.tv/activate e DEPOIS o
// codigo. Com o simbolo a camera abre a pagina direto. Mesmo cuidado da tela
// de login (login.c): NEAREST, fundo claro com zona de silencio.
static GLuint texQrVin;
static char   qrVinDe[256];

static void qrVinTex(const char *texto) {
  Qr q; int lado, x, y; unsigned char *px;
  if (!texto || !texto[0]) return;
  if (!strcmp(qrVinDe, texto) && texQrVin) return;
  if (!qr_gerar(&q, texto)) return;
  lado = q.lado + 8;                    // 4 modulos de silencio por lado
  px = (unsigned char *)malloc((size_t)lado * lado * 3);
  if (!px) return;
  memset(px, 255, (size_t)lado * lado * 3);
  for (y = 0; y < q.lado; y++)
    for (x = 0; x < q.lado; x++)
      if (qr_modulo(&q, x, y)) {
        size_t i = ((size_t)(y + 4) * lado + (x + 4)) * 3;
        px[i] = px[i + 1] = px[i + 2] = 0;
      }
  if (!texQrVin) glGenTextures(1, &texQrVin);
  glBindTexture(GL_TEXTURE_2D, texQrVin);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lado, lado, 0, GL_RGB,
               GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  free(px);
  snprintf(qrVinDe, sizeof qrVinDe, "%s", texto);
}

static void desenhaVinculo(const char *servico, const char *codigo,
                           const char *endereco, const char *falha, int esperando);

// --- COLUNA DE SECOES -------------------------------------------------------
// Ela nao e enfeite: e o unico caminho entre categorias que o controle da TV
// alcanca (ver a nota em ajustes_evento). Por isso ela esta SEMPRE visivel, e
// nao so quando tem foco — um atalho que a pessoa nao ve nao existe, foi o que
// aconteceu com PgUp/PgDn.
static void desenhaIndice(void) {
  float y = AJ_TOPO;
  int j;
  for (j = 0; j < nSecoes + 2; j++) {
    if (j == 2) {
      gfx_cor((GfxRect){AJ_IDX_X + 14, y + 6, AJ_IDX_W - 28, 1}, 0, 0.16f, 0.18f, 0.22f, 1);
      y += 24;
    }
    int selecionado = j == uxIndice, foco = selecionado && focoIndice;
    const char *titulo = j == 0 ? "Buscar ajuste" : j == 1 ? "Diferentes do padrão" : TELA[secIni[j - 2]].titulo;
    const char *icone = j == 0 ? "menu_search" : j == 1 ? "aj_rotate-ccw-clock" : TELA[secIni[j - 2]].icone;
    GfxRect r = { AJ_IDX_X, y, AJ_IDX_W, AJ_IDX_H };
    float c = foco ? (focoEscuro() ? 0.10f : 1.0f) : selecionado ? 0.94f : 0.67f;
    if (foco) desenhaSuperficie(r, 0.20f, 1, 1);
    else if (selecionado) gfx_cor(r, 0.20f, 0.114f, 0.145f, 0.204f, 1);
    gfx_icone((GfxRect){r.x + 14, y + 13, 28, 28}, icone, c, c, c, 1);
    TxtLinha t = txt_linha_corta(TXT_CAPTION, titulo, (int)(c * 255), (int)(c * 255), (int)(c * 255), 255, r.w - 66);
    txt_desenhar(t, r.x + 54, y + (r.h - t.h) * 0.5f);
    y += AJ_IDX_H + 4;
  }
}

// --- FOLHA "ORDENAR E ATIVAR FILEIRAS" --------------------------------------
#define AJ_FIL_W      1480.0f
#define AJ_FIL_H       920.0f
#define AJ_FIL_Y        80.0f
#define AJ_FIL_LINHA    76.0f
#define AJ_FIL_LGAP      6.0f
// SEIS, e nao sete. A linha cresceu para 76 para caber o nome do addon sob o
// titulo — sete linhas de 76 terminariam em y+578 e a ficha comeca em y+684.
#define AJ_FIL_VIS       6      // linhas desenhadas por vez

// As quatro colunas, em x relativo ao cartao. A primeira e a ALCA DE MOVER: e
// nela que OK pega e solta a fileira.
static const struct { float x, w; const char *cabec; } AJ_FIL_COL[AJ_FIL_CAMPOS] = {
  {   40.0f, 600.0f, "Fileira"  },
  {  670.0f, 190.0f, "Estado"   },
  {  880.0f, 250.0f, "Card"     },
  { 1150.0f, 250.0f, "Tamanho"  },
};

// POR QUE ESTA FILEIRA NAO ESCOLHE A FORMA DO CARD. Dizer "Fixo" e deixar a
// pessoa apertando OK sem efeito e o mesmo defeito das linhas inativas da lista
// principal: a dependencia tem de ficar visivel.
static const char *motivoFormaFixa(const char *chave) {
  if (!strcmp(chave, "continue_watching"))
    return "A forma desta fileira vem de Estilo do \"Continuar assistindo\".";
  if (!strcmp(chave, "social_activity"))
    return "O feed dos amigos usa o card com o nome de quem assistiu.";
  return "A forma desta fileira não é escolhida aqui.";
}

// PREVIA DAS FORMAS DE CARD, desenhada com as MEDIDAS DE VERDADE.
//
// A coluna "Card" oferece sete nomes e a de "Tamanho" tres nomes, e nada dizia
// o que cada um faz com a fileira — "Coleção" e "Serviço" sao os dois arte
// deitada, e a diferenca entre eles e so tamanho. Pedido do dono: "coloque mais
// informacoes, imagens e svg sobre cada tipo de card na fileira que nao tem
// info sobre, e do tamanho tambem".
//
// Os numeros abaixo sao os MESMOS de home.c (larguraDe/alturaDe), reduzidos por
// um fator unico — e por isso a previa mostra a proporcao E a diferenca de
// tamanho entre as formas, que e justamente o que o nome nao diz. Se home.c
// mudar uma medida, esta previa passa a mentir; a tabela cita a fonte para que
// quem mexer la saiba que ha um segundo lugar.
static const struct { float w, h; } AJ_FIL_FORMA[FIL_TIPO_N] = {
  { 212.0f, 322.0f },   // AUTO     — desenhado como fantasma, ver abaixo
  { 212.0f, 322.0f },   // CARTAZ   — NV_CARD_W x NV_CARD_H
  { NV_DESTAQUE_EDITORIAL_W, NV_DESTAQUE_EDITORIAL_H }, // DESTAQUE — 16:9
  { 480.0f, 270.0f },   // COLECAO
  { 360.0f, 203.0f },   // SERVICO
  { 212.0f, 320.0f },   // TOP10
  { NV_DESTAQUE_QUADRADO_W, NV_DESTAQUE_QUADRADO_H }, // DESTAQUE 4:3
  { 212.0f, 322.0f },   // RANKING  — cartaz NV_CARD_W x NV_CARD_H, numeral no vao
  { NV_DIN_LARGA_W, NV_DIN_LARGA_H }, // LARGA — faixa 16:9 com o nome dentro
  // Os dois 4:3 maiores: o fator de fil_tipo_fator (fileiras.c) sobre o 4:3.
  { NV_DESTAQUE_QUADRADO_W * 1.25f, NV_DESTAQUE_QUADRADO_H * 1.25f }, // 4:3 MEDIO
  { NV_DESTAQUE_QUADRADO_W * 1.5f,  NV_DESTAQUE_QUADRADO_H * 1.5f },  // 4:3 GRANDE
};

// Uma frase por forma. Diz o que a forma E e para que serve, nao como se chama.
static const char *aj_fil_forma_ajuda(int t) {
  switch (t) {
    case FIL_TIPO_CARTAZ:   return i18n("Cartaz em pé 2:3, o mesmo das fileiras de catálogo.");
    case FIL_TIPO_DESTAQUE: return i18n("Arte deitada panorâmica 16:9, como a faixa Destaques.");
    case FIL_TIPO_COLECAO:  return i18n("Arte deitada média: cabe mais que a grande e ainda mostra o cenário.");
    case FIL_TIPO_SERVICO:  return i18n("Arte deitada compacta: a que cabe mais títulos por fileira.");
    case FIL_TIPO_TOP10:    return i18n("Os cartazes empilhados num card só; OK abre a fileira com o número do ranking sobre cada cartaz.");
    case FIL_TIPO_RANKING:  return i18n("Número grande ao lado de cada cartaz, como o Top 10 da Dinâmica. Mostra todos os itens da fileira.");
    case FIL_TIPO_DESTAQUE_QUADRADO:
      return i18n("Arte maior em 4:3: recorta a capa para preencher todo o card.");
    case FIL_TIPO_DESTAQUE_QUADRADO_M:
      return i18n("Arte em 4:3 ainda maior: cabem dois cards e meio por tela.");
    case FIL_TIPO_DESTAQUE_QUADRADO_G:
      return i18n("A maior arte em 4:3: dois cards e um pedaço do próximo por tela.");
    case FIL_TIPO_LARGA:    return i18n("Arte deitada 16:9 com o nome do título dentro do card.");
    default:                return i18n("O app escolhe pela fileira: retomada e coleções já têm forma própria.");
  }
}


// COR DE PRESSAO do cache: verde com folga, amarelo perto do teto, vermelho
// encostado. Pedido do dono (16/09): o grafico e a barra passam a dizer com
// cor o que o numero diz com digitos. As faixas vem do comportamento medido
// em tex_cache.c: acima de ~90% o cache despeja a cada arte nova (encostado),
// entre 70 e 90 ele ainda absorve uma tela de fileiras sem despejar.
// Tons ABAFADOS (Glass UI, 03/10): e cor de estado, nao de acento, e o verde
// vivo de antes gritava mais que o resto do painel.
static void corPressao(float t, float *r, float *g, float *b) {
  if (t < 0.70f)      { *r = 0.298f; *g = 0.765f; *b = 0.541f; }   // #4cc38a
  else if (t < 0.90f) { *r = 0.910f; *g = 0.722f; *b = 0.290f; }   // #e8b84a
  else                { *r = 0.898f; *g = 0.325f; *b = 0.294f; }   // #e5534b
}


// Previews em foco por opção. Cada família desenha a mesma superfície do app
// e realça a peça que a opção altera; os controles binários usam `valor[]`.
// Não mostram valores de conta/chaves e não encenam conclusão de ações remotas.
typedef enum {
  AJPV_REPRO, AJPV_HOME, AJPV_CONTINUAR, AJPV_DETALHE, AJPV_FOCO,
  AJPV_PROFUNDIDADE, AJPV_CARTAZ, AJPV_INTERFACE, AJPV_CONTA,
  AJPV_RASTREIO, AJPV_ABOUT, AJPV_TMDB, AJPV_MDB, AJPV_TV, AJPV_ACAO
} AjPreview;

static AjPreview familiaPreviaOpcao(int op) {
  switch (op) {
    case AJ_LEG2_POS: case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA:
    case AJ_QUALIDADE: case AJ_DV: case AJ_ATMOS: case AJ_LEG_LINGUA: case AJ_LEG_LINGUA2: case AJ_CACHE_SEEK:
    case AJ_DV_MKV:
    case AJ_LEG_FORCADA:
    case AJ_AUD_LINGUA: case AJ_PAUSA_OVERLAY: case AJ_PLR_CLASSIF: case AJ_FONTE_MANUAL:
    case AJ_FONTE_AUTO: case AJ_FONTE_REPOR: case AJ_FONTE_TEXTO: case AJ_FONTE_ORDEM_ADDON: case AJ_SELOS_CORES: case AJ_FONTE_TOCAR_CONFERINDO: case AJ_FONTE_AQUECER: case AJ_FONTE_CONFERIR_VARIAS: case AJ_FONTE_PREPARAR:
    case AJ_FONTE_PRIORIDADE: case AJ_FONTE_HDR:
    case AJ_SELOS_PACOTE:
    case AJ_REACAO_CREDITOS:
    case AJ_FONTE_PRAZO: case AJ_TAM_MAX: case AJ_TAM_MIN: case AJ_PROPORCAO_PADRAO: case AJ_DTS_AC3:
    case AJ_FONTE_ESCOPO: case AJ_FONTE_ADDONS_PERM: case AJ_FONTE_PLUGINS_PERM: case AJ_FONTE_OUTROS:
    case AJ_FONTE_REGEX: case AJ_FONTE_REGEX_PADRAO: case AJ_FONTE_REGEX_MODELO:
    case AJ_FONTE_ORDEM: case AJ_FONTE_ORDEM_USO:
      return AJPV_REPRO;
    case AJ_HOME_LAYOUT:
    case AJ_LANDSCAPE: case AJ_HERO_CHEIO: case AJ_HERO_FUNDO:
    case AJ_HERO_ARTE_DIF: case AJ_HERO_TRAILER: case AJ_FIL_LIMITE:
    case AJ_ESPACO_FILEIRAS: case AJ_ESPACO_TITULOS:
    case AJ_HERO_TRAILER_SOM: case AJ_HERO_TRAILER_ESPERA: case AJ_HERO_TRANSICAO: case AJ_LOGO_TRAILER:
    case AJ_ADDON_FUNDO: case AJ_ADDON_LOGO:
    case AJ_FIL_ORDEM: case AJ_RAIL: case AJ_RAIL_MODERNA:
    case AJ_RAIL_BLUR: case AJ_HERO: case AJ_HERO_CATALOGOS:
    case AJ_PS_FUNDO: case AJ_PS_CONTINUAR: case AJ_DESCOBRIR: case AJ_ROTULOS:
    case AJ_NOME_ADDON: case AJ_SUFIXO_TIPO: case AJ_OCULTAR_NLANC:
    case AJ_NOTAS_HOME: case AJ_GRAD_CLASSICO: case AJ_SELO_VISTO:
      return AJPV_HOME;
    case AJ_CW_LIGADO: case AJ_CW_OK: case AJ_CW_FONTE:
    case AJ_CW_ESTILO: case AJ_CW_THUMB: case AJ_CW_BLUR_PROX:
    case AJ_CW_FURTHEST: case AJ_CW_NAO_EXIBIDOS: case AJ_CW_ORDEM: case AJ_CW_RETIDO_TAMBEM:
      return AJPV_CONTINUAR;
    case AJ_DET_BLUR_NAO_VISTOS: case AJ_DET_TRAILER: case AJ_DET_META_EXT:
    case AJ_DET_SO_CINEMETA: case AJ_BUSCA_CINEMETA: case AJ_BUSCA_NUVIO: case AJ_BUSCA_ORIGEM:
    case AJ_DET_DATA_CHEIA: case AJ_DET_VEU: case AJ_DET_TRAILER_AUTO:
    case AJ_DET_TRAILER_SOM:
    case AJ_TRAILER_QUAL: case AJ_TRAILER_ASPECTO: case AJ_TRAILER_FONTE: case AJ_TRAILER_ZOOM_TPK:
      return AJPV_DETALHE;
    case AJ_EXPANDIR: case AJ_EXPANDIR_ATRASO: case AJ_NAV_RAPIDA:
    case AJ_BORDA_FOCO:
      return AJPV_FOCO;
    case AJ_PROF: case AJ_PROF_BORDA: case AJ_PROF_BRILHO:
    case AJ_PROF_COBERTURA: case AJ_PROF_POSTERS: case AJ_PROF_CW:
    case AJ_PROF_EPS: case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS:
      return AJPV_PROFUNDIDADE;
    case AJ_LARGURA_DP: case AJ_RAIO_DP:
      return AJPV_CARTAZ;
    case AJ_IDIOMA: case AJ_ANIM: case AJ_TEMA:
    case AJ_COR_LOGO: case AJ_FONTE_UI: case AJ_VIDRO: case AJ_VIDRO_CONTORNO:
    case AJ_RELOGIO: case AJ_RELOGIO_POS: case AJ_SAIDA_PLAYER: case AJ_RELOGIO_12H:
    case AJ_TAMANHO_UI: case AJ_TAMANHO_AJUSTES: case AJ_LAYOUT_AJUSTES: case AJ_FUNDO: case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO:
    case AJ_AVANCADAS: case AJ_LOGO_APP: case AJ_ABERTURA:
    case AJ_ESMAECER: case AJ_BRILHO_PLAYER: case AJ_MANTER_VIDEO:
    case AJ_DESCANSO_ESTILO: case AJ_DESCANSO_FONTE:
      return AJPV_INTERFACE;
    case AJ_PERFIL_ATIVO: case AJ_SYNC: case AJ_SAIR:
    case AJ_PERFIL_PESQ: case AJ_PERFIL_EDITAR: case AJ_ADDONS_PRINCIPAL: case AJ_ENQUETES:
      return AJPV_CONTA;
    case AJ_SALVOS_DEST: case AJ_TRAKT: case AJ_SIMKL: case AJ_DISCORD: case AJ_HIST_CONTA: case AJ_SOCIAL:
      return AJPV_RASTREIO;
    case AJ_VERSAO_I: case AJ_ATUALIZAR: case AJ_ENVIAR_LOG: case AJ_GUIA: case AJ_NOVIDADES20: case AJ_APOIAR:
    case AJ_ENVIO_AUTO: case AJ_VER_REGISTRO:
      return AJPV_ABOUT;
    case AJ_TMDB_LIGADO: case AJ_TMDB_IDIOMA: case AJ_TMDB_ARTE:
    case AJ_TMDB_BASICO: case AJ_TMDB_FICHA: case AJ_TMDB_DATAS:
    case AJ_TMDB_ELENCO: case AJ_TMDB_PROD: case AJ_TMDB_REDES:
    case AJ_TMDB_EPS: case AJ_TMDB_TRAILERS: case AJ_TMDB_MAIS:
    case AJ_TMDB_COL: case AJ_TMDB_CW:
      return AJPV_TMDB;
    case AJ_MDB_LIGADO: case AJ_MDB_CHAVE: case AJ_MDB_TRAKT:
    case AJ_MDB_IMDB: case AJ_MDB_TMDB: case AJ_MDB_LETTER:
    case AJ_MDB_TOMATES: case AJ_MDB_AUDIENCIA: case AJ_MDB_META:
    case AJ_MDB_MAL:
    case AJ_NT_IMDB: case AJ_NT_TOMATES: case AJ_NT_AUDIENCIA: case AJ_NT_META:
    case AJ_NT_METAUSER: case AJ_NT_TRAKT: case AJ_NT_TMDB: case AJ_NT_LETTER:
    case AJ_NT_MAL: case AJ_NT_EBERT: case AJ_NT_SCORE:
      return AJPV_MDB;
    case AJ_RESOLUCAO: case AJ_QUALIDADE_IMG: case AJ_TEX_MB:
    case AJ_ESPACO:
      return AJPV_TV;
    case AJ_ADDONS: case AJ_PLUGINS: case AJ_STALKER_PORTAL: case AJ_STALKER_MAC:
    case AJ_STALKER_LIMPAR: case AJ_XTREAM_SERVIDOR: case AJ_XTREAM_USUARIO:
    case AJ_XTREAM_SENHA: case AJ_XTREAM_LIMPAR: case AJ_FANART_CHAVE:
    case AJ_SEEKR_CHAVE: case AJ_SEEKR_TESTAR:
    case AJ_SELOS_PACOTE_ADD: case AJ_SELOS_PACOTE_REM:
    case AJ_XTREAM_CONTA:
    case AJ_DIAGNOSTICO: case AJ_VELOCIDADE: case AJ_LIVETV_DIAG:
    case AJ_P2P_URL: case AJ_P2P_TESTAR:
    case AJ_JF_SERVIDOR: case AJ_JF_ENTRAR: case AJ_JF_SAIR:
    case AJ_EM_SERVIDOR: case AJ_EM_ENTRAR: case AJ_EM_SAIR:
    case AJ_PX_ENTRAR: case AJ_PX_SERVIDOR: case AJ_PX_SAIR:
    case AJ_POSTER_INST: case AJ_POSTER_TOKEN: case AJ_POSTER_EXTRA:
    case AJ_POSTER_CHAVE: case AJ_POSTER_MODELO: case AJ_POSTER_TESTAR:
    case AJ_DEBRID_AD: case AJ_DEBRID_AD_TESTAR: case AJ_DEBRID_RD:
    case AJ_DEBRID_TB: case AJ_DEBRID_PM:
      return AJPV_ACAO;
    default:
      return (AjPreview)-1;
  }
}

#include "ajustes_ux_desenho.inc"

#ifdef AJUSTES_TESTE
void teclado_teste_texto(const char *t);
void teclado_teste_foco(int f, int c);
static int ajQuadroAddons;   // captura: a tela de addons no lugar de Ajustes
static int ajQuadroPlugins;  // captura: a de plugins (foco + 1)
#endif
static void ajDesenharTudo(Uint32 agora);
// Own Settings scale: the virtual canvas and the active drawing factor agree.
void ajustes_desenhar(Uint32 agora) {
  AJ_ESCALA_INI();
  ajDesenharTudo(agora);
  AJ_ESCALA_FIM();
}
static void ajDesenharTudo(Uint32 agora) {
  // A TELA E DONA DO PROPRIO FUNDO (Glass UI): a arte do titulo com o veu, e
  // as tres ilhas por cima. Ver ajustes_ux_desenho.inc.
#ifdef AJUSTES_TESTE
  if (ajQuadroAddons) { ajustes_desenhar_addons(ajQuadroAddons - 1); return; }
  if (ajQuadroPlugins) { AjPluginsVista v = { 0, ajQuadroPlugins - 1, 0, 0, "" }; ajustes_desenhar_plugins(&v); return; }
#endif
  montarTela();
  if (guiaAberto) { ponteiro_camada(); guiaDesenhar(); return; }
  ajDesenharTela();

  // A folha de fileiras cobre a lista; o vinculo cobre as duas, porque ele e a
  // unica coisa aqui com prazo (o codigo do dispositivo expira).
  // A folha de fileiras fica em 1080p em qualquer "Tamanho da interface": e
  // uma tabela de quatro colunas com o painel de previa ao lado, desenhada
  // para a largura inteira; na tela virtual de 150% (1280) as colunas se
  // atropelam. Ela ja ocupa a tela toda em 100%.
  // Ponteiro (#99): a folha de fileiras tem o teclado; sem alvo proprio, o
  // clique e o OK (ponteiro.h, CAMADAS) — a rail de tras nao pode engoli-lo.
  if (filAberta) { ponteiro_camada(); ESCALA_REAL_INI(); desenhaFileiras(); ESCALA_REAL_FIM(); }
  if (riscoFolha) desenhaRiscoFolha();
  if (frAberta == 3) desenhaFolhaOrdem();
  else if (frAberta) desenhaFolhaPermitidos();

  // Por cima de tudo: enquanto um vinculo esta em andamento, ele e a pergunta
  // da tela.
  { TraEstado ta = traktauth_estado();
    SmkEstado sa = simklauth_estado();
#ifdef AJUSTES_TESTE
    if (ajVinculoTeste) desenhaVinculo("Trakt", "8F3K2QPA", "https://trakt.tv/activate", NULL, 1); else
#endif
    if (ta == TRA_PEDINDO || ta == TRA_AGUARDANDO || ta == TRA_ERRO)
      desenhaVinculo("Trakt", traktauth_codigo(), traktauth_url(),
                     traktauth_erro(), ta == TRA_AGUARDANDO);
    else if (sa == SMK_PEDINDO || sa == SMK_AGUARDANDO || sa == SMK_ERRO)
      desenhaVinculo("Simkl", simklauth_codigo(), simklauth_url(),
                     simklauth_erro(), sa == SMK_AGUARDANDO);
    else { DisEstado da = discord_estado();
      if (da == DIS_PEDINDO || da == DIS_AGUARDANDO || da == DIS_ERRO)
        desenhaVinculo("Discord", discord_codigo(), discord_url(),
                       discord_erro(), da == DIS_AGUARDANDO); } }

  // A modal de digitacao e a ultima: ela e sempre a pergunta mais recente da
  // tela, e tem de ficar por cima ate do cartao de vinculo.
  if (apoioAberto) apoioDesenhar();
  if (teclado_aberto()) teclado_desenhar(agora);
}

float ajustes_ilha_x(void) { return aj2X0() * ajustes_tamanho_ajustes(); }
int ajustes_relogio_cabe(void) {
  TraEstado ta = traktauth_estado();
  SmkEstado sa = simklauth_estado();
  if (teclado_aberto() || (ajModalAberto() && !filAberta)) return 0;
  if (ta == TRA_PEDINDO || ta == TRA_AGUARDANDO || ta == TRA_ERRO) return 0;
  if (sa == SMK_PEDINDO || sa == SMK_AGUARDANDO || sa == SMK_ERRO) return 0;
  return 1;
}

#ifdef AJUSTES_TESTE
int ajustes_teste_focar_opcao(int op) {
  int i;
  montarTela();
  if (op < 0 || op >= AJ_N) return 0;
  for (i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].tipo != IT_OPC || TELA[i].op != op) continue;
    focarOpcao(op); scrollY = velY = 0.0f;
    return 1;
  }
  return 0;
}

// Inventario das previas: a i-esima opcao visivel da TELA (op, categoria,
// chave do disco e rotulo) e a cena dela desenhada sozinha num instante fixo.
int ajustes_teste_cena_item(int i, int *op, int *sec, const char **chave, const char **rot) {
  int k, n = 0;
  montarTela();
  for (k = 0; k < AJ_N_TELA; k++) {
    if (TELA[k].tipo != IT_OPC || TELA[k].op < 0 || TELA[k].op >= AJ_N) continue;
    if (n++ != i) continue;
    *op = TELA[k].op; *sec = secDoItem[k];
    *chave = CHAVE[*op] ? (CHAVE[*op][0] == '-' ? CHAVE[*op] + 1 : CHAVE[*op]) : "";
    *rot = rotuloOpcao(*op);
    return 1;
  }
  return 0;
}
void ajustes_teste_cena_desenhar(int op, float t, float x, float y, float w) {
  ajcTesteT = t;
  ajCenaGerada(uxSecaoDe(op), op, x, y, w);
  ajcTesteT = -1.0f;
}
void ajustes_teste_tema(int tema, int vidro);
void ajustes_teste_ux_captura(int cenario) {
  memcpy(valor, valorPadrao, sizeof valor);
  ajustes_teste_vidro_env();   // o memcpy acima apagaria NUVIO_SHOT_VIDRO_*
  if (getenv("NUVIO_SHOT_TEMA") || getenv("NUVIO_SHOT_VIDRO"))   // idem NUVIO_SHOT_TEMA / _VIDRO
    ajustes_teste_tema(getenv("NUVIO_SHOT_TEMA") ? atoi(getenv("NUVIO_SHOT_TEMA")) : -1,
                       getenv("NUVIO_SHOT_VIDRO") && atoi(getenv("NUVIO_SHOT_VIDRO")));
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  uxCancelar(); uxAviso[0] = 0; uxRetornarOp = -1;
  scrollY = velY = 0; paginaA = 1;
  focarSecao(0); focoIndice = 1;
  if (cenario == 1 || cenario == 2) focarOpcao(AJ_HOME_LAYOUT);
  if (cenario == 2) { uxAbrirEditor(AJ_HOME_LAYOUT); uxEvento(SDLK_DOWN); }
  if (cenario == 3) { focarOpcao(AJ_HERO_TRAILER_ESPERA); uxAbrirEditor(AJ_HERO_TRAILER_ESPERA); }
  if (cenario == 4 || cenario == 5) {
    valor[AJ_TEMA] = 1; valor[AJ_RELOGIO] = 1;
    uxListarDiferencas(); uxIndice = 1; focoIndice = 0; uxDifFoco = 0;
    if (cenario == 5) { uxAbrirEditor(AJ_TEMA); uxRodape = uxRestaurar = 1; }
  }
  if (cenario == 6) focarOpcao(AJ_TEX_MB);
  if (cenario == 7) { focarOpcao(AJ_LARGURA_DP); uxAbrirEditor(AJ_LARGURA_DP); uxEvento(SDLK_DOWN); }
  if (cenario == 8) { focarSecao(0); uxIndice = 1; focoIndice = 1; uxChipAv = 1; }
  if (cenario == 9) { valor[AJ_IDIOMA] = IDIOMA_EN + 1; focarOpcao(AJ_HOME_LAYOUT); uxAbrirEditor(AJ_HOME_LAYOUT); }
  if (cenario == 21) focarOpcao(AJ_FOCO_TRAILER);
  if (cenario == 22) { focarOpcao(AJ_FOCO_TRAILER); uxAbrirEditor(AJ_FOCO_TRAILER); }
  if (cenario >= 10 && cenario < 21) { focarSecao(cenario - 10); focoIndice = 1; }
}

// O numero da opcao pela chave do disco/da conta ("idioma", "tmdb_language"): as
// capturas nao conhecem o enum, que mora aqui. -1 se nao existe.
int ajustes_teste_op_por_chave(const char *chave) {
  int i;
  for (i = 0; i < AJ_N; i++) if (CHAVE[i] && !strcmp(CHAVE[i], chave)) return i;
  return -1;
}
int ajustes_teste_op_atualizar(void) { return AJ_ATUALIZAR; }
int ajustes_teste_op_icone(int escolha) { valor[AJ_ICONE_APP] = escolha; return AJ_ICONE_APP; }
// O primeiro dos onze interruptores de "Notas no titulo" (consecutivos no enum).
int ajustes_teste_primeira_nota_titulo(void) { return AJ_NT_IMDB; }

int ajustes_teste_familia_previa(int op) {
  return (int)familiaPreviaOpcao(op);
}

// As capturas do #202 escolhem tema e vidro sem arquivo de ajustes.
void ajustes_teste_tema(int tema, int vidro) {
  if (tema >= 0 && tema < AJ_N_TEMAS_OPC) valor[AJ_TEMA] = tema;
  valor[AJ_VIDRO] = vidro ? 0 : 1;
}

// OS QUADROS DO MOCKUP (ajustes-mockup.html), um por id, para a captura lado
// a lado (tests/ajustes_shot.sh com NUVIO_AJ_QUADROS). Tema e vidro ficam os
// que a captura escolheu.
// Os quadros do mockup do Guia de uso (guia-mockup.html).
static int ajustesTesteGuia(const char *id) {
  static const struct { const char *id; int arte, idx, col; const char *ent; int daNov; } Q[] = {
    { "guia-capitulos", 13, GI_CAP0 + 1, GC_INDICE, NULL, 0 },
    { "guia-fontes", 15, GI_CAP0 + 3, GC_LISTA, "f-folha", 0 },
    { "guia-ilha", 0, GI_CAP0 + 5, GC_LISTA, "i-avisos", 0 },
    { "guia-fileiras", 2, GI_CAP0 + 1, GC_BOTOES, "h-fileiras", 0 },
    { "guia-legendas", 21, GI_CAP0 + 4, GC_LISTA, "l-ass", 0 },
    { "guia-novo", 7, GI_NOVO, GC_LISTA, "a-vidro", 0 },
    { "guia-vindo-do-whatsnew", 7, GI_CAP0, GC_LISTA, "c-guia", 1 },
    { "guia-busca", 21, GI_BUSCA, GC_INDICE, NULL, 0 },
  };
  int i, k;
  guiaFechar();
  if (!strcmp(id, "guia")) { ajArteFundoN = 12; focarOpcao(AJ_GUIA); return 1; }
  for (i = 0; i < (int)(sizeof Q / sizeof *Q); i++) {
    if (strcmp(id, Q[i].id)) continue;
    ajArteFundoN = Q[i].arte;
    focarOpcao(AJ_GUIA);
    guiaAbrir(Q[i].daNov);
    gv.idx = Q[i].idx; gv.col = Q[i].col; gv.ent = -1; gv.botao = 0;
    if (Q[i].ent) for (k = 0; k < GUIA_NENT; k++) if (!strcmp(GUIA_ENT[k].id, Q[i].ent)) gv.ent = k;
    guiaRolar(); guiaRol = guiaRolAlvo;
    return 1;
  }
  return 0;
}
int ajustes_teste_quadro(const char *id) {
  int tema = valor[AJ_TEMA], vidro = valor[AJ_VIDRO];
  memcpy(valor, valorPadrao, sizeof valor);
  valor[AJ_TEMA] = tema; valor[AJ_VIDRO] = vidro;
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  // NUVIO_SHOT_IDIOMA=N (IDIOMA_*: 1 en, 4 ru, 6 de...): o quadro sai nesse idioma.
  if (getenv("NUVIO_SHOT_IDIOMA") && *getenv("NUVIO_SHOT_IDIOMA")) valor[AJ_IDIOMA] = atoi(getenv("NUVIO_SHOT_IDIOMA")) + 1;
  // NUVIO_SHOT_LAYOUT=2 (HOME_LAYOUT_*): the owner's Dinamica layout, whose menu
  // pill sits in the Settings corner (with NUVIO_SHOT_MENU in the capture).
  if (getenv("NUVIO_SHOT_LAYOUT") && *getenv("NUVIO_SHOT_LAYOUT")) valor[AJ_HOME_LAYOUT] = atoi(getenv("NUVIO_SHOT_LAYOUT"));
  // NUVIO_SHOT_AVANCADAS=1: opcoes avancadas a mostra (a pilula ligada no indice).
  if (getenv("NUVIO_SHOT_AVANCADAS")) valor[AJ_AVANCADAS] = 0;
  uxCancelar(); uxAviso[0] = 0; uxRetornarOp = -1;
  memset(maisAberto, 0, sizeof maisAberto);
  scrollY = velY = 0; paginaA = 1;
  filAberta = 0; riscoFolha = 0; apoioAberto = 0;
  focarSecao(0); focoIndice = 1;
  ajArteFundoN = 12; ajMemFixa = 0; ajQuadroAddons = 0; ajQuadroPlugins = 0; ajVinculoTeste = 0;
  if (teclado_aberto()) { SDL_Event e = { 0 }; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; teclado_evento(&e); }
  aj2PoseFixa = 0;
  if (!strncmp(id, "v2-", 3)) {   // Ajustes v2 (ajustes-v2.html): os quadros do mockup
    valor[AJ_TEMA] = getenv("NUVIO_SHOT_TEMA") ? tema : 2;   // Oceano, como o mockup
    valor[AJ_LANDSCAPE] = 1;   // o mockup: "Pôsteres horizontais" desligado
    ajArteFundoN = 12;
    if (!strcmp(id, "v2-menu")) { focarSecao(0); uxIndice = 2; focoIndice = 1; }
    else if (!strncmp(id, "v2-menu-", 8) && id[8] >= '0' && id[8] <= '9') {   // index on category N
      int sN = atoi(id + 8);
      if (sN >= nSecoes) return 0;
      focarSecao(sN); uxIndice = 2 + sN; focoIndice = 1;
    }
    else if (!strncmp(id, "v2-sec-", 7) && id[7] >= '0' && id[7] <= '9') {   // category N opened, first row focused
      int sN = atoi(id + 7);
      if (sN >= nSecoes) return 0;
      focar(primeiroDaSecao(sN)); focoIndice = 0;
    }
    // 2.0.3: "v2-mais-N" = a linha "Mais opcoes" da categoria N em foco;
    // "v2-mais-aberto-N" = a mesma, aberta (as avancadas logo abaixo).
    else if (!strncmp(id, "v2-mais-", 8)) {
      int ab = !strncmp(id + 8, "aberto-", 7), sN = atoi(id + 8 + (ab ? 7 : 0));
      if (sN < 0 || sN >= nSecoes || maisItem[sN] < 0) return 0;
      maisAberto[sN] = ab;
      focar(maisItem[sN]); focoIndice = 0;
    }
    // "v2-topo-T" = o alvo T do alto da grade (0 Buscar ... 3 Painel/Lista, 4 Tour, 5 Resolver).
    else if (!strncmp(id, "v2-topo-", 8)) { focarSecao(0); focoIndice = 1; uxTopo = atoi(id + 8); }
    else if (!strcmp(id, "v2-menu-passando")) { ajArteFundoN = 13; focarSecao(1); uxIndice = 3; focoIndice = 1; }
    else if (!strcmp(id, "v2-aberto") || !strcmp(id, "v2-130")) focarOpcao(AJ_HOME_LAYOUT);
    else if (!strcmp(id, "v2-transicao")) {
      focarOpcao(AJ_HOME_LAYOUT);
      aj2PoseFixa = 1;
      aj2PoseTeste = (Aj2Pose){ 0.975f, -5.0f, 0.70f, 0.45f,  0.92f, 380.0f, AJ2_TOPO + 0.30f * (NV_VTELA_H - AJ2_TOPO - AJ2_MARGEM), 0.62f, 1.0f,
                                184.0f, 0.28f,  0.0f,  1.0f, 0.0f, 0.0f };
    }
    else if (!strcmp(id, "v2-editor")) { int k; focarOpcao(AJ_FIL_LIMITE); uxAbrirEditor(AJ_FIL_LIMITE); for (k = 0; k < 5; k++) uxEvento(SDLK_RIGHT); }
    else if (!strcmp(id, "v2-fundo-opcao")) { ajArteFundoN = 7; focarOpcao(AJ_FUNDO); uxAbrirEditor(AJ_FUNDO); uxPendente = 2; }
    else if (!strncmp(id, "v2-fundo-", 9)) {
      ajArteFundoN = 7;
      valor[AJ_FUNDO] = !strcmp(id, "v2-fundo-borrada") ? 1 : !strncmp(id, "v2-fundo-frost", 14) ? 2 : 0;
      if (!strcmp(id, "v2-fundo-frost-ambar")) valor[AJ_TEMA] = 5;
      focarOpcao(AJ_TEMA); uxAbrirEditor(AJ_TEMA);
    }
    else return 0;
    scrollY = velY = 0;
    return 1;
  }
  if (!strncmp(id, "op:", 3)) {   // qualquer opcao, pela chave do disco
    // "op:chave=N" grava o valor N antes de focar; "op:chave=N,outra=M" grava
    // tambem outras opcoes (a previa de uma depende das vizinhas).
    int op, k, primeira = -1;
    char lista[256], *par, *ctx = NULL;
    snprintf(lista, sizeof lista, "%s", id + 3);
    for (par = strtok_r(lista, ",", &ctx); par; par = strtok_r(NULL, ",", &ctx)) {
      char *ig = strchr(par, '=');
      if (ig) *ig = 0;
      for (op = -1, k = 0; k < AJ_N; k++) {
        const char *c = CHAVE[k];
        if (c && c[0] == '-') c++;
        if (c && !strcmp(c, par)) op = k;
      }
      if (op < 0) return 0;
      if (ig) valor[op] = atoi(ig + 1);
      if (primeira < 0) primeira = op;
    }
    if (primeira < 0) return 0;
    focarOpcao(primeira);
  }
  else if (!strcmp(id, "apoio-painel")) { ajArteFundoN = 12; focarOpcao(AJ_APOIAR); apoioAbrir(); }
  else if (!strncmp(id, "guia", 4)) { if (!ajustesTesteGuia(id)) return 0; }
  else if (!strcmp(id, "principal")) { focarOpcao(AJ_HOME_LAYOUT); }
  // 2.0 N1: logo e abertura. NUVIO_N1_LOGO / NUVIO_N1_ABERT escolhem o valor salvo.
  else if (!strncmp(id, "n1-", 3)) {
    if (getenv("NUVIO_N1_LOGO")) valor[AJ_LOGO_APP] = atoi(getenv("NUVIO_N1_LOGO"));
    if (getenv("NUVIO_N1_ABERT")) valor[AJ_ABERTURA] = atoi(getenv("NUVIO_N1_ABERT"));
    ajArteFundoN = 3;
    if (!strcmp(id, "n1-logo")) focarOpcao(AJ_LOGO_APP);
    else if (!strcmp(id, "n1-abertura")) focarOpcao(AJ_ABERTURA);
    else if (!strcmp(id, "n1-logo-seletor")) { focarOpcao(AJ_LOGO_APP); uxAbrirEditor(AJ_LOGO_APP); }
    else if (!strcmp(id, "n1-abertura-seletor")) { focarOpcao(AJ_ABERTURA); uxAbrirEditor(AJ_ABERTURA); }
    else return 0;
  }
  // O aviso "Ajuste salvo nesta TV." logo depois de mudar uma opcao (foto do
  // dono, 04/10: o aviso cobria a ultima linha).
  else if (!strcmp(id, "aviso")) { focarOpcao(AJ_TAMANHO_AJUSTES); uxNotificar("Ajuste salvo nesta TV."); }
  else if (!strcmp(id, "cartazes")) { ajArteFundoN = 13; valor[AJ_LARGURA_DP] = 128; focarOpcao(AJ_LARGURA_DP); }
  else if (!strcmp(id, "memoria")) { ajArteFundoN = 21; ajMemFixa = 1; focarOpcao(AJ_ESPACO); }
  else if (!strcmp(id, "cor")) { ajArteFundoN = 9; focarOpcao(AJ_TEMA); uxAbrirEditor(AJ_TEMA); }
  else if (!strcmp(id, "relogio")) { ajArteFundoN = 0; focarOpcao(AJ_RELOGIO); }
  else if (!strcmp(id, "reproducao")) { ajArteFundoN = 15; valor[AJ_QUALIDADE] = 1; focarOpcao(AJ_DV); }
  else if (!strcmp(id, "contas")) { ajArteFundoN = 13; focarOpcao(AJ_TRAKT); }
  else if (!strcmp(id, "teclado")) {
    SDL_Event e = { 0 };
    ajArteFundoN = 13; focarOpcao(AJ_FANART_CHAVE);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
    eventoTela(&e);
    teclado_teste_texto("3f9a2c"); teclado_teste_foco(0, 2);
  }
  else if (!strcmp(id, "trakt")) { ajArteFundoN = 13; focarOpcao(AJ_TRAKT); ajVinculoTeste = 1; }
  else if (!strcmp(id, "editor-escolha")) { ajArteFundoN = 15; valor[AJ_QUALIDADE] = 1; focarOpcao(AJ_QUALIDADE); uxAbrirEditor(AJ_QUALIDADE); uxEvento(SDLK_DOWN); }
  else if (!strcmp(id, "editor-numero")) { focarOpcao(AJ_FIL_LIMITE); uxAbrirEditor(AJ_FIL_LIMITE); { int i; for (i = 0; i < 5; i++) uxEvento(SDLK_RIGHT); } }
  else if (!strcmp(id, "confirmacao")) { focarOpcao(AJ_FIL_LIMITE); uxAbrirEditor(AJ_FIL_LIMITE); uxPendente = 17; uxAvisoRisco = SEG_AVISO_FILEIRAS; uxConfirmar = 1; }
  else if (!strcmp(id, "fileiras") || !strcmp(id, "fileiras-fora")) {
    ajArteFundoN = 2;
    focarOpcao(AJ_FIL_ORDEM);
    fil_definir_tipo("com.linvo.cinemeta_movie_top", FIL_TIPO_DESTAQUE); filAberta = 1; filFoco = 3; filCampo = 2; filPegou = 0; filTopo = 0; filNaBarra = 0;
    filAba = !strcmp(id, "fileiras-fora"); filForaAgrupada = 1;
    if (getenv("NUVIO_SHOT_DADOS")) fil_normalizar();   // como o OK na linha faz, com o arquivo de verdade
    if (filAba) {   // o grupo do mockup: Akashi, AIOStreams e Xperience fora da Home
      int k;
      static const int FORA[4] = { 11, 13, 14, 15 };
      for (k = 0; k < 4; k++) if (k < fil_n() && fil_estado(FORA[k]) != FIL_FORA) fil_remover(FORA[k]);
      filMontarLista(); filFoco = 0;
      for (k = 0; k < filListaN; k++) if (filLista[k] == 7) filFoco = k;
    }
  }
  else if (!strcmp(id, "diferencas")) {
    ajArteFundoN = 3;
    valor[AJ_HOME_LAYOUT] = HOME_LAYOUT_DINAMICA; valor[AJ_ANIM] = 1; valor[AJ_LARGURA_DP] = 128;
    valor[AJ_QUALIDADE] = 1; valor[AJ_HERO_TRAILER] = 0;
    uxListarDiferencas(); uxIndice = 1; focoIndice = 0; uxDifFoco = 0;
  }
  else if (!strcmp(id, "addons")) {
    static const char *const NOME[8] = { "Cinemeta", "AIOStreams", "Xperience", "TMDB", "Akashi", "MDBList", "OpenSubtitles v3", "Torrentio" };
    static const char *const REC[8] = { "\"catalog\",\"meta\"", "\"catalog\",\"stream\"", "\"catalog\"", "\"catalog\",\"meta\"",
                                        "\"catalog\"", "\"catalog\"", "\"subtitles\"", "\"stream\"" };
    int k;
    ajArteFundoN = 3;
    if (addons_n() == 0)
      for (k = 0; k < 8; k++) {
        char url[200], man[400];
        snprintf(url, sizeof url, "https://%s.exemplo.org/manifest.json", NOME[k][0] == 'O' ? "opensubtitles" : NOME[k]);
        addons_adicionar(NOME[k], url);
        snprintf(man, sizeof man, "{\"id\":\"x.%d\",\"name\":\"%s\",\"resources\":[%s],\"types\":[\"movie\",\"series\"]}", k, NOME[k], REC[k]);
        addons_manifesto_lido(k, man);
        if ((k == 4 || k == 7) && addons_ativo(k)) addons_alternar(k);
      }
    ajQuadroAddons = 2;
  }
  else if (!strcmp(id, "plugins")) { ajArteFundoN = 3; ajQuadroPlugins = 1; }
  else return 0;
  scrollY = velY = 0;
  return 1;
}

// A arte atras da tela nas capturas (o indice da amostra de deploy/app/art).
void ajustes_teste_arte(int n) { ajArteFundoN = n; }

void ajustes_teste_fonte_interface(int familia) {
  if (familia < TXT_FAMILIA_INTER || familia > TXT_FAMILIA_ATKINSON) return;
  valor[AJ_FONTE_UI] = familia;
  txt_definir_fonte_interface((TxtFamilia)familia);
}
// #202: abre a folha de permitidos (1 add-ons, 2 plugins) com o foco na
// linha `foco` (0 = "Todos").
void ajustes_teste_permitidos(int qual, int foco) { frAberta = qual; frFoco = foco; }
#endif

#ifdef NV_SHOT_HOOKS
// Capturas: muda uma opcao pela chave do ajustes.txt ("relogioPosLocal") sem
// gravar. 0 = nao achou.
int ajustes_shot_valor(const char *chave, int v) {
  int i;
  if (!strcmp(chave, "seekrChave")) { snprintf(seekrChave, sizeof seekrChave, "%s", v ? "captura" : ""); return 1; }
  for (i = 0; i < AJ_N; i++) {
    const char *c = CHAVE[i];
    if (c && c[0] == '-') c++;
    if (c && !strcmp(c, chave)) { valor[i] = v; return 1; }
  }
  return 0;
}
#endif

// --- CENTRAL DE CONTROLE (central.h) -----------------------------------------
// A porta da central para trocar uma opcao sem abrir esta tela. A opcao e
// achada pela CHAVE do ajustes.txt (estavel entre versoes; o enum nao e). So
// escolhas curtas (2 a 6 valores) que existem na tela DESTE build; as que
// pedem a folha de risco (fileiras) ficam fora. A troca passa por
// definirValorDireto: a mesma gravacao e os mesmos efeitos do OK da tela.
int ajustes_rapido_op(const char *chave) {
  int i, op = -1, n;
  if (!chave || !chave[0]) return -1;
  for (i = 0; i < AJ_N; i++)
    if (CHAVE[i] && CHAVE[i][0] != '-' && !strcmp(CHAVE[i], chave)) { op = i; break; }
  if (op < 0 || OPCOES[op].tipo != OP_ESCOLHA) return -1;
  if (op == AJ_FIL_LIMITE || op == AJ_ITENS_FILEIRA) return -1;
  n = nValores(op);
  if (n < 2 || n > 6) return -1;
  for (i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == op) return op;
  return -1;
}
const char *ajustes_rapido_rotulo(int op) { return op >= 0 && op < AJ_N ? rotuloOpcao(op) : ""; }
const char *ajustes_rapido_valor(int op) { return op >= 0 && op < AJ_N ? textoValor(op) : ""; }
int ajustes_rapido_ligado(int op) { return op >= 0 && op < AJ_N && ehInterruptor(op) ? lig(op) : -1; }
int ajustes_rapido_passo(int op, int dir) {
  if (op < 0 || op >= AJ_N) return 0;
  { int r; ajOrigemDireto = AJLOG_CENTRAL; r = definirValorDireto(op, passoAdiante(op, dir < 0 ? -1 : 1)); ajOrigemDireto = AJLOG_AJUSTES; return r; }
}
