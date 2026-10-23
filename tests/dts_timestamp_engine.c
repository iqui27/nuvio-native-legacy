/* Real FFmpeg resampling/encoding; controlled source gaps, no network or TV. */
#define NV_DTS_FFMPEG
#ifndef NV_TEST_LEGACY_RECOVERY
#define NV_DTS_WEBOS_RECOVERY
#endif
#include "../src/dts/dts_engine.c"
#include <assert.h>
static void init(DtsEngine *e,int channels,int codec) {
 memset(e,0,sizeof *e); /* SDR DTS recovery with P7 conversion disabled. */
 const AVCodec *c=codec?avcodec_find_encoder(codec):NULL;
 e->enc=avcodec_alloc_context3(c);e->dec=avcodec_alloc_context3(NULL);assert(e->enc&&e->dec);
 e->enc->time_base=(AVRational){1,48000};e->enc->sample_fmt=AV_SAMPLE_FMT_FLTP;e->enc->sample_rate=48000;
 av_channel_layout_default(&e->enc->ch_layout,channels);e->dec->pkt_timebase=(AVRational){1,48000};
 if(c) {e->enc->bit_rate=channels>2?640000:192000;assert(avcodec_open2(e->enc,c,NULL)>=0);}
 e->fifo=av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP,channels,4096);assert(e->fifo);
 e->encoded=av_frame_alloc();e->out=av_packet_alloc();assert(e->encoded&&e->out);
}
static void cleanup(DtsEngine *e) {
 swr_free(&e->swr);av_audio_fifo_free(e->fifo);av_channel_layout_uninit(&e->input_layout);
 avcodec_free_context(&e->enc);avcodec_free_context(&e->dec);av_frame_free(&e->encoded);av_packet_free(&e->out);
}
static AVFrame *frame(int rate,int format,int channels,int count,int64_t pts) {
 AVFrame *f=av_frame_alloc();assert(f);f->sample_rate=rate;f->format=format;f->nb_samples=count;
 av_channel_layout_default(&f->ch_layout,channels);f->best_effort_timestamp=pts;assert(av_frame_get_buffer(f,0)>=0);
 if(format==AV_SAMPLE_FMT_FLTP) for(int c=0;c<channels;c++) for(int i=0;i<count;i++) ((float*)f->extended_data[c])[i]=0.25f;
 else if(format==AV_SAMPLE_FMT_FLT) for(int i=0;i<count*channels;i++) ((float*)f->data[0])[i]=0.25f;
 else {assert(format==AV_SAMPLE_FMT_S16);for(int i=0;i<count*channels;i++) ((int16_t*)f->data[0])[i]=8192;}
 return f;
}
static int64_t expected(DtsEngine *e) {
 return e->sample_pts+av_audio_fifo_size(e->fifo)+(e->swr?av_rescale_rnd(swr_get_delay(e->swr,e->input_rate),48000,e->input_rate,AV_ROUND_NEAR_INF):0);
}
static void boundary(int gap,int failure) {
 DtsEngine e;init(&e,2,0);e.timeline=1;
 AVFrame *f=frame(48000,AV_SAMPLE_FMT_FLTP,2,512,gap);
 int n=resample(&e,f);
 if(failure) {assert(n<0&&!strcmp(e.error,DTS_ENGINE_TIMESTAMP_DISCONTINUITY));assert(!av_audio_fifo_size(e.fifo)&&!e.pad_count);}
 else {assert(n==512+(gap>4800?gap:0));assert(e.pad_count==(gap>4800));}
 cleanup(&e);av_frame_free(&f);
}
static void observed(int64_t incoming,int64_t want,int gap) {
 DtsEngine e;init(&e,2,0);e.timeline=1;e.sample_pts=want-512;
 AVFrame *f=frame(48000,AV_SAMPLE_FMT_FLTP,2,512,incoming);
 float zero[512]={0};void *pre[2]={zero,zero};assert(av_audio_fifo_write(e.fifo,pre,512)==512);
 assert(resample(&e,f)==gap+512);assert(e.sample_pts==want-512&&e.pad_samples==gap&&e.pad_count==1);
 int n=av_audio_fifo_size(e.fifo);assert(n==gap+1024);
 float *data[2]={av_malloc_array(n,sizeof(float)),av_malloc_array(n,sizeof(float))};assert(data[0]&&data[1]);
 assert(av_audio_fifo_peek(e.fifo,(void**)data,n)==n);
 for(int c=0;c<2;c++) {for(int i=0;i<n;i++) assert(data[c][i]==(i<gap+512?0.0f:0.25f));av_free(data[c]);}
 f->best_effort_timestamp+=512;assert(resample(&e,f)==512&&e.pad_count==1);
 cleanup(&e);av_frame_free(&f);
}
static void budget(int gap,int allowed) {
 DtsEngine e;init(&e,2,0);e.timeline=1;
 AVFrame *f=frame(48000,AV_SAMPLE_FMT_FLTP,2,512,0);
 for(int i=0;i<allowed;i++){f->best_effort_timestamp=expected(&e)+gap;assert(resample(&e,f)>0);}
 int before=av_audio_fifo_size(e.fifo);f->best_effort_timestamp=expected(&e)+gap;
 assert(resample(&e,f)<0&&av_audio_fifo_size(e.fifo)==before);
 /* A later 30 s of media progress opens a new bounded window. */
 e.sample_pts+=30LL*48000;f->best_effort_timestamp=expected(&e)+gap;
 assert(resample(&e,f)>0&&e.pad_count==1&&e.pad_samples==gap);
 /* First frame after seek/new timeline resets a spent budget. */
 av_audio_fifo_reset(e.fifo);swr_free(&e.swr);av_channel_layout_uninit(&e.input_layout);e.timeline=0;
 f->best_effort_timestamp=9000;assert(resample(&e,f)>0&&!e.pad_count&&!e.pad_samples&&e.sample_pts==9000);
 cleanup(&e);av_frame_free(&f);
}
static void packets(DtsEngine *e,int64_t *last,int *count) {
 int r;while((r=avcodec_receive_packet(e->enc,e->out))>=0) {
  assert(e->out->pts!=AV_NOPTS_VALUE&&e->out->pts>*last);*last=e->out->pts;(*count)++;av_packet_unref(e->out);
 }
 assert(r==AVERROR(EAGAIN)||r==AVERROR_EOF);
}
static void drain(DtsEngine *e,int64_t *last,int *count,int final) {
 while(encode_fifo(e,final)>0) packets(e,last,count);
}
static void encode(int rate,int format,int channels,int codec,int gap) {
 DtsEngine e;init(&e,channels,codec);int64_t last=INT64_MIN;int packets_out=0;
 int64_t input_count=0,offset=0;const int64_t base=3*48000;int64_t converted=0;
 for(int i=0;i<100;i++) {
  if(i==4) offset+=gap;
  int64_t pts=base+av_rescale_rnd(input_count,48000,rate,AV_ROUND_NEAR_INF)+offset;
  AVFrame *f=frame(rate,format,channels,1024,pts);int r=resample(&e,f);assert(r>=0);converted+=r;
  input_count+=1024;av_frame_free(&f);drain(&e,&last,&packets_out,0);
 }
 int r=resample(&e,NULL);assert(r>=0);converted+=r;
 int64_t want=av_rescale_rnd(input_count,48000,rate,AV_ROUND_NEAR_INF)+gap;
 assert(llabs(converted-want)<=2);assert(e.pad_count==1&&abs(e.pad_samples-gap)<=2);
 drain(&e,&last,&packets_out,1);assert(!av_audio_fifo_size(e.fifo));
 assert(e.sample_pts>=base+want&&e.sample_pts<base+want+e.enc->frame_size+2);
 assert(avcodec_send_frame(e.enc,NULL)>=0);packets(&e,&last,&packets_out);assert(packets_out>20);
 cleanup(&e);
}
int main(void) {
 av_log_set_level(AV_LOG_ERROR);
 boundary(0,0);boundary(4800,0);boundary(-4800,0);boundary(4801,0);boundary(24000,0);
 boundary(24001,1);boundary(-4801,1);boundary(263800*48,1);
 observed(275317LL*48,13205488,9728);observed(405739LL*48,19463696,11776);
 budget(4801,4);budget(24000,2);
 for(int rate=0;rate<3;rate++) for(int fmt=0;fmt<3;fmt++) {
  int rates[]={44100,48000,96000},formats[]={AV_SAMPLE_FMT_FLTP,AV_SAMPLE_FMT_FLT,AV_SAMPLE_FMT_S16};
  encode(rates[rate],formats[fmt],2,AV_CODEC_ID_AAC,9728);
  encode(rates[rate],formats[fmt],6,AV_CODEC_ID_AC3,11776);
 }
 puts("PASS: observed gaps, silence content, jitter/size/budget guards, reset, 18 real AAC/AC3 timelines with FIFO/resampler delay");
 return 0;
}
