#include "linguas.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

// Tabela com ACENTO — e nome de idioma na tela, nao identificador. Traz os
// codigos de tres letras (ISO 639-2) alem dos de duas, porque MKV de release
// etiqueta quase sempre com os de tres. Estava dentro de video.c, privada;
// addons.c mantinha uma versao propria com tres idiomas. Uma so agora.
static const struct { const char *cod, *nome; } NOMES[] = {
  { "pt", "Português" },  { "pob", "Português (BR)" }, { "por", "Português" },
  { "pt-br", "Português (BR)" }, { "ptb", "Português (BR)" }, { "br", "Português (BR)" },
  { "en", "Inglês" },     { "eng", "Inglês" },
  { "es", "Espanhol" },   { "spa", "Espanhol" }, { "esp", "Espanhol" },
  { "fr", "Francês" },    { "fre", "Francês" },  { "fra", "Francês" },
  { "de", "Alemão" },     { "ger", "Alemão" },   { "deu", "Alemão" },
  { "it", "Italiano" },   { "ita", "Italiano" },
  { "ja", "Japonês" },    { "jpn", "Japonês" },
  { "ko", "Coreano" },    { "kor", "Coreano" },
  { "zh", "Chinês" },     { "chi", "Chinês" },   { "zho", "Chinês" },
  { "ru", "Russo" },      { "rus", "Russo" },
  { "ar", "Árabe" },      { "ara", "Árabe" },
  { "hi", "Hindi" },      { "hin", "Hindi" },
  { "nl", "Holandês" },   { "dut", "Holandês" }, { "nld", "Holandês" },
  { "sv", "Sueco" },      { "swe", "Sueco" },
  { "no", "Norueguês" },  { "nor", "Norueguês" },
  { "da", "Dinamarquês" },{ "dan", "Dinamarquês" },
  { "fi", "Finlandês" },  { "fin", "Finlandês" },
  { "pl", "Polonês" },    { "pol", "Polonês" },
  { "tr", "Turco" },      { "tur", "Turco" },
  { "he", "Hebraico" },   { "heb", "Hebraico" },
  { "th", "Tailandês" },  { "tha", "Tailandês" },
  { "cs", "Tcheco" },     { "cze", "Tcheco" },
  { "el", "Grego" },      { "gre", "Grego" },
  { "hu", "Húngaro" },    { "hun", "Húngaro" },
  { "ro", "Romeno" },     { "rum", "Romeno" },
  { "uk", "Ucraniano" },  { "ukr", "Ucraniano" },
  { "vi", "Vietnamita" }, { "vie", "Vietnamita" },
  { "id", "Indonésio" },  { "ind", "Indonésio" },
};
#define NOMES_N ((int)(sizeof NOMES / sizeof *NOMES))

const char *ling_nome(const char *c) {
  int i;
  if (!c || !*c) return "";
  for (i = 0; i < NOMES_N; i++)
    if (!strcasecmp(c, NOMES[i].cod)) return NOMES[i].nome;
  // Sem nome na tabela, devolve o CODIGO EM MAIUSCULAS — e o que o app web faz
  // quando nao sabe nomear ("ENG", "POR"). Mostrar o codigo diz alguma coisa;
  // cair em "Legenda 3" nao diz nada.
  //
  // RODIZIO DE BUFFERS, e nao um `static char` unico: com um so, duas chamadas
  // na MESMA expressao devolvem o mesmo ponteiro. O teste pegou isso valendo
  // "glg" == "cat", porque a comparacao acontecia depois das duas escritas.
  // Tambem afeta qualquer printf com dois idiomas.
  { static char cx[4][16]; static int giro;
    char *d = cx[giro++ & 3];
    size_t k;
    for (k = 0; c[k] && k + 1 < 16; k++)
      d[k] = (c[k] >= 'a' && c[k] <= 'z') ? (char)(c[k] - 32) : c[k];
    d[k] = 0;
    return d; }
}

