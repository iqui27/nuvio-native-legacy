// A TROCA DE CENA DOS AJUSTES (imagem da categoria / previa da opcao).
//
// Cada categoria e cada submenu tem a sua arte (AJ_ARTE_SEC, em
// ajustes_ux_visual.inc), entao andar na lista troca a imagem do quadro. Sem
// isto a troca era um corte, e a arte que ainda nao tinha sido lida aparecia
// como o cinza de reserva por alguns quadros.
//
// Aqui a cena pedida so entra quando esta PRONTA: enquanto as imagens dela
// carregam, o quadro continua mostrando a cena anterior (a nova e desenhada
// com opacidade 0, so para pedir as texturas e contar as que faltam, em
// ajCenaFaltas). Pronta, ela entra por cima da anterior em AJ_TROCA_S.
//
// Custo: 1 camada parada (o desenho direto de sempre, sem FBO); 2 camadas do
// tamanho do quadro so durante a espera/troca. A cena nova entra com a
// opacidade de grupo, peca por peca, sobre a anterior ainda inteira: chega
// exatamente na cena nova em t=1 e nao precisa de alvo fora da tela (a C9 nao
// perdoa FBO a toa).
//
// Sem troca (corte, como antes): primeira vez que o quadro aparece, quadro que
// mudou de tamanho, e cenas de alturas diferentes (o texto embaixo acompanha a
// cena pedida e a anterior ficaria por cima dele).
#define AJ_TROCA_S 0.22f       // duracao da troca
#define AJ_TROCA_ESPERA 1.2f   // teto da espera pelas imagens da cena nova
// Seta segurada (um pedido novo menos de AJ_TROCA_RAPIDO depois do anterior):
// a troca so comeca quando o foco para por AJ_TROCA_ASSENTA. Sem isto, andar
// depressa pelas categorias emendava uma troca na outra e o quadro ficava com
// duas camadas o tempo todo.
#define AJ_TROCA_RAPIDO 0.30f
#define AJ_TROCA_ASSENTA 0.15f

typedef struct { int sec, op, chave; } AjCenaRef;
typedef struct {
  AjCenaRef mostra, alvo;   // a cena no quadro e a que vai entrar
  float t;                  // 0 = esperando a alvo ficar pronta; (0,1] = entrando
  float espera, w, h;       // h = altura da cena no quadro
  int trocando, quadro;     // quadro = o ultimo em que foi desenhado
  int pedida, rapido;       // a chave pedida no quadro anterior; seta segurada
  float desde;              // tempo desde que o pedido mudou
} AjCenaTrocaE;
static AjCenaTrocaE ajTroca[AJ_TROCA_N];
static int ajTrocaQuadro = 1;
static float ajTrocaDt;

static void ajCenaTrocaAtualizar(float dt) {
  ajTrocaQuadro++;
  ajTrocaDt = dt > 0.0f ? (dt < 0.1f ? dt : 0.1f) : 0.0f;
}

// A "mesma imagem": duas opcoes com a mesma chave desenham a mesma cena (so
// muda o detalhe da opcao em foco) e trocam direto, sem espera nem mistura.
// Acompanha o despacho de ajPrevia/ajPreviaCarta: previa nova com cena propria
// entra aqui, senao ela troca em corte com as vizinhas de submenu.
static int ajCenaChave(int sec, int op) {
  // 2.0.2: cada opcao tem o seu desenho gerado; trocar de opcao e uma troca
  // de cena (cruzamento de 0,22 s, sem espera: nao ha imagem para ler).
  return op < 0 ? 2000 + sec : op;
}
static float ajCenaTrocaDesenhar(int quadro, AjCenaRef c, float x, float y, float w, float corte) {
  if (quadro == AJ_TROCA_RESUMO) return aj2PreviaCategoria(c.sec, x, y, w, corte);
  if (c.op < 0) return ajVisualCena(c.sec, -1, x, y, w);
  return aj2Previa(c.op, x, y, w);
}

static float ajCenaTroca(int quadro, int sec, int op, float x, float y, float w, float corte) {
  AjCenaTrocaE *e = &ajTroca[quadro];
  AjCenaRef ped = { sec, op, ajCenaChave(sec, op) };
  float grupo = gfx_opacidade_grupo, h;
  int novoQuadro = e->quadro != ajTrocaQuadro, faltas;
  // Quadro que nao estava na tela no quadro anterior, ou que mudou de largura:
  // nao ha cena anterior para segurar.
  if (e->quadro < ajTrocaQuadro - 1 || fabsf(e->w - w) > 0.5f) {
    e->mostra = ped; e->trocando = 0; e->pedida = ped.chave; e->desde = AJ_TROCA_RAPIDO;
  }
  e->quadro = ajTrocaQuadro; e->w = w;
  if (novoQuadro) e->desde += ajTrocaDt;
  if (ped.chave != e->pedida) {
    e->rapido = e->desde < AJ_TROCA_RAPIDO;
    e->pedida = ped.chave; e->desde = 0;
  }
  if (!e->trocando) {
    if (ped.chave == e->mostra.chave) {   // a mesma imagem: desenho direto
      e->mostra = ped;
      return e->h = ajCenaTrocaDesenhar(quadro, ped, x, y, w, corte);
    }
    e->trocando = 1; e->alvo = ped; e->t = 0; e->espera = 0;
  }
  // Esperando: a alvo acompanha o foco. Entrando: a troca termina na cena em
  // que comecou e a seguinte parte dela (nada de corte no meio).
  if (e->t <= 0.0f) {
    if (ped.chave == e->mostra.chave) {   // voltou para a cena que esta no quadro
      e->trocando = 0; e->mostra = ped;
      return e->h = ajCenaTrocaDesenhar(quadro, ped, x, y, w, corte);
    }
    if (ped.chave != e->alvo.chave) e->espera = 0;
    e->alvo = ped;
    // A cena nova, invisivel: pede as texturas dela, mede a altura e diz se
    // falta alguma.
    faltas = ajCenaFaltas;
    gfx_opacidade_grupo = 0;
    h = ajCenaTrocaDesenhar(quadro, e->alvo, x, y, w, corte);
    gfx_opacidade_grupo = grupo;
    faltas = ajCenaFaltas - faltas;
    if (fabsf(h - e->h) > 1.0f) {   // alturas diferentes: corte
      e->trocando = 0; e->mostra = ped;
      return e->h = ajCenaTrocaDesenhar(quadro, ped, x, y, w, corte);
    }
    ajCenaTrocaDesenhar(quadro, e->mostra, x, y, w, corte);
    if (novoQuadro) e->espera += ajTrocaDt;
    if (faltas > 0 && e->espera < AJ_TROCA_ESPERA) return h;
    if (e->rapido && e->desde < AJ_TROCA_ASSENTA) return h;
    if (ajustes_animacoes_reduzidas()) { e->mostra = e->alvo; e->trocando = 0; return h; }
    e->t = 0.0001f;   // pronta: comeca a entrar no proximo quadro
    return h;
  }
  if (novoQuadro) e->t += ajTrocaDt / AJ_TROCA_S;
  if (e->t >= 1.0f) {
    e->mostra = e->alvo; e->trocando = 0;
    h = e->h = ajCenaTrocaDesenhar(quadro, e->mostra, x, y, w, corte);
  } else {
    float k = e->t * e->t * (3.0f - 2.0f * e->t);
    ajCenaTrocaDesenhar(quadro, e->mostra, x, y, w, corte);
    gfx_opacidade_grupo = grupo * k;
    h = ajCenaTrocaDesenhar(quadro, e->alvo, x, y, w, corte);
    gfx_opacidade_grupo = grupo;
  }
  // O texto embaixo e o da opcao em foco: a altura devolvida e a da cena dela
  // (igual a da que esta no quadro, senao teria sido corte).
  return h;
}

// --- A IMAGEM DE CADA SUBMENU ---------------------------------------------------------
// Dono, 05/10: "temos que mudar as imagens de todos os submenus, deixar cada
// submenu diferente, nao quero tudo repetido". Cada bloco (ROT) da lista — e
// os grupos logicos das categorias sem ROT (Idiomas, Desempenho, Sobre) — tem
// a SUA cena: a parte do app que ele controla, com arte real (a amostra do
// submenu, AJ_ARTE_SEC, ou o catalogo da conta), as pecas reais (marcas de
// formato, selos, avatares, logos de nota, nomes de fileira e de addon) e o
// VALOR ATUAL das opcoes (desligada = apagada). A peca da opcao em foco ganha
// o anel do acento; andar dentro do bloco so move o anel (mesma chave em
// ajCenaChave, troca direta), andar para outro bloco troca a cena (mistura de
// ajCenaTroca). Os blocos cujas opcoes ja tem previa propria (Continuar
// assistindo, Logo e abertura, Texto e tamanho, Memoria, Registro) continuam
// com elas. Coordenadas num quadro de 640x360, como ajVisualCena.
enum {
  AJC_HOME_ESTILO = 0, AJC_HOME_FILEIRAS, AJC_HOME_BARRA, AJC_HOME_PERFIL,
  AJC_TIT_APARECE, AJC_TIT_NOTAS, AJC_TIT_DADOS, AJC_TIT_BUSCA,
  AJC_CAR_FOCO, AJC_CAR_FORMA, AJC_CAR_ORIGEM,
  AJC_TRL_ONDE, AJC_TRL_SOM,
  AJC_APA_VIDRO, AJC_APA_MOVIMENTO,
  AJC_REP_FONTE, AJC_REP_IMAGEM, AJC_REP_DURANTE, AJC_REP_SEEKR,
  AJC_IDI_INTERFACE, AJC_IDI_FAIXAS, AJC_IDI_LEGENDA2,
  AJC_TV_PROVEDOR, AJC_TV_PROBLEMAS,
  AJC_CON_PERFIS, AJC_CON_AVISOS, AJC_CON_SOCIAL, AJC_CON_CHAVES, AJC_CON_P2P, AJC_CON_SERVIDORES,
  AJC_DES_TELA, AJC_DES_DIAG,
  AJC_SOB_GUIA, AJC_SOB_APP,
  AJC_FON_ADDONS,
  AJC_N
};
static int ajCenaSecao(int op) {
  switch (op) {
    case AJ_LANDSCAPE: case AJ_HERO: case AJ_HERO_CHEIO: case AJ_HERO_CATALOGOS: case AJ_HERO_TRANSICAO:
      return AJC_HOME_ESTILO;
    case AJ_FIL_ORDEM: case AJ_FIL_LIMITE: case AJ_ITENS_FILEIRA: case AJ_ESPACO_FILEIRAS: case AJ_ESPACO_TITULOS: case AJ_OCULTAR_NLANC: case AJ_NOTAS_HOME:
    case AJ_ROTULOS: case AJ_NOME_ADDON: case AJ_SUFIXO_TIPO: case AJ_SELO_VISTO:
      return AJC_HOME_FILEIRAS;
    case AJ_RAIL_MODERNA: case AJ_RAIL: case AJ_MENU_EXPLORAR: case AJ_MENU_GUIA: case AJ_MENU_AGENDA:
    case AJ_MENU_PERFIL: case AJ_RAIL_BLUR: case AJ_DESCOBRIR:
      return AJC_HOME_BARRA;
    case AJ_PS_FUNDO: case AJ_PS_CONTINUAR: return AJC_HOME_PERFIL;
    case AJ_DET_VEU: case AJ_DET_DATA_CHEIA: case AJ_DET_BLUR_NAO_VISTOS: return AJC_TIT_APARECE;
    case AJ_NT_IMDB: case AJ_NT_TOMATES: case AJ_NT_AUDIENCIA: case AJ_NT_META: case AJ_NT_METAUSER:
    case AJ_NT_TRAKT: case AJ_NT_TMDB: case AJ_NT_LETTER: case AJ_NT_MAL: case AJ_NT_EBERT: case AJ_NT_SCORE:
    case AJ_MDB_LIGADO: case AJ_MDB_TRAKT: case AJ_MDB_IMDB: case AJ_MDB_TMDB: case AJ_MDB_LETTER:
    case AJ_MDB_TOMATES: case AJ_MDB_AUDIENCIA: case AJ_MDB_META: case AJ_MDB_MAL:
    case AJ_MDB_CHAVE:   // a chave abre o bloco Notas: e ela que libera as notas do MDBList
      return AJC_TIT_NOTAS;
    case AJ_TMDB_LIGADO: case AJ_TMDB_ARTE: case AJ_TMDB_BASICO: case AJ_TMDB_FICHA: case AJ_TMDB_DATAS:
    case AJ_TMDB_ELENCO: case AJ_TMDB_PROD: case AJ_TMDB_REDES: case AJ_TMDB_EPS: case AJ_TMDB_TRAILERS:
    case AJ_TMDB_MAIS: case AJ_TMDB_COL: case AJ_TMDB_CW: case AJ_DET_META_EXT: case AJ_DET_SO_CINEMETA:
      return AJC_TIT_DADOS;
    case AJ_BUSCA_CINEMETA: case AJ_BUSCA_NUVIO: case AJ_BUSCA_ORIGEM: return AJC_TIT_BUSCA;
    case AJ_EXPANDIR: case AJ_EXPANDIR_ATRASO: case AJ_BORDA_FOCO: case AJ_GRAD_CLASSICO: return AJC_CAR_FOCO;
    case AJ_PROF: case AJ_PROF_BORDA: case AJ_PROF_BRILHO: case AJ_PROF_COBERTURA: case AJ_PROF_POSTERS:
    case AJ_PROF_CW: case AJ_PROF_EPS: case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS:
      return AJC_CAR_FORMA;
    case AJ_HERO_FUNDO: case AJ_HERO_ARTE_DIF: case AJ_POSTER_PROV: case AJ_ADDON_POSTER: case AJ_ADDON_FUNDO:
    case AJ_ADDON_LOGO: case AJ_COL_ARTE_CONTA: case AJ_POSTER_INST: case AJ_POSTER_TOKEN: case AJ_POSTER_EXTRA:
    case AJ_POSTER_CHAVE: case AJ_POSTER_MODELO: case AJ_POSTER_TESTAR:
    case AJ_FANART_CHAVE:
      return AJC_CAR_ORIGEM;
    case AJ_HERO_TRAILER: case AJ_FOCO_TRAILER: case AJ_DET_TRAILER_AUTO: case AJ_DET_TRAILER: return AJC_TRL_ONDE;
    case AJ_HERO_TRAILER_SOM: case AJ_DET_TRAILER_SOM: case AJ_TRAILER_FONTE: case AJ_HERO_TRAILER_ESPERA:
    case AJ_TRAILER_QUAL: case AJ_TRAILER_ASPECTO: case AJ_TRAILER_ZOOM_TPK: case AJ_LOGO_TRAILER:
      return AJC_TRL_SOM;
    case AJ_VIDRO: case AJ_VIDRO_CONTORNO: case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO: return AJC_APA_VIDRO;
    case AJ_ANIM: case AJ_RELOGIO: case AJ_RELOGIO_12H: return AJC_APA_MOVIMENTO;
    case AJ_FONTE_MANUAL: case AJ_FONTE_AUTO: case AJ_FONTE_REPOR: case AJ_FONTE_PRIORIDADE: case AJ_FONTE_TOCAR_CONFERINDO: case AJ_FONTE_AQUECER: case AJ_FONTE_CONFERIR_VARIAS: case AJ_FONTE_PREPARAR:
    case AJ_FONTE_PRAZO: case AJ_FONTE_TEXTO: case AJ_FONTE_ORDEM_ADDON: case AJ_TAM_MAX: case AJ_TAM_MIN: case AJ_SELOS_CORES: case AJ_SELOS_PACOTE: case AJ_SELOS_PACOTE_ADD:
    case AJ_FONTE_ESCOPO: case AJ_FONTE_ADDONS_PERM: case AJ_FONTE_PLUGINS_PERM: case AJ_FONTE_OUTROS:
    case AJ_FONTE_REGEX: case AJ_FONTE_REGEX_PADRAO: case AJ_FONTE_REGEX_MODELO:
    case AJ_FONTE_ORDEM: case AJ_FONTE_ORDEM_USO:
    case AJ_SELOS_PACOTE_REM:
      return AJC_REP_FONTE;
    // 2.0.3: HDR e Dolby Vision mora em Imagem e som.
    case AJ_FONTE_HDR:
    case AJ_QUALIDADE: case AJ_DV: case AJ_DV_MKV: case AJ_ATMOS: case AJ_DTS_AC3: case AJ_CACHE_SEEK: return AJC_REP_IMAGEM;
    case AJ_PAUSA_OVERLAY: case AJ_PLR_CLASSIF: case AJ_REACAO_CREDITOS: case AJ_SAIDA_PLAYER: case AJ_MANTER_VIDEO: case AJ_PROPORCAO_PADRAO: return AJC_REP_DURANTE;
    case AJ_SEEKR_LIGADO: case AJ_SEEKR_CHAVE: case AJ_SEEKR_TESTAR: case AJ_SEEKR_FITA: case AJ_SEEKR_AJUSTE:
      return AJC_REP_SEEKR;
    case AJ_IDIOMA: case AJ_TMDB_IDIOMA: return AJC_IDI_INTERFACE;
    case AJ_AUD_LINGUA: case AJ_LEG_LINGUA: case AJ_LEG_SYNC_AUTO: case AJ_LEG_SYNC_AUDIO: case AJ_LEG_FORCADA: return AJC_IDI_FAIXAS;
    case AJ_LEG_LINGUA2: case AJ_LEG2_POS: case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA:
      return AJC_IDI_LEGENDA2;
    case AJ_XTREAM_SERVIDOR: case AJ_XTREAM_USUARIO: case AJ_XTREAM_SENHA: case AJ_XTREAM_CONTA:
    case AJ_XTREAM_LIMPAR: case AJ_STALKER_PORTAL: case AJ_STALKER_MAC: case AJ_STALKER_LIMPAR: case AJ_EPG_PAIS:
      return AJC_TV_PROVEDOR;
    case AJ_LIVETV_DIAG: case AJ_LIVETV_RES: case AJ_LIVETV_ESPERA: case AJ_LIVETV_FORMATO: case AJ_LIVETV_MODO:
    case AJ_LIVETV_PROXY:
      return AJC_TV_PROBLEMAS;
    case AJ_PERFIL_ATIVO: case AJ_SYNC:
    case AJ_PERFIL_PESQ: case AJ_PERFIL_EDITAR: case AJ_SAIR:
      return AJC_CON_PERFIS;
    case AJ_ADDONS: case AJ_PLUGINS: case AJ_ADDONS_PRINCIPAL: return AJC_FON_ADDONS;
    case AJ_ENQUETES: return AJC_CON_AVISOS;
    case AJ_DISCORD: case AJ_SALVOS_DEST: case AJ_CW_FONTE: case AJ_CW_PROXIMO: case AJ_HIST_CONTA: case AJ_SOCIAL: return AJC_CON_SOCIAL;
    case AJ_DEBRID_RD: case AJ_DEBRID_TB: case AJ_DEBRID_PM: case AJ_DEBRID_AD: case AJ_DEBRID_AD_TESTAR:
      return AJC_CON_CHAVES;
    case AJ_P2P_LIGADO: case AJ_P2P_URL: case AJ_P2P_TESTAR: case AJ_P2P_LIMITE: return AJC_CON_P2P;
    case AJ_JF_LIGADO: case AJ_JF_SERVIDOR: case AJ_JF_ENTRAR: case AJ_JF_SAIR: case AJ_EM_SERVIDOR:
    case AJ_EM_ENTRAR: case AJ_EM_SAIR: case AJ_PX_ENTRAR: case AJ_PX_SERVIDOR: case AJ_PX_SAIR:
      return AJC_CON_SERVIDORES;
    case AJ_RESOLUCAO: case AJ_QUALIDADE_IMG: case AJ_GPU_EFEITOS: case AJ_NAV_RAPIDA: return AJC_DES_TELA;
    case AJ_MEDIDOR: case AJ_DIAGNOSTICO: case AJ_VELOCIDADE: return AJC_DES_DIAG;
    case AJ_GUIA: return AJC_SOB_GUIA;
    case AJ_VERSAO_I: case AJ_ATUALIZAR: return AJC_SOB_APP;
    default: return -1;
  }
}

