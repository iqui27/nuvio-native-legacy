/*
 * libass NEON32 engine check (tools/tpk/libass-nv). CI only: runs under
 * qemu-arm in the TPK build image, never on the TV and never in tests/.
 *
 *   verify <font dir> <file.ass>...
 *
 * Renders every sample on a fixed time grid at 1920x1080 with the plain C
 * bitmap engine, then again with the NEON engine (ass_nv_set_engine, cold
 * caches both times), and compares every ASS_Image byte for byte: position,
 * size, colour and the w bytes of every row. Exit status 1 on any difference.
 * Also checks the NEON engine with 4 blur threads, and that the dialogue
 * opacity (ass_nv_set_dialogue_alpha, 0003) only changes colours.
 * 0004: the sharp + soft split passes at the SAME size, merged by
 * ass_nv_image_key, give exactly the normal frame; the soft pass at half
 * size runs; events marked nv_sign get no dialogue opacity; skipping the
 * dialogue leaves exactly the frame with every dialogue line taken out.
 */
/* 64-bit inodes (CI overlay fs): without this, 32-bit readdir() fails with
 * EOVERFLOW and no font gets loaded. */
#define _FILE_OFFSET_BITS 64

#include <ass/ass.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W 1920
#define H 1080
#define STEP_MS 83
#define MAX_FRAMES 4096

typedef struct { uint64_t hash; unsigned imgs; } Frame;

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
    const unsigned char *b = p;
    while (n--) { h ^= *b++; h *= 1099511628211ULL; }
    return h;
}

static Frame hash_frame(ASS_Image *img)
{
    Frame f = { 1469598103934665603ULL, 0 };
    for (; img; img = img->next) {
        int hdr[5] = { img->w, img->h, img->dst_x, img->dst_y, (int) img->color };
        f.hash = fnv(f.hash, hdr, sizeof(hdr));
        for (int y = 0; y < img->h; y++)
            f.hash = fnv(f.hash, img->bitmap + (size_t) y * img->stride, img->w);
        f.imgs++;
    }
    return f;
}

/* The same without the alpha byte of the colour (dialogue opacity check);
 * *faded counts images at least half transparent. */
static Frame hash_frame_noalpha(ASS_Image *img, unsigned *faded)
{
    Frame f = { 1469598103934665603ULL, 0 };
    for (; img; img = img->next) {
        int hdr[5] = { img->w, img->h, img->dst_x, img->dst_y, (int) (img->color & ~0xFFu) };
        f.hash = fnv(f.hash, hdr, sizeof(hdr));
        for (int y = 0; y < img->h; y++)
            f.hash = fnv(f.hash, img->bitmap + (size_t) y * img->stride, img->w);
        f.imgs++;
        if ((img->color & 0xFF) >= 0x80) (*faded)++;
    }
    return f;
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static int n_fonts, n_msgs;
static unsigned faded_total, soft_total;

static void load_fonts(ASS_Library *lib, const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) { fprintf(stderr, "no font dir %s\n", dir); exit(2); }
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n < 5 || (strcmp(e->d_name + n - 4, ".ttf") && strcmp(e->d_name + n - 4, ".otf")))
            continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        FILE *fp = fopen(path, "rb");
        if (!fp) continue;
        fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
        char *buf = malloc(sz);
        if (fread(buf, 1, sz, fp) == (size_t) sz) {
            ass_add_font(lib, e->d_name, buf, sz);
            n_fonts++;
        }
        free(buf);
        fclose(fp);
    }
    closedir(d);
    if (!n_fonts) { fprintf(stderr, "no fonts loaded from %s\n", dir); exit(2); }
}

static void quiet(int level, const char *fmt, va_list va, void *data)
{
    (void) data;
    if (level > 2 || n_msgs >= 40) return;
    n_msgs++;
    fprintf(stderr, "  libass[%d]: ", level);
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
}

static long long track_end(ASS_Track *t)
{
    long long end = 0;
    for (int i = 0; i < t->n_events; i++) {
        long long e = t->events[i].Start + t->events[i].Duration;
        if (e > end) end = e;
    }
    return end;
}

