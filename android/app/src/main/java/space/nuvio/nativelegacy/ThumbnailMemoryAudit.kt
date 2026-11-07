package space.nuvio.nativelegacy

import android.app.ActivityManager
import android.content.Context
import android.os.Debug
import android.os.SystemClock
import android.util.Log
import java.io.File

/** Opt-in diagnostic (debug.nuvio.miniatura bit 512). No codec policy changes. */
internal class ThumbnailMemoryAudit(private val source: Int) {
    private val started = SystemClock.elapsedRealtime()
    private var frames = 0
    private var minimum = Long.MAX_VALUE

    fun snapshot(phase: String) {
        try {
            val context = NvPlayer.contexto() ?: return
            val manager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
            val memory = ActivityManager.MemoryInfo().also { manager.getMemoryInfo(it) }
            minimum = minOf(minimum, memory.availMem)
            val process = Debug.MemoryInfo().also { Debug.getMemoryInfo(it) }
            val rss = File("/proc/self/status").useLines { lines ->
                lines.firstOrNull { it.startsWith("VmRSS:") }?.substringAfter(':')?.trim()?.substringBefore(' ')
            } ?: "-1"
            val java = Runtime.getRuntime().let { it.totalMemory() - it.freeMemory() }
            val line = "source=$source phase=$phase elapsedMs=${SystemClock.elapsedRealtime() - started} " +
                "frames=$frames availableKB=${memory.availMem / 1024} minAvailableKB=${minimum / 1024} " +
                "appRssKB=$rss appPssKB=${process.totalPss} javaUsedKB=${java / 1024} " +
                "nativeHeapKB=${Debug.getNativeHeapAllocatedSize() / 1024}"
            Log.i("nuvio-miniatura", "[decoder-memory] $line")
            File(context.filesDir, "thumbnail-memory-audit.log").let { file ->
                if (file.length() > 512 * 1024) file.delete()
                file.appendText("${System.currentTimeMillis()} $line\n")
            }
        } catch (e: Exception) {
            Log.w("nuvio-miniatura", "[decoder-memory] snapshot unavailable: ${e.javaClass.simpleName}")
        }
    }
    fun frame() {
        frames++
        if (frames == 1 || frames % 30 == 0) snapshot(if (frames == 1) "first-thumbnail" else "decoding")
    }
}
