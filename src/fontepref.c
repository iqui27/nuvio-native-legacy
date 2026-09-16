// A fonte escolhida a mao, lembrada por titulo e por perfil. Ver a nota longa
// em fontepref.h — em especial a ordem de casamento (bingeGroup declarado pelo
// addon, depois provedor + trilha de audio), por que a chave nao pode ser a
// url, e por que a preferencia vence.
#include "fontepref.h"
#include "dados.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static FontePref tabela[FONTEPREF_MAX];
static int nTab;
static int carregado;
static int perfil;

// UM ARQUIVO POR PERFIL (fontepref-p<N>.txt), como fileirasui-p<N>.txt em
// fileiras.c e pelo mesmo motivo: sem a separacao, a escolha de audio de um
// perfil mandaria na reproducao do outro — que e o defeito que a separacao das
// fileiras existiu para consertar. p0 e "ninguem escolheu perfil ainda", que e
// o caso de quem nao tem conta.
//
// FONTEPREF_PERFIS e ate onde fontepref_esquecer varre. CONTA_PERFIL_MAX e 8
// hoje (perfis.h); 16 cobre o dobro sem custo — sao dados_apagar em arquivo
// que nao existe.
#define FONTEPREF_PERFIS 16

static const char *arquivoDoPerfil(void) {
  static char nome[48];
  snprintf(nome, sizeof nome, "fontepref-p%d.txt", perfil);
  return nome;
}

// --- assinatura de idioma/audio ---------------------------------------------
//
// BANDEIRA E DUAS LETRAS, e e por isso que ela e decodificada em vez de
// comparada como texto. 🇧🇷 sao dois "regional indicator": U+1F1E7 U+1F1F7, em
// UTF-8 F0 9F 87 A7 e F0 9F 87 B7. O ultimo byte menos 0xA6 da a letra ASCII.
// Decodificar transforma a bandeira num codigo comparavel ("BR") em vez de
// exigir uma tabela de 250 sequencias de bytes.
#define RI_BASE 0xA6

static int bandeira(const unsigned char *p, char *dst) {
  if (p[0] != 0xF0 || p[1] != 0x9F || p[2] != 0x87) return 0;
  if (p[3] < 0xA6 || p[3] > 0xBF) return 0;
  if (p[4] != 0xF0 || p[5] != 0x9F || p[6] != 0x87) return 0;
  if (p[7] < 0xA6 || p[7] > 0xBF) return 0;
  dst[0] = (char)('A' + (p[3] - RI_BASE));
  dst[1] = (char)('A' + (p[7] - RI_BASE));
  dst[2] = 0;
  return 1;
}

// TERMOS ESCRITOS. A lista e curta de proposito: cada entrada a mais e uma
// chance de duas fontes que eram iguais deixarem de casar por causa de uma
// palavra que so uma delas traz. Entram as marcas que os addons realmente
// escrevem no `name`/`description` e que uma pessoa notaria trocada.
//
// "dublado" vira DUB e nao POR: dublado diz que HA dublagem, nao em que
// idioma. "nacional", no jargao de lancamento em portugues, diz o idioma.
//
// TUDO SEM ACENTO de proposito: o texto da fonte passa por normalizar() antes,
// que rebaixa "Portugues"/"Português"/"PORTUGUÊS" a uma forma so. Sem isso a
// mesma fonte anunciada com e sem acento em dois episodios geraria duas
// assinaturas diferentes e o casamento falharia sem ninguem entender por que.
static const struct { const char *termo; const char *cod; } TERMOS[] = {
  { "dual audio",  "DUAL" }, { "dual-audio", "DUAL" }, { "dualaudio", "DUAL" },
  { "multi audio", "MULTI" }, { "multi-audio", "MULTI" }, { "multiaudio", "MULTI" },
  { "multi-subs",  "MULTI" },
  { "dublado",     "DUB"  }, { "dublagem",  "DUB" }, { "dubbed", "DUB" },
  { "nacional",    "POR"  },
  { "portuguese",  "POR"  }, { "portugues", "POR" },
  { "brazilian",   "POR"  }, { "brasileiro", "POR" },
  { "pt-br",       "POR"  }, { "ptbr", "POR" }, { "pt_br", "POR" },
  { "english",     "ENG"  }, { "ingles", "ENG" },
  { "spanish",     "SPA"  }, { "espanol", "SPA" },
  { "latino",      "SPA"  }, { "castellano", "SPA" },
  { "french",      "FRA"  }, { "frances", "FRA" }, { "francais", "FRA" },
  { "german",      "GER"  }, { "deutsch", "GER" }, { "alemao", "GER" },
  { "italian",     "ITA"  }, { "italiano", "ITA" },
  { "japanese",    "JPN"  }, { "japones", "JPN" },
  { "korean",      "KOR"  }, { "coreano", "KOR" },
  { "hindi",       "HIN"  }, { "tamil", "TAM" }, { "telugu", "TEL" },
  { "russian",     "RUS"  }, { "russo", "RUS" },
  { "legendado",   "LEG"  }, { "subbed", "LEG" }
};
#define N_TERMOS ((int)(sizeof TERMOS / sizeof *TERMOS))

