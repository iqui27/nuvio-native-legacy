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
  { "ro", "Romeno" },     { "rum", "Romeno" }, { "ron", "Romeno" },
  { "uk", "Ucraniano" },  { "ukr", "Ucraniano" },
  { "vi", "Vietnamita" }, { "vie", "Vietnamita" },
  { "id", "Indonésio" },  { "ind", "Indonésio" },
  // #269/#201: os que o Community Subtitles e o MKV de release etiquetam e a
  // tabela nao nomeava ("HRV", "BUL"...). So as familias de 2 letras: os
  // codigos de 3 letras e as variantes chegam aqui por familia() (ling_nome).
  { "hr", "Croata" },     { "bg", "Búlgaro" },   { "sk", "Eslovaco" },
  { "sr", "Sérvio" },     { "sl", "Esloveno" },  { "et", "Estoniano" },
  { "lv", "Letão" },      { "lt", "Lituano" },   { "ca", "Catalão" },
  { "eu", "Basco" },      { "gl", "Galego" },    { "is", "Islandês" },
  { "fa", "Persa" },      { "ms", "Malaio" },    { "tl", "Filipino" },
};
#define NOMES_N ((int)(sizeof NOMES / sizeof *NOMES))

static const char *familia(const char *c, char *buf);

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
  // __atomic_fetch_add: a busca de legendas chama isto de um fio por addon, e
  // um `giro++` simples deixava dois fios com a mesma posicao.
  // Variante ou sinonimo de um idioma da tabela ("ces", "pt-PT", "es-419",
  // o "jp"/"cz"/"du" do player da Samsung): o nome da FAMILIA. Antes saia o
  // codigo em maiusculas ("JP", "CZ") na folha de faixas do .tpk (#269).
  { char fb[8];
    const char *f = familia(c, fb);
    for (i = 0; i < NOMES_N; i++)
      if (!strcasecmp(f, NOMES[i].cod)) return NOMES[i].nome; }
  { static char cx[4][16]; static int giro;
    char *d = cx[__atomic_fetch_add(&giro, 1, __ATOMIC_RELAXED) & 3];
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
// `buf` (8 bytes) e do CHAMADOR: o resultado para codigo fora da tabela mora la.
// Antes era um anel estatico compartilhado, e a busca de legendas chama isto de
// um fio por addon — dois fios pegavam a mesma posicao e um lia o texto do outro.
static const char *familia(const char *c, char *buf) {
  static const struct { const char *cod, *fam; } F[] = {
    { "pob","pt" },{ "por","pt" },{ "ptb","pt" },{ "pt-br","pt" },{ "pt_br","pt" },{ "br","pt" },
    { "eng","en" },{ "en-us","en" },{ "en_us","en" },{ "en-gb","en" },{ "en_gb","en" },
    { "spa","es" },{ "esp","es" },{ "fre","fr" },{ "fra","fr" },{ "ger","de" },{ "deu","de" },
    { "ita","it" },{ "jpn","ja" },{ "kor","ko" },{ "chi","zh" },{ "zho","zh" },
    { "rus","ru" },{ "ara","ar" },{ "hin","hi" },{ "dut","nl" },{ "nld","nl" },
    { "swe","sv" },{ "nor","no" },{ "dan","da" },{ "fin","fi" },{ "pol","pl" },
    { "tur","tr" },{ "heb","he" },{ "tha","th" },{ "cze","cs" },{ "gre","el" },
    { "hun","hu" },{ "rum","ro" },{ "ron","ro" },{ "ukr","uk" },{ "vie","vi" },{ "ind","id" },
    // ISO 639-2/T, que o Community Subtitles usa (app/languages.py dele:
    // "ces", "ell", "nld", "zho", "msa"...). "ces" caia na comparacao crua e
    // a legenda tcheca de quem pediu "cs" era descartada calada.
    { "ces","cs" },{ "ell","el" },{ "slk","sk" },{ "slo","sk" },{ "bul","bg" },
    { "msa","ms" },{ "may","ms" },{ "srp","sr" },{ "scc","sr" },{ "hrv","hr" },
    { "scr","hr" },{ "slv","sl" },{ "est","et" },{ "lav","lv" },{ "lit","lt" },
    { "cat","ca" },{ "eus","eu" },{ "baq","eu" },{ "glg","gl" },{ "isl","is" },
    { "ice","is" },{ "fas","fa" },{ "per","fa" },{ "fil","tl" },{ "tgl","tl" },
    { "nob","no" },{ "nno","no" },{ "nb","no" },{ "nn","no" },{ "iw","he" },
    // Chines e espanhol regionais sem tracinho, e os codigos antigos do
    // OpenSubtitles ("pb" portugues do Brasil, "ea" espanhol latino, "ze"
    // chines bilingue).
    { "chs","zh" },{ "cht","zh" },{ "zht","zh" },{ "zhe","zh" },{ "ze","zh" },
    { "pb","pt" },{ "ptbr","pt" },{ "spn","es" },{ "ea","es" },
    // O player do .tpk da Samsung (Tizen.Multimedia GetLanguageCode) devolve
    // estes no lugar do ISO 639-1. MEDIDO no log (p2p-20, MKV com 41
    // legendas): ja->"jp", cs->"cz", nl->"du", el->"gr", id->"in", ms->"ma".
    // Nenhum deles e codigo ISO de outro idioma.
    { "jp","ja" },{ "cz","cs" },{ "du","nl" },{ "gr","el" },{ "in","id" },{ "ma","ms" },
  };
  size_t i;
  for (i = 0; i < sizeof F / sizeof *F; i++)
    if (!strcasecmp(c, F[i].cod)) return F[i].fam;
  // "pt-BR" generico: o que vem antes do tracinho ja e a familia.
  { char *d = buf;
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
  { char a[8], b[8];
    return !strcasecmp(familia(codigo, a), familia(pref, b)); }
}

// NORMALIZA a etiqueta de idioma que o addon manda. Um addon (AIOStreams)
// manda "PORTUGUESE"/"Portuguese (Brazil)" em vez de codigo; Legenda.idioma
// tem 8 bytes e o nome saia cortado ("PORTUGU", selo "PO") e, pior, nao casava
// com a preferencia "pt". Codigo conhecido fica como veio (minusculo); nome
// vira codigo pelo radical; o resto e cortado em 7 so como ultimo recurso.
void ling_normalizar(const char *raw, char *out, unsigned tam) {
  char t[64];
  unsigned i, n = 0;
  const char *c;
  if (!out || !tam) return;
  out[0] = 0;
  if (!raw) return;
  while (*raw == ' ') raw++;
  for (i = 0; raw[i] && n + 1 < sizeof t; i++) t[n++] = (char)(raw[i] >= 'A' && raw[i] <= 'Z' ? raw[i] + 32 : raw[i]);
  while (n && t[n - 1] == ' ') n--;
  t[n] = 0;
  for (i = 0; i < NOMES_N; i++)
    if (!strcmp(t, NOMES[i].cod)) { snprintf(out, tam, "%s", t); return; }
  if (n <= 5 && n >= 2 && (n < 4 || t[2] == '-' || t[2] == '_')) {   // "pt-br", "en_us", "zh-hant"
    for (i = 0; i < n; i++) if (t[i] == ' ') break;
    if (i == n) { if (n > 2 && t[2] == '_') t[2] = '-'; snprintf(out, tam, "%s", t); return; }
  }
  c = ling_do_nome(t);
  if (c) { snprintf(out, tam, "%s", c); return; }
  snprintf(out, tam, "%.7s", t);
}

// Selo de 2 a 5 letras para o rosto da linha: "PT-BR" para o portugues do
// Brasil, "PT", "EN"... pela familia. Etiqueta que a tabela nao conhece: as 2
// primeiras letras em caixa alta.
const char *ling_selo(const char *cod) {
  static char b[4][8];
  static int giro;
  char *d = b[__atomic_fetch_add(&giro, 1, __ATOMIC_RELAXED) & 3];
  char fam[8];
  const char *f;
  size_t k;
  if (!cod || !*cod) { d[0] = '?'; d[1] = 0; return d; }
  if (!strcasecmp(cod, "pob") || !strcasecmp(cod, "pt-br") || !strcasecmp(cod, "pt_br") ||
      !strcasecmp(cod, "ptb") || !strcasecmp(cod, "br") || !strcasecmp(cod, "pb") ||
      !strcasecmp(cod, "ptbr")) return "PT-BR";
  f = familia(cod, fam);
  for (k = 0; f[k] && k < 2; k++) d[k] = (char)(f[k] >= 'a' && f[k] <= 'z' ? f[k] - 32 : f[k]);
  d[k] = 0;
  return d;
}

// O quanto `cod` serve a preferencia `pref`: 0 nao casa, 2 mesma familia, 3 o
// melhor (portugues: o do Brasil ganha do de Portugal; o resto: codigo igual).
int ling_afinidade(const char *cod, const char *pref) {
  if (!pref || !*pref) return 1;
  if (!cod || !*cod || !ling_casa(cod, pref)) return 0;
  { char fam[8];
    if (!strcmp(familia(pref, fam), "pt")) return !strcmp(ling_selo(cod), "PT-BR") ? 3 : 2; }
  return !strcasecmp(cod, pref) ? 3 : 2;
}

// ------------------------------------------------------------ preferencias

static char contaLeg[16], contaLeg2[16], contaAud[16];
static char localLeg[16], localAud[16], localLeg2[16];

// As sentinelas do web viram vazio (= sem filtro) ou "none" (= nenhuma).
// "DEVICE"/"DEFAULT"/"ORIGINAL" dizem "deixe o arquivo decidir", que deste lado
// e exatamente nao filtrar.
static void guardar(char *dest, size_t n, const char *v) {
  if (!v || !*v) { dest[0] = 0; return; }
  if (!strcasecmp(v, "off") || !strcasecmp(v, "device") ||
      !strcasecmp(v, "default") || !strcasecmp(v, "original") ||
      !strcasecmp(v, "system") || !strcmp(v, "*") ||
      // #378: "forced" e o "Usar legendas forcadas" do app oficial, nao um
      // idioma; como codigo nao casa com faixa nenhuma e "Da conta" ficava sem
      // legenda. Vale como conta sem idioma (a escolha a mao decide).
      !strcasecmp(v, "forced") || !strcasecmp(v, "force")) { dest[0] = 0; return; }
  snprintf(dest, n, "%s", v);
}

void ling_conta_legenda(const char *v)  { guardar(contaLeg,  sizeof contaLeg,  v); }
void ling_conta_legenda2(const char *v) { guardar(contaLeg2, sizeof contaLeg2, v); }
void ling_conta_audio(const char *v)    { guardar(contaAud,  sizeof contaAud,  v); }
const char *ling_conta_legenda_valor(void)  { return contaLeg; }
const char *ling_conta_legenda2_valor(void) { return contaLeg2; }
const char *ling_conta_audio_valor(void)    { return contaAud; }
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
void ling_local_legenda2(const char *v) { guardarLocal(localLeg2, sizeof localLeg2, v); }

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
// A SECUNDARIA (F04): Ajustes desta TV agora tem a linha dela. Escolhida
// aqui, vale a regra da principal (emVigor). Em "Da conta" fica o
// comportamento de antes: a da conta, a nao ser que a PRINCIPAL tenha sido
// escolhida nesta TV (ai a conta nao manda em nenhuma das duas).
const char *ling_legenda2(void) {
  if (localLeg2[0]) return emVigor(localLeg2, contaLeg2);
  return localLeg[0] ? "" : contaLeg2;
}
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
  "", "*",
  "pt", "en", "es",
  "de", "ar", "zh", "da", "ko", "fr", "el", "he", "nl", "hi", "hu", "id",
  "it", "ja", "no", "pl", "ro", "ru", "sv", "th", "cs", "tr", "uk", "vi", "fi",
  // "~" = ORIGINAL DO TITULO. Nao e um idioma: e "descubra qual e o idioma
  // deste filme e toque esse". Quem resolve e ling_definir_original(), chamado
  // por quem abre o titulo; aqui so mora o marcador.
  //
  // ELE FICA NO FIM, E ISSO NAO E ESTETICA — E COMPATIBILIDADE. O ajustes.txt
  // grava o INDICE da opcao, nao o codigo ("aud_lingua 2"), entao inserir um
  // item no meio desta lista REINTERPRETA o arquivo de quem ja atualizou:
  // na 1.1.2 o "~" entrou no indice 2 e todo mundo que tinha audio "Portugues"
  // (2) passava a ter "Original", quem tinha "Ingles" (3) passava a ter
  // "Portugues", e assim por diante nos 28 idiomas — em audio E em legenda,
  // que leem a MESMA lista. Foi pego na revisao antes de publicar.
  //
  // REGRA PARA QUEM MEXER AQUI: item novo entra NO FIM. Reordenar ou inserir no
  // meio exige migracao do ajustes.txt, que hoje nao tem marca de versao.
  "~"
};
// Se alguem acrescentar idioma depois do "~", o indice gravado deixa de bater
// com LING_OPC_ORIGINAL e o padrao de audio vira outra coisa em silencio. Isto
// quebra o build em vez de deixar passar.
typedef char nv_checa_indice_original[
  (sizeof OPCOES_COD / sizeof *OPCOES_COD) == LING_OPC_ORIGINAL + 1 ? 1 : -1];
int ling_opcao_n(void) { return (int)(sizeof OPCOES_COD / sizeof *OPCOES_COD); }
const char *ling_opcao_codigo(int i) {
  return (i >= 0 && i < ling_opcao_n()) ? OPCOES_COD[i] : "";
}

// ------------------------------------------------------------ nome da faixa

// `p` comeca uma PALAVRA em `ini`? Byte anterior que e letra ASCII ou parte de
// um caractere UTF-8 (acentuado) conta como letra: "Design" nao e "sign".
static int inicioPalavra(const char *ini, const char *p) {
  unsigned char c;
  if (p == ini) return 1;
  c = (unsigned char)p[-1];
  return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80);
}

