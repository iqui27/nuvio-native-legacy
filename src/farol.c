// FAROL NA HOME — a adocao da cena (issue 45). Ver farol.h para o contrato.
//
// FLUXO DE UMA VOLTA (farol_home_fileiras, chamada por montar() a cada
// publicacao, inclusive nas servidas do snapshot):
//   1. portoes: "Recomendar nesta TV" desligado ou modo seguro -> nada.
//   2. a cena adotada ainda vale? vale 24 h (vale_s da cena) ou ate evento
//      forte — a mudanca do modelo farolgosto medida por um hash da copia.
//      Precisando recompor e a Home na tela: a cena antiga FICA (os cartazes
//      nao trocam com a pessoa olhando, PLANO-GOSTO 4.2). Com a Home fora,
//      no arranque, ou com o perfil trocado: recompoe.
//   3. compor() = modelo + candidatos + sementes -> farolcena_home (44) ->
//      farolpeca_ler (39) -> adocao: pecas gosto1/gosto2 viram FarolHomeFil
//      com textos ja renderizados. COMPOSITOR FALHOU -> a cena invalida e a
//      antiga NAO e reaproveitada (decisao do dono no contrato).
//   4. entrega: cada fileira vira uma CatFileira contigua no lote — itens
//      resolvidos por IMDb contra o catalogo publicado (os fora do catalogo
//      entram como CatItem minimo, com o imdb que busca_pediu_abrir entende).
//      fil_oculta manda: fileira oculta na hora da entrega nao entra.
//
// ESTABILIDADE DOS ITENS. A cena guarda REFERENCIAS (imdb/tipo/titulo), nunca
// indice de cat_item — cat_revisao() guardado so para diagnostico/teste.
//
// QUANTO CUSTA. Estatico, uma copia por fio (o da descoberta): fichas 23 KB +
// candidatos 12 KB + refs 18 KB + cena validada 26 KB + json 8 KB + a cena
// adotada 11 KB ~ 98 KB, dentro do orcamento da secao 4.4 com a cena
// transitoria ja descontada (valida, consome e guarda so o recorte).
#include "farol.h"
#include "farolcena.h"      // ANTES de qualquer farolrec.h direto (cabecalho)
#include "farolsinal.h"
#include "farolcompor.h"
#include "faroltexto.h"
#include "dts_tv.h"
#include "farolsonda.h"
#include "redesaude.h"
#include "addons.h"
#include "debrid.h"
#include "memlog.h"
#include "webosver.h"
#include "dts/dts_hls_tpk.h"
#include "dts/dts_playback.h"
#include "seguro.h"
#include "perfilinfantil.h"
#include "fileiras.h"
#include "diario.h"
#include "salvos.h"
#include "perfis.h"
#include "idioma.h"
#include "mapa.h"
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Prototipos locais (equivalente ao home_fileiras_na_tela): esses headers puxam
// SDL2/GL e o Farol permanece puro nos fixtures sem video/UI.
int home_fileiras_na_tela(void);
const char *app_tela_nome(void);
int recomenda_pessoa_canonica(const char *id, char *dst, size_t tam);
void stream_farol_fatos(int *fonteDv, int *fonteHdr, int *fonteGBx10,
                        int *fonteMbps, int *fonteDebrid, int *temOutraFonte,
                        int *temFonteMenor, int *temFonteSemDv,
                        int *temFonteSemDts, int *tvHdr, int *tvDv,
                        int *vazaoMbps);
int video_registro_negado(void);
int video_audio_nao_suportado(void);
int video_n_audio(void);
// video.h puxa SDL2 (o Farol permanece puro nas fixtures); as assinaturas
// reais usam const VideoFaixa *, aqui declarada opaca — mesma ABI, sem SDL.
typedef struct VideoFaixa VideoFaixa;
const VideoFaixa *video_audio(int i);
int video_audio_atual(void);
int video_faixa_dts(const VideoFaixa *f);
int video_dts_estado(void);
int video_tem_dolby_vision(void);
const char *video_hdr(void);
#ifdef NV_TPK
int video_tpk_tizen_maior(void);
#endif
int player_ultimo_erro_farol(FarolErro *out, int *foiDebrid);
uint64_t player_ultimo_erro_tentativa(void);
uint64_t player_ultimo_erro_seq(void);
int player_externo_disponivel_tentativa(uint64_t tentativa);
const char *atualizacao_nova(void);

#define FAROL_VIDEO_DTS_NENHUM 0
#define FAROL_VIDEO_DTS_FALHOU 4

// Ganchos fracos para testes isolados que compilam sem farolcompor.c.
__attribute__((weak)) unsigned farolcompor_home_geracao(void) { return 0; }
__attribute__((weak)) int farolcompor_cena_adotada(FarolCena *cenaOut,
                                                   FarolComporCand *candOut,
                                                   int *nCandOut, int maxCand,
                                                   uint32_t *hModeloOut) {
  (void)cenaOut; (void)candOut; (void)maxCand;
  if (nCandOut) *nCandOut = 0;
  if (hModeloOut) *hModeloOut = 0;
  return 0;
}
__attribute__((weak)) int farolcompor_pedir_home(const FarolGosto *g,
                                                 const FarolComporCand *cand, int nCand,
                                                 const FarolComporAncora *ancoras, int nAncoras,
                                                 const char *(*nomePessoaFn)(int32_t, void *),
                                                 void *u, uint32_t hModelo) {
  (void)g; (void)cand; (void)nCand; (void)ancoras; (void)nAncoras;
  (void)nomePessoaFn; (void)u; (void)hModelo;
  return 0;
}
__attribute__((weak)) void farolcompor_trocar_perfil(void) {}
__attribute__((weak)) void farolcompor_zerar_gosto(void) {}

#define FAROL_CAND_MAX 240          // 60 x 4 blocos; 512 era para um catalogo
#define FAROL_SEM_MAX  16
#define FAROL_PESSOA_T 640          // hash/id -> nome, para os argumentos p<id>
#define DIA_EPOCA       1767225600LL    // 1/1/2026 UTC — o mesmo de farolsinal

// ----------------------------------------------------------------- cena ----
typedef struct {
  char imdb[64], tipo[8], titulo[160];
  char motivo[200];                 // linha ja renderizada; "" = sem motivo
} FarolHomeItem;
typedef struct {
  char chave[32];                   // "farol_gosto1" | "farol_gosto2"
  char titulo[96], sub[96];
  int  nItens, marcada;             // marcada = farolsinal_mostrado ja rodou
  FarolHomeItem it[FAROL_ITENS_MAX];
} FarolHomeFil;
static struct {
  int       valida;
  long long feita, valeS;           // quando composta e quanto vale (s)
  int       perfil;                 // perfis_ativo() da adocao
  unsigned  revCat;                 // cat_revisao() da adocao (diagnostico)
  uint32_t  hModelo;                // hash do FarolGosto — o "evento forte"
  unsigned  comporGen;              // geracao de farolcompor adotada
  int       nFil;
  FarolHomeFil fil[2];
} adotada;
static pthread_mutex_t travaCena = PTHREAD_MUTEX_INITIALIZER;
// perfis_ativo() da ultima cena que saiu por falha/limpeza, sob travaCena:
// e o que impede readotar outra cena no MESMO perfil com a Home a vista.
// -1 = nenhuma cena saiu ainda (o arranque e a troca de perfil adotam na
// hora, mesmo com a Home desenhada).
static int      perfilSaiu = -1;
// Sobe a cada adocao e a cada invalidacao; a Home cacheia o motivo por ela
// (lida por __atomic, sem trava no fio do desenho).
static unsigned geracaoCena;
// A EPOCA do gosto: sobe so no "Zerar o gosto". Cada CatFileira farol_ sai
// da entrega carimbada com ela; a publicacao e a remontagem da descoberta
// descartam o carimbo velho, entao um lote em voo ou a filsMontadas de
// antes do reset nao republicam fileiras mortas (FAROL45C).
static unsigned seloGosto;

// ---------------------------------------------------------------- trabalho -
// Uma montagem por vez (o fio da descoberta); estatico porque CatItem tem
// ~16 KB e a pilha do fio no Tizen e curta.
static FarolFicha    fichasC[FAROL_CAND_MAX];
static FarolRecCand  cand[FAROL_CAND_MAX];
static uint16_t      salvoD[FAROL_CAND_MAX];      // paralelo a cand (a cena)
static struct { char imdb[64]; char tipo[8]; char titulo[160]; int32_t catIdx; }
                     refC[FAROL_CAND_MAX];        // paralelo a cand (nos)
static int           nCandAtivo;
static FarolFicha    fichasS[FAROL_SEM_MAX];
static FarolRecSemente sem[FAROL_SEM_MAX];
static const char   *semRef[FAROL_SEM_MAX];       // paralelo a sem
static char          semIds[FAROL_SEM_MAX][64];   // "tt..." guardado
static struct { int32_t id; char nome[64]; } pessoaT[FAROL_PESSOA_T];
static int           nPessoaT;
static FarolCena     cenaP;
static FarolCena     llmCena;
static FarolComporCand compCand[FAROL_COMPOR_CAND_MAX];
static FarolComporAncora compAnc[FAROL_COMPOR_ANCORA_MAX];
static FarolComporCand llmCand[FAROL_COMPOR_CAND_MAX];
static char          cenaJson[FAROL_CENA_MAX + 1];
static char          cwImdb[48][64]; static int nCwImdb;
static char          motOut[200], titOut[96];

