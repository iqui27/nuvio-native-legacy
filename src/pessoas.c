// Ver pessoas.h. Uma ilha (o material de plrui.h), sete paginas.
//
// O MODELO: a pagina diz QUE linhas existem (montar()), o desenho pinta cada
// TIPO de linha de um jeito so e o OK despacha pelo codigo da linha. Nada aqui
// guarda dado de terceiro alem do que recomenda.c entrega por copia — e nada
// aqui chega perto da rede.
//
// O REDESENHO DE 06/10/2026 (dono: "a UX ta pessima", tem de mostrar o NOME e o
// APELIDO, e o criador do app com um selo). O que mudou e por que:
//  - A PESSOA E UMA LINHA COM ACAO. Antes a lista so abria o cartao, e o
//    "Pedir amizade" morava um OK e uma pagina adiante; agora cada linha tem a
//    sua UNICA acao a direita (Adicionar / Aceitar) e o estado quando nao ha o
//    que fazer (Pedido enviado / Amigos). O cartao continua a um ← de distancia.
//  - O OK SEMPRE RESPONDE NA PROPRIA LINHA: a pilula vira "Enviando…" com o
//    anel, depois "Pedido enviado" aceso no acento; falha vira "Tentar de novo"
//    ali mesmo, com a frase do erro no rodape.
//  - A BUSCA E UM CAMPO, nao um item de menu, e os pedidos que chegaram ficam
//    no topo da primeira pagina (eram um numero entre parenteses).
//  - CARREGANDO, VAZIO E ERRO SAO ESTADOS DA PAGINA (esqueleto, ou icone +
//    frase + o que fazer), e nao uma linha de rodape que some em 4 s.
#include "pessoas.h"
#include "recomenda.h"
#include "recenviar.h"
#include "teclado.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "tex_cache.h"
#include "botoes.h"
#include "badges.h"
#include "plrui.h"
#define NV_ESCALA_TELA_ATIVA   // mede pela tela do fator ativo (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#include <stdio.h>
#include <string.h>

#define PE_W         1040.0f
#define PE_PAD         56.0f
#define PE_INTERNO   (PE_W - PE_PAD * 2.0f)
#define PE_RAIO        36.0f
#define PE_MARGEM      64.0f   // da ilha a borda de cima/baixo da tela
#define PE_RODAPE      92.0f   // aviso (32) + dicas (36) + ar
#define PE_MAXL        64      // comunidade: REC_COMUNIDADE_MAX + secoes e acoes
// A linha mora na ilha com 18 px de recuo dos dois lados do texto: a superficie
// do foco passa 18 px para fora da coluna, como na folha de Fontes.
#define PE_RECUO       18.0f
#define PE_H_PESSOA    96.0f
#define PE_H_NAV       80.0f
#define PE_H_BUSCA     80.0f
#define PE_H_SECAO     58.0f
#define PE_H_VAZIO    236.0f
#define PE_GAP          6.0f
#define PE_AV          60.0f
#define PE_AV_CARTAO  128.0f
#define PE_SW_W        64.0f
#define PE_SW_H        34.0f
#define PE_FEITO_MS  2600u     // quanto o "acabou de dar certo" fica aceso

static int pagina;

enum { PG_MENU = 0, PG_LISTA, PG_CARTAO, PG_PEDIDOS, PG_BLOQ, PG_PERFIL, PG_GEN };

// O que cada linha faz ao OK.
enum { A_NADA = 0,
       A_BUSCAR, A_GOSTO, A_PEDIDOS, A_PERFIL, A_BLOQUEADOS,
       A_COMUNIDADE, A_MAIS, A_CODIGO,           // a lista da comunidade e "Ver mais"
       A_PESSOA,                       // abre o cartao de quem esta na linha
       A_PEDIR, A_CANCELAR, A_ACEITAR, A_RECUSAR, A_BLOQUEAR, A_DESBLOQ,
       A_P_PESQ, A_P_APELIDO, A_P_BIO, A_P_GEN, A_P_FOTO, A_P_REC, A_P_ATIV,
       A_P_APAGAR,
       A_GEN,                          // alterna o genero de indice `n`
       A_REPETIR };                    // refaz a ultima busca/lista que falhou

// Como a linha e desenhada. So T_BUSCA, T_PESSOA e T_NAV recebem foco.
enum { T_NAV = 0, T_BUSCA, T_PESSOA, T_SECAO, T_VAZIO, T_ESQ };

typedef struct {
  int  tipo, acao, n;
  char rot[96];
  char sub[160];
  const char *icone;       // T_NAV/T_VAZIO: nome de art/icones; NULL = sem
  int  sw;                 // -1 sem interruptor; 0/1
  int  perigo;             // 1 = rotulo no tom de alerta (bloquear, apagar)
  RecPessoa p;             // T_PESSOA (e o disco de T_NAV com temPessoa)
  int  temPessoa;
  char pub[16];            // alvo de A_DESBLOQ
  char dica[8];            // T_PESSOA: "#abcd" quando outra linha tem o mesmo nome
} Linha;

static int   aberto, foco, coluna = 1;   // coluna 1 = a pilula de acao da linha
static float anim, rolagem, rolagemAlvo;
#ifdef NV_TOUCH_UI
static ToqueRolagem toque;
static void toqueRetomar(void);
static int toqueRolar(const PonteiroRolagem *e) {
  if (!aberto || teclado_aberto()) return 0;
  int r = toquerol_evento(&toque, e);
  if (r) rolagemAlvo = rolagem;
  return r;
}
#endif
static Linha linhas[PE_MAXL];
static float linhaY[PE_MAXL];            // topo de cada linha dentro da lista
static float conteudoH;
static int   nL;
static float focoAnim[PE_MAXL], colAnim[PE_MAXL];
static char  aviso[220];
static int   avisoErro;
static Uint32 avisoAte;
static int   opAtual;             // A_* em andamento na rede, ou 0
static char  acaoPub[16];         // de quem e a acao em andamento/que falhou
static int   acaoFalhou;          // 1 = a ultima acao em acaoPub falhou
static char  feitoPub[16];        // quem acabou de mudar (a pilula acende)
static Uint32 feitoAte;
static int   confirmando = -1;    // indice de linha esperando o segundo OK
static RecPerfil rasc;            // rascunho de "Meu perfil"
static int   rascVivo;
static char  cartaoPub[16];
static int   voltaPara = -1;      // pagina de onde o cartao veio
static int   tecladoPara;         // 0 nada, A_BUSCAR, A_P_APELIDO, A_P_BIO
static int   focoVolta = -1;      // linha da lista de onde o cartao foi aberto
// A LISTA TEM ESTADO PROPRIO: a origem que foi pedida (1 busca, 2 gosto, 3
// comunidade), se ainda esta vindo e se falhou — os achados de recomenda.c sao
// os da ULTIMA resposta boa, e mostrar os velhos enquanto os novos nao chegam
// leria como "a busca deu isto".
static int   listaOrigem, listaCarregando, listaErro;
static char  ultimaBusca[32];

static const char *ALFA_TEXTO = "abcdefghijklmnopqrstuvwxyz0123456789-";

static int teclaOk(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

int pessoas_aberto(void) { return aberto; }

static void dizer(const char *s, Uint32 agora, unsigned ms) {
  snprintf(aviso, sizeof aviso, "%s", s);
  avisoErro = 0;
  avisoAte = ms ? agora + ms : 0;
}
// O erro fica mais: e ele que diz o que fazer.
static void dizerErro(const char *s, Uint32 agora) {
  dizer(s, agora, 7000);
  avisoErro = 1;
}

static void carregarRascunho(void) {
  RecPerfil p;
  recomenda_perfil(&p);
  // Publicado: a verdade e a do aparelho. Nao publicado: o rascunho de memoria
  // (o que ela digitou e ainda nao ligou) so perde o que o aparelho ja zerou.
  if (p.publicado || !rascVivo) rasc = p;
  else rasc.ativ = p.ativ;
  rascVivo = 1;
}

static void irPara(int pg) {
  pagina = pg;
  foco = 0; coluna = 1;
  rolagem = rolagemAlvo = 0.0f;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toque);
#endif
  confirmando = -1;
  memset(focoAnim, 0, sizeof focoAnim);
  memset(colAnim, 0, sizeof colAnim);
}

static void abrirComum(void) {
  aberto = 1;
  aviso[0] = 0; avisoAte = 0; avisoErro = 0;
  opAtual = 0; confirmando = -1; voltaPara = -1; tecladoPara = 0;
  acaoPub[0] = 0; acaoFalhou = 0; feitoPub[0] = 0;
  listaOrigem = 0; listaCarregando = 0; listaErro = 0;
  recomenda_soc_limpar();
  // O ciclo pedido aqui tambem rele os pedidos de amizade: e assim que eles
  // aparecem no topo da primeira pagina sem a pessoa entrar em "Pedidos".
  recomenda_pedir_agora();
  carregarRascunho();
}

int pessoas_abrir(void) {
  if (!recomenda_ativo()) return 0;
  abrirComum();
  irPara(PG_MENU);
  return 1;
}

int pessoas_abrir_perfil(void) {
  if (!recomenda_ativo()) return 0;
  abrirComum();
  irPara(PG_PERFIL);
  return 1;
}

// --- rascunho e publicacao -----------------------------------------------------

// Publicado, cada mudanca vai ao servidor na hora; nao publicado, so muda o
// rascunho. `ativ = -1` diz a recomenda_perfil_publicar "nao mexa no nivel".
static void aplicarRascunho(void) {
  if (!rasc.publicado) return;
  { RecPerfil p = rasc;
    p.ativ = -1;
    if (!recomenda_perfil_publicar(&p)) rasc.publicado = 0; }
}

int pessoas_definir_pesquisavel(int ligado) {
  if (!recomenda_ativo()) return 0;
  carregarRascunho();
  if (!ligado) {
    recomenda_perfil_despublicar();
    rasc.publicado = 0;
    return 1;
  }
  if (strlen(rasc.apelido) < 2) {
    // SEM APELIDO NAO LIGA: e o unico nome que estranhos veem, e a pessoa tem de
    // escolhe-lo. O caminho e abrir "Meu perfil" e pedir.
    pessoas_abrir_perfil();
    tecladoPara = A_P_APELIDO;
    teclado_abrir_com("Apelido",
                      "Como as pessoas vão te achar. Use - entre as palavras.",
                      REC_APELIDO_MAX, ALFA_TEXTO, NULL);
    return 0;
  }
  rasc.publicado = 1;
  { RecPerfil p = rasc; p.ativ = -1;
    if (!recomenda_perfil_publicar(&p)) { rasc.publicado = 0; return 0; } }
  return 1;
}