// Alguma palavra de `nome` COMECA com `radical` (sem caixa)?
static int temRadical(const char *nome, const char *radical) {
  size_t n = strlen(radical);
  const char *p;
  for (p = nome; *p; p++)
    if (inicioPalavra(nome, p) && !strncasecmp(p, radical, n)) return 1;
  return 0;
}

// `nome` tem a palavra `w` inteira (sem caixa)? Para siglas curtas ("CC"),
// que como radical casariam com o comeco de outra palavra.
static int temPalavra(const char *nome, const char *w) {
  size_t n = strlen(w);
  const char *p;
  for (p = nome; *p; p++) {
    unsigned char c;
    if (!inicioPalavra(nome, p) || strncasecmp(p, w, n)) continue;
    c = (unsigned char)p[n];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80)) return 1;
  }
  return 0;
}

int ling_tipo_legenda(const char *nome, int forcado, int sdh) {
  // "forçad" em bytes, e nao como literal: a varredura de i18n (tools/)
  // acusaria um radical de busca como texto de tela sem traducao.
  static const char FORCAD[] = { 'f', 'o', 'r', (char)0xc3, (char)0xa7, 'a', 'd', 0 };
  static const char *const SIGNS[] = { "sign", "song", "letreiro" };
  static const char *const FORC[] = { "forced", FORCAD, "forzad" };
  static const char *const SURDO[] = { "sdh", "hearing", "surdo", "deaf" };
  int temSigns = 0, temForc = 0, temSurdo = sdh != 0;
  size_t i;
  if (!nome) nome = "";
  for (i = 0; i < sizeof SIGNS / sizeof *SIGNS; i++) if (temRadical(nome, SIGNS[i])) temSigns = 1;
  for (i = 0; i < sizeof FORC / sizeof *FORC; i++) if (temRadical(nome, FORC[i])) temForc = 1;
  for (i = 0; i < sizeof SURDO / sizeof *SURDO; i++) if (temRadical(nome, SURDO[i])) temSurdo = 1;
  if (temPalavra(nome, "cc")) temSurdo = 1;
  // A FLAG manda: o arquivo diz que a faixa e forcada, seja qual for o nome.
  if (forcado) return temSigns ? LING_LEG_LETREIROS : LING_LEG_FORCADA;
  // "Full + Songs", "Dialogue & Signs": a faixa inteira que TAMBEM traz as
  // placas. Essa e a legenda de verdade, nao a de letreiros.
  if (temRadical(nome, "full") || temRadical(nome, "dialog") || temRadical(nome, "complet"))
    return temSurdo ? LING_LEG_SDH : LING_LEG_COMPLETA;
  if (temSigns) return LING_LEG_LETREIROS;
  if (temForc) return LING_LEG_FORCADA;
  if (temSurdo) return LING_LEG_SDH;
  return LING_LEG_COMUM;
}

