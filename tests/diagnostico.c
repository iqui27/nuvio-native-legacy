// A TABELA POR APARELHO E A REGRA DE RESTAURAR, sem SDL e sem aparelho.
//
// POR QUE EXISTE. O otimizador so pode ficar ligado se duas coisas nunca
// quebrarem em silencio: (1) nenhum perfil passa do que a RAM e a plataforma
// aguentam — 300 MB de textura numa LG de 1 GB e o app sumindo sem cartao, e
// `original` ou 300 MB no Tizen ja foram MEDIDOS piores (registro 1450,
// QN85Q70 de 17/09); (2) um reteste pior SEMPRE restaura. As duas sao
// aritmetica em perfiltv.c, e e ela que se exercita aqui, nos dois alvos.
#include "perfiltv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const long RAMS[] = { 0, 512, 799, 800, 1024, 1199, 1200, 1350, 1999,
                             2000, 2048, 2245, 2999, 3000, 4096, 8192 };
#define N_RAMS (int)(sizeof RAMS / sizeof *RAMS)

static void tabela(void) {
  PtvPerfil p;
  // LG, os degraus que ja estavam em tex_cache.c (ver perfiltv.c).
  assert(ptv_tex_auto_mb(PTV_LG, 0) == 96);
  assert(ptv_tex_auto_mb(PTV_LG, 624) == 48);
  assert(ptv_tex_auto_mb(PTV_LG, 1024) == 48);
  assert(ptv_tex_auto_mb(PTV_LG, 964) == 48);    // registros 1720-1774
  assert(ptv_tex_auto_mb(PTV_LG, 1350) == 128);   // agregado de 23/09 (era 96)
  assert(ptv_tex_auto_mb(PTV_LG, 1236) == 128);
  assert(ptv_tex_auto_mb(PTV_LG, 1999) == 128);
  assert(ptv_tex_auto_mb(PTV_LG, 2245) == 128);   // a C9 do relatorio de 22/09
  assert(ptv_tex_auto_mb(PTV_LG, 3000) == 192);
  assert(ptv_tex_teto_mb(PTV_LG, 1024) == 64);
  assert(ptv_tex_teto_mb(PTV_LG, 1350) == 160);
  assert(ptv_tex_teto_mb(PTV_LG, 2245) == 300);
  assert(ptv_tex_teto_mb(PTV_LG, 4096) == 512);
  ptv_padrao(PTV_LG, 2245, &p);
  assert(p.texMb == 128 && p.fiosRede == 4 && p.heroiLarg == 1920);
  ptv_padrao(PTV_LG, 1024, &p);
  assert(p.texMb == 48 && p.fiosRede == 2 && p.heroiLarg == 1280);
  // Tizen: deviceMemory; o teto e o proprio automatico.
  assert(ptv_tex_auto_mb(PTV_TIZEN, 0) == 96);
  assert(ptv_tex_auto_mb(PTV_TIZEN, 1024) == 64);
  assert(ptv_tex_auto_mb(PTV_TIZEN, 2048) == 96);
  assert(ptv_tex_auto_mb(PTV_TIZEN, 4096) == 128);
  ptv_padrao(PTV_TIZEN, 2048, &p);
  assert(p.texMb == 96 && p.fiosRede == 2 && p.heroiLarg == 1280);
  ptv_padrao(PTV_TIZEN, 1024, &p);
  assert(p.texMb == 64 && p.fiosRede == 2 && p.heroiLarg == 1280);
  puts("ok  tabela por aparelho (LG MemTotal, Tizen deviceMemory)");
  // .tpk nativo (PTV_TPK): tabela propria, e a LG/Tizen acima nao mudaram.
  assert(ptv_tex_auto_mb(PTV_TPK, 0) == 96);
  assert(ptv_tex_auto_mb(PTV_TPK, 1108) == 64);   // registro 8825, Tizen 5.0
  assert(ptv_tex_auto_mb(PTV_TPK, 2048) == 96);
  assert(ptv_tex_auto_mb(PTV_TPK, 3072) == 128);
  assert(ptv_tex_teto_mb(PTV_TPK, 1108) == 96);
  assert(ptv_tex_teto_mb(PTV_TPK, 3072) == 256);
  assert(ptv_fios_rede_max(PTV_TPK) == 4);
  ptv_definir_gpu_fraca(0);
  ptv_padrao(PTV_TPK, 1108, &p);
  assert(p.texMb == 64 && p.fiosRede == 2 && p.heroiLarg == 1280);
  ptv_padrao(PTV_TPK, 2048, &p);
  assert(p.texMb == 96 && p.fiosRede == 4 && p.heroiLarg == 1920);
  ptv_definir_gpu_fraca(1);
  ptv_padrao(PTV_TPK, 2048, &p);
  assert(p.heroiLarg == 1280);
  ptv_definir_gpu_fraca(0);
  // #286: heroi 960 so no .tpk com GPU Utgard marcada; sem a marca, nada muda.
  assert(!ptv_gpu_utgard_atual());
  ptv_definir_gpu_utgard(1);
  ptv_definir_gpu_fraca(1);
  ptv_padrao(PTV_TPK, 1108, &p);
  assert(p.heroiLarg == 960 && p.texMb == 64 && p.fiosRede == 2);
  ptv_padrao(PTV_TPK, 2048, &p);
  assert(p.heroiLarg == 960);
  ptv_padrao(PTV_LG, 1024, &p);
  assert(p.heroiLarg == 1280);   // outras plataformas: intactas
  ptv_definir_gpu_utgard(0);
  ptv_definir_gpu_fraca(1);
  ptv_padrao(PTV_TPK, 1108, &p);
  assert(p.heroiLarg == 1280);   // GPU fraca que nao e Utgard (Midgard): 1280
  ptv_definir_gpu_fraca(0);
  ptv_padrao(PTV_TPK, 2048, &p);
  assert(p.heroiLarg == 1920);
  puts("ok  heroi 960 so na Utgard do .tpk");
  assert(ptv_gpu_fraca("Mali-TDVX"));
  assert(ptv_gpu_fraca("Mali-T830"));
  assert(ptv_gpu_fraca("Mali-450 MP"));
  assert(!ptv_gpu_fraca("Mali-G52"));
  assert(!ptv_gpu_fraca("Mali-G71"));
  // Trailer automatico da home vetado so no Utgard (ajustes_trailer_hero_vetar).
  assert(ptv_gpu_utgard("Mali-400 MP"));
  assert(ptv_gpu_utgard("Mali-450 MP"));
  assert(ptv_gpu_utgard("ARM Mali-470"));
  assert(!ptv_gpu_utgard("Mali-TDVX"));
  assert(!ptv_gpu_utgard("Mali-G57"));
  assert(!ptv_gpu_utgard("Mali-4"));
  assert(!ptv_gpu_utgard(NULL));
  assert(!ptv_gpu_fraca("Apple M4 Pro"));
  assert(!ptv_gpu_fraca(NULL));
  puts("ok  tabela do .tpk nativo (PTV_TPK) e GPU fraca pelo GL_RENDERER");
}

