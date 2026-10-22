// Pacotes de selos: formas de JSON aceitas, dedupe, limite de 3, padrao ruim
// pulado, casamento com nomes reais de release, extracao do blob da conta e
// persistencia por perfil. Ver selospacote.h e regexjs.h.
#include "selospacote.h"
#include "regexjs.h"
#include "perfis.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- disco e perfil de mentira ----------------------------------------------
static int perfil = 1;
int perfis_ativo(void) { return perfil; }
static struct { char nome[64]; char *txt; } disco[64];
static int nd;
int dados_gravar(const char *nome, const char *c) {
  for (int i = 0; i < nd; i++) if (!strcmp(disco[i].nome, nome)) { free(disco[i].txt); disco[i].txt = strdup(c); return 1; }
  snprintf(disco[nd].nome, sizeof disco[nd].nome, "%s", nome); disco[nd++].txt = strdup(c); return 1;
}
char *dados_ler(const char *nome) {
  for (int i = 0; i < nd; i++) if (!strcmp(disco[i].nome, nome) && disco[i].txt) return strdup(disco[i].txt);
  return NULL;
}
int dados_apagar(const char *nome) {
  for (int i = 0; i < nd; i++) if (!strcmp(disco[i].nome, nome)) { free(disco[i].txt); disco[i].txt = NULL; }
  return 1;
}
// Troca de perfil e arranque novo: esquece a memoria do modulo (reler do disco).
extern void selospacote_iniciar(void);
static void reiniciarPerfil(int p) { perfil = p; }

static const char *ARQ = "Silo.S02E05.2160p.WEB-DL.DV.HDR.DDP5.1.Atmos.H265-FLUX.mkv";

static const char *PACOTE_BARE =
  "{\"groups\":[{\"id\":\"g1\",\"name\":\"Resolucao\",\"color\":\"#FFBE01\"}],"
  "\"filters\":["
  "{\"id\":\"1\",\"groupId\":\"g1\",\"name\":\"4K\",\"pattern\":\"(?i)\\\\b(2160p|4k|uhd)\\\\b\",\"imageURL\":\"https://x.invalid/4k.png\"},"
  "{\"id\":\"2\",\"name\":\"Dolby Vision\",\"pattern\":\"(?i)\\\\b(dv|dovi|dolby[ .]?vision)\\\\b\",\"imageURL\":\"https://x.invalid/dv.png\"},"
  "{\"id\":\"3\",\"name\":\"REMUX\",\"pattern\":\"(?i)\\\\bremux\\\\b\",\"imageURL\":\"https://x.invalid/remux.png\"},"
  "{\"id\":\"4\",\"name\":\"Atmos\",\"pattern\":\"(?i)atmos\",\"tagColor\":\"#112233\",\"textColor\":\"FFFFFF\",\"borderColor\":\"80FF0000\"},"
  "{\"id\":\"5\",\"name\":\"Ruim\",\"pattern\":\"(unclosed\",\"imageURL\":\"https://x.invalid/ruim.png\"},"
  "{\"id\":\"6\",\"name\":\"Desligado\",\"pattern\":\"mkv\",\"isEnabled\":false},"
  "{\"id\":\"7\",\"name\":\"4K de novo\",\"pattern\":\"2160\",\"imageURL\":\"HTTPS://X.INVALID/4K.PNG\"},"
  "{\"name\":\"\",\"pattern\":\"x\"}],"
  "\"sourceUrl\":\"https://packs.invalid/um.json\"}";

static void casa(const char *campos[], int nc, const char *esperado[], int ne) {
  unsigned short ids[SELOS_MAX_CASADOS];
  int n = selospacote_casar(campos, nc, ids, SELOS_MAX_CASADOS);
  if (n != ne) { printf("casou %d, esperava %d:", n, ne); for (int i = 0; i < n; i++) printf(" %s", selospacote_filtro(ids[i])->nome); printf("\n"); }
  assert(n == ne);
  for (int i = 0; i < ne; i++) assert(!strcmp(selospacote_filtro(ids[i])->nome, esperado[i]));
}