// O quadro da cena em desenho (coordenadas 640x360 -> tela).
static struct { float x, y, s, h, ar, ag, ab; int op; } ajc;
#define CX(a) (ajc.x + (a) * ajc.s)
#define CY(b) (ajc.y + (b) * ajc.s)
#define CR(a,b,c,d) (GfxRect){ CX(a), CY(b), (c) * ajc.s, (d) * ajc.s }
#define CVID(a,b,c,d,r) ajCenaVidro(CR(a,b,c,d), (r) * ajc.s, 1)
#define CVIDA(a,b,c,d,r,al) ajCenaVidro(CR(a,b,c,d), (r) * ajc.s, al)
#define CBAR(a,b,c,al) gfx_cor(CR(a,b,c,7), 0.5f, 0.953f, 0.949f, 0.937f, al)
#define CACC(a,b,c,d,al) gfx_cor(CR(a,b,c,d), 0.5f, ajc.ar, ajc.ag, ajc.ab, al)
#define CICON(n,a,b,d,al) ajIcone(n, CX(a), CY(b), (d) * ajc.s, al)
#define CIMG(c,a,b,cw,ch,r,al) ajImg(c, CR(a,b,cw,ch), (r) * ajc.s, al)
#define ON(o) (lig(o) ? 1.0f : 0.3f)
// O anel do acento em volta da peca de `o`, quando `o` e a opcao em foco.
static void ajcFoco(int o, float a, float b, float c, float d, float raio) {
  if (o != ajc.op) return;
  gfx_anel_fora(CR(a, b, c, d), raio / d, 3.0f * ajc.s, 2.5f * ajc.s, ajc.ar, ajc.ag, ajc.ab, 1);
}
// Texto (traduzido) que cabe em `maxW`; devolve a largura no quadro 640.
static float ajcTxt(const char *t, float px, int forte, float a, float b, float maxW, float al) {
  TxtEstilo e = ajEstiloPxS(px * ajc.s, forte);
  TxtLinha l;
  if (!t || !t[0]) return 0;
  l = maxW > 0 ? txt_linha_corta(e, t, AJ_TX, maxW * ajc.s) : txt_linha(e, t, AJ_TX);
  txt_desenhar_alpha(l, CX(a), CY(b) + (px * ajc.s * 1.2f - l.h) * 0.5f, al);
  return l.w / ajc.s;
}
static float ajcTxtDir(const char *t, float px, int forte, float dir, float b, float maxW, float al) {
  TxtEstilo e = ajEstiloPxS(px * ajc.s, forte);
  TxtLinha l;
  if (!t || !t[0]) return 0;
  l = txt_linha_corta(e, t, AJ_TX, maxW * ajc.s);
  txt_desenhar_alpha(l, CX(dir) - l.w, CY(b) + (px * ajc.s * 1.2f - l.h) * 0.5f, al);
  return l.w / ajc.s;
}
// Arte borrada: o decode pequeno da mesma imagem esticado (o que a Home
// Dinamica faz), sem passada de desfoque.
static void ajcBorrada(const char *c, GfxRect r, float raio, float a) {
  GLuint t;
  if (!c || !c[0]) return;
  t = tex_obter_larg(c, 24.0f);
  if (!t) { gfx_cor(r, raio / r.h, 0.16f, 0.165f, 0.18f, 0.6f * a); ajCenaFaltas++; return; }
  gfx_tex_aspect_atual = tex_aspecto(c);
  gfx_rect(r, t, GFX_ARTE, 0, 0, 0, raio / r.h, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0;
}
// Uma linha de lista (rotulo + valor) no vidro, com o realce da opcao em foco.
static void ajcLinha(int o, const char *ico, float a, float b, float w, float hh, float al) {
  int f = o == ajc.op;
  if (f) gfx_cor(CR(a, b, w, hh), 0.5f, 1, 1, 1, 0.12f);
  if (ico) CICON(ico, a + 12, b + (hh - 18) * 0.5f, 18, 0.8f * al);
  ajcTxt(rotuloOpcao(o), 12, f, a + (ico ? 38 : 14), b + (hh - 14.4f) * 0.5f, w * 0.55f - (ico ? 38 : 14), (f ? 1.0f : 0.8f) * al);
  ajcTxtDir(textoValor(o), 11, 0, a + w - 14, b + (hh - 13.2f) * 0.5f, w * 0.42f, 0.55f * al);
  if (f) CACC(a - 10, b + hh * 0.5f - 3, 6, 6, 1);
}
// Uma peca "tela": arte de fundo com veu na base, para as cenas de 3 telas.
static void ajcTela(const char *arte, float a, float b, float c, float d, float al) {
  CIMG(arte, a, b, c, d, 12, al);
  gfx_veu_css(CR(a, b + d * 0.45f, c, d * 0.55f), 0, 0, 1.0f, 0.75f * al);
}
// Um still de episodio da amostra (deploy/app/art/ep: so as series tem) e
// uma foto de elenco (art/elenco, 12 titulos), escolhidos pelo titulo `n`.
static const char *ajcEp(int n, int k) {
  static const char *const S[10] = { "00_1", "01_1", "15_1", "17_1", "19_1", "21_1", "23_1", "25_1", "27_1", "29_1" };
  static char b[6][1200];
  static int j;
  j = (j + 1) % 6;
  snprintf(b[j], sizeof b[j], "%s/ep/%s_%02d.jpg", uxDirArte, S[(n < 0 ? 0 : n) % 10], k);
  return b[j];
}
static const char *ajcElenco(int n, int k) {
  static char b[4][1200];
  static int j;
  j = (j + 1) % 4;
  snprintf(b[j], sizeof b[j], "%s/elenco/%02d_%d.jpg", uxDirArte, (n < 0 ? 0 : n) % 12, k);
  return b[j];
}
// Indice de um logo de canal/titulo da amostra.
static const char *ajcLogoAmostra(int n) {
  static char b[4][1200];
  static int k;
  k = (k + 1) % 4;
  snprintf(b[k], sizeof b[k], "%s/logo/%02d.png", uxDirArte, n % 8);
  return b[k];
}

// --- TELA INICIAL ---------------------------------------------------------------------
// Estilo: o destaque de perto (arte cheia ou em faixa, catalogo, pontos da
// troca) e a primeira fileira deitada ou em pe.
static void ajcHomeEstilo(int A) {
  int hero = ajustes_hero_ligado(), land = valor[AJ_LANDSCAPE] == 0, cheio = lig(AJ_HERO_CHEIO), i;
  float ha = hero ? 1.0f : 0.25f, by;
  GfxRect q = { ajc.x, ajc.y, 640 * ajc.s, ajc.h };
  gfx_cor(q, 22 / q.h, 0.047f, 0.051f, 0.063f, 1);
  if (cheio) CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 360, 22, ha);
  else { CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 250, 22, ha);
         gfx_rect(CR(0, 150, 640, 101), 0, GFX_VEU_BAIXO, 0, 0, 0, 0, 0.047f, 0.051f, 0.063f, 1); }
  gfx_veu_css((GfxRect){ ajc.x, ajc.y, 640 * 0.7f * ajc.s, ajc.h }, 2, 0, 1.0f, 0.8f);
  gfx_veu_css(CR(0, 200, 640, 160), 0, 0, 1.0f, 0.8f);
  { float cw = ajcTxt(textoValor(AJ_HERO_CATALOGOS), 11, 1, 50, 26, 220, 0) + 28;
    CVIDA(36, 22, cw, 24, 12, ha); ajcTxt(textoValor(AJ_HERO_CATALOGOS), 11, 1, 50, 26, 220, ha);
    ajcFoco(AJ_HERO_CATALOGOS, 36, 22, cw, 24, 12); }
  if (ajLogo(ajArte(AJA_LOGO, A), CX(36), CY(62), 220 * ajc.s, 66 * ajc.s, ha) <= 0) ajcTxt(ajTituloN(A), 24, 1, 36, 80, 300, ha);
  ajcTxt(ajMetaTitulo(A), 11, 0, 36, 136, 300, 0.7f * ha);
  CACC(36, 160, 110, 32, ha); CVIDA(154, 160, 32, 32, 16, ha); CVIDA(194, 160, 32, 32, 16, ha);
  for (i = 0; i < 5; i++) gfx_cor(CR(36 + i * 12 + (i ? 14 : 0), 206, i ? 6 : 20, 6), 0.5f, 1, 1, 1, (i ? 0.35f : 0.9f) * ha);
  ajcFoco(AJ_HERO_TRANSICAO, 36, 203, 82, 12, 6);
  ajcFoco(AJ_HERO, 24, 14, 300, 204, 18);
  ajcFoco(AJ_HERO_CHEIO, 8, 8, 624, 344, 18);
  by = land ? 252 : 236;
  gfx_recorte(ajc.x, ajc.y, 640 * ajc.s, ajc.h);
  for (i = 0; i < 6; i++) {
    float cw = land ? 132 : 76, ch = land ? 74 : 114, bx = 36 + i * (cw + 12);
    if (bx > 640) break;
    CIMG(ajArte(land ? AJA_FUNDO : AJA_POSTER, AJ_POS[(i + 3) % 14]), bx, by, cw, ch, 8, i == 0 ? 1.0f : 0.8f);
  }
  gfx_sem_recorte();
  ajcFoco(AJ_LANDSCAPE, 36, by, land ? 132 : 76, land ? 74 : 114, 8);
}
// Fileiras: tres fileiras de verdade (nome, numero da ordem, cartazes), com
// rotulo, nota, selo de assistido, nao lancado e o corte do limite.
static void ajcHomeFileiras(int A) {
  int ordem[3], n = 0, i, k, lim = valor[AJ_FIL_LIMITE];
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.72f);
  for (i = 0; i < fil_n() && n < 3; i++) if (fil_estado(i) != FIL_FORA) ordem[n++] = i;
  for (i = 0; i < 3; i++) {
    float by = 22 + i * (102 + 10 * ajustes_espaco_fileiras()), lx = 70, al = i == 2 && lim <= 2 ? 0.3f : 1.0f;
    const char *t = i < n ? (fil_titulo(ordem[i])[0] ? fil_titulo(ordem[i]) : fil_chave(ordem[i])) : ajTituloN(i);
    char num[4];
    snprintf(num, sizeof num, "%d", i + 1);
    gfx_cor(CR(36, by, 22, 22), 0.5f, 1, 1, 1, 0.12f * al);
    ajcTxt(num, 11, 1, 43, by + 4, 0, 0.8f * al);
    lx += ajcTxt(t, 13, 1, lx, by + 3, 220, al) + 8;
    if (lig(AJ_NOME_ADDON)) { float w2 = ajcTxt("Cinemeta", 10, 0, lx + 8, by + 5, 0, 0) + 16;
      CVIDA(lx, by + 2, w2, 18, 9, al); ajcTxt("Cinemeta", 10, 0, lx + 8, by + 5, 0, 0.7f * al);
      if (i == 0) ajcFoco(AJ_NOME_ADDON, lx, by + 2, w2, 18, 9); lx += w2 + 6; }
    if (lig(AJ_SUFIXO_TIPO)) { float w2 = ajcTxt("Filmes", 10, 0, lx + 8, by + 5, 0, 0) + 16;
      CVIDA(lx, by + 2, w2, 18, 9, al); ajcTxt("Filmes", 10, 0, lx + 8, by + 5, 0, 0.7f * al);
      if (i == 0) ajcFoco(AJ_SUFIXO_TIPO, lx, by + 2, w2, 18, 9); }
    for (k = 0; k < 7; k++) {
      float px = 36 + k * (70 + 12 * ajustes_espaco_titulos()), py = by + 30;
      if (px + 70 > 604) break;
      const char *c = i < n && k < 3 ? ajPosterFileira(ordem[i], k) : ajArte(AJA_POSTER, AJ_POS[(i * 5 + k) % 14]);
      int naoLanc = i == 0 && k == 4;
      if (naoLanc && lig(AJ_OCULTAR_NLANC)) { gfx_anel(CR(px, py, 70, 72), 8.0f / 72, 1.5f * ajc.s, 1, 1, 1, 0.25f * al); }
      else CIMG(c, px, py, 70, 72, 8, (naoLanc ? 0.55f : 1.0f) * al);
      if (naoLanc) { CICON("aj_clock", px + 26, py + 26, 18, 0.8f * al); if (i == 0) ajcFoco(AJ_OCULTAR_NLANC, px, py, 70, 72, 8); }
      if (lig(AJ_ROTULOS)) CBAR(px + 4, py + 64 - 2, 44, 0.75f * al);
      if (valor[AJ_NOTAS_HOME] == 0 && k != 4) { gfx_cor(CR(px + 4, py + 4, 26, 12), 0.25f, 0.96f, 0.77f, 0.09f, al);
        if (i == 0 && k == 0) ajcFoco(AJ_NOTAS_HOME, px + 4, py + 4, 26, 12, 4); }
      if (lig(AJ_SELO_VISTO) && k == 1) { gfx_cor(CR(px + 50, py + 4, 16, 16), 0.5f, ajc.ar, ajc.ag, ajc.ab, al);
        if (i == 0) ajcFoco(AJ_SELO_VISTO, px + 50, py + 4, 16, 16, 8); }
    }
    if (i == 0) { ajcFoco(AJ_ITENS_FILEIRA, 36, by + 30, 568, 72, 8); ajcFoco(AJ_ROTULOS, 36, by + 90, 52, 10, 5); }
  }
  ajcFoco(AJ_FIL_ORDEM, 36, 22, 22, 246, 11);
  { char t[16]; float w2;
    snprintf(t, sizeof t, "%d", lim);
    gfx_cor(CR(36, 240, 568, 2), 0, ajc.ar, ajc.ag, ajc.ab, 0.8f);
    w2 = ajcTxt(t, 11, 1, 0, 0, 0, 0) + 20;
    CACC(604 - w2, 231, w2, 20, 1);
    { int ti = ajTinta(); TxtLinha l = txt_linha(ajEstiloPxS(11 * ajc.s, 1), t, ti, ti, ti, 255);
      txt_desenhar(l, CX(604 - w2 * 0.5f) - l.w * 0.5f, CY(241) - l.h * 0.5f); }
    ajcFoco(AJ_FIL_LIMITE, 604 - w2, 231, w2, 20, 10); }
}
// Barra lateral: o menu do app, com os itens que as opcoes ligam e desligam.
static void ajcHomeBarra(int A) {
  static const struct { const char *ico, *nome; int op; } IT[7] = {
    { "aj_house", "Início", -1 }, { "aj_search", "Buscar", AJ_DESCOBRIR }, { "aj_compass", "Explorar", AJ_MENU_EXPLORAR },
    { "aj_tv-minimal-play", "Guia TV", AJ_MENU_GUIA }, { "aj_calendar", "Agenda", AJ_MENU_AGENDA },
    { "aj_user-round", "Perfil", AJ_MENU_PERFIL }, { "aj_settings-2", "Ajustes", -1 } };
  int larga = !ajustes_rail_recolhida() || ajustes_rail_moderna(), i;
  float rw = larga ? 200 : 64;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.15f);
  gfx_veu_css((GfxRect){ ajc.x, ajc.y, 640 * 0.6f * ajc.s, ajc.h }, 2, 0, 1.0f, 0.75f);
  if (ajLogo(ajArte(AJA_LOGO, A), CX(rw + 70), CY(150), 230 * ajc.s, 70 * ajc.s, 1) <= 0) ajcTxt(ajTituloN(A), 22, 1, rw + 70, 170, 300, 1);
  CACC(rw + 70, 236, 104, 30, 1);
  if (ajustes_rail_moderna_blur() && ajustes_rail_moderna()) ajcBorrada(ajArte(AJA_FUNDO, A), CR(28, 24, rw, 312), 26 * ajc.s, 1);
  CVID(28, 24, rw, 312, 26);
  for (i = 0; i < 7; i++) {
    float iy = 44 + i * 40, al = IT[i].op == AJ_DESCOBRIR || IT[i].op < 0 ? 1.0f : ON(IT[i].op);
    if (i == 0) gfx_cor(CR(36, iy - 6, rw - 16, 32), 0.5f, 1, 1, 1, 0.14f);
    CICON(IT[i].ico, 48, iy, 20, 0.85f * al);
    if (larga) ajcTxt(IT[i].nome, 13, i == 0, 80, iy + 2, rw - 70, (i == 0 ? 1.0f : 0.75f) * al);
    if (IT[i].op >= 0) ajcFoco(IT[i].op, 36, iy - 6, rw - 16, 32, 16);
  }
  ajcFoco(AJ_RAIL, 28, 24, rw, 312, 26);
  ajcFoco(AJ_RAIL_MODERNA, 28, 24, rw, 312, 26);
  ajcFoco(AJ_RAIL_BLUR, 28, 24, rw, 312, 26);
}
// Escolha de perfil: o fundo escolhido e os avatares de verdade.
static void ajcHomePerfil(int A) {
  int n = perfis_n(), qtd = n > 0 ? (n < 4 ? n : 4) : 1, i, v = valor[AJ_PS_FUNDO];
  GfxRect q = { ajc.x, ajc.y, 640 * ajc.s, ajc.h };
  gfx_cor(q, 22 / q.h, 0.047f, 0.051f, 0.063f, 1);
  gfx_recorte(q.x, q.y, q.w, q.h);
  // Os indices sao os de V_PS_FUNDO: 0 Filmes, 1 Listras, 2 Arte do perfil,
  // 3 Luz, 4 Projetor. (1 e 2 estavam trocados aqui: a previa de "Listras"
  // mostrava a arte borrada e a de "Arte do perfil", as listras.)
  if (v == 0) { for (i = 0; i < 24; i++) CIMG(ajArte(AJA_POSTER, AJ_POS[i % 14]), (i % 8) * 82 - 10, (i / 8) * 124 - 14, 76, 116, 8, 0.45f); }
  else if (v == 2) ajcBorrada(ajArte(AJA_FUNDO, A), q, 22, 0.8f);
  else if (v == 1) { char c[1200]; snprintf(c, sizeof c, "%s/marcas/login-fundo.jpg", uxDirArte); ajImg(c, q, 22, 1); }
  else {
    const ContaPerfil *pf = n > 0 ? perfis_item(0) : NULL;
    float lr = 0.43f, lg = 0.18f, lb = 0.92f;
    unsigned cv = 0;
    if (pf && pf->corHex[0] == '#' && sscanf(pf->corHex + 1, "%6x", &cv) == 1) {
      lr = ((cv >> 16) & 255) / 255.0f; lg = ((cv >> 8) & 255) / 255.0f; lb = (cv & 255) / 255.0f;
    }
    if (v == 4) { lr = 0.62f * 0.95f + 0.38f * lr; lg = 0.62f * 0.67f + 0.38f * lg; lb = 0.62f * 0.35f + 0.38f * lb; }
    gfx_rect(CR(v == 4 ? 70 : -120, v == 4 ? -260 : 60, 500, 500), 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
             lr, lg, lb, v == 4 ? 0.55f : 0.45f);
    if (v == 4) { for (i = 0; i < 16; i++) gfx_cor(CR(i * 42 - 6, 4, 22, 12), 0.3f, 0.24f, 0.17f, 0.12f, 1);
                  for (i = 0; i < 16; i++) gfx_cor(CR(i * 42 - 6, 344, 22, 12), 0.3f, 0.24f, 0.17f, 0.12f, 1); }
  }
  gfx_sem_recorte();
  gfx_cor(q, 22 / q.h, 0.031f, 0.031f, 0.039f, 0.45f);
  ajcFoco(AJ_PS_FUNDO, 8, 8, 624, 344, 18);
  { TxtLinha l = ajTxtC(ajEstiloPxS(22 * ajc.s, 1), "Quem está assistindo?", 560 * ajc.s);
    txt_desenhar(l, CX(320) - l.w * 0.5f, CY(70)); }
  for (i = 0; i < qtd; i++) {
    const ContaPerfil *p = n > 0 ? perfis_item(i) : perfis_item_ativo();
    float d = i == 0 ? 104 : 88, cx = 320 + (i - (qtd - 1) * 0.5f) * 132, bx = cx - d * 0.5f, by = i == 0 ? 136 : 144;
    gfx_cor(CR(bx, by, d, d), 0.5f, 0.24f, 0.25f, 0.29f, 1);
    if (p && p->avatarUrl[0]) CIMG(p->avatarUrl, bx, by, d, d, d * 0.5f, i == 0 ? 1.0f : 0.65f);
    else { char av[1200]; snprintf(av, sizeof av, "%s/icones/mt_perfil.png", uxDirArte);
           ajLogo(av, CX(bx + d * 0.22f), CY(by + d * 0.22f), d * 0.56f * ajc.s, d * 0.56f * ajc.s, 0.9f); }
    if (i == 0) gfx_anel_fora(CR(bx, by, d, d), 0.5f, 3 * ajc.s, 2.5f * ajc.s, 0.953f, 0.949f, 0.937f, 0.95f);
    { TxtLinha nm = ajTxtC(ajEstiloPxS(13 * ajc.s, i == 0), p && p->nome[0] ? p->nome : "Perfil", 120 * ajc.s);
      txt_desenhar_alpha(nm, CX(cx) - nm.w * 0.5f, CY(by + d + 12), i == 0 ? 1.0f : 0.6f); }
  }
}

