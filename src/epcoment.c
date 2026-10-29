#include "epcoment.h"
#include "ctxmenu.h"
#include "catalogo.h"
#include "extras.h"
#include "sinopse.h"
#include "descoberta.h"
#include "text.h"
#include "idioma.h"
#include "ajustes.h"
#include "js.h"
#include "notasui.h"
#include "anim.h"
#include "layout.h"
#include "ponteiro.h"
#include "tex_cache.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static char show[64];
static int temp, episodio, foco, dentro, quantidade, notaImdb, notaTmdb;
static float scroll;
static int filtro, cardCap;
static int rawIndex(int pos) {
  int n=extras_n_comentarios_ep();
  for (int i=0;i<n;i++) if (!filtro || extras_comentario_ep_nota(i)==filtro) {
    if (pos--==0) return i;
  }
  return -1;
}
// Byte ranges survive texture eviction. Rewrap only on content/font/width changes.
static struct Card {
  char texto[16384], data[80];
  unsigned short inicio[4], tamanho[4];
  int n, revelado;
  float comprimentos[4];
  float largura, escala;
  TxtFamilia familia;
} *cards;
static int reservar(void) {
  int n=extras_n_comentarios_ep();
  if (n>cardCap) {
    struct Card *v=realloc(cards,n*sizeof *v);
    if (!v) return 0;
    memset(v+cardCap,0,(n-cardCap)*sizeof *v); cards=v; cardCap=n;
  }
  return 1;
}

