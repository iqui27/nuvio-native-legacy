#include "addons.h"
#include "idioma.h"
#include "linguas.h"
#include "streams.h"
#include "debrid.h"
#include "rede.h"
#include "js.h"
#include "marco.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <stdatomic.h>

#define ADD_MAX 12

// `fonte` marca quem realmente entrega stream. Descoberto pelo manifesto: o
// Xperience declara resources catalog/meta/subtitles e NENHUM stream, entao
// respondia {"streams":[]} para tudo. Consultar quem nao fornece e um
// round-trip jogado fora em CADA abertura de titulo.
// `ativo` existe porque a lista precisa MOSTRAR o que esta desligado. Antes, um
// addon desligado na conta era descartado na leitura, entao nao havia como
// ve-lo nem religa-lo pela TV — so pelo celular.
// `sondado` diz se as capacidades vieram do MANIFESTO ou sao a suposicao
// inicial. A diferenca importa na tela: "ainda nao sei" e diferente de "nao
// fornece".
static struct {
  char nome[64]; char base[600];
  int fonte, catalogo, legenda;
  int ativo, sondado;
  char id[96];   // "id" do manifesto; as colecoes da conta apontam para ele
} addon[ADD_MAX];
static int nAddon;
static _Atomic AddEstado estado = ADD_PARADO;
static pthread_t fio;
static char alvoId[64], alvoTipo[16];
static int fioVivo;
static Stream *resultado;
static int nResultado;
static char pendId[64], pendTipo[16];

// --- leitura do arquivo de configuracao -------------------------------------

int addons_carregar(const char *dirArte) {
  char caminho[600], linha[900];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/addons.txt", dirArte ? dirArte : ".");
  f = fopen(caminho, "r");
  if (!f) { printf("[addons] sem %s\n", caminho); return 0; }
  nAddon = 0;
  while (nAddon < ADD_MAX && fgets(linha, sizeof linha, f)) {
    char *tab = strchr(linha, '\t');
    char *fim;
    size_t n;
    // TAB e nao "|" como separador: nome de addon contem "|" de verdade
    // ("AIOStreams | ElfHosted") e partir no primeiro pipe corrompia a URL.
    if (!tab) continue;
    *tab = 0;
    fim = tab + 1 + strlen(tab + 1);
    while (fim > tab + 1 && (fim[-1] == '\n' || fim[-1] == '\r' || fim[-1] == ' ')) *--fim = 0;
    if (linha[0] == '#' || !tab[1]) continue;
    // Terceira coluna (opcional): 1 = fornece stream. Ausente vale 1, para
    // arquivo antigo continuar funcionando.
    addon[nAddon].fonte = 1;
    addon[nAddon].catalogo = 1;
    addon[nAddon].legenda = 0;
    { char *tab2 = strchr(tab + 1, '\t');
      if (tab2) {
        char *tab3;
        *tab2 = 0;
        tab3 = strchr(tab2 + 1, '\t');
        if (tab3) {
          char *tab4 = strchr(tab3 + 1, '\t');
          *tab3 = 0;
          if (tab4) { *tab4 = 0; addon[nAddon].legenda = atoi(tab4 + 1); }
          addon[nAddon].catalogo = atoi(tab3 + 1);
        }
        addon[nAddon].fonte = atoi(tab2 + 1);
      } }
    snprintf(addon[nAddon].nome, sizeof addon[nAddon].nome, "%s", linha);
    snprintf(addon[nAddon].base, sizeof addon[nAddon].base, "%s", tab + 1);
    // A URL guardada aponta para o manifesto; a base e ela sem esse sufixo.
    n = strlen(addon[nAddon].base);
    if (n > 14 && !strcmp(addon[nAddon].base + n - 14, "/manifest.json"))
      addon[nAddon].base[n - 14] = 0;
    else while (n && addon[nAddon].base[n - 1] == '/') addon[nAddon].base[--n] = 0;
    nAddon++;
  }
  fclose(f);
  { int f = 0, k;
    for (k = 0; k < nAddon; k++) f += addon[k].fonte;
    printf("[addons] %d configurados, %d fornecem stream\n", nAddon, f); }
  return nAddon;
}

// A base normalizada de uma entrada da conta, do mesmo jeito que o laco abaixo
// normaliza. Existe para poder COMPARAR sem duplicar a regra.
static void baseNormalizada(const char *url, char *dst, size_t tam) {
  size_t k;
  snprintf(dst, tam, "%s", url);
  k = strlen(dst);
  if (k > 14 && !strcmp(dst + k - 14, "/manifest.json")) dst[k - 14] = 0;
  else while (k && dst[k - 1] == '/') dst[--k] = 0;
}

