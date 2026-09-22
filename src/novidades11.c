// Cartao de NOVIDADES DA 1.1 — sete paginas, uma por assunto.
//
// POR QUE EXISTE: a 1.1 nao e "mais uma tela". Ela traz a Agenda, as listas da
// Biblioteca, os paineis da pagina do titulo, a pergunta de consentimento do
// Social e a atualizacao pelo proprio app — e, junto com isso, mexe em coisas
// que dependem do APARELHO: o que cada botao colorido faz (ou nao faz) em cada
// controle, o que o pacote do Tizen nao consegue, o que muda com e sem o
// Homebrew Channel, e ate onde este binario alcanca em webOS 3, 4 e 5+. O dono
// pediu isso de uma vez: "um modal quando entrar no app dizendo do update novo
// para 1.1, explica tudo, lembrando da diferenca de botoes".
//
// IRMAO DE recintro.c E DE novidades.c: mesma anatomia (cartao central sobre a
// home, figura a esquerda, quatro linhas de recurso a direita, arquivo-marca na
// pasta de dados, esquerda/direita andam, tres... aqui SETE pontos dizem onde a
// pessoa esta). Quem ja viu um dos outros sabe ler este sem aprender nada.
//
// SETE PAGINAS E NAO UMA LONGA. A regra e a de recintro.c levada a serio: um
// cartao de TV e lido em pe, e uma pagina que precisa de mais de quatro linhas
// virou folheto. Cada pagina aqui responde UMA pergunta ("o que e a Agenda?",
// "o que o botao amarelo faz?"), e a que nao coubesse em quatro linhas foi
// cortada, nao espremida.
//
// O OK AVANCA, e so fecha na ULTIMA — a mesma decisao de recintro.c, pela mesma
// razao: num cartao de varias paginas o OK que fecha e uma armadilha, porque o
// gesto mais natural do controle apagaria seis telas que a pessoa nunca saberia
// que existiam. Voltar fecha em qualquer ponto; ninguem e obrigado a ler tudo.
//
// A MARCA E POR CONTEUDO, nao por versao. "novidades-11.txt" cobre ESTA rodada.
// Nao reaproveita e nao sobrescreve "novidades-guia.txt": quem viu o cartao do
// Guia de TV nao ve aquele de novo, e quem nunca viu nenhum dos dois recebe
// so este.
//
// NENHUMA FIGURA E IMAGEM. Nao ha renderizador de SVG neste app (ver gfx.h), e
// o orcamento de textura ja vive encostado no teto na TV do dono (tex_cache.c).
// Entao cada pagina desenha uma MINIATURA da tela de verdade com as primitivas
// dela: a linha do tempo sai com o eixo, o no e o numeral de agendaui.c; a
// curva de qualidade sai com a MESMA tecnica de serieaud.c (o shader nao tem
// rotacao, entao poligonal e sequencia de retangulos verticais); a barra de
// progresso sai com o trilho e o cheio de atualizacao.c; e os rotulos dentro
// das miniaturas sao as chaves de i18n que aquelas telas ja usam ("Exibição",
// "Lista", "Atualizar agora"), nunca texto novo — assim a figura nao pode
// divergir da tela em outro idioma.
//
// TODA AFIRMACAO SOBRE APARELHO FOI CONFERIDA NO CODIGO ANTES DE SER ESCRITA, e
// as que nao passaram no teste ficaram de fora. Onde a fonte esta:
//   botao vermelho   registro.c:26 (scancode 486) e tools/tizen-shell.html:546
//                    (403 vira F9 sintetico). registro.c:19-24 diz que a
//                    CHEGADA da tecla na LG nunca foi confirmada — e por isso a
//                    pagina 6 diz isso, em vez de prometer.
//   botao verde      so existe no shell do Tizen (tizen-shell.html:522), e
//                    nunca chega ao C. Na LG nao ha scancode 487 em src/.
//   botao amarelo    registrado no Tizen (tizen-shell.html:349) e SEM TRATADOR
//                    em lugar nenhum. Nao faz nada nas duas.
//   botao azul       NV_SCANCODE_BLUE 489 (layout.h:87) — app.c:405, app.c:460,
//                    player.c:1285, guia.c:575. No Tizen quem faz isso e
//                    CANAL + (tizen-shell.html:529), e salvosintro.c:296 ja
//                    escreve "CANAL +" sob __EMSCRIPTEN__.
//   sem root         atualizacao.c:266-272 — o app grava com uid 5152,
//                    /usr/bin/luna-send e rwx------ root, e o que sobra e o
//                    luna-send-pub falando com o Homebrew Channel. E ele que
//                    baixa os 36 MB, nao nos (atualizacao.c:273).
//   sem instalador   temInstalador() (atualizacao.c:349) abre o appinfo.json do
//                    hbchannel; sem ele o cartao perde os botoes e fica so com
//                    o endereco (atualizacao.c:536).
//   Tizen            AT_INSTALA 0 (atualizacao.c:70) — la o cartao e sempre so
//                    aviso; sem selo de HDR/Dolby (video_tizen.c:846) e sem o
//                    zoom que corta a barra preta (video_tizen.c:68).
//   webOS 3/4/5+     README.md:19-99. A 4 e MEDIDA (C9, 60 fps); a 5+ e
//                    RELATADA por quem tem (issue #26, B4 de 2024); a 3 e um
//                    pacote separado do ramo webos3. ATUALIZADO em 16/09: o
//                    README ainda diz "ninguem aqui tem uma webOS 3", o que
//                    continua verdade, mas ja NAO e a unica evidencia — um
//                    testador rodou numa 3.4.3 e relatou, e quatro
//                    pre-releases (exp.1..exp.4) sairam do que ele achou. Por
//                    isso a linha desta pagina diz "relatado", o mesmo nivel
//                    da webOS 5+, e nao "experimental": os dois casos sao a
//                    mesma coisa — funciona no aparelho de outra pessoa, nao
//                    no meu. O que ninguem aqui tem
//                    aparelho. A pagina 7 usa essas tres palavras de proposito.
//
// O QUE NAO ENTROU, por nao ter sido possivel confirmar: qualquer numero de
// desempenho fora da C9, e a promessa de que o vermelho abre o painel numa LG.
#include "novidades11.h"
#include "agendaui.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N11_ARQ "novidades-11.txt"

// Mesma geometria de novidades.c e recintro.c — a familia de anuncios fica
// reconhecivel de longe, e quem ja viu um sabe onde olhar neste.
#define N11_W        1440.0f
#define N11_H         800.0f
#define N11_X        ((NV_TELA_W - N11_W) * 0.5f)
#define N11_Y        ((NV_TELA_H - N11_H) * 0.5f)
#define N11_PAD        64.0f
#define N11_FIG_W     560.0f
#define N11_TXT_X     (N11_X + N11_PAD + N11_FIG_W + 56.0f)
#define N11_TXT_W     (N11_X + N11_W - N11_PAD - N11_TXT_X)
#define N11_FEAT_H    124.0f
#define N11_FEAT_ICO   64.0f
#define N11_ABRIR_MS  280.0f
#define N11_FECHAR_MS 160.0f
#define N11_PAG_MS    170.0f
#define N11_PAGINAS       7

