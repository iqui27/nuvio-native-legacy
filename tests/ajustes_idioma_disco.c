// #129: IDIOMA DE LEGENDA E AUDIO ESCOLHIDO NA TV SOBREVIVE AO ARRANQUE.
//
// O relato: "escolhi ingles para legenda e audio, saio dos ajustes e nao
// fica salvo; o filme seguinte nao liga a legenda em ingles". O ajustes.txt
// recebia "legendaIdioma 3" certinho — quem perdia era a LEITURA.
// ajustes_dir le o arquivo passando cada valor por limita(), que para as duas
// linhas de idioma consulta nValores() -> nLingua. E nLingua so era
// preenchido por rotulosDeIdioma(), chamado DEPOIS do laco de leitura: no
// arranque a lista tinha "1 valor", todo indice >= 1 era recusado como fora
// da faixa, e a legenda voltava a "Da conta" e o audio ao "Original".
//
// Este teste grava o arquivo como gravar() gravaria, le com ajustes_dir num
// processo que nunca abriu a tela de Ajustes (nLingua zerado, igual ao
// arranque) e confere o indice E o que chega em linguas.c, que e o que o
// player consulta (ling_legenda/ling_audio).
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>

int main(void) {
  char dir[] = "/tmp/nuvio-aj-idioma-XXXXXX";
  char caminho[700];
  FILE *f;
  int en = -1, es = -1, i;
  assert(mkdtemp(dir));
  for (i = 0; i < ling_opcao_n(); i++) {
    if (!strcmp(ling_opcao_codigo(i), "en")) en = i;
    if (!strcmp(ling_opcao_codigo(i), "es")) es = i;
  }
  assert(en > 1 && es > 1);
  assert(nLingua == 0);   // o estado do arranque: a tela nunca abriu

  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "legendaIdioma %d\naudioIdioma %d\n", en, es);
  fclose(f);

  ajustes_dir(dir);
  assert(valor[AJ_LEG_LINGUA] == en);
  assert(valor[AJ_AUD_LINGUA] == es);
  assert(!strcmp(ling_legenda(), "en"));
  assert(!strcmp(ling_audio(), "es"));

  // A conta com outro idioma nao desfaz a escolha desta TV (linguas.c: a
  // local ganha). E o blob da linha "[ajustes] idiomas da conta" do relato.
  ajustes_aplicar_blob("{\"features\":{\"player_settings\":{"
    "\"subtitle_preferred_language\":{\"type\":\"string\",\"value\":\"pt-br\"},"
    "\"preferred_audio_language\":{\"type\":\"string\",\"value\":\"\"}}}}");
  assert(valor[AJ_LEG_LINGUA] == en);
  assert(valor[AJ_AUD_LINGUA] == es);
  assert(!strcmp(ling_legenda(), "en"));
  assert(!strcmp(ling_audio(), "es"));

  // ABRIR A TELA DE AJUSTES nao zera a escolha (#129, a causa de campo):
  // conferirPadroes comparava o idioma contra o n=2 da tabela e voltava tudo
  // a "Da conta" — "padrao fora da lista em 4 (\"Idioma do áudio\")".
  ajustes_iniciar();
  assert(valor[AJ_LEG_LINGUA] == en);
  assert(valor[AJ_AUD_LINGUA] == es);
  ajustes_iniciar();
  assert(valor[AJ_LEG_LINGUA] == en);

  // O que ficou em disco continua sendo a escolha, para o proximo arranque.
  { char linha[96]; int achouLeg = 0;
    f = fopen(caminho, "r");
    assert(f);
    while (fgets(linha, sizeof linha, f)) {
      int v;
      if (sscanf(linha, "legendaIdioma %d", &v) == 1) { assert(v == en); achouLeg = 1; }
    }
    fclose(f);
    assert(achouLeg); }

  unlink(caminho);
  { char tmp[700]; snprintf(tmp, sizeof tmp, "%s/ajustes.tmp", dir); unlink(tmp); }
  rmdir(dir);
  puts("ajustes_idioma_disco: ok");
  return 0;
}
