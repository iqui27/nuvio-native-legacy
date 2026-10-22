/* Original adapter; see README.md for pinned SDK/firmware ABI and ownership contract. */
#include "adapter.h"
extern "C" {
#include "js.h"
}
#include <starfish-media-pipeline/StarfishMediaAPIs.h>
#include <dlfcn.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <vector>
using SF = StarfishMediaAPIs;
using Callback = void (*)(int, int64_t, const char *, void *);
struct Native {
  void *lib = nullptr;
  void (*ctor)(SF *, const char *) = nullptr;
  void (*dtor)(SF *) = nullptr;
  bool (*load)(SF *, const char *, Callback, void *) = nullptr;
  std::string (*feed)(SF *, const char *) = nullptr;
  bool (*play)(SF *) = nullptr, (*pause)(SF *) = nullptr,
       (*flush)(SF *, const char *) = nullptr, (*eos)(SF *) = nullptr,
       (*unload)(SF *) = nullptr, (*foreground)(SF *) = nullptr;
  const char *(*id)(SF *) = nullptr;
  bool (*volume)(SF *,const char *) = nullptr;
  ~Native() { if (lib) dlclose(lib); }
  template<typename T> bool symbol(T &out, const char *name) {
    out = reinterpret_cast<T>(dlsym(lib, name));
    if (!out) fprintf(stderr, "[dts] firmware symbol missing: %s\n", name);
    return out != nullptr;
  }
  bool open() {
    /* RTLD_NODELETE: a sonda abria e fechava a lib a cada video. No webOS 3
     * a 2a sonda da sessao caia (SIGSEGV com pc fora de qualquer modulo,
     * libgobject/libplayerAPIs na pilha; 65SJ800V/OLED55B7P). Suspeita: o
     * dlclose descarregava dependencias dela que o processo nao tinha
     * (GObject, GStreamer e outras; a GLib o video.c ja abre por conta)
     * com fios delas ainda rodando. Carregada uma vez, fica. */
    lib = dlopen("libplayerAPIs.so", RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
    if (!lib) lib = dlopen("libplayerAPIs.so.1", RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
    if (!lib) {
      fprintf(stderr, "[dts] firmware library load failed: %s\n", dlerror());
      return false;
    }
#if _GLIBCXX_USE_CXX11_ABI
    const char *feed_symbol = "_ZN17StarfishMediaAPIs4FeedB5cxx11EPKc";
#else
    const char *feed_symbol = "_ZN17StarfishMediaAPIs4FeedEPKc";
#endif
    volume = reinterpret_cast<decltype(volume)>(dlsym(lib,"_ZN17StarfishMediaAPIs9setVolumeEPKc")); /* Optional. */
    return symbol(ctor,"_ZN17StarfishMediaAPIsC1EPKc") &&
      symbol(dtor,"_ZN17StarfishMediaAPIsD1Ev") &&
      symbol(load,"_ZN17StarfishMediaAPIs4LoadEPKcPFvixS1_PvES2_") &&
      symbol(feed,feed_symbol) && symbol(play,"_ZN17StarfishMediaAPIs4PlayEv") &&
      symbol(pause,"_ZN17StarfishMediaAPIs5PauseEv") &&
      symbol(flush,"_ZN17StarfishMediaAPIs5flushEPKc") &&
      symbol(eos,"_ZN17StarfishMediaAPIs7pushEOSEv") &&
      symbol(unload,"_ZN17StarfishMediaAPIs6UnloadEv") &&
      symbol(id,"_ZN17StarfishMediaAPIs10getMediaIDEv") &&
      symbol(foreground,"_ZN17StarfishMediaAPIs16notifyForegroundEv");
  }
};
static std::string quote(const char *s) {
  std::string r = "\"";
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s ? s : ""); *p; ++p) {
    if (*p == '\\' || *p == '"') { r += '\\'; r += char(*p); }
    else if (*p < 32) { char b[7]; snprintf(b,sizeof b,"\\u%04x",*p); r += b; }
    else r += char(*p);
  }
  return r + '"';
}
struct Pipeline {
  Native n; SF *sf = nullptr;
  std::string app, window, media, error;
  std::mutex media_lock;
  void (*event)(void *, const char *) = nullptr; void *user = nullptr;
  std::atomic<bool> completed{false}, alive{true}, callback_seen{false};
  /* Accepted controls belong to the worker. Native state callbacks never
   * clear these guards: a late/duplicate callback must not reissue controls. */
  bool loaded = false, want_play = false, play_issued = false, pause_issued = false, poisoned = false;
  bool audio_fed = false, video_fed = false, started = false;
  bool backpressure_reported = false;
  uint64_t refused_feeds = 0;
  unsigned play_count = 0, pause_count = 0;
  std::vector<unsigned char> pending;
  ~Pipeline() {
    alive = false;
    if (sf) { if (loaded) n.unload(sf); n.dtor(sf); ::operator delete(sf); }
    /* Pending pointer storage survives all native teardown calls. */
  }
};
/* Native UID is an instance identity, not the application identity. App ID
 * belongs in Load.option.appId. Let firmware allocate its own unique UID.
 * Some firmwares do not publish a media ID until Load/preroll. */