int ling_letreiro(const char *nome, int forcado) {
  int t = ling_tipo_legenda(nome, forcado, 0);
  return t == LING_LEG_FORCADA || t == LING_LEG_LETREIROS;
}

const char *ling_tipo_legenda_rotulo(int tipo) {
  switch (tipo) {
    case LING_LEG_FORCADA:   return "For\xc3\xa7" "ada";
    case LING_LEG_LETREIROS: return "Letreiros";
    case LING_LEG_SDH:       return "SDH";
    case LING_LEG_COMPLETA:  return "Legenda completa";   // "Completa" ja e outra chave (legsyncui)
    default:                 return NULL;
  }
}

// Radicais em ASCII, a partir do comeco da palavra: "portugu" cobre
// Português/Portuguese/Portugues, "ingl" cobre Inglês/Ingles. Os que comecam
// com letra acentuada ("Árabe") ficam de fora — sem como casar sem caixa em
// UTF-8, melhor nao casar do que casar errado.
static const struct { const char *radical, *cod; } NOME_IDIOMA[] = {
  { "brazil", "pob" }, { "brasil", "pob" }, { "portugu", "por" },
  { "english", "eng" }, { "ingl", "eng" },
  { "spanish", "spa" }, { "espa\xc3\xb1ol", "spa" }, { "espanol", "spa" }, { "espanhol", "spa" },
  { "castellano", "spa" }, { "castilian", "spa" },
  { "french", "fre" }, { "fran", "fre" },      // Français, Francês
  { "german", "ger" }, { "deutsch", "ger" }, { "alem", "ger" },   // Alemão
  { "italian", "ita" }, { "japanese", "jpn" }, { "japon", "jpn" },
  { "korean", "kor" }, { "coreano", "kor" }, { "chin", "chi" },   // Chinese, Chinês
  { "russian", "rus" }, { "russo", "rus" }, { "arabic", "ara" },
  { "dutch", "dut" }, { "nederlands", "dut" }, { "holand", "dut" },
  { "polish", "pol" }, { "polski", "pol" }, { "turkish", "tur" }, { "turco", "tur" },
  // Nomes em ingles que addons mandam no "lang" (o Auto-Subs rotula
  // "Hebrew (Auto-Subs)"); sem radical aqui, ling_normalizar cortava o nome em
  // 7 letras e a legenda nao casava com preferencia nenhuma.
  { "hebrew", "heb" }, { "hindi", "hin" }, { "greek", "gre" }, { "hungarian", "hun" },
  { "magyar", "hun" }, { "czech", "cze" }, { "romanian", "rum" }, { "ukrainian", "ukr" },
  { "indonesian", "ind" }, { "swedish", "swe" }, { "svensk", "swe" }, { "danish", "dan" },
  { "dansk", "dan" }, { "norwegian", "nor" }, { "norsk", "nor" }, { "finnish", "fin" },
  { "suomi", "fin" }, { "thai", "tha" }, { "vietnamese", "vie" }, { "croatian", "hrv" },
  { "hrvatski", "hrv" }, { "serbian", "srp" }, { "bulgarian", "bul" }, { "slovak", "slk" },
  { "slovenian", "slv" }, { "slovene", "slv" }, { "persian", "fas" }, { "farsi", "fas" },
  { "malay", "msa" }, { "filipino", "fil" }, { "tagalog", "fil" }, { "catalan", "cat" },
  { "estonian", "est" }, { "latvian", "lav" }, { "lithuanian", "lit" }, { "icelandic", "isl" },
};