// ---------------------------------------------------------------- relogio --
static long long (*relS)(void);
static int (*visFn)(void);
static long long agoraS(void) { return relS ? relS() : (long long)time(NULL); }
void farol_teste_relogio(long long (*s)(void)) { relS = s; }
void farol_teste_home_visivel(int (*v)(void)) { visFn = v; }
// A cena nova so e adotada com a Home (e o detalhe por cima dela) fora da
// tela — abrir um titulo nao pode trocar os cartazes debaixo dos olhos.
// "Na tela" e a Home DESENHADA, nao a camada de cima: o menu lateral e o
// painel de Salvos abrem sobre a Home E sobre outras telas, entao
// app_tela_nome() so nao decide. O desenho marca a cada quadro e a copia
// parada do painel marca pelo mesmo contador (home_fileiras_na_tela /
// home_fileiras_vistas) — se o motivo do Farol continua pintando, a cena
// nao pode trocar.
static int homeVisivel(void) {
  const char *t;
  if (visFn) return visFn();
  t = app_tela_nome();
  if (t && !strcmp(t, "home")) return 1;
  return home_fileiras_na_tela();
}
static int diaDe(long long quando) {
  long long d = (quando - DIA_EPOCA) / 86400;
  if (d < 0) d = 0;
  if (d > 65535) d = 65535;
  return (int)d;
}
// A MESMA tabela de farolsinal.c: fds (dom/sab) * 4 + [manha,tarde,noite,mad].
static int faixaDeHora(int wd, int h) {
  int p = h < 6 ? 3 : h < 12 ? 0 : h < 18 ? 1 : 2;
  return ((wd == 0 || wd == 6) ? 4 : 0) + p;
}
static int faixaAgoraFim(long long quando, int *fimSem) {
  time_t t = (time_t)quando; struct tm tmv;
  localtime_r(&t, &tmv);
  if (fimSem) *fimSem = (tmv.tm_wday == 0 || tmv.tm_wday == 6);
  return faixaDeHora(tmv.tm_wday, tmv.tm_hour);
}
// "Evento forte" barato: o modelo mudou (terminou, recusou, salvou...). O
// hash e da COPIA que farolsinal_modelo devolve — a estrutura nao tem
// ponteiros, o mesmo estado da o mesmo hash — MAS so do que e entrada de
// recomendacao. O anel de exibidos (mostProx + mostrado[]) fica fora: ele
// so aprende "ja apareceu na fileira", e se entrasse no hash a propria
// exposicao faria a cena que acabou de subir parar de valer (revisao
// FAROL45 r1).
static uint32_t hashModelo(const FarolGosto *g) {
  static const struct { size_t de, ate; } T[] = {
    { 0, offsetof(FarolGosto, mostProx) },
    { offsetof(FarolGosto, genero), offsetof(FarolGosto, mostrado) },
    { offsetof(FarolGosto, horaResto), sizeof(FarolGosto) },
  };
  const unsigned char *p = (const unsigned char *)g;
  uint32_t h = 2166136261u;
  size_t t, i;
  for (t = 0; t < sizeof T / sizeof *T; t++)
    for (i = T[t].de; i < T[t].ate; i++) h = (h ^ p[i]) * 16777619u;
  return h;
}

// ------------------------------------------------- renderizacao de texto ---
static const char *modeloLiteral(const char *m) {
  static const struct { const char *nome, *lit; } MODS[] = {
#define FAROL_MOD_X(n) { "FT_" #n, FT_##n },
    FAROL_FRASES(FAROL_MOD_X)
#undef FAROL_MOD_X
  };
  size_t i;
  if (!m || !m[0]) return NULL;
  for (i = 0; i < sizeof MODS / sizeof *MODS; i++)
    if (!strcmp(m, MODS[i].nome)) return MODS[i].lit;
  return NULL;
}

// id de genero -> nome. Positivo e o id do TMDB; negativo e o mapa_hash_nome
// do NOME do genero como o catalogo o escreveu (PT ou EN, por isso as duas
// listas). Sem traducao o argumento nao vira linha — a regra da ALMA.
static const char *generoNome(long id) {
  // Os nomes de exibicao sao so os que a tabela de idiomas ja conhece —
  // o combinado da TV vira o genero canonico do mapa (10759 -> Acao,
  // 10768 -> Guerra; 10770 "Filme para TV" fica sem nome e sem linha).
  static const struct { int id; const char *pt; } G[] = {
    { 28, "Ação" }, { 12, "Aventura" }, { 16, "Animação" }, { 35, "Comédia" },
    { 80, "Crime" }, { 99, "Documentário" }, { 18, "Drama" }, { 10751, "Família" },
    { 14, "Fantasia" }, { 36, "História" }, { 27, "Terror" }, { 10402, "Música" },
    { 9648, "Mistério" }, { 10749, "Romance" }, { 878, "Ficção científica" },
    { 53, "Suspense" }, { 10752, "Guerra" }, { 37, "Faroeste" },
    { 10759, "Ação" }, { 10762, "Infantil" }, { 10763, "Notícias" },
    { 10764, "Reality" }, { 10765, "Ficção científica" }, { 10766, "Novela" },
    { 10767, "Talk show" }, { 10768, "Guerra" },
  };
  // id negativo = hash do NOME guardado na ficha, que pode vir em PT ou em
  // EN conforme a fonte: a chave e o nome como veio, a exibicao e sempre a
  // forma canonica que a tabela de idiomas traduz.
  static const struct { const char *chave; const char *pt; } N[] = {
    { "Ação", "Ação" }, { "Aventura", "Aventura" }, { "Animação", "Animação" },
    { "Comédia", "Comédia" }, { "Crime", "Crime" },
    { "Documentário", "Documentário" }, { "Drama", "Drama" },
    { "Família", "Família" }, { "Fantasia", "Fantasia" },
    { "História", "História" }, { "Terror", "Terror" }, { "Música", "Música" },
    { "Mistério", "Mistério" }, { "Romance", "Romance" },
    { "Ficção científica", "Ficção científica" }, { "Suspense", "Suspense" },
    { "Guerra", "Guerra" }, { "Faroeste", "Faroeste" },
    { "Infantil", "Infantil" }, { "Notícias", "Notícias" },
    { "Reality", "Reality" }, { "Novela", "Novela" },
    { "Talk show", "Talk show" },
    { "Action", "Ação" }, { "Action & Adventure", "Ação" },
    { "Adventure", "Aventura" }, { "Animation", "Animação" },
    { "Comedy", "Comédia" }, { "Crime", "Crime" },
    { "Documentary", "Documentário" }, { "Drama", "Drama" },
    { "Family", "Família" }, { "Fantasy", "Fantasia" },
    { "History", "História" }, { "Horror", "Terror" }, { "Music", "Música" },
    { "Mystery", "Mistério" }, { "Romance", "Romance" },
    { "Sci-Fi & Fantasy", "Ficção científica" },
    { "Science Fiction", "Ficção científica" }, { "Thriller", "Suspense" },
    { "War", "Guerra" }, { "War & Politics", "Guerra" },
    { "Western", "Faroeste" }, { "Kids", "Infantil" }, { "News", "Notícias" },
    { "Reality", "Reality" }, { "Soap", "Novela" }, { "Talk", "Talk show" },
  };
  size_t i;
  if (id > 0) {
    for (i = 0; i < sizeof G / sizeof *G; i++)
      if (G[i].id == id) return i18n(G[i].pt);
    return NULL;
  }
  for (i = 0; i < sizeof N / sizeof *N; i++)
    if ((long)mapa_hash_nome(N[i].chave) == id) return i18n(N[i].pt);
  return NULL;
}

// id de pessoa -> nome. A tabela e alimentada na varredura do catalogo com
// os dois espacos de id da ficha (tmdb > 0 e mapa_hash_nome < 0).
static void pessoaGrava(const char *nome, long tmdb) {
  if (!nome[0] || nPessoaT >= FAROL_PESSOA_T) return;
  long ids[2] = { tmdb > 0 ? tmdb : 0, mapa_hash_nome(nome) };
  for (int k = 0; k < 2; k++) {
    long id = ids[k];
    int i;
    if (!id) continue;
    for (i = 0; i < nPessoaT; i++) if (pessoaT[i].id == id) break;
    if (i == nPessoaT) {
      if (nPessoaT >= FAROL_PESSOA_T - 1) return;
      pessoaT[nPessoaT].id = (int32_t)id;
      snprintf(pessoaT[nPessoaT].nome, sizeof pessoaT[nPessoaT].nome, "%s", nome);
      nPessoaT++;
    }
  }
}
static const char *pessoaNome(long id) {
  int i;
  for (i = 0; i < nPessoaT; i++) if (pessoaT[i].id == id) return pessoaT[i].nome;
  return NULL;
}

