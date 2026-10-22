// Cartoes da ilha — ver ilhacart.h.
#include "ilhacart.h"
#include "avisodisp.h"
#include "ilha.h"
#include "agenda.h"
#include "ajustes.h"
#include "avisos.h"
#include "catalogo.h"
#include "home.h"
#include "idioma.h"
#include "recomenda.h"
#include "perfis.h"
#include "sessao.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define VIVO_OCIOSO_MS (30u * 60u * 1000u)
#define ESTREIA_SONDA_MS 1000u

static IlhaCartao vivo, estreia;
static int temVivo, temEstreia;
static Uint32 ultimaTecla, ultimaSonda;
static unsigned vivoSeq;
static char vivoConta[96];
static int vivoPerfil;

unsigned ilhacart_vivo_seq(void) { return vivoSeq; }
const char *ilhacart_vivo_imdb(void) { return temVivo ? vivo.imdb : ""; }

void ilhacart_esquecer_vivo(void) {
  temVivo = 0; vivoConta[0] = 0; vivoPerfil = 0;
  memset(&vivo, 0, sizeof vivo);
  ilha_cartao_invalidar(ILHA_VIVO);
}

void ilhacart_validar_identidade(void) {
  // O modal/pedido pode ainda ter a copia depois de a pilula ficar ociosa.
  // A identidade capturada continua valendo ate sua invalidacao explicita.
  if (vivoConta[0] && (!sessao_logada() || vivoPerfil != perfis_ativo() ||
                  strcmp(vivoConta, sessao_usuario())))
    ilhacart_esquecer_vivo();
}

int ilhacart_vivo_vale(const IlhaCartao *c) {
  ilhacart_validar_identidade();
  return c && vivoConta[0] && !strcmp(c->chave, vivo.chave) &&
         !strcmp(c->imdb, vivo.imdb) && c->serie == vivo.serie &&
         c->t == vivo.t && c->e == vivo.e;
}

// O episodio (T, E) na lista que o catalogo ja tem desse titulo: nome, sinopse
// e o still. Sem lista (serie que nunca abriu nesta sessao), fica o do titulo.
static void doEpisodio(int idx, int t, int e, IlhaCartao *c) {
  int i, n = idx >= 0 ? cat_n_episodios(idx) : 0;
  for (i = 0; i < n; i++) {
    const CatEp *ep = cat_episodio(idx, i);
    if (ep && ep->temporada == t && ep->episodio == e) {
      if (ep->nome[0]) snprintf(c->epNome, sizeof c->epNome, "%s", ep->nome);
      if (ep->sinopse[0]) snprintf(c->sinopse, sizeof c->sinopse, "%s", ep->sinopse);
      if (ep->thumb[0]) snprintf(c->arte, sizeof c->arte, "%s", ep->thumb);
      return;
    }
  }
}

static void doTitulo(const CatItem *ci, IlhaCartao *c) {
  snprintf(c->imdb, sizeof c->imdb, "%s", ci->imdb);
  snprintf(c->titulo, sizeof c->titulo, "%s", ci->titulo);
  snprintf(c->poster, sizeof c->poster, "%s", ci->poster);
  snprintf(c->logo, sizeof c->logo, "%s", ci->logo);
  c->serie = !strcmp(ci->tipo, "series");
}

