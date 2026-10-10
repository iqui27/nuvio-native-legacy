#define _POSIX_C_SOURCE 200809L
#include "dts/dts_pipeline.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <pthread.h>
static int loads, plays, times, ends, video_info, full_events, drained_events;
static void (*complete_load)(void);
static void *finish_load(void *ctx) { (void)ctx;complete_load();return NULL; }
static void event(void *ctx,const char *j) {
 assert(ctx==&loads);
 assert(strstr(j,"\"mediaId\":\"fake-media\"") ||
        (getenv("DTS_MEDIA_ID_PHASE") && strstr(j,"\"mediaId\":\"\"")));
 if(strstr(j,"\"loadCompleted\"")) loads++;
 if(strstr(j,"\"playing\"")) plays++;
 if(strstr(j,"\"currentTime\":3000")) times++;
 if(strstr(j,"\"endOfStream\"")) ends++;
 if(strstr(j,"\"videoInfo\"")) { assert(strstr(j,"\"hdrType\":\"HDR10\""));video_info++; }
 if(strstr(j,"\"native-buffer-full\"")) {
  assert(strstr(j,"kind=") && strstr(j,"ptsNs=3000000000") && strstr(j,"bytes=4"));full_events++;
 }
 if(strstr(j,"\"native-buffer-drained\"")) drained_events++;
 if(strstr(j,"\"type\":0,") || strstr(j,"\"type\":18,") ||
    strstr(j,"\"type\":44,") || strstr(j,"\"type\":45,") || strstr(j,"\"type\":46,"))
  assert(strstr(j,"\"detail\":\"\""));
 if(strstr(j,"\"type\":18,")) assert(strstr(j,"\"errorText\":\"\""));
}
int main(int argc,char **argv) {
 assert(argc==3);
 setenv("NUVIO_DTS_ADAPTER_DIR","/no-such-dts-directory",1);
 assert(!dts_pipeline_available(0)); assert(!dts_pipeline_available(2));
 assert(!dts_pipeline_create("app","",0,event,&loads));
 setenv("NUVIO_DTS_ADAPTER_DIR",argv[2],1); dts_pipeline_available_esquecer(); assert(!dts_pipeline_available(4));
 setenv("NUVIO_DTS_ADAPTER_DIR",argv[1],1); dts_pipeline_available_esquecer();
 assert(dts_pipeline_available(0));
 void *native=dlopen("libplayerAPIs.so",RTLD_NOW|RTLD_LOCAL);assert(native);
 int (*play_calls)(void),(*pause_calls)(void);
 *(void **)(&play_calls)=dlsym(native,"dts_fixture_play_calls");assert(play_calls);
 *(void **)(&pause_calls)=dlsym(native,"dts_fixture_pause_calls");assert(pause_calls);
 *(void **)(&complete_load)=dlsym(native,"dts_fixture_complete_load");assert(complete_load);
 void (*numeric_events)(void);
 *(void **)(&numeric_events)=dlsym(native,"dts_fixture_numeric_events");assert(numeric_events);
 assert(dts_pipeline_available(3) != dts_pipeline_available(6));
 DtsPipeline *p=dts_pipeline_create("app","\"window",0,event,&loads); assert(p);
 assert(!strcmp(dts_pipeline_media_id(p),getenv("DTS_MEDIA_ID_PHASE") ? "" : "fake-media"));
 DtsMediaInfo m={0}; strcpy(m.video_codec,"hevc"); strcpy(m.audio_codec,"aac"); m.channels=2;m.sample_rate=48000;m.width=1920;m.height=1080;
 strcpy(m.audio_codec,"ac3");m.channels=4;assert(!dts_pipeline_load(p,&m,2.5));m.channels=2;
 strcpy(m.audio_codec,"eac3");assert(!dts_pipeline_load(p,&m,2.5));strcpy(m.audio_codec,"aac");
 m.channels=6;assert(!dts_pipeline_load(p,&m,2.5));m.channels=2;
 m.sample_rate=44100;assert(!dts_pipeline_load(p,&m,2.5));m.sample_rate=48000;
 m.dovi_profile=5; assert(!dts_pipeline_load(p,&m,2.5));m.dovi_profile=0;
 assert(dts_pipeline_load(p,&m,2.5)); assert(dts_pipeline_play(p)); assert(plays==0);
 numeric_events();
 for(int i=0;i<50;i++) assert(dts_pipeline_play(p));
 assert(play_calls()==0);
 const char *phase=getenv("DTS_MEDIA_ID_PHASE");
 assert(!strcmp(dts_pipeline_media_id(p),phase && !strcmp(phase,"preroll") ? "" : "fake-media"));
 unsigned char data[]={1,2,3,4}; DtsFrame frame={0};frame.data=data;frame.size=4;frame.kind=DTS_AUDIO;frame.pts_ns=3000000000LL;
 for(int i=0;i<10;i++) assert(dts_pipeline_feed(p,&frame)==0);
 assert(play_calls()==0);data[0]=2;
 assert(dts_pipeline_feed(p,&frame)==1);
 if(getenv("DTS_DEFER_COMPLETION")) {
  assert(loads==0 && plays==0 && play_calls()==0);
  pthread_t thread;assert(!pthread_create(&thread,NULL,finish_load,NULL));assert(!pthread_join(thread,NULL));
  assert(dts_pipeline_feed(p,&frame)==1);
 }
 assert(play_calls()==0); // Audio alone does not complete initial A/V preroll.
 frame.kind=DTS_VIDEO;assert(dts_pipeline_feed(p,&frame)==1);
 assert(loads==1 && plays==1 && video_info==1 && play_calls()==1);
 // Duplicate load/state reports and hundreds of accepted audio/video feeds
 // must never issue a second native Play or repeatedly resume the renderer.
 complete_load();assert(loads==2);
 for(int i=0;i<500;i++) { frame.kind=i%2?DTS_AUDIO:DTS_VIDEO;assert(dts_pipeline_feed(p,&frame)==1);assert(dts_pipeline_play(p)); }
 assert(play_calls()==1 && pause_calls()==0);
 assert(!strcmp(dts_pipeline_media_id(p),"fake-media"));
 if (getenv("DTS_EXPECT_NO_VOLUME")) assert(!dts_pipeline_volume(p,50));
 else { assert(dts_pipeline_volume(p,999));assert(dts_pipeline_volume(p,-5)); }
 for(int i=0;i<50;i++) assert(dts_pipeline_pause(p));
 assert(pause_calls()==1 && play_calls()==1);
 for(int i=0;i<50;i++) assert(dts_pipeline_feed(p,&frame)==1);
 assert(play_calls()==1); // Feeding preroll while paused cannot resume it.
 for(int i=0;i<50;i++) assert(dts_pipeline_play(p));
 assert(play_calls()==2 && pause_calls()==1);
 data[0]=1;for(int i=0;i<10;i++) assert(dts_pipeline_feed(p,&frame)==0);data[0]=2;
 for(int i=0;i<50;i++) assert(dts_pipeline_feed(p,&frame)==1);
 assert(play_calls()==2);
 assert(dts_pipeline_flush(p,5));assert(dts_pipeline_play(p));
 assert(play_calls()==3); /* Flush invalidates the previously accepted Play. */
 assert(dts_pipeline_pause(p));assert(pause_calls()==2);
 assert(dts_pipeline_flush(p,5));assert(dts_pipeline_pause(p));
 assert(pause_calls()==3); /* The same applies when scrubbing while paused. */
 assert(dts_pipeline_play(p));assert(play_calls()==4);
 assert(dts_pipeline_eos(p));assert(ends==1 && play_calls()==4);
 data[0]=3;assert(dts_pipeline_feed(p,&frame)==-1);
 memset(data,0,sizeof data); /* native retained pointer must still point at its own copy */
 assert(strstr(dts_pipeline_error(p),"Pending"));assert(dts_pipeline_feed(p,&frame)==-1);
 dts_pipeline_destroy(p);dts_pipeline_destroy(NULL);
 // Completion while the pending frame is refused must apply deferred Play
 // exactly once. The fixture accepts it on the next retry only after Play.
 int was_deferred=getenv("DTS_DEFER_COMPLETION")!=NULL;
 setenv("DTS_DEFER_COMPLETION","1",1);
 p=dts_pipeline_create("app","\"window",0,event,&loads);assert(p);
 assert(dts_pipeline_load(p,&m,2.5));assert(dts_pipeline_play(p));
 data[0]=2;frame.kind=DTS_VIDEO;assert(dts_pipeline_feed(p,&frame)==1);
 frame.kind=DTS_AUDIO;assert(dts_pipeline_feed(p,&frame)==1 && play_calls()==0);
 data[0]=4;frame.kind=DTS_AUDIO;
 assert(dts_pipeline_feed(p,&frame)==0 && play_calls()==0);
 pthread_t complete_thread;
 assert(!pthread_create(&complete_thread,NULL,finish_load,NULL));assert(!pthread_join(complete_thread,NULL));
 assert(dts_pipeline_feed(p,&frame)==0 && play_calls()==1);
 assert(dts_pipeline_feed(p,&frame)==1 && play_calls()==1);
 int initial_full=full_events,initial_drained=drained_events;
 data[0]=1;
 for(int i=0;i<1000;i++) assert(dts_pipeline_feed(p,&frame)==0);
 assert(play_calls()==1 && full_events==initial_full+2);
 data[0]=4;assert(dts_pipeline_feed(p,&frame)==1);
 assert(drained_events==initial_drained+1 && play_calls()==1);
 initial_full=full_events;initial_drained=drained_events;
 for(int i=0;i<1000;i++) {
  data[0]=1;assert(dts_pipeline_feed(p,&frame)==0);
  data[0]=4;assert(dts_pipeline_feed(p,&frame)==1);
 }
 assert(full_events==initial_full && drained_events==initial_drained && play_calls()==1);
 // A paused pipeline with identical refusal must retain pause intent.
 assert(dts_pipeline_pause(p));assert(dts_pipeline_flush(p,5));
 complete_load();
 for(int i=0;i<20;i++) assert(dts_pipeline_feed(p,&frame)==0);
 assert(play_calls()==1 && pause_calls()==1);
 assert(dts_pipeline_play(p));assert(dts_pipeline_feed(p,&frame)==1);
 assert(play_calls()==2);
 dts_pipeline_destroy(p);
 if(!was_deferred) unsetenv("DTS_DEFER_COMPLETION");
 // A synchronous Load completion can precede all feed calls. Repeated Play
 // requests must wait for the first accepted packet from each stream.
 p=dts_pipeline_create("app","\"window",0,event,&loads);assert(p);
 assert(dts_pipeline_load(p,&m,2.5));complete_load();
 for(int i=0;i<50;i++) assert(dts_pipeline_play(p));
 assert(play_calls()==0);
 data[0]=2;frame.kind=DTS_VIDEO;
 assert(dts_pipeline_feed(p,&frame)==1 && play_calls()==0);
 for(int i=0;i<50;i++) assert(dts_pipeline_play(p));
 assert(play_calls()==0);
 frame.kind=DTS_AUDIO;assert(dts_pipeline_feed(p,&frame)==1 && play_calls()==1);
 for(int i=0;i<50;i++) assert(dts_pipeline_play(p));
 assert(play_calls()==1);
 dts_pipeline_destroy(p);
 p=dts_pipeline_create("app","\"window",0,event,&loads);assert(p);
 strcpy(m.audio_codec,"aac");m.channels=2;m.sample_rate=48000;strcpy(m.hdr,"DolbyVision");
 m.dovi_profile=7;m.dovi_bl_present=1;m.dovi_rpu_present=1;assert(!dts_pipeline_load(p,&m,2.5));
 m.dovi_profile=8;m.dovi_el_present=1;assert(!dts_pipeline_load(p,&m,2.5));
 m.dovi_el_present=0;assert(dts_pipeline_load(p,&m,2.5));dts_pipeline_destroy(p);
 dlclose(native);
 puts("DTS adapter ABI, native control counts, asynchronous preroll, timestamps and retained pointer tests passed");
}
