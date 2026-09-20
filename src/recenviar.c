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

// O CARTAZ DA OBRA, no cabecalho das duas telas do envio.
//
// POR QUE ELE EXISTE AQUI, e nao e enfeite: esta tela e um PASSO DE
// CONFIRMACAO. A pessoa ja escolheu a obra; o que o cartao tem de responder e
// "estou mandando a coisa certa, e para quem". Ate agora o titulo respondia
// isso sozinho, em texto — e texto se LE, enquanto o cartaz se RECONHECE, e ele
// e o unico desenho desta tela que a pessoa ACABOU de ver na fileira de onde
// veio. Na tela "O que dizer?" o ganho e maior ainda: la o cabecalho troca o
// titulo pelo NOME DO AMIGO, entao no exato passo em que o OK envia nao havia
// mais nada na tela dizendo QUAL obra vai.
//
// 102 E MEDIDO, nao gosto. tex_obter_larg arredonda o teto de decode para
// ceil32(larg * escala * 1,25) com piso de 128 (capDeLargura, tex_cache.c):
// toda largura ate 102 cai no MESMO teto de 128 e custa o mesmo, e 103 salta
// para 160. 102 e portanto o maior cartaz que este cartao desenha sem subir de
// faixa. A faixa de cima custaria ~200 KB por 25% mais de largura, e este
// cartaz nao e o assunto da tela — e a confirmacao dele.
//
// O PRECO, MEDIDO e nao estimado (tests/social_shot.c, medirCartaz, com
// tex_estatisticas antes e depois):
//   QUENTE — a arte JA pedida pela fileira da home: 0 itens, 0 bytes. O cache e
//     indexado por CAMINHO e so re-decodifica quando o teto pedido e MAIOR que
//     o que ja esta la (nota de PROMOCAO em tex_cache.c), entao pedir 102 sobre
//     uma textura de teto 320 reaproveita a que ja esta na GPU. E o caso comum:
//     a pessoa acabou de vir da fileira onde esse cartaz estava desenhado.
//   FRIO — arte que saiu do cache: 1 item, 131 076 bytes (128 KB). Nao sao os
//     98 KB de 128x192x4: bytesTextura cobra tambem a piramide de mipmap, que
//     sao 32 KB a mais.
#define RE_ARTE_W    102.0f
#define RE_ARTE_H    153.0f          // 2:3, a forma do poster do TMDB (288x432)
#define RE_ARTE_GAP   28.0f
// Altura do cabecalho quando ha cartaz. A folga de 32 sai do PIOR CASO, e nao
// de arredondamento: titulo de DUAS linhas mais a linha de meta empurra a
// pergunta para 183 e ela termina perto de 211 abaixo do topo do cartao. Com
// estes 32 a primeira linha da lista comeca em 233 — 22px de respiro, o mesmo
// que as telas sem cartaz tem hoje. CONFERIDO na captura de titulo longo.
#define RE_ARTE_CAB  (RE_ARTE_H + 32.0f)

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

// O cartaz so entra onde ele RESPONDE alguma coisa: nas duas telas do envio.
// Na tela de amigos ele seria enfeite — ela fala de codigo de pareamento e de
// contato, nao da obra — e quando ela abre pela aba Social nao ha obra nenhuma
// (temItem 0). Sem URL de arte o espaco tambem nao e reservado: um retangulo
// cinza permanente e pior que o cabecalho antigo.
static int temArte(void) {
  return temItem && item.poster[0] && pagina != RE_PAG_AMIGOS;
}

