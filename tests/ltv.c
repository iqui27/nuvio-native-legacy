// tests/ltv.sh: modelo da Live TV por fonte (src/ltv.c) — arena, chaves de
// fonte, teto por plataforma, grupos quando a fonte passa do teto, numeracao
// da decisao 7, digitos, favoritos antigos/novos, preferencias e o M3U em
// fluxo (pedaco a pedaco, cabecalhos, url-tvg).
#include "../src/ltv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Disco dublado: as preferencias vao para uma string.
static char disco[8192];
char *dados_ler(const char *nome) { (void)nome; return disco[0] ? strdup(disco) : NULL; }
int dados_gravar(const char *nome, const char *c) { (void)nome; snprintf(disco, sizeof disco, "%s", c); return 1; }

static int falhas;
#define OK(c, m) do { if (c) printf("ok  %s\n", m); else { printf("FALHOU %s\n", m); falhas++; } } while (0)

typedef struct { int n; char nomes[64][80], grupos[64][80], urls[64][200], uas[64][80], ids[64][40]; int chno[64]; } Col;
static int colher(const LtvM3uCanal *c, void *u) {
  Col *k = u;
  if (k->n >= 64) return 0;
  snprintf(k->nomes[k->n], 80, "%s", c->nome);
  snprintf(k->grupos[k->n], 80, "%s", c->grupo);
  snprintf(k->urls[k->n], 200, "%s", c->url);
  snprintf(k->uas[k->n], 80, "%s", c->ua);
  snprintf(k->ids[k->n], 40, "%s", c->tvgId);
  k->chno[k->n] = c->chno;
  k->n++;
  return 1;
}