// Codigo canonico de FAMILIA, para que uma preferencia por "pt" aceite a faixa
// etiquetada "pob", "por" ou "pt-BR". Pedir portugues e receber "nao ha
// legenda" porque o arquivo diz "pob" seria absurdo para quem assiste — a
// distincao regional importa na hora de ORDENAR, nao na de aceitar.
static const char *familia(const char *c) {
  static const struct { const char *cod, *fam; } F[] = {
    { "pob","pt" },{ "por","pt" },{ "ptb","pt" },{ "pt-br","pt" },{ "pt_br","pt" },{ "br","pt" },
    { "eng","en" },{ "en-us","en" },{ "en_us","en" },{ "en-gb","en" },{ "en_gb","en" },
    { "spa","es" },{ "esp","es" },{ "fre","fr" },{ "fra","fr" },{ "ger","de" },{ "deu","de" },
    { "ita","it" },{ "jpn","ja" },{ "kor","ko" },{ "chi","zh" },{ "zho","zh" },
    { "rus","ru" },{ "ara","ar" },{ "hin","hi" },{ "dut","nl" },{ "nld","nl" },
    { "swe","sv" },{ "nor","no" },{ "dan","da" },{ "fin","fi" },{ "pol","pl" },
    { "tur","tr" },{ "heb","he" },{ "tha","th" },{ "cze","cs" },{ "gre","el" },
    { "hun","hu" },{ "rum","ro" },{ "ukr","uk" },{ "vie","vi" },{ "ind","id" },
  };
  size_t i;
  for (i = 0; i < sizeof F / sizeof *F; i++)
    if (!strcasecmp(c, F[i].cod)) return F[i].fam;
  // "pt-BR" generico: o que vem antes do tracinho ja e a familia.
  { static char base[4][8]; static int giro;
    char *d = base[giro++ & 3];
    size_t k;
    for (k = 0; c[k] && c[k] != '-' && c[k] != '_' && k + 1 < 8; k++)
      d[k] = (char)(c[k] >= 'A' && c[k] <= 'Z' ? c[k] + 32 : c[k]);
    d[k] = 0;
    return d; }
}

// Duas etiquetas sao o MESMO idioma quando a familia coincide. Codigo que a
// tabela nao conhece cai na comparacao crua, sem inventar parentesco.
int ling_casa(const char *codigo, const char *pref) {
  if (!pref || !*pref) return 1;            // sem preferencia: passa tudo
  if (!codigo || !*codigo) return 0;
  if (!strcasecmp(codigo, pref)) return 1;
  return !strcasecmp(familia(codigo), familia(pref));
}

// ------------------------------------------------------------ preferencias

static char contaLeg[16], contaLeg2[16], contaAud[16];
static char localLeg[16], localAud[16];

// As sentinelas do web viram vazio (= sem filtro) ou "none" (= nenhuma).
// "DEVICE"/"DEFAULT"/"ORIGINAL" dizem "deixe o arquivo decidir", que deste lado
// e exatamente nao filtrar.
static void guardar(char *dest, size_t n, const char *v) {
  if (!v || !*v) { dest[0] = 0; return; }
  if (!strcasecmp(v, "off") || !strcasecmp(v, "device") ||
      !strcasecmp(v, "default") || !strcasecmp(v, "original") ||
      !strcasecmp(v, "system") || !strcmp(v, "*")) { dest[0] = 0; return; }
  snprintf(dest, n, "%s", v);
}

void ling_conta_legenda(const char *v)  { guardar(contaLeg,  sizeof contaLeg,  v); }
void ling_conta_legenda2(const char *v) { guardar(contaLeg2, sizeof contaLeg2, v); }
void ling_conta_audio(const char *v)    { guardar(contaAud,  sizeof contaAud,  v); }
// A escolha LOCAL preserva o "*": "Todas" e uma decisao ("nao filtre"), nao a
// ausencia de decisao. Tratar as duas como vazio fazia escolher "Todas" cair de
// volta na preferencia da conta — ou seja, o ajuste nao obedecia.
static void guardarLocal(char *dest, size_t n, const char *v) {
  if (v && !strcmp(v, "*")) { snprintf(dest, n, "*"); return; }
  // "~" pelo mesmo motivo do "*": e uma DECISAO ("o original deste titulo"), e
  // deixar a conta sobrescrever faria o ajuste nao obedecer.
  if (v && !strcmp(v, "~")) { snprintf(dest, n, "~"); return; }
  guardar(dest, n, v);
}
void ling_local_legenda(const char *v)  { guardarLocal(localLeg, sizeof localLeg, v); }
void ling_local_audio(const char *v)    { guardarLocal(localAud, sizeof localAud, v); }

