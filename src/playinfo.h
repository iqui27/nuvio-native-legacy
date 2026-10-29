#ifndef NV_PLAYINFO_H
#define NV_PLAYINFO_H
#include "streams.h"
#include "video.h"
#include "p2pmotor.h"

enum { PI_MODE, PI_SOURCE, PI_FILE, PI_CONTAINER, PI_CODEC, PI_RESOLUTION,
       PI_FPS, PI_HDR, PI_BITRATE, PI_AUDIO, PI_SUBTITLE, PI_BUFFER,
       PI_DROPPED, PI_PEERS, PI_DOWNLOAD, PI_N };
typedef struct { char value[PI_N][192]; int p2p; } PlayInfo;
void playinfo_model(PlayInfo *m, const Stream *s, const VideoStats *stats,
                    const VideoInfo *info, int width, int height, const char *hdr,
                    const VideoFaixa *audio, const VideoFaixa *subtitle,
                    int externalSubtitle, const P2pmEstado *p2p, int declaredKbps);
void playinfo_open(void);
void playinfo_close(void);
int playinfo_event(const SDL_Event *e);
void playinfo_draw(Uint32 now);
#ifdef NV_SHOT_HOOKS
const PlayInfo *playinfo_sample(void);
float playinfo_alpha(void);
#endif
#endif