// NENHUM candidato, em nenhuma RAM, em nenhum modo, passa do teto.
static void limites(void) {
  int i, plat, modo;
  for (plat = 0; plat <= PTV_ANDROID; plat++)
    for (modo = 0; modo < 2; modo++)
      for (i = 0; i < N_RAMS; i++) {
        PtvPerfil c;
        long m = RAMS[i];
        ptv_candidato((PtvPlataforma)plat, m, (PtvModo)modo, 0, &c);
        assert(c.texMb >= 16 && c.texMb <= ptv_tex_teto_mb((PtvPlataforma)plat, m));
        assert(c.fiosRede >= 1 && c.fiosRede <= ptv_fios_rede_max((PtvPlataforma)plat));
        assert(c.heroiLarg >= 1280 && c.heroiLarg <= ptv_heroi_max((PtvPlataforma)plat, m));
        if (plat == PTV_TIZEN) {
          // A Samsung nunca ganha mais textura que o automatico medido nem os
          // 300 MB do alto-cache, em modo nenhum.
          assert(c.texMb <= ptv_tex_auto_mb(PTV_TIZEN, m));
          assert(c.texMb <= 128);
          if (m < 2000) assert(c.heroiLarg == 1280);
        }
        if ((plat == PTV_LG || plat == PTV_ANDROID) && m && m < 1200)
          assert(c.texMb <= 64 && c.heroiLarg == 1280 && c.fiosRede <= 2);
      }
  { PtvPerfil c;
    ptv_candidato(PTV_LG, 2245, PTV_QUALIDADE, 0, &c);
    assert(c.texMb == 300 && c.fiosRede == 4 && c.heroiLarg == 1920);
    ptv_candidato(PTV_LG, 2245, PTV_DESEMPENHO, 0, &c);
    assert(c.texMb == 128 && c.fiosRede == 2 && c.heroiLarg == 1280);
    // Escolha manual (ou alto-cache): o orcamento fica, o resto muda.
    ptv_candidato(PTV_LG, 2245, PTV_DESEMPENHO, 300, &c);
    assert(c.texMb == 300 && c.fiosRede == 2);
    // Manual acima do teto (perfil antigo, TV trocada) volta ao teto.
    ptv_candidato(PTV_LG, 1024, PTV_QUALIDADE, 300, &c);
    assert(c.texMb == 64); }
  // LG de 658 MB (diagnostico de campo): o candidato de Qualidade sobe a
  // textura ao teto, mas os fios ficam nos 2 da tabela; e um perfil salvo com
  // 4 fios, aprovado antes desta regra, volta a 2 ao ser lido.
  { PtvPerfil c, salvo = { 64, 4, 1280 };
    ptv_candidato(PTV_LG, 658, PTV_QUALIDADE, 0, &c);
    assert(c.texMb == 64 && c.fiosRede == 2 && c.heroiLarg == 1280);
    assert(ptv_limitar(PTV_LG, 658, &salvo) == 1 && salvo.fiosRede == 2 && salvo.texMb == 64);
    ptv_candidato(PTV_LG, 1500, PTV_QUALIDADE, 0, &c);
    assert(c.fiosRede == 4); }
  { PtvPerfil lido = { 999, 9, 3840 };
    assert(ptv_limitar(PTV_TIZEN, 2048, &lido) == 1);
    assert(lido.texMb == 96 && lido.fiosRede == 2 && lido.heroiLarg == 1920); }
  puts("ok  limites por RAM e plataforma em todo candidato");
}