// --- PAGINA DO TITULO ---------------------------------------------------------------
// O que aparece: o fundo com o escurecimento escolhido, a data e os episodios
// (os nao assistidos borrados quando a opcao pede).
static void ajcTitAparece(int A) {
  float veu = valor[AJ_DET_VEU] / 100.0f;
  int i;
  char ep[1200];
  CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 360, 22, 1);
  gfx_veu_css((GfxRect){ ajc.x, ajc.y, 640 * 0.75f * ajc.s, ajc.h }, 2, 0, 1.0f, 0.95f * veu);
  gfx_veu_css(CR(0, 170, 640, 190), 0, 0, 1.0f, 0.9f);
  ajcFoco(AJ_DET_VEU, 8, 8, 624, 150, 18);
  if (ajLogo(ajArte(AJA_LOGO, A), CX(36), CY(36), 220 * ajc.s, 70 * ajc.s, 1) <= 0) ajcTxt(ajTituloN(A), 24, 1, 36, 56, 300, 1);
  { float w2 = ajcTxt(lig(AJ_DET_DATA_CHEIA) ? "14/03/2019" : "2019", 13, 1, 36, 122, 0, 0.9f);
    ajcFoco(AJ_DET_DATA_CHEIA, 32, 118, w2 + 8, 24, 6); }
  for (i = 0; i < 4; i++) {
    float bx = 36 + i * 146;
    int visto = i == 0, borra = !visto && lig(AJ_DET_BLUR_NAO_VISTOS);
    snprintf(ep, sizeof ep, "%s", ajcEp(A, i + 1));
    if (borra) ajcBorrada(ep, CR(bx, 196, 134, 76), 10 * ajc.s, 1); else CIMG(ep, bx, 196, 134, 76, 10, 1);
    if (borra) CICON("aj_eye-off", bx + 55, 222, 24, 0.85f);
    gfx_cor(CR(bx + 8, 262, 118, 4), 0.5f, 1, 1, 1, 0.25f);
    if (visto) gfx_cor(CR(bx + 8, 262, 118, 4), 0.5f, ajc.ar, ajc.ag, ajc.ab, 1);
    CBAR(bx, 284, 90, 0.75f); CBAR(bx, 300, 60, 0.4f);
  }
  ajcFoco(AJ_DET_BLUR_NAO_VISTOS, 182, 196, 426, 76, 10);
}
// Notas: o cartaz e o quadro de notas, cada fonte com o seu logo; o MDBList
// e um grupo com cabecalho proprio.
static void ajcTitNotas(int A) {
  static const int NT[11] = { AJ_NT_IMDB, AJ_NT_TOMATES, AJ_NT_AUDIENCIA, AJ_NT_META, AJ_NT_METAUSER, AJ_NT_TRAKT,
                              AJ_NT_TMDB, AJ_NT_LETTER, AJ_NT_MAL, AJ_NT_EBERT, AJ_NT_SCORE };
  static const int MDB[8] = { AJ_MDB_IMDB, AJ_MDB_TOMATES, AJ_MDB_AUDIENCIA, AJ_MDB_META, AJ_MDB_TRAKT, AJ_MDB_TMDB,
                              AJ_MDB_LETTER, AJ_MDB_MAL };
  int mdb = ajc.op >= AJ_MDB_LIGADO && ajc.op <= AJ_MDB_MAL, n = mdb ? 8 : 11, i, mOn = lig(AJ_MDB_LIGADO);
  const int *g = mdb ? MDB : NT;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.66f);
  CIMG(ajArte(AJA_POSTER, A), 36, 40, 150, 225, 12, 1);
  ajcTxt(ajTituloN(A), 13, 1, 36, 278, 160, 0.9f);
  if (mdb) { float w2 = ajcTxt("MDBList", 13, 1, 222, 26, 0, 0) + 28;
    CVIDA(208, 20, w2, 28, 14, 1); ajcTxt("MDBList", 13, 1, 222, 26, 0, mOn ? 1.0f : 0.4f);
    if (mOn) CACC(208 + w2 + 8, 31, 7, 7, 1);
    ajcFoco(AJ_MDB_LIGADO, 208, 20, w2, 28, 14); }
  for (i = 0; i < n; i++) {
    float bx = 208 + (i % 2) * 204, by = (mdb ? 62 : 26) + (i / 2) * 50;
    const char *nota = "", *m = ajNotaMarca(g[i], &nota);
    float al = lig(g[i]) && (!mdb || mOn) ? 1.0f : 0.3f;
    CVIDA(bx, by, 192, 40, 14, al);
    if (m) { char cam[1200]; snprintf(cam, sizeof cam, "%s/marcas/%s.png", uxDirArte, m);
             ajLogo(cam, CX(bx + 12), CY(by + 9), 30 * ajc.s, 22 * ajc.s, al);
             ajcTxt(rotuloOpcao(g[i]), 10, 0, bx + 50, by + 13, 90, 0.6f * al); }
    else ajcTxt(rotuloOpcao(g[i]), 11, 1, bx + 12, by + 12, 130, 0.85f * al);
    if (!m) nota = g[i] == AJ_NT_MAL || g[i] == AJ_MDB_MAL ? "8.6" : g[i] == AJ_NT_EBERT ? "3.5" : "84";
    ajcTxtDir(nota, 14, 1, bx + 180, by + 11, 60, al);
    ajcFoco(g[i], bx, by, 192, 40, 14);
  }
}
// Dados do titulo: os blocos que o TMDB enriquece, cada um com o que mostra.
static void ajcTitDados(int A) {
  static const int OP[6] = { AJ_TMDB_ELENCO, AJ_TMDB_EPS, AJ_TMDB_TRAILERS, AJ_TMDB_COL, AJ_TMDB_MAIS, AJ_TMDB_FICHA };
  int mestre = lig(AJ_TMDB_LIGADO), i, k;
  char c[1200];
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.68f);
  for (i = 0; i < 6; i++) {
    float bx = 36 + (i % 3) * 196, by = 28 + (i / 3) * 160;
    int o = OP[i], f = o == ajc.op;
    float al = mestre && lig(o) ? 1.0f : 0.3f;
    if (i == 1 && ajc.op == AJ_TMDB_CW) { o = AJ_TMDB_CW; f = 1; al = mestre && lig(o) ? 1.0f : 0.3f; }
    if (i == 5 && (ajc.op == AJ_TMDB_ARTE || ajc.op == AJ_TMDB_BASICO || ajc.op == AJ_TMDB_DATAS || ajc.op == AJ_TMDB_PROD ||
                   ajc.op == AJ_TMDB_REDES)) { o = ajc.op; f = 1; al = mestre && lig(o) ? 1.0f : 0.3f; }
    CVIDA(bx, by, 180, 146, 18, 1);
    switch (i) {
      case 0: for (k = 0; k < 3; k++) { snprintf(c, sizeof c, "%s", ajcElenco(A, k));
                CIMG(c, bx + 14 + k * 54, by + 18, 46, 46, 23, al); } break;
      case 1: for (k = 0; k < 2; k++) { snprintf(c, sizeof c, "%s", ajcEp(A, k + 1));
                CIMG(c, bx + 14 + k * 80, by + 18, 72, 46, 8, al); } break;
      case 2: CIMG(ajArte(AJA_FUNDO, A + 1), bx + 14, by + 14, 152, 56, 8, al); CICON("aj_play", bx + 80, by + 32, 20, al); break;
      case 3: for (k = 2; k >= 0; k--) CIMG(ajArte(AJA_POSTER, AJ_POS[(A + k) % 14]), bx + 50 + k * 16, by + 12 - k * 2 + 4, 46, 66, 6, al * (1.0f - k * 0.2f)); break;
      case 4: for (k = 0; k < 3; k++) CIMG(ajArte(AJA_POSTER, AJ_POS[(A + 5 + k) % 14]), bx + 14 + k * 54, by + 12, 46, 66, 6, al); break;
      default: CBAR(bx + 14, by + 20, 120, 0.7f * al); CBAR(bx + 14, by + 36, 150, 0.4f * al); CBAR(bx + 14, by + 52, 100, 0.4f * al);
               { char t[1200]; snprintf(t, sizeof t, "%s/marcas/tmdb.png", uxDirArte); ajLogo(t, CX(bx + 120), CY(by + 64), 44 * ajc.s, 14 * ajc.s, al); } break;
    }
    ajcTxt(rotuloOpcao(o), 12, f, bx + 14, by + 96, 152, (f ? 1.0f : 0.75f) * (al < 1 ? 0.5f : 1.0f));
    ajcTxt(textoValor(o), 10, 0, bx + 14, by + 116, 152, 0.5f);
    if (f) gfx_anel_fora(CR(bx, by, 180, 146), 18.0f / 146, 3 * ajc.s, 2.5f * ajc.s, ajc.ar, ajc.ag, ajc.ab, 1);
  }
  ajcFoco(AJ_TMDB_LIGADO, 28, 20, 584, 318, 22);
  ajcFoco(AJ_DET_META_EXT, 28, 20, 584, 318, 22);
  ajcFoco(AJ_DET_SO_CINEMETA, 28, 20, 584, 318, 22);
}
// Busca: o campo e os resultados, com a fonte do catalogo.
static void ajcTitBusca(int A) {
  int i;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.7f);
  CVID(36, 28, 568, 48, 24);
  CICON("aj_search", 54, 41, 22, 0.85f);
  ajcTxt(ajTituloN(A), 16, 1, 88, 40, 380, 1);
  gfx_cor(CR(88 + 2, 40, 2, 20), 0, ajc.ar, ajc.ag, ajc.ab, 0);
  { float w2 = ajcTxt("Cinemeta", 11, 1, 0, 0, 0, 0) + 24;
    CVIDA(604 - w2 - 8, 37, w2, 30, 15, ON(AJ_BUSCA_CINEMETA));
    ajcTxt("Cinemeta", 11, 1, 604 - w2 + 4, 44, 0, ON(AJ_BUSCA_CINEMETA));
    ajcFoco(AJ_BUSCA_CINEMETA, 604 - w2 - 8, 37, w2, 30, 15); }
  for (i = 0; i < 12; i++) CIMG(ajArte(AJA_POSTER, AJ_POS[(A + i) % 14]), 36 + (i % 6) * 96, 100 + (i / 6) * 138, 84, 126, 10, i == 0 ? 1.0f : 0.8f);
}