void ilhacart_player_saiu(int indice, double posSeg, double durSeg, int t, int e) {
  ilhacart_validar_identidade();
  const CatItem *ci = sessao_logada() && sessao_usuario()[0] &&
                     home_retorno_vale(indice, posSeg, durSeg) ? cat_item(indice) : NULL;
  if (!ci || !ci->imdb[0]) { temVivo = 0; ilha_cartao(ILHA_VIVO, NULL); return; }
  memset(&vivo, 0, sizeof vivo);
  doTitulo(ci, &vivo);
  if (vivo.serie && t > 0 && e > 0) {
    vivo.t = t; vivo.e = e;
    if (t == ci->temporada && e == ci->episodio && ci->nomeEpisodio[0])
      snprintf(vivo.epNome, sizeof vivo.epNome, "%s", ci->nomeEpisodio);
    doEpisodio(indice, t, e, &vivo);
  }
  if (!vivo.arte[0]) snprintf(vivo.arte, sizeof vivo.arte, "%s", ci->backdrop);
  if (!vivo.sinopse[0]) snprintf(vivo.sinopse, sizeof vivo.sinopse, "%s", ci->sinopse);
  vivo.progresso = (float)(posSeg / durSeg);
  vivo.restanteMin = (int)((durSeg - posSeg) / 60.0 + 0.5);
  vivoSeq++;
  // A copia do modal e desta instancia, nao apenas do IMDb/episodio. Duas
  // contas vendo o mesmo episodio nunca tornam um pedido antigo valido.
  snprintf(vivo.chave, sizeof vivo.chave, "vivo:%u", vivoSeq);
  temVivo = 1;
  snprintf(vivoConta, sizeof vivoConta, "%s", sessao_usuario());
  vivoPerfil = perfis_ativo();
  ultimaTecla = SDL_GetTicks();
  ilha_cartao(ILHA_VIVO, &vivo);
  printf("[ilha] atividade ao vivo: %s T%dE%d %.0f%%, faltam %d min\n", vivo.imdb, vivo.t,
         vivo.e, vivo.progresso * 100.0f, vivo.restanteMin);
}

void ilhacart_tecla(Uint32 agora) { ultimaTecla = agora; }

// --- AMIGO VENDO AGORA (02/10; passageiro desde 06/10) ---------------------------
// Os eventos de INICIO de AMIGOS (grau 1) com menos de 15 min no feed — a
// mesma regra de RecAmigo.temAgora (recomenda.h). Viram UM AVISO PASSAGEIRO
// e mais nada: "Ana está vendo Severance" (um so) ou "Ana e mais 2 estão
// assistindo" (varios), que some sozinho em poucos segundos.
//
// ANTES havia tambem o TERCEIRO CARTAO ao lado do relogio, que ficava enquanto
// o evento valesse; com varios amigos vendo coisas ele trocava de um para o
// outro e NUNCA saia da pilula (relato do dono, 06/10: "nao precisa persistir
// na ilha: so mostra e some"). O cartao saiu; quem quer saber o que os amigos
// estao vendo tem a aba Atividade e a fileira "Entre amigos" da Home.
//
// UMA VEZ POR AMIGO + TITULO, EM DISCO (avisodisp, chave "vendo:<pessoa>:<imdb>"):
// o mesmo amigo no mesmo titulo nao volta a avisar nesta sessao nem depois de
// reiniciar — no episodio seguinte da mesma serie tambem nao. Titulo novo,
// aviso novo. OK no aviso abre a aba Atividade (no titulo, quando e um so).
//
// SO O QUE DIZ ALGO (dono, 06/10/2026: "deixar a TV so quando assistiu ou
// curtiu"): alem do comeco de uma SERIE, o aviso sai para terminou / gostou /
// nao gostou / nota (ilhacart_noticia), cada um uma vez por amigo + titulo.
// Filme comecado, saida do player ("parou") e salvo: nada.
#define AMIGO_AGORA_S   (15 * 60)
#define AMIGO_SONDA_MS  5000u
#define AMIGO_ILHA_MS   6000u
#define AMIGO_MAX_EV    12
static Uint32 amigoSonda;
static char vendoPedido[32];
static int temVendoPedido;

void ilhacart_vendo_acao(const char *chave) {
  if (!chave || strncmp(chave, "vendo:", 6)) return;
  snprintf(vendoPedido, sizeof vendoPedido, "%s", chave + 6);
  temVendoPedido = 1;
}

int ilhacart_pediu_atividade(char *imdb, size_t tam) {
  if (!temVendoPedido) return 0;
  temVendoPedido = 0;
  if (imdb && tam) snprintf(imdb, tam, "%s", vendoPedido);
  return 1;
}

