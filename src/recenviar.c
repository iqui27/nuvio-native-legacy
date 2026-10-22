// Ver recenviar.h para por que estas telas sairam de dentro de ctxmenu.c.
//
// QUATRO TELAS NA MESMA ILHA, e nao quatro modulos: "Para quem?", "O que
// dizer?", o envio (enviando / enviado / falhou) e "Amigos" tem o mesmo
// material, o mesmo foco, o mesmo Voltar e a mesma lista rolavel. O que muda
// entre elas e a lista de linhas e o cabecalho — e a ilha MUDA DE TAMANHO com
// mola entre uma e outra (a pilula do relogio faz o mesmo), em vez de pular.
//
// O REDESENHO DA 2.0 (06/10/2026). A versao anterior era a pilha da 1.x: o
// logo solto no topo, a pergunta em cinza, linhas de botao primario (pilula
// cheia branca/acento) e o resultado do envio numa frase no rodape enquanto a
// tela voltava sozinha para a lista de amigos. Agora:
//   - ENVIO EM DUAS COLUNAS. A esquerda a ARTE do titulo (o fundo 16:9, com o
//     logo por cima) e, depois de escolhido, o amigo; a direita o passo. A
//     arte horizontal identifica a obra sem repetir a capa que a pessoa acabou
//     de ver no cartaz.
//   - AS LINHAS SAO AS DO MENU DO CARTAZ (ctxmenu.c, linhaCtx): sem miolo em
//     repouso, o foco e a superficie um degrau mais clara (plrui_linha_foco) e
//     o rotulo acende inteiro. A pilula cheia fica para botao.
//   - O AMIGO TEM ROSTO E NOME DE VERDADE: foto ou inicial (rec_avatar), o nome
//     e, embaixo, o @ do Trakt quando ha. "Amigo #343" e o nome que o servidor
//     inventa para quem nao escolheu nenhum (social.js, resolverNome) — aqui ele
//     vira "Amigo sem nome", vai para o fim da lista e o numero desce para a
//     linha de baixo, que e o que ainda separa dois sem nome.
//   - O RESULTADO FICA NA ILHA: girando, depois o visto no acento com a frase
//     que foi, e so entao ela fecha; o mesmo "Enviado para X" sai na ilha do
//     relogio (ilhaacao). Falha NAO fecha: diz o que fazer e o OK tenta de novo.
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
#include "logotitulo.h"
#include "plrui.h"
#include "ilha.h"
#include "ilhaacao.h"
#define NV_ESCALA_TELA_ATIVA   // mede pela tela do fator ativo (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// --- MEDIDAS -----------------------------------------------------------------
//
// O ENVIO E MAIS LARGO QUE A TELA DE AMIGOS porque leva duas colunas: a arte
// de 384 (16:9 = 216 de alto) e a lista de 576, onde cabe "Assiste isso hoje"
// em 24/600 com folga e um nome longo ainda corta so no fim. A tela de AMIGOS
// fica em 860: as seis caixas do codigo precisam de 646px para caber legiveis
// a tres metros, e a lista ali e de coluna unica.
#define RE_PAD        40.0f
#define RE_RAIO       32.0f
#define RE_W_ENVIO  1080.0f
#define RE_W_AMIGOS  860.0f
#define RE_ARTE_W    384.0f
#define RE_ARTE_H    216.0f
#define RE_ARTE_RAIO  20.0f
#define RE_COL_GAP    40.0f
#define RE_RW        (RE_W_ENVIO - 2.0f * RE_PAD - RE_ARTE_W - RE_COL_GAP)
#define RE_IW_AMIGOS (RE_W_AMIGOS - 2.0f * RE_PAD)
// Linhas: a do amigo leva o rosto de 52 (o .av das ilhas), a da frase e a
// .it do menu do cartaz (60) com um pouco mais de ar para 24/600.
#define RE_L_PESSOA   76.0f
#define RE_L_FRASE    64.0f
#define RE_L_GAP       6.0f
#define RE_AV         52.0f
// Cabecalho da coluna: kicker (15) + titulo 40/700, e a lista 100 abaixo do
// topo da coluna.
#define RE_CAB       100.0f
#define RE_RODAPE     64.0f   // do fim do corpo ao fim da ilha, sem o PAD
#define RE_JANELA      6      // linhas visiveis antes de a lista rolar
#define RE_JANELA_AM   5      // na tela de amigos, que ja leva o codigo
#define RE_SECAO      44.0f   // "SEUS AMIGOS" entre as acoes e os contatos

// Caixa de um caractere do codigo. Ver a mesma decisao em teclado.c: separado
// por caractere para poder ser DITADO ao telefone sem contar letra errada.
#define RE_COD_W     96.0f
#define RE_COD_H    116.0f
#define RE_COD_GAP   14.0f

#define RE_LOGO_W   300.0f
#define RE_LOGO_H    72.0f

enum { RE_PAG_CONTATOS = 0, RE_PAG_MODELOS, RE_PAG_AMIGOS, RE_PAG_ENVIO };
enum { RE_ENV_INDO = 1, RE_ENV_OK, RE_ENV_FALHA };
// O QUE CADA LINHA E. Contar e rotular saem de linhaTipo, e o desenho e o
// evento usam a mesma resposta: a linha extra de "Adicionar um amigo" (ou de
// "Tentar de novo" sem servico) nao tem de ser lembrada em dois lugares.
enum { RL_NADA = 0, RL_CONTATO, RL_ADICIONAR, RL_TENTAR, RL_FRASE,
       RL_TRAKT, RL_CODIGO, RL_AMIGO, RL_REENVIAR };

static int   aberto, pagina, foco, topo;
static float anim;
static int   temItem;              // 0 quando abriu direto na tela de amigos
static int   voltaParaContatos;    // "Amigos" veio de dentro do envio
static CatItem item;               // COPIA; ver recenviar.h
static RecContato ctts[REC_CONTATOS_MAX];
static int   nCtts;
static char  alvoId[96], alvoNome[64], alvoAvatar[256], alvoIdCor[96];
static int   focoContato;          // para onde o Voltar devolve o foco
static int   modeloEsc;            // a frase que foi (indice)
static int   envEstado;            // RE_ENV_* na pagina de envio
static int   esperandoEnvio;       // o resultado ainda nao chegou (ver atualizar)
static char  aviso[192];
static Uint32 fecharEm;
static int   confirmandoRemover = -1;   // indice do contato a remover, ou -1
// MOLA DO FOCO POR LINHA, como focoAnim em ctxmenu.c: o realce entra em ~120
// ms em vez de saltar. Uma posicao por linha possivel.
#define RE_MAXL (REC_CONTATOS_MAX + 4)
static float focoAnim[RE_MAXL];
// Molas da ilha: tamanho (a forma muda entre as telas), a troca de conteudo
// (entra 24 px de lado, como as folhas) e a rolagem da lista.
static float wAnim, hAnim, troca = 1.0f, rolar;
static int   trocaDir = 1, assentar_ = 1;
#ifdef NV_TOUCH_UI
static ToqueRolagem toque;
static float toquePasso;
static void toqueRetomar(void);
static int toqueRolar(const PonteiroRolagem *e) {
  PonteiroRolagem local = *e;
  if (!aberto || teclado_aberto() || pagina == RE_PAG_ENVIO || toquePasso <= 0.0f) return 0;
  local.delta /= toquePasso;
  return toquerol_evento(&toque, &local);
}
#endif