// MINUSCULA E SEM ACENTO. Os addons escrevem a mesma coisa de tres jeitos
// ("Dual Audio", "DUAL AUDIO", "Dual Áudio") e o que a assinatura precisa e que
// os tres virem o mesmo texto. So a faixa latina do UTF-8 (0xC3 0x80..0xBF) e
// rebaixada: e onde moram os acentos do portugues, do espanhol e do frances. O
// resto dos bytes passa intacto — inclusive as bandeiras, que sao 0xF0 e nao
// entram nesta faixa.
static void normalizar(const char *src, char *dst, size_t tam) {
  static const char *BASE = "aaaaaaaceeeeiiiidnooooo*ouuuuy";  /* C0..DE */
  size_t k = 0;
  if (!tam) return;
  for (; src && *src && k + 1 < tam; src++) {
    unsigned char c = (unsigned char)*src;
    if (c == 0xC3 && (unsigned char)src[1] >= 0x80 && (unsigned char)src[1] <= 0xBE) {
      unsigned char d = (unsigned char)src[1];
      // 0x80..0x9E e a metade maiuscula, 0xA0..0xBE a minuscula: o mesmo
      // indice serve para as duas.
      unsigned idx = (d >= 0xA0 ? d - 0xA0 : d - 0x80);
      if (idx < 30) { dst[k++] = BASE[idx]; src++; continue; }
    }
    dst[k++] = (char)tolower(c);
  }
  dst[k] = 0;
}

// Token isolado, a mesma disciplina de stream_parse.c: "dub" nao pode casar
// dentro de "Dubai", e "leg" nao pode casar dentro de "legacy". A fronteira e
// alfanumerica ASCII.
static int tokenEm(const char *s, const char *t) {
  size_t n = strlen(t);
  const char *p;
  for (p = s; *p; p++)
    if ((p == s || !isalnum((unsigned char)p[-1])) &&
        !strncmp(p, t, n) && !isalnum((unsigned char)p[n])) return 1;
  return 0;
}

#define TRILHA_MARCAS 10

static int jaTem(char marcas[][6], int n, const char *cod) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(marcas[i], cod)) return 1;
  return 0;
}