static int   aberto, decidido, pagina, sentido;
static float entrada, passo = 1.0f;

int novidades11_aberto(void) { return aberto; }

// Grava a marca. Sem pasta gravavel isto e no-op e o cartao volta no proximo
// arranque; e honesto, e dados.c ja disse no log por que nao ha pasta. Mesma
// escolha de novidades.c e de recintro.c.
static void marcarVisto(void) { dados_gravar(N11_ARQ, "1\n"); }

void novidades11_abrir(int pag) {
  aberto = 1;
  decidido = 1;
  pagina = (pag < 0 || pag >= N11_PAGINAS) ? 0 : pag;
  sentido = 0;
  passo = 1.0f;
  entrada = 1.0f;
}

void novidades11_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N11_ARQ);
  if (s) { free(s); return; }
  aberto = 1;
  pagina = 0;
  sentido = 0;
  passo = 1.0f;
}

static void fechar(void) {
  aberto = 0;
  marcarVisto();
}

static void ir(int d) {
  int nova = pagina + d;
  if (nova < 0 || nova >= N11_PAGINAS) return;
  pagina = nova;
  sentido = d;
  // COM ANIMACOES REDUZIDAS A PAGINA TROCA SECA. O deslize existe para dizer
  // "voce andou"; quem desliga animacao continua sabendo disso pelos pontos do
  // rodape, e um conteudo que aparece deslocado e meio transparente e
  // exatamente o que esse ajuste pede para nao acontecer.
  passo = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
}

void novidades11_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT)  { ir(-1); return; }
  if (k == SDLK_RIGHT) { ir(+1); return; }
  // OK AVANCA ATE A ULTIMA, e la fecha. Ver a nota no topo.
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (pagina < N11_PAGINAS - 1) ir(+1);
    else fechar();
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    fechar();
    return;
  }
  // CIMA/BAIXO ficam engolidos: nao ha o que focar, e vazar a tecla para a home
  // moveria o foco dela debaixo do cartao. Mesma regra de novidades.c.
}

void novidades11_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (passo < 1.0f) passo = anim_rampa(passo, 1.0f, dt, N11_PAG_MS);
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N11_ABRIR_MS : N11_FECHAR_MS);
}