// O cartaz, ENCAIXADO NA PROPORCAO dentro de uma caixa de altura FIXA.
//
// GFX_ARTE NAO TEM "COVER" — ver a tabela PRECISA em gfx.c, linha do modo 25:
// ele mapeia a textura direto no quad, entao um quad 2:3 com arte 16:9 dentro
// ESTICA a imagem. E nao e caso hipotetico: `poster` nem sempre e um cartaz
// (canal de addon e obra vinda de lista alheia chegam com arte deitada nesse
// campo), e a propria captura deste recurso vinha semeando um backdrop 16:9 ali
// — era o teste validando com confianca total um desenho que a TV nunca faz.
//
// Por isso a arte e ENCAIXADA (cabe inteira, sem corte e sem deformacao) e
// ancorada no TOPO. A CAIXA nao muda de altura, so a arte dentro dela: assim a
// lista nao pula quando a arte chega nem quando ela tem outra forma. Ancorar no
// topo, e nao centrar, mantem a borda de cima do cartaz na MESMA linha do
// chapeu ao lado seja qual for a proporcao.
//
// GFX_CARD FOI REJEITADO, e e o que recomenda.c e salvospainel.c usam nos
// cartazes deles: com `foco` 0 ele escurece a arte em 20% e ainda recorta 3% de
// cada borda para o parallax. Num cartaz de 102px esses 3% comem o nome
// impresso na arte, e o escurecimento tira justamente o que faz um cartaz ser
// reconhecido a tres metros. Aqui a arte tem de aparecer como ela e.
//
// O QUE SE PERDE COM ISSO, e e uma troca consciente: sem o escurecimento e sem
// borda, um cartaz de moldura PRETA se funde com o painel (0,11) e perde a
// silhueta. Um anel resolveria, e um anel esta fora do vocabulario desta base
// ("nao use contorno onde da para preencher") alem de custar mais um quad. A
// arte de um cartaz real quase nunca e preta ate a borda; a silhueta e o premio
// menor, e a arte intacta e o premio maior.
static void desenhaArte(float x, float y, float a) {
  GLuint tex = tex_obter_larg(item.poster, RE_ARTE_W);
  float asp = tex ? tex_aspecto(item.poster) : 0.0f;
  GfxRect r = { 0, 0, RE_ARTE_W, RE_ARTE_H };
  float raio;
  if (!tex || asp <= 0.01f) {
    // ARTE QUE AINDA NAO CHEGOU E ESQUELETO, na caixa CHEIA: sem proporcao
    // conhecida nao ha o que encaixar, e 2:3 e a aposta certa porque todo
    // `poster` do TMDB e 2:3. Esqueleto e nao buraco pelo motivo escrito no
    // DESIGN.md — "carregando" e "nao existe" sao dois estados, e um vazio
    // preto no lugar da arte le como defeito, nao como espera.
    //
    // NV_COR_ESQUELETO foi medido contra a PAGINA (#0D0D0D, 1,40:1) e aqui ele
    // cai sobre o PAINEL (0,11), onde da 1,23:1 — menos. Fica assim mesmo, e a
    // razao e que 1,23:1 e exatamente o contraste das linhas em repouso desta
    // mesma lista (lum 0,176 sobre o mesmo painel), que a captura mostra
    // legiveis. Clarear so o esqueleto o faria saltar mais que as linhas.
    // CONFERIDO na reducao a 1/3, que e a proxima de 3 m.
    r.x = x; r.y = y;
    gfx_cor(r, 8.0f / RE_ARTE_H, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
            NV_COR_ESQUELETO_B, a);
    return;
  }
  r.h = RE_ARTE_W / asp;
  if (r.h > RE_ARTE_H) { r.h = RE_ARTE_H; r.w = r.h * asp; }
  r.x = x + (RE_ARTE_W - r.w) * 0.5f;
  r.y = y;
  // O RAIO E FRACAO DA ALTURA, com os dois limites do SDF (ver DESIGN.md): 8px
  // constantes em qualquer proporcao. Dividir por min(w,h), que e o vicio desta
  // base, daria 8/102 numa arte deitada e 8/153 num cartaz — duas silhuetas
  // diferentes na mesma tela, e ja custou cinco defeitos fotografados.
  raio = 8.0f / r.h;
  if (raio > 0.5f) raio = 0.5f;
  if (raio > 0.5f * r.w / r.h) raio = 0.5f * r.w / r.h;
  gfx_rect(r, tex, GFX_ARTE, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
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

// Uma linha da lista, com o mesmo foco invertido do menu de contexto: fundo
// claro e texto escuro, em DEGRAU e nao interpolado — a cor faz parte da chave
// do cache de linhas de text.c e uma cor por quadro apaga o texto (ver a nota
// longa em ctxmenu.c).
static void desenhaLinha(float x, float y, const char *rot, int focada,
                         float a) {
  GfxRect r = { x, y, RE_INTERNO, RE_LINHA };
  float fr = 0.176f, fg = 0.176f, fb = 0.176f;
  int cor = 240;
  if (focada) cor = (int)(ajustes_acento_tinta(&fr, &fg, &fb) * 255.0f + 0.5f);
  gfx_cor(r, 14.0f / RE_LINHA, fr, fg, fb, a);
  { TxtLinha t = txt_linha_corta(TXT_PLR_CORPO, rot, cor, cor, cor, 255,
                                 r.w - 88.0f);
    txt_desenhar_alpha(t, r.x + 44.0f, y + (RE_LINHA - t.h) * 0.5f, a); }
  if (focada) {
    TxtLinha seta = txt_linha(TXT_CAPTION2, "▸", cor, cor, cor, 255);
    txt_desenhar_alpha(seta, r.x + 16.0f, y + (RE_LINHA - seta.h) * 0.5f, a);
  }
}

void recenviar_desenhar(Uint32 agora) {
  float a = anim_suave(anim), alt, x, y, cab, cabTopo, hx, hy, hw;
  int i, n, vis;
  const char *titulo, *chapeu, *pergunta, *rodape;
  (void)agora;
  if (anim < 0.01f) return;

  n = nLinhas();
  vis = n < RE_JANELA ? n : RE_JANELA;
  if (vis < 1) vis = 1;

  // ALTURA DO CABECALHO, por pagina. A de amigos carrega as seis caixas do
  // codigo e duas linhas de explicacao; as outras duas carregam so o titulo e
  // a pergunta. Uma altura unica deixaria um buraco de 200px nas duas.
  //
  // O CARTAZ MANDA NAS DUAS TELAS DO ENVIO, E DE PROPOSITO NAS DUAS. A nota no
  // topo deste arquivo promete "tres telas na MESMA moldura": por um cartaz so
  // em "Para quem?" o cartao encolheria 65px ao passar para "O que dizer?", e
  // uma moldura que muda de tamanho entre dois passos do mesmo gesto e
  // exatamente o que aquela nota existe para impedir.
  cabTopo = temArte() ? RE_ARTE_CAB : 120.0f;
  if (pagina == RE_PAG_AMIGOS)          cab = 150.0f + RE_COD_H + 112.0f;
  else if (pagina == RE_PAG_CONTATOS && nCtts == 0) cab = cabTopo + 116.0f;
  else                                  cab = cabTopo;

  alt = RE_PAD * 2.0f + cab + (float)vis * (RE_LINHA + RE_GAP) - RE_GAP
        + RE_RODAPE;
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

  // A COLUNA DE TEXTO COMECA DEPOIS DO CARTAZ quando ele existe, e volta a
  // margem quando nao existe. Uma coluna sempre recuada deixaria a tela de
  // amigos com 130px de vazio a esquerda do proprio titulo dela.
  hx = x + RE_PAD; hw = RE_INTERNO;
  hy = y + RE_PAD;
  if (temArte()) {
    desenhaArte(x + RE_PAD, y + RE_PAD, a);
    hx += RE_ARTE_W + RE_ARTE_GAP;
    hw -= RE_ARTE_W + RE_ARTE_GAP;
  } else {
    // O CHAPEU SO SOBREVIVE ONDE NAO HA CARTAZ, e a razao e que ele e a METADE
    // REPETIDA do cabecalho. "RECOMENDAR A UM AMIGO" e "Para quem?" dizem a
    // mesma coisa — a um amigo, para quem — e duas linhas dizendo a mesma coisa
    // em volta do titulo sao o que faz um cartao parecer formulario.
    //
    // DAS DUAS, A PERGUNTA E A QUE FICA: ela MUDA entre os dois passos ("Para
    // quem?" e "O que dizer?"), entao e ela que diz onde a pessoa esta no
    // gesto; o chapeu e igual nos dois e ja foi dito pelo item de menu que
    // abriu isto, um toque atras. Na tela de AMIGOS ele continua, porque la nao
    // ha cartaz e "AMIGOS" e a unica linha que nomeia a tela.
    TxtLinha t = txt_linha(TXT_CAPTION2, chapeu, 174, 178, 188, 255);
    txt_desenhar_alpha(t, hx, hy, a * 0.95f);
    hy += 28.0f;
  }

  if (temArte()) {
    // O TITULO EM ATE DUAS LINHAS, e so aqui. A coluna encolheu 130px para o
    // cartaz caber ao lado, mas a ALTURA do cartaz paga por isso. Sem as duas
    // linhas, "O Senhor dos Anéis: A Sociedade do Anel" terminaria em
    // reticencia justamente no cartao que existe para confirmar QUAL obra vai —
    // o corte ficaria pior do que era ANTES do cartaz, e nao melhor.
    //
    // 46 de entrelinha e o corpo de 38 do TXT_HEADLINE mais 8, que e a folga
    // que as duas linhas precisam para um acento da segunda nao encostar na
    // perna da primeira. Duas linhas e o teto porque tres ja passariam da
    // altura do cartaz e o cabecalho teria de crescer — e ele nao pode, sob
    // pena de a moldura mudar entre os dois passos.
    hy += txt_bloco(TXT_HEADLINE, titulo, 245, 248, 255, hx, hy, hw, 46.0f,
                    a, 2) + 6.0f;
    // A SEGUNDA LINHA E, NAS DUAS TELAS, O QUE O TITULO GRANDE NAO DIZ.
    //
    // Em "Para quem?" o titulo grande e a OBRA, e o que falta e QUAL versao
    // dela: o campo `meta` do proprio CatItem ("1994 · 2h22", "2016 · 4
    // temporadas"). E o que um cartaz nao resolve — refilmagem e serie homonima
    // tem nome igual e arte parecida, e a pergunta desta tela e "estou mandando
    // a coisa certa". Dado do item, nao montado aqui: o DESIGN.md proibe
    // inventar metadado para encher espaco.
    //
    // Em "O que dizer?" o titulo grande e a PESSOA, e o que falta e a OBRA. Sem
    // esta linha o cartao tinha um buraco de 80px ao lado do cartaz — e, pior,
    // o passo em que o OK ENVIA nao trazia o nome do que esta sendo enviado em
    // lugar nenhum. Com ela a tela le como uma frase so: mando ISTO para ELA.
    //
    // Em #BEC0C8 e nao no cinza da pergunta: isto e CONTEUDO e a pergunta e
    // ROTULO, e a escada de texto do DESIGN.md separa os dois por um degrau.
    { const char *sub = pagina == RE_PAG_CONTATOS ? item.meta : item.titulo;
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
  } else {
    TxtLinha t = txt_linha_corta(TXT_HEADLINE, titulo, 245, 248, 255, 255, hw);
    txt_desenhar_alpha(t, hx, hy, a);
    hy = y + RE_PAD + 74.0f;
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
    // ABAIXO DO CARTAZ E EM LARGURA CHEIA, e nao na coluna do texto: sao duas
    // oracoes que ja usavam os 764px inteiros, e espremidas em 634 ao lado do
    // cartaz virariam tres linhas — uma a mais do que o `maxLinhas` aceita, ou
    // seja, o fim da frase (o que FAZER) sumiria de novo. Os deslocamentos
    // seguem `cabTopo` em vez dos 120 cravados de antes, senao o bloco
    // atravessaria o cartaz.
    TxtLinha t1 = txt_linha(TXT_CALLOUT, "Nenhum amigo ainda",
                            240, 242, 248, 255);
    txt_desenhar_alpha(t1, x + RE_PAD, y + RE_PAD + cabTopo - 14.0f, a * 0.96f);
    txt_bloco(TXT_CAPTION,
        "Amigos do Trakt entram sozinhos, mas só depois de instalarem o app. Para os outros, troquem o código de 6 letras.",
        190, 194, 204, x + RE_PAD, y + RE_PAD + cabTopo + 26.0f, RE_INTERNO,
        30.0f, a * 0.9f, 2);
  }

  for (i = topo; i < n && i - topo < RE_JANELA; i++) {
    float by = y + RE_PAD + cab + (float)(i - topo) * (RE_LINHA + RE_GAP);
    desenhaLinha(x + RE_PAD, by, rotulo(i), i == foco, a);
  }

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