// --- CARTAZES E ARTE ----------------------------------------------------------------
// Cartaz em foco: o cartaz que abre a arte deitada, com o atraso, a borda e o
// gradiente classico.
static void ajcCarFoco(int A) {
  int exp = ajustes_expandir_poster(), i;
  float fw = exp ? 270 : 104, raio = ajustes_raio_poster_px() * 104 / 256.0f;
  GfxRect f;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.72f);
  CIMG(ajArte(AJA_POSTER, AJ_POS[2]), 36, 110, 104, 156, raio, 0.7f);
  f = CR(156, 104, fw, 168);
  gfx_rect((GfxRect){ f.x - 14 * ajc.s, f.y + 6 * ajc.s, f.w + 28 * ajc.s, f.h + 30 * ajc.s }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.6f);
  ajImg(ajArte(exp ? AJA_FUNDO : AJA_POSTER, A), f, raio * ajc.s, 1);
  if (exp) { ajLogo(ajArte(AJA_LOGO, A), CX(170), CY(206), 140 * ajc.s, 42 * ajc.s, 1);
             CIMG(ajArte(AJA_POSTER, A), 156, 104, 112, 168, raio, 1); }
  if (lig(AJ_GRAD_CLASSICO)) gfx_veu_base(f, raio * ajc.s / f.h, 0.55f, 0.85f);
  if (ajustes_borda_foco()) gfx_anel(f, raio * ajc.s / f.h, 3 * ajc.s, 0.953f, 0.949f, 0.937f, 0.95f);
  ajcFoco(AJ_EXPANDIR, 156, 104, fw, 168, raio);
  ajcFoco(AJ_GRAD_CLASSICO, 156, 190, fw, 82, raio);
  if (ajc.op == AJ_BORDA_FOCO) CACC(156 + fw * 0.5f - 4, 288, 8, 8, 1);
  for (i = 0; i < 2; i++) CIMG(ajArte(AJA_POSTER, AJ_POS[5 + i]), 156 + fw + 18 + i * 122, 110, 104, 156, raio, 0.7f);
  { float w2 = ajcTxt(textoValor(AJ_EXPANDIR_ATRASO), 12, 1, 0, 0, 0, 0) + 48;
    CVID(156, 52, w2, 32, 16); CICON("aj_clock", 168, 59, 18, 0.85f);
    ajcTxt(textoValor(AJ_EXPANDIR_ATRASO), 12, 1, 194, 60, 0, exp ? 1.0f : 0.4f);
    ajcFoco(AJ_EXPANDIR_ATRASO, 156, 52, w2, 32, 16); }
}
// Forma: a profundidade em cada tipo de cartao (cartaz, continuar, episodio,
// elenco, trailer), com brilho, reflexo e cobertura da borda.
static void ajcCarForma(int A) {
  static const int OP[5] = { AJ_PROF_POSTERS, AJ_PROF_CW, AJ_PROF_EPS, AJ_PROF_ELENCO, AJ_PROF_TRAILERS };
  int prof = ajustes_profundidade(), i;
  float borda = valor[AJ_PROF_BORDA] / 100.0f, brilho = valor[AJ_PROF_BRILHO] / 100.0f, cob = valor[AJ_PROF_COBERTURA] / 100.0f;
  GfxRect g;
  char c[1200];
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.7f);
  g = CR(48, 36, 180, 270);
  gfx_rect((GfxRect){ g.x - 18 * ajc.s, g.y + 8 * ajc.s, g.w + 36 * ajc.s, g.h + 40 * ajc.s }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, prof ? 0.7f : 0.3f);
  ajImg(ajArte(AJA_POSTER, A), g, 16 * ajc.s, 1);
  if (prof) { gfx_brilho_topo(g, 16 * ajc.s / g.h, 0.25f + 0.5f * cob, 1, 1, 1, 0.35f * brilho + 0.05f);
              gfx_anel(g, 16 * ajc.s / g.h, 2 * ajc.s, 1, 1, 1, 0.45f * borda); }
  ajcFoco(AJ_PROF, 48, 36, 180, 270, 16);
  ajcFoco(AJ_PROF_BORDA, 48, 36, 180, 270, 16);
  ajcFoco(AJ_PROF_BRILHO, 48, 36, 180, 120, 16);
  ajcFoco(AJ_PROF_COBERTURA, 48, 36, 180, 270, 16);
  for (i = 0; i < 5; i++) {
    float bx = 268 + (i % 2) * 174, by = 36 + (i / 2) * 98, cw = i == 0 ? 56 : i == 3 ? 72 : 150, ch = i == 0 ? 84 : 72;
    float al = prof && lig(OP[i]) ? 1.0f : 0.45f;
    GfxRect r;
    if (i == 3) { snprintf(c, sizeof c, "%s", ajcElenco(A, 0)); r = CR(bx + 39, by + 6, 72, 72); ajImg(c, r, 36 * ajc.s, 1); }
    else if (i == 2) { snprintf(c, sizeof c, "%s", ajcEp(A, 1)); r = CR(bx, by + 6, cw, ch); ajImg(c, r, 10 * ajc.s, 1); }
    else { r = CR(bx + (i == 0 ? 47 : 0), by + 2, cw, ch); ajImg(ajArte(i == 0 ? AJA_POSTER : AJA_FUNDO, AJ_POS[(A + i) % 14]), r, 10 * ajc.s, 1); }
    if (i == 1) gfx_cor((GfxRect){ r.x + 8 * ajc.s, r.y + r.h - 9 * ajc.s, (r.w - 16 * ajc.s) * 0.6f, 4 * ajc.s }, 0.5f, ajc.ar, ajc.ag, ajc.ab, 1);
    if (i == 4) CICON("aj_play", bx + 65, by + 28, 20, 0.9f);
    if (al >= 1) { gfx_brilho_topo(r, (i == 3 ? 0.5f : 10 * ajc.s / r.h), 0.3f, 1, 1, 1, 0.25f); gfx_anel(r, i == 3 ? 0.5f : 10 * ajc.s / r.h, 1.5f * ajc.s, 1, 1, 1, 0.35f); }
    else gfx_cor(r, i == 3 ? 0.5f : 10 * ajc.s / r.h, 0.031f, 0.031f, 0.039f, 0.35f);
    if (OP[i] == ajc.op) gfx_anel_fora(r, i == 3 ? 0.5f : 10 * ajc.s / r.h, 3 * ajc.s, 2.5f * ajc.s, ajc.ar, ajc.ag, ajc.ab, 1);
  }
}
// De onde vem a arte: fundo, cartaz e logo, cada um com a sua fonte; o
// cartao ao lado do destaque (mesma arte ou outra) e as pastas da conta.
static void ajcCarOrigem(int A) {
  int pv = valor[AJ_POSTER_PROV] != 0, k, f;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.72f);
  CIMG(ajArte(AJA_FUNDO, A), 36, 36, 280, 158, 14, 1);
  ajcTxt(rotuloOpcao(AJ_HERO_FUNDO), 11, 0, 36, 202, 200, 0.55f);
  ajcTxt(textoValor(AJ_HERO_FUNDO), 13, 1, 36, 220, 200, 1);
  f = ajc.op == AJ_HERO_FUNDO || ajc.op == AJ_ADDON_FUNDO;
  if (f) gfx_anel_fora(CR(36, 36, 280, 158), 14.0f / 158, 3 * ajc.s, 2.5f * ajc.s, ajc.ar, ajc.ag, ajc.ab, 1);
  CIMG(ajArte(AJA_FUNDO, lig(AJ_HERO_ARTE_DIF) ? A + 7 : A), 236, 140, 96, 54, 8, 1);
  gfx_anel(CR(236, 140, 96, 54), 8.0f / 54, 2 * ajc.s, 0.953f, 0.949f, 0.937f, 0.9f);
  ajcFoco(AJ_HERO_ARTE_DIF, 236, 140, 96, 54, 8);
  CIMG(ajArte(AJA_POSTER, A), 352, 36, 118, 177, 12, 1);
  if (pv) { gfx_cor(CR(352, 189, 118, 24), 0, 0, 0, 0, 0.6f); gfx_cor(CR(360, 196, 30, 10), 0.3f, 0.96f, 0.77f, 0.09f, 1);
            gfx_cor(CR(396, 196, 30, 10), 0.3f, 0.9f, 0.25f, 0.2f, 1); }
  ajcTxt(textoValor(AJ_POSTER_PROV), 13, 1, 352, 220, 118, 1);
  f = ajc.op == AJ_POSTER_PROV || ajc.op == AJ_ADDON_POSTER || (ajc.op >= AJ_POSTER_INST && ajc.op <= AJ_POSTER_TESTAR);
  if (f) gfx_anel_fora(CR(352, 36, 118, 177), 12.0f / 177, 3 * ajc.s, 2.5f * ajc.s, ajc.ar, ajc.ag, ajc.ab, 1);
  CVID(490, 36, 114, 70, 14);
  ajLogo(ajArte(AJA_LOGO, A), CX(502), CY(48), 90 * ajc.s, 46 * ajc.s, 1);
  ajcTxt(lig(AJ_ADDON_LOGO) ? "Addon" : "TMDB", 13, 1, 490, 116, 114, 1);
  ajcFoco(AJ_ADDON_LOGO, 490, 36, 114, 70, 14);
  for (k = 2; k >= 0; k--) CIMG(ajArte(AJA_POSTER, AJ_POS[(A + k) % 14]), 500 + k * 14, 186 - k * 6, 64, 96, 8, lig(AJ_COL_ARTE_CONTA) ? 1.0f - k * 0.2f : 0.35f);
  ajcFoco(AJ_COL_ARTE_CONTA, 500, 174, 92, 108, 8);
  if (ajc.op >= AJ_POSTER_INST && ajc.op <= AJ_POSTER_TESTAR) { CVID(352, 252, 252, 40, 20); CICON("aj_key-round", 366, 262, 20, 0.85f);
    ajcTxt(textoValor(ajc.op), 12, 0, 396, 264, 196, 0.8f); }
}

