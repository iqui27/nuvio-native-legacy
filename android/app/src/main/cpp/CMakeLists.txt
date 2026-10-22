# Nucleo C do Nuvio no Android: libmain.so (o nome que o SDLActivity carrega).
#   libSDL2.so   compartilhada (o SDLActivity a carrega antes do main)
#   SDL2_image   estatica, so stb + WebP (libwebp 1.4.0 compilada daqui)
#   SDL2_ttf     estatica, freetype embutido (external/freetype do tarball)
# Os fontes vem de ~/.cache/nuvio-android/src (tools/android.sh baixa).
cmake_minimum_required(VERSION 3.22)
project(nuvio C)

set(NUVIO_RAIZ "" CACHE PATH "raiz do repositorio")
set(NUVIO_SDL_SRC "$ENV{HOME}/.cache/nuvio-android/src" CACHE PATH "fontes do SDL")
set(NUVIO_ENV_CMAKE "" CACHE FILEPATH "chaves -D geradas por tools/android.sh (fora do git)")
set(NUVIO_P2P_MOTOR "" CACHE PATH "motor P2P de tools/p2p-motor/build-android.sh (vazio = sem motor)")

set(SDL_DIR   "${NUVIO_SDL_SRC}/SDL2-2.30.9")
set(IMG_DIR   "${NUVIO_SDL_SRC}/SDL2_image-2.8.2")
set(TTF_DIR   "${NUVIO_SDL_SRC}/SDL2_ttf-2.22.0")
set(WEBP_DIR  "${NUVIO_SDL_SRC}/libwebp-1.4.0")
foreach(d SDL_DIR IMG_DIR TTF_DIR WEBP_DIR)
  if(NOT EXISTS "${${d}}")
    message(FATAL_ERROR "fonte ausente: ${${d}} (rode tools/android.sh)")
  endif()
endforeach()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---- SDL2 (sem audio: o app so reproduz video pelo ExoPlayer) ----
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC ON CACHE BOOL "" FORCE)  # SDL_android.c chama Android_AddHaptic
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI ON CACHE BOOL "" FORCE)  # SDLActivity chama HIDDeviceRegisterCallback sempre
add_subdirectory("${SDL_DIR}" "${CMAKE_BINARY_DIR}/sdl2" EXCLUDE_FROM_ALL)

# ---- libwebp (so decodificador + demux) ----
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_LIBWEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_ENABLE_SWAP_16BIT_CSP OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory("${WEBP_DIR}" "${CMAKE_BINARY_DIR}/webp" EXCLUDE_FROM_ALL)

# ---- SDL2_image: carrega so os formatos com LOAD_* (stb = png/jpg/bmp/tga/gif, webp); o resto compila como stub ----
set(IMG_SRC IMG.c IMG_avif.c IMG_bmp.c IMG_gif.c IMG_jpg.c IMG_jxl.c IMG_lbm.c IMG_pcx.c IMG_png.c
    IMG_pnm.c IMG_qoi.c IMG_stb.c IMG_svg.c IMG_tga.c IMG_tif.c IMG_webp.c IMG_xcf.c IMG_xpm.c IMG_xv.c)
list(TRANSFORM IMG_SRC PREPEND "${IMG_DIR}/src/")
add_library(SDL2_image STATIC ${IMG_SRC})
target_include_directories(SDL2_image PUBLIC "${IMG_DIR}/include" PRIVATE "${IMG_DIR}/src" "${WEBP_DIR}/src")
target_compile_definitions(SDL2_image PRIVATE
  USE_STBIMAGE SDL_IMAGE_USE_COMMON_BACKEND LOAD_BMP LOAD_GIF LOAD_JPG LOAD_PNG LOAD_TGA LOAD_WEBP)
target_link_libraries(SDL2_image PRIVATE SDL2::SDL2 webpdemux webp)
target_compile_options(SDL2_image PRIVATE -w)

# ---- SDL2_ttf: freetype embutido, sem harfbuzz ----
set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
# Com BUILD_SHARED_LIBS=OFF o SDL_ttf procura SDL2::SDL2-static; aqui o SDL e
# compartilhado, entao esse nome vira ponte para SDL2::SDL2.
add_library(SDL2::SDL2-static INTERFACE IMPORTED GLOBAL)
target_link_libraries(SDL2::SDL2-static INTERFACE SDL2::SDL2)
add_subdirectory("${TTF_DIR}" "${CMAKE_BINARY_DIR}/sdl2_ttf" EXCLUDE_FROM_ALL)

