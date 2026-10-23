/* Inserted into SDL_EGL_CreateSurface in the build tree only. */
#ifdef SDL_VIDEO_DRIVER_ANDROID
    if (SDL_GetHintBoolean("NUVIO_HDR_UI", SDL_FALSE)) {
        const char *ext = _this->egl_data->eglQueryString(_this->egl_data->egl_display, EGL_EXTENSIONS);
        const SDL_bool pq = SDL_EGL_HasExtension(_this, SDL_EGL_DISPLAY_EXTENSION, "EGL_EXT_gl_colorspace_bt2020_pq");
        EGLint r = 0, g = 0, b = 0, a = 0, component = 0;
        PFNEGLQUERYSURFACEPROC querySurface = (PFNEGLQUERYSURFACEPROC)SDL_EGL_GetProcAddress(_this, "eglQuerySurface");
        int i, first, count;
        const struct { int32_t space; const char *name; } attempts[] = {
            { ADATASPACE_BT2020_PQ, "pq" },
            { ADATASPACE_BT2020_ITU_PQ, "pq-limited" },
            { ADATASPACE_BT2020_HLG, "hlg" },
            { ADATASPACE_SCRGB_LINEAR, "scrgb-linear" },
            { STANDARD_BT2020 | TRANSFER_LINEAR | RANGE_EXTENDED, "bt2020-linear" }
        };
        SDL_Log("[hdr-ui] EGL extensions: %s", ext ? ext : "none");
        SDL_SetHint("NUVIO_HDR_EGL", "none");
        SDL_SetHint("NUVIO_HDR_CONFIG", "unknown");
        nvHdrWindow = NULL;
        _this->egl_data->eglGetConfigAttrib(_this->egl_data->egl_display, _this->egl_data->egl_config, EGL_RED_SIZE, &r);
        _this->egl_data->eglGetConfigAttrib(_this->egl_data->egl_display, _this->egl_data->egl_config, EGL_GREEN_SIZE, &g);
        _this->egl_data->eglGetConfigAttrib(_this->egl_data->egl_display, _this->egl_data->egl_config, EGL_BLUE_SIZE, &b);
        _this->egl_data->eglGetConfigAttrib(_this->egl_data->egl_display, _this->egl_data->egl_config, EGL_ALPHA_SIZE, &a);
        if (r == 16 && g == 16 && b == 16 && a == 16)
            _this->egl_data->eglGetConfigAttrib(_this->egl_data->egl_display, _this->egl_data->egl_config, EGL_COLOR_COMPONENT_TYPE_EXT, &component);
        SDL_Log("[hdr-ui] config=%d/%d/%d/%d component=0x%x", r, g, b, a, component);
        if (r == 10 && g == 10 && b == 10 && a == 2) { first = 0; count = 3; }
        else if (r == 16 && g == 16 && b == 16 && a == 16 && component == EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT) { first = 3; count = 5; }
        else {
            SDL_SetError("HDR config rejected: %d/%d/%d/%d component=0x%x", r, g, b, a, component);
            SDL_Log("[hdr-ui] %s", SDL_GetError());
            return EGL_NO_SURFACE;
        }
        if (android_get_device_api_level() < 28 || !nvHdrLoad()) {
            SDL_SetError("HDR dataspace symbol unavailable (API %d)", android_get_device_api_level());
            SDL_Log("[hdr-ui] %s", SDL_GetError());
            return EGL_NO_SURFACE;
        }
        for (i = first; i < count; ++i) {
            EGLint originalAttr = attribs[attr - 1];
            int usePq = pq && i == 0;
            nvHdrTag(nw, attempts[i].space, attempts[i].name, "before-create");
            if (usePq) {
                attribs[attr - 1] = 0x309D; /* EGL_GL_COLORSPACE_KHR */
                attribs[attr] = 0x3340; /* EGL_GL_COLORSPACE_BT2020_PQ_EXT */
                attribs[attr + 1] = EGL_NONE;
            }
            surface = _this->egl_data->eglCreateWindowSurface(
                _this->egl_data->egl_display, _this->egl_data->egl_config, nw, attribs);
            attribs[attr - 1] = originalAttr;
            if (surface == EGL_NO_SURFACE) {
                EGLint error = _this->egl_data->eglGetError();
                SDL_Log("[hdr-ui] attempt=%s stage=create egl=0x%04x", attempts[i].name, error);
                continue;
            }
            /* Verify that the created surface belongs to the config we inspected. */
            EGLint surfaceConfig = 0, selectedConfig = 0;
            if (!querySurface || !querySurface(_this->egl_data->egl_display, surface, EGL_CONFIG_ID, &surfaceConfig) ||
                !_this->egl_data->eglGetConfigAttrib(_this->egl_data->egl_display, _this->egl_data->egl_config, EGL_CONFIG_ID, &selectedConfig) ||
                surfaceConfig != selectedConfig) {
                _this->egl_data->eglDestroySurface(_this->egl_data->egl_display, surface);
                continue;
            }
            SDL_Log("[hdr-ui] attempt=%s stage=create egl=ok colorspace=%s config=%d/%d/%d/%d component=0x%x id=%d",
                    attempts[i].name, usePq ? "pq" : "default", r, g, b, a, component, surfaceConfig);
            if (nvHdrTag(nw, attempts[i].space, attempts[i].name, "after-create") == 0) {
                nvHdrWindow = nw;
                nvHdrSurface = surface;
                nvHdrDataspace = attempts[i].space;
                nvHdrName = attempts[i].name;
                nvHdrFirstSwap = 0;
                nvHdrMetadata(_this, surface);
                format_got = ANativeWindow_getFormat(nw);
                Android_SetFormat(format_wanted, format_got);
                SDL_SetHint("NUVIO_HDR_CONFIG", first == 0 ? "1010102" : "FP16");
                SDL_SetHint("NUVIO_HDR_EGL", attempts[i].name);
                return surface;
            }
            _this->egl_data->eglDestroySurface(_this->egl_data->egl_display, surface);
        }
        nvHdrTag(nw, ADATASPACE_UNKNOWN, "sdr-reset", "fallback");
        SDL_SetError("HDR dataspace attempts exhausted");
        return EGL_NO_SURFACE;
    }
#endif