void fontepref_trilha(const Stream *s, char *dst, unsigned tam) {
  char texto[6000];
  char marcas[TRILHA_MARCAS][6];
  int n = 0, i, j, len;
  unsigned k = 0;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!s) return;
  // ROTULO + DESCRICAO + ARQUIVO, nesta ordem, porque cada addon escreve a
  // lingua num lugar: o Torrentio manda as bandeiras no `title` (que vira
  // descricao), o AIOStreams escreve "Dual Audio" no `name`, e alguns grupos so
  // deixam a marca no nome do arquivo.
  { char bruto[6000];
    snprintf(bruto, sizeof bruto, "%s %s %s", s->rotulo, s->descricao, s->arquivo);
    normalizar(bruto, texto, sizeof texto); }
  len = (int)strlen(texto);

  for (i = 0; i < len && n < TRILHA_MARCAS; i++) {
    char cod[4];
    // 8 bytes a frente: bandeira() le o par inteiro. Sem esta guarda ela leria
    // depois do NUL na ultima bandeira da string. O comprimento e medido UMA
    // vez: strlen dentro do laco num texto de 6 KB e trabalho quadratico numa
    // funcao chamada por fonte da lista.
    if ((unsigned char)texto[i] == 0xF0 && i + 8 <= len &&
        bandeira((const unsigned char *)texto + i, cod)) {
      if (!jaTem(marcas, n, cod)) snprintf(marcas[n++], 6, "%s", cod);
      i += 7;
    }
  }
  for (j = 0; j < N_TERMOS && n < TRILHA_MARCAS; j++)
    if (tokenEm(texto, TERMOS[j].termo) && !jaTem(marcas, n, TERMOS[j].cod))
      snprintf(marcas[n++], 6, "%s", TERMOS[j].cod);

  // ORDENADA: a assinatura tem de ser a mesma quando as marcas chegam em ordem
  // diferente, e chegam — o mesmo grupo escreve "🇧🇷 / 🇺🇸" num episodio e
  // "🇺🇸 / 🇧🇷" no seguinte. Insercao, porque sao no maximo dez.
  for (i = 1; i < n; i++) {
    char tmp[6];
    snprintf(tmp, sizeof tmp, "%s", marcas[i]);
    for (j = i; j > 0 && strcmp(marcas[j - 1], tmp) > 0; j--)
      snprintf(marcas[j], 6, "%s", marcas[j - 1]);
    snprintf(marcas[j], 6, "%s", tmp);
  }
  for (i = 0; i < n && k + 1 < tam; i++)
    k += (unsigned)snprintf(dst + k, tam - k, "%s%s", i ? "+" : "", marcas[i]);
}

// --- tabela -----------------------------------------------------------------

void fontepref_id_base(const char *id, char *dst, unsigned tam) {
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!id) return;
  snprintf(dst, tam, "%.*s", (int)strcspn(id, ":"), id);
}

static int achar(const char *base) {
  int i;
  if (!base || !base[0]) return -1;
  for (i = 0; i < nTab; i++) if (!strcmp(tabela[i].id, base)) return i;
  return -1;
}

// Tira TAB e controle. O rotulo do Torrentio e "Torrentio\n1080p" — com a
// quebra de linha dentro, o arquivo de uma linha por titulo viraria duas e a
// leitura seguinte perderia metade da tabela em silencio.
static void limpar(char *s) {
  for (; *s; s++) if ((unsigned char)*s < 32) *s = ' ';
}

static char *campo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else { *p = NULL; }
  return ini;
}

// O bingeGroup ENTROU NO FIM DA LINHA, depois do rotulo, e nao no meio.
//
// O arquivo da 1.0.55 tem seis campos. Enfiar o setimo antes do rotulo faria a
// leitura do arquivo ja existente pousar o ROTULO dentro do bingeGroup —
// silenciosamente, porque campo que falta vira vazio e a linha continua
// valida. No fim, o arquivo velho le igualzinho e so nao tem bingeGroup, que e
// a verdade: a 1.0.55 nao sabia o que e isso.
//
// O rotulo deixa de ser "o campo que pode conter qualquer coisa" e passa a ser
// separado por TAB como os outros. Pode: limpar() ja troca TODO byte abaixo de
// 32 por espaco antes de gravar, e o TAB e um deles. O ultimo campo agora e o
// bingeGroup, que passa pelo mesmo limpar().
static void gravar(void) {
  size_t cap = (size_t)FONTEPREF_MAX * 560u + 64u;
  char *buf = (char *)malloc(cap);
  size_t k = 0;
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio fontes v1\n");
  for (i = 0; i < nTab && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k, "%s\t%lld\t%d\t%s\t%s\t%s\t%s\n",
                          tabela[i].id, tabela[i].quandoS, tabela[i].altura,
                          tabela[i].trilha, tabela[i].provedor, tabela[i].rotulo,
                          tabela[i].bingeGroup);
  dados_gravar(arquivoDoPerfil(), buf);
  free(buf);
}