// Resolve um argumento do modelo a NOME. NULL = nao resolve: a linha cai
// inteira, nunca sai id cru na tela (ALMA + CONTRATO de argumentos da 44).
static const char *argNomeExt(const char *a, const FarolComporCand *sc, int nSc,
                              char *dst, size_t cap) {
  if (!a || !a[0]) return NULL;
  if (a[0] == 'c' && a[1] >= '0' && a[1] <= '9') {
    int idx = atoi(a + 1);
    CatItem ci;
    if (sc) {
      if (idx < 0 || idx >= nSc) return NULL;
      if (sc[idx].imdb[0] && cat_copiar_por_id(sc[idx].imdb, NULL, &ci) &&
          ci.titulo[0] && !strcmp(ci.imdb, sc[idx].imdb)) {
        snprintf(dst, cap, "%s", ci.titulo); return dst;
      }
      if (sc[idx].titulo[0]) { snprintf(dst, cap, "%s", sc[idx].titulo); return dst; }
      return NULL;
    }
    if (idx < 0 || idx >= nCandAtivo) return NULL;
    if (refC[idx].catIdx >= 0 && cat_copiar_item(refC[idx].catIdx, &ci) &&
        ci.titulo[0]) { snprintf(dst, cap, "%s", ci.titulo); return dst; }
    return NULL;
  }
  if (a[0] == 't' && a[1] == 't') {
    // cat_indice_por_imdb catupa cat_revisao() internamente; a copia sai do
    // catalogo da troca, nao de uma versao a meio.
    int i = cat_indice_por_imdb(a);
    CatItem ci; char t[160];
    if (i >= 0 && cat_copiar_item(i, &ci) && ci.titulo[0] &&
        !strcmp(ci.imdb, a)) {
      snprintf(dst, cap, "%s", ci.titulo); return dst;
    }
    if (diario_nome(a, t, sizeof t, NULL, NULL, 0) && t[0]) {
      snprintf(dst, cap, "%s", t); return dst;
    }
    if (sc) {
      int k;
      for (k = 0; k < nSc; k++) {
        if (!strcmp(sc[k].imdb, a) && sc[k].titulo[0]) {
          snprintf(dst, cap, "%s", sc[k].titulo); return dst;
        }
      }
    }
    return NULL;
  }
  if (a[0] == 'g' || a[0] == 'p') {
    const char *n = a[0] == 'g' ? generoNome(atol(a + 1))
                                : pessoaNome(atol(a + 1));
    if (!n) return NULL;
    snprintf(dst, cap, "%s", n); return dst;
  }
  if (a[0] == 'k') return NULL;      // sem tabela id->nome de tema local
  if (a[0] == 'u')
    return recomenda_pessoa_canonica(a + 1, dst, cap) ? dst : NULL;
  snprintf(dst, cap, "%s", a);       // numero, duracao "95", hora "23:40"
  return dst;
}

// FarolTexto -> string pronta (i18n aplicada sobre o literal do modelo,
// como o resto do app). 0 = sem linha honesta: sem modelo, aridade torta
// ou argumento que nao virou nome.
static int renderTextoExt(const FarolCena *c, const FarolTexto *t,
                          const FarolComporCand *sc, int nSc,
                          char *dst, size_t cap) {
  char ordem[FAROL_ARGS + 1], buf[FAROL_ARGS][160];
  const char *sv[FAROL_ARGS];
  int dv[FAROL_ARGS], nO = 0, i, cod = 0;
  const char *fmt, *p;
  dst[0] = 0;
  if (!t || !farolpeca_texto_tem(t)) return 0;
  if (t->usaX) {
    const char *x = farolpeca_x(c, t);
    snprintf(dst, cap, "%s", x ? x : "");
    return dst[0] != 0;
  }
  fmt = modeloLiteral(t->m);
  if (!fmt) return 0;
  fmt = i18n(fmt);
  for (p = fmt; *p && nO < FAROL_ARGS; p++)
    if (p[0] == '%' && (p[1] == 's' || p[1] == 'd')) { ordem[nO] = p[1]; nO++; p++; }
  ordem[nO] = 0;
  if (nO != t->nArgs) return 0;
  for (i = 0; i < nO; i++) {
    if (ordem[i] == 'd') { dv[i] = atoi(t->a[i]); sv[i] = NULL; }
    else {
      const char *s = argNomeExt(t->a[i], sc, nSc, buf[i], sizeof buf[i]);
      if (!s) return 0;
      sv[i] = s; dv[i] = 0;
    }
    cod = cod * 4 + (ordem[i] == 'd' ? 2 : 1);
  }
  switch (cod) {
  case 0:                     snprintf(dst, cap, "%s", fmt); break;
  case 1:                     snprintf(dst, cap, fmt, sv[0]); break;
  case 2:                     snprintf(dst, cap, fmt, dv[0]); break;
  case 5:                     snprintf(dst, cap, fmt, sv[0], sv[1]); break;
  case 6:                     snprintf(dst, cap, fmt, sv[0], dv[1]); break;
  case 9:                     snprintf(dst, cap, fmt, dv[0], sv[1]); break;
  case 10:                    snprintf(dst, cap, fmt, dv[0], dv[1]); break;
  case 21:                    snprintf(dst, cap, fmt, sv[0], sv[1], sv[2]); break;
  case 25:                    snprintf(dst, cap, fmt, sv[0], dv[1], sv[2]); break;
  case 26:                    snprintf(dst, cap, fmt, sv[0], dv[1], dv[2]); break;
  case 37:                    snprintf(dst, cap, fmt, dv[0], sv[1], sv[2]); break;
  case 38:                    snprintf(dst, cap, fmt, dv[0], sv[1], sv[2]); break;
  case 41:                    snprintf(dst, cap, fmt, dv[0], dv[1], sv[2]); break;
  case 42:                    snprintf(dst, cap, fmt, dv[0], dv[1], dv[2]); break;
  case 85:                    snprintf(dst, cap, fmt, sv[0], sv[1], sv[2], sv[3]); break;
  default:                    return 0;
  }
  return dst[0] != 0;
}

__attribute__((unused))
static int renderTexto(const FarolCena *c, const FarolTexto *t,
                       char *dst, size_t cap) {
  return renderTextoExt(c, t, NULL, 0, dst, cap);
}

// --------------------------------------------------------------- sementes --
// Ate 16 titulos vistos mais recentes: a regra de diversidade ("mesma
// semente") e o motivo FMO_TERMINOU leem daqui. `ref` e o "tt..." que nomeia
// a semente nos argumentos; id que nao e IMDb fica sem ref e o motivo
// TERMINOU dela nao sai (sem motivo verdadeiro, sem frase).
static int coletarSementes(int perfil) {
  int n = diario_n(perfil), ns = 0, i;
  for (i = n - 1; i >= 0 && ns < FAROL_SEM_MAX; i--) {
    DiarioEntrada e; DiarioTitulo t; int j, idx;
    const CatItem *ci;
    if (!diario_entrada(perfil, i, &e) || !e.id[0]) continue;
    for (j = 0; j < ns; j++) if (!strcmp(semIds[j], e.id)) break;
    if (j < ns) continue;
    snprintf(semIds[ns], sizeof semIds[ns], "%s", e.id);
    memset(&fichasS[ns], 0, sizeof fichasS[ns]);
    memset(&sem[ns], 0, sizeof sem[ns]);
    sem[ns].ficha = NULL;
    idx = cat_indice_por_imdb(e.id);
    if (idx >= 0 && (ci = cat_item(idx)) != NULL &&
        farolficha_ler_item(ci, &fichasS[ns]))
      sem[ns].ficha = &fichasS[ns];
    // "terminou" so e dito quando a entrada NAO e parcial — numa serie o
    // agregado nao sabe dizer "fechou", entao o motivo fica para o filme.
    sem[ns].terminou = e.tipo == DIARIO_FILME && !(e.flags & DIARIO_F_PARCIAL);
    if (!diario_titulo(perfil, e.id, &t)) { /* sem agregado, vale a entrada */ }
    semRef[ns] = !strncmp(e.id, "tt", 2) ? semIds[ns] : NULL;
    ns++;
  }
  return ns;
}

