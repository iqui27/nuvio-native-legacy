package space.nuvio.nativelegacy

import java.io.File

internal object ApkCache {
    // A named download wins; otherwise retain only the newest uninstalled APK.
    fun prune(dir: File, installed: Long, downloading: String?, version: (File) -> Long?) {
        val apks = dir.listFiles()?.filter { it.name.endsWith(".apk") && it.isFile } ?: return
        val versions = apks.associateWith(version)
        val keep = if (downloading != null) apks.find { it.name == downloading }
            else apks.filter { (versions[it] ?: 0) > installed }
                .maxByOrNull { versions[it] ?: 0 }
        for (apk in apks) {
            if (apk != keep || (versions[apk]?.let { it <= installed } == true)) apk.delete()
        }
    }
}