// --- linhas de cada pagina --------------------------------------------------------

static Linha *nova(int tipo, int acao, const char *rot) {
  Linha *l;
  if (nL >= PE_MAXL) return NULL;
  l = &linhas[nL++];
  memset(l, 0, sizeof *l);
  l->tipo = tipo; l->acao = acao; l->sw = -1;
  snprintf(l->rot, sizeof l->rot, "%s", rot ? rot : "");
  return l;
}
static Linha *navL(int acao, const char *icone, const char *rot, const char *sub) {
  Linha *l = nova(T_NAV, acao, rot);
  if (l) { l->icone = icone; snprintf(l->sub, sizeof l->sub, "%s", sub ? i18n(sub) : ""); }
  return l;
}
static void secao(const char *rot, int n) {
  Linha *l = nova(T_SECAO, A_NADA, rot);
  if (l) l->n = n;
}
static void vazio(const char *icone, const char *titulo, const char *texto) {
  Linha *l = nova(T_VAZIO, A_NADA, titulo);
  if (l) { l->icone = icone; snprintf(l->sub, sizeof l->sub, "%s", i18n(texto)); }
}

// A frase de CONTEXTO da pessoa, depois do @apelido: o que ajuda a reconhecer
// alguem que nao se conhece pelo nome. A relacao NAO entra aqui — ela e a
// pilula da direita, e dize-la duas vezes era o que deixava a linha confusa.
static void contexto(char *dst, size_t tam, const RecPessoa *p) {
  // "Gosto parecido": o servidor diz POR QUE sugeriu (p->motivo). Servidor
  // antigo nao manda o campo e cai nas regras de baixo, como sempre.
  if (p->motivo == REC_MOTIVO_GENEROS && p->motivoGeneros) {
    char gs[96] = "";
    int i, k = 0, n = 0;
    for (i = 0; i < REC_GENEROS_N && n < 3; i++) {
      if (!(p->motivoGeneros & (1u << i))) continue;
      k += snprintf(gs + k, sizeof gs - (size_t)k, "%s%s", n ? ", " : "", i18n(rec_genero_rotulo(i)));
      n++;
    }
    snprintf(dst, tam, i18n("Também gosta de %s"), gs);
    return;
  }
  if (p->motivo == REC_MOTIVO_ATIVOS && !p->emComum) {
    snprintf(dst, tam, "%s", i18n("Perfil ativo na comunidade"));
    return;
  }
  if (p->emComum > 0) snprintf(dst, tam, i18n("%d títulos em comum"), p->emComum);
  else if (p->vendo[0]) snprintf(dst, tam, i18n("Assistiu recentemente: %s"), p->vendo);
  else snprintf(dst, tam, "%s", p->bio);
}

static void pessoaL(const RecPessoa *p) {
  Linha *l = nova(T_PESSOA, A_PESSOA, "");
  if (!l) return;
  l->temPessoa = 1; l->p = *p;
}

static const char *rotuloAtiv(int n) {
  return n == 2 ? "O que assisti e o que estou assistindo"
       : n == 1 ? "O que assisti" : "Desligada";
}

static const char *tituloLista(int origem) {
  return origem == 3 ? "Comunidade Nuvio Native" : origem == 2 ? "Gosto parecido" : "Resultados da busca";
}

static void marcarHomonimos(void);
static void montar(void) {
  int i, np = recomenda_n_pedidos();
  char b[160];
  nL = 0;
  if (pagina == PG_MENU) {
    nova(T_BUSCA, A_BUSCAR, "");
    // O CODIGO NA MESMA FOLHA (06/10, "tudo mais junto"): o painel tem um botao
    // so, "Adicionar pessoas", e aqui estao as duas formas — buscar e digitar
    // o codigo de um amigo — com o codigo de quem esta olhando logo ali.
    { const char *cod = recomenda_meu_codigo();
      Linha *l = navL(A_CODIGO, "aj_user-plus", "Adicionar por código", NULL);
      if (l) {
        if (cod[0]) snprintf(l->sub, sizeof l->sub, i18n("O seu código é %s"), cod);
        else snprintf(l->sub, sizeof l->sub, "%s", i18n("Digite o código de um amigo")); } }
    if (np > 0) {
      secao("Pedidos de amizade", np);
      for (i = 0; i < np && i < 3; i++) { RecPessoa p; if (recomenda_pedido(i, &p)) pessoaL(&p); }
      if (np > 3) { snprintf(b, sizeof b, i18n("Ver todos os pedidos (%d)"), np);
                    navL(A_PEDIDOS, "aj_inbox", b, NULL); }
    }
    secao("Descobrir", 0);
    { int pq = recomenda_pesquisavel();
      navL(A_COMUNIDADE, "aj_users", "Comunidade Nuvio Native",
           pq ? "Todo mundo que ligou o Perfil pesquisável" : "Ligue o Perfil pesquisável para ver");
      navL(A_GOSTO, "aj_sparkles", "Gosto parecido",
           pq ? "Quem assiste e curte o mesmo que você" : "Ligue o Perfil pesquisável para ver"); }
    secao("Você", 0);
    { Linha *l = navL(A_PERFIL, "aj_user-round", "Meu perfil", NULL);
      if (l) {
        if (rasc.publicado && rasc.apelido[0])
          snprintf(l->sub, sizeof l->sub, "@%s \xc2\xb7 %s", rasc.apelido, i18n("Pesquisável"));
        else snprintf(l->sub, sizeof l->sub, "%s", i18n("Ninguém te acha ainda. Abra para ligar."));
      } }
    if (np == 0) navL(A_PEDIDOS, "aj_inbox", "Pedidos de amizade", "Nenhum pedido por enquanto.");
    else if (np <= 3) navL(A_PEDIDOS, "aj_inbox", "Pedidos de amizade", NULL);
    navL(A_BLOQUEADOS, "aj_shield", "Pessoas bloqueadas", NULL);
  } else if (pagina == PG_LISTA) {
    int n = recomenda_n_achados();
    if (listaOrigem == 1) nova(T_BUSCA, A_BUSCAR, ultimaBusca);
    if (listaCarregando) {
      for (i = 0; i < 4; i++) nova(T_ESQ, A_NADA, "");
    } else if (listaErro) {
      vazio("aj_wifi-off", "Não deu para carregar", "Confira a internet da TV e tente de novo.");
      navL(A_REPETIR, "aj_rotate-cw", "Tentar de novo", NULL);
    } else if (n == 0) {
      if (listaOrigem == 1) {
        snprintf(b, sizeof b, i18n("Ninguém encontrado para \xe2\x80\x9c%s\xe2\x80\x9d"), ultimaBusca);
        vazio("aj_search", b, "Confira o apelido ou peça o código de 6 letras. Só aparece quem ligou o Perfil pesquisável.");
      } else if (listaOrigem == 3 && recomenda_comunidade_fechada()) {
        vazio("aj_eye-off", "Ligue seu perfil para ver a comunidade",
              "Quem não aparece não vê os outros. Ligue em Meu perfil.");
        navL(A_PERFIL, "aj_user-round", "Abrir Meu perfil", NULL);
      } else if (listaOrigem == 2) {
        if (!recomenda_pesquisavel()) {
          vazio("aj_eye-off", "Ligue seu perfil para ver quem tem gosto parecido",
                "Quem não aparece não vê os outros. Ligue em Meu perfil.");
        } else {
          vazio("aj_sparkles", "Ninguém com gosto parecido por enquanto",
                "Assista e marque títulos, ou escolha gêneros em Meu perfil.");
        }
        navL(A_PERFIL, "aj_user-round", "Abrir Meu perfil", NULL);
      } else vazio("aj_users", "Ninguém na comunidade por enquanto", "Volte mais tarde.");
    } else {
      for (i = 0; i < n; i++) { RecPessoa p; if (recomenda_achado(i, &p)) pessoaL(&p); }
      if (listaOrigem == 3 && recomenda_comunidade_mais())
        navL(A_MAIS, opAtual == A_MAIS ? NULL : "aj_chevrons-down",
             opAtual == A_MAIS ? "Carregando…" : "Ver mais pessoas", NULL);
    }
  } else if (pagina == PG_PEDIDOS) {
    if (np == 0 && opAtual == A_PEDIDOS) for (i = 0; i < 2; i++) nova(T_ESQ, A_NADA, "");
    else if (np == 0) vazio("aj_inbox", "Nenhum pedido por enquanto",
                            "Quando alguém pedir sua amizade, aparece aqui e no topo de Encontrar pessoas.");
    for (i = 0; i < np; i++) { RecPessoa p; if (recomenda_pedido(i, &p)) pessoaL(&p); }
  } else if (pagina == PG_BLOQ) {
    int n = recomenda_n_bloqueados();
    if (n == 0 && opAtual == A_BLOQUEADOS) for (i = 0; i < 2; i++) nova(T_ESQ, A_NADA, "");
    else if (n == 0) vazio("aj_shield", "Você não bloqueou ninguém",
                           "Quem você bloquear não te acha, não vê seu perfil e não te pede amizade.");
    for (i = 0; i < n; i++) {
      char pub[16], nome[64];
      if (!recomenda_bloqueado(i, pub, sizeof pub, nome, sizeof nome)) continue;
      { Linha *l = nova(T_NAV, A_DESBLOQ, !nome[0] || rec_nome_reserva(nome) ? i18n("Sem nome") : nome);
        if (!l) break;
        snprintf(l->pub, sizeof l->pub, "%s", pub);
        l->temPessoa = 1;
        snprintf(l->sub, sizeof l->sub, "%s", confirmando == nL - 1 ? i18n("OK de novo para desbloquear")
                                                                   : i18n("OK desbloqueia")); }
    }
  } else if (pagina == PG_CARTAO) {
    RecPessoa c;
    if (recomenda_cartao(&c)) {
      if (!strcmp(c.relacao, "recebido")) {
        navL(A_ACEITAR, "aj_check", "Aceitar pedido", NULL);
        navL(A_RECUSAR, "aj_x", "Recusar pedido", "A pessoa não é avisada.");
      } else if (!strcmp(c.relacao, "enviado"))
        navL(A_CANCELAR, "aj_x", "Cancelar pedido", "Esperando a pessoa aceitar.");
      else if (strcmp(c.relacao, "amigo")) navL(A_PEDIR, "aj_user-plus", "Pedir amizade", NULL);
      { Linha *l = navL(A_BLOQUEAR, "aj_shield", confirmando >= 0 ? "Bloquear? OK confirma" : "Bloquear",
                        confirmando >= 0 ? NULL : "Desfaz a amizade e some da lista dela.");
        if (l) l->perigo = 1; }
    }
  } else if (pagina == PG_PERFIL) {
    Linha *l;
    l = navL(A_P_PESQ, "aj_search", "Perfil pesquisável",
             rasc.publicado ? "Pessoas te acham pelo apelido." : "Ninguém te acha pelo apelido.");
    if (l) l->sw = rasc.publicado ? 1 : 0;
    l = navL(A_P_APELIDO, "aj_type", "Apelido", NULL);
    if (l) { if (rasc.apelido[0]) snprintf(l->sub, sizeof l->sub, "@%s", rasc.apelido);
             else snprintf(l->sub, sizeof l->sub, "%s", i18n("Escolha como te acharão")); }
    l = navL(A_P_BIO, "aj_quote", "Bio curta", NULL);
    if (l) snprintf(l->sub, sizeof l->sub, "%s", rasc.bio[0] ? rasc.bio : i18n("Opcional"));
    l = navL(A_P_GEN, "aj_clapperboard", "Gêneros favoritos", NULL);
    if (l) {
      int k, n = 0;
      for (k = 0; k < REC_GENEROS_N; k++) if (rasc.generos & (1u << k)) n++;
      if (n) snprintf(l->sub, sizeof l->sub, i18n("%d escolhidos"), n);
      else snprintf(l->sub, sizeof l->sub, "%s", i18n("Opcional"));
    }
    l = navL(A_P_FOTO, "aj_image", "Mostrar minha foto", NULL); if (l) l->sw = rasc.foto ? 1 : 0;
    l = navL(A_P_REC, "aj_eye", "Mostrar o que assisti recentemente", NULL); if (l) l->sw = rasc.recentes ? 1 : 0;
    navL(A_P_ATIV, "aj_activity", "Atividade para amigos", rotuloAtiv(rasc.ativ));
    l = navL(A_P_APAGAR, "aj_delete", confirmando >= 0 ? "Apagar tudo? OK confirma" : "Apagar meus dados sociais", NULL);
    if (l) l->perigo = 1;
  } else if (pagina == PG_GEN) {
    for (i = 0; i < REC_GENEROS_N; i++) {
      Linha *l = navL(A_GEN, NULL, rec_genero_rotulo(i), NULL);
      if (l) { l->n = i; l->sw = (rasc.generos & (1u << i)) ? 1 : 0; }
    }
  }
  marcarHomonimos();
}

