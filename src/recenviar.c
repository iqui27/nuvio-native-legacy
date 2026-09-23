// Ver recenviar.h para por que estas telas sairam de dentro de ctxmenu.c.
//
// TRES TELAS NA MESMA MOLDURA, e nao tres modulos: "Para quem?", "O que
// dizer?" e "Amigos" tem a mesma caixa, o mesmo foco, o mesmo Voltar e a mesma
// lista rolavel. O que muda entre elas e a lista de linhas e o cabecalho.
//
// O TECLADO NAO MORA AQUI. Digitar o codigo abre teclado.c por cima desta
// modal, e esta modal so le o resultado — o mesmo contrato que o resto do app
// usa para camadas empilhadas.
#include "recenviar.h"
#include "recomenda.h"
#include "teclado.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "tex_cache.h"
#include "artehero.h"
#include <stdio.h>
#include <string.h>

// MAIS LARGA QUE O MENU DE CONTEXTO (720), e a diferenca tem uma medida por
// tras: as seis caixas do codigo de pareamento precisam de 646px para caber
// legiveis a tres metros, e dentro de 720 com 44 de recuo dos dois lados
// sobravam 632. Encolher a caixa do caractere para caber era desfazer a unica
// razao de a tela existir.
#define RE_W        860.0f
#define RE_PAD       48.0f
#define RE_LINHA     86.0f
#define RE_GAP       12.0f
#define RE_RODAPE    70.0f
#define RE_JANELA     5        // linhas visiveis antes de a lista rolar
#define RE_INTERNO  (RE_W - RE_PAD * 2.0f)

// Caixa de um caractere do codigo. Ver a mesma decisao em teclado.c: separado
// por caractere para poder ser DITADO ao telefone sem contar letra errada.
#define RE_COD_W     96.0f
#define RE_COD_H    116.0f
#define RE_COD_GAP   14.0f

// LOGO DO TITULO, nunca o poster: a arte horizontal identifica a obra sem
// repetir a capa que o dono ja viu na tela anterior. Entra no lugar tipografico
// do nome da obra; no passo da mensagem fica abaixo do nome do destinatario.
#define RE_LOGO_W   280.0f
#define RE_LOGO_H    64.0f
#define RE_LOGO_CAB 184.0f

enum { RE_PAG_CONTATOS = 0, RE_PAG_MODELOS, RE_PAG_AMIGOS };

static int   aberto, pagina, foco, topo;
static float anim;
static int   temItem;              // 0 quando abriu direto na tela de amigos
static int   voltaParaContatos;    // "Amigos" veio de dentro do envio
static CatItem item;               // COPIA; ver recenviar.h
static RecContato ctts[REC_CONTATOS_MAX];
static int   nCtts;
static char  alvoId[96], alvoNome[64];
static char  aviso[192];
static Uint32 fecharEm;
static int   confirmandoRemover = -1;   // indice do contato a remover, ou -1

