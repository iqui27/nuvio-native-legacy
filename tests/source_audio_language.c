#include "../src/streams.c"
#include <assert.h>

static void addon(const char *fields,const char *pref,int expected) {
  char json[12000];snprintf(json,sizeof json,"{\"streams\":[{\"url\":\"https://example.test/video\",%s}]}",fields);
  Stream *v=NULL;assert(stream_extrair(json,"Turkish provider name is not audio",&v)==1);
  ling_local_audio(pref);lista=v;n=1;soDub=1;
  int result=passaChips(0);
  if(result!=expected)fprintf(stderr,"Audio fallback mismatch: %s, language=%s, expected=%d, hints=%s, declared=%s\n",fields,pref,expected,v[0].audioRotulos,v[0].audioIdiomas);
  assert(result==expected);
  free(v);lista=NULL;n=0;
}
int main(void) {
  Stream s = {0};
  ling_local_audio("pl");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "Polish, eng");
  lista = &s; n = 1; soDub = 1;
  assert(passaChips(0) && "Polish preference must accept Polish audio");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "Portuguese");
  snprintf(s.legendaIdiomas, sizeof s.legendaIdiomas, "Polish");
  assert(!passaChips(0) && "Polish subtitles must not count as Polish audio");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "pol");
  assert(passaChips(0));
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "pl-PL");
  assert(passaChips(0));
  ling_local_audio("en");
  assert(!passaChips(0));
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "English");
  assert(passaChips(0));
  ling_local_audio("pt");
  assert(!passaChips(0));
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "pt-BR");
  assert(passaChips(0));
  s.audioIdiomas[0] = s.legendaIdiomas[0] = 0;
  snprintf(s.rotulo, sizeof s.rotulo, "Dublado");
  assert(!passaChips(0) && "Unspecified dubbing is not declared audio");
  ling_local_audio("*");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "Polish");
  assert(passaChips(0) && "All audio languages leaves the filter open");
  ling_local_audio("~"); ling_definir_original("pl");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "Polish");
  assert(passaChips(0));
  const char *turkish[]={
    "\"title\":\"Movie.2026.1080p.TR.DUAL.mkv\"",
    "\"title\":\"Movie.1080p.TUR.mkv\"",
    "\"title\":\"Movie [Turkish] 1080p\"",
    "\"description\":\"Movie 1080p / Türkçe Dublaj\"",
    "\"title\":\"Movie Dublaj\"",
    "\"title\":\"🇹🇷 1080p\"",
    "\"title\":\"Movie🇹🇷\"",
    "\"title\":\"1080p🇹🇷🇬🇧\"",
    "\"title\":\"1080p\\ud83c\\uddf9\\ud83c\\uddf7\"",
    "\"name\":\"AIOStreams 🇹🇷\"",
    "\"filename\":\"Movie.turkce.1080p.mkv\"",
    "\"behaviorHints\":{\"filename\":\"Movie.1080p.tr.mkv\"}",
    "\"clientResolve\":{\"stream\":{\"raw\":{\"filename\":\"Movie.1080p.TUR.mkv\"}}}"
  };
  for(size_t i=0;i<sizeof turkish/sizeof *turkish;i++) {
    addon(turkish[i],"tr",1);addon(turkish[i],"pl",0);
  }
  const char *subtitles[]={
    "\"title\":\"Subtitles: Turkish\"",
    "\"title\":\"Turkish subtitles\"",
    "\"title\":\"TR SUB\"",
    "\"title\":\"Movie.1080p.TUR.ENG.SUBS.mkv\"",
    "\"title\":\"📝 🇹🇷\"",
    "\"title\":\"Multi Subs / 🇹🇷\"",
    "\"title\":\"Altyazı: TR\"",
    "\"title\":\"Subtitles: English | Turkish\"",
    "\"title\":\"Subtitles: English; Turkish\"",
    "\"title\":\"Subtitles:\\nTR\"",
    "\"title\":\"📝 TR | Turkish\"",
    "\"title\":\"📝 English 💾 1 GB | Turkish\"",
    "\"subtitleLanguages\":[\"tr\"]",
    "\"subtitles\":[{\"lang\":\"tr\",\"url\":\"https://example.test/tr.srt\"}]"
  };
  for(size_t i=0;i<sizeof subtitles/sizeof *subtitles;i++)addon(subtitles[i],"tr",0);
  addon("\"title\":\"🇹🇷 / 🇬🇧 Subs / 🇫🇷\"","tr",1);
  addon("\"title\":\"🇹🇷 / 🇬🇧 Subs / 🇫🇷\"","en",1);
  addon("\"title\":\"🇹🇷 / 🇬🇧 Subs / 🇫🇷\"","fr",0);
  addon("\"title\":\"📝 Turkish\\nAudio: English\"","en",1);
  addon("\"title\":\"📝 English\\n🌐 🇹🇷\"","tr",1);
  // Structured metadata remains authoritative; hints never invent tracks.
  addon("\"audioLanguages\":[\"en\"],\"title\":\"🇹🇷 TUR Dublaj\"","tr",0);
  addon("\"audioLanguages\":[\"tr\"],\"title\":\"English\"","tr",1);
  addon("\"title\":\"English\",\"description\":\"Audio: Turkish\"","en",0);
  const char *languages[]={"PL","Polish","Polski","🇵🇱","German","Deutsch","🇩🇪","French","VFF","🇫🇷","Spanish","🇲🇽","English","🇺🇸","Japanese","🇯🇵","ESP","Latino","LATAM","💃🏻","CHS","CHT","JP"};
  const char *codes[]={"pl","pl","pl","pl","de","de","de","fr","fr","fr","es","es","en","en","ja","ja","es","es","es","es","zh","zh","ja"};
  for(size_t i=0;i<sizeof languages/sizeof *languages;i++) {
    char field[256];snprintf(field,sizeof field,"\"title\":\"Movie [ %s ] 1080p\"",languages[i]);addon(field,codes[i],1);
  }
  addon("\"title\":\"Dual Audio Multi Dubbed\"","tr",0);
  addon("\"title\":\"Dublado\"","pt",0);
  addon("\"title\":\"https://TR.example.test/Turkish/🇹🇷\"","tr",0);
  addon("\"title\":\"it was a good film\"","it",0);
  addon("\"title\":\"Movie no language information\"","tr",0);
  addon("\"title\":\"TRIGGER TURKEY ENTR TUR1080p Turkishly\"","tr",0);
  addon("\"title\":\"Unknown\"","*",1);
  // Inferring a chip match must not change the explicit audio display.
  Stream *parsed=NULL;assert(stream_extrair("{\"streams\":[{\"url\":\"https://example.test/1\",\"title\":\"🇹🇷\"},{\"url\":\"https://example.test/2\"}]}","fixture",&parsed)==2);
  assert(!parsed[0].audioIdiomas[0] && parsed[0].audioRotulos[0] && !parsed[1].audioRotulos[0]);free(parsed);
  // Diagnostics distinguish absent audio evidence, subtitle-only hits and
  // matching streams hidden by the other source-sheet chips or addon tab.
  Stream diagnostic[6]={{0}};lista=diagnostic;n=6;soDub=1;soMp4=1;soCache=1;filtro=0;
  ling_local_audio("tr");
  snprintf(diagnostic[0].audioIdiomas,sizeof diagnostic[0].audioIdiomas,"tr");
  diagnostic[0].mp4=1;diagnostic[0].emCache=1;
  snprintf(diagnostic[1].audioRotulos,sizeof diagnostic[1].audioRotulos,"Turkish");
  diagnostic[1].mp4=1;
  snprintf(diagnostic[2].audioIdiomas,sizeof diagnostic[2].audioIdiomas,"en");
  snprintf(diagnostic[2].legendaIdiomas,sizeof diagnostic[2].legendaIdiomas,"tr");
  snprintf(diagnostic[3].audioRotulos,sizeof diagnostic[3].audioRotulos,"tr");
  snprintf(diagnostic[4].legendaIdiomas,sizeof diagnostic[4].legendaIdiomas,"tr");
  AudioFiltroResumo d=resumoFiltroAudio();
  assert(d.declarados==2 && d.rotulos==2 && d.desconhecidos==2);
  assert(d.casaDeclarado==1 && d.casaRotulo==2 && d.soLegenda==2);
  assert(d.mp4==1 && d.cache==1 && d.visiveis==1);
  snprintf(provedores[1],sizeof provedores[1],"another fixture addon");filtro=1;
  d=resumoFiltroAudio();assert(d.addon==1 && !d.visiveis);
  filtro=0;soMp4=soCache=0;lista=NULL;n=0;
  puts("PASS: selected-language chip, add-on label fallback, declared precedence and subtitle/URL/provider isolation");
  return 0;
}