// A lista que chegou e IGUAL a que ja esta valendo?
//
// Sem esta pergunta, `remontar` era ligado a cada ciclo de sync so porque a
// resposta chegou — e a resposta chega a cada cinco minutos, identica. O
// resultado era um ciclo de descoberta completo (Trakt, todos os manifestos,
// todos os catalogos) a cada cinco minutos, para sempre, e a home reassentando
// junto. Tambem e ele que republica o catalogo por baixo de quem esta parado
// numa pagina de titulo.
static int listaIgual(const AddonRemoto *nova, int n) {
  int i, k = 0;
  for (i = 0; i < n && k < ADD_MAX; i++) {
    char base[600];
    if (!nova[i].url[0]) continue;
    baseNormalizada(nova[i].url, base, sizeof base);
    if (k >= nAddon) return 0;
    if (strcmp(addon[k].base, base)) return 0;
    if (addon[k].ativo != (nova[i].ativo ? 1 : 0)) return 0;
    k++;
  }
  return k == nAddon;
}

int addons_definir_lista(const AddonRemoto *nova, int n) {
  int i, aceitos = 0;
  if (!nova || n <= 0) {
    // Vazio nao substitui. Ver o comentario no cabecalho: uma resposta vazia
    // nao se distingue de uma delecao, e a diferenca entre as duas e a pessoa
    // ficar ou nao sem nenhuma fonte.
    printf("[addons] lista da conta veio vazia; mantendo a local (%d)\n", nAddon);
    return 0;
  }
  // NADA MUDOU: nao substitui e diz que nao mudou. Substituir seria pior do que
  // inutil — o laco abaixo zera `sondado` e o `id` do manifesto, entao reaplicar
  // uma lista identica jogaria fora o que a sonda aprendeu e faria as colecoes
  // da conta perderem a URL dos addons ate a proxima leitura.
  if (listaIgual(nova, n)) return 0;
  for (i = 0; i < n && aceitos < ADD_MAX; i++) {
    size_t k;
    // Addon DESLIGADO tambem entra: ele aparece na lista e pode ser religado
    // aqui. So nao e consultado (ver ativoParaConsulta).
    if (!nova[i].url[0]) continue;
    // ZERAR A ENTRADA INTEIRA, e nao so os campos que a conta traz.
    //
    // `id` (o do manifesto) so e preenchido pela sonda, e este laco nunca o
    // tocava: numa segunda chamada — e ela acontece a cada ciclo de sync — o
    // slot herdava o id do addon que estava ANTES naquela posicao, agora com
    // uma base diferente. addons_base_por_id passava a devolver a base ERRADA
    // para aquele id, e quem consulta esse mapa sao as fontes das colecoes da
    // conta (colecoes.c): a pasta abria o catalogo de outro addon, ou nenhum.
    memset(&addon[aceitos], 0, sizeof addon[aceitos]);
    snprintf(addon[aceitos].nome, sizeof addon[aceitos].nome, "%s",
             nova[i].nome[0] ? nova[i].nome : "Addon");
    snprintf(addon[aceitos].base, sizeof addon[aceitos].base, "%s", nova[i].url);
    k = strlen(addon[aceitos].base);
    if (k > 14 && !strcmp(addon[aceitos].base + k - 14, "/manifest.json"))
      addon[aceitos].base[k - 14] = 0;
    else while (k && addon[aceitos].base[k - 1] == '/') addon[aceitos].base[--k] = 0;
    // A conta nao diz o que cada addon fornece; o manifesto e que diria, e
    // consultar todos no arranque custaria uma viagem por addon. Assumir que
    // fornece tudo faz no maximo uma consulta vazia a mais por titulo — o
    // contrario (assumir que nao fornece) esconderia fontes de verdade.
    // Ate o manifesto responder, assume-se que fornece tudo — inclusive
    // LEGENDA, que antes ficava em 0 e contradizia o comentario acima. O
    // efeito de legenda=0 era pior do que uma consulta a mais: buscarLegendas
    // pula quem nao declara legenda, entao numa conta sincronizada o
    // OpenSubtitles nunca era consultado e nao havia legenda nenhuma.
    addon[aceitos].fonte = 1;
    addon[aceitos].catalogo = 1;
    addon[aceitos].legenda = 1;
    addon[aceitos].sondado = 0;
    addon[aceitos].ativo = nova[i].ativo ? 1 : 0;
    aceitos++;
  }
  if (aceitos == 0) {
    printf("[addons] a conta veio sem addons utilizaveis; mantendo a local\n");
    return 0;
  }
  nAddon = aceitos;
  printf("[addons] %d vindos da conta\n", nAddon);
  return 1;
}