int main(void) {
  char e[96];
  // ---- as 3 formas de JSON ----
  assert(selospacote_n() == 0 && selospacote_ativo() == -1);
  assert(selospacote_adicionar("nao e json", "https://a.invalid/x") == SELOS_ERR_JSON);
  assert(selospacote_adicionar("[1,2]", "https://a.invalid/x") == SELOS_ERR_JSON);
  assert(selospacote_adicionar("{\"foo\":1}", "https://a.invalid/x") == SELOS_ERR_JSON);
  assert(selospacote_adicionar("{\"filters\":[{\"name\":\"\",\"pattern\":\"\"}]}", "https://a.invalid/x") == SELOS_ERR_VAZIO);
  assert(selospacote_n() == 0);

  // forma 3 (bare), a URL do pacote vem do JSON
  assert(selospacote_adicionar(PACOTE_BARE, "https://origem.invalid/ignorada") == SELOS_OK);
  assert(selospacote_n() == 1 && selospacote_da_tv(0));
  assert(!strcmp(selospacote_nome(0), "Resolucao"));
  assert(!strcmp(selospacote_url(0), "https://packs.invalid/um.json"));
  assert(selospacote_ativo() == 0);          // recem-adicionado ja fica escolhido
  assert(selospacote_n_filtros(0) == 5);     // 8 validos-ish: sem nome, desligado e padrao ruim ficam fora

  // forma 3 sem sourceUrl: a URL baixada vira o nome da origem; sem grupo, nome = host
  assert(selospacote_adicionar("{\"filters\":[{\"name\":\"HDR\",\"pattern\":\"hdr\"}]}", "https://dois.invalid/pack.json?x=1") == SELOS_OK);
  assert(selospacote_n() == 2);
  assert(!strcmp(selospacote_nome(1), "dois.invalid"));
  assert(selospacote_ativo() == 1);

  // forma 1 ({imports}) com dedupe por URL sem caixa: o dois e substituido, o tres entra
  assert(selospacote_adicionar(
    "{\"imports\":[{\"sourceUrl\":\"HTTPS://DOIS.invalid/pack.json?x=1\",\"filters\":[{\"name\":\"DV\",\"pattern\":\"dv\"},{\"name\":\"HDR\",\"pattern\":\"hdr\"}]},"
    "{\"sourceUrl\":\"https://tres.invalid/t.json\",\"isActive\":false,\"filters\":{\"name\":\"X\",\"pattern\":\"x\"}}]}", "https://z.invalid") == SELOS_OK);
  assert(selospacote_n() == 3);
  assert(selospacote_n_filtros(1) == 2);
  // limite de 3
  assert(selospacote_adicionar("{\"filters\":[{\"name\":\"A\",\"pattern\":\"a\"}]}", "https://quatro.invalid/q") == SELOS_ERR_LIMITE);
  assert(selospacote_n() == 3);
  // remover o terceiro; forma 2 ({streamBadgeRules:{imports}}) entra no lugar
  assert(selospacote_remover(2) == 1 && selospacote_n() == 2);
  assert(selospacote_adicionar(
    "{\"streamBadgeRules\":{\"imports\":[{\"sourceUrl\":\"https://forma2.invalid/p\",\"filters\":[{\"name\":\"F2\",\"pattern\":\"f2\"}]}]}}", "x") == SELOS_OK);
  assert(selospacote_n() == 3 && !strcmp(selospacote_url(2), "https://forma2.invalid/p"));
  assert(selospacote_ativo() == 2);
  assert(selospacote_remover(2) == 1 && selospacote_ativo() == -1);   // saiu o ativo: Do Nuvio
  assert(selospacote_remover(1) == 1 && selospacote_n() == 1);

  // ---- casamento (pacote 0, o do exemplo) ----
  selospacote_escolher(0);
  assert(selospacote_ativo() == 0);
  { const char *c[] = { ARQ };
    // ordem do pacote; "4K de novo" tem a mesma imagem (sem caixa) que "4K": nao repete;
    // REMUX nao casa; o de padrao ruim e o desligado nao existem
    const char *esp[] = { "4K", "Dolby Vision", "Atmos" };
    casa(c, 1, esp, 3); }
  { const char *c[] = { NULL, "", "Torrentio\n4k UHD", "Filme.1080p.BluRay.REMUX.AVC" };
    const char *esp[] = { "4K", "REMUX" };
    casa(c, 4, esp, 2); }
  { const char *c[] = { "Filme 1080p WEB-DL x264" };
    casa(c, 1, NULL, 0); }
  // \b de verdade: "dvd" nao e DV, "RDV" tambem nao
  { const char *c[] = { "Filme.DVDRip.RDV.mkv" };
    casa(c, 1, NULL, 0); }
  // o filtro so de texto traz as cores do pacote
  { unsigned short ids[4]; const char *c[] = { "x.Atmos.mkv" };
    assert(selospacote_casar(c, 1, ids, 4) == 1);
    const SeloFiltro *f = selospacote_filtro(ids[0]);
    assert(f->temTag && f->temTexto && f->temBorda && !f->imagem[0]);
    assert(f->tag[0] > .06f && f->tag[0] < .08f);          // 0x11
    assert(f->texto[0] == 1.0f && f->texto[3] == 1.0f);    // FFFFFF sem '#'
    assert(f->borda[0] == 1.0f && f->borda[1] == 0.0f && f->borda[3] > .49f && f->borda[3] < .51f); } // #AARRGGBB

  // ---- escolha e persistencia por perfil ----
  assert(selospacote_remover(0) == 1);
  assert(selospacote_adicionar(PACOTE_BARE, "x") == SELOS_OK);
  reiniciarPerfil(2);
  assert(selospacote_n() == 0 && selospacote_ativo() == -1);   // outro perfil: nada
  reiniciarPerfil(1);
  assert(selospacote_n() == 1 && selospacote_ativo() == 0);    // voltou do disco
  selospacote_escolher(-1);
  assert(selospacote_ativo() == -1);
  reiniciarPerfil(2); reiniciarPerfil(1);
  assert(selospacote_ativo() == -1 && selospacote_n() == 1);   // "Do Nuvio" lembrado
  { const char *c[] = { ARQ }; unsigned short ids[4]; assert(selospacote_casar(c, 1, ids, 4) == 0); }

  // ---- blob da conta: o JSON do pacote DENTRO de uma string ----
  perfil = 3;
  { static const char *BLOB =
      "{\"version\":1,\"features\":{\"layout_settings\":{\"x\":1},\"stream_badge_settings\":{"
      "\"stream_badge_rules\":{\"type\":\"string\",\"value\":\"{\\\"imports\\\":[{\\\"sourceUrl\\\":\\\"https://conta.invalid/pack\\\","
      "\\\"isActive\\\":true,\\\"groups\\\":[{\\\"id\\\":\\\"a\\\",\\\"name\\\":\\\"Da conta\\\"}],"
      "\\\"filters\\\":[{\\\"id\\\":\\\"1\\\",\\\"name\\\":\\\"Atmos\\\",\\\"pattern\\\":\\\"(?i)atmos\\\",\\\"imageURL\\\":\\\"https://c.invalid/a.png\\\"}]}]}\"},"
      "\"show_file_size_badges\":{\"type\":\"boolean\",\"value\":true}}}}";
    selospacote_conta_do_blob(BLOB);
    assert(selospacote_n() == 1 && !selospacote_da_tv(0) && !strcmp(selospacote_nome(0), "Da conta"));
    assert(selospacote_ativo() == 0);        // nunca escolheu: vale o ativo da conta
    assert(selospacote_remover(0) == 0);     // da conta: so se remove no web
    { const char *c[] = { ARQ }; const char *esp[] = { "Atmos" }; casa(c, 1, esp, 1); }
    // valor direto (sem {type,value}) tambem serve; e a conta manda "" = sem pacotes
    unsigned v = selospacote_versao();
    selospacote_conta_do_blob(BLOB);
    assert(selospacote_versao() == v);       // blob igual: nada recompila
    selospacote_conta_do_blob("{\"features\":{\"stream_badge_settings\":{\"stream_badge_rules\":\"\"}}}");
    assert(selospacote_n() == 0 && selospacote_ativo() == -1);
    // a conta junta com os da TV: conta primeiro, soma no limite de 3
    selospacote_conta_do_blob(BLOB);
    assert(selospacote_adicionar(PACOTE_BARE, "x") == SELOS_OK);
    assert(selospacote_n() == 2 && !selospacote_da_tv(0) && selospacote_da_tv(1));
    assert(selospacote_adicionar("{\"filters\":[{\"name\":\"A\",\"pattern\":\"a\"}],\"sourceUrl\":\"HTTPS://CONTA.invalid/pack\"}", "x") == SELOS_ERR_DUPLICADO);
  }

  // ---- o motor de regex ----
  { RegexJs *r;
    assert(!regexjs_compilar_web("(a", e, sizeof e) && e[0]);
    assert(!regexjs_compilar_web("\\1", e, sizeof e));
    assert(!regexjs_compilar_web("[z-a]", e, sizeof e));
    assert(!regexjs_compilar_web("a{3,2}", e, sizeof e));
    r = regexjs_compilar_web("(?i)h[ .]?26[45]", e, sizeof e);
    assert(r && regexjs_testar(r, ARQ, strlen(ARQ))); regexjs_liberar(r);
    r = regexjs_compilar_web("(a+)+b", e, sizeof e);       // catastrofico: estoura o teto e nao casa, sem travar
    { char ruim[4000]; memset(ruim, 'a', sizeof ruim - 1); ruim[sizeof ruim - 1] = 0;
      assert(r && !regexjs_testar(r, ruim, strlen(ruim))); }
    regexjs_liberar(r);
    { char longo[1008];
      const char *padroes[] = {
        "(?<=WEB-)DL", "(?<=BD-|WEB-)DL", "(?<=[A-Z]{2,3}-)DL",
        "(?<=W(?:E|X)?B-)DL", "(?<=WEB-(?=DL))DL", "(?<!BD-)DL"
      };
      memset(longo, 'x', 1000); strcpy(longo + 1000, " WEB-DL");
      for (size_t i = 0; i < sizeof padroes / sizeof *padroes; i++) {
        r = regexjs_compilar_web(padroes[i], e, sizeof e);
        assert(r && regexjs_testar(r, longo, strlen(longo))); regexjs_liberar(r);
      }
      r = regexjs_compilar_web("(?<!WEB-)DL", e, sizeof e);
      assert(r && !regexjs_testar(r, longo, strlen(longo))); regexjs_liberar(r);
    }
    // O limite conta bytes, inclusive para ponto, classe e literal UTF-8.
    { const char *padroes[] = { "(?<=.)DL", "(?<=[^x])DL", "(?<=\\u{1F600})DL" };
      const char *t = "\xF0\x9F\x98\x80" "DL";
      for (size_t i = 0; i < sizeof padroes / sizeof *padroes; i++) {
        r = regexjs_compilar_web(padroes[i], e, sizeof e);
        assert(r && regexjs_testar(r, t, strlen(t))); regexjs_liberar(r);
      }
    }
    r = regexjs_compilar_web("(?<=W.*-)DL", e, sizeof e);
    assert(r && regexjs_testar(r, "WEB-DL", 6)); regexjs_liberar(r);
    r = regexjs_compilar_web("(?<=a{0})DL", e, sizeof e);
    assert(r && regexjs_testar(r, "DL", 2)); regexjs_liberar(r);
    r = regexjs_compilar_web("(?<=\\.)DV(?=\\.)", e, sizeof e);
    assert(r && regexjs_testar(r, ARQ, strlen(ARQ))); regexjs_liberar(r);
    r = regexjs_compilar_web("(?i)^(?:(?!remux).)*$", e, sizeof e);
    assert(r && regexjs_testar(r, ARQ, strlen(ARQ)) && !regexjs_testar(r, "a.REMUX.b", 9)); regexjs_liberar(r);
    r = regexjs_compilar_web("(?i)caf\\u00e9|\xc3\xa9", e, sizeof e);   // UTF-8
    assert(r && regexjs_testar(r, "Cafe\xc3\xa9", 6)); regexjs_liberar(r);
    r = regexjs_compilar_web("(?m)^b$", e, sizeof e);
    assert(r && regexjs_testar(r, "a\nb\nc", 5)); regexjs_liberar(r);
    r = regexjs_compilar_web("^b$", e, sizeof e);
    assert(r && !regexjs_testar(r, "a\nb\nc", 5)); regexjs_liberar(r);
  }
  printf("selospacote: ok\n");
  return 0;
}