// --- PRIMITIVAS ---------------------------------------------------------------
//
// A poligonal e a MESMA TECNICA de serieaud.c: o shader nao tem rotacao, entao
// um trecho inclinado vira uma sequencia de retangulos verticais que se cobrem.
// Copiada e nao chamada de la porque la ela e `static` e vive junto do painel
// de verdade — e esta miniatura nao deve poder quebrar aquele arquivo.
// A CURVA DO ARCO DE QUALIDADE — suave e continua, e as duas palavras custam
// coisas diferentes.
//
// CONTINUA: a versao anterior desenhava cada trecho como uma PILHA DE ATE 10
// retangulos horizontais. Dez degraus para cobrir 60 px de queda sao degraus de
// 6 px, e a 3 metros aquilo nao le como linha: le como escada. O dono viu na
// captura. Aqui o passo e de 1 px em x, e cada amostra e um retangulo que vai
// do y desta amostra ao y da proxima — com 1 px de passo os degraus somem
// dentro da propria espessura do traco.
//
// SUAVE: mesmo com passo de 1 px, ligar os pontos com RETAS deixa um bico em
// cada episodio. Catmull-Rom passa exatamente pelos pontos (nao e aproximacao,
// como Bezier: a nota do episodio 4 continua sendo desenhada na altura da nota
// do episodio 4) e chega neles com a tangente dada pelos vizinhos, entao o bico
// vira inflexao. Nas pontas o ponto e espelhado, senao a curva entra e sai reta
// e a primeira e a ultima subida ficam diferentes das do meio.
//
// NAO E O `alt` QUE MANDA no custo: sao ~330 retangulos por curva, todos
// gfx_cor sem textura, e o painel de imagens da C9 ja mede o custo de
// preenchimento como o recurso escasso. Se um dia isto pesar, o lugar de
// economizar e o passo (2 px ainda le como linha), nao o numero de pontos.
static float cr1(float p0, float p1, float p2, float p3, float t) {
  float t2 = t * t, t3 = t2 * t;
  return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                 (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                 (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

static void curva(float x0, float passoX, const float *ys, int n, float esp,
                  float r, float g, float b, float a) {
  float largura = passoX * (float)(n - 1);
  int amostras = (int)largura, k;
  float ant = ys[0];
  if (n < 2 || amostras < 2) return;
  for (k = 1; k <= amostras; k++) {
    float u = (float)k / (float)amostras * (float)(n - 1);
    int i = (int)u;
    float t = u - (float)i, yv;
    int i0, i3;
    if (i > n - 2) { i = n - 2; t = 1.0f; }
    i0 = i - 1; i3 = i + 2;
    // ESPELHAR a ponta em vez de repetir o extremo: repetindo, a tangente de
    // entrada nasce zero e o primeiro trecho sai horizontal.
    yv = cr1(i0 < 0 ? 2.0f * ys[0] - ys[1] : ys[i0], ys[i], ys[i + 1],
             i3 > n - 1 ? 2.0f * ys[n - 1] - ys[n - 2] : ys[i3], t);
    { float cx = x0 + (float)(k - 1) / (float)amostras * largura;
      GfxRect s = { cx, (ant < yv ? ant : yv) - esp * 0.5f,
                    largura / (float)amostras + 1.0f,
                    fabsf(yv - ant) + esp };
      gfx_cor(s, 0.0f, r, g, b, a); }
    ant = yv;
  }
}

static void disco(float cx, float cy, float d, float r, float g, float b,
                  float a) {
  GfxRect p = { cx - d * 0.5f, cy - d * 0.5f, d, d };
  gfx_cor(p, 0.5f, r, g, b, a);
}

// Barra de texto falso. Toda figura daqui usa isto para o que e PROSA na tela
// de verdade — escrever texto de mentira numa miniatura de 0,3 de escala da
// uma linha ilegivel que o olho tenta ler e nao consegue, que e pior que uma
// barra que ninguem tenta ler.
static void barra(float x, float y, float w, float h, float lum, float a) {
  gfx_cor((GfxRect){ x, y, w, h }, 0.5f, lum, lum + 0.01f, lum + 0.05f, a);
}

// A pilula de foco desta familia: superficie CLARA preenchida, sem anel. Quem
// desenha texto por cima usa 17,17,17. Ver a nota de FOCO em agendaui.c.
// Tinta que contrasta com a pilula de foco (0..255).
static int tintaFoco(void) { float r,g,b; return (int)(ajustes_acento_tinta(&r,&g,&b)*255.0f+0.5f); }
static void focoPilula(GfxRect r, float raioPx, float a) {
  float fr, fg, fb;
  ajustes_acento_tinta(&fr, &fg, &fb);
  gfx_cor(r, raioPx / (r.w < r.h ? r.w : r.h), fr, fg, fb, a);
}

// --- FIGURA 1: a linha do tempo da Agenda -------------------------------------
//
// As tres colunas de agendaui.c em miniatura: ESTACAO (numeral do dia + dia da
// semana), EIXO com um no por serie, e CONTEUDO. O numeral e o no ficam no
// MESMO y, que e o que faz a coluna ler como calendario em vez de rotulo — se
// esta miniatura perder isso, ela vira a "pilha de pilulas" que aquela tela
// deixou de ser.
//
// O DESPERTADOR E O DE VERDADE: agendaui_despertador, a mesma funcao que a tela
// e o cartao de lembrete chamam. Redesenha-lo aqui a mao seria um segundo
// despertador com o mesmo nome, livre para divergir do primeiro.
static void figAgenda(float x, float y, float a, Uint32 agora) {
  static const char *DIA[3] = { "16", "17", "19" };
  const float passoY = 104.0f, eixoX = x + 92.0f, cx = x + 132.0f;
  const float cw = N11_FIG_W - 132.0f;
  float cr, cg, cb;
  int i;
  agendaui_cor_lembrete(1, 0, &cr, &cg, &cb);

  // O eixo inteiro primeiro, para os nos caírem por cima dele.
  //
  // RAIO ZERO, e isto foi PAGO numa captura: num retangulo de 3 px de largura
  // por 232 de altura o SDF arredondado com raio 0,5 erodiu a barra inteira e
  // sobrou uma lasca de 50 px na altura do segundo no. Linha fina e retangulo
  // reto — e o que serieaud.c ja faz na poligonal dele.
  gfx_cor((GfxRect){ eixoX - 2.0f, y + 16.0f, 4.0f, passoY * 2.0f + 28.0f },
          0.0f, 0.34f, 0.35f, 0.41f, a * 0.95f);

  for (i = 0; i < 3; i++) {
    float ry = y + (float)i * passoY;
    int foco = (i == 0);
    float ny = ry + 30.0f;
    GfxRect linha = { cx, ry, cw, 84.0f };
    float lumTexto = foco ? 0.10f : 0.46f;

    // ESTACAO: o numeral e texto de verdade (digito nao precisa de traducao) e
    // a palavra "hoje" so na primeira, como na tela.
    { TxtLinha t = txt_linha(TXT_TITULO3, DIA[i],
                             foco ? 240 : 150, foco ? 242 : 154,
                             foco ? 248 : 165, 255);
      txt_desenhar_alpha(t, eixoX - 30.0f - (float)t.w, ny - (float)t.h * 0.5f,
                         a); }
    if (foco) {
      TxtLinha t = txt_linha(TXT_MINI, i18n("hoje"),
                             (int)(cr * 255.0f), (int)(cg * 255.0f),
                             (int)(cb * 255.0f), 255);
      txt_desenhar_alpha(t, eixoX - 30.0f - (float)t.w, ny + 20.0f, a * 0.95f);
    }

    // LINHA FOCADA: superficie clara PREENCHIDA e so na coluna do conteudo.
    // Preenchendo ate o eixo, o eixo e o numeral sumiriam dentro dela — e o
    // defeito que agendaui.c descreve e evita.
    if (foco) focoPilula(linha, 14.0f, a);

    // NO do eixo: cheio e maior na estreia de hoje. Os outros dois precisam
    // vencer o proprio eixo em que estao — a 11 px e 0,42 eles sumiam dentro
    // da linha.
    disco(eixoX, ny, foco ? 20.0f : 14.0f,
          foco ? cr : 0.56f, foco ? cg : 0.58f, foco ? cb : 0.64f, a);

    // CONTEUDO: cartaz 2:3 e tres barras (titulo, episodio, apoio).
    // O CARTAZ DA LINHA FOCADA E ESCURO, e nao claro: a 0,80 sobre a pilula de
    // 0,961 ele virava um fantasma — a mesma pedra que recintro.c documenta
    // ("o cartaz da linha focada SUMIA dentro do proprio foco").
    gfx_cor((GfxRect){ cx + 14.0f, ry + 12.0f, 40.0f, 60.0f }, 0.16f,
            foco ? 0.62f : 0.175f, foco ? 0.62f : 0.180f,
            foco ? 0.66f : 0.205f, a);
    barra(cx + 68.0f, ry + 18.0f, (cw - 160.0f) * (0.92f - 0.16f * (float)i),
          11.0f, lumTexto, a * 0.95f);
    barra(cx + 68.0f, ry + 38.0f, (cw - 160.0f) * (0.60f - 0.10f * (float)i),
          8.0f, foco ? 0.30f : 0.34f, a * 0.9f);
    barra(cx + 68.0f, ry + 54.0f, (cw - 160.0f) * (0.46f + 0.08f * (float)i),
          8.0f, foco ? 0.42f : 0.28f, a * 0.85f);

    // DESPERTADOR: so nas duas com lembrete ligado, como na tela.
    if (i != 1) {
      GfxRect ic = { cx + cw - 56.0f, ry + 24.0f, 36.0f, 36.0f };
      float dr = cr, dg = cg, db = cb;
      // Sobre a pilula clara o esmeralda cheio some; agendaui_cor_lembrete ja
      // devolve a variante escura quando o fundo e claro (segundo argumento).
      if (foco) agendaui_cor_lembrete(1, 1, &dr, &dg, &db);
      agendaui_despertador(ic, 1, dr, dg, db, a, agora, 0);
    }
  }
}

// --- FIGURA 2: a Biblioteca em exibicao de LISTA ------------------------------
//
// Em cima a faixa de tres seletores de biblioteca.c, com o do meio em foco; a
// pilula traz o ROTULO e o VALOR de verdade ("Exibição" / "Lista"), que sao as
// chaves daquela tela — em ingles a figura muda junto com ela. Embaixo, tres
// linhas da exibicao nova (miniatura 2:3 a esquerda, linha inteira preenchida
// no foco), que e a diferenca que a pagina anuncia.
static void figListas(float x, float y, float a) {
  const float pw = (N11_FIG_W - 2.0f * 12.0f) / 3.0f, ph = 62.0f;
  int i;
  for (i = 0; i < 3; i++) {
    GfxRect p = { x + (float)i * (pw + 12.0f), y, pw, ph };
    int foco = (i == 1);
    int cor = foco ? tintaFoco() : 179;
    if (foco) focoPilula(p, 14.0f, a);
    else      gfx_cor(p, 14.0f / ph, 1.0f, 1.0f, 1.0f, 0.06f * a);
    { TxtLinha r = txt_linha_corta(TXT_MINI,
        i18n(i == 0 ? "Fonte" : i == 1 ? "Exibição" : "Listas públicas"),
        cor, cor, cor, 255, pw - 32.0f);
      txt_desenhar_alpha(r, p.x + 16.0f, p.y + 10.0f, a * 0.9f); }
    { int c2 = foco ? tintaFoco() : 236;
      TxtLinha v = txt_linha_corta(TXT_CAPTION2,
        i18n(i == 0 ? "Trakt" : i == 1 ? "Lista" : "Procurar"),
        c2, c2, c2, 255, pw - 32.0f);
      txt_desenhar_alpha(v, p.x + 16.0f, p.y + 30.0f, a); }
  }

  // A LINHA E A EXIBICAO NOVA, entao ela e o assunto da figura e ganha o
  // tamanho: 84 px de altura com a miniatura 2:3 de 44x66, que sao as medidas
  // de biblioteca.c (BIB_LIN_H 108, mini 64x96) reduzidas pelo mesmo fator.
  for (i = 0; i < 3; i++) {
    float ry = y + ph + 28.0f + (float)i * 96.0f;
    GfxRect l = { x, ry, N11_FIG_W, 84.0f };
    int foco = (i == 0);
    float lum = foco ? 0.12f : 0.48f;
    if (foco) focoPilula(l, 14.0f, a);
    gfx_cor((GfxRect){ x + 14.0f, ry + 9.0f, 44.0f, 66.0f }, 0.18f,
            foco ? 0.62f : 0.175f, foco ? 0.62f : 0.180f,
            foco ? 0.66f : 0.205f, a);
    barra(x + 74.0f, ry + 24.0f, (N11_FIG_W - 160.0f) * (0.88f - 0.18f * (float)i),
          12.0f, lum, a * 0.95f);
    barra(x + 74.0f, ry + 50.0f, (N11_FIG_W - 160.0f) * (0.44f + 0.09f * (float)i),
          9.0f, foco ? 0.34f : 0.30f, a * 0.85f);
  }
}

// --- FIGURA 3: o arco de qualidade e o radar de desistencia -------------------
//
// Os dois paineis de serieaud.c empilhados, com a FORMA que eles teriam numa
// temporada de verdade: nota que cai no meio e sobe no fim, retencao que cai
// monotona. As duas series sao inventadas e so podem ser (nao ha rede aqui);
// o que nao pode ser inventado e o FORMATO, e ele e o de la.
//
// AS DUAS ESCALAS SAO NORMALIZADAS, e essa foi a correcao que a primeira
// captura exigiu: desenhadas em escala absoluta (0 a 10 e 0 a 100%) as duas
// series usavam um terco da altura do painel e saiam PLANAS — duas linhas
// quase retas nao dizem "a temporada melhorou" nem "o pessoal desistiu", que e
// a unica coisa que esta figura tem para dizer. serieaud.c faz o mesmo nos
// paineis de verdade, pelo mesmo motivo.
static void figAudiencia(float x, float y, float a) {
  // OS DOIS VETORES JA VEM NORMALIZADOS de 0 a 1 — a altura util do painel. Na
  // primeira captura eles traziam a grandeza crua (nota 5,8 a 9,8 de 10;
  // retencao 100% a 64%) e as duas series usavam menos de metade da altura:
  // sairam PLANAS, e uma curva plana nao diz "a temporada melhorou". Normalizar
  // aqui e o que serieaud.c faz nos paineis de verdade.
  static const float NOTA[8] = { 0.34f, 0.10f, 0.52f, 0.42f,
                                 0.66f, 0.58f, 0.86f, 1.00f };
  static const float RET[8]  = { 1.00f, 0.78f, 0.66f, 0.60f,
                                 0.54f, 0.49f, 0.45f, 0.41f };
  const float pw = N11_FIG_W, altA = 176.0f, altB = 168.0f;
  const float px = x + 26.0f, plw = pw - 52.0f, passoX = plw / 7.0f;
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);

  // --- ARCO DE QUALIDADE ----------------------------------------------------
  gfx_cor((GfxRect){ x, y, pw, altA }, 18.0f / altA, 0.105f, 0.108f, 0.122f, a);
  { TxtLinha t = txt_linha(TXT_MINI, i18n("nota do episódio"),
                           150, 154, 165, 255);
    txt_desenhar_alpha(t, px, y + 14.0f, a * 0.9f); }
  { float base = y + altA - 34.0f, topo = y + 62.0f, alt = base - topo;
    // Referencia tracejada: a media da temporada. Sem ela a curva e so um
    // desenho bonito — e com ela da para ver QUAIS episodios ficaram acima.
    float media = 0.0f;
    for (i = 0; i < 8; i++) media += NOTA[i];
    media /= 8.0f;
    { float k, ym = base - media * alt;
      for (k = 0.0f; k < plw; k += 20.0f) {
        float lg = 11.0f;
        if (k + lg > plw) lg = plw - k;
        gfx_cor((GfxRect){ px + k, ym, lg, 2.0f }, 0.0f,
                0.42f, 0.44f, 0.50f, a * 0.45f);
      } }
    { float ys[8];
      for (i = 0; i < 8; i++) ys[i] = base - NOTA[i] * alt;
      curva(px, passoX, ys, 8, 5.0f, ar, ag, ab, a); }
    // TODOS OS PONTOS DO MESMO TAMANHO. O ultimo era desenhado com 18 px
    // contra 11 dos outros, para "terminar" a curva — e numa miniatura aquilo
    // nao le como enfase, le como bolha fora de lugar no fim de uma linha que
    // ja termina sozinha. Curva suave nao precisa de ponto final gordo.
    for (i = 0; i < 8; i++)
      disco(px + (float)i * passoX, base - NOTA[i] * alt,
            9.0f, 0.95f, 0.96f, 0.98f, a); }

  // --- RADAR DE DESISTENCIA -------------------------------------------------
  { float by = y + altA + 20.0f;
    // A BASE DESCE E O TOPO SOBE: antes as barras viviam numa faixa de 76 px no
    // meio de um painel de 168, e metade do painel ficava vazia embaixo delas.
    // Barra curta no meio de espaco vazio nao le como barra, le como mancha.
    float base = by + altB - 22.0f, topo = by + 46.0f, alt = base - topo;
    float bw = passoX * 0.44f;
    gfx_cor((GfxRect){ x, by, pw, altB }, 18.0f / altB, 0.105f, 0.108f, 0.122f, a);
    { TxtLinha t = txt_linha(TXT_MINI, i18n("quem continua"),
                             150, 154, 165, 255);
      txt_desenhar_alpha(t, px, by + 14.0f, a * 0.9f); }
    for (i = 0; i < 8; i++) {
      // O E1 E O DENOMINADOR — ele vale 100% por definicao, e por isso e a
      // unica barra cheia. As outras sao a mesma cor com menos alfa: a queda
      // tem de ser lida na ALTURA, nao no matiz.
      float h = RET[i] * alt;
      GfxRect b = { px + (float)i * passoX - bw * 0.5f, base - h, bw, h };
      // O RAIO E FRACAO DA ALTURA — nao do menor lado. A versao anterior
      // dividia por min(largura,altura) achando que assim daria pixel
      // constante, e o resultado esta na captura que o dono mandou: a primeira
      // barra saiu capsula inteira e as ultimas quase retangulo. O `r` do SDF
      // (ver FS_SDF em gfx.c) vive no espaco em que a ALTURA vale 1, entao
      // 8/min() numa barra alta e estreita pede 8/29 da ALTURA, que sao 20 px,
      // e nao 8. Dividir pela altura da 8 px em qualquer barra.
      //
      // O teto nao e 0.5 e sim metade da LARGURA medida na mesma escala: numa
      // barra mais larga que alta, 0.5 da altura ainda deixaria o canto reto
      // na horizontal.
      float raio = h > 0.1f ? 8.0f / h : 0.5f;
      float teto = h > 0.1f ? 0.5f * bw / h : 0.5f;
      if (raio > 0.5f) raio = 0.5f;
      if (raio > teto) raio = teto;
      gfx_cor(b, raio, ar, ag, ab, a * (i == 0 ? 0.92f : 0.50f));
    } }
}

// --- FIGURA 4: a pergunta de consentimento do Social --------------------------
//
// A aba SOCIAL aberta com a pergunta ocupando a lista inteira, como
// salvospainel.c a monta: sem recomendacao por baixo, porque "uma pergunta que
// se pode ignorar rolando a tela nao foi feita". O "NAO" vem PRIMEIRO e leva o
// foco — e a resposta padrao la, e inverter isso aqui ensinaria o contrario da
// tela.
static void figSocial(float x, float y, float a) {
  const float e = 0.62f, pw = 776.0f * e, pad = 44.0f * e;
  float px = x + (N11_FIG_W - pw) * 0.5f, tx, ty, th;
  int i;
  float ar, ag, ab;
  GfxRect p = { px, y, pw, 396.0f };
  ajustes_acento(&ar, &ag, &ab);

  gfx_cor(p, 0.036f, 0.075f, 0.078f, 0.088f, a);
  gfx_rect(p, 0, GFX_ANEL, 0, 2.0f / pw, 0, 0.036f, 0.32f, 0.34f, 0.40f,
           a * 0.7f);

  // Linha de abas, com SOCIAL aberta — mesma escala de recintro.c:figPainel.
  tx = px + pad; ty = y + 56.0f * e; th = 52.0f * e;
  for (i = 0; i < 2; i++) {
    int ativa = (i == 1), cor = ativa ? 20 : 176;
    TxtLinha t = txt_linha(TXT_MINI, i18n(i == 0 ? "SALVOS" : "SOCIAL"),
                           cor, cor, cor, 255);
    GfxRect pil = { tx, ty, (float)t.w + 44.0f * e, th };
    if (ativa) gfx_cor(pil, NV_RAIO_PILL, ar, ag, ab, a);
    else       gfx_cor(pil, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, 0.04f * a);
    txt_desenhar_alpha(t, pil.x + 22.0f * e, ty + (th - (float)t.h) * 0.5f, a);
    tx += pil.w + 14.0f * e;
  }

  // O enunciado, em duas barras: a pergunta e prosa na tela de verdade.
  barra(px + pad, y + 118.0f, pw - pad * 2.0f, 12.0f, 0.52f, a * 0.95f);
  barra(px + pad, y + 142.0f, (pw - pad * 2.0f) * 0.72f, 9.0f, 0.32f, a * 0.9f);

  // As duas respostas, com os rotulos DE VERDADE e o "nao" em foco.
  for (i = 0; i < 2; i++) {
    GfxRect l = { px + pad, y + 190.0f + (float)i * 76.0f,
                  pw - pad * 2.0f, 62.0f };
    int foco = (i == 0), cor = foco ? tintaFoco() : 236;
    TxtLinha t = txt_linha_corta(TXT_CAPTION2,
        i18n(foco ? "Não, não quero aparecer" : "Sim, pode me mostrar"),
        cor, cor, cor, 255, l.w - 56.0f);
    if (foco) focoPilula(l, 14.0f, a);
    else      gfx_cor(l, 14.0f / l.h, 0.176f, 0.180f, 0.196f, a);
    txt_desenhar_alpha(t, l.x + 28.0f, l.y + (l.h - (float)t.h) * 0.5f, a);
  }
}

// --- FIGURA 5: os DOIS estados do cartao de atualizacao -----------------------
//
// E a figura que carrega a pagina inteira, porque a diferenca entre ter e nao
// ter o Homebrew Channel nao e uma frase: e um cartao com botoes e barra contra
// um cartao com um endereco. Os dois desenhados juntos, um sobre o outro, com o
// trilho e o cheio de atualizacao.c (NV_RAIO_PILL, acento no cheio).
static void figAtualizar(float x, float y, float a) {
  const float pw = N11_FIG_W;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);

  // --- COM instalador: barra de progresso e os dois botoes.
  { GfxRect c = { x, y, pw, 236.0f };
    float bx = x + 24.0f;
    int i;
    gfx_cor(c, 16.0f / c.h, 0.105f, 0.108f, 0.122f, a);
    barra(bx, y + 26.0f, 190.0f, 14.0f, 0.55f, a * 0.95f);
    { TxtLinha t = txt_linha(TXT_MINI, i18n("Instalando..."),
                             232, 236, 246, 255);
      txt_desenhar_alpha(t, bx, y + 58.0f, a * 0.95f); }
    { GfxRect trilho = { bx, y + 92.0f, pw - 48.0f - 70.0f, 10.0f };
      GfxRect cheio  = { bx, y + 92.0f, (pw - 48.0f - 70.0f) * 0.62f, 10.0f };
      gfx_cor(trilho, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, 0.14f * a);
      gfx_cor(cheio, NV_RAIO_PILL, ar, ag, ab, a);
      { TxtLinha t = txt_linha(TXT_MINI, "62%", 200, 204, 214, 255);
        txt_desenhar_alpha(t, trilho.x + trilho.w + 16.0f, y + 86.0f,
                           a * 0.95f); } }
    for (i = 0; i < 2; i++) {
      int foco = (i == 0), cor = foco ? tintaFoco() : 236;
      TxtLinha t = txt_linha(TXT_MINI,
          i18n(foco ? "Atualizar agora" : "Depois"), cor, cor, cor, 255);
      GfxRect b = { bx, y + 142.0f, (float)t.w + 48.0f, 54.0f };
      if (foco) focoPilula(b, 27.0f, a);
      else      gfx_cor(b, NV_RAIO_PILL, 0.176f, 0.176f, 0.196f, a);
      txt_desenhar_alpha(t, b.x + 24.0f, b.y + (54.0f - (float)t.h) * 0.5f, a);
      bx += b.w + 14.0f;
    } }

  // --- SEM instalador: o mesmo cartao sem botao nenhum. Mais baixo e mais
  // apagado de proposito — a pagina diz que este e o caso pobre, e a figura
  // nao deve dar aos dois o mesmo peso.
  { float y2 = y + 258.0f;
    GfxRect c = { x, y2, pw, 132.0f };
    gfx_cor(c, 16.0f / c.h, 0.085f, 0.088f, 0.098f, a);
    barra(x + 24.0f, y2 + 24.0f, 190.0f, 14.0f, 0.38f, a * 0.85f);
    { TxtLinha t = txt_linha_corta(TXT_MINI,
        "github.com/iqui27/nuvio-native-legacy/releases",
        150, 154, 165, 255, pw - 48.0f);
      txt_desenhar_alpha(t, x + 24.0f, y2 + 58.0f, a * 0.85f); }
    { TxtLinha t = txt_linha(TXT_MINI, i18n("OK para fechar"),
                             132, 136, 146, 255);
      txt_desenhar_alpha(t, x + pw - 24.0f - (float)t.w, y2 + 92.0f,
                         a * 0.8f); } }
}

// --- FIGURA 6: a fileira colorida do controle ---------------------------------
//
// O controle visto de frente, so a parte que importa: o anel do D-pad com o OK
// no meio e, embaixo, as quatro teclas de cor. A que a LG usa (AZUL) fica
// CHEIA; as que nao fazem nada no aparelho ficam em ANEL, com o miolo vazado —
// e a unica forma de dizer "existe mas nao responde" sem escrever nada dentro
// de um circulo de 40 px. A legenda embaixo diz qual e qual.
static void figControle(float x, float y, float a) {
  static const float COR[4][3] = { { 0.86f, 0.24f, 0.24f },
                                   { 0.30f, 0.74f, 0.38f },
                                   { 0.92f, 0.76f, 0.20f },
                                   { 0.30f, 0.56f, 0.94f } };
  // 1 = faz alguma coisa em pelo menos uma das duas TVs e CHEIA; 0 = so anel.
  static const int VIVA[4] = { 1, 1, 0, 1 };
  const float ctlW = 176.0f, ctlH = 350.0f;
  float cx = x + (N11_FIG_W - ctlW) * 0.5f, meio = cx + ctlW * 0.5f;
  GfxRect corpo = { cx, y, ctlW, ctlH };
  int i;

  // O RAIO E FRACAO DA ALTURA (ver FS_SDF em gfx.c: o `r` vive no espaco em
  // que a altura vale 1). Este comentario dizia "menor lado" e estava errado:
  // 40/176 aqui nao pede 40 px, pede 0,227 da altura, que sao 79 px. Como o
  // que se queria era justamente ponta bem arredondada, o NUMERO ficou — o que
  // muda e parar de contar mentira sobre ele para quem vier depois.
  gfx_cor(corpo, 40.0f / ctlW, 0.125f, 0.128f, 0.142f, a);

  // Anel do D-pad e o OK no meio, so para o objeto ser reconhecivel como
  // controle antes de alguem ler qualquer palavra.
  //
  // TRES DISCOS, e nao um GFX_ANEL. O dono olhou a tela rodando e disse que
  // este anel "nao ta redondo liso": ele saia com lado reto, um octogono. A
  // nota dez linhas abaixo ja dizia isso dos botoes coloridos e eu deixei o
  // D-pad de fora achando que 8 px em 124 era fino o bastante — nao era. Aqui
  // nao ha o que ajustar: disco cheio com disco do fundo por dentro da um anel
  // exato em qualquer diametro, porque os dois lados da espessura sao a mesma
  // borda de circulo que o GFX_COR ja desenha lisa.
  { disco(meio, y + 108.0f, 124.0f, 0.30f, 0.31f, 0.36f, a);
    disco(meio, y + 108.0f, 108.0f, 0.125f, 0.128f, 0.142f, a);
    disco(meio, y + 108.0f, 54.0f, 0.20f, 0.21f, 0.24f, a); }

  // A FILEIRA DE CORES, que e o assunto da pagina.
  //
  // O BOTAO INERTE E DOIS DISCOS, e nao um GFX_ANEL: a primeira captura saiu
  // com um quadrado arredondado no lugar do circulo. O anel com raio 0,5 SO da
  // circulo quando a espessura e pequena em relacao ao diametro (o anel do
  // D-pad, 8 px em 124, sai redondo); a 3,6 px em 26 ele erode os cantos e
  // vira um squircle. Disco de cor com um disco da COR DO CORPO por dentro da
  // o anel exato, em qualquer tamanho.
  for (i = 0; i < 4; i++) {
    float bx = meio - 54.0f + (float)i * 36.0f, by = y + 232.0f;
    disco(bx, by, 30.0f, COR[i][0], COR[i][1], COR[i][2],
          a * (VIVA[i] ? 1.0f : 0.85f));
    if (!VIVA[i]) disco(bx, by, 19.0f, 0.125f, 0.128f, 0.142f, a);
  }
  // Duas teclas anonimas embaixo, so para a fileira de cores nao ficar sendo o
  // fim do controle — num controle de verdade ela nunca e.
  for (i = 0; i < 2; i++)
    gfx_cor((GfxRect){ meio - 62.0f + (float)i * 68.0f, y + 288.0f, 56.0f, 22.0f },
            0.5f, 0.20f, 0.21f, 0.24f, a * 0.8f);

  // LEGENDA: o que cheio e vazado querem dizer. Em TXT_CAPTION2 e nao TXT_MINI
  // — e a unica coisa que explica a figura, e a 3 m o MINI daqui ja e o limite.
  { float ly = y + ctlH + 30.0f;
    disco(x + 16.0f, ly + 11.0f, 22.0f, 0.86f, 0.87f, 0.90f, a * 0.95f);
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("faz alguma coisa"),
                             186, 190, 200, 255);
      txt_desenhar_alpha(t, x + 40.0f, ly, a * 0.9f); }
    disco(x + 268.0f, ly + 11.0f, 22.0f, 0.86f, 0.87f, 0.90f, a * 0.95f);
    disco(x + 268.0f, ly + 11.0f, 14.0f, 0.075f, 0.078f, 0.088f, a);
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("não faz nada"),
                             186, 190, 200, 255);
      txt_desenhar_alpha(t, x + 292.0f, ly, a * 0.9f); } }
}