int addons_exportar(AddonRemoto *saida, int max) {
  int i, k = 0;
  for (i = 0; i < nAddon && k < max; i++) {
    snprintf(saida[k].nome, sizeof saida[k].nome, "%s", addon[i].nome);
    snprintf(saida[k].url, sizeof saida[k].url, "%s", addon[i].base);
    saida[k].ativo = addon[i].ativo;
    k++;
  }
  return k;
}

void addons_esquecer(void) {
  memset(addon, 0, sizeof addon);
  nAddon = 0;
  printf("[addons] lista esquecida (saiu da conta)\n");
}

int addons_n(void) { return nAddon; }

const char *addons_base(int i) {
  return (i >= 0 && i < nAddon) ? addon[i].base : "";
}

// Quarta coluna de addons.txt. Como a de stream, ausente vale 1 — arquivo
// antigo continua funcionando, so faz uma consulta a mais que pode dar vazio.
int addons_tem_catalogo(int i) {
  return (i >= 0 && i < nAddon) ? (addon[i].ativo && addon[i].catalogo) : 0;
}
AddEstado addons_estado(void) {
  AddEstado e = atomic_load(&estado);
  // Publica no fio da UI: nenhum desenho observa uma lista parcialmente escrita.
  if (fioVivo && e != ADD_BUSCANDO) {
    pthread_join(fio, NULL);
    fioVivo = 0;
    if (!pendId[0]) stream_definir_lista(resultado, nResultado);
    free(resultado); resultado = NULL; nResultado = 0;
    if (pendId[0]) {
      char id[64], tipo[16];
      snprintf(id, sizeof id, "%s", pendId);
      snprintf(tipo, sizeof tipo, "%s", pendTipo);
      pendId[0] = 0;
      addons_buscar(id, tipo);
      return ADD_BUSCANDO;
    }
  }
  return e;
}

// --- leitura tolerante de JSON ----------------------------------------------
// Um analisador completo nao se paga aqui: o formato e conhecido e raso, e o
// que importa e nunca travar com campo faltando. Cada funcao devolve o que
// achou ou nada, e quem chama decide.

static const char *pulaEspaco(const char *p) {
  while (*p && (unsigned char)*p <= ' ') p++;
  return p;
}

// Copia o valor textual de "chave" dentro do objeto que comeca em `obj`,
// respeitando escapes. Devolve 1 se achou.
static int campoTexto(const char *obj, const char *fimObj, const char *chave,
                      char *dst, size_t tam) {
  char busca[48];
  const char *p;
  size_t k = 0;
  snprintf(busca, sizeof busca, "\"%s\"", chave);
  p = strstr(obj, busca);
  if (!p || p >= fimObj) return 0;
  p = pulaEspaco(p + strlen(busca));
  if (*p != ':') return 0;
  p = pulaEspaco(p + 1);
  if (*p != '"') return 0;
  p++;
  while (*p && *p != '"' && k + 1 < tam) {
    if (*p == '\\' && p[1]) {
      p++;
      // \u..... vira "?" de proposito: os nomes vem cheios de emoji e o texto
      // e so para exibicao. Decodificar UTF-16 aqui seria trabalho sem retorno.
      if (*p == 'u') { p += 5; dst[k++] = ' '; continue; }
      if (*p == 'n' || *p == 't' || *p == 'r') { p++; dst[k++] = ' '; continue; }
    }
    dst[k++] = *p++;
  }
  dst[k] = 0;
  return k > 0;
}

// Acha o fim do objeto JSON que comeca em `p` (que aponta para '{').
static const char *fimObjeto(const char *p) {
  int prof = 0, texto = 0;
  for (; *p; p++) {
    if (texto) { if (*p == '\\') p++; else if (*p == '"') texto = 0; continue; }
    if (*p == '"') texto = 1;
    else if (*p == '{') prof++;
    else if (*p == '}' && --prof == 0) return p + 1;
  }
  return p;
}

// --- legendas ---------------------------------------------------------------

static Legenda legs[LEG_MAX];
static int nLegs;
static pthread_t fioLeg;
static int fioLegVivo, fioLegCriado, legParar;
static char legId[64], legTipo[16];
static unsigned legGeracao;
static pthread_mutex_t legTrava = PTHREAD_MUTEX_INITIALIZER;

