plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// Tudo que vem de fora do projeto entra por propriedade (-P) que o
// tools/android.sh passa; os padroes servem para abrir no Android Studio.
val raiz = rootProject.projectDir.parentFile                         // raiz do repo
val cache = file(System.getProperty("user.home") + "/.cache/nuvio-android")
val sdlSrc = (findProperty("nuvio.sdlSrc") as String?) ?: "$cache/src"
val estagio = (findProperty("nuvio.estagio") as String?) ?: "${raiz}/build/android"
val ffmpegAar = (findProperty("nuvio.ffmpegAar") as String?)?.let { file(it) }
require(ffmpegAar == null || ffmpegAar.isFile) {
    "nuvio.ffmpegAar must point to an existing AAR"
}

// Versao: fonte unica e deploy/app/appinfo.json (mesma do env.sh).
val versao = Regex("\"version\"\\s*:\\s*\"([^\"]+)\"")
    .find(File(raiz, "deploy/app/appinfo.json").readText())!!.groupValues[1]
val (vx, vy, vz) = versao.split(".").map { it.toInt() }

android {
    namespace = "space.nuvio.nativelegacy"
    compileSdk = 35
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "space.nuvio.nativelegacy"
        minSdk = 24
        targetSdk = 35
        versionCode = vx * 10000 + vy * 100 + vz
        versionName = versao
        ndk { abiFilters += listOf("arm64-v8a", "armeabi-v7a") }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=c++_static",  // hidapi do SDL (hid.cpp) e C++
                    "-DNUVIO_RAIZ=$raiz",
                    "-DNUVIO_SDL_SRC=$sdlSrc",
                    "-DNUVIO_ENV_CMAKE=$estagio/nuvio-env.cmake",
                    "-DNUVIO_P2P_MOTOR=${(findProperty("nuvio.p2pMotor") as String?) ?: ""}",
                )
                arguments += "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    // Assinatura de release so existe com keystore no ambiente; sem ela o
    // assembleRelease nem e chamado (tools/android.sh).
    val ks = System.getenv("NUVIO_KEYSTORE")
    signingConfigs {
        if (ks != null) create("release") {
            storeFile = file(ks)
            storePassword = System.getenv("NUVIO_KEYSTORE_PASS")
            keyAlias = System.getenv("NUVIO_KEY_ALIAS")
            keyPassword = System.getenv("NUVIO_KEY_PASS")
        }
    }
    buildTypes {
        debug {
            // Nunca substituir a instalacao de producao durante diagnosticos.
            applicationIdSuffix = ".strictdebug"
            versionNameSuffix = "-strictdebug"
        }
        release {
            isMinifyEnabled = false
            if (ks != null) signingConfig = signingConfigs.getByName("release")
        }
    }

    // O nucleo abre libcurl.so/libjpeg.so por dlopen: precisam estar em
    // lib/<abi>/ ja extraidas (extractNativeLibs), nao dentro do APK.
    packaging { jniLibs { useLegacyPackaging = true } }
    // Arte e fontes ja vem comprimidas/pequenas; sem recompressao de png/ttf.
    androidResources { noCompress += listOf("png", "jpg", "webp", "ttf", "json") }

    sourceSets["main"].apply {
        java.srcDir("build/sdl-java")                 // classes org.libsdl.app copiadas do SDL
        assets.srcDir("$estagio/assets")              // art/ e fonts/ (tools/android.sh)
        jniLibs.srcDir("$estagio/jnilibs")            // libcurl.so/libjpeg.so, se existirem
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    testOptions { unitTests.isReturnDefaultValues = true }
}

// Copia as classes Java do SDL no build (nada de copia grande no git).
val copiaSdlJava = tasks.register<Copy>("copiaSdlJava") {
    from("$sdlSrc/SDL2-2.30.9/android-project/app/src/main/java/org/libsdl/app")
    into("build/sdl-java/org/libsdl/app")
    // PILHA DE 16 MB PARA O main() DO NUCLEO. O SDL cria o SDLThread com a
    // pilha padrao da JVM (~1 MB); o nucleo foi escrito para os 8 MB do fio
    // principal da LG e do .tpk (tpk.c), e o guia estourou a pilha com 900
    // canais numa copia local (SIGSEGV em guia.c, TCL, 30/09/2026).
    // JOIN COM PRAZO (#266). O onDestroy do SDL espera o main() do C sem
    // limite, no fio da interface; com o C preso o app nem saia ("so
    // reiniciando a TV"). 3 s e depois segue: o NuvioActivity mata o processo
    // ao sair.
    filter { linha: String ->
        linha.replace("new Thread(new SDLMain(), \"SDLThread\")",
                      "new Thread(null, new SDLMain(), \"SDLThread\", 16L * 1024 * 1024)")
             .replace("SDLActivity.mSDLThread.join();", "SDLActivity.mSDLThread.join(3000);")
    }
}
tasks.named("preBuild") { dependsOn(copiaSdlJava) }

dependencies {
    // Video (NvPlayer.kt, de outro agente): Media3 ExoPlayer.
    implementation("androidx.media3:media3-exoplayer:1.8.0")
    implementation("androidx.media3:media3-exoplayer-hls:1.8.0")
    implementation("androidx.media3:media3-exoplayer-dash:1.8.0")
    // Decodificador de AUDIO por FFmpeg (DTS, DTS-HD, TrueHD, E-AC-3...) para
    // quando a TV nao tem o decodificador nem passa o bitstream adiante: sem
    // ele a fonte toca muda (evento 9). Compilado e publicado pela Jellyfin no
    // Maven Central, na mesma versao do Media3. So entra quando a plataforma
    // nao serve (EXTENSION_RENDERER_MODE_ON no NvPlayer).
    if (ffmpegAar != null) {
        // Published 1.8.0+1 classes/32-bit JNI; selected 64-bit JNI rebuilt for 16KB pages.
        implementation(files(ffmpegAar))
        implementation("androidx.media3:media3-decoder:1.8.0")
    } else {
        implementation("org.jellyfin.media3:media3-ffmpeg-decoder:1.8.0+1")
    }
    implementation("androidx.core:core-ktx:1.13.1")
    testImplementation("junit:junit:4.13.2")
}