// --- TRAILERS ------------------------------------------------------------------------
// Onde toca: destaque, cartaz em foco e pagina do titulo, cada um ligado ou nao.
static void ajcTrlOnde(int A) {
  static const int OP[3] = { AJ_HERO_TRAILER, AJ_FOCO_TRAILER, AJ_DET_TRAILER_AUTO };
  int i, k;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.75f);
  for (i = 0; i < 3; i++) {
    float bx = 36 + i * 196, al = ON(OP[i]);
    ajcTela(ajArte(AJA_FUNDO, A + i * 3), bx, 70, 180, 102, al);
    if (i == 1) for (k = 0; k < 3; k++) CIMG(ajArte(AJA_POSTER, AJ_POS[(A + k) % 14]), bx + 10 + k * 56, 120, 44, 66, 6, k == 0 ? al : 0.6f * al);
    else ajLogo(ajArte(AJA_LOGO, A + i * 3), CX(bx + 10), CY(132), 90 * ajc.s, 28 * ajc.s, al);
    if (lig(OP[i])) { CVID(bx + 8, 78, 30, 22, 11); CICON("aj_play", bx + 16, 82, 14, 0.9f); }
    gfx_cor(CR(bx + 10, 164, 160, 3), 0.5f, 1, 1, 1, 0.25f * al);
    if (lig(OP[i])) gfx_cor(CR(bx + 10, 164, 70, 3), 0.5f, ajc.ar, ajc.ag, ajc.ab, 1);
    ajcTxt(rotuloOpcao(OP[i]), 12, OP[i] == ajc.op, bx, 196, 180, OP[i] == ajc.op ? 1.0f : 0.7f);
    ajcTxt(textoValor(OP[i]), 11, 0, bx, 216, 180, 0.5f);
    ajcFoco(OP[i], bx, 70, 180, 102, 12);
  }
  { float w2 = ajcTxt("Trailer", 11, 1, 0, 0, 0, 0) + 24, al = ON(AJ_DET_TRAILER);
    CVIDA(428 + 8, 250, w2, 26, 13, al); ajcTxt("Trailer", 11, 1, 448, 255, 0, al);
    ajcFoco(AJ_DET_TRAILER, 436, 250, w2, 26, 13); }
}
// Som e imagem: o trailer tocando com a fonte, o som, a espera, a qualidade
// e a proporcao (tarjas) escolhidas.
static void ajcTrlSom(int A) {
  int q = valor[AJ_TRAILER_QUAL], tarja = valor[AJ_TRAILER_ASPECTO] == 0, som = lig(AJ_HERO_TRAILER_SOM) || lig(AJ_DET_TRAILER_SOM);
  FormatoMarca m = q == 3 ? FMT_720 : FMT_1080;
  float mw;
  CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 360, 22, 1);
  if (tarja) { gfx_cor(CR(0, 0, 640, 40), 0, 0, 0, 0, 0.92f); gfx_cor(CR(0, 320, 640, 40), 0, 0, 0, 0, 0.92f); }
  ajcFoco(AJ_TRAILER_ASPECTO, 8, 8, 624, 344, 18);
  ajcFoco(AJ_TRAILER_ZOOM_TPK, 8, 8, 624, 344, 18);
  gfx_veu_css(CR(0, 160, 640, 200), 0, 0, 1.0f, 0.75f);
  if (!lig(AJ_LOGO_TRAILER)) ajLogo(ajArte(AJA_LOGO, A), CX(36), CY(204), 200 * ajc.s, 60 * ajc.s, 1);
  ajcFoco(AJ_LOGO_TRAILER, 32, 200, 208, 68, 8);
  { float w2 = ajcTxt(textoValor(AJ_TRAILER_FONTE), 12, 1, 0, 0, 220, 0) + 52;
    CVID(28, 54, w2, 32, 16); CICON("aj_server", 40, 61, 18, 0.85f);
    ajcTxt(textoValor(AJ_TRAILER_FONTE), 12, 1, 66, 62, 220, 1);
    ajcFoco(AJ_TRAILER_FONTE, 28, 54, w2, 32, 16); }
  CVID(570, 54, 42, 42, 21); CICON("aj_audio-lines", 580, 64, 22, som ? 0.95f : 0.3f);
  if (!som) gfx_cor(CR(578, 74, 26, 2.5f), 0.5f, 1, 1, 1, 0.9f);
  ajcFoco(AJ_HERO_TRAILER_SOM, 570, 54, 42, 42, 21); ajcFoco(AJ_DET_TRAILER_SOM, 570, 54, 42, 42, 21);
  mw = marca_formato_largura(m, 22 * ajc.s) / ajc.s;
  marca_formato(m, CX(604 - mw), CY(282), 22 * ajc.s, 1, 1, 1, 0.9f);
  ajcFoco(AJ_TRAILER_QUAL, 600 - mw, 280, mw + 8, 26, 6);
  { float w2 = ajcTxt(textoValor(AJ_HERO_TRAILER_ESPERA), 12, 1, 0, 0, 0, 0) + 48;
    CVID(36, 280, w2, 30, 15); CICON("aj_clock", 46, 286, 18, 0.85f);
    ajcTxt(textoValor(AJ_HERO_TRAILER_ESPERA), 12, 1, 72, 287, 0, 1);
    ajcFoco(AJ_HERO_TRAILER_ESPERA, 36, 280, w2, 30, 15); }
}

// --- APARENCIA ------------------------------------------------------------------------
// Cores e vidro: a ilha de verdade sobre a arte (opacidade, fosco, contorno)
// e a escala das cinco opacidades.
static void ajcApaVidro(int A) {
  int vid = ajVid(), i, op5 = valor[AJ_VIDRO_OPAC];
  GfxRect il = CR(36, 36, 290, 288);
  CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 360, 22, 1);
  if (vid && ajustes_vidro_fosco()) { gfx_recorte(il.x, il.y, il.w, il.h); ajcBorrada(ajArte(AJA_FUNDO, A), CR(0, 0, 640, 360), 0, 1); gfx_sem_recorte(); }
  if (vid) gfx_cor(il, 26 * ajc.s / il.h, 0.055f, 0.059f, 0.071f, ajustes_vidro_opacidade());
  else gfx_cor(il, 26 * ajc.s / il.h, 0.082f, 0.086f, 0.102f, 0.98f);
  if (vid && ajustes_vidro_contorno()) gfx_anel(il, 26 * ajc.s / il.h, 1.5f * ajc.s, 1, 1, 1, 0.22f);
  for (i = 0; i < 5; i++) {
    float by = 60 + i * 50;
    if (i == 1) gfx_cor(CR(48, by - 10, 266, 40), 0.5f, 1, 1, 1, 0.12f);
    CBAR(66, by + 6, i == 1 ? 150 : 110 + (i * 23) % 60, i == 1 ? 0.9f : 0.5f);
  }
  ajcFoco(AJ_VIDRO, 36, 36, 290, 288, 26);
  ajcFoco(AJ_VIDRO_CONTORNO, 36, 36, 290, 288, 26);
  ajcFoco(AJ_VIDRO_FOSCO, 36, 36, 290, 288, 26);
  CVID(352, 230, 252, 94, 22);
  for (i = 0; i < 5; i++) {
    static const float F[5] = { 0.60f, 0.70f, 0.78f, 0.86f, 0.92f };
    GfxRect r = CR(368 + i * 46, 246, 38, 38);
    ajImg(ajArte(AJA_FUNDO, A), r, 10 * ajc.s, 1);
    gfx_cor(r, 10 * ajc.s / r.h, 0.055f, 0.059f, 0.071f, F[i]);
    if (i == op5) gfx_anel_fora(r, 10 * ajc.s / r.h, 2 * ajc.s, 2 * ajc.s, 0.953f, 0.949f, 0.937f, 0.9f);
  }
  ajcTxt(textoValor(AJ_VIDRO_OPAC), 11, 1, 368, 296, 220, 0.75f);
  ajcFoco(AJ_VIDRO_OPAC, 352, 230, 252, 94, 22);
}
// Movimento e relogio: o cartaz que anda (com o rastro da mola, ou parado
// nas reduzidas) e a ilha do relogio no canto.
static void ajcApaMovimento(int A) {
  int red = valor[AJ_ANIM] == 1, i;
  char hora[12];
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.55f);
  for (i = 0; i < 5; i++) CIMG(ajArte(AJA_POSTER, AJ_POS[(A + i) % 14]), 36 + i * 118, 120, 100, 150, 12, i == 2 ? 1.0f : 0.55f);
  if (!red) for (i = 1; i <= 3; i++) gfx_anel(CR(272 - i * 26, 120, 100, 150), 12.0f / 150, 2 * ajc.s, ajc.ar, ajc.ag, ajc.ab, 0.5f - i * 0.12f);
  gfx_anel_fora(CR(272, 120, 100, 150), 12.0f / 150, 3 * ajc.s, 2.5f * ajc.s, 0.953f, 0.949f, 0.937f, 0.95f);
  for (i = 0; i < 24; i++) {
    float t = i / 23.0f, v = red ? (t > 0.02f ? 1.0f : 0.0f) : 1.0f - (1 + 8 * t) * expf(-8 * t);
    gfx_cor(CR(40 + t * 180, 330 - v * 36, 4, 4), 0.5f, ajc.ar, ajc.ag, ajc.ab, 0.9f);
  }
  ajcFoco(AJ_ANIM, 28, 112, 584, 230, 16);
  if (lig(AJ_RELOGIO)) { ajCenaHora(hora, sizeof hora); CVID(486, 26, 126, 46, 23); ajcTxt(hora, 20, 1, 506, 35, 0, 1); }
  else gfx_anel(CR(486, 26, 126, 46), 0.5f, 1.5f * ajc.s, 1, 1, 1, 0.2f);
  ajcFoco(AJ_RELOGIO, 486, 26, 126, 46, 23);
}