// HOMONIMOS (#202). O servidor ja junta as copias da MESMA pessoa; o que sobra
// com o mesmo nome na lista sao pessoas diferentes (dois perfis da casa que
// escolheram o mesmo apelido, ou alguem no Trakt e outra na conta Nuvio). Sem
// pista as duas linhas eram identicas. A pista e o comeco do handle publico:
// ele ja e o que estranhos recebem, nao diz conta, nome nem e-mail, e e
// sorteado de novo quando a pessoa despublica. So aparece quando ha colisao.
static void marcarHomonimos(void) {
  int i, j;
  char a[80], b[80];
  for (i = 0; i < nL; i++) linhas[i].dica[0] = 0;
  for (i = 0; i < nL; i++) {
    if (linhas[i].tipo != T_PESSOA || !linhas[i].p.pub[0]) continue;
    rec_identidade(linhas[i].p.nome, linhas[i].p.apelido, a, sizeof a, NULL, 0);
    for (j = 0; j < nL; j++) {
      if (j == i || linhas[j].tipo != T_PESSOA || !strcmp(linhas[i].p.pub, linhas[j].p.pub)) continue;
      rec_identidade(linhas[j].p.nome, linhas[j].p.apelido, b, sizeof b, NULL, 0);
      if (!SDL_strcasecmp(a, b)) {
        snprintf(linhas[i].dica, sizeof linhas[i].dica, "#%.4s", linhas[i].p.pub);
        break;
      }
    }
  }
}

static int focavel(int i) {
  return i >= 0 && i < nL && (linhas[i].tipo == T_NAV || linhas[i].tipo == T_BUSCA ||
                              linhas[i].tipo == T_PESSOA);
}
// Primeira linha focavel a partir de `i` andando `passo`; -1 se nao ha.
static int proxFocavel(int i, int passo) {
  for (; i >= 0 && i < nL; i += passo) if (focavel(i)) return i;
  return -1;
}
static void acertarFoco(void) {
  if (focavel(foco)) return;
  { int f = proxFocavel(foco < nL ? foco : nL - 1, -1);
    if (f < 0) f = proxFocavel(0, 1);
    foco = f < 0 ? 0 : f; }
}

// --- a acao da linha de uma pessoa ---------------------------------------------------

// A UNICA ACAO que a linha oferece, pela relacao: Adicionar (sem relacao) ou
// Aceitar (ela me pediu). "Pedido enviado" e "Amigos" sao estado, nao acao: o
// OK nessas linhas abre o cartao, que e onde cancelar e bloquear moram.
static int acaoDaPessoa(const RecPessoa *p) {
  if (!strcmp(p->relacao, "recebido")) return A_ACEITAR;
  if (!p->relacao[0]) return A_PEDIR;
  return A_NADA;
}
static int temColunaAcao(int i) {
  return i >= 0 && i < nL && linhas[i].tipo == T_PESSOA && acaoDaPessoa(&linhas[i].p) != A_NADA;
}

// PONTEIRO (#99): foco pelas MESMAS variaveis das setas — `foco` (a linha) e,
// numa pessoa com acao, `coluna` (b: 1 = a pilula, 0 = a pessoa). O OK do
// clique segue por pessoas_evento (aplicar). Idempotente; com o teclado
// aberto nao mexe em nada (ele e camada propria).
static int   pnRegistrar;          // a ilha assentada: vale registrar alvos
static float pnTopo, pnBase;       // a janela da lista (recorte vertical)
static void ponteiroLinha(int i, int col) {
  int temCol;
  if (!aberto || teclado_aberto() || !focavel(i)) return;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toque);
#endif
  temCol = temColunaAcao(i);
  if (foco == i && (!temCol || coluna == col)) return;
  foco = i; confirmando = -1;
  if (temCol) coluna = col;
}
int pessoas_teste_foco(int *col) { if (col) *col = coluna; return foco; }

// --- acao ---------------------------------------------------------------------------

static void abrirTeclado(int alvo) {
  tecladoPara = alvo;
  if (alvo == A_BUSCAR)
    teclado_abrir_com("Buscar pessoas",
                      "Apelido (3 letras ou mais) ou o código de 6 letras.",
                      24, ALFA_TEXTO, NULL);
  else if (alvo == A_P_APELIDO)
    teclado_abrir_com("Apelido", "Como as pessoas vão te achar. Use - entre as palavras.",
                      REC_APELIDO_MAX, ALFA_TEXTO, rasc.apelido[0] ? rasc.apelido : NULL);
  else
    teclado_abrir_com("Bio curta", "Uma frase sobre você. Sem e-mail nem link.",
                      REC_BIO_MAX, ALFA_TEXTO, rasc.bio[0] ? rasc.bio : NULL);
}

// Pede uma lista e ja vai para ela, com o esqueleto: a resposta chega na
// pagina certa, e o OK nunca fica parecendo sem efeito.
static void pedirLista(int origem, Uint32 agora) {
  int ok = origem == 1 ? recomenda_buscar(ultimaBusca)
         : origem == 2 ? recomenda_sugeridos_gosto() : recomenda_comunidade(0);
  if (!ok) {
    if (origem == 1 && recomenda_soc_estado() == REC_SOC_CURTA) {
      recomenda_soc_limpar();
      dizerErro(i18n("Digite pelo menos 3 letras."), agora);
    } else dizer(i18n("Aguarde..."), agora, 2500);
    return;
  }
  opAtual = origem == 1 ? A_BUSCAR : origem == 2 ? A_GOSTO : A_COMUNIDADE;
  irPara(PG_LISTA);
  listaOrigem = origem; listaCarregando = 1; listaErro = 0;
  aviso[0] = 0;
}

static void acaoPessoa(const RecPessoa *p, int acao, Uint32 agora) {
  int ok = 0;
  if (opAtual) { dizer(i18n("Aguarde..."), agora, 2000); return; }
  if (acao == A_PEDIR) ok = recomenda_pedir_amizade(p->pub);
  else if (acao == A_ACEITAR) ok = recomenda_aceitar(p->pub);
  if (!ok) return;
  opAtual = acao;
  snprintf(acaoPub, sizeof acaoPub, "%s", p->pub);
  acaoFalhou = 0;
  aviso[0] = 0;
}

