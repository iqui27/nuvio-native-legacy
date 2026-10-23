"""Compile actual app identity selection and player scrobble gates in isolation."""
from pathlib import Path
import subprocess
import sys
import tempfile

app = Path('src/app.c').read_text()
player = Path('src/player.c').read_text()
# Registration must follow the synchronous player open, which clears CDN identity.
selected_play = app.split('static void parteTocar(', 1)[1].split('static void abrirParte(', 1)[0]
assert selected_play.index('definirFonteVOD(url)') < selected_play.index('video_tpk_cdn_arquivo('), 'selected-file CDN identity registered before player open'
selection = app.split('static void abrirParte(', 1)[1].split('  torrentSessao++;', 1)[0]
resume = app.split('    if (parteFase == PF_RETOMAR) {\n', 1)[1].split('    parteTocar(url, partePos);', 1)[0].rsplit('    }', 1)[0]
start = player.split('{ int ok = fontePrincipal()', 1)[1].split(';   // PRONTO', 1)[0]
pause = player.split('if (tocouS > 0.0f && cs', 1)[1].split(' {', 1)[0]
# Keep the actual duration gate, with harmless preceding predicates removed.
pause = pause[pause.index('duracaoSeg >='):].rstrip().removesuffix(')')
eof = player.split('    int noFim = ', 1)[1].split('\n  }\n  // Com o painel', 1)[0]
duration = player.split('int player_parece_parte(void)', 1)[1].split('// O titulo do aviso', 1)[0]
countdown = player.split('static void durFonteContagem(', 1)[1].split('static void durFonteAtualizar(', 1)[0]
# Compile the real confirmation hook when present; baseline has no cancellation.
confirm = player.split('void player_confirmar_partes(void) {', 1)[1].split('\n}', 1)[0] if 'void player_confirmar_partes(void) {' in player else ''
automatic = app.split('    { const Stream *fs = stream_atual()', 1)[1].split('        if (!partes_lista_copiar(&parteArqLista, &l))', 1)[0]
confirmation = app.split('        partes_sessao_iniciar(alvo, &l, l.atual);', 1)[1].split('        if (!partes_lista_copiar(&parteArqLista, &l))', 1)[0]
closing = player.split('  double cred = credEfetivo();\n  int duracaoContradita', 1)[1]
closing = '  double cred = credEfetivo();\n  int duracaoContradita' + closing.split('  // Concluido e concluido:', 1)[0]
trakt_close = next(line.strip() for line in player.splitlines() if 'if (!duracaoContradita) trakt_marcar(' in line)
def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

next_part = app.split('    if (parteProx != prox)', 1)[1].split('    // parteProxFase == 2:', 1)[0]
next_part = '    if (parteProx != prox)' + next_part
rules = '\n'.join(function(player, signature) for signature in (
    'int player_regra_proximo(double', 'int player_regra_visto(double', 'int player_regra_concluiu(double'))
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned Uint32;
#define main partes_existing_tests
#include "tests/partes.c"
#undef main
typedef struct { char tipo[16]; char titulo[64]; char imdb[24]; } CatItem;
static CatItem ci = {.tipo="series", .imdb="tt-test"};
static int catalogAvailable = 1;
static Stream source, voltaFonte;
static int sourceCount=1, sourceIndex;
static int aguardandoFonte, playing, gotT, gotE;
const Stream *stream_item(int n) { return n>=0 && n<sourceCount ? &source : NULL; }
''' + function(Path('src/streams.c').read_text(), 'void stream_definir_atual(int').replace('atual =', 'sourceIndex =').replace('i < n', 'i < sourceCount') + r'''

