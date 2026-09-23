// DUBLE do tex_cache para tests/home.sh, que linka home.c sem SDL nem GL.
//
// arte_hero_do_item (home.c) pergunta ao cache se o still do episodio falhou
// e qual a largura dele. Em 226af57 o link passava sem isto: o -dead_strip
// levava o ramo de tela cheia embora. Com a escolha de fonte indo para
// artehero_url_destaque (22/09), `-Wl,-why_live,_arte_hero_do_item` mostra a
// funcao viva por arte_por_identidade <- main, e o link quebrava em
// _tex_falhou/_tex_largura_fonte antes de o teste rodar um caso — o que
// escondia a falha deliberada dele (nFileiras == 17). "Nada falhou, largura
// desconhecida" e o estado de um cache que ainda nao viu a url.
int tex_falhou(const char *caminho) { (void)caminho; return 0; }
int tex_largura_fonte(const char *caminho) { (void)caminho; return 0; }