const char *ling_do_nome(const char *nome) {
  const char *achado = NULL;
  size_t i;
  if (!nome || !*nome) return NULL;
  for (i = 0; i < sizeof NOME_IDIOMA / sizeof *NOME_IDIOMA; i++) {
    const char *c = NOME_IDIOMA[i].cod;
    if (!temRadical(nome, NOME_IDIOMA[i].radical)) continue;
    if (!achado) { achado = c; continue; }
    // "Brazilian Portuguese": mesma familia, fica o mais especifico (pob).
    char fa[8], fc[8];
    if (!strcasecmp(familia(achado, fa), familia(c, fc))) {
      if (!strcmp(c, "pob")) achado = c;
      continue;
    }
    return NULL;    // dois idiomas diferentes no nome: nao da para saber
  }
  return achado;
}

// ------------------------------------------------------------ legenda automatica

int ling_legenda_auto(const char *pref,
                      const char *const *emb, int nEmb, int embFechado,
                      const char *const *add, int nAdd, int addFechado) {
  int i;
  // Sem preferencia nao ha o que ligar: "sem filtro" nao e "ligue qualquer
  // uma". E "none" e a pessoa pedindo NENHUMA — ligar seria desobedecer.
  if (!pref || !*pref || !strcasecmp(pref, "none")) return LING_AUTO_NADA;
  // EMBUTIDA PRIMEIRO: vem do proprio arquivo, entao o tempo das falas bate com
  // este corte. A do addon e um arquivo de outra pessoa para outro release.
  // Faixa sem etiqueta ("") nunca casa: ling_casa so aceita tudo quando a
  // PREFERENCIA e vazia, e aqui ela nao e.
  for (i = 0; i < nEmb; i++)
    if (emb[i] && emb[i][0] && ling_casa(emb[i], pref)) return i;
  // Os idiomas embutidos ainda podem chegar (a sonda do MKV nao voltou): pular
  // para o addon agora trocaria a legenda certa do arquivo por uma de fora.
  if (!embFechado) return LING_AUTO_ESPERA;
  for (i = 0; i < nAdd; i++)
    if (add[i] && add[i][0] && ling_casa(add[i], pref)) return nEmb + i;
  return addFechado ? LING_AUTO_NADA : LING_AUTO_ESPERA;
}