// --- FIGURA 7: o app dentro da TV, e a legenda das tres certezas -------------
//
// A TV com o app em miniatura diz o assunto ("este app, no seu aparelho") e a
// LEGENDA embaixo diz a unica coisa que a coluna da direita nao consegue dizer
// sozinha: que MEDIDO, RELATADO e EXPERIMENTAL sao tres graus diferentes de
// certeza, e nao tres sinonimos de "funciona".
//
// A primeira versao desta figura repetia, embaixo da TV, os quatro nomes de
// plataforma que ja estao nos titulos da direita. Duas listas iguais lado a
// lado nao informam o dobro: informam metade, porque o olho le a segunda
// procurando a diferenca e nao acha nenhuma.
static void figAparelhos(float x, float y, float a) {
  // DOIS GRAUS, nao tres. A legenda tinha "experimental" para a webOS 3; ela
  // passou a "relatado" em 16/09, quando um testador confirmou numa 3.4.3, e
  // uma legenda com um nivel que nenhuma linha da direita usa e pior que
  // legenda nenhuma: o olho procura a quem ela se refere e nao acha.
  static const char *NIVEL[2] = { "medido aqui", "relatado" };
  const float tvW = 448.0f, tvH = 262.0f;
  float tx = x + (N11_FIG_W - tvW) * 0.5f;
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);

  gfx_cor((GfxRect){ tx, y, tvW, tvH }, 12.0f / tvH, 0.20f, 0.21f, 0.24f, a);
  gfx_cor((GfxRect){ tx + 9.0f, y + 9.0f, tvW - 18.0f, tvH - 18.0f },
          8.0f / (tvH - 18.0f), 0.055f, 0.057f, 0.065f, a);
  gfx_cor((GfxRect){ tx + tvW * 0.5f - 50.0f, y + tvH + 3.0f, 100.0f, 11.0f },
          0.5f, 0.20f, 0.21f, 0.24f, a);

  // O app dentro dela: hero em cima e uma fileira de cartazes embaixo.
  { float sx = tx + 24.0f, sy = y + 24.0f, sw = tvW - 48.0f;
    gfx_cor((GfxRect){ sx, sy, sw, 100.0f }, 0.06f, 0.14f, 0.145f, 0.17f, a);
    barra(sx + 16.0f, sy + 28.0f, sw * 0.44f, 13.0f, 0.60f, a * 0.95f);
    barra(sx + 16.0f, sy + 54.0f, sw * 0.28f, 9.0f, 0.34f, a * 0.9f);
    for (i = 0; i < 5; i++) {
      GfxRect c = { sx + (float)i * 78.0f, sy + 118.0f, 66.0f, 66.0f };
      // OS CINCO CARTAZES SAO A MESMA COR. Na primeira captura eu variava so o
      // canal VERMELHO com o indice, e a fileira saia esmaecendo para marrom —
      // uma cor que nao existe em lugar nenhum desta interface. Variar
      // luminancia e variar os tres canais juntos.
      float lum = 0.19f + 0.022f * (float)i;
      if (i == 1)
        gfx_cor((GfxRect){ c.x - 5.0f, c.y - 5.0f, c.w + 10.0f, c.h + 10.0f },
                0.14f, ar, ag, ab, a);
      gfx_cor(c, 0.14f, lum, lum + 0.005f, lum + 0.035f, a);
    } }

  // A LEGENDA DOS DOIS GRAUS: disco CHEIO para o que foi medido neste
  // aparelho, disco VAZADO para o que so existe como relato de outra pessoa.
  // A diferenca entre os dois e a coisa inteira que esta figura tem a dizer.
  for (i = 0; i < 2; i++) {
    float ly = y + tvH + 42.0f + (float)i * 44.0f;
    disco(x + 16.0f, ly + 11.0f, 22.0f, ar, ag, ab, a);
    if (i) disco(x + 16.0f, ly + 11.0f, 14.0f, 0.075f, 0.078f, 0.088f, a);
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n(NIVEL[i]), 200, 200, 200, 255);
      txt_desenhar_alpha(t, x + 40.0f, ly, a * 0.95f); }
  }
}