static ASS_Renderer *renderer(ASS_Library *lib, int simd)
{
    ASS_Renderer *r = ass_renderer_init(lib);
    if (!r) return NULL;
    ass_set_frame_size(r, W, H);
    ass_set_storage_size(r, W, H);
    ass_set_fonts(r, NULL, "Roboto", ASS_FONTPROVIDER_NONE, NULL, 0);
    int got = ass_nv_set_engine(r, simd);
    if (got != simd) {
        fprintf(stderr, "engine %d requested, %d in use\n", simd, got);
        ass_renderer_done(r);
        return NULL;
    }
    return r;
}

static int run(ASS_Library *lib, const char *file, int simd, int threads, Frame *out, int *n_out, double *ms)
{
    ass_nv_set_threads(threads);
    ASS_Renderer *r = renderer(lib, simd);
    if (!r) return -1;
    ASS_Track *t = ass_read_file(lib, (char *) file, NULL);
    if (!t) { fprintf(stderr, "cannot read %s\n", file); return -1; }
    long long end = track_end(t);
    if (!simd)
        printf("  track: %d events, %d styles, end %lld ms, %d fonts added\n", t->n_events, t->n_styles, end, n_fonts);
    int n = 0;
    unsigned imgs = 0;
    double t0 = now_ms();
    for (long long ms_t = 0; ms_t < end && n < MAX_FRAMES; ms_t += STEP_MS) {
        out[n] = hash_frame(ass_render_frame(r, t, ms_t, NULL));
        imgs += out[n++].imgs;
    }
    *ms = now_ms() - t0;
    *n_out = n;
    printf("  %-4s x%d %4d frames %6u images  %9.1f ms\n", simd ? "NEON" : "C", ass_nv_get_threads(), n, imgs, *ms);
    ass_free_track(t);
    ass_renderer_done(r);
    return 0;
}

/* Dialogue opacity 127 (half): the bitmaps and positions must be the normal
 * pass's, only alpha bytes change; 255 must be the normal pass. (At 0 libass
 * drops the invisible images altogether.) */
static int run_alpha(ASS_Library *lib, const char *file, int alpha, int noalpha, Frame *out, int frames, unsigned *faded)
{
    ASS_Renderer *r = renderer(lib, 1);
    if (!r) return -1;
    ass_nv_set_dialogue_alpha(r, alpha);
    ASS_Track *t = ass_read_file(lib, (char *) file, NULL);
    if (!t) return -1;
    for (int i = 0; i < frames; i++) {
        ASS_Image *img = ass_render_frame(r, t, (long long) i * STEP_MS, NULL);
        out[i] = noalpha ? hash_frame_noalpha(img, faded) : hash_frame(img);
    }
    ass_free_track(t);
    ass_renderer_done(r);
    return 0;
}

/* 0004 ---------------------------------------------------------------- */
#define SPLIT_MAX_IMGS 8192
/* The two passes' lists merged by key (both already in key order), as the
 * app does it (assrender.c). Ties: the sharp pass first. */
static int merge_split(ASS_Image *a, ASS_Image *b, ASS_Image **out)
{
    int n = 0;
    while ((a || b) && n < SPLIT_MAX_IMGS) {
        if (a && (!b || ass_nv_image_key(a) <= ass_nv_image_key(b))) { out[n++] = a; a = a->next; }
        else { out[n++] = b; b = b->next; }
    }
    return n;
}

static Frame hash_list(ASS_Image **v, int n)
{
    Frame f = { 1469598103934665603ULL, 0 };
    for (int i = 0; i < n; i++) {
        ASS_Image *img = v[i];
        int hdr[5] = { img->w, img->h, img->dst_x, img->dst_y, (int) img->color };
        f.hash = fnv(f.hash, hdr, sizeof(hdr));
        for (int y = 0; y < img->h; y++)
            f.hash = fnv(f.hash, img->bitmap + (size_t) y * img->stride, img->w);
        f.imgs++;
    }
    return f;
}

/* ref: the normal frames (engine `simd`, 1 thread). Returns the number of
 * differing frames; *soft counts the images the soft pass drew. */