static void regra(void) {
  PtvMedida a = { 1886, 10, 0, 33, 0 }, b;
  const char *m = NULL;
  // Melhor ou igual: mantem.
  b = a; b.artesMs = 1500;
  assert(!ptv_depois_pior(&a, &b, &m) && m == NULL);
  // Ruido de rede dentro da folga (25% + 150 ms): mantem.
  b = a; b.artesMs = 1886 + 1886 / 4 + 150;
  assert(!ptv_depois_pior(&a, &b, &m));
  // Mais lento que a folga: restaura, com o motivo.
  b.artesMs++;
  assert(ptv_depois_pior(&a, &b, &m) && m && strstr(m, "lentas"));
  // Uma falha a mais: restaura, sem folga.
  b = a; b.falhas = 1;
  assert(ptv_depois_pior(&a, &b, &m) && strstr(m, "falharam"));
  // Arte da tela despejada: restaura.
  b = a; b.despejosQuentes = 1;
  assert(ptv_depois_pior(&a, &b, &m) && strstr(m, "descartada"));
  // Pior quadro: so conta a partir de 50 ms e acima de 1,5x + 20.
  b = a; b.piorQuadroMs = 49;
  assert(!ptv_depois_pior(&a, &b, &m));
  b.piorQuadroMs = 33 + 16 + 20 + 1;
  assert(ptv_depois_pior(&a, &b, &m) && strstr(m, "quadro"));
  puts("ok  reteste pior restaura; melhor ou dentro da folga mantem");
}