// Uma linha de recurso: icone em disco + titulo + descricao de ate 2 linhas.
// Mesmo desenho e mesmas medidas de novidades.c e recintro.c.
static float feature(float x, float y, float w, const char *icone,
                     const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, N11_FEAT_ICO, N11_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255,
                                 w - N11_FEAT_ICO - 20.0f);
    txt_desenhar_alpha(t, x + N11_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + N11_FEAT_ICO + 20.0f, y + 40.0f,
                        w - N11_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 2);
    return h > N11_FEAT_H - 40.0f ? h + 40.0f : N11_FEAT_H; }
}

// Os sete pontos. O ativo e MAIOR e sai na cor do tema; os outros sao cinza —
// nunca um branco cravado. Mesma regra de recintro.c; so o passo encolheu, de
// 30 para 26, porque sete pontos a 30 encostavam no rodape da direita.
static void pontos(float x, float y, float a) {
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < N11_PAGINAS; i++) {
    float d = (i == pagina) ? 16.0f : 10.0f;
    GfxRect p = { x + (float)i * 26.0f + (16.0f - d) * 0.5f,
                  y + (16.0f - d) * 0.5f, d, d };
    if (i == pagina) gfx_cor(p, 0.5f, ar, ag, ab, a);
    else             gfx_cor(p, 0.5f, 0.34f, 0.35f, 0.40f, a * 0.9f);
  }
}