int addons_n_legendas(void) {
  int n;
  pthread_mutex_lock(&legTrava); n = nLegs; pthread_mutex_unlock(&legTrava);
  return n;
}
const Legenda *addons_legenda(int i) {
  const Legenda *r = NULL;
  pthread_mutex_lock(&legTrava);
  if (i >= 0 && i < nLegs) r = &legs[i];
  pthread_mutex_unlock(&legTrava);
  return r;
}

// Grupos de idioma da busca de legenda, NA ORDEM em que aparecem.
//
// O QUE ESTAVA AQUI: duas listas cravadas ("pob","pt-br",... e "eng","en",...)
// com o comentario "o usuario pediu explicitamente estes dois grupos". Toda
// legenda de outro idioma era descartada sem aviso — quem instala o pacote e
// fala espanhol abria o player e nao achava legenda nenhuma.
//
// AGORA: os grupos vem da preferencia (Ajustes desta TV, senao a conta). SEM
// preferencia nenhuma, ha UM grupo vazio, e grupo vazio casa com tudo: a lista
// sai sem filtro. Ver linguas.h.
static int gruposIdioma(const char *g[2]) {
  const char *a = ling_legenda(), *b = ling_legenda2();
  int n = 0;
  if (a[0] && strcasecmp(a, "none")) g[n++] = a;
  if (b[0] && strcasecmp(b, "none") && !ling_casa(b, a)) g[n++] = b;
  if (!n) { g[n++] = ""; }
  return n;
}


static int pedidoMudou(unsigned geracao) {
  int mudou;
  pthread_mutex_lock(&legTrava);
  mudou = legParar || geracao != legGeracao;
  pthread_mutex_unlock(&legTrava);
  return mudou;
}

static void episodioPedido(const char *id, int *temporada, int *episodio) {
  const char *p = strchr(id, ':');
  *temporada = *episodio = 0;
  if (p) sscanf(p + 1, "%d:%d", temporada, episodio);
}

static int episodioCorreto(const char *obj, const char *fim, int temporada, int episodio) {
  int t, e;
  if (temporada <= 0 || episodio <= 0) return 1;
  t = (int)js_num(obj, fim, "season", -1);
  e = (int)js_num(obj, fim, "episode", -1);
  // Alguns addons antigos nao devolvem os campos. Quando devolvem, eles sao
  // uma garantia: nunca mostre T2E3 numa busca por T2E4.
  if ((t >= 0 && t != temporada) || (e >= 0 && e != episodio)) return 0;
  if (t >= 0 || e >= 0) return 1;
  // Alguns addons omitem season/episode mas devolvem o episodio no nome do
  // arquivo. Antes aceitavamos S02E03 numa busca por T2E4 e depois fabricavamos
  // o rotulo T2E4 com base no pedido, escondendo o erro. Se o nome traz uma
  // identidade verificavel, ela precisa casar; nome sem marcador segue aceito.
  { char nome[160] = "", baixo[160]; size_t i;
    if (!js_texto(obj, fim, "subtitleFileName", nome, sizeof nome))
      js_texto(obj, fim, "movieReleaseName", nome, sizeof nome);
    for (i = 0; nome[i] && i + 1 < sizeof baixo; i++)
      baixo[i] = (char)tolower((unsigned char)nome[i]);
    baixo[i] = 0;
    for (i = 0; baixo[i]; i++) {
      int nt = -1, ne = -1;
      if (sscanf(baixo + i, "s%2de%2d", &nt, &ne) == 2 ||
          sscanf(baixo + i, "%2dx%2d", &nt, &ne) == 2)
        return nt == temporada && ne == episodio;
    }
  }
  return 1;
}