// -------------------------------------------------------------- candidatos -
static int candAcha(const char *imdb) {
  int i;
  for (i = 0; i < nCandAtivo; i++) if (!strcmp(refC[i].imdb, imdb)) return i;
  return -1;
}
static int emColecao(const char *imdb) {
  int i;
  for (i = 0; i < nCwImdb; i++) if (!strcmp(cwImdb[i], imdb)) return 1;
  return 0;
}
static void candDeItem(const CatItem *ci, int catIdx, int fontes, int flags) {
  FarolRecCand *c;
  int i = nCandAtivo;
  if (i >= FAROL_CAND_MAX) return;
  memset(&cand[i], 0, sizeof cand[i]);
  memset(&fichasC[i], 0, sizeof fichasC[i]);
  if (farolficha_ler_item(ci, &fichasC[i])) {
    // A ficha ja guardada (do detalhe do TMDB, com tema real) vale mais que
    // a reconstrucao do catalogo — sem tema a fileira "porque voce viu X"
    // nunca sai. So leitura do cache; nao enche a LRU com ficha do catalogo.
    FarolFicha det;
    if (farolsinal_ficha_do_item(ci, &det) && det.nTema > fichasC[i].nTema)
      fichasC[i] = det;
    cand[i].ficha = &fichasC[i];
    cand[i].chave = farolficha_chave(&fichasC[i]);
  }
  cand[i].h = mapa_hash_titulo(ci->titulo);
  cand[i].semente = FRC_SEM_SEMENTE;
  cand[i].fontes = (uint8_t)fontes;
  cand[i].flags = (uint8_t)flags;
  snprintf(refC[i].imdb, sizeof refC[i].imdb, "%s", ci->imdb);
  snprintf(refC[i].tipo, sizeof refC[i].tipo, "%s", ci->tipo);
  snprintf(refC[i].titulo, sizeof refC[i].titulo, "%s", ci->titulo);
  refC[i].catIdx = catIdx;
  salvoD[i] = 0;
  nCandAtivo = i + 1;
  (void)c;
}
// A fileira "Continuar assistindo" publicada na volta anterior: o que ja
// esta nela sai dos candidatos (FRC_FL_CONTINUAR — regra so da Home). Os
// imdb sao capturados DENTRO da trava do catalogo (cat_fileira_imdb) — ler
// cat_fileira()/cat_item() por fora corria com cat_trocar_continuar()
// reescrevendo a fileira no meio da coleta (revisao FAROL45 r1).
static void coletarContinuar(void) {
  nCwImdb = cat_fileira_imdb("continue_watching", cwImdb, 48, 64);
}
// Varredura do catalogo publicado + salvos nao vistos. Vistos, duplicatas,
// itens do Continuar e o que o perfil infantil nao pode tocar saem aqui
// (VISTO/CONTINUAR nem viram candidato; FORA_CLASS fica de flag para o motor
// unir por chave como manda a tabela 3.1).
static int coletarCandidatos(int infantil) {
  int i, n = cat_n(), ns;
  nCandAtivo = 0; nPessoaT = 0;
  coletarContinuar();
  for (i = 0; i < n && nCandAtivo < FAROL_CAND_MAX; i++) {
    const CatItem *ci = cat_item(i);
    int e;
    if (!ci || !ci->imdb[0]) continue;
    for (e = 0; e < CAT_ELENCO_MAX && ci->elenco[e].nome[0]; e++)
      pessoaGrava(ci->elenco[e].nome, ci->elenco[e].tmdb);
    if (strcmp(ci->tipo, "movie") && strcmp(ci->tipo, "series")) continue;
    if (cat_visto(ci) || emColecao(ci->imdb) || candAcha(ci->imdb) >= 0) continue;
    candDeItem(ci, i, FRC_F_CATALOGO,
               infantil && !perfil_infantil_classif_ok(ci->classificacao)
                 ? FRC_FL_FORA_CLASS : 0);
  }
  // SALVOS: flag + dia nos que ja sao candidato; os fora do catalogo entram
  // so com id IMDb (contrato FORA_CAT + TEM_IMDB) e ficha do proprio SalvoItem.
  ns = salvos_n();
  for (i = 0; i < ns && nCandAtivo < FAROL_CAND_MAX; i++) {
    const SalvoItem *s = salvos_item(i);
    int j; DiarioTitulo t;
    if (!s || !s->id[0] || emColecao(s->id)) continue;
    j = candAcha(s->id);
    if (j >= 0) {
      cand[j].fontes |= FRC_F_SALVO;
      salvoD[j] = (uint16_t)diaDe(s->quandoS);
      continue;
    }
    if (strncmp(s->id, "tt", 2)) continue;           // sem TEM_IMDB nao entra
    if (diario_titulo(perfis_ativo(), s->id, &t) && t.vezes > 0) continue;
    {
      CatItem tmp;
      memset(&tmp, 0, sizeof tmp);
      snprintf(tmp.imdb, sizeof tmp.imdb, "%s", s->id);
      snprintf(tmp.tipo, sizeof tmp.tipo, "%s", s->tipo);
      snprintf(tmp.titulo, sizeof tmp.titulo, "%s", s->titulo);
      candDeItem(&tmp, -1, FRC_F_SALVO, FRC_FL_FORA_CAT | FRC_FL_TEM_IMDB);
      salvoD[nCandAtivo - 1] = (uint16_t)diaDe(s->quandoS);
    }
  }
  return nCandAtivo;
}

static const char *comporPessoaLookup(int32_t id, void *u) {
  (void)u;
  return pessoaNome((long)id);
}

// Titulo de ancora SO com proveniencia publica (o catalogo). O nome que so o
// Diario conhece e privado: nunca vira `gosto.ancoras` (PLANO-GOSTO 5.3).
static void comporTituloPublico(const char *imdb, char *dst, size_t cap) {
  CatItem ci;
  dst[0] = '\0';
  if (!imdb || !imdb[0]) return;
  if (cat_copiar_por_id(imdb, NULL, &ci) && ci.titulo[0] &&
      !strcmp(ci.imdb, imdb)) {
    snprintf(dst, cap, "%s", ci.titulo);
  }
}

static int comporMontarResumo(const FarolCenaCtx *ctx, int *nCandOut, int *nAncOut) {
  FarolRecCtx rctx;
  FarolRecItem rec[FAROL_COMPOR_CAND_MAX];
  int nr, i, nc = 0, na = 0;

  *nCandOut = 0;
  *nAncOut = 0;
  if (!ctx || !ctx->g || ctx->nCand <= 0) return 0;

  memset(&rctx, 0, sizeof rctx);
  rctx.g = ctx->g;
  rctx.sementes = ctx->sementes;
  rctx.nSementes = ctx->nSementes;
  rctx.dia = ctx->dia;
  rctx.faixaHora = ctx->faixaHora;
  rctx.naHome = 1;
  rctx.semente = ctx->semente;

  nr = farolrec_escolher(&rctx, ctx->cand, ctx->nCand, rec, FAROL_COMPOR_CAND_MAX);
  if (nr <= 0) return 0;

  memset(compCand, 0, sizeof compCand);
  memset(compAnc, 0, sizeof compAnc);

  for (i = 0; i < nr && nc < FAROL_COMPOR_CAND_MAX; i++) {
    int idx = rec[i].cand;
    FarolComporCand *cc;
    const FarolFicha *f;
    int m, k;
    if (idx < 0 || idx >= ctx->nCand) continue;
    if (strncmp(refC[idx].imdb, "tt", 2) != 0) continue;

    cc = &compCand[nc];
    snprintf(cc->imdb, sizeof cc->imdb, "%s", refC[idx].imdb);
    cc->tipo = !strcmp(refC[idx].tipo, "series") ? 1 : 0;
    snprintf(cc->titulo, sizeof cc->titulo, "%s", refC[idx].titulo);
    cc->nota10 = -1;
    cc->pontos = rec[i].pontos;

    f = ctx->cand[idx].ficha;
    if (f) {
      cc->tmdb = f->tmdb;
      cc->ano = (int16_t)f->ano;
      cc->minutos = (int16_t)f->minutos;
      if (f->nota > 0 && f->nota <= 100) cc->nota10 = (int16_t)f->nota;
      for (k = 0; k < f->nGenero && k < FAROL_FICHA_GEN && cc->nGeneros < 8; k++) {
        if (f->genero[k] > 0) cc->generos[cc->nGeneros++] = f->genero[k];
      }
    }

    for (m = 0; m < rec[i].nMotivo && m < FAROL_REC_MOTIVOS && cc->nMotivos < FAROL_COMPOR_MOT_MAX; m++) {
      const FarolRecMotivo *rm = &rec[i].motivo[m];
      if (rm->tipo == FMO_AMIGO || rm->tipo == FMO_RECOMENDADO) {
        // Sempre anonimizado no fio HTTP.
        cc->motivos[cc->nMotivos].tipo = rm->tipo;
        cc->motivos[cc->nMotivos].a = 0;
        cc->motivos[cc->nMotivos].b = 0;
        cc->nMotivos++;
      } else if (rm->tipo == FMO_TERMINOU) {
        int sIdx = rm->a;
        const char *tt = (sIdx >= 0 && sIdx < ctx->nSementes) ? ctx->sementeRef[sIdx] : NULL;
        int ai = -1, a;
        if (!tt || strncmp(tt, "tt", 2) != 0) continue;
        // Entrada parcial (ou serie) nao sustenta "terminou".
        if (!ctx->sementes[sIdx].terminou) continue;
        for (a = 0; a < na; a++) {
          if (!strcmp(compAnc[a].id, tt)) { ai = a; break; }
        }
        if (ai < 0 && na < FAROL_COMPOR_ANCORA_MAX) {
          char tit[100];
          comporTituloPublico(tt, tit, sizeof tit);
          if (tit[0]) {
            snprintf(compAnc[na].id, sizeof compAnc[na].id, "%s", tt);
            snprintf(compAnc[na].titulo, sizeof compAnc[na].titulo, "%s", tit);
            ai = na++;
          }
        }
        if (ai >= 0) {
          cc->motivos[cc->nMotivos].tipo = FMO_TERMINOU;
          cc->motivos[cc->nMotivos].a = ai;
          cc->motivos[cc->nMotivos].b = 0;
          cc->nMotivos++;
        }
      } else if (rm->tipo == FMO_GENERO || rm->tipo == FMO_PESSOA ||
                 rm->tipo == FMO_TEMA || rm->tipo == FMO_FORA_CURVA) {
        if (rm->a <= 0) continue;
        cc->motivos[cc->nMotivos].tipo = rm->tipo;
        cc->motivos[cc->nMotivos].a = rm->a;
        cc->motivos[cc->nMotivos].b = 0;
        cc->nMotivos++;
      } else if (rm->tipo == FMO_SALVO || rm->tipo == FMO_CABE_HOJE || rm->tipo == FMO_REVER) {
        cc->motivos[cc->nMotivos].tipo = rm->tipo;
        cc->motivos[cc->nMotivos].a = (rm->tipo == FMO_CABE_HOJE) ? cc->minutos : 0;
        cc->motivos[cc->nMotivos].b = 0;
        cc->nMotivos++;
      }
    }
    nc++;
  }

  // Sem completar vagas de ancora com sementes: so vai a ancora que um
  // motivo FMO_TERMINOU acima cita (e o corpo ainda descarta a que nenhum
  // candidato efetivamente enviado referencia).

  *nCandOut = nc;
  *nAncOut = na;
  return nc > 0;
}

