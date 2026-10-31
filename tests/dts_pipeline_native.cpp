#include <starfish-media-pipeline/StarfishMediaAPIs.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <atomic>
extern "C" {
#include "js.h"
}
static void (*callback)(int,long long,const char *,void *);
static void *context;
static const unsigned char *retained;
static std::atomic<bool> complete{false}, loaded{false};
static int play_calls, pause_calls;
static bool playing, seeking;
static int seek_calls;
StarfishMediaAPIs::StarfishMediaAPIs(const char *uid) {
 // UID names a native media instance. Supplying app ID here aliases instances.
 assert(uid==nullptr); retained=nullptr; complete=false; loaded=false;play_calls=pause_calls=0;playing=false;seeking=false;seek_calls=0;
}
StarfishMediaAPIs::~StarfishMediaAPIs() { if (retained) assert(retained[0]==3); callback=nullptr; }
bool StarfishMediaAPIs::Load(const char *j,void (*cb)(int,long long,const char *,void *),void *ctx) {
 // Follow the native parser's nested paths rather than finding any matching
 // text anywhere in the payload, which hid misplaced options in past fixtures.
 char option[4096], external[3072], contents[2048], es[1024], codec[256], text[128];
 char buffering[1024], source[256];
 const char *arg=js_array(j,nullptr,"args");assert(arg && !js_prox(js_fim(arg)));
 assert(js_texto_raiz_em(arg,js_fim(arg),"mediaTransportType",text,sizeof text) && !strcmp(text,"BUFFERSTREAM"));
 assert(js_bruto(arg,js_fim(arg),"option",option,sizeof option));
 assert(js_texto_raiz(option,"appId",text,sizeof text) && !strcmp(text,"app"));
 assert(js_texto_raiz(option,"windowId",text,sizeof text) && !strcmp(text,"\"window"));
 assert(js_bruto(option,nullptr,"externalStreamingInfo",external,sizeof external));
 assert(js_bruto(external,nullptr,"bufferingCtrInfo",buffering,sizeof buffering));
 assert(js_num(buffering,nullptr,"preBufferByte",-1)==-1);
 assert(js_num(buffering,nullptr,"bufferMinLevel",-1)==-1);
 assert(js_num(buffering,nullptr,"bufferMaxLevel",-1)==-1);
 assert(js_num(buffering,nullptr,"qBufferLevelVideo",-1)==-1);
 assert(js_num(buffering,nullptr,"qBufferLevelAudio",-1)==-1);
 assert(js_bruto(buffering,nullptr,"srcBufferLevelVideo",source,sizeof source));
 assert(js_num(source,nullptr,"minimum",-1)==0 && js_num(source,nullptr,"maximum",-1)==8388608);
 assert(js_bruto(buffering,nullptr,"srcBufferLevelAudio",source,sizeof source));
 assert(js_num(source,nullptr,"minimum",-1)==0 && js_num(source,nullptr,"maximum",-1)==2097152);
 assert(js_bruto(external,nullptr,"contents",contents,sizeof contents));
 assert(js_bruto(contents,nullptr,"esInfo",es,sizeof es) && js_num(es,nullptr,"ptsToDecode",-1)==2500000000);
 assert(js_bruto(contents,nullptr,"codec",codec,sizeof codec));
 assert(js_texto_raiz(codec,"video",text,sizeof text) && !strcmp(text,"H265"));
 assert(js_texto_raiz(codec,"audio",text,sizeof text) && !strcmp(text,"AAC"));
 assert(strstr(j,"\"format\":\"RAW\"")); assert(strstr(j,"\"frequency\":48"));
 assert(strstr(j,"\"channels\":2"));
 if(strstr(j,"DolbyHdrInfo")) {
  assert(strstr(j,"\"profileId\":8"));assert(strstr(j,"\"trackType\":\"single\""));
 }
 assert(strstr(j,"\\\"window")); callback=cb; context=ctx; loaded=true; return true;
}
extern "C" int dts_fixture_play_calls() { return play_calls; }
extern "C" int dts_fixture_pause_calls() { return pause_calls; }
extern "C" void dts_fixture_numeric_events() {
 // Numeric callbacks may leave the string slot unusable. Reading it is an
 // ABI violation, even for logging. Deliberately make that fail immediately.
 const char *unused=reinterpret_cast<const char *>(uintptr_t(1));
 callback(0x2c,0,unused,context);callback(0x2d,0,unused,context);callback(0x2e,0,unused,context);
 callback(PF_EVENT_TYPE_INT_ERROR,123,unused,context);
}
extern "C" void dts_fixture_complete_load() {
 if(!complete.exchange(true)) callback(4,0,"{\"width\":1920,\"height\":1080,\"hdrType\":\"HDR10\"}",context);
 callback(0x16,0,"",context);
}
std::string StarfishMediaAPIs::Feed(const char *j) {
 void *address=nullptr; size_t size=0; long long pts=0; int kind=0;
 assert(sscanf(j,"{\"bufferAddr\":\"%p\",\"bufferSize\":%zu,\"pts\":%lld,\"esData\":%d}",&address,&size,&pts,&kind)==4);
 assert(size==4 && pts==3000000000LL && (kind==1 || kind==2));
 auto *data=static_cast<unsigned char *>(address);
 if(data[0]==1) return "BufferFull";
 // A completed preroll queue cannot drain until the deferred Play is issued.
 if(data[0]==4 && !playing) return "BufferFull";
 if(data[0]==3) { retained=data; return "Pending"; }
 if(!complete && !getenv("DTS_DEFER_COMPLETION")) dts_fixture_complete_load();
 callback(0,pts,reinterpret_cast<const char *>(uintptr_t(1)),context); return "Ok";
}
bool StarfishMediaAPIs::Play() { assert(complete && !seeking);play_calls++;playing=true; callback(0x1a,0,"",context); return true; }
bool StarfishMediaAPIs::Pause() { assert(!seeking);pause_calls++;playing=false;callback(0x1b,0,"",context); return true; }
bool StarfishMediaAPIs::flush(const char *) { assert(!"Raw flush must not be used for a native seek");return false; }
extern "C" int dts_fixture_seek_calls() { return seek_calls; }
extern "C" void dts_fixture_complete_seek() {
 if(!seeking) return;
 seeking=false;
 // SEEKDONE text is not needed. Treat its slot as opaque too.
 callback(PF_EVENT_TYPE_STR_STATE_UPDATE__SEEKDONE,0,reinterpret_cast<const char *>(uintptr_t(1)),context);
}
#ifndef DTS_NO_SEEK
bool StarfishMediaAPIs::Seek(const char *value) {
 assert(!strcmp(value,"5000"));seek_calls++;
 if(getenv("DTS_REFUSE_SEEK")) return false;
 seeking=true;playing=false;
 // A late clock from the previous segment must not escape the adapter.
 callback(0,1697863000000LL,reinterpret_cast<const char *>(uintptr_t(1)),context);
 if(!getenv("DTS_DEFER_SEEK")) dts_fixture_complete_seek();
 return true;
}
#endif
bool StarfishMediaAPIs::pushEOS() { assert(playing);callback(0x1c,0,"",context); return true; }
bool StarfishMediaAPIs::Unload() { if(retained) assert(retained[0]==3); return true; }
const char *StarfishMediaAPIs::getMediaID() {
 const char *phase=getenv("DTS_MEDIA_ID_PHASE");
 if(phase && !strcmp(phase,"load") && !loaded) return nullptr;
 if(phase && !strcmp(phase,"preroll") && !complete) return "";
 return "fake-media";
}
bool StarfishMediaAPIs::notifyForeground() { return !getenv("DTS_FOREGROUND_FALSE"); }

#ifndef DTS_NO_VOLUME
bool StarfishMediaAPIs::setVolume(const char *j) { assert(strstr(j,"\"volume\":100") || strstr(j,"\"volume\":0"));return true; }
#endif