int main(void) {
  // --- arena
  { LtvArena a = {0};
    const char *x = ltv_dup(&a, "SporTV"), *v = ltv_dup(&a, ""), *y;
    int i;
    for (i = 0; i < 20000; i++) y = ltv_dup(&a, "um nome de canal comprido o bastante");
    OK(!strcmp(x, "SporTV") && v[0] == 0, "arena copia e vazio nao aloca");
    OK(!strcmp(y, "um nome de canal comprido o bastante") && !strcmp(x, "SporTV"),
       "ponteiro antigo continua valido depois de muitos blocos");
    OK(ltv_arena_bytes(&a) >= 20000u * 38u, "arena conta o reservado");
    ltv_arena_soltar(&a);
    OK(ltv_arena_bytes(&a) == 0, "soltar zera"); }

  // --- chaves
  { char a[24], b[24], c[24];
    ltv_chave_fonte(LTV_ADDON, "https://addon.um/x", a, sizeof a);
    ltv_chave_fonte(LTV_ADDON, "https://addon.dois/x", b, sizeof b);
    ltv_chave_fonte(LTV_M3U, "http://u:p@lista/get.php?user=u&pass=p", c, sizeof c);
    OK(a[0] == 'a' && strlen(a) == 9 && strcmp(a, b), "chave de addon por hash da base");
    OK(c[0] == 'm' && !strstr(c, "pass") && !strstr(c, "lista"), "chave de M3U nao carrega URL nem credencial");
    ltv_chave_fonte(LTV_XTREAM, "", a, sizeof a);
    OK(!strcmp(a, "xt"), "xtream = xt"); }

  // --- teto
  OK(ltv_teto_canais(0, 2200) == 30000 && ltv_teto_canais(3, 0) == 30000, "webOS/Android 30 mil");
  OK(ltv_teto_canais(2, 2500) == 20000, ".tpk 20 mil");
  OK(ltv_teto_canais(1, 0) == 8000, ".wgt 8 mil");
  OK(ltv_teto_canais(0, 1000) == 5000, "TV de 1 GB: 5 mil");

  // --- grupos
  { int n[5] = { 4000, 9000, 2500, 300, 7000 };
    const char *nomes[5] = { "RO | SPORT", "RO | FILME", "UK | NEWS", "Kids", "Adultos" };
    unsigned char e[5];
    int s = ltv_grupos_admitir(n, nomes, 5, NULL, 0, 30000, e);
    OK(s == 22800 && e[0] && e[4], "tudo cabe: todos entram");
    s = ltv_grupos_admitir(n, nomes, 5, NULL, 0, 8000, e);
    OK(s == 6800 && e[0] && !e[1] && e[2] && e[3] && !e[4],
       "passa do teto: grupos INTEIROS na ordem do provedor, pulando o que nao cabe");
    { const char *sel[2] = { "RO | FILME", "Kids" };
      s = ltv_grupos_admitir(n, nomes, 5, sel, 2, 20000, e);
      OK(s == 9300 && e[1] && e[3] && !e[0], "a escolha da pessoa manda"); }
    { const char *sel[1] = { "nao existe mais" };
      s = ltv_grupos_admitir(n, nomes, 5, sel, 1, 8000, e);
      OK(s == 6800 && e[0], "escolha que nao casa cai no padrao"); }
    { int um[1] = { 12000 }; const char *nm[1] = { "Tudo" };
      s = ltv_grupos_admitir(um, nm, 1, NULL, 0, 5000, e);
      OK(s == 5000 && e[0], "fonte de um grupo so nao vira zero"); } }

  // --- numeracao
  { LtvNum c[] = {
      { 0, 101, 0, 1 },   // addon A (segundo na barra)
      { 1, 101, 0, 0 },   // Xtream (primeiro na barra)
      { 1, 5, 1, 0 },     // Xtream, favorito
      { 0, 5, 0, 1 },
      { 0, 7, 0, 1 },
    };
    int amb = 0;
    OK(ltv_numero_achar(c, 5, 101, 0, &amb) == 0 && amb, "fonte filtrada: o numero dela, e ambiguo");
    OK(ltv_numero_achar(c, 5, 101, -1, &amb) == 1, "Todas: fontes na ordem da barra");
    OK(ltv_numero_achar(c, 5, 5, -1, &amb) == 2, "Todas: favoritos primeiro");
    OK(ltv_numero_achar(c, 5, 7, -1, &amb) == 4 && !amb, "numero unico nao e ambiguo");
    OK(ltv_numero_achar(c, 5, 7, 1, &amb) == -1, "numero que a fonte filtrada nao tem");
    OK(ltv_numero_achar(c, 5, 999, -1, &amb) == -1, "numero inexistente"); }

  // --- digitos
  { LtvDigitos g = {{0}, 0};
    int n = 0;
    OK(!ltv_dig_apertar(&g, 0, 100), "0 sozinho nao comeca (e o ultimo canal)");
    ltv_dig_apertar(&g, 1, 100); ltv_dig_apertar(&g, 0, 400); ltv_dig_apertar(&g, 2, 700);
    OK(ltv_dig_ativo(&g) && !strcmp(g.d, "102"), "1 0 2");
    OK(!ltv_dig_pronto(&g, 1500, &n), "antes do prazo nao confirma");
    OK(ltv_dig_pronto(&g, 700 + LTV_DIG_MS, &n) && n == 102 && !ltv_dig_ativo(&g), "prazo confirma 102");
    ltv_dig_apertar(&g, 9, 10);
    OK(ltv_dig_confirmar(&g, &n) && n == 9, "OK confirma na hora"); }

  // --- favoritos
  { char k[24], id[96];
    OK(ltv_fav_linha("cs:channel:abc\n", k, sizeof k, id, sizeof id) && !k[0] && !strcmp(id, "cs:channel:abc"),
       "favorito antigo: so o id, qualquer fonte");
    OK(ltv_fav_linha("a1b2c3d4\tcs:channel:abc", k, sizeof k, id, sizeof id) &&
       !strcmp(k, "a1b2c3d4") && !strcmp(id, "cs:channel:abc"), "favorito novo: chave TAB id");
    OK(!ltv_fav_linha("   \n", k, sizeof k, id, sizeof id), "linha vazia"); }

  // --- preferencias
  { LtvPref *p, v[4];
    const char *g[3];
    int n;
    disco[0] = 0;
    ltv_pref_carregar(2);
    p = ltv_pref("xt", 1);
    p->az = 1; p->fusoMin = -180; p->formato = 2; p->modo = 1;
    { const char *gs[2] = { "RO | SPORT", "Kids & Family" }; ltv_pref_definir_grupos(p, gs, 2); }
    ltv_pref_gravar();
    n = ltv_pref_de_texto(disco, v, 4);
    OK(n == 1 && v[0].az == 1 && v[0].fusoMin == -180 && v[0].formato == 2 && v[0].modo == 1 && v[0].proxy == -1,
       "preferencias ida e volta");
    ltv_pref_carregar(2);
    p = ltv_pref("xt", 0);
    n = p ? ltv_pref_grupos(p, g, 3) : 0;
    OK(n == 2 && !strcmp(g[0], "RO | SPORT") && !strcmp(g[1], "Kids & Family"), "grupos escolhidos voltam");
    OK(!ltv_pref("nada", 0), "fonte sem preferencia");
    n = ltv_pref_de_texto("xt\ttz=99999\n", v, 4);
    OK(n == 1 && v[0].fusoMin == 840, "fuso preso a +14 h"); }

  // --- M3U em fluxo
  { const char *m3u =
      "\xef\xbb\xbf#EXTM3U url-tvg=\"http://epg.exemplo/guide.xml.gz,http://outra\" x-tvg-url=\"x\"\r\n"
      "#EXTINF:-1 tvg-id=\"globo.br\" tvg-name=\"Globo\" tvg-logo=\"http://l/g.png?a=1\" group-title=\"Abertos\" tvg-chno=\"4\",Globo HD\r\n"
      "http://srv/live/u/p/1.ts|User-Agent=VLC/3&Referer=http://ref\r\n"
      "#EXTINF:-1 tvg-id=\"\" group-title=\"Esportes, ao vivo\",ESPN, Brasil\n"
      "#EXTVLCOPT:http-user-agent=Kodi\n"
      "http://srv/live/u/p/2.m3u8\n"
      "#EXTINF:0,Sem grupo\n"
      "#EXTGRP:Filmes\n"
      "http://srv/3\n"
      "http://url-orfa-sem-extinf\n"
      "#EXTINF:-1,Ultima sem quebra\n"
      "http://srv/4";
    LtvM3u p;
    Col c;
    size_t i, n = strlen(m3u);
    memset(&c, 0, sizeof c);
    ltv_m3u_iniciar(&p, colher, &c);
    for (i = 0; i < n; i += 7) ltv_m3u_alimentar(&p, m3u + i, n - i < 7 ? n - i : 7);
    ltv_m3u_fim(&p);
    OK(c.n == 4, "quatro canais (URL orfa fica de fora)");
    OK(!strcmp(p.urlTvg, "http://epg.exemplo/guide.xml.gz"), "url-tvg: a primeira URL");
    OK(!strcmp(c.nomes[0], "Globo HD") && !strcmp(c.ids[0], "globo.br") && c.chno[0] == 4 &&
       !strcmp(c.grupos[0], "Abertos"), "atributos do EXTINF");
    OK(!strcmp(c.urls[0], "http://srv/live/u/p/1.ts") && !strcmp(c.uas[0], "VLC/3"), "|User-Agent= vira cabecalho e sai da URL");
    OK(!strcmp(c.nomes[1], "ESPN, Brasil"), "nome depois da primeira virgula fora de aspas");
    OK(!strcmp(c.grupos[1], "Esportes, ao vivo") && !strcmp(c.uas[1], "Kodi"), "virgula dentro de aspas e EXTVLCOPT");
    OK(!strcmp(c.grupos[2], "Filmes"), "#EXTGRP");
    OK(!strcmp(c.nomes[3], "Ultima sem quebra") && !strcmp(c.urls[3], "http://srv/4"), "ultima linha sem quebra"); }

  // M3U de 20 mil canais em pedacos de 4 KB: memoria do parser fixa.
  { char *big = malloc(20000 * 120 + 64), *w;
    LtvM3u p;
    int i, n = 0;
    w = big + sprintf(big, "#EXTM3U\n");
    for (i = 0; i < 20000; i++)
      w += sprintf(w, "#EXTINF:-1 tvg-id=\"c%d\" group-title=\"G%d\",Canal %d\nhttp://s/%d.ts\n", i, i % 200, i, i);
    ltv_m3u_iniciar(&p, NULL, NULL);
    for (i = 0; i < (int)(w - big); i += 4096)
      ltv_m3u_alimentar(&p, big + i, (size_t)((w - big) - i < 4096 ? (w - big) - i : 4096));
    ltv_m3u_fim(&p);
    n = p.nCanais;
    OK(n == 20000, "M3U de 20 mil linhas em pedacos");
    free(big); }

  { char v[64];
    OK(ltv_m3u_atributo("#EXTINF:-1 TVG-ID=\"X\"", "tvg-id", v, sizeof v) && !strcmp(v, "X"), "atributo sem caixa");
    OK(!ltv_m3u_atributo("#EXTINF:-1 xtvg-id=\"X\"", "tvg-id", v, sizeof v), "nome de atributo inteiro"); }


  // --- fontes por URL (M9)
  { LtvUrl v[LTV_URLS_MAX];
    char h[64];
    int n;
    disco[0] = 0;
    ltv_urls_carregar(1);
    OK(ltv_urls_adicionar(LTV_URL_M3U, "", "http://u:SENHA@lista.exemplo:8080/get.php?username=u&password=SENHA") == 0,
       "M3U por URL entra");
    OK(ltv_urls_adicionar(LTV_URL_XMLTV, "Minha grade", " https://epg.exemplo/a.xml.gz \n") == 1, "XMLTV por URL entra (aparada)");
    OK(ltv_urls_adicionar(LTV_URL_M3U, "", "ftp://nao") < 0 && ltv_urls_adicionar(LTV_URL_M3U, "", "") < 0,
       "so http(s)");
    n = ltv_urls_copiar(v, LTV_URLS_MAX);
    OK(n == 2 && !strcmp(v[0].nome, "lista.exemplo:8080") && !strstr(v[0].nome, "SENHA"),
       "nome padrao = host, sem credencial");
    OK(!strcmp(v[1].url, "https://epg.exemplo/a.xml.gz") && v[1].tipo == LTV_URL_XMLTV, "tipo e URL aparada");
    ltv_urls_carregar(1);
    OK(ltv_urls_copiar(v, LTV_URLS_MAX) == 2, "ida e volta no disco");
    ltv_urls_remover(0);
    OK(ltv_urls_copiar(v, LTV_URLS_MAX) == 1 && v[0].tipo == LTV_URL_XMLTV, "remover");
    ltv_url_host("https://user:pw@h.exemplo/x?y", h, sizeof h);
    OK(!strcmp(h, "h.exemplo"), "host sem usuario/senha/caminho"); }
  printf("%s (%d falha(s))\n", falhas ? "FALHOU" : "tudo ok", falhas);
  return falhas ? 1 : 0;
}