static int teclaOk(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

static void recarregarContatos(void) {
  nCtts = recomenda_contatos(ctts, REC_CONTATOS_MAX);
}

int recenviar_aberto(void) { return aberto; }

static void abrirComum(void) {
  aberto = 1;
  foco = 0; topo = 0;
  alvoId[0] = 0; alvoNome[0] = 0;
  aviso[0] = 0; fecharEm = 0;
  confirmandoRemover = -1;
  recarregarContatos();
  // A lista ja esta no aparelho (recomenda.c a guarda do ultimo ciclo); o
  // pedido em paralelo e para o caso de um amigo ter entrado desde a ultima
  // sondagem de 60 s.
  recomenda_pedir_agora();
  recomenda_envio_limpar();
  recomenda_vinculo_limpar();
  recomenda_trakt_limpar();
}

int recenviar_abrir(const CatItem *ci) {
  if (!recomenda_ativo() || !ci || !ci->imdb[0]) return 0;
  item = *ci;
  temItem = 1;
  voltaParaContatos = 0;
  pagina = RE_PAG_CONTATOS;
  abrirComum();
  return 1;
}

int recenviar_abrir_amigos(void) {
  if (!recomenda_ativo()) return 0;
  memset(&item, 0, sizeof item);
  temItem = 0;
  voltaParaContatos = 0;
  pagina = RE_PAG_AMIGOS;
  abrirComum();
  return 1;
}

// --- LINHAS DE CADA PAGINA ---------------------------------------------------
//
// UMA FUNCAO PARA CONTAR E UMA PARA ROTULAR, e o desenho e o evento usam as
// duas. Enquanto isto estava em ctxmenu.c a contagem vivia num `switch` e o
// rotulo noutro, e a linha extra de "Adicionar um amigo" teria de ser
// lembrada nos dois.

// Na tela de contatos a ULTIMA linha e sempre "Adicionar um amigo" — inclusive
// (e principalmente) quando a lista esta vazia. Uma tela que so diz "nao ha
// ninguem" e uma porta fechada.
static int nLinhas(void) {
  if (pagina == RE_PAG_CONTATOS) return nCtts + 1;
  if (pagina == RE_PAG_MODELOS)  return REC_MODELOS;
  return 2 + nCtts;   // procurar no Trakt, digitar codigo, e cada contato
}

static const char *rotulo(int i) {
  static char buf[160];
  if (pagina == RE_PAG_CONTATOS)
    return (i >= 0 && i < nCtts) ? ctts[i].nome : "Adicionar um amigo";
  if (pagina == RE_PAG_MODELOS) {
    const char *m = recomenda_modelo(i);
    return m ? m : "";
  }
  if (i == 0) return "Procurar amigos do Trakt agora";
  if (i == 1) return "Digitar o código de um amigo";
  if (i - 2 >= nCtts) return "";
  if (confirmandoRemover == i - 2) {
    // DUAS CONFIRMACOES PARA APAGAR, e a segunda e a propria linha: um OK
    // distraido numa lista de nomes nao pode desfazer um vinculo que custou
    // um codigo ditado por telefone. O servidor apaga os DOIS lados.
    snprintf(buf, sizeof buf, i18n("Remover %s? OK confirma"), ctts[i - 2].nome);
    return buf;
  }
  return ctts[i - 2].nome;
}

static void ajustarJanela(void) {
  int n = nLinhas();
  if (foco < 0) foco = 0;
  if (foco >= n) foco = n > 0 ? n - 1 : 0;
  if (foco < topo) topo = foco;
  if (foco >= topo + RE_JANELA) topo = foco - RE_JANELA + 1;
  if (topo < 0) topo = 0;
}

static void irPara(int pag) {
  pagina = pag;
  foco = 0; topo = 0;
  confirmandoRemover = -1;
}

// --- EVENTOS -----------------------------------------------------------------

static void aplicar(void) {
  int n = nLinhas();
  if (n < 1) return;
  if (pagina == RE_PAG_CONTATOS) {
    if (foco >= nCtts) {
      voltaParaContatos = 1;
      irPara(RE_PAG_AMIGOS);
      return;
    }
    snprintf(alvoId,   sizeof alvoId,   "%s", ctts[foco].id);
    snprintf(alvoNome, sizeof alvoNome, "%s", ctts[foco].nome);
    irPara(RE_PAG_MODELOS);
    return;
  }
  if (pagina == RE_PAG_MODELOS) {
    // O INDICE DO MODELO E O QUE VIAJA, nao a frase: assim a mesma
    // recomendacao chega em portugues numa TV e em ingles na outra.
    if (recomenda_enviar(&item, alvoId, foco, "")) {
      snprintf(aviso, sizeof aviso, i18n("Enviando para %s..."), alvoNome);
    } else {
      snprintf(aviso, sizeof aviso, "%s",
               i18n("Não foi possível enviar. Tente novamente."));
      fecharEm = SDL_GetTicks() + 2200;
    }
    irPara(RE_PAG_CONTATOS);
    return;
  }
  // Amigos.
  if (foco == 0) {
    recomenda_procurar_trakt();
    return;
  }
  if (foco == 1) {
    teclado_abrir("Código do amigo",
                  "Seis letras ou números, como ele te passou.", 6);
    return;
  }
  { int c = foco - 2;
    if (c < 0 || c >= nCtts) return;
    if (confirmandoRemover != c) { confirmandoRemover = c; return; }
    recomenda_remover_contato(ctts[c].id);
    confirmandoRemover = -1;
    recarregarContatos();
    ajustarJanela(); }
}

void recenviar_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return;
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  // Repeticao automatica NUNCA e uma segunda escolha: quem quer clicar duas
  // vezes solta e aperta de novo. Sem isto, o OK ainda afundado que veio do
  // menu de contexto escolheria o primeiro amigo da lista sozinho.
  if (e->key.repeat && teclaOk(k)) return;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || k == SDLK_LEFT ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    // VOLTA UM PASSO, e nao fecha a modal: quem errou o amigo quer trocar de
    // amigo, nao recomecar do cartaz.
    if (confirmandoRemover >= 0) { confirmandoRemover = -1; return; }
    if (pagina == RE_PAG_MODELOS)  { irPara(RE_PAG_CONTATOS); return; }
    if (pagina == RE_PAG_AMIGOS && voltaParaContatos) {
      voltaParaContatos = 0;
      irPara(RE_PAG_CONTATOS);
      return;
    }
    aberto = 0;
    return;
  }
  if (k == SDLK_UP)   { foco--; confirmandoRemover = -1; ajustarJanela(); return; }
  if (k == SDLK_DOWN) { foco++; confirmandoRemover = -1; ajustarJanela(); return; }
  if (teclaOk(k)) { aplicar(); return; }
}