static int popularFilDeCena(const FarolCena *cp, const FarolComporCand *sc, int nSc,
                            int nCandRef, FarolHomeFil filOut[2]) {
  int p, f = 0, i;
  for (p = 0; p < cp->nPecas && f < 2; p++) {
    const FarolPeca *pc = &cp->pecas[p];
    FarolHomeFil *nf;
    if (pc->tipo != FP_FILEIRA) continue;
    if (strcmp(pc->u.fileira.id, "gosto1") && strcmp(pc->u.fileira.id, "gosto2"))
      continue;
    if (pc->u.fileira.nItens < 6) continue;             // o minimo da P1
    nf = &filOut[f];
    memset(nf, 0, sizeof *nf);
    snprintf(nf->chave, sizeof nf->chave, "farol_%s", pc->u.fileira.id);
    if (!renderTextoExt(cp, &pc->u.fileira.titulo, sc, nSc,
                        nf->titulo, sizeof nf->titulo))
      snprintf(nf->titulo, sizeof nf->titulo, "%s", i18n(FT_FIL_RECOMENDADOS));
    renderTextoExt(cp, &pc->u.fileira.sub, sc, nSc, nf->sub, sizeof nf->sub);
    for (i = 0; i < pc->u.fileira.nItens && i < FAROL_ITENS_MAX; i++) {
      const FarolRef *ref = &pc->u.fileira.itens[i];
      FarolHomeItem *hi = &nf->it[i];
      const char *imdb = NULL, *tipo = "movie";
      char titulo[160] = "";
      // Um so CatItem no escopo do item: `tipo` pode apontar para ci.tipo e e
      // lido depois dos ramos (antes, ci morria no fim do bloco do ramo).
      CatItem ci;
      if (sc && ref->c >= 0 && ref->c < nSc) {
        imdb = sc[ref->c].imdb;
        tipo = sc[ref->c].tipo ? "series" : "movie";
        if (imdb[0] && cat_copiar_por_id(imdb, NULL, &ci) &&
            !strcmp(ci.imdb, imdb) && ci.titulo[0]) {
          snprintf(titulo, sizeof titulo, "%s", ci.titulo);
          if (ci.tipo[0]) tipo = ci.tipo;
        } else if (sc[ref->c].titulo[0]) {
          snprintf(titulo, sizeof titulo, "%s", sc[ref->c].titulo);
        }
      } else if (!sc && ref->c >= 0 && ref->c < nCandRef) { // "c<n>" regras
        imdb = refC[ref->c].imdb; tipo = refC[ref->c].tipo;
        if (refC[ref->c].catIdx >= 0 &&
            cat_copiar_item(refC[ref->c].catIdx, &ci) &&
            !strcmp(ci.imdb, imdb))
          snprintf(titulo, sizeof titulo, "%s", ci.titulo);
        else if (refC[ref->c].titulo[0])
          snprintf(titulo, sizeof titulo, "%s", refC[ref->c].titulo);
      } else if (ref->s[0]) {                               // "tt<digitos>"
        imdb = ref->s;
        if (cat_copiar_por_id(ref->s, NULL, &ci) &&
            !strcmp(ci.imdb, ref->s)) {
          snprintf(titulo, sizeof titulo, "%s", ci.titulo);
          if (ci.tipo[0]) tipo = ci.tipo;
        } else {
          diario_nome(ref->s, titulo, sizeof titulo, NULL, NULL, 0);
        }
      }
      if (!imdb || !imdb[0]) break;                     // itens sao contiguos
      snprintf(hi->imdb, sizeof hi->imdb, "%s", imdb);
      snprintf(hi->tipo, sizeof hi->tipo, "%s", tipo);
      snprintf(hi->titulo, sizeof hi->titulo, "%s", titulo);
      nf->nItens++;
    }
    if (nf->nItens < 6) continue;
    for (i = 0; i < pc->u.fileira.nMotivo; i++) {
      const FarolMotivo *mo = &pc->u.fileira.motivo[i];
      if (mo->item < 0 || mo->item >= nf->nItens) continue;
      renderTextoExt(cp, &mo->t, sc, nSc,
                     nf->it[mo->item].motivo, sizeof nf->it[mo->item].motivo);
    }
    f++;
  }
  return f;
}

// ----------------------------------------------------------------- compor --
// Compoe, valida e ADOTA. Devolve 1 quando uma cena nova foi adotada.
// Falhou em qualquer degrau -> 0 e quem chama decide sobre a antiga (o
// contrato: nunca reaproveitar).
static int compor(long long agora, int perfil) {
  static struct {
    int valida, nFil; FarolHomeFil fil[2];
    long long feita, valeS; int perfil; unsigned revCat; uint32_t hModelo;
    unsigned comporGen;
  } nova;
  FarolGosto g;
  FarolCenaCtx ctx;
  int nSem, nCand, n, rc, f = 0, fimSem;
  int nCompCand = 0, nCompAnc = 0, nLlmCand = 0;
  uint32_t hm, llmHm = 0;

  if (!farolsinal_modelo(&g)) return 0;
  if (farolrec_nivel(&g) < 1) return 0;                 // frio 0: nunca sai
  memset(&nova, 0, sizeof nova);
  hm = hashModelo(&g);
  nSem = coletarSementes(perfil);
  nCand = coletarCandidatos(perfil_infantil_ativo());
  memset(&ctx, 0, sizeof ctx);
  ctx.g = &g;
  ctx.cand = cand; ctx.nCand = nCand;
  ctx.sementes = sem; ctx.nSementes = nSem;
  ctx.sementeRef = semRef;
  ctx.salvoDia = salvoD;
  ctx.dia = (uint16_t)diaDe(agora);
  ctx.faixaHora = (uint8_t)faixaAgoraFim(agora, &fimSem);
  ctx.fimSemana = (uint8_t)fimSem;
  ctx.perfil = perfil;
  ctx.feita = agora;
  ctx.valeS = 86400;                    // a cena da Home vale 24 h (4.2)
  ctx.semente = (uint32_t)ctx.dia * 2654435761u ^ (uint32_t)(perfil + 1);

  if (comporMontarResumo(&ctx, &nCompCand, &nCompAnc)) {
    farolcompor_pedir_home(&g, compCand, nCompCand, compAnc, nCompAnc,
                           comporPessoaLookup, NULL, hm);
  }

  // Preferir a cena LLM ja adotada no fio principal quando valida para o gosto
  // atual; qualquer falha/recusa cai calado para a cena deterministica (44).
  if (farolcompor_cena_adotada(&llmCena, llmCand, &nLlmCand,
                               FAROL_COMPOR_CAND_MAX, &llmHm) &&
      (llmHm == 0 || llmHm == hm)) {
    f = popularFilDeCena(&llmCena, llmCand, nLlmCand, nLlmCand, nova.fil);
    if (f > 0) nova.comporGen = farolcompor_home_geracao();
  }

  if (f <= 0) {
    n = farolcena_home(cenaJson, sizeof cenaJson, &ctx, NULL, 0);
    if (n <= 0) return 0;
    rc = farolpeca_ler(&cenaP, cenaJson, nCand);
    if (rc <= 0) return 0;
    f = popularFilDeCena(&cenaP, NULL, 0, nCand, nova.fil);
  }

  nova.nFil = f;
  if (!f) return 0;
  nova.valida = 1;
  nova.feita = agora;
  nova.valeS = ctx.valeS > 0 ? ctx.valeS : 86400;
  nova.perfil = perfil;
  nova.revCat = cat_revisao();
  nova.hModelo = hm;
  pthread_mutex_lock(&travaCena);
  adotada.valida = nova.valida; adotada.feita = nova.feita;
  adotada.valeS = nova.valeS; adotada.perfil = nova.perfil;
  adotada.revCat = nova.revCat; adotada.hModelo = nova.hModelo;
  adotada.comporGen = nova.comporGen;
  adotada.nFil = nova.nFil;
  memcpy(adotada.fil, nova.fil, sizeof adotada.fil);
  perfilSaiu = -1;                        // a cena do perfil esta valida
  __atomic_add_fetch(&geracaoCena, 1, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&travaCena);
  return 1;
}

static void invalidar(void) {
  pthread_mutex_lock(&travaCena);
  if (adotada.valida) perfilSaiu = adotada.perfil;
  memset(&adotada, 0, sizeof adotada);
  __atomic_add_fetch(&geracaoCena, 1, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&travaCena);
}

// ------------------------------------------------------------------ saida --
int farol_fileira(const char *chave) {
  return chave && (!strcmp(chave, "farol_gosto1") ||
                   !strcmp(chave, "farol_gosto2"));
}

