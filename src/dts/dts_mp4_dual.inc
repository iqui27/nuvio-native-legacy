/* Included only by the opt-in MP4 test build. The ordinary MKV/MP4 reader is
 * unchanged unless this particular engine was explicitly opened for the test.
 * Bounded compressed BL/RPU queues are paired by exact timestamps; four
 * HTTP slots are reserved before any range workers start. */
#include "mp4_hevc_sps.h"

static int mp4_sps_packet(const uint8_t *p,size_t n,int length,NvMp4Sps *s) {
  if(!p || n>NV_MP4_PACKET_MAX || length<1 || length>4)return -1;
  for(unsigned count=0;n && count<512;count++) {
    if(n<(size_t)length)return -1;
    uint32_t size=0;for(int i=0;i<length;i++)size=(size<<8)|p[i];
    p+=length;n-=(size_t)length;
    if(size<2 || size>n)return -1;
    if(((p[0]>>1)&63)==33)return nv_mp4_sps(p,size,s);
    p+=size;n-=size;
  }
  return n?-1:0;
}
static int mp4_sps_hvcc(const AVCodecParameters *p,NvMp4Sps *s) {
  const uint8_t *h=p->extradata;size_t n=(size_t)p->extradata_size,at=23;
  if(!h || n<23 || n>NV_MP4_PACKET_MAX || h[0]!=1)return -1;
  unsigned total=0;
  for(unsigned a=0;a<h[22];a++) {
    if(n-at<3)return -1;
    unsigned type=h[at]&63,count=((unsigned)h[at+1]<<8)|h[at+2];at+=3;
    if(count>512-total)return -1;
    total+=count;
    for(unsigned k=0;k<count;k++) {
      if(n-at<2)return -1;
      unsigned size=((unsigned)h[at]<<8)|h[at+1];at+=2;
      if(size<2 || size>n-at)return -1;
      if(type==33)return nv_mp4_sps(h+at,size,s);
      at+=size;
    }
  }
  return 0;
}
static int mp4_restore_hdr(DtsEngine *e,int base,int verify_sps) {
  AVCodecParameters *p=e->fmt->streams[base]->codecpar;
  if(!verify_sps && p->color_primaries!=AVCOL_PRI_UNSPECIFIED && p->color_trc!=AVCOL_TRC_UNSPECIFIED)return 0;
  NvMp4Sps s={0};int result=mp4_sps_hvcc(p,&s),read=0;
  if(!result) {
    AVPacket *packet=av_packet_alloc();
    if(!packet)return fail(e,"MP4 SPS probe allocation",AVERROR(ENOMEM));
    size_t bytes=0;
    /* This temporary packet is covered by the existing 16 MiB reservation.
     * Probe only at initial open, before audio setup, frame delivery or resume. */
    for(unsigned i=0;i<64 && bytes<NV_MP4_PACKET_MAX && !result;i++) {
      if(cancelled(e)) {result=-1;break;}
      int r=av_read_frame(e->fmt,packet);read=1;
      if(r<0) {result=-1;break;}
      if(packet->size<0 || (size_t)packet->size>NV_MP4_PACKET_MAX-bytes)result=-1;
      else {
        bytes+=(size_t)packet->size;
        if(packet->stream_index==base)result=mp4_sps_packet(packet->data,(size_t)packet->size,(p->extradata[21]&3)+1,&s);
      }
      av_packet_unref(packet);
    }
    av_packet_free(&packet);
  }
  if(result!=1)return fail(e,"MP4 base SPS missing or invalid within probe budget",0);
  printf("[mp4-bd-test] base SPS: size=%dx%d depth=%d hdr=%d/%d/%d; from=%s\n",
         s.width,s.height,s.depth,s.primaries,s.transfer,s.matrix,read?"in-band":"hvcC");fflush(stdout);
  if(s.width!=p->width || s.height!=p->height || s.depth!=10 ||
     s.primaries!=AVCOL_PRI_BT2020 || s.transfer!=AVCOL_TRC_SMPTE2084)
    return fail(e,"MP4 base SPS does not confirm HDR10 Main10",0);
  if(verify_sps && s.matrix!=AVCOL_SPC_BT2020_NCL)
    return fail(e,"MP4 P8 base SPS does not confirm BT2020 non-constant matrix",0);
  if((p->color_primaries!=AVCOL_PRI_UNSPECIFIED && (int)p->color_primaries!=s.primaries) ||
     (p->color_trc!=AVCOL_TRC_UNSPECIFIED && (int)p->color_trc!=s.transfer) ||
     (p->color_space!=AVCOL_SPC_UNSPECIFIED && (int)p->color_space!=s.matrix))
    return fail(e,"MP4 container and base SPS colour conflict",0);
  if(read) {
    int r=av_seek_frame(e->fmt,base,0,AVSEEK_FLAG_BACKWARD);
    if(r<0)return fail(e,"MP4 SPS probe rewind",r);
    avformat_flush(e->fmt);
  }
  p->color_primaries=s.primaries;p->color_trc=s.transfer;p->color_space=s.matrix;
  if(p->color_range==AVCOL_RANGE_UNSPECIFIED)p->color_range=s.full_range?AVCOL_RANGE_JPEG:AVCOL_RANGE_MPEG;
  puts("[mp4-bd-test] restored HDR10 colour metadata from base SPS; no video decode");fflush(stdout);
  return 0;
}
static void mp4_pair_reset(DtsEngine *e) {
  NvMp4Pair *m=e->mp4;
  if(!m)return;
  for(int i=0;i<MP4_BASE_QUEUE;i++)av_packet_unref(m->base[i]);
  for(int i=0;i<MP4_RPU_QUEUE;i++)m->rpu[i].size=0;
  m->base_count=m->rpu_count=m->aligned=m->align_drops=0;m->base_bytes=0;
  m->anchor_valid=m->first_base_seen=m->preroll_skipped=0;m->preroll_bytes=0;
}
/* MOV seeks the tracks independently. A coarse EL sync index can deliver
 * obsolete RPUs long before the first BL keyframe. Determine the actual BL
 * anchor, then repeat the identical seek to retain all required A/V packets.
 * The probe and discard are bounded; neither queue nor range budget grows. */
