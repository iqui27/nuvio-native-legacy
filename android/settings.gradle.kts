pluginManagement {
    repositories {
        google()
        // Google's mirror of Maven Central first: Maven Central answers CI with
        // 429 Too Many Requests (owner 25/11/2026, build-apk on feat/subtitle-rewrite).
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        // Google's mirror of Maven Central first: Maven Central answers CI with
        // 429 Too Many Requests (owner 25/11/2026, build-apk on feat/subtitle-rewrite).
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        mavenCentral()
    }
}
rootProject.name = "nuvio-android"
include(":app")