// Duas voltas podem chamar isto de fios ao mesmo tempo (montar cheio e o
// parcial da refacao): sem a trava as duas liam "sem cena", compunham e
// entregavam ao mesmo tempo — cena adotada duas vezes, mostrados marcados
// em dobro e geracao queimada (revisao FAROL45 r1). A trava e so da cola:
// compor pode demorar, mas travaCena (a do motivo/desenho) fica tomada so
// nas copias curtas — o desenho nao espera a cena inteira sair.
static pthread_mutex_t travaCompor = PTHREAD_MUTEX_INITIALIZER;
static void home_fileiras_(CatItem *lote, int *n, int cap,
                           CatFileira *fils, int *nFils, int capFils) {
  static struct {
    int valida, nFil; FarolHomeFil fil[2];
    long long feita, valeS; int perfil, perfilSaiu; uint32_t hModelo;
    unsigned comporGen;
  } snap;
  long long agora;
  int precisa, perfil, f, marcou[2] = { 0, 0 };
  if (!lote || !n || !fils || !nFils) return;
  // O selo vai lido ANTES de copiar a cena: uma leitura depois do bump do
  // zerar so acontece com a chamada ja posterior ao invalidar() — cena
  // velha nunca sai com carimbo novo (FAROL45C).
  const unsigned selo = __atomic_load_n(&seloGosto, __ATOMIC_ACQUIRE);
  // "Nao aparece": recomendacao desligada nesta TV ou modo seguro.
  if (!farolsinal_recomendar() || seguro_perfil_ativo()) { invalidar(); return; }
  agora = agoraS();
  perfil = perfis_ativo();
  pthread_mutex_lock(&travaCena);
  snap.valida = adotada.valida; snap.nFil = adotada.nFil;
  snap.feita = adotada.feita; snap.valeS = adotada.valeS;
  snap.perfil = adotada.perfil; snap.hModelo = adotada.hModelo;
  snap.comporGen = adotada.comporGen;
  snap.perfilSaiu = perfilSaiu;
  memcpy(snap.fil, adotada.fil, sizeof snap.fil);
  pthread_mutex_unlock(&travaCena);
  // Precisa de cena nova? Arranque e troca de perfil recompem sempre; 24 h
  // ou evento forte (modelo mudou) recompem so com a Home fora da tela —
  // cartaz nao troca com a pessoa olhando.
  precisa = !snap.valida || snap.perfil != perfil;
  if (!precisa) {
    FarolGosto g;
    unsigned cGen = farolcompor_home_geracao();
    // Geracao do compositor mudou: cena LLM nova a adotar, ou a projecao
    // LLM anterior (comporGen != 0) perdeu a validade — consentimento
    // revogado, esquecer, troca de perfil — e a deterministica volta.
    if (!farolsinal_modelo(&g) || hashModelo(&g) != snap.hModelo ||
        agora - snap.feita >= snap.valeS ||
        (cGen != snap.comporGen &&
         (snap.comporGen != 0 ||
          farolcompor_cena_adotada(NULL, NULL, NULL, 0, NULL)))) {
      if (!homeVisivel()) precisa = 1;
    }
  } else if (snap.valida && snap.perfil != perfil && homeVisivel()) {
    // A cena adotada e do perfil ANTERIOR: nao emite (vazamento de gosto
    // entre perfis) e nao recompoe com a Home na tela — a proxima volta
    // fora da Home resolve. Na pratica a troca de perfil passa pela tela
    // de escolha, entao a volta seguinte ja compoe.
    memset(&snap, 0, sizeof snap);
    return;
  }
  // Readocao proibida com a Home a vista: a fileira ja saiu de cena (o
  // compositor falhou, ou zerar/opt-out limpou) e trazer uma cena nova de
  // volta troca os cartazes debaixo dos olhos. Vale para o MESMO perfil que
  // perdeu a cena — a primeira cena do perfil novo e a do arranque saem
  // na hora, mesmo com a Home desenhada (revisao FAROL45 r1).
  if (precisa && !snap.valida && snap.perfilSaiu == perfil &&
      snap.perfilSaiu >= 0 && homeVisivel())
    return;
  if (precisa && !compor(agora, perfil)) {
    invalidar();
    memset(&snap, 0, sizeof snap);       // falhou: a antiga nao volta
    return;
  }
  if (precisa) {
    pthread_mutex_lock(&travaCena);
    snap.valida = adotada.valida; snap.nFil = adotada.nFil;
    snap.feita = adotada.feita; snap.valeS = adotada.valeS;
    snap.perfil = adotada.perfil; snap.hModelo = adotada.hModelo;
    snap.comporGen = adotada.comporGen;
    memcpy(snap.fil, adotada.fil, sizeof snap.fil);
    pthread_mutex_unlock(&travaCena);
  }
  if (!snap.valida) return;
  for (f = 0; f < snap.nFil && f < 2; f++) {
    FarolHomeFil *sf = &snap.fil[f];
    CatFileira *cf;
    int i;
    if (sf->nItens < 6) continue;
    // Fileira oculta em Ajustes > Fileiras nao entra (oculta fica oculta).
    if (fil_oculta(sf->chave)) continue;
    if (*nFils >= capFils || *n >= cap) break;
    cf = &fils[*nFils];
    memset(cf, 0, sizeof *cf);
    snprintf(cf->chave, sizeof cf->chave, "%s", sf->chave);
    snprintf(cf->titulo, sizeof cf->titulo, "%s", sf->titulo);
    cf->tipo[0] = 0;                    // mista: nenhum sufixo de tipo
    cf->farolSelo = selo;               // a epoca desta emissao (FAROL45C)
    cf->ini = *n; cf->n = 0;
    for (i = 0; i < sf->nItens && *n < cap; i++) {
      CatItem *dst = &lote[*n];
      int idx = cat_indice_por_imdb(sf->it[i].imdb);
      // O indice sai do catalogo da troca (cat_revisao dentro do indice) e
      // a copia vem conferida pelo imdb — um item nunca entrega meta de
      // outro (revisao FAROL45 r1).
      if (idx >= 0 && cat_copiar_item(idx, dst) &&
          !strcmp(dst->imdb, sf->it[i].imdb)) {
        (*n)++; cf->n++;
      } else {
        // Fora do catalogo: o minimo que abre (busca_pediu_abrir vai pelo
        // imdb) e que nao quebra a fileira. Sem arte e sem meta: o cartaz
        // fica com o titulo, como qualquer item sem capa.
        memset(dst, 0, sizeof *dst);
        snprintf(dst->imdb, sizeof dst->imdb, "%s", sf->it[i].imdb);
        snprintf(dst->tipo, sizeof dst->tipo, "%s", sf->it[i].tipo);
        snprintf(dst->titulo, sizeof dst->titulo, "%s", sf->it[i].titulo);
        (*n)++; cf->n++;
      }
    }
    if (cf->n < 6) { *n = cf->ini; memset(cf, 0, sizeof *cf); continue; }
    (*nFils)++;
    if (!sf->marcada) marcou[f] = 1;
  }
  // Exposicao: uma marcacao por cena adotada e fileira ENTREGUE (o anel de
  // mostrados mede recusa passiva — so conta o que subiu na tela).
  for (f = 0; f < snap.nFil && f < 2; f++) {
    if (!marcou[f]) continue;
    for (int i = 0; i < snap.fil[f].nItens; i++) {
      int idx = cat_indice_por_imdb(snap.fil[f].it[i].imdb);
      CatItem ci;
      if (idx >= 0 && cat_copiar_item(idx, &ci) &&
          !strcmp(ci.imdb, snap.fil[f].it[i].imdb))
        farolsinal_mostrado(&ci);
      else {
        memset(&ci, 0, sizeof ci);
        snprintf(ci.imdb, sizeof ci.imdb, "%s", snap.fil[f].it[i].imdb);
        snprintf(ci.tipo, sizeof ci.tipo, "%s", snap.fil[f].it[i].tipo);
        snprintf(ci.titulo, sizeof ci.titulo, "%s", snap.fil[f].it[i].titulo);
        farolsinal_mostrado(&ci);
      }
    }
    pthread_mutex_lock(&travaCena);
    for (int g2 = 0; g2 < adotada.nFil; g2++)
      if (!strcmp(adotada.fil[g2].chave, snap.fil[f].chave))
        adotada.fil[g2].marcada = 1;
    pthread_mutex_unlock(&travaCena);
  }
}

void farol_home_fileiras(CatItem *lote, int *n, int cap,
                         CatFileira *fils, int *nFils, int capFils) {
  pthread_mutex_lock(&travaCompor);
  home_fileiras_(lote, n, cap, fils, nFils, capFils);
  pthread_mutex_unlock(&travaCompor);
}

const char *farol_motivo(const char *chaveFil, int coluna,
                         const char *imdb) {
  int f;
  const char *r = NULL;
  if (!chaveFil || coluna < 0 || !imdb || !imdb[0]) return NULL;
  pthread_mutex_lock(&travaCena);
  for (f = 0; f < adotada.nFil && f < 2; f++) {
    if (strcmp(adotada.fil[f].chave, chaveFil)) continue;
    // O imdb que a Home entregou na posicao e o imdb da cena adotada? A
    // fileira na tela pode ser de uma cena anterior; conferir antes de
    // atribuir o motivo ao cartaz (revisao FAROL45 r1).
    if (coluna < adotada.fil[f].nItens &&
        !strcmp(adotada.fil[f].it[coluna].imdb, imdb) &&
        adotada.fil[f].it[coluna].motivo[0]) {
      snprintf(motOut, sizeof motOut, "%s", adotada.fil[f].it[coluna].motivo);
      r = motOut;
    }
    break;
  }
  pthread_mutex_unlock(&travaCena);
  return r;
}