static void aplicar(void) {
  Linha *l;
  Uint32 agora = SDL_GetTicks();
  RecPessoa c;
  if (foco < 0 || foco >= nL || !focavel(foco)) return;
  l = &linhas[foco];
  if (l->tipo == T_PESSOA && coluna == 1 && temColunaAcao(foco)) {
    acaoPessoa(&l->p, acaoDaPessoa(&l->p), agora);
    return;
  }
  switch (l->acao) {
    case A_BUSCAR: abrirTeclado(A_BUSCAR); return;
    case A_REPETIR: pedirLista(listaOrigem ? listaOrigem : 1, agora); return;
    case A_COMUNIDADE:
    case A_GOSTO:
      // RECIPROCO (e o servidor confere de novo): quem nao se mostra nao ganha
      // uma janela para ver os outros. O OK leva a quem resolve: Meu perfil.
      if (!recomenda_pesquisavel()) {
        carregarRascunho(); irPara(PG_PERFIL);
        dizer(i18n("Ligue o Perfil pesquisável em Meu perfil para ver a comunidade."), agora, 5000);
        return;
      }
      pedirLista(l->acao == A_GOSTO ? 2 : 3, agora);
      return;
    case A_MAIS:
      if (opAtual) return;
      if (recomenda_comunidade(recomenda_comunidade_pagina() + 1)) opAtual = A_MAIS;
      return;
    case A_PEDIDOS:
      irPara(PG_PEDIDOS);
      if (recomenda_listar_pedidos()) opAtual = A_PEDIDOS;
      return;
    case A_PERFIL: carregarRascunho(); irPara(PG_PERFIL); return;
    case A_CODIGO:
      // A folha do codigo e outra modal: esta se fecha antes de abri-la.
      aberto = 0;
      recenviar_abrir_amigos();
      return;
    case A_BLOQUEADOS:
      irPara(PG_BLOQ);
      if (recomenda_listar_bloqueados()) opAtual = A_BLOQUEADOS;
      return;
    case A_PESSOA:
      if (!l->temPessoa || opAtual) return;
      voltaPara = pagina;
      focoVolta = foco;
      snprintf(cartaoPub, sizeof cartaoPub, "%s", l->p.pub);
      if (recomenda_ver_perfil(cartaoPub)) { opAtual = A_PESSOA; dizer(i18n("Abrindo..."), agora, 0); }
      return;
    case A_PEDIR:
      if (recomenda_pedir_amizade(cartaoPub)) { opAtual = A_PEDIR; snprintf(acaoPub, sizeof acaoPub, "%s", cartaoPub);
                                               dizer(i18n("Enviando pedido..."), agora, 0); }
      return;
    case A_CANCELAR:
      if (recomenda_cancelar_pedido(cartaoPub)) opAtual = A_CANCELAR;
      return;
    case A_ACEITAR:
      if (recomenda_aceitar(cartaoPub)) { opAtual = A_ACEITAR; snprintf(acaoPub, sizeof acaoPub, "%s", cartaoPub); }
      return;
    case A_RECUSAR:
      if (recomenda_recusar(cartaoPub)) opAtual = A_RECUSAR;
      return;
    case A_BLOQUEAR:
      // DOIS OK: bloquear derruba a amizade nos dois lados, e um OK distraido
      // nao pode fazer isso.
      if (confirmando != foco) { confirmando = foco; return; }
      confirmando = -1;
      if (recomenda_cartao(&c) && recomenda_bloquear(c.pub)) opAtual = A_BLOQUEAR;
      return;
    case A_DESBLOQ:
      if (confirmando != foco) { confirmando = foco; return; }
      confirmando = -1;
      if (recomenda_desbloquear(l->pub)) opAtual = A_DESBLOQ;
      return;
    case A_P_PESQ:
      if (rasc.publicado) { pessoas_definir_pesquisavel(0); rasc.publicado = 0; }
      else pessoas_definir_pesquisavel(1);
      return;
    case A_P_APELIDO: abrirTeclado(A_P_APELIDO); return;
    case A_P_BIO: abrirTeclado(A_P_BIO); return;
    case A_P_GEN: irPara(PG_GEN); return;
    case A_P_FOTO: rasc.foto = !rasc.foto; aplicarRascunho(); return;
    case A_P_REC: rasc.recentes = !rasc.recentes; aplicarRascunho(); return;
    case A_P_ATIV:
      rasc.ativ = (rasc.ativ + 1) % 3;
      recomenda_atividade_nivel(rasc.ativ);
      return;
    case A_P_APAGAR:
      if (confirmando != foco) { confirmando = foco; return; }
      confirmando = -1;
      recomenda_apagar_dados_sociais();
      memset(&rasc, 0, sizeof rasc);
      dizer(i18n("Seus dados sociais foram apagados."), agora, 3500);
      return;
    case A_GEN:
      if (!(rasc.generos & (1u << l->n))) {
        int k, n = 0;
        for (k = 0; k < REC_GENEROS_N; k++) if (rasc.generos & (1u << k)) n++;
        // O servidor guarda no maximo 5: aceitar um sexto aqui deixaria a tela
        // dizendo uma coisa e o perfil outra.
        if (n >= 5) { dizerErro(i18n("Máximo de 5 gêneros."), agora); return; }
      }
      rasc.generos ^= 1u << l->n;
      aplicarRascunho();
      return;
    default: return;
  }
}

// A coluna ESCOLHIDA vale para as linhas seguintes: quem foi ao perfil de uma
// pessoa (←) e desce a lista continua no perfil, nao volta para a pilula.
static void moverFoco(int passo) {
  int f = proxFocavel(foco + passo, passo);
  if (f >= 0) foco = f;
  confirmando = -1;
}

void pessoas_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return;
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN) return;
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) toqueRetomar();
#endif
  k = e->key.keysym.sym;
  if (e->key.repeat && teclaOk(k)) return;
  // ← NA PILULA VAI A PESSOA (OK abre o perfil); → volta a pilula. Em qualquer
  // outro lugar ← continua sendo Voltar, como sempre foi nesta modal.
  if (k == SDLK_LEFT && coluna == 1 && temColunaAcao(foco)) { coluna = 0; return; }
  if (k == SDLK_RIGHT) { if (temColunaAcao(foco)) coluna = 1; return; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || k == SDLK_LEFT ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    if (confirmando >= 0) { confirmando = -1; return; }
    if (pagina == PG_GEN) { irPara(PG_PERFIL); return; }
    if (pagina == PG_CARTAO) {
      int volta = voltaPara;
      irPara(volta >= 0 ? volta : PG_MENU);
      // Volta para a MESMA pessoa da lista: numa comunidade de 40 nomes,
      // recomecar do primeiro a cada cartao fechado seria andar tudo de novo.
      if (volta >= 0 && focoVolta >= 0) { montar(); foco = focoVolta; coluna = 0; acertarFoco(); }
      focoVolta = -1;
      return;
    }
    if (pagina != PG_MENU) { irPara(PG_MENU); return; }
    aberto = 0;
    return;
  }
  if (k == SDLK_UP)   { moverFoco(-1); return; }
  if (k == SDLK_DOWN) { moverFoco(1); return; }
  if (teclaOk(k)) { aplicar(); return; }
}

// --- atualizar ---------------------------------------------------------------------------

static const char *textoEstado(int est) {
  switch (est) {
    case REC_SOC_LIMITE:      return "Muitas tentativas. Tente de novo mais tarde.";
    case REC_SOC_NAO_ACHOU:   return "Perfil indisponível.";
    case REC_SOC_SEM_APELIDO: return "Escolha um apelido em Meu perfil para pedir amizade.";
    case REC_SOC_CURTA:       return "Digite pelo menos 3 letras.";
    default:                  return "Não foi possível agora. Tente novamente.";
  }
}

// O nome de quem esta em `pub`, para a frase do rodape.
static void nomeDe(const char *pub, char *dst, size_t tam) {
  int i;
  dst[0] = 0;
  for (i = 0; i < nL; i++)
    if (linhas[i].tipo == T_PESSOA && !strcmp(linhas[i].p.pub, pub)) {
      char l2[64];
      rec_identidade(linhas[i].p.nome, linhas[i].p.apelido, dst, tam, l2, sizeof l2);
      return;
    }
  { RecPessoa c;
    if (recomenda_cartao(&c) && !strcmp(c.pub, pub)) {
      char l2[64];
      rec_identidade(c.nome, c.apelido, dst, tam, l2, sizeof l2);
    } }
}

// O fim de cada operacao de rede: para onde a tela vai e o que ela diz.
static void concluiu(int est, Uint32 agora) {
  int op = opAtual;
  char quem[80], b[200];
  opAtual = 0;
  if (op == A_BUSCAR || op == A_GOSTO || op == A_COMUNIDADE) {
    listaCarregando = 0;
    listaErro = est != REC_SOC_OK;
    if (listaErro && est != REC_SOC_FALHA) dizerErro(i18n(textoEstado(est)), agora);
    if (!listaErro) listaOrigem = recomenda_achados_origem();
    montar();
    // Na busca o foco ja cai na primeira PESSOA, nao no campo: e ela que a
    // pessoa veio procurar.
    foco = (!listaErro && listaOrigem == 1 && recomenda_n_achados() > 0) ? 1 : 0;
    acertarFoco();
    return;
  }
  if (est != REC_SOC_OK) {
    if (op == A_PEDIR || op == A_ACEITAR) acaoFalhou = 1;
    dizerErro(i18n(textoEstado(est)), agora);
    return;
  }
  switch (op) {
    case A_MAIS:
      // Fica onde esta: o foco ja estava em "Ver mais", que agora e a primeira
      // pessoa nova (ou a ultima, se nao veio ninguem).
      aviso[0] = 0;
      break;
    case A_PESSOA:
      irPara(PG_CARTAO); aviso[0] = 0;
      break;
    case A_PEDIR:
      nomeDe(acaoPub, quem, sizeof quem);
      if (quem[0]) snprintf(b, sizeof b, i18n("Pedido enviado para %s."), quem);
      else snprintf(b, sizeof b, "%s", i18n("Pedido enviado. Quando a pessoa aceitar, vocês viram amigos."));
      dizer(b, agora, 4200);
      snprintf(feitoPub, sizeof feitoPub, "%s", acaoPub); feitoAte = agora + PE_FEITO_MS;
      if (pagina == PG_CARTAO) { recomenda_ver_perfil(cartaoPub); opAtual = A_PESSOA + 1000; }
      break;
    case A_PESSOA + 1000:
      irPara(PG_CARTAO); confirmando = -1;
      break;
    case A_CANCELAR:
      dizer(i18n("Pedido cancelado."), agora, 3000);
      recomenda_ver_perfil(cartaoPub); opAtual = A_PESSOA + 1000;
      break;
    case A_ACEITAR:
      nomeDe(acaoPub, quem, sizeof quem);
      if (quem[0]) snprintf(b, sizeof b, i18n("Você e %s agora são amigos."), quem);
      else snprintf(b, sizeof b, "%s", i18n("Agora vocês são amigos."));
      dizer(b, agora, 4200);
      snprintf(feitoPub, sizeof feitoPub, "%s", acaoPub); feitoAte = agora + PE_FEITO_MS;
      if (pagina == PG_CARTAO) { recomenda_ver_perfil(cartaoPub); opAtual = A_PESSOA + 1000; }
      break;
    case A_RECUSAR:
      dizer(i18n("Pedido recusado. A pessoa não é avisada."), agora, 3800);
      irPara(voltaPara >= 0 ? voltaPara : PG_MENU);
      break;
    case A_BLOQUEAR:
      dizer(i18n("Pessoa bloqueada."), agora, 3500);
      irPara(PG_MENU);
      break;
    case A_DESBLOQ:
      dizer(i18n("Pessoa desbloqueada."), agora, 3000);
      if (recomenda_listar_bloqueados()) opAtual = A_BLOQUEADOS;
      break;
    default: break;
  }
  if (op == A_PEDIR || op == A_ACEITAR) { acaoPub[0] = 0; acaoFalhou = 0; }
}