static int teclaOk(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

// --- O NOME QUE APARECE -------------------------------------------------------
//
// "Amigo #343" (ou "Friend #343", do rec_nome_exibicao local) e um NOME DE
// RESERVA: o servidor o monta com o rowid quando a conta nao tem nome de
// perfil nem de conta (servidor/recomendacoes/src/social.js, resolverNome). Na
// lista ele parecia o nome de alguem. 1 quando `nome` e esse formato; `num`
// recebe o numero.
static int casaReserva(const char *nome, const char *fmt, int *num) {
  const char *p = strstr(fmt, "%d");
  size_t n;
  int v = 0, dig = 0;
  if (!p) return 0;
  n = (size_t)(p - fmt);
  if (strncmp(nome, fmt, n)) return 0;
  nome += n;
  while (*nome >= '0' && *nome <= '9') { v = v * 10 + (*nome - '0'); nome++; dig++; }
  if (!dig || strcmp(nome, p + 2)) return 0;
  if (num) *num = v;
  return 1;
}
static int nomeDeReserva(const char *nome, int *num) {
  if (!nome || !nome[0]) return 1;
  return casaReserva(nome, "Amigo #%d", num) || casaReserva(nome, i18n("Amigo #%d"), num);
}

// O @ do Trakt: o proprio id ("trakt:<slug>") ou uma identidade ligada que o
// amigo deixou os amigos verem (RecContato.ids). "" quando nao ha.
static const char *slugTrakt(const RecContato *c) {
  int i;
  if (!strncmp(c->id, "trakt:", 6) && c->id[6]) return c->id + 6;
  for (i = 0; i < c->nIds && i < REC_CONTATO_IDS; i++)
    if (!strncmp(c->ids[i], "trakt:", 6) && c->ids[i][6]) return c->ids[i] + 6;
  return "";
}

// Linha principal e secundaria de um contato. A principal e o nome; sem nome
// de verdade, o @ do Trakt; sem os dois, "Amigo sem nome" com o numero
// embaixo. A secundaria e o @ quando ele diz algo que o nome nao diz.
static int nomesDe(const RecContato *c, char *nome, size_t tn, char *sub, size_t ts) {
  const char *slug = slugTrakt(c);
  int num = 0, reserva = nomeDeReserva(c->nome, &num);
  sub[0] = 0;
  if (!reserva) {
    snprintf(nome, tn, "%s", c->nome);
    if (slug[0] && strcasecmp(slug, c->nome)) snprintf(sub, ts, "@%s", slug);
    return 0;
  }
  if (slug[0]) { snprintf(nome, tn, "@%s", slug); return 0; }
  snprintf(nome, tn, "%s", i18n("Amigo sem nome"));
  if (num > 0) snprintf(sub, ts, "%s \xc2\xb7 #%d", i18n("Ainda não escolheu um nome"), num);
  else snprintf(sub, ts, "%s", i18n("Ainda não escolheu um nome"));
  return 1;
}

// QUEM TEM NOME VEM PRIMEIRO, na ordem do servidor (ORDER BY nome); os sem
// nome descem para o fim, na mesma ordem relativa. Estavel de proposito: a
// lista e relida a cada quadro e o foco nao pode trocar de pessoa sozinho.
static void ordenarContatos(void) {
  RecContato tmp[REC_CONTATOS_MAX];
  int i, k = 0, pass;
  if (nCtts < 2) return;
  for (pass = 0; pass < 2; pass++)
    for (i = 0; i < nCtts; i++) {
      int sem = nomeDeReserva(ctts[i].nome, NULL) && !slugTrakt(&ctts[i])[0];
      if (sem == pass) tmp[k++] = ctts[i];
    }
  memcpy(ctts, tmp, sizeof(RecContato) * (size_t)nCtts);
}

static void recarregarContatos(void) {
  nCtts = recomenda_contatos(ctts, REC_CONTATOS_MAX);
  ordenarContatos();
}

// SEM SERVICO: nenhum contato E nenhum codigo. O codigo vai para o disco na
// primeira resposta de /v1/eu, entao sem ele esta TV nunca falou com o
// servico — "Nenhum amigo ainda" ali mandaria trocar um codigo que nao existe.
static int semServico(void) {
  return nCtts == 0 && !recomenda_meu_codigo()[0];
}

int recenviar_aberto(void) { return aberto; }

static void abrirComum(void) {
  aberto = 1;
  foco = 0; topo = 0;
  alvoId[0] = 0; alvoNome[0] = 0; alvoAvatar[0] = 0; alvoIdCor[0] = 0;
  aviso[0] = 0; fecharEm = 0;
  envEstado = 0; focoContato = 0; modeloEsc = 0;
  confirmandoRemover = -1;
  memset(focoAnim, 0, sizeof focoAnim);
  troca = 1.0f; rolar = 0.0f; assentar_ = 1;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toque);
#endif
  recarregarContatos();
  // A lista ja esta no aparelho (recomenda.c a guarda do ultimo ciclo); o
  // pedido em paralelo e para o caso de um amigo ter entrado desde a ultima
  // sondagem de 60 s.
  recomenda_pedir_agora();
  if (!esperandoEnvio) recomenda_envio_limpar();
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

static int nLinhas(void) {
  if (pagina == RE_PAG_CONTATOS) return semServico() ? 2 : nCtts + 1;
  if (pagina == RE_PAG_MODELOS)  return REC_MODELOS;
  if (pagina == RE_PAG_ENVIO)    return envEstado == RE_ENV_FALHA ? 1 : 0;
  return 2 + nCtts;   // procurar no Trakt, digitar codigo, e cada contato
}

static int linhaTipo(int i) {
  if (i < 0 || i >= nLinhas()) return RL_NADA;
  if (pagina == RE_PAG_CONTATOS) {
    // Na tela de contatos a ULTIMA linha e sempre "Adicionar um amigo" —
    // inclusive (e principalmente) quando a lista esta vazia. Uma tela que so
    // diz "nao ha ninguem" e uma porta fechada.
    if (semServico()) return i == 0 ? RL_TENTAR : RL_ADICIONAR;
    return i < nCtts ? RL_CONTATO : RL_ADICIONAR;
  }
  if (pagina == RE_PAG_MODELOS) return RL_FRASE;
  if (pagina == RE_PAG_ENVIO)   return RL_REENVIAR;
  if (i == 0) return RL_TRAKT;
  if (i == 1) return RL_CODIGO;
  return RL_AMIGO;
}

// O contato da linha `i` desta pagina, ou NULL quando a linha e uma acao.
static const RecContato *contatoDaLinha(int i) {
  int t = linhaTipo(i);
  if (t == RL_CONTATO) return &ctts[i];
  if (t == RL_AMIGO && i - 2 < nCtts) return &ctts[i - 2];
  return NULL;
}

static float alturaLinha(int i) {
  return linhaTipo(i) == RL_FRASE ? RE_L_FRASE : RE_L_PESSOA;
}

static int janela(void) {
  return pagina == RE_PAG_AMIGOS ? RE_JANELA_AM : RE_JANELA;
}

static void ajustarJanela(void) {
  int n = nLinhas(), jan = janela();
  if (foco < 0) foco = 0;
  if (foco >= n) foco = n > 0 ? n - 1 : 0;
  if (foco < topo) topo = foco;
  if (foco >= topo + jan) topo = foco - jan + 1;
  if (topo > n - jan) topo = n - jan;
  if (topo < 0) topo = 0;
}

static void irPara(int pag, int novoFoco) {
  trocaDir = pag == RE_PAG_AMIGOS || pag > pagina ? 1 : -1;
  if (pagina == RE_PAG_AMIGOS && pag != RE_PAG_AMIGOS) trocaDir = -1;
  pagina = pag;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toque);
#endif
  foco = novoFoco; topo = 0;
  confirmandoRemover = -1;
  aviso[0] = 0;
  memset(focoAnim, 0, sizeof focoAnim);
  ajustarJanela();
  rolar = (float)topo;
  troca = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
}

