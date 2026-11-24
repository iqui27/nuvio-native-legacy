// Testes puros de src/farolbuscaparse.c (sem rede, sem pthread, sem SDL).
#include "farolbuscaparse.h"
#include "faroltexto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void t1_normalizar_frase(void) {
  char out[FAROL_BUSCA_FRASE_BYTES];
  char longa[260];

  assert(farolbusca_normalizar_frase("  um  suspense\tcurto\npara hoje  ", out, sizeof out) == 1);
  assert(!strcmp(out, "um suspense curto para hoje"));

  assert(farolbusca_normalizar_frase("comédia leve com café", out, sizeof out) == 1);
  assert(!strcmp(out, "comédia leve com café"));

  // Rejeicoes: vazia, espacos, controle proibido, URL, UTF-8 invalido.
  assert(farolbusca_normalizar_frase(NULL, out, sizeof out) == 0);
  assert(farolbusca_normalizar_frase("", out, sizeof out) == 0);
  assert(farolbusca_normalizar_frase("   \t\n  ", out, sizeof out) == 0);
  assert(farolbusca_normalizar_frase("filme\x01ruim", out, sizeof out) == 0);
  assert(farolbusca_normalizar_frase("ver https://exemplo.invalid/x", out, sizeof out) == 0);
  assert(farolbusca_normalizar_frase("ver www.exemplo.invalid", out, sizeof out) == 0);
  assert(farolbusca_normalizar_frase("utf8 \xFF quebrado", out, sizeof out) == 0);

  // Corte limpo no limite de 200 codepoints.
  memset(longa, 'a', 230);
  longa[230] = '\0';
  assert(farolbusca_normalizar_frase(longa, out, sizeof out) == 1);
  assert(strlen(out) == FAROL_BUSCA_FRASE_MAX);
}

static void t2_montar_corpo_privacidade(void) {
  char buf[FAROL_BUSCA_CORPO_MAX + 1];
  const char *gens[] = { "suspense", "drama", "suspense", "http://bad", "a@b.com" };
  const char *hist[] = { "tt0000101", "tmdb:550", "invalido", "tt0000101", "https://bad" };
  const char *vist[] = { "tt0000202", "tmdb:0", "tmdb:900" };
  FarolBuscaCtx ctx;
  size_t n;

  // Perfil privado: APENAS {"frase":"..."} — mesmo se todos os outros campos vierem cheios.
  memset(&ctx, 0, sizeof ctx);
  ctx.privado = 1;
  ctx.infantil = 1;
  ctx.consentiu = 1;
  ctx.idioma = "pt-BR";
  ctx.generos = gens; ctx.nGeneros = 5;
  ctx.historico = hist; ctx.nHistorico = 5;
  ctx.vistos = vist; ctx.nVistos = 3;
  n = farolbusca_montar_corpo("um \"suspense\" curto\\hoje", &ctx, buf, sizeof buf);
  assert(n > 0);
  assert(!strcmp(buf, "{\"frase\":\"um \\\"suspense\\\" curto\\\\hoje\"}"));

  // Sem contexto: consentiu:0 explicito.
  n = farolbusca_montar_corpo("algo leve", NULL, buf, sizeof buf);
  assert(n > 0);
  assert(!strcmp(buf, "{\"frase\":\"algo leve\",\"consentiu\":0}"));

  // Sem consentimento: envia consentiu:0 explicito e bloqueia generos/historico/vistos.
  memset(&ctx, 0, sizeof ctx);
  ctx.privado = 0;
  ctx.consentiu = 0;
  ctx.idioma = "pt-BR";
  ctx.generos = gens; ctx.nGeneros = 5;
  ctx.historico = hist; ctx.nHistorico = 5;
  ctx.vistos = vist; ctx.nVistos = 3;
  n = farolbusca_montar_corpo("suspense curto", &ctx, buf, sizeof buf);
  assert(n > 0);
  assert(!strcmp(buf, "{\"frase\":\"suspense curto\",\"idioma\":\"pt-BR\",\"consentiu\":0}"));

  // Infantil mesmo com consentiu=1: envia infantil:1 + consentiu:0, sem gosto/historico.
  ctx.infantil = 1;
  ctx.consentiu = 1;
  n = farolbusca_montar_corpo("desenho curto", &ctx, buf, sizeof buf);
  assert(n > 0);
  assert(!strcmp(buf, "{\"frase\":\"desenho curto\",\"idioma\":\"pt-BR\",\"infantil\":1,\"consentiu\":0}"));

  // Com consentimento (adulto, nao privado): filtra invalidos e deduplica.
  ctx.infantil = 0;
  ctx.consentiu = 1;
  ctx.idioma = "en-US";
  n = farolbusca_montar_corpo("short thriller", &ctx, buf, sizeof buf);
  assert(n > 0);
  assert(strstr(buf, "\"consentiu\":1") != NULL);
  assert(strstr(buf, "\"idioma\":\"en-US\"") != NULL);
  assert(strstr(buf, "\"generos\":[\"suspense\",\"drama\"]") != NULL);
  assert(strstr(buf, "\"historico\":[\"tt0000101\",\"tmdb:550\"]") != NULL);
  assert(strstr(buf, "\"vistos\":[\"tt0000202\",\"tmdb:900\"]") != NULL);
  assert(strstr(buf, "http://") == NULL && strstr(buf, "@") == NULL);

  // Idioma malformado e ignorado (nao quebra o pedido).
  ctx.idioma = "invalido_123";
  ctx.generos = NULL; ctx.nGeneros = 0;
  ctx.historico = NULL; ctx.nHistorico = 0;
  ctx.vistos = NULL; ctx.nVistos = 0;
  n = farolbusca_montar_corpo("drama", &ctx, buf, sizeof buf);
  assert(n > 0 && strstr(buf, "\"idioma\"") == NULL);
}