// Chave de disco do par amigo + titulo. O id da pessoa pode ser longo
// ("nuvio:<uuid>:<perfil>"); o teto da chave (AVD_CHAVE) e respeitado cortando
// o id pelo comeco, que e o que varia menos.
void ilhacart_vendo_chave(char *dst, size_t tam, const char *pessoa, const char *imdb) {
  size_t lp = strlen(pessoa ? pessoa : "");
  const char *p = pessoa ? pessoa : "";
  if (lp > 52) p += lp - 52;
  snprintf(dst, tam, "vendo:%s:%.20s", p, imdb ? imdb : "");
}

int ilhacart_noticia(const RecEvento *ev, long long agora, char *txt, size_t tam) {
  char f1[96], f2[200];
  const char *nome, *tit;
  if (tam) txt[0] = 0;
  if (!ev || !tam || ev->grau > 1 || !ev->imdb[0] || ev->quando <= 0) return 0;
  if (agora - ev->quando > AMIGO_AGORA_S || agora - ev->quando < -60) return 0;
  nome = ilha_forte(f1, sizeof f1, ev->pessoaNome[0] ? ev->pessoaNome : "?");
  tit = ilha_forte(f2, sizeof f2, ev->titulo[0] ? ev->titulo : "?");
  switch (ev->acao) {
    case REC_ACAO_INICIO:
      // O servidor novo so manda o comeco de uma serie; o antigo manda todo
      // "comecou" — filme comecado nao e noticia.
      if (strcmp(ev->midia, "series")) return 0;
      snprintf(txt, tam, i18n("%s está vendo %s"), nome, tit);
      return 1;
    case REC_ACAO_FIM: {
      int r = ev->temReacFim ? ev->reacFim : 2;
      if (ev->eps > 1)
        snprintf(txt, tam, i18n("%s viu %d episódios de %s"), nome, ev->eps, tit);
      else if (r == 1) snprintf(txt, tam, i18n("%s terminou %s e gostou"), nome, tit);
      else if (r == -1) snprintf(txt, tam, i18n("%s terminou %s e não gostou"), nome, tit);
      else snprintf(txt, tam, i18n("%s terminou %s"), nome, tit);
      return 2; }
    case REC_ACAO_REACAO:
      if (ev->reacao > 0) snprintf(txt, tam, i18n("%s gostou de %s"), nome, tit);
      else if (ev->reacao < 0) snprintf(txt, tam, i18n("%s não gostou de %s"), nome, tit);
      else snprintf(txt, tam, i18n("%s achou %s mais ou menos"), nome, tit);
      return 2;
    case REC_ACAO_NOTA:
      if (ev->nota <= 0) return 0;
      snprintf(txt, tam, i18n("%s deu %d/10 para %s"), nome, (ev->nota + 5) / 10, tit);
      return 2;
    default:
      return 0;          // abandono (saiu do player), salvo: nao e aviso
  }
}

// A chave em disco de uma NOTICIA (terminou/reagiu/nota) do amigo no titulo:
// a mesma forma de ilhacart_vendo_chave, com outro prefixo por tipo de fato.
static void noticiaChave(char *dst, size_t tam, int acao, const char *pessoa, const char *imdb) {
  char v[AVD_CHAVE];
  const char *pre = acao == REC_ACAO_FIM ? "fim:" : acao == REC_ACAO_REACAO ? "rea:" : "not:";
  ilhacart_vendo_chave(v, sizeof v, pessoa, imdb);
  snprintf(dst, tam, "%s%s", pre, v + 6);
}

