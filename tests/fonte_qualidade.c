// R9 (04/10): "a escolha automatica pegou o menor e de pior resolucao em todos
// os filmes". Reproduz a lista do dono — debrid/Torrentio 4k, 1080p, 720p mais
// um plugin MegaEmbed 1080 MP4 sem tamanho — e fixa quem vence em cada estado
// do orcamento do StreamFit. Sem rede, sem GL: a lista em memoria e o
// stream_automatico (a mesma pontuacao da verificacao e da folha).
#include "streams.h"
#include "streamfit.h"
#include "ajustes.h"
#include "badges.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void fonte(Stream *s, const char *prov, const char *rot, int altura, int mp4,
                  int foraCache, unsigned long long bytes, const char *host) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rot);
  snprintf(s->url, sizeof s->url, "https://%s/v/%s.%s", host, rot, mp4 ? "mp4" : "mkv");
  s->altura = altura; s->mp4 = mp4; s->foraCache = foraCache;
  s->tamanhoBytes = bytes; s->fileIdx = -1;
}
static void plugin(Stream *s, const char *rot, int altura) {
  fonte(s, "MegaEmbed", rot, altura, 1, 0, 0, "streamtape.invalid");
  snprintf(s->bingeGroup, sizeof s->bingeGroup, "nuvio-plugin|megaembed|%d", altura);
}
static char dirAj[256];
// prio 0 Equilibrio / 1 Maxima / 2 Fluidez; hdr 0 Preferir / 1 Indiferente / 2 Evitar; dv 0 = ligado (V_LIGA).
static void cfg(int prio, int hdr, int dv) {
  char c[300]; FILE *f;
  snprintf(c, sizeof c, "%s/ajustes.txt", dirAj);
  f = fopen(c, "w"); assert(f);
  fprintf(f, "fontePrioridadeLocal %d\nfonteHdrLocal %d\ndolbyVision %d\n", prio, hdr, dv);
  fclose(f);
  ajustes_dir(dirAj);
  assert(ajustes_fonte_prioridade() == prio && ajustes_fonte_hdr() == hdr && ajustes_dolby_vision() == !dv);
}
// Faixa de tamanho: indices de V_TAMANHO_GB (0 Sem limite, 1=1 GB, 2=2, 3=4, 4=8, 5=15, 6=30).
static void cfgTam(int max, int min) {
  char c[300]; FILE *f;
  snprintf(c, sizeof c, "%s/ajustes.txt", dirAj);
  f = fopen(c, "w"); assert(f);
  fprintf(f, "fontePrioridadeLocal 0\nfonteHdrLocal 0\ndolbyVision 0\ntamanhoMaxLocal %d\ntamanhoMinLocal %d\n", max, min);
  fclose(f);
  ajustes_dir(dirAj);
}
// Fonte de debrid em cache com o selo vindo do proprio rotulo, como o parser faz.
static void deb(Stream *s, const char *rot, int altura, int mp4, int dolbyVision) {
  fonte(s, "Torrentio", rot, altura, mp4, 0, 0, "td.invalid");
  s->dolbyVision = dolbyVision;
  s->badges = badges_detectar(rot);
}
static const char *vence(const Stream *l, int n) {
  int i;
  usleep(250000);   // a foto do StreamFit no automatico vale 200 ms
  stream_definir_alvo("tt6933238");
  stream_definir_lista(l, n);
  i = stream_automatico();
  assert(i >= 0);
  return stream_item(i)->rotulo;
}
#define GB(x) ((unsigned long long)(x) * 1000000000ULL)