static void refresh_media(Pipeline *p) {
  const char *id = p->n.id(p->sf);
  if (!id || !*id) return;
  std::lock_guard<std::mutex> lock(p->media_lock);
  p->media = id;
}
static std::string event_prefix(Pipeline *p) {
  std::lock_guard<std::mutex> lock(p->media_lock);
  return "{\"mediaId\":" + quote(p->media.c_str());
}
static void stage(Pipeline *p, const char *name, const char *detail = "") {
  if (!p->event) return;
  std::string j = event_prefix(p) + ",\"dtsStage\":{\"name\":" + quote(name) +
    ",\"detail\":" + quote(detail) + "}}";
  p->event(p->user,j.c_str());
}
static void callback(int type, int64_t value, const char *str, void *ctx) {
  auto *p = static_cast<Pipeline *>(ctx);
  if (!p->alive) return;
  if (!p->callback_seen.exchange(true)) {
    char detail[48]; snprintf(detail,sizeof detail,"type=%d",type);
    stage(p,"native-callback",detail);
  }
  if (type != PF_EVENT_TYPE_STR_VIDEO_INFO && type != PF_EVENT_TYPE_FRAMEREADY && type != PF_EVENT_TYPE_INT_ERROR &&
      type != PF_EVENT_TYPE_STR_ERROR && type != PF_EVENT_TYPE_STR_STATE_UPDATE__LOADCOMPLETED &&
      type != PF_EVENT_TYPE_STR_STATE_UPDATE__PLAYING && type != PF_EVENT_TYPE_STR_STATE_UPDATE__PAUSED &&
      type != PF_EVENT_TYPE_STR_STATE_UPDATE__ENDOFSTREAM && type != 0x2c && type != 0x2d && type != 0x2e)
    return; /* Unused native metadata can exceed the main-thread queue's event budget. */
  /* The callback ABI shares one pointer slot across string and numeric
   * events. Numeric/frame/buffer notifications do not promise text there. */
  const bool textual = type == PF_EVENT_TYPE_STR_VIDEO_INFO || type == PF_EVENT_TYPE_STR_ERROR ||
    type == PF_EVENT_TYPE_STR_STATE_UPDATE__LOADCOMPLETED || type == PF_EVENT_TYPE_STR_STATE_UPDATE__PLAYING ||
    type == PF_EVENT_TYPE_STR_STATE_UPDATE__PAUSED || type == PF_EVENT_TYPE_STR_STATE_UPDATE__ENDOFSTREAM;
  if (!textual) str = nullptr;
  const char *video_json = type == PF_EVENT_TYPE_STR_VIDEO_INFO ? str : nullptr;
  if (str && strnlen(str,129) > 128) str = "native detail exceeds event budget";
  if (type == PF_EVENT_TYPE_STR_STATE_UPDATE__LOADCOMPLETED) p->completed = true;
  if (p->event) {
    const char *name = nullptr;
    switch (type) {
      case PF_EVENT_TYPE_STR_STATE_UPDATE__LOADCOMPLETED: name = "loadCompleted"; break;
      case PF_EVENT_TYPE_STR_STATE_UPDATE__PLAYING: name = "playing"; break;
      case PF_EVENT_TYPE_STR_STATE_UPDATE__PAUSED: name = "paused"; break;
      case PF_EVENT_TYPE_STR_STATE_UPDATE__ENDOFSTREAM: name = "endOfStream"; break;
    }
    std::string j = event_prefix(p);
    if (video_json) {
      char hdr[48] = "";
      const double width = js_num(video_json,nullptr,"width",0);
      const double height = js_num(video_json,nullptr,"height",0);
      const double rate = js_num(video_json,nullptr,"frameRate",0);
      js_texto(video_json,nullptr,"hdrType",hdr,sizeof hdr);
      j += ",\"videoInfo\":{\"hdrType\":" + quote(hdr[0] ? hdr : "none");
      if (width > 0 && width <= 65536) j += ",\"width\":" + std::to_string(static_cast<int>(width));
      if (height > 0 && height <= 65536) j += ",\"height\":" + std::to_string(static_cast<int>(height));
      if (rate > 0 && rate <= 1000) j += ",\"frameRate\":" + std::to_string(rate);
      j += "}";
    }
    if (name) j += "," + quote(name) + ":{}";
    if (type == PF_EVENT_TYPE_FRAMEREADY)
      j += ",\"currentTime\":{\"currentTime\":" + std::to_string(value / 1000000) + "}";
    if (type == PF_EVENT_TYPE_INT_ERROR || type == PF_EVENT_TYPE_STR_ERROR)
      j += ",\"error\":{\"errorCode\":" + std::to_string(value) + ",\"errorText\":" + quote(str) + "}";
    j += ",\"dtsEvent\":{\"type\":" + std::to_string(type) + ",\"value\":" +
      std::to_string(value) + ",\"detail\":" + quote(str) + "}}";
    p->event(p->user,j.c_str());
  }
}
static int probe() { Native n; return n.open(); }
static void *create(const char *app, const char *window, void (*event)(void *,const char *), void *user) {
  std::unique_ptr<Pipeline> p(new (std::nothrow) Pipeline);
  if (!p || !p->n.open()) return nullptr;
  SF *storage = static_cast<SF *>(::operator new(sizeof(SF),std::nothrow));
  if (!storage) return nullptr;
  try { p->n.ctor(storage,nullptr); }
  catch (...) { ::operator delete(storage); return nullptr; }
  p->sf = storage;
  p->app = app ? app : ""; p->window = window ? window : "";
  refresh_media(p.get());
  p->event = event; p->user = user;
  stage(p.get(),"native-created",p->media.empty() ? "media ID pending" : "media ID available");
  return p.release();
}
static void destroy(void *ctx) { delete static_cast<Pipeline *>(ctx); }
static int64_t target_ns(double target) {
  return static_cast<int64_t>(target * 1000000000.0);
}
static bool valid_target(double t) { return std::isfinite(t) && t >= 0 && t < 9e9; }
static const char *video_codec(const char *c) {
  if (!strcmp(c,"h264")) return "H264";
  if (!strcmp(c,"hevc") || !strcmp(c,"h265")) return "H265";
  if (!strcmp(c,"vp9")) return "VP9";
  if (!strcmp(c,"av1")) return "AV1";
  return nullptr;
}
static int load(void *ctx,const DtsMediaInfo *m,double target) {
  auto *p = static_cast<Pipeline *>(ctx); const char *v = video_codec(m->video_codec);
  const bool ac3 = !strcmp(m->audio_codec,"ac3");
  const bool dv = m->dovi_profile != 0;
  const bool valid_dv = (m->dovi_profile == 5 || m->dovi_profile == 8) &&
    !strcmp(v ? v : "", "H265") && m->dovi_bl_present && m->dovi_rpu_present && !m->dovi_el_present;
  /* AC-3 (DTS converted to 5.1, or passed through on the Dolby Vision path)
   * and E-AC-3 (passed through). Names and the ac3PlusInfo block are the ones
   * libpf-1.0.so parses on webOS 4.10; on the LG C9 E-AC-3 5.1 was heard and
   * a passed-through AC-3 5.1 track played with codec "AC3" + ac3PlusInfo. */
  const char *pass = !strcmp(m->audio_codec,"eac3") ? "AC3 PLUS" : ac3 ? "AC3" : nullptr;
  const bool audio_ok = pass ? (m->channels > 0 && m->channels <= 8 && m->sample_rate > 0)
    : (!strcmp(m->audio_codec,"aac") && m->channels == 2 && m->sample_rate == 48000);
  if (p->loaded || !v || !valid_target(target) || m->width <= 0 || m->height <= 0 || !audio_ok ||
      (dv && !valid_dv) || (m->hdr[0] && strcmp(m->hdr,"SDR") && strcmp(m->hdr,"PQ") &&
      strcmp(m->hdr,"HDR10") && strcmp(m->hdr,"HLG") && !(dv && !strcmp(m->hdr,"DolbyVision")))) {
    p->error = "unsupported codec, HDR/Dolby Vision metadata, target or repeated load"; return 0;
  }
  std::ostringstream j;
  j << "{\"args\":[{\"mediaTransportType\":\"BUFFERSTREAM\",\"option\":{\"appId\":" << quote(p->app.c_str());
  if (!p->window.empty()) j << ",\"windowId\":" << quote(p->window.c_str());
  j << ",\"queryPosition\":false,\"externalStreamingInfo\":{\"contents\":{\"codec\":{\"video\":" << quote(v)
    << ",\"audio\":" << quote(pass ? pass : "AAC")
    << "}";
  if (dv) j << ",\"DolbyHdrInfo\":{\"encryptionType\":\"clear\",\"profileId\":"
    << m->dovi_profile << ",\"trackType\":\"single\"}";
  j << ",\"format\":\"RAW\",\"provider\":\"Chrome\",\"esInfo\":{\"pauseAtDecodeTime\":true,\"seperatedPTS\":true,\"ptsToDecode\":"
    << target_ns(target) << ",\"videoWidth\":" << m->width << ",\"videoHeight\":" << m->height;
  if (m->fps_num > 0 && m->fps_den > 0) j << ",\"videoFpsValue\":" << m->fps_num << ",\"videoFpsScale\":" << m->fps_den;
  j << "}";
  if (pass) j << ",\"ac3PlusInfo\":{\"channels\":" << m->channels << ",\"frequency\":"
    << m->sample_rate / 1000.0 << "}";
  else j << ",\"aacInfo\":{\"channels\":" << m->channels << ",\"frequency\":"
    << m->sample_rate / 1000.0 << ",\"profile\":2,\"format\":\"raw\"}";
  j << "},\"bufferingCtrInfo\":{\"srcBufferLevelVideo\":{\"minimum\":0,\"maximum\":8388608},\"srcBufferLevelAudio\":{\"minimum\":0,\"maximum\":2097152}}},\"transmission\":{\"contentsType\":\"LIVE\",\"trickType\":\"client-side\"}}}]}";
  /* Foreground notification is advisory: both reference players still Load
   * when it returns false. The visible application's state is already managed
   * by webOS; let Load report whether resources can actually be acquired. */
  if (!p->n.foreground(p->sf)) stage(p,"native-foreground-refused","continuing to Load");
  { char d[96]; snprintf(d,sizeof d,"dv=%d profile=%d audio=%s ch=%d",int(dv),m->dovi_profile,
      pass ? pass : "AAC",m->channels); stage(p,"native-load-requested",d); }
  if (!p->n.load(p->sf,j.str().c_str(),callback,p)) {
    p->error = "Starfish Load refused"; return 0;
  }
  p->error.clear(); p->loaded = true;
  refresh_media(p);
  stage(p,"native-load-accepted");
  return 1;
}
static int play(void *ctx) {
  auto *p = static_cast<Pipeline *>(ctx); p->want_play = true;
  if (!p->loaded || p->poisoned) return 0;
  if (!p->completed) return 1; /* Intent is applied on worker feed/eos after preroll. */
  /* Some firmware completes Load synchronously before accepting any packets.
   * Both configured streams need their first packet before initial Play.
   * Once started, pause/resume controls keep their normal immediate ordering. */
  if (!p->started && (!p->audio_fed || !p->video_fed)) return 1;
  if (p->play_issued) return 1;
  if (!p->n.play(p->sf)) { p->error = "Starfish Play refused"; return 0; }
  p->play_issued = true; p->pause_issued = false; p->started = true;
  char detail[48];snprintf(detail,sizeof detail,"count=%u",++p->play_count);
  stage(p,"native-play-accepted",detail);
  return 1;
}
static int feed(void *ctx,const DtsFrame *f) {
  auto *p = static_cast<Pipeline *>(ctx);
  if (!p->loaded || p->poisoned || !f->data || !f->size || f->size > 16u*1024u*1024u ||
      (f->kind != DTS_VIDEO && f->kind != DTS_AUDIO) || f->pts_ns < 0) {
    p->error = "invalid packet or unavailable pipeline"; return -1;
  }
  try { p->pending.assign(f->data,f->data+f->size); }
  catch (...) { p->error = "packet copy allocation failed"; return -1; }
  char j[256];
  snprintf(j,sizeof j,"{\"bufferAddr\":\"%p\",\"bufferSize\":%zu,\"pts\":%lld,\"esData\":%d}",
    p->pending.data(),p->pending.size(),static_cast<long long>(f->pts_ns),f->kind);
  const std::string result = p->n.feed(p->sf,j);
  if (result.find("Pending") == std::string::npos && result.find("BufferFull") != std::string::npos) {
    p->pending.clear();
    ++p->refused_feeds;
    /* Keep diagnostics bounded: ordinary alternating Full/Ok replies must
     * not flood the application's event queue. Sustained refusal identifies
     * the stream holding up demux progress without exposing packet pointers. */
    if (!p->backpressure_reported || p->refused_feeds % 500 == 0) {
      p->backpressure_reported = true;
      char detail[160];
      snprintf(detail,sizeof detail,"kind=%s ptsNs=%lld bytes=%zu retries=%llu ready=%d play=%d",
        f->kind == DTS_AUDIO ? "audio" : "video",static_cast<long long>(f->pts_ns),
        f->size,static_cast<unsigned long long>(p->refused_feeds),
        int(p->completed.load()),int(p->play_issued));
      stage(p,"native-buffer-full",detail);
    }
    /* Completion can arrive while the preroll buffers are already full.
     * Applying a deferred Play must not depend on accepting another packet:
     * the stopped decoder may need that one control to drain its queue. */
    if (p->want_play && p->completed && !p->play_issued && !play(p)) return -1;
    return 0;
  }
  if (result.find("Pending") == std::string::npos && result.find("Ok") != std::string::npos) {
    p->pending.clear();
    if (p->refused_feeds >= 500) {
      char detail[64];
      snprintf(detail,sizeof detail,"retries=%llu",static_cast<unsigned long long>(p->refused_feeds));
      stage(p,"native-buffer-drained",detail);
    }
    p->refused_feeds = 0;
    refresh_media(p);
    bool &first = f->kind == DTS_AUDIO ? p->audio_fed : p->video_fed;
    if (!first) { first = true; stage(p,f->kind == DTS_AUDIO ? "audio-feed-accepted" : "video-feed-accepted"); }
    if (p->want_play && p->completed && !p->play_issued && !play(p)) return -1;
    return 1;
  }
  /* No published Pending completion/ownership contract: retain until native teardown. */
  p->poisoned = true; p->error = "unrecognized/asynchronous Feed result: " + result;
  return -1;
}
static int pause(void *ctx) {
  auto *p = static_cast<Pipeline *>(ctx); p->want_play = false;
  if (!p->loaded || p->poisoned) return 0;
  if (!p->completed) return 1;
  if (p->pause_issued) return 1;
  if (!p->n.pause(p->sf)) { p->error = "Starfish Pause refused"; return 0; }
  p->play_issued = false; p->pause_issued = true;
  char detail[48];snprintf(detail,sizeof detail,"count=%u",++p->pause_count);
  stage(p,"native-pause-accepted",detail);
  return 1;
}
static int flush(void *ctx,double target) {
  auto *p = static_cast<Pipeline *>(ctx);
  if (!p->loaded || p->poisoned || !valid_target(target)) return 0;
  std::string j = "{\"ptsToDecode\":" + std::to_string(target_ns(target)) + "}";
  if (!p->n.flush(p->sf,j.c_str())) { p->error = "Starfish flush refused"; return 0; }
  /* Flush stops/pauses the firmware decoders even if their prior Play or
   * Pause was accepted. Those controls must be issued again for this seek.
   * A flush does not promise a second LOADCOMPLETED notification. */
  p->play_issued = p->pause_issued = false;
  p->audio_fed = p->video_fed = false;
  p->refused_feeds = 0;
  stage(p,"native-flush-accepted");
  return 1;
}
static int eos(void *ctx) {
  auto *p = static_cast<Pipeline *>(ctx);
  if (!p->loaded || p->poisoned) return 0;
  if (p->want_play && p->completed && !p->play_issued && !play(p)) return 0;
  if (!p->n.eos(p->sf)) { p->error = "Starfish pushEOS refused"; return 0; }
  return 1;
}
static const char *media_id(void *ctx) {
  auto *p = static_cast<Pipeline *>(ctx);
  refresh_media(p);
  return p->media.c_str();
}
static const char *error(void *ctx) { return static_cast<Pipeline *>(ctx)->error.c_str(); }
static int volume(void *ctx,int pct) {
  auto *p = static_cast<Pipeline *>(ctx);
  if (!p->n.volume || !p->loaded || p->poisoned) return 0;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  std::string j = "{\"mediaId\":" + quote(p->media.c_str()) + ",\"volume\":" +
    std::to_string(pct) + ",\"ease\":{\"duration\":0,\"type\":\"Linear\"}}";
  if (!p->n.volume(p->sf,j.c_str())) { p->error = "Starfish volume refused"; return 0; }
  return 1;
}
extern "C" const DtsAdapter *nuvio_dts_adapter_v2() {
  static const DtsAdapter api = {DTS_ADAPTER_ABI,sizeof(DtsAdapter),probe,create,destroy,load,feed,play,pause,flush,eos,media_id,error,volume};
  return &api;
}