// A posicao de cada linha e a rolagem que deixa a focada inteira na janela.
static float alturaLinha(const Linha *l) {
  switch (l->tipo) {
    case T_PESSOA: case T_ESQ: return PE_H_PESSOA;
    case T_BUSCA:  return PE_H_BUSCA;
    case T_SECAO:  return PE_H_SECAO;
    case T_VAZIO:  return PE_H_VAZIO;
    default:       return PE_H_NAV;
  }
}
static void medir(void) {
  int i;
  float y = 0.0f;
  for (i = 0; i < nL; i++) {
    linhaY[i] = y;
    y += alturaLinha(&linhas[i]) + PE_GAP;
  }
  conteudoH = nL ? y - PE_GAP : 0.0f;
}

static float janelaH(float cab);
static float alturaIlha(void);
static float janelaFoco(void) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) {
    /* Choose the same scale from the same unscaled geometry as drawing,
     * including on the first frame or after a rotation. */
    ESCALA_SE_COUBER_INI(PE_W, alturaIlha());
    float h = janelaH(0);
    ESCALA_SE_COUBER_FIM();
    return h;
  }
#endif
  return janelaH(0);
}

void pessoas_atualizar(float dt, Uint32 agora) {
  int i, est;
  if (!aberto && anim < 0.002f) { anim = 0.0f; return; }
  anim = ajustes_animacoes_reduzidas()
           ? (aberto ? 1.0f : 0.0f)
           : anim_mola(anim, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  teclado_atualizar(dt, agora);
  for (i = 0; i < PE_MAXL; i++) {
    float alvo = aberto && foco == i ? 1.0f : 0.0f;
    float alvoC = alvo > 0.5f && coluna == 1 && temColunaAcao(i) ? 1.0f : 0.0f;
    focoAnim[i] = ajustes_animacoes_reduzidas() ? alvo : anim_mola(focoAnim[i], alvo, dt, NV_MOLA_FOCO);
    colAnim[i] = ajustes_animacoes_reduzidas() ? alvoC : anim_mola(colAnim[i], alvoC, dt, NV_MOLA_FOCO);
  }
  if (!aberto) return;

  // O TECLADO: o resultado e CONSUMIDO NA LEITURA, entao isto roda uma vez por
  // confirmacao. So o alvo que abriu o teclado pode ler.
  if (tecladoPara && !teclado_aberto()) {
    int r = teclado_resultado();
    int alvo = tecladoPara;
    tecladoPara = 0;
    if (r == TECLADO_PRONTO) {
      const char *t = teclado_texto();
      if (alvo == A_BUSCAR) {
        snprintf(ultimaBusca, sizeof ultimaBusca, "%s", t);
        { char *w = ultimaBusca; for (; *w; w++) if (*w == '-') *w = ' '; }
        pedirLista(1, agora);
      } else if (alvo == A_P_APELIDO) {
        snprintf(rasc.apelido, sizeof rasc.apelido, "%s", t);
        { char *w = rasc.apelido; for (; *w; w++) if (*w == '-') *w = ' '; }
        if (strlen(rasc.apelido) < 2) {
          rasc.apelido[0] = 0;
          dizerErro(i18n("O apelido precisa de 2 letras ou mais."), agora);
        } else {
          // Escolheu o apelido para ligar o perfil? Liga: era isso que o
          // interruptor de Ajustes tinha pedido.
          if (!rasc.publicado && !recomenda_pesquisavel()) pessoas_definir_pesquisavel(1);
          else aplicarRascunho();
        }
      } else if (alvo == A_P_BIO) {
        snprintf(rasc.bio, sizeof rasc.bio, "%s", t);
        { char *w = rasc.bio; for (; *w; w++) if (*w == '-') *w = ' '; }
        aplicarRascunho();
      }
    }
  }

  est = recomenda_soc_estado();
  if (est != REC_SOC_NADA && est != REC_SOC_INDO) {
    if (opAtual) concluiu(est, agora);
    else if (est == REC_SOC_CURTA) dizerErro(i18n(textoEstado(est)), agora);
    recomenda_soc_limpar();
  }
  if (avisoAte && (Sint32)(agora - avisoAte) >= 0) { aviso[0] = 0; avisoAte = 0; avisoErro = 0; }
  if (feitoPub[0] && (Sint32)(agora - feitoAte) >= 0) feitoPub[0] = 0;
  montar();
  acertarFoco();
  medir();
  // ROLAGEM: a linha focada inteira na janela, com meia linha de folga quando
  // da (quem desce ve que ha mais embaixo). A secao logo acima da linha focada
  // entra junto: um cabecalho sozinho no pe da janela nao diz nada.
#ifdef NV_TOUCH_UI
  if (toque.livre) {
    float jh = telefoneui_ativo() ? janelaFoco() : toque.regiao.h;
    rolagem = rolagemAlvo = toquerol_clamp(rolagem, 0.0f, fmaxf(0.0f, conteudoH - jh));
    return;
  }
#endif
  { float jh = janelaFoco(), topoF, baseF;
    if (foco >= 0 && foco < nL) {
      topoF = linhaY[foco];
      if (foco > 0 && linhas[foco - 1].tipo == T_SECAO) topoF = linhaY[foco - 1];
      if (proxFocavel(0, 1) == foco) topoF = 0.0f;
      baseF = linhaY[foco] + alturaLinha(&linhas[foco]);
      if (foco + 1 < nL) baseF += PE_GAP + alturaLinha(&linhas[foco + 1]) * 0.45f;
      if (topoF < rolagemAlvo) rolagemAlvo = topoF;
      if (baseF > rolagemAlvo + jh) rolagemAlvo = baseF - jh;
    }
    // A JANELA COMECA NO TOPO DE UMA LINHA: rolada no meio, a primeira linha
    // aparecia so com metade do texto colada no subtitulo. Arredonda para a
    // proxima linha inteira (a focada continua inteira: ela esta mais abaixo).
    if (rolagemAlvo > 0.0f) {
      for (i = 0; i < nL; i++)
        if (linhaY[i] >= rolagemAlvo - 0.5f) { rolagemAlvo = linhaY[i]; break; }
    }
    if (rolagemAlvo > conteudoH - jh) rolagemAlvo = conteudoH - jh;
    if (rolagemAlvo < 0.0f) rolagemAlvo = 0.0f;
    rolagem = ajustes_animacoes_reduzidas() ? rolagemAlvo : anim_mola(rolagem, rolagemAlvo, dt, NV_MOLA_FOCO); }
}

// --- desenho -------------------------------------------------------------------------------

// O INTERRUPTOR no acento quando ligado (o "chip ligado" da folha de Fontes),
// trilho branco a 18 % desligado. A bola ligada vai na TINTA do acento: no
// acento claro (o branco do padrao) uma bola branca sumia no trilho.
static void desenhaInterruptor(float x, float y, int lig, float a) {
  GfxRect t = { x, y, PE_SW_W, PE_SW_H };
  float bola = PE_SW_H - 8.0f, bx = lig ? x + PE_SW_W - bola - 4.0f : x + 4.0f, ar, ag, ab, k = 0.97f;
  if (lig) { k = ajustes_acento_tinta(&ar, &ag, &ab); gfx_cor(t, 0.5f, ar, ag, ab, a); }
  else gfx_cor(t, 0.5f, 1, 1, 1, 0.18f * a);
  gfx_rect((GfxRect){ bx, y + 4.0f, bola, bola }, 0, GFX_DISCO, 0, 0, 0, 0, k, k, k, a);
}

// Linha de texto da ilha (#f3f2ef); a hierarquia vem do alfa de quem desenha.
static TxtLinha txtI(TxtEstilo e, const char *s, float maxW) {
  return txt_linha_corta(e, s, 243, 242, 239, 255, maxW);
}

// O acento clareado: texto e icone "no tom do acento" sobre o escuro.
static void tomAcento(float *r, float *g, float *b) {
  ajustes_acento(r, g, b);
  *r += (1.0f - *r) * 0.35f; *g += (1.0f - *g) * 0.35f; *b += (1.0f - *b) * 0.35f;
}

// A superficie do foco de LINHA (plrui_linha_foco: branco 12 % no vidro, o
// degrau cinza com sombra no solido), 18 px para fora da coluna do texto.
static GfxRect retLinha(float x, float y, float h) {
  return (GfxRect){ x - PE_RECUO, y, PE_INTERNO + PE_RECUO * 2.0f, h };
}

// A PILULA DA ACAO de uma pessoa, ancorada pela DIREITA em `xd`. Devolve a
// largura ocupada. As caras:
//   Adicionar / Aceitar        botao de ilha (plrui_botao): o foco da coluna o
//                              enche no acento
//   Enviando... / Aceitando... a mesma pilula em repouso, com o anel girando
//   Tentar de novo             botao, com o icone de repetir
//   Pedido enviado             estado: icone + texto a 62 %, sem fundo
//   Amigos                     estado no tom do acento, fundo a 20 %
// Acabou de dar certo: a pilula de estado acende no acento cheio e apaga
// devagar ate o repouso — e o "visto" que responde ao OK.
static float pilulaPessoa(float xd, float yc, const RecPessoa *p, float fCol, Uint32 agora, float a) {
  int acao = acaoDaPessoa(p);
  int minha = acaoPub[0] && !strcmp(acaoPub, p->pub);
  int feito = feitoPub[0] && !strcmp(feitoPub, p->pub);
  if (minha && (opAtual == A_PEDIR || opAtual == A_ACEITAR)) {
    const char *rot = opAtual == A_ACEITAR ? "Aceitando…" : "Enviando…";
    float w = plrui_botao_largura(rot, "aj_loader");
    GfxRect r = { xd - w, yc - 30.0f, w, 60.0f };
    TxtLinha l = txtI(TXT_G21B, rot, 400.0f);
    plrui_botao_repouso(r, a);
    plrui_anel(r.x + 28.0f + 11.0f, yc, 30.0f, 1, agora, a);
    txt_desenhar_alpha(l, r.x + 28.0f + 22.0f + 12.0f, yc - (float)l.h * 0.5f, a * 0.85f);
    return w;
  }
  if (acao != A_NADA) {
    const char *rot = minha && acaoFalhou ? "Tentar de novo" : acao == A_ACEITAR ? "Aceitar" : "Adicionar";
    const char *ic = minha && acaoFalhou ? "aj_rotate-cw" : acao == A_ACEITAR ? "aj_check" : "aj_user-plus";
    float w = plrui_botao_largura(rot, ic);
    plrui_botao(xd - w, yc - 30.0f, rot, ic, fCol, a);
    return w;
  }
  { int amigo = !strcmp(p->relacao, "amigo");
    const char *rot = amigo ? "Amigos" : "Pedido enviado";
    const char *ic = amigo || feito ? "aj_circle-check" : "aj_clock";
    float ar, ag, ab, tr = 0.953f, tg = 0.949f, tb = 0.937f, ta = 0.62f;
    TxtLinha l = txt_linha(TXT_G21B, rot, 255, 255, 255, 255);
    float w = 20.0f + 22.0f + 10.0f + (float)l.w + 22.0f;
    GfxRect r = { xd - w, yc - 24.0f, w, 48.0f };
    ajustes_acento(&ar, &ag, &ab);
    if (feito) {
      float k = anim_clamp((float)(Sint32)(feitoAte - agora) / (float)PE_FEITO_MS, 0.0f, 1.0f);
      gfx_cor(r, 0.5f, ar, ag, ab, (0.20f + 0.80f * k) * a);
      if (k > 0.5f) { float t = ajustes_acento_tinta(NULL, NULL, NULL); tr = tg = tb = t; }
      else tomAcento(&tr, &tg, &tb);
      ta = 1.0f;
    } else if (amigo) {
      gfx_cor(r, 0.5f, ar, ag, ab, 0.20f * a);
      tomAcento(&tr, &tg, &tb); ta = 1.0f;
    }
    gfx_icone((GfxRect){ r.x + 20.0f, yc - 11.0f, 22.0f, 22.0f }, ic, tr, tg, tb, ta * a);
    { TxtLinha c = txt_linha(TXT_G21B, rot, (int)(tr * 255.0f), (int)(tg * 255.0f), (int)(tb * 255.0f), 255);
      txt_desenhar_alpha(c, r.x + 52.0f, yc - (float)c.h * 0.5f, ta * a); }
    return w;
  }
}

// O NOME E O SELO na mesma linha. O selo nunca empurra o nome para fora: o
// nome e cortado para caber com ele.
static void nomeComSelo(TxtEstilo es, const char *nome, const char *selo, float x, float y,
                        float larg, float alfaNome, float a) {
  float sw = rec_selo_pessoa_largura(selo);
  TxtLinha n = txtI(es, nome, sw > 0.0f ? larg - sw - 12.0f : larg);
  txt_desenhar_alpha(n, x, y, a * alfaNome);
  if (sw > 0.0f) rec_selo_pessoa(x + (float)n.w + 12.0f, y + ((float)n.h - BADGE_H) * 0.5f + 1.0f, selo, a);
}

static void desenhaPessoa(float x, float y, int i, Uint32 agora, float a) {
  const Linha *l = &linhas[i];
  const RecPessoa *p = &l->p;
  float f = focoAnim[i], h = PE_H_PESSOA, yc = y + h * 0.5f, tx, xd;
  // So nas linhas COM acao: sem ela o OK ja e "ver perfil" e o rodape diz.
  int naPessoa = f > 0.5f && coluna == 0 && temColunaAcao(i);
  char l1[80], l2[64], ctx[160], baixo[240];
  if (f > 0.01f) plrui_linha_foco(retLinha(x, y, h), 24.0f, f * a);
  rec_identidade(p->nome, p->apelido, l1, sizeof l1, l2, sizeof l2);
  if (l->dica[0]) {
    size_t k = strlen(l2);
    snprintf(l2 + k, sizeof l2 - k, "%s%s", k ? " " : "", l->dica);
  }
  rec_avatar_estilo((GfxRect){ x, yc - PE_AV * 0.5f, PE_AV, PE_AV }, p->avatar, l1, p->pub, a, TXT_ILHA_INICIAL);
  tx = x + PE_AV + 20.0f;
  xd = x + PE_INTERNO;
  { float pw = pilulaPessoa(xd, yc, p, colAnim[i], agora, a);
    // A pilula por cima da linha (registrada depois): ali o foco e a coluna 1.
    if (pnRegistrar && temColunaAcao(i))
      ponteiro_alvo_faixa(xd - pw, yc - 30.0f, pw, 60.0f, pnTopo, pnBase, ponteiroLinha, NULL, i, 1);
    xd -= pw + 24.0f; }
  // O FOCO NA PESSOA (coluna 0, ← na pilula): o chevron antes da pilula diz
  // "OK abre o perfil" sem precisar ler o rodape.
  if (naPessoa) {
    gfx_icone((GfxRect){ xd - 24.0f, yc - 12.0f, 24.0f, 24.0f }, "aj_chevron-right", 1, 1, 1, 0.7f * a);
    xd -= 24.0f + 16.0f;
  }
  contexto(ctx, sizeof ctx, p);
  if (l2[0] && ctx[0]) snprintf(baixo, sizeof baixo, "%s  \xc2\xb7  %s", l2, ctx);
  else snprintf(baixo, sizeof baixo, "%s", l2[0] ? l2 : ctx);
  { float alto = 30.0f + (baixo[0] ? 32.0f : 0.0f), ty = yc - alto * 0.5f;
    // O nome a 88 % em repouso e cheio no foco, como a linha do Social.
    nomeComSelo(TXT_ILHA_NOME, l1, p->selo, tx, ty, xd - tx, 0.88f + 0.12f * f, a);
    if (baixo[0]) txt_desenhar_alpha(txtI(TXT_ILHA_SUB, baixo, xd - tx), tx, ty + 36.0f, a * 0.62f); }
}

static void desenhaNav(float x, float y, int i, float a) {
  const Linha *l = &linhas[i];
  float f = focoAnim[i], h = PE_H_NAV, yc = y + h * 0.5f, tx = x, fim = x + PE_INTERNO;
  int alerta = confirmando == i || l->perigo;
  if (f > 0.01f) plrui_linha_foco(retLinha(x, y, h), 22.0f, f * a);
  if (l->temPessoa) {
    rec_avatar_estilo((GfxRect){ x, yc - 26.0f, 52.0f, 52.0f }, "", l->rot, l->pub, a, TXT_ILHA_INICIAL);
    tx += 52.0f + 20.0f;
  } else if (l->icone) {
    // O icone num disco de 52 a 8 % (o ladrilho das linhas de Ajustes).
    GfxRect d = { x, yc - 26.0f, 52.0f, 52.0f };
    float ir = 0.95f, ig = 0.95f, ib = 0.94f;
    if (alerta) { ir = 1.0f; ig = 0.55f; ib = 0.50f; }
    gfx_cor(d, 0.5f, 1, 1, 1, 0.08f * a);
    gfx_icone((GfxRect){ d.x + 14.0f, d.y + 14.0f, 24.0f, 24.0f }, l->icone, ir, ig, ib, 0.9f * a);
    tx += 52.0f + 20.0f;
  } else if (l->acao == A_MAIS) {
    plrui_anel(x + 26.0f, yc, 34.0f, 1, SDL_GetTicks(), a);
    tx += 52.0f + 20.0f;
  }
  if (l->sw >= 0) {
    desenhaInterruptor(fim - PE_SW_W, yc - PE_SW_H * 0.5f, l->sw, a);
    fim -= PE_SW_W + 24.0f;
  } else if (l->acao != A_P_ATIV && l->acao != A_DESBLOQ && l->acao != A_BLOQUEAR &&
             l->acao != A_P_APAGAR && l->acao != A_REPETIR && l->acao != A_ACEITAR &&
             l->acao != A_RECUSAR && l->acao != A_CANCELAR && l->acao != A_PEDIR) {
    gfx_icone((GfxRect){ fim - 24.0f, yc - 12.0f, 24.0f, 24.0f }, "aj_chevron-right", 1, 1, 1,
              (0.35f + 0.4f * f) * a);
    fim -= 24.0f + 20.0f;
  }
  { int r = alerta ? 255 : 243, g = alerta ? 150 : 242, b = alerta ? 140 : 239;
    TxtLinha t = txt_linha_corta(TXT_ILHA_NOME, l->rot, r, g, b, 255, fim - tx);
    if (l->sub[0]) {
      float alto = 30.0f + 32.0f, ty = yc - alto * 0.5f;
      txt_desenhar_alpha(t, tx, ty, a * (0.88f + 0.12f * f));
      txt_desenhar_alpha(txtI(TXT_ILHA_SUB, l->sub, fim - tx), tx, ty + 36.0f, a * 0.62f);
    } else txt_desenhar_alpha(t, tx, yc - (float)t.h * 0.5f, a * (0.88f + 0.12f * f)); }
}

// O CAMPO DE BUSCA: parece campo (fundo a 6 %, lupa, o texto de exemplo
// apagado), e o OK abre o mesmo teclado de sempre.
static void desenhaBusca(float x, float y, int i, float a) {
  const Linha *l = &linhas[i];
  float f = focoAnim[i], h = PE_H_BUSCA, yc = y + h * 0.5f, dw = 0.0f;
  GfxRect r = retLinha(x, y, h);
  if (ajustes_vidro()) gfx_cor(r, 24.0f / h, 1, 1, 1, 0.06f * a);
  else gfx_cor(r, 24.0f / h, 0.098f, 0.102f, 0.118f, a);
  if (f > 0.01f) plrui_linha_foco(r, 24.0f, f * a);
  if (l->rot[0] && f > 0.5f) {
    TxtLinha t = txtI(TXT_ILHA_SUB, "OK para buscar de novo", 320.0f);
    dw = (float)t.w + 24.0f;
    txt_desenhar_alpha(t, x + PE_INTERNO - (float)t.w, yc - (float)t.h * 0.5f, a * 0.5f);
  }
  gfx_icone((GfxRect){ x + 2.0f, yc - 14.0f, 28.0f, 28.0f }, "aj_search", 1, 1, 1, (0.55f + 0.4f * f) * a);
  // O TEXTO DE EXEMPLO e regular e apagado (o digitado e semibold e cheio):
  // e a diferenca que faz o campo ler como campo, e nao como rotulo.
  { const char *s = l->rot[0] ? l->rot : "Buscar por apelido ou código de 6 letras";
    TxtLinha t = txtI(l->rot[0] ? TXT_ILHA_NOME : TXT_ILHA_CORPO, s, PE_INTERNO - 70.0f - dw);
    txt_desenhar_alpha(t, x + 58.0f, yc - (float)t.h * 0.5f, a * (l->rot[0] ? 1.0f : 0.48f + 0.14f * f)); }
}

static void desenhaSecao(float x, float y, const Linha *l, float a) {
  float by = y + PE_H_SECAO - 30.0f, w = plrui_kicker(l->rot, -1.0f, 0.0f, 0, 0, 0, 0);
  plrui_kicker(l->rot, x, by, 243, 242, 239, a * 0.55f);
  // O NUMERO DE PEDIDOS numa pilula no acento: e o que chama o olho para o topo.
  if (l->n > 0) {
    char n[12];
    snprintf(n, sizeof n, "%d", l->n);
    badge_desenhar(x + w + 12.0f, by + 8.0f - BADGE_H * 0.5f, n, BADGE_REALCE_CHEIO, a);
  }
}

static void desenhaVazio(float x, float y, const Linha *l, float a) {
  float cx = x + PE_INTERNO * 0.5f, yy = y + 30.0f;
  GfxRect d = { cx - 40.0f, yy, 80.0f, 80.0f };
  gfx_cor(d, 0.5f, 1, 1, 1, 0.07f * a);
  gfx_icone((GfxRect){ d.x + 22.0f, d.y + 22.0f, 36.0f, 36.0f }, l->icone ? l->icone : "aj_info",
            0.95f, 0.95f, 0.94f, 0.85f * a);
  yy += 80.0f + 22.0f;
  { TxtLinha t = txtI(TXT_ILHA_NOME, l->rot, PE_INTERNO);
    txt_desenhar_alpha(t, cx - (float)t.w * 0.5f, yy, a); yy += (float)t.h + 10.0f; }
  // O texto centrado em ate duas linhas, numa coluna mais estreita: frase
  // larga e centrada cansa a leitura. txt_bloco nao centra; cada linha e
  // medida e posta no meio aqui.
  { float cw = PE_INTERNO * 0.8f;
    const char *s = l->sub;
    char ln[2][160];
    int nl = 0;
    // DUAS FRASES QUE CABEM CADA UMA NUMA LINHA quebram entre elas: a quebra
    // por largura deixava uma palavra sozinha na segunda linha.
    { const char *pt = strstr(s, ". ");
      if (pt && !strstr(pt + 2, ". ")) {
        size_t k1 = (size_t)(pt - s) + 1;
        if (k1 < sizeof ln[0] && strlen(pt + 2) < sizeof ln[1]) {
          memcpy(ln[0], s, k1); ln[0][k1] = 0;
          snprintf(ln[1], sizeof ln[1], "%s", pt + 2);
          if ((float)txt_largura(TXT_ILHA_SUB, ln[0]) <= cw && (float)txt_largura(TXT_ILHA_SUB, ln[1]) <= cw) {
            nl = 2; s = "";
          }
        }
      } }
    while (*s && nl < 2) {
      const char *corte = s, *p = s;
      while (*p) {
        const char *q = p;
        char tmp[160];
        size_t k;
        while (*q == ' ') q++;
        while (*q && *q != ' ') q++;
        k = (size_t)(q - s) < sizeof tmp - 1 ? (size_t)(q - s) : sizeof tmp - 1;
        memcpy(tmp, s, k); tmp[k] = 0;
        if ((float)txt_largura(TXT_ILHA_SUB, tmp) > cw && corte != s) break;
        corte = q; p = q;
        if (!*q) break;
      }
      { size_t k = (size_t)(corte - s) < sizeof ln[0] - 1 ? (size_t)(corte - s) : sizeof ln[0] - 1;
        memcpy(ln[nl], s, k); ln[nl][k] = 0; }
      s = corte; while (*s == ' ') s++;
      nl++;
    }
    { int q;
      for (q = 0; q < nl; q++) {
        TxtLinha t = txtI(TXT_ILHA_SUB, ln[q], cw);
        txt_desenhar_alpha(t, cx - (float)t.w * 0.5f, yy + 28.0f * (float)q, a * 0.62f);
      } } }
}

// Esqueleto de uma linha de pessoa: disco, duas barras e a pilula.
static void desenhaEsq(float x, float y, float a) {
  float yc = y + PE_H_PESSOA * 0.5f;
  gfx_esqueleto((GfxRect){ x, yc - PE_AV * 0.5f, PE_AV, PE_AV }, 0.5f, 1, 1, 1, 0.07f * a);
  gfx_esqueleto((GfxRect){ x + PE_AV + 20.0f, yc - 22.0f, 260.0f, 20.0f }, 0.5f, 1, 1, 1, 0.07f * a);
  gfx_esqueleto((GfxRect){ x + PE_AV + 20.0f, yc + 10.0f, 180.0f, 16.0f }, 0.5f, 1, 1, 1, 0.05f * a);
  gfx_esqueleto((GfxRect){ x + PE_INTERNO - 170.0f, yc - 30.0f, 170.0f, 60.0f }, 0.5f, 1, 1, 1, 0.05f * a);
}

// Generos escolhidos, como selos (os que cabem numa linha).
static float selosGeneros(float x, float y, float larg, unsigned m, float a) {
  int i;
  float xx = x;
  for (i = 0; i < REC_GENEROS_N; i++)
    if (m & (1u << i)) {
      const char *g = i18n(rec_genero_rotulo(i));
      float w = badge_largura(g);
      if (xx + w > x + larg) break;
      xx += badge_desenhar(xx, y, g, BADGE_NEUTRO, a) + BADGE_GAP;
    }
  return xx > x ? BADGE_H : 0.0f;
}

// --- cabecalho por pagina ---------------------------------------------------------

static const char *chapeuDe(void) { return pagina == PG_CARTAO ? "Perfil" : "Amigos"; }
static const char *tituloDe(void) {
  switch (pagina) {
    case PG_MENU:    return "Adicionar pessoas";
    case PG_LISTA:   return tituloLista(listaOrigem);
    case PG_PEDIDOS: return "Pedidos de amizade";
    case PG_BLOQ:    return "Pessoas bloqueadas";
    case PG_PERFIL:  return "Meu perfil";
    case PG_GEN:     return "Gêneros favoritos";
    default:         return "";
  }
}
static const char *subDe(void) {
  switch (pagina) {
    case PG_MENU:    return "Busque pelo nome ou @apelido, ou digite o código de 6 letras de um amigo.";
    case PG_LISTA:   return listaOrigem == 3 ? "Quem ligou o Perfil pesquisável, com atividade mais recente primeiro."
                          : listaOrigem == 2 ? "Quem tem títulos ou gêneros em comum com você."
                          : "";
    case PG_PEDIDOS: return recomenda_n_pedidos() ? "Só vira amizade se você aceitar. Recusar não avisa a pessoa." : "";
    case PG_PERFIL:  return "Estranhos veem só o apelido e o que você ligar aqui. Desligar apaga do servidor na hora.";
    case PG_GEN:     return "Até 5 aparecem no seu perfil.";
    default:         return "";
  }
}
// Linhas que o subtitulo ocupa (0, 1 ou 2), pela largura medida.
static int linhasSub(const char *s) {
  if (!s || !s[0]) return 0;
  return (float)txt_largura(TXT_ILHA_CORPO, i18n(s)) > PE_INTERNO ? 2 : 1;
}

static int linhasBio(const char *s) {
  return (float)txt_largura(TXT_ILHA_CORPO, s) > PE_INTERNO ? 2 : 1;
}

static float alturaCab(const RecPessoa *c, int temC) {
  if (pagina == PG_CARTAO) {
    float h = 20.0f + 14.0f + PE_AV_CARTAO + 26.0f;
    if (temC && c->bio[0]) h += 38.0f + (linhasBio(c->bio) > 1 ? 32.0f : 0.0f);
    if (temC && c->generos) h += BADGE_H + 18.0f;
    if (temC && recomenda_cartao_n_recentes()) h += 34.0f;
    return h + 8.0f;
  }
  { int n = linhasSub(subDe());
    return 20.0f + 14.0f + 50.0f + (n ? 8.0f + 32.0f * (float)n : 0.0f) + 24.0f; }
}

// A janela da lista: o que sobra da tela depois de cabecalho e rodape.
static float janelaH(float cab) {
  RecPessoa c;
  int temC = pagina == PG_CARTAO && recomenda_cartao(&c);
  float max;
  if (cab <= 0.0f) cab = alturaCab(&c, temC);
  max = NV_TELA_H - PE_MARGEM * 2.0f - PE_PAD * 2.0f - cab - PE_RODAPE;
  if (max < PE_H_PESSOA) max = PE_H_PESSOA;
  return conteudoH < max ? conteudoH : max;
}

static void desenhaCabCartao(float hx, float hy, const RecPessoa *c, float a) {
  char l1[80], l2[64], rel[96];
  float tx = hx + PE_AV_CARTAO + 32.0f, tw = PE_INTERNO - PE_AV_CARTAO - 32.0f, y;
  rec_identidade(c->nome, c->apelido, l1, sizeof l1, l2, sizeof l2);
  rec_avatar_estilo((GfxRect){ hx, hy, PE_AV_CARTAO, PE_AV_CARTAO }, c->avatar, l1, c->pub, a, TXT_ILHA_TITULO);
  rel[0] = 0;
  if (!strcmp(c->relacao, "amigo")) snprintf(rel, sizeof rel, "%s", i18n("Amigos"));
  else if (!strcmp(c->relacao, "enviado")) snprintf(rel, sizeof rel, "%s", i18n("Pedido enviado"));
  else if (!strcmp(c->relacao, "recebido")) snprintf(rel, sizeof rel, "%s", i18n("Quer ser seu amigo"));
  { float bloco = 48.0f + (l2[0] ? 34.0f : 0.0f) + (rel[0] ? 12.0f + BADGE_H : 0.0f);
    y = hy + (PE_AV_CARTAO - bloco) * 0.5f;
    nomeComSelo(TXT_ILHA_TITULO, l1, c->selo, tx, y, tw, 1.0f, a);
    y += 48.0f;
    if (l2[0]) { txt_desenhar_alpha(txtI(TXT_ILHA_CORPO, l2, tw), tx, y + 2.0f, a * 0.62f); y += 34.0f; }
    if (rel[0])
      badge_desenhar(tx, y + 12.0f, rel, !strcmp(c->relacao, "recebido") ? BADGE_REALCE_CHEIO
                                       : !strcmp(c->relacao, "amigo") ? BADGE_REALCE : BADGE_NEUTRO, a); }
  y = hy + PE_AV_CARTAO + 26.0f;
  if (c->bio[0]) {
    txt_bloco_corta(TXT_ILHA_CORPO, c->bio, 243, 242, 239, hx, y, PE_INTERNO, 32.0f, a * 0.88f, 2);
    y += 38.0f + (linhasBio(c->bio) > 1 ? 32.0f : 0.0f);
  }
  if (c->generos) y += selosGeneros(hx, y, PE_INTERNO, c->generos, a) + 18.0f;
  if (recomenda_cartao_n_recentes()) {
    char rec[300], tit[80];
    size_t k = (size_t)snprintf(rec, sizeof rec, "%s: ", i18n("Assistiu recentemente"));
    int q;
    for (q = 0; q < recomenda_cartao_n_recentes() && k + 90 < sizeof rec; q++)
      if (recomenda_cartao_recente(q, tit, sizeof tit))
        k += (size_t)snprintf(rec + k, sizeof rec - k, "%s%s", q ? "  \xc2\xb7  " : "", tit);
    txt_desenhar_alpha(txtI(TXT_ILHA_SUB, rec, PE_INTERNO), hx, y, a * 0.62f);
  }
}

// O RODAPE: a frase da ultima acao (visto no tom do acento, alerta em
// vermelho, anel enquanto vai) e as dicas de tecla DA LINHA FOCADA — "OK
// Adicionar · ← Ver perfil" numa pessoa, "OK Selecionar" num item. As teclas
// mudam com o foco porque respondem a "o que este OK faz agora?".
static void desenhaRodape(float x, float y, float a) {
  const char *k[4], *r[4];
  int n = 0;
  if (aviso[0]) {
    float ar, ag, ab;
    const char *ic = avisoErro ? "aj_triangle-alert" : "aj_circle-check";
    tomAcento(&ar, &ag, &ab);
    if (avisoErro) { ar = 1.0f; ag = 0.50f; ab = 0.44f; }
    if (opAtual && !avisoErro) { plrui_anel(x + 11.0f, y + 14.0f, 28.0f, 1, SDL_GetTicks(), a); ic = NULL; }
    if (ic) gfx_icone((GfxRect){ x, y + 3.0f, 22.0f, 22.0f }, ic, ar, ag, ab, a);
    txt_desenhar_alpha(txtI(TXT_ILHA_SUB, aviso, PE_INTERNO - 34.0f), x + 34.0f, y + 2.0f, a * 0.9f);
  }
  if (foco >= 0 && foco < nL && focavel(foco)) {
    const Linha *l = &linhas[foco];
    k[n] = "OK";
    if (l->tipo == T_PESSOA && coluna == 1 && temColunaAcao(foco)) {
      r[n++] = acaoDaPessoa(&l->p) == A_ACEITAR ? "Aceitar" : "Adicionar";
      k[n] = "\xe2\x86\x90"; r[n++] = "Ver perfil";
    } else if (l->tipo == T_PESSOA) {
      r[n++] = "Ver perfil";
      if (temColunaAcao(foco)) { k[n] = "\xe2\x86\x92";
                                 r[n++] = acaoDaPessoa(&l->p) == A_ACEITAR ? "Aceitar" : "Adicionar"; }
    } else if (l->tipo == T_BUSCA) r[n++] = "Digitar";
    else r[n++] = l->sw >= 0 ? "Alternar" : "Selecionar";
  }
  k[n] = "Voltar"; r[n++] = pagina == PG_MENU ? "Fechar" : "Voltar";
  plrui_dicas(k, r, n, x, y + 32.0f + 30.0f, 0, a);
}

static void pessoas_desenharCorpo_(Uint32 agora);
static float alturaIlha(void) {
  RecPessoa c;
  int temC = pagina == PG_CARTAO && recomenda_cartao(&c);
  float cab = alturaCab(&c, temC);
  return PE_PAD * 2.0f + cab + janelaH(cab) + PE_RODAPE;
}
#ifdef NV_TOUCH_UI
static void toqueRetomar(void) {
  if (toque.livre && nL > 0) {
    float ponto = rolagem + toque.regiao.h * 0.35f;
    int i = 0;
    while (i + 1 < nL && linhaY[i + 1] <= ponto) i++;
    foco = i; acertarFoco();
  }
  toquerol_limpar(&toque);
}
#endif
// Cartao de tela quase cheia: ampliado so se ainda couber (escala.h).
void pessoas_desenhar(Uint32 agora) {
  ESCALA_SE_COUBER_INI(PE_W, alturaIlha());
  pessoas_desenharCorpo_(agora);
  ESCALA_SE_COUBER_FIM();
}
static void pessoas_desenharCorpo_(Uint32 agora) {
  float a = anim_suave(anim), alt, x, y, cab, jh, hx, hy, ly;
  int i;
  RecPessoa c;
  int temC = 0;
  if (anim < 0.01f) return;

  if (pagina == PG_CARTAO) temC = recomenda_cartao(&c);
  cab = alturaCab(&c, temC);
  jh = janelaH(cab);
  alt = PE_PAD * 2.0f + cab + jh + PE_RODAPE;
  x = (NV_TELA_W - PE_W) * 0.5f;
  y = (NV_TELA_H - alt) * 0.5f + (1.0f - a) * 40.0f;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, (ajustes_vidro() ? 0.55f : 0.66f) * anim);
  // A ILHA no material das outras do Glass UI (plrui_material): o mesmo miolo
  // da folha de Fontes e do player, vidro ou solido pelo ajuste.
  plrui_material((GfxRect){ x, y, PE_W, alt }, PE_RAIO, 1, a);

  hx = x + PE_PAD; hy = y + PE_PAD;
  plrui_kicker(chapeuDe(), hx, hy, 243, 242, 239, a * 0.45f);
  hy += 20.0f + 14.0f;
  if (pagina == PG_CARTAO) {
    if (temC) desenhaCabCartao(hx, hy, &c, a);
  } else {
    TxtLinha t = txtI(TXT_ILHA_TITULO, tituloDe(), PE_INTERNO);
    txt_desenhar_alpha(t, hx, hy, a);
    hy += 50.0f;
    if (linhasSub(subDe()))
      txt_bloco_corta(TXT_ILHA_CORPO, subDe(), 243, 242, 239, hx, hy + 8.0f, PE_INTERNO, 32.0f, a * 0.62f, 2);
  }

  // A LISTA, recortada na janela e rolada pela mola.
  ly = y + PE_PAD + cab;
  gfx_recorte(x, ly - 8.0f, PE_W, jh + 16.0f);
  // So assentada (a mola da entrada passa de 1) e sem o teclado por cima.
  pnRegistrar = aberto && anim > 0.99f && anim < 1.01f && !teclado_aberto();
  pnTopo = ly - 8.0f; pnBase = ly + jh + 8.0f;
#ifdef NV_TOUCH_UI
  if (pnRegistrar) {
    toquerol_vincular(&toque, (GfxRect){ x, ly, PE_W, jh }, gfx_escala(), 0.0f, fmaxf(0.0f, conteudoH - jh), 1, &rolagem);
    ponteiro_rolagem(toqueRolar);
  }
#endif
  for (i = 0; i < nL; i++) {
    float ry = ly + linhaY[i] - rolagem, h = alturaLinha(&linhas[i]);
    if (ry + h < ly - 8.0f || ry > ly + jh + 8.0f) continue;
    if (pnRegistrar && focavel(i)) {
      GfxRect r = retLinha(hx, ry, h);
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, pnTopo, pnBase, ponteiroLinha, NULL, i, 0);
    }
    switch (linhas[i].tipo) {
      case T_PESSOA: desenhaPessoa(hx, ry, i, agora, a); break;
      case T_BUSCA:  desenhaBusca(hx, ry, i, a); break;
      case T_SECAO:  desenhaSecao(hx, ry, &linhas[i], a); break;
      case T_VAZIO:  desenhaVazio(hx, ry, &linhas[i], a); break;
      case T_ESQ:    desenhaEsq(hx, ry, a); break;
      default:       desenhaNav(hx, ry, i, a); break;
    }
  }
  gfx_sem_recorte();
  // Ha mais em cima/embaixo: o fio da borda da janela acende.
  if (rolagem > 1.0f)
    gfx_cor((GfxRect){ hx, ly - 8.0f, PE_INTERNO, 1.0f }, 0.0f, 1, 1, 1, 0.12f * a);
  if (rolagem + jh < conteudoH - 1.0f)
    gfx_cor((GfxRect){ hx, ly + jh + 8.0f, PE_INTERNO, 1.0f }, 0.0f, 1, 1, 1, 0.12f * a);

  pnRegistrar = 0;
  desenhaRodape(hx, y + alt - PE_PAD - PE_RODAPE + 18.0f, a);
  teclado_desenhar(agora);
}