// A MARGEM DO VEREDITO (ptv_decidir), com os numeros dos relatorios de campo
// que trocavam a TV de perfil a cada rodada. Medida: { ms, prontas, falhas,
// pior quadro, despejos quentes }.
static PtvMedida medida(int ms, int falhas, int quadro, int desp) {
  PtvMedida x = { ms, 10 - falhas, falhas, quadro, desp };
  return x;
}

static void margem(void) {
  const PtvPerfil q96 = { 96, 4, 1920 }, q160 = { 160, 4, 1920 };
  const PtvPerfil d96 = { 96, 2, 1280 }, d160 = { 160, 2, 1280 };
  const PtvPerfil s96_1920 = { 96, 2, 1920 }, s96_1280 = { 96, 2, 1280 };
  PtvMedida a, b;
  const char *m = NULL;

  // Android de 2 GB: 300 MB e candidato para imagens, nao compromisso de
  // aplicar. Sem ganho/arte descartada, 128 MB continuam; sob pressao, sobe.
  { PtvPerfil atual, cand;
    ptv_padrao(PTV_ANDROID, 2245, &atual);
    ptv_candidato(PTV_ANDROID, 2245, PTV_QUALIDADE, 0, &cand);
    assert(atual.texMb == 128 && cand.texMb == 300);
    a = medida(1000, 0, 33, 0); b = medida(950, 0, 33, 0);
    assert(ptv_decidir(&atual, &cand, &a, &b, 0, &m) == PTV_DEC_RUIDO);
    assert(ptv_decidir(&atual, &cand, &a, &b, 2, &m) == PTV_DEC_APLICAR); }

  // LG, rodada A: 96|4|1920 -> 160|4|1920, artes 316 -> 297 ms (6%, 19 ms).
  // Sem arte visivel despejada, mais memoria nao se justifica: fica o 96.
  a = medida(316, 0, 33, 0); b = medida(297, 0, 33, 0);
  assert(ptv_decidir(&q96, &q160, &a, &b, 0, &m) == PTV_DEC_RUIDO);
  assert(m && strstr(m, "memória"));
  // O mesmo com arte visivel despejada na sessao (ou no passe ANTES): sobe.
  assert(ptv_decidir(&q96, &q160, &a, &b, 3, &m) == PTV_DEC_APLICAR && m == NULL);
  a.despejosQuentes = 1; b.despejosQuentes = 1;
  assert(ptv_decidir(&q96, &q160, &a, &b, 0, &m) == PTV_DEC_APLICAR);

  // LG, rodada B: 160|4|1920 -> 96|2|1280, artes 320 -> 360 ms. Nao passa da
  // folga de restaurar (25% + 150), mas descer fios e heroi exige GANHO.
  a = medida(320, 0, 33, 0); b = medida(360, 0, 33, 0);
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_RUIDO);
  assert(m && strstr(m, "Menos recursos"));
  // Mesmo MAIS rapido, mas dentro da margem (max(15%, 80 ms)): nao desce.
  b = medida(250, 0, 33, 0);   // -70 ms
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_RUIDO);

  // Outra LG (1236 MB): 96|4|1920 -> 96|2|1280 -> 160|4|1920 em rodadas
  // seguidas. Com artes na faixa de ruido, a primeira descida nao acontece.
  a = medida(1100, 0, 40, 0); b = medida(1010, 0, 40, 0);    // -90 ms, 8%
  assert(ptv_decidir(&q96, &d96, &a, &b, 0, &m) == PTV_DEC_RUIDO);

  // Outra LG: 160|2|1280 -> 160|4|1920 -> 160|2|1280. Subir o heroi e o que
  // Qualidade pede: fica se nao piorou. A VOLTA para 2 fios/1280 dentro do
  // ruido e que nao acontece mais.
  a = medida(900, 0, 33, 0); b = medida(950, 0, 33, 0);
  assert(ptv_decidir(&d160, &q160, &a, &b, 0, &m) == PTV_DEC_APLICAR);
  a = medida(950, 0, 33, 0); b = medida(900, 0, 33, 0);
  assert(ptv_decidir(&q160, &d160, &a, &b, 0, &m) == PTV_DEC_RUIDO);
  // Tanto faz a ordem: com o perfil ja em 160|4|1920, a rodada seguinte de
  // Qualidade da ja_no_perfil (candidato igual, nem chega aqui) e a de
  // Desempenho precisa ganhar de verdade.

  // GANHO CLARO: desce e fica. 1000 -> 800 ms (20%, 200 ms), quadro igual.
  a = medida(1000, 0, 33, 0); b = medida(800, 0, 33, 0);
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_APLICAR && m == NULL);
  // Na borda: exatamente 15% (150 ms) passa; 149 nao.
  b.artesMs = 850;
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_APLICAR);
  b.artesMs = 851;
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_RUIDO);
  // Amostra curta: o piso de 80 ms vale (15% de 300 = 45 nao basta).
  a = medida(300, 0, 20, 0); b = medida(230, 0, 20, 0);
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_RUIDO);
  b.artesMs = 220;
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_APLICAR);
  // Mais rapido, mas o pior quadro subiu mais que um quadro (e passa de
  // 50 ms): nao conta como ganho.
  a = medida(1000, 0, 40, 0); b = medida(700, 0, 58, 0);
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_RUIDO);
  b.piorQuadroMs = 57;
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_APLICAR);
  // Contagem que cai e ganho sem margem de tempo: uma falha a menos.
  a = medida(1000, 1, 33, 0); b = medida(1000, 0, 33, 0);
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_APLICAR);
  // ... e menos arte visivel despejada.
  a = medida(1000, 0, 33, 2); b = medida(1000, 0, 33, 0);
  assert(ptv_decidir(&q160, &d96, &a, &b, 0, &m) == PTV_DEC_APLICAR);
  // So fios a mais, sem ganho: ruido.
  { const PtvPerfil f2 = { 128, 2, 1920 }, f4 = { 128, 4, 1920 };
    a = medida(1000, 0, 33, 0); b = medida(980, 0, 33, 0);
    assert(ptv_decidir(&f2, &f4, &a, &b, 0, &m) == PTV_DEC_RUIDO);
    assert(m && strstr(m, "claramente")); }

  // SAMSUNG 2 GB: 96|2|1280 -> 96|2|1920, artes 3845 -> 5632 ms. Piorou
  // alem da folga: restaura, com o motivo de ptv_depois_pior.
  a = medida(3845, 0, 60, 0); b = medida(5632, 0, 60, 0);
  assert(ptv_decidir(&s96_1280, &s96_1920, &a, &b, 0, &m) == PTV_DEC_RESTAURAR);
  assert(m && strstr(m, "lentas"));
  // Falha a mais ou arte despejada a mais restaura mesmo com heroi subindo.
  a = medida(1000, 0, 33, 0); b = medida(700, 1, 33, 0);
  assert(ptv_decidir(&q96, &q160, &a, &b, 5, &m) == PTV_DEC_RESTAURAR);
  puts("ok  margem: ruido nao troca perfil, ganho claro troca, pior restaura");
}