# O nucleo inclui <SDL2/SDL_image.h> e <SDL2/SDL_ttf.h>: monta um include/SDL2 com eles.
file(COPY "${IMG_DIR}/include/SDL_image.h" "${TTF_DIR}/SDL_ttf.h" DESTINATION "${CMAKE_BINARY_DIR}/inc/SDL2")

# ---- nucleo: src/*.c menos os alvos Tizen ----
file(GLOB NUVIO_SRC CONFIGURE_DEPENDS "${NUVIO_RAIZ}/src/*.c" "${NUVIO_RAIZ}/src/dts/*.c")
list(FILTER NUVIO_SRC EXCLUDE REGEX "/(video_tizen|video_tpk|tpk)\\.c$")
add_library(main SHARED ${NUVIO_SRC})
target_include_directories(main PRIVATE "${NUVIO_RAIZ}/src" "${CMAKE_BINARY_DIR}/inc")
target_compile_definitions(main PRIVATE NV_ANDROID NV_TOUCH_UI)
if(ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "x86_64")
  # Both modes ship in one APK; NDK r27 needs a 16KB-aligned RELRO end.
  foreach(t main SDL2)
    target_link_options(${t} PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,-z,common-page-size=16384")
  endforeach()
endif()
target_compile_options(main PRIVATE -O2 -Wno-unused-result)

# Chaves (-D) do tools/env.sh. Sem o arquivo, ou com chave vazia, o build sai
# com strings vazias, como o tools/build-local.sh.
set(NV_KEYS NV_SUPABASE_URL NV_SUPABASE_ANON_KEY NV_TV_LOGIN_BASE NV_TRAKT_CLIENT_ID
    NV_TRAKT_CLIENT_SECRET NV_SIMKL_CLIENT_ID NV_SIMKL_APP NV_TMDB_API_KEY NV_SEEKR_API_KEY NV_REC_URL NV_DISCORD_CLIENT_ID NV_VERSAO)
if(NUVIO_ENV_CMAKE AND EXISTS "${NUVIO_ENV_CMAKE}")
  include("${NUVIO_ENV_CMAKE}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${NUVIO_ENV_CMAKE}")
endif()
foreach(k ${NV_KEYS})
  # \" literal: o gerador de build escapa o resto.
  target_compile_definitions(main PRIVATE "${k}=\"${${k}}\"")
endforeach()

target_link_libraries(main PRIVATE SDL2::SDL2 SDL2_image SDL2_ttf webpdemux webp GLESv2 EGL log android z m dl)
# Todo pthread_create do nucleo passa por __wrap_pthread_create (src/android.c):
# sem atributo de pilha, o fio nasce com 8 MB como no glibc da LG/Tizen, e nao
# com o ~1 MB do bionic. O nucleo tem vetores grandes na pilha dos fios.
target_link_options(main PRIVATE "-Wl,--wrap=pthread_create")

# Motor P2P embutido (src/p2pmotor.h): nuvio-engine + libtorrent + OpenSSL,
# estaticos e ja compilados por tools/p2p-motor/build-android.sh. C++ dentro de
# um nucleo C: liga com o driver C++ e o libc++ estatico do ANDROID_STL.
if(NUVIO_P2P_MOTOR)
  set(P2P_LIB "${NUVIO_P2P_MOTOR}/lib/${ANDROID_ABI}")
  if(NOT EXISTS "${P2P_LIB}/libnuvio_engine.a")
    message(FATAL_ERROR "motor P2P ausente em ${P2P_LIB} (rode tools/p2p-motor/build-android.sh)")
  endif()
  enable_language(CXX)
  set_target_properties(main PROPERTIES LINKER_LANGUAGE CXX)
  target_compile_definitions(main PRIVATE NV_P2P_MOTOR)
  target_include_directories(main PRIVATE "${NUVIO_P2P_MOTOR}/include")
  target_link_libraries(main PRIVATE "${P2P_LIB}/libnuvio_engine.a"
    "${P2P_LIB}/libtorrent-rasterbar.a" "${P2P_LIB}/libssl.a" "${P2P_LIB}/libcrypto.a" c++experimental)
  target_link_options(main PRIVATE "-Wl,--gc-sections")
endif()