static const CatItem *cat_item(int n) { (void)n; return catalogAvailable ? &ci : NULL; }
static int player_aberto(void) { return playing; }
static int player_indice(void) { return 0; }
static int detail_indice(void) { return 0; }
static int currentT=1, currentE=1;
static void player_episodio_atual(int *t,int *e) { *t=currentT; *e=currentE; }
static void detail_ep_foco(int *t,int *e) { *t=1; *e=1; }
static void player_encerrar(void) {}
static void limparFonteVOD(void) { memset(&voltaFonte,0,sizeof voltaFonte); }
static void parteZerar(void) {}
static void consumirFolhaTocar(int *t,int *e) { *t=1; *e=1; }
static void diagnostico_cancelar_vazao(void) {}
static void player_abrir(int n,void *p) { (void)n; (void)p; }
// Execute the production episode-list invalidation gate, rather than a recording-only setter.
static int abrindoSessao, aberto=1;
int stream_n(void) { return sourceCount; }
int stream_lista_do_alvo(const char *a) { (void)a; return gotT==currentT && gotE==currentE; }
static int idxAtual(void);
static void cat_id_stream(int i,int t,int e,char *a,size_t n) { (void)i; snprintf(a,n,"tt-test:%d:%d",t,e); }
void stream_invalidar(const char *why) { (void)why; sourceCount=0; sourceIndex=-1; memset(&source,0,sizeof source); }
static void player_definir_episodio(int t,int e) {
  gotT=t; gotE=e;
  CatItem *c=&ci;
  int epT=t, epE=e;
''' + '  if (c->imdb[0]) {' + function(player, 'void player_definir_episodio(int').split('  if (c->imdb[0]) {',1)[1].split('  // T/E vira',1)[0] + r'''
}
static void selectedFile(''' + selection + r'''
}
static void selectFile(int source, const PartesLista *l, int k) {
  sourceCount=1;
  selectedFile(source,l,k);
}
static PartesLista parteArqLista;
static int parteArqK, parteMp4;
static void alvoPlayer(char *alvo, size_t n) { snprintf(alvo,n,"tt-resume"); }
static void parteRecuar(const char *reason) { (void)reason; assert(0); }
static int nomeMp4(const char *name) { (void)name; return 0; }
static void resumeFile(PartesLista l) {
  char alvo[64];
''' + resume + r'''
}
static int fontePrincipal(void) { return 1; }
static int video_pronto(void) { return 1; }
static int video_ativo(void) { return 1; }
static int ehCanal(void) { return 0; }
static int partes;
static int partesAtiva(void) { return partes; }
static double duracaoCatalogo(void) { return 7200; }
static int player_duracao_suspeita(double d,double c) { return d < c/2; }
static int ended = 1, hasNext;
static double clockEof;
static int video_terminou(void) { return ended; }
static double video_pos(void) { return 1800; }
static double monoSeg(void) { return clockEof; }
static int desc_episodios_carregando(int i) { (void)i; return 0; }
static int idxAtual(void) { return 0; }
static int player_proximo_episodio(void) { return hasNext; }
static int checkEof(int meio, int eSerie) {
  int saindo=0, pediuSair=0, scrubbing=0, retomandoSalto=0, aguardaBusca=0;
  double duracaoSeg=1800, posSeg=1800, eofDesde=-1;
  int noFim = ''' + eof + r'''
  return saindo || pediuSair;
}
static Uint32 ticks, durFonteAcaoAte, toastAte, durFonteSkipEm;
static int durFonteCurta, durFonteManter, pedProximaFonte, saindo;
Uint32 SDL_GetTicks(void) { return ticks; }
int player_parece_parte(void)''' + duration + r'''
void player_confirmar_partes(void) {''' + confirm + r'''
}
static void durFonteContagem(''' + countdown + r'''
static void confirmParts(void) {
  const char *alvo="tt-duration";
''' + confirmation + r'''
}
static int parteAutoEsperando;
static char parteAutoUrl[4096];
static PartesLista automaticPack;
int stream_atual(void) { return 0; }
static int video_falhou(void) { return 0; }
static double video_duracao(void) { return 1200; }
static const char *video_url_atual(void) { return "https://fixture/part1.mkv"; }
static void requestParts(const Stream *s, const char *title) { (void)s; (void)title; }
static int resultParts(const Stream *s, PartesLista *l) { (void)s; *l=automaticPack; return PARTES_PRONTO; }
#define debrid_tb_disponivel() 1
#define partes_pedir requestParts
#define partes_resultado_de resultParts
static void detectParts(void) {
  char alvo[64];
  PartesLista l;
  { const Stream *fs = stream_atual()''' + automatic + r'''
      }
  }
}
#undef debrid_tb_disponivel
#undef partes_pedir
#undef partes_resultado_de
static int parteProx=-1, parteProxFase, requests, openingErrors, linkResult;
static Uint32 parteProxDesde;
static char parteProxUrl[4096];
static void requestNext(int u,int item,int file,int mode) {
  (void)u; (void)item; (void)file; (void)mode; requests++;
}
static int nextResult(char *url,size_t n,PartesLista *l) {
  (void)url; (void)n; (void)l; return linkResult;
}
static void parteFalhou(const char *reason) { (void)reason; openingErrors++; }
#define partes_link_pedir requestNext
#define partes_link_estado nextResult
static void watchNext(void) {
  PartesLista list={.n=2,.arq=(ParteArq[2]){{0}}};
  const PartesLista *sl=&list;
  int prox=1,r;
  double d=1800,pos=1800;
''' + next_part + r'''
}
#undef partes_link_pedir
#undef partes_link_estado
#include <math.h>
static double intro_creditos_janela(double d) { return fmin(180.0, d*.2); }
static int scrobble_concluiu(double p,double d,int fim) { return fim || (d>1 && p>=d*.9); }
static double credJanelaDe(double d) { return intro_creditos_janela(d); }
static double intro_fim_estimado(double d) { return d>=120 ? 50 : 0; }
''' + rules + r'''
static double posSeg, duracaoSeg, assistidoSeg, testCredits, virtualPos, virtualDur;
static int jaRetido, comVideo=1, finalPart, forgot, saved, marked, watched;
static double chapterCredits;
static double video_creditos(void) { return chapterCredits; }
static double credEfetivo(void) { return testCredits; }
static int concluiuAgora(double cred) { return player_regra_concluiu(posSeg,duracaoSeg,cred); }
static int partesVirt(double *p,double *d,int *u) { *p=virtualPos; *d=virtualDur; *u=finalPart; return partes; }
static void alvoStream(char *s,size_t n) { snprintf(s,n,"tt-completion"); }
static int sessao_usuario(void) { return 0; }
static int perfis_ativo(void) { return 0; }
static int testArquivo(const char *a,PartesPonto *p) { (void)a; memset(p,0,sizeof *p); return 1; }
static void testForget(const char *a,int u,int p) { (void)a;(void)u;(void)p; forgot++; }
static void testSave(const char *a,int u,int p,const PartesPonto *pt) { (void)a;(void)u;(void)p;(void)pt; saved++; }
static void trakt_marcar(const char *id,double p,double d,double s,int done) { (void)id;(void)s; marked++; watched=done; if(done) assert(p==d && d==5760); }
#define partes_arquivo_de testArquivo
#define partes_ponto_esquecer testForget
#define partes_ponto_guardar testSave
static int closeMovie(void) {
  double base=0, baseDur=0, basePos=0, basePct=0;
  int chegou=0;
''' + closing + r'''
  double pos=assistiu ? durG : posG;
  const char *id="tt-completion";
  ''' + trakt_close + r'''
  return assistiu;
}
#undef partes_arquivo_de
#undef partes_ponto_esquecer
#undef partes_ponto_guardar
int main(void) {
  int failed=0;
  for (int early=0;early<2;early++) {
    parteProx=-1; requests=openingErrors=0; ticks=100; ended=!early; linkResult=-1;
    watchNext();
    ended=1;
    for (int i=0;i<10 && !openingErrors;i++) { ticks+=100; watchNext(); }
    if (!openingErrors || requests>2) {
      puts("FAIL next-part immediate errors must stop after at most one EOF retry"); failed=1;
    } else puts("PASS next-part immediate errors stop with file-opening error");
  }
  parteProx=-1; requests=openingErrors=0; ticks=100; ended=1; linkResult=0;
  watchNext(); ticks+=30001; watchNext();
  assert(openingErrors==1 && requests==1);
  puts("PASS next-part pending request times out after 30 seconds");
  PartesLista completionMovie = {.n=4,.atual=3,.arq=(ParteArq[]){
    {.id=1,.tipo=PARTE_EP}, {.id=2,.tipo=PARTE_EP},
    {.id=3,.tipo=PARTE_EP}, {.id=4,.tipo=PARTE_EP}}};
  partes_sessao_iniciar("tt-completion", &completionMovie, 3);
  partes=1; finalPart=1; posSeg=duracaoSeg=1440; assistidoSeg=1440;
  virtualPos=virtualDur=5760; testCredits=5520;
  if (!closeMovie() || forgot!=1 || saved || marked!=1 || !watched) {
    puts("FAIL final multipart EOF with whole-movie credits must mark watched, clear resume and close Trakt"); failed=1;
  } else puts("PASS final multipart EOF marks watched, clears resume and closes Trakt");
  finalPart=0; virtualPos=1440; forgot=saved=marked=watched=0;
  assert(!closeMovie() && !forgot && saved==1 && !watched);
  finalPart=1; virtualPos=5760; chapterCredits=testCredits=1800;
  forgot=saved=marked=watched=0;
  assert(!closeMovie() && !forgot && saved==1 && !marked);
  chapterCredits=0; testCredits=6000;
  forgot=saved=marked=watched=0;
  assert(!closeMovie() && !forgot && saved==1 && !marked);
  partes_sessao_parar("completion done");
  partes=0;

  // A 40-minute part of a 130-minute movie arms rejection at t=1 s.
  PartesLista movie = {.n=3,.atual=0,.arq=(ParteArq[]){
    {.id=1,.tipo=PARTE_EP}, {.id=2,.tipo=PARTE_EP}, {.id=3,.tipo=PARTE_EP}}};
  for (int expired=0; expired<2; expired++) {
    partes_sessao_parar("reset duration case");
    durFonteSkipEm=durFonteAcaoAte=toastAte=9000;
    durFonteManter=1; pedProximaFonte=0; ticks=3000;
    if (expired) durFonteContagem(9000); // queued rejection before app confirmation
    partes_sessao_iniciar("tt-duration", &movie, 0);
    confirmParts();
    durFonteContagem(10000);
    if (player_pediu_proxima_fonte() || durFonteSkipEm || durFonteManter || player_aviso_duracao_ativo()) {
      puts("FAIL confirmed multipart retains duration rejection"); failed=1;
    }
  }
  partes_sessao_parar("duration done");
  durFonteSkipEm=9000; pedProximaFonte=0; durFonteContagem(10000);
  assert(player_pediu_proxima_fonte()==2); // unconfirmed short sources still skip
  if (!failed) puts("PASS multipart confirmation cancels pending and queued rejection");
  /* EOF stays latched while prefetch waits or retries, then final EOF exits. */
  int eofFailed=0;
  for (int retry=0;retry<2;retry++) {
    for (clockEof=0;clockEof<=30;clockEof+=10)
      if (checkEof(1,0)) eofFailed=1;
  }
  if (eofFailed) puts("FAIL intermediate multipart EOF exits while next link waits/retries");
  else puts("PASS intermediate multipart EOF waits for transition");
  failed |= eofFailed;
  assert(checkEof(0,0));
  hasNext=1; assert(!checkEof(0,1)); hasNext=0;
  static PartesLista pack;
  assert(partes_ler_item("{\"id\":7,\"files\":[{\"id\":1,\"name\":\"Show.S00E02.mkv\",\"size\":300000000}]}",NULL,0,&pack));
  assert(pack.arq[0].temporada==0 && pack.arq[0].episodio==2);
  assert(pack.arq[0].tipo==PARTE_EP && partes_principais(&pack)==1);
  static PartesLista specials;
  assert(partes_ler_item("{\"id\":9,\"files\":[{\"id\":1,\"name\":\"Show.S00E01.mkv\",\"size\":300000000},{\"id\":2,\"name\":\"Show.S00E02.mkv\",\"size\":300000000}]}",NULL,0,&specials));
  specials.atual=0;
  assert(partes_principais(&specials)==2);
  char hashes[1][41]={H1};
  const char *unknownMovie="{\"data\":[{\"id\":7,\"hash\":\"" H1 "\",\"files\":[{\"id\":1,\"name\":\"Movie.CD1.mkv\",\"size\":300},{\"id\":2,\"name\":\"Movie.CD2.mkv\",\"size\":400},{\"id\":3,\"name\":\"Movie.CD3.mkv\",\"size\":350}]}]}";
  for (int mismatch=0;mismatch<2;mismatch++) {
    assert(partes_achar(unknownMovie,0,hashes,1,1,"",mismatch ? 999 : 0,&automaticPack)==1);
    if (automaticPack.atual!=-1) {
      puts("FAIL hash-only multipart identity must remain unknown"); failed=1;
    } else puts("PASS hash-only multipart identity remains unknown");
    strcpy(ci.tipo,"movie"); strcpy(source.url,"https://fixture/part1.mkv");
    playing=1; durFonteCurta=1; currentT=currentE=0;
    parteAutoEsperando=0; parteAutoUrl[0]=0;
    partes_sessao_parar("unknown current file");
    detectParts();
    if (partes_sessao_lista()) {
      puts("FAIL unknown current file starts automatic multipart playback"); failed=1;
    } else puts("PASS unknown current file blocks automatic multipart playback");
  }
  assert(partes_achar(unknownMovie,0,hashes,1,1,"Movie.CD1.mkv",0,&automaticPack)==1 && automaticPack.atual==0);
  assert(partes_achar(unknownMovie,0,hashes,1,1,"",350,&automaticPack)==1 && automaticPack.atual==2);
  automaticPack=specials;
  strcpy(source.url,"https://fixture/part1.mkv");
  playing=1; durFonteCurta=1;
  for (int scenario=0;scenario<6;scenario++) {
    int movie=scenario==5;
    catalogAvailable=scenario!=4;
    strcpy(ci.tipo,movie ? "movie" : "series");
    currentT=scenario==1 ? 1 : scenario==2 ? -1 : 0;
    currentE=scenario<2 ? 1 : scenario==2 ? -1 : 0;
    parteAutoEsperando=0; parteAutoUrl[0]=0;
    partes_sessao_parar("automatic detection test");
    detectParts();
    if (!!partes_sessao_lista()!=movie) {
      puts("FAIL automatic detection chains series specials or rejects a movie"); failed=1;
    }
  }
  puts("PASS automatic detection accepts only catalog movies");
  partes_sessao_parar("automatic detection done");
  catalogAvailable=1;
  strcpy(ci.tipo,"series");
  for (int season=-1;season<=1;season++) {
    currentT=season; currentE=season<0 ? -1 : 1;
    partes_sessao_parar("resume test");
    resumeFile(specials);
    if (partes_sessao_lista()) {
      puts("FAIL series Resume starts multipart session"); failed=1;
    }
    assert(parteArqK==0 && parteArqLista.arq[0].id==1);
  }
  currentT=currentE=1;
  partes_sessao_parar("resume test");
  strcpy(ci.tipo,"movie"); resumeFile(specials);
  assert(!partes_sessao_lista());
  currentT=currentE=0; resumeFile(specials);
  assert(partes_sessao_lista());
  partes_sessao_parar("resume test done");
  strcpy(ci.tipo,"series"); currentT=currentE=1;
  for (playing=0;playing<2;playing++) {
    selectFile(0,&pack,0);
    if (gotT!=0 || gotE!=2) { puts("FAIL specials-only pack selects S01E02 instead of S00E02"); failed=1; }
  }
  assert(partes_ler_item("{\"id\":8,\"files\":[{\"id\":1,\"name\":\"Movie.CD1.mkv\",\"size\":300},{\"id\":2,\"name\":\"Movie.CD2.mkv\",\"size\":300}]}",NULL,0,&pack));
  assert(partes_principais(&pack)==2 && partes_seguinte(&pack,0)==1);
  assert(!pack.arq[0].temporadaZero);
  PartesLista l = {.n=3,.atual=0,.arq=(ParteArq[]){
    {.temporada=1,.episodio=1,.tipo=PARTE_EP},
    {.temporada=1,.episodio=3,.tipo=PARTE_EP},
    {.tipo=PARTE_ESPECIAL,.extra=1}}};
  for (playing=0;playing<2;playing++) {
    sourceCount=1;
    strcpy(source.url,"https://fixture/release");
    strcpy(source.bingeGroup,"release-a");
    selectFile(0,&l,1);
    const Stream *s;
    s = stream_item(sourceIndex);
''' + function(app, 'static void lembrarTocando(void)').split('  s = stream_item(stream_atual());',1)[1].split('  if (!s) return;',1)[0] + r'''
    const Stream *selected=s;
    if (!selected || strcmp(selected->bingeGroup,"release-a")) {
      puts("FAIL selected release identity lost after episode-list invalidation"); return 1;
    }
    puts("PASS selected release survives production episode-list invalidation");
    sourceCount=1;
    if (gotT!=1 || gotE!=3) { puts("FAIL selected S01E03 inherits S01E01"); return 1; }
    selectFile(0,&l,2);
    if (gotT!=-1 || gotE!=-1) { puts("FAIL unnumbered OVA inherits episode"); return 1; }
    l.arq[2].episodio=2;
    selectFile(0,&l,2);
    assert(gotT==0 && gotE==2);
    l.arq[2].episodio=0;
    l.arq[1].temporada=0; selectFile(0,&l,1); assert(gotT==1 && gotE==3); l.arq[1].temporada=1;
  }
  strcpy(ci.tipo,"movie"); selectFile(0,&l,1); assert(gotT==1 && gotE==1);
  float duracaoSeg=1800; int tocando=1,scrubbing=0,saindo=0,erroFonte=0,esperandoFonte=0;
  for (partes=0;partes<2;partes++) {
    int start = fontePrincipal()''' + start + r''';
    int pause = ''' + pause + r''';
    if (start!=partes || pause!=partes) { puts("FAIL multipart progress/start/pause gate"); return 1; }
  }
  puts("PASS selected identity and multipart start/pause gates");
  return failed;
}
'''
# Run identity and gate cases independently so baseline exposes both findings.
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp)/'review.c'
    src.write_text(code)
    includes = ['-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2'] if sys.platform == 'darwin' else subprocess.check_output(['pkg-config', '--cflags', 'sdl2'], text=True).split()
    subprocess.run(['cc', '-w', '-I.', '-Isrc', *includes, str(src), 'src/partes.c', 'src/js.c',
                    '-lpthread', '-lm', '-o', tmp+'/review'], check=True)
    result = subprocess.run([tmp+'/review'])
    # The explicit unknown sentinel must survive the player's series default.
    default = player.split('// 0/0 e sem episodio;', 1)[1].split('// AS FONTES', 1)[0]
    unknown_guard = 't < 0 && e < 0' in player or 't == -1 && e == -1' in player
    print('PASS' if unknown_guard else 'FAIL', 'player preserves unknown file identity')
    raise SystemExit(result.returncode or (not unknown_guard))