// --- REPRODUCAO -----------------------------------------------------------------------
// Escolha da fonte: a folha de Fontes, com os selos de verdade, a ordem, o
// automatico e a espera pelos add-ons.
static void ajcRepFonte(int A) {
  static const char *const ADD[4] = { "Torrentio", "AIOStreams", "Comet", "MediaFusion" };
  static const char *const SEL[4][4] = { { "r-4k", "v-dv", "a-atmos", "q-remux" }, { "r-4k", "v-hdr10", "a-ddp", "q-webdl" },
                                         { "r-1080", "v-sdr", "a-dd", "q-bluray" }, { "r-720", "a-dd", NULL, NULL } };
  int i, k, cores = lig(AJ_SELOS_CORES), auto_ = valor[AJ_FONTE_AUTO] == 0;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.3f);
  gfx_veu_css((GfxRect){ ajc.x, ajc.y, 640 * 0.5f * ajc.s, ajc.h }, 2, 0, 1.0f, 0.8f);
  if (ajLogo(ajArte(AJA_LOGO, A), CX(28), CY(40), 170 * ajc.s, 54 * ajc.s, 1) <= 0) ajcTxt(ajTituloN(A), 18, 1, 28, 54, 180, 1);
  CVID(220, 20, 396, 320, 24);
  ajcFoco(AJ_FONTE_MANUAL, 220, 20, 396, 320, 24);
  { float w2 = ajcTxt(textoValor(AJ_FONTE_AUTO), 11, 1, 0, 0, 160, 0) + 40;
    if (auto_) CACC(236, 34, w2, 26, 0.9f); else CVID(236, 34, w2, 26, 13);
    CICON("aj_zap", 244, 39, 16, 0.9f);
    { int ti = auto_ ? ajTinta() : 243; TxtLinha l = txt_linha_corta(ajEstiloPxS(11 * ajc.s, 1), textoValor(AJ_FONTE_AUTO), ti, ti, ti, 255, 160 * ajc.s);
      txt_desenhar(l, CX(264), CY(47) - l.h * 0.5f); }
    ajcFoco(AJ_FONTE_AUTO, 236, 34, w2, 26, 13); }
  { float w2 = ajcTxt(textoValor(AJ_FONTE_PRAZO), 11, 1, 0, 0, 0, 0) + 40;
    CVID(600 - w2, 34, w2, 26, 13); CICON("aj_clock", 608 - w2, 39, 16, 0.85f);
    ajcTxt(textoValor(AJ_FONTE_PRAZO), 11, 1, 628 - w2, 41, 0, 0.85f);
    ajcFoco(AJ_FONTE_PRAZO, 600 - w2, 34, w2, 26, 13); }
  for (i = 0; i < 4; i++) {
    float by = 74 + i * 64;
    uint64_t m = 0;
    for (k = 0; k < 4; k++) if (SEL[i][k]) m |= badges_bit(SEL[i][k]);
    if (i == 0) gfx_cor(CR(230, by, 376, 58), 0.3f, 1, 1, 1, 0.12f);
    { char n[4]; snprintf(n, sizeof n, "%d", i + 1); ajcTxt(n, 11, 1, 240, by + 8, 0, 0.45f); }
    if (valor[AJ_FONTE_TEXTO] == 1) { CBAR(260, by + 12, 200, 0.7f); CBAR(260, by + 26, 150, 0.4f); }
    else ajcTxt(ADD[i], 12, i == 0, 260, by + 6, 200, i == 0 ? 1.0f : 0.75f);
    if (cores) badges_desenhar_selos(m, CX(260), CY(by + 32), 300 * ajc.s, 16 * ajc.s, 1);
    else badges_desenhar(m, CX(260), CY(by + 32), 300 * ajc.s, 16 * ajc.s, 0.85f);
    if (i == 1) CICON("aj_rotate-cw", 580, by + 20, 16, ON(AJ_FONTE_REPOR) * 0.85f);
  }
  ajcFoco(AJ_FONTE_PRIORIDADE, 232, 74, 20, 250, 8);
  ajcFoco(AJ_FONTE_HDR, 258, 104, 64, 22, 6);
  ajcFoco(AJ_FONTE_REPOR, 574, 152, 28, 28, 14);
  ajcFoco(AJ_FONTE_TEXTO, 256, 76, 220, 20, 6);
  ajcFoco(AJ_SELOS_CORES, 258, 104, 220, 22, 6);
  ajcFoco(AJ_SELOS_PACOTE, 258, 104, 220, 22, 6);
  ajcFoco(AJ_SELOS_PACOTE_ADD, 258, 104, 220, 22, 6);
  ajcFoco(AJ_SELOS_PACOTE_REM, 258, 104, 220, 22, 6);
}
// Imagem e som: o painel de informacao do player (video, audio, as
// resolucoes que o teto deixa passar) e o trecho guardado no disco.
static void ajcRepImagem(int A) {
  static const FormatoMarca F[3] = { FMT_720, FMT_1080, FMT_4K };
  int sel = valor[AJ_QUALIDADE], i;
  float bx;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.55f);
  CVID(36, 30, 276, 250, 24);
  ajcTxt("Vídeo", 13, 1, 56, 48, 0, 0.6f);
  bx = 56 + marca_formato(FMT_4K, CX(56), CY(78), 30 * ajc.s, 1, 1, 1, sel <= 1 ? 0.95f : 0.3f) / ajc.s + 14;
  { float mw = marca_formato(FMT_DV, CX(bx), CY(78), 30 * ajc.s, 1, 1, 1, lig(AJ_DV) ? 0.95f : 0.25f) / ajc.s;
    ajcFoco(AJ_DV, bx - 4, 74, mw + 8, 38, 8); }
  ajcTxt("Áudio", 13, 1, 56, 140, 0, 0.6f);
  { float mw = marca_formato(FMT_ATMOS, CX(56), CY(168), 30 * ajc.s, 1, 1, 1, lig(AJ_ATMOS) ? 0.95f : 0.25f) / ajc.s;
    ajcFoco(AJ_ATMOS, 52, 164, mw + 8, 38, 8); }
  for (i = 0; i < 7; i++) gfx_cor(CR(56 + i * 34, 228, 22, 22), 0.5f, 1, 1, 1, i < 5 ? 0.6f : 0.25f);
  CVID(328, 30, 276, 250, 24);
  for (i = 0; i < 3; i++) {
    int passa = sel == 0 || sel == 1 || (sel == 2 && i <= 1) || (sel == 3 && i == 0);
    float h2 = 60 + i * 50, cx = 352 + i * 82, mw = marca_formato_largura(F[i], 18 * ajc.s) / ajc.s;
    gfx_cor(CR(cx, 230 - h2, 64, h2), 12.0f / h2, passa ? ajc.ar : 1, passa ? ajc.ag : 1, passa ? ajc.ab : 1, passa ? 0.7f : 0.07f);
    marca_formato(F[i], CX(cx + 32 - mw * 0.5f), CY(244), 18 * ajc.s, 1, 1, 1, passa ? 0.9f : 0.3f);
  }
  ajcFoco(AJ_QUALIDADE, 328, 30, 276, 250, 24);
  gfx_cor(CR(36, 310, 568, 6), 0.5f, 1, 1, 1, 0.2f);
  if (valor[AJ_CACHE_SEEK] != 0) gfx_cor(CR(36, 310, 400, 6), 0.5f, 1, 1, 1, 0.45f);
  CACC(36, 310, 220, 6, 1);
  gfx_cor(CR(250, 305, 16, 16), 0.5f, 1, 1, 1, 1);
  ajcFoco(AJ_CACHE_SEEK, 256, 304, 184, 18, 6);
}
// Durante e depois: o painel da pausa, o cartao dos creditos com a reacao e
// para onde o app volta ao sair.
static void ajcRepDurante(int A) {
  int i;
  CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 360, 22, 1);
  gfx_veu_css((GfxRect){ ajc.x, ajc.y, 640 * 0.6f * ajc.s, ajc.h }, 2, 0, 1.0f, lig(AJ_PAUSA_OVERLAY) ? 0.9f : 0.3f);
  if (lig(AJ_PAUSA_OVERLAY)) {
    if (ajLogo(ajArte(AJA_LOGO, A), CX(36), CY(40), 200 * ajc.s, 60 * ajc.s, 1) <= 0) ajcTxt(ajTituloN(A), 22, 1, 36, 58, 260, 1);
    ajcTxt(ajMetaTitulo(A), 11, 0, 36, 112, 260, 0.7f);
    CBAR(36, 136, 240, 0.55f); CBAR(36, 152, 220, 0.55f); CBAR(36, 168, 150, 0.55f);
  }
  ajcFoco(AJ_PAUSA_OVERLAY, 28, 32, 270, 150, 12);
  CVID(372, 194, 240, 112, 20);
  CIMG(ajArte(AJA_FUNDO, A + 1), 384, 206, 104, 58, 8, 1);
  gfx_anel(CR(500, 214, 40, 40), 0.5f, 3 * ajc.s, ajc.ar, ajc.ag, ajc.ab, 1);
  ajcTxt("Próximo episódio", 11, 1, 384, 276, 200, 0.85f);
  if (lig(AJ_REACAO_CREDITOS)) { CICON("aj_thumbs-up", 556, 212, 18, 0.9f); CICON("aj_heart", 556, 238, 18, 0.9f); }
  ajcFoco(AJ_REACAO_CREDITOS, 548, 204, 34, 60, 10);
  gfx_cor(CR(36, 326, 568, 5), 0.5f, 1, 1, 1, 0.22f); CACC(36, 326, 530, 5, 1);
  { float w2 = ajcTxt(textoValor(AJ_SAIDA_PLAYER), 11, 1, 0, 0, 220, 0) + 44;
    CVID(36, 280, w2, 28, 14); CICON("aj_corner-up-left", 46, 285, 16, 0.85f);
    ajcTxt(textoValor(AJ_SAIDA_PLAYER), 11, 1, 70, 287, 220, 0.9f);
    ajcFoco(AJ_SAIDA_PLAYER, 36, 280, w2, 28, 14); }
  { float w3 = ajcTxt(textoValor(AJ_PROPORCAO_PADRAO), 11, 1, 0, 0, 160, 0) + 44;
    CVID(372, 150, w3, 28, 14); CICON("aj_maximize-2", 382, 155, 16, 0.85f);
    ajcTxt(textoValor(AJ_PROPORCAO_PADRAO), 11, 1, 406, 157, 160, 0.9f);
    ajcFoco(AJ_PROPORCAO_PADRAO, 372, 150, w3, 28, 14); }
  (void)i;
}
// Seekr: a barra de tempo com a miniatura do ponto e a fita (anterior e
// seguinte), a chave e a sincronia.
static void ajcRepSeekr(int A) {
  float al = ON(AJ_SEEKR_LIGADO);
  int fita = lig(AJ_SEEKR_FITA), i;
  char ep[1200];
  CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 360, 22, 1);
  gfx_veu_css(CR(0, 120, 640, 240), 0, 0, 1.0f, 0.9f);
  for (i = -1; i <= 1; i++) {
    float cw = i ? 140 : 196, ch = cw * 9 / 16, cx = 320 + i * 178 - cw * 0.5f, cy = 250 - ch;
    if (i && !fita) continue;
    snprintf(ep, sizeof ep, "%s", ajcEp(A, i + 3));
    CIMG(ep, cx, cy, cw, ch, 10, (i ? 0.6f : 1.0f) * al);
    if (!i) gfx_anel(CR(cx, cy, cw, ch), 10.0f / ch, 2 * ajc.s, 1, 1, 1, 0.9f * al);
    ajcTxt(i < 0 ? "41:10" : i > 0 ? "41:30" : "41:20", 11, !i, cx + cw * 0.5f - 16, 256, 0, (i ? 0.5f : 1.0f) * al);
  }
  ajcFoco(AJ_SEEKR_FITA, 92, 160, 456, 112, 10);
  ajcFoco(AJ_SEEKR_LIGADO, 222, 140, 196, 112, 10);
  gfx_cor(CR(36, 300, 568, 5), 0.5f, 1, 1, 1, 0.22f); CACC(36, 300, 284, 5, 1);
  gfx_cor(CR(312, 294, 16, 16), 0.5f, 1, 1, 1, 1);
  { float w2 = ajcTxt("Seekr", 12, 1, 0, 0, 0, 0) + 52;
    CVID(28, 24, w2 + 120, 34, 17); CICON("aj_key-round", 40, 32, 18, 0.85f);
    ajcTxt("Seekr", 12, 1, 66, 31, 0, 1); ajcTxt(textoValor(AJ_SEEKR_CHAVE), 11, 0, 66 + w2 - 40, 32, 110, 0.6f);
    ajcFoco(AJ_SEEKR_CHAVE, 28, 24, w2 + 120, 34, 17); ajcFoco(AJ_SEEKR_TESTAR, 28, 24, w2 + 120, 34, 17); }
  { float w2 = ajcTxt(textoValor(AJ_SEEKR_AJUSTE), 12, 1, 0, 0, 0, 0) + 28;
    CVID(320 + 98 + 10, 90, w2, 28, 14); ajcTxt(textoValor(AJ_SEEKR_AJUSTE), 12, 1, 442, 96, 0, al);
    ajcFoco(AJ_SEEKR_AJUSTE, 428, 90, w2, 28, 14); }
}

// --- IDIOMAS E LEGENDAS ---------------------------------------------------------------
// Interface e metadados: o menu e a pagina do titulo no idioma escolhido.
static void ajcIdiInterface(int A) {
  static const char *const M[4] = { "Início", "Buscar", "Explorar", "Ajustes" };
  int i;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.55f);
  CVID(36, 36, 210, 288, 24);
  CICON("aj_languages", 56, 54, 22, 0.85f);
  ajcTxt(textoValor(AJ_IDIOMA), 12, 1, 86, 56, 150, 1);
  for (i = 0; i < 4; i++) {
    if (i == 3) gfx_cor(CR(46, 98 + i * 48 - 8, 190, 36), 0.5f, 1, 1, 1, 0.12f);
    ajcTxt(M[i], 14, i == 3, 64, 98 + i * 48, 160, i == 3 ? 1.0f : 0.7f);
  }
  ajcFoco(AJ_IDIOMA, 36, 36, 210, 288, 24);
  CIMG(ajArte(AJA_FUNDO, A), 266, 36, 338, 190, 16, 1);
  gfx_veu_css(CR(266, 120, 338, 106), 0, 0, 1.0f, 0.85f);
  ajLogo(ajArte(AJA_LOGO, A), CX(284), CY(150), 150 * ajc.s, 46 * ajc.s, 1);
  CBAR(266, 244, 300, 0.55f); CBAR(266, 260, 260, 0.55f); CBAR(266, 276, 180, 0.55f);
  { float w2 = ajcTxt(textoValor(AJ_TMDB_IDIOMA), 11, 1, 0, 0, 200, 0) + 44;
    CVID(266, 296, w2, 28, 14); CICON("aj_file-text", 276, 301, 16, 0.85f);
    ajcTxt(textoValor(AJ_TMDB_IDIOMA), 11, 1, 300, 303, 200, 0.9f); }
  ajcFoco(AJ_TMDB_IDIOMA, 266, 36, 338, 288, 16);
}
// Audio e legendas: o player com a legenda e o painel de faixas.
static void ajcIdiFaixas(int A) {
  static const int OP[5] = { AJ_AUD_LINGUA, AJ_LEG_LINGUA, AJ_LEG_LINGUA2, AJ_LEG_SYNC_AUTO, AJ_LEG_SYNC_AUDIO };
  static const char *const IC[5] = { "aj_audio-lines", "aj_captions", "aj_captions", "aj_activity", "aj_activity" };
  int i, n = 4;
#ifdef NV_ANDROID
  n = 5;
#endif
  if (ajc.op == AJ_LEG_SYNC_AUDIO) n = 5;
  CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 360, 22, 1);
  gfx_veu_css(CR(0, 220, 640, 140), 0, 0, 1.0f, 0.6f);
  gfx_cor(CR(70, 296, 260, 20), 0.25f, 0, 0, 0, 0.55f); CBAR(80, 302, 240, 0.95f);
  gfx_cor(CR(100, 22, 200, 20), 0.25f, 0, 0, 0, 0.55f); CBAR(110, 28, 180, 0.85f);
  CVID(372, 30, 240, 300, 22);
  for (i = 0; i < n; i++) {
    float by = 50 + i * (n == 5 ? 54 : 66);
    if (OP[i] == ajc.op) gfx_cor(CR(382, by - 6, 220, 56), 0.3f, 1, 1, 1, 0.12f);
    CICON(IC[i], 394, by + 4, 18, 0.85f);
    ajcTxt(rotuloOpcao(OP[i]), 11, 0, 420, by + 4, 176, 0.6f);
    ajcTxt(textoValor(OP[i]), 13, 1, 420, by + 22, 176, 1);
  }
  if (ajc.op == AJ_LEG_LINGUA2) ajcFoco(AJ_LEG_LINGUA2, 100, 22, 200, 20, 6);
  if (ajc.op == AJ_LEG_LINGUA) ajcFoco(AJ_LEG_LINGUA, 70, 296, 260, 20, 6);
}
// Segunda legenda: as duas legendas no player, a segunda no lugar, no
// tamanho, na cor, no fundo e na borda escolhidos.
static void ajcIdiLegenda2(int A) {
  static const float COR[6][3] = { { 1, 1, 1 }, { 1, 0.86f, 0.2f }, { 0.4f, 0.9f, 0.4f }, { 0.4f, 0.65f, 1 },
                                   { 1, 0.35f, 0.3f }, { 0.08f, 0.08f, 0.08f } };
  static const float FU[5] = { 0, 0.25f, 0.5f, 0.75f, 1.0f };
  static const float TAM[7] = { 1.0f, 0.6f, 0.8f, 1.0f, 1.2f, 1.4f, 1.6f };
  int c = ajustes_leg2_cor(), f = ajustes_leg2_fundo(), b = ajustes_leg2_borda(), junto = ajustes_leg2_junto();
  float k = TAM[valor[AJ_LEG2_TAMANHO] >= 0 && valor[AJ_LEG2_TAMANHO] < 7 ? valor[AJ_LEG2_TAMANHO] : 0];
  float bw = 220 * k, bh = 9 * k, by = junto ? 262 - bh - 18 : 26, bx = 320 - bw * 0.5f;
  const float *cc = c >= 0 && c < 6 ? COR[c] : COR[0];
  CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 360, 22, 1);
  gfx_veu_css(CR(0, 240, 640, 120), 0, 0, 1.0f, 0.55f);
  gfx_cor(CR(190, 286, 260, 22), 0.25f, 0, 0, 0, 0.5f); CBAR(200, 293, 240, 0.95f);
  gfx_cor(CR(bx - 10, by - 6, bw + 20, bh + 12), 0.25f, 0, 0, 0, f >= 0 && f < 5 ? FU[f] : 0.5f);
  if (b > 0) gfx_cor(CR(bx - 1.5f, by - 1.5f, bw + 3, bh + 3), 0.5f, 0, 0, 0, 0.9f);
  gfx_cor(CR(bx, by, bw, bh), 0.5f, cc[0], cc[1], cc[2], 0.95f);
  ajcFoco(AJ_LEG2_POS, bx - 10, by - 6, bw + 20, bh + 12, 6);
  ajcFoco(AJ_LEG2_TAMANHO, bx - 10, by - 6, bw + 20, bh + 12, 6);
  ajcFoco(AJ_LEG2_COR, bx - 10, by - 6, bw + 20, bh + 12, 6);
  ajcFoco(AJ_LEG2_FUNDO, bx - 10, by - 6, bw + 20, bh + 12, 6);
  ajcFoco(AJ_LEG2_BORDA, bx - 10, by - 6, bw + 20, bh + 12, 6);
  if (ajc.op >= 0) { float w2 = ajcTxt(textoValor(ajc.op), 12, 1, 0, 0, 240, 0) + 28;
    CVID(604 - w2, 312, w2, 30, 15); ajcTxt(textoValor(ajc.op), 12, 1, 618 - w2, 319, 240, 1); }
}