static int mp4_seek_anchor(DtsEngine *e,int64_t timestamp) {
  NvMp4Pair *m=e->mp4;
  int r=avformat_seek_file(e->fmt,-1,INT64_MIN,timestamp,timestamp,AVSEEK_FLAG_BACKWARD);
  if(r<0)return r;
  AVPacket *p=av_packet_alloc();
  if(!p)return AVERROR(ENOMEM);
  unsigned count=0;size_t bytes=0;int found=0;
  while(count<1024 && bytes<2*NV_MP4_PACKET_MAX) {
    if(cancelled(e))break;
    r=av_read_frame(e->fmt,p);if(r<0)break;
    count++;
    if(p->size<0 || (unsigned)p->size>NV_MP4_PACKET_MAX ||
       (unsigned)p->size>2*NV_MP4_PACKET_MAX-bytes ||
       av_packet_get_side_data(p,AV_PKT_DATA_ENCRYPTION_INFO,NULL))break;
    bytes+=(size_t)p->size;
    if(p->stream_index==e->info.video_stream) {
      if(p->dts!=AV_NOPTS_VALUE && p->pts!=AV_NOPTS_VALUE && (p->flags&AV_PKT_FLAG_KEY)) {
        m->anchor_dts=p->dts;m->anchor_pts=p->pts;found=1;
      }
      break;
    }
    av_packet_unref(p);
  }
  av_packet_free(&p);
  if(!found)return fail(e,"MP4 seek base keyframe absent within probe budget",0);
  r=avformat_seek_file(e->fmt,-1,INT64_MIN,timestamp,timestamp,AVSEEK_FLAG_BACKWARD);
  if(r<0)return r;
  m->anchor_valid=1;
  printf("[mp4-bd-test] seek anchor: target=%.3f BL-dts-ns=%lld BL-pts-ns=%lld probePackets=%u probeBytes=%zu\n",
    timestamp/1000000.0,(long long)ns(m->anchor_dts,e->fmt->streams[e->info.video_stream]->time_base),
    (long long)ns(m->anchor_pts,e->fmt->streams[e->info.video_stream]->time_base),count,bytes);fflush(stdout);
  return 0;
}
static void mp4_pop_base(NvMp4Pair *m) {
  AVPacket *first=m->base[0];m->base_bytes-=(size_t)first->size;
  av_packet_unref(first);
  for(int i=1;i<m->base_count;i++)m->base[i-1]=m->base[i];
  m->base[--m->base_count]=first;
}
static int mp4_p8_first_rpu(DtsEngine *e,int base) {
  AVPacket *p=av_packet_alloc();if(!p)return fail(e,"MP4 P8 probe allocation",AVERROR(ENOMEM));
  uint8_t rpu[NV_MP4_RPU_MAX];size_t bytes=0,size=0;int valid=0;
  for(int i=0;i<64 && bytes<NV_MP4_PACKET_MAX;i++) {
    if(cancelled(e) || av_read_frame(e->fmt,p)<0)break;
    if(p->size<0 || (unsigned)p->size>NV_MP4_PACKET_MAX-bytes ||
       av_packet_get_side_data(p,AV_PKT_DATA_ENCRYPTION_INFO,NULL))break;
    bytes+=(size_t)p->size;
    if(p->stream_index==base) {
      AVCodecParameters *c=e->fmt->streams[base]->codecpar;
      valid=nv_mp4_rpu(p->data,p->size,(c->extradata[21]&3)+1,rpu,&size)==0 && nv_mp4_p8_valid(rpu,size);
      break;
    }
    av_packet_unref(p);
  }
  av_packet_free(&p);
  if(!valid)return fail(e,"MP4 first P8 RPU absent or invalid within probe budget",0);
  int r=av_seek_frame(e->fmt,base,0,AVSEEK_FLAG_BACKWARD);
  if(r<0)return fail(e,"MP4 P8 probe rewind",r);
  avformat_flush(e->fmt);
  printf("[mp4-bd-test] first P8 RPU validated: bytes=%zu; metadata unchanged\n",size);fflush(stdout);
  return 0;
}
/* Return 1 only for a verified single-track HDR10-compatible P8.1 source.
 * Reuse the ordinary Annex-B path; never run the P7 metadata converter. */