// O TITULO DA PAGINA e o TOPO DA FIGURA andam juntos: cada miniatura tem a sua
// altura, e uma origem unica deixaria a do controle encostada no rodape e a da
// Agenda boiando. A tabela fica aqui, e nao espalhada por sete `if`.
//
// AS SETE GANHARAM +36 em 16/09: o dono viu a captura e disse que o titulo
// estava colado na figura. O titulo e TXT_TITULO3 desenhado a partir de
// N11_Y+96, entao com a figura comecando em 152 sobravam pouco mais de 20 px
// de folga sob a linha de base — a figura lia como parte do titulo. Nao da
// para mexer so na menor: a distancia tem de ser a MESMA nas sete, senao a
// troca de pagina faz o titulo pular.
static const float FIG_Y[N11_PAGINAS] = {
  232.0f, 208.0f, 200.0f, 212.0f, 208.0f, 190.0f, 188.0f
};

static const char *tituloPagina(int p) {
  switch (p) {
    case 0:  return "Agenda e lembretes";
    case 1:  return "Listas na Biblioteca";
    case 2:  return "Na página do título";
    case 3:  return "Quem pode te encontrar";
    case 4:  return "Atualizar pelo app";
    case 5:  return "Os botões coloridos";
    default: return "A sua TV";
  }
}

