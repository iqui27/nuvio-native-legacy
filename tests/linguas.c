// A regra que importa: SEM preferencia, nada e filtrado. Foi o contrario disso
// (dois idiomas cravados no codigo) que deixava quem fala espanhol sem legenda
// nenhuma, em silencio.
//
//   cc tests/linguas.c src/linguas.c -Isrc -o /tmp/t-linguas && /tmp/t-linguas
#include "linguas.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void ok(const char *oque, int cond) {
  if (!cond) { printf("FALHOU: %s\n", oque); falhas++; }
}

int main(void) {
  // Sem preferencia nenhuma: tudo passa, inclusive idioma que a tabela nem
  // conhece.
  ok("vazio aceita ingles",   ling_casa("en", ""));
  ok("vazio aceita coreano",  ling_casa("kor", ""));
  ok("vazio aceita galego",   ling_casa("glg", ""));

  // Variantes do mesmo idioma casam entre si, nas duas familias ISO.
  ok("pt casa com por",   ling_casa("por", "pt"));
  ok("pt casa com pob",   ling_casa("pob", "pt"));
  ok("pt casa com pt-BR", ling_casa("pt-BR", "pt"));
  ok("en casa com eng",   ling_casa("eng", "en"));
  ok("es NAO casa com pt", !ling_casa("spa", "pt"));

  // Subs.ro emits ISO 639-2/T ron; MKV often uses bibliographic rum.
  ok("ro accepts Subs.ro ron", ling_casa("ron", "ro"));
  ok("rum accepts ron", ling_casa("ron", "rum"));
  ok("ron accepts ro", ling_casa("ro", "ron"));
  ok("ron does not accept English", !ling_casa("eng", "ron"));
  ok("ron display name", !strcmp(ling_nome("ron"), "Romeno"));
  { const char *roAddon[] = { "ron" };
    ok("auto selects Subs.ro Romanian", ling_legenda_auto("ro", NULL, 0, 1, roAddon, 1, 1) == 0);
  }

  // Codigo desconhecido pelos dois lados: comparacao crua, sem inventar.
  ok("glg casa com glg",  ling_casa("glg", "glg"));
  ok("glg nao casa cat",  !ling_casa("glg", "cat"));
  ok("xyz casa com xyz",  ling_casa("xyz", "xyz"));
  ok("xyz nao casa xyw",  !ling_casa("xyz", "xyw"));

  // #201/#269: ISO 639-2/T do Community Subtitles e as variantes que addons
  // mandam. Antes "ces" nao casava "cs" e a legenda tcheca sumia calada.
  ok("cs casa ces",        ling_casa("ces", "cs"));
  ok("el casa ell",        ling_casa("ell", "el"));
  ok("no casa nob",        ling_casa("nob", "no"));
  ok("no casa nb",         ling_casa("nb", "no"));
  ok("zh casa zh-Hant",    ling_casa("zh-Hant", "zh"));
  ok("zh casa zho",        ling_casa("zho", "zh"));
  ok("es casa es-419",     ling_casa("es-419", "es"));
  ok("pt casa pt-PT",      ling_casa("pt-PT", "pt"));
  ok("pt casa pb",         ling_casa("pb", "pt"));
  ok("hr casa hrv",        ling_casa("hrv", "hr"));
  ok("hr casa hbs-hrv",    ling_casa("hbs-hrv", "hr"));
  ok("hr casa scr",        ling_casa("scr", "hr"));
  ok("sr casa hbs-srp",    ling_casa("hbs-srp", "sr"));
  ok("bs casa bos",        ling_casa("bos", "bs"));
  ok("hrv NAO casa sr",    !ling_casa("hrv", "sr"));
  ok("ces NAO casa sk",    !ling_casa("ces", "sk"));
  // O player da Samsung (#269): "jp", "cz", "du", "gr", "in", "ma".
  ok("ja casa jp (Samsung)", ling_casa("jp", "ja"));
  ok("cs casa cz (Samsung)", ling_casa("cz", "cs"));
  ok("nl casa du (Samsung)", ling_casa("du", "nl"));
  ok("el casa gr (Samsung)", ling_casa("gr", "el"));
  ok("id casa in (Samsung)", ling_casa("in", "id"));
  ok("ms casa ma (Samsung)", ling_casa("ma", "ms"));
  ok("nome de jp",  !strcmp(ling_nome("jp"), "Japonês"));
  ok("nome de cz",  !strcmp(ling_nome("cz"), "Tcheco"));
  ok("nome de du",  !strcmp(ling_nome("du"), "Holandês"));
  ok("nome de ces", !strcmp(ling_nome("ces"), "Tcheco"));
  ok("nome de es-419", !strcmp(ling_nome("es-419"), "Espanhol"));
  ok("nome de zh-Hans", !strcmp(ling_nome("zh-Hans"), "Chinês"));
  ok("nome de hrv", !strcmp(ling_nome("hrv"), "Croata"));
  ok("nome de bs", !strcmp(ling_nome("bs"), "Bósnio"));
  // #429: hr/sr/bs entraram NO FIM (regra de compatibilidade do ajustes.txt,
  // que grava o indice): 31, 32, 33 — nunca no meio da lista.
  ok("opcao hr no fim", !strcmp(ling_opcao_codigo(ling_opcao_n() - 3), "hr"));
  ok("opcao sr no fim", !strcmp(ling_opcao_codigo(ling_opcao_n() - 2), "sr"));
  ok("opcao bs no fim", !strcmp(ling_opcao_codigo(ling_opcao_n() - 1), "bs"));
  // Indices JA GRAVADOS em ajustes.txt nao podem mudar: 16 e "id" (nao "hr")
  // e o "~" fica no indice LING_OPC_ORIGINAL, antes dos idiomas novos.
  ok("indice 16 continua id", !strcmp(ling_opcao_codigo(16), "id"));
  ok("original no indice fixo", !strcmp(ling_opcao_codigo(LING_OPC_ORIGINAL), "~"));
  ok("selo de pb", !strcmp(ling_selo("pb"), "PT-BR"));
  // O "lang" com nome por extenso (Auto-Subs: "Hebrew (Auto-Subs)").
  { char c[16];
    ling_normalizar("Hebrew (Auto-Subs)", c, sizeof c);
    ok("Hebrew (Auto-Subs) vira heb", !strcmp(c, "heb") && ling_casa(c, "he"));
    ling_normalizar("Spanish (Auto-Subs)", c, sizeof c);
    ok("Spanish (Auto-Subs) vira spa", ling_casa(c, "es"));
    ling_normalizar("Czech", c, sizeof c);
    ok("Czech casa cs", ling_casa(c, "cs"));
    ling_normalizar("ces", c, sizeof c);
    ok("ces fica ces e casa cs", !strcmp(c, "ces") && ling_casa(c, "cs"));
    ling_normalizar("pt-BR", c, sizeof c);
    ok("pt-BR normalizado casa pt", ling_casa(c, "pt") && !strcmp(ling_selo(c), "PT-BR"));
  }

  // Nomes para a tela.
  ok("nome de pob", !strcmp(ling_nome("pob"), "Português (BR)"));
  ok("nome de spa", !strcmp(ling_nome("spa"), "Espanhol"));
  // Sem nome na tabela, o CODIGO em maiusculas — diz mais que "Legenda 3".
  ok("nome de glg", !strcmp(ling_nome("glg"), "Galego"));
  ok("nome de xyz", !strcmp(ling_nome("xyz"), "XYZ"));

  // As sentinelas do app web viram "sem filtro", nunca um idioma inventado.
  ling_conta_audio("DEVICE");   ok("DEVICE = sem filtro",  !ling_audio()[0]);
  ling_conta_audio("DEFAULT");  ok("DEFAULT = sem filtro", !ling_audio()[0]);
  ling_conta_legenda("off");    ok("off = sem filtro",     !ling_legenda()[0]);
  ling_conta_legenda("es");     ok("es entra",             !strcmp(ling_legenda(), "es"));

  // A escolha desta TV ganha da conta, e voltar para "" devolve o comando a ela.
  ling_local_legenda("fr");     ok("local vence a conta",  !strcmp(ling_legenda(), "fr"));
  ling_local_legenda("");       ok("sem local, volta a conta", !strcmp(ling_legenda(), "es"));

  // "Todas" na tela e "*" no codigo, e tem de significar SEM FILTRO.
  ling_local_legenda("*");      ok("* = sem filtro",       !ling_legenda()[0]);

  // #129: a legenda preferida LIGA sozinha. Embutida antes da de addon;
  // espera enquanto os idiomas embutidos podem chegar; nunca liga sem
  // preferencia nem com "none".
  { const char *emb[] = { "por", "eng" }, *semEtiqueta[] = { "", "" };
    const char *add[] = { "pt-br", "en" };
    ok("auto: embutida en", ling_legenda_auto("en", emb, 2, 1, add, 2, 1) == 1);
    ok("auto: embutida mesmo com a sonda aberta",
       ling_legenda_auto("en", emb, 2, 0, add, 2, 0) == 1);
    ok("auto: espera a sonda do MKV",
       ling_legenda_auto("en", semEtiqueta, 2, 0, add, 2, 1) == LING_AUTO_ESPERA);
    ok("auto: addon quando o arquivo nao tem",
       ling_legenda_auto("en", semEtiqueta, 2, 1, add, 2, 1) == 3);
    ok("auto: pt aceita pt-br do addon",
       ling_legenda_auto("pt", NULL, 0, 1, add, 2, 1) == 0);
    ok("auto: espera o fio dos addons",
       ling_legenda_auto("es", emb, 2, 1, add, 2, 0) == LING_AUTO_ESPERA);
    ok("auto: nada em es",
       ling_legenda_auto("es", emb, 2, 1, add, 2, 1) == LING_AUTO_NADA);
    ok("auto: sem preferencia nao liga",
       ling_legenda_auto("", emb, 2, 1, add, 2, 1) == LING_AUTO_NADA);
    ok("auto: none nao liga",
       ling_legenda_auto("none", emb, 2, 1, add, 2, 1) == LING_AUTO_NADA); }

  // Faixa so de LETREIROS: pelo nome ou pela FlagForced; a faixa inteira que
  // tambem traz as placas ("Full + Songs") nao e letreiro.
  ok("letreiro: Signs & Songs", ling_letreiro("Signs & Songs", 0));
  ok("letreiro: English [Forced]", ling_letreiro("English [Forced]", 0));
  ok("letreiro: songs", ling_letreiro("songs", 0));
  ok("letreiro: Português (Forçada)", ling_letreiro("Portugu\xc3\xaas (For\xc3\xa7" "ada)", 0));
  ok("letreiro: flag do arquivo", ling_letreiro("", 1));
  ok("letreiro: Full nao", !ling_letreiro("Full + Songs", 0));
  ok("letreiro: SDH nao", !ling_letreiro("English SDH", 0));
  ok("letreiro: Design nao e sign", !ling_letreiro("Design", 0));
  ok("letreiro: sem nome nem flag", !ling_letreiro(NULL, 0));

  // Idioma pelo NOME da faixa, so quando ele diz um idioma com todas as letras.
  { const char *c;
    c = ling_do_nome("Portugu\xc3\xaas");      ok("nome: Português = por", c && !strcmp(c, "por"));
    c = ling_do_nome("Brazilian Portuguese");  ok("nome: Brazilian Portuguese = pob", c && !strcmp(c, "pob"));
    c = ling_do_nome("English SDH");           ok("nome: English SDH = eng", c && !strcmp(c, "eng"));
    c = ling_do_nome("Espa\xc3\xb1ol (Latino)"); ok("nome: Español = spa", c && !strcmp(c, "spa"));
    ok("nome: Signs & Songs nao diz idioma", !ling_do_nome("Signs & Songs"));
    ok("nome: dois idiomas = nenhum", !ling_do_nome("English / Portuguese"));
    ok("nome: Full nao diz idioma", !ling_do_nome("Full"));
    ok("nome: vazio", !ling_do_nome("")); }

  // #287: o TIPO da legenda e a escolha automatica com o audio.
  ok("tipo: flag = forcada", ling_tipo_legenda("", 1, 0) == LING_LEG_FORCADA);
  ok("tipo: nome Forced", ling_tipo_legenda("English [Forced]", 0, 0) == LING_LEG_FORCADA);
  ok("tipo: Forcada pt", ling_tipo_legenda("Portugu\xc3\xaas (For\xc3\xa7" "ada)", 0, 0) == LING_LEG_FORCADA);
  ok("tipo: Signs & Songs", ling_tipo_legenda("Signs & Songs", 0, 0) == LING_LEG_LETREIROS);
  ok("tipo: flag + Signs = letreiros", ling_tipo_legenda("Signs", 1, 0) == LING_LEG_LETREIROS);
  ok("tipo: SDH pelo nome", ling_tipo_legenda("English SDH", 0, 0) == LING_LEG_SDH);
  ok("tipo: SDH pela flag", ling_tipo_legenda("", 0, 1) == LING_LEG_SDH);
  ok("tipo: CC palavra", ling_tipo_legenda("English (CC)", 0, 0) == LING_LEG_SDH);
  ok("tipo: Full", ling_tipo_legenda("Full", 0, 0) == LING_LEG_COMPLETA);
  ok("tipo: Full + Songs = completa", ling_tipo_legenda("Full + Songs", 0, 0) == LING_LEG_COMPLETA);
  ok("tipo: Full SDH = SDH", ling_tipo_legenda("Full SDH", 0, 0) == LING_LEG_SDH);
  ok("tipo: sem nome = comum", ling_tipo_legenda(NULL, 0, 0) == LING_LEG_COMUM);
  ok("tipo: Design/Accent nao", ling_tipo_legenda("Design Accent", 0, 0) == LING_LEG_COMUM);
  ok("tipo: rotulos", !strcmp(ling_tipo_legenda_rotulo(LING_LEG_SDH), "SDH") &&
     !ling_tipo_legenda_rotulo(LING_LEG_COMUM) && !strcmp(ling_tipo_legenda_rotulo(LING_LEG_FORCADA), "For\xc3\xa7" "ada"));
  { // pt completa, pt forcada, en SDH, en completa
    const char *emb[] = { "por", "por", "eng", "eng" }, *add[] = { "pob", "eng" };
    const int tp[] = { LING_LEG_COMUM, LING_LEG_FORCADA, LING_LEG_SDH, LING_LEG_COMPLETA };
    ok("auto287: audio pt + leg pt -> forcada pt",
       ling_legenda_auto_tipo("pt", "por", 1, emb, tp, 4, 1, add, 2, 1) == 1);
    ok("auto287: audio en + leg pt -> completa pt, nao a forcada",
       ling_legenda_auto_tipo("pt", "eng", 1, emb, tp, 4, 1, add, 2, 1) == 0);
    ok("auto287: ajuste desligado -> completa pt como antes",
       ling_legenda_auto_tipo("pt", "por", 0, emb, tp, 4, 1, add, 2, 1) == 0);
    ok("auto287: audio desconhecido -> completa",
       ling_legenda_auto_tipo("pt", "", 1, emb, tp, 4, 1, add, 2, 1) == 0);
    ok("auto287: leg en, audio pt -> completa en antes da SDH",
       ling_legenda_auto_tipo("en", "por", 1, emb, tp, 4, 1, add, 2, 1) == 3);
    ok("auto287: none -> nada", ling_legenda_auto_tipo("none", "por", 1, emb, tp, 4, 1, add, 2, 1) == LING_AUTO_NADA); }
  { // so SDH e forcada em ingles; addon pt
    const char *emb[] = { "eng", "eng" }, *add[] = { "pob" };
    const int tp[] = { LING_LEG_FORCADA, LING_LEG_SDH };
    ok("auto287: so SDH serve -> SDH",
       ling_legenda_auto_tipo("en", "por", 1, emb, tp, 2, 1, add, 1, 1) == 1);
    ok("auto287: audio pt sem forcada pt -> nada (nunca o addon)",
       ling_legenda_auto_tipo("pt", "pt-BR", 1, emb, tp, 2, 1, add, 1, 1) == LING_AUTO_NADA);
    ok("auto287: sonda pendente -> espera",
       ling_legenda_auto_tipo("pt", "por", 1, emb, tp, 2, 0, add, 1, 1) == LING_AUTO_ESPERA);
    ok("auto287: audio en + leg pt -> addon pt",
       ling_legenda_auto_tipo("pt", "eng", 1, emb, tp, 2, 1, add, 1, 1) == 2); }
  { // so forcada no idioma, audio em outro: nunca a forcada como principal
    const char *emb[] = { "por" }; const int tp[] = { LING_LEG_FORCADA };
    ok("auto287: forcada nunca e a principal",
       ling_legenda_auto_tipo("pt", "eng", 1, emb, tp, 1, 1, NULL, 0, 1) == LING_AUTO_NADA);
    const int tl[] = { LING_LEG_LETREIROS };
    ok("auto287: sem forcada, letreiros com audio no idioma",
       ling_legenda_auto_tipo("pt", "por", 1, emb, tl, 1, 1, NULL, 0, 1) == 0); }

  // #205: quem pediu "hr" prefere a faixa "hr" a uma "hbs" que tambem serve —
  // o casamento exato de familia vence o conjunto, mesmo vindo depois.
  { const char *emb[] = { "hbs", "hr" }, *add[] = { "sh", "sr" };
    ok("auto205: hr pedido vence hbs na frente",
       ling_legenda_auto("hr", emb, 2, 1, add, 2, 1) == 1);
    ok("auto205: sr do addon vence sh na frente",
       ling_legenda_auto("sr", NULL, 0, 1, add, 2, 1) == 1);
    ok("auto205: hbs pedido vence hr na frente",
       ling_legenda_auto("hbs", emb, 2, 1, NULL, 0, 1) == 0);
    ok("auto205: sem hr, hbs serve",
       ling_legenda_auto("hr", emb, 1, 1, NULL, 0, 1) == 0);
    const int tp[] = { LING_LEG_COMUM, LING_LEG_COMUM };
    ok("auto205: tipo tambem ranqueia hr antes de hbs",
       ling_legenda_auto_tipo("hr", "", 0, emb, tp, 2, 1, NULL, 0, 1) == 1);
    const char *steve[] = { "eng", "hbs-hrv" };
    ok("auto429: Steve hbs-hrv liga para hr",
       ling_legenda_auto("hr", steve, 2, 1, NULL, 0, 1) == 1); }

  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("linguas ok\n");
  return 0;
}
