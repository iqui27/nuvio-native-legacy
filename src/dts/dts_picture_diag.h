#ifndef NV_DTS_PICTURE_DIAG_H
#define NV_DTS_PICTURE_DIAG_H
/* Picture diagnostics (sampled HEVC structure, timing, engine/worker/native
 * copies, [hevc-annexb] decisions) were added to chase the LG HDR10/DV8
 * picture fault. They now need NV_PICTURE_DIAG on top of the buffer/HTTP
 * diagnostics (NV_HTTP_BUFFER_DIAG), so release builds keep buffer data and
 * HTTP logs without the per-picture lines. */
#if defined(NV_HTTP_BUFFER_DIAG) && defined(NV_PICTURE_DIAG)
#define NV_DTS_PICTURE_DIAG 1
#endif
#endif