// --- EVENTOS -----------------------------------------------------------------

static void enviar(void) {
  // O INDICE DO MODELO E O QUE VIAJA, nao a frase: assim a mesma
  // recomendacao chega em portugues numa TV e em ingles na outra.
  if (recomenda_enviar(&item, alvoId, modeloEsc, "")) {
    envEstado = RE_ENV_INDO;
    esperandoEnvio = 1;
  } else envEstado = RE_ENV_FALHA;
}

static void aplicar(void) {
  int t = linhaTipo(foco);
  switch (t) {
    case RL_CONTATO: {
      char sub[128];
      const RecContato *c = &ctts[foco];
      snprintf(alvoId, sizeof alvoId, "%s", c->id);
      snprintf(alvoAvatar, sizeof alvoAvatar, "%s", c->avatar);
      snprintf(alvoIdCor, sizeof alvoIdCor, "%s", c->id);
      nomesDe(c, alvoNome, sizeof alvoNome, sub, sizeof sub);
      focoContato = foco;
      irPara(RE_PAG_MODELOS, 0);
      return; }
    case RL_ADICIONAR:
      voltaParaContatos = 1;
      irPara(RE_PAG_AMIGOS, 0);
      return;
    case RL_TENTAR:
      recomenda_pedir_agora();
      snprintf(aviso, sizeof aviso, "%s", i18n("Aguarde..."));
      return;
    case RL_FRASE:
      modeloEsc = foco;
      irPara(RE_PAG_ENVIO, 0);
      enviar();
      return;
    case RL_REENVIAR:
      enviar();
      return;
    case RL_TRAKT:
      recomenda_procurar_trakt();
      return;
    case RL_CODIGO:
      teclado_abrir("Código do amigo",
                    "Seis letras ou números, como ele te passou.", 6);
      return;
    case RL_AMIGO: {
      int c = foco - 2;
      if (c < 0 || c >= nCtts) return;
      // DUAS CONFIRMACOES PARA APAGAR, e a segunda e a propria linha: um OK
      // distraido numa lista de nomes nao pode desfazer um vinculo que custou
      // um codigo ditado por telefone. O servidor apaga os DOIS lados.
      if (confirmandoRemover != c) { confirmandoRemover = c; return; }
      recomenda_remover_contato(ctts[c].id);
      confirmandoRemover = -1;
      recarregarContatos();
      ajustarJanela();
      return; }
    default: return;
  }
}

void recenviar_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return;
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN) return;
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) toqueRetomar();
#endif
  k = e->key.keysym.sym;
  // Repeticao automatica NUNCA e uma segunda escolha: quem quer clicar duas
  // vezes solta e aperta de novo. Sem isto, o OK ainda afundado que veio do
  // menu de contexto escolheria o primeiro amigo da lista sozinho.
  if (e->key.repeat && teclaOk(k)) return;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || k == SDLK_LEFT ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    // VOLTA UM PASSO, e nao fecha a modal: quem errou o amigo quer trocar de
    // amigo, nao recomecar do cartaz. E o foco volta para onde estava.
    if (confirmandoRemover >= 0) { confirmandoRemover = -1; return; }
    if (pagina == RE_PAG_ENVIO && envEstado == RE_ENV_FALHA) {
      irPara(RE_PAG_MODELOS, modeloEsc);
      return;
    }
    if (pagina == RE_PAG_MODELOS)  { irPara(RE_PAG_CONTATOS, focoContato); return; }
    if (pagina == RE_PAG_AMIGOS && voltaParaContatos) {
      voltaParaContatos = 0;
      recarregarContatos();
      irPara(RE_PAG_CONTATOS, nLinhas() - 1);
      return;
    }
    // Enviando, Voltar so fecha: o envio segue no fio de recomenda.c e o
    // resultado sai na ilha do relogio (ver recenviar_atualizar).
    aberto = 0;
    return;
  }
  if (pagina == RE_PAG_ENVIO && envEstado == RE_ENV_OK) {
    if (teclaOk(k)) aberto = 0;
    return;
  }
  if (k == SDLK_UP)   { foco--; confirmandoRemover = -1; ajustarJanela(); return; }
  if (k == SDLK_DOWN) { foco++; confirmandoRemover = -1; ajustarJanela(); return; }
  if (teclaOk(k)) { aplicar(); return; }
}

// --- GEOMETRIA ---------------------------------------------------------------
//
// UMA CONTA SO para o tamanho da ilha, usada pelo desenho, pela mola e pelo
// teste de captura. Tudo em unidades de layout, a partir do topo da ilha.
typedef struct {
  float w, h;          // a ilha
  float corpoH;        // do PAD de cima ao rodape
  float listaY;        // topo da lista, relativo ao topo da ilha
  float textoH;        // bloco de explicacao acima da lista (estados vazios)
} ReGeo;