int main(void) {
  Stream l[8];
  int kb[8] = {5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000}, n;
  snprintf(dirAj, sizeof dirAj, "%s/nv-fq-aj", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  { char m[300]; snprintf(m, sizeof m, "mkdir -p %s", dirAj); (void)!system(m); }
  cfg(0, 0, 0);   // padroes: Equilibrio, Preferir, DV ligado; qualidade Automatica
  streamfit_limpar();

  // 1) A LISTA DO DONO, sem medida nenhuma (hosts=0 na C9).
  n = 0;
  plugin(&l[n++], "MegaEmbed - 1080", 1080);
  fonte(&l[n++], "Torrentio", "Torrentio 720p", 720, 0, 0, 0, "td.invalid");
  fonte(&l[n++], "Torrentio", "Torrentio 1080p", 1080, 0, 0, 0, "td.invalid");
  fonte(&l[n++], "Torrentio", "Torrentio 4k", 2160, 0, 0, 0, "td.invalid");
  assert(!strcmp(vence(l, n), "Torrentio 4k"));
  // sem 4K: o 1080p de debrid (MKV) ganha do 1080 MP4 do embed, mesmo com o +5000 do MP4.
  assert(!strcmp(vence(l, n - 1), "Torrentio 1080p"));
  // 720p de debrid em cache tambem ganha de um embed 1080 (origem vem antes de altura).
  { Stream m[2]; plugin(&m[0], "MegaEmbed - 1080", 1080);
    fonte(&m[1], "Torrentio", "Torrentio 720p", 720, 0, 0, 0, "td.invalid");
    assert(!strcmp(vence(m, 2), "Torrentio 720p")); }
  // debrid FORA do cache so abre um aviso: o embed que toca agora passa na frente.
  { Stream m[2]; plugin(&m[0], "MegaEmbed - 1080", 1080);
    fonte(&m[1], "Debridio", "[AD] Debridio 1080p", 1080, 0, 1, 0, "td.invalid");
    assert(!strcmp(vence(m, 2), "MegaEmbed - 1080")); }
  // so o embed: ele toca como sempre.
  { Stream m[1]; plugin(&m[0], "MegaEmbed - 480", 480);
    assert(!strcmp(vence(m, 1), "MegaEmbed - 480")); }
  // a ordem de chegada nao decide: o embed chegando por ultimo ou por primeiro da no mesmo.
  { Stream m[2]; fonte(&m[0], "Torrentio", "Torrentio 1080p", 1080, 0, 0, 0, "td.invalid");
    plugin(&m[1], "MegaEmbed - 1080", 1080);
    assert(!strcmp(vence(m, 2), "Torrentio 1080p")); }

  // 2) ORCAMENTO DO STREAMFIT. 4K de 60 GB e 1080p de 1,5 GB, filme de 2 h.
  n = 0;
  plugin(&l[n++], "MegaEmbed - 1080", 1080);
  fonte(&l[n++], "Torrentio", "Torrentio 1080p", 1080, 0, 0, GB(1.5), "td.invalid");
  fonte(&l[n++], "Torrentio", "Torrentio 4k", 2160, 0, 0, GB(60), "td.invalid");
  stream_fit_duracao("tt6933238", 7200, SF_DUR_METADATA);
  streamfit_rede(7);
  // 2a) sem medida: NAO rebaixa, a 4K vence.
  assert(!strcmp(vence(l, n), "Torrentio 4k"));
  // 2b) pouca confianca (4 intervalos < 5): continua sem medida, a 4K vence.
  assert(streamfit_diagnostico(7, "https://td.invalid/x", kb, 4, streamfit_agora_ms()) == 0);
  assert(!strcmp(vence(l, n), "Torrentio 4k"));
  // 2c) medida confiavel de ~5 Mbps: a 4K (66 Mbps) e pesada e desce; o 1080p de 1,5 GB cabe.
  assert(streamfit_diagnostico(7, "https://td.invalid/x", kb, 8, streamfit_agora_ms()) == 8);
  assert(!strcmp(vence(l, n), "Torrentio 1080p"));
  // 2d) medida de outro host nao vale para este: volta a nao rebaixar.
  streamfit_limpar(); streamfit_rede(7);
  assert(streamfit_diagnostico(7, "https://outro.invalid/x", kb, 8, streamfit_agora_ms()) == 8);
  assert(!strcmp(vence(l, n), "Torrentio 4k"));
  // 2e) rede rapida: a 4K cabe e vence.
  { int rapido[8] = {90000, 90000, 90000, 90000, 90000, 90000, 90000, 90000};
    streamfit_limpar(); streamfit_rede(7);
    assert(streamfit_diagnostico(7, "https://td.invalid/x", rapido, 8, streamfit_agora_ms()) == 8);
    assert(!strcmp(vence(l, n), "Torrentio 4k")); }
  // 2f) tudo pesado e so sobra o embed: a debrid pesada AINDA ganha do embed.
  streamfit_limpar(); streamfit_rede(7);
  assert(streamfit_diagnostico(7, "https://td.invalid/x", kb, 8, streamfit_agora_ms()) == 8);
  { Stream m[2]; plugin(&m[0], "MegaEmbed - 1080", 1080);
    fonte(&m[1], "Torrentio", "Torrentio 4k", 2160, 0, 0, GB(60), "td.invalid");
    assert(!strcmp(vence(m, 2), "Torrentio 4k")); }

  // 3) R9b: A LISTA DO DONO COM HDR/DV. "Pegando sem HDR, sem Dolby Vision."
  //    4K DV (mp4), 4K HDR10, 4K SDR, 1080p SDR MP4 de plugin, 4K DV fora de cache.
  { Stream m[6];
    deb(&m[0], "Torrentio 4k SDR", 2160, 0, 0);
    deb(&m[1], "Torrentio 4k HDR10", 2160, 0, 0);
    deb(&m[2], "Torrentio 4k DV HDR", 2160, 1, 1);
    plugin(&m[3], "MegaEmbed - 1080", 1080);
    deb(&m[4], "Debridio 4k DV uncached", 2160, 1, 1); m[4].foraCache = 1;
    deb(&m[5], "Torrentio 1080p DV HDR10", 1080, 1, 1);
    for (int prio = 0; prio < 3; prio++) {
      cfg(prio, 0, 0);
      assert(!strcmp(vence(m, 6), "Torrentio 4k DV HDR"));    // DV ganha em todo modo
    }
    // sem a de DV: HDR10 4K ganha do SDR 4K em todo modo
    cfg(0, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k HDR10"));
    cfg(1, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k HDR10"));
    cfg(2, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k HDR10"));
    // Indiferente: o formato nao conta, fica a ordem da lista (SDR veio primeiro)
    cfg(0, 1, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k SDR"));
    // Evitar: SDR na mesma resolucao, mesmo com HDR vindo antes na lista
    { Stream e[2] = { m[1], m[0] };
      cfg(0, 2, 0); assert(!strcmp(vence(e, 2), "Torrentio 4k SDR")); }
    // DV desligado em Ajustes: a de DV vira HDR10 de base e perde para o HDR10 4K mkv so no empate
    // (mesmo nivel); com Preferir e DV ligado ela ganhava, desligada o mp4 desempata igual.
    cfg(1, 0, 1); assert(!strcmp(vence(m, 6), "Torrentio 4k DV HDR") || !strcmp(vence(m, 6), "Torrentio 4k HDR10"));
    assert(stream_pontos(&m[2]) <= stream_pontos(&m[1]) + 300);
  }
  // Equilibrio: 1080p HDR10 vence 4K SDR (um degrau abaixo), 720p HDR10 nao. Maxima: 4K SDR vence 1080p DV.
  { Stream m[2];
    deb(&m[0], "Torrentio 4k SDR", 2160, 0, 0);
    deb(&m[1], "Torrentio 1080p HDR10", 1080, 0, 0);
    cfg(0, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 1080p HDR10"));
    cfg(1, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k SDR"));
    cfg(2, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k SDR"));
    deb(&m[1], "Torrentio 720p HDR10", 720, 0, 0);
    cfg(0, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k SDR"));
    // TV sem HDR conhecida: HDR nao vale nada
    deb(&m[1], "Torrentio 4k HDR10", 2160, 0, 0);
    stream_definir_tela(0, 0); cfg(0, 0, 0);
    { Stream e[2] = { m[1], m[0] }; assert(!strcmp(vence(e, 2), "Torrentio 4k HDR10")); }  // empate: ordem da lista
    stream_definir_tela(-1, -1);
  }
  // Perfil 5 (sem base HDR10) em MP4 NA LG toca como DV de verdade (a TV aciona o
  // DV nativo no MP4), entao nao e mais rebaixado abaixo do HDR10; em MKV, com o
  // DV desligado em Ajustes ou fora da LG continua atras do HDR10 (cor lavada).
  // Este arquivo compila streams.c como o host (LG). Ver tests/fonte_lg_mp4.c.
  { Stream m[2];
    deb(&m[0], "Torrentio 4k DV Profile 5", 2160, 1, 1);
    deb(&m[1], "Torrentio 4k HDR10", 2160, 0, 0);
    cfg(0, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k DV Profile 5"));
    cfg(1, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k DV Profile 5"));
    cfg(0, 0, 1); assert(!strcmp(vence(m, 2), "Torrentio 4k HDR10"));
    deb(&m[0], "Torrentio 4k DV Profile 5", 2160, 0, 1);   // MKV
    cfg(0, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k HDR10"));
    cfg(1, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k HDR10"));
  }
  // O que o dono viu: nunca um embed 1080 SDR na frente de debrid HDR em cache.
  { Stream m[3];
    plugin(&m[0], "MegaEmbed - 1080", 1080);
    deb(&m[1], "Debridio 1080p HDR10", 1080, 0, 0);
    deb(&m[2], "Torrentio 4k DV HDR", 2160, 1, 1);
    for (int prio = 0; prio < 3; prio++) { cfg(prio, 0, 0); assert(!strcmp(vence(m, 3), "Torrentio 4k DV HDR")); }
  }
  // StreamFit: Maxima nunca rebaixa por velocidade; Equilibrio e Fluidez rebaixam com medida confiavel.
  { Stream m[2];
    deb(&m[0], "Torrentio 4k DV HDR", 2160, 1, 1); m[0].tamanhoBytes = GB(60);
    deb(&m[1], "Torrentio 1080p", 1080, 0, 0); m[1].tamanhoBytes = GB(1.5);
    streamfit_limpar(); streamfit_rede(9);
    stream_fit_duracao("tt6933238", 7200, SF_DUR_METADATA);
    assert(streamfit_diagnostico(9, "https://td.invalid/x", kb, 8, streamfit_agora_ms()) == 8);
    cfg(1, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 4k DV HDR"));
    cfg(0, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 1080p"));
    cfg(2, 0, 0); assert(!strcmp(vence(m, 2), "Torrentio 1080p"));
  }

  // Comecar rapido: o 1080p leve em cache passa na frente do 4K pesado; Maxima/Equilibrio mantem o 4K.
  // A fonte fora de cache nunca vence a em cache, nem aqui.
  { Stream m[3];
    deb(&m[0], "Torrentio 4k DV HDR", 2160, 1, 1); m[0].tamanhoBytes = GB(60);
    deb(&m[1], "Torrentio 1080p HDR10", 1080, 0, 0); m[1].tamanhoBytes = GB(4);
    deb(&m[2], "Debridio 1080p uncached", 1080, 0, 0); m[2].foraCache = 1; m[2].tamanhoBytes = GB(1);
    streamfit_limpar();
    cfg(2, 0, 0); assert(!strcmp(vence(m, 3), "Torrentio 1080p HDR10"));
    cfg(1, 0, 0); assert(!strcmp(vence(m, 3), "Torrentio 4k DV HDR"));
    cfg(0, 0, 0); assert(!strcmp(vence(m, 3), "Torrentio 4k DV HDR"));
    // tamanho desconhecido (0) nao custa nada: sem medida o 4K nao perde por "peso"
    m[0].tamanhoBytes = 0; cfg(2, 0, 0); assert(!strcmp(vence(m, 3), "Torrentio 4k DV HDR"));
  }

  // FAIXA DE TAMANHO (1 GB = 1024^3 bytes). Fora dela vai para o fim da fila, nunca sai.
  { const unsigned long long G = 1ULL << 30;
    Stream m[3];
    deb(&m[0], "Torrentio 4k", 2160, 0, 0); m[0].tamanhoBytes = 40 * G;   // melhor por qualidade
    deb(&m[1], "Torrentio 1080p", 1080, 0, 0); m[1].tamanhoBytes = 6 * G;
    deb(&m[2], "Torrentio 720p", 720, 0, 0); m[2].tamanhoBytes = G / 2;
    streamfit_limpar();
    // sem limites: ordem de sempre
    cfgTam(0, 0); assert(!strcmp(vence(m, 3), "Torrentio 4k"));
    // maximo 8 GB: o 4K de 40 GB sai da frente, ganha o 1080p de 6 GB
    cfgTam(4, 0); assert(!strcmp(vence(m, 3), "Torrentio 1080p"));
    assert(stream_pontos(&m[0]) < stream_pontos(&m[2]) && !stream_cabe_no_teto(&m[0]) && stream_cabe_no_teto(&m[1]));
    // maximo 4 GB: sobra so o 720p de 512 MB
    cfgTam(3, 0); assert(!strcmp(vence(m, 3), "Torrentio 720p"));
    // minimo 1 GB: o 720p de 512 MB fica para tras; o 4K ganha (sem maximo)
    cfgTam(0, 1); assert(!strcmp(vence(m, 3), "Torrentio 4k") && !stream_cabe_no_teto(&m[2]));
    // faixa 1..8 GB: so o 1080p esta dentro
    cfgTam(4, 1); assert(!strcmp(vence(m, 3), "Torrentio 1080p"));
    assert(!stream_cabe_no_teto(&m[0]) && !stream_cabe_no_teto(&m[2]));
    // limites inclusivos: exatamente 8 GB cabe no maximo de 8 GB e no minimo de 8 GB
    m[1].tamanhoBytes = 8 * G; cfgTam(4, 4); assert(stream_cabe_no_teto(&m[1]));
    m[1].tamanhoBytes = 6 * G;
    // minimo MAIOR que o maximo: o minimo e ignorado (vale so o maximo de 2 GB)
    cfgTam(2, 4); assert(!strcmp(vence(m, 3), "Torrentio 720p"));
    assert(stream_cabe_no_teto(&m[2]) && !stream_cabe_no_teto(&m[1]));
    // tamanho DESCONHECIDO continua elegivel: o 4K sem tamanho ganha mesmo com faixa estreita
    m[0].tamanhoBytes = 0; m[0].tamanhoMB = 0;
    cfgTam(2, 0); assert(!strcmp(vence(m, 3), "Torrentio 4k") && stream_cabe_no_teto(&m[0]));
    // sem bytes exatos vale o tamanhoMB do texto
    m[0].tamanhoMB = 40 * 1024; cfgTam(4, 0); assert(!stream_cabe_no_teto(&m[0]));
    m[0].tamanhoMB = 0;
    // TUDO fora da faixa: a melhor ainda toca (o teto e preferencia, nao filtro)
    m[0].tamanhoBytes = 40 * G;
    cfgTam(1, 0);   // maximo 1 GB: so o 720p (512 MB) cabe
    assert(!strcmp(vence(m, 3), "Torrentio 720p"));
    m[2].tamanhoBytes = 3 * G;   // agora nenhuma cabe em 1 GB
    assert(!strcmp(vence(m, 3), "Torrentio 4k"));
    cfgTam(0, 0);
  }

  streamfit_limpar();
  // #284: placeholder do addon nunca e escolhido; 0p real vem depois de 720p.
  { Stream m[4];
    deb(&m[0], "Meteor - Not configured", 0, 1, 0);
    deb(&m[1], "Embed69 - {}", 0, 1, 0);
    deb(&m[2], "Torrentio AD error", 0, 1, 0);
    deb(&m[3], "Torrentio 720p", 720, 0, 0);
    for (int prio = 0; prio < 3; prio++) { cfg(prio, 0, 0); assert(!strcmp(vence(m, 4), "Torrentio 720p")); }
    { Stream e[2]; deb(&e[0], "Torrentio WEB", 0, 1, 0); deb(&e[1], "Torrentio 480p", 480, 0, 0);
      for (int prio = 0; prio < 3; prio++) { cfg(prio, 0, 0); assert(!strcmp(vence(e, 2), "Torrentio 480p")); } }
    { Stream e[1]; deb(&e[0], "Torrentio WEB", 0, 1, 0);   // 0p real e a unica: ainda toca
      cfg(0, 0, 0); assert(!strcmp(vence(e, 1), "Torrentio WEB")); }
  }
  // #203: tela sem Dolby Vision (Samsung): DV de perfil desconhecido fica abaixo do HDR10; perfil 8 nao.
  { Stream m[3];
    deb(&m[0], "Torrentio 4k DV", 2160, 1, 1);
    deb(&m[1], "Torrentio 4k HDR10", 2160, 1, 0);
    deb(&m[2], "Torrentio 4k DV Profile 8 HDR10", 2160, 1, 1);
    stream_definir_tela(-1, 0); cfg(1, 0, 0);
    assert(stream_pontos(&m[0]) < stream_pontos(&m[1]));
    assert(!strcmp(vence(m, 2), "Torrentio 4k HDR10"));
    assert(stream_pontos(&m[2]) >= stream_pontos(&m[1]) - 300);   // base HDR10 declarada toca
    stream_definir_tela(-1, -1);   // desconhecido: DV volta a valer
    assert(stream_pontos(&m[0]) > stream_pontos(&m[1]));
  }
  puts("fonte_qualidade: ok");
  return 0;
}