static void amigoAtualizar(Uint32 agora) {
  RecEvento ev, novos[AMIGO_MAX_EV];
  char vistas[AMIGO_MAX_EV][96], textos[AMIGO_MAX_EV][240];
  int tipos[AMIGO_MAX_EV];
  int i, j, n, nNovos = 0, nVistas = 0, soVendo = 1;
  long long t = (long long)time(NULL);
  if (amigoSonda && agora - amigoSonda < AMIGO_SONDA_MS) return;
  amigoSonda = agora ? agora : 1;
  n = recomenda_ativo() ? recomenda_feed_n() : 0;
  for (i = 0; i < n && i < AMIGO_MAX_EV; i++) {
    char chave[AVD_CHAVE], txt[240];
    int dup = 0, tipo;
    if (!recomenda_feed_item(i, &ev)) continue;
    tipo = ilhacart_noticia(&ev, t, txt, sizeof txt);
    if (!tipo) continue;
    // Um aviso por PESSOA nesta volta: o evento mais novo dela (o feed vem do
    // mais novo para o mais velho).
    for (j = 0; j < nVistas; j++) if (!strcmp(vistas[j], ev.pessoa)) dup = 1;
    if (dup) continue;
    snprintf(vistas[nVistas++], sizeof vistas[0], "%s", ev.pessoa);
    if (tipo == 1) ilhacart_vendo_chave(chave, sizeof chave, ev.pessoa, ev.imdb);
    else noticiaChave(chave, sizeof chave, ev.acao, ev.pessoa, ev.imdb);
    if (avisodisp_tem(chave)) continue;
    avisodisp_por(chave);
    if (tipo != 1) soVendo = 0;
    tipos[nNovos] = tipo;
    snprintf(textos[nNovos], sizeof textos[0], "%s", txt);
    novos[nNovos++] = ev;
  }
  if (!nNovos) return;
  { char txt[240], f1[96], meta[24] = "", chave[32];
    IlhaAvisoEx e;
    const RecEvento *p = &novos[0];
    const char *nome = p->pessoaNome[0] ? p->pessoaNome : "?";
    memset(&e, 0, sizeof e);
    if (nNovos == 1 || !soVendo) {
      // Um so, ou noticias misturadas: a frase do mais novo ("Ana terminou X e
      // gostou"); o resto conta no "+N" e esta inteiro na aba Atividade.
      snprintf(txt, sizeof txt, "%s", textos[0]);
      if (nNovos > 1) snprintf(meta, sizeof meta, "+%d", nNovos - 1);
      else if (p->temporada > 0 && p->episodio > 0 && !(p->acao == REC_ACAO_FIM && p->eps > 1))
        snprintf(meta, sizeof meta, i18n("T%dE%d"), p->temporada, p->episodio);
      e.capa = p->poster[0] ? p->poster : "-"; e.meta = meta; e.vivo = tipos[0] == 1;
      snprintf(chave, sizeof chave, "vendo:%.24s", nNovos == 1 ? p->imdb : "");
    } else {
      if (nNovos == 2) snprintf(txt, sizeof txt, i18n("%s e mais 1 estão assistindo"), ilha_forte(f1, sizeof f1, nome));
      else snprintf(txt, sizeof txt, i18n("%s e mais %d estão assistindo"), ilha_forte(f1, sizeof f1, nome), nNovos - 1);
      snprintf(chave, sizeof chave, "vendo:");
    }
    e.chave = chave; e.tipo = ILHA_INFO; e.texto = txt; e.ms = AMIGO_ILHA_MS;
    e.rosto = p->pessoaAvatar; e.rostoNome = nome;
    e.acao = 1;
    ilha_avisar_ex(&e);
    printf("[ilha] amigos: %d aviso(s) novo(s)\n", nNovos); }
}

void ilhacart_dispensar(int qual) {
  if (qual == ILHA_VIVO) { temVivo = 0; ilha_cartao(ILHA_VIVO, NULL); return; }
  if (qual == ILHA_AMIGO) { ilha_cartao(ILHA_AMIGO, NULL); return; }
  // "Dispensar" (06/10): o episodio sai da lista de Avisos e nao volta, nem
  // depois de reiniciar (avisodisp.h). O lembrete da serie continua ligado.
  if (temEstreia) avisos_dispensar(estreia.avisoId);
  temEstreia = 0;
  ilha_cartao(ILHA_ESTREIA, NULL);
}