// A geracao da cena adotada: cache de motivo por quadro na Home.
unsigned farol_cena_geracao(void) {
  return __atomic_load_n(&geracaoCena, __ATOMIC_ACQUIRE);
}

const char *farol_fileira_titulo(const char *chaveFil) {
  int f;
  const char *r = NULL;
  if (!chaveFil) return NULL;
  pthread_mutex_lock(&travaCena);
  for (f = 0; f < adotada.nFil && f < 2; f++)
    if (!strcmp(adotada.fil[f].chave, chaveFil)) {
      snprintf(titOut, sizeof titOut, "%s", adotada.fil[f].titulo);
      r = titOut;
      break;
    }
  pthread_mutex_unlock(&travaCena);
  return r;
}

// ---------------------------------------------------------------- remocao --
void farol_esquecer(void) {
  invalidar();
  farol_sessao_reiniciar();
  farolcompor_trocar_perfil();
}

// 1 quando a fileira do Farol pode estar publicada: "Recomendar nesta TV"
// ligado e modo seguro fora. A descoberta confere de novo no ponto de
// publicacao e na remontagem sem rede — um opt-out no meio da montagem nao
// republica as fileiras farol_ (revisao FAROL45 r1).
int farol_ativo(void) {
  return farolsinal_recomendar() && !seguro_perfil_ativo();
}

// O CONTRATO: cat_remover primeiro, identidade/estado depois — e a trava da
// cena so e tomada DEPOIS de soltar a do catalogo (ordem de travas unica).
void farol_remover_fileiras(const char *dirArte) {
  cat_remover_fileiras_prefixo(dirArte, CAT_PREFIXO_FAROL);
  invalidar();
}

// "Zerar/apagar o gosto". A REMOCAO das fileiras acontece com a identidade
// atual, antes de qualquer reset de modelo — a regravacao do cache cai no
// perfil novo se a ordem inverter (contrato, item 5). Depois a cena sai de
// cena (a remocao ja invalidou) e o modelo zera pela API do farolsinal:
// sem isso a fileira voltava na proxima volta com o gosto antigo (revisao
// FAROL45 r1).
unsigned farol_selo(void) {
  return __atomic_load_n(&seloGosto, __ATOMIC_ACQUIRE);
}

void farol_gosto_zerar(const char *dirArte) {
  farol_remover_fileiras(dirArte);
  // O bump vem depois da remocao (que ja invalidou a cena): uma emissao
  // inteira dentro do reset carimba o selo velho e e podada na publicacao;
  // a que comeca depois dele ja ve a cena invalidada e recompoe do modelo
  // zerado (FAROL45C).
  __atomic_add_fetch(&seloGosto, 1, __ATOMIC_ACQ_REL);
  farolsinal_zerar_gosto();
  farolcompor_zerar_gosto();
}

// "Recomendar nesta TV": ao DESLIGAR, as fileiras saem primeiro e a
// preferencia e registrada depois; ligar nao toca no catalogo.
void farol_recomendar_definir(int ligado, const char *dirArte) {
  if (!ligado) {
    farol_remover_fileiras(dirArte);
    farolcompor_trocar_perfil();
  }
  farolsinal_definir_recomendar(ligado);
}

// --------------------------------------------------- diagnostico / fachada --
static pthread_mutex_t travaDiag = PTHREAD_MUTEX_INITIALIZER;
static int temVeredito;
static FarolVeredito vereditoSessao, vereditoSaida;
static FarolFatos fatosSessao;
static uint64_t sessaoDesdeSeq;
#ifdef RESOLVER_TESTE
static int usaFatosFixos;
static FarolFatos fatosFixos;
#endif

static FarolPlat platAtual(void) {
#if defined(NV_ANDROID)
  return FP_ANDROID;
#elif defined(NV_TPK)
  return FP_TIZEN_TPK;
#elif defined(__EMSCRIPTEN__)
  return FP_TIZEN_WGT;
#elif defined(NV_DESKTOP_MPV)
  return FP_OUTRA;
#else
  return FP_WEBOS;
#endif
}

static int osMaiorAtual(FarolPlat plat) {
  int m = 0;
  if (plat == FP_TIZEN_TPK) {
#ifdef NV_TPK
    m = video_tpk_tizen_maior();
#endif
    return m > 0 ? m : -1;
  }
  if (plat == FP_WEBOS) {
    m = nv_webos_major();
    return m > 0 ? m : -1;
  }
  return -1;
}

static int evNaJanela(uint32_t agoraMs, uint32_t tMs, uint32_t janelaMs) {
  int32_t d = (int32_t)(agoraMs - tMs);
  return d >= 0 && (uint32_t)d <= janelaMs;
}

static const char *acaoTokenSeguro(int acao) {
  const char *tok = farolregra_acao_token(acao);
  if (tok && tok[0]) return tok;
  if (acao == FAROL_A_ENVIAR_REGISTRO) return "enviar_registro";
  return "x";
}