void fontepref_iniciar(void) {
  char *b, *linha, *prox;
  if (carregado) return;
  carregado = 1;
  nTab = 0;
  b = dados_ler(arquivoDoPerfil());
  if (!b) return;
  for (linha = b; linha && *linha && nTab < FONTEPREF_MAX; linha = prox) {
    char *p, *id, *quando, *altura, *trilha, *provedor, *rotulo, *binge;
    char *fim = strchr(linha, '\n');
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    p = linha;
    id       = campo(&p);
    quando   = campo(&p);
    altura   = campo(&p);
    trilha   = campo(&p);
    provedor = campo(&p);
    rotulo   = campo(&p);
    // O bingeGroup E O ULTIMO campo, como o titulo em salvos.txt: sendo o
    // ultimo nao precisa de separador depois dele. Campo que faltar vira vazio
    // e a linha continua valida — que e exatamente o que acontece ao ler o
    // arquivo de seis campos gravado pela 1.0.55, e o certo: aquela versao nao
    // lia bingeGroup nenhum.
    binge = p ? p : (char *)"";
    if (strncmp(id, "tt", 2)) continue;
    if (!provedor[0]) continue;      // sem provedor a linha nao casa com nada
    { FontePref *f = &tabela[nTab++];
      memset(f, 0, sizeof *f);
      snprintf(f->id, sizeof f->id, "%s", id);
      snprintf(f->trilha, sizeof f->trilha, "%s", trilha);
      snprintf(f->provedor, sizeof f->provedor, "%s", provedor);
      snprintf(f->rotulo, sizeof f->rotulo, "%s", rotulo);
      snprintf(f->bingeGroup, sizeof f->bingeGroup, "%s", binge);
      f->quandoS = atoll(quando);
      f->altura = atoi(altura); }
  }
  free(b);
  printf("[fonte] %d preferencia(s) de fonte no perfil %d\n", nTab, perfil);
  fflush(stdout);
}

void fontepref_definir_perfil(int p) {
  if (p < 0) p = 0;
  if (p == perfil && carregado) return;
  perfil = p;
  nTab = 0;
  carregado = 0;
}

void fontepref_esquecer(void) {
  int p;
  // APAGA O ARQUIVO DE TODOS OS PERFIS, e nao so o do perfil corrente. Ao
  // sair da conta os perfis somem junto (perfis_esquecer), entao um arquivo
  // de perfil 3 que ficasse para tras seria lido pela proxima pessoa que
  // acabasse no perfil 3 da conta dela.
  for (p = 0; p <= FONTEPREF_PERFIS; p++) {
    char nome[48];
    snprintf(nome, sizeof nome, "fontepref-p%d.txt", p);
    dados_apagar(nome);
  }
  nTab = 0;
  carregado = 1;   // ja sabemos o que ha no disco: nada
}

// VENCEU? Ver FONTEPREF_VALIDADE_S em fontepref.h para o prazo e o porque.
// Aqui ficam as duas bordas, que sao as que mordem numa TV:
//
//   quandoS <= 0  vale para sempre. Linha sem carimbo e linha que veio de um
//                 arquivo que nao tinha a coluna, ou de um atoll que falhou.
//                 Tratar ausencia de data como "velhissima" apagaria a escolha
//                 de todo mundo no primeiro arranque da versao seguinte — o
//                 lado errado para errar.
//   agora < quandoS  (preferencia "do futuro") TAMBEM vale. O app web recusa
//                 esse caso; aqui nao, de proposito: esta TV arranca com o
//                 relogio errado e so acerta quando a rede sobe, entao "do
//                 futuro" e o estado NORMAL dos primeiros segundos de uso. Uma
//                 preferencia honrada a mais custa uma fonte; expirar a tabela
//                 inteira a cada tomada tirada da parede custa o recurso.
static int venceu(const FontePref *f) {
  long long agora = (long long)time(NULL);
  if (!f || f->quandoS <= 0) return 0;
  if (agora <= f->quandoS) return 0;
  return agora - f->quandoS > FONTEPREF_VALIDADE_S;
}