static void persistencia(void) {
  PtvPerfil p = { 300, 4, 1920 }, q;
  char buf[200];
  assert(ptv_serializar(&p, "qualidade", buf, sizeof buf));
  memset(&q, 0, sizeof q);
  assert(ptv_ler(buf, &q) && !memcmp(&p, &q, sizeof p));
  // O .cfg da versao 1 nao diz o que foi aprovado: nao vale como perfil.
  assert(!ptv_ler("versao=1\nmodo=qualidade\nantes_mb=128\n", &q));
  puts("ok  perfil aprovado grava e le; versao 1 ignorada");
}

static void sugestao(void) {
  PtvFonte f[PTV_N_FONTES];
  PtvSugestao s;
  memset(f, 0, sizeof f);
  // O CASO DO DONO (22/09): outra arte ligada, destaque em TMDB (virtual:
  // /find + download) muito mais lento que o catalogo do card.
  f[PTV_FONTE_CATALOGO] = (PtvFonte){ 3, 0, 0, 1200, 2500000, 1920, 1080, 0 };  // 400 ms
  f[PTV_FONTE_METAHUB]  = (PtvFonte){ 3, 0, 0, 1350, 2500000, 1920, 1080, 0 };  // 450 ms
  f[PTV_FONTE_TMDB]     = (PtvFonte){ 3, 0, 2400, 3300, 900000, 1280, 720, 0 }; // 1900 ms
  f[PTV_FONTE_TRAKT]    = (PtvFonte){ 2, 1, 3000, 2000, 600000, 1280, 720, 0 }; // 2500 ms
  assert(ptv_sugerir_destaque(f, PTV_FONTE_TMDB, PTV_FONTE_CATALOGO, 0, 1, &s));
  // Trakt tambem e lenta; Metahub nao: fica arte diferente, pelo Metahub.
  assert(s.fonte == PTV_FONTE_METAHUB && s.diferente == 1);
  assert(s.lenta == PTV_FONTE_TMDB && s.msLenta == 1900 && s.msBase == 400);
  // Sem nenhuma outra rapida: desligar "Destaque com outra arte".
  f[PTV_FONTE_METAHUB].downloadMs = 9000;
  assert(ptv_sugerir_destaque(f, PTV_FONTE_TMDB, PTV_FONTE_CATALOGO, 3, 1, &s));
  assert(s.diferente == 0 && s.fonte == 3);
  // Mais lenta mas abaixo de 800 ms por arte: nao e "claramente" (nada).
  f[PTV_FONTE_TMDB] = (PtvFonte){ 3, 0, 600, 1500, 0, 0, 0, 0 };  // 700 ms > 2x400
  assert(!ptv_sugerir_destaque(f, PTV_FONTE_TMDB, PTV_FONTE_CATALOGO, 0, 1, &s));
  // So falhou onde o card respondeu: conta como lenta.
  f[PTV_FONTE_TMDB] = (PtvFonte){ 0, 3, 900, 0, 0, 0, 0, 0 };
  assert(ptv_sugerir_destaque(f, PTV_FONTE_TMDB, PTV_FONTE_CATALOGO, 3, 1, &s));
  // Mesma arte nos dois, fonte escolhida lenta: troca para a mais rapida.
  f[PTV_FONTE_TMDB] = (PtvFonte){ 3, 0, 2400, 3300, 0, 0, 0, 0 };
  assert(ptv_sugerir_destaque(f, PTV_FONTE_TMDB, PTV_FONTE_TMDB, 3, 0, &s));
  assert(s.fonte == PTV_FONTE_CATALOGO && s.diferente == 0);
  // Automatico com a mesma arte: nao ha o que propor.
  assert(!ptv_sugerir_destaque(f, PTV_FONTE_CATALOGO, PTV_FONTE_CATALOGO, 0, 0, &s));
  assert(ptv_fonte_da_url("https://nuvio.invalid/arte/tmdb/w1280/tt1") == PTV_FONTE_TMDB);
  assert(ptv_fonte_da_url("https://images.metahub.space/background/medium/tt1/img") == PTV_FONTE_METAHUB);
  assert(ptv_fonte_da_url("https://walter.trakt.tv/images/x.jpg") == PTV_FONTE_TRAKT);
  assert(ptv_fonte_da_url("https://nuvio.invalid/arte/tmdbalt/w1280/tt1/m2") == PTV_FONTE_TMDB_OUTRO);
  assert(ptv_fonte_da_url("https://nuvio.invalid/arte/apple/1920/tt1/m/2024/X") == PTV_FONTE_APPLE);
  assert(ptv_fonte_da_url("https://is1-ssl.mzstatic.com/image/thumb/X/1920x1080.jpg") == PTV_FONTE_APPLE);
  assert(ptv_fonte_da_url("https://nuvio.invalid/arte/anime/large/kitsu:1/s/0/") == PTV_FONTE_ANIME);
  assert(ptv_fonte_da_url("https://assets.fanart.tv/fanart/movies/1/moviebackground/a.jpg") == PTV_FONTE_FANART);
  puts("ok  sugestao de fonte do destaque (2x e > 800 ms por arte)");

  // O RELATORIO 1669 (C9, 22/09), com as fontes de 23/09: destaque em
  // Metahub com outra arte ligada. Metahub baixou os MESMOS bytes do catalogo
  // (1763947 nos dois) — iguais = 3 — e a sugestao de entao foi "alvo:metahub"
  // com o ajuste JA em Metahub. Agora: a do destaque e igual ao card, entao
  // propoe a mais rapida REALMENTE diferente (Apple), nunca Metahub.
  memset(f, 0, sizeof f);
  f[PTV_FONTE_CATALOGO]   = (PtvFonte){ 3, 0, 0, 903, 1763947, 1920, 1080, 0 };   // 301 ms
  f[PTV_FONTE_METAHUB]    = (PtvFonte){ 3, 0, 0, 923, 1763947, 1920, 1080, 3 };   // 307 ms, = card
  f[PTV_FONTE_TMDB]       = (PtvFonte){ 3, 0, 1939, 3348, 502709, 1280, 720, 1 }; // 1762 ms
  f[PTV_FONTE_TRAKT]      = (PtvFonte){ 3, 0, 1314, 1109, 297678, 1280, 720, 0 }; // 807 ms
  f[PTV_FONTE_APPLE]      = (PtvFonte){ 3, 0, 1350, 810, 900000, 1920, 1080, 0 }; // 720 ms
  f[PTV_FONTE_TMDB_OUTRO] = (PtvFonte){ 3, 0, 1950, 3400, 510000, 1280, 720, 0 }; // 1783 ms
  assert(ptv_sugerir_destaque(f, PTV_FONTE_METAHUB, PTV_FONTE_CATALOGO, 2, 1, &s));
  assert(s.motivo == PTV_MOTIVO_IGUAL && s.diferente == 1);
  assert(s.fonte == PTV_FONTE_APPLE && s.alvo == PTV_FONTE_APPLE);
  // Ja em Apple: nunca propoe o que o ajuste ja tem; Apple nao e igual nem
  // lenta frente ao card (720 < 2x301? nao: 720 < 800) -> nada a propor.
  assert(!ptv_sugerir_destaque(f, PTV_FONTE_APPLE, PTV_FONTE_CATALOGO, 5, 1, &s));
  // Em TMDB com outra arte (= o outro do TMDB, 1783 ms, lento): a mais rapida
  // diferente e nao lenta e a Apple; Metahub (igual) e Trakt (807 > 2x301 e >
  // 800 = lenta) ficam de fora.
  assert(ptv_sugerir_destaque(f, PTV_FONTE_TMDB_OUTRO, PTV_FONTE_CATALOGO, 3, 1, &s));
  assert(s.motivo == PTV_MOTIVO_LENTA && s.fonte == PTV_FONTE_APPLE);
  // Sem Apple: nenhuma diferente e rapida -> desligar outra arte.
  f[PTV_FONTE_APPLE] = (PtvFonte){ 0 };
  assert(ptv_sugerir_destaque(f, PTV_FONTE_TMDB_OUTRO, PTV_FONTE_CATALOGO, 3, 1, &s));
  assert(s.diferente == 0 && s.fonte == 3);
  // Igual ao card e nenhuma outra de verdade: nao propoe nada (desligar nao
  // mudaria a foto).
  assert(!ptv_sugerir_destaque(f, PTV_FONTE_METAHUB, PTV_FONTE_CATALOGO, 2, 1, &s));
  // O outro do TMDB nao tem valor de ajuste sem outra arte; o padrao nao tem
  // com ela.
  assert(ptv_ajuste_da_fonte(PTV_FONTE_TMDB_OUTRO, 1) == PTV_FONTE_TMDB);
  assert(ptv_ajuste_da_fonte(PTV_FONTE_TMDB_OUTRO, 0) == -1);
  assert(ptv_ajuste_da_fonte(PTV_FONTE_TMDB, 1) == -1);
  assert(ptv_ajuste_da_fonte(PTV_FONTE_LOGO, 0) == -1);
  // Sem outra arte, em Metahub (rapido): nunca propoe a propria Metahub.
  f[PTV_FONTE_METAHUB].iguais = 0;
  assert(!ptv_sugerir_destaque(f, PTV_FONTE_METAHUB, PTV_FONTE_METAHUB, 2, 0, &s));
  puts("ok  sugestao nunca repete o ajuste nem a foto do card (relatorio 1669)");
}