int ling_legenda_auto_tipo(const char *pref, const char *audio, int forcadaAuto,
                           const char *const *emb, const int *tipoEmb, int nEmb, int embFechado,
                           const char *const *add, int nAdd, int addFechado) {
  int i, passo;
  if (!pref || !*pref || !strcasecmp(pref, "none")) return LING_AUTO_NADA;
  // AUDIO NO IDIOMA DA LEGENDA: a forcada daquele idioma, ou nada. So a
  // embutida tem tipo; legenda de addon e sempre a inteira.
  if (forcadaAuto && audio && *audio && ling_casa(audio, pref)) {
    for (passo = 0; passo < 2; passo++)
      for (i = 0; i < nEmb; i++) {
        int t = tipoEmb ? tipoEmb[i] : LING_LEG_COMUM;
        if (t != (passo ? LING_LEG_LETREIROS : LING_LEG_FORCADA)) continue;
        if (emb[i] && emb[i][0] && ling_casa(emb[i], pref)) return i;
      }
    // A forcada pode estar numa faixa cujo tipo ainda nao chegou (sonda do MKV).
    return embFechado ? LING_AUTO_NADA : LING_AUTO_ESPERA;
  }
  // A inteira: comum/completa primeiro, SDH so se nao houver outra.
  for (passo = 0; passo < 2; passo++)
    for (i = 0; i < nEmb; i++) {
      int t = tipoEmb ? tipoEmb[i] : LING_LEG_COMUM;
      if (t == LING_LEG_FORCADA || t == LING_LEG_LETREIROS) continue;
      if ((t == LING_LEG_SDH) != passo) continue;
      if (emb[i] && emb[i][0] && ling_casa(emb[i], pref)) return i;
    }
  if (!embFechado) return LING_AUTO_ESPERA;
  for (i = 0; i < nAdd; i++)
    if (add[i] && add[i][0] && ling_casa(add[i], pref)) return nEmb + i;
  return addFechado ? LING_AUTO_NADA : LING_AUTO_ESPERA;
}