static int mp4_single_p8(DtsEngine *e) {
  int base=-1,videos=0;
  const AVDOVIDecoderConfigurationRecord *c=NULL;
  for(unsigned i=0;i<e->fmt->nb_streams;i++) {
    AVCodecParameters *p=e->fmt->streams[i]->codecpar;
    if(p->codec_type!=AVMEDIA_TYPE_VIDEO || (e->fmt->streams[i]->disposition&AV_DISPOSITION_ATTACHED_PIC))continue;
    videos++;base=(int)i;
    const AVPacketSideData *d=av_packet_side_data_get(p->coded_side_data,p->nb_coded_side_data,AV_PKT_DATA_DOVI_CONF);
    if(d && d->size>=sizeof *c)c=(const AVDOVIDecoderConfigurationRecord *)d->data;
  }
  if(videos!=1 || !c || c->dv_profile!=8)return 0;
  AVStream *b=e->fmt->streams[base];AVCodecParameters *p=b->codecpar;
  int compatibility=c->dv_bl_signal_compatibility_id;
  printf("[mp4-bd-test] single P8 track=%d videos=%d version=%d.%d compat=%d bl=%d el=%d rpu=%d size=%dx%d hdr=%d/%d/%d hvcc=%d\n",
    base,videos,c->dv_version_major,c->dv_version_minor,c->dv_bl_signal_compatibility_id,
    c->bl_present_flag,c->el_present_flag,c->rpu_present_flag,p->width,p->height,
    p->color_primaries,p->color_trc,p->color_space,p->extradata_size);fflush(stdout);
  if(c->dv_version_major!=1 || c->dv_version_minor || (compatibility!=0 && compatibility!=1) ||
     !c->bl_present_flag || c->el_present_flag || !c->rpu_present_flag || p->codec_id!=AV_CODEC_ID_HEVC ||
     p->width<=0 || p->height<=0 || b->time_base.num<=0 || b->time_base.den<=0 ||
     b->avg_frame_rate.num<=0 || b->avg_frame_rate.den<=0 || !p->extradata ||
     p->extradata_size<23 || p->extradata[0]!=1)
    return fail(e,"MP4 P8 recovery requires a single verified P8.1 track",0);
  /* A zero compatibility ID is not proof of HDR10. Require the real Main10
   * 4:2:0 PQ/BT.2020 SPS even when the container already advertises HDR10. */
  if(mp4_restore_hdr(e,base,compatibility==0)<0)return -1;
  if(p->color_primaries!=AVCOL_PRI_BT2020 || p->color_trc!=AVCOL_TRC_SMPTE2084)
    return fail(e,"MP4 P8 base colour metadata is not HDR10",0);
  if(mp4_p8_first_rpu(e,base)<0)return -1;
  if(compatibility==0) {
    e->mp4_p8_compat_verified=1;
    puts("[mp4-bd-test] P8 compatibility verified: input=0 output=1; HDR10 SPS and P8 RPU confirmed");fflush(stdout);
  }
  e->info.video_stream=base;e->dv7=0;
  printf("[mp4-bd-test] verified single-track P8.1: video=%d; RPU passthrough; reserved=16 MiB\n",base);fflush(stdout);
  return 1;
}
static int mp4_configure(DtsEngine *e) {
  if(!e->allow_mp4)return 0;
  if(!e->dv ||
     !e->fmt->iformat || !strstr(e->fmt->iformat->name,"mov"))
    return fail(e,"MP4 test requires a Dolby Vision MP4",0);
  int single=mp4_single_p8(e);if(single)return single<0?-1:0;
  if(!e->allow_p7 || !nv_dv7_available())return fail(e,"MP4 P7 conversion unavailable",0);
  int base=-1,el=-1,videos=0;
  for(unsigned i=0;i<e->fmt->nb_streams;i++) {
    AVCodecParameters *p=e->fmt->streams[i]->codecpar;
    if(p->codec_type!=AVMEDIA_TYPE_VIDEO || (e->fmt->streams[i]->disposition&AV_DISPOSITION_ATTACHED_PIC))continue;
    videos++;
    if(p->codec_id!=AV_CODEC_ID_HEVC)return fail(e,"MP4 test requires HEVC BL and EL",0);
    const AVPacketSideData *d=av_packet_side_data_get(p->coded_side_data,p->nb_coded_side_data,AV_PKT_DATA_DOVI_CONF);
    if(d && d->size>=sizeof(AVDOVIDecoderConfigurationRecord)) {
      const AVDOVIDecoderConfigurationRecord *c=(const AVDOVIDecoderConfigurationRecord *)d->data;
      printf("[mp4-bd-test] track=%u tag=%08x profile=%d bl=%d el=%d rpu=%d compat=%d\n",i,p->codec_tag,
             c->dv_profile,c->bl_present_flag,c->el_present_flag,c->rpu_present_flag,c->dv_bl_signal_compatibility_id);fflush(stdout);
      if(c->dv_profile!=7 || c->bl_present_flag || !c->el_present_flag || !c->rpu_present_flag || el>=0)
        return fail(e,"MP4 test refuses unrelated Dolby Vision tracks",0);
      el=(int)i;
    } else {
      if(base>=0)return fail(e,"MP4 test has ambiguous base tracks",0);
      base=(int)i;
    }
  }
  if(videos!=2 || base<0 || el<0)return fail(e,"MP4 test requires separate BL and P7 EL tracks",0);
  AVStream *b=e->fmt->streams[base],*v=e->fmt->streams[el];
  AVCodecParameters *bp=b->codecpar,*ep=v->codecpar;
  printf("[mp4-bd-test] BL track=%d size=%dx%d hdr=%d/%d/%d fps=%d/%d timebase=%d/%d hvcc=%d; EL track=%d size=%dx%d fps=%d/%d timebase=%d/%d hvcc=%d\n",
         base,bp->width,bp->height,bp->color_primaries,bp->color_trc,bp->color_space,
         b->avg_frame_rate.num,b->avg_frame_rate.den,b->time_base.num,b->time_base.den,bp->extradata_size,
         el,ep->width,ep->height,v->avg_frame_rate.num,v->avg_frame_rate.den,v->time_base.num,v->time_base.den,ep->extradata_size);
  fflush(stdout);
  if(bp->width<=0 || bp->height<=0 || ep->width<=0 || ep->height<=0 || bp->width<ep->width || bp->height<ep->height)
    return fail(e,"MP4 BL/EL dimensions are incompatible",0);
  if(b->time_base.num<=0 || b->time_base.den<=0 || v->time_base.num<=0 || v->time_base.den<=0)
    return fail(e,"MP4 BL/EL time bases are invalid",0);
  if(b->avg_frame_rate.num<=0 || b->avg_frame_rate.den<=0 || v->avg_frame_rate.num<=0 ||
     v->avg_frame_rate.den<=0 || av_cmp_q(b->avg_frame_rate,v->avg_frame_rate))
    return fail(e,"MP4 BL/EL frame rates are incompatible",0);
  if(!bp->extradata || !ep->extradata || bp->extradata_size<23 || ep->extradata_size<23 || bp->extradata[0]!=1 || ep->extradata[0]!=1)
    return fail(e,"MP4 BL/EL HEVC configuration is invalid",0);
  if(mp4_restore_hdr(e,base,0)<0)return -1;
  if(bp->color_trc!=AVCOL_TRC_SMPTE2084 || bp->color_primaries!=AVCOL_PRI_BT2020)
    return fail(e,"MP4 base colour metadata is not HDR10",0);
  e->mp4=av_mallocz(sizeof *e->mp4);
  if(!e->mp4)return fail(e,"MP4 pairing allocation",AVERROR(ENOMEM));
  for(int i=0;i<MP4_BASE_QUEUE;i++) {
    e->mp4->base[i]=av_packet_alloc();
    if(!e->mp4->base[i])return fail(e,"MP4 pairing packet allocation",AVERROR(ENOMEM));
  }
  e->mp4->base_length=(bp->extradata[21]&3)+1;
  e->mp4->el_length=(ep->extradata[21]&3)+1;
  e->mp4->el_stream=el;e->info.video_stream=base;
  mp4_pair_reset(e);
  printf("[mp4-bd-test] verified dual-track P7: BL=%d %dx%d EL=%d %dx%d; output=P8.1; reserved=16 MiB\n",
         base,bp->width,bp->height,el,ep->width,ep->height);fflush(stdout);
  return 0;
}
static int mp4_read_packet(DtsEngine *e) {
  NvMp4Pair *m=e->mp4;
  for(;;) {
    if(cancelled(e))return fail(e,"MP4 pairing cancelled",0);
    if(m->base_count) {
      AVPacket *base=m->base[0];
      AVStream *b=e->fmt->streams[e->info.video_stream],*v=e->fmt->streams[m->el_stream];
      if(base->pts==AV_NOPTS_VALUE || base->dts==AV_NOPTS_VALUE)
        return fail(e,"MP4 base timestamps absent",0);
      int match=-1,future=0;
      for(int i=0;i<MP4_RPU_QUEUE;i++)if(m->rpu[i].size) {
        if(m->rpu[i].pts==AV_NOPTS_VALUE || m->rpu[i].dts==AV_NOPTS_VALUE)
          return fail(e,"MP4 RPU timestamps absent",0);
        int order=av_compare_ts(base->dts,b->time_base,m->rpu[i].dts,v->time_base);
        if(!order) {
          if(av_compare_ts(base->pts,b->time_base,m->rpu[i].pts,v->time_base))
            return fail(e,"MP4 BL/EL presentation timestamps do not match",0);
          match=i;break;
        }
        if(order<0)future=1;
        else if(!m->aligned) {
          m->rpu[i].size=0;m->rpu_count--;m->align_drops++;
        }
      }
      if(m->align_drops>128)return fail(e,"MP4 seek alignment limit",0);
      if(match>=0) {
        size_t size=m->rpu[match].size;int length=m->base_length,old=base->size;
        if((length<4 && size>=(UINT32_C(1)<<(8*length))) ||
           (unsigned)old>NV_MP4_PACKET_MAX-size-(unsigned)length)
          return fail(e,"MP4 paired video packet exceeds its budget",0);
        int r=av_grow_packet(base,length+(int)size);
        if(r<0)return fail(e,"MP4 paired video allocation",r);
        for(int k=0;k<length;k++)base->data[old+k]=(uint8_t)(size>>(8*(length-k-1)));
        memcpy(base->data+old+length,m->rpu[match].data,size);
        m->rpu[match].size=0;m->rpu_count--;
        if(!m->aligned) {
          printf("[mp4-bd-test] BL/RPU aligned: dropped-preroll=%d pts-ns=%lld\n",m->align_drops,(long long)ns(base->pts,b->time_base));fflush(stdout);
        }
        m->aligned=1;
        /* The paired packet now belongs to the existing output/filter path. */
        m->base_bytes-=(size_t)old;
        av_packet_move_ref(e->in,base);
        mp4_pop_base(m);
        return 0;
      }
      if(future) {
        if(m->aligned)return fail(e,"MP4 RPU missing for a base picture",0);
        mp4_pop_base(m);m->align_drops++;continue;
      }
    }
    int r=av_read_frame(e->fmt,e->in);
    if(r<0) {
      if(r==AVERROR_EOF && (m->base_count || m->rpu_count))
        return fail(e,"MP4 ended with an unpaired BL/EL frame",0);
      return r;
    }
    if(av_packet_get_side_data(e->in,AV_PKT_DATA_ENCRYPTION_INFO,NULL)) {
      av_packet_unref(e->in);return fail(e,"Encrypted MP4 packet unsupported",0);
    }
    if(e->in->stream_index==e->info.video_stream) {
      if(m->anchor_valid && !m->first_base_seen) {
        if(e->in->dts!=m->anchor_dts || e->in->pts!=m->anchor_pts || !(e->in->flags&AV_PKT_FLAG_KEY))
          return fail(e,"MP4 seek base anchor changed after rewind",0);
        m->first_base_seen=1;
      }
      if(m->base_count==MP4_BASE_QUEUE || e->in->size<=0 ||
         (unsigned)e->in->size>NV_MP4_PACKET_MAX-m->base_bytes)
        return fail(e,"MP4 BL interleave exceeds its bounded queue",0);
      m->base_bytes+=(size_t)e->in->size;
      av_packet_move_ref(m->base[m->base_count++],e->in);
    } else if(e->in->stream_index==m->el_stream) {
      if(e->in->dts==AV_NOPTS_VALUE || e->in->pts==AV_NOPTS_VALUE)
        return fail(e,"MP4 RPU timestamps absent",0);
      if(m->anchor_valid && !m->aligned &&
         av_compare_ts(e->in->dts,e->fmt->streams[m->el_stream]->time_base,
                       m->anchor_dts,e->fmt->streams[e->info.video_stream]->time_base)<0) {
        if(++m->preroll_skipped>1024 || e->in->size<0 ||
           (unsigned)e->in->size>2*NV_MP4_PACKET_MAX-m->preroll_bytes)
          return fail(e,"MP4 obsolete EL preroll exceeds scan budget",0);
        m->preroll_bytes+=(size_t)e->in->size;
        av_packet_unref(e->in);continue;
      }
      int slot=0;while(slot<MP4_RPU_QUEUE && m->rpu[slot].size)slot++;
      if(slot==MP4_RPU_QUEUE)return fail(e,"MP4 RPU interleave exceeds its bounded queue",0);
      if(nv_mp4_rpu(e->in->data,e->in->size,m->el_length,m->rpu[slot].data,&m->rpu[slot].size)<0)
        return fail(e,"MP4 EL has missing, duplicate or invalid RPU",0);
      m->rpu[slot].pts=e->in->pts;m->rpu[slot].dts=e->in->dts;m->rpu_count++;
      av_packet_unref(e->in);
    } else return 0;
  }
}