// --- TV AO VIVO -----------------------------------------------------------------------
// Provedor: o formulario do Xtream (ou do portal Stalker) e os canais.
static void ajcTvProvedor(int A) {
  static const int XT[4] = { AJ_XTREAM_SERVIDOR, AJ_XTREAM_USUARIO, AJ_XTREAM_SENHA, AJ_XTREAM_CONTA };
  static const int ST[2] = { AJ_STALKER_PORTAL, AJ_STALKER_MAC };
  static const char *const ICX[4] = { "aj_server", "aj_user-round", "aj_key-round", "aj_circle-check" };
  int stalker = ajc.op == AJ_STALKER_PORTAL || ajc.op == AJ_STALKER_MAC || ajc.op == AJ_STALKER_LIMPAR, i;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.66f);
  CVID(36, 28, 320, 304, 24);
  { float w1 = ajcTxt("Xtream", 12, 1, 0, 0, 0, 0) + 28, w2 = ajcTxt("Stalker", 12, 1, 0, 0, 0, 0) + 28;
    if (!stalker) CACC(52, 44, w1, 28, 0.9f); else gfx_cor(CR(52, 44, w1, 28), 0.5f, 1, 1, 1, 0.08f);
    if (stalker) CACC(60 + w1, 44, w2, 28, 0.9f); else gfx_cor(CR(60 + w1, 44, w2, 28), 0.5f, 1, 1, 1, 0.08f);
    ajcTxt("Xtream", 12, 1, 66, 50, 0, 1); ajcTxt("Stalker", 12, 1, 74 + w1, 50, 0, 1); }
  for (i = 0; i < (stalker ? 2 : 4); i++) {
    int o = stalker ? ST[i] : XT[i];
    ajcLinha(o, stalker ? (i ? "aj_key-round" : "aj_server") : ICX[i], 48, 88 + i * 44, 296, 38, 1);
  }
  ajcLinha(stalker ? AJ_STALKER_LIMPAR : AJ_XTREAM_LIMPAR, "aj_x", 48, stalker ? 176 : 264, 296, 38, 0.8f);
  ajcLinha(AJ_EPG_PAIS, "aj_calendar", 48, stalker ? 220 : 264 + 0, 296, 38, ajc.op == AJ_EPG_PAIS || stalker ? 1.0f : 0.0f);
  for (i = 0; i < 9; i++) {
    float bx = 376 + (i % 3) * 78, by = 40 + (i / 3) * 96;
    const CatItem *canal = cat_n() > 0 ? cat_item((i * 3) % cat_n()) : NULL;
    CVID(bx, by, 70, 84, 14);
    if (canal && !strcmp(canal->tipo, "channel") && canal->logo[0]) ajLogo(canal->logo, CX(bx + 10), CY(by + 26), 50 * ajc.s, 32 * ajc.s, 0.95f);
    else ajLogo(ajcLogoAmostra(i), CX(bx + 10), CY(by + 28), 50 * ajc.s, 28 * ajc.s, 0.95f);
  }
}
// Se o canal nao abre: o canal carregando e a lista de verificacao com o
// valor de cada ajuste.
static void ajcTvProblemas(int A) {
  static const int OP[6] = { AJ_LIVETV_DIAG, AJ_LIVETV_RES, AJ_LIVETV_ESPERA, AJ_LIVETV_FORMATO, AJ_LIVETV_MODO, AJ_LIVETV_PROXY };
  int i;
  CIMG(ajArte(AJA_FUNDO, A), 0, 0, 640, 360, 22, 1);
  gfx_cor(CR(0, 0, 640, 360), 22.0f / 360, 0.031f, 0.031f, 0.039f, 0.5f);
  CVID(28, 26, 92, 32, 16); CACC(42, 37, 10, 10, 1); CBAR(60, 38, 44, 0.85f);
  ajLogo(ajcLogoAmostra(A), CX(36), CY(76), 80 * ajc.s, 40 * ajc.s, 0.95f);
  for (i = 0; i < 12; i++) {
    float an = i * 6.2832f / 12, r2 = 26;
    gfx_cor(CR(170 + cosf(an) * r2 - 3, 190 + sinf(an) * r2 - 3, 6, 6), 0.5f, ajc.ar, ajc.ag, ajc.ab, 0.2f + 0.8f * i / 11.0f);
  }
  CVID(340, 26, 272, 308, 22);
  for (i = 0; i < 6; i++) ajcLinha(OP[i], i == 0 ? "aj_stethoscope" : "aj_circle-check", 350, 40 + i * 48, 252, 42, 1);
}

// --- CONTAS E SERVICOS ----------------------------------------------------------------
// Conta e perfis: o cartao do perfil ativo e a lista de addons de verdade.
static void ajcConPerfis(int A) {
  const ContaPerfil *p = perfis_item_ativo();
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.62f);
  CVID(28, 24, 270, 312, 24);
  gfx_cor(CR(48, 44, 72, 72), 0.5f, 0.24f, 0.25f, 0.29f, 1);
  if (p && p->avatarUrl[0]) CIMG(p->avatarUrl, 48, 44, 72, 72, 36, 1);
  else { char av[1200]; snprintf(av, sizeof av, "%s/icones/mt_perfil.png", uxDirArte); ajLogo(av, CX(64), CY(60), 40 * ajc.s, 40 * ajc.s, 0.9f); }
  ajcTxt(p && p->nome[0] ? p->nome : "Perfil", 17, 1, 134, 58, 150, 1);
  ajcTxt(textoValor(AJ_PERFIL_ATIVO), 11, 0, 134, 84, 150, 0.6f);
  ajcFoco(AJ_PERFIL_ATIVO, 48, 44, 72, 72, 36);
  ajcFoco(AJ_PERFIL_EDITAR, 48, 44, 72, 72, 36);
  ajcLinha(AJ_SYNC, "aj_cloud", 38, 140, 250, 40, 1);
  ajcLinha(AJ_PERFIL_PESQ, "aj_globe", 38, 186, 250, 40, 1);
  ajcLinha(AJ_PERFIL_EDITAR, "aj_user-round", 38, 232, 250, 40, 1);
  ajcLinha(AJ_SAIR, "aj_log-out", 38, 278, 250, 40, 0.8f);
  // A direita, os perfis da conta como na tela "Quem esta assistindo?" (os
  // addons sairam daqui para Fontes e addons). Sem perfis carregados: o ativo
  // e o lugar de um novo.
  CVID(314, 24, 298, 312, 24);
  ajcTxt("Quem está assistindo?", 13, 1, 336, 44, 260, 0.9f);
  { int np = perfis_n(), k;
    if (np > 4) np = 4;
    for (k = 0; k < (np > 0 ? np : 1) + (np < 4 ? 1 : 0); k++) {
      const ContaPerfil *q = np > 0 && k < np ? perfis_item(k) : (k == 0 ? p : NULL);
      int novo = np > 0 ? k >= np : k >= 1;
      float cx = 340 + (k % 2) * 136, cy = 86 + (k / 2) * 118;
      int ativo = q && q->indice == perfis_ativo();
      gfx_cor(CR(cx + 24, cy, 72, 72), 0.5f, 0.24f, 0.25f, 0.29f, novo ? 0.5f : 1.0f);
      if (novo) CICON("aj_plus", cx + 46, cy + 22, 28, 0.6f);
      else if (q && q->avatarUrl[0]) CIMG(q->avatarUrl, cx + 24, cy, 72, 72, 36, 1);
      else { char av[1200]; snprintf(av, sizeof av, "%s/icones/mt_perfil.png", uxDirArte); ajLogo(av, CX(cx + 40), CY(cy + 16), 40 * ajc.s, 40 * ajc.s, ativo || np == 0 ? 0.9f : 0.6f); }
      if (ativo || (np == 0 && k == 0)) gfx_anel_fora(CR(cx + 24, cy, 72, 72), 0.5f, 2.0f * ajc.s, 2.0f * ajc.s, 0.953f, 0.949f, 0.937f, 0.9f);
      { const char *nm = novo ? "Novo perfil" : q && q->nome[0] ? q->nome : "Perfil";
        TxtLinha l = txt_linha_corta(ajEstiloPxS(11 * ajc.s, ativo), nm, AJ_TX, 120 * ajc.s);
        txt_desenhar_alpha(l, CX(cx + 60) - l.w * 0.5f, CY(cy + 82), novo ? 0.5f : 0.85f); }
    } }
}
// Fontes e addons > Addons: a lista dos addons da conta (ligado = interruptor
// no acento) e, ao lado, plugins e os addons do perfil principal.
static void ajcFonAddons(int A) {
  static const char *const AMOSTRA[5] = { "Cinemeta", "Torrentio", "OpenSubtitles v3", "TMDB", "AIOStreams" };
  int i, n = addons_n();
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.62f);
  CVID(28, 24, 340, 312, 24);
  ajcLinha(AJ_ADDONS, "aj_puzzle", 38, 34, 320, 40, 1);
  for (i = 0; i < 5; i++) {
    float by = 88 + i * 46;
    int liga = i < n ? addons_ativo(i) : i != 3;
    const char *nm = i < n ? addons_nome(i) : AMOSTRA[i];
    gfx_cor(CR(50, by + 4, 28, 28), 0.5f, 1, 1, 1, 0.1f);
    CICON("aj_puzzle", 56, by + 10, 16, liga ? 0.85f : 0.4f);
    ajcTxt(nm, 12, 0, 92, by + 9, 190, liga ? 0.9f : 0.45f);
    gfx_cor(CR(300, by + 7, 40, 22), 0.5f, 1, 1, 1, 0.12f);
    if (liga) gfx_cor(CR(300, by + 7, 40, 22), 0.5f, ajc.ar, ajc.ag, ajc.ab, 0.85f);
    gfx_cor(CR(liga ? 321 : 303, by + 10, 16, 16), 0.5f, 1, 1, 1, liga ? 1.0f : 0.55f);
  }
  ajcFoco(AJ_ADDONS, 40, 82, 316, 236, 14);
  CVID(384, 24, 228, 312, 24);
  ajcLinha(AJ_PLUGINS, "aj_puzzle", 394, 34, 208, 40, 1);
  // "Usar os addons do perfil principal" e longo: rotulo inteiro em cima, valor embaixo.
  { int f = ajc.op == AJ_ADDONS_PRINCIPAL;
    float al = lig(AJ_ADDONS_PRINCIPAL) ? 1.0f : 0.6f;
    if (f) gfx_cor(CR(394, 80, 208, 52), 0.25f, 1, 1, 1, 0.12f);
    CICON("aj_link", 406, 88, 18, 0.8f * al);
    ajcTxt(rotuloOpcao(AJ_ADDONS_PRINCIPAL), 11, f, 432, 86, 166, (f ? 1.0f : 0.8f) * al);
    ajcTxt(textoValor(AJ_ADDONS_PRINCIPAL), 10, 0, 432, 106, 166, 0.55f * al);
    if (f) CACC(384, 103, 6, 6, 1); }
  for (i = 0; i < 3; i++) {
    float by = 152 + i * 60;
    CIMG(ajArte(AJA_POSTER, AJ_POS[(A + i) % 14]), 404, by, 34, 50, 6, 0.9f);
    ajcTxt(ajTituloN(AJ_POS[(A + i) % 14]), 12, 1, 450, by + 8, 150, 0.85f);
    ajcTxt(AMOSTRA[1 + i * 2 > 4 ? 4 : 1 + i * 2], 10, 0, 450, by + 28, 150, 0.5f);
  }
}
// Notificacoes: a enquete aberta na ilha do relogio.
static void ajcConAvisos(int A) {
  static const float P[3] = { 0.58f, 0.27f, 0.15f };
  float al = ON(AJ_ENQUETES);
  int i;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.3f);
  gfx_veu_css((GfxRect){ ajc.x, ajc.y, 640 * 0.55f * ajc.s, ajc.h }, 2, 0, 1.0f, 0.75f);
  if (ajLogo(ajArte(AJA_LOGO, A), CX(36), CY(220), 200 * ajc.s, 60 * ajc.s, 1) <= 0) ajcTxt(ajTituloN(A), 22, 1, 36, 240, 260, 1);
  CVIDA(292, 24, 320, 214, 26, al);
  CICON("aj_bell", 312, 42, 22, 0.9f * al);
  CBAR(346, 48, 180, 0.85f * al);
  for (i = 0; i < 3; i++) {
    float by = 86 + i * 46;
    gfx_cor(CR(312, by, 280, 36), 0.5f, 1, 1, 1, 0.08f * al);
    gfx_cor(CR(312, by, 280 * P[i], 36), 0.5f, ajc.ar, ajc.ag, ajc.ab, (i == 0 ? 0.55f : 0.25f) * al);
    CBAR(328, by + 14, 90 + i * 20, 0.8f * al);
  }
  ajcFoco(AJ_ENQUETES, 292, 24, 320, 214, 26);
}
// Discord, onde o + salva, de onde vem o Continuar: o status no Discord e os
// destinos de cada coisa.
static void ajcConSocial(int A) {
  char marca[1200];
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.64f);
  CVID(36, 30, 568, 120, 22);
  gfx_cor(CR(56, 50, 36, 36), 0.5f, 0.345f, 0.396f, 0.949f, 1);
  CICON("aj_message-square", 64, 58, 20, 0.95f);
  ajcTxt("Discord", 13, 1, 104, 56, 200, 1); ajcTxt(textoValor(AJ_DISCORD), 11, 0, 104, 76, 200, 0.6f);
  CIMG(ajArte(AJA_POSTER, A), 380, 44, 60, 90, 8, ajEstadoLigado(AJ_DISCORD) ? 1.0f : 0.5f);
  ajcTxt("Assistindo", 11, 0, 452, 56, 140, 0.6f); ajcTxt(ajTituloN(A), 13, 1, 452, 74, 140, 1);
  gfx_cor(CR(452, 104, 130, 4), 0.5f, 1, 1, 1, 0.2f); gfx_cor(CR(452, 104, 60, 4), 0.5f, 0.345f, 0.396f, 0.949f, 1);
  ajcFoco(AJ_DISCORD, 36, 30, 568, 120, 22);
  CVID(36, 168, 276, 164, 22);
  CICON("aj_bookmark", 56, 188, 26, 0.9f);
  ajcTxt(rotuloOpcao(AJ_SALVOS_DEST), 11, 0, 56, 230, 236, 0.6f);
  ajcTxt(textoValor(AJ_SALVOS_DEST), 15, 1, 56, 250, 236, 1);
  ajcFoco(AJ_SALVOS_DEST, 36, 168, 276, 164, 22);
  CVID(328, 168, 276, 164, 22);
  CICON("aj_rotate-ccw-clock", 348, 188, 26, 0.9f);
  snprintf(marca, sizeof marca, "%s/marcas/trakt_wordmark.png", uxDirArte);
  ajLogo(marca, CX(500), CY(190), 84 * ajc.s, 24 * ajc.s, 0.9f);
  ajcTxt(rotuloOpcao(AJ_CW_FONTE), 11, 0, 348, 230, 236, 0.6f);
  ajcTxt(textoValor(AJ_CW_FONTE), 15, 1, 348, 250, 236, 1);
  ajcFoco(AJ_CW_FONTE, 328, 168, 276, 164, 22);
}
// Debrid: o cofre, uma linha por servico com o estado da chave. As chaves do
// MDBList e do Fanart moram junto do que elas liberam (Notas, De onde vem a arte).
static void ajcConChaves(int A) {
  static const int OP[5] = { AJ_DEBRID_RD, AJ_DEBRID_TB, AJ_DEBRID_PM, AJ_DEBRID_AD, AJ_DEBRID_AD_TESTAR };
  int i;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.66f);
  CVID(36, 30, 568, 300, 24);
  for (i = 0; i < 5; i++) ajcLinha(OP[i], i == 4 ? "aj_shield-check" : "aj_key-round", 50, 44 + i * 56, 540, 46, 1);
}
// P2P: a TV, o servidor de streaming na rede e os pares.
static void ajcConP2p(int A) {
  float al = ON(AJ_P2P_LIGADO), sx = 430, sy = 160;
  int i, k;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.72f);
  for (i = 0; i < 6; i++) {
    float an = i * 6.2832f / 6 + 0.4f, px = sx + cosf(an) * 120, py = sy + sinf(an) * 110;
    for (k = 1; k < 10; k++) gfx_cor(CR(sx + (px - sx) * k / 10 - 1.5f, sy + (py - sy) * k / 10 - 1.5f, 3, 3), 0.5f, 1, 1, 1, 0.3f * al);
    CVIDA(px - 16, py - 16, 32, 32, 16, al); CICON("aj_users", px - 9, py - 9, 18, 0.7f * al);
  }
  for (k = 1; k < 16; k++) gfx_cor(CR(130 + (sx - 130) * k / 16 - 2, sy + 0 - 2, 4, 4), 0.5f, ajc.ar, ajc.ag, ajc.ab, al);
  CVID(70, sy - 50, 120, 100, 22); CICON("aj_tv-minimal-play", 110, sy - 20, 40, 0.9f);
  CVIDA(sx - 36, sy - 36, 72, 72, 36, al); CICON("aj_server", sx - 18, sy - 18, 36, 0.9f * al);
  ajcFoco(AJ_P2P_LIGADO, sx - 36, sy - 36, 72, 72, 36);
  { float w2 = ajcTxt(textoValor(AJ_P2P_URL), 12, 1, 0, 0, 300, 0) + 28;
    CVID(36, 300, w2, 30, 15); ajcTxt(textoValor(AJ_P2P_URL), 12, 1, 50, 307, 300, 0.9f);
    ajcFoco(AJ_P2P_URL, 36, 300, w2, 30, 15); }
  { float w2 = ajcTxt(rotuloOpcao(AJ_P2P_TESTAR), 12, 1, 0, 0, 220, 0) + 48;
    CVID(604 - w2, 300, w2, 30, 15); CICON("aj_zap", 614 - w2, 306, 18, 0.85f);
    ajcTxt(rotuloOpcao(AJ_P2P_TESTAR), 12, 1, 638 - w2, 307, 220, 0.9f);
    ajcFoco(AJ_P2P_TESTAR, 604 - w2, 300, w2, 30, 15); }
}
// Servidores pessoais: Jellyfin, Emby e Plex, cada um com o estado, e a
// biblioteca que entra no app.
static void ajcConServidores(int A) {
  static const char *const NM[3] = { "Jellyfin", "Emby", "Plex" };
  static const float COR[3][3] = { { 0.667f, 0.361f, 0.765f }, { 0.322f, 0.71f, 0.294f }, { 0.898f, 0.627f, 0.051f } };
  static const int ENT[3] = { AJ_JF_ENTRAR, AJ_EM_ENTRAR, AJ_PX_ENTRAR }, SRV[3] = { AJ_JF_SERVIDOR, AJ_EM_SERVIDOR, AJ_PX_SERVIDOR },
                   SAI[3] = { AJ_JF_SAIR, AJ_EM_SAIR, AJ_PX_SAIR };
  float al = ON(AJ_JF_LIGADO);
  int i;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.66f);
  for (i = 0; i < 3; i++) {
    float bx = 36 + i * 196;
    int f = ajc.op == ENT[i] || ajc.op == SRV[i] || ajc.op == SAI[i];
    CVIDA(bx, 30, 180, 170, 20, al);
    gfx_cor(CR(bx + 16, 46, 44, 44), 0.3f, COR[i][0], COR[i][1], COR[i][2], al);
    { TxtLinha l = txt_linha(ajEstiloPxS(20 * ajc.s, 1), NM[i] + 0, 255, 255, 255, 255);
      (void)l; ajcTxt(NM[i], 16, 1, bx + 70, 56, 104, al); }
    { char m[2] = { NM[i][0], 0 }; TxtLinha l = txt_linha(ajEstiloPxS(22 * ajc.s, 1), m, 255, 255, 255, 255);
      txt_desenhar_alpha(l, CX(bx + 38) - l.w * 0.5f, CY(68) - l.h * 0.5f, al); }
    ajcTxt(textoValor(SRV[i]), 11, 0, bx + 16, 106, 150, 0.6f * al);
    ajcTxt(textoValor(ENT[i]), 12, 1, bx + 16, 128, 150, 0.9f * al);
    CICON("aj_log-out", bx + 16, 160, 16, 0.5f * al);
    if (f) gfx_anel_fora(CR(bx, 30, 180, 170), 20.0f / 170, 3 * ajc.s, 2.5f * ajc.s, ajc.ar, ajc.ag, ajc.ab, 1);
  }
  ajcFoco(AJ_JF_LIGADO, 36, 30, 572, 170, 20);
  for (i = 0; i < 7; i++) CIMG(ajArte(AJA_POSTER, AJ_POS[(A + i) % 14]), 36 + i * 82, 222, 70, 105, 8, 0.85f * al);
}