static void t3_parsear_resposta(void) {
  FarolBuscaResp r;
  const char *jOk =
    "{"
      "\"ok\":1,"
      "\"ids\":["
        "{\"tmdb\":550,\"imdb\":\"tt0137523\",\"tipo\":\"movie\"},"
        "{\"tmdb\":550,\"imdb\":\"tt0137523\",\"tipo\":\"movie\"},"
        "{\"tmdb\":1396,\"imdb\":\"\",\"tipo\":\"series\"},"
        "{\"tmdb\":0,\"imdb\":\"tt0903747\",\"tipo\":\"tv\"},"
        "{\"tmdb\":0,\"imdb\":\"invalido\",\"tipo\":\"movie\"}"
      "],"
      "\"filtro\":{"
        "\"generos\":[\"suspense\",\"crime\",\"suspense\"],"
        "\"duracao_max\":100,"
        "\"decada\":1990,"
        "\"parecido_com\":\"A\\u00e7\\u00e3o \\ud83d\\ude00\","
        "\"tipo\":\"filme\","
        "\"extra_ignorado\":{\"a\":[1,2,true,false,null]}"
      "},"
      "\"origem\":\"llm\","
      "\"llm_status\":200,"
      "\"campo_futuro\":123.45e-2"
    "}";

  assert(farolbusca_parsear_resposta(jOk, strlen(jOk), &r) == 1);
  assert(r.ok == 1);
  assert(r.nIds == 3);
  assert(r.ids[0].tmdb == 550 && !strcmp(r.ids[0].imdb, "tt0137523") && !strcmp(r.ids[0].tipo, "movie"));
  assert(r.ids[1].tmdb == 1396 && r.ids[1].imdb[0] == '\0' && !strcmp(r.ids[1].tipo, "series"));
  assert(r.ids[2].tmdb == 0 && !strcmp(r.ids[2].imdb, "tt0903747") && !strcmp(r.ids[2].tipo, "series"));
  assert(r.filtro.nGeneros == 2);
  assert(!strcmp(r.filtro.generos[0], "suspense") && !strcmp(r.filtro.generos[1], "crime"));
  assert(r.filtro.duracao_max == 100);
  assert(r.filtro.decada == 1990);
  assert(strstr(r.filtro.parecido_com, "Ação") != NULL);
  assert(!strcmp(r.filtro.tipo, "filme"));
  assert(r.origem == FB_ORIG_LLM);
  assert(r.llm_status == 200);

  // Palavras (422 interno) + ids vazio (ressalva 1 da farol-29).
  {
    const char *jVazio =
      "{\"ok\":1,\"ids\":[],\"filtro\":{\"generos\":[],\"duracao_max\":null,"
      "\"decada\":null,\"parecido_com\":null,\"tipo\":\"qualquer\"},"
      "\"origem\":\"palavras\",\"llm_status\":422}";
    assert(farolbusca_parsear_resposta(jVazio, strlen(jVazio), &r) == 1);
    assert(r.ok == 1 && r.nIds == 0 && r.origem == FB_ORIG_PALAVRAS && r.llm_status == 422);
    assert(!strcmp(r.filtro.tipo, "qualquer"));
  }

  // Rejeicoes: ok=0, sem ids, truncado, lixo no fim, nao-objeto.
  {
    const char *ruins[] = {
      "",
      "{}",
      "{\"ok\":0,\"ids\":[]}",
      "{\"ok\":1}",
      "{\"ok\":1,\"ids\":[{",
      "{\"ok\":1,\"ids\":[]} lixo",
      "[1,2,3]",
      "{\"ok\":1,\"ids\":\"nao_array\"}"
    };
    size_t i;
    for (i = 0; i < sizeof ruins / sizeof ruins[0]; i++) {
      assert(farolbusca_parsear_resposta(ruins[i], strlen(ruins[i]), &r) == 0);
      assert(r.ok == 0 && r.nIds == 0);
    }
  }
}