void recenviar_atualizar(float dt, Uint32 agora) {
  int est;
  if (!aberto && anim < 0.002f) { anim = 0.0f; return; }
  anim = ajustes_animacoes_reduzidas()
           ? (aberto ? 1.0f : 0.0f)
           : anim_mola(anim, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  teclado_atualizar(dt, agora);
  if (!aberto) return;

  // A LISTA E RELIDA POR QUADRO, e nao so na abertura. O fio de recomenda.c
  // sonda a cada 60 s e um amigo pode entrar com esta tela aberta — e, mais
  // comum, entra por causa do "procurar no Trakt" ou do codigo que acabou de
  // ser digitado. Custa um mutex e um memcpy de ate 40 registros, so enquanto
  // a modal esta em pe.
  recarregarContatos();

  // O CODIGO DIGITADO VIRA UM PEDIDO SO QUANDO O TECLADO FECHA, e nao a cada
  // tecla: teclado_resultado() e consumido na leitura, entao este bloco roda
  // uma vez por confirmacao.
  { int r = teclado_resultado();
    if (r == TECLADO_PRONTO && !recomenda_vincular(teclado_texto()))
      snprintf(aviso, sizeof aviso, "%s",
               i18n("O código tem 6 letras ou números.")); }

  // O RESULTADO VEM DO FIO de recomenda.c, nao daqui: esta tela so le o estado
  // e o transforma na frase curta do rodape.
  est = recomenda_vinculo_estado();
  if (est == REC_VINC_INDO) {
    snprintf(aviso, sizeof aviso, "%s", i18n("Procurando esse código..."));
  } else if (est == REC_VINC_OK) {
    const char *nome = recomenda_vinculo_nome();
    snprintf(aviso, sizeof aviso, i18n("%s agora é seu amigo"),
             nome[0] ? nome : i18n("Seu amigo"));
    recomenda_vinculo_limpar();
    recarregarContatos();
  } else if (est == REC_VINC_NAO_ACHOU) {
    snprintf(aviso, sizeof aviso, "%s",
             i18n("Ninguém tem esse código. Confira as 6 letras."));
    recomenda_vinculo_limpar();
  } else if (est == REC_VINC_EU_MESMO) {
    snprintf(aviso, sizeof aviso, "%s", i18n("Esse código é o seu."));
    recomenda_vinculo_limpar();
  } else if (est == REC_VINC_FALHA) {
    snprintf(aviso, sizeof aviso, "%s",
             i18n("Não foi possível adicionar. Tente novamente."));
    recomenda_vinculo_limpar();
  }

  est = recomenda_trakt_estado();
  if (est == REC_TRAKT_INDO) {
    snprintf(aviso, sizeof aviso, "%s", i18n("Procurando no Trakt..."));
  } else if (est == REC_TRAKT_PRONTO) {
    int n = recomenda_trakt_achados();
    if (n > 0) snprintf(aviso, sizeof aviso,
                        i18n("%d amigo(s) do Trakt entraram na lista"), n);
    // A FRASE HONESTA, e nao "nada encontrado": o motivo de zero nao e uma
    // falha de busca, e que ninguem que ele segue instalou o app ainda — e e
    // isso que ele precisa saber para ir atras do codigo.
    else snprintf(aviso, sizeof aviso, "%s",
                  i18n("Ninguém que você segue no Trakt usa o Nuvio ainda."));
    recomenda_trakt_limpar();
    recarregarContatos();
  } else if (est == REC_TRAKT_SEM_CONTA) {
    snprintf(aviso, sizeof aviso, "%s",
             i18n("Sem conta do Trakt neste aparelho."));
    recomenda_trakt_limpar();
  }

  est = recomenda_envio_estado();
  if (alvoNome[0] && !fecharEm) {
    if (est == REC_ENVIO_OK) {
      snprintf(aviso, sizeof aviso, i18n("Enviado para %s"), alvoNome);
      fecharEm = agora + 1400;
      recomenda_envio_limpar();
    } else if (est == REC_ENVIO_FALHA) {
      snprintf(aviso, sizeof aviso, "%s",
               i18n("Não foi possível enviar. Tente novamente."));
      fecharEm = agora + 2400;
      recomenda_envio_limpar();
    }
  }
  if (fecharEm && (Sint32)(agora - fecharEm) >= 0) { aberto = 0; fecharEm = 0; }
  ajustarJanela();
}

// --- DESENHO -----------------------------------------------------------------

// Logo normalizado do catalogo. SVG sem rasterizador e tratado como ausencia;
// neste caso o titulo tipografico toma o lugar da arte, sem reservar um buraco.
static const char *urlLogoTitulo(void) {
  static char url[512];
  const char *u;
  size_t n;
  if (!temItem || !item.logo[0] || pagina == RE_PAG_AMIGOS) return NULL;
  u = artehero_url_logo_larg(item.logo, RE_LOGO_W);
  if (!u || !u[0]) return NULL;
  n = strlen(u);
  if (n >= sizeof url) return NULL;
  memcpy(url, u, n + 1);
  return url;
}

static void desenhaLogo(const char *url, GLuint tex, float x, float y,
                        const char *fallback, float a) {
  float asp = tex ? tex_aspecto(url) : 0.0f;
  if (!tex || asp <= 0.01f) {
    TxtLinha t = txt_linha_corta(TXT_HEADLINE, fallback, 245, 248, 255, 255,
                                 RE_LOGO_W);
    txt_desenhar_alpha(t, x, y + (RE_LOGO_H - t.h) * 0.5f, a);
    return;
  }
  { float w = RE_LOGO_W, h = w / asp;
    GfxRect r;
    GfxModo modo;
    if (h > RE_LOGO_H) { h = RE_LOGO_H; w = h * asp; }
    r = (GfxRect){ x + (RE_LOGO_W - w) * 0.5f,
                   y + (RE_LOGO_H - h) * 0.5f, w, h };
    modo = tex_marca_escura(url) ? GFX_MARCA : GFX_TEXTO;
    gfx_rect(r, tex, modo, 0, 0, 0, 0, 0.96f, 0.97f, 0.99f, a);
  }
}

// As seis caixas do codigo. Centradas em `larg` a partir de `x`.
static float desenhaCodigo(float x, float y, float larg, float a) {
  const char *cod = recomenda_meu_codigo();
  int i, n = 6;
  float bw = RE_COD_W;
  // ALINHADO A ESQUERDA, como todo o resto do cartao. Centrado dentro da
  // largura util, o bloco de seis caixas nascia ~60px a direita do paragrafo
  // que o explica, e na captura os dois brigavam por uma margem so.
  if (bw * (float)n + RE_COD_GAP * (float)(n - 1) > larg)
    bw = (larg - RE_COD_GAP * (float)(n - 1)) / (float)n;
  for (i = 0; i < n; i++) {
    GfxRect b = { x, y, bw, RE_COD_H };
    char ch[2];
    // O RAIO DO gfx_cor E FRACAO DA ALTURA, nao pixel. Um "14" aqui viraria
    // uma pilula de 116px de alto — e foi assim que a primeira versao deste
    // recurso saiu na foto.
    gfx_cor(b, 14.0f / RE_COD_H, 1.0f, 1.0f, 1.0f, 0.10f * a);
    if ((int)strlen(cod) == n) {
      TxtLinha t;
      ch[0] = cod[i]; ch[1] = 0;
      t = txt_linha(TXT_TITULO2, ch, 246, 248, 255, 255);
      txt_desenhar_alpha(t, b.x + (b.w - t.w) * 0.5f,
                         b.y + (b.h - t.h) * 0.5f, a);
    }
    x += bw + RE_COD_GAP;
  }
  return RE_COD_H;
}

// Foco como o cartao de episodios: lavagem escura, texto claro e ponto de acento.
// A cor do fundo nao depende do tema, entao o texto nao pisca entre texturas.
// O contato da linha `i` desta pagina, ou NULL quando a linha e uma acao.
static const RecContato *contatoDaLinha(int i) {
  if (pagina == RE_PAG_CONTATOS) return (i >= 0 && i < nCtts) ? &ctts[i] : NULL;
  if (pagina == RE_PAG_AMIGOS)   return (i >= 2 && i - 2 < nCtts) ? &ctts[i - 2] : NULL;
  return NULL;
}

// Cabecalho "Seus amigos" entre as duas acoes e a lista, na tela de amigos
// (dono, 20/09/2026: "separar com o titulo que sao amigos ja adicionados").
#define RE_SECAO 44.0f
static float secaoAntes(int i) {
  return (pagina == RE_PAG_AMIGOS && i == 2 && nCtts > 0) ? RE_SECAO : 0.0f;
}

static void desenhaLinha(float x, float y, const char *rot, int focada,
                         float a, const RecContato *c) {
  GfxRect r = { x, y, RE_INTERNO, RE_LINHA };
  float ar, ag, ab, tx = r.x + 44.0f;
  int cor = focada ? 245 : 220;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cartao_foco_vidro(r, 14.0f / RE_LINHA, focada ? 1.0f : 0.0f,
                        a, ar, ag, ab);
  // FOTO DO AMIGO (ou a inicial), como na aba Social: a linha so com o nome
  // nao dizia quem era.
  if (c) {
    float d = RE_LINHA - 24.0f;
    GfxRect av = { r.x + 22.0f, y + 12.0f, d, d };
    rec_avatar(av, c->avatar, c->nome, c->id, a);
    tx = av.x + d + 18.0f;
  }
  { TxtLinha t = txt_linha_corta(TXT_PLR_CORPO, rot, cor, cor, cor, 255,
                                 r.x + r.w - 24.0f - tx);
    txt_desenhar_alpha(t, tx, y + (RE_LINHA - t.h) * 0.5f, a); }
}

void recenviar_desenhar(Uint32 agora) {
  float a = anim_suave(anim), alt, x, y, cab, cabTopo, hx, hy, hw;
  int i, n, vis;
  GLuint logoTex;
  const char *titulo, *chapeu, *pergunta, *rodape, *logo;
  (void)agora;
  if (anim < 0.01f) return;

  n = nLinhas();
  vis = n < RE_JANELA ? n : RE_JANELA;
  if (vis < 1) vis = 1;
  logo = urlLogoTitulo();
  logoTex = logo ? tex_obter_larg(logo, RE_LOGO_W) : 0;

  // ALTURA DO CABECALHO, por pagina. A de amigos carrega as seis caixas do
  // codigo e duas linhas de explicacao; as outras duas carregam so o titulo e
  // a pergunta. Uma altura unica deixaria um buraco de 200px nas duas.
  //
  // O logo acompanha os dois passos do envio, preservando a mesma moldura.
  cabTopo = logo ? RE_LOGO_CAB : 120.0f;
  if (pagina == RE_PAG_AMIGOS)          cab = 150.0f + RE_COD_H + 112.0f;
  else if (pagina == RE_PAG_CONTATOS && nCtts == 0) cab = cabTopo + 116.0f;
  else                                  cab = cabTopo;

  alt = RE_PAD * 2.0f + cab + (float)vis * (RE_LINHA + RE_GAP) - RE_GAP
        + RE_RODAPE;
  { int k; for (k = topo; k < n && k - topo < RE_JANELA; k++) alt += secaoAntes(k); }
  x = (NV_TELA_W - RE_W) * 0.5f;
  y = (NV_TELA_H - alt) * 0.5f;
  y += (1.0f - a) * 40.0f;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * anim);
  { GfxRect p = { x, y, RE_W, alt };
    gfx_cor(p, 24.0f / alt, 0.11f, 0.11f, 0.13f, 0.98f * a); }

  if (pagina == RE_PAG_AMIGOS) {
    chapeu = "AMIGOS";
    titulo = temItem || voltaParaContatos ? "Adicionar um amigo" : "Seus amigos";
    pergunta = "Seu código";
  } else if (pagina == RE_PAG_CONTATOS) {
    chapeu = "RECOMENDAR A UM AMIGO";
    titulo = item.titulo;
    pergunta = "Para quem?";
  } else {
    chapeu = "RECOMENDAR A UM AMIGO";
    titulo = alvoNome;
    pergunta = "O que dizer?";
  }

  // O LOGO ocupa o lugar do titulo, nao uma coluna de cartaz. Na tela de
  // mensagem ele vem depois do nome do destinatario para manter a hierarquia.
  hx = x + RE_PAD; hw = RE_INTERNO;
  hy = y + RE_PAD;
  if (!logo) {
    // O CHAPEU SO SOBREVIVE ONDE NAO HA LOGO, e a razao e que ele e a METADE
    // REPETIDA do cabecalho. "RECOMENDAR A UM AMIGO" e "Para quem?" dizem a
    // mesma coisa — a um amigo, para quem — e duas linhas dizendo a mesma coisa
    // em volta do titulo sao o que faz um cartao parecer formulario.
    //
    // DAS DUAS, A PERGUNTA E A QUE FICA: ela MUDA entre os dois passos ("Para
    // quem?" e "O que dizer?"), entao e ela que diz onde a pessoa esta no
    // gesto; o chapeu e igual nos dois e ja foi dito pelo item de menu que
    // abriu isto, um toque atras. Na tela de AMIGOS ele continua, porque la nao
    // ha logo e "AMIGOS" e a unica linha que nomeia a tela.
    TxtLinha t = txt_linha(TXT_CAPTION2, chapeu, 174, 178, 188, 255);
    txt_desenhar_alpha(t, hx, hy, a * 0.95f);
    hy += 28.0f;
  }

  if (logo && pagina == RE_PAG_CONTATOS) {
    desenhaLogo(logo, logoTex, hx, hy, item.titulo, a);
    hy += RE_LOGO_H + 8.0f;
    if (item.meta[0]) {
      TxtLinha t = txt_linha_corta(TXT_DET_META2, item.meta, 190, 192, 200,
                                   255, hw);
      txt_desenhar_alpha(t, hx, hy, a * 0.95f);
      hy += t.h + 8.0f;
    }
  } else {
    // No passo do amigo o nome dele continua primeiro; a marca do filme ocupa
    // a linha seguinte. Sem logo, o titulo escrito e o fallback.
    hy += txt_bloco(TXT_HEADLINE, titulo, 245, 248, 255, hx, hy, hw, 46.0f,
                    a, 2) + 6.0f;
    if (logo) {
      desenhaLogo(logo, logoTex, hx, hy, item.titulo, a);
      hy += RE_LOGO_H + 6.0f;
    }
    { const char *sub = pagina == RE_PAG_CONTATOS ? item.meta
                         : logo ? item.meta : item.titulo;
      if (sub[0]) {
        TxtLinha t = txt_linha_corta(TXT_DET_META2, sub, 190, 192, 200, 255,
                                     hw);
        txt_desenhar_alpha(t, hx, hy, a * 0.95f);
        hy += t.h + 8.0f;
      } }
    // A PERGUNTA DESCE PARA JUNTO DA LISTA, e nao segue o fluxo do titulo. Ela
    // e o ROTULO DA LISTA, nao a terceira linha da identificacao da obra: com
    // ela colada em "1994 · 2h22" as duas viravam um paragrafo cinza de duas
    // linhas e a pergunta perdia o que ela rotula. Ancorada a 50px do fim do
    // cabecalho ela fica a ~22px da primeira linha, ou seja, mais perto do que
    // ela pergunta do que do que ela nao pergunta.
    //
    // E um PISO, nao uma posicao: com titulo de duas linhas mais a meta o fluxo
    // ja chega em 182 e passa a mandar. Sem o max o texto se sobreporia.
    //
    // NO ESTADO VAZIO ELA NAO DESCE. Ali quem ocupa o fim do cabecalho e o
    // bloco "Nenhum amigo ainda", e a pergunta empurrada para baixo encostava
    // nele a 8px — as duas viravam um paragrafo so na captura. Sem a ancora a
    // pergunta fica junto do titulo e sobram 55px ate o bloco, que e a mesma
    // respiracao das outras telas.
    if (!(pagina == RE_PAG_CONTATOS && nCtts == 0)) {
      float ancora = y + RE_PAD + cabTopo - 50.0f;
      if (hy < ancora) hy = ancora;
    }
  }
  { TxtLinha t = txt_linha(TXT_DET_META2, pergunta, 150, 154, 163, 255);
    txt_desenhar_alpha(t, hx, hy, a * 0.9f); }

  if (pagina == RE_PAG_AMIGOS) {
    float cy = y + RE_PAD + 108.0f;
    cy += desenhaCodigo(x + RE_PAD, cy, RE_INTERNO, a) + 16.0f;
    // EM BLOCO pelo mesmo motivo do estado vazio: em ingles a frase e mais
    // longa que em portugues, e uma linha cortada perde o fim.
    cy += txt_bloco(TXT_CAPTION,
        recomenda_meu_codigo()[0]
          ? "Dite este código ao seu amigo. Ele digita aqui e vocês dois viram contatos."
          // SEM CODIGO AINDA NAO E ERRO: /v1/eu ainda nao respondeu, e isso e
          // normal nos primeiros segundos do arranque ou com a TV sem rede.
          : "Seu código aparece assim que a TV falar com o serviço.",
        200, 204, 214, x + RE_PAD, cy, RE_INTERNO, 30.0f, a * 0.95f, 2) + 6.0f;
    // A LINHA DO TRAKT SEGUE O BLOCO, e nao um deslocamento fixo: em ingles a
    // frase de cima passa para duas linhas e as duas se sobreporiam.
    { TxtLinha t = txt_linha_corta(TXT_CAPTION2,
          "Amigos do Trakt entram sozinhos quando instalarem o app.",
          160, 164, 175, 255, RE_INTERNO);
      txt_desenhar_alpha(t, x + RE_PAD, cy, a * 0.88f); }
  } else if (pagina == RE_PAG_CONTATOS && nCtts == 0) {
    // O ESTADO VAZIO DIZ POR QUE, e nao so "vazio". A lista esta vazia porque
    // o vinculo do Trakt so alcanca quem JA usa o servico — e a frase antiga
    // ("Amigos do Trakt entram sozinhos") dizia a metade que faz parecer que o
    // aplicativo esta quebrado, ja que eles nao entraram.
    // EM BLOCO, e nao numa linha cortada: a frase tem duas oracoes e a primeira
    // versao saiu na captura terminando em "Para os..." — ou seja, a metade
    // que diz O QUE FAZER era justamente a que nao cabia.
    // ABAIXO DO CABECALHO E EM LARGURA CHEIA, e nao numa coluna: sao duas
    // oracoes que ja usavam os 764px inteiros, e espremidas em 634 ao lado do
    // espremidas numa coluna virariam tres linhas — uma a mais do que aceita, ou
    // seja, o fim da frase (o que FAZER) sumiria de novo. Os deslocamentos
    // seguem `cabTopo` em vez dos 120 cravados de antes, senao o bloco
    // atravessaria o cabecalho.
    TxtLinha t1 = txt_linha(TXT_CALLOUT, "Nenhum amigo ainda",
                            240, 242, 248, 255);
    txt_desenhar_alpha(t1, x + RE_PAD, y + RE_PAD + cabTopo - 14.0f, a * 0.96f);
    txt_bloco(TXT_CAPTION,
        "Amigos do Trakt entram sozinhos, mas só depois de instalarem o app. Para os outros, troquem o código de 6 letras.",
        190, 194, 204, x + RE_PAD, y + RE_PAD + cabTopo + 26.0f, RE_INTERNO,
        30.0f, a * 0.9f, 2);
  }

  { float extra = 0.0f;
    for (i = topo; i < n && i - topo < RE_JANELA; i++) {
      float by;
      if (secaoAntes(i) > 0.0f) {
        TxtLinha t = txt_linha(TXT_CAPTION2, "Seus amigos", 150, 154, 165, 255);
        extra += secaoAntes(i);
        by = y + RE_PAD + cab + (float)(i - topo) * (RE_LINHA + RE_GAP) + extra;
        txt_desenhar_alpha(t, x + RE_PAD, by - t.h - 10.0f, a * 0.9f);
      } else
        by = y + RE_PAD + cab + (float)(i - topo) * (RE_LINHA + RE_GAP) + extra;
      desenhaLinha(x + RE_PAD, by, rotulo(i), i == foco, a, contatoDaLinha(i));
    } }

  if (aviso[0]) {
    TxtLinha t = txt_linha_corta(TXT_CAPTION, aviso, 214, 218, 228, 255,
                                 RE_INTERNO);
    txt_desenhar_alpha(t, x + RE_PAD, y + alt - RE_PAD - RE_RODAPE + 4.0f,
                       a * 0.95f);
  }
  rodape = pagina == RE_PAG_CONTATOS && !temItem
             ? "↑ ↓ Navegar   OK Selecionar   Voltar Fechar"
             : "↑ ↓ Navegar   OK Selecionar   Voltar Anterior";
  { TxtLinha t = txt_linha(TXT_CAPTION2, rodape, 155, 159, 169, 255);
    txt_desenhar_alpha(t, x + RE_PAD, y + alt - RE_PAD - t.h, a * 0.86f); }

  // O teclado fica POR CIMA desta modal, e nao no lugar dela: ele e uma
  // pergunta curta sobre a tela que continua valendo atras.
  teclado_desenhar(agora);
}
