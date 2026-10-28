# Nuvio on iOS: the same C core as the TVs inside a UIKit host.
#   libSDL2 static (UIKit video driver, GL ES 2), SDL2_image (stb + WebP),
#   SDL2_ttf (freetype embedded), libwebp: all from the Android source cache.
#   The app target is a bundle; art and fonts are copied in after the link.
# Configure with the Xcode generator so the project lives in Xcode:
#   cmake -S ios -B build/ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
#         -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0 -DNUVIO_RAIZ=$PWD
cmake_minimum_required(VERSION 3.22)
project(nuvio-ios C OBJC)

set(NUVIO_RAIZ "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH "repository root")
set(NUVIO_SDL_SRC "$ENV{HOME}/.cache/nuvio-android/src" CACHE PATH "SDL sources (tools/android.sh downloads them)")
set(NUVIO_ENV_CMAKE "" CACHE FILEPATH "-D keys written by tools/env.sh --env-file (outside git)")
set(NUVIO_BUNDLE_ID "space.nuvio.legacy" CACHE STRING "bundle identifier")

set(SDL_DIR   "${NUVIO_SDL_SRC}/SDL2-2.30.9")
set(IMG_DIR   "${NUVIO_SDL_SRC}/SDL2_image-2.8.2")
set(TTF_DIR   "${NUVIO_SDL_SRC}/SDL2_ttf-2.22.0")
set(WEBP_DIR  "${NUVIO_SDL_SRC}/libwebp-1.4.0")
foreach(d SDL_DIR IMG_DIR TTF_DIR WEBP_DIR)
  if(NOT EXISTS "${${d}}")
    message(FATAL_ERROR "missing source: ${${d}} (run tools/android.sh once)")
  endif()
endforeach()

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---- SDL2, static, no audio (the player owns sound) ----
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_METAL OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_METAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${SDL_DIR}" "${CMAKE_BINARY_DIR}/sdl2" EXCLUDE_FROM_ALL)
# iOS 27 refuses to launch apps without the UIScene life cycle. SDL 2.30 creates
# its UIWindow the old way; a private copy of SDL_uikitwindow.m attaches that
# window to the connected UIWindowScene (NuvioSceneDelegate, ios.m, is the
# scene the manifest names). The shared source cache is never edited.
file(READ "${SDL_DIR}/src/video/uikit/SDL_uikitwindow.m" NV_UIKITWINDOW)
string(REPLACE "UIWindow *uiwindow = [[SDL_uikitwindow alloc] initWithFrame:data.uiscreen.bounds];"
  "UIWindow *uiwindow = [[SDL_uikitwindow alloc] initWithFrame:data.uiscreen.bounds];
        for (UIScene *nvScene in UIApplication.sharedApplication.connectedScenes) {
            if ([nvScene isKindOfClass:UIWindowScene.class]) { uiwindow.windowScene = (UIWindowScene *)nvScene; break; }
        }" NV_UIKITWINDOW "${NV_UIKITWINDOW}")
string(REPLACE "#include \"../../" "#include \"${SDL_DIR}/src/" NV_UIKITWINDOW "${NV_UIKITWINDOW}")
string(REPLACE "#include \"../" "#include \"${SDL_DIR}/src/video/" NV_UIKITWINDOW "${NV_UIKITWINDOW}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SDL_uikitwindow_scene.m" "${NV_UIKITWINDOW}")
get_target_property(NV_SDL_SOURCES SDL2-static SOURCES)
list(FILTER NV_SDL_SOURCES EXCLUDE REGEX "/SDL_uikitwindow\\.m$")
set_property(TARGET SDL2-static PROPERTY SOURCES "${NV_SDL_SOURCES}" "${CMAKE_CURRENT_BINARY_DIR}/SDL_uikitwindow_scene.m")
target_include_directories(SDL2-static PRIVATE "${SDL_DIR}/src/video/uikit" "${SDL_DIR}/src/video" "${SDL_DIR}/src" "${SDL_DIR}/include")

# ---- libwebp (decoder + demux only) ----
foreach(o ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO WEBPMUX EXTRAS LIBWEBPMUX)
  set(WEBP_BUILD_${o} OFF CACHE BOOL "" FORCE)
endforeach()
set(WEBP_ENABLE_SWAP_16BIT_CSP OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory("${WEBP_DIR}" "${CMAKE_BINARY_DIR}/webp" EXCLUDE_FROM_ALL)

# ---- SDL2_image: stb (png/jpg/bmp/tga/gif) + webp, the rest compiled as stubs ----
set(IMG_SRC IMG.c IMG_avif.c IMG_bmp.c IMG_gif.c IMG_jpg.c IMG_jxl.c IMG_lbm.c IMG_pcx.c IMG_png.c
    IMG_pnm.c IMG_qoi.c IMG_stb.c IMG_svg.c IMG_tga.c IMG_tif.c IMG_webp.c IMG_xcf.c IMG_xpm.c IMG_xv.c)
list(TRANSFORM IMG_SRC PREPEND "${IMG_DIR}/src/")
add_library(SDL2_image STATIC ${IMG_SRC})
target_include_directories(SDL2_image PUBLIC "${IMG_DIR}/include" PRIVATE "${IMG_DIR}/src" "${WEBP_DIR}/src")
target_compile_definitions(SDL2_image PRIVATE
  USE_STBIMAGE SDL_IMAGE_USE_COMMON_BACKEND LOAD_BMP LOAD_GIF LOAD_JPG LOAD_PNG LOAD_TGA LOAD_WEBP)
target_link_libraries(SDL2_image PRIVATE SDL2::SDL2-static webpdemux webp)
target_compile_options(SDL2_image PRIVATE -w)

# ---- SDL2_ttf: embedded freetype, no harfbuzz ----
set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${TTF_DIR}" "${CMAKE_BINARY_DIR}/sdl2_ttf" EXCLUDE_FROM_ALL)

# The core includes <SDL2/SDL_image.h> and <SDL2/SDL_ttf.h>.
file(COPY "${IMG_DIR}/include/SDL_image.h" "${TTF_DIR}/SDL_ttf.h" DESTINATION "${CMAKE_BINARY_DIR}/inc/SDL2")

# ---- the app: src/*.c minus the Tizen targets, plus the iOS host ----
file(GLOB NUVIO_SRC CONFIGURE_DEPENDS "${NUVIO_RAIZ}/src/*.c" "${NUVIO_RAIZ}/src/dts/*.c")
list(FILTER NUVIO_SRC EXCLUDE REGEX "/(video_tizen|video_tpk|tpk)\\.c$")
add_executable(Nuvio MACOSX_BUNDLE ${NUVIO_SRC} "${CMAKE_CURRENT_LIST_DIR}/ios.m")
target_include_directories(Nuvio PRIVATE "${NUVIO_RAIZ}/src" "${CMAKE_BINARY_DIR}/inc" "${CMAKE_CURRENT_LIST_DIR}")
target_compile_definitions(Nuvio PRIVATE NV_IOS NV_TOUCH_UI GLES_SILENCE_DEPRECATION)
target_compile_options(Nuvio PRIVATE -O2 -Wno-unused-result -Wno-deprecated-declarations)
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/ios.m" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

set(NV_KEYS NV_SUPABASE_URL NV_SUPABASE_ANON_KEY NV_TV_LOGIN_BASE NV_TRAKT_CLIENT_ID
    NV_TRAKT_CLIENT_SECRET NV_SIMKL_CLIENT_ID NV_SIMKL_APP NV_TMDB_API_KEY NV_SEEKR_API_KEY NV_REC_URL NV_DISCORD_CLIENT_ID NV_VERSAO)
if(NUVIO_ENV_CMAKE AND EXISTS "${NUVIO_ENV_CMAKE}")
  include("${NUVIO_ENV_CMAKE}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${NUVIO_ENV_CMAKE}")
endif()
foreach(k ${NV_KEYS})
  target_compile_definitions(Nuvio PRIVATE "${k}=\"${${k}}\"")
endforeach()

target_link_libraries(Nuvio PRIVATE SDL2::SDL2-static SDL2_image SDL2_ttf webpdemux webp z m
  "-framework UIKit" "-framework Foundation" "-framework CoreGraphics" "-framework QuartzCore"
  "-framework OpenGLES" "-framework GameController" "-framework CoreMotion" "-framework AVFoundation"
  "-framework CoreVideo" "-framework CoreMedia" "-framework CoreHaptics" "-framework Metal" "-framework IOKit")

set_target_properties(Nuvio PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/Info.plist.in"
  MACOSX_BUNDLE_GUI_IDENTIFIER "${NUVIO_BUNDLE_ID}"
  MACOSX_BUNDLE_BUNDLE_NAME "Nuvio"
  MACOSX_BUNDLE_BUNDLE_VERSION "${NV_VERSAO}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${NV_VERSAO}"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${NUVIO_BUNDLE_ID}"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
  XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
  XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
  XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "NO")

# Art and fonts ride inside the bundle; the host passes the art path as argv[1]
# and the core finds the fonts next to it (../fonts).
add_custom_command(TARGET Nuvio POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_BUNDLE_CONTENT_DIR:Nuvio>/art" "$<TARGET_BUNDLE_CONTENT_DIR:Nuvio>/fonts"
  # Only git-tracked art and fonts: ignored personal files live next to them.
  COMMAND bash "${NUVIO_RAIZ}/tools/ios-assets.sh" "${NUVIO_RAIZ}" "$<TARGET_BUNDLE_CONTENT_DIR:Nuvio>"
  COMMENT "copying art and fonts into Nuvio.app")