static void *buscarLegendas(void *u) {
  (void)u;
  for (;;) {
    Legenda achadas[LEG_MAX] = {{0}};
    char id[64], tipo[16];
    unsigned geracao;
    int nAchadas = 0, temporada, episodio, i;

    pthread_mutex_lock(&legTrava);
    if (legParar) { fioLegVivo = 0; pthread_mutex_unlock(&legTrava); return NULL; }
    snprintf(id, sizeof id, "%s", legId);
    snprintf(tipo, sizeof tipo, "%s", legTipo);
    geracao = legGeracao;
    pthread_mutex_unlock(&legTrava);
    episodioPedido(id, &temporada, &episodio);

    for (i = 0; i < nAddon && nAchadas < LEG_MAX; i++) {
      char url[900], *corpo;
      const char *p;
    // Addon que nao declara legenda nao e consultado: o AIOStreams responderia
    // vazio e o Xperience tambem, dois round-trips sem retorno.
      if (!addon[i].ativo || !addon[i].legenda) continue;
      snprintf(url, sizeof url, "%s/subtitles/%s/%s.json",
               addon[i].base, tipo, id);
      corpo = rede_baixar(url, 25);
      if (pedidoMudou(geracao)) { free(corpo); break; }
      if (!corpo) continue;
      p = js_array(corpo, NULL, "subtitles");
      {
        const char *grupos[2];
        int nGrupos = gruposIdioma(grupos), gi;
        // Uma passada por grupo garante a ordem preferido -> alternativo e
        // evita que doze resultados do primeiro idioma consumam a lista inteira
        // antes do segundo. O teto por grupo e deliberado para navegacao por
        // D-pad — e vira a lista toda quando ha um grupo so.
        for (gi = 0; gi < nGrupos; gi++) {
          const char *grupo = grupos[gi];
          int teto = nGrupos > 1 ? LEG_MAX / 2 : LEG_MAX;
          const char *q = p;
          int noGrupo = 0, j;
          for (j = 0; j < nAchadas; j++)
            if (ling_casa(achadas[j].idioma, grupo)) noGrupo++;
          while (q && nAchadas < LEG_MAX && noGrupo < teto) {
            const char *f = js_fim(q);
            char l[16] = "", nome[120] = "";
            Legenda *d = &achadas[nAchadas];
            if (episodioCorreto(q, f, temporada, episodio) &&
                js_texto(q, f, "lang", l, sizeof l) && ling_casa(l, grupo) &&
                js_texto(q, f, "url", d->url, sizeof d->url)) {
              js_texto(q, f, "subtitleFileName", nome, sizeof nome);
              if (!nome[0]) js_texto(q, f, "movieReleaseName", nome, sizeof nome);
              snprintf(d->idioma, sizeof d->idioma, "%s", l);
              if (temporada > 0 && episodio > 0)
                snprintf(d->rotulo, sizeof d->rotulo, i18n("T%dE%d  \xc2\xb7  %s%s%.22s"),
                         temporada, episodio, ling_nome(l), nome[0] ? "  \xc2\xb7  " : "", nome);
              else
                snprintf(d->rotulo, sizeof d->rotulo, "%s%s%.36s",
                         ling_nome(l), nome[0] ? "  \xc2\xb7  " : "", nome);
              nAchadas++; noGrupo++;
            }
            q = js_prox(f);
          }
        }
      }
      free(corpo);
    }

    pthread_mutex_lock(&legTrava);
    if (legParar) { fioLegVivo = 0; pthread_mutex_unlock(&legTrava); return NULL; }
    if (geracao != legGeracao) { pthread_mutex_unlock(&legTrava); continue; }
    memcpy(legs, achadas, sizeof achadas);
    nLegs = nAchadas;
    fioLegVivo = 0;
    pthread_mutex_unlock(&legTrava);
    printf("[legendas] %s: %d\n", id, nAchadas);
    fflush(stdout);
    return NULL;
  }
}


// ------------------------------------------------------------ lista e sonda

int addons_ativo(int i)   { return (i >= 0 && i < nAddon) ? addon[i].ativo : 0; }
int addons_sondado(int i) { return (i >= 0 && i < nAddon) ? addon[i].sondado : 0; }
const char *addons_nome(int i) {
  return (i >= 0 && i < nAddon) ? addon[i].nome : "";
}
int addons_fornece(int i, int oque) {
  if (i < 0 || i >= nAddon) return 0;
  if (oque == ADD_CATALOGO) return addon[i].catalogo;
  if (oque == ADD_STREAM)   return addon[i].fonte;
  return addon[i].legenda;
}
int addons_alternar(int i) {
  if (i < 0 || i >= nAddon) return 0;
  addon[i].ativo = !addon[i].ativo;
  printf("[addons] %s: %s\n", addon[i].nome, addon[i].ativo ? "ligado" : "desligado");
  fflush(stdout);
  return addon[i].ativo;
}

// SONDA DO MANIFESTO. Ate ela responder, o app assume que todo addon fornece
// tudo — e essa suposicao custa no maximo uma consulta vazia. O manifesto diz a
// verdade, e e o que a tela mostra: sem isso a lista so poderia repetir a
// suposicao, que e o mesmo que nao informar nada.
//
// Roda em fio proprio e UMA vez por lista: sao N viagens, e faze-las no
// arranque atrasaria a primeira tela por addon configurado.
static pthread_t fioSonda;
static int sondaViva;