static float listaAltura(int ini, int max) {
  int i, n = nLinhas();
  float h = 0.0f;
  for (i = ini; i < n && i - ini < max; i++) {
    if (i > ini) h += RE_L_GAP;
    if (pagina == RE_PAG_AMIGOS && i == 2 && nCtts > 0) h += RE_SECAO;
    h += alturaLinha(i);
  }
  return h;
}

static const char *textoVazio(void) {
  if (pagina != RE_PAG_CONTATOS) return NULL;
  if (semServico())
    return "Seus amigos aparecem aqui assim que a TV falar com o serviço.";
  if (nCtts == 0)
    return "Amigos do Trakt entram sozinhos, mas só depois de instalarem o app. Para os outros, troquem o código de 6 letras.";
  return NULL;
}

// Altura da coluna esquerda do envio: a arte, a linha do que o titulo e e,
// depois do passo do amigo, o cartao "Para Pedro".
static float colunaArteH(void) {
  float h = RE_ARTE_H + 14.0f + 26.0f;
  if (pagina == RE_PAG_MODELOS || pagina == RE_PAG_ENVIO) h += 24.0f + 72.0f;
  return h;
}

// Bloco do codigo na tela de amigos: kicker, caixas, as duas frases.
static float blocoCodigoH(void) {
  float h = 30.0f + RE_COD_H + 16.0f;
  h += txt_bloco_corta(TXT_ILHA_TEXTO, recomenda_meu_codigo()[0]
         ? "Dite este código ao seu amigo. Ele digita aqui e vocês dois viram contatos."
         : "Seu código aparece assim que a TV falar com o serviço.",
         0, 0, 0, 0, 0, RE_IW_AMIGOS, 30.0f, 0.0f, 2);
  h += 30.0f;    // "Amigos do Trakt entram sozinhos quando instalarem o app."
  return h;
}

static void medir(ReGeo *g) {
  float corpo;
  memset(g, 0, sizeof *g);
  if (pagina == RE_PAG_AMIGOS) {
    g->w = RE_W_AMIGOS;
    g->listaY = RE_PAD + RE_CAB + blocoCodigoH() + 24.0f;
    corpo = g->listaY - RE_PAD + listaAltura(topo, janela());
  } else {
    float col;
    const char *tv = textoVazio();
    g->w = RE_W_ENVIO;
    if (tv) g->textoH = txt_bloco_corta(TXT_ILHA_TEXTO, tv, 0, 0, 0, 0, 0, RE_RW, 30.0f, 0.0f, 3) + 28.0f;
    g->listaY = RE_PAD + RE_CAB + g->textoH;
    if (pagina == RE_PAG_ENVIO) {
      // A ALTURA DO PASSO DAS FRASES, que e de onde se chega aqui: o resultado
      // aparece no lugar da lista sem a ilha encolher no meio do gesto.
      col = RE_CAB + (float)REC_MODELOS * (RE_L_FRASE + RE_L_GAP) - RE_L_GAP;
    } else {
      int jan = janela();
      col = RE_CAB + g->textoH + listaAltura(topo, jan);
    }
    corpo = col > colunaArteH() ? col : colunaArteH();
  }
  if (aviso[0]) corpo += 44.0f;
  g->corpoH = corpo;
  g->h = RE_PAD + corpo + RE_RODAPE + RE_PAD - 14.0f;
}

// Para o teste de captura (zoom 130): a altura da ilha nesta pagina.
static float alturaCartao(void) {
  ReGeo g;
  medir(&g);
  return g.h;
}

// --- ATUALIZACAO -------------------------------------------------------------

// O RESULTADO DO ENVIO, aberta ou nao. Quem fecha com Voltar no meio do envio
// ainda fica sabendo: a frase sai na ilha do relogio.
static void resultadoEnvio(Uint32 agora) {
  int est;
  if (!esperandoEnvio) return;
  est = recomenda_envio_estado();
  if (est != REC_ENVIO_OK && est != REC_ENVIO_FALHA) return;
  esperandoEnvio = 0;
  recomenda_envio_limpar();
  if (est == REC_ENVIO_OK) {
    char frase[160];
    IlhaAcao ac;
    snprintf(frase, sizeof frase, i18n("Enviado para %s"), alvoNome);
    memset(&ac, 0, sizeof ac);
    ac.icone = "aj_send";
    ac.frase = frase;
    ac.titulo = item.titulo;
    ac.onde = i18n(recomenda_modelo(modeloEsc) ? recomenda_modelo(modeloEsc) : "");
    ac.thumb = item.poster;
    ac.arte = item.backdrop;
    ac.tipo = ILHA_OK;
    ilhaacao_feita(&ac);
    if (aberto && pagina == RE_PAG_ENVIO) {
      envEstado = RE_ENV_OK;
      fecharEm = agora + 1600;
    }
  } else {
    if (aberto && pagina == RE_PAG_ENVIO) {
      envEstado = RE_ENV_FALHA;
      foco = 0;
    } else
      ilha_avisar("recenviar-falha", ILHA_ERRO, NULL,
                  i18n("Não foi possível enviar. Tente novamente."), 5000, 0);
  }
}