const FontePref *fontepref_do_titulo(const char *id) {
  char base[24];
  int k;
  if (!carregado) fontepref_iniciar();
  fontepref_id_base(id, base, sizeof base);
  k = achar(base);
  if (k < 0) return NULL;
  if (venceu(&tabela[k])) {
    // Nao apaga e nao grava: ver a nota em fontepref.h. A linha morre sozinha
    // na proxima escolha (reescrita) ou quando a tabela encher (a mais antiga
    // sai primeiro, e vencida e velha por definicao).
    printf("[fonte] preferencia de %s venceu (%lld dias); perguntando de novo\n",
           tabela[k].id,
           ((long long)time(NULL) - tabela[k].quandoS) / (24LL * 3600LL));
    fflush(stdout);
    return NULL;
  }
  return &tabela[k];
}

int fontepref_tem(const char *id) { return fontepref_do_titulo(id) != NULL; }

int fontepref_guardar(const char *id, const Stream *s) {
  char base[24];
  int k;
  FontePref novo;
  if (!s || !s->provedor[0]) return 0;
  // Carregar sob demanda, pela mesma razao de salvos_definir: gravar por cima
  // de uma tabela nao lida APAGARIA o arquivo inteiro.
  if (!carregado) fontepref_iniciar();
  fontepref_id_base(id, base, sizeof base);
  if (!base[0]) return 0;

  memset(&novo, 0, sizeof novo);
  snprintf(novo.id, sizeof novo.id, "%s", base);
  snprintf(novo.provedor, sizeof novo.provedor, "%s", s->provedor);
  snprintf(novo.rotulo, sizeof novo.rotulo, "%s", s->rotulo);
  snprintf(novo.bingeGroup, sizeof novo.bingeGroup, "%s", s->bingeGroup);
  fontepref_trilha(s, novo.trilha, sizeof novo.trilha);
  novo.altura = s->altura;
  novo.quandoS = (long long)time(NULL);
  limpar(novo.provedor);
  limpar(novo.rotulo);
  limpar(novo.trilha);
  limpar(novo.bingeGroup);

  k = achar(base);
  if (k < 0) {
    if (nTab >= FONTEPREF_MAX) {
      // Cheia: sai a mais antiga. Nunca falhar em guardar — a alternativa e
      // uma tabela que para de aprender no titulo 200 e ninguem descobre.
      int i, velho = 0;
      for (i = 1; i < nTab; i++)
        if (tabela[i].quandoS < tabela[velho].quandoS) velho = i;
      k = velho;
    } else {
      k = nTab++;
    }
  } else if (!strcmp(tabela[k].provedor, novo.provedor) &&
             !strcmp(tabela[k].trilha, novo.trilha) &&
             !strcmp(tabela[k].bingeGroup, novo.bingeGroup) &&
             !strcmp(tabela[k].rotulo, novo.rotulo) &&
             tabela[k].altura == novo.altura &&
             !venceu(&tabela[k])) {
    // Mesma escolha de novo: nao reescrever o arquivo por um carimbo de tempo.
    // MENOS quando ela venceu — ai o carimbo E a novidade, e sem reescrever a
    // pessoa reescolheria a mesma fonte a cada abertura ate o fim dos tempos.
    return 0;
  }
  tabela[k] = novo;
  gravar();
  printf("[fonte] preferencia de %s: %s / %s / binge=%s\n", base, novo.provedor,
         novo.trilha[0] ? novo.trilha : "(sem marca de idioma)",
         novo.bingeGroup[0] ? novo.bingeGroup : "(o addon nao declarou)");
  fflush(stdout);
  return 1;
}