static void capacidadesDoManifesto(int i, const char *corpo) {
  const char *r = strstr(corpo, "\"resources\"");
  int cat = 0, str = 0, leg = 0;
  // O ID E O NOME VEM PRIMEIRO, ANTES DE QUALQUER RETORNO CEDO.
  //
  // Estavam no fim da funcao, depois de tres `return` que dependem de
  // "resources" — um campo que o protocolo pede mas que addon real as vezes
  // omite ou escreve de forma que este leitor nao alcanca. Nesse caso o addon
  // ficava PARA SEMPRE sem id, addons_base_por_id devolvia "" e as pastas de
  // colecao da conta abriam sem nenhuma fileira (issue #10). As capacidades
  // seguem sendo suposicao otimista quando o campo nao da para ler, que e o
  // que `sondado` distingue.
  // O "id" DA RAIZ, e nao o primeiro "id" do documento: um manifesto Stremio
  // tem "id" tambem dentro de catalogs[] e de behaviorHints. Ver js_texto_raiz
  // em js.h, que e onde este leitor mora agora — o TMDB precisou do mesmo.
  if (js_texto_raiz(corpo, "id", addon[i].id, sizeof addon[i].id))
    printf("[addons] %s: id do manifesto = %s\n", addon[i].nome, addon[i].id);
  // js_texto_raiz, e nao js_texto: exatamente a mesma armadilha que o
  // comentario acima descreve para o "id", repetida na instrucao seguinte. Um
  // manifesto Stremio tem "name" tambem dentro de cada catalogs[], e o leitor
  // solto pega o PRIMEIRO do documento. O Bingecat declara um catalogo chamado
  // "search" e o addon inteiro passava a se chamar "search" — no log, na folha
  // de fileiras dos Ajustes e em qualquer lugar que mostre de onde a fileira
  // veio.
  { char nome[64];
    if (js_texto_raiz(corpo, "name", nome, sizeof nome) && nome[0])
      snprintf(addon[i].nome, sizeof addon[i].nome, "%s", nome); }
  if (!r) {
    printf("[addons] %s: manifesto sem \"resources\" legivel; capacidades ficam supostas\n",
           addon[i].nome);
    fflush(stdout);
    return;
  }
  // Pular a CHAVE e ir ao valor. MEDIDO na TV: js_fim sobre a aspa de
  // "resources" devolve o proprio ponteiro, o trecho ficava vazio e TODO addon
  // virava catalogo=0 stream=0 legenda=0 — a primeira busca de fontes (antes
  // da sonda) achava 38, e a partir da segunda nenhum addon era consultado.
  r = strchr(r + 11, ':');
  if (!r) return;
  r++;
  while (*r == ' ' || *r == '\n' || *r == '\t') r++;
  // O campo aceita duas formas no protocolo Stremio: lista de strings
  // ("catalog") e lista de objetos ({"name":"stream",...}). Procurar o NOME
  // solto cobre as duas sem escrever dois analisadores.
  { const char *fim = js_fim(r);
    if (!fim) fim = corpo + strlen(corpo);
    { size_t n = (size_t)(fim - r);
      char *trecho = malloc(n + 1);
      if (!trecho) return;
      memcpy(trecho, r, n); trecho[n] = 0;
      cat = strstr(trecho, "catalog")   != NULL;
      str = strstr(trecho, "stream")    != NULL;
      leg = strstr(trecho, "subtitles") != NULL;
      free(trecho); } }
  addon[i].catalogo = cat;
  addon[i].fonte    = str;
  addon[i].legenda  = leg;
  addon[i].sondado  = 1;
  printf("[addons] %s: catalogo=%d stream=%d legenda=%d\n",
         addon[i].nome, cat, str, leg);
  fflush(stdout);
}

static void *sondar(void *u) {
  int i;
  (void)u;
  for (i = 0; i < nAddon; i++) {
    char url[700], *corpo;
    if (addon[i].sondado) continue;
    snprintf(url, sizeof url, "%s/manifest.json", addon[i].base);
    corpo = rede_baixar(url, 12);
    if (!corpo) {
      // Sem resposta NAO vira "nao fornece nada": ficaria um addon bom apagado
      // da lista por uma falha de rede. Fica como estava, por sondar.
      printf("[addons] manifesto sem resposta: %s\n", addon[i].nome);
      continue;
    }
    capacidadesDoManifesto(i, corpo);
    free(corpo);
  }
  sondaViva = 0;
  return NULL;
}

void addons_manifesto_lido(int i, const char *corpo) {
  if (i < 0 || i >= nAddon || !corpo || !*corpo) return;
  capacidadesDoManifesto(i, corpo);
}

void addons_sondar_manifestos(void) {
  if (sondaViva || nAddon <= 0) return;
  sondaViva = 1;
  if (pthread_create(&fioSonda, NULL, sondar, NULL) != 0) sondaViva = 0;
  else pthread_detach(fioSonda);
}