void novidades11_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y, ap, dx;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0,
          0.72f * entrada);

  dy = (1.0f - a) * 36.0f;
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido e UMA luz difusa na cor de
  // realce entrando pelo canto superior esquerdo, presa aos cantos do cartao
  // (GFX_LUZ). Com o veu de tela cheia ja pago, e a ultima camada grande daqui.
  { GfxRect p = { N11_X, N11_Y + dy, N11_W, N11_H };
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor(p, 28.0f / N11_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / N11_H, N11_H * 0.1f, -N11_H * 0.1f, N11_H * 0.65f, ar, ag, ab, 0.22f * a);
    gfx_recorte(N11_X, N11_Y + dy, N11_W, N11_H); }

  // A TROCA DE PAGINA E UM DESLIZE CURTO, na direcao da tecla: sem ele sete
  // paginas parecem a MESMA tela piscando texto diferente, e a pessoa perde a
  // nocao de que andou. Com "reduzir animacoes" o `passo` ja nasce em 1 (ver
  // ir()), entao dx e 0 e ap e `a`: a pagina troca seca, sem transparencia.
  { float s = anim_suave(passo);
    dx = (1.0f - s) * 44.0f * (float)(sentido >= 0 ? 1 : -1);
    ap = a * (0.30f + 0.70f * s); }

  // --- coluna da figura ------------------------------------------------------
  { float fx = N11_X + N11_PAD + dx, fy = N11_Y + dy + FIG_Y[pagina];
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.1"),
                             150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, N11_Y + dy + 64.0f, ap * 0.92f); }
    { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n(tituloPagina(pagina)),
                                   246, 247, 252, 255, N11_FIG_W);
      txt_desenhar_alpha(t, fx, N11_Y + dy + 96.0f, ap); }
    switch (pagina) {
      case 0:  figAgenda(fx, fy, ap, agora); break;
      case 1:  figListas(fx, fy, ap);        break;
      case 2:  figAudiencia(fx, fy, ap);     break;
      case 3:  figSocial(fx, fy, ap);        break;
      case 4:  figAtualizar(fx, fy, ap);     break;
      case 5:  figControle(fx, fy, ap);      break;
      default: figAparelhos(fx, fy, ap);     break;
    } }

  // --- coluna de recursos ----------------------------------------------------
  y = N11_Y + dy + 96.0f;
  { float fx = N11_TXT_X + dx, fw = N11_TXT_W;
    switch (pagina) {
      case 0:
        y += feature(fx, y, fw, "menu_agenda",
              "Uma linha do tempo",
              "As séries que você acompanha em ordem de estreia, com o dia, o "
              "episódio e quanto falta.", ap);
        y += feature(fx, y, fw, "lembrete",
              "Lembrar-me, na página da série",
              "O despertador ao lado de assistir marca o próximo episódio. "
              "Verde quando está ligado.", ap);
        y += feature(fx, y, fw, "oculto",
              "O aviso é dentro do app",
              "Nenhuma TV acorda um app fechado. No dia do episódio, o aviso "
              "aparece assim que você abrir o Nuvio.", ap);
        y += feature(fx, y, fw, "check",
              "O foco ficou mais claro",
              "O que está selecionado agora é preenchido, sem contorno, em "
              "toda a interface.", ap);
        break;
      case 1:
        y += feature(fx, y, fw, "menu_library",
              "Três fontes, um lugar",
              "As listas do Trakt, as pastas da sua conta Nuvio e o que você "
              "acompanha no Simkl.", ap);
        y += feature(fx, y, fw, "menu_search",
              "Procure a lista de qualquer pessoa",
              "As listas públicas do Trakt entram pela busca. Não precisa "
              "vincular conta.", ap);
        y += feature(fx, y, fw, "mais",
              "Fixe, ou mande para a Home",
              "Uma lista boa fica fixada na Biblioteca, ou vira uma fileira na "
              "tela inicial.", ap);
        y += feature(fx, y, fw, "aspecto",
              "Cartazes ou lista",
              "O seletor Exibição troca a grade por uma linha por título, e "
              "guarda a escolha.", ap);
        break;
      case 2:
        y += feature(fx, y, fw, "menu_profile",
              "Arco de qualidade",
              "A nota de cada episódio da temporada, em curva, direto do "
              "Trakt.", ap);
        y += feature(fx, y, fw, "naovisto",
              "Radar de desistência",
              "Quantas pessoas ainda marcam o episódio N depois de terem "
              "marcado o primeiro.", ap);
        y += feature(fx, y, fw, "legenda",
              "Frases marcantes",
              "As falas que ficaram, vindas do Wikiquote. Título sem página "
              "não mostra a seção.", ap);
        y += feature(fx, y, fw, "menu_guide",
              "Ficha de produção",
              "Orçamento, bilheteria, prêmios e onde foi filmado, tudo vindo "
              "do Wikidata.", ap);
        break;
      case 3:
        y += feature(fx, y, fw, "visto",
              "O app pergunta antes",
              "Na primeira vez que você abre a aba Social ele faz a pergunta, "
              "e \"não\" é a primeira opção.", ap);
        y += feature(fx, y, fw, "recomendar",
              "Só quem já te conhece",
              "Dizendo sim, você aparece para quem tem o seu contato ou te "
              "segue no Trakt. Para mais ninguém.", ap);
        y += feature(fx, y, fw, "oculto",
              "Ninguém vê o que você assiste",
              "Nome e foto, e nada mais. O que você assistiu, salvou ou "
              "recomendou não sai desta TV.", ap);
        y += feature(fx, y, fw, "menu_settings",
              "Mude quando quiser",
              "O interruptor fica no fim da aba Social. Um OK desliga.", ap);
        break;
      case 4:
        y += feature(fx, y, fw, "menu_settings",
              "O app avisa da versão nova",
              "Ele confere o GitHub com a tela inicial já de pé, e mostra o "
              "que mudou.", ap);
        y += feature(fx, y, fw, "addon",
              "Com o Homebrew Channel, ele se instala sozinho",
              "Atualizar agora entrega o pacote ao Homebrew Channel. A barra "
              "mostra o andamento até o fim.", ap);
        y += feature(fx, y, fw, "oculto",
              "Sem ele, o aviso e o endereço",
              "O Nuvio não roda como root e não alcança o instalador da TV. O "
              "cartão mostra onde baixar.", ap);
        y += feature(fx, y, fw, "pause",
              "No Samsung, sempre manual",
              "O .wgt vive dentro do navegador da TV, que não instala widget. "
              "Lá o cartão é só aviso.", ap);
        break;
      case 5:
        // AS FRASES SAO CURTAS porque txt_bloco corta na SEGUNDA linha.
        //
        // E elas dizem o que a tecla FAZ, nao quanta prova eu tenho de que ela
        // chega: o dono leu a primeira versao ("confirmado no Samsung; na LG a
        // tecla esta ligada, mas sem confirmacao") e disse que era coisa de
        // amador. Grau de evidencia e problema meu, nao de quem esta no sofa.
        y += feature(fx, y, fw, "menu_settings",
              "VERMELHO · o registro",
              "Abre o painel de log por cima de qualquer tela. É dali que sai "
              "o print quando alguma coisa dá errado.", ap);
        y += feature(fx, y, fw, "menu_library",
              "AZUL · Salvos e o guia",
              "Na LG abre Salvos, abre o guia com um canal no ar e volta do "
              "vídeo pequeno. No Samsung isso é o CANAL +.", ap);
        y += feature(fx, y, fw, "addon",
              "VERDE · só no Samsung",
              "Traz de volta o painel de diagnóstico do navegador, que fica "
              "fora do app.", ap);
        y += feature(fx, y, fw, "oculto",
              "AMARELO · livre",
              "Não faz nada nas duas. E se o seu controle não tem a fileira de "
              "cores, nada aqui depende dela.", ap);
        break;
      default:
        y += feature(fx, y, fw, "check",
              "webOS 4",
              "A referência do projeto: uma C9 de 2019, 60 quadros por segundo "
              "na tela inicial.", ap);
        y += feature(fx, y, fw, "play",
              "webOS 5 e mais novas",
              "A LG tirou a libAcbAPI na 5 e o app passou a usar a janela "
              "exportada do SDL. Roda até os aparelhos de 2024.", ap);
        // MESMO ICONE do webOS 5, porque agora e o MESMO grau de certeza.
        // Enquanto esta linha dizia "experimental" ela usava o icone de
        // "oculto", o mesmo do AMARELO que nao faz nada — com "relatado" no
        // titulo aquele icone passaria a contradizer o texto.
        y += feature(fx, y, fw, "play",
              "webOS 3",
              "Tem pacote próprio, do ramo webos3 — não é o mesmo arquivo. "
              "Essas TVs não têm H.265 nem HDR.", ap);
        y += feature(fx, y, fw, "aspecto",
              "Samsung Tizen",
              "O mesmo app em WebAssembly, com vídeo pelo AVPlay. Sem selo de "
              "HDR ou Dolby e sem o zoom que corta a barra preta.", ap);
        break;
    } }

  // --- rodape: onde estou e o que a tecla faz -------------------------------
  pontos(N11_TXT_X, N11_Y + dy + N11_H - 104.0f, a);
  { const char *dica = pagina < N11_PAGINAS - 1
        ? "OK ou → para continuar · Voltar fecha"
        : "OK para começar";
    TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n(dica),
                                 140, 144, 154, 255, N11_TXT_W);
    txt_desenhar_alpha(t, N11_TXT_X, N11_Y + dy + N11_H - 62.0f, a * 0.85f); }

  gfx_sem_recorte();
}