static void dimensoes(void) {
  static const unsigned char png[24] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n',
    0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0x07, 0x80, 0, 0, 0x04, 0x38 };
  static const unsigned char jpg[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0, 4, 0, 0,
    0xFF, 0xC0, 0, 17, 8, 0x02, 0xD0, 0x05, 0x00, 3, 0, 0, 0, 0 };
  static const unsigned char gif[10] = { 'G', 'I', 'F', '8', '9', 'a', 0x2C, 0x01, 0xC2, 0x01 };
  unsigned char webp[30] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P', 'V', 'P', '8', 'X' };
  int w, h;
  assert(ptv_dimensoes(png, sizeof png, &w, &h) && w == 1920 && h == 1080);
  assert(ptv_dimensoes(jpg, sizeof jpg, &w, &h) && w == 1280 && h == 720);
  assert(ptv_dimensoes(gif, sizeof gif, &w, &h) && w == 300 && h == 450);
  webp[24] = 0x7F; webp[25] = 0x07; webp[27] = 0x37; webp[28] = 0x04;   // 1920x1080
  assert(ptv_dimensoes(webp, sizeof webp, &w, &h) && w == 1920 && h == 1080);
  assert(!ptv_dimensoes((const unsigned char *)"<html>", 6, &w, &h));
  puts("ok  dimensoes pelo cabecalho (PNG, JPEG, GIF, WebP)");
}

