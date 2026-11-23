// Owner-approved Mac · perfis, 1440x900. Profile data and actions stay shared.
#include "desktop_native.h"
static void psDesktopDesenhar(Uint32 agora, float a, float subida) {
  float k = NV_TELA_W / 1440.0f, ky = NV_TELA_H / 900.0f;
  int m = perfis_n(), total = nFila();
  float d = diametro(total), gap = vaoDe(total, d);
  float x = (NV_TELA_W - total * d - (total - 1) * gap) * .5f;
  if (modoFundo() == PS_FUNDO_ARTE) {
    int layers[2]={ambAnt,ambAtual};
    for(int layer=0;layer<2;layer++) {
      int i=layers[layer];
      const ContaPerfil *p=i>=0 && i<m?perfis_item(i):NULL;
      if(!p || !p->fundoUrl[0] || !ambFonte[i]) continue;
      float opacity=layer?ambT:1;
      gfx_tex_aspect_atual=tex_aspecto(p->fundoUrl);
      gfx_rect((GfxRect){0,33*ky,NV_TELA_W,867*ky},ambFonte[i],GFX_FUNDO,
        .66f,1,0,0,12/255.f,14/255.f,19/255.f,a*opacity);
      gfx_tex_aspect_atual=0;
    }
  }
  desktop_native_profile_text(i18n("Quem está assistindo?"), NV_TELA_W * .5f,
    178 * ky + subida, 42 * k, 1, 1, a);
  if (!m) {
    desktop_native_profile_text(i18n(sync_estado() == SYNC_FALHOU ?
      "Não foi possível carregar os perfis. OK: tentar novamente" : "Carregando os perfis da sua conta…"),
      NV_TELA_W * .5f, 460 * ky, 18 * k, 0, .72f, a);
    if (sync_estado() == SYNC_FALHOU)
      ponteiro_alvo(160*k, 440*ky, NV_TELA_W-320*k, 80*ky, NULL, perfisRetentar, 0, 0);
    return;
  }
  desktop_native_profile_text(i18n("Cada perfil tem sua própria lista e seu progresso."),
    NV_TELA_W * .5f, 242 * ky + subida, 16 * k, 0, .78f, a);
  for (int i = 0; i < total; i++) {
    const ContaPerfil *p = i < m ? perfis_item(i) : NULL;
    float f = p ? animFoco[i] : animTile;
    float cx = x + i * (d + gap) + d * .5f;
    GfxRect av = {cx-d*.5f, filaY()+subida-2*k*f, d, d};
    if (p) {
      // Preserve static/GIF avatars through the shared decoder, without TV halo/lift.
      if (p->avatarUrl[0]) disco(av, p, f, a, i == foco && pinDe < 0);
      else {
        float r=.49f,g=.34f,b=1;
        char initial[8];
        corDe(p->corHex,&r,&g,&b); corLegivel(&r,&g,&b);
        float dim=.75f+.25f*f;
        gfx_rect(av,0,GFX_DISCO,0,0,0,0,r*dim,g*dim,b*dim,a);
        inicialDe(p->nome,initial,sizeof initial);
        desktop_native_profile_text(initial,cx,av.y+(d-67*k)*.5f,56*k,0,1,a);
      }
      // A fine neutral ring, never the TV-sized focus expansion.
      // Nonzero continuous-ring marker preserves selection in pointer mode.
      gfx_rect((GfxRect){av.x-2*k*f,av.y-2*k*f,d+4*k*f,d+4*k*f},
        0,GFX_ANEL,0,k*(1+f)/(d+4*k*f),.25f,.5f, .95f,.95f,.94f,a*(.12f+.88f*f));
      desktop_native_profile_text(p->nome,cx,filaY()+d+22*k+subida,18*k,0,1,a);
      if (!preparando && pinDe < 0) {
        ponteiro_alvo(av.x,av.y,d,d+120*k,perfilFocar,perfilAtivar,i,p->indice);
        if (p->temPin) desktop_native_profile_text("PIN",cx,filaY()+d+64*k,13*k,1,.72f,a);
        else if (contQuer) {
          if (contCard[i].tem) contDesenhar(i,cx,filaY()+d+70*k,1,a,d+gap,1);
          else desktop_native_profile_text(i18n("Nada a continuar"),cx,
            filaY()+d+64*k+subida,13*k,0,.72f,a);
        }
      }
      if (preparando && i == foco) giro(av,agora,a,ajustes_animacoes_reduzidas());
    } else {
      // Native macOS add button, sharing the exact C hit target and action.
      if (!preparando && pinDe < 0 && !perfisedit_aberto())
        desktop_native_profile_button(av,"+",1,f>.5f,a,tileAtivar,m,2);
      desktop_native_profile_text(i18n("Adicionar perfil"),cx,filaY()+d+22*k+subida,18*k,0,1,a);
      if (!preparando && pinDe < 0)
        ponteiro_alvo(av.x,av.y,d,d+55*k,tileFocar,tileAtivar,m,0);
    }
  }
  if (rodapeVisivel() && pinDe < 0 && !perfisedit_aberto()) {
    const char *labels[2] = {"Adicionar perfil","Gerenciar perfis"};
    for (int b=0;b<2;b++) {
      GfxRect r={(526+204*b)*k,680*ky,184*k,36*k};
      int enabled=b || podeAdicionar();
      desktop_native_profile_button(r,i18n(labels[b]),enabled,
        fase==1 && botao==b,a,botaoAtivar,b,b);
      if (enabled) ponteiro_alvo(r.x,r.y,r.w,r.h,botaoFocar,botaoAtivar,b,0);
    }
  }
  if (preparando) desktop_native_profile_text(i18n("Preparando o perfil…"),
    NV_TELA_W*.5f,680*ky,18*k,0,.85f,a);
  if (animPin > .004f) desenhaPin();
  if (perfisedit_aberto()) perfisedit_desenhar(agora);
}