void addons_legendas_reiniciar(void) {
  char id[64], tp[16];
  pthread_mutex_lock(&legTrava);
  snprintf(id, sizeof id, "%s", legId);
  snprintf(tp, sizeof tp, "%s", legTipo);
  // Zerar o alvo e o que desarma a guarda de "mesmo pedido" logo abaixo; a
  // geracao nova faz o fio vivo, se houver, descartar o que ja tinha juntado.
  legId[0] = 0; legTipo[0] = 0;
  nLegs = 0;
  legGeracao++;
  pthread_mutex_unlock(&legTrava);
  if (id[0]) addons_buscar_legendas(id, tp[0] ? tp : "movie");
}

void addons_buscar_legendas(const char *imdb, const char *tipo) {
  int serie, juntar = 0;
  char id[64], tp[16];
  if (!nAddon || !imdb || !*imdb) return;
  serie = tipo && !strcmp(tipo, "series");
  if (serie && !strchr(imdb, ':'))
    snprintf(id, sizeof id, "%s:1:1", imdb);
  else
    snprintf(id, sizeof id, "%s", imdb);
  snprintf(tp, sizeof tp, "%s", serie ? "series" : "movie");

  pthread_mutex_lock(&legTrava);
  if (!strcmp(id, legId) && !strcmp(tp, legTipo) && (fioLegVivo || nLegs > 0)) {
    pthread_mutex_unlock(&legTrava);
    return;
  }
  snprintf(legId, sizeof legId, "%s", id);
  snprintf(legTipo, sizeof legTipo, "%s", tp);
  legGeracao++;
  nLegs = 0;
  if (fioLegVivo) { pthread_mutex_unlock(&legTrava); return; }
  juntar = fioLegCriado;
  pthread_mutex_unlock(&legTrava);

  if (juntar) pthread_join(fioLeg, NULL);
  pthread_mutex_lock(&legTrava);
  fioLegCriado = 0;
  legParar = 0;
  fioLegVivo = 1;
  if (pthread_create(&fioLeg, NULL, buscarLegendas, NULL) != 0) fioLegVivo = 0;
  else fioLegCriado = 1;
  pthread_mutex_unlock(&legTrava);
}

// UM FIO POR ADDON DE FONTE.
//
// MEDIDO NA TV, na sessao do dono: 16,5 s entre abrir o titulo e ter uma fonte
// escolhida (detail_abrir 51098 -> fonte escolhida 67659). Eram consultas em
// SERIE com 25 s de timeout cada; um addon lento atrasa todos os outros, e a
// tela fica com "buscando" o tempo todo.
//
// Os addons sao independentes e `extrair` so escreve no balde que recebe, entao
// cada um le no proprio. A ORDEM e preservada na juncao: ela decide qual fonte
// o automatico ve primeiro, e trocar a ordem trocaria a fonte escolhida.
#define ADD_FIOS 4

typedef struct {
  int    idx;                 // qual addon
  Stream *achados;
  int    n;
} BaldeFonte;

static BaldeFonte *baldes;
static int nBaldes, proxBalde;
static pthread_mutex_t addTrava = PTHREAD_MUTEX_INITIALIZER;

static void *fioFontes(void *u) {
  (void)u;
  for (;;) {
    int meu, i;
    char url[900], *corpo;
    pthread_mutex_lock(&addTrava);
    if (proxBalde >= nBaldes) { pthread_mutex_unlock(&addTrava); return NULL; }
    meu = proxBalde++;
    pthread_mutex_unlock(&addTrava);
    i = baldes[meu].idx;
    snprintf(url, sizeof url, "%s/stream/%s/%s.json",
             addon[i].base, alvoTipo, alvoId);
    // 12 s e nao 25: com os addons em paralelo o timeout deixa de ser somado,
    // mas continua sendo o tempo que o dono espera pelo mais lento.
    corpo = rede_baixar(url, 12);
    if (!corpo) { printf("[addons] %s: sem resposta\n", addon[i].nome); continue; }
    baldes[meu].n = stream_extrair(corpo, addon[i].nome, &baldes[meu].achados);
    printf("[addons] %s: %d fontes (%u bytes)\n",
           addon[i].nome, baldes[meu].n, (unsigned)strlen(corpo));
    free(corpo);
  }
}

