// #390: o "Modelo de URL dos posteres" colado do celular (ou digitado) com
// mais de 299 caracteres, chave com maiusculas e {imdb} no fim nao era salvo:
// a modal cortava em 299 (PP_MODELO_MAX - 1), o alfabeto do campo baixava a
// caixa da chave, e o validador recusava o que sobrava sem gravar nada.
// Caminho de verdade: modal do campo -> texto do celular -> pstDefinir ->
// posteres.txt -> leitura de volta -> URL montada. A chave e de mentira.
#include "dados.h"
#include "idioma.h"
#include "posterprov.h"
#include "arteurl.h"
#include "teclado.h"
#include "sistexto.h"
#include "entrada_texto.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int ajustes_teste_poster_modelo(const char *colado, char *lido, size_t n);
void ajustes_teste_poster_recarregar(char *lido, size_t n);
const char *ajustes_teste_poster_aviso(void);
void ajustes_teste_poster_definir(int campo, const char *texto);

static int falhas;
#define OK(c, m) do { if (c) printf("ok   %s\n", m); else { printf("FALHA %s\n", m); falhas++; } } while (0)

int main(void) {
  char modelo[NV_ARTE_URL_MAX * 2], lido[NV_ARTE_URL_MAX * 2], u[NV_ARTE_URL_MAX];
  int aviso, k;
  dados_iniciar(NULL);   // NUVIO_DADOS: diretorio temporario do .sh

  // Posters Plus style settings, fake keys, reserved query values already encoded.
  snprintf(modelo, sizeof modelo,
    "https://postersplus.example.invalid/poster?tmdb_id={tmdb_id}&imdb_id={imdb_id}"
    "&stremio_id={imdb_id}&type={type}&primary_client=stremio_tv_nuvio"
    "&tmdb_key=FAKE_ABCdef0123456789&mdblist_key=FAKE_XYZabc0123456789"
    "&top_gradient=off&bottom_gradient=low&vignette_poster_color_bottom=true"
    "&vignette_color_blur=0.50&vignette_color_local=false&vignette_color_style=muted"
    "&sash_mode=notch&fallback_to_imdb=true&tmdb_rating_source=direct&score_color_mode=0"
    "&movie_weights=imdb%%3A1.00&tv_weights=imdb%%3A1.00&use_original_art=true"
    "&logo_language=ru&logo_priority=native%%2Cenglish%%2Coriginal%%2Cneutral%%2Ctext"
    "&fallback_bg_style=photoreal&meta_order=rating%%2Cyear%%2Cgenre"
    "&sash_badge_size_w=1.40&sash_badge_size_h=1.20&badge_display_mode=0");
  assert(strlen(modelo) > 700);
  OK(TECLADO_LONGO >= (int)strlen(modelo), "keyboard accepts the complete Posters Plus template");
  aviso = ajustes_teste_poster_modelo(modelo, lido, sizeof lido);
  OK(aviso == 0 && !strcmp(lido, modelo), "Posters Plus template saves intact");
  ajustes_teste_poster_recarregar(lido, sizeof lido);
  OK(!strcmp(lido, modelo), "Posters Plus template reloads intact");
  OK(posterprov_montar_url(posterprov_cfg(), "tt0903747", 1396, "series", u, sizeof u) &&
     strlen(u) > 700 && strstr(u, "tmdb_id=1396&imdb_id=tt0903747") &&
     strstr(u, "type=series&") && strstr(u, "FAKE_ABCdef0123456789") &&
     strstr(u, "movie_weights=imdb%3A1.00") && strstr(u, "logo_language=ru&") &&
     strstr(u, "&badge_display_mode=0") && !strchr(u, '{'),
     "Posters Plus URL expands IDs, preserves key case, encoding and final settings");
  OK(strlen(u) > 700 && !strcmp(posterprov_card("tt0903747", 1396, "series", "fallback"), u) &&
     posterprov_e_provedor(u), "complete Posters Plus URL reaches the card and provider download path");
  { char evento[NV_ARTE_URL_MAX + 1];
    st_teste_ligar(1);
    st_ime_abrir(ST_TECLADO, "", TECLADO_LONGO);
    snprintf(evento, sizeof evento, "D%s", modelo);
    st_teste_evento(evento);
    OK(st_ler(ST_TECLADO, lido, sizeof lido) == ST_FIM && !strcmp(lido, modelo),
       "native keyboard event preserves the complete template");
    st_fechar(ST_TECLADO);
    st_teste_ligar(0);
    texto_sistema_abrir("", 0);
    texto_sistema_ponte_valor(modelo);
    texto_sistema_quadro();
    OK(!strcmp(texto_sistema_valor(), modelo), "TV keyboard bridge preserves the complete template");
    texto_sistema_fechar();
  }
  // ~380 caracteres: chave de mentira com maiusculas, config longa, {imdb} no fim.
  k = snprintf(modelo, sizeof modelo,
               "https://posters.exemplo.invalid/FAKEKEY_ABCdef0123456789XYZ/config=");
  while (k < 360) k += snprintf(modelo + k, sizeof modelo - (size_t)k, "Ab1-Cd2_Ef3~");
  k += snprintf(modelo + k, sizeof modelo - (size_t)k, "/poster/{type}/{imdb}.jpg");
  printf("modelo de %d caracteres\n", k);
  assert(k > 300 && k < 400);

  aviso = ajustes_teste_poster_modelo(modelo, lido, sizeof lido);
  printf("aviso longo=%d\n", aviso);
  OK(aviso == 0, "o modelo longo e aceito (sem aviso de invalido)");
  OK(!strcmp(lido, modelo), "posteres.txt devolve o modelo inteiro, na mesma caixa");
  OK(posterprov_cfg()->prov == PP_MODELO && !strcmp(posterprov_cfg()->modelo, modelo),
     "o provedor ativo recebe o modelo inteiro");
  OK(posterprov_montar_url(posterprov_cfg(), "tt0111161", 0, "movie", u, sizeof u) &&
     strstr(u, "FAKEKEY_ABCdef") && strstr(u, "/poster/movie/tt0111161.jpg"),
     "a URL do cartaz sai com a chave intacta e o {imdb} trocado");

  // O curto de sempre continua igual.
  aviso = ajustes_teste_poster_modelo("https://meu.servidor.invalid/{type}/{imdb}.jpg", lido, sizeof lido);
  printf("aviso curto=%d lido=%s\n", aviso, lido);
  OK(aviso == 0 && !strcmp(lido, "https://meu.servidor.invalid/{type}/{imdb}.jpg"), "modelo curto inalterado");
  // #390: aliases passam pelo mesmo caminho de gravacao do modelo proprio.
  { const char *aliases[] = { "{imdb_id}", "{tmdb_id}", "{id}" };
    const char *ids[] = { "tt0111161", "278", "tt0111161" };
    int a;
    for (a = 0; a < 3; a++) {
      char esperado[PP_URL_MAX];
      snprintf(modelo, sizeof modelo, "https://posters.exemplo.invalid/{type}/%s.jpg", aliases[a]);
      aviso = ajustes_teste_poster_modelo(modelo, lido, sizeof lido);
      snprintf(esperado, sizeof esperado, "https://posters.exemplo.invalid/movie/%s.jpg", ids[a]);
      OK(aviso == 0 && !strcmp(lido, modelo) &&
         posterprov_montar_url(posterprov_cfg(), "tt0111161", 278, "movie", u, sizeof u) &&
         !strcmp(u, esperado), aliases[a]);
      if (a == 2) {
        OK(posterprov_montar_url(posterprov_cfg(), "tmdb:t1399", 0, "series", u, sizeof u) &&
           !strcmp(u, "https://posters.exemplo.invalid/series/1399.jpg"),
           "{id} usa TMDB quando nao ha IMDb");
        OK(posterprov_montar_url(posterprov_cfg(), "tt0111161", 0, "movie", u, sizeof u) &&
           !strcmp(u, esperado), "{id} aceita somente IMDb");
        OK(posterprov_montar_url(posterprov_cfg(), "", 278, "movie", u, sizeof u) &&
           !strcmp(u, "https://posters.exemplo.invalid/movie/278.jpg"), "{id} aceita somente TMDB");
        OK(!posterprov_montar_url(posterprov_cfg(), "", 0, "movie", u, sizeof u),
           "{id} sem identificador continua recusado");
      } else {
        OK(!posterprov_montar_url(posterprov_cfg(), a == 0 ? "tmdb:278" : "tt0111161",
                                 0, "movie", u, sizeof u), "alias nao inventa identificador ausente");
      }
      // Expansion reaches the common URL limit before the template limit.
      { int max = a == 1 ? 19 : 23, i;
        k = snprintf(modelo, sizeof modelo, "https://posters.exemplo.invalid/");
        for (i = 0; i < 12; i++) k += snprintf(modelo + k, sizeof modelo - (size_t)k, "%s", aliases[a]);
        while (k + 12 * (max - (int)strlen(aliases[a])) < PP_URL_MAX - 1) modelo[k++] = 'a';
        modelo[k] = 0;
        OK(posterprov_modelo_cabe(modelo), "alias at the URL limit fits");
        modelo[k++] = 'a'; modelo[k] = 0;
        OK(posterprov_modelo_valido(modelo) && !posterprov_modelo_cabe(modelo),
           "alias exceeding the URL limit is rejected");
      }
    }
  }
  // Sem marcador continua recusado.
  aviso = ajustes_teste_poster_modelo("https://meu.servidor.invalid/x.jpg", lido, sizeof lido);
  OK(aviso != 0, "modelo sem marcador continua recusado");
  OK(!strcmp(ajustes_teste_poster_aviso(),
     i18n("modelo inválido: use http(s):// e {imdb}, {imdb_id}, {tmdb}, {tmdb_id}, {id}, {type} ou {tipo_tmdb}")),
     "Done/OK mostra recusa de sintaxe no aviso de Ajustes");

  // Expansion must fit even when the template itself fits the input limit.
  { char m[PP_MODELO_MAX], u2[PP_URL_MAX];
    int j = snprintf(m, sizeof m, "https://posters.exemplo.invalid/");
    int i;
    for (i = 0; i < PP_URL_MAX / 23 + 1; i++) j += snprintf(m + j, sizeof m - (size_t)j, "{imdb}");
    while (j < 400) m[j++] = 'a';
    m[j] = 0;
    assert(strlen(m) < PP_MODELO_MAX);
    aviso = ajustes_teste_poster_modelo(m, lido, sizeof lido);
    printf("notice oversized expansion=%d\n", aviso);
    OK(aviso != 0, "template with oversized expansion is rejected when saving");
    OK(!strcmp(ajustes_teste_poster_aviso(),
       i18n("modelo longo demais: o endereço pronto passa de 2048 caracteres")),
       "Done/OK mostra recusa de tamanho no aviso de Ajustes");
    // Copilot no #408: recusado por TAMANHO, nao pela sintaxe. O aviso de
    // sintaxe ("use http(s):// e {imdb}...") mandaria corrigir o que esta certo.
    { int sintaxe = ajustes_teste_poster_modelo("https://meu.servidor.invalid/x.jpg", lido, sizeof lido);
      printf("aviso sintaxe=%d tamanho=%d\n", sintaxe, aviso);
      OK(sintaxe != 0 && aviso != sintaxe, "longo demais tem aviso proprio, diferente do de sintaxe"); }
    // 400 com um {imdb} so continua valendo e monta.
    j = snprintf(m, sizeof m, "https://posters.exemplo.invalid/FAKEKEY/{imdb}/");
    while (j < 400) m[j++] = 'b';
    m[j] = 0;
    aviso = ajustes_teste_poster_modelo(m, lido, sizeof lido);
    OK(aviso == 0 && !strcmp(lido, m) &&
       posterprov_montar_url(posterprov_cfg(), "tt0111161", 0, "movie", u2, sizeof u2),
       "400 caracteres com um {imdb} e aceito e monta"); }

  // Codex P2 (rodada 2): o pior caso vale so para o que se DIGITA. O modelo que
  // uma versao anterior gravou (< 300, marcadores conhecidos) e lido de volta
  // mesmo que o pior caso passe de 512: 299 caracteres com 13 x {imdb} = 520 no
  // pior caso, 338 com tt0111161 — monta.
  { char m[PP_MODELO_MAX], arq[PP_MODELO_MAX + 16], u3[PP_URL_MAX];
    int j = snprintf(m, sizeof m, "https://posters.exemplo.invalid/");
    int i;
    for (i = 0; i < 13; i++) j += snprintf(m + j, sizeof m - (size_t)j, "{imdb}");
    while (j < 299) m[j++] = 'c';
    m[j] = 0;
    snprintf(arq, sizeof arq, "modelo=%s\n", m);
    assert(dados_gravar("posteres.txt", arq));
    ajustes_teste_poster_recarregar(lido, sizeof lido);
    OK(!strcmp(lido, m), "modelo gravado por versao anterior (pior caso > 512) e mantido ao abrir");
    { PosterProvCfg c;
      memset(&c, 0, sizeof c);
      c.prov = PP_MODELO;
      snprintf(c.modelo, sizeof c.modelo, "%s", lido);
      OK(posterprov_montar_url(&c, "tt0111161", 0, "movie", u3, sizeof u3) && strlen(u3) == 338,
         "e ele monta (338 caracteres com tt0111161)"); } }

  // Captura o stdout de pstDefinir: uma linha por recusa e nenhuma chave.
  { const char *ruins[] = {
      "https://usuario:SEGREDO@sem-ponto/{id}?api_key=SEGREDO",
      "https://posters.exemplo.invalid/SEGREDO/sem-token",
      "https://posters.exemplo.invalid/SEGREDO?fmt=webp",
      "https://posters.exemplo.invalid/SEGREDO",
      "https://usuario:SEGREDO@posters.exemplo.invalid/SEGREDO/{desconhecido}?api_key=SEGREDO",
      "posters.exemplo.invalid/SEGREDO/{imdb}",
      "SEGREDO" };
    const char *motivos[] = { "instancia invalida", "token invalido", "parametros invalidos", "chave invalida",
                             "sintaxe invalida", "sintaxe invalida", "sintaxe invalida" };
    FILE *log = tmpfile();
    int salvo, a;
    char linha[1024], esperado[1024], redigida[96];
    assert(log);
    fflush(stdout); salvo = dup(STDOUT_FILENO); assert(salvo >= 0);
    assert(dup2(fileno(log), STDOUT_FILENO) >= 0);
    for (a = 0; a < 7; a++) ajustes_teste_poster_definir(a < 4 ? a : 4, ruins[a]);
    snprintf(modelo, sizeof modelo, "https://posters.exemplo.invalid/SEGREDO/");
    k = (int)strlen(modelo);
    for (a = 0; a < 100; a++) k += snprintf(modelo + k, sizeof modelo - (size_t)k, "{imdb}");
    ajustes_teste_poster_definir(4, modelo);
    ajustes_teste_poster_definir(4, "https://posters.exemplo.invalid/{id}");
    fflush(stdout); assert(dup2(salvo, STDOUT_FILENO) >= 0); close(salvo);
    rewind(log);
    for (a = 0; a < 7; a++) {
      posterprov_redigir(a < 5 ? ruins[a] : "", redigida, sizeof redigida);
      snprintf(esperado, sizeof esperado, "[poster] modelo recusado: %s (n=%zu) %s\n",
               motivos[a], strlen(ruins[a]), redigida);
      linha[0] = 0;
      OK(fgets(linha, sizeof linha, log) && !strcmp(linha, esperado) && !strstr(linha, "SEGREDO"),
         "recusa gera uma linha de log com motivo, tamanho e URL redigida");
    }
    posterprov_redigir(modelo, redigida, sizeof redigida);
    snprintf(esperado, sizeof esperado, "[poster] modelo recusado: endereco pronto longo demais (n=%zu) %s\n",
             strlen(modelo), redigida);
    OK(fgets(linha, sizeof linha, log) && !strcmp(linha, esperado) && !strstr(linha, "SEGREDO"),
       "log de recusa por tamanho tambem redige a chave");
    snprintf(esperado, sizeof esperado, "[poster] modelo salvo (n=%zu)\n",
             strlen("https://posters.exemplo.invalid/{id}"));
    OK(fgets(linha, sizeof linha, log) && !strcmp(linha, esperado), "sucesso gera log de modelo salvo");
    OK(!fgets(linha, sizeof linha, log), "nao ha linhas extras nem URL em claro no log");
    fclose(log);
  }

  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("tudo ok\n");
  return 0;
}
