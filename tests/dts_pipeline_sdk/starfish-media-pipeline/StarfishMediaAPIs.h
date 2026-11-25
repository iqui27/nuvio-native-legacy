/* Host fixture ONLY. Real ARM adapter builds include the inspected NDK header. */
#pragma once
#include <string>
#include <stdint.h>
class StarfishMediaAPIs {
 public:
  StarfishMediaAPIs(const char *);
  ~StarfishMediaAPIs();
  std::string Feed(const char *);
  bool Load(const char *,void (*)(int,long long,const char *,void *),void *);
  bool Play(); bool Pause(); bool flush(const char *); bool Seek(const char *); bool pushEOS();
  bool Unload(); const char *getMediaID(); bool notifyForeground(); bool setVolume(const char *);
 private: char storage[8192];
};
enum {
 PF_EVENT_TYPE_STR_VIDEO_INFO=4, PF_EVENT_TYPE_FRAMEREADY=0, PF_EVENT_TYPE_INT_ERROR=0x12, PF_EVENT_TYPE_STR_ERROR=0x13,
 PF_EVENT_TYPE_STR_STATE_UPDATE__LOADCOMPLETED=0x16,
 PF_EVENT_TYPE_STR_STATE_UPDATE__SEEKDONE=0x19,
 PF_EVENT_TYPE_STR_STATE_UPDATE__PLAYING=0x1a,
 PF_EVENT_TYPE_STR_STATE_UPDATE__PAUSED=0x1b,
 PF_EVENT_TYPE_STR_STATE_UPDATE__ENDOFSTREAM=0x1c
};
