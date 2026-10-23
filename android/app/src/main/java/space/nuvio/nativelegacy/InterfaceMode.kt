package space.nuvio.nativelegacy

import java.io.File

// Device classification and the persisted numbers are independent of screen
// shape, touch hardware and whichever controller happens to be connected.
object InterfaceMode {
    const val TV = 0
    const val MOBILE = 1
    const val AUTO = 2
    enum class Orientation { LANDSCAPE, SENSOR_LANDSCAPE, FULL_USER }

    fun automaticMobile(tvUiMode: Boolean, televisionFeature: Boolean,
                        leanbackFeature: Boolean): Boolean =
        !(tvUiMode || televisionFeature || leanbackFeature)

    fun resolve(preference: Int, automaticMobile: Boolean): Boolean = when (preference) {
        TV -> false
        MOBILE -> true
        else -> automaticMobile
    }

    fun orientation(mobile: Boolean, fullScreenPlayer: Boolean): Orientation = when {
        !mobile -> Orientation.LANDSCAPE
        fullScreenPlayer -> Orientation.SENSOR_LANDSCAPE
        else -> Orientation.FULL_USER
    }

    internal fun viewport(mobile: Boolean, width: Int, height: Int): TouchViewport.Bounds? =
        if (mobile) TouchViewport.fit(width, height, 0, 0, 0, 0) else null

    fun preference(lines: Sequence<String>): Int {
        var preference = AUTO
        for (line in lines) {
            // Match the native 96-byte buffer (94 bytes plus LF and NUL).
            if (line.removeSuffix("\r").toByteArray(Charsets.UTF_8).size > 94) continue
            val fields = line.trim().split(Regex("\\s+"), limit = 3)
            if (fields.size < 2 || fields[0] != "interfaceModoLocal") continue
            val number = Regex("^[+-]?\\d+").find(fields[1])?.value?.toIntOrNull()
            number?.takeIf { it in TV..AUTO }?.let { preference = it }
        }
        return preference
    }

    fun readPreference(file: File): Int = try {
        file.useLines { preference(it) }
    } catch (_: Exception) { AUTO }
}
