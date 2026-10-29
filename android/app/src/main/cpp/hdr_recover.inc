/* Called with Android_ActivityMutex held, after SDL consumed the EGL error. */
    if (SDL_GetHintBoolean("NUVIO_HDR_UI", SDL_FALSE) && retval < 0 &&
        SDL_strstr(SDL_GetError(), "EGL_BAD_SURFACE")) {
        SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
        SDL_GLContext context = SDL_GL_GetCurrentContext();
        SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, NULL);
        SDL_EGL_DestroySurface(_this, data->egl_surface);
        data->egl_surface = EGL_NO_SURFACE;
        if (data->native_window) ANativeWindow_release(data->native_window);
        data->native_window = Android_JNI_GetNativeWindow();
        if (data->native_window)
            data->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType)data->native_window);
        if (data->egl_surface != EGL_NO_SURFACE && context &&
            SDL_EGL_MakeCurrent(_this, data->egl_surface, context) == 0) {
            /* Render the next frame before swapping the replacement buffer. */
            retval = 0;
        } else {
            retval = SDL_SetError("HDR EGL surface recovery failed");
        }
    }