// A escolha desta TV ganha da conta. Quem mexeu no aparelho quis mexer AQUI, e
// ver a proxima sincronizacao desfazer isso e o tipo de comportamento que faz a
// pessoa parar de confiar no ajuste.
// O IDIOMA ORIGINAL DO TITULO QUE ESTA ABERTO AGORA.
//
// Quem sabe disso e a ficha do TMDB (extras_idioma_original), e quem abre o
// titulo e quem avisa. Este arquivo nao consulta ninguem de proposito: ele e a
// politica de idioma e nao a fonte do dado — fazer linguas.c incluir extras.h
// amarraria a tabela de idiomas ao pedido de rede da tela de titulo.
static char origAtual[8];
void ling_definir_original(const char *cod) {
  if (!cod || !*cod) { origAtual[0] = 0; return; }
  snprintf(origAtual, sizeof origAtual, "%s", cod);
}
const char *ling_original(void) { return origAtual; }

static const char *emVigor(const char *local, const char *conta) {
  if (!local[0]) return conta;      // sem escolha local: a conta manda
  if (local[0] == '*') return "";   // "Todas": sem filtro, e a conta nao volta
  // "Original": vale o idioma DESTE titulo. Sem ele — titulo aberto direto da
  // home, ou TMDB que nao respondeu — devolve "" e o comportamento e o de
  // sempre: nao trocar de faixa. Chutar um idioma aqui seria pior que nao
  // trocar, porque a pessoa nao teria como saber que foi o app que escolheu.
  if (local[0] == '~') return origAtual;
  return local;
}
const char *ling_legenda(void)  { return emVigor(localLeg, contaLeg); }
// A secundaria so existe na conta: a tela oferece uma escolha, nao duas.
const char *ling_legenda2(void) { return localLeg[0] ? "" : contaLeg2; }
const char *ling_audio(void)    { return emVigor(localAud, contaAud); }

// ------------------------------------------------------------ lista da UI

// A LISTA COBRE TODA A TABELA DE NOMES, e nao um recorte de doze idiomas.
//
// O recorte anterior parava em "hi" e deixava de fora sueco, holandes, polones,
// turco, tcheco, grego, hebraico e mais — todos com nome nesta mesma tabela e
// todos aceitos por ling_casa. O efeito e o do relato do Reddit: um usuario
// sueco abre Ajustes, nao encontra "Sueco" para escolher, e fica com o que a
// conta trouxe. Nao havia motivo tecnico para o recorte; a linha de Ajustes
// percorre a lista com esquerda/direita e nao tem teto proprio (ver
// LING_MAX_OPC em ajustes.c, que acompanha este tamanho).
//
// ORDEM: os tres primeiros sao os idiomas com mais legenda publicada no acervo
// que este app consulta, e depois vem o resto por nome. Ordenar tudo por nome
// poria "Alemão" na frente de "Português" numa lista de trinta itens navegada
// tecla por tecla.
static const char *OPCOES_COD[] = {
  // "~" = ORIGINAL DO TITULO. Nao e um idioma: e "descubra qual e o idioma
  // deste filme e toque esse". Quem resolve e ling_definir_original(), chamado
  // por quem abre o titulo; aqui so mora o marcador.
  "", "*", "~",
  "pt", "en", "es",
  "de", "ar", "zh", "da", "ko", "fr", "el", "he", "nl", "hi", "hu", "id",
  "it", "ja", "no", "pl", "ro", "ru", "sv", "th", "cs", "tr", "uk", "vi", "fi"
};
int ling_opcao_n(void) { return (int)(sizeof OPCOES_COD / sizeof *OPCOES_COD); }
const char *ling_opcao_codigo(int i) {
  return (i >= 0 && i < ling_opcao_n()) ? OPCOES_COD[i] : "";
}