static void android_policy(void) {
  const int locks[] = { 0, 16, 48, 96, 300, 999 };
  for (int gpu = 0; gpu <= 1; gpu++) {
    ptv_definir_gpu_fraca(gpu);
    for (int i=0;i<N_RAMS;i++) {
      long mem = RAMS[i];
      PtvPerfil a, old;
      assert(ptv_tex_auto_mb(PTV_ANDROID,mem) == ptv_tex_auto_mb(PTV_LG,mem));
      assert(ptv_tex_teto_mb(PTV_ANDROID,mem) == ptv_tex_teto_mb(PTV_LG,mem));
      ptv_padrao(PTV_ANDROID,mem,&a); ptv_padrao(PTV_LG,mem,&old);
      assert(!memcmp(&a,&old,sizeof a));
      for (int mode=0;mode<2;mode++) for (int j=0;j<6;j++) {
        ptv_candidato(PTV_ANDROID,mem,(PtvModo)mode,locks[j],&a);
        ptv_candidato(PTV_LG,mem,(PtvModo)mode,locks[j],&old);
        assert(!memcmp(&a,&old,sizeof a));
      }
      a = (PtvPerfil){999,9,3840}; old = a;
      assert(ptv_limitar(PTV_ANDROID,mem,&a) == ptv_limitar(PTV_LG,mem,&old));
      assert(!memcmp(&a,&old,sizeof a));
    }
  }
  ptv_definir_gpu_fraca(0);
  assert(!strcmp(ptv_plataforma_nome(PTV_ANDROID),"android"));
  PtvPerfil before, candidate;
  ptv_padrao(PTV_ANDROID,2245,&before);
  ptv_candidato(PTV_ANDROID,2245,PTV_DESEMPENHO,96,&candidate);
  assert(candidate.texMb==96 && candidate.fiosRede==2 && candidate.heroiLarg==1280);
  PtvMedida a={1000,10,0,20,0}, bad={1800,9,1,20,0}, noise=a, good={700,10,0,20,0};
  assert(ptv_decidir(&before,&candidate,&a,&bad,0,NULL)==PTV_DEC_RESTAURAR);
  assert(ptv_decidir(&before,&candidate,&a,&noise,0,NULL)==PTV_DEC_RUIDO);
  assert(ptv_decidir(&before,&candidate,&a,&good,0,NULL)==PTV_DEC_APLICAR);
  puts("ok  Android identity preserves native policy, overrides, and rollback");
}

int main(void) {
  android_policy();
  tabela();
  limites();
  regra();
  margem();
  persistencia();
  sugestao();
  dimensoes();
  return 0;
}
