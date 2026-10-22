// O MODELO DO SOCIAL (socialvis.h) sem GL: agrupar por pessoa, "agora" na
// frente, um titulo por cartao, o feed sem SV_ATIVIDADE, os textos de status e
// a novidade que apaga ao ser vista e fica gravada.
#include "socialvis.h"
#include "dados.h"
#include "recomenda.h"
#include "ajustes.h"
#include "idioma.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static SvEvento ev(const char *pid, const char *nome, int acao, const char *imdb,
                   const char *titulo, long long quando) {
  SvEvento e;
  memset(&e, 0, sizeof e);
  snprintf(e.pessoaId, sizeof e.pessoaId, "%s", pid);
  snprintf(e.pessoaNome, sizeof e.pessoaNome, "%s", nome);
  e.acao = acao; e.reacao = SV_REAC_NADA; e.pct = -1; e.restanteMin = -1;
  snprintf(e.imdb, sizeof e.imdb, "%s", imdb);
  snprintf(e.titulo, sizeof e.titulo, "%s", titulo);
  e.quando = quando;
  return e;
}

int main(void) {
  long long agora = (long long)time(NULL);
  SvEvento v[8];
  char buf[200];
  int n = 0;
  dados_iniciar("deploy/app/art");
  ajustes_dir(dados_dir());
  v[n++] = ev("nuvio:p", "Pedro", SV_FIM, "tt1", "Um", agora - 3600);
  v[n++] = ev("nuvio:p", "Pedro", SV_INICIO, "tt2", "Dois", agora - 7200);
  v[n++] = ev("nuvio:p", "Pedro", SV_REACAO, "tt1", "Um", agora - 3500);   // mesmo titulo
  v[n++] = ev("trakt:m", "Marina", SV_ATIVIDADE, "tt3", "Tres", 0);       // nao entra no feed
  v[n++] = ev("nuvio:z", "Zeca", SV_AGORA, "tt4", "Quatro", agora - 60);
  v[n] = ev("nuvio:z", "Zeca", SV_AGORA, "tt4", "Quatro", 0);
  v[n].temporada = 3; v[n].episodio = 4; v[n].restanteMin = 12; v[n].pct = 64;
  socialvis_definir_feed(v, n);

  assert(socialvis_n_amigos() == 3);
  assert(!strcmp(socialvis_amigo(0)->id, "nuvio:z"));      // ao vivo na frente
  assert(socialvis_amigo(0)->agora == 1);
  assert(socialvis_amigo(1)->nTit == 2);                   // tt1 uma vez so
  assert(socialvis_n_ao_vivo() == 1);
  // Feed LIMPO: sem a SV_ATIVIDADE, sem o "comecou" de filme (tt2) e com a
  // reacao de tt1 dentro do "terminou" (uma linha por pessoa e titulo).
  assert(socialvis_n_eventos() == 2);
  assert(socialvis_evento(0)->acao == SV_AGORA);
  { int i;
    for (i = 0; i < socialvis_n_eventos(); i++) assert(socialvis_evento(i)->acao != SV_ATIVIDADE); }

  socialvis_status(&v[n], buf, sizeof buf);
  printf("status ao vivo: %s\n", buf);
  assert(strstr(buf, "3E4") && strstr(buf, "12"));
  socialvis_dia(&v[0], buf, sizeof buf);
  printf("dia: %s\n", buf);

  // A novidade: todo mundo nasce "novo"; vista, apaga e sobrevive a refazer.
  assert(socialvis_amigo(1)->novo == 1);
  socialvis_marcar_visto("nuvio:p");
  socialvis_definir_feed(v, n);
  assert(socialvis_amigo(socialvis_amigo_indice("nuvio:p"))->novo == 0);
  // Mudou o titulo mais novo: e novidade de novo.
  v[0].quando = agora - 10; snprintf(v[0].imdb, sizeof v[0].imdb, "tt9");
  socialvis_definir_feed(v, n);
  assert(socialvis_amigo(socialvis_amigo_indice("nuvio:p"))->novo == 1);

  { SvPerfil p;
    assert(socialvis_perfil("nuvio:p", &p));
    assert(p.nAssistindo == 1 && p.gostoPct == -1); }
  // A tracker rating has no known like/dislike value: it must not populate
  // Recently liked. Only a real positive reaction belongs there.
  v[0] = ev("nuvio:p", "Pedro", SV_AVALIOU, "tt20", "Rated", agora);
  v[1] = ev("nuvio:p", "Pedro", SV_REACAO, "tt21", "Liked", agora - 1);
  v[1].reacao = SV_REAC_GOSTOU;
  socialvis_definir_feed(v, 2);
  { SvPerfil p;
    assert(socialvis_perfil("nuvio:p", &p));
    assert(p.nGostou == 1 && !strcmp(p.gostou[0].imdb, "tt21")); }

  v[0] = ev("nuvio:p", "Pedro", SV_REACAO, "tt30", "Changed opinion", agora - 10);
  v[0].reacao = SV_REAC_GOSTOU;
  v[1] = v[0]; v[1].quando = agora; v[1].reacao = SV_REAC_NAO;
  socialvis_definir_feed(v, 2);
  { SvPerfil p;
    assert(socialvis_perfil("nuvio:p", &p) && p.nGostou == 0); }

  // Account/profile generation invalidates render-thread caches, including the
  // recommendation chain and previously seen activity, without sharing consent.
  socialvis_atualizar();
  { SvPerfil p = {0}; SvEnviada m;
    p.nMandou = 1;
    snprintf(p.mandou[0].imdb, sizeof p.mandou[0].imdb, "tt21");
    socialvis_definir_perfil_extra("nuvio:p", &p);
    assert(socialvis_ultima_enviada("nuvio:p", &m));
    recomenda_esquecer();
    socialvis_atualizar();
    assert(!socialvis_ultima_enviada("nuvio:p", &m));
    assert(socialvis_n_amigos() == 0 && socialvis_n_eventos() == 0); }
  // --- A LIMPEZA DO FEED (dono, 06/10/2026: "muito sujo") -----------------------
  { SvEvento in[12], out[12];
    int m = 0, k;
    char vb[120];
    in[m] = ev("nuvio:a", "Ana", SV_FIM, "tt50", "Serie", agora - 100); snprintf(in[m].tipo, 8, "series");
    in[m].temporada = 1; in[m].episodio = 3; m++;
    in[m] = ev("nuvio:a", "Ana", SV_REACAO, "tt50", "Serie", agora - 110); snprintf(in[m].tipo, 8, "series");
    in[m].reacao = SV_REAC_GOSTOU; m++;
    in[m] = ev("nuvio:a", "Ana", SV_FIM, "tt50", "Serie", agora - 200); snprintf(in[m].tipo, 8, "series");
    in[m].temporada = 1; in[m].episodio = 2; m++;
    in[m] = ev("nuvio:a", "Ana", SV_INICIO, "tt50", "Serie", agora - 250); snprintf(in[m].tipo, 8, "series");
    in[m].temporada = 1; in[m].episodio = 2; m++;
    in[m] = ev("nuvio:a", "Ana", SV_FIM, "tt50", "Serie", agora - 300); snprintf(in[m].tipo, 8, "series");
    in[m].temporada = 1; in[m].episodio = 1; m++;
    in[m] = ev("nuvio:a", "Ana", SV_ABANDONO, "tt51", "Parou", agora - 400); m++;
    in[m] = ev("nuvio:a", "Ana", SV_SALVO, "tt52", "Salvou", agora - 500); m++;
    in[m] = ev("nuvio:b", "Bia", SV_INICIO, "tt53", "Filme", agora - 600); snprintf(in[m].tipo, 8, "movie"); m++;
    in[m] = ev("nuvio:b", "Bia", SV_INICIO, "tt54", "Nova", agora - 700); snprintf(in[m].tipo, 8, "series");
    in[m].temporada = 1; in[m].episodio = 1; m++;
    in[m] = ev("nuvio:b", "Bia", SV_AVALIOU, "tt55", "Nota", agora - 800); in[m].nota = 80; m++;
    k = socialvis_feed_limpar(in, m, out, 12);
    assert(k == 3);
    assert(out[0].acao == SV_FIM && !strcmp(out[0].imdb, "tt50") && out[0].eps == 3);
    assert(out[0].reacao == SV_REAC_GOSTOU && out[0].episodio == 3 && out[0].quando == agora - 100);
    assert(out[1].acao == SV_INICIO && !strcmp(out[1].imdb, "tt54"));   // so a serie nova
    assert(out[2].acao == SV_AVALIOU && out[2].nota == 80);
    snprintf(vb, sizeof vb, "%s", socialvis_verbo(&out[0]));
    printf("verbo agrupado: %s\n", vb);
    assert(strstr(vb, "3"));
    assert(!strcmp(socialvis_verbo(&out[1]), i18n("começou a série")));
    // O mesmo episodio terminado duas vezes conta um.
    m = 0;
    in[m] = ev("nuvio:a", "Ana", SV_FIM, "tt60", "S", agora - 1); snprintf(in[m].tipo, 8, "series");
    in[m].temporada = 1; in[m].episodio = 1; m++;
    in[m] = in[0]; in[m].quando = agora - 2; m++;
    k = socialvis_feed_limpar(in, m, out, 12);
    assert(k == 1 && out[0].eps == 0); }
  printf("socialvis: ok\n");
  return 0;
}