void epcoment_abrir(const char *imdb, int t, int e) {
  snprintf(show, sizeof show, "%s", imdb);
  temp = t; episodio = e; foco = dentro = 0;
  quantidade = -2; scroll = 0;
  filtro=0;
  if (cards) memset(cards, 0, cardCap*sizeof *cards);
  notaImdb = notaTmdb = 0;
  int item = cat_indice_por_imdb(show);
  for (int i = 0; item >= 0 && i < cat_n_episodios(item); i++) {
    const CatEp *ep = cat_episodio(item, i);
    if (ep && ep->temporada == t && ep->episodio == e) { notaImdb = ep->notaImdb; notaTmdb=ep->nota; break; }
  }
  if (ajustes_nota_episodio(EX_IMDB) && !notaImdb) {
    printf("[nuvio] episode IMDb %s S%dE%d: metadata has no per-episode IMDb rating\n", show, t, e);
    fflush(stdout);
  }
  extras_pedir_comentarios_ep(show, temp, episodio);
}
int epcoment_quantidade(void) {
  if (!extras_comentarios_ep_do(show, temp, episodio)) return -1;
  int total=extras_n_comentarios_ep(), n=0;
  if (!total && extras_comentarios_ep_carregando()) return -1;
  for (int i=0;i<total;i++) n += !filtro || extras_comentario_ep_nota(i)==filtro;
  return n;
}
void epcoment_atualizar(float dt) {
  // A previous episode's worker may still be finishing when this menu opens.
  if (!extras_comentarios_ep_do(show, temp, episodio) && !extras_comentarios_ep_carregando())
    extras_pedir_comentarios_ep(show, temp, episodio);
  int n=epcoment_quantidade();
  if (dentro && n>=0 && foco>=n-2 && extras_comentarios_ep_tem_mais()) extras_comentarios_ep_mais();
  reservar();
  scroll = ajustes_animacoes_reduzidas() ? foco * 280.0f : anim_mola(scroll, foco * 280.0f, dt, NV_MOLA_TELA);
}
int epcoment_foco(void) { return dentro ? foco : -1; }
void epcoment_menu(void) { dentro = 0; }
int epcoment_entrar(void) { if (!extras_n_comentarios_ep()) return 0; dentro = 1; return 1; }
void comentario_abrir_inteiro(int indice) {
  if (indice < 0 || indice >= epcoment_quantidade()) return;
  char data[64];
  foco = indice;
  int visible=indice+1;
  indice = rawIndex(indice);
  if (indice < 0) return;
  desc_data_extenso(extras_comentario_ep_data(indice), data, sizeof data);
  sinopse_comentario(extras_comentario_ep_usuario(indice), extras_comentario_ep_texto(indice), data,
                    extras_comentario_ep_nota(indice), extras_comentario_ep_curtidas(indice),
                    extras_comentario_ep_respostas(indice), 0, visible, epcoment_quantidade());
}
int epcoment_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!dentro || e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  // The menu is left of reviews. Cycle only ratings represented in the list.
  if (k == SDLK_LEFT) { epcoment_menu(); return 1; }
  if (k == SDLK_RIGHT) {
    if (!e->key.repeat) {
      int available[11] = {1};
      for (int i=0; i<extras_n_comentarios_ep(); i++) {
        int rating=extras_comentario_ep_nota(i);
        if (rating>=1 && rating<=10) available[rating]=1;
      }
      do { filtro = filtro ? filtro-1 : 10; } while (!available[filtro]);
      foco=0; scroll=0;
    }
    return 1;
  }
  if (k == SDLK_UP || k == SDLK_DOWN) {
    foco += k == SDLK_DOWN ? 1 : -1;
    if (foco >= epcoment_quantidade()) foco = epcoment_quantidade() - 1;
    if (foco < 0) foco = 0;
    return 1;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (!e->key.repeat) {
      int i=rawIndex(foco);
      if (i>=0 && reservar()) {
        if (extras_comentario_ep_spoiler(i) && !cards[i].revelado) cards[i].revelado=1;
        else comentario_abrir_inteiro(foco);
      }
    }
    return 1;
  }
  return 0; // Back belongs to the entire context menu, including fallback.
}
static void texto(TxtEstilo estilo, const char *s, float x, float y, float a) {
  txt_desenhar_alpha(txt_linha(estilo, s, 243, 242, 239, 255), x, y, a);
}
static void preparar(int i, float w) {
  const char *s = extras_comentario_ep_texto(i), *p;
  if (cards[i].n && cards[i].largura == w && cards[i].escala == gfx_escala() &&
      cards[i].familia == txt_fonte_interface() && !strcmp(cards[i].texto, s)) return;
  snprintf(cards[i].texto, sizeof cards[i].texto, "%s", s);
  cards[i].largura = w; cards[i].escala = gfx_escala(); cards[i].familia = txt_fonte_interface();
  cards[i].n = 0; p = cards[i].texto;
  while (*p && cards[i].n < 4) {
    char buf[16384]; size_t n = 0, quebra = 0;
    const char *inicio;
    while (*p == ' ' || *p == '\r') p++;
    if (!*p) break;
    inicio = p;
    while (*p && *p != '\n') {
      size_t k = 1;
      while (p[k] && ((unsigned char)p[k] & 0xc0) == 0x80) k++;
      if (n + k >= sizeof buf) break;
      memcpy(buf + n, p, k); buf[n + k] = 0;
      if (n && txt_largura(TXT_ILHA_TEXTO, buf) > w) break;
      if (*p == ' ') quebra = n;
      n += k; p += k;
    }
    if (*p && *p != '\n' && quebra) { n = quebra; p = inicio + quebra; }
    int j = cards[i].n++;
    cards[i].inicio[j] = (unsigned short)(inicio - cards[i].texto);
    cards[i].tamanho[j] = (unsigned short)n;
    memcpy(buf, inicio, n); buf[n] = 0;
    cards[i].comprimentos[j] = txt_largura(TXT_ILHA_TEXTO, buf);
    if (*p == '\n') p++;
    if (p == inicio && *p) p++;
  }
  if (*p && cards[i].n==4) cards[i].n=5;
  long long ms = js_ms_iso(extras_comentario_ep_data(i));
  int dias = ms ? (int)(((long long)time(NULL) - ms / 1000) / 86400) : 0;
  if (dias > 0) snprintf(cards[i].data, sizeof cards[i].data, i18n("há %d dias"), dias);
  else snprintf(cards[i].data, sizeof cards[i].data, "%s", i18n("Agora"));
}
static void focar(int i, int b) { (void)b; if (i >= 0 && i < epcoment_quantidade()) { foco = i; dentro = 1; } }
static void ativar(int i, int b) {
  (void)b; focar(i, 0);
  int raw=rawIndex(i);
  if (raw<0 || !reservar()) return;
  if (extras_comentario_ep_spoiler(raw) && !cards[raw].revelado) cards[raw].revelado=1;
  else comentario_abrir_inteiro(i);
}
void epcoment_desenhar(GfxRect r, float a) {
  ExEpNotas notas = {0}; char buf[120];
  int n = epcoment_quantidade();
  float x = r.x + 28, top = r.y + 270, w = r.w - 56;
  if (!reservar()) return;
  if (n >= 0) extras_notas_ep(&notas);
  int tmdb=extras_comentarios_ep_tmdb();
  if (notas.nota) snprintf(buf, sizeof buf, idioma_ponto_decimal(ajustes_idioma()) ? "%d.%d" : "%d,%d", notas.nota / 10, notas.nota % 10);
  else snprintf(buf, sizeof buf, "%s", "—");
  float ar,ag,ab; ajustes_acento(&ar,&ag,&ab);
  if (!tmdb) {
    float scoreW=220;
    TxtLinha score=txt_linha(TXT_AJ_NUM110,buf,243,242,239,255);
    float sx=x+(scoreW-score.w*.85f)*.5f;
    gfx_transformar(sx,r.y+44,.85f,0,0);
    txt_desenhar_alpha(score,sx,r.y+44,a);
    gfx_sem_transformar();
    int total=0;
    for (int k=1;k<=10;k++) total+=notas.distribuicao[k];
    // Show 10..5 to preserve the existing panel; shares include all ten buckets.
    float bx=x+266,bw=w-332;
    int high=filtro>0 && filtro<5 ? filtro+5 : 10;
    for (int k=high;k>=high-5;k--) {
      float y=r.y+22+(high-k)*28;
      int selected=k==filtro;
      snprintf(buf,sizeof buf,"%d",k);
      texto(TXT_G18M,buf,bx-18-txt_largura(TXT_G18M,buf),y,.85f*a);
      gfx_cor((GfxRect){bx,y+7,bw,12},.5f,1,1,1,.08f*a);
      float share=total ? (float)notas.distribuicao[k]/total : 0;
      if (share>0) gfx_cor((GfxRect){bx,y+7,bw*share,12},.5f,selected?ar:.65f,selected?ag:.67f,selected?ab:.71f,a);
      snprintf(buf,sizeof buf,"%d%%",(int)(share*100+.5f));
      texto(TXT_G18M,buf,x+w-txt_largura(TXT_G18M,buf),y,selected?a:.62f*a);
    }
  } else {
    texto(TXT_G28B,"TMDB",x,r.y+30,a);
    texto(TXT_ILHA_TEXTO,i18n("TMDB · reviews da série"),x,r.y+85,.8f*a);
  }
  // Bottom row: the title page's rating line on the left, keycaps on the right.
  // "→" shows the current filter, so the old separate chip is gone.
  if (filtro) snprintf(buf,sizeof buf,i18n("Nota %d/10"),filtro);
  else snprintf(buf,sizeof buf,"%s",i18n("Todas"));
  const char *keys[2][2]={{"→",buf},{"←",i18n("Menu")}};
  float kx=x+w;
  for (int i=0;i<2;i++) {
    float kw=txt_largura(TXT_G18M,keys[i][0])+txt_largura(TXT_G18R,keys[i][1])+34;
    kx-=kw;
    if (i==0 && filtro) gfx_cor((GfxRect){kx,r.y+208,kw,30},.5f,ar,ag,ab,.28f*a);
    else gfx_cor((GfxRect){kx,r.y+208,kw,30},.5f,1,1,1,.07f*a);
    texto(TXT_G18M,keys[i][0],kx+12,r.y+211,.8f*a);
    texto(TXT_G18R,keys[i][1],kx+22+txt_largura(TXT_G18M,keys[i][0]),r.y+211,(i==0?.95f:.72f)*a);
    kx-=10;
  }
  if (!tmdb) {
    int cru[EX_NFONTES]={0}; unsigned char querer[EX_NFONTES]={0};
    cru[EX_TRAKT]=notas.nota*10; querer[EX_TRAKT]=1;
    cru[EX_IMDB]=notaImdb; querer[EX_IMDB]=ajustes_nota_episodio(EX_IMDB);
    cru[EX_TMDB]=notaTmdb*10; querer[EX_TMDB]=ajustes_nota_episodio(EX_TMDB);
    NotasPlano plano; notasui_planejar(&plano,cru,kx-x-10,0,querer);
    notasui_desenhar_linha(&plano,x,r.y+223,a);
  }
  gfx_cor((GfxRect){x, top - 16, w, 1}, 0, 1, 1, 1, .12f * a);
  if (n <= 0) {
    texto(TXT_ILHA_TEXTO, n < 0 ? i18n("Carregando comentários…") : filtro ? i18n("Nenhum comentário com esta nota.") : i18n("Nenhum comentário ainda."), x + 20, top + 34, .75f * a);
    if (extras_comentarios_ep_carregando()) texto(TXT_G18R,i18n("Carregando mais comentários…"),x+20,top+90,.7f*a);
    return;
  }
  if (quantidade != n) { quantidade = n; if (foco >= n) foco = 0; }
  gfx_recorte(x - 4, top - 4, w + 8, r.h - 290);
  // The worker may append a page after reservar(): draw only reserved cards.
  int pos=-1, total=extras_n_comentarios_ep();
  if (total > cardCap) total = cardCap;
  for (int i=0; i<total; i++) {
    if (filtro && extras_comentario_ep_nota(i)!=filtro) continue;
    pos++;
    float y = top + pos * 280 - scroll;
    float h = 280;
    if (y + h < top || y > r.y + r.h) continue;
    preparar(i, w - 44);
    int ativo = dentro && foco == pos;
    float ca = a * (foco == pos ? 1 : .65f);
    GfxRect card = {x, y, w, h};
    if (ativo) {
      gfx_cor(card, 18 / h, .21f, .216f, .235f, a);

    }
    if (y >= top && y + h <= r.y + r.h - 20)
      ponteiro_alvo(card.x, card.y, card.w, card.h, focar, ativar, pos, 0);
    const char *user = extras_comentario_ep_usuario(i);
    char inicial[8] = "?"; size_t k = 1;
    if (*user) { while (user[k] && ((unsigned char)user[k] & 0xc0) == 0x80 && k < 7) k++; memcpy(inicial, user, k); inicial[k] = 0; }
    gfx_cor((GfxRect){x + 24, y + 28, 44, 44}, .5f, i % 2 ? .15f : .35f, i % 2 ? .65f : .47f, i % 2 ? .43f : .98f, ca);
    texto(TXT_G21B, inicial, x + 37, y + 36, ca);
    txt_desenhar_alpha(txt_linha_corta(TXT_G21B, user, 243, 242, 239, 255, w - 220), x + 82, y + 24, ca);
    texto(TXT_G18R, cards[i].data, x + 82, y + 53, .6f * ca);
    int nota = extras_comentario_ep_nota(i);
    if (nota) {
      snprintf(buf, sizeof buf, "♥ %d/10", nota);
      txt_desenhar_alpha(txt_linha(TXT_G21B, buf, 255, 78, 108, 255), x + w - 105, y + 40, ca);
    }
    int hidden = extras_comentario_ep_spoiler(i) && !cards[i].revelado;
    for (int j = 0; j < cards[i].n && j < 4; j++) {
      char l[16384]; int len = cards[i].tamanho[j];
      memcpy(l, cards[i].texto + cards[i].inicio[j], len); l[len] = 0;
      if (j == 3 && j + 1 < cards[i].n) snprintf(l + len, sizeof l - len, "…");
      float ty = y + 94 + j * 31;
      // Conceal every glyph with soft overlapping strokes; no spoiler texture is exposed.
      if (hidden) {
        float width = cards[i].comprimentos[j];
        for (int b = 0; b < 3; b++) gfx_cor((GfxRect){x + 24 - b, ty + 7 - b * 2, width + b * 2, 9 + b * 4}, .5f, .75f, .76f, .79f, .08f * ca);
      } else texto(TXT_ILHA_TEXTO, l, x + 24, ty, .87f * ca);
    }
    snprintf(buf, sizeof buf, "♥ %d", extras_comentario_ep_curtidas(i));
    texto(TXT_G18R, buf, x + 24, y + h - 44, .58f * ca);
    gfx_icone((GfxRect){x + 116, y + h - 42, 19, 19}, "aj_message-square", 1, 1, 1, .58f * ca);
    snprintf(buf, sizeof buf, "%d", extras_comentario_ep_respostas(i));
    texto(TXT_G18R, buf, x + 142, y + h - 44, .58f * ca);
  }
  gfx_sem_recorte();
  if (extras_comentarios_ep_carregando()) {
    gfx_cor((GfxRect){x,r.y+r.h-44,w,34},.5f,.08f,.085f,.10f,a);
    texto(TXT_G18R,i18n("Carregando mais comentários…"),x+16,r.y+r.h-39,.85f*a);
  }
  float track = r.h - 298, thumb = track / n;
  gfx_cor((GfxRect){r.x + r.w - 16, top, 4, track}, 2 / track, 1, 1, 1, .14f * a);
  float fraction = n > 1 ? foco / (float)(n - 1) : 0;
  gfx_cor((GfxRect){r.x + r.w - 16, top + fraction * (track - thumb), 4, thumb}, 2 / thumb, 1, 1, 1, .9f * a);
}