// DESEMPATE, o mesmo das duas camadas. Varias fontes podem dividir o mesmo
// bingeGroup (o addon agrupa por servico/qualidade, nao por arquivo), e varias
// podem dividir provedor + trilha. Resolucao igual vale 2, rotulo igual vale 4
// — o rotulo e o sinal mais forte porque e o texto que a pessoa leu na folha
// quando escolheu.
static int nota(const Stream *s, const FontePref *p) {
  int n = 0;
  if (s->altura == p->altura) n += 2;
  if (!strcmp(s->rotulo, p->rotulo)) n += 4;
  return n;
}

// CAMADA 1: o bingeGroup que o addon declarou. Sem provedor na condicao, como
// no app web (um `find` por bingeGroup e mais nada): o campo JA E a declaracao
// de identidade de fonte, e exigir provedor por cima so criaria um jeito novo
// de nao casar.
static int porBinge(const FontePref *p) {
  int i, melhor = -1, melhorNota = -1, total = stream_n();
  if (!p->bingeGroup[0]) return -1;
  for (i = 0; i < total; i++) {
    const Stream *s = stream_item(i);
    int n;
    if (!s || !s->bingeGroup[0]) continue;
    if (strcmp(s->bingeGroup, p->bingeGroup)) continue;
    n = nota(s, p);
    // `>` e nao `>=`, a mesma disciplina de stream_automatico: em empate fica
    // o PRIMEIRO da lista, que e a ordem em que o addon devolveu.
    if (n > melhorNota) { melhorNota = n; melhor = i; }
  }
  return melhor;
}

// CAMADA 2: a heuristica da 1.0.55, intacta. Ela atende os addons que nao
// mandam bingeGroup — que sao muitos — e o episodio em que o addon MUDOU o
// rotulo de agrupamento.
static int porTrilha(const FontePref *p) {
  int i, melhor = -1, melhorNota = -1, total = stream_n();
  for (i = 0; i < total; i++) {
    const Stream *s = stream_item(i);
    char t[FONTEPREF_TRILHA];
    int n;
    if (!s || !s->provedor[0]) continue;
    if (strcasecmp(s->provedor, p->provedor)) continue;
    fontepref_trilha(s, t, sizeof t);
    // TRILHA IDENTICA OU NADA. Ver a nota de fontepref.h: "mesmo provedor, outro
    // audio" e a queixa do issue, nao a solucao dele.
    if (strcmp(t, p->trilha)) continue;
    n = nota(s, p);
    if (n > melhorNota) { melhorNota = n; melhor = i; }
  }
  return melhor;
}

int fontepref_escolher(const char *id) {
  const FontePref *p = fontepref_do_titulo(id);
  int melhor;
  if (!p) return -1;

  melhor = porBinge(p);
  if (melhor >= 0) {
    printf("[fonte] preferida de %s na posicao %d por bingeGroup (%s)\n",
           p->id, melhor, p->bingeGroup);
    fflush(stdout);
    return melhor;
  }
  // BINGEGROUP GUARDADO QUE NAO CASOU NAO ENCERRA A BUSCA. O app web tem um
  // modo (`bingeGroupOnly`) em que a falha abre a folha; aqui nao ha esse
  // modo, e cair na heuristica e o certo: o addon que troca de servico de
  // debrid troca o bingeGroup inteiro sem trocar de fonte nenhuma.
  melhor = porTrilha(p);

  if (melhor >= 0)
    printf("[fonte] preferida de %s na posicao %d (%s)\n", p->id, melhor,
           p->trilha[0] ? p->trilha : "sem marca de idioma");
  else
    printf("[fonte] preferida de %s (%s / %s) nao esta na lista de hoje, indo de automatico\n",
           p->id, p->provedor, p->trilha[0] ? p->trilha : "sem marca");
  fflush(stdout);
  return melhor;
}