static void t4_intencao_local_e_frases(void) {
  FarolBuscaLocal loc;

  farolbusca_intencao_local("um suspense curto para hoje à noite", &loc);
  assert(loc.intencao == FA_CURTO && loc.maxMin == FAROL_ACHA_CURTO_PADRAO);
  assert(!strcmp(loc.genero, "Suspense"));

  farolbusca_intencao_local("filme de comédia de até 85 min", &loc);
  assert(loc.intencao == FA_CURTO && loc.maxMin == 85);
  assert(!strcmp(loc.genero, "Comédia"));

  farolbusca_intencao_local("algo de 1h30", &loc);
  assert(loc.intencao == FA_CURTO && loc.maxMin == 90);
  assert(loc.genero[0] == '\0');

  farolbusca_intencao_local("uma hora e meia de terror", &loc);
  assert(loc.intencao == FA_CURTO && loc.maxMin == 90);
  assert(!strcmp(loc.genero, "Terror"));

  farolbusca_intencao_local("algo de ficção científica que eu ainda não vi", &loc);
  assert(loc.intencao == FA_NAO_VI);
  assert(!strcmp(loc.genero, "Ficção científica"));

  farolbusca_intencao_local("continuar de onde parei na série", &loc);
  assert(loc.intencao == FA_RETOMAR);

  // Palavra inteira: "duração" nao casa "acao".
  farolbusca_intencao_local("me surpreenda com boa duração", &loc);
  assert(loc.intencao == FA_SURPRESA);
  assert(loc.genero[0] == '\0');

  // Frases de estado da §3.3 (sempre FT_* de faroltexto.h).
  assert(!strcmp(farolbusca_frase_estado(FB_MOTIVO_BUSCANDO), FT_BUSCA_FRASE_PROCURANDO));
  assert(!strcmp(farolbusca_frase_estado(FB_MOTIVO_LIMITE), FT_BUSCA_FRASE_LIMITE));
  assert(!strcmp(farolbusca_frase_estado(FB_MOTIVO_OFFLINE), FT_BUSCA_FRASE_OFFLINE));
  assert(!strcmp(farolbusca_frase_estado(FB_MOTIVO_PRAZO), FT_BUSCA_FRASE_OFFLINE));
  assert(!strcmp(farolbusca_frase_estado(FB_MOTIVO_ERRO), FT_BUSCA_FRASE_OFFLINE));
  assert(!strcmp(farolbusca_frase_estado(FB_MOTIVO_VAZIO), FT_BUSCA_FRASE_OFFLINE));
  assert(!strcmp(farolbusca_frase_estado(FB_MOTIVO_CANCELADO), ""));
}

int main(void) {
  t1_normalizar_frase();
  t2_montar_corpo_privacidade();
  t3_parsear_resposta();
  t4_intencao_local_e_frases();
  puts("farol_busca_parse: ok");
  return 0;
}