void recenviar_atualizar(float dt, Uint32 agora) {
  int est, red = ajustes_animacoes_reduzidas();
  resultadoEnvio(agora);
  if (!aberto && anim < 0.002f) { anim = 0.0f; return; }
  anim = red ? (aberto ? 1.0f : 0.0f)
             : anim_mola(anim, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  teclado_atualizar(dt, agora);
  { int i;
    for (i = 0; i < RE_MAXL; i++)
      focoAnim[i] = red ? (aberto && foco == i ? 1.0f : 0.0f)
                        : anim_mola(focoAnim[i], aberto && foco == i ? 1.0f : 0.0f,
                                    dt, NV_MOLA_FOCO); }
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
  // e o transforma na frase curta acima do rodape.
  est = recomenda_vinculo_estado();
  if (est == REC_VINC_INDO) {
    snprintf(aviso, sizeof aviso, "%s", i18n("Procurando esse código..."));
  } else if (est == REC_VINC_OK) {
    const char *nome = recomenda_vinculo_nome();
    snprintf(aviso, sizeof aviso, i18n("%s agora é seu amigo"),
             nome[0] && !nomeDeReserva(nome, NULL) ? nome : i18n("Seu amigo"));
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
  // "Aguarde..." do Tentar de novo sai quando o servico responde.
  if (pagina == RE_PAG_CONTATOS && !semServico() && !strcmp(aviso, i18n("Aguarde...")))
    aviso[0] = 0;

  if (fecharEm && (Sint32)(agora - fecharEm) >= 0) { aberto = 0; fecharEm = 0; }
  ajustarJanela();

  { ReGeo g;
    medir(&g);
    if (assentar_ || red) { wAnim = g.w; hAnim = g.h; assentar_ = 0; }
    else {
      wAnim = anim_mola(wAnim, g.w, dt, NV_MOLA_FOCO * 0.6f);
      hAnim = anim_mola(hAnim, g.h, dt, NV_MOLA_FOCO * 0.6f);
    }
    troca = red ? 1.0f : anim_mola(troca, 1.0f, dt, NV_MOLA_FOCO * 0.5f);
#ifdef NV_TOUCH_UI
    if (!toque.livre)
#endif
      rolar = red ? (float)topo : anim_mola(rolar, (float)topo, dt, NV_MOLA_FOCO * 0.7f); }
}

// --- DESENHO -----------------------------------------------------------------

// A ilha: o material da folha de Fontes e do menu do cartaz (vidro com a
// opacidade e o Frost configurados, ou o solido equivalente).
static void ilhaMaterial(GfxRect p, float a) {
  float raio = RE_RAIO / (p.h > 1.0f ? p.h : 1.0f);
  if (ajustes_vidro()) {
    gfx_rect((GfxRect){p.x-18.0f,p.y-8.0f,p.w+36.0f,p.h+40.0f},
             0,GFX_SOMBRA,1.0f,0,0,.5f,0,0,0,.38f*a);
    gfx_vidro_folha(p,raio,a);
    gfx_luz_canto(p, raio, p.w * .22f, -p.h * .40f, p.w * .62f, 1, 1, 1, .06f * a);
  } else plrui_material(p, RE_RAIO, 1, a);
}

// O ROSTO de 52: a foto, ou o disco na cor do id com a inicial em 20/700
// (o .av das ilhas). Sem nome de verdade, "?" no lugar da inicial.
static void rosto(GfxRect r, const RecContato *c, float a) {
  char nome[64], sub[128];
  int reserva = nomesDe(c, nome, sizeof nome, sub, sizeof sub);
  rec_avatar_estilo(r, c->avatar, reserva ? "?" : (nome[0] == '@' ? nome + 1 : nome),
                    c->id, a, TXT_ILHA_INICIAL);
}

// O disco de uma ACAO, no lugar do rosto: o mesmo tamanho, branco a 8 % (o
// botao em repouso das ilhas) e o icone do Lucide no meio. Assim os rotulos de
// acao e os nomes alinham na mesma coluna.
static void discoAcao(GfxRect r, const char *icone, float lum, float a) {
  if (ajustes_vidro()) gfx_rect(r, 0, GFX_DISCO, 0, 0, 0, 0, 1, 1, 1, .08f * a);
  else gfx_rect(r, 0, GFX_DISCO, 0, 0, 0, 0, .141f, .149f, .173f, a);
  gfx_icone((GfxRect){ r.x + (r.w - 24.0f) * .5f, r.y + (r.h - 24.0f) * .5f, 24.0f, 24.0f },
            icone, .953f, .949f, .937f, lum * a);
}

// UMA LINHA. Repouso sem miolo; foco = plrui_linha_foco (a superficie do menu
// do cartaz) + o rotulo acende de .66 para 1 + a seta na ponta, que diz que o
// OK leva adiante. A linha cresce 4 px de cada lado com o foco: na TV, a tres
// metros, o que mexe e o que se ve primeiro.
static void desenhaLinha(int i, float x, float y, float w, float a) {
  int t = linhaTipo(i);
  float h = alturaLinha(i);
  float f = i < RE_MAXL ? focoAnim[i] : 0.0f;
  float k = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
  float lum = .66f + .34f * k;
  float cresce = 4.0f * k;
  GfxRect r = { x - cresce, y, w + 2.0f * cresce, h };
  float tx, fim = x + w - 20.0f;
  const RecContato *c = contatoDaLinha(i);
  int remover = t == RL_AMIGO && confirmandoRemover == i - 2;
  char nome[160], sub[160];
  const char *icone = NULL;
  nome[0] = sub[0] = 0;
  plrui_linha_foco(r, h * .5f, k * a);
  if (c) {
    GfxRect av = { x + 12.0f, y + (h - RE_AV) * .5f, RE_AV, RE_AV };
    rosto(av, c, (.85f + .15f * k) * a);
    nomesDe(c, nome, sizeof nome, sub, sizeof sub);
    if (remover) {
      char quem[64];
      snprintf(quem, sizeof quem, "%s", nome);
      snprintf(nome, sizeof nome, i18n("Remover %s? OK confirma"), quem);
      sub[0] = 0;
    }
    tx = av.x + RE_AV + 18.0f;
  } else if (t == RL_FRASE) {
    const char *m = recomenda_modelo(i);
    snprintf(nome, sizeof nome, "%s", m ? i18n(m) : "");
    gfx_icone((GfxRect){ x + 22.0f, y + (h - 22.0f) * .5f, 22.0f, 22.0f }, "aj_quote",
              .953f, .949f, .937f, (.30f + .45f * k) * a);
    tx = x + 22.0f + 22.0f + 18.0f;
  } else {
    const char *rot = "";
    switch (t) {
      case RL_ADICIONAR: rot = "Adicionar um amigo";              icone = "aj_user-plus"; break;
      case RL_TENTAR:    rot = "Tentar de novo";                  icone = "aj_rotate-cw"; break;
      case RL_REENVIAR:  rot = "Tentar de novo";                  icone = "aj_rotate-cw"; break;
      case RL_TRAKT:     rot = "Procurar amigos do Trakt agora";  icone = "aj_search"; break;
      case RL_CODIGO:    rot = "Digitar o código de um amigo";    icone = "aj_keyboard"; break;
      default: break;
    }
    snprintf(nome, sizeof nome, "%s", i18n(rot));
    { GfxRect d = { x + 12.0f, y + (h - RE_AV) * .5f, RE_AV, RE_AV };
      discoAcao(d, icone, lum, a);
      tx = d.x + RE_AV + 18.0f; }
  }
  // A SETA DO FOCO: so na linha em foco, e nunca na de remover (ali o OK
  // apaga, nao avanca).
  if (k > .01f && !remover && t != RL_AMIGO) {
    gfx_icone((GfxRect){ fim - 24.0f, y + (h - 24.0f) * .5f, 24.0f, 24.0f },
              t == RL_FRASE ? "aj_send" : "aj_chevron-right", .953f, .949f, .937f, k * .9f * a);
    fim -= 24.0f + 14.0f;
  }
  { int vr = remover ? 240 : 243, vg = remover ? 190 : 242, vb = remover ? 130 : 239;
    TxtLinha t1 = txt_linha_corta(TXT_ILHA_NOME, nome, vr, vg, vb, 255, fim - tx);
    if (sub[0]) {
      TxtLinha t2 = txt_linha_corta(TXT_ILHA_SUB, sub, 243, 242, 239, 255, fim - tx);
      float tot = (float)t1.h + (float)t2.h, oy = y + (h - tot) * .5f;
      txt_desenhar_alpha(t1, tx, oy, lum * a);
      txt_desenhar_alpha(t2, tx, oy + (float)t1.h, (.40f + .22f * k) * a);
    } else
      txt_desenhar_alpha(t1, tx, y + (h - (float)t1.h) * .5f, lum * a);
  }
}

// A LISTA, com a rolagem em mola e recortada na janela. A barra fina a direita
// so aparece quando ha mais linhas do que cabem.
// PONTEIRO (#99): foco pela MESMA variavel das setas (`foco`, com a janela
// acertada como no ↑ ↓); o OK do clique segue por recenviar_evento (aplicar).
// Idempotente; com o teclado aberto ou no resultado do envio nao mexe.
static void ponteiroLinha(int i, int b) {
  (void)b;
  if (!aberto || teclado_aberto() || pagina == RE_PAG_ENVIO) return;
  if (i < 0 || i >= nLinhas()) return;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toque);
#endif
  foco = i; confirmandoRemover = -1; ajustarJanela();
}
int recenviar_teste_foco(int *pag) { if (pag) *pag = pagina; return foco; }
#ifdef NV_TOUCH_UI
static void toqueRetomar(void) {
  if (toque.livre && nLinhas() > 0) {
    float ponto = rolar * toquePasso + toque.regiao.h * 0.35f, y = 0.0f;
    int i;
    for (i = 0; i + 1 < nLinhas(); i++) {
      if (pagina == RE_PAG_AMIGOS && i == 2 && nCtts > 0) y += RE_SECAO;
      if (y + alturaLinha(i) >= ponto) break;
      y += alturaLinha(i) + RE_L_GAP;
    }
    foco = i;
  }
  toquerol_limpar(&toque);
  ajustarJanela();
}
#endif

static void desenhaLista(float x, float y, float w, float a) {
  int i, n = nLinhas(), jan = janela();
  float visH = listaAltura(topo, jan);
  // So assentada: a entrada e a troca de passo paradas e a lista sem rolar.
  int reg = aberto && anim > 0.99f && anim < 1.01f && troca > 0.99f &&
            !teclado_aberto() &&
#ifdef NV_TOUCH_UI
            (toque.livre ||
#endif
            (rolar > (float)topo - 0.01f && rolar < (float)topo + 0.01f)
#ifdef NV_TOUCH_UI
            )
#endif
            ;
  float passo = (pagina == RE_PAG_MODELOS ? RE_L_FRASE : RE_L_PESSOA) + RE_L_GAP;
#ifdef NV_TOUCH_UI
  if (reg) {
    toquePasso = passo;
    toquerol_vincular(&toque, (GfxRect){ x, y, w, visH }, gfx_escala(), 0.0f,
                      fmaxf(0.0f, listaAltura(0, n) - visH) / passo, 1, &rolar);
    ponteiro_rolagem(toqueRolar);
  }
#endif
  float desl = (rolar - (float)topo) * passo;
  int rola = n > jan;
  if (rola) gfx_recorte(x - 12.0f, y - 4.0f, w + 24.0f, visH + 8.0f);
  { float yy = y - (float)topo * passo - desl;
    for (i = 0; i < n; i++) {
      float h = alturaLinha(i);
      if (pagina == RE_PAG_AMIGOS && i == 2 && nCtts > 0) {
        if (yy + RE_SECAO > y - passo && yy < y + visH + passo)
          plrui_kicker("Seus amigos", x + 12.0f, yy + 12.0f, 243, 242, 239, .45f * a);
        yy += RE_SECAO;
      }
      if (yy + h >= y - passo && yy <= y + visH + passo) {
        if (reg) ponteiro_alvo_faixa(x, yy, w, h, y - 4.0f, y + visH + 4.0f, ponteiroLinha, NULL, i, 0);
        desenhaLinha(i, x, yy, w, a);
      }
      yy += h + RE_L_GAP;
    } }
  if (rola) {
    float trilhoX = x + w + 14.0f;
    float frac = (float)jan / (float)n;
    float th = visH * frac;
    float ty = y + (visH - th) * (rolar / (float)(n - jan));
    gfx_sem_recorte();
    gfx_cor((GfxRect){ trilhoX, y, 4.0f, visH }, .5f, 1, 1, 1, .08f * a);
    gfx_cor((GfxRect){ trilhoX, ty, 4.0f, th }, .5f, 1, 1, 1, .45f * a);
  }
}

// As seis caixas do codigo, alinhadas a esquerda como o resto do cartao.
static float desenhaCodigo(float x, float y, float larg, float a) {
  const char *cod = recomenda_meu_codigo();
  int i, n = 6;
  float bw = RE_COD_W;
  if (bw * (float)n + RE_COD_GAP * (float)(n - 1) > larg)
    bw = (larg - RE_COD_GAP * (float)(n - 1)) / (float)n;
  for (i = 0; i < n; i++) {
    GfxRect b = { x, y, bw, RE_COD_H };
    char ch[2];
    // O RAIO DO gfx_cor E FRACAO DA ALTURA, nao pixel. Um "14" aqui viraria
    // uma pilula de 116px de alto — e foi assim que a primeira versao deste
    // recurso saiu na foto.
    if (ajustes_vidro()) gfx_vidro_painel(b, 14.0f / RE_COD_H, 0.55f, a);
    else gfx_cor(b, 14.0f / RE_COD_H, 0.141f, 0.149f, 0.173f, a);
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

// A ARTE DO TITULO: o fundo 16:9 (ou o cartaz, recortado) com o veu na base e
// o logo por cima — ou o nome, enquanto o logo nao chega ou nao existe. E o
// "o que" do gesto inteiro, entao fica parada nos dois passos.
static void desenhaArte(float x, float y, float a) {
  GfxRect r = { x, y, RE_ARTE_W, RE_ARTE_H };
  float raio = RE_ARTE_RAIO / RE_ARTE_H;
  const char *u = item.backdrop[0] && !tex_falhou(item.backdrop) ? item.backdrop : item.poster;
  GLuint tex = u[0] ? tex_obter_larg(u, RE_ARTE_W) : 0;
  if (tex) {
    // COVER SEMPRE: sem fundo, o cartaz retrato e recortado no meio em vez de
    // ficar espremido numa tarja escura dentro do 16:9.
    gfx_tex_aspect_atual = tex_aspecto(u);
    gfx_card_forcar_cover_atual = 1.0f;
    gfx_rect(r, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
    gfx_card_forcar_cover_atual = 0.0f;
    gfx_tex_aspect_atual = 0.0f;
  } else gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
  gfx_veu_base(r, raio, .78f, .82f * a);
  logotitulo_desenhar(&item, item.titulo, TXT_ILHA_NOME, x + 20.0f,
                      y + RE_ARTE_H - 18.0f - RE_LOGO_H * .78f, RE_LOGO_W * .78f,
                      RE_LOGO_H * .78f, RE_ARTE_W - 40.0f, a);
  // "Serie · 2024 · 56 min" embaixo da arte, como no menu do cartaz.
  { char meta[220];
    const char *tp = !strcmp(item.tipo, "movie") ? i18n("Filme")
                   : !strcmp(item.tipo, "series") ? i18n("Série") : "";
    snprintf(meta, sizeof meta, "%s%s%s", tp, tp[0] && item.meta[0] ? " \xc2\xb7 " : "", item.meta);
    if (meta[0]) {
      TxtLinha m = txt_linha_corta(TXT_ILHA_APOIO, meta, 243, 242, 239, 255, RE_ARTE_W);
      txt_desenhar_alpha(m, x + 2.0f, y + RE_ARTE_H + 14.0f, .5f * a);
    } }
}

// O AMIGO ESCOLHIDO, embaixo da arte, a partir do passo da frase: "Para" e o
// nome ao lado do rosto. E a resposta do passo anterior continuando na tela —
// o Voltar troca.
static void desenhaPara(float x, float y, float a) {
  GfxRect r = { x, y, RE_ARTE_W, 72.0f };
  char linha[128];
  if (ajustes_vidro()) gfx_cor(r, .5f, 1, 1, 1, .06f * a);
  else gfx_cor(r, .5f, .118f, .122f, .141f, a);
  rec_avatar_estilo((GfxRect){ x + 10.0f, y + 10.0f, RE_AV, RE_AV }, alvoAvatar,
                    alvoNome[0] == '@' ? alvoNome + 1 : alvoNome, alvoIdCor, a, TXT_ILHA_INICIAL);
  snprintf(linha, sizeof linha, i18n("Para %s"), alvoNome);
  { TxtLinha t = txt_linha_corta(TXT_ILHA_NOME, linha, 243, 242, 239, 255,
                                 RE_ARTE_W - RE_AV - 10.0f - 18.0f - 24.0f);
    txt_desenhar_alpha(t, x + 10.0f + RE_AV + 18.0f, y + (72.0f - (float)t.h) * .5f, a); }
}

// Os dois passos do envio, em dois tracos no canto do kicker: o de agora
// cheio, o outro apagado. Diz "falta uma escolha" sem escrever "1 de 2".
static void desenhaPassos(float xDir, float y, float a) {
  int p = pagina == RE_PAG_CONTATOS ? 0 : 1, i;
  for (i = 0; i < 2; i++) {
    GfxRect s = { xDir - (float)(2 - i) * 34.0f + 6.0f, y, 28.0f, 5.0f };
    gfx_cor(s, .5f, 1, 1, 1, (i <= p ? .80f : .16f) * a);
  }
}

// O RESULTADO no lugar da lista: girando, o visto no acento, ou o aviso de
// falha com o que fazer. Centrado na altura do passo das frases.
static void desenhaEnvio(float x, float y, float w, float h, float a, Uint32 agora) {
  float cx = x + w * .5f, cy = y + h * .40f;
  char buf[200];
  if (envEstado == RE_ENV_INDO) {
    plrui_anel(cx, cy - 20.0f, 64.0f, 0, agora, a);
    snprintf(buf, sizeof buf, i18n("Enviando para %s..."), alvoNome);
    { TxtLinha t = txt_linha_corta(TXT_ILHA_NOME, buf, 243, 242, 239, 255, w);
      txt_desenhar_alpha(t, cx - (float)t.w * .5f, cy + 40.0f, .8f * a); }
    return;
  }
  if (envEstado == RE_ENV_OK) {
    float ar, ag, ab, ti;
    GfxRect d = { cx - 48.0f, cy - 96.0f, 96.0f, 96.0f };
    ajustes_acento(&ar, &ag, &ab);
    ti = (float)ajustes_tinta_foco() / 255.0f;
    gfx_rect((GfxRect){ d.x - 24.0f, d.y - 8.0f, d.w + 48.0f, d.h + 48.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, .5f, ar, ag, ab, .30f * a);
    gfx_rect(d, 0, GFX_DISCO, 0, 0, 0, 0, ar, ag, ab, a);
    gfx_icone((GfxRect){ d.x + 26.0f, d.y + 26.0f, 44.0f, 44.0f }, "aj_check", ti, ti, ti, a);
    snprintf(buf, sizeof buf, i18n("Enviado para %s"), alvoNome);
    { TxtLinha t = txt_linha_corta(TXT_ILHA_PERGUNTA, buf, 243, 242, 239, 255, w);
      txt_desenhar_alpha(t, cx - (float)t.w * .5f, cy + 24.0f, a); }
    { const char *m = recomenda_modelo(modeloEsc);
      if (m) {
        snprintf(buf, sizeof buf, "\xe2\x80\x9c%s\xe2\x80\x9d", i18n(m));
        TxtLinha t = txt_linha_corta(TXT_ILHA_TEXTO, buf, 243, 242, 239, 255, w);
        txt_desenhar_alpha(t, cx - (float)t.w * .5f, cy + 84.0f, .62f * a);
      } }
    return;
  }
  // FALHA: o icone num disco apagado, a frase e o que fazer; a linha de
  // "Tentar de novo" vem embaixo, ja em foco.
  { GfxRect d = { x, y + 4.0f, 72.0f, 72.0f };
    if (ajustes_vidro()) gfx_rect(d, 0, GFX_DISCO, 0, 0, 0, 0, 1, 1, 1, .08f * a);
    else gfx_rect(d, 0, GFX_DISCO, 0, 0, 0, 0, .141f, .149f, .173f, a);
    gfx_icone((GfxRect){ d.x + 20.0f, d.y + 20.0f, 32.0f, 32.0f }, "aj_triangle-alert",
              .94f, .745f, .51f, a);
    txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_NOME, "Não foi possível enviar agora.",
                                       243, 242, 239, 255, w - 96.0f),
                       x + 96.0f, y + 8.0f, a);
    txt_bloco_corta(TXT_ILHA_TEXTO, "Confira a internet e tente de novo.", 243, 242, 239,
                    x + 96.0f, y + 44.0f, w - 96.0f, 30.0f, .62f * a, 2);
    desenhaLinha(0, x, y + 120.0f, w, a); }
}

static void recenviar_desenharCorpo_(Uint32 agora);
// Cartao de tela quase cheia: ampliado so se ainda couber (escala.h).
void recenviar_desenhar(Uint32 agora) {
  ESCALA_SE_COUBER_INI(RE_W_ENVIO, alturaCartao());
  recenviar_desenharCorpo_(agora);
  ESCALA_SE_COUBER_FIM();
}

static void recenviar_desenharCorpo_(Uint32 agora) {
  float a = anim_suave(anim), ac, x, y, cx, cy, sx, sy;
  ReGeo g;
  if (anim < 0.01f) return;
  medir(&g);
  if (wAnim < 1.0f || hAnim < 1.0f) { wAnim = g.w; hAnim = g.h; }

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0,
          (ajustes_vidro() ? 0.34f : 0.46f) * anim);
  // A ILHA no tamanho da mola, centrada; entra subindo 40 px.
  cx = NV_TELA_W * 0.5f;
  cy = NV_TELA_H * 0.5f + (1.0f - a) * 40.0f;
  sx = cx - wAnim * 0.5f; sy = cy - hAnim * 0.5f;
  ilhaMaterial((GfxRect){ sx, sy, wAnim, hAnim }, a);

  // O CONTEUDO segue o tamanho-ALVO, ancorado no centro da ilha: durante a
  // mola a forma alcanca o conteudo, que entra com a troca (24 px de lado).
  ac = a * anim_suave(troca);
  x = cx - g.w * 0.5f + (1.0f - troca) * 24.0f * (float)trocaDir;
  y = cy - g.h * 0.5f;

  if (pagina == RE_PAG_AMIGOS) {
    float lx = x + RE_PAD, ly = y + RE_PAD, cyy;
    plrui_kicker("Amigos", lx, ly + 2.0f, 243, 242, 239, .45f * ac);
    txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_TITULO,
                         temItem || voltaParaContatos ? "Adicionar um amigo" : "Seus amigos",
                         243, 242, 239, 255, RE_IW_AMIGOS),
                       lx, ly + 26.0f, ac);
    cyy = ly + RE_CAB;
    plrui_kicker("Seu código", lx, cyy, 243, 242, 239, .45f * ac);
    cyy += 30.0f;
    cyy += desenhaCodigo(lx, cyy, RE_IW_AMIGOS, ac) + 16.0f;
    // EM BLOCO: em ingles a frase e mais longa que em portugues, e uma linha
    // cortada perde o fim.
    cyy += txt_bloco_corta(TXT_ILHA_TEXTO,
        recomenda_meu_codigo()[0]
          ? "Dite este código ao seu amigo. Ele digita aqui e vocês dois viram contatos."
          // SEM CODIGO AINDA NAO E ERRO: /v1/eu ainda nao respondeu, e isso e
          // normal nos primeiros segundos do arranque ou com a TV sem rede.
          : "Seu código aparece assim que a TV falar com o serviço.",
        243, 242, 239, lx, cyy, RE_IW_AMIGOS, 30.0f, .78f * ac, 2);
    txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_TEXTO,
                         "Amigos do Trakt entram sozinhos quando instalarem o app.",
                         243, 242, 239, 255, RE_IW_AMIGOS),
                       lx, cyy, .45f * ac);
    desenhaLista(lx, y + g.listaY, RE_IW_AMIGOS, ac);
  } else {
    float lx = x + RE_PAD, ly = y + RE_PAD;
    float rx = lx + RE_ARTE_W + RE_COL_GAP;
    const char *titulo = pagina == RE_PAG_MODELOS ? "O que dizer?" : "Para quem?";
    const char *tv = textoVazio();
    // A coluna da arte nao troca entre os passos (fica fora da troca): so o
    // cartao "Para" entra.
    desenhaArte(lx, ly, a);
    if (pagina == RE_PAG_MODELOS || pagina == RE_PAG_ENVIO)
      desenhaPara(lx, ly + RE_ARTE_H + 14.0f + 26.0f + 24.0f, ac);

    // No resultado o titulo e o proprio resultado (desenhaEnvio).
    if (pagina == RE_PAG_ENVIO) titulo = NULL;
    if (pagina == RE_PAG_CONTATOS && semServico()) titulo = "Sem conexão com o serviço";
    else if (pagina == RE_PAG_CONTATOS && nCtts == 0) titulo = "Nenhum amigo ainda";
    plrui_kicker("Recomendar a um amigo", rx, ly + 2.0f, 243, 242, 239, .45f * ac);
    if (pagina != RE_PAG_ENVIO) desenhaPassos(rx + RE_RW, ly + 8.0f, ac);
    if (titulo)
      txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_TITULO, titulo, 243, 242, 239, 255, RE_RW),
                         rx, ly + 26.0f, ac);
    if (tv)
      txt_bloco_corta(TXT_ILHA_TEXTO, tv, 243, 242, 239, rx, ly + RE_CAB, RE_RW, 30.0f,
                      .62f * ac, 3);
    if (pagina == RE_PAG_ENVIO) {
      float top = envEstado == RE_ENV_FALHA ? ly + 40.0f : ly + 26.0f;
      desenhaEnvio(rx, top, RE_RW, ly + g.corpoH - top, ac, agora);
    } else
      desenhaLista(rx, y + g.listaY, RE_RW, ac);
  }

  // O AVISO (vinculo, Trakt, "Aguarde..."), acima do rodape.
  if (aviso[0]) {
    TxtLinha t = txt_linha_corta(TXT_ILHA_APOIO, aviso, 243, 242, 239, 255,
                                 g.w - 2.0f * RE_PAD);
    txt_desenhar_alpha(t, x + RE_PAD, y + RE_PAD + g.corpoH - 30.0f, .85f * ac);
  }
  { const char *teclas[] = { "↑ ↓", "OK", "Voltar" };
    // "Fechar" onde o Voltar fecha a ilha; "Voltar" onde ele volta um passo.
    int volta = pagina == RE_PAG_MODELOS ||
                (pagina == RE_PAG_AMIGOS && voltaParaContatos) ||
                (pagina == RE_PAG_ENVIO && envEstado == RE_ENV_FALHA);
    const char *rotulos[] = { "Navegar",
                              pagina == RE_PAG_MODELOS ? "Enviar" : "Selecionar",
                              volta ? "Voltar" : "Fechar" };
    int n = 3, i0 = 0;
    // No resultado nao ha o que navegar: so o Voltar (e o OK que fecha).
    if (pagina == RE_PAG_ENVIO && envEstado != RE_ENV_FALHA) { i0 = 2; n = 1; }
    plrui_dicas(teclas + i0, rotulos + i0, n, x + RE_PAD,
                y + g.h - RE_PAD - 4.0f, 0, a); }

  // O teclado fica POR CIMA desta modal, e nao no lugar dela: ele e uma
  // pergunta curta sobre a tela que continua valendo atras.
  teclado_desenhar(agora);
}