// --- DESEMPENHO DESTA TV --------------------------------------------------------------
// Imagem da interface: a lupa no cartaz (decode cheio ou economico), os
// efeitos de vidro e a navegacao rapida.
static void ajcDesTela(int A) {
  int eco = valor[AJ_QUALIDADE_IMG] != 0, i;
  GfxRect lupa;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.7f);
  CIMG(ajArte(AJA_POSTER, A), 36, 30, 196, 294, 14, 1);
  lupa = CR(140, 150, 130, 130);
  gfx_rect((GfxRect){ lupa.x - 10 * ajc.s, lupa.y, lupa.w + 20 * ajc.s, lupa.h + 26 * ajc.s }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.6f);
  if (eco) ajcBorrada(ajArte(AJA_POSTER, A), lupa, 65 * ajc.s, 1); else ajImg(ajArte(AJA_POSTER, A), lupa, 65 * ajc.s, 1);
  gfx_anel(lupa, 0.5f, 4 * ajc.s, 0.953f, 0.949f, 0.937f, 0.95f);
  ajcFoco(AJ_QUALIDADE_IMG, 140, 150, 130, 130, 65);
  { float w2 = ajcTxt(textoValor(AJ_RESOLUCAO), 12, 1, 0, 0, 200, 0) + 48;
    CVID(36, 30, w2, 30, 15); CICON("aj_monitor-play", 46, 36, 18, 0.85f);
    ajcTxt(textoValor(AJ_RESOLUCAO), 12, 1, 72, 37, 200, 1); ajcFoco(AJ_RESOLUCAO, 36, 30, w2, 30, 15); }
  CVID(300, 30, 304, 130, 22);
  ajcBorrada(ajArte(AJA_FUNDO, A), CR(316, 46, 120, 98), 14 * ajc.s, 1);
  gfx_cor(CR(316, 46, 120, 98), 14.0f / 98, 0.055f, 0.059f, 0.071f, 0.5f);
  CICON("aj_layers", 362, 82, 26, 0.9f);
  ajcTxt(rotuloOpcao(AJ_GPU_EFEITOS), 12, 1, 450, 66, 140, 0.9f);
  ajcTxt(textoValor(AJ_GPU_EFEITOS), 11, 0, 450, 88, 140, 0.6f);
  ajcFoco(AJ_GPU_EFEITOS, 300, 30, 304, 130, 22);
  CVID(300, 176, 304, 148, 22);
  for (i = 0; i < 4; i++) CIMG(ajArte(AJA_POSTER, AJ_POS[(A + i + 2) % 14]), 318 + i * 70, 196, 60, 90, 8, i == 2 ? 1.0f : 0.6f);
  if (lig(AJ_NAV_RAPIDA)) { for (i = 0; i < 3; i++) gfx_cor(CR(330 + i * 12, 296 + i * 0, 90 - i * 20, 3), 0.5f, ajc.ar, ajc.ag, ajc.ab, 0.9f - i * 0.25f);
                            CICON("aj_zap", 570, 290, 18, 0.9f); }
  ajcFoco(AJ_NAV_RAPIDA, 300, 176, 304, 148, 22);
}
// Diagnostico: a Home com o medidor no canto, o teste de velocidade e o
// diagnostico.
static void ajcDesDiag(int A) {
  int i;
  ajPreviaHomeArte(ajc.x, ajc.y, 640 * ajc.s, A);
  gfx_cor(CR(0, 0, 640, 360), 22.0f / 360, 0.031f, 0.031f, 0.039f, 0.45f);
  if (valor[AJ_MEDIDOR] != 0) {
    CVID(20, 20, 150, 52, 16);
    ajcTxt("60 fps", 14, 1, 34, 28, 0, 1);
    for (i = 0; i < 10; i++) gfx_cor(CR(34 + i * 12, 60 - (i % 3) * 3 - 4, 8, (i % 3) * 3 + 4), 0.2f, ajc.ar, ajc.ag, ajc.ab, 0.8f);
  }
  ajcFoco(AJ_MEDIDOR, 20, 20, 150, 52, 16);
  CVID(36, 200, 250, 132, 22);
  CICON("aj_stethoscope", 56, 216, 24, 0.9f);
  ajcTxt(rotuloOpcao(AJ_DIAGNOSTICO), 12, 1, 92, 220, 180, 1);
  for (i = 0; i < 3; i++) { CICON("aj_check", 56, 258 + i * 22, 16, 0.85f); CBAR(82, 262 + i * 22, 120 - i * 20, 0.5f); }
  ajcFoco(AJ_DIAGNOSTICO, 36, 200, 250, 132, 22);
  CVID(354, 160, 250, 172, 22);
  for (i = 0; i < 25; i++) {
    float an = 3.1416f + i * 3.1416f / 24, cx = 479 + cosf(an) * 84, cy = 270 + sinf(an) * 84;
    gfx_cor(CR(cx - 4, cy - 4, 8, 8), 0.5f, i < 17 ? ajc.ar : 1, i < 17 ? ajc.ag : 1, i < 17 ? ajc.ab : 1, i < 17 ? 1.0f : 0.15f);
  }
  { TxtLinha l = txt_linha(ajEstiloPxS(30 * ajc.s, 1), "86", AJ_TX); txt_desenhar(l, CX(479) - l.w * 0.5f, CY(236)); }
  ajcTxt("Mbps", 11, 0, 464, 276, 0, 0.6f);
  ajcTxt(rotuloOpcao(AJ_VELOCIDADE), 11, 1, 374, 302, 210, 0.85f);
  ajcFoco(AJ_VELOCIDADE, 354, 160, 250, 172, 22);
}

// --- SOBRE E AJUDA --------------------------------------------------------------------
// Guia de uso: os capitulos de verdade e uma pagina aberta.
static void ajcSobGuia(int A) {
  int i, k;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.62f);
  CVID(36, 24, 250, 312, 24);
  for (i = 0; i < 6 && i < GUIA_NCAPS; i++) {
    float by = 40 + i * 48;
    if (i == 1) gfx_cor(CR(46, by - 6, 230, 40), 0.5f, 1, 1, 1, 0.12f);
    CICON(GUIA_CAPS[i].icone, 58, by + 4, 20, 0.85f);
    ajcTxt(GUIA_CAPS[i].titulo, 12, i == 1, 88, by + 6, 180, i == 1 ? 1.0f : 0.7f);
  }
  CVID(302, 24, 302, 312, 24);
  ajcTxt(GUIA_CAPS[1].titulo, 16, 1, 322, 44, 260, 1);
  CBAR(322, 80, 250, 0.5f); CBAR(322, 96, 220, 0.5f);
  for (k = 0; k < 3; k++) ajPreviaHomeArte(CX(322 + k * 90), CY(124), 82 * ajc.s, AJ_POS[(A + k) % 14]);
  for (k = 0; k < 3; k++) { CBAR(322, 196 + k * 22, 240 - k * 40, 0.4f); }
  CACC(322, 280, 120, 30, 1);
  ajcFoco(AJ_GUIA, 28, 16, 584, 328, 26);
}
// O app: o simbolo, a versao, a atualizacao baixando e o que ha de novo.
static void ajcSobApp(int A) {
  int i;
  ajCenaArte(ajArte(AJA_FUNDO, A), (GfxRect){ ajc.x, ajc.y, 640 * ajc.s, ajc.h }, 22, 0.66f);
  CVID(36, 36, 220, 288, 24);
  logoapp_miniatura(logoapp_atual(), CR(76, 60, 140, 140), 1, 1.0f);
  { TxtLinha l = txt_linha(ajEstiloPxS(16 * ajc.s, 1), NV_VERSAO, AJ_TX); txt_desenhar(l, CX(146) - l.w * 0.5f, CY(224)); }
  ajcFoco(AJ_VERSAO_I, 60, 216, 172, 32, 10);
  ajcTxt(textoValor(AJ_VERSAO_I), 11, 0, 60, 262, 180, 0.55f);
  CVID(272, 36, 332, 288, 24);
  CICON("aj_download", 292, 56, 24, 0.9f);
  ajcTxt(rotuloOpcao(AJ_ATUALIZAR), 13, 1, 326, 59, 260, 1);
  ajcTxt(textoValor(AJ_ATUALIZAR), 11, 0, 326, 80, 260, 0.6f);
  gfx_cor(CR(292, 108, 292, 8), 0.5f, 1, 1, 1, 0.12f); CACC(292, 108, 186, 8, 1);
  ajcFoco(AJ_ATUALIZAR, 286, 48, 304, 76, 12);
  for (i = 0; i < 4; i++) { CICON("aj_sparkles", 292, 146 + i * 42, 18, 0.7f); CBAR(320, 152 + i * 42, 220 - i * 30, 0.5f); }
}

static float ajSecaoDesenhar(int cena, int op, float x, float y, float w) {
  // Sem opcao (a cena no resumo da categoria): a arte da categoria (ajcSecA).
  int A = op >= 0 || ajcSecA < 0 ? ajCenaArteOp(op) : ajcSecA;
  ajc.x = x; ajc.y = y; ajc.s = w / 640.0f; ajc.h = w * 9.0f / 16.0f; ajc.op = op;
  ajustes_acento(&ajc.ar, &ajc.ag, &ajc.ab);
  gfx_recorte(x, y, w, ajc.h);
  switch (cena) {
    case AJC_HOME_ESTILO: ajcHomeEstilo(A); break;
    case AJC_HOME_FILEIRAS: ajcHomeFileiras(A); break;
    case AJC_HOME_BARRA: ajcHomeBarra(A); break;
    case AJC_HOME_PERFIL: ajcHomePerfil(A); break;
    case AJC_TIT_APARECE: ajcTitAparece(A); break;
    case AJC_TIT_NOTAS: ajcTitNotas(A); break;
    case AJC_TIT_DADOS: ajcTitDados(A); break;
    case AJC_TIT_BUSCA: ajcTitBusca(A); break;
    case AJC_CAR_FOCO: ajcCarFoco(A); break;
    case AJC_CAR_FORMA: ajcCarForma(A); break;
    case AJC_CAR_ORIGEM: ajcCarOrigem(A); break;
    case AJC_TRL_ONDE: ajcTrlOnde(A); break;
    case AJC_TRL_SOM: ajcTrlSom(A); break;
    case AJC_APA_VIDRO: ajcApaVidro(A); break;
    case AJC_APA_MOVIMENTO: ajcApaMovimento(A); break;
    case AJC_REP_FONTE: ajcRepFonte(A); break;
    case AJC_REP_IMAGEM: ajcRepImagem(A); break;
    case AJC_REP_DURANTE: ajcRepDurante(A); break;
    case AJC_REP_SEEKR: ajcRepSeekr(A); break;
    case AJC_IDI_INTERFACE: ajcIdiInterface(A); break;
    case AJC_IDI_FAIXAS: ajcIdiFaixas(A); break;
    case AJC_IDI_LEGENDA2: ajcIdiLegenda2(A); break;
    case AJC_TV_PROVEDOR: ajcTvProvedor(A); break;
    case AJC_TV_PROBLEMAS: ajcTvProblemas(A); break;
    case AJC_CON_PERFIS: ajcConPerfis(A); break;
    case AJC_CON_AVISOS: ajcConAvisos(A); break;
    case AJC_CON_SOCIAL: ajcConSocial(A); break;
    case AJC_CON_CHAVES: ajcConChaves(A); break;
    case AJC_CON_P2P: ajcConP2p(A); break;
    case AJC_CON_SERVIDORES: ajcConServidores(A); break;
    case AJC_DES_TELA: ajcDesTela(A); break;
    case AJC_DES_DIAG: ajcDesDiag(A); break;
    case AJC_SOB_GUIA: ajcSobGuia(A); break;
    case AJC_SOB_APP: ajcSobApp(A); break;
    case AJC_FON_ADDONS: ajcFonAddons(A); break;
    default: break;
  }
  gfx_sem_recorte();
  return ajc.h;
}
static float ajFontesResumo(int A, float x, float y, float w) {
  float h;
  ajcSecA = A; h = ajSecaoDesenhar(AJC_FON_ADDONS, -1, x, y, w); ajcSecA = -1;
  return h;
}
#undef CX
#undef CY
#undef CR
#undef CVID
#undef CVIDA
#undef CBAR
#undef CACC
#undef CICON
#undef CIMG
#undef ON