static int check_split(ASS_Library *lib, const char *file, int simd, const Frame *ref, int frames, unsigned *soft)
{
    static ASS_Image *v[SPLIT_MAX_IMGS];
    ass_nv_set_threads(1);
    ASS_Renderer *ra = renderer(lib, simd), *rb = renderer(lib, simd), *rh = renderer(lib, simd);
    if (!ra || !rb || !rh) return frames + 1;
    ass_nv_set_split(ra, 1, 0);
    ass_nv_set_split(rb, 2, 1);
    ass_nv_set_split(rh, 2, 1);
    ass_set_frame_size(rh, W / 2, H / 2);
    ASS_Track *t = ass_read_file(lib, (char *) file, NULL);
    if (!t) return frames + 1;
    int diff = 0;
    for (int k = 0; k < frames; k++) {
        long long ms_t = (long long) k * STEP_MS;
        ASS_Image *a = ass_render_frame(ra, t, ms_t, NULL);
        ASS_Image *b = ass_render_frame(rb, t, ms_t, NULL);
        for (ASS_Image *p = b; p; p = p->next) (*soft)++;
        int n = merge_split(a, b, v);
        Frame f = hash_list(v, n);
        if (f.hash != ref[k].hash || f.imgs != ref[k].imgs) {
            if (diff < 5) printf("  split DIFF at %lld ms (%u vs %u images)\n", ms_t, f.imgs, ref[k].imgs);
            diff++;
        }
        ass_render_frame(rh, t, ms_t, NULL);   /* half size: must just run */
    }
    ass_free_track(t);
    ass_renderer_done(ra); ass_renderer_done(rb); ass_renderer_done(rh);
    return diff;
}

/* Every event marked a sign: dialogue opacity 127 must change nothing; and
 * skipping dialogue must equal a track with the non-sign events removed. */