static void *buscar(void *u) {
  Stream *achados = NULL;
  int n = 0, i;
  (void)u;
  marco("addons: consulta inicio");

  nBaldes = 0; proxBalde = 0;
  baldes = calloc((size_t)(nAddon > 0 ? nAddon : 1), sizeof(BaldeFonte));
  if (baldes)
    for (i = 0; i < nAddon; i++)
      if (addon[i].ativo && addon[i].fonte) baldes[nBaldes++].idx = i;

  if (baldes && nBaldes > 0) {
    pthread_t fios[ADD_FIOS];
    int criados = 0, q;
    for (q = 0; q < ADD_FIOS && q < nBaldes; q++)
      if (pthread_create(&fios[criados], NULL, fioFontes, NULL) == 0) criados++;
    if (!criados) fioFontes(NULL);        // sem fios: em serie, mesmo resultado
    for (q = 0; q < criados; q++) pthread_join(fios[q], NULL);
    // Junta NA ORDEM DOS ADDONS, que e a ordem em que o dono os instalou.
    for (q = 0; q < nBaldes; q++) {
      int k = baldes[q].n;
      if (k > 0) {
        Stream *tmp = realloc(achados, sizeof(Stream) * (size_t)(n + k));
        if (tmp) { achados = tmp;
          memcpy(achados + n, baldes[q].achados, sizeof(Stream) * (size_t)k);
          n += k;
        } else printf("[addons] memoria insuficiente para %d fontes\n", k);
      }
      free(baldes[q].achados);
    }
  }
  free(baldes); baldes = NULL; nBaldes = 0;

  marco(n ? "addons: fontes recebidas" : "addons: nenhuma fonte");
  resultado = achados; nResultado = n;
  printf("[addons] total %d\n", n);
  fflush(stdout);
  atomic_store(&estado, n ? ADD_PRONTO : ADD_VAZIO);
  return NULL;
}

void addons_buscar(const char *imdb, const char *tipo) {
  int serie;
  if (!imdb || !*imdb) return;
  if (!nAddon) { stream_definir_lista(NULL, 0); estado = ADD_VAZIO; return; }
  if (fioVivo) {
    if (strcmp(imdb, alvoId) || strcmp(tipo ? tipo : "movie", alvoTipo)) {
      snprintf(pendId, sizeof pendId, "%s", imdb);
      snprintf(pendTipo, sizeof pendTipo, "%s", tipo ? tipo : "movie");
    }
    return;
  }
  stream_definir_lista(NULL, 0);
  serie = tipo && !strcmp(tipo, "series");
  // Serie SEM episodio devolve lista vazia, com HTTP 200 e sem erro nenhum
  // (medido: 14 bytes de resposta). O identificador tem de ser
  // "tt1234567:temporada:episodio". Como o catalogo ainda nao traz lista de
  // episodios, assume T1E1 — e o mesmo lugar onde o episodio real entra quando
  // houver.
  if (serie && !strchr(imdb, ':'))
    snprintf(alvoId, sizeof alvoId, "%s:1:1", imdb);
  else
    snprintf(alvoId, sizeof alvoId, "%s", imdb);
  snprintf(alvoTipo, sizeof alvoTipo, "%s", tipo && *tipo ? tipo : "movie");
  { int t = 0, e = 0; const char *dp = strchr(alvoId, ':');
    if (dp) sscanf(dp + 1, "%d:%d", &t, &e);
    debrid_definir_episodio(t, e); }
  estado = ADD_BUSCANDO;
  fioVivo = 1;
  if (pthread_create(&fio, NULL, buscar, NULL) != 0) { fioVivo = 0; estado = ADD_PARADO; }
}

void addons_encerrar(void) {
  int juntarLeg;
  if (fioVivo) pthread_join(fio, NULL);
  fioVivo = 0;
  pthread_mutex_lock(&legTrava);
  legParar = 1; legGeracao++; juntarLeg = fioLegCriado;
  pthread_mutex_unlock(&legTrava);
  if (juntarLeg) pthread_join(fioLeg, NULL);
  pthread_mutex_lock(&legTrava);
  fioLegCriado = fioLegVivo = 0; nLegs = 0;
  pthread_mutex_unlock(&legTrava);
  free(resultado); resultado = NULL; nResultado = 0;
  estado = ADD_PARADO;
}

// As fontes de colecao da conta trazem addonId (o "id" do manifesto), nao a
// URL. So a sonda sabe o id, entao a resposta e vazia ate ela passar por
// aquele addon — quem chama tenta de novo depois.
const char *addons_base_por_id(const char *id) {
  int i;
  if (!id || !*id) return "";
  for (i = 0; i < nAddon; i++)
    if (addon[i].id[0] && !strcmp(addon[i].id, id)) return addon[i].base;
  return "";
}