void ilhacart_adiar(int qual) {
  if (qual != ILHA_ESTREIA) { ilhacart_dispensar(qual); return; }
  // "Depois": so nesta sessao. avisos_estreia_pendente pula o adiado, entao a
  // proxima sonda nao o repoe; o item fica nao lido na lista.
  if (temEstreia) avisodisp_sessao_por(estreia.avisoId);
  temEstreia = 0;
  ilha_cartao(ILHA_ESTREIA, NULL);
}

static void montarEstreia(const char *id, const char *imdb) {
  const AgItem *ag = agenda_registro(imdb);
  int idx = cat_indice_por_imdb(imdb);
  const CatItem *ci = idx >= 0 ? cat_item(idx) : NULL;
  memset(&estreia, 0, sizeof estreia);
  snprintf(estreia.avisoId, sizeof estreia.avisoId, "%s", id);
  snprintf(estreia.imdb, sizeof estreia.imdb, "%s", imdb);
  estreia.progresso = -1.0f;
  if (ci) doTitulo(ci, &estreia);
  estreia.serie = 1;
  // O registro da agenda vale ate a proxima escrita: copiado aqui, na hora.
  if (ag) {
    if (!estreia.titulo[0]) snprintf(estreia.titulo, sizeof estreia.titulo, "%s", ag->titulo);
    if (!estreia.poster[0]) snprintf(estreia.poster, sizeof estreia.poster, "%s", ag->poster);
    estreia.t = ag->temporada; estreia.e = ag->episodio;
    snprintf(estreia.epNome, sizeof estreia.epNome, "%s", ag->nomeEp);
    snprintf(estreia.sinopse, sizeof estreia.sinopse, "%s", ag->sinopse);
    if (agenda_dias(ag->dataProx) == 0)
      snprintf(estreia.quando, sizeof estreia.quando, "%s", i18n("hoje"));
  }
  if (idx >= 0 && estreia.t > 0 && estreia.e > 0) doEpisodio(idx, estreia.t, estreia.e, &estreia);
  if (!estreia.arte[0] && ci) snprintf(estreia.arte, sizeof estreia.arte, "%s", ci->backdrop);
  if (!estreia.sinopse[0] && ci) snprintf(estreia.sinopse, sizeof estreia.sinopse, "%s", ci->sinopse);
  snprintf(estreia.chave, sizeof estreia.chave, "estreia:%s", id);
}

void ilhacart_atualizar(Uint32 agora, const char *imdbAberto) {
  ilhacart_validar_identidade();
  if (temVivo && agora - ultimaTecla > VIVO_OCIOSO_MS) {
    printf("[ilha] atividade ao vivo saiu: 30 min sem tecla\n");
    temVivo = 0;
    ilha_cartao(ILHA_VIVO, NULL);
  }
  amigoAtualizar(agora);
  if (ultimaSonda && agora - ultimaSonda < ESTREIA_SONDA_MS) return;
  ultimaSonda = agora ? agora : 1;
  { char id[72], imdb[64];
    int ha = ajustes_relogio_ligado() && avisos_estreia_pendente(id, sizeof id, imdb, sizeof imdb);
    // A PAGINA DO TITULO ABERTA E "VI": o aviso ja cumpriu o papel.
    if (ha && imdbAberto && !strcmp(imdbAberto, imdb)) {
      avisos_marcar_visto(id);
      ha = 0;
    }
    if (!ha) {
      if (temEstreia) { temEstreia = 0; ilha_cartao(ILHA_ESTREIA, NULL); }
      return;
    }
    // Remonta a cada sonda: o catalogo ou a lista de episodios podem ter
    // chegado depois (logo, still). A chave so muda se o aviso mudar.
    montarEstreia(id, imdb);
    temEstreia = 1;
    ilha_cartao(ILHA_ESTREIA, &estreia); }
}
