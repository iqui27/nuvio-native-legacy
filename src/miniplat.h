// Where the seek-bar thumbnails are made on the device itself (miniatura.c)
// instead of coming from the Seekr API (seekr.c): Samsung .tpk, LG webOS and
// Android. The .wgt (WebAssembly) and the desktop builds keep Seekr.
#ifndef NV_MINIPLAT_H
#define NV_MINIPLAT_H
#if defined(NV_TPK) || defined(NV_WEBOS) || defined(NV_ANDROID)
#define NV_MINIATURAS 1
#endif
#endif