void farol_fatos_ler(FarolFatos *out) {
  FarolEvento ev[FAROL_ANEL_N];
  ResolverFalha falha;
  FarolSonda sonda;
  char motBuf[RES_FRASE_MAX], recBuf[64];
  const char *vNova, *hdr;
  int i, n, nAud, at, temFalha, fonteAtualDeb = -1;
  uint64_t seqErro;
  int iPronto = -1, iErro = -1, iBusca = -1, iBuscaAnt = -1;
  int falhas = 0, temAbrir = 0, erroNativoPend = 0, reconPend = 0;
  int paradas = 0, recAudioAnel = 0, lentos = 0, prontoAposErro = 0;
  uint64_t seqFim = 0, seqIni = 0, desdeSeq = 0, diagTentativa = 0;
  uint32_t agoraMs;
  long agora;
  const uint32_t ttlMs = (uint32_t)RES_REJEITA_S * 1000u;

  if (!out) return;

#ifdef RESOLVER_TESTE
  {
    int fixo = 0;
    pthread_mutex_lock(&travaDiag);
    if (usaFatosFixos) {
      *out = fatosFixos;
      fixo = 1;
    }
    desdeSeq = sessaoDesdeSeq;
    pthread_mutex_unlock(&travaDiag);
    if (fixo) return;
  }
#else
  pthread_mutex_lock(&travaDiag);
  desdeSeq = sessaoDesdeSeq;
  pthread_mutex_unlock(&travaDiag);
#endif

  farolregra_fatos_iniciar(out);
  agoraMs = farolanel_agora_ms();
  agora = (long)time(NULL);

  out->plat = platAtual();
  out->osMaior = osMaiorAtual(out->plat);
  out->offline = rede_saude_offline() ? 1 : 0;
  out->addonsQtd = addons_n();
  if (out->addonsQtd < 0) out->addonsQtd = 0;
  motBuf[0] = 0;
  if (out->addonsQtd > 0 && !addons_ocupado() &&
      addons_motivo_vazio(motBuf, sizeof motBuf) && motBuf[0])
    out->addonsMotivo = 1;
  out->debridSemPlano = debrid_sem_plano() ? 1 : 0;
  recBuf[0] = 0;
  out->debridRecusa = debrid_recusa(recBuf, sizeof recBuf) ? 1 : 0;
  out->tvNegou = video_registro_negado() ? 1 : 0;

  temFalha = resolverregra_falha_recente(&falha, agora);
  out->haFalha = (temFalha && agora >= falha.agora &&
                  agora - falha.agora <= RES_REJEITA_S) ? 1 : 0;
  out->erroIdadeS = -1;
  out->fonteDebrid = -1;
  // Erro de reproducao e falha do Resolver anteriores a fronteira da sessao
  // (seq do anel em que o erro foi gravado <= sessaoDesdeSeq) sao da tentativa
  // anterior: o reset nao pode reaproveita-los. seqErro 0 = sem erro gravado,
  // e a falha recente solitaria continua contando como no pai.
  seqErro = player_ultimo_erro_seq();
  if (seqErro && seqErro <= desdeSeq) {
    out->haFalha = 0;
  } else if (out->haFalha &&
             player_ultimo_erro_farol(&out->erro, &out->fonteDebrid)) {
    out->erroIdadeS = (int)(agora - falha.agora);
    diagTentativa = player_ultimo_erro_tentativa();
  }

  n = farolanel_ler_seq(ev, FAROL_ANEL_N, 0, &seqFim);
  seqIni = seqFim - (uint64_t)n;
  for (i = 0; i < n; i++) {
    uint64_t seq = seqIni + (uint64_t)i;
    if (seq < desdeSeq) continue;
    if (ev[i].ev == FE_PARADO && evNaJanela(agoraMs, ev[i].t_ms, 5u * 60u * 1000u))
      paradas++;
    if (!evNaJanela(agoraMs, ev[i].t_ms, ttlMs)) continue;
    if (ev[i].ev == FE_PRONTO) iPronto = i;
    else if (ev[i].ev == FE_ERRO_FONTE || ev[i].ev == FE_RECUSA_SINC) iErro = i;
    else if (ev[i].ev == FE_ADDON_BUSCA) { iBuscaAnt = iBusca; iBusca = i; }
    else if (ev[i].ev == FE_AUDIO_RECUSADO) recAudioAnel++;
  }

  for (i = (iPronto >= 0 ? iPronto + 1 : 0); i < n; i++) {
    uint64_t seq = seqIni + (uint64_t)i;
    uint16_t tipo = ev[i].ev;
    if (seq < desdeSeq || !evNaJanela(agoraMs, ev[i].t_ms, ttlMs)) continue;
    if (tipo == FE_ABRIR) {
      if (erroNativoPend && !reconPend) falhas++;
      temAbrir = 1;
      erroNativoPend = 0;
      reconPend = 0;
    } else if (tipo == FE_ERRO_FONTE) {
      falhas++;
      erroNativoPend = 0;
      reconPend = 0;
    } else if (tipo == FE_RECUSA_SINC) {
      int seguidoErroFonte = 0;
      for (int j = i + 1; j < n; j++) {
        uint64_t jSeq = seqIni + (uint64_t)j;
        if (jSeq < desdeSeq || !evNaJanela(agoraMs, ev[j].t_ms, ttlMs)) continue;
        if (ev[j].ev == FE_ABRIR || ev[j].ev == FE_PRONTO) break;
        if (ev[j].ev == FE_ERRO_FONTE) { seguidoErroFonte = 1; break; }
      }
      if (!seguidoErroFonte) {
        falhas++;
        erroNativoPend = 0;
        reconPend = 0;
      }
    } else if (tipo == FE_ERRO_NATIVO || tipo == FE_PIPELINE_MORREU) {
      if (temAbrir) erroNativoPend = 1;
    } else if (tipo == FE_RECONEXAO) {
      if (erroNativoPend) reconPend = 1;
    }
  }
  if (erroNativoPend && !reconPend) falhas++;

  out->falhasSeguidas = falhas;
  out->paradas5min = paradas;
  out->audioRecusado = recAudioAnel + (video_audio_nao_suportado() ? 1 : 0);

  out->faixaDts = -1;
  out->outraFaixa = -1;
  nAud = video_n_audio();
  at = video_audio_atual();
  if (nAud > 0) {
    const VideoFaixa *cur = (at >= 0 && at < nAud) ? video_audio(at) : NULL;
    if (cur) out->faixaDts = video_faixa_dts(cur) > 0 ? 1 : 0;
    else if (video_dts_estado() != FAROL_VIDEO_DTS_NENHUM) out->faixaDts = 1;
    out->outraFaixa = 0;
    for (i = 0; i < nAud; i++) {
      const VideoFaixa *fi;
      if (i == at) continue;
      fi = video_audio(i);
      if (!fi) continue;
      if (out->faixaDts > 0 && video_faixa_dts(fi) > 0) continue;
      out->outraFaixa = 1;
      break;
    }
  } else if (video_dts_estado() != FAROL_VIDEO_DTS_NENHUM) {
    out->faixaDts = 1;
  }

  if (out->plat == FP_WEBOS)
    out->conversaoPossivel = (dts_playback_enabled() &&
                              video_dts_estado() != FAROL_VIDEO_DTS_FALHOU) ? 1 : 0;
  else if (out->plat == FP_TIZEN_TPK)
    out->conversaoPossivel = (dts_tpk_disponivel() &&
                              video_dts_estado() != FAROL_VIDEO_DTS_FALHOU) ? 1 : 0;
  else
    out->conversaoPossivel = -1;

  // A fonte atual entra em variavel separada: a origem do erro do player
  // (out->fonteDebrid, 0/1) tem precedencia, e a fonte atual so completa
  // quando o erro nao marca debrid — a mesma precedencia do Resolver do pai
  // (resolver.c: stream_item(stream_atual()) so quando fonteDebrid < 0).
  stream_farol_fatos(&out->fonteDv, &out->fonteHdr, &out->fonteGBx10,
                     &out->fonteMbps, &fonteAtualDeb, &out->temOutraFonte,
                     &out->temFonteMenor, &out->temFonteSemDv,
                     &out->temFonteSemDts, &out->tvHdr, &out->tvDv,
                     &out->vazaoMbps);
  if (out->fonteDebrid < 0) out->fonteDebrid = fonteAtualDeb;
  if (out->fonteDv < 0 && video_tem_dolby_vision() > 0) out->fonteDv = 1;
  if (out->fonteHdr < 0) {
    hdr = video_hdr();
    if (hdr && hdr[0] && strcmp(hdr, "none") && strcmp(hdr, "desconhecido") &&
        strcmp(hdr, "unknown"))
      out->fonteHdr = 1;
  }

  out->memLivreMB = (int)memlog_livre_mb();
  out->sondaRange = -1;
  if (farolsonda_estado(&sonda) && !sonda.cancelada && sonda.http > 0)
    out->sondaRange = sonda.http;
  vNova = atualizacao_nova();
  out->versaoNova = (vNova && vNova[0]) ? 1 : 0;

  out->addonsLentos = -1;
  out->addonsMudos = -1;
  if (iBusca >= 0) {
    out->addonsMudos = ev[iBusca].b >= 0 ? ev[iBusca].b : 0;
    for (i = (iBuscaAnt >= 0 ? iBuscaAnt + 1 : 0); i < iBusca; i++) {
      uint64_t seq = seqIni + (uint64_t)i;
      if (seq < desdeSeq || !evNaJanela(agoraMs, ev[i].t_ms, ttlMs)) continue;
      if (ev[i].ev == FE_ADDON_LENTO && !ev[i].b) lentos++;
    }
    out->addonsLentos = lentos;
  }

  prontoAposErro = (iPronto >= 0 && (iErro < 0 || iPronto > iErro));
#ifdef NV_ANDROID
  out->externoDisponivel = (!prontoAposErro && diagTentativa &&
                            player_externo_disponivel_tentativa(diagTentativa)) ? 1 : 0;
#else
  (void)prontoAposErro;
  (void)diagTentativa;
  out->externoDisponivel = 0;
#endif
}

int farol_julgar(int sintoma) {
  FarolFatos fatos;
  FarolVeredito v;
  int cod;
  farol_fatos_ler(&fatos);
  fatos.sintoma = sintoma;
  farolregra_julgar(sintoma, &fatos, &v);
  cod = v.regra ? (int)v.regra->codigo : FAROL_C_NADA_ACHADO;
  pthread_mutex_lock(&travaDiag);
  fatosSessao = fatos;
  vereditoSessao = v;
  temVeredito = 1;
  pthread_mutex_unlock(&travaDiag);
  farol_ev(FM_FAROL, FE_FAROL_VEREDITO, 0, cod, sintoma,
           farolregra_codigo_nome(cod));
  return cod;
}

const FarolVeredito *farol_veredito(void) {
  const FarolVeredito *r = NULL;
  pthread_mutex_lock(&travaDiag);
  if (temVeredito) {
    vereditoSaida = vereditoSessao;
    r = &vereditoSaida;
  }
  pthread_mutex_unlock(&travaDiag);
  return r;
}

int farol_veredito_copiar(FarolVeredito *veredito, FarolFatos *fatos) {
  int ok;
  pthread_mutex_lock(&travaDiag);
  ok = temVeredito;
  if (ok) {
    if (veredito) *veredito = vereditoSessao;
    if (fatos) *fatos = fatosSessao;
  }
  pthread_mutex_unlock(&travaDiag);
  return ok;
}

void farol_registrar_acao(int acao) {
  farol_ev(FM_FAROL, FE_FAROL_ACAO, 0, acao, 0, acaoTokenSeguro(acao));
}

void farol_registrar_resultado(int acao, int ok) {
  farol_ev(FM_FAROL, FE_FAROL_RESULTADO, ok ? 0 : 1, acao, ok ? 1 : 0,
           acaoTokenSeguro(acao));
}

void farol_registrar_envio(int http) {
  int ok = http >= 200 && http < 300;
  farol_ev(FM_FAROL, FE_FAROL_ENVIO, ok ? 0 : 2, http, 0, ok ? "ok" : "erro");
}

void farol_registrar_consertou(int acao) {
  farol_ev(FM_FAROL, FE_FAROL_CONSERTOU, 0, acao, 1, acaoTokenSeguro(acao));
}

int farol_eventos(FarolEvento *dst, int max) {
  if (!dst || max <= 0) return 0;
  return farolanel_ler_ultimos(dst, max > FAROL_ANEL_N ? FAROL_ANEL_N : max, 0);
}

void farol_sessao_reiniciar(void) {
  uint64_t seq = farolanel_seq();
  pthread_mutex_lock(&travaDiag);
  temVeredito = 0;
  memset(&vereditoSessao, 0, sizeof vereditoSessao);
  memset(&vereditoSaida, 0, sizeof vereditoSaida);
  memset(&fatosSessao, 0, sizeof fatosSessao);
  sessaoDesdeSeq = seq;
  pthread_mutex_unlock(&travaDiag);
}

#ifdef RESOLVER_TESTE
void farol_teste_fatos(const FarolFatos *f) {
  pthread_mutex_lock(&travaDiag);
  if (f) {
    fatosFixos = *f;
    usaFatosFixos = 1;
  } else {
    usaFatosFixos = 0;
    memset(&fatosFixos, 0, sizeof fatosFixos);
  }
  pthread_mutex_unlock(&travaDiag);
}
#endif