static int check_sign(ASS_Library *lib, const char *file, int simd, const Frame *ref, int frames)
{
    int diff = 0;
    ASS_Renderer *r = renderer(lib, simd);
    ASS_Track *t = ass_read_file(lib, (char *) file, NULL);
    if (!r || !t) return frames + 1;
    for (int i = 0; i < t->n_events; i++) t->events[i].nv_sign = 1;
    ass_nv_set_dialogue_alpha(r, 127);
    ass_nv_set_skip_dialogue(r, 1);
    for (int k = 0; k < frames; k++) {
        Frame f = hash_frame(ass_render_frame(r, t, (long long) k * STEP_MS, NULL));
        if (f.hash != ref[k].hash || f.imgs != ref[k].imgs) diff++;
    }
    /* skip vs removed: unmark, render with skip; a second track keeps only
     * libass's own signs (\pos etc.) by asking the same renderer rule:
     * nothing is dropped there when skip is off and every event is marked. */
    for (int i = 0; i < t->n_events; i++) t->events[i].nv_sign = 0;
    ass_nv_set_dialogue_alpha(r, 255);
    ASS_Renderer *r2 = renderer(lib, simd);
    ASS_Track *t2 = ass_read_file(lib, (char *) file, NULL);
    if (!r2 || !t2) return frames + 1;
    {   /* t2: dialogue lines (no hard override, normal Effect) deleted */
        int w = 0;
        for (int i = 0; i < t2->n_events; i++) {
            ASS_Event *e = &t2->events[i];
            const char *x = e->Text ? e->Text : "";
            int sign = (e->Effect && (!strncmp(e->Effect, "Banner", 6) || !strncmp(e->Effect, "Scroll", 6))) ||
                       strstr(x, "\\pos") || strstr(x, "\\move") ||
                       strstr(x, "\\clip") || strstr(x, "\\iclip") || strstr(x, "\\org") || strstr(x, "\\p");
            if (sign) t2->events[w++] = *e;
            else { free(e->Name); free(e->Effect); free(e->Text); free(e->render_priv); }
        }
        t2->n_events = w;
    }
    for (int k = 0; k < frames; k++) {
        Frame f = hash_frame(ass_render_frame(r, t, (long long) k * STEP_MS, NULL));
        Frame g = hash_frame(ass_render_frame(r2, t2, (long long) k * STEP_MS, NULL));
        if (f.hash != g.hash || f.imgs != g.imgs) diff++;
    }
    /* -1 (owner 19/11/2026): the app knows every line is dialogue, even with
     * \pos, \move or a drawing: with skip on nothing at all is drawn. */
    for (int i = 0; i < t->n_events; i++) t->events[i].nv_sign = -1;
    for (int k = 0; k < frames; k++) {
        Frame f = hash_frame(ass_render_frame(r, t, (long long) k * STEP_MS, NULL));
        if (f.imgs) diff++;
    }
    ass_free_track(t); ass_free_track(t2);
    ass_renderer_done(r); ass_renderer_done(r2);
    return diff;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: verify <font dir> <file.ass>...\n"); return 2; }
    if (!ass_nv_simd_available()) { fprintf(stderr, "no SIMD engine in this libass build\n"); return 1; }
    ASS_Library *lib = ass_library_init();
    ass_set_message_cb(lib, quiet, NULL);
    load_fonts(lib, argv[1]);

    static Frame a[MAX_FRAMES], b[MAX_FRAMES], c[MAX_FRAMES], d[MAX_FRAMES];
    int bad = 0;
    double tc = 0, tn = 0;
    for (int i = 2; i < argc; i++) {
        int na = 0, nb = 0, nc = 0;
        double ma, mb, mc;
        printf("%s\n", argv[i]);
        if (run(lib, argv[i], 0, 1, a, &na, &ma) || run(lib, argv[i], 1, 1, b, &nb, &mb) ||
            run(lib, argv[i], 1, 4, c, &nc, &mc)) { bad = 1; continue; }
        tc += ma; tn += mb;
        int diff = 0, imgs = 0;
        for (int k = 0; k < na; k++) {
            imgs += a[k].imgs;
            if (a[k].hash != b[k].hash || a[k].imgs != b[k].imgs ||
                a[k].hash != c[k].hash || a[k].imgs != c[k].imgs) {
                if (diff < 5)
                    printf("  DIFF at %d ms (%u vs %u images)\n", k * STEP_MS, a[k].imgs, b[k].imgs);
                diff++;
            }
        }
        if (na != nb || na != nc) diff++;
        if (!imgs) { printf("  no images rendered\n"); diff++; }
        printf("  %s: %d frames, %d differ\n", diff ? "FAIL" : "identical", na, diff);
        /* Dialogue opacity: 255 = untouched; 127 = same bitmaps, alpha only. */
        {
            static Frame e[MAX_FRAMES], g[MAX_FRAMES];
            /* d: the 255 pass without alpha, the reference for the 127 pass */
            unsigned faded0 = 0, faded = 0;
            int adiff = 0;
            int odiff = 0;
            if (run_alpha(lib, argv[i], 255, 0, e, na, &faded0) || run_alpha(lib, argv[i], 255, 1, d, na, &faded0) ||
                run_alpha(lib, argv[i], 127, 1, g, na, &faded)) {
                printf("  dialogue opacity: FAIL to run\n"); bad = 1; continue;
            }
            for (int k = 0; k < na; k++) {
                if (e[k].hash != b[k].hash || e[k].imgs != b[k].imgs) adiff++;
                if (g[k].hash != d[k].hash || g[k].imgs != d[k].imgs) odiff++;
            }
            faded = faded > faded0 ? faded - faded0 : 0;
            printf("  dialogue opacity: 255 %s, 127 changes alpha only: %s | %u images dimmed\n",
                   adiff ? "FAIL" : "identical", odiff ? "FAIL" : "yes", faded);
            diff += adiff + odiff;
            faded_total += faded;
        }
        /* 0004: split passes and the sign flag (NEON engine, b is its reference). */
        {
            unsigned soft = 0;
            int sdiff = check_split(lib, argv[i], 1, b, na, &soft);
            int gdiff = check_sign(lib, argv[i], 1, b, na);
            printf("  split sharp+soft = normal: %s (%u soft images) | signs: %s\n",
                   sdiff ? "FAIL" : "identical", soft, gdiff ? "FAIL" : "ok");
            diff += sdiff + gdiff;
            soft_total += soft;
        }
        bad |= diff != 0;
    }
    printf("total C %.0f ms, NEON %.0f ms (x%.2f; emulated, not TV speed)\n", tc, tn, tn > 0 ? tc / tn : 0);
    if (!faded_total) { printf("no image was faded: the dialogue opacity check tested nothing\n"); bad = 1; }
    if (!soft_total) { printf("no soft image: the split check tested nothing\n"); bad = 1; }
    printf(bad ? "RESULT: FAIL\n" : "RESULT: NEON engine (1 and 4 threads) bit-identical to C;"
                 " dialogue opacity changes alpha only; split = normal; signs ok\n");
    ass_library_done(lib);
    return bad;
}
